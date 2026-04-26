#include <cmath>
#include <iostream>
#include <string>

#include "Features/ESP/C4CardModel.hpp"

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

    bool ExpectNear(const double actual, const double expected, const double epsilon, const std::string& message)
    {
        if (std::abs(actual - expected) <= epsilon)
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

    const C4CardModel planting = BuildC4CardModel({
        .Valid = true,
        .Planted = false,
        .StartedArming = true,
        .PlantingViaUse = false,
        .BombSite = -1,
        .BeingDefused = false,
        .TimeRemaining = 0.0f,
        .DefuseCountDown = 0.0f,
        .CanDefuse = false,
        .PlantCountdown = 2.4f,
    });

    ok &= ExpectEqual(
        static_cast<int>(planting.Mode),
        static_cast<int>(C4CardMode::Planting),
        "planting signal should keep the timer card visible in planting mode");
    ok &= ExpectNear(
        planting.PlantCountdown,
        2.4,
        0.001,
        "planting mode should preserve the planting countdown");

    const C4CardModel planted = BuildC4CardModel({
        .Valid = true,
        .Planted = true,
        .StartedArming = false,
        .PlantingViaUse = false,
        .BombSite = 1,
        .BeingDefused = true,
        .TimeRemaining = 18.6f,
        .DefuseCountDown = 7.1f,
        .CanDefuse = true,
        .PlantCountdown = 0.0f,
    });

    ok &= ExpectEqual(
        static_cast<int>(planted.Mode),
        static_cast<int>(C4CardMode::Planted),
        "planted bomb should stay in planted timer-card mode");
    ok &= ExpectEqual(planted.BombSite, 1, "planted mode should preserve bomb site");
    ok &= ExpectTrue(planted.BeingDefused, "planted mode should preserve defuse state");
    ok &= ExpectNear(planted.TimeRemaining, 18.6, 0.001, "planted mode should preserve explode countdown");
    ok &= ExpectNear(planted.DefuseCountDown, 7.1, 0.001, "planted mode should preserve defuse countdown");
    ok &= ExpectTrue(planted.CanDefuse, "planted mode should preserve defuse result");

    const C4CardModel carried = BuildC4CardModel({
        .Valid = true,
        .Planted = false,
        .StartedArming = false,
        .PlantingViaUse = false,
        .BombSite = -1,
        .BeingDefused = false,
        .TimeRemaining = 0.0f,
        .DefuseCountDown = 0.0f,
        .CanDefuse = false,
        .PlantCountdown = 0.0f,
    });

    ok &= ExpectEqual(
        static_cast<int>(carried.Mode),
        static_cast<int>(C4CardMode::Hidden),
        "carried or dropped bomb should not reuse the timer card");

    const C4CardModel invalidPlantedTimer = BuildC4CardModel({
        .Valid = true,
        .Planted = true,
        .StartedArming = false,
        .PlantingViaUse = false,
        .BombSite = 0,
        .BeingDefused = true,
        .TimeRemaining = 45000.0f,
        .DefuseCountDown = 45000.0f,
        .CanDefuse = true,
        .PlantCountdown = 0.0f,
    });

    ok &= ExpectEqual(
        static_cast<int>(invalidPlantedTimer.Mode),
        static_cast<int>(C4CardMode::Hidden),
        "planted card should hide impossible countdown values outside 0-100 seconds");

    const C4CardModel invalidPlantTimer = BuildC4CardModel({
        .Valid = true,
        .Planted = false,
        .StartedArming = true,
        .PlantingViaUse = false,
        .BombSite = -1,
        .BeingDefused = false,
        .TimeRemaining = 0.0f,
        .DefuseCountDown = 0.0f,
        .CanDefuse = false,
        .PlantCountdown = 45000.0f,
    });

    ok &= ExpectEqual(
        static_cast<int>(invalidPlantTimer.Mode),
        static_cast<int>(C4CardMode::Hidden),
        "planting card should hide impossible planting countdown values outside 0-100 seconds");

    const C4CardModel expiredPlantTimer = BuildC4CardModel({
        .Valid = true,
        .Planted = false,
        .StartedArming = true,
        .PlantingViaUse = false,
        .BombSite = -1,
        .BeingDefused = false,
        .TimeRemaining = 0.0f,
        .DefuseCountDown = 0.0f,
        .CanDefuse = false,
        .PlantCountdown = 0.0f,
    });

    ok &= ExpectEqual(
        static_cast<int>(expiredPlantTimer.Mode),
        static_cast<int>(C4CardMode::Hidden),
        "planting card should hide when planting countdown has expired");

    if (!ok)
        return 1;

    std::cout << "[PASS] c4_card_logic_tests\n";
    return 0;
}
