#include <Pch.hpp>
#include <SDK.hpp>
#include "ESP.hpp"
#include <Aimbot/Aimbot.hpp>
#include <Overlay/Localization.hpp>
#include <array>
#include <cfloat>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::string LocalizeRuntimeText(const std::string& text)
    {
        if (text.empty() || !Localization::IsChinese())
            return text;

        return Localization::TranslateImpl(text);
    }

    struct BoneDataRaw
    {
        Vector3 Position{};
        std::uint8_t Padding[0x14]{};
    };

    constexpr int kMaxControllers = 64;
    constexpr int kAliveLifeStateA = 0;
    constexpr int kAliveLifeStateB = 256;
    constexpr int kHeadBone = 6;
    constexpr float kTriggerHeadScaleFixed = 7.0f;
    constexpr float kTriggerTorsoScaleFixed = 8.0f;
    constexpr float kTriggerArmsScaleFixed = 6.0f;
    constexpr float kTriggerLegsScaleFixed = 5.0f;

    constexpr std::array<int, 17> kTrackedBones = {
        0, 2, 4, 5, 6,
        8, 9, 10,
        13, 14, 15,
        22, 23, 24,
        25, 26, 27
    };

    constexpr std::array<std::pair<int, int>, 16> kBoneLinks = {
        std::pair{ 0, 2 },
        std::pair{ 2, 4 },
        std::pair{ 4, 5 },
        std::pair{ 5, 6 },

        std::pair{ 4, 8 },
        std::pair{ 8, 9 },
        std::pair{ 9, 10 },

        std::pair{ 4, 13 },
        std::pair{ 13, 14 },
        std::pair{ 14, 15 },

        std::pair{ 0, 22 },
        std::pair{ 22, 23 },
        std::pair{ 23, 24 },

        std::pair{ 0, 25 },
        std::pair{ 25, 26 },
        std::pair{ 26, 27 }
    };

    struct DebugBoneLink
    {
        int FromBone = 0;
        int ToBone = 0;
        std::uint64_t BoneMask = 0;
    };

    constexpr std::array<DebugBoneLink, 16> kDebugBoneLinks = {
        DebugBoneLink{ 0, 2, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(2) },
        DebugBoneLink{ 2, 4, Structs::BoneMaskFromBoneId(2) | Structs::BoneMaskFromBoneId(4) },
        DebugBoneLink{ 4, 5, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(5) },
        DebugBoneLink{ 5, 6, Structs::BoneMaskFromBoneId(5) | Structs::BoneMaskFromBoneId(6) },

        DebugBoneLink{ 4, 8, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(8) },
        DebugBoneLink{ 8, 9, Structs::BoneMaskFromBoneId(8) | Structs::BoneMaskFromBoneId(9) },
        DebugBoneLink{ 9, 10, Structs::BoneMaskFromBoneId(9) | Structs::BoneMaskFromBoneId(10) },

        DebugBoneLink{ 4, 13, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(13) },
        DebugBoneLink{ 13, 14, Structs::BoneMaskFromBoneId(13) | Structs::BoneMaskFromBoneId(14) },
        DebugBoneLink{ 14, 15, Structs::BoneMaskFromBoneId(14) | Structs::BoneMaskFromBoneId(15) },

        DebugBoneLink{ 0, 22, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(22) },
        DebugBoneLink{ 22, 23, Structs::BoneMaskFromBoneId(22) | Structs::BoneMaskFromBoneId(23) },
        DebugBoneLink{ 23, 24, Structs::BoneMaskFromBoneId(23) | Structs::BoneMaskFromBoneId(24) },

        DebugBoneLink{ 0, 25, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(25) },
        DebugBoneLink{ 25, 26, Structs::BoneMaskFromBoneId(25) | Structs::BoneMaskFromBoneId(26) },
        DebugBoneLink{ 26, 27, Structs::BoneMaskFromBoneId(26) | Structs::BoneMaskFromBoneId(27) }
    };

    constexpr std::uint64_t kAllBonesMask = Structs::AimAllBoneMask;

    std::string WeaponIdToName(const int weaponId)
    {
        switch (weaponId)
        {
        case 1: return "Deagle";
        case 2: return "Dual Berettas";
        case 3: return "Five-Seven";
        case 4: return "Glock-18";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "PP-Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus x27";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 42: return "Knife";
        case 43: return "Flashbang";
        case 44: return "HE Grenade";
        case 45: return "Smoke";
        case 46: return "Molotov";
        case 47: return "Decoy";
        case 48: return "Incendiary";
        case 49: return "C4";
        case 57: return "Healthshot";
        case 59: return "Knife (T)";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75 Auto";
        case 64: return "R8 Revolver";
        default: return {};
        }
    }

    float DistanceSquared3D(const Vector3& a, const Vector3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    bool IsLikelyUserAddress(const uint64_t address)
    {
        return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
    }

    bool IsNonZeroPosition(const Vector3& position)
    {
        return std::abs(position.x) > 0.01f ||
            std::abs(position.y) > 0.01f ||
            std::abs(position.z) > 0.01f;
    }

    float Dot3(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vector3 Cross3(const Vector3& a, const Vector3& b)
    {
        return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float Length3(const Vector3& v)
    {
        return std::sqrt(Dot3(v, v));
    }

    Vector3 Normalize3(const Vector3& v)
    {
        const float length = Length3(v);
        if (length <= 0.0001f)
            return Vector3{};

        return v / length;
    }

    float Distance2D(const Vector2& a, const Vector2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float DistancePointToSegment2D(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd)
    {
        const float vx = segmentEnd.x - segmentStart.x;
        const float vy = segmentEnd.y - segmentStart.y;
        const float wx = point.x - segmentStart.x;
        const float wy = point.y - segmentStart.y;
        const float segmentLenSq = vx * vx + vy * vy;

        if (segmentLenSq < 0.0001f)
            return Distance2D(point, segmentStart);

        const float t = std::clamp((wx * vx + wy * vy) / segmentLenSq, 0.0f, 1.0f);
        const Vector2 closest{
            segmentStart.x + t * vx,
            segmentStart.y + t * vy
        };

        return Distance2D(point, closest);
    }

    float BoneLinkRadiusScale(const int fromBone, const int toBone)
    {
        const auto match = [&](const int a, const int b)
        {
            return (fromBone == a && toBone == b) || (fromBone == b && toBone == a);
        };

        if (match(0, 2)) return 1.45f;
        if (match(2, 4)) return 1.40f;
        if (match(4, 5)) return 1.25f;
        if (match(5, 6)) return 1.10f;

        if (match(4, 8) || match(4, 13)) return 1.12f;
        if (match(8, 9) || match(13, 14)) return 0.92f;
        if (match(9, 10) || match(14, 15)) return 0.78f;

        if (match(0, 22) || match(0, 25)) return 1.28f;
        if (match(22, 23) || match(25, 26)) return 1.12f;
        if (match(23, 24) || match(26, 27)) return 0.90f;

        return 1.0f;
    }

    bool BuildProjectedSegmentBoxCorners(
        const Vector3& fromWorld,
        const Vector3& toWorld,
        const float radiusPx,
        const Matrix& viewMatrix,
        std::array<Vector2, 8>& outScreenCorners)
    {
        const Vector3 segment = toWorld - fromWorld;
        const float segmentLength = Length3(segment);
        if (segmentLength < 0.001f)
            return false;

        const Vector3 center = (fromWorld + toWorld) * 0.5f;
        Vector2 centerScreen{};
        if (!sdk.WorldToScreen(center, centerScreen, viewMatrix))
            return false;

        const Vector3 dir = segment / segmentLength;
        const Vector3 helperAxis = std::fabs(dir.z) < 0.95f
            ? Vector3{ 0.0f, 0.0f, 1.0f }
            : Vector3{ 0.0f, 1.0f, 0.0f };

        Vector3 right = Normalize3(Cross3(dir, helperAxis));
        if (Length3(right) < 0.001f)
        {
            const Vector3 fallbackAxis = std::fabs(dir.x) < 0.95f
                ? Vector3{ 1.0f, 0.0f, 0.0f }
                : Vector3{ 0.0f, 1.0f, 0.0f };
            right = Normalize3(Cross3(dir, fallbackAxis));
        }
        if (Length3(right) < 0.001f)
            return false;

        Vector3 up = Normalize3(Cross3(right, dir));
        if (Length3(up) < 0.001f)
            return false;

        auto estimatePxPerWorld = [&](const Vector3& axis) -> float
        {
            Vector2 axisScreen{};
            if (!sdk.WorldToScreen(center + axis, axisScreen, viewMatrix))
                return 0.0f;
            return Distance2D(centerScreen, axisScreen);
        };

        const float pxPerWorldRight = estimatePxPerWorld(right);
        const float pxPerWorldUp = estimatePxPerWorld(up);
        float pxPerWorld = 0.0f;
        if (pxPerWorldRight > 0.001f && pxPerWorldUp > 0.001f)
            pxPerWorld = 0.5f * (pxPerWorldRight + pxPerWorldUp);
        else
            pxPerWorld = (std::max)(pxPerWorldRight, pxPerWorldUp);

        if (pxPerWorld < 0.001f)
            return false;

        const float halfWidthWorld = std::clamp(radiusPx / pxPerWorld, 0.01f, 40.0f);
        const float halfLengthWorld = (std::max)(segmentLength * 0.5f, halfWidthWorld * 0.50f);

        const Vector3 axisForward = dir * halfLengthWorld;
        const Vector3 axisRight = right * halfWidthWorld;
        const Vector3 axisUp = up * halfWidthWorld;

        const std::array<Vector3, 8> worldCorners = {
            center - axisForward - axisRight - axisUp,
            center - axisForward + axisRight - axisUp,
            center - axisForward + axisRight + axisUp,
            center - axisForward - axisRight + axisUp,
            center + axisForward - axisRight - axisUp,
            center + axisForward + axisRight - axisUp,
            center + axisForward + axisRight + axisUp,
            center + axisForward - axisRight + axisUp
        };

        for (size_t i = 0; i < worldCorners.size(); ++i)
        {
            if (!sdk.WorldToScreen(worldCorners[i], outScreenCorners[i], viewMatrix))
                return false;
        }

        return true;
    }

    int NormalizeBombSiteRaw(const int rawSite)
    {
        if (rawSite == 0)
            return 0; // A
        if (rawSite == 1 || rawSite == 2)
            return 1; // B (some dumps are 1-based)
        if (rawSite == 'A' || rawSite == 'a')
            return 0;
        if (rawSite == 'B' || rawSite == 'b')
            return 1;
        return -1;
    }

    float ReadGlobalCurrentTime()
    {
        if (!Globals::ClientBase || !Offsets::Client::dwGlobalVars)
            return 0.0f;

        const uint64_t globalVars = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGlobalVars);
        if (!IsLikelyUserAddress(globalVars))
            return 0.0f;

        float currentTime = mem.Read<float>(globalVars + 0x2C);
        if (currentTime <= 0.0f)
            currentTime = mem.Read<float>(globalVars + 0x30);
        return currentTime;
    }

    struct SampledEntityData
    {
        int HandleIndex = 0;
        uint64_t Controller = 0;

        uint32_t PawnHandle = 0;
        uint64_t Pawn = 0;

        int Health = 0;
        int Team = 0;
        int LifeState = 0;
        int MaxHealth = 100;

        uint64_t SceneNode = 0;
        Vector3 OldOrigin{};
        Vector3 AbsOrigin{};
        bool HasAbsOrigin = false;
        Vector3 ViewOffset{ 0.0f, 0.0f, 64.0f };
        uint64_t BoneArray = 0;

        int Armor = 0;
        bool IsScoped = false;
        bool HasDefuser = false;
        float FlashDuration = 0.0f;
        bool RefreshStatus = false;
    };
}

void ESP::RenderWatermark(ImDrawList* drawList) const
{
    if (!config.Visuals.Watermark)
        return;

    const std::string text = "Cosmic ESP";
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

    const ImVec2 basePos = ImVec2(12.0f, 12.0f);
    const ImVec2 bgMin = basePos - ImVec2(6.0f, 4.0f);
    const ImVec2 bgMax = basePos + textSize + ImVec2(6.0f, 4.0f);

    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(18, 18, 18, 165), 4.0f);
    drawList->AddRect(bgMin, bgMax, IM_COL32(255, 255, 255, 55), 4.0f);
    drawList->AddText(basePos, ToImColor(config.Visuals.WatermarkColor), text.c_str());
}

void ESP::RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player) const
{
    const ImU32 boxColor = GetBoxColor(player.IsVisible);

    if (config.Visuals.Box)
    {
        drawList->AddRect(
            player.BoxMin,
            player.BoxMax,
            boxColor,
            0.0f,
            0,
            1.3f
        );
    }

    if (config.Visuals.Health)
    {
        const float boxHeight = player.BoxMax.y - player.BoxMin.y;
        const float healthRatio = std::clamp(static_cast<float>(player.Health) / static_cast<float>((std::max)(player.MaxHealth, 1)), 0.0f, 1.0f);

        const ImVec2 healthBgMin(player.BoxMin.x - 6.0f, player.BoxMin.y);
        const ImVec2 healthBgMax(player.BoxMin.x - 3.0f, player.BoxMax.y);
        const ImVec2 healthFillMin(player.BoxMin.x - 6.0f, player.BoxMax.y - boxHeight * healthRatio);
        const ImVec2 healthFillMax(player.BoxMin.x - 3.0f, player.BoxMax.y);

        const ImU32 healthColor = IM_COL32(
            static_cast<int>((1.0f - healthRatio) * 255.0f),
            static_cast<int>(healthRatio * 255.0f),
            80,
            255
        );

        drawList->AddRectFilled(healthBgMin, healthBgMax, IM_COL32(20, 20, 20, 185));
        drawList->AddRectFilled(healthFillMin, healthFillMax, healthColor);
        drawList->AddRect(healthBgMin, healthBgMax, IM_COL32(0, 0, 0, 220));
    }

    if (config.Visuals.Name)
    {
        const std::string nameText = player.Name.empty() ? Localization::Pick("Unknown", "未知") : player.Name;
        const ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());

        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMin.y - textSize.y - 3.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.NameColor), nameText.c_str());
    }

    if (config.Visuals.Weapon && !player.WeaponName.empty())
    {
        const std::string weaponText = LocalizeRuntimeText(player.WeaponName);
        const ImVec2 textSize = ImGui::CalcTextSize(weaponText.c_str());
        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMax.y + 2.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.WeaponColor), weaponText.c_str());
    }

    if (config.Visuals.Bones)
    {
        ImU32 bonesColor = ToImColor(config.Visuals.BonesColor);
        if (config.Visuals.VisibleCheck && player.IsVisible)
            bonesColor = ToImColor(config.Visuals.BonesColorVisible);
        RenderSkeleton(drawList, player, bonesColor);
    }

    if (config.Aim.TriggerHitboxDebug)
    {
        RenderTriggerHitboxDebug(drawList, player);
    }

    struct StatusLine
    {
        std::string Text{};
        ImU32 Color = IM_COL32(220, 220, 220, 255);
    };

    std::vector<StatusLine> statusLines{};
    statusLines.reserve(5);

    if (player.IsScoped)
        statusLines.push_back({ Localization::Pick("Scoped", "开镜"), IM_COL32(220, 220, 220, 255) });

    if (player.FlashDuration > 0.01f)
        statusLines.push_back({ Localization::Pick("Flashed", "致盲"), IM_COL32(255, 214, 120, 255) });

    if (config.Visuals.Armor)
        statusLines.push_back({ std::string(Localization::Pick("AR:", "甲:")) + std::to_string(player.Armor), ToImColor(config.Visuals.ArmorColor) });

    if (config.Visuals.Money && player.ShowMoney)
        statusLines.push_back({ "$" + std::to_string(player.Money), ToImColor(config.Visuals.MoneyColor) });

    if (config.Visuals.Defuser && player.HasDefuser)
        statusLines.push_back({ Localization::Pick("Kit", "拆弹钳"), ToImColor(config.Visuals.DefuserColor) });

    float lineOffset = 0.0f;
    for (const StatusLine& line : statusLines)
    {
        const ImVec2 textPos(player.BoxMax.x + 4.0f, player.BoxMin.y + lineOffset);
        drawList->AddText(textPos, line.Color, line.Text.c_str());
        lineOffset += ImGui::GetFontSize() + 1.0f;
    }
}

