#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "cache_format.h"

namespace vis {

struct LoadedCacheData {
    const CacheHeader* header = nullptr;
    const Vec3* vertices = nullptr;
    const std::uint32_t* indices = nullptr;
    const std::uint8_t* triangle_kinds = nullptr; // Optional metadata section.
    const std::uint32_t* triangle_material_hashes = nullptr; // Optional material metadata section.
    const BvhNodeDisk* bvh_nodes = nullptr;
    const std::uint32_t* bvh_primitive_order = nullptr;
    std::size_t vertex_count = 0;
    std::size_t index_count = 0;
    std::size_t triangle_kind_count = 0;
    std::size_t triangle_material_hash_count = 0;
    std::size_t bvh_node_count = 0;
    std::size_t bvh_primitive_count = 0;
};

class CacheFileView {
public:
    CacheFileView() = default;
    ~CacheFileView();

    CacheFileView(const CacheFileView&) = delete;
    CacheFileView& operator=(const CacheFileView&) = delete;

    CacheFileView(CacheFileView&& other) noexcept;
    CacheFileView& operator=(CacheFileView&& other) noexcept;

    bool open(const std::string& path, std::string* out_error);
    void close();

    bool loaded() const { return loaded_; }
    LoadedCacheData data() const { return data_; }
    std::string path() const { return path_; }

private:
    bool parse(std::string* out_error);

private:
    bool loaded_ = false;
    std::string path_;

#if defined(_WIN32)
    void* file_handle_ = nullptr;
    void* mapping_handle_ = nullptr;
#endif
    void* mapped_ptr_ = nullptr;
    std::size_t mapped_size_ = 0;
    LoadedCacheData data_{};
    std::string owned_bytes_;
};

bool write_cache_file(const std::string& output_path, const CompiledMapData& data, std::string* out_error);

} // namespace vis
