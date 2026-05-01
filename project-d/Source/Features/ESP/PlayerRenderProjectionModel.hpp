#pragma once

#include <span>
#include <vector>

#include <Math/Matrix.hpp>
#include <Math/Vector.hpp>

#include "PlayerBoxModel.hpp"

namespace PlayerRenderProjectionModel
{
    struct WorldBone
    {
        int Index = 0;
        Vector3 World{};
    };

    struct ProjectedBone
    {
        int Index = 0;
        Vector3 World{};
        Vector2 Screen{};
        bool OnScreen = false;
    };

    struct Projection
    {
        std::vector<ProjectedBone> Bones{};
        Vector3 HeadWorld{};
        Vector2 HeadScreen{};
        bool HasHead = false;
        PlayerBoxModel::Box2D Box{};
        bool HasBox = false;
    };

    inline bool ProjectWorldToScreen(
        const Vector3& world,
        Vector2& screen,
        const Matrix& matrix,
        const float screenWidth,
        const float screenHeight)
    {
        const float w =
            matrix[3][0] * world.x +
            matrix[3][1] * world.y +
            matrix[3][2] * world.z +
            matrix[3][3];

        if (w < 0.001f)
            return false;

        const float invW = 1.0f / w;
        const float ndcX =
            (matrix[0][0] * world.x +
                matrix[0][1] * world.y +
                matrix[0][2] * world.z +
                matrix[0][3]) * invW;
        const float ndcY =
            (matrix[1][0] * world.x +
                matrix[1][1] * world.y +
                matrix[1][2] * world.z +
                matrix[1][3]) * invW;

        const float halfWidth = screenWidth * 0.5f;
        const float halfHeight = screenHeight * 0.5f;
        screen.x = halfWidth + ndcX * halfWidth;
        screen.y = halfHeight - ndcY * halfHeight;
        return true;
    }

    inline Projection BuildProjection(
        std::span<const WorldBone> worldBones,
        const Matrix& viewMatrix,
        const float screenWidth,
        const float screenHeight,
        const int headBoneIndex)
    {
        Projection projection{};
        projection.Bones.reserve(worldBones.size());

        std::vector<PlayerBoxModel::ScreenBone> screenBones{};
        screenBones.reserve(worldBones.size());

        for (const WorldBone& worldBone : worldBones)
        {
            ProjectedBone projected{};
            projected.Index = worldBone.Index;
            projected.World = worldBone.World;
            projected.OnScreen = ProjectWorldToScreen(
                worldBone.World,
                projected.Screen,
                viewMatrix,
                screenWidth,
                screenHeight);

            if (worldBone.Index == headBoneIndex)
            {
                projection.HeadWorld = worldBone.World;
                projection.HeadScreen = projected.Screen;
                projection.HasHead = projected.OnScreen;
            }

            screenBones.push_back(PlayerBoxModel::ScreenBone{
                worldBone.Index,
                projected.Screen,
                projected.OnScreen
            });
            projection.Bones.push_back(projected);
        }

        projection.HasBox = PlayerBoxModel::BuildBoxFromBones(
            screenBones,
            screenWidth,
            screenHeight,
            projection.Box);

        return projection;
    }
}
