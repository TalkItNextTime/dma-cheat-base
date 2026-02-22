#include "cache_io.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "hash.h"

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

namespace vis {

namespace {

template <typename T>
bool in_bounds(const std::byte* base, std::size_t size, const T* ptr, std::size_t count) {
    const auto* p = reinterpret_cast<const std::byte*>(ptr);
    const std::size_t bytes = sizeof(T) * count;
    if (p < base) {
        return false;
    }
    if ((p + bytes) < p) {
        return false;
    }
    return static_cast<std::size_t>(p - base) + bytes <= size;
}

const std::byte* ptr_at(const std::byte* base, std::size_t size, std::uint64_t offset, std::uint64_t bytes) {
    if (offset > size || bytes > size || offset + bytes > size) {
        return nullptr;
    }
    return base + static_cast<std::size_t>(offset);
}

void append_bytes(std::vector<std::byte>* dst, const void* src, std::size_t size) {
    const auto* p = static_cast<const std::byte*>(src);
    dst->insert(dst->end(), p, p + size);
}

} // namespace

CacheFileView::~CacheFileView() {
    close();
}

CacheFileView::CacheFileView(CacheFileView&& other) noexcept {
    *this = std::move(other);
}

CacheFileView& CacheFileView::operator=(CacheFileView&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    close();
    loaded_ = other.loaded_;
    path_ = std::move(other.path_);
#if defined(_WIN32)
    file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#endif
    mapped_ptr_ = other.mapped_ptr_;
    mapped_size_ = other.mapped_size_;
    data_ = other.data_;
    owned_bytes_ = std::move(other.owned_bytes_);

    other.loaded_ = false;
    other.mapped_ptr_ = nullptr;
    other.mapped_size_ = 0;
    other.data_ = {};
    return *this;
}

bool CacheFileView::open(const std::string& path, std::string* out_error) {
    close();
    path_ = path;

#if defined(_WIN32)
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (out_error != nullptr) {
            *out_error = "CreateFileA failed for " + path;
        }
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        if (out_error != nullptr) {
            *out_error = "GetFileSizeEx failed";
        }
        return false;
    }
    if (size.QuadPart <= 0) {
        CloseHandle(file);
        if (out_error != nullptr) {
            *out_error = "cache file is empty";
        }
        return false;
    }
    const HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) {
        CloseHandle(file);
        if (out_error != nullptr) {
            *out_error = "CreateFileMappingA failed";
        }
        return false;
    }
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        CloseHandle(mapping);
        CloseHandle(file);
        if (out_error != nullptr) {
            *out_error = "MapViewOfFile failed";
        }
        return false;
    }

    file_handle_ = file;
    mapping_handle_ = mapping;
    mapped_ptr_ = view;
    mapped_size_ = static_cast<std::size_t>(size.QuadPart);
#else
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (out_error != nullptr) {
            *out_error = "failed to open cache file: " + path;
        }
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    owned_bytes_.assign(bytes.begin(), bytes.end());
    mapped_ptr_ = owned_bytes_.data();
    mapped_size_ = owned_bytes_.size();
#endif

    if (!parse(out_error)) {
        close();
        return false;
    }

    loaded_ = true;
    return true;
}

void CacheFileView::close() {
    loaded_ = false;
    path_.clear();
    data_ = {};

#if defined(_WIN32)
    if (mapped_ptr_ != nullptr) {
        UnmapViewOfFile(mapped_ptr_);
    }
    if (mapping_handle_ != nullptr) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping_handle_));
    }
    if (file_handle_ != nullptr) {
        CloseHandle(reinterpret_cast<HANDLE>(file_handle_));
    }
    file_handle_ = nullptr;
    mapping_handle_ = nullptr;
#endif
    mapped_ptr_ = nullptr;
    mapped_size_ = 0;
    owned_bytes_.clear();
}

