#include <Pch.hpp>
#include <SDK.hpp>
#include <ESP/ESP.hpp>
#include "Aimbot.hpp"

#include <array>
#include <cfloat>
#include <unordered_set>

namespace
{
    struct BoneDataRaw
    {
        Vector3 Position{};
        std::uint8_t Padding[0x14]{};
    };

    struct TargetCandidate
    {
        uint64_t Pawn = 0;
        Vector3 World{};
        Vector2 Screen{};
        float ScreenDistance = FLT_MAX;
        float WorldDistance = FLT_MAX;
        float AllowedFovPx = 0.0f;
        int Team = 0;
    };

    struct WeaponRuntimeState
    {
        std::uint64_t WeaponEntity = 0;
        int WeaponId = 0;
        int Category = Structs::AimWeapon_Rifle;
        bool IsGun = false;
        bool IsReloading = false;
        bool IsDeagle = false;
        bool IsRevolver = false;
    };

    struct TriggerRuntimeProfile
    {
        float HitboxRadiusPx = 4.5f;
        int PreFireDelayMs = 35;
        int PostFireIntervalMs = 55;
        int TimeoutForceFireMs = 0;
        int HoldFireMs = 8;
        std::uint64_t BoneMask = 0ull;
    };

    struct BoneLink
    {
        int FromBone = 0;
        int ToBone = 0;
        std::uint64_t BoneMask = 0;
    };

    struct TriggerHitboxMatch
    {
        int FromBone = -1;
        int ToBone = -1;
        float DistancePx = FLT_MAX;
        bool IsPoint = false;
    };

    constexpr int kMaxControllers = 64;
    constexpr int kAliveLifeStateA = 0;
    constexpr int kAliveLifeStateB = 256;
    constexpr float kDefaultEyeHeight = 64.0f;
    constexpr int kDefaultTriggerHoldMs = 8;
    constexpr std::uint64_t kAllBonesMask = Structs::AimAllBoneMask;
    constexpr auto kTriggerTrackedBones = Structs::AimBoneIds;
    constexpr float kTriggerHeadScaleFixed = 7.0f;
    constexpr float kTriggerTorsoScaleFixed = 8.0f;
    constexpr float kTriggerArmsScaleFixed = 6.0f;
    constexpr float kTriggerLegsScaleFixed = 5.0f;
    constexpr float kFlashBlockOverlayStrongThreshold = 0.60f; // 强致盲暂停阈值；值越高，越早恢复自瞄/扳机。
    constexpr float kFlashBlockOverlaySoftThreshold = 0.60f; // 软致盲暂停阈值（需配合 duration）；值越高，恢复越早。
    constexpr float kFlashBlockDurationAssistSec = 0.08f;

    constexpr std::array<BoneLink, 16> kTriggerBoneLinks = {
        BoneLink{ 0, 2, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(2) },
        BoneLink{ 2, 4, Structs::BoneMaskFromBoneId(2) | Structs::BoneMaskFromBoneId(4) },
        BoneLink{ 4, 5, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(5) },
        BoneLink{ 5, 6, Structs::BoneMaskFromBoneId(5) | Structs::BoneMaskFromBoneId(6) },

        BoneLink{ 4, 8, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(8) },
        BoneLink{ 8, 9, Structs::BoneMaskFromBoneId(8) | Structs::BoneMaskFromBoneId(9) },
        BoneLink{ 9, 10, Structs::BoneMaskFromBoneId(9) | Structs::BoneMaskFromBoneId(10) },

        BoneLink{ 4, 13, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(13) },
        BoneLink{ 13, 14, Structs::BoneMaskFromBoneId(13) | Structs::BoneMaskFromBoneId(14) },
        BoneLink{ 14, 15, Structs::BoneMaskFromBoneId(14) | Structs::BoneMaskFromBoneId(15) },

        BoneLink{ 0, 22, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(22) },
        BoneLink{ 22, 23, Structs::BoneMaskFromBoneId(22) | Structs::BoneMaskFromBoneId(23) },
        BoneLink{ 23, 24, Structs::BoneMaskFromBoneId(23) | Structs::BoneMaskFromBoneId(24) },

        BoneLink{ 0, 25, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(25) },
        BoneLink{ 25, 26, Structs::BoneMaskFromBoneId(25) | Structs::BoneMaskFromBoneId(26) },
        BoneLink{ 26, 27, Structs::BoneMaskFromBoneId(26) | Structs::BoneMaskFromBoneId(27) }
    };

    constexpr auto kAimbotProbeBones = Structs::AimBoneIds;

    bool IsLikelyUserAddress(const std::uint64_t address)
    {
        return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
    }

    bool IsAlive(const int health, const int lifeState)
    {
        return health > 0 && (lifeState == kAliveLifeStateA || lifeState == kAliveLifeStateB);
    }

    bool IsNonZeroPosition(const Vector3& value)
    {
        return std::fabs(value.x) > 0.01f || std::fabs(value.y) > 0.01f || std::fabs(value.z) > 0.01f;
    }

    float ComputeFlashOverlayNormalized(const float overlayAlphaRaw, const float maxAlphaRaw)
    {
        const float overlayAlpha = (std::max)(0.0f, overlayAlphaRaw);
        const float maxAlpha = (std::max)(0.0f, maxAlphaRaw);

        const bool overlayLooksUnit = overlayAlpha <= 1.5f;
        const bool maxLooksUnit = maxAlpha <= 1.5f;

        const float overlay01 = overlayLooksUnit
            ? std::clamp(overlayAlpha, 0.0f, 1.0f)
            : std::clamp(overlayAlpha / 255.0f, 0.0f, 1.0f);

        if (maxAlpha <= 0.001f)
            return overlay01;

        const float max01 = maxLooksUnit
            ? std::clamp(maxAlpha, 0.0f, 1.0f)
            : std::clamp(maxAlpha / 255.0f, 0.0f, 1.0f);

        if (max01 <= 0.001f)
            return overlay01;

        // Mixed scales (for example overlay in 0..1 but max in 0..255): trust overlay itself.
        if (overlayLooksUnit != maxLooksUnit)
            return overlay01;

        const float ratio = std::clamp(overlay01 / max01, 0.0f, 1.0f);
        return (std::max)(overlay01, ratio);
    }

    bool IsPlayerEffectivelyFlashed(const std::uint64_t localPawn)
    {
        if (!IsLikelyUserAddress(localPawn))
            return false;

        const float flashDuration = Offsets::Schema::m_flFlashDuration
            ? (std::max)(0.0f, mem.Read<float>(localPawn + Offsets::Schema::m_flFlashDuration))
            : 0.0f;

        // Prefer current overlay alpha to avoid over-blocking during the fade-out phase.
        if (Offsets::Schema::m_flFlashOverlayAlpha)
        {
            const float overlayAlpha = (std::max)(
                0.0f,
                mem.Read<float>(localPawn + Offsets::Schema::m_flFlashOverlayAlpha)
            );

            float maxAlpha = 0.0f;
            if (Offsets::Schema::m_flFlashMaxAlpha)
            {
                maxAlpha = (std::max)(
                    0.0f,
                    mem.Read<float>(localPawn + Offsets::Schema::m_flFlashMaxAlpha)
                );
            }

            const float flashStrength = ComputeFlashOverlayNormalized(overlayAlpha, maxAlpha);
            if (flashStrength >= kFlashBlockOverlayStrongThreshold)
                return true;
            if (flashStrength >= kFlashBlockOverlaySoftThreshold && flashDuration >= kFlashBlockDurationAssistSec)
                return true;
            return false;
        }

        return false;
    }

    std::uint64_t NormalizeBoneMask(std::uint64_t mask, const std::uint64_t fallbackMask)
    {
        mask &= kAllBonesMask;
        return mask != 0ull ? mask : fallbackMask;
    }

