#pragma once

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