bool CacheFileView::parse(std::string* out_error) {
    if (mapped_ptr_ == nullptr || mapped_size_ < sizeof(CacheHeader)) {
        if (out_error != nullptr) {
            *out_error = "mapped cache too small";
        }
        return false;
    }

    const auto* base = reinterpret_cast<const std::byte*>(mapped_ptr_);
    const auto* header = reinterpret_cast<const CacheHeader*>(base);

    if (std::memcmp(header->magic, kCacheMagic.data(), kCacheMagic.size()) != 0) {
        if (out_error != nullptr) {
            *out_error = "invalid cache magic";
        }
        return false;
    }
    if (header->version != kCacheVersion) {
        if (out_error != nullptr) {
            *out_error = "unsupported cache version";
        }
        return false;
    }

    const std::size_t section_table_bytes = sizeof(CacheSectionEntry) * header->section_count;
    const std::size_t section_table_offset = sizeof(CacheHeader);
    if (section_table_offset + section_table_bytes > mapped_size_) {
        if (out_error != nullptr) {
            *out_error = "section table out of bounds";
        }
        return false;
    }

    const auto* sections = reinterpret_cast<const CacheSectionEntry*>(base + section_table_offset);
    std::unordered_map<std::uint32_t, CacheSectionEntry> section_map;
    section_map.reserve(header->section_count);
    for (std::uint32_t i = 0; i < header->section_count; ++i) {
        section_map[sections[i].id] = sections[i];
    }

    const auto it_vertices = section_map.find(static_cast<std::uint32_t>(CacheSectionId::Vertices));
    const auto it_indices = section_map.find(static_cast<std::uint32_t>(CacheSectionId::Indices));
    const auto it_bvh_nodes = section_map.find(static_cast<std::uint32_t>(CacheSectionId::BvhNodes));
    const auto it_bvh_order = section_map.find(static_cast<std::uint32_t>(CacheSectionId::BvhPrimitiveOrder));
    if (it_vertices == section_map.end() || it_indices == section_map.end() || it_bvh_nodes == section_map.end() || it_bvh_order == section_map.end()) {
        if (out_error != nullptr) {
            *out_error = "missing required cache sections";
        }
        return false;
    }

    const auto* vertices_ptr = reinterpret_cast<const Vec3*>(ptr_at(base, mapped_size_, it_vertices->second.offset, it_vertices->second.size));
    const auto* indices_ptr = reinterpret_cast<const std::uint32_t*>(ptr_at(base, mapped_size_, it_indices->second.offset, it_indices->second.size));
    const auto* bvh_nodes_ptr = reinterpret_cast<const BvhNodeDisk*>(ptr_at(base, mapped_size_, it_bvh_nodes->second.offset, it_bvh_nodes->second.size));
    const auto* bvh_order_ptr = reinterpret_cast<const std::uint32_t*>(ptr_at(base, mapped_size_, it_bvh_order->second.offset, it_bvh_order->second.size));

    if (vertices_ptr == nullptr || indices_ptr == nullptr || bvh_nodes_ptr == nullptr || bvh_order_ptr == nullptr) {
        if (out_error != nullptr) {
            *out_error = "cache section pointer out of bounds";
        }
        return false;
    }

    const std::size_t vertex_count = static_cast<std::size_t>(it_vertices->second.size / sizeof(Vec3));
    const std::size_t index_count = static_cast<std::size_t>(it_indices->second.size / sizeof(std::uint32_t));
    const std::size_t bvh_node_count = static_cast<std::size_t>(it_bvh_nodes->second.size / sizeof(BvhNodeDisk));
    const std::size_t bvh_primitive_count = static_cast<std::size_t>(it_bvh_order->second.size / sizeof(std::uint32_t));
    if (index_count % 3u != 0u) {
        if (out_error != nullptr) {
            *out_error = "cache index count not divisible by 3";
        }
        return false;
    }
    const std::size_t tri_count = index_count / 3u;

    const auto it_metadata = section_map.find(static_cast<std::uint32_t>(CacheSectionId::Metadata));
    const std::uint8_t* triangle_kinds_ptr = nullptr;
    std::size_t triangle_kind_count = 0;
    if (it_metadata != section_map.end()) {
        triangle_kinds_ptr = reinterpret_cast<const std::uint8_t*>(
            ptr_at(base, mapped_size_, it_metadata->second.offset, it_metadata->second.size));
        if (triangle_kinds_ptr == nullptr) {
            if (out_error != nullptr) {
                *out_error = "cache metadata pointer out of bounds";
            }
            return false;
        }
        triangle_kind_count = static_cast<std::size_t>(it_metadata->second.size);
        if (triangle_kind_count != tri_count) {
            if (out_error != nullptr) {
                *out_error = "cache metadata triangle-kind count mismatch";
            }
            return false;
        }
        if (!in_bounds(base, mapped_size_, triangle_kinds_ptr, triangle_kind_count)) {
            if (out_error != nullptr) {
                *out_error = "cache metadata failed bounds validation";
            }
            return false;
        }
    }

    if (!in_bounds(base, mapped_size_, vertices_ptr, vertex_count) || !in_bounds(base, mapped_size_, indices_ptr, index_count) ||
        !in_bounds(base, mapped_size_, bvh_nodes_ptr, bvh_node_count) || !in_bounds(base, mapped_size_, bvh_order_ptr, bvh_primitive_count)) {
        if (out_error != nullptr) {
            *out_error = "cache sections failed bounds validation";
        }
        return false;
    }

    std::uint64_t payload_begin = UINT64_MAX;
    for (std::uint32_t i = 0; i < header->section_count; ++i) {
        payload_begin = std::min<std::uint64_t>(payload_begin, sections[i].offset);
    }
    if (payload_begin >= mapped_size_) {
        if (out_error != nullptr) {
            *out_error = "invalid payload begin";
        }
        return false;
    }
    const std::size_t payload_size = mapped_size_ - static_cast<std::size_t>(payload_begin);
    const auto* payload_ptr = base + static_cast<std::size_t>(payload_begin);
    const std::uint32_t payload_crc = crc32(payload_ptr, payload_size);
    if (payload_crc != header->payload_crc32) {
        if (out_error != nullptr) {
            *out_error = "cache payload crc mismatch";
        }
        return false;
    }

    data_.header = header;
    data_.vertices = vertices_ptr;
    data_.indices = indices_ptr;
    data_.triangle_kinds = triangle_kinds_ptr;
    data_.bvh_nodes = bvh_nodes_ptr;
    data_.bvh_primitive_order = bvh_order_ptr;
    data_.vertex_count = vertex_count;
    data_.index_count = index_count;
    data_.triangle_kind_count = triangle_kind_count;
    data_.bvh_node_count = bvh_node_count;
    data_.bvh_primitive_count = bvh_primitive_count;
    return true;
}