    bool IsBoneEnabledByMask(const std::uint64_t mask, const int boneId)
    {
        return (mask & Structs::BoneMaskFromBoneId(boneId)) != 0ull;
    }

    float BonePointRadiusScale(const int boneId)
    {
        switch (boneId)
        {
        case 0: return 1.45f; // pelvis
        case 2: return 1.35f; // spine
        case 4: return 1.40f; // chest
        case 5: return 1.20f; // neck

        case 8:
        case 13:
            return 1.15f; // shoulders

        case 9:
        case 14:
            return 0.95f; // elbows

        case 10:
        case 15:
            return 0.80f; // hands

        case 22:
        case 25:
            return 1.25f; // thighs

        case 23:
        case 26:
            return 1.05f; // knees

        case 24:
        case 27:
            return 0.90f; // feet

        default:
            break;
        }

        return 1.0f;
    }

    float BoneLinkRadiusScale(const int fromBone, const int toBone)
    {
        const auto match = [&](const int a, const int b)
        {
            return (fromBone == a && toBone == b) || (fromBone == b && toBone == a);
        };

        if (match(0, 2)) return 1.45f;   // pelvis -> spine
        if (match(2, 4)) return 1.40f;   // spine -> chest
        if (match(4, 5)) return 1.25f;   // chest -> neck
        if (match(5, 6)) return 1.10f;   // neck -> head

        if (match(4, 8) || match(4, 13)) return 1.12f; // shoulder roots
        if (match(8, 9) || match(13, 14)) return 0.92f; // upper arm
        if (match(9, 10) || match(14, 15)) return 0.78f; // forearm/hand

        if (match(0, 22) || match(0, 25)) return 1.28f; // hip -> thigh
        if (match(22, 23) || match(25, 26)) return 1.12f; // thigh -> knee
        if (match(23, 24) || match(26, 27)) return 0.90f; // calf -> foot

        return 1.0f;
    }

    float ToScreenDistance(const Vector2& from, const Vector2& to)
    {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float FovDegreesToPixels(const float fovDegrees)
    {
        const float clampedFov = std::clamp(fovDegrees, 0.0f, 180.0f);
        return (clampedFov / 180.0f) * ScreenCenter.x;
    }

    float SineEase01(const float value)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return 0.5f * (1.0f - std::cos(t * math::PI));
    }

    int QuantizeMouseStep(const float value)
    {
        if (std::fabs(value) < 0.01f)
            return 0;

        const int rounded = static_cast<int>(std::lround(value));
        if (rounded != 0)
            return rounded;

        return value > 0.0f ? 1 : -1;
    }

    int ResolveWeaponCategoryFromDefinition(const int weaponId)
    {
        switch (weaponId)
        {
        case 1: case 2: case 3: case 4:
        case 30: case 32: case 36:
        case 61: case 63: case 64:
            return Structs::AimWeapon_Pistol;

        case 17: case 19: case 23:
        case 24: case 26: case 33: case 34:
            return Structs::AimWeapon_Smg;

        case 25: case 27: case 29: case 35:
            return Structs::AimWeapon_Shotgun;

        case 7: case 8: case 10: case 13:
        case 16: case 39: case 60:
            return Structs::AimWeapon_Rifle;

        case 9: case 11: case 38: case 40:
            return Structs::AimWeapon_Sniper;

        case 14: case 28:
            return Structs::AimWeapon_Lmg;

        default:
            break;
        }

        return Structs::AimWeapon_Rifle;
    }

    bool IsGunWeaponDefinition(const int weaponId)
    {
        if (weaponId <= 0)
            return false;

        switch (weaponId)
        {
        case 31:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
        case 57:
        case 59:
            return false;

        default:
            break;
        }

        return weaponId > 0 && weaponId < 200;
    }

    bool TryReadLocalWeaponState(const SDK::CoreCache& core, WeaponRuntimeState& outState)
    {
        outState = {};

        if (!IsLikelyUserAddress(core.LocalPawn))
            return false;

        if (!Offsets::Schema::m_pWeaponServices || !Offsets::Schema::m_hActiveWeapon ||
            !Offsets::Schema::m_AttributeManager || !Offsets::Schema::m_Item || !Offsets::Schema::m_iItemDefinitionIndex)
        {
            return false;
        }

        const std::uint64_t weaponServices = mem.Read<std::uint64_t>(core.LocalPawn + Offsets::Schema::m_pWeaponServices);
        if (!IsLikelyUserAddress(weaponServices))
            return false;

        const std::uint32_t activeWeaponHandle = mem.Read<std::uint32_t>(weaponServices + Offsets::Schema::m_hActiveWeapon);
        if (!activeWeaponHandle)
            return false;

        const std::uint64_t activeWeapon = sdk.ResolveEntityFromHandle(activeWeaponHandle, core.EntityList);
        if (!IsLikelyUserAddress(activeWeapon))
            return false;

        outState.WeaponEntity = activeWeapon;

        const std::uint64_t itemRoot = activeWeapon + Offsets::Schema::m_AttributeManager + Offsets::Schema::m_Item;
        outState.WeaponId = std::abs(mem.Read<short>(itemRoot + Offsets::Schema::m_iItemDefinitionIndex));
        outState.Category = ResolveWeaponCategoryFromDefinition(outState.WeaponId);
        outState.IsGun = IsGunWeaponDefinition(outState.WeaponId);
        outState.IsDeagle = outState.WeaponId == 1;
        outState.IsRevolver = outState.WeaponId == 64;

        if (Offsets::Schema::m_bInReload)
            outState.IsReloading = mem.Read<std::uint8_t>(activeWeapon + Offsets::Schema::m_bInReload) != 0;

        return true;
    }

    float DistancePointToSegment2D(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd)
    {
        const float vx = segmentEnd.x - segmentStart.x;
        const float vy = segmentEnd.y - segmentStart.y;
        const float wx = point.x - segmentStart.x;
        const float wy = point.y - segmentStart.y;
        const float segmentLenSq = vx * vx + vy * vy;

        if (segmentLenSq < 0.0001f)
            return ToScreenDistance(point, segmentStart);

        const float t = std::clamp((wx * vx + wy * vy) / segmentLenSq, 0.0f, 1.0f);
        const Vector2 closest{
            segmentStart.x + t * vx,
            segmentStart.y + t * vy
        };

        return ToScreenDistance(point, closest);
    }

