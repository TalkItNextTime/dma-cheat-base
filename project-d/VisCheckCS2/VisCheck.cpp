#include "VisCheck.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <utility>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr char kCacheMagic[8] = { 'V', 'M', 'A', 'P', 'C', 'C', 'H', '1' };
    constexpr std::uint32_t kCacheVersion = 1u;
    constexpr float kRayEpsilon = 1e-6f;

    enum class CacheSectionId : std::uint32_t
    {
        Vertices = 1,
        Indices = 2,
        BvhNodes = 3,
        BvhPrimitiveOrder = 4,
        TriangleKinds = 5,
        TriangleMaterialHashes = 6
    };

    struct CacheHeader
    {
        char magic[8];
        std::uint32_t version = 0;
        std::uint32_t section_count = 0;
        std::uint64_t source_hash = 0;
        std::uint64_t build_unix_seconds = 0;
        std::uint32_t static_tri_count = 0;
        std::uint32_t bvh_node_count = 0;
        std::uint32_t vertex_count = 0;
        std::uint32_t index_count = 0;
        std::uint32_t attr_mask = 0;
        std::uint32_t payload_crc32 = 0;
        char map_name[64]{};
    };

    struct CacheSectionEntry
    {
        std::uint32_t id = 0;
        std::uint32_t reserved = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    struct CacheBvhChildDisk
    {
        AABB bounds{};
        std::uint32_t index = 0;
        std::uint32_t count = 0;
        std::uint8_t is_leaf = 0;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;
    };

    struct CacheBvhNodeDisk
    {
        std::uint32_t child_count = 0;
        CacheBvhChildDisk children[4]{};
    };

    struct RayCache
    {
        Vector3 origin{};
        Vector3 dir{};
        Vector3 invDir{};
    };

    template <typename T>
    bool ReadPod(const std::vector<std::uint8_t>& bytes, const std::size_t offset, T& out)
    {
        if (offset > bytes.size() || sizeof(T) > (bytes.size() - offset))
            return false;
        std::memcpy(&out, bytes.data() + offset, sizeof(T));
        return true;
    }

    bool IsRangeValid(const std::uint64_t offset, const std::uint64_t size, const std::size_t totalSize)
    {
        if (offset > static_cast<std::uint64_t>(totalSize) || size > static_cast<std::uint64_t>(totalSize))
            return false;
        if (offset + size < offset)
            return false;
        return static_cast<std::size_t>(offset + size) <= totalSize;
    }

    template <typename T>
    bool CopySectionToVector(
        const std::vector<std::uint8_t>& bytes,
        const CacheSectionEntry& section,
        std::vector<T>& out)
    {
        if (section.size % sizeof(T) != 0)
            return false;
        if (!IsRangeValid(section.offset, section.size, bytes.size()))
            return false;

        const std::size_t count = static_cast<std::size_t>(section.size / sizeof(T));
        out.clear();
        out.resize(count);
        if (count == 0)
            return true;

        std::memcpy(
            out.data(),
            bytes.data() + static_cast<std::size_t>(section.offset),
            static_cast<std::size_t>(section.size));
        return true;
    }

    RayCache MakeRay(const Vector3& origin, const Vector3& dir)
    {
        auto safeInv = [](const float component)
        {
            constexpr float minAbs = 1e-8f;
            const float safe = std::fabs(component) < minAbs
                ? (component >= 0.0f ? minAbs : -minAbs)
                : component;
            return 1.0f / safe;
        };

        return {
            origin,
            dir,
            { safeInv(dir.x), safeInv(dir.y), safeInv(dir.z) }
        };
    }

    bool RayIntersectsAabb(const RayCache& ray, const AABB& aabb, const float maxDistance, float* outNear)
    {
        float tMin = 0.0f;
        float tMax = maxDistance;

        const float* origin = &ray.origin.x;
        const float* invDir = &ray.invDir.x;
        const float* boundsMin = &aabb.min.x;
        const float* boundsMax = &aabb.max.x;

        for (int axis = 0; axis < 3; ++axis)
        {
            float t1 = (boundsMin[axis] - origin[axis]) * invDir[axis];
            float t2 = (boundsMax[axis] - origin[axis]) * invDir[axis];
            if (t1 > t2)
                std::swap(t1, t2);

            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMax < tMin)
                return false;
        }

        if (outNear)
            *outNear = tMin;
        return tMax >= 0.0f;
    }
}

