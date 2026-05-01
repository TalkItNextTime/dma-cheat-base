#include <iostream>
#include <string>
#include <vector>

#include "Features/ESP/PlayerBoxModel.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    PlayerBoxModel::ScreenBone Bone(const int index, const float x, const float y)
    {
        return PlayerBoxModel::ScreenBone{
            index,
            Vector2{ x, y },
            true
        };
    }
}

int main()
{
    bool ok = true;

    const std::vector<PlayerBoxModel::ScreenBone> normalBones = {
        Bone(6, 500.0f, 200.0f),
        Bone(1, 500.0f, 300.0f),
        Bone(17, 480.0f, 380.0f),
        Bone(20, 520.0f, 380.0f),
        Bone(19, 475.0f, 520.0f),
        Bone(22, 525.0f, 520.0f),
    };

    PlayerBoxModel::Box2D normalBox{};
    ok &= ExpectTrue(
        PlayerBoxModel::BuildBoxFromBones(normalBones, 1920.0f, 1080.0f, normalBox),
        "normal tracked bones should build a box");
    ok &= ExpectTrue(
        PlayerBoxModel::IsBoxSane(normalBox, 1920.0f, 1080.0f),
        "normal tracked bone box should be sane");

    std::vector<PlayerBoxModel::ScreenBone> draggedBones = normalBones;
    draggedBones.push_back(Bone(11, 1850.0f, 520.0f));
    PlayerBoxModel::Box2D draggedBox{};
    ok &= ExpectTrue(
        !PlayerBoxModel::BuildBoxFromBones(draggedBones, 1920.0f, 1080.0f, draggedBox),
        "single far projected bone should reject dragged box");

    const std::vector<PlayerBoxModel::ScreenBone> wideBones = {
        Bone(6, 300.0f, 300.0f),
        Bone(1, 900.0f, 305.0f),
        Bone(17, 450.0f, 310.0f),
        Bone(20, 750.0f, 315.0f),
    };
    PlayerBoxModel::Box2D wideBox{};
    ok &= ExpectTrue(
        !PlayerBoxModel::BuildBoxFromBones(wideBones, 1920.0f, 1080.0f, wideBox),
        "extreme width-to-height box should be rejected");

    const std::vector<PlayerBoxModel::ScreenBone> missingCoreBones = {
        Bone(11, 500.0f, 300.0f),
        Bone(15, 520.0f, 305.0f),
        Bone(19, 510.0f, 330.0f),
    };
    PlayerBoxModel::Box2D missingCoreBox{};
    ok &= ExpectTrue(
        !PlayerBoxModel::BuildBoxFromBones(missingCoreBones, 1920.0f, 1080.0f, missingCoreBox),
        "box should require enough core body bones");

    if (!ok)
        return 1;

    std::cout << "[PASS] player_box_model_tests\n";
    return 0;
}
