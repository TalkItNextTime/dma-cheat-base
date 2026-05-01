#include <iostream>
#include <string>

#include "Features/ESP/WeaponEspModel.hpp"

namespace
{
    bool ExpectEqual(const std::string& actual, const std::string& expected, const std::string& message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: " << expected << "\n"
                  << "  actual  : " << actual << '\n';
        return false;
    }

    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(7), "AK-47", "AK-47 id should map to display name");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("AK-47"), "ak47", "AK-47 should map to ak47 icon");
    ok &= ExpectEqual(WeaponEspModel::GsiNameFromName("AK-47"), "weapon_ak_47", "AK-47 should map to GSI token");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(43), "Flashbang", "flashbang id should map to display name");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Flashbang"), "flashbang", "flashbang should map to icon");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(49), "C4", "C4 id should map to display name");
    ok &= ExpectEqual(WeaponEspModel::GsiNameFromName("C4"), "weapon_c4", "C4 should map to canonical GSI token");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(42), "Knife", "default CT knife should map");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Knife"), "knife", "default knife icon should map");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(59), "Knife (T)", "default T knife should map");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Knife (T)"), "knife_t", "T knife icon should map");

    const int specialKnives[] = {
        500, 503, 505, 506, 507, 508, 509, 512, 514, 515,
        516, 517, 518, 519, 520, 521, 522, 523, 525, 526
    };

    for (const int weaponId : specialKnives)
    {
        const std::string name = WeaponEspModel::NameFromDefinitionId(weaponId);
        const std::string icon = WeaponEspModel::IconTokenFromName(name);
        ok &= ExpectTrue(!name.empty(), "special knife id should not resolve to empty name: " + std::to_string(weaponId));
        ok &= ExpectTrue(name.rfind("Weapon ", 0) != 0, "special knife id should not fall back to Weapon N: " + std::to_string(weaponId));
        ok &= ExpectTrue(!icon.empty(), "special knife id should resolve to an icon token: " + std::to_string(weaponId));
    }

    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(503), "Classic Knife", "classic knife should map by id");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Classic Knife"), "knife_css", "classic knife should map to existing icon");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(507), "Karambit", "karambit should map by id");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Karambit"), "knife_karambit", "karambit should map to existing icon");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(526), "Kukri Knife", "kukri should map by id");
    ok &= ExpectEqual(WeaponEspModel::IconTokenFromName("Kukri Knife"), "knife_kukri", "kukri should map to existing icon");
    ok &= ExpectEqual(WeaponEspModel::NameFromDefinitionId(501), "Knife", "unknown high knife-like id should use generic knife");

    if (!ok)
        return 1;

    std::cout << "[PASS] weapon_esp_model_tests\n";
    return 0;
}
