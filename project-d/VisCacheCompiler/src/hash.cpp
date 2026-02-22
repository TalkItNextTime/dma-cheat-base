#include "hash.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace vis {

namespace {

constexpr std::uint64_t kFnvOffsetBasis64 = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime64 = 1099511628211ull;

const std::array<std::uint32_t, 256>& crc32_table() {
    static const std::array<std::uint32_t, 256> table = []() {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
            }
            t[i] = c;
        }
        return t;
    }();
    return table;
}

} // namespace

std::uint64_t fnv1a64(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = kFnvOffsetBasis64;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= kFnvPrime64;
    }
    return hash;
}

std::uint64_t fnv1a64(const std::vector<std::byte>& data) {
    if (data.empty()) {
        return kFnvOffsetBasis64;
    }
    return fnv1a64(data.data(), data.size());
}

std::uint64_t fnv1a64_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open file for hash: " + path);
    }

    std::uint64_t hash = kFnvOffsetBasis64;
    std::array<char, 1 << 16> buffer{};
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read_count = in.gcount();
        for (std::streamsize i = 0; i < read_count; ++i) {
            hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(i)]);
            hash *= kFnvPrime64;
        }
    }
    return hash;
}

std::uint32_t crc32_combine(std::uint32_t seed, const std::byte* data, std::size_t size) {
    std::uint32_t c = ~seed;
    const auto& table = crc32_table();
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t b = static_cast<std::uint8_t>(data[i]);
        c = table[(c ^ b) & 0xFFu] ^ (c >> 8u);
    }
    return ~c;
}

std::uint32_t crc32(const std::byte* data, std::size_t size) {
    return crc32_combine(0u, data, size);
}

} // namespace vis
