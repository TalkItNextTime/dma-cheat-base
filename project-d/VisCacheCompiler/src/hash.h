#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vis {

std::uint64_t fnv1a64(const void* data, std::size_t size);
std::uint64_t fnv1a64(const std::vector<std::byte>& data);
std::uint64_t fnv1a64_file(const std::string& path);

std::uint32_t crc32(const std::byte* data, std::size_t size);
std::uint32_t crc32_combine(std::uint32_t seed, const std::byte* data, std::size_t size);

} // namespace vis
