#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

#include "imgui/imgui.h"

namespace SoundEspModel
{
    enum class SoundKind
    {
        Unknown,
        WeaponFire,
        Footstep,
        Jump,
        Land,
        Grenade,
        Reload,
        Bomb
    };

    struct RippleStyle
    {
        SoundKind Kind = SoundKind::Unknown;
        ImVec4 Color = ImVec4(0.86f, 0.86f, 0.86f, 1.0f);
        float SpeedPxPerSecond = 220.0f;
        float LifetimeSeconds = 1.25f;
        float Thickness = 1.4f;
        float Strength = 0.65f;
        float VerticalLiftPx = 0.0f;
    };

    inline std::string ToLowerAscii(std::string_view value)
    {
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return lowered;
    }

    inline bool ContainsAny(const std::string& text, std::initializer_list<std::string_view> needles)
    {
        for (const std::string_view needle : needles)
        {
            if (!needle.empty() && text.find(needle) != std::string::npos)
                return true;
        }
        return false;
    }

    inline RippleStyle ClassifySoundName(std::string_view rawName)
    {
        const std::string name = ToLowerAscii(rawName);

        if (ContainsAny(name, { "fire", "shoot", "weapon_" }))
            return { SoundKind::WeaponFire, ImVec4(1.0f, 0.08f, 0.08f, 1.0f), 520.0f, 1.55f, 2.4f, 1.0f, 0.0f };

        if (ContainsAny(name, { "footstep", "step", "run", "walk" }))
            return { SoundKind::Footstep, ImVec4(0.25f, 0.62f, 1.0f, 1.0f), 210.0f, 1.15f, 1.3f, 0.55f, 0.0f };

        if (ContainsAny(name, { "player_jump", "jump" }))
            return { SoundKind::Jump, ImVec4(0.72f, 0.35f, 1.0f, 1.0f), 280.0f, 1.35f, 1.7f, 0.75f, 18.0f };

        if (ContainsAny(name, { "player_land", "land" }))
            return { SoundKind::Land, ImVec4(1.0f, 0.48f, 0.12f, 1.0f), 300.0f, 1.30f, 1.8f, 0.78f, 0.0f };

        if (ContainsAny(name, { "pinpull", "grenade_pin", "throw" }))
            return { SoundKind::Grenade, ImVec4(1.0f, 0.86f, 0.15f, 1.0f), 260.0f, 1.45f, 1.8f, 0.82f, 0.0f };

        if (ContainsAny(name, { "reload" }))
            return { SoundKind::Reload, ImVec4(0.70f, 0.92f, 1.0f, 1.0f), 190.0f, 1.10f, 1.2f, 0.45f, 0.0f };

        if (ContainsAny(name, { "bomb_plant", "plant" }))
            return { SoundKind::Bomb, ImVec4(1.0f, 0.72f, 0.25f, 1.0f), 330.0f, 1.70f, 2.0f, 0.92f, 0.0f };

        return {};
    }

    inline bool ShouldAcceptDistance(const float distance, const float maxDistance)
    {
        return std::isfinite(distance) && distance >= 0.0f && distance <= (std::max)(0.0f, maxDistance);
    }

    inline float GroundRippleRadius(const RippleStyle& style, const float ageSeconds)
    {
        return 5.0f + (std::max)(0.0f, ageSeconds) * (std::max)(0.0f, style.SpeedPxPerSecond);
    }

    inline bool ShouldRenderPlayerInfo(const bool legitMode, const bool pawnHasActiveSoundRipple, const bool pawnIsVisible)
    {
        return !legitMode || pawnHasActiveSoundRipple || pawnIsVisible;
    }

    inline bool ShouldPollPawnSoundsFromLocalState(const bool localStateReadOk, const int, const int)
    {
        return localStateReadOk;
    }
}
