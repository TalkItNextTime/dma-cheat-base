#include <iostream>
#include <limits>
#include <string>

#define private public
#include "VisCheckCS2/VisCheck.h"
#undef private

namespace {

Vector3 VecSub(const Vector3& a, const Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 VecCross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float VecDot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool RayIntersectsTriangleCull(const Vector3& rayOrigin, const Vector3& rayDir, const TriangleCombined& tri, float& outT) {
    constexpr float kEps = 1e-6f;
    const Vector3 edge1 = VecSub(tri.v1, tri.v0);
    const Vector3 edge2 = VecSub(tri.v2, tri.v0);
    const Vector3 h = VecCross(rayDir, edge2);
    const float a = VecDot(edge1, h);

    // Cull backfaces and near-parallel triangles.
    if (a <= kEps) {
        return false;
    }

    const float f = 1.0f / a;
    const Vector3 s = VecSub(rayOrigin, tri.v0);
    const float u = f * VecDot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vector3 q = VecCross(s, edge1);
    const float v = f * VecDot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    outT = f * VecDot(edge2, q);
    return outT > kEps;
}

} // namespace

int main() {
    const std::string cachePath = "project-d/Maps/de_dust2";
    VisCheck vis(cachePath);
    if (!vis.IsReady()) {
        std::cout << "ready=0\n";
        return 1;
    }

    const Vector3 enemyPos = {-459.517944f, 1948.084839f, 70.931084f};
    const Vector3 localPos1 = {-315.529541f, 1516.848267f, 70.738289f};
    const Vector3 localPos2 = {-383.557007f, 1497.592041f, 70.357262f};
    const Vector3 localPos3 = {-436.130127f, 1471.261475f, 70.658875f};

    const bool v1 = vis.IsPointVisible(localPos1, enemyPos);
    const bool v2 = vis.IsPointVisible(localPos2, enemyPos);
    const bool v3 = vis.IsPointVisible(localPos3, enemyPos);
    const auto multiRayVisible = [&](const Vector3& src, const Vector3& dst) {
        int passCount = 0;
        const float offsets[3] = { 0.0f, 32.0f, 64.0f };
        for (const float zOff : offsets) {
            const Vector3 s = { src.x, src.y, src.z + zOff };
            const Vector3 d = { dst.x, dst.y, dst.z + zOff };
            if (vis.IsPointVisible(s, d)) {
                passCount++;
            }
        }
        return passCount >= 2;
    };
    const bool mv1 = multiRayVisible(localPos1, enemyPos);
    const bool mv2 = multiRayVisible(localPos2, enemyPos);
    const bool mv3 = multiRayVisible(localPos3, enemyPos);

    const auto visibleWithBackfaceCull = [&](const Vector3& src, const Vector3& dst) {
        const Vector3 rayDelta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
        const float distance = std::sqrt(rayDelta.x * rayDelta.x + rayDelta.y * rayDelta.y + rayDelta.z * rayDelta.z);
        if (distance <= 1e-6f) {
            return true;
        }
        const Vector3 dir = { rayDelta.x / distance, rayDelta.y / distance, rayDelta.z / distance };

        float hitDistance = std::numeric_limits<float>::max();
        const std::size_t triCount = vis.cacheIndices.size() / 3u;
        for (const std::uint32_t triId : vis.cachePrimitiveOrder) {
            if (triId >= triCount) {
                continue;
            }
            const std::size_t base = static_cast<std::size_t>(triId) * 3u;
            const std::uint32_t i0 = vis.cacheIndices[base + 0u];
            const std::uint32_t i1 = vis.cacheIndices[base + 1u];
            const std::uint32_t i2 = vis.cacheIndices[base + 2u];
            if (i0 >= vis.cacheVertices.size() || i1 >= vis.cacheVertices.size() || i2 >= vis.cacheVertices.size()) {
                continue;
            }

            const TriangleCombined tri{ vis.cacheVertices[i0], vis.cacheVertices[i1], vis.cacheVertices[i2] };
            float t = 0.0f;
            if (!RayIntersectsTriangleCull(src, dir, tri, t)) {
                continue;
            }
            if (t < hitDistance && t <= distance) {
                hitDistance = t;
            }
        }

        return hitDistance >= distance;
    };

    const bool cv1 = visibleWithBackfaceCull(localPos1, enemyPos);
    const bool cv2 = visibleWithBackfaceCull(localPos2, enemyPos);
    const bool cv3 = visibleWithBackfaceCull(localPos3, enemyPos);

    const auto dumpRay = [&](const char* name, const Vector3& src, const Vector3& dst) {
        const Vector3 rayDelta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
        const float distance = std::sqrt(rayDelta.x * rayDelta.x + rayDelta.y * rayDelta.y + rayDelta.z * rayDelta.z);
        Vector3 dir = { 0.0f, 0.0f, 0.0f };
        if (distance > 1e-6f) {
            dir = { rayDelta.x / distance, rayDelta.y / distance, rayDelta.z / distance };
        }

        float hitDistance = std::numeric_limits<float>::max();
        bool blocked = false;
        std::uint32_t bestTriId = std::numeric_limits<std::uint32_t>::max();
        TriangleCombined bestTri{};

        const std::size_t triCount = vis.cacheIndices.size() / 3u;
        for (const std::uint32_t triId : vis.cachePrimitiveOrder) {
            if (triId >= triCount) {
                continue;
            }
            const std::size_t base = static_cast<std::size_t>(triId) * 3u;
            const std::uint32_t i0 = vis.cacheIndices[base + 0u];
            const std::uint32_t i1 = vis.cacheIndices[base + 1u];
            const std::uint32_t i2 = vis.cacheIndices[base + 2u];
            if (i0 >= vis.cacheVertices.size() || i1 >= vis.cacheVertices.size() || i2 >= vis.cacheVertices.size()) {
                continue;
            }

            const TriangleCombined tri{ vis.cacheVertices[i0], vis.cacheVertices[i1], vis.cacheVertices[i2] };
            float t = 0.0f;
            if (!vis.RayIntersectsTriangle(src, dir, tri, t)) {
                continue;
            }
            if (t > 1e-6f && t < hitDistance && t <= distance) {
                hitDistance = t;
                blocked = true;
                bestTriId = triId;
                bestTri = tri;
            }
        }

        Vector3 hitPoint{};
        if (blocked) {
            hitPoint = {
                src.x + dir.x * hitDistance,
                src.y + dir.y * hitDistance,
                src.z + dir.z * hitDistance
            };
        }

        std::cout
            << name
            << " blocked=" << static_cast<int>(blocked)
            << " hit_t=" << hitDistance
            << " ray_len=" << distance
            << " tri_id=" << (blocked ? static_cast<int>(bestTriId) : -1)
            << " hit=(" << hitPoint.x << "," << hitPoint.y << "," << hitPoint.z << ")"
            << " tri_v0=(" << bestTri.v0.x << "," << bestTri.v0.y << "," << bestTri.v0.z << ")"
            << " tri_v1=(" << bestTri.v1.x << "," << bestTri.v1.y << "," << bestTri.v1.z << ")"
            << " tri_v2=(" << bestTri.v2.x << "," << bestTri.v2.y << "," << bestTri.v2.z << ")"
            << "\n";
    };

    std::cout << "ready=1\n";
    std::cout << static_cast<int>(v1) << " | " << static_cast<int>(v2) << " | " << static_cast<int>(v3) << "\n";
    std::cout << "multi=" << static_cast<int>(mv1) << " | " << static_cast<int>(mv2) << " | " << static_cast<int>(mv3) << "\n";
    std::cout << "cull=" << static_cast<int>(cv1) << " | " << static_cast<int>(cv2) << " | " << static_cast<int>(cv3) << "\n";
    dumpRay("r1", localPos1, enemyPos);
    dumpRay("r2", localPos2, enemyPos);
    dumpRay("r3", localPos3, enemyPos);
    return 0;
}