    bool IsCrosshairOnPawnBoneHitbox(
        const SDK::CoreCache& core,
        const std::uint64_t pawn,
        const Vector2& crosshair,
        std::uint64_t boneMask,
        const float hitboxRadiusPx,
        const float headRadiusPx,
        float& outBestDistance,
        TriggerHitboxMatch* outMatch)
    {
        outBestDistance = FLT_MAX;
        if (outMatch)
            *outMatch = {};

        if (!Offsets::Schema::m_pGameSceneNode || !Offsets::Schema::m_modelState)
            return false;

        boneMask = NormalizeBoneMask(boneMask, Structs::BoneMaskFromBoneId(Structs::AimHeadBoneId));
        const float clampedBodyRadius = (std::max)(0.5f, hitboxRadiusPx);
        const float clampedHeadRadius = (std::max)(0.5f, headRadiusPx);
        bool hitDetected = false;

        const std::uint64_t sceneNode = mem.Read<std::uint64_t>(pawn + Offsets::Schema::m_pGameSceneNode);
        if (!IsLikelyUserAddress(sceneNode))
            return false;

        const std::uint64_t boneArray = mem.Read<std::uint64_t>(sceneNode + Offsets::Schema::m_modelState + Offsets::Layout::BoneArray);
        if (!IsLikelyUserAddress(boneArray))
            return false;

        std::array<BoneDataRaw, kTriggerTrackedBones.size()> rawBones{};
        const auto scatter = mem.CreateScatterHandle();
        if (!scatter)
            return false;

        for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
        {
            const std::uint64_t boneAddress = boneArray + static_cast<std::uint64_t>(kTriggerTrackedBones[i]) * Offsets::Layout::BoneStride;
            mem.AddScatterReadRequest(scatter, boneAddress, &rawBones[i], sizeof(BoneDataRaw));
        }

        mem.ExecuteReadScatter(scatter);
        mem.CloseScatterHandle(scatter);

        std::array<Vector2, kTriggerTrackedBones.size()> screenBones{};
        std::array<bool, kTriggerTrackedBones.size()> onScreen{};

        auto findBoneSlot = [](const int boneId) -> int
        {
            for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
            {
                if (kTriggerTrackedBones[i] == boneId)
                    return static_cast<int>(i);
            }

            return -1;
        };

        for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
        {
            if (!IsNonZeroPosition(rawBones[i].Position))
                continue;

            Vector2 screenPos{};
            if (!sdk.WorldToScreen(rawBones[i].Position, screenPos, core.ViewMatrix))
                continue;

            screenBones[i] = screenPos;
            onScreen[i] = true;
        }

        float adaptiveHeadRadius = clampedHeadRadius;
        const int headSlot = findBoneSlot(Structs::AimHeadBoneId);
        const int pelvisSlot = findBoneSlot(0);
        float distanceScale = 1.0f;
        if (headSlot >= 0 && pelvisSlot >= 0 && onScreen[headSlot] && onScreen[pelvisSlot])
        {
            constexpr float kReferenceBodyHeightPx = 150.0f;
            const float bodyHeight = (std::max)(1.0f, std::fabs(screenBones[pelvisSlot].y - screenBones[headSlot].y));
            distanceScale = std::clamp(bodyHeight / kReferenceBodyHeightPx, 0.20f, 3.00f);
        }
        adaptiveHeadRadius = std::clamp(clampedHeadRadius * distanceScale, 0.5f, 100.0f);
        const float adaptiveBodyRadius = std::clamp(clampedBodyRadius * distanceScale, 0.5f, 100.0f);

        const auto hitboxRegionFromBone = [](const int boneId) -> int
        {
            if (boneId == Structs::AimHeadBoneId)
                return 0; // head
            switch (boneId)
            {
            case 8: case 9: case 10:
            case 13: case 14: case 15:
                return 2; // arms
            case 22: case 23: case 24:
            case 25: case 26: case 27:
                return 3; // legs
            default:
                return 1; // torso
            }
        };
        const auto hitboxRegionScale = [](const int region) -> float
        {
            switch (region)
            {
            case 0: return kTriggerHeadScaleFixed;
            case 2: return kTriggerArmsScaleFixed;
            case 3: return kTriggerLegsScaleFixed;
            default: break;
            }
            return kTriggerTorsoScaleFixed;
        };

        for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
        {
            if (!onScreen[i])
                continue;

            if (!IsBoneEnabledByMask(boneMask, kTriggerTrackedBones[i]))
                continue;

            const float pointDistance = ToScreenDistance(crosshair, screenBones[i]);
            const int pointRegion = hitboxRegionFromBone(kTriggerTrackedBones[i]);
            const float pointThresholdBase = kTriggerTrackedBones[i] == Structs::AimHeadBoneId
                ? adaptiveHeadRadius
                : adaptiveBodyRadius * BonePointRadiusScale(kTriggerTrackedBones[i]);
            const float pointThreshold = pointThresholdBase * hitboxRegionScale(pointRegion);
            if (pointDistance < outBestDistance)
            {
                outBestDistance = pointDistance;
                if (outMatch)
                {
                    outMatch->FromBone = kTriggerTrackedBones[i];
                    outMatch->ToBone = kTriggerTrackedBones[i];
                    outMatch->DistancePx = pointDistance;
                    outMatch->IsPoint = true;
                }
            }

            if (pointDistance <= pointThreshold)
                hitDetected = true;
        }

        for (const BoneLink& link : kTriggerBoneLinks)
        {
            const bool endpointSelected =
                IsBoneEnabledByMask(boneMask, link.FromBone) ||
                IsBoneEnabledByMask(boneMask, link.ToBone);
            if (!endpointSelected)
                continue;

            const int fromSlot = findBoneSlot(link.FromBone);
            const int toSlot = findBoneSlot(link.ToBone);
            if (fromSlot < 0 || toSlot < 0)
                continue;

            if (!onScreen[fromSlot] || !onScreen[toSlot])
                continue;

            const float distance = DistancePointToSegment2D(crosshair, screenBones[fromSlot], screenBones[toSlot]);
            const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
                ? 0
                : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                    ? hitboxRegionFromBone(link.FromBone)
                    : 1);
            float threshold = adaptiveBodyRadius * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
            if (distance < outBestDistance)
            {
                outBestDistance = distance;
                if (outMatch)
                {
                    outMatch->FromBone = link.FromBone;
                    outMatch->ToBone = link.ToBone;
                    outMatch->DistancePx = distance;
                    outMatch->IsPoint = false;
                }
            }

            if (distance <= threshold)
                hitDetected = true;
        }

        return hitDetected;
    }

    bool IsCrosshairOnSnapshotBoneHitbox(
        const TriggerBoneSnapshot& snapshot,
        const Vector2& crosshair,
        std::uint64_t boneMask,
        const float hitboxRadiusPx,
        const float headRadiusPx,
        float& outBestDistance)
    {
        outBestDistance = FLT_MAX;

        boneMask = NormalizeBoneMask(boneMask, Structs::BoneMaskFromBoneId(Structs::AimHeadBoneId));
        const float clampedBodyRadius = (std::max)(0.5f, hitboxRadiusPx);
        const float clampedHeadRadius = (std::max)(0.5f, headRadiusPx);
        bool hitDetected = false;

        auto findBoneSlot = [](const int boneId) -> int
        {
            for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
            {
                if (kTriggerTrackedBones[i] == boneId)
                    return static_cast<int>(i);
            }

            return -1;
        };

        float distanceScale = 1.0f;
        const int headSlot = findBoneSlot(Structs::AimHeadBoneId);
        const int pelvisSlot = findBoneSlot(0);
        if (headSlot >= 0 && pelvisSlot >= 0 &&
            snapshot.BoneValid[static_cast<std::size_t>(headSlot)] &&
            snapshot.BoneValid[static_cast<std::size_t>(pelvisSlot)])
        {
            constexpr float kReferenceBodyHeightPx = 150.0f;
            const Vector2 headScreen = snapshot.Bones[static_cast<std::size_t>(headSlot)].Screen;
            const Vector2 pelvisScreen = snapshot.Bones[static_cast<std::size_t>(pelvisSlot)].Screen;
            const float bodyHeight = (std::max)(1.0f, std::fabs(pelvisScreen.y - headScreen.y));
            distanceScale = std::clamp(bodyHeight / kReferenceBodyHeightPx, 0.20f, 3.00f);
        }

        const float adaptiveHeadRadius = std::clamp(clampedHeadRadius * distanceScale, 0.5f, 100.0f);
        const float adaptiveBodyRadius = std::clamp(clampedBodyRadius * distanceScale, 0.5f, 100.0f);

        const auto hitboxRegionFromBone = [](const int boneId) -> int
        {
            if (boneId == Structs::AimHeadBoneId)
                return 0; // head
            switch (boneId)
            {
            case 8: case 9: case 10:
            case 13: case 14: case 15:
                return 2; // arms
            case 22: case 23: case 24:
            case 25: case 26: case 27:
                return 3; // legs
            default:
                return 1; // torso
            }
        };
        const auto hitboxRegionScale = [](const int region) -> float
        {
            switch (region)
            {
            case 0: return kTriggerHeadScaleFixed;
            case 2: return kTriggerArmsScaleFixed;
            case 3: return kTriggerLegsScaleFixed;
            default: break;
            }
            return kTriggerTorsoScaleFixed;
        };

        for (size_t i = 0; i < kTriggerTrackedBones.size(); ++i)
        {
            if (i >= snapshot.BoneValid.size() || !snapshot.BoneValid[i])
                continue;

            if (!IsBoneEnabledByMask(boneMask, kTriggerTrackedBones[i]))
                continue;

            const float pointDistance = ToScreenDistance(crosshair, snapshot.Bones[i].Screen);
            const int pointRegion = hitboxRegionFromBone(kTriggerTrackedBones[i]);
            const float pointThresholdBase = kTriggerTrackedBones[i] == Structs::AimHeadBoneId
                ? adaptiveHeadRadius
                : adaptiveBodyRadius * BonePointRadiusScale(kTriggerTrackedBones[i]);
            const float pointThreshold = pointThresholdBase * hitboxRegionScale(pointRegion);
            if (pointDistance < outBestDistance)
                outBestDistance = pointDistance;

            if (pointDistance <= pointThreshold)
                hitDetected = true;
        }

        for (const BoneLink& link : kTriggerBoneLinks)
        {
            const bool endpointSelected =
                IsBoneEnabledByMask(boneMask, link.FromBone) ||
                IsBoneEnabledByMask(boneMask, link.ToBone);
            if (!endpointSelected)
                continue;

            const int fromSlot = findBoneSlot(link.FromBone);
            const int toSlot = findBoneSlot(link.ToBone);
            if (fromSlot < 0 || toSlot < 0)
                continue;

            if (!snapshot.BoneValid[static_cast<std::size_t>(fromSlot)] ||
                !snapshot.BoneValid[static_cast<std::size_t>(toSlot)])
            {
                continue;
            }

            const float distance = DistancePointToSegment2D(
                crosshair,
                snapshot.Bones[static_cast<std::size_t>(fromSlot)].Screen,
                snapshot.Bones[static_cast<std::size_t>(toSlot)].Screen
            );
            const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
                ? 0
                : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                    ? hitboxRegionFromBone(link.FromBone)
                    : 1);
            const float threshold = adaptiveBodyRadius * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
            if (distance < outBestDistance)
                outBestDistance = distance;

            if (distance <= threshold)
                hitDetected = true;
        }

        return hitDetected;
    }
}