VisCheck::VisCheck(const std::string& cacheFilePath)
{
    ready = LoadCacheGeometry(cacheFilePath);
    if (!ready)
        std::cerr << "Failed to load cache map file: " << cacheFilePath << std::endl;
}

bool VisCheck::LoadCacheGeometry(const std::string& cacheFilePath)
{
    ready = false;
    cacheVertices.clear();
    cacheIndices.clear();
    cacheTriangleKinds.clear();
    cacheTriangleMaterialHashes.clear();
    hasMaterialSection = false;
    cacheNodes.clear();
    cachePrimitiveOrder.clear();
    debugTriangles.clear();
    debugOccluderBounds.clear();

    std::ifstream in(cacheFilePath, std::ios::binary);
    if (!in)
        return false;

    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()
    };

    if (bytes.size() < sizeof(CacheHeader))
        return false;

    CacheHeader header{};
    if (!ReadPod(bytes, 0, header))
        return false;

    if (std::memcmp(header.magic, kCacheMagic, sizeof(kCacheMagic)) != 0)
        return false;
    if (header.version != kCacheVersion)
        return false;
    if (header.section_count == 0)
        return false;

    const std::size_t sectionTableOffset = sizeof(CacheHeader);
    const std::size_t sectionTableBytes = static_cast<std::size_t>(header.section_count) * sizeof(CacheSectionEntry);
    if (sectionTableOffset > bytes.size() || sectionTableBytes > (bytes.size() - sectionTableOffset))
        return false;

    std::unordered_map<std::uint32_t, CacheSectionEntry> sectionById{};
    sectionById.reserve(header.section_count);
    for (std::uint32_t i = 0; i < header.section_count; ++i)
    {
        CacheSectionEntry section{};
        const std::size_t offset = sectionTableOffset + static_cast<std::size_t>(i) * sizeof(CacheSectionEntry);
        if (!ReadPod(bytes, offset, section))
            return false;
        sectionById[section.id] = section;
    }

    const auto findSection = [&](const CacheSectionId id) -> const CacheSectionEntry*
    {
        const auto it = sectionById.find(static_cast<std::uint32_t>(id));
        return it == sectionById.end() ? nullptr : &it->second;
    };

    const CacheSectionEntry* verticesSection = findSection(CacheSectionId::Vertices);
    const CacheSectionEntry* indicesSection = findSection(CacheSectionId::Indices);
    const CacheSectionEntry* nodesSection = findSection(CacheSectionId::BvhNodes);
    const CacheSectionEntry* orderSection = findSection(CacheSectionId::BvhPrimitiveOrder);
    const CacheSectionEntry* kindsSection = findSection(CacheSectionId::TriangleKinds);
    const CacheSectionEntry* materialSection = findSection(CacheSectionId::TriangleMaterialHashes);
    if (!verticesSection || !indicesSection || !nodesSection || !orderSection)
        return false;

    if (!CopySectionToVector(bytes, *verticesSection, cacheVertices))
        return false;
    if (!CopySectionToVector(bytes, *indicesSection, cacheIndices))
        return false;
    if (!CopySectionToVector(bytes, *orderSection, cachePrimitiveOrder))
        return false;
    if (kindsSection != nullptr)
    {
        if (!CopySectionToVector(bytes, *kindsSection, cacheTriangleKinds))
            return false;
    }
    else
    {
        cacheTriangleKinds.clear();
    }

    if (materialSection != nullptr)
    {
        if (!CopySectionToVector(bytes, *materialSection, cacheTriangleMaterialHashes))
            return false;
        hasMaterialSection = true;
    }
    else
    {
        cacheTriangleMaterialHashes.clear();
        hasMaterialSection = false;
    }

    std::vector<CacheBvhNodeDisk> diskNodes{};
    if (!CopySectionToVector(bytes, *nodesSection, diskNodes))
        return false;

    if (cacheVertices.empty() || cacheIndices.empty() || diskNodes.empty() || cachePrimitiveOrder.empty())
        return false;
    if (cacheIndices.size() % 3 != 0)
        return false;

    cacheNodes.clear();
    cacheNodes.resize(diskNodes.size());
    for (std::size_t nodeIndex = 0; nodeIndex < diskNodes.size(); ++nodeIndex)
    {
        const CacheBvhNodeDisk& diskNode = diskNodes[nodeIndex];
        CacheBvhNode& runtimeNode = cacheNodes[nodeIndex];
        runtimeNode.childCount = (std::min)(diskNode.child_count, 4u);
        for (std::uint32_t childIndex = 0; childIndex < runtimeNode.childCount; ++childIndex)
        {
            const CacheBvhChildDisk& diskChild = diskNode.children[childIndex];
            CacheBvhChild& runtimeChild = runtimeNode.children[childIndex];
            runtimeChild.bounds = diskChild.bounds;
            runtimeChild.index = diskChild.index;
            runtimeChild.count = diskChild.count;
            runtimeChild.isLeaf = diskChild.is_leaf != 0;
        }
    }

    const std::size_t triCount = cacheIndices.size() / 3;
    if (!cacheTriangleKinds.empty() && cacheTriangleKinds.size() != triCount)
        return false;
    if (!cacheTriangleMaterialHashes.empty() && cacheTriangleMaterialHashes.size() != triCount)
        return false;
    if (hasMaterialSection && cacheTriangleMaterialHashes.empty() && triCount > 0)
        return false;

    for (const std::uint32_t triId : cachePrimitiveOrder)
    {
        if (triId >= triCount)
            return false;
    }

    for (const CacheBvhNode& node : cacheNodes)
    {
        for (std::uint32_t childIndex = 0; childIndex < node.childCount; ++childIndex)
        {
            const CacheBvhChild& child = node.children[childIndex];
            if (child.isLeaf)
            {
                const std::size_t begin = child.index;
                const std::size_t count = child.count;
                if (begin > cachePrimitiveOrder.size() || count > (cachePrimitiveOrder.size() - begin))
                    return false;
            }
            else
            {
                if (child.index >= cacheNodes.size())
                    return false;
            }
        }
    }

    BuildCacheDebugGeometry();
    ready = true;
    return true;
}

