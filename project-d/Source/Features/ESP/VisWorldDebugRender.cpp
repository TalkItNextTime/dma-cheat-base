#include "VisWorldDebugRender.hpp"

#include <algorithm>

namespace
{
    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs)
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs)
    {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Vector3 ComputeTriangleCenter(const WorldDebugTriangle& tri)
    {
        return {
            (tri.V0.x + tri.V1.x + tri.V2.x) / 3.0f,
            (tri.V0.y + tri.V1.y + tri.V2.y) / 3.0f,
            (tri.V0.z + tri.V1.z + tri.V2.z) / 3.0f
        };
    }
}

bool ShouldCullTriangleBackface(const WorldDebugTriangle& tri, const Vector3& cameraPos)
{
    const Vector3 edge01 = Subtract(tri.V1, tri.V0);
    const Vector3 edge02 = Subtract(tri.V2, tri.V0);
    const Vector3 normal = Cross(edge01, edge02);
    const Vector3 center = ComputeTriangleCenter(tri);
    const Vector3 toCamera = Subtract(cameraPos, center);
    return Dot(normal, toCamera) <= 0.0f;
}

float ComputeTriangleDepthSqr(const WorldDebugTriangle& tri, const Vector3& cameraPos)
{
    const Vector3 center = ComputeTriangleCenter(tri);
    const Vector3 delta = Subtract(center, cameraPos);
    return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

void SortWorldDebugTrianglesBackToFront(std::vector<WorldDebugScreenTriangle>& triangles)
{
    std::sort(
        triangles.begin(),
        triangles.end(),
        [](const WorldDebugScreenTriangle& lhs, const WorldDebugScreenTriangle& rhs)
        {
            return lhs.DepthSqr > rhs.DepthSqr;
        });
}