void Aimbot::Update()
{
    UpdateAimbot();
    UpdateTriggerbot();
}

float Aimbot::GetCurrentFovRadiusPx() const
{
    return m_CurrentFovRadiusPx.load(std::memory_order_relaxed);
}

float Aimbot::GetCurrentTriggerHitboxRadiusPx() const
{
    return m_CurrentTriggerHitboxRadiusPx.load(std::memory_order_relaxed);
}

float Aimbot::GetCurrentTriggerHeadRadiusPx() const
{
    return m_CurrentTriggerHeadRadiusPx.load(std::memory_order_relaxed);
}

std::uint64_t Aimbot::GetCurrentTriggerBoneMask() const
{
    return m_CurrentTriggerBoneMask.load(std::memory_order_relaxed);
}

int Aimbot::GetCurrentTriggerDetectMode() const
{
    return m_CurrentTriggerDetectMode.load(std::memory_order_relaxed);
}

bool Aimbot::IsAimbotHotkeyActiveVisual() const
{
    return m_AimbotHotkeyActiveVisual.load(std::memory_order_relaxed);
}

bool Aimbot::HasAimbotTargetVisual() const
{
    return m_AimbotHasTargetVisual.load(std::memory_order_relaxed);
}

bool Aimbot::IsTriggerHotkeyActiveVisual() const
{
    return m_TriggerHotkeyActiveVisual.load(std::memory_order_relaxed);
}

bool Aimbot::HasTriggerTargetVisual() const
{
    return m_TriggerHasTargetVisual.load(std::memory_order_relaxed);
}

void Aimbot::ReleaseTriggerMouseIfHeld()
{
    if (!m_TriggerMouseHeld)
        return;

    std::lock_guard lock(m_KmboxMutex);
    Kmbox.Mouse.Left(false);
    m_TriggerMouseHeld = false;
    m_TriggerMouseReleaseAt = {};
}

void Aimbot::UpdateTriggerMouseState(const std::chrono::steady_clock::time_point& now)
{
    if (!m_TriggerMouseHeld)
        return;

    if (m_TriggerMouseReleaseAt.time_since_epoch().count() == 0 || now < m_TriggerMouseReleaseAt)
        return;

    ReleaseTriggerMouseIfHeld();
}

bool Aimbot::IsKeybindActive(const int virtualKey, const int mode, bool& toggleState, bool& wasDown)
{
    constexpr int kOnKeyDown = static_cast<int>(ImAdd::KeyBindOptions::OnKeyDown);
    constexpr int kOnToggle = static_cast<int>(ImAdd::KeyBindOptions::OnToggle);
    constexpr int kAlways = static_cast<int>(ImAdd::KeyBindOptions::Always);

    if (mode == kAlways)
    {
        toggleState = false;
        wasDown = false;
        return true;
    }

    if (virtualKey <= 0)
    {
        toggleState = false;
        wasDown = false;
        return false;
    }

    const bool down = overlay.IsHostKeyDown(virtualKey);

    if (mode == kOnToggle)
    {
        if (down && !wasDown)
            toggleState = !toggleState;

        wasDown = down;
        return toggleState;
    }

    if (mode != kOnKeyDown)
        toggleState = false;

    wasDown = down;
    return down;
}

bool Aimbot::IsAnyAimbotHotkeyActive()
{
    const bool primary = IsKeybindActive(
        config.Aim.AimbotKey,
        config.Aim.AimbotKeyMode,
        m_AimbotPrimaryKey.ToggleState,
        m_AimbotPrimaryKey.WasDown
    );

    bool secondary = false;
    if (config.Aim.AimbotSecondKeyEnabled)
    {
        secondary = IsKeybindActive(
            config.Aim.AimbotSecondKey,
            config.Aim.AimbotSecondKeyMode,
            m_AimbotSecondaryKey.ToggleState,
            m_AimbotSecondaryKey.WasDown
        );
    }
    else
    {
        m_AimbotSecondaryKey = {};
    }

    return primary || secondary;
}

bool Aimbot::IsAnyTriggerHotkeyActive()
{
    const bool primaryDown = config.Aim.TriggerKey > 0 && overlay.IsHostKeyDown(config.Aim.TriggerKey);
    const bool secondaryDown = config.Aim.TriggerSecondKeyEnabled &&
        config.Aim.TriggerSecondKey > 0 &&
        overlay.IsHostKeyDown(config.Aim.TriggerSecondKey);

    return primaryDown || secondaryDown;
}

void Aimbot::TriggerFireClick(const int holdMs)
{
    const int clampedHoldMs = std::clamp(holdMs, 0, 1200);
    const auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(m_KmboxMutex);
        Kmbox.Mouse.Left(true);
        m_TriggerMouseHeld = true;
    }

    if (clampedHoldMs <= 0)
        m_TriggerMouseReleaseAt = now;
    else
        m_TriggerMouseReleaseAt = now + std::chrono::milliseconds(clampedHoldMs);

    m_LastTriggerClick = now;
}

void Aimbot::ResetTriggerWindow()
{
    m_TriggerCandidateSince = {};
    m_LastTriggerPawn = 0;
}

