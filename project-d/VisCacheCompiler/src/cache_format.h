#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vischeck/vis_types.h"

namespace vis {

constexpr std::array<char, 8> kCacheMagic = {'V', 'M', 'A', 'P', 'C', 'C', 'H', '1'};
constexpr std::uint32_t kCacheVersion = 1u;

enum class CacheSectionId : std::uint32_t {
    Vertices = 1,
    Indices = 2,
    BvhNodes = 3,
    BvhPrimitiveOrder = 4,
    Metadata = 5,
    TriangleMaterialHashes = 6,
};

struct CacheHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t section_count;
    std::uint64_t source_hash;
    std::uint64_t build_unix_seconds;
    std::uint32_t static_tri_count;
    std::uint32_t bvh_node_count;
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    std::uint32_t attr_mask;
    std::uint32_t payload_crc32;
    char map_name[64];
};

struct CacheSectionEntry {
    std::uint32_t id;
    std::uint32_t reserved;
    std::uint64_t offset;
    std::uint64_t size;
};

struct BvhChild {
    Aabb bounds;
    std::uint32_t index = 0; // child node index or primitive start
    std::uint32_t count = 0; // primitive count if leaf, zero if internal
    std::uint8_t is_leaf = 0;
    std::uint8_t reserved0 = 0;
    std::uint16_t reserved1 = 0;
};

struct BvhNodeDisk {
    std::uint32_t child_count = 0;
    BvhChild children[4];
};

struct IndexedMesh {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
};

struct CompiledMapData {
    std::string map_name;
    std::uint64_t source_hash = 0;
    std::uint32_t attr_mask = 0;
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint8_t> triangle_kinds; // Per-triangle source kind (mesh/hull), indexed by triangle id.
    std::vector<std::uint32_t> triangle_material_hashes; // Per-triangle material hash, indexed by triangle id.
    std::vector<BvhNodeDisk> bvh_nodes;
    std::vector<std::uint32_t> bvh_primitive_order;
};

} // namespace vis
