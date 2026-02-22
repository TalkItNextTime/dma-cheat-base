#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

#include "vphys_parser.h"

namespace vis {

struct CompileOptions {
    // Empty set means allow all indices, then rely on semantic filtering from m_collisionAttributes.
    std::unordered_set<std::uint32_t> allowed_collision_indices = {};
    std::uint32_t bvh_leaf_size = 8u;
};

struct CompileReport {
    std::string map_name;
    std::string source_vphys;
    std::string output_cache;
    std::uint64_t source_hash = 0;
    std::uint64_t elapsed_ms = 0;
    VphysParseStats parse_stats{};
    std::uint32_t cache_triangles = 0;
    std::uint32_t cache_vertices = 0;
    std::uint32_t cache_bvh_nodes = 0;
    std::uint32_t cache_attr_mask = 0;
};

bool compile_vphys_to_cache(
    const std::string& vphys_path,
    const std::string& output_cache_path,
    const std::string& map_name,
    const CompileOptions& options,
    CompileReport* out_report,
    std::string* out_error);

} // namespace vis