void Aimbot::UpdateAimbot()
{
    auto setAimbotVisual = [&](const bool hotkeyActive, const bool hasTarget)
    {
        m_AimbotHotkeyActiveVisual.store(hotkeyActive, std::memory_order_relaxed);
        m_AimbotHasTargetVisual.store(hasTarget, std::memory_order_relaxed);
    };

    if (!ProcInfo::KmboxInitialized)
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_LockedTargetPawn = 0;
        m_AimbotHotkeyWasActive = false;
        m_LastTargetScanAt = {};
        setAimbotVisual(false, false);
        return;
    }

    const bool menuOpen = overlay.shouldRenderMenu;
    if (menuOpen)
    {
        // Block actual aimbot action while menu is open.
        m_LockedTargetPawn = 0;
        m_AimbotHotkeyWasActive = false;
        m_LastTargetScanAt = {};
    }

    if (!Offsets::Schema::m_iHealth || !Offsets::Schema::m_iTeamNum || !Offsets::Schema::m_lifeState)
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        setAimbotVisual(false, false);
        return;
    }

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !IsLikelyUserAddress(core.LocalPawn) || !IsLikelyUserAddress(core.EntityList))
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        setAimbotVisual(false, false);
        return;
    }

    const int localHealth = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_iHealth);
    const int localLifeState = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_lifeState);
    if (!IsAlive(localHealth, localLifeState))
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_LockedTargetPawn = 0;
        m_LastTargetScanAt = {};
        setAimbotVisual(false, false);
        return;
    }

    WeaponRuntimeState weapon{};
    if (!TryReadLocalWeaponState(core, weapon))
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        setAimbotVisual(false, false);
        return;
    }

    if (!weapon.IsGun || weapon.IsReloading)
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_LockedTargetPawn = 0;
        m_LastTargetScanAt = {};
        setAimbotVisual(false, false);
        return;
    }

    if (config.Aim.BlockAimbotWhenFlashed && IsPlayerEffectivelyFlashed(core.LocalPawn))
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_LockedTargetPawn = 0;
        m_LastTargetScanAt = {};
        setAimbotVisual(false, false);
        return;
    }

    const int localTeam = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_iTeamNum);

    Vector3 localOrigin{};
    if (Offsets::Schema::m_vOldOrigin)
        localOrigin = mem.Read<Vector3>(core.LocalPawn + Offsets::Schema::m_vOldOrigin);

    Vector3 localViewOffset{ 0.0f, 0.0f, kDefaultEyeHeight };
    if (Offsets::Schema::m_vecViewOffset)
    {
        const Vector3 rawViewOffset = mem.Read<Vector3>(core.LocalPawn + Offsets::Schema::m_vecViewOffset);
        if (IsNonZeroPosition(rawViewOffset))
            localViewOffset = rawViewOffset;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_TargetSwitchDelayActive && now >= m_TargetSwitchReadyAt)
        m_TargetSwitchDelayActive = false;

    const int weaponCategory = std::clamp(weapon.Category, 0, Structs::AimWeapon_Count - 1);
    Structs::AimWeaponProfile profile = config.Aim.WeaponProfiles[weaponCategory];
    if (profile.Fov <= 0.01f)
        profile.Fov = (std::max)(config.Aim.AimbotFov, 0.1f);
    if (profile.Smooth <= 0.01f)
        profile.Smooth = (std::max)(config.Aim.AimbotSmooth, 1.0f);
    profile.TargetStrategy = std::clamp(profile.TargetStrategy, 0, static_cast<int>(Structs::AimTargetStrategyNames.size()) - 1);
    profile.TargetSwitchDelayMs = (std::max)(0, profile.TargetSwitchDelayMs);
    const std::uint64_t aimbotBoneMask = NormalizeBoneMask(profile.BoneMask, Structs::AimDefaultAimbotBoneMask);

    const Vector3 localEye = localOrigin + localViewOffset;
    const Vector2 screenCenter{ ScreenCenter.x, ScreenCenter.y };

    const float baseFovPx = FovDegreesToPixels(profile.Fov);
    float displayFovPx = baseFovPx;
    if (baseFovPx <= 0.01f)
    {
        m_CurrentFovRadiusPx.store(0.0f, std::memory_order_relaxed);
        setAimbotVisual(false, false);
        return;
    }
    const bool aimbotEnabled = config.Aim.Aimbot && !menuOpen;
    const bool hotkeyActive = aimbotEnabled ? IsAnyAimbotHotkeyActive() : false;
    const bool justReleased = m_AimbotHotkeyWasActive && !hotkeyActive;
    m_AimbotHotkeyWasActive = hotkeyActive;
    if (justReleased)
    {
        m_LockedTargetPawn = 0;
        m_LastTargetScanAt = {};
    }

    std::unordered_set<std::uint64_t> visiblePawns{};
    if (config.Aim.AimVisible)
        visiblePawns = esp.GetVisiblePawnSetSnapshot();

    auto isPawnVisibleForAim = [&](const std::uint64_t pawn) -> bool
    {
        if (!config.Aim.AimVisible)
            return true;
        return visiblePawns.find(pawn) != visiblePawns.end();
    };

    auto buildCandidate = [&](const std::uint64_t pawn, TargetCandidate& outCandidate) -> bool
    {
        if (!IsLikelyUserAddress(pawn) || pawn == core.LocalPawn)
            return false;

        int health = 0;
        int team = 0;
        int lifeState = 0;
        if (!sdk.ReadBasicEntityState(pawn, health, team, lifeState))
            return false;

        if (!IsAlive(health, lifeState))
            return false;

        if (!config.Aim.AimFriendly && localTeam > 0 && team == localTeam)
            return false;

        if (!isPawnVisibleForAim(pawn))
            return false;

        Vector3 targetWorld{};
        Vector2 targetScreen{};
        bool hasTargetBone = false;

        if (Offsets::Schema::m_pGameSceneNode && Offsets::Schema::m_modelState)
        {
            const std::uint64_t sceneNode = mem.Read<std::uint64_t>(pawn + Offsets::Schema::m_pGameSceneNode);
            if (IsLikelyUserAddress(sceneNode))
            {
                const std::uint64_t boneArray = mem.Read<std::uint64_t>(sceneNode + Offsets::Schema::m_modelState + Offsets::Layout::BoneArray);
                if (IsLikelyUserAddress(boneArray))
                {
                    std::array<BoneDataRaw, kAimbotProbeBones.size()> rawBones{};
                    const auto scatter = mem.CreateScatterHandle();
                    if (scatter)
                    {
                        for (size_t i = 0; i < kAimbotProbeBones.size(); ++i)
                        {
                            const std::uint64_t address = boneArray + static_cast<std::uint64_t>(kAimbotProbeBones[i]) * Offsets::Layout::BoneStride;
                            mem.AddScatterReadRequest(scatter, address, &rawBones[i], sizeof(BoneDataRaw));
                        }

                        mem.ExecuteReadScatter(scatter);
                        mem.CloseScatterHandle(scatter);

                        float bestDistance = FLT_MAX;
                        for (size_t i = 0; i < kAimbotProbeBones.size(); ++i)
                        {
                            const int boneId = kAimbotProbeBones[i];
                            if (!IsBoneEnabledByMask(aimbotBoneMask, boneId))
                                continue;

                            const Vector3& boneWorld = rawBones[i].Position;
                            if (!IsNonZeroPosition(boneWorld))
                                continue;

                            Vector2 boneScreen{};
                            if (!sdk.WorldToScreen(boneWorld, boneScreen, core.ViewMatrix))
                                continue;

                            if (config.Aim.AimVisible)
                            {
                                if (boneScreen.x < 0.0f || boneScreen.x > Screen.x ||
                                    boneScreen.y < 0.0f || boneScreen.y > Screen.y)
                                {
                                    continue;
                                }
                            }

                            const float boneDistance = ToScreenDistance(screenCenter, boneScreen);
                            if (!hasTargetBone || boneDistance < bestDistance)
                            {
                                hasTargetBone = true;
                                bestDistance = boneDistance;
                                targetWorld = boneWorld;
                                targetScreen = boneScreen;
                            }
                        }
                    }
                }
            }
        }

        if (!hasTargetBone)
        {
            if (!Offsets::Schema::m_vOldOrigin)
                return false;

            targetWorld = mem.Read<Vector3>(pawn + Offsets::Schema::m_vOldOrigin);
            targetWorld = targetWorld + Vector3{ 0.0f, 0.0f, 72.0f };

            if (!sdk.WorldToScreen(targetWorld, targetScreen, core.ViewMatrix))
                return false;

            if (config.Aim.AimVisible)
            {
                if (targetScreen.x < 0.0f || targetScreen.x > Screen.x ||
                    targetScreen.y < 0.0f || targetScreen.y > Screen.y)
                {
                    return false;
                }
            }
        }

        const float worldDistance = std::sqrt(
            std::pow(targetWorld.x - localEye.x, 2.0f) +
            std::pow(targetWorld.y - localEye.y, 2.0f) +
            std::pow(targetWorld.z - localEye.z, 2.0f)
        );

        const float allowedFovPx = baseFovPx;

        outCandidate.Pawn = pawn;
        outCandidate.World = targetWorld;
        outCandidate.Screen = targetScreen;
        outCandidate.ScreenDistance = ToScreenDistance(screenCenter, targetScreen);
        outCandidate.WorldDistance = worldDistance;
        outCandidate.AllowedFovPx = allowedFovPx;
        outCandidate.Team = team;
        return true;
    };

    auto findBestTargetCandidate = [&](TargetCandidate& outBestCandidate) -> bool
    {
        float bestScore = FLT_MAX;
        bool foundCandidate = false;

        for (int controllerIndex = 1; controllerIndex <= kMaxControllers; ++controllerIndex)
        {
            const std::uint64_t controller = sdk.ResolveEntityFromHandle(static_cast<std::uint32_t>(controllerIndex));
            if (!IsLikelyUserAddress(controller))
                continue;

            const std::uint64_t pawn = sdk.ResolvePawnFromController(controller);
            TargetCandidate candidate{};
            if (!buildCandidate(pawn, candidate))
                continue;

            if (candidate.ScreenDistance > candidate.AllowedFovPx)
                continue;

            float score = candidate.ScreenDistance;
            if (profile.TargetStrategy == Structs::AimStrategy_Distance)
                score = candidate.WorldDistance;
            else if (profile.TargetStrategy == Structs::AimStrategy_Hybrid)
                score = candidate.ScreenDistance * 0.72f + candidate.WorldDistance * 0.015f;

            if (!foundCandidate || score < bestScore)
            {
                foundCandidate = true;
                bestScore = score;
                outBestCandidate = candidate;
            }
        }

        return foundCandidate;
    };

    TargetCandidate activeTarget{};
    bool hasActiveTarget = false;

    if (aimbotEnabled && hotkeyActive && m_LockedTargetPawn != 0)
    {
        TargetCandidate lockedCandidate{};
        if (buildCandidate(m_LockedTargetPawn, lockedCandidate) &&
            lockedCandidate.ScreenDistance <= lockedCandidate.AllowedFovPx)
        {
            activeTarget = lockedCandidate;
            hasActiveTarget = true;
            displayFovPx = lockedCandidate.AllowedFovPx;
        }
        else
        {
            m_LockedTargetPawn = 0;
            if (!m_TargetSwitchDelayActive)
            {
                m_TargetSwitchDelayActive = true;
                m_TargetSwitchReadyAt = now + std::chrono::milliseconds(profile.TargetSwitchDelayMs);
            }
        }
    }

    if (!aimbotEnabled || !hotkeyActive)
    {
        m_CurrentFovRadiusPx.store(displayFovPx, std::memory_order_relaxed);
        m_LastTargetScanAt = {};
        setAimbotVisual(aimbotEnabled && hotkeyActive, false);
        return;
    }

    if (!hasActiveTarget && !m_TargetSwitchDelayActive)
    {
        constexpr auto kTargetScanInterval = std::chrono::milliseconds(4); // 250Hz target scan
        if (m_LastTargetScanAt.time_since_epoch().count() == 0 || now - m_LastTargetScanAt >= kTargetScanInterval)
        {
            m_LastTargetScanAt = now;

            TargetCandidate bestCandidate{};
            if (findBestTargetCandidate(bestCandidate))
            {
                m_LockedTargetPawn = bestCandidate.Pawn;
                activeTarget = bestCandidate;
                hasActiveTarget = true;
                displayFovPx = bestCandidate.AllowedFovPx;
            }
        }
    }
    else if (m_TargetSwitchDelayActive)
    {
        displayFovPx = baseFovPx;
    }

    m_CurrentFovRadiusPx.store(displayFovPx, std::memory_order_relaxed);
    if (!hasActiveTarget)
    {
        setAimbotVisual(true, false);
        return;
    }

    m_LastTargetScanAt = now;
    Vector2 delta{
        activeTarget.Screen.x - screenCenter.x,
        activeTarget.Screen.y - screenCenter.y
    };

    const float rawDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const float smooth = std::clamp(profile.Smooth, 1.0f, 100.0f);
    const float baseSmoothing = std::clamp(1.0f / (0.90f + 0.09f * smooth), 0.03f, 1.0f);
    const float distanceBoost = std::clamp(rawDistance / 170.0f, 0.0f, 1.5f);
    const float smoothingFactor = std::clamp(baseSmoothing * (1.0f + distanceBoost * 0.45f), 0.03f, 1.0f);
    const float eased = smoothingFactor;
    Vector2 move{
        delta.x * eased,
        delta.y * eased
    };

    if (rawDistance > 0.001f && profile.CurveStrength > 0.001f)
    {
        const Vector2 perpendicular{
            -delta.y / rawDistance,
            delta.x / rawDistance
        };
        const float curveWindow = std::sin(eased * math::PI);
        const float curveAmount = std::clamp(rawDistance * profile.CurveStrength * 0.10f * curveWindow, 0.0f, 12.0f);
        const float curveSign = (m_LockedTargetPawn & 1ULL) ? 1.0f : -1.0f;
        move.x += perpendicular.x * curveAmount * curveSign;
        move.y += perpendicular.y * curveAmount * curveSign;
    }

    const float deadzone = (std::max)(0.0f, config.Aim.DeadzonePx);
    if (rawDistance <= deadzone)
    {
        move.x = 0.0f;
        move.y = 0.0f;
    }

    int moveX = QuantizeMouseStep(move.x);
    int moveY = QuantizeMouseStep(move.y);
    if (moveX == 0 && moveY == 0 && rawDistance > deadzone)
    {
        if (std::fabs(delta.x) > 0.15f)
            moveX = delta.x > 0.0f ? 1 : -1;
        if (std::fabs(delta.y) > 0.15f)
            moveY = delta.y > 0.0f ? 1 : -1;
    }

    if (moveX == 0 && moveY == 0)
    {
        setAimbotVisual(true, true);
        return;
    }

    setAimbotVisual(true, true);
    std::lock_guard lock(m_KmboxMutex);
    Kmbox.Mouse.Move(moveX, moveY);
}

