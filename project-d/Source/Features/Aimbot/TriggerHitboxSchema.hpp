#pragma once

#include <array>
#include <cstdint>

#include "Config/Structs.hpp"

namespace TriggerHitboxSchema
{
    struct BoneLink
    {
        int FromBone = 0;
        int ToBone = 0;
        std::uint64_t BoneMask = 0;
    };

    inline constexpr auto TrackedBones = Structs::AimBoneIds;
    inline constexpr int SizingRootBoneId = 1;
    inline constexpr float ReferenceBodyHeightPx = 150.0f;

    inline constexpr float HeadScaleFixed = 7.0f;
    inline constexpr float TorsoScaleFixed = 8.0f;
    inline constexpr float ArmsScaleFixed = 6.0f;
    inline constexpr float LegsScaleFixed = 5.0f;

    inline constexpr std::array<BoneLink, 16> Links = {
        BoneLink{ 1, 2, Structs::BoneMaskFromBoneId(1) | Structs::BoneMaskFromBoneId(2) },
        BoneLink{ 2, 23, Structs::BoneMaskFromBoneId(2) | Structs::BoneMaskFromBoneId(23) },
        BoneLink{ 23, 6, Structs::BoneMaskFromBoneId(23) | Structs::BoneMaskFromBoneId(6) },
        BoneLink{ 6, 7, Structs::BoneMaskFromBoneId(6) | Structs::BoneMaskFromBoneId(7) },

        BoneLink{ 6, 9, Structs::BoneMaskFromBoneId(6) | Structs::BoneMaskFromBoneId(9) },
        BoneLink{ 9, 10, Structs::BoneMaskFromBoneId(9) | Structs::BoneMaskFromBoneId(10) },
        BoneLink{ 10, 11, Structs::BoneMaskFromBoneId(10) | Structs::BoneMaskFromBoneId(11) },

        BoneLink{ 6, 13, Structs::BoneMaskFromBoneId(6) | Structs::BoneMaskFromBoneId(13) },
        BoneLink{ 13, 14, Structs::BoneMaskFromBoneId(13) | Structs::BoneMaskFromBoneId(14) },
        BoneLink{ 14, 15, Structs::BoneMaskFromBoneId(14) | Structs::BoneMaskFromBoneId(15) },

        BoneLink{ 1, 17, Structs::BoneMaskFromBoneId(1) | Structs::BoneMaskFromBoneId(17) },
        BoneLink{ 17, 18, Structs::BoneMaskFromBoneId(17) | Structs::BoneMaskFromBoneId(18) },
        BoneLink{ 18, 19, Structs::BoneMaskFromBoneId(18) | Structs::BoneMaskFromBoneId(19) },

        BoneLink{ 1, 20, Structs::BoneMaskFromBoneId(1) | Structs::BoneMaskFromBoneId(20) },
        BoneLink{ 20, 21, Structs::BoneMaskFromBoneId(20) | Structs::BoneMaskFromBoneId(21) },
        BoneLink{ 21, 22, Structs::BoneMaskFromBoneId(21) | Structs::BoneMaskFromBoneId(22) }
    };

    inline constexpr int RegionFromBone(const int boneId)
    {
        if (boneId == Structs::AimHeadBoneId)
            return 0;

        switch (boneId)
        {
        case 9:
        case 10:
        case 11:
        case 13:
        case 14:
        case 15:
            return 2;

        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
            return 3;

        default:
            return 1;
        }
    }

    inline constexpr int RegionFromLink(const int fromBone, const int toBone)
    {
        const int fromRegion = RegionFromBone(fromBone);
        const int toRegion = RegionFromBone(toBone);
        if (fromRegion == 0 || toRegion == 0)
            return 0;
        return fromRegion == toRegion ? fromRegion : 1;
    }

    inline constexpr float RegionScale(const int region)
    {
        switch (region)
        {
        case 0: return HeadScaleFixed;
        case 2: return ArmsScaleFixed;
        case 3: return LegsScaleFixed;
        default: return TorsoScaleFixed;
        }
    }

    inline constexpr float BonePointRadiusScale(const int boneId)
    {
        switch (boneId)
        {
        case 1: return 1.45f;
        case 2: return 1.35f;
        case 23: return 1.40f;
        case 6: return 1.20f;

        case 9:
        case 13:
            return 1.15f;

        case 10:
        case 14:
            return 0.95f;

        case 11:
        case 15:
            return 0.80f;

        case 17:
        case 20:
            return 1.25f;

        case 18:
        case 21:
            return 1.05f;

        case 19:
        case 22:
            return 0.90f;

        default:
            return 1.0f;
        }
    }

    inline constexpr float BoneLinkRadiusScale(const int fromBone, const int toBone)
    {
        const auto match = [&](const int a, const int b)
        {
            return (fromBone == a && toBone == b) || (fromBone == b && toBone == a);
        };

        if (match(1, 2)) return 1.45f;
        if (match(2, 23)) return 1.40f;
        if (match(23, 6)) return 1.25f;
        if (match(6, 7)) return 1.10f;

        if (match(6, 9) || match(6, 13)) return 1.12f;
        if (match(9, 10) || match(13, 14)) return 0.92f;
        if (match(10, 11) || match(14, 15)) return 0.78f;

        if (match(1, 17) || match(1, 20)) return 1.28f;
        if (match(17, 18) || match(20, 21)) return 1.12f;
        if (match(18, 19) || match(21, 22)) return 0.90f;

        return 1.0f;
    }
}
