#pragma once

#include <cstdint>
#include <string>

namespace vis {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Aabb {
    Vec3 min;
    Vec3 max;
};

struct Mat3x4 {
    float m[3][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
};

enum class ObstacleKind : std::uint32_t {
    Door = 1,
    Breakable = 2,
    Generic = 3,
};

enum class SampleProfile : std::uint32_t {
    SingleRay = 0,
    HeadChestPelvis = 1,
};

enum class HitLayer : std::uint32_t {
    None = 0,
    StaticMap = 1,
    DynamicObstacle = 2,
};

struct DynamicObstacle {
    std::uint64_t id = 0;
    ObstacleKind kind = ObstacleKind::Generic;
    Mat3x4 transform{};
    Aabb local_bounds{};
    bool enabled = true;
    std::uint64_t revision = 0;
};

struct RayQuery {
    Vec3 start{};
    Vec3 end{};
    std::uint32_t mask = 0xFFFFFFFFu;
    SampleProfile sample_profile = SampleProfile::HeadChestPelvis;
    float max_t_override = 0.0f;
};

struct DebugStats {
    std::uint32_t static_node_visits = 0;
    std::uint32_t static_tri_tests = 0;
    std::uint32_t dynamic_node_visits = 0;
    std::uint32_t dynamic_leaf_tests = 0;
};

struct VisResult {
    bool visible = false;
    float first_hit_t = 0.0f;
    HitLayer hit_layer = HitLayer::None;
    std::uint64_t hit_id = 0;
    DebugStats debug_stats{};
};

struct VisEngineConfig {
    std::string cache_root = ".\\cache";
    std::uint32_t worker_threads = 0;
    float epsilon = 1.5f;
    SampleProfile default_profile = SampleProfile::HeadChestPelvis;
    std::uint32_t dynamic_hz = 128;
    std::uint32_t occluder_mask = 0xFFFFFFFFu;
    bool debug_trace = false;
};

struct MapInfo {
    std::string map_name;
    std::uint32_t cache_version = 0;
    std::uint32_t static_tri_count = 0;
    bool loaded = false;
};

} // namespace vis