bool write_cache_file(const std::string& output_path, const CompiledMapData& data, std::string* out_error) {
    if (data.vertices.empty() || data.indices.empty() || data.bvh_nodes.empty() || data.bvh_primitive_order.empty()) {
        if (out_error != nullptr) {
            *out_error = "compiled map data is incomplete";
        }
        return false;
    }
    if (data.indices.size() % 3u != 0u) {
        if (out_error != nullptr) {
            *out_error = "compiled map indices not divisible by 3";
        }
        return false;
    }
    const std::size_t tri_count = data.indices.size() / 3u;
    if (!data.triangle_kinds.empty() && data.triangle_kinds.size() != tri_count) {
        if (out_error != nullptr) {
            *out_error = "triangle_kinds count does not match triangle count";
        }
        return false;
    }
    const bool has_triangle_kinds = data.triangle_kinds.size() == tri_count && tri_count > 0u;

    CacheHeader header{};
    std::memcpy(header.magic, kCacheMagic.data(), kCacheMagic.size());
    header.version = kCacheVersion;
    header.section_count = has_triangle_kinds ? 5u : 4u;
    header.source_hash = data.source_hash;
    header.build_unix_seconds = static_cast<std::uint64_t>(std::time(nullptr));
    header.static_tri_count = static_cast<std::uint32_t>(data.indices.size() / 3u);
    header.bvh_node_count = static_cast<std::uint32_t>(data.bvh_nodes.size());
    header.vertex_count = static_cast<std::uint32_t>(data.vertices.size());
    header.index_count = static_cast<std::uint32_t>(data.indices.size());
    header.attr_mask = data.attr_mask;
    header.payload_crc32 = 0u;
    std::memset(header.map_name, 0, sizeof(header.map_name));
    const std::size_t copy_name = std::min<std::size_t>(data.map_name.size(), sizeof(header.map_name) - 1u);
    std::memcpy(header.map_name, data.map_name.data(), copy_name);

    std::vector<CacheSectionEntry> sections;
    sections.reserve(header.section_count);

    std::uint64_t payload_offset = static_cast<std::uint64_t>(sizeof(CacheHeader) + sizeof(CacheSectionEntry) * header.section_count);

    const auto add_section = [&](CacheSectionId id, std::uint64_t size) {
        CacheSectionEntry e{};
        e.id = static_cast<std::uint32_t>(id);
        e.offset = payload_offset;
        e.size = size;
        sections.push_back(e);
        payload_offset += size;
    };

    add_section(CacheSectionId::Vertices, static_cast<std::uint64_t>(data.vertices.size() * sizeof(Vec3)));
    add_section(CacheSectionId::Indices, static_cast<std::uint64_t>(data.indices.size() * sizeof(std::uint32_t)));
    add_section(CacheSectionId::BvhNodes, static_cast<std::uint64_t>(data.bvh_nodes.size() * sizeof(BvhNodeDisk)));
    add_section(CacheSectionId::BvhPrimitiveOrder, static_cast<std::uint64_t>(data.bvh_primitive_order.size() * sizeof(std::uint32_t)));
    if (has_triangle_kinds) {
        add_section(CacheSectionId::Metadata, static_cast<std::uint64_t>(data.triangle_kinds.size() * sizeof(std::uint8_t)));
    }

    std::vector<std::byte> out_bytes;
    out_bytes.reserve(static_cast<std::size_t>(payload_offset));

    append_bytes(&out_bytes, &header, sizeof(header));
    append_bytes(&out_bytes, sections.data(), sections.size() * sizeof(CacheSectionEntry));
    append_bytes(&out_bytes, data.vertices.data(), data.vertices.size() * sizeof(Vec3));
    append_bytes(&out_bytes, data.indices.data(), data.indices.size() * sizeof(std::uint32_t));
    append_bytes(&out_bytes, data.bvh_nodes.data(), data.bvh_nodes.size() * sizeof(BvhNodeDisk));
    append_bytes(&out_bytes, data.bvh_primitive_order.data(), data.bvh_primitive_order.size() * sizeof(std::uint32_t));
    if (has_triangle_kinds) {
        append_bytes(&out_bytes, data.triangle_kinds.data(), data.triangle_kinds.size() * sizeof(std::uint8_t));
    }

    const std::size_t payload_begin = sizeof(CacheHeader) + sizeof(CacheSectionEntry) * sections.size();
    const std::uint32_t payload_crc = crc32(out_bytes.data() + payload_begin, out_bytes.size() - payload_begin);
    auto* mutable_header = reinterpret_cast<CacheHeader*>(out_bytes.data());
    mutable_header->payload_crc32 = payload_crc;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(output_path).parent_path(), ec);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        if (out_error != nullptr) {
            *out_error = "failed to create cache file: " + output_path;
        }
        return false;
    }
    out.write(reinterpret_cast<const char*>(out_bytes.data()), static_cast<std::streamsize>(out_bytes.size()));
    if (!out.good()) {
        if (out_error != nullptr) {
            *out_error = "failed to write cache file: " + output_path;
        }
        return false;
    }
    return true;
}

} // namespace vis
