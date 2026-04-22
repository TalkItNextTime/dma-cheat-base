#include <iostream>
#include <string>
#include <vector>

#include "Features/ESP/VisWorldDebugRender.hpp"

namespace
{
    bool ExpectTrue(bool value, const std::string& message)
    {
        if (value)
            return true;
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(std::size_t actual, std::size_t expected, const std::string& message)
    {
        if (actual == expected)
            return true;
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    const Vector3 camera{ 0.0f, 0.0f, 0.0f };
    const WorldDebugTriangle facing{
        { 10.0f, -10.0f, 10.0f },
        { 10.0f, 10.0f, 10.0f },
        { 10.0f, 0.0f, -10.0f }
    };
    const WorldDebugTriangle backFacing{
        facing.V0,
        facing.V2,
        facing.V1
    };

    ok &= ExpectTrue(
        !ShouldCullTriangleBackface(facing, camera),
        "front-facing triangle should remain visible");
    ok &= ExpectTrue(
        ShouldCullTriangleBackface(backFacing, camera),
        "reversed winding should be culled");

    std::vector<WorldDebugScreenTriangle> tris{
        { {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, 25.0f, 0xAAAAAAAAu, 0xBBBBBBBBu },
        { {0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}, 400.0f, 0xCCCCCCCCu, 0xDDDDDDDDu },
        { {0.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, 3.0f}, 100.0f, 0xEEEEEEEEu, 0xFFFFFFFFu }
    };

    SortWorldDebugTrianglesBackToFront(tris);

    ok &= ExpectEqual(tris.size(), 3u, "sort should keep all triangles");
    ok &= ExpectTrue(tris[0].DepthSqr == 400.0f, "furthest triangle should render first");
    ok &= ExpectTrue(tris[1].DepthSqr == 100.0f, "middle triangle should render second");
    ok &= ExpectTrue(tris[2].DepthSqr == 25.0f, "nearest triangle should render last");
    ok &= ExpectTrue(tris[0].FillColor == 0xCCCCCCCCu, "sort should keep fill color paired with the furthest triangle");
    ok &= ExpectTrue(tris[2].EdgeColor == 0xBBBBBBBBu, "sort should keep edge color paired with the nearest triangle");

    if (!ok)
        return 1;

    std::cout << "[PASS] vis_world_debug_render_tests\n";
    return 0;
}