void ESP::RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, const ImU32 color) const
{
    if (player.Bones.empty())
        return;

    auto getBone = [&](const int index) -> const BonePoint*
    {
        for (const BonePoint& bone : player.Bones)
        {
            if (bone.Index == index)
                return &bone;
        }

        return nullptr;
    };

    for (const auto& [from, to] : kBoneLinks)
    {
        const BonePoint* fromBone = getBone(from);
        const BonePoint* toBone = getBone(to);
        if (!fromBone || !toBone)
            continue;

        if (!fromBone->OnScreen || !toBone->OnScreen)
            continue;

        drawList->AddLine(fromBone->Screen.ToImVec2(), toBone->Screen.ToImVec2(), color, 1.0f);
    }
}

void ESP::RenderTriggerHitboxDebug(ImDrawList* drawList, const PlayerEspSnapshot& player) const
{
    if (!drawList || player.Bones.empty())
        return;

    const float unifiedRadius = std::clamp(config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
    const float hitboxScale = std::clamp(config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
    const float hitboxAddPx = std::clamp(config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
    const float headBaseRadius = std::clamp(config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);

    const float hitboxRadiusPx = std::clamp(unifiedRadius * hitboxScale + hitboxAddPx, 0.5f, 80.0f);
    const float headRadiusPx = std::clamp(headBaseRadius * hitboxScale + hitboxAddPx, 0.5f, 100.0f);

    std::uint64_t boneMask = aim.GetCurrentTriggerBoneMask() & kAllBonesMask;
    if (boneMask == 0ull)
        boneMask = kAllBonesMask;

    const Vector2 crosshair{ ScreenCenter.x, ScreenCenter.y };
    float bestNormalizedDistance = FLT_MAX;
    int activeFromBone = -1;
    int activeToBone = -1;

    auto getBone = [&](const int index) -> const BonePoint*
    {
        for (const BonePoint& bone : player.Bones)
        {
            if (bone.Index == index)
                return &bone;
        }

        return nullptr;
    };

    float distanceScale = 1.0f;
    const BonePoint* headBoneForSizing = getBone(Structs::AimHeadBoneId);
    const BonePoint* pelvisBoneForSizing = getBone(0);
    if (headBoneForSizing && pelvisBoneForSizing && headBoneForSizing->OnScreen && pelvisBoneForSizing->OnScreen)
    {
        constexpr float kReferenceBodyHeightPx = 150.0f;
        const float bodyHeight = (std::max)(1.0f, std::fabs(pelvisBoneForSizing->Screen.y - headBoneForSizing->Screen.y));
        distanceScale = std::clamp(bodyHeight / kReferenceBodyHeightPx, 0.20f, 3.00f);
    }
    const float adaptiveBodyRadiusPx = std::clamp(hitboxRadiusPx * distanceScale, 0.5f, 100.0f);
    const float adaptiveHeadRadiusPx = std::clamp(headRadiusPx * distanceScale, 0.5f, 100.0f);
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

    for (const DebugBoneLink& link : kDebugBoneLinks)
    {
        const bool endpointSelected =
            (boneMask & Structs::BoneMaskFromBoneId(link.FromBone)) != 0ull ||
            (boneMask & Structs::BoneMaskFromBoneId(link.ToBone)) != 0ull;
        if (!endpointSelected)
            continue;

        const BonePoint* fromBone = getBone(link.FromBone);
        const BonePoint* toBone = getBone(link.ToBone);
        if (!fromBone || !toBone || !fromBone->OnScreen || !toBone->OnScreen)
            continue;

        const float distancePx = DistancePointToSegment2D(crosshair, fromBone->Screen, toBone->Screen);
        const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
            ? 0
            : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                ? hitboxRegionFromBone(link.FromBone)
                : 1);
        const float threshold = adaptiveBodyRadiusPx * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
        const float normalized = distancePx / threshold;
        if (normalized < bestNormalizedDistance)
        {
            bestNormalizedDistance = normalized;
            activeFromBone = link.FromBone;
            activeToBone = link.ToBone;
        }
    }

    const bool hasActiveSegment = bestNormalizedDistance <= 1.0f;
    const ImU32 baseColor = ToImColor(config.Aim.TriggerHitboxDebugColor);
    const ImU32 activeColor = ToImColor(config.Aim.TriggerHitboxDebugActiveColor);
    const float lineThickness = std::clamp(config.Aim.TriggerHitboxDebugThickness, 0.5f, 4.0f);

    constexpr std::array<std::pair<int, int>, 12> kBoxEdges = {
        std::pair{ 0, 1 }, std::pair{ 1, 2 }, std::pair{ 2, 3 }, std::pair{ 3, 0 },
        std::pair{ 4, 5 }, std::pair{ 5, 6 }, std::pair{ 6, 7 }, std::pair{ 7, 4 },
        std::pair{ 0, 4 }, std::pair{ 1, 5 }, std::pair{ 2, 6 }, std::pair{ 3, 7 }
    };

    const Matrix viewMatrix = Globals::ViewMatrix;
    for (const DebugBoneLink& link : kDebugBoneLinks)
    {
        const bool endpointSelected =
            (boneMask & Structs::BoneMaskFromBoneId(link.FromBone)) != 0ull ||
            (boneMask & Structs::BoneMaskFromBoneId(link.ToBone)) != 0ull;
        if (!endpointSelected)
            continue;

        const BonePoint* fromBone = getBone(link.FromBone);
        const BonePoint* toBone = getBone(link.ToBone);
        if (!fromBone || !toBone)
            continue;

        if (!IsNonZeroPosition(fromBone->World) || !IsNonZeroPosition(toBone->World))
            continue;

        const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
            ? 0
            : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                ? hitboxRegionFromBone(link.FromBone)
                : 1);
        const float segmentRadiusPx = adaptiveBodyRadiusPx * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
        std::array<Vector2, 8> projectedCorners{};
        if (!BuildProjectedSegmentBoxCorners(fromBone->World, toBone->World, segmentRadiusPx, viewMatrix, projectedCorners))
            continue;

        const bool isActive = hasActiveSegment && link.FromBone == activeFromBone && link.ToBone == activeToBone;
        const ImU32 color = isActive ? activeColor : baseColor;

        for (const auto& [edgeStart, edgeEnd] : kBoxEdges)
        {
            drawList->AddLine(
                projectedCorners[edgeStart].ToImVec2(),
                projectedCorners[edgeEnd].ToImVec2(),
                color,
                lineThickness
            );
        }
    }

    if (config.Aim.TriggerHeadSphereDebug &&
        (boneMask & Structs::BoneMaskFromBoneId(Structs::AimHeadBoneId)) != 0ull)
    {
        const BonePoint* headBone = getBone(Structs::AimHeadBoneId);
        if (headBone && headBone->OnScreen)
        {
            const float headThreshold = (std::max)(0.5f, adaptiveHeadRadiusPx * hitboxRegionScale(0));
            const bool headActive = Distance2D(crosshair, headBone->Screen) <= headThreshold;
            drawList->AddCircle(
                headBone->Screen.ToImVec2(),
                headThreshold,
                headActive ? activeColor : baseColor,
                48,
                (std::max)(1.0f, lineThickness + 0.2f)
            );
        }
    }
}

void ESP::RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const
{
    if (!c4.Valid || !config.Visuals.C4)
        return;

    auto formatSeconds1 = [](const float value) -> std::string
    {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%.1f", (std::max)(0.0f, value));
        return buffer;
    };

    if (c4.OnScreen)
    {
        const ImU32 markerColor = c4.Planted ? ToImColor(config.Visuals.C4Color) : IM_COL32(240, 220, 70, 255);
        drawList->AddCircleFilled(c4.Screen.ToImVec2(), 4.0f, markerColor, 12);
        drawList->AddCircle(c4.Screen.ToImVec2(), 8.0f, markerColor, 12, 1.2f);
        drawList->AddText(
            ImVec2(c4.Screen.x + 9.0f, c4.Screen.y - 15.0f),
            markerColor,
            c4.Planted ? Localization::Pick("C4(Planted)", "C4(已安放)") : "C4"
        );
    }

    if (!c4.Planted)
        return;

    std::string siteName = Localization::Pick("Unknown", "未知");
    if (c4.BombSite == 0)
        siteName = "A";
    else if (c4.BombSite == 1)
        siteName = "B";

    const std::string line1 =
        std::string(Localization::Pick("C4 Site: ", "C4点位: ")) +
        siteName + " | " +
        (c4.BeingDefused ? Localization::Pick("Defusing", "拆弹中") : Localization::Pick("Not Defusing", "未拆弹"));

    std::string line2 = std::string(Localization::Pick("Explode: ", "爆炸: ")) + formatSeconds1(c4.TimeRemaining) + "s  " + Localization::Pick("Defuse: ", "拆弹: ");
    if (c4.BeingDefused)
        line2 += formatSeconds1(c4.DefuseCountDown) + "s";
    else
        line2 += "--";

    std::string line3 = Localization::Pick("Defuse Result: --", "拆弹结果: --");
    if (c4.BeingDefused)
        line3 = std::string(Localization::Pick("Defuse Result: ", "拆弹结果: ")) + (c4.CanDefuse ? Localization::Pick("SUCCESS", "成功") : Localization::Pick("FAIL", "失败"));

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    constexpr float panelW = 250.0f;
    constexpr float panelH = 84.0f;
    const float panelX = std::clamp(config.Visuals.C4PanelPosX * displaySize.x, 8.0f, (std::max)(8.0f, displaySize.x - panelW - 8.0f));
    const float panelY = std::clamp(config.Visuals.C4PanelPosY * displaySize.y, 8.0f, (std::max)(8.0f, displaySize.y - panelH - 8.0f));
    drawList->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(18, 18, 18, 190),
        5.0f
    );
    drawList->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        ToImColor(config.Visuals.C4Color),
        5.0f,
        0,
        1.2f
    );

    drawList->AddText(ImVec2(panelX + 10.0f, panelY + 8.0f), ToImColor(config.Visuals.C4Color), line1.c_str());
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 28.0f),
        c4.BeingDefused ? ToImColor(config.Visuals.DefuserColor) : IM_COL32(225, 225, 225, 255),
        line2.c_str()
    );
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 48.0f),
        c4.BeingDefused ? (c4.CanDefuse ? IM_COL32(120, 235, 120, 255) : IM_COL32(255, 110, 110, 255)) : IM_COL32(225, 225, 225, 255),
        line3.c_str()
    );
}

