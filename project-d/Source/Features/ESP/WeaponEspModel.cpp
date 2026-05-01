#include "WeaponEspModel.hpp"

#include <algorithm>
#include <cctype>

namespace
{
    std::string ToLowerAscii(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    std::string TrimAscii(std::string value)
    {
        const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
            value.erase(value.begin());
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
            value.pop_back();

        return value;
    }

    std::string NormalizeIconToken(std::string value)
    {
        value = ToLowerAscii(TrimAscii(std::move(value)));
        if (value.empty())
            return {};

        constexpr const char* kPngSuffix = ".png";
        if (value.size() > 4 && value.compare(value.size() - 4, 4, kPngSuffix) == 0)
            value = value.substr(0, value.size() - 4);

        for (char& ch : value)
        {
            if (ch == ' ' || ch == '-' || ch == '.')
                ch = '_';
        }

        if (value.rfind("weapon_", 0) == 0)
            value = value.substr(7);
        return value;
    }
}

namespace WeaponEspModel
{
    std::string NameFromDefinitionId(const int weaponId)
    {
        switch (weaponId)
        {
        case 1: return "Deagle";
        case 2: return "Dual Berettas";
        case 3: return "Five-Seven";
        case 4: return "Glock-18";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "PP-Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus x27";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 42: return "Knife";
        case 43: return "Flashbang";
        case 44: return "HE Grenade";
        case 45: return "Smoke";
        case 46: return "Molotov";
        case 47: return "Decoy";
        case 48: return "Incendiary";
        case 49: return "C4";
        case 57: return "Healthshot";
        case 59: return "Knife (T)";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75 Auto";
        case 64: return "R8 Revolver";
        case 500: return "Bayonet";
        case 503: return "Classic Knife";
        case 505: return "Flip Knife";
        case 506: return "Gut Knife";
        case 507: return "Karambit";
        case 508: return "M9 Bayonet";
        case 509: return "Huntsman Knife";
        case 512: return "Falchion Knife";
        case 514: return "Bowie Knife";
        case 515: return "Butterfly Knife";
        case 516: return "Shadow Daggers";
        case 517: return "Paracord Knife";
        case 518: return "Survival Knife";
        case 519: return "Ursus Knife";
        case 520: return "Navaja Knife";
        case 521: return "Nomad Knife";
        case 522: return "Stiletto Knife";
        case 523: return "Talon Knife";
        case 525: return "Skeleton Knife";
        case 526: return "Kukri Knife";
        default:
            if (weaponId >= 500 && weaponId < 600)
                return "Knife";
            return {};
        }
    }

    std::string IconTokenFromName(const std::string& weaponName)
    {
        if (weaponName.empty())
            return {};

        if (weaponName == "Deagle") return "deagle";
        if (weaponName == "Dual Berettas") return "elite";
        if (weaponName == "Five-Seven") return "fiveseven";
        if (weaponName == "Glock-18") return "glock";
        if (weaponName == "AK-47") return "ak47";
        if (weaponName == "AUG") return "aug";
        if (weaponName == "AWP") return "awp";
        if (weaponName == "FAMAS") return "famas";
        if (weaponName == "G3SG1") return "g3sg1";
        if (weaponName == "Galil AR") return "galilar";
        if (weaponName == "M249") return "m249";
        if (weaponName == "M4A4") return "m4a1";
        if (weaponName == "MAC-10") return "mac10";
        if (weaponName == "P90") return "p90";
        if (weaponName == "MP5-SD") return "mp5sd";
        if (weaponName == "UMP-45") return "ump45";
        if (weaponName == "XM1014") return "xm1014";
        if (weaponName == "PP-Bizon") return "bizon";
        if (weaponName == "MAG-7") return "mag7";
        if (weaponName == "Negev") return "negev";
        if (weaponName == "Sawed-Off") return "sawedoff";
        if (weaponName == "Tec-9") return "tec9";
        if (weaponName == "Zeus x27") return "taser";
        if (weaponName == "P2000") return "p2000";
        if (weaponName == "MP7") return "mp7";
        if (weaponName == "MP9") return "mp9";
        if (weaponName == "Nova") return "nova";
        if (weaponName == "P250") return "p250";
        if (weaponName == "SCAR-20") return "scar20";
        if (weaponName == "SG 553") return "sg556";
        if (weaponName == "SSG 08") return "ssg08";
        if (weaponName == "Knife") return "knife";
        if (weaponName == "Knife (T)") return "knife_t";
        if (weaponName == "Flashbang") return "flashbang";
        if (weaponName == "HE Grenade") return "hegrenade";
        if (weaponName == "Smoke") return "smokegrenade";
        if (weaponName == "Molotov") return "molotov";
        if (weaponName == "Decoy") return "decoy";
        if (weaponName == "Incendiary") return "incgrenade";
        if (weaponName == "C4") return "c4";
        if (weaponName == "Healthshot") return "healthshot";
        if (weaponName == "M4A1-S") return "m4a1_silencer";
        if (weaponName == "USP-S") return "usp_silencer";
        if (weaponName == "CZ75 Auto") return "cz75a";
        if (weaponName == "R8 Revolver") return "revolver";
        if (weaponName == "Bayonet") return "bayonet";
        if (weaponName == "Classic Knife") return "knife_css";
        if (weaponName == "Flip Knife") return "knife_flip";
        if (weaponName == "Gut Knife") return "knife_gut";
        if (weaponName == "Karambit") return "knife_karambit";
        if (weaponName == "M9 Bayonet") return "knife_m9_bayonet";
        if (weaponName == "Huntsman Knife") return "knife_tactical";
        if (weaponName == "Falchion Knife") return "knife_falchion";
        if (weaponName == "Bowie Knife") return "knife_survival_bowie";
        if (weaponName == "Butterfly Knife") return "knife_butterfly";
        if (weaponName == "Shadow Daggers") return "knife_push";
        if (weaponName == "Paracord Knife") return "knife_cord";
        if (weaponName == "Survival Knife") return "knife_outdoor";
        if (weaponName == "Ursus Knife") return "knife_ursus";
        if (weaponName == "Navaja Knife") return "knife_gypsy_jackknife";
        if (weaponName == "Nomad Knife") return "knife_canis";
        if (weaponName == "Stiletto Knife") return "knife_stiletto";
        if (weaponName == "Talon Knife") return "knife_widowmaker";
        if (weaponName == "Skeleton Knife") return "knife_skeleton";
        if (weaponName == "Kukri Knife") return "knife_kukri";

        return NormalizeIconToken(weaponName);
    }

    std::string GsiNameFromName(const std::string& rawName)
    {
        if (rawName.empty())
            return "weapon_knife";

        std::string normalized = ToLowerAscii(rawName);
        if (normalized == "c4")
            return "weapon_c4";

        std::string token{};
        token.reserve(normalized.size() + 8);
        for (const char ch : normalized)
        {
            const unsigned char value = static_cast<unsigned char>(ch);
            if (std::isalnum(value))
                token.push_back(ch);
            else if (ch == ' ' || ch == '-' || ch == '/')
                token.push_back('_');
        }

        if (token.empty())
            token = "knife";
        if (token.rfind("weapon_", 0) != 0)
            token = "weapon_" + token;
        return token;
    }
}
