#include <iostream>
#include <string>

#include "SDK/Offsets.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(const std::uint32_t actual, const std::uint32_t expected, const std::string& message)
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

    constexpr std::uint32_t kValidWeaponHandle = 0x00001234u;
    constexpr std::uint32_t kInvalidHandleAllBits = 0xFFFFFFFFu;
    constexpr std::uint32_t kInvalidHandleMaskOnly = Offsets::EntityList::HandleMask;

    ok &= ExpectTrue(
        Offsets::EntityList::IsHandleValid(kValidWeaponHandle),
        "normal weapon handles should remain valid after the schema update");
    ok &= ExpectTrue(
        !Offsets::EntityList::IsHandleValid(kInvalidHandleAllBits),
        "0xFFFFFFFF active-weapon handles must be rejected before entity-list resolution");
    ok &= ExpectTrue(
        !Offsets::EntityList::IsHandleValid(kInvalidHandleMaskOnly),
        "0x7FFF masked handles must be treated as invalid sentinels");

    ok &= ExpectEqual(
        Offsets::EntityList::HandleIndex(kValidWeaponHandle),
        kValidWeaponHandle & Offsets::EntityList::HandleMask,
        "handle index helper should preserve the entity-list decode mask");

    if (!ok)
        return 1;

    std::cout << "[PASS] active_weapon_handle_tests\n";
    return 0;
}
