#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "imgui/imgui.h"

namespace GrenadeEntityEspModel
{
    enum class Type : std::uint8_t
    {
        Unknown = 0,
        HE,
        Molotov,
        Smoke,
        Flash,
        Decoy
    };

    struct TypeToggles
    {
        bool HE = true;
        bool Molotov = true;
        bool Smoke = true;
        bool Flash = true;
        bool Decoy = true;
    };

    inline Type ClassifyDesignerName(std::string_view designerName)
    {
        const bool isProjectile = designerName.find("projectile") != std::string_view::npos;
        const bool isInferno = designerName.find("inferno") != std::string_view::npos;
        if (!isProjectile && !isInferno)
            return Type::Unknown;

        if (designerName.find("hegrenade") != std::string_view::npos)
            return Type::HE;
        if (designerName.find("molotov") != std::string_view::npos || designerName.find("incendiary") != std::string_view::npos || designerName.find("incgrenade") != std::string_view::npos || isInferno)
            return Type::Molotov;
        if (designerName.find("smokegrenade") != std::string_view::npos)
            return Type::Smoke;
        if (designerName.find("flashbang") != std::string_view::npos)
            return Type::Flash;
        if (designerName.find("decoy") != std::string_view::npos)
            return Type::Decoy;
        return Type::Unknown;
    }

    inline bool ShouldShowType(const Type type, const TypeToggles& toggles)
    {
        switch (type)
        {
        case Type::HE:
            return toggles.HE;
        case Type::Molotov:
            return toggles.Molotov;
        case Type::Smoke:
            return toggles.Smoke;
        case Type::Flash:
            return toggles.Flash;
        case Type::Decoy:
            return toggles.Decoy;
        default:
            return false;
        }
    }

    inline const char* DisplayName(const Type type)
    {
        switch (type)
        {
        case Type::HE:
            return "HE";
        case Type::Molotov:
            return "Fire";
        case Type::Smoke:
            return "Smoke";
        case Type::Flash:
            return "Flash";
        case Type::Decoy:
            return "Decoy";
        default:
            return "Unknown";
        }
    }

    inline const char* IconToken(const Type type)
    {
        switch (type)
        {
        case Type::HE:
            return "hegrenade";
        case Type::Molotov:
            return "molotov";
        case Type::Smoke:
            return "smokegrenade";
        case Type::Flash:
            return "flashbang";
        case Type::Decoy:
            return "decoy";
        default:
            return "worldent";
        }
    }

    inline ImVec4 CountdownColor(const Type type, const float countdownSeconds, const ImVec4& configuredColor)
    {
        if ((type == Type::HE || type == Type::Flash) && countdownSeconds >= 0.0f && countdownSeconds < 1.0f)
            return ImVec4(1.0f, 0.12f, 0.10f, std::max(configuredColor.w, 0.95f));
        return configuredColor;
    }
}
