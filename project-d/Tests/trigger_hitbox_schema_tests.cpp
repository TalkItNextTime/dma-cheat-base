#include <iostream>
#include <string>

#include "Features/Aimbot/TriggerHitboxSchema.hpp"

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

    ok &= ExpectEqual(
        static_cast<int>(TriggerHitboxSchema::Links.size()),
        16,
        "trigger hitbox schema should keep 16 segment links");
    ok &= ExpectEqual(
        TriggerHitboxSchema::SizingRootBoneId,
        1,
        "trigger hitbox sizing root should use latest pelvis/spine_lower point");
    ok &= ExpectEqual(
        TriggerHitboxSchema::Links[0].FromBone,
        1,
        "trigger hitbox first spine link should start from latest root bone");
    ok &= ExpectEqual(
        TriggerHitboxSchema::Links[2].ToBone,
        6,
        "trigger hitbox torso chain should pass through latest spine_upper point");
    ok &= ExpectEqual(
        TriggerHitboxSchema::Links[15].ToBone,
        22,
        "trigger hitbox last leg link should end on latest right foot");

    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromBone(7),
        0,
        "head bone should remain in head region");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromBone(23),
        1,
        "middle torso bone should be classified as torso");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromBone(11),
        2,
        "left wrist should be classified as arm");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromBone(22),
        3,
        "right foot should be classified as leg");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromLink(1, 23),
        1,
        "mixed torso links should still resolve as torso thickness");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromLink(17, 18),
        3,
        "same-region leg links should keep leg thickness");
    ok &= ExpectEqual(
        TriggerHitboxSchema::RegionFromLink(6, 7),
        0,
        "links touching head point should resolve as head thickness");
    ok &= ExpectEqual(
        static_cast<int>(TriggerHitboxSchema::ReferenceBodyHeightPx),
        150,
        "distance scaling should keep the shared latest body-height baseline");

    ok &= ExpectTrue(
        TriggerHitboxSchema::BonePointRadiusScale(1) > TriggerHitboxSchema::BonePointRadiusScale(11) &&
        TriggerHitboxSchema::BonePointRadiusScale(17) > TriggerHitboxSchema::BonePointRadiusScale(19),
        "point radius scales should keep torso/thigh segments thicker than wrists/feet");
    ok &= ExpectTrue(
        TriggerHitboxSchema::BoneLinkRadiusScale(1, 17) > TriggerHitboxSchema::BoneLinkRadiusScale(18, 19) &&
        TriggerHitboxSchema::BoneLinkRadiusScale(2, 23) > TriggerHitboxSchema::BoneLinkRadiusScale(10, 11),
        "segment radius scales should still taper from torso to limbs with the new schema");

    if (!ok)
        return 1;

    std::cout << "[PASS] trigger_hitbox_schema_tests\n";
    return 0;
}