bool ESP::BuildBoneData(const uint64_t boneArray, PlayerEspSnapshot& inOutSnapshot) const
{
    if (!boneArray || !IsLikelyUserAddress(boneArray))
        return false;

    std::array<BoneDataRaw, kTrackedBones.size()> rawBones{};
    const auto scatter = mem.CreateScatterHandle();
    if (!scatter)
        return false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const uint64_t boneAddress = boneArray + static_cast<uint64_t>(kTrackedBones[i]) * Offsets::Layout::BoneStride;
        mem.AddScatterReadRequest(scatter, boneAddress, &rawBones[i], sizeof(BoneDataRaw));
    }

    mem.ExecuteReadScatter(scatter);
    mem.CloseScatterHandle(scatter);

    inOutSnapshot.Bones.clear();
    inOutSnapshot.Bones.reserve(kTrackedBones.size());

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    bool hasHead = false;
    bool anyOnScreen = false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const int boneIndex = kTrackedBones[i];
        const BoneDataRaw& rawBone = rawBones[i];

        BonePoint point{};
        point.Index = boneIndex;
        point.World = rawBone.Position;
        point.OnScreen = sdk.WorldToScreen(point.World, point.Screen);

        if (boneIndex == kHeadBone)
        {
            inOutSnapshot.HeadPosition = point.World;
            hasHead = true;
        }

        if (point.OnScreen)
        {
            anyOnScreen = true;
            minX = (std::min)(minX, point.Screen.x);
            minY = (std::min)(minY, point.Screen.y);
            maxX = (std::max)(maxX, point.Screen.x);
            maxY = (std::max)(maxY, point.Screen.y);
        }

        inOutSnapshot.Bones.push_back(point);
    }

    if (!hasHead)
    {
        inOutSnapshot.HeadPosition = inOutSnapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };
    }

    if (!anyOnScreen)
        return false;

    const float width = maxX - minX;
    const float height = maxY - minY;
    if (width < 3.0f || height < 3.0f)
        return false;

    const float paddingX = (std::max)(width * 0.08f, 3.0f);
    const float paddingY = (std::max)(height * 0.06f, 3.0f);

    inOutSnapshot.BoxMin = ImVec2(minX - paddingX, minY - paddingY);
    inOutSnapshot.BoxMax = ImVec2(maxX + paddingX, maxY + paddingY);

    return true;
}

