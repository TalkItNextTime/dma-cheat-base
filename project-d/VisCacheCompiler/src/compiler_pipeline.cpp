#include "compiler_pipeline.h"

#include <chrono>
#include <filesystem>

#include "bvh.h"
#include "cache_io.h"
#include "hash.h"

namespace vis {

bool compile_vphys_to_cache(
    const std::string& vphys_path,
    const std::string& output_cache_path,
    const std::string& map_name,
    const CompileOptions& options,
    CompileReport* out_report,
    std::string* out_error) {
    if (out_report == nullptr) {
        if (out_error != nullptr) {
            *out_error = "out_report is null";
        }
        return false;
    }
    *out_report = {};
    out_report->source_vphys = vphys_path;
    out_report->output_cache = output_cache_path;

    const auto t0 = std::chrono::steady_clock::now();

    VphysParseResult parse_result{};
    if (!parse_vphys_file(vphys_path, options.allowed_collision_indices, &parse_result, out_error)) {
        return false;
    }

    BvhBuildResult bvh{};
    std::string bvh_error;
    if (!build_bvh4(parse_result.mesh.vertices, parse_result.mesh.indices, options.bvh_leaf_size, &bvh, &bvh_error)) {
        if (out_error != nullptr) {
            *out_error = "bvh build failed: " + bvh_error;
        }
        return false;
    }

    CompiledMapData map{};
    map.map_name = map_name;
    map.source_hash = fnv1a64_file(vphys_path);
    map.attr_mask = parse_result.stats.accepted_attr_mask;
    map.vertices = std::move(parse_result.mesh.vertices);
    map.indices = std::move(parse_result.mesh.indices);
    map.triangle_kinds = std::move(parse_result.triangle_sources);
    map.triangle_material_hashes = std::move(parse_result.triangle_material_hashes);
    map.bvh_nodes = std::move(bvh.nodes);
    map.bvh_primitive_order = std::move(bvh.primitive_order);

    if (!write_cache_file(output_cache_path, map, out_error)) {
        return false;
    }

    const auto t1 = std::chrono::steady_clock::now();
    out_report->map_name = map_name;
    out_report->source_hash = map.source_hash;
    out_report->parse_stats = parse_result.stats;
    out_report->cache_triangles = static_cast<std::uint32_t>(map.indices.size() / 3u);
    out_report->cache_vertices = static_cast<std::uint32_t>(map.vertices.size());
    out_report->cache_bvh_nodes = static_cast<std::uint32_t>(map.bvh_nodes.size());
    out_report->cache_attr_mask = map.attr_mask;
    out_report->elapsed_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    return true;
}

} // namespace vis
