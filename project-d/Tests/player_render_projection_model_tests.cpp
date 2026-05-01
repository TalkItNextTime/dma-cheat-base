#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "Features/ESP/PlayerRenderProjectionModel.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool NearlyEqual(const float a, const float b, const float epsilon = 0.01f)
    {
        return std::fabs(a - b) <= epsilon;
    }

    Matrix IdentityView()
    {
        Matrix matrix{};
        matrix[0][0] = 1.0f;
        matrix[1][1] = 1.0f;
        matrix[2][2] = 1.0f;
        matrix[3][3] = 1.0f;
        return matrix;
    }

    PlayerRenderProjectionModel::WorldBone Bone(const int index, const float x, const float y, const float z = 0.0f)
    {
        return PlayerRenderProjectionModel::WorldBone{
            index,
            Vector3{ x, y, z }
        };
    }
}

int main()
{
    bool ok = true;

    const std::vector<PlayerRenderProjectionModel::WorldBone> bones = {
        Bone(7, 0.00f, 0.25f),
        Bone(6, 0.00f, 0.18f),
        Bone(1, 0.00f, 0.00f),
        Bone(17, -0.04f, -0.14f),
        Bone(20, 0.04f, -0.14f),
        Bone(19, -0.05f, -0.30f),
        Bone(22, 0.05f, -0.30f),
    };

    Matrix firstView = IdentityView();
    Matrix secondView = IdentityView();
    secondView[0][3] = 0.10f;

    const auto first = PlayerRenderProjectionModel::BuildProjection(
        bones,
        firstView,
        1920.0f,
        1080.0f,
        7);
    const auto second = PlayerRenderProjectionModel::BuildProjection(
        bones,
        secondView,
        1920.0f,
        1080.0f,
        7);

    ok &= ExpectTrue(first.HasBox, "first view should build a projected box");
    ok &= ExpectTrue(second.HasBox, "second view should build a projected box");
    ok &= ExpectTrue(first.HasHead && second.HasHead, "head bone should be projected in both views");
    ok &= ExpectTrue(
        !NearlyEqual(first.HeadScreen.x, second.HeadScreen.x),
        "projected head X must change when the view matrix changes");
    ok &= ExpectTrue(
        second.HeadScreen.x > first.HeadScreen.x,
        "positive matrix X offset should move projected bones right");

    const std::vector<PlayerRenderProjectionModel::WorldBone> behindCamera = {
        Bone(7, 0.0f, 0.25f, 0.0f),
        Bone(6, 0.0f, 0.18f, 0.0f),
        Bone(1, 0.0f, 0.00f, 0.0f),
        Bone(17, -0.04f, -0.14f, 0.0f),
    };
    Matrix invalidView = IdentityView();
    invalidView[3][3] = 0.0f;

    const auto invalid = PlayerRenderProjectionModel::BuildProjection(
        behindCamera,
        invalidView,
        1920.0f,
        1080.0f,
        7);
    ok &= ExpectTrue(!invalid.HasBox, "invalid projected bones should not build a box");
    ok &= ExpectTrue(!invalid.HasHead, "invalid head projection should not be marked visible");

    if (!ok)
        return 1;

    std::cout << "[PASS] player_render_projection_model_tests\n";
    return 0;
}