C4Snapshot ESP::ReadC4Snapshot() const
{
    C4Snapshot snapshot{};
    if (!Globals::ClientBase || !Offsets::Client::dwPlantedC4)
        return snapshot;

    const float gameTime = ReadGlobalCurrentTime();
    const std::uint64_t nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    static std::uint64_t bombPlantStartMs = 0;
    static std::uint64_t bombDefuseStartMs = 0;
    static bool wasDefusing = false;
    static bool canDefuseLatched = false;

    auto readBool = [](const std::uint64_t address, bool& outValue)
    {
        outValue = mem.Read<std::uint8_t>(address) != 0;
    };

    auto readBombWorldPos = [](const std::uint64_t bombEntity, Vector3& outPos) -> bool
    {
        outPos = {};
        if (!IsLikelyUserAddress(bombEntity))
            return false;

        if (Offsets::Schema::m_pGameSceneNode && Offsets::Schema::m_vecAbsOrigin)
        {
            const std::uint64_t sceneNode = mem.Read<std::uint64_t>(bombEntity + Offsets::Schema::m_pGameSceneNode);
            if (IsLikelyUserAddress(sceneNode))
            {
                const Vector3 absPos = mem.Read<Vector3>(sceneNode + Offsets::Schema::m_vecAbsOrigin);
                if (IsNonZeroPosition(absPos))
                {
                    outPos = absPos;
                    return true;
                }
            }
        }

        if (Offsets::Schema::m_vecC4ExplodeSpectatePos)
        {
            const Vector3 spectatePos = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vecC4ExplodeSpectatePos);
            if (IsNonZeroPosition(spectatePos))
            {
                outPos = spectatePos;
                return true;
            }
        }

        if (Offsets::Schema::m_vOldOrigin)
        {
            const Vector3 oldOrigin = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vOldOrigin);
            if (IsNonZeroPosition(oldOrigin))
            {
                outPos = oldOrigin;
                return true;
            }
        }

        return false;
    };

    auto resolveBombEntityFromHolder = [&](const std::uint64_t holderAddress) -> std::uint64_t
    {
        if (!IsLikelyUserAddress(holderAddress))
            return 0;

        // Primary path used by reference project: holder -> entity pointer.
        const std::uint64_t indirectEntity = mem.Read<std::uint64_t>(holderAddress);
        if (IsLikelyUserAddress(indirectEntity))
            return indirectEntity;

        // Fallback for builds where global points directly to entity.
        Vector3 validatePos{};
        if (readBombWorldPos(holderAddress, validatePos))
            return holderAddress;

        return 0;
    };

    const std::uint64_t clientBase = Globals::ClientBase;
    const std::uint64_t plantedHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwPlantedC4);

    bool plantedFlag = false;
    if (Offsets::Client::dwPlantedC4 >= 0x8)
    {
        const std::uint8_t plantedByte = mem.Read<std::uint8_t>(clientBase + Offsets::Client::dwPlantedC4 - 0x8);
        plantedFlag = plantedByte != 0;
    }

    const std::uint64_t plantedEntity = resolveBombEntityFromHolder(plantedHolder);
    if (!plantedFlag && IsLikelyUserAddress(plantedEntity) && Offsets::Schema::m_bBombTicking)
    {
        bool tickingCandidate = false;
        readBool(plantedEntity + Offsets::Schema::m_bBombTicking, tickingCandidate);

        bool defusedCandidate = false;
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, defusedCandidate);

        plantedFlag = tickingCandidate && !defusedCandidate;
    }

    if (plantedFlag && IsLikelyUserAddress(plantedEntity))
    {
        snapshot.Valid = true;

        if (Offsets::Schema::m_nBombSite)
        {
            int normalizedSite = NormalizeBombSiteRaw(static_cast<int>(mem.Read<std::uint8_t>(plantedEntity + Offsets::Schema::m_nBombSite)));
            if (normalizedSite < 0)
                normalizedSite = NormalizeBombSiteRaw(mem.Read<int>(plantedEntity + Offsets::Schema::m_nBombSite));
            snapshot.BombSite = normalizedSite;
        }

        if (Offsets::Schema::m_bBombTicking)
            readBool(plantedEntity + Offsets::Schema::m_bBombTicking, snapshot.BombTicking);
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, snapshot.BombDefused);
        if (Offsets::Schema::m_bBeingDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBeingDefused, snapshot.BeingDefused);
        if (Offsets::Schema::m_flTimerLength)
            snapshot.TimerLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flTimerLength);
        if (Offsets::Schema::m_flDefuseLength)
            snapshot.DefuseLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseLength);
        if (Offsets::Schema::m_flC4Blow)
            snapshot.BlowTime = mem.Read<float>(plantedEntity + Offsets::Schema::m_flC4Blow);
        if (Offsets::Schema::m_flDefuseCountDown)
            snapshot.DefuseCountDown = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseCountDown);

        snapshot.Planted = snapshot.BombTicking && !snapshot.BombDefused;

        if (snapshot.BombTicking && snapshot.BlowTime > 0.001f && gameTime > 0.001f)
        {
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.BlowTime - gameTime);
            bombPlantStartMs = 0;
        }
        else if (snapshot.BombTicking && snapshot.TimerLength > 0.001f)
        {
            if (bombPlantStartMs == 0)
                bombPlantStartMs = nowMs;
            const float elapsed = static_cast<float>(nowMs - bombPlantStartMs) / 1000.0f;
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.TimerLength - elapsed);
        }
        else
        {
            bombPlantStartMs = 0;
            snapshot.TimeRemaining = 0.0f;
        }

        if (snapshot.BeingDefused)
        {
            if (snapshot.DefuseCountDown > 0.001f && gameTime > 0.001f)
            {
                snapshot.DefuseCountDown = (std::max)(0.0f, snapshot.DefuseCountDown - gameTime);
                bombDefuseStartMs = 0;
            }
            else
            {
                if (bombDefuseStartMs == 0)
                    bombDefuseStartMs = nowMs;
                const float elapsed = static_cast<float>(nowMs - bombDefuseStartMs) / 1000.0f;
                const float total = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
                snapshot.DefuseCountDown = (std::max)(0.0f, total - elapsed);
            }

            const float totalDefuse = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
            snapshot.DefuseProgress = std::clamp(1.0f - (snapshot.DefuseCountDown / totalDefuse), 0.0f, 1.0f);

            if (!wasDefusing)
                canDefuseLatched = snapshot.DefuseCountDown <= (snapshot.TimeRemaining + 0.05f);

            snapshot.CanDefuse = canDefuseLatched;
            wasDefusing = true;
        }
        else
        {
            bombDefuseStartMs = 0;
            snapshot.DefuseCountDown = 0.0f;
            snapshot.DefuseProgress = 0.0f;
            snapshot.CanDefuse = false;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        if (snapshot.BombDefused)
        {
            bombPlantStartMs = 0;
            bombDefuseStartMs = 0;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        readBombWorldPos(plantedEntity, snapshot.Position);
    }
    else
    {
        bombPlantStartMs = 0;
        bombDefuseStartMs = 0;
        wasDefusing = false;
        canDefuseLatched = false;

        if (Offsets::Client::dwWeaponC4)
        {
            const std::uint64_t weaponHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwWeaponC4);
            const std::uint64_t weaponEntity = resolveBombEntityFromHolder(weaponHolder);
            if (IsLikelyUserAddress(weaponEntity))
            {
                snapshot.Valid = true;
                snapshot.Planted = false;
                readBombWorldPos(weaponEntity, snapshot.Position);
            }
        }
    }

    if (IsNonZeroPosition(snapshot.Position))
        snapshot.OnScreen = sdk.WorldToScreen(snapshot.Position, snapshot.Screen);

    return snapshot;
}

bool ESP::IsAlive(const int health, const int lifeState) const
{
    if (health <= 0 || health > 200)
        return false;

    return lifeState == kAliveLifeStateA || lifeState == kAliveLifeStateB;
}

ImU32 ESP::GetBoxColor(const bool isVisible) const
{
    if (!config.Visuals.VisibleCheck)
        return ToImColor(config.Visuals.BoxColor);

    if (isVisible)
        return ToImColor(config.Visuals.BoxColorVisible);

    return IM_COL32(255, 255, 255, 255);
}

ImU32 ESP::ToImColor(const ImVec4& color) const
{
    return ImGui::ColorConvertFloat4ToU32(color);
}

