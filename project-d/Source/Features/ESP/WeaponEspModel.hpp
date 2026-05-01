#pragma once

#include <string>

namespace WeaponEspModel
{
    std::string NameFromDefinitionId(int weaponId);
    std::string IconTokenFromName(const std::string& weaponName);
    std::string GsiNameFromName(const std::string& rawName);
}
