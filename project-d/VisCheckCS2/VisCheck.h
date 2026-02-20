#pragma once
#include <memory>
#include <vector>
#include "OptimizedGeometry.h"
#include "Math.hpp"

struct BVHNode {
    AABB bounds;
    std::unique_ptr<BVHNode> left;
    std::unique_ptr<BVHNode> right;
    const std::vector<TriangleCombined>* triangles = nullptr;
    size_t begin = 0;
    size_t end = 0;

    bool IsLeaf() const {
        return left == nullptr && right == nullptr;
    }
};

class VisCheck
{
public:
    VisCheck(const std::string& optimizedGeometryFile);
    bool IsPointVisible(const Vector3& point1, const Vector3& point2);
    bool IsReady() const;
    bool RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir,
        const TriangleCombined& triangle, float& t);

private:
    OptimizedGeometry geometry;
    bool ready = false;
    std::vector<std::unique_ptr<BVHNode>> bvhNodes;
    std::unique_ptr<BVHNode> BuildBVH(std::vector<TriangleCombined>& tris, size_t begin, size_t end);
    bool IntersectBVH(const BVHNode* node, const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance);
};
