#pragma once

#include <span>

#include <Math/Vector.hpp>

namespace PlayerBoxModel
{
    struct ScreenBone
    {
        int Index = 0;
        Vector2 Screen{};
        bool OnScreen = false;
    };

    struct Box2D
    {
        Vector2 Min{};
        Vector2 Max{};
    };

    bool IsBoxSane(const Box2D& box, float screenWidth, float screenHeight);
    bool BuildBoxFromBones(std::span<const ScreenBone> bones, float screenWidth, float screenHeight, Box2D& outBox);
}
