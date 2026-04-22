#include <iostream>
#include <string>

#include "Config/Structs.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(const int actual, const int expected, const std::string& message)
    {
        if (actual == expected)
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

    ok &= ExpectEqual(static_cast<int>(Structs::AimBoneIds.size()), 17, "bone schema should keep 17 tracked points");
    ok &= ExpectEqual(Structs::AimBoneIds[0], 1, "root tracked bone should match latest spine_lower index");
    ok &= ExpectEqual(Structs::AimBoneIds[2], 23, "middle torso bone should match latest spine_middle2 index");
    ok &= ExpectEqual(Structs::AimBoneIds[4], 7, "top tracked bone should match latest neck index");
    ok &= ExpectEqual(Structs::AimBoneIds[7], 11, "left hand endpoint should match latest wrist index");
    ok &= ExpectEqual(Structs::AimBoneIds[16], 22, "right foot endpoint should match latest foot index");
    ok &= ExpectEqual(Structs::AimHeadBoneId, 7, "head-target slot should follow the latest top-point index");

    ok &= ExpectTrue(
        Structs::BoneMaskFromBoneId(23) != 0ull &&
        Structs::BoneMaskFromBoneId(11) != 0ull &&
        Structs::BoneMaskFromBoneId(22) != 0ull,
        "updated torso, arm, and leg bones should all resolve into selectable mask bits");

    if (!ok)
        return 1;

    std::cout << "[PASS] bone_schema_tests\n";
    return 0;
}