bool VisCheck::IsReady() const
{
    return ready;
}

bool VisCheck::HasPenetrationMaterialData() const
{
    return ready && hasMaterialSection && !cacheTriangleMaterialHashes.empty();
}

void VisCheck::BuildCacheDebugGeometry()
{
    debugTriangles.clear();
    debugOccluderBounds.clear();

    if (cacheVertices.empty() || cacheIndices.empty() || cachePrimitiveOrder.empty() || cacheNodes.empty())
        return;

    debugTriangles.reserve(cachePrimitiveOrder.size());
    for (std::size_t i = 0; i < cachePrimitiveOrder.size(); ++i)
    {
        const std::uint32_t triId = cachePrimitiveOrder[i];
        const std::size_t triBase = static_cast<std::size_t>(triId) * 3;
        if (triBase + 2 >= cacheIndices.size())
            continue;

        const std::uint32_t i0 = cacheIndices[triBase];
        const std::uint32_t i1 = cacheIndices[triBase + 1];
        const std::uint32_t i2 = cacheIndices[triBase + 2];
        if (i0 >= cacheVertices.size() || i1 >= cacheVertices.size() || i2 >= cacheVertices.size())
            continue;

        std::uint8_t sourceKind = 0;
        if (triId < cacheTriangleKinds.size())
            sourceKind = cacheTriangleKinds[triId];

        debugTriangles.emplace_back(cacheVertices[i0], cacheVertices[i1], cacheVertices[i2], sourceKind);
    }

    debugOccluderBounds.reserve(cacheNodes.size() * 2u);
    for (const CacheBvhNode& node : cacheNodes)
    {
        for (std::uint32_t childIndex = 0; childIndex < node.childCount; ++childIndex)
        {
            const CacheBvhChild& child = node.children[childIndex];
            if (!child.isLeaf)
                continue;

            debugOccluderBounds.push_back(child.bounds);
        }
    }
}

