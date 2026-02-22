#include "bvh.h"

#include <algorithm>
#include <array>
#include <numeric>

namespace vis {

namespace {

struct TriBuildData {
    Aabb bounds{};
    Vec3 centroid{};
};

struct ChildRef {
    Aabb bounds{};
    bool is_leaf = false;
    std::uint32_t index = 0;
    std::uint32_t count = 0;
};

float axis_value(const Vec3& v, int axis) {
    switch (axis) {
    case 0:
        return v.x;
    case 1:
        return v.y;
    default:
        return v.z;
    }
}

int choose_axis(const Aabb& bounds) {
    const float ex = bounds.max.x - bounds.min.x;
    const float ey = bounds.max.y - bounds.min.y;
    const float ez = bounds.max.z - bounds.min.z;
    if (ex >= ey && ex >= ez) {
        return 0;
    }
    if (ey >= ez) {
        return 1;
    }
    return 2;
}

std::vector<std::pair<std::size_t, std::size_t>> split_ranges(std::size_t begin, std::size_t end, std::size_t target_children) {
    const std::size_t count = end - begin;
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(target_children);
    for (std::size_t i = 0; i < target_children; ++i) {
        const std::size_t a = begin + (count * i) / target_children;
        const std::size_t b = begin + (count * (i + 1)) / target_children;
        if (a < b) {
            ranges.emplace_back(a, b);
        }
    }
    return ranges;
}

ChildRef build_recursive(
    std::vector<std::uint32_t>& tri_order,
    const std::vector<TriBuildData>& tri_data,
    std::size_t begin,
    std::size_t end,
    std::uint32_t leaf_size,
    std::uint32_t depth,
    std::vector<BvhNodeDisk>* nodes,
    std::vector<std::uint32_t>* primitive_order) {
    const std::size_t count = end - begin;
    Aabb bounds = make_empty_aabb();
    Aabb centroid_bounds = make_empty_aabb();
    for (std::size_t i = begin; i < end; ++i) {
        const TriBuildData& d = tri_data[tri_order[i]];
        bounds = union_aabb(bounds, d.bounds);
        centroid_bounds = grow_aabb(centroid_bounds, d.centroid);
    }

    if (count <= leaf_size || depth >= 32u) {
        const std::uint32_t start = static_cast<std::uint32_t>(primitive_order->size());
        primitive_order->insert(primitive_order->end(), tri_order.begin() + static_cast<std::ptrdiff_t>(begin), tri_order.begin() + static_cast<std::ptrdiff_t>(end));
        return {
            bounds,
            true,
            start,
            static_cast<std::uint32_t>(count),
        };
    }

    const int axis = choose_axis(centroid_bounds);
    std::sort(
        tri_order.begin() + static_cast<std::ptrdiff_t>(begin),
        tri_order.begin() + static_cast<std::ptrdiff_t>(end),
        [&](std::uint32_t lhs, std::uint32_t rhs) {
            const float lv = axis_value(tri_data[lhs].centroid, axis);
            const float rv = axis_value(tri_data[rhs].centroid, axis);
            return lv < rv;
        });

    std::size_t child_count = 4;
    if (count < static_cast<std::size_t>(leaf_size) * 3u) {
        child_count = 3;
    }
    if (count < static_cast<std::size_t>(leaf_size) * 2u) {
        child_count = 2;
    }
    child_count = std::min<std::size_t>(child_count, count);

    const std::uint32_t node_index = static_cast<std::uint32_t>(nodes->size());
    nodes->push_back({});
    BvhNodeDisk& node = nodes->back();

    const std::vector<std::pair<std::size_t, std::size_t>> ranges = split_ranges(begin, end, child_count);
    std::uint32_t out_child = 0;
    for (const auto& [cbegin, cend] : ranges) {
        ChildRef child = build_recursive(tri_order, tri_data, cbegin, cend, leaf_size, depth + 1u, nodes, primitive_order);
        BvhChild& disk = node.children[out_child];
        disk.bounds = child.bounds;
        disk.index = child.index;
        disk.count = child.count;
        disk.is_leaf = child.is_leaf ? 1u : 0u;
        ++out_child;
    }
    node.child_count = out_child;

    return {
        bounds,
        false,
        node_index,
        0u,
    };
}

} // namespace

bool build_bvh4(
    const std::vector<Vec3>& vertices,
    const std::vector<std::uint32_t>& indices,
    std::uint32_t leaf_size,
    BvhBuildResult* out_result,
    std::string* out_error) {
    if (out_result == nullptr) {
        if (out_error != nullptr) {
            *out_error = "null out_result";
        }
        return false;
    }
    if (indices.size() % 3u != 0u) {
        if (out_error != nullptr) {
            *out_error = "index count must be a multiple of 3";
        }
        return false;
    }
    const std::size_t tri_count = indices.size() / 3u;
    if (tri_count == 0u) {
        if (out_error != nullptr) {
            *out_error = "no triangles to build bvh";
        }
        return false;
    }
    if (leaf_size == 0u) {
        leaf_size = 8u;
    }

    std::vector<TriBuildData> tri_data(tri_count);
    for (std::size_t tri = 0; tri < tri_count; ++tri) {
        const std::uint32_t i0 = indices[tri * 3u + 0u];
        const std::uint32_t i1 = indices[tri * 3u + 1u];
        const std::uint32_t i2 = indices[tri * 3u + 2u];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            if (out_error != nullptr) {
                *out_error = "triangle index out of vertex range";
            }
            return false;
        }
        Aabb bounds = make_empty_aabb();
        bounds = grow_aabb(bounds, vertices[i0]);
        bounds = grow_aabb(bounds, vertices[i1]);
        bounds = grow_aabb(bounds, vertices[i2]);
        tri_data[tri].bounds = bounds;
        tri_data[tri].centroid = centroid(bounds);
    }

