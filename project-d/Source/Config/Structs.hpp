#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "imgui/imgui.h"

namespace Structs
{
    enum AimWeaponGroup : int
    {
        AimWeapon_Pistol = 0,
        AimWeapon_Smg,
        AimWeapon_Shotgun,
        AimWeapon_Rifle,
        AimWeapon_Sniper,
        AimWeapon_Lmg,
        AimWeapon_Count
    };

    inline constexpr std::array<const char*, AimWeapon_Count> AimWeaponGroupNames = {
        "Pistol",
        "SMG",
        "Shotgun",
        "Rifle",
        "Sniper",
        "LMG"
    };

    inline constexpr std::array<const char*, AimWeapon_Count> AimWeaponGroupNamesZh = {
        "手枪",
        "冲锋枪",
        "霰弹枪",
        "步枪",
        "狙击枪",
        "轻机枪"
    };

    enum AimTargetStrategy : int
    {
        AimStrategy_Crosshair = 0,
        AimStrategy_Distance,
        AimStrategy_Hybrid
    };

    inline constexpr std::array<const char*, 3> AimTargetStrategyNames = {
        "Crosshair Closest",
        "Distance Closest",
        "Hybrid"
    };

    inline constexpr std::array<const char*, 3> AimTargetStrategyNamesZh = {
        "准星最近",
        "距离最近",
        "混合"
    };

    enum AimHitGroupBit : std::uint32_t
    {
        AimHit_Head = 1u << 0,
        AimHit_UpperChest = 1u << 1,
        AimHit_Torso = 1u << 2,
        AimHit_Pelvis = 1u << 3,
        AimHit_Arms = 1u << 4,
        AimHit_Legs = 1u << 5
    };

    inline constexpr std::array<const char*, 6> AimHitGroupNames = {
        "Head",
        "Upper Chest",
        "Torso",
        "Pelvis",
        "Arms",
        "Legs"
    };

    inline constexpr std::array<const char*, 6> AimHitGroupNamesZh = {
        "头部",
        "上胸",
        "躯干",
        "骨盆",
        "手臂",
        "腿部"
    };

    inline constexpr std::array<int, 17> AimBoneIds = {
        0, 2, 4, 5, 6,
        8, 9, 10,
        13, 14, 15,
        22, 23, 24,
        25, 26, 27
    };

    inline constexpr std::array<const char*, 17> AimBoneNames = {
        "Pelvis",
        "Spine",
        "Chest",
        "Neck",
        "Head",
        "L Shoulder",
        "L Elbow",
        "L Hand",
        "R Shoulder",
        "R Elbow",
        "R Hand",
        "L Thigh",
        "L Knee",
        "L Foot",
        "R Thigh",
        "R Knee",
        "R Foot"
    };

    inline constexpr std::array<const char*, 17> AimBoneNamesZh = {
        "骨盆",
        "脊柱",
        "胸部",
        "颈部",
        "头部",
        "左肩",
        "左肘",
        "左手",
        "右肩",
        "右肘",
        "右手",
        "左大腿",
        "左膝",
        "左脚",
        "右大腿",
        "右膝",
        "右脚"
    };

    inline constexpr int AimHeadBoneId = 6;
    inline constexpr std::uint64_t AimAllBoneMask = (1ull << AimBoneIds.size()) - 1ull;
    inline constexpr std::uint64_t AimDefaultAimbotBoneMask =
        (1ull << 1) | // Spine
        (1ull << 2) | // Chest
        (1ull << 3) | // Neck
        (1ull << 4);  // Head

    inline constexpr int BoneSlotFromId(const int boneId)
    {
        for (size_t i = 0; i < AimBoneIds.size(); ++i)
        {
            if (AimBoneIds[i] == boneId)
                return static_cast<int>(i);
        }
        return -1;
    }

    inline constexpr std::uint64_t BoneMaskFromBoneId(const int boneId)
    {
        const int slot = BoneSlotFromId(boneId);
        if (slot < 0)
            return 0ull;
        return 1ull << static_cast<std::uint64_t>(slot);
    }

