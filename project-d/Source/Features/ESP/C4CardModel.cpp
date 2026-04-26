#include "C4CardModel.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsLegalCountdownSeconds(const float value)
    {
        return std::isfinite(value) && value >= 0.0f && value <= 100.0f;
    }

    bool IsActivePlantCountdownSeconds(const float value)
    {
        return std::isfinite(value) && value > 0.0f && value <= 100.0f;
    }
}

C4CardModel BuildC4CardModel(const C4CardInput& input)
{
    if (!input.Valid)
        return {};

    if (input.Planted)
    {
        if (!IsLegalCountdownSeconds(input.TimeRemaining))
            return {};

        if (input.BeingDefused && !IsLegalCountdownSeconds(input.DefuseCountDown))
            return {};

        return {
            .Mode = C4CardMode::Planted,
            .BombSite = input.BombSite,
            .BeingDefused = input.BeingDefused,
            .TimeRemaining = (std::max)(0.0f, input.TimeRemaining),
            .DefuseCountDown = (std::max)(0.0f, input.DefuseCountDown),
            .CanDefuse = input.CanDefuse,
            .PlantCountdown = 0.0f,
        };
    }

    if (input.StartedArming || input.PlantingViaUse)
    {
        if (!IsActivePlantCountdownSeconds(input.PlantCountdown))
            return {};

        return {
            .Mode = C4CardMode::Planting,
            .BombSite = -1,
            .BeingDefused = false,
            .TimeRemaining = 0.0f,
            .DefuseCountDown = 0.0f,
            .CanDefuse = false,
            .PlantCountdown = (std::max)(0.0f, input.PlantCountdown),
        };
    }

    return {};
}