std::string ESP::ReadPlayerName(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_iszPlayerName)
        return {};

    char nameBuffer[128]{};
    if (!mem.Read(controller + Offsets::Schema::m_iszPlayerName, nameBuffer, sizeof(nameBuffer)))
        return {};

    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    std::string name(nameBuffer);
    name.erase(std::remove_if(name.begin(), name.end(), [](char c)
    {
        const unsigned char value = static_cast<unsigned char>(c);
        return value < 0x20 && value != '\t';
    }), name.end());

    return name;
}

int ESP::ReadMoney(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_pInGameMoneyServices || !Offsets::Schema::m_iAccount)
        return 0;

    const uint64_t moneyServices = mem.Read<uint64_t>(controller + Offsets::Schema::m_pInGameMoneyServices);
    if (!moneyServices || !IsLikelyUserAddress(moneyServices))
        return 0;

    return mem.Read<int>(moneyServices + Offsets::Schema::m_iAccount);
}

std::string ESP::ReadWeaponName(const uint64_t pawn) const
{
    if (!pawn || !Offsets::Schema::m_pWeaponServices || !Offsets::Schema::m_hActiveWeapon)
        return {};
    if (!Offsets::Schema::m_AttributeManager || !Offsets::Schema::m_Item || !Offsets::Schema::m_iItemDefinitionIndex)
        return {};

    const uint64_t weaponServices = mem.Read<uint64_t>(pawn + Offsets::Schema::m_pWeaponServices);
    if (!weaponServices || !IsLikelyUserAddress(weaponServices))
        return {};

    const uint32_t activeWeaponHandle = mem.Read<uint32_t>(weaponServices + Offsets::Schema::m_hActiveWeapon);
    if (!activeWeaponHandle)
        return {};

    const uint64_t weapon = sdk.ResolveEntityFromHandle(activeWeaponHandle);
    if (!weapon || !IsLikelyUserAddress(weapon))
        return {};

    const std::uint64_t itemDefinitionIndexAddress =
        weapon +
        static_cast<uint64_t>(Offsets::Schema::m_AttributeManager) +
        static_cast<uint64_t>(Offsets::Schema::m_Item) +
        static_cast<uint64_t>(Offsets::Schema::m_iItemDefinitionIndex);

    const int weaponId = static_cast<int>(mem.Read<std::uint16_t>(itemDefinitionIndexAddress));

    if (weaponId <= 0)
        return {};

    const std::string resolvedName = WeaponIdToName(weaponId);
    if (!resolvedName.empty())
        return resolvedName;

    return std::string(Localization::Pick("Weapon ", "武器 ")) + std::to_string(weaponId);
}

void ESP::UpdateVisCheckState()
{
    ConsumeMapLoadResult();

    // Keep map/.opt status alive by default; VisibleCheck only controls usage, not loading state.
    m_VisCheckEnabled = true;

    const auto now = std::chrono::steady_clock::now();
    if (m_LastMapPoll.time_since_epoch().count() == 0 ||
        now - m_LastMapPoll >= std::chrono::milliseconds(1000))
    {
        m_LastPolledMapName = sdk.GetCurrentMapName();
        //PerfDebug::RecordMapPoll(!m_LastPolledMapName.empty());
        m_LastMapPoll = now;
    }

    const std::string& mapName = m_LastPolledMapName;
    if (mapName.empty())
    {
        m_MapStatus = Localization::Pick("Map Status: (No Map)", "地图状态: (无地图)");
        return;
    }

    if (m_VisCheck && mapName == m_CurrentMapName)
    {
        m_MapStatus = BuildMapStatus(mapName, "Loaded");
        return;
    }

    if (!m_VisCheck && mapName == m_CurrentMapName && m_CurrentOptPath.empty())
    {
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    for (const PendingMapLoad& pending : m_PendingMapLoads)
    {
        if (pending.RequestId == m_ActiveMapRequestId && pending.MapName == mapName)
            return;
    }

    const std::string optPath = ResolveOptPath(mapName);
    if (optPath.empty())
    {
        m_VisCheck.reset();
        m_CurrentMapName = mapName;
        m_CurrentOptPath.clear();
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    RequestMapLoad(mapName, optPath);
}

void ESP::RequestMapLoad(const std::string& mapName, const std::string& optPath)
{
    PendingMapLoad pending{};
    pending.RequestId = ++m_NextMapRequestId;
    pending.MapName = mapName;
    pending.OptPath = optPath;
    pending.Future = std::async(std::launch::async, [optPath]()
    {
        auto visCheck = std::make_unique<VisCheck>(optPath);
        if (visCheck && visCheck->IsReady())
            return visCheck;

        return std::unique_ptr<VisCheck>{};
    });

    m_ActiveMapRequestId = pending.RequestId;
    m_MapStatus = BuildMapStatus(mapName, "Loading");
    m_PendingMapLoads.push_back(std::move(pending));
}

void ESP::ConsumeMapLoadResult()
{
    for (size_t index = 0; index < m_PendingMapLoads.size();)
    {
        PendingMapLoad& pending = m_PendingMapLoads[index];
        if (!pending.Future.valid())
        {
            m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
            continue;
        }

        if (pending.Future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            ++index;
            continue;
        }

        std::unique_ptr<VisCheck> loadedVisCheck = pending.Future.get();
        const bool requestIsCurrent = pending.RequestId == m_ActiveMapRequestId;

        if (requestIsCurrent)
        {
            if (loadedVisCheck)
            {
                m_VisCheck = std::move(loadedVisCheck);
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Loaded");
            }
            else
            {
                m_VisCheck.reset();
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Load Failed");
            }
        }

        m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
    }
}

std::string ESP::ResolveOptPath(const std::string& mapName) const
{
    if (mapName.empty())
        return {};

    const std::string fileName = mapName + ".opt";
    std::vector<std::filesystem::path> candidates{};
    candidates.reserve(64);

    auto appendCandidates = [&](std::filesystem::path base)
    {
        std::error_code ec{};
        for (int depth = 0; depth < 6 && !base.empty(); ++depth)
        {
            candidates.push_back(base / "maps" / fileName);
            candidates.push_back(base / "Maps" / fileName);
            candidates.push_back(base / "project-d" / "maps" / fileName);
            candidates.push_back(base / "project-d" / "Maps" / fileName);
            candidates.push_back(base / fileName);

            const std::filesystem::path parent = base.parent_path();
            if (parent == base || parent.empty())
                break;
            base = parent;
        }
    };

    std::error_code cwdError{};
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError)
        appendCandidates(cwd);

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        appendCandidates(std::filesystem::path(modulePath).parent_path());

    for (const std::filesystem::path& candidate : candidates)
    {
        if (candidate.empty())
            continue;

        std::error_code existsError{};
        if (std::filesystem::exists(candidate, existsError) && !existsError)
            return candidate.string();
    }

    return {};
}

std::string ESP::BuildMapStatus(const std::string& mapName, const char* suffix) const
{
    std::string status = Localization::Pick("Map Status: ", "地图状态: ");
    status += mapName.empty() ? Localization::Pick("(Unknown)", "(未知)") : (mapName + ".opt");
    status += " (";
    status += Localization::Localize(suffix);
    status += ")";
    return status;
}

bool ESP::CheckVisibility(const Vector3& src, const Vector3& dst) const
{
    if (!m_VisCheck)
        return false;

    constexpr float maxDistance = 5000.0f;
    constexpr float maxDistanceSqr = maxDistance * maxDistance;
    if (DistanceSquared3D(src, dst) > maxDistanceSqr)
        return false;

    const auto start = std::chrono::steady_clock::now();
    const bool isVisible = m_VisCheck->IsPointVisible(src, dst);
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    if (durationUs >= 0)
        //PerfDebug::RecordVisCheck(static_cast<std::uint64_t>(durationUs), isVisible);

    return isVisible;
}

bool ESP::IsPawnVisibleCached(const uint64_t pawn) const
{
    if (!pawn)
        return false;

    std::lock_guard lock(m_RenderFrameMutex);
    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        if (player.Pawn == pawn)
            return player.IsVisible;
    }

    return false;
}

std::unordered_set<uint64_t> ESP::GetVisiblePawnSetSnapshot() const
{
    std::unordered_set<uint64_t> visiblePawns{};

    std::lock_guard lock(m_RenderFrameMutex);
    visiblePawns.reserve(m_RenderFrame.Players.size());
    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        if (player.Pawn != 0 && player.IsVisible)
            visiblePawns.insert(player.Pawn);
    }

    return visiblePawns;
}

std::vector<TriggerBoneSnapshot> ESP::GetTriggerBoneSnapshots() const
{
    std::vector<TriggerBoneSnapshot> snapshots{};

    auto slotFromBoneId = [](const int boneId) -> int
    {
        for (std::size_t i = 0; i < kTrackedBones.size(); ++i)
        {
            if (kTrackedBones[i] == boneId)
                return static_cast<int>(i);
        }

        return -1;
    };

    std::lock_guard lock(m_RenderFrameMutex);
    snapshots.reserve(m_RenderFrame.Players.size());

    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        TriggerBoneSnapshot snapshot{};
        snapshot.Pawn = player.Pawn;
        snapshot.Team = player.Team;
        snapshot.Health = player.Health;
        snapshot.LifeState = player.LifeState;
        snapshot.IsVisible = player.IsVisible;
        snapshot.BoxMin = player.BoxMin;
        snapshot.BoxMax = player.BoxMax;

        for (const BonePoint& bone : player.Bones)
        {
            const int slot = slotFromBoneId(bone.Index);
            if (slot < 0 || static_cast<std::size_t>(slot) >= TriggerBoneSnapshot::BoneCount)
                continue;

            snapshot.Bones[static_cast<std::size_t>(slot)] = bone;
            snapshot.BoneValid[static_cast<std::size_t>(slot)] = bone.OnScreen;
        }

        snapshots.push_back(std::move(snapshot));
    }

    return snapshots;
}

