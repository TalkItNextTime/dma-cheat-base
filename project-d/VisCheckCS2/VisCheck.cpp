#include "VisCheck.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>

namespace
{
    constexpr size_t LEAF_THRESHOLD = 32;

    inline float TriangleCentroidByAxis(const TriangleCombined& tri, int axis)
    {
        if (axis == 0)
            return (tri.v0.x + tri.v1.x + tri.v2.x) * (1.0f / 3.0f);
        if (axis == 1)
            return (tri.v0.y + tri.v1.y + tri.v2.y) * (1.0f / 3.0f);
        return (tri.v0.z + tri.v1.z + tri.v2.z) * (1.0f / 3.0f);
    }
}

VisCheck::VisCheck(const std::string& optimizedGeometryFile) {
    if (!geometry.LoadFromFile(optimizedGeometryFile)) {
        std::cerr << "Failed to load optimized file: " << optimizedGeometryFile << std::endl;
        ready = false;
        return;
    }

    bvhNodes.reserve(geometry.meshes.size());
    for (auto& mesh : geometry.meshes)
    {
        if (mesh.empty())
        {
            bvhNodes.push_back(nullptr);
            continue;
        }

        auto root = BuildBVH(mesh, 0, mesh.size());
        bvhNodes.push_back(std::move(root));
    }

    ready = !bvhNodes.empty();
}

bool VisCheck::IsReady() const
{
    return ready;
}

std::unique_ptr<BVHNode> VisCheck::BuildBVH(std::vector<TriangleCombined>& tris, size_t begin, size_t end) {
    auto node = std::make_unique<BVHNode>();

    if (begin >= end)
        return node;

    AABB bounds = tris[begin].ComputeAABB();
    for (size_t i = begin + 1; i < end; ++i) {
        AABB triAABB = tris[i].ComputeAABB();
        bounds.min.x = (std::min)(bounds.min.x, triAABB.min.x);
        bounds.min.y = (std::min)(bounds.min.y, triAABB.min.y);
        bounds.min.z = (std::min)(bounds.min.z, triAABB.min.z);
        bounds.max.x = (std::max)(bounds.max.x, triAABB.max.x);
        bounds.max.y = (std::max)(bounds.max.y, triAABB.max.y);
        bounds.max.z = (std::max)(bounds.max.z, triAABB.max.z);
    }
    node->bounds = bounds;

    const size_t count = end - begin;
    if (count <= LEAF_THRESHOLD) {
        node->triangles = &tris;
        node->begin = begin;
        node->end = end;
        return node;
    }

    Vector3 diff = VectorSub(bounds.max, bounds.min);
    int axis = (diff.x > diff.y && diff.x > diff.z) ? 0 : ((diff.y > diff.z) ? 1 : 2);

    auto beginIt = tris.begin() + static_cast<long long>(begin);
    auto endIt = tris.begin() + static_cast<long long>(end);
    size_t mid = begin + (count / 2);
    auto midIt = tris.begin() + static_cast<long long>(mid);

    std::nth_element(beginIt, midIt, endIt, [axis](const TriangleCombined& a, const TriangleCombined& b) {
        return TriangleCentroidByAxis(a, axis) < TriangleCentroidByAxis(b, axis);
    });

    node->left = BuildBVH(tris, begin, mid);
    node->right = BuildBVH(tris, mid, end);

    return node;
}

bool VisCheck::IntersectBVH(const BVHNode* node, const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance) {
    if (!node)
        return false;

    if (!node->bounds.RayIntersects(rayOrigin, rayDir)) {
        return false;
    }

    bool hit = false;
    if (node->IsLeaf()) {
        if (!node->triangles)
            return false;

        const auto& mesh = *node->triangles;
        for (size_t i = node->begin; i < node->end; ++i) {
            const auto& tri = mesh[i];
            float t;
            if (RayIntersectsTriangle(rayOrigin, rayDir, tri, t)) {
                if (t < maxDistance && t < hitDistance) {
                    hitDistance = t;
                    hit = true;
                }
            }
        }
    }
    else {
        if (node->left) {
            hit |= IntersectBVH(node->left.get(), rayOrigin, rayDir, maxDistance, hitDistance);
        }
        if (node->right) {
            hit |= IntersectBVH(node->right.get(), rayOrigin, rayDir, maxDistance, hitDistance);
        }
    }
    return hit;
}

bool VisCheck::IsPointVisible(const Vector3& point1, const Vector3& point2)
{
    if (!ready)
        return true;

    Vector3 rayDelta = { point2.x - point1.x, point2.y - point1.y, point2.z - point1.z };
    float distance = std::sqrt(VectorDot(rayDelta, rayDelta));
    if (distance <= 1e-4f)
        return true;

    Vector3 rayDir = { rayDelta.x / distance, rayDelta.y / distance, rayDelta.z / distance };

    float hitDistance = std::numeric_limits<float>::max();
    for (const auto& bvhRoot : bvhNodes) {
        if (!bvhRoot)
            continue;

        if (IntersectBVH(bvhRoot.get(), point1, rayDir, distance, hitDistance)) {
            if (hitDistance < distance) {
                return false;
            }
        }
    }

    return true;
}

bool VisCheck::RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir, const TriangleCombined& triangle, float& t)
{
    const float EPSILON = 1e-7f;

    Vector3 edge1 = VectorSub(triangle.v1, triangle.v0);
    Vector3 edge2 = VectorSub(triangle.v2, triangle.v0);
    Vector3 h = VectorCross(rayDir, edge2);
    float a = VectorDot(edge1, h);

    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    Vector3 s = VectorSub(rayOrigin, triangle.v0);
    float u = f * VectorDot(s, h);

    if (u < 0.0f || u > 1.0f)
        return false;

    Vector3 q = VectorCross(s, edge1);
    float v = f * VectorDot(rayDir, q);

    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * VectorDot(edge2, q);

    return (t > EPSILON);
}
