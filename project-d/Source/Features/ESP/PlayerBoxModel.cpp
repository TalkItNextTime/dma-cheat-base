#include "PlayerBoxModel.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
    bool IsFinitePoint(const Vector2& point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y);
    }

    bool IsCoreBodyBone(const int boneIndex)
    {
        switch (boneIndex)
        {
        case 1:
        case 2:
        case 6:
        case 17:
        case 19:
        case 20:
        case 22:
        case 23:
            return true;
        default:
            return false;
        }
    }
}

namespace PlayerBoxModel
{
    bool IsBoxSane(const Box2D& box, const float screenWidth, const float screenHeight)
    {
        const float width = box.Max.x - box.Min.x;
        const float height = box.Max.y - box.Min.y;
        if (!std::isfinite(width) || !std::isfinite(height))
            return false;
        if (width < 4.0f || height < 8.0f)
            return false;
        if (screenWidth > 0.0f && width > screenWidth * 0.45f)
            return false;
        if (screenHeight > 0.0f && height > screenHeight * 0.95f)
            return false;

        const float aspect = width / height;
        if (aspect < 0.10f || aspect > 1.15f)
            return false;

        return true;
    }

    bool BuildBoxFromBones(std::span<const ScreenBone> bones, const float screenWidth, const float screenHeight, Box2D& outBox)
    {
        float minX = FLT_MAX;
        float minY = FLT_MAX;
        float maxX = -FLT_MAX;
        float maxY = -FLT_MAX;
        int validCount = 0;
        int coreCount = 0;

        for (const ScreenBone& bone : bones)
        {
            if (!bone.OnScreen || !IsFinitePoint(bone.Screen))
                continue;

            ++validCount;
            if (IsCoreBodyBone(bone.Index))
                ++coreCount;

            minX = (std::min)(minX, bone.Screen.x);
            minY = (std::min)(minY, bone.Screen.y);
            maxX = (std::max)(maxX, bone.Screen.x);
            maxY = (std::max)(maxY, bone.Screen.y);
        }

        if (validCount < 4 || coreCount < 4)
            return false;

        const float width = maxX - minX;
        const float height = maxY - minY;
        if (width < 3.0f || height < 3.0f)
            return false;

        const float paddingX = (std::max)(width * 0.08f, 3.0f);
        const float paddingY = (std::max)(height * 0.06f, 3.0f);
        outBox.Min = Vector2{ minX - paddingX, minY - paddingY };
        outBox.Max = Vector2{ maxX + paddingX, maxY + paddingY };

        return IsBoxSane(outBox, screenWidth, screenHeight);
    }
}