void ESP::EnsureSamplerStarted()
{
    bool expected = false;
    if (!m_SamplerStarted.compare_exchange_strong(expected, true))
        return;

    std::thread([this]()
    {
        SamplerLoop();
    }).detach();
}

void ESP::UpdateRoundEpoch(const uint64_t localPawn, const bool localAlive)
{
    const auto now = std::chrono::steady_clock::now();
    if (m_LastRoundEpochTick.time_since_epoch().count() == 0)
        m_LastRoundEpochTick = now;

    bool shouldAdvanceEpoch = false;
    if (localPawn && m_LastRoundLocalPawn && localPawn != m_LastRoundLocalPawn)
    {
        shouldAdvanceEpoch = true;
    }
    else if (localAlive && !m_LastRoundLocalAlive)
    {
        if (now - m_LastRoundEpochTick > std::chrono::seconds(8))
            shouldAdvanceEpoch = true;
    }

    if (shouldAdvanceEpoch)
    {
        ++m_RoundEpoch;
        m_LastRoundEpochTick = now;
        m_PawnRuntimeCache.clear();
        m_LastFreezePeriod = false;
        m_LastFreezeEndTick = {};
    }

    if (localPawn)
        m_LastRoundLocalPawn = localPawn;
    m_LastRoundLocalAlive = localAlive;
}

void ESP::SamplerLoop()
{
    constexpr auto kSampleIntervalIdle = std::chrono::milliseconds(4); // 250Hz
    constexpr auto kSampleIntervalHot = std::chrono::milliseconds(2);  // 500Hz
    constexpr auto kOverrunYield = std::chrono::milliseconds(1);

    while (Globals::Running)
    {
        const auto cycleStart = std::chrono::steady_clock::now();

        RenderFrame sampledFrame{};
        sampledFrame.MapStatus = m_MapStatus;
        SampleFrame(sampledFrame);

        {
            std::lock_guard lock(m_RenderFrameMutex);
            std::swap(m_RenderFrame, sampledFrame);
        }

        const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - cycleStart
        ).count();
        if (sampleUs >= 0)
            PerfDebug::RecordEspSampleFrame(static_cast<std::uint64_t>(sampleUs));

        const bool boneTriggerHot =
            config.Aim.Trigger &&
            std::clamp(config.Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1) == Structs::TriggerDetect_BoneHitbox &&
            aim.IsTriggerHotkeyActiveVisual();
        const auto targetInterval = boneTriggerHot ? kSampleIntervalHot : kSampleIntervalIdle;
        const auto elapsed = std::chrono::steady_clock::now() - cycleStart;
        if (elapsed < targetInterval)
            std::this_thread::sleep_for(targetInterval - elapsed);
        else
            std::this_thread::sleep_for(kOverrunYield);
    }
}

