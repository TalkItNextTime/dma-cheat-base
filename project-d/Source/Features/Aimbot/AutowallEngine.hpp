#pragma once

#include <Pch.hpp>
#include "../../../VisCheckCS2/VisCheck.h"

class AutowallEngine
{
public:
    struct Result
    {
        float RemainingDamage = 0.0f;
        bool CanPenetrate = false;
    };

    static AutowallEngine& Get()
    {
        static AutowallEngine instance;
        return instance;
    }

    Result Evaluate(
        const Vector3& shooter,
        const Vector3& target,
        int weaponId,
        const std::vector<VisCheck::PenetrationSegment>& segments) const;

private:
    struct WeaponPenetrationData
    {
        float BaseDamage = 36.0f;
        float RangeModifier = 0.98f;
        float PenetrationPower = 160.0f;
    };

    struct MaterialPenetrationData
    {
        std::string Name = "default";
        float DistanceModifier = 0.5f;
        float DamageModifier = 0.5f;
    };

    AutowallEngine();

    const WeaponPenetrationData& ResolveWeapon(int weaponId) const;
    const MaterialPenetrationData& ResolveMaterial(std::uint32_t materialHash) const;
    bool LoadMaterialTables();
    static float Distance3D(const Vector3& a, const Vector3& b);
    static std::string ToLowerAscii(std::string value);

private:
    std::unordered_map<int, WeaponPenetrationData> m_WeaponTable{};
    std::unordered_map<std::uint32_t, MaterialPenetrationData> m_MaterialsByHash{};
    MaterialPenetrationData m_DefaultMaterial{};
    WeaponPenetrationData m_DefaultWeapon{};
    bool m_MaterialsLoaded = false;
};

