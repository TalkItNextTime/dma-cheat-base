#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "cache_format.h"

namespace vis {

struct VphysParseStats {
    std::uint64_t mesh_blocks_seen = 0;
    std::uint64_t mesh_blocks_accepted = 0;
    std::uint64_t triangles_emitted = 0;
    std::uint64_t triangles_emitted_from_mesh = 0;
    std::uint64_t triangles_emitted_from_hull = 0;
    std::uint64_t triangles_skipped_invalid = 0;
    std::uint64_t triangles_skipped_degenerate = 0;
    std::uint32_t accepted_attr_mask = 0;
};

enum class ParsedTriangleSource : std::uint8_t {
    Unknown = 0,
    Mesh = 1,
    Hull = 2,
};

struct VphysParseResult {
    IndexedMesh mesh;
    std::vector<std::uint8_t> triangle_sources;
    VphysParseStats stats;
};

bool parse_vphys_file(
    const std::string& vphys_path,
    const std::unordered_set<std::uint32_t>& allowed_collision_indices,
    VphysParseResult* out_result,
    std::string* out_error);

} // namespace vis
