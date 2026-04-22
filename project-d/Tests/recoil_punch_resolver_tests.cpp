#include <cmath>
#include <iostream>
#include <string>

#include "Features/Aimbot/RecoilPunchResolver.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectNear(const float actual, const float expected, const float epsilon, const std::string& message)
    {
        if (std::fabs(actual - expected) <= epsilon)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: " << expected << "\n"
                  << "  actual  : " << actual << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    const AimPunchServiceState splitPunch{
        .PredictableBaseAngle = { 0.45f, -0.20f, 0.00f },
        .UnpredictableBaseAngle = { 0.05f, 0.08f, 0.00f },
    };

    const Vector3 resolvedSplitPunch = ResolveRecoilPunch(splitPunch);
    ok &= ExpectNear(
        resolvedSplitPunch.x,
        0.50f,
        0.0001f,
        "resolver should merge predictable and unpredictable pitch recoil");
    ok &= ExpectNear(
        resolvedSplitPunch.y,
        -0.12f,
        0.0001f,
        "resolver should merge predictable and unpredictable yaw recoil");
    ok &= ExpectNear(
        resolvedSplitPunch.z,
        0.00f,
        0.0001f,
        "resolver should keep roll at zero for recoil compensation");

    const AimPunchServiceState zeroPunch{};
    ok &= ExpectTrue(
        ResolveRecoilPunch(zeroPunch).Zero(),
        "resolver should preserve a zero recoil state");

    if (!ok)
        return 1;

    std::cout << "[PASS] recoil_punch_resolver_tests\n";
    return 0;
}
