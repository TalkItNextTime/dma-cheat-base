#pragma once

#include <algorithm>

#include "Math/Vector.hpp"

struct AimPunchServiceState
{
    Vector3 PredictableBaseAngle{};
    Vector3 UnpredictableBaseAngle{};
};

inline Vector3 ResolveRecoilPunch(const AimPunchServiceState& state)
{
    return state.PredictableBaseAngle + state.UnpredictableBaseAngle;
}

inline Vector2 ClampSprayAxisMove(const Vector2& move, const float maxStepX, const float maxStepY)
{
    const float clampedMaxStepX = (std::max)(1.0f, maxStepX);
    const float clampedMaxStepY = (std::max)(1.0f, maxStepY);
    return {
        std::clamp(move.x, -clampedMaxStepX, clampedMaxStepX),
        std::clamp(move.y, -clampedMaxStepY, clampedMaxStepY),
    };
}