bool ESP::SampleFrame(RenderFrame& outFrame)
{
    UpdateVisCheckState();
    outFrame.MapStatus = m_MapStatus;

    const bool needVisibilityChecks = config.Aim.AimVisible || config.Visuals.VisibleCheck;
    const bool needTriggerBoneSampling =
        config.Aim.Trigger &&
        std::clamp(config.Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1) == Structs::TriggerDetect_BoneHitbox;
    const bool needEntitySampling = config.Visuals.Enabled || needVisibilityChecks || needTriggerBoneSampling;
    if (!needEntitySampling)
        return true;

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn || !core.EntityList)
        return true;

    struct LocalFields
    {
        int Team = 0;
        int Health = 0;
        int LifeState = 0;
        Vector3 Origin{};
        Vector3 ViewOffset{};
    } local{};

    const auto localScatter = mem.CreateScatterHandle();
    if (!localScatter)
        return false;

    if (Offsets::Schema::m_iTeamNum)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iTeamNum, &local.Team, sizeof(local.Team));
    if (Offsets::Schema::m_iHealth)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iHealth, &local.Health, sizeof(local.Health));
    if (Offsets::Schema::m_lifeState)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_lifeState, &local.LifeState, sizeof(local.LifeState));
    if (Offsets::Schema::m_vOldOrigin)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vOldOrigin, &local.Origin, sizeof(local.Origin));
    if (Offsets::Schema::m_vecViewOffset)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vecViewOffset, &local.ViewOffset, sizeof(local.ViewOffset));

    mem.ExecuteReadScatter(localScatter);
    mem.CloseScatterHandle(localScatter);

    Vector3 localViewOffset = { 0.0f, 0.0f, 64.0f };
    if (std::abs(local.ViewOffset.x) > 0.001f || std::abs(local.ViewOffset.y) > 0.001f || std::abs(local.ViewOffset.z) > 0.001f)
        localViewOffset = local.ViewOffset;

    const Vector3 localEyePosition = local.Origin + localViewOffset;
    const int localTeam = local.Team;

    UpdateRoundEpoch(core.LocalPawn, IsAlive(local.Health, local.LifeState));
    const auto now = std::chrono::steady_clock::now();

    bool freezePeriod = false;
    bool freezeValid = false;
    if (Offsets::Client::dwGameRules && Offsets::Schema::m_bFreezePeriod)
    {
        const uint64_t gameRules = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGameRules);
        if (IsLikelyUserAddress(gameRules))
        {
            freezePeriod = mem.Read<bool>(gameRules + Offsets::Schema::m_bFreezePeriod);
            freezeValid = true;
        }
    }

    if (freezeValid)
    {
        if (freezePeriod)
        {
            if (!m_LastFreezePeriod &&
                (m_LastRoundEpochTick.time_since_epoch().count() == 0 ||
                    now - m_LastRoundEpochTick > std::chrono::seconds(8)))
            {
                ++m_RoundEpoch;
                m_LastRoundEpochTick = now;
                m_PawnRuntimeCache.clear();
            }

            m_LastFreezePeriod = true;
        }
        else if (m_LastFreezePeriod)
        {
            m_LastFreezePeriod = false;
            m_LastFreezeEndTick = now;
        }
    }

    const uint64_t controllerChunk = mem.Read<uint64_t>(core.EntityList + Offsets::EntityList::ListStart);
    if (!IsLikelyUserAddress(controllerChunk))
        return true;

    std::array<uint64_t, kMaxControllers + 1> controllerPointers{};
    if (const auto controllerScatter = mem.CreateScatterHandle())
    {
        for (int index = 1; index <= kMaxControllers; ++index)
        {
            const uint64_t entryAddress = controllerChunk + static_cast<uint64_t>(index) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(controllerScatter, entryAddress, &controllerPointers[index], sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(controllerScatter);
        mem.CloseScatterHandle(controllerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData> entities{};
    entities.reserve(kMaxControllers);

    for (int index = 1; index <= kMaxControllers; ++index)
    {
        const uint64_t controller = controllerPointers[index];
        if (!IsLikelyUserAddress(controller))
            continue;

        SampledEntityData data{};
        data.HandleIndex = index;
        data.Controller = controller;
        entities.push_back(data);
    }

    outFrame.ResolvedControllers = static_cast<std::uint32_t>(entities.size());
    if (entities.empty())
        return true;

    const uint32_t pawnHandlePrimaryOffset = Offsets::Schema::m_hPlayerPawn ? Offsets::Schema::m_hPlayerPawn : Offsets::Schema::m_hPawn;
    const uint32_t pawnHandleFallbackOffset = (Offsets::Schema::m_hPlayerPawn && Offsets::Schema::m_hPawn)
        ? Offsets::Schema::m_hPawn
        : 0;

    if (!pawnHandlePrimaryOffset)
        return true;

    std::vector<uint32_t> fallbackPawnHandles(entities.size(), 0);
    if (const auto pawnHandleScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            mem.AddScatterReadRequest(
                pawnHandleScatter,
                entities[i].Controller + pawnHandlePrimaryOffset,
                &entities[i].PawnHandle,
                sizeof(uint32_t)
            );

            if (pawnHandleFallbackOffset)
            {
                mem.AddScatterReadRequest(
                    pawnHandleScatter,
                    entities[i].Controller + pawnHandleFallbackOffset,
                    &fallbackPawnHandles[i],
                    sizeof(uint32_t)
                );
            }
        }

        mem.ExecuteReadScatter(pawnHandleScatter);
        mem.CloseScatterHandle(pawnHandleScatter);
    }
    else
    {
        return false;
    }

    std::vector<uint32_t> decodedHi(entities.size(), 0);
    std::vector<uint32_t> decodedLo(entities.size(), 0);
    std::unordered_set<uint32_t> uniqueHi{};

    for (size_t i = 0; i < entities.size(); ++i)
    {
        if (!entities[i].PawnHandle && pawnHandleFallbackOffset)
            entities[i].PawnHandle = fallbackPawnHandles[i];

        const uint32_t handleIndex = entities[i].PawnHandle & Offsets::EntityList::HandleMask;
        if (!handleIndex)
            continue;

        const uint32_t hi = handleIndex >> Offsets::EntityList::HandleHighShift;
        const uint32_t lo = handleIndex & Offsets::EntityList::HandleLowMask;
        decodedHi[i] = hi;
        decodedLo[i] = lo;
        uniqueHi.insert(hi);
    }

    std::unordered_map<uint32_t, uint64_t> chunkPointers{};
    chunkPointers.reserve(uniqueHi.size());
    for (const uint32_t hi : uniqueHi)
        chunkPointers.emplace(hi, 0ULL);

    if (!chunkPointers.empty())
    {
        if (const auto chunkScatter = mem.CreateScatterHandle())
        {
            for (auto& [hi, chunkPointer] : chunkPointers)
            {
                const uint64_t chunkAddress = core.EntityList + Offsets::EntityList::ListStart + static_cast<uint64_t>(hi) * Offsets::EntityList::ChunkStride;
                mem.AddScatterReadRequest(chunkScatter, chunkAddress, &chunkPointer, sizeof(uint64_t));
            }

            mem.ExecuteReadScatter(chunkScatter);
            mem.CloseScatterHandle(chunkScatter);
        }
        else
        {
            return false;
        }
    }

    if (const auto pawnPointerScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (!(entities[i].PawnHandle & Offsets::EntityList::HandleMask))
                continue;

            const auto chunkIt = chunkPointers.find(decodedHi[i]);
            if (chunkIt == chunkPointers.end())
                continue;

            const uint64_t chunkPointer = chunkIt->second;
            if (!IsLikelyUserAddress(chunkPointer))
                continue;

            const uint64_t pawnAddress = chunkPointer + static_cast<uint64_t>(decodedLo[i]) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(pawnPointerScatter, pawnAddress, &entities[i].Pawn, sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(pawnPointerScatter);
        mem.CloseScatterHandle(pawnPointerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData*> activeEntities{};
    activeEntities.reserve(entities.size());
    for (SampledEntityData& entity : entities)
    {
        if (!IsLikelyUserAddress(entity.Pawn))
            continue;
        if (entity.Pawn == core.LocalPawn)
            continue;
        activeEntities.push_back(&entity);
    }

    if (activeEntities.empty())
        return true;

    if (const auto pawnFieldScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (Offsets::Schema::m_iHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iHealth, &entity->Health, sizeof(entity->Health));
            if (Offsets::Schema::m_iTeamNum)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iTeamNum, &entity->Team, sizeof(entity->Team));
            if (Offsets::Schema::m_lifeState)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_lifeState, &entity->LifeState, sizeof(entity->LifeState));
            if (Offsets::Schema::m_iMaxHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iMaxHealth, &entity->MaxHealth, sizeof(entity->MaxHealth));
            if (Offsets::Schema::m_pGameSceneNode)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_pGameSceneNode, &entity->SceneNode, sizeof(entity->SceneNode));
            if (Offsets::Schema::m_vOldOrigin)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vOldOrigin, &entity->OldOrigin, sizeof(entity->OldOrigin));
            if (Offsets::Schema::m_vecViewOffset)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vecViewOffset, &entity->ViewOffset, sizeof(entity->ViewOffset));

            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            entity->RefreshStatus = runtimeCache.LastStatusRead.time_since_epoch().count() == 0 ||
                now - runtimeCache.LastStatusRead >= std::chrono::milliseconds(150);

            if (!entity->RefreshStatus)
            {
                entity->Armor = runtimeCache.Armor;
                entity->IsScoped = runtimeCache.IsScoped;
                entity->HasDefuser = runtimeCache.HasDefuser;
                entity->FlashDuration = runtimeCache.FlashDuration;
            }
            else
            {
                if (Offsets::Schema::m_ArmorValue)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_ArmorValue, &entity->Armor, sizeof(entity->Armor));
                if (Offsets::Schema::m_bIsScoped)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bIsScoped, &entity->IsScoped, sizeof(entity->IsScoped));
                if (Offsets::Schema::m_bHasDefuser)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bHasDefuser, &entity->HasDefuser, sizeof(entity->HasDefuser));
                if (Offsets::Schema::m_flFlashDuration)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_flFlashDuration, &entity->FlashDuration, sizeof(entity->FlashDuration));
            }
        }

        mem.ExecuteReadScatter(pawnFieldScatter);
        mem.CloseScatterHandle(pawnFieldScatter);
    }
    else
    {
        return false;
    }

    for (SampledEntityData* entity : activeEntities)
    {
        if (std::abs(entity->ViewOffset.x) <= 0.001f && std::abs(entity->ViewOffset.y) <= 0.001f && std::abs(entity->ViewOffset.z) <= 0.001f)
            entity->ViewOffset = { 0.0f, 0.0f, 64.0f };

        if (entity->RefreshStatus)
        {
            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            runtimeCache.Armor = (std::max)(0, entity->Armor);
            runtimeCache.IsScoped = entity->IsScoped;
            runtimeCache.HasDefuser = entity->HasDefuser;
            runtimeCache.FlashDuration = (std::max)(0.0f, entity->FlashDuration);
            runtimeCache.LastStatusRead = now;
        }

        if (!IsLikelyUserAddress(entity->SceneNode))
            entity->SceneNode = 0;
    }

    if (const auto sceneScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (!entity->SceneNode)
                continue;

            if (Offsets::Schema::m_vecAbsOrigin)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_vecAbsOrigin,
                    &entity->AbsOrigin,
                    sizeof(entity->AbsOrigin)
                );
                entity->HasAbsOrigin = true;
            }

            if (Offsets::Schema::m_modelState)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_modelState + Offsets::Layout::BoneArray,
                    &entity->BoneArray,
                    sizeof(entity->BoneArray)
                );
            }
        }

        mem.ExecuteReadScatter(sceneScatter);
        mem.CloseScatterHandle(sceneScatter);
    }
    else
    {
        return false;
    }

    outFrame.Players.clear();
    outFrame.Players.reserve(activeEntities.size());

    std::unordered_set<uint64_t> activeControllers{};
    std::unordered_set<uint64_t> activePawns{};
    activeControllers.reserve(activeEntities.size());
    activePawns.reserve(activeEntities.size());

    constexpr auto kMoneyPollInterval = std::chrono::seconds(1);
    constexpr auto kPostFreezeDuration = std::chrono::seconds(20);
    constexpr auto kEstimatedFreezeDuration = std::chrono::seconds(15); // fallback when freeze flag is unavailable.
    bool moneyWindowActive = false;
    if (m_LastRoundEpochTick.time_since_epoch().count() != 0)
    {
        if (freezeValid)
        {
            moneyWindowActive = freezePeriod ||
                (m_LastFreezeEndTick.time_since_epoch().count() != 0 &&
                    now - m_LastFreezeEndTick <= kPostFreezeDuration);
        }
        else
        {
            const auto roundElapsed = now - m_LastRoundEpochTick;
            moneyWindowActive = roundElapsed <= (kEstimatedFreezeDuration + kPostFreezeDuration);
        }
    }

    for (SampledEntityData* entity : activeEntities)
    {
        if (!IsAlive(entity->Health, entity->LifeState))
            continue;

        if (config.Visuals.TeamCheck && !config.Aim.AimFriendly && localTeam > 0 && entity->Team == localTeam)
            continue;

        activeControllers.insert(entity->Controller);
        activePawns.insert(entity->Pawn);

        ControllerIdentityCache& identityCache = m_ControllerIdentityCache[entity->Controller];
        if (identityCache.RoundEpoch != m_RoundEpoch)
        {
            identityCache.Name = ReadPlayerName(entity->Controller);
            identityCache.RoundEpoch = m_RoundEpoch;
            if (moneyWindowActive)
            {
                identityCache.Money = ReadMoney(entity->Controller);
                identityCache.LastMoneyRead = now;
            }
        }
        else if (moneyWindowActive &&
            (identityCache.LastMoneyRead.time_since_epoch().count() == 0 ||
                now - identityCache.LastMoneyRead >= kMoneyPollInterval))
        {
            identityCache.Money = ReadMoney(entity->Controller);
            identityCache.LastMoneyRead = now;
        }

        PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
        if (runtimeCache.WeaponName.empty() ||
            runtimeCache.LastWeaponRead.time_since_epoch().count() == 0 ||
            now - runtimeCache.LastWeaponRead >= std::chrono::milliseconds(500))
        {
            runtimeCache.WeaponName = ReadWeaponName(entity->Pawn);
            runtimeCache.LastWeaponRead = now;
        }

        PlayerEspSnapshot snapshot{};
        snapshot.Controller = entity->Controller;
        snapshot.Pawn = entity->Pawn;
        snapshot.SceneNode = entity->SceneNode;
        snapshot.BoneArray = IsLikelyUserAddress(entity->BoneArray) ? entity->BoneArray : 0;

        snapshot.Health = entity->Health;
        snapshot.MaxHealth = entity->MaxHealth > 0 ? entity->MaxHealth : 100;
        snapshot.Team = entity->Team;
        snapshot.LifeState = entity->LifeState;

        snapshot.Armor = runtimeCache.Armor;
        snapshot.IsScoped = runtimeCache.IsScoped;
        snapshot.HasDefuser = runtimeCache.HasDefuser;
        snapshot.FlashDuration = runtimeCache.FlashDuration;

        snapshot.Origin = entity->HasAbsOrigin ? entity->AbsOrigin : entity->OldOrigin;
        snapshot.EyePosition = snapshot.Origin + entity->ViewOffset;

        snapshot.Name = identityCache.Name;
        snapshot.Money = identityCache.Money;
        snapshot.ShowMoney = moneyWindowActive;
        snapshot.WeaponName = runtimeCache.WeaponName;

        bool hasBoxData = false;
        const bool needBoneData = (config.Visuals.Bones || config.Aim.TriggerHitboxDebug || needTriggerBoneSampling) && snapshot.BoneArray;
        if (needBoneData)
            hasBoxData = BuildBoneData(snapshot.BoneArray, snapshot);

        if (!hasBoxData)
        {
            Vector2 screenHead{};
            Vector2 screenFeet{};
            snapshot.HeadPosition = snapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };

            if (!sdk.WorldToScreen(snapshot.HeadPosition, screenHead) || !sdk.WorldToScreen(snapshot.Origin, screenFeet))
                continue;

            const float boxHeight = std::abs(screenFeet.y - screenHead.y);
            if (boxHeight < 4.0f)
                continue;

            const float boxWidth = boxHeight * 0.45f;
            snapshot.BoxMin = ImVec2(screenFeet.x - boxWidth * 0.5f, screenHead.y);
            snapshot.BoxMax = ImVec2(screenFeet.x + boxWidth * 0.5f, screenFeet.y);
        }

        if (needVisibilityChecks && m_VisCheckEnabled)
            snapshot.IsVisible = CheckVisibility(localEyePosition, snapshot.HeadPosition);

        outFrame.Players.push_back(std::move(snapshot));
    }

    for (auto it = m_ControllerIdentityCache.begin(); it != m_ControllerIdentityCache.end();)
    {
        if (activeControllers.find(it->first) == activeControllers.end())
            it = m_ControllerIdentityCache.erase(it);
        else
            ++it;
    }

    for (auto it = m_PawnRuntimeCache.begin(); it != m_PawnRuntimeCache.end();)
    {
        if (activePawns.find(it->first) == activePawns.end())
            it = m_PawnRuntimeCache.erase(it);
        else
            ++it;
    }

    if (config.Visuals.C4)
    {
        constexpr auto kC4IntervalIdle = std::chrono::microseconds(16667);   // 60Hz
        constexpr auto kC4IntervalPlanted = std::chrono::microseconds(10000); // 100Hz

        const bool cacheValid = m_LastC4Sample.time_since_epoch().count() != 0;
        const auto targetInterval = m_C4Cache.Planted ? kC4IntervalPlanted : kC4IntervalIdle;

        if (!cacheValid || (now - m_LastC4Sample) >= targetInterval)
        {
            m_C4Cache = ReadC4Snapshot();
            m_LastC4Sample = now;
        }

        outFrame.C4 = m_C4Cache;
    }
    else
    {
        m_C4Cache = C4Snapshot{};
        m_LastC4Sample = {};
        outFrame.C4 = C4Snapshot{};
    }

    return true;
}

