#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cache_format.h"
#include "math_util.h"

namespace vis {

struct BvhBuildResult {
    std::vector<BvhNodeDisk> nodes;
    std::vector<std::uint32_t> primitive_order;
};

bool build_bvh4(
    const std::vector<Vec3>& vertices,
    const std::vector<std::uint32_t>& indices,
    std::uint32_t leaf_size,
    BvhBuildResult* out_result,
    std::string* out_error);

struct StaticBvhView {
    const Vec3* vertices = nullptr;
    std::size_t vertex_count = 0;
    const std::uint32_t* indices = nullptr;
    std::size_t index_count = 0;
    const BvhNodeDisk* nodes = nullptr;
    std::size_t node_count = 0;
    const std::uint32_t* primitive_order = nullptr;
    std::size_t primitive_count = 0;
};

struct RayHit {
    bool hit = false;
    float t = 0.0f;
    std::uint32_t tri_id = 0;
};

bool trace_static_bvh(
    const StaticBvhView& view,
    const Ray& ray,
    float t_max,
    float epsilon,
    DebugStats* stats,
    RayHit* out_hit);

} // namespace vis