    enum TriggerDetectMode : int
    {
        TriggerDetect_BoneHitbox = 0,
        TriggerDetect_CrosshairEntity
    };

    inline constexpr std::array<const char*, 2> TriggerDetectModeNames = {
        "Bone Hitbox",
        "Crosshair Entity"
    };

    inline constexpr std::array<const char*, 2> TriggerDetectModeNamesZh = {
        "骨骼命中盒",
        "准星实体"
    };

    enum TriggerSpecialWeapon : int
    {
        TriggerSpecial_Deagle = 0,
        TriggerSpecial_Revolver,
        TriggerSpecial_Count
    };

    inline constexpr std::array<const char*, TriggerSpecial_Count> TriggerSpecialWeaponNames = {
        "Desert Eagle",
        "R8 Revolver"
    };

    inline constexpr std::array<const char*, TriggerSpecial_Count> TriggerSpecialWeaponNamesZh = {
        "沙漠之鹰",
        "R8 左轮"
    };

    struct AimWeaponProfile
    {
        float Fov = 6.5f;
        float Smooth = 16.0f;
        float SprayAxisStrengthX = 1.30f;
        float SprayAxisStrengthY = 1.55f;
        float SprayAxisMaxStepX = 6.0f;
        float SprayAxisMaxStepY = 8.0f;
        float SprayAxisDeadzoneX = 0.08f;
        float SprayAxisDeadzoneY = 0.06f;
        float CurveStrength = 0.22f;
        std::uint64_t BoneMask = AimDefaultAimbotBoneMask;

        int TargetStrategy = AimStrategy_Crosshair;
        int TargetSwitchDelayMs = 120;
    };

    struct TriggerWeaponProfile
    {
        float HitboxRadiusPx = 4.5f;
        int PreFireDelayMs = 35;
        int PostFireIntervalMs = 55;
        int TimeoutForceFireMs = 0;
        std::uint64_t BoneMask = AimAllBoneMask;
    };

    struct TriggerSpecialProfile
    {
        float HitboxRadiusPx = 4.5f;
        int PreFireDelayMs = 35;
        int PostFireIntervalMs = 425;
        int TimeoutForceFireMs = 0;
        int HoldFireMs = 8;
        std::uint64_t BoneMask = AimAllBoneMask;
    };

    struct KmboxConfig 
    {
        bool Enabled = false;
        std::string Ip{};
        unsigned short Port = 0;
        std::string Uuid{};
    };

    struct AimConfig 
    {
        bool Trigger = false;
        int TriggerKey = 0;
        int TriggerKeyMode = 1;
        int TriggerDelay = 0;
        bool TriggerSecondKeyEnabled = false;
        int TriggerSecondKey = 0;
        int TriggerSecondKeyMode = 1;
        int TriggerMinIntervalMs = 35;
        int TriggerDetectMode = TriggerDetect_BoneHitbox;
        float TriggerUnifiedHitboxRadiusPx = 4.5f;
        float TriggerHitboxScale = 1.0f;
        float TriggerHitboxAddPx = 0.0f;
        float TriggerHeadRadiusPx = 9.0f;
        float TriggerHeadScale = 1.15f;
        float TriggerTorsoScale = 1.20f;
        float TriggerArmsScale = 0.90f;
        float TriggerLegsScale = 1.00f;
        bool TriggerHeadSphereDebug = true;
        std::uint64_t TriggerBoneMask = AimAllBoneMask;
        std::uint32_t TriggerHitGroupMask =
            AimHit_Head |
            AimHit_UpperChest |
            AimHit_Torso |
            AimHit_Pelvis |
            AimHit_Arms |
            AimHit_Legs;

        bool TriggerHitboxDebug = false;
        ImVec4 TriggerHitboxDebugColor = ImVec4(1.0f, 0.55f, 0.2f, 0.9f);
        ImVec4 TriggerHitboxDebugActiveColor = ImVec4(0.2f, 1.0f, 0.35f, 0.95f);
        float TriggerHitboxDebugThickness = 1.0f;

        bool BlockTriggerWhenFlashed = false;
        bool BlockAimbotWhenFlashed = false;

