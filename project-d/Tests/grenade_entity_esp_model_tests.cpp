#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "Features/ESP/GrenadeEntityEspModel.hpp"

namespace
{
    bool Expect(bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "[FAIL] " << message << '\n';
        return condition;
    }

    bool NearlyEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) < 0.001f;
    }
}

int main()
{
    bool ok = true;

    using GrenadeEntityEspModel::Type;

    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("hegrenade_projectile") == Type::HE, "he projectile should classify as HE");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("flashbang_projectile") == Type::Flash, "flash projectile should classify as Flash");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("smokegrenade_projectile") == Type::Smoke, "smoke projectile should classify as Smoke");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("molotov_projectile") == Type::Molotov, "molotov projectile should classify as Fire");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("incendiarygrenade_projectile") == Type::Molotov, "incendiary projectile should classify as Fire");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("inferno") == Type::Molotov, "inferno should classify as Fire");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("decoy_projectile") == Type::Decoy, "decoy projectile should classify as Decoy");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("weapon_hegrenade") == Type::Unknown, "held HE weapon should not classify as ESP entity");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("weapon_flashbang") == Type::Unknown, "held flash weapon should not classify as ESP entity");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("weapon_smokegrenade") == Type::Unknown, "held smoke weapon should not classify as ESP entity");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("weapon_molotov") == Type::Unknown, "held molotov weapon should not classify as ESP entity");
    ok &= Expect(GrenadeEntityEspModel::ClassifyDesignerName("weapon_decoy") == Type::Unknown, "held decoy weapon should not classify as ESP entity");

    GrenadeEntityEspModel::TypeToggles toggles{};
    toggles.Smoke = false;
    ok &= Expect(!GrenadeEntityEspModel::ShouldShowType(Type::Smoke, toggles), "disabled smoke should not render");
    ok &= Expect(GrenadeEntityEspModel::ShouldShowType(Type::HE, toggles), "enabled HE should render");

    const ImVec4 normal(0.2f, 0.6f, 1.0f, 0.7f);
    const ImVec4 urgentHe = GrenadeEntityEspModel::CountdownColor(Type::HE, 0.9f, normal);
    const ImVec4 normalHe = GrenadeEntityEspModel::CountdownColor(Type::HE, 1.0f, normal);
    const ImVec4 smoke = GrenadeEntityEspModel::CountdownColor(Type::Smoke, 0.5f, normal);

    ok &= Expect(urgentHe.x > 0.95f && urgentHe.y < 0.2f && urgentHe.z < 0.2f && urgentHe.w >= 0.95f, "HE under 1s should use urgent red");
    ok &= Expect(NearlyEqual(normalHe.x, normal.x) && NearlyEqual(normalHe.y, normal.y), "HE at 1s should keep configured color");
    ok &= Expect(NearlyEqual(smoke.x, normal.x) && NearlyEqual(smoke.y, normal.y), "smoke countdown should keep configured color");

    if (!ok)
        return 1;

    std::cout << "[PASS] grenade_entity_esp_model_tests\n";
    return 0;
}