    std::vector<std::uint32_t> tri_order(tri_count);
    std::iota(tri_order.begin(), tri_order.end(), 0u);

    BvhBuildResult result{};
    result.nodes.reserve(tri_count / 2u);
    result.primitive_order.reserve(tri_count);

    ChildRef root = build_recursive(tri_order, tri_data, 0u, tri_order.size(), leaf_size, 0u, &result.nodes, &result.primitive_order);

    if (root.is_leaf) {
        BvhNodeDisk root_node{};
        root_node.child_count = 1u;
        root_node.children[0].bounds = root.bounds;
        root_node.children[0].index = root.index;
        root_node.children[0].count = root.count;
        root_node.children[0].is_leaf = 1u;
        result.nodes.insert(result.nodes.begin(), root_node);
    } else if (root.index != 0u) {
        std::swap(result.nodes[0], result.nodes[root.index]);
    }

    *out_result = std::move(result);
    return true;
}

bool trace_static_bvh(
    const StaticBvhView& view,
    const Ray& ray,
    float t_max,
    float epsilon,
    DebugStats* stats,
    RayHit* out_hit) {
    if (out_hit == nullptr) {
        return false;
    }
    out_hit->hit = false;
    out_hit->t = t_max;
    out_hit->tri_id = 0;

    if (view.nodes == nullptr || view.node_count == 0u || view.indices == nullptr || view.vertices == nullptr || view.primitive_order == nullptr) {
        return false;
    }

    std::array<std::uint32_t, 128> stack{};
    std::uint32_t stack_size = 0u;
    stack[stack_size++] = 0u;

    while (stack_size > 0u) {
        const std::uint32_t node_index = stack[--stack_size];
        if (node_index >= view.node_count) {
            continue;
        }
        const BvhNodeDisk& node = view.nodes[node_index];
        if (stats != nullptr) {
            stats->static_node_visits++;
        }

        struct ChildVisit {
            float t_near;
            const BvhChild* child;
        };
        std::array<ChildVisit, 4> visits{};
        std::uint32_t visit_count = 0u;
        for (std::uint32_t i = 0u; i < node.child_count && i < 4u; ++i) {
            const BvhChild& child = node.children[i];
            float t_near = 0.0f;
            if (ray_aabb_intersect(ray, child.bounds, out_hit->t, &t_near)) {
                visits[visit_count++] = {t_near, &child};
            }
        }
        std::sort(
            visits.begin(),
            visits.begin() + static_cast<std::ptrdiff_t>(visit_count),
            [](const ChildVisit& a, const ChildVisit& b) { return a.t_near < b.t_near; });

        for (std::uint32_t i = 0u; i < visit_count; ++i) {
            const BvhChild& child = *visits[i].child;
            if (child.is_leaf != 0u) {
                const std::size_t begin = child.index;
                const std::size_t end = begin + child.count;
                if (end > view.primitive_count) {
                    continue;
                }
                for (std::size_t p = begin; p < end; ++p) {
                    const std::uint32_t tri_id = view.primitive_order[p];
                    const std::size_t base = static_cast<std::size_t>(tri_id) * 3u;
                    if (base + 2u >= view.index_count) {
                        continue;
                    }
                    const std::uint32_t i0 = view.indices[base + 0u];
                    const std::uint32_t i1 = view.indices[base + 1u];
                    const std::uint32_t i2 = view.indices[base + 2u];
                    if (i0 >= view.vertex_count || i1 >= view.vertex_count || i2 >= view.vertex_count) {
                        continue;
                    }
                    if (stats != nullptr) {
                        stats->static_tri_tests++;
                    }
                    float hit_t = 0.0f;
                    if (ray_triangle_intersect(ray, view.vertices[i0], view.vertices[i1], view.vertices[i2], out_hit->t, epsilon, &hit_t)) {
                        out_hit->hit = true;
                        out_hit->t = hit_t;
                        out_hit->tri_id = tri_id;
                    }
                }
            } else {
                if (stack_size < stack.size()) {
                    stack[stack_size++] = child.index;
                }
            }
        }
    }

    return out_hit->hit;
}

} // namespace vis