        bool Aimbot = false;

        bool DrawFov = false;
        ImVec4 AimbotFovColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool AimFriendly = false;
        bool AimVisible = false;

        int AimbotKey = 0;
        int AimbotKeyMode = 1;
        bool AimbotSecondKeyEnabled = false;
        int AimbotSecondKey = 0;
        int AimbotSecondKeyMode = 1;
        std::uint64_t AimbotBoneMask = AimDefaultAimbotBoneMask;
        std::uint32_t AimbotHitGroupMask = AimHit_Head | AimHit_UpperChest | AimHit_Torso;

        float DeadzonePx = 1.2f;

        // Legacy values remain for backward compatibility with old configs.
        float AimbotFov = 6.5f;
        float AimbotSmooth = 16.0f;

        std::array<AimWeaponProfile, AimWeapon_Count> WeaponProfiles{};
        int WeaponProfileEditorIndex = 0;

        std::array<TriggerWeaponProfile, AimWeapon_Count> TriggerProfiles{};
        int TriggerProfileEditorIndex = 0;

        std::array<TriggerSpecialProfile, TriggerSpecial_Count> TriggerSpecialProfiles{};
        int TriggerSpecialEditorIndex = 0;
    };

    struct VisualsConfig 
    {
        bool Enabled = false;
        bool VSync = false;
        bool TeamCheck = false;
        bool VisibleCheck = false;
        bool VisCheckDebug = false;
        int VisCheckDebugMode = 1; // Deprecated legacy config key. Debug now always renders full map overlay.
        float VisCheckDebugMaxDistance = 3200.0f;
        int VisCheckDebugMaxItems = 1400;
        ImVec4 VisCheckDebugColor = ImVec4(0.25f, 0.85f, 1.0f, 0.65f);

        bool Background = false;

        bool Hitmarker = false;
        ImVec4 HitmarkerColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool Watermark = false;
        ImVec4 WatermarkColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool Name = false;
        ImVec4 NameColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool Box = false;
        ImVec4 BoxColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 BoxColorVisible = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool Health = false;
        bool Armor = true;
        ImVec4 ArmorColor = ImVec4(0.65f, 0.85f, 1.0f, 1.0f);
        bool Money = true;
        ImVec4 MoneyColor = ImVec4(0.65f, 1.0f, 0.65f, 1.0f);

        bool Weapon = false;
        ImVec4 WeaponColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        bool Bones = false;
        ImVec4 BonesColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 BonesColorVisible = ImVec4(0.45f, 1.0f, 0.55f, 1.0f);

        bool C4 = true;
        ImVec4 C4Color = ImVec4(1.0f, 0.55f, 0.35f, 1.0f);
        float C4PanelPosX = 0.02f;
        float C4PanelPosY = 0.06f;
        bool Defuser = true;
        ImVec4 DefuserColor = ImVec4(1.0f, 0.82f, 0.2f, 1.0f);

        bool GrenadeHelper = false;
        bool GrenadeHelperFilterByWeapon = true;
        bool GrenadeHelperDrawStand = true;
        bool GrenadeHelperDrawAim = true;
        bool GrenadeHelperManualTypeOverride = false;
        int GrenadeHelperManualType = 0;
        float GrenadeHelperStandTolerance = 35.0f;
        float GrenadeHelperFocusRadius = 20.0f;
        float GrenadeHelperMaxStandDrawDistance = 2000.0f;
        float GrenadeHelperLooseGuideDistance = 200.0f;
        float GrenadeHelperTopHintOffsetX = 0.5f;
        float GrenadeHelperTopHintOffsetY = 0.03f;
        ImVec4 GrenadeHelperStandColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        ImVec4 GrenadeHelperAimColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        ImVec4 GrenadeHelperGuideLineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.75f);
        ImVec4 GrenadeHelperFontColor = ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
        float GrenadeHelperFontSize = 16.0f;
        ImVec4 GrenadeHelperTopHintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        float GrenadeHelperTopHintFontSize = 30.0f;
    };
}
