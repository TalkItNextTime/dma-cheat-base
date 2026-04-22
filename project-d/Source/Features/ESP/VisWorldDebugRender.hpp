#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Math/Vector.hpp"

struct WorldDebugTriangle
{
    Vector3 V0{};
    Vector3 V1{};
    Vector3 V2{};
};

struct WorldDebugScreenTriangle
{
    Vector2 P0{};
    Vector2 P1{};
    Vector2 P2{};
    float DepthSqr = 0.0f;
    std::uint32_t FillColor = 0;
    std::uint32_t EdgeColor = 0;
};

bool ShouldCullTriangleBackface(const WorldDebugTriangle& tri, const Vector3& cameraPos);
float ComputeTriangleDepthSqr(const WorldDebugTriangle& tri, const Vector3& cameraPos);
void SortWorldDebugTrianglesBackToFront(std::vector<WorldDebugScreenTriangle>& triangles);