const std::vector<TriangleCombined>& VisCheck::GetDebugTriangles() const
{
    return debugTriangles;
}

const std::vector<AABB>& VisCheck::GetDebugOccluderBounds() const
{
    return debugOccluderBounds;
}

bool VisCheck::IntersectCacheBvh(
    const Vector3& rayOrigin,
    const Vector3& rayDir,
    const float maxDistance,
    float& hitDistance) const
{
    if (!ready || cacheNodes.empty() || cacheIndices.empty() || cacheVertices.empty() || cachePrimitiveOrder.empty())
        return false;

    const RayCache ray = MakeRay(rayOrigin, rayDir);
    bool hit = false;
    hitDistance = (std::min)(hitDistance, maxDistance);

    std::vector<std::uint32_t> stack{};
    stack.reserve(256);
    stack.push_back(0);

    while (!stack.empty())
    {
        const std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if (nodeIndex >= cacheNodes.size())
            continue;

        const CacheBvhNode& node = cacheNodes[nodeIndex];

        struct ChildVisit
        {
            float tNear = 0.0f;
            const CacheBvhChild* child = nullptr;
        };

        std::array<ChildVisit, 4> visits{};
        std::uint32_t visitCount = 0;

        for (std::uint32_t i = 0; i < node.childCount && i < 4; ++i)
        {
            const CacheBvhChild& child = node.children[i];
            float tNear = 0.0f;
            if (RayIntersectsAabb(ray, child.bounds, hitDistance, &tNear))
            {
                visits[visitCount++] = { tNear, &child };
            }
        }

        std::sort(
            visits.begin(),
            visits.begin() + static_cast<std::ptrdiff_t>(visitCount),
            [](const ChildVisit& lhs, const ChildVisit& rhs)
            {
                return lhs.tNear < rhs.tNear;
            });

        for (std::uint32_t i = 0; i < visitCount; ++i)
        {
            const CacheBvhChild* child = visits[i].child;
            if (!child)
                continue;

            if (child->isLeaf)
            {
                const std::size_t begin = child->index;
                const std::size_t end = begin + child->count;
                if (begin > cachePrimitiveOrder.size() || end > cachePrimitiveOrder.size())
                    continue;

                for (std::size_t primitive = begin; primitive < end; ++primitive)
                {
                    const std::uint32_t triId = cachePrimitiveOrder[primitive];
                    const std::size_t triBase = static_cast<std::size_t>(triId) * 3;
                    if (triBase + 2 >= cacheIndices.size())
                        continue;

                    const std::uint32_t i0 = cacheIndices[triBase];
                    const std::uint32_t i1 = cacheIndices[triBase + 1];
                    const std::uint32_t i2 = cacheIndices[triBase + 2];
                    if (i0 >= cacheVertices.size() || i1 >= cacheVertices.size() || i2 >= cacheVertices.size())
                        continue;

                    const TriangleCombined tri{ cacheVertices[i0], cacheVertices[i1], cacheVertices[i2] };
                    float t = 0.0f;
                    if (!RayIntersectsTriangle(rayOrigin, rayDir, tri, t))
                        continue;

                    if (t > kRayEpsilon && t < hitDistance && t <= maxDistance)
                    {
                        hitDistance = t;
                        hit = true;
                    }
                }
            }
            else
            {
                if (child->index < cacheNodes.size())
                    stack.push_back(child->index);
            }
        }
    }

    return hit;
}