void ESP::Render(ImDrawList* drawList)
{
    const auto renderStart = std::chrono::steady_clock::now();
    EnsureSamplerStarted();

    std::uint32_t resolvedControllers = 0;
    std::uint32_t drawnPlayers = 0;
    const auto publishPerf = [&]()
    {
        const auto renderUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - renderStart
        ).count();

        if (renderUs >= 0)
        {
           /* PerfDebug::RecordEspFrame(
                static_cast<std::uint64_t>(renderUs),
                resolvedControllers,
                drawnPlayers
            );*/
        }
    };

    if (!drawList)
    {
        publishPerf();
        return;
    }

    if (!config.Visuals.Enabled)
    {
        publishPerf();
        return;
    }

    RenderFrame frame{};
    {
        std::lock_guard lock(m_RenderFrameMutex);
        frame = m_RenderFrame;
    }

    resolvedControllers = frame.ResolvedControllers;
    drawnPlayers = static_cast<std::uint32_t>(frame.Players.size());

    RenderWatermark(drawList);
    float leftHudY = config.Visuals.Watermark ? 34.0f : 12.0f;

    const ImVec2 statusPos(12.0f, leftHudY);
    const char* mapStatus = frame.MapStatus.empty() ? Localization::Pick("Map Status: (Waiting)", "地图状态: (等待中)") : frame.MapStatus.c_str();
    drawList->AddText(statusPos, IM_COL32(210, 210, 210, 255), mapStatus);
    leftHudY += ImGui::GetFontSize() + 2.0f;

    if (config.Aim.Aimbot || config.Aim.Trigger)
    {
        const bool aimbotHotkeyActive = aim.IsAimbotHotkeyActiveVisual();
        const bool aimbotHasTarget = aim.HasAimbotTargetVisual();
        const bool triggerHotkeyActive = aim.IsTriggerHotkeyActiveVisual();
        const bool triggerHasTarget = aim.HasTriggerTargetVisual();

        const char* aimbotState = Localization::Pick("OFF", "关闭");
        ImU32 aimbotColor = IM_COL32(180, 180, 180, 255);
        if (config.Aim.Aimbot)
        {
            aimbotState = Localization::Pick("READY", "就绪");
            aimbotColor = IM_COL32(220, 220, 220, 255);
            if (aimbotHotkeyActive)
            {
                aimbotState = Localization::Pick("HOTKEY", "热键");
                aimbotColor = IM_COL32(255, 220, 120, 255);
            }
            if (aimbotHasTarget)
            {
                aimbotState = Localization::Pick("LOCK", "锁定");
                aimbotColor = IM_COL32(120, 255, 155, 255);
            }
        }

        const char* triggerState = Localization::Pick("OFF", "关闭");
        ImU32 triggerColor = IM_COL32(180, 180, 180, 255);
        if (config.Aim.Trigger)
        {
            triggerState = Localization::Pick("READY", "就绪");
            triggerColor = IM_COL32(220, 220, 220, 255);
            if (triggerHotkeyActive)
            {
                triggerState = Localization::Pick("HOTKEY", "热键");
                triggerColor = IM_COL32(255, 220, 120, 255);
            }
            if (triggerHasTarget)
            {
                triggerState = Localization::Pick("HIT", "命中");
                triggerColor = IM_COL32(120, 255, 155, 255);
            }
        }

        char aimbotStatusLine[64]{};
        char triggerStatusLine[64]{};
        std::snprintf(aimbotStatusLine, sizeof(aimbotStatusLine), Localization::Pick("Aimbot: %s", "自瞄: %s"), aimbotState);
        std::snprintf(triggerStatusLine, sizeof(triggerStatusLine), Localization::Pick("Trigger: %s", "扳机: %s"), triggerState);
        drawList->AddText(ImVec2(12.0f, leftHudY), aimbotColor, aimbotStatusLine);
        leftHudY += ImGui::GetFontSize() + 2.0f;
        drawList->AddText(ImVec2(12.0f, leftHudY), triggerColor, triggerStatusLine);
        leftHudY += ImGui::GetFontSize() + 2.0f;
    }

    if (config.Aim.Aimbot && config.Aim.DrawFov)
    {
        float radius = aim.GetCurrentFovRadiusPx();
        if (radius <= 0.01f)
            radius = (std::max)(2.0f, (config.Aim.WeaponProfiles[Structs::AimWeapon_Rifle].Fov / 180.0f) * ScreenCenter.x);

        drawList->AddCircle(
            ImVec2(ScreenCenter.x, ScreenCenter.y),
            radius,
            ToImColor(config.Aim.AimbotFovColor),
            96,
            1.3f
        );

        char fovText[64]{};
        const bool aimbotHasTarget = aim.HasAimbotTargetVisual();
        const bool aimbotHotkeyActive = aim.IsAimbotHotkeyActiveVisual();
        const char* aimState = aimbotHasTarget ? Localization::Pick("LOCK", "锁定") : (aimbotHotkeyActive ? Localization::Pick("HOTKEY", "热键") : Localization::Pick("IDLE", "待机"));
        std::snprintf(fovText, sizeof(fovText), Localization::Pick("FOV %.1f px [%s]", "FOV %.1f 像素 [%s]"), radius, aimState);
        drawList->AddText(
            ImVec2(ScreenCenter.x + radius + 8.0f, ScreenCenter.y - ImGui::GetFontSize() * 0.5f),
            ToImColor(config.Aim.AimbotFovColor),
            fovText
        );
    }

    if (config.Aim.TriggerHitboxDebug)
    {
        const int detectMode = std::clamp(
            aim.GetCurrentTriggerDetectMode(),
            0,
            static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1
        );
        const float unifiedRadius = std::clamp(config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
        const float hitboxScale = std::clamp(config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
        const float hitboxAddPx = std::clamp(config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
        const float headBaseRadius = std::clamp(config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);
        const float bodyRadius = std::clamp(unifiedRadius * hitboxScale + hitboxAddPx, 0.5f, 80.0f);
        const float headRadius = std::clamp(headBaseRadius * hitboxScale + hitboxAddPx, 0.5f, 100.0f);
        char triggerDebugText[128]{};
        std::snprintf(
            triggerDebugText,
            sizeof(triggerDebugText),
            Localization::Pick("Trigger Debug: %s | Body %.1f px | Head %.1f px", "扳机调试: %s | 身体 %.1f 像素 | 头部 %.1f 像素"),
            Localization::Localize(Structs::TriggerDetectModeNames[detectMode]),
            bodyRadius,
            headRadius
        );

        drawList->AddText(
            ImVec2(12.0f, leftHudY),
            ToImColor(config.Aim.TriggerHitboxDebugColor),
            triggerDebugText
        );
        leftHudY += ImGui::GetFontSize() + 2.0f;
    }

    for (const PlayerEspSnapshot& player : frame.Players)
        RenderPlayer(drawList, player);

    if (config.Visuals.C4)
        RenderC4(drawList, frame.C4);

    publishPerf();
}