void Aimbot::UpdateTriggerbot()
{
    const auto now = std::chrono::steady_clock::now();
    const auto triggerCycleStart = now;
    bool recordCycleMetrics = false;
    struct TriggerCycleScope
    {
        bool& Enabled;
        std::chrono::steady_clock::time_point Start;

        ~TriggerCycleScope()
        {
            if (!Enabled)
                return;

            const std::uint64_t cycleUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - Start).count()
            );
            PerfDebug::RecordTriggerCycle(cycleUs);
        }
    };
    const TriggerCycleScope triggerCycleScope{ recordCycleMetrics, triggerCycleStart };
    UpdateTriggerMouseState(now);
    auto setTriggerVisual = [&](const bool hotkeyActive, const bool hasTarget)
    {
        m_TriggerHotkeyActiveVisual.store(hotkeyActive, std::memory_order_relaxed);
        m_TriggerHasTargetVisual.store(hasTarget, std::memory_order_relaxed);
    };

    if (!config.Aim.Trigger || !ProcInfo::KmboxInitialized)
    {
        const std::uint64_t fallbackBoneMask = NormalizeBoneMask(config.Aim.TriggerBoneMask, kAllBonesMask);
        m_CurrentTriggerDetectMode.store(
            std::clamp(config.Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1),
            std::memory_order_relaxed
        );
        m_CurrentTriggerBoneMask.store(fallbackBoneMask, std::memory_order_relaxed);
        m_TriggerHotkeyWasActive = false;
        m_TriggerHotkeyDownSince = {};
        m_TriggerShotSinceHotkeyDown = false;
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    if (overlay.shouldRenderMenu)
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    if (!Offsets::Schema::m_iHealth || !Offsets::Schema::m_iTeamNum || !Offsets::Schema::m_lifeState)
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !IsLikelyUserAddress(core.LocalPawn))
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    const int configuredTriggerMode = std::clamp(
        config.Aim.TriggerDetectMode,
        0,
        static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1
    );
    const std::uint64_t configuredTriggerBoneMask = NormalizeBoneMask(config.Aim.TriggerBoneMask, kAllBonesMask);
    m_CurrentTriggerDetectMode.store(configuredTriggerMode, std::memory_order_relaxed);
    m_CurrentTriggerBoneMask.store(configuredTriggerBoneMask, std::memory_order_relaxed);
    const bool hotkeyActive = IsAnyTriggerHotkeyActive();

    if (hotkeyActive && !m_TriggerHotkeyWasActive)
    {
        m_TriggerHotkeyDownSince = now;
        m_TriggerShotSinceHotkeyDown = false;
        ResetTriggerWindow();
        PerfDebug::RecordTriggerHotkeyDown();
    }
    m_TriggerHotkeyWasActive = hotkeyActive;
    recordCycleMetrics = hotkeyActive;

    if (!hotkeyActive)
    {
        m_TriggerHotkeyDownSince = {};
        m_TriggerShotSinceHotkeyDown = false;
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    const int localHealth = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_iHealth);
    const int localLifeState = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_lifeState);
    if (!IsAlive(localHealth, localLifeState))
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    WeaponRuntimeState weapon{};
    if (!TryReadLocalWeaponState(core, weapon))
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    if (!weapon.IsGun || weapon.IsReloading)
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    if (config.Aim.BlockTriggerWhenFlashed && IsPlayerEffectivelyFlashed(core.LocalPawn))
    {
        ResetTriggerWindow();
        ReleaseTriggerMouseIfHeld();
        m_CurrentTriggerHitboxRadiusPx.store(0.0f, std::memory_order_relaxed);
        m_CurrentTriggerHeadRadiusPx.store(0.0f, std::memory_order_relaxed);
        setTriggerVisual(false, false);
        return;
    }

    TriggerRuntimeProfile profile{};
    if (weapon.IsDeagle)
    {
        const Structs::TriggerSpecialProfile& special = config.Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Deagle];
        profile.PreFireDelayMs = special.PreFireDelayMs;
        profile.PostFireIntervalMs = special.PostFireIntervalMs;
        profile.TimeoutForceFireMs = special.TimeoutForceFireMs;
        profile.HoldFireMs = special.HoldFireMs;
        profile.BoneMask = special.BoneMask;
    }
    else if (weapon.IsRevolver)
    {
        const Structs::TriggerSpecialProfile& special = config.Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Revolver];
        profile.PreFireDelayMs = special.PreFireDelayMs;
        profile.PostFireIntervalMs = special.PostFireIntervalMs;
        profile.TimeoutForceFireMs = special.TimeoutForceFireMs;
        profile.HoldFireMs = special.HoldFireMs;
        profile.BoneMask = special.BoneMask;
    }
    else
    {
        const int category = std::clamp(weapon.Category, 0, Structs::AimWeapon_Count - 1);
        const Structs::TriggerWeaponProfile& base = config.Aim.TriggerProfiles[category];
        profile.PreFireDelayMs = base.PreFireDelayMs;
        profile.PostFireIntervalMs = base.PostFireIntervalMs;
        profile.TimeoutForceFireMs = base.TimeoutForceFireMs;
        profile.HoldFireMs = kDefaultTriggerHoldMs;
        profile.BoneMask = base.BoneMask;
    }

    profile.PreFireDelayMs = std::clamp(profile.PreFireDelayMs, 0, 2000);
    profile.PostFireIntervalMs = std::clamp(profile.PostFireIntervalMs, 0, 3000);
    profile.TimeoutForceFireMs = std::clamp(profile.TimeoutForceFireMs, 0, 5000);
    profile.HoldFireMs = std::clamp(profile.HoldFireMs, 0, 1200);
    profile.BoneMask = NormalizeBoneMask(profile.BoneMask, configuredTriggerBoneMask);

    const float unifiedRadius = std::clamp(config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
    const float hitboxScale = std::clamp(config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
    const float hitboxAddPx = std::clamp(config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
    const float effectiveBodyRadius = std::clamp(unifiedRadius * hitboxScale + hitboxAddPx, 0.5f, 80.0f);
    const float headRadiusBase = std::clamp(config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);
    const float effectiveHeadRadius = std::clamp(headRadiusBase * hitboxScale + hitboxAddPx, 0.5f, 100.0f);

    const std::uint64_t triggerBoneMask = profile.BoneMask;
    const int triggerDetectMode = configuredTriggerMode;
    m_CurrentTriggerHitboxRadiusPx.store(effectiveBodyRadius, std::memory_order_relaxed);
    m_CurrentTriggerHeadRadiusPx.store(effectiveHeadRadius, std::memory_order_relaxed);
    m_CurrentTriggerBoneMask.store(triggerBoneMask, std::memory_order_relaxed);
    m_CurrentTriggerDetectMode.store(triggerDetectMode, std::memory_order_relaxed);

    const bool postFireIntervalActive = m_LastTriggerClick.time_since_epoch().count() != 0 &&
        now - m_LastTriggerClick < std::chrono::milliseconds(profile.PostFireIntervalMs);
    if (postFireIntervalActive && m_TriggerShotSinceHotkeyDown)
    {
        setTriggerVisual(true, false);
        return;
    }

    // Force-fire timeout should not wait for heavy entity scans.
    if (profile.TimeoutForceFireMs > 0 &&
        !m_TriggerShotSinceHotkeyDown &&
        m_TriggerHotkeyDownSince.time_since_epoch().count() != 0)
    {
        const auto timeoutNow = std::chrono::steady_clock::now();
        const std::uint64_t hotkeyAgeUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(timeoutNow - m_TriggerHotkeyDownSince).count()
        );
        const bool timeoutHit = hotkeyAgeUs >= static_cast<std::uint64_t>(profile.TimeoutForceFireMs) * 1000ull;
        PerfDebug::RecordTriggerTimeoutCheck(hotkeyAgeUs, timeoutHit);
        if (timeoutHit)
        {
            const std::uint64_t decisionUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(timeoutNow - triggerCycleStart).count()
            );
            setTriggerVisual(true, false);
            const auto sendStart = std::chrono::steady_clock::now();
            TriggerFireClick(profile.HoldFireMs);
            const std::uint64_t sendUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - sendStart).count()
            );
            PerfDebug::RecordTriggerFire(hotkeyAgeUs, decisionUs, sendUs, true);
            UpdateTriggerMouseState(std::chrono::steady_clock::now());
            m_TriggerShotSinceHotkeyDown = true;
            ResetTriggerWindow();
            return;
        }
    }

    const auto scanStart = std::chrono::steady_clock::now();
    const int localTeam = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_iTeamNum);
    const Vector2 screenCenter{ ScreenCenter.x, ScreenCenter.y };
    std::unordered_set<std::uint64_t> visiblePawns{};
    if (config.Aim.AimVisible)
        visiblePawns = esp.GetVisiblePawnSetSnapshot();

    auto isPawnVisibleForAim = [&](const std::uint64_t pawn) -> bool
    {
        if (!config.Aim.AimVisible)
            return true;
        return visiblePawns.find(pawn) != visiblePawns.end();
    };

    std::uint64_t triggerPawn = 0;
    float triggerDistance = FLT_MAX;

    const std::vector<TriggerBoneSnapshot> triggerSnapshots =
        triggerDetectMode == Structs::TriggerDetect_BoneHitbox
        ? esp.GetTriggerBoneSnapshots()
        : std::vector<TriggerBoneSnapshot>{};
    if (triggerDetectMode == Structs::TriggerDetect_BoneHitbox)
        PerfDebug::RecordTriggerSnapshotFrame(triggerSnapshots.empty());

    auto canUseTargetSnapshot = [&](const TriggerBoneSnapshot& target) -> bool
    {
        if (!IsLikelyUserAddress(target.Pawn) || target.Pawn == core.LocalPawn)
            return false;

        if (!IsAlive(target.Health, target.LifeState))
            return false;

        if (!config.Aim.AimFriendly && localTeam > 0 && target.Team == localTeam)
            return false;

        if (config.Aim.AimVisible && !target.IsVisible)
            return false;

        return true;
    };

    auto tryEvaluateTriggerSnapshotByBone = [&](const TriggerBoneSnapshot& target)
    {
        if (!canUseTargetSnapshot(target))
            return;

        auto pointToAabbDistance = [](const Vector2& point, const ImVec2& bmin, const ImVec2& bmax) -> float
        {
            const float clampedX = std::clamp(point.x, bmin.x, bmax.x);
            const float clampedY = std::clamp(point.y, bmin.y, bmax.y);
            const float dx = point.x - clampedX;
            const float dy = point.y - clampedY;
            return std::sqrt(dx * dx + dy * dy);
        };

        const float coarseRadius = std::clamp(
            (std::max)(effectiveHeadRadius * kTriggerHeadScaleFixed, effectiveBodyRadius * kTriggerTorsoScaleFixed) * 3.0f,
            24.0f,
            520.0f
        );
        if (pointToAabbDistance(screenCenter, target.BoxMin, target.BoxMax) > coarseRadius)
            return;

        float bestDistance = FLT_MAX;
        if (!IsCrosshairOnSnapshotBoneHitbox(
            target,
            screenCenter,
            triggerBoneMask,
            effectiveBodyRadius,
            effectiveHeadRadius,
            bestDistance))
        {
            return;
        }

        if (!triggerPawn || bestDistance < triggerDistance)
        {
            triggerPawn = target.Pawn;
            triggerDistance = bestDistance;
        }
    };

    auto findSnapshotByPawn = [&](const std::uint64_t pawn) -> const TriggerBoneSnapshot*
    {
        if (pawn == 0 || triggerSnapshots.empty())
            return nullptr;

        for (const TriggerBoneSnapshot& snapshot : triggerSnapshots)
        {
            if (snapshot.Pawn == pawn)
                return &snapshot;
        }

        return nullptr;
    };

    auto canUseTargetPawn = [&](const std::uint64_t pawn) -> bool
    {
        if (!IsLikelyUserAddress(pawn) || pawn == core.LocalPawn)
            return false;

        int targetHealth = 0;
        int targetTeam = 0;
        int targetLifeState = 0;
        if (!sdk.ReadBasicEntityState(pawn, targetHealth, targetTeam, targetLifeState))
            return false;

        if (!IsAlive(targetHealth, targetLifeState))
            return false;

        if (!config.Aim.AimFriendly && localTeam > 0 && targetTeam == localTeam)
            return false;

        if (!isPawnVisibleForAim(pawn))
            return false;

        return true;
    };

    auto resolveHintedPawnFromCrosshairEntity = [&]() -> std::uint64_t
    {
        if (!Offsets::Schema::m_iIDEntIndex)
            return 0;

        const int idEntIndex = mem.Read<int>(core.LocalPawn + Offsets::Schema::m_iIDEntIndex);
        if (idEntIndex <= 0)
            return 0;

        const std::uint64_t hintedEntity = sdk.ResolveEntityFromHandle(static_cast<std::uint32_t>(idEntIndex), core.EntityList);
        if (!IsLikelyUserAddress(hintedEntity))
            return 0;

        std::uint64_t hintedPawn = hintedEntity;
        int hintedHealth = 0;
        int hintedTeam = 0;
        int hintedLifeState = 0;
        if (!sdk.ReadBasicEntityState(hintedPawn, hintedHealth, hintedTeam, hintedLifeState))
            hintedPawn = sdk.ResolvePawnFromController(hintedEntity);

        return hintedPawn;
    };

    if (triggerDetectMode == Structs::TriggerDetect_CrosshairEntity)
    {
        const std::uint64_t hintedPawn = resolveHintedPawnFromCrosshairEntity();
        if (canUseTargetPawn(hintedPawn))
        {
            triggerPawn = hintedPawn;
            triggerDistance = 0.0f;
        }
    }
    else
    {
        if (!triggerSnapshots.empty())
        {
            // Fast-path current/last target to reduce perceived trigger latency.
            if (const TriggerBoneSnapshot* lastSnapshot = findSnapshotByPawn(m_LastTriggerPawn))
                tryEvaluateTriggerSnapshotByBone(*lastSnapshot);

            if (!triggerPawn)
            {
                for (const TriggerBoneSnapshot& snapshot : triggerSnapshots)
                    tryEvaluateTriggerSnapshotByBone(snapshot);
            }
        }
    }

    const std::uint64_t scanUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - scanStart).count()
    );
    PerfDebug::RecordTriggerScan(scanUs, triggerPawn != 0);

    bool shouldFireByDetection = false;
    bool preFireGateHit = false;
    if (triggerPawn)
    {
        if (m_LastTriggerPawn != triggerPawn)
        {
            m_LastTriggerPawn = triggerPawn;
            m_TriggerCandidateSince = now;
        }

        if (profile.PreFireDelayMs <= 0)
            shouldFireByDetection = true;
        else if (m_TriggerCandidateSince.time_since_epoch().count() != 0 &&
            now - m_TriggerCandidateSince >= std::chrono::milliseconds(profile.PreFireDelayMs))
        {
            shouldFireByDetection = true;
        }
        else
        {
            preFireGateHit = true;
        }
    }
    else
    {
        ResetTriggerWindow();
    }

    if (!shouldFireByDetection)
    {
        if (preFireGateHit)
            PerfDebug::RecordTriggerPreFireGate();
        setTriggerVisual(true, triggerPawn != 0);
        return;
    }

    const auto fireNow = std::chrono::steady_clock::now();
    const std::uint64_t hotkeyAgeUs = m_TriggerHotkeyDownSince.time_since_epoch().count() != 0
        ? static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(fireNow - m_TriggerHotkeyDownSince).count())
        : 0ull;
    const std::uint64_t decisionUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(fireNow - triggerCycleStart).count()
    );
    setTriggerVisual(true, true);
    const auto sendStart = std::chrono::steady_clock::now();
    TriggerFireClick(profile.HoldFireMs);
    const std::uint64_t sendUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - sendStart).count()
    );
    PerfDebug::RecordTriggerFire(hotkeyAgeUs, decisionUs, sendUs, false);
    UpdateTriggerMouseState(std::chrono::steady_clock::now());
    m_TriggerShotSinceHotkeyDown = true;
}