void VisCheck::CollectCacheIntersections(
    const Vector3& rayOrigin,
    const Vector3& rayDir,
    const float maxDistance,
    std::vector<std::pair<float, std::uint32_t>>& outIntersections) const
{
    outIntersections.clear();
    if (!ready || cacheNodes.empty() || cacheIndices.empty() || cacheVertices.empty() || cachePrimitiveOrder.empty())
        return;

    const RayCache ray = MakeRay(rayOrigin, rayDir);
    std::vector<std::uint32_t> stack{};
    stack.reserve(256);
    stack.push_back(0);

    while (!stack.empty())
    {
        const std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if (nodeIndex >= cacheNodes.size())
            continue;

        const CacheBvhNode& node = cacheNodes[nodeIndex];

        struct ChildVisit
        {
            float tNear = 0.0f;
            const CacheBvhChild* child = nullptr;
        };

        std::array<ChildVisit, 4> visits{};
        std::uint32_t visitCount = 0;

        for (std::uint32_t i = 0; i < node.childCount && i < 4; ++i)
        {
            const CacheBvhChild& child = node.children[i];
            float tNear = 0.0f;
            if (RayIntersectsAabb(ray, child.bounds, maxDistance, &tNear))
                visits[visitCount++] = { tNear, &child };
        }

        std::sort(
            visits.begin(),
            visits.begin() + static_cast<std::ptrdiff_t>(visitCount),
            [](const ChildVisit& lhs, const ChildVisit& rhs)
            {
                return lhs.tNear < rhs.tNear;
            });

        for (std::uint32_t i = 0; i < visitCount; ++i)
        {
            const CacheBvhChild* child = visits[i].child;
            if (!child)
                continue;

            if (child->isLeaf)
            {
                const std::size_t begin = child->index;
                const std::size_t end = begin + child->count;
                if (begin > cachePrimitiveOrder.size() || end > cachePrimitiveOrder.size())
                    continue;

                for (std::size_t primitive = begin; primitive < end; ++primitive)
                {
                    const std::uint32_t triId = cachePrimitiveOrder[primitive];
                    const std::size_t triBase = static_cast<std::size_t>(triId) * 3;
                    if (triBase + 2 >= cacheIndices.size())
                        continue;

                    const std::uint32_t i0 = cacheIndices[triBase];
                    const std::uint32_t i1 = cacheIndices[triBase + 1];
                    const std::uint32_t i2 = cacheIndices[triBase + 2];
                    if (i0 >= cacheVertices.size() || i1 >= cacheVertices.size() || i2 >= cacheVertices.size())
                        continue;

                    const TriangleCombined tri{ cacheVertices[i0], cacheVertices[i1], cacheVertices[i2] };
                    float t = 0.0f;
                    if (!RayIntersectsTriangle(rayOrigin, rayDir, tri, t))
                        continue;

                    if (t > kRayEpsilon && t <= maxDistance)
                        outIntersections.emplace_back(t, triId);
                }
            }
            else if (child->index < cacheNodes.size())
            {
                stack.push_back(child->index);
            }
        }
    }

    if (outIntersections.empty())
        return;

    std::sort(
        outIntersections.begin(),
        outIntersections.end(),
        [](const std::pair<float, std::uint32_t>& lhs, const std::pair<float, std::uint32_t>& rhs)
        {
            if (lhs.first == rhs.first)
                return lhs.second < rhs.second;
            return lhs.first < rhs.first;
        });

    constexpr float kIntersectionDedupEpsilon = 0.03f;
    std::vector<std::pair<float, std::uint32_t>> deduped{};
    deduped.reserve(outIntersections.size());

    for (const auto& hit : outIntersections)
    {
        if (!deduped.empty() && std::fabs(hit.first - deduped.back().first) <= kIntersectionDedupEpsilon)
        {
            if (hit.second < deduped.back().second)
                deduped.back() = hit;
            continue;
        }

        deduped.push_back(hit);
    }

    outIntersections.swap(deduped);
}

