#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Math.hpp"

class VisCheck
{
public:
    VisCheck(const std::string& cacheFilePath);
    bool IsPointVisible(const Vector3& point1, const Vector3& point2);
    bool IsReady() const;
    bool RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir, const TriangleCombined& triangle, float& t) const;
    const std::vector<TriangleCombined>& GetDebugTriangles() const;
    const std::vector<AABB>& GetDebugOccluderBounds() const;

private:
    struct CacheBvhChild
    {
        AABB bounds{};
        std::uint32_t index = 0;
        std::uint32_t count = 0;
        bool isLeaf = false;
    };

    struct CacheBvhNode
    {
        std::uint32_t childCount = 0;
        std::array<CacheBvhChild, 4> children{};
    };

private:
    bool ready = false;
    std::vector<Vector3> cacheVertices;
    std::vector<std::uint32_t> cacheIndices;
    std::vector<std::uint8_t> cacheTriangleKinds;
    std::vector<CacheBvhNode> cacheNodes;
    std::vector<std::uint32_t> cachePrimitiveOrder;
    std::vector<TriangleCombined> debugTriangles;
    std::vector<AABB> debugOccluderBounds;

    bool LoadCacheGeometry(const std::string& cacheFilePath);
    bool IntersectCacheBvh(const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance) const;
    void BuildCacheDebugGeometry();
};