bool VisCheck::TracePenetrationSegments(
    const Vector3& point1,
    const Vector3& point2,
    std::vector<PenetrationSegment>& outSegments) const
{
    outSegments.clear();
    if (!HasPenetrationMaterialData())
        return false;

    const Vector3 rayDelta = { point2.x - point1.x, point2.y - point1.y, point2.z - point1.z };
    const float distance = std::sqrt(VectorDot(rayDelta, rayDelta));
    if (distance <= 1e-4f)
        return true;

    const Vector3 rayDir = { rayDelta.x / distance, rayDelta.y / distance, rayDelta.z / distance };
    std::vector<std::pair<float, std::uint32_t>> intersections{};
    CollectCacheIntersections(point1, rayDir, distance, intersections);

    if (intersections.size() < 2)
        return true;

    constexpr float kPairEpsilon = 0.03f;
    std::size_t index = 0;
    while (index + 1 < intersections.size())
    {
        const float entryDistance = std::clamp(intersections[index].first, 0.0f, distance);
        const float exitDistance = std::clamp(intersections[index + 1].first, 0.0f, distance);
        index += 2;

        if (exitDistance <= entryDistance + kPairEpsilon)
            continue;

        const std::uint32_t entryTriId = intersections[index - 2].second;
        const std::uint32_t exitTriId = intersections[index - 1].second;
        const std::uint32_t entryMaterialHash =
            entryTriId < cacheTriangleMaterialHashes.size() ? cacheTriangleMaterialHashes[entryTriId] : 0u;
        const std::uint32_t exitMaterialHash =
            exitTriId < cacheTriangleMaterialHashes.size() ? cacheTriangleMaterialHashes[exitTriId] : 0u;

        PenetrationSegment segment{};
        segment.entryDistance = entryDistance;
        segment.exitDistance = exitDistance;
        segment.thickness = exitDistance - entryDistance;
        segment.entryMaterialHash = entryMaterialHash;
        segment.exitMaterialHash = exitMaterialHash;
        outSegments.push_back(segment);
    }

    return true;
}

bool VisCheck::IsPointVisible(const Vector3& point1, const Vector3& point2)
{
    if (!ready)
        return true;

    const Vector3 rayDelta = { point2.x - point1.x, point2.y - point1.y, point2.z - point1.z };
    const float distance = std::sqrt(VectorDot(rayDelta, rayDelta));
    if (distance <= 1e-4f)
        return true;

    const Vector3 rayDir = { rayDelta.x / distance, rayDelta.y / distance, rayDelta.z / distance };

    float hitDistance = std::numeric_limits<float>::max();
    if (IntersectCacheBvh(point1, rayDir, distance, hitDistance) && hitDistance < distance)
        return false;

    return true;
}

bool VisCheck::RayIntersectsTriangle(
    const Vector3& rayOrigin,
    const Vector3& rayDir,
    const TriangleCombined& triangle,
    float& t) const
{
    const Vector3 edge1 = VectorSub(triangle.v1, triangle.v0);
    const Vector3 edge2 = VectorSub(triangle.v2, triangle.v0);
    const Vector3 h = VectorCross(rayDir, edge2);
    const float a = VectorDot(edge1, h);

    if (std::fabs(a) < kRayEpsilon)
        return false;

    const float f = 1.0f / a;
    const Vector3 s = VectorSub(rayOrigin, triangle.v0);
    const float u = f * VectorDot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    const Vector3 q = VectorCross(s, edge1);
    const float v = f * VectorDot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * VectorDot(edge2, q);
    return t > kRayEpsilon;
}
