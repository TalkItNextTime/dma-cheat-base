#include <Pch.hpp>
#include <SDK.hpp>
#include "ESP.hpp"
#include "KeyIconsEmbedded.hpp"
#include <Aimbot/Aimbot.hpp>
#include <Overlay/Localization.hpp>
#include <Overlay/Overlay.hpp>
#include <array>
#include <cfloat>
#include <cstdio>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <wincrypt.h>
#include <wincodec.h>

#pragma comment(lib, "Crypt32.lib")

namespace
{
    std::string LocalizeRuntimeText(const std::string& text)
    {
        if (text.empty() || !Localization::IsChinese())
            return text;

        return Localization::TranslateImpl(text);
    }

    struct BoneDataRaw
    {
        Vector3 Position{};
        std::uint8_t Padding[0x14]{};
    };

    constexpr int kMaxControllers = 64;
    constexpr int kAliveLifeStateA = 0;
    constexpr int kAliveLifeStateB = 256;
    constexpr int kHeadBone = 6;
    constexpr float kTriggerHeadScaleFixed = 7.0f;
    constexpr float kTriggerTorsoScaleFixed = 8.0f;
    constexpr float kTriggerArmsScaleFixed = 6.0f;
    constexpr float kTriggerLegsScaleFixed = 5.0f;

    constexpr std::array<int, 17> kTrackedBones = {
        0, 2, 4, 5, 6,
        8, 9, 10,
        13, 14, 15,
        22, 23, 24,
        25, 26, 27
    };

    constexpr std::array<std::pair<int, int>, 16> kBoneLinks = {
        std::pair{ 0, 2 },
        std::pair{ 2, 4 },
        std::pair{ 4, 5 },
        std::pair{ 5, 6 },

        std::pair{ 4, 8 },
        std::pair{ 8, 9 },
        std::pair{ 9, 10 },

        std::pair{ 4, 13 },
        std::pair{ 13, 14 },
        std::pair{ 14, 15 },

        std::pair{ 0, 22 },
        std::pair{ 22, 23 },
        std::pair{ 23, 24 },

        std::pair{ 0, 25 },
        std::pair{ 25, 26 },
        std::pair{ 26, 27 }
    };

    struct DebugBoneLink
    {
        int FromBone = 0;
        int ToBone = 0;
        std::uint64_t BoneMask = 0;
    };

    constexpr std::array<DebugBoneLink, 16> kDebugBoneLinks = {
        DebugBoneLink{ 0, 2, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(2) },
        DebugBoneLink{ 2, 4, Structs::BoneMaskFromBoneId(2) | Structs::BoneMaskFromBoneId(4) },
        DebugBoneLink{ 4, 5, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(5) },
        DebugBoneLink{ 5, 6, Structs::BoneMaskFromBoneId(5) | Structs::BoneMaskFromBoneId(6) },

        DebugBoneLink{ 4, 8, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(8) },
        DebugBoneLink{ 8, 9, Structs::BoneMaskFromBoneId(8) | Structs::BoneMaskFromBoneId(9) },
        DebugBoneLink{ 9, 10, Structs::BoneMaskFromBoneId(9) | Structs::BoneMaskFromBoneId(10) },

        DebugBoneLink{ 4, 13, Structs::BoneMaskFromBoneId(4) | Structs::BoneMaskFromBoneId(13) },
        DebugBoneLink{ 13, 14, Structs::BoneMaskFromBoneId(13) | Structs::BoneMaskFromBoneId(14) },
        DebugBoneLink{ 14, 15, Structs::BoneMaskFromBoneId(14) | Structs::BoneMaskFromBoneId(15) },

        DebugBoneLink{ 0, 22, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(22) },
        DebugBoneLink{ 22, 23, Structs::BoneMaskFromBoneId(22) | Structs::BoneMaskFromBoneId(23) },
        DebugBoneLink{ 23, 24, Structs::BoneMaskFromBoneId(23) | Structs::BoneMaskFromBoneId(24) },

        DebugBoneLink{ 0, 25, Structs::BoneMaskFromBoneId(0) | Structs::BoneMaskFromBoneId(25) },
        DebugBoneLink{ 25, 26, Structs::BoneMaskFromBoneId(25) | Structs::BoneMaskFromBoneId(26) },
        DebugBoneLink{ 26, 27, Structs::BoneMaskFromBoneId(26) | Structs::BoneMaskFromBoneId(27) }
    };

    constexpr std::uint64_t kAllBonesMask = Structs::AimAllBoneMask;

    std::string WeaponIdToName(const int weaponId)
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
        default: return {};
        }
    }

    std::string ToLowerAscii(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    std::string NormalizeMapName(std::string value)
    {
        if (value.empty())
            return {};

        std::string lowered = ToLowerAscii(value);
        if (lowered.rfind("maps/", 0) == 0 || lowered.rfind("maps\\", 0) == 0)
            value = value.substr(5);

        lowered = ToLowerAscii(value);
        constexpr const char* kBspSuffix = ".bsp";
        if (lowered.size() > 4 && lowered.compare(lowered.size() - 4, 4, kBspSuffix) == 0)
            value = value.substr(0, value.size() - 4);

        return value;
    }

    bool ReadVectorField(const json& obj, const char* field, Vector3& out)
    {
        if (!obj.contains(field) || !obj[field].is_object())
            return false;

        const json& node = obj[field];
        if (!node.contains("x") || !node["x"].is_number() ||
            !node.contains("y") || !node["y"].is_number() ||
            !node.contains("z") || !node["z"].is_number())
        {
            return false;
        }

        out.x = node["x"].get<float>();
        out.y = node["y"].get<float>();
        out.z = node["z"].get<float>();
        return true;
    }

    float DistanceSquared2D(const Vector2& a, const Vector2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool IsOnScreen(const Vector2& point)
    {
        return point.x >= 0.0f && point.y >= 0.0f && point.x <= Screen.x && point.y <= Screen.y;
    }

    Vector2 ClampToScreenEdge(const Vector2& point, const float padding = 8.0f)
    {
        Vector2 clamped = point;
        const float maxX = (std::max)(padding, Screen.x - padding);
        const float maxY = (std::max)(padding, Screen.y - padding);
        clamped.x = std::clamp(clamped.x, padding, maxX);
        clamped.y = std::clamp(clamped.y, padding, maxY);
        return clamped;
    }

    const char* GrenadeTypeLabelByIndex(const int index)
    {
        switch (index)
        {
        case 0: return "Smoke";
        case 1: return "Flash";
        case 2: return "HE";
        case 3: return "Decoy";
        case 4: return "Molotov";
        default: return "Unknown";
        }
    }

    int GrenadeTypeIndexByLabel(const std::string& grenadeType)
    {
        const std::string lowered = ToLowerAscii(grenadeType);
        if (lowered == "smoke")
            return 0;
        if (lowered == "flash" || lowered == "flashbang")
            return 1;
        if (lowered == "he" || lowered == "he grenade")
            return 2;
        if (lowered == "decoy")
            return 3;
        if (lowered == "molotov" || lowered == "incendiary")
            return 4;
        return -1;
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

    std::string ToUpperAscii(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::toupper(c));
        });
        return text;
    }

    struct ThrowKeySet
    {
        std::string Mouse = "LB";
        bool W = false;
        bool A = false;
        bool S = false;
        bool D = false;
        bool Ctrl = false;
        bool Space = false;
    };

    bool ContainsAnyKeyword(const std::string& text, const std::initializer_list<const char*>& keys)
    {
        if (text.empty())
            return false;

        const std::string lowered = ToLowerAscii(text);
        for (const char* raw : keys)
        {
            if (!raw || !*raw)
                continue;

            const std::string key(raw);
            if (text.find(key) != std::string::npos || lowered.find(ToLowerAscii(key)) != std::string::npos)
                return true;
        }

        return false;
    }

    std::string NormalizeThrowMouseToken(const std::string& tokenUpper)
    {
        if (tokenUpper == "LRB" || tokenUpper == "DUAL" || tokenUpper == "BOTH")
            return "LRB";
        if (tokenUpper == "RB" || tokenUpper == "RMB" || tokenUpper == "RIGHT" || tokenUpper == "RIGHTCLICK" || tokenUpper == "MOUSE2")
            return "RB";
        if (tokenUpper == "LB" || tokenUpper == "LMB" || tokenUpper == "LEFT" || tokenUpper == "LEFTCLICK" || tokenUpper == "MOUSE1")
            return "LB";
        return {};
    }

    void ParseThrowTokenIntoKeys(const std::string& token, ThrowKeySet& inOutKeys)
    {
        const std::string trimmed = TrimAscii(token);
        if (trimmed.empty())
            return;

        const std::string tokenUpper = ToUpperAscii(trimmed);
        if (const std::string mouse = NormalizeThrowMouseToken(tokenUpper); !mouse.empty())
        {
            inOutKeys.Mouse = mouse;
            return;
        }

        if (tokenUpper == "W")
        {
            inOutKeys.W = true;
            return;
        }
        if (tokenUpper == "A")
        {
            inOutKeys.A = true;
            return;
        }
        if (tokenUpper == "S")
        {
            inOutKeys.S = true;
            return;
        }
        if (tokenUpper == "D")
        {
            inOutKeys.D = true;
            return;
        }
        if (tokenUpper == "CTRL" || tokenUpper == "CONTROL" || tokenUpper == "DUCK" || tokenUpper == "CROUCH")
        {
            inOutKeys.Ctrl = true;
            return;
        }
        if (tokenUpper == "SPACE" || tokenUpper == "JUMP")
        {
            inOutKeys.Space = true;
            return;
        }
    }

    ThrowKeySet ParseThrowTypeToKeys(const std::string& throwTypeRaw)
    {
        ThrowKeySet keys{};
        const std::string throwType = TrimAscii(throwTypeRaw);
        if (throwType.empty())
            return keys;

        const std::string lowered = ToLowerAscii(throwType);
        if (lowered == "standthrow")
            return keys;
        if (lowered == "jumpthrow")
        {
            keys.Space = true;
            return keys;
        }
        if (lowered == "runthrow")
        {
            keys.W = true;
            return keys;
        }
        if (lowered == "runjumpthrow" || lowered == "runjump")
        {
            keys.W = true;
            keys.Space = true;
            return keys;
        }

        size_t begin = 0;
        while (begin <= throwType.size())
        {
            const size_t plusPos = throwType.find('+', begin);
            const size_t endPos = (plusPos == std::string::npos) ? throwType.size() : plusPos;
            ParseThrowTokenIntoKeys(throwType.substr(begin, endPos - begin), keys);
            if (plusPos == std::string::npos)
                break;
            begin = plusPos + 1;
        }

        return keys;
    }

    std::string BuildCanonicalThrowType(const ThrowKeySet& keys)
    {
        std::string result = keys.Mouse.empty() ? std::string("LB") : keys.Mouse;

        const auto appendKey = [&](const char* key)
        {
            result += "+";
            result += key;
        };

        if (keys.W) appendKey("W");
        if (keys.A) appendKey("A");
        if (keys.S) appendKey("S");
        if (keys.D) appendKey("D");
        if (keys.Ctrl) appendKey("Ctrl");
        if (keys.Space) appendKey("Space");

        return result;
    }

    std::vector<std::string> BuildThrowHintTokens(const ThrowKeySet& keys)
    {
        std::vector<std::string> keyboard{};
        if (keys.W) keyboard.push_back("W");
        if (keys.A) keyboard.push_back("A");
        if (keys.S) keyboard.push_back("S");
        if (keys.D) keyboard.push_back("D");
        if (keys.Ctrl) keyboard.push_back("Ctrl");
        if (keys.Space) keyboard.push_back("Space");

        std::vector<std::string> out{};
        out.reserve(keyboard.size() * 2 + 1);
        out.push_back(keys.Mouse.empty() ? std::string("LB") : keys.Mouse);

        if (!keyboard.empty())
        {
            out.push_back("Plus");
            for (size_t i = 0; i < keyboard.size(); ++i)
            {
                if (i > 0)
                    out.push_back("Plus");
                out.push_back(keyboard[i]);
            }
        }

        return out;
    }

    bool EndsWith(const std::string& text, const std::string& suffix)
    {
        if (suffix.empty())
            return true;
        if (text.size() < suffix.size())
            return false;
        return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    void MergeRemarkSegment(std::string& inOutRemark, const std::string& segment)
    {
        const std::string trimmedSegment = TrimAscii(segment);
        if (trimmedSegment.empty())
            return;

        inOutRemark = TrimAscii(inOutRemark);
        if (inOutRemark.empty())
        {
            inOutRemark = trimmedSegment;
            return;
        }

        if (inOutRemark.find(trimmedSegment) != std::string::npos || trimmedSegment.find(inOutRemark) != std::string::npos)
            return;

        inOutRemark += " | ";
        inOutRemark += trimmedSegment;
    }

    bool ExtractTrailingBracketRemark(std::string& inOutName, std::string& outRemark)
    {
        outRemark.clear();
        std::string trimmedName = TrimAscii(inOutName);
        if (trimmedName.empty())
            return false;

        struct BracketPair
        {
            const char* Open = nullptr;
            const char* Close = nullptr;
        };

        static const std::array<BracketPair, 4> kPairs = {
            BracketPair{ "(", ")" },
            BracketPair{ "[", "]" },
            BracketPair{ "（", "）" },
            BracketPair{ "【", "】" }
        };

        for (const BracketPair& pair : kPairs)
        {
            if (!pair.Open || !pair.Close)
                continue;

            const std::string openToken = pair.Open;
            const std::string closeToken = pair.Close;
            if (openToken.empty() || closeToken.empty() || !EndsWith(trimmedName, closeToken))
                continue;

            const size_t closePos = trimmedName.size() - closeToken.size();
            const size_t openPos = trimmedName.rfind(openToken, closePos);
            if (openPos == std::string::npos || openPos + openToken.size() > closePos)
                continue;

            const std::string inside = TrimAscii(trimmedName.substr(
                openPos + openToken.size(),
                closePos - (openPos + openToken.size())
            ));
            const std::string baseName = TrimAscii(trimmedName.substr(0, openPos));
            if (baseName.empty())
                continue;

            inOutName = baseName;
            outRemark = inside;
            return true;
        }

        inOutName = trimmedName;
        return false;
    }

    void ApplyNameThrowHints(const std::string& spotName, ThrowKeySet& inOutKeys)
    {
        if (spotName.empty())
            return;

        if (ContainsAnyKeyword(spotName, { "双键", "双按", "双", "左右键", "dual", "both", "lrb", "鍙岄敭" }))
            inOutKeys.Mouse = "LRB";
        else if (ContainsAnyKeyword(spotName, { "右键", "right click", "rmb", "rb", "鍙抽敭" }))
            inOutKeys.Mouse = "RB";

        if (ContainsAnyKeyword(spotName, { "蹲下", "下蹲", "蹲", "crouch", "duck", "ctrl", "韫" }))
            inOutKeys.Ctrl = true;

        const bool runJumpThrow = ContainsAnyKeyword(spotName, {
            "跑跳投", "跑跳", "助跑跳", "runjumpthrow", "runjump", "run jump throw", "run jump", "run-jump"
        });
        const bool runThrow = ContainsAnyKeyword(spotName, {
            "跑投", "助跑投", "runthrow", "run throw", "run-throw"
        });
        const bool jumpThrow = ContainsAnyKeyword(spotName, {
            "跳投", "jumpthrow", "jump throw", "jump-throw"
        });

        if (runJumpThrow)
        {
            inOutKeys.W = true;
            inOutKeys.Space = true;
            return;
        }

        if (runThrow)
            inOutKeys.W = true;
        if (jumpThrow)
            inOutKeys.Space = true;
    }

    std::string ExtractArrivalRemarkFromName(const std::string& spotName)
    {
        if (spotName.empty())
            return {};

        const std::string lowered = ToLowerAscii(spotName);
        struct Keyword
        {
            const char* Text = nullptr;
            bool UseLowered = false;
        };

        static const std::array<Keyword, 11> keywords = {
            Keyword{ "跑到", false },
            Keyword{ "走到", false },
            Keyword{ "跑至", false },
            Keyword{ "走至", false },
            Keyword{ "到达", false },
            Keyword{ "run to", true },
            Keyword{ "walk to", true },
            Keyword{ "go to", true },
            Keyword{ "到", false },
            Keyword{ "至", false },
            Keyword{ "to ", true }
        };

        size_t bestPos = std::string::npos;
        for (const Keyword& keyword : keywords)
        {
            if (!keyword.Text || !*keyword.Text)
                continue;

            const std::string needle = keyword.Text;
            const size_t pos = keyword.UseLowered ? lowered.find(needle) : spotName.find(needle);
            if (pos == std::string::npos)
                continue;
            if (bestPos == std::string::npos || pos < bestPos)
                bestPos = pos;
        }

        if (bestPos == std::string::npos)
            return {};

        return TrimAscii(spotName.substr(bestPos));
    }

    std::string NormalizeThrowType(const std::string& throwType, const std::string& spotName, std::string* inOutRemark)
    {
        std::string normalizedName = TrimAscii(spotName);
        std::string extractedBracketRemark{};
        ExtractTrailingBracketRemark(normalizedName, extractedBracketRemark);

        ThrowKeySet keys = ParseThrowTypeToKeys(throwType);
        ApplyNameThrowHints(normalizedName, keys);
        ApplyNameThrowHints(extractedBracketRemark, keys);

        if (inOutRemark)
        {
            *inOutRemark = TrimAscii(*inOutRemark);
            ApplyNameThrowHints(*inOutRemark, keys);
            MergeRemarkSegment(*inOutRemark, extractedBracketRemark);
            MergeRemarkSegment(*inOutRemark, ExtractArrivalRemarkFromName(normalizedName));
        }

        return BuildCanonicalThrowType(keys);
    }

    bool IsUtilityGrenadeType(const std::string& grenadeType)
    {
        return GrenadeTypeIndexByLabel(grenadeType) >= 0;
    }

    std::string LocalizeThrowTypeLabel(std::string throwType)
    {
        return NormalizeThrowType(throwType, {}, nullptr);
    }

    std::string NormalizeThrowTokenName(const std::string& token)
    {
        const std::string upper = ToUpperAscii(TrimAscii(token));
        if (upper == "LB" || upper == "LMB" || upper == "LEFT" || upper == "LEFTCLICK")
            return "LB";
        if (upper == "RB" || upper == "RMB" || upper == "RIGHT" || upper == "RIGHTCLICK")
            return "RB";
        if (upper == "LRB" || upper == "DUAL" || upper == "BOTH")
            return "LRB";
        if (upper == "PLUS" || upper == "+")
            return "Plus";
        if (upper == "W")
            return "W";
        if (upper == "A")
            return "A";
        if (upper == "S")
            return "S";
        if (upper == "D")
            return "D";
        if (upper == "CTRL" || upper == "CONTROL")
            return "Ctrl";
        if (upper == "SPACE")
            return "Space";
        if (upper == "SPACE_EN" || upper == "SPACEEN" || upper == "SPACE-EN")
            return "Space_en";
        return {};
    }

    std::string ThrowTokenFallbackText(const std::string& rawToken)
    {
        const std::string normalized = NormalizeThrowTokenName(rawToken);
        if (normalized == "Plus")
            return "+";
        if (normalized == "LB")
            return "LB";
        if (normalized == "RB")
            return "RB";
        if (normalized == "LRB")
            return "LRB";
        if (normalized == "W")
            return "W";
        if (normalized == "A")
            return "A";
        if (normalized == "S")
            return "S";
        if (normalized == "D")
            return "D";
        if (normalized == "Ctrl")
            return "Ctrl";
        if (normalized == "Space")
            return "Space";
        if (normalized == "Space_en")
            return "Space";
        return rawToken;
    }

    std::string RepairMalformedGrenadeJsonText(const std::string& sourceText)
    {
        if (sourceText.empty())
            return sourceText;

        std::string repaired{};
        repaired.reserve(sourceText.size() + 128);

        size_t begin = 0;
        while (begin <= sourceText.size())
        {
            const size_t lineEnd = sourceText.find('\n', begin);
            const size_t sliceEnd = (lineEnd == std::string::npos) ? sourceText.size() : lineEnd;
            std::string line = sourceText.substr(begin, sliceEnd - begin);

            if (line.find("\"name\"") != std::string::npos)
            {
                int quoteCount = 0;
                bool escaped = false;
                for (const char ch : line)
                {
                    if (ch == '\\' && !escaped)
                    {
                        escaped = true;
                        continue;
                    }

                    if (ch == '"' && !escaped)
                        ++quoteCount;

                    escaped = false;
                }

                if ((quoteCount % 2) != 0)
                {
                    const size_t commaPos = line.find_last_of(',');
                    if (commaPos != std::string::npos)
                        line.insert(commaPos, "\"");
                    else
                        line.push_back('"');
                }
            }

            repaired += line;
            if (lineEnd == std::string::npos)
                break;

            repaired.push_back('\n');
            begin = lineEnd + 1;
        }

        return repaired;
    }

    template <typename T>
    void ReleaseCom(T*& ptr)
    {
        if (ptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }

    struct KeyIconTexture
    {
        ID3D11ShaderResourceView* Srv = nullptr;
        int Width = 0;
        int Height = 0;
    };

    struct KeyIconCache
    {
        std::mutex Mutex{};
        bool Base64Loaded = false;
        std::unordered_map<std::string, std::string> Base64ByToken{};
        std::unordered_map<std::string, KeyIconTexture> Textures{};

        ~KeyIconCache()
        {
            for (auto& pair : Textures)
                ReleaseCom(pair.second.Srv);
        }
    };

    KeyIconCache& GetKeyIconCache()
    {
        static KeyIconCache cache{};
        return cache;
    }

    bool EnsureKeyIconBase64Loaded(KeyIconCache& cache)
    {
        if (cache.Base64Loaded)
            return true;

        std::unordered_map<std::string, std::string> parsed = EmbeddedKeyIcons::BuildKeyIconsBase64Map();
        if (parsed.empty())
            return false;

        std::unordered_map<std::string, std::string> normalized{};
        normalized.reserve(parsed.size());
        for (auto& [rawToken, base64Text] : parsed)
        {
            const std::string token = NormalizeThrowTokenName(rawToken);
            if (token.empty() || base64Text.empty())
                continue;

            normalized[token] = std::move(base64Text);
        }

        if (normalized.empty())
            return false;

        cache.Base64ByToken = std::move(normalized);
        cache.Base64Loaded = true;
        return true;
    }

    bool DecodeBase64ViaWinApi(const std::string& base64Text, std::vector<std::uint8_t>& outBytes)
    {
        if (base64Text.empty())
            return false;
        if (base64Text.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
            return false;

        DWORD outSize = 0;
        if (!CryptStringToBinaryA(base64Text.c_str(), static_cast<DWORD>(base64Text.size()), CRYPT_STRING_BASE64_ANY, nullptr, &outSize, nullptr, nullptr))
            return false;

        outBytes.assign(outSize, 0u);
        if (!CryptStringToBinaryA(base64Text.c_str(), static_cast<DWORD>(base64Text.size()), CRYPT_STRING_BASE64_ANY, outBytes.data(), &outSize, nullptr, nullptr))
        {
            outBytes.clear();
            return false;
        }

        outBytes.resize(outSize);
        return true;
    }

    bool DecodePngViaWic(const std::vector<std::uint8_t>& pngBytes, std::vector<std::uint8_t>& outPixels, UINT& outWidth, UINT& outHeight)
    {
        outPixels.clear();
        outWidth = 0;
        outHeight = 0;
        if (pngBytes.empty())
            return false;

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool needUninit = SUCCEEDED(initHr);

        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;

        bool success = false;
        do
        {
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
                break;
            if (FAILED(factory->CreateStream(&stream)))
                break;
            if (pngBytes.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
                break;
            if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(pngBytes.data()), static_cast<DWORD>(pngBytes.size()))))
                break;
            if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
                break;
            if (FAILED(decoder->GetFrame(0, &frame)))
                break;
            if (FAILED(factory->CreateFormatConverter(&converter)))
                break;
            if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom)))
                break;
            if (FAILED(converter->GetSize(&outWidth, &outHeight)))
                break;
            if (outWidth == 0 || outHeight == 0)
                break;

            const UINT stride = outWidth * 4u;
            const UINT totalSize = stride * outHeight;
            outPixels.assign(totalSize, 0u);
            if (FAILED(converter->CopyPixels(nullptr, stride, totalSize, outPixels.data())))
            {
                outPixels.clear();
                break;
            }

            success = true;
        }
        while (false);

        ReleaseCom(converter);
        ReleaseCom(frame);
        ReleaseCom(decoder);
        ReleaseCom(stream);
        ReleaseCom(factory);
        if (needUninit)
            CoUninitialize();

        return success;
    }

    bool CreateTextureFromRgba(ID3D11Device* device, const std::vector<std::uint8_t>& rgbaPixels, const UINT width, const UINT height, KeyIconTexture& outTexture)
    {
        if (!device || rgbaPixels.empty() || width == 0 || height == 0)
            return false;

        D3D11_TEXTURE2D_DESC textureDesc{};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = rgbaPixels.data();
        initData.SysMemPitch = static_cast<UINT>(width * 4u);

        ID3D11Texture2D* texture = nullptr;
        if (FAILED(device->CreateTexture2D(&textureDesc, &initData, &texture)))
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        const HRESULT srvHr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
        texture->Release();
        if (FAILED(srvHr) || !srv)
            return false;

        outTexture.Srv = srv;
        outTexture.Width = static_cast<int>(width);
        outTexture.Height = static_cast<int>(height);
        return true;
    }

    const KeyIconTexture* GetKeyIconTexture(const std::string& rawToken, ID3D11Device* device)
    {
        const std::string token = NormalizeThrowTokenName(rawToken);
        if (token.empty() || !device)
            return nullptr;

        const std::string preferredToken = (token == "Space" && !Localization::IsChinese())
            ? std::string("Space_en")
            : token;
        const bool hasFallbackToken = preferredToken != token;

        KeyIconCache& cache = GetKeyIconCache();
        std::lock_guard lock(cache.Mutex);

        auto findLoadedTexture = [&](const std::string& key) -> const KeyIconTexture*
        {
            auto loaded = cache.Textures.find(key);
            if (loaded == cache.Textures.end())
                return nullptr;
            return loaded->second.Srv ? &loaded->second : nullptr;
        };

        if (const KeyIconTexture* loaded = findLoadedTexture(preferredToken))
            return loaded;
        if (hasFallbackToken)
        {
            if (const KeyIconTexture* loaded = findLoadedTexture(token))
                return loaded;
        }

        if (!EnsureKeyIconBase64Loaded(cache))
            return nullptr;

        auto loadEncodedTexture = [&](const std::string& key) -> const KeyIconTexture*
        {
            const auto encodedIt = cache.Base64ByToken.find(key);
            if (encodedIt == cache.Base64ByToken.end())
                return nullptr;

            std::string base64Text = encodedIt->second;

            std::vector<std::uint8_t> pngBytes{};
            if (!DecodeBase64ViaWinApi(base64Text, pngBytes))
                return nullptr;
            base64Text.clear();
            base64Text.shrink_to_fit();

            std::vector<std::uint8_t> rgbaPixels{};
            UINT width = 0;
            UINT height = 0;
            if (!DecodePngViaWic(pngBytes, rgbaPixels, width, height))
                return nullptr;

            KeyIconTexture texture{};
            if (!CreateTextureFromRgba(device, rgbaPixels, width, height, texture))
                return nullptr;

            pngBytes.clear();
            pngBytes.shrink_to_fit();
            rgbaPixels.clear();
            rgbaPixels.shrink_to_fit();

            cache.Base64ByToken.erase(encodedIt);
            if (cache.Base64ByToken.empty())
            {
                std::unordered_map<std::string, std::string> empty{};
                cache.Base64ByToken.swap(empty);
            }

            cache.Textures[key] = texture;
            return cache.Textures[key].Srv ? &cache.Textures[key] : nullptr;
        };

        if (const KeyIconTexture* created = loadEncodedTexture(preferredToken))
            return created;
        if (hasFallbackToken)
        {
            if (const KeyIconTexture* created = loadEncodedTexture(token))
                return created;
        }

        return nullptr;
    }
    float DistanceSquared3D(const Vector3& a, const Vector3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    bool IsLikelyUserAddress(const uint64_t address)
    {
        return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
    }

    bool IsNonZeroPosition(const Vector3& position)
    {
        return std::abs(position.x) > 0.01f ||
            std::abs(position.y) > 0.01f ||
            std::abs(position.z) > 0.01f;
    }

    float Dot3(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vector3 Cross3(const Vector3& a, const Vector3& b)
    {
        return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float Length3(const Vector3& v)
    {
        return std::sqrt(Dot3(v, v));
    }

    Vector3 Normalize3(const Vector3& v)
    {
        const float length = Length3(v);
        if (length <= 0.0001f)
            return Vector3{};

        return v / length;
    }

    float Distance2D(const Vector2& a, const Vector2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float DistancePointToSegment2D(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd)
    {
        const float vx = segmentEnd.x - segmentStart.x;
        const float vy = segmentEnd.y - segmentStart.y;
        const float wx = point.x - segmentStart.x;
        const float wy = point.y - segmentStart.y;
        const float segmentLenSq = vx * vx + vy * vy;

        if (segmentLenSq < 0.0001f)
            return Distance2D(point, segmentStart);

        const float t = std::clamp((wx * vx + wy * vy) / segmentLenSq, 0.0f, 1.0f);
        const Vector2 closest{
            segmentStart.x + t * vx,
            segmentStart.y + t * vy
        };

        return Distance2D(point, closest);
    }

    float BoneLinkRadiusScale(const int fromBone, const int toBone)
    {
        const auto match = [&](const int a, const int b)
        {
            return (fromBone == a && toBone == b) || (fromBone == b && toBone == a);
        };

        if (match(0, 2)) return 1.45f;
        if (match(2, 4)) return 1.40f;
        if (match(4, 5)) return 1.25f;
        if (match(5, 6)) return 1.10f;

        if (match(4, 8) || match(4, 13)) return 1.12f;
        if (match(8, 9) || match(13, 14)) return 0.92f;
        if (match(9, 10) || match(14, 15)) return 0.78f;

        if (match(0, 22) || match(0, 25)) return 1.28f;
        if (match(22, 23) || match(25, 26)) return 1.12f;
        if (match(23, 24) || match(26, 27)) return 0.90f;

        return 1.0f;
    }

    bool BuildProjectedSegmentBoxCorners(
        const Vector3& fromWorld,
        const Vector3& toWorld,
        const float radiusPx,
        const Matrix& viewMatrix,
        std::array<Vector2, 8>& outScreenCorners)
    {
        const Vector3 segment = toWorld - fromWorld;
        const float segmentLength = Length3(segment);
        if (segmentLength < 0.001f)
            return false;

        const Vector3 center = (fromWorld + toWorld) * 0.5f;
        Vector2 centerScreen{};
        if (!sdk.WorldToScreen(center, centerScreen, viewMatrix))
            return false;

        const Vector3 dir = segment / segmentLength;
        const Vector3 helperAxis = std::fabs(dir.z) < 0.95f
            ? Vector3{ 0.0f, 0.0f, 1.0f }
            : Vector3{ 0.0f, 1.0f, 0.0f };

        Vector3 right = Normalize3(Cross3(dir, helperAxis));
        if (Length3(right) < 0.001f)
        {
            const Vector3 fallbackAxis = std::fabs(dir.x) < 0.95f
                ? Vector3{ 1.0f, 0.0f, 0.0f }
                : Vector3{ 0.0f, 1.0f, 0.0f };
            right = Normalize3(Cross3(dir, fallbackAxis));
        }
        if (Length3(right) < 0.001f)
            return false;

        Vector3 up = Normalize3(Cross3(right, dir));
        if (Length3(up) < 0.001f)
            return false;

        auto estimatePxPerWorld = [&](const Vector3& axis) -> float
        {
            Vector2 axisScreen{};
            if (!sdk.WorldToScreen(center + axis, axisScreen, viewMatrix))
                return 0.0f;
            return Distance2D(centerScreen, axisScreen);
        };

        const float pxPerWorldRight = estimatePxPerWorld(right);
        const float pxPerWorldUp = estimatePxPerWorld(up);
        float pxPerWorld = 0.0f;
        if (pxPerWorldRight > 0.001f && pxPerWorldUp > 0.001f)
            pxPerWorld = 0.5f * (pxPerWorldRight + pxPerWorldUp);
        else
            pxPerWorld = (std::max)(pxPerWorldRight, pxPerWorldUp);

        if (pxPerWorld < 0.001f)
            return false;

        const float halfWidthWorld = std::clamp(radiusPx / pxPerWorld, 0.01f, 40.0f);
        const float halfLengthWorld = (std::max)(segmentLength * 0.5f, halfWidthWorld * 0.50f);

        const Vector3 axisForward = dir * halfLengthWorld;
        const Vector3 axisRight = right * halfWidthWorld;
        const Vector3 axisUp = up * halfWidthWorld;

        const std::array<Vector3, 8> worldCorners = {
            center - axisForward - axisRight - axisUp,
            center - axisForward + axisRight - axisUp,
            center - axisForward + axisRight + axisUp,
            center - axisForward - axisRight + axisUp,
            center + axisForward - axisRight - axisUp,
            center + axisForward + axisRight - axisUp,
            center + axisForward + axisRight + axisUp,
            center + axisForward - axisRight + axisUp
        };

        for (size_t i = 0; i < worldCorners.size(); ++i)
        {
            if (!sdk.WorldToScreen(worldCorners[i], outScreenCorners[i], viewMatrix))
                return false;
        }

        return true;
    }

    int NormalizeBombSiteRaw(const int rawSite)
    {
        if (rawSite == 0)
            return 0; // A
        if (rawSite == 1 || rawSite == 2)
            return 1; // B (some dumps are 1-based)
        if (rawSite == 'A' || rawSite == 'a')
            return 0;
        if (rawSite == 'B' || rawSite == 'b')
            return 1;
        return -1;
    }

    float ReadGlobalCurrentTime()
    {
        if (!Globals::ClientBase || !Offsets::Client::dwGlobalVars)
            return 0.0f;

        const uint64_t globalVars = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGlobalVars);
        if (!IsLikelyUserAddress(globalVars))
            return 0.0f;

        float currentTime = mem.Read<float>(globalVars + 0x2C);
        if (currentTime <= 0.0f)
            currentTime = mem.Read<float>(globalVars + 0x30);
        return currentTime;
    }

    struct SampledEntityData
    {
        int HandleIndex = 0;
        uint64_t Controller = 0;

        uint32_t PawnHandle = 0;
        uint64_t Pawn = 0;

        int Health = 0;
        int Team = 0;
        int LifeState = 0;
        int MaxHealth = 100;

        uint64_t SceneNode = 0;
        Vector3 OldOrigin{};
        Vector3 AbsOrigin{};
        bool HasAbsOrigin = false;
        Vector3 ViewOffset{ 0.0f, 0.0f, 64.0f };
        uint64_t BoneArray = 0;

        int Armor = 0;
        bool IsScoped = false;
        bool HasDefuser = false;
        float FlashDuration = 0.0f;
        bool RefreshStatus = false;
    };
}

void ESP::RenderWatermark(ImDrawList* drawList) const
{
    if (!config.Visuals.Watermark)
        return;

    const std::string text = "Cosmic ESP";
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

    const ImVec2 basePos = ImVec2(12.0f, 12.0f);
    const ImVec2 bgMin = basePos - ImVec2(6.0f, 4.0f);
    const ImVec2 bgMax = basePos + textSize + ImVec2(6.0f, 4.0f);

    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(18, 18, 18, 165), 4.0f);
    drawList->AddRect(bgMin, bgMax, IM_COL32(255, 255, 255, 55), 4.0f);
    drawList->AddText(basePos, ToImColor(config.Visuals.WatermarkColor), text.c_str());
}

void ESP::RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player) const
{
    const ImU32 boxColor = GetBoxColor(player.IsVisible);

    if (config.Visuals.Box)
    {
        drawList->AddRect(
            player.BoxMin,
            player.BoxMax,
            boxColor,
            0.0f,
            0,
            1.3f
        );
    }

    if (config.Visuals.Health)
    {
        const float boxHeight = player.BoxMax.y - player.BoxMin.y;
        const float healthRatio = std::clamp(static_cast<float>(player.Health) / static_cast<float>((std::max)(player.MaxHealth, 1)), 0.0f, 1.0f);

        const ImVec2 healthBgMin(player.BoxMin.x - 6.0f, player.BoxMin.y);
        const ImVec2 healthBgMax(player.BoxMin.x - 3.0f, player.BoxMax.y);
        const ImVec2 healthFillMin(player.BoxMin.x - 6.0f, player.BoxMax.y - boxHeight * healthRatio);
        const ImVec2 healthFillMax(player.BoxMin.x - 3.0f, player.BoxMax.y);

        const ImU32 healthColor = IM_COL32(
            static_cast<int>((1.0f - healthRatio) * 255.0f),
            static_cast<int>(healthRatio * 255.0f),
            80,
            255
        );

        drawList->AddRectFilled(healthBgMin, healthBgMax, IM_COL32(20, 20, 20, 185));
        drawList->AddRectFilled(healthFillMin, healthFillMax, healthColor);
        drawList->AddRect(healthBgMin, healthBgMax, IM_COL32(0, 0, 0, 220));
    }

    if (config.Visuals.Name)
    {
        const std::string nameText = player.Name.empty() ? Localization::Pick("Unknown", "鏈煡") : player.Name;
        const ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());

        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMin.y - textSize.y - 3.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.NameColor), nameText.c_str());
    }

    if (config.Visuals.Weapon && !player.WeaponName.empty())
    {
        const std::string weaponText = LocalizeRuntimeText(player.WeaponName);
        const ImVec2 textSize = ImGui::CalcTextSize(weaponText.c_str());
        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMax.y + 2.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.WeaponColor), weaponText.c_str());
    }

    if (config.Visuals.Bones)
    {
        ImU32 bonesColor = ToImColor(config.Visuals.BonesColor);
        if (config.Visuals.VisibleCheck && player.IsVisible)
            bonesColor = ToImColor(config.Visuals.BonesColorVisible);
        RenderSkeleton(drawList, player, bonesColor);
    }

    if (config.Aim.TriggerHitboxDebug)
    {
        RenderTriggerHitboxDebug(drawList, player);
    }

    struct StatusLine
    {
        std::string Text{};
        ImU32 Color = IM_COL32(220, 220, 220, 255);
    };

    std::vector<StatusLine> statusLines{};
    statusLines.reserve(5);

    if (player.IsScoped)
        statusLines.push_back({ Localization::Pick("Scoped", "开镜"), IM_COL32(220, 220, 220, 255) });

    if (player.FlashDuration > 0.01f)
        statusLines.push_back({ Localization::Pick("Flashed", "致盲"), IM_COL32(255, 214, 120, 255) });

    if (config.Visuals.Armor)
        statusLines.push_back({ std::string(Localization::Pick("AR:", "甲:")) + std::to_string(player.Armor), ToImColor(config.Visuals.ArmorColor) });

    if (config.Visuals.Money && player.ShowMoney)
        statusLines.push_back({ "$" + std::to_string(player.Money), ToImColor(config.Visuals.MoneyColor) });

    if (config.Visuals.Defuser && player.HasDefuser)
        statusLines.push_back({ Localization::Pick("Kit", "拆弹钳"), ToImColor(config.Visuals.DefuserColor) });

    float lineOffset = 0.0f;
    for (const StatusLine& line : statusLines)
    {
        const ImVec2 textPos(player.BoxMax.x + 4.0f, player.BoxMin.y + lineOffset);
        drawList->AddText(textPos, line.Color, line.Text.c_str());
        lineOffset += ImGui::GetFontSize() + 1.0f;
    }
}

void ESP::RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, const ImU32 color) const
{
    if (player.Bones.empty())
        return;

    auto getBone = [&](const int index) -> const BonePoint*
    {
        for (const BonePoint& bone : player.Bones)
        {
            if (bone.Index == index)
                return &bone;
        }

        return nullptr;
    };

    for (const auto& [from, to] : kBoneLinks)
    {
        const BonePoint* fromBone = getBone(from);
        const BonePoint* toBone = getBone(to);
        if (!fromBone || !toBone)
            continue;

        if (!fromBone->OnScreen || !toBone->OnScreen)
            continue;

        drawList->AddLine(fromBone->Screen.ToImVec2(), toBone->Screen.ToImVec2(), color, 1.0f);
    }
}

void ESP::RenderTriggerHitboxDebug(ImDrawList* drawList, const PlayerEspSnapshot& player) const
{
    if (!drawList || player.Bones.empty())
        return;

    const float unifiedRadius = std::clamp(config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
    const float hitboxScale = std::clamp(config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
    const float hitboxAddPx = std::clamp(config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
    const float headBaseRadius = std::clamp(config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);

    const float hitboxRadiusPx = std::clamp(unifiedRadius * hitboxScale + hitboxAddPx, 0.5f, 80.0f);
    const float headRadiusPx = std::clamp(headBaseRadius * hitboxScale + hitboxAddPx, 0.5f, 100.0f);

    std::uint64_t boneMask = aim.GetCurrentTriggerBoneMask() & kAllBonesMask;
    if (boneMask == 0ull)
        boneMask = kAllBonesMask;

    const Vector2 crosshair{ ScreenCenter.x, ScreenCenter.y };
    float bestNormalizedDistance = FLT_MAX;
    int activeFromBone = -1;
    int activeToBone = -1;

    auto getBone = [&](const int index) -> const BonePoint*
    {
        for (const BonePoint& bone : player.Bones)
        {
            if (bone.Index == index)
                return &bone;
        }

        return nullptr;
    };

    float distanceScale = 1.0f;
    const BonePoint* headBoneForSizing = getBone(Structs::AimHeadBoneId);
    const BonePoint* pelvisBoneForSizing = getBone(0);
    if (headBoneForSizing && pelvisBoneForSizing && headBoneForSizing->OnScreen && pelvisBoneForSizing->OnScreen)
    {
        constexpr float kReferenceBodyHeightPx = 150.0f;
        const float bodyHeight = (std::max)(1.0f, std::fabs(pelvisBoneForSizing->Screen.y - headBoneForSizing->Screen.y));
        distanceScale = std::clamp(bodyHeight / kReferenceBodyHeightPx, 0.20f, 3.00f);
    }
    const float adaptiveBodyRadiusPx = std::clamp(hitboxRadiusPx * distanceScale, 0.5f, 100.0f);
    const float adaptiveHeadRadiusPx = std::clamp(headRadiusPx * distanceScale, 0.5f, 100.0f);
    const auto hitboxRegionFromBone = [](const int boneId) -> int
    {
        if (boneId == Structs::AimHeadBoneId)
            return 0; // head
        switch (boneId)
        {
        case 8: case 9: case 10:
        case 13: case 14: case 15:
            return 2; // arms
        case 22: case 23: case 24:
        case 25: case 26: case 27:
            return 3; // legs
        default:
            return 1; // torso
        }
    };
    const auto hitboxRegionScale = [](const int region) -> float
    {
        switch (region)
        {
        case 0: return kTriggerHeadScaleFixed;
        case 2: return kTriggerArmsScaleFixed;
        case 3: return kTriggerLegsScaleFixed;
        default: break;
        }
        return kTriggerTorsoScaleFixed;
    };

    for (const DebugBoneLink& link : kDebugBoneLinks)
    {
        const bool endpointSelected =
            (boneMask & Structs::BoneMaskFromBoneId(link.FromBone)) != 0ull ||
            (boneMask & Structs::BoneMaskFromBoneId(link.ToBone)) != 0ull;
        if (!endpointSelected)
            continue;

        const BonePoint* fromBone = getBone(link.FromBone);
        const BonePoint* toBone = getBone(link.ToBone);
        if (!fromBone || !toBone || !fromBone->OnScreen || !toBone->OnScreen)
            continue;

        const float distancePx = DistancePointToSegment2D(crosshair, fromBone->Screen, toBone->Screen);
        const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
            ? 0
            : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                ? hitboxRegionFromBone(link.FromBone)
                : 1);
        const float threshold = adaptiveBodyRadiusPx * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
        const float normalized = distancePx / threshold;
        if (normalized < bestNormalizedDistance)
        {
            bestNormalizedDistance = normalized;
            activeFromBone = link.FromBone;
            activeToBone = link.ToBone;
        }
    }

    const bool hasActiveSegment = bestNormalizedDistance <= 1.0f;
    const ImU32 baseColor = ToImColor(config.Aim.TriggerHitboxDebugColor);
    const ImU32 activeColor = ToImColor(config.Aim.TriggerHitboxDebugActiveColor);
    const float lineThickness = std::clamp(config.Aim.TriggerHitboxDebugThickness, 0.5f, 4.0f);

    constexpr std::array<std::pair<int, int>, 12> kBoxEdges = {
        std::pair{ 0, 1 }, std::pair{ 1, 2 }, std::pair{ 2, 3 }, std::pair{ 3, 0 },
        std::pair{ 4, 5 }, std::pair{ 5, 6 }, std::pair{ 6, 7 }, std::pair{ 7, 4 },
        std::pair{ 0, 4 }, std::pair{ 1, 5 }, std::pair{ 2, 6 }, std::pair{ 3, 7 }
    };

    const Matrix viewMatrix = Globals::ViewMatrix;
    for (const DebugBoneLink& link : kDebugBoneLinks)
    {
        const bool endpointSelected =
            (boneMask & Structs::BoneMaskFromBoneId(link.FromBone)) != 0ull ||
            (boneMask & Structs::BoneMaskFromBoneId(link.ToBone)) != 0ull;
        if (!endpointSelected)
            continue;

        const BonePoint* fromBone = getBone(link.FromBone);
        const BonePoint* toBone = getBone(link.ToBone);
        if (!fromBone || !toBone)
            continue;

        if (!IsNonZeroPosition(fromBone->World) || !IsNonZeroPosition(toBone->World))
            continue;

        const int linkRegion = (hitboxRegionFromBone(link.FromBone) == 0 || hitboxRegionFromBone(link.ToBone) == 0)
            ? 0
            : (hitboxRegionFromBone(link.FromBone) == hitboxRegionFromBone(link.ToBone)
                ? hitboxRegionFromBone(link.FromBone)
                : 1);
        const float segmentRadiusPx = adaptiveBodyRadiusPx * BoneLinkRadiusScale(link.FromBone, link.ToBone) * hitboxRegionScale(linkRegion);
        std::array<Vector2, 8> projectedCorners{};
        if (!BuildProjectedSegmentBoxCorners(fromBone->World, toBone->World, segmentRadiusPx, viewMatrix, projectedCorners))
            continue;

        const bool isActive = hasActiveSegment && link.FromBone == activeFromBone && link.ToBone == activeToBone;
        const ImU32 color = isActive ? activeColor : baseColor;

        for (const auto& [edgeStart, edgeEnd] : kBoxEdges)
        {
            drawList->AddLine(
                projectedCorners[edgeStart].ToImVec2(),
                projectedCorners[edgeEnd].ToImVec2(),
                color,
                lineThickness
            );
        }
    }

    if (config.Aim.TriggerHeadSphereDebug &&
        (boneMask & Structs::BoneMaskFromBoneId(Structs::AimHeadBoneId)) != 0ull)
    {
        const BonePoint* headBone = getBone(Structs::AimHeadBoneId);
        if (headBone && headBone->OnScreen)
        {
            const float headThreshold = (std::max)(0.5f, adaptiveHeadRadiusPx * hitboxRegionScale(0));
            const bool headActive = Distance2D(crosshair, headBone->Screen) <= headThreshold;
            drawList->AddCircle(
                headBone->Screen.ToImVec2(),
                headThreshold,
                headActive ? activeColor : baseColor,
                48,
                (std::max)(1.0f, lineThickness + 0.2f)
            );
        }
    }
}

void ESP::RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const
{
    if (!c4.Valid || !config.Visuals.C4)
        return;

    auto formatSeconds1 = [](const float value) -> std::string
    {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%.1f", (std::max)(0.0f, value));
        return buffer;
    };

    if (c4.OnScreen)
    {
        const ImU32 markerColor = c4.Planted ? ToImColor(config.Visuals.C4Color) : IM_COL32(240, 220, 70, 255);
        drawList->AddCircleFilled(c4.Screen.ToImVec2(), 4.0f, markerColor, 12);
        drawList->AddCircle(c4.Screen.ToImVec2(), 8.0f, markerColor, 12, 1.2f);
        drawList->AddText(
            ImVec2(c4.Screen.x + 9.0f, c4.Screen.y - 15.0f),
            markerColor,
            c4.Planted ? Localization::Pick("C4(Planted)", "C4(已下包)") : "C4"
        );
    }

    if (!c4.Planted)
        return;

    std::string siteName = Localization::Pick("Unknown", "未知");
    if (c4.BombSite == 0)
        siteName = "A";
    else if (c4.BombSite == 1)
        siteName = "B";

    const std::string line1 =
        std::string(Localization::Pick("C4 Site: ", "C4包点: ")) +
        siteName + " | " +
        (c4.BeingDefused ? Localization::Pick("Defusing", "正在拆包") : Localization::Pick("Not Defusing", "未在拆包"));

    std::string line2 = std::string(Localization::Pick("Explode: ", "爆炸: ")) + formatSeconds1(c4.TimeRemaining) + "s  " + Localization::Pick("Defuse: ", "鎷嗗脊: ");
    if (c4.BeingDefused)
        line2 += formatSeconds1(c4.DefuseCountDown) + "s";
    else
        line2 += "--";

    std::string line3 = Localization::Pick("Defuse Result: --", "拆包结果: --");
    if (c4.BeingDefused)
        line3 = std::string(Localization::Pick("Defuse Result: ", "拆包结果: ")) + (c4.CanDefuse ? Localization::Pick("SUCCESS", "鎴愬姛") : Localization::Pick("FAIL", "澶辫触"));

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    constexpr float panelW = 250.0f;
    constexpr float panelH = 84.0f;
    const float panelX = std::clamp(config.Visuals.C4PanelPosX * displaySize.x, 8.0f, (std::max)(8.0f, displaySize.x - panelW - 8.0f));
    const float panelY = std::clamp(config.Visuals.C4PanelPosY * displaySize.y, 8.0f, (std::max)(8.0f, displaySize.y - panelH - 8.0f));
    drawList->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(18, 18, 18, 190),
        5.0f
    );
    drawList->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        ToImColor(config.Visuals.C4Color),
        5.0f,
        0,
        1.2f
    );

    drawList->AddText(ImVec2(panelX + 10.0f, panelY + 8.0f), ToImColor(config.Visuals.C4Color), line1.c_str());
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 28.0f),
        c4.BeingDefused ? ToImColor(config.Visuals.DefuserColor) : IM_COL32(225, 225, 225, 255),
        line2.c_str()
    );
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 48.0f),
        c4.BeingDefused ? (c4.CanDefuse ? IM_COL32(120, 235, 120, 255) : IM_COL32(255, 110, 110, 255)) : IM_COL32(225, 225, 225, 255),
        line3.c_str()
    );
}

void ESP::RenderGrenadeHelper(ImDrawList* drawList, const GrenadeHelperSnapshot& helper) const
{
    if (!drawList || !config.Visuals.GrenadeHelper || !helper.Valid)
        return;

    const float aimCircleRadius = std::clamp(config.Visuals.GrenadeHelperFocusRadius, 5.0f, 50.0f);
    const ImU32 standPointColor = ToImColor(config.Visuals.GrenadeHelperStandColor);
    const ImU32 aimPointColor = ToImColor(config.Visuals.GrenadeHelperAimColor);
    const ImU32 guideLineColor = ToImColor(config.Visuals.GrenadeHelperGuideLineColor);
    const ImU32 helperTextColor = ToImColor(config.Visuals.GrenadeHelperFontColor);
    const ImU32 topHintColor = IM_COL32(255, 255, 255, 255);
    const float helperFontSize = std::clamp(config.Visuals.GrenadeHelperFontSize, 10.0f, 32.0f);
    const float topHintFontSize = std::clamp(config.Visuals.GrenadeHelperTopHintFontSize, 20.0f, 100.0f);

    for (const GrenadeStandRenderItem& stand : helper.StandItems)
    {
        drawList->AddCircleFilled(stand.Screen.ToImVec2(), 4.0f, standPointColor, 14);
        if (!stand.Label.empty())
            drawList->AddText(
                ImGui::GetFont(),
                helperFontSize,
                ImVec2(stand.Screen.x + 6.0f, stand.Screen.y + 8.0f),
                helperTextColor,
                stand.Label.c_str()
            );
    }

    for (const GrenadeAimRenderItem& aimItem : helper.AimItems)
    {
        drawList->AddCircle(aimItem.Screen.ToImVec2(), aimCircleRadius, aimPointColor, 24, aimItem.IsTarget ? 2.8f : 2.0f);

        if (aimItem.DrawGuide)
        {
            drawList->AddLine(
                helper.Cross.ToImVec2(),
                aimItem.Screen.ToImVec2(),
                guideLineColor,
                aimItem.IsTarget ? 2.0f : 1.0f
            );
        }

        if (!aimItem.Label.empty())
        {
            drawList->AddText(
                ImGui::GetFont(),
                helperFontSize,
                ImVec2(aimItem.Screen.x + 12.0f, aimItem.Screen.y - 16.0f),
                helperTextColor,
                aimItem.Label.c_str()
            );
        }
    }

    if (!helper.TopHintTokens.empty())
    {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float screenWidth = displaySize.x > 1.0f ? displaySize.x : Screen.x;
        const float screenHeight = displaySize.y > 1.0f ? displaySize.y : Screen.y;

        float ratioX = config.Visuals.GrenadeHelperTopHintOffsetX;
        float ratioY = config.Visuals.GrenadeHelperTopHintOffsetY;

        // Backward compatibility for old pixel-offset configs.
        if (ratioX < 0.0f || ratioX > 1.0f)
            ratioX = std::clamp(0.5f + ratioX / (std::max)(1.0f, screenWidth), 0.0f, 1.0f);
        if (ratioY < 0.0f || ratioY > 1.0f)
            ratioY = std::clamp((28.0f + ratioY) / (std::max)(1.0f, screenHeight), 0.0f, 1.0f);

        struct HintItem
        {
            std::string Token{};
            const KeyIconTexture* Icon = nullptr;
            ImVec2 Size{};
            bool IsText = false;
        };

        const float iconTargetHeight = std::clamp(topHintFontSize, 20.0f, 100.0f);
        const float itemSpacing = std::clamp(iconTargetHeight * 0.15f, 4.0f, 14.0f);
        std::vector<HintItem> hintItems{};
        hintItems.reserve(helper.TopHintTokens.size());

        float totalWidth = 0.0f;
        float rowHeight = 0.0f;
        for (const std::string& tokenRaw : helper.TopHintTokens)
        {
            HintItem item{};
            item.Token = NormalizeThrowTokenName(tokenRaw);
            if (item.Token.empty())
                item.Token = tokenRaw;

            item.Icon = GetKeyIconTexture(item.Token, Overlay::device);
            if (item.Icon && item.Icon->Width > 0 && item.Icon->Height > 0)
            {
                item.Size.y = iconTargetHeight;
                item.Size.x = iconTargetHeight * static_cast<float>(item.Icon->Width) / static_cast<float>(item.Icon->Height);
                item.IsText = false;
            }
            else
            {
                item.IsText = true;
                const std::string fallbackText = ThrowTokenFallbackText(item.Token);
                item.Size = ImGui::GetFont()->CalcTextSizeA(topHintFontSize, FLT_MAX, 0.0f, fallbackText.c_str());
            }

            rowHeight = (std::max)(rowHeight, item.Size.y);
            if (!hintItems.empty())
                totalWidth += itemSpacing;
            totalWidth += item.Size.x;
            hintItems.push_back(std::move(item));
        }

        if (hintItems.empty())
            return;

        const float anchorX = ratioX * screenWidth;
        const float anchorY = ratioY * screenHeight;
        const float rowX = std::clamp(
            anchorX - totalWidth * 0.5f,
            8.0f,
            (std::max)(8.0f, screenWidth - totalWidth - 8.0f)
        );
        const float rowY = std::clamp(
            anchorY,
            8.0f,
            (std::max)(8.0f, screenHeight - rowHeight - 8.0f)
        );

        float cursorX = rowX;
        for (const HintItem& item : hintItems)
        {
            const float drawY = rowY + (rowHeight - item.Size.y) * 0.5f;
            if (!item.IsText && item.Icon && item.Icon->Srv)
            {
                drawList->AddImage(
                    reinterpret_cast<ImTextureID>(item.Icon->Srv),
                    ImVec2(cursorX, drawY),
                    ImVec2(cursorX + item.Size.x, drawY + item.Size.y),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    topHintColor
                );
            }
            else
            {
                const std::string fallbackText = ThrowTokenFallbackText(item.Token);
                drawList->AddText(
                    ImGui::GetFont(),
                    topHintFontSize,
                    ImVec2(cursorX, drawY),
                    topHintColor,
                    fallbackText.c_str()
                );
            }

            cursorX += item.Size.x + itemSpacing;
        }

        const std::string remark = TrimAscii(helper.TopHintRemark);
        if (!remark.empty())
        {
            const float remarkFontSize = std::clamp(config.Visuals.GrenadeHelperFontSize, 10.0f, topHintFontSize);
            const ImVec2 remarkSize = ImGui::GetFont()->CalcTextSizeA(remarkFontSize, FLT_MAX, 0.0f, remark.c_str());
            const float remarkY = std::clamp(
                rowY + rowHeight + std::clamp(iconTargetHeight * 0.18f, 4.0f, 12.0f),
                8.0f,
                (std::max)(8.0f, screenHeight - remarkSize.y - 8.0f)
            );
            const float remarkX = std::clamp(
                anchorX - remarkSize.x * 0.5f,
                8.0f,
                (std::max)(8.0f, screenWidth - remarkSize.x - 8.0f)
            );
            drawList->AddText(ImGui::GetFont(), remarkFontSize, ImVec2(remarkX, remarkY), helperTextColor, remark.c_str());
        }
    }
}

bool ESP::BuildBoneData(const uint64_t boneArray, PlayerEspSnapshot& inOutSnapshot) const
{
    if (!boneArray || !IsLikelyUserAddress(boneArray))
        return false;

    std::array<BoneDataRaw, kTrackedBones.size()> rawBones{};
    const auto scatter = mem.CreateScatterHandle();
    if (!scatter)
        return false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const uint64_t boneAddress = boneArray + static_cast<uint64_t>(kTrackedBones[i]) * Offsets::Layout::BoneStride;
        mem.AddScatterReadRequest(scatter, boneAddress, &rawBones[i], sizeof(BoneDataRaw));
    }

    mem.ExecuteReadScatter(scatter);
    mem.CloseScatterHandle(scatter);

    inOutSnapshot.Bones.clear();
    inOutSnapshot.Bones.reserve(kTrackedBones.size());

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    bool hasHead = false;
    bool anyOnScreen = false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const int boneIndex = kTrackedBones[i];
        const BoneDataRaw& rawBone = rawBones[i];

        BonePoint point{};
        point.Index = boneIndex;
        point.World = rawBone.Position;
        point.OnScreen = sdk.WorldToScreen(point.World, point.Screen);

        if (boneIndex == kHeadBone)
        {
            inOutSnapshot.HeadPosition = point.World;
            hasHead = true;
        }

        if (point.OnScreen)
        {
            anyOnScreen = true;
            minX = (std::min)(minX, point.Screen.x);
            minY = (std::min)(minY, point.Screen.y);
            maxX = (std::max)(maxX, point.Screen.x);
            maxY = (std::max)(maxY, point.Screen.y);
        }

        inOutSnapshot.Bones.push_back(point);
    }

    if (!hasHead)
    {
        inOutSnapshot.HeadPosition = inOutSnapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };
    }

    if (!anyOnScreen)
        return false;

    const float width = maxX - minX;
    const float height = maxY - minY;
    if (width < 3.0f || height < 3.0f)
        return false;

    const float paddingX = (std::max)(width * 0.08f, 3.0f);
    const float paddingY = (std::max)(height * 0.06f, 3.0f);

    inOutSnapshot.BoxMin = ImVec2(minX - paddingX, minY - paddingY);
    inOutSnapshot.BoxMax = ImVec2(maxX + paddingX, maxY + paddingY);

    return true;
}

C4Snapshot ESP::ReadC4Snapshot() const
{
    C4Snapshot snapshot{};
    if (!Globals::ClientBase || !Offsets::Client::dwPlantedC4)
        return snapshot;

    const float gameTime = ReadGlobalCurrentTime();
    const std::uint64_t nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    static std::uint64_t bombPlantStartMs = 0;
    static std::uint64_t bombDefuseStartMs = 0;
    static bool wasDefusing = false;
    static bool canDefuseLatched = false;

    auto readBool = [](const std::uint64_t address, bool& outValue)
    {
        outValue = mem.Read<std::uint8_t>(address) != 0;
    };

    auto readBombWorldPos = [](const std::uint64_t bombEntity, Vector3& outPos) -> bool
    {
        outPos = {};
        if (!IsLikelyUserAddress(bombEntity))
            return false;

        if (Offsets::Schema::m_pGameSceneNode && Offsets::Schema::m_vecAbsOrigin)
        {
            const std::uint64_t sceneNode = mem.Read<std::uint64_t>(bombEntity + Offsets::Schema::m_pGameSceneNode);
            if (IsLikelyUserAddress(sceneNode))
            {
                const Vector3 absPos = mem.Read<Vector3>(sceneNode + Offsets::Schema::m_vecAbsOrigin);
                if (IsNonZeroPosition(absPos))
                {
                    outPos = absPos;
                    return true;
                }
            }
        }

        if (Offsets::Schema::m_vecC4ExplodeSpectatePos)
        {
            const Vector3 spectatePos = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vecC4ExplodeSpectatePos);
            if (IsNonZeroPosition(spectatePos))
            {
                outPos = spectatePos;
                return true;
            }
        }

        if (Offsets::Schema::m_vOldOrigin)
        {
            const Vector3 oldOrigin = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vOldOrigin);
            if (IsNonZeroPosition(oldOrigin))
            {
                outPos = oldOrigin;
                return true;
            }
        }

        return false;
    };

    auto resolveBombEntityFromHolder = [&](const std::uint64_t holderAddress) -> std::uint64_t
    {
        if (!IsLikelyUserAddress(holderAddress))
            return 0;

        // Primary path used by reference project: holder -> entity pointer.
        const std::uint64_t indirectEntity = mem.Read<std::uint64_t>(holderAddress);
        if (IsLikelyUserAddress(indirectEntity))
            return indirectEntity;

        // Fallback for builds where global points directly to entity.
        Vector3 validatePos{};
        if (readBombWorldPos(holderAddress, validatePos))
            return holderAddress;

        return 0;
    };

    const std::uint64_t clientBase = Globals::ClientBase;
    const std::uint64_t plantedHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwPlantedC4);

    bool plantedFlag = false;
    if (Offsets::Client::dwPlantedC4 >= 0x8)
    {
        const std::uint8_t plantedByte = mem.Read<std::uint8_t>(clientBase + Offsets::Client::dwPlantedC4 - 0x8);
        plantedFlag = plantedByte != 0;
    }

    const std::uint64_t plantedEntity = resolveBombEntityFromHolder(plantedHolder);
    if (!plantedFlag && IsLikelyUserAddress(plantedEntity) && Offsets::Schema::m_bBombTicking)
    {
        bool tickingCandidate = false;
        readBool(plantedEntity + Offsets::Schema::m_bBombTicking, tickingCandidate);

        bool defusedCandidate = false;
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, defusedCandidate);

        plantedFlag = tickingCandidate && !defusedCandidate;
    }

    if (plantedFlag && IsLikelyUserAddress(plantedEntity))
    {
        snapshot.Valid = true;

        if (Offsets::Schema::m_nBombSite)
        {
            int normalizedSite = NormalizeBombSiteRaw(static_cast<int>(mem.Read<std::uint8_t>(plantedEntity + Offsets::Schema::m_nBombSite)));
            if (normalizedSite < 0)
                normalizedSite = NormalizeBombSiteRaw(mem.Read<int>(plantedEntity + Offsets::Schema::m_nBombSite));
            snapshot.BombSite = normalizedSite;
        }

        if (Offsets::Schema::m_bBombTicking)
            readBool(plantedEntity + Offsets::Schema::m_bBombTicking, snapshot.BombTicking);
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, snapshot.BombDefused);
        if (Offsets::Schema::m_bBeingDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBeingDefused, snapshot.BeingDefused);
        if (Offsets::Schema::m_flTimerLength)
            snapshot.TimerLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flTimerLength);
        if (Offsets::Schema::m_flDefuseLength)
            snapshot.DefuseLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseLength);
        if (Offsets::Schema::m_flC4Blow)
            snapshot.BlowTime = mem.Read<float>(plantedEntity + Offsets::Schema::m_flC4Blow);
        if (Offsets::Schema::m_flDefuseCountDown)
            snapshot.DefuseCountDown = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseCountDown);

        snapshot.Planted = snapshot.BombTicking && !snapshot.BombDefused;

        if (snapshot.BombTicking && snapshot.BlowTime > 0.001f && gameTime > 0.001f)
        {
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.BlowTime - gameTime);
            bombPlantStartMs = 0;
        }
        else if (snapshot.BombTicking && snapshot.TimerLength > 0.001f)
        {
            if (bombPlantStartMs == 0)
                bombPlantStartMs = nowMs;
            const float elapsed = static_cast<float>(nowMs - bombPlantStartMs) / 1000.0f;
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.TimerLength - elapsed);
        }
        else
        {
            bombPlantStartMs = 0;
            snapshot.TimeRemaining = 0.0f;
        }

        if (snapshot.BeingDefused)
        {
            if (snapshot.DefuseCountDown > 0.001f && gameTime > 0.001f)
            {
                snapshot.DefuseCountDown = (std::max)(0.0f, snapshot.DefuseCountDown - gameTime);
                bombDefuseStartMs = 0;
            }
            else
            {
                if (bombDefuseStartMs == 0)
                    bombDefuseStartMs = nowMs;
                const float elapsed = static_cast<float>(nowMs - bombDefuseStartMs) / 1000.0f;
                const float total = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
                snapshot.DefuseCountDown = (std::max)(0.0f, total - elapsed);
            }

            const float totalDefuse = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
            snapshot.DefuseProgress = std::clamp(1.0f - (snapshot.DefuseCountDown / totalDefuse), 0.0f, 1.0f);

            if (!wasDefusing)
                canDefuseLatched = snapshot.DefuseCountDown <= (snapshot.TimeRemaining + 0.05f);

            snapshot.CanDefuse = canDefuseLatched;
            wasDefusing = true;
        }
        else
        {
            bombDefuseStartMs = 0;
            snapshot.DefuseCountDown = 0.0f;
            snapshot.DefuseProgress = 0.0f;
            snapshot.CanDefuse = false;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        if (snapshot.BombDefused)
        {
            bombPlantStartMs = 0;
            bombDefuseStartMs = 0;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        readBombWorldPos(plantedEntity, snapshot.Position);
    }
    else
    {
        bombPlantStartMs = 0;
        bombDefuseStartMs = 0;
        wasDefusing = false;
        canDefuseLatched = false;

        if (Offsets::Client::dwWeaponC4)
        {
            const std::uint64_t weaponHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwWeaponC4);
            const std::uint64_t weaponEntity = resolveBombEntityFromHolder(weaponHolder);
            if (IsLikelyUserAddress(weaponEntity))
            {
                snapshot.Valid = true;
                snapshot.Planted = false;
                readBombWorldPos(weaponEntity, snapshot.Position);
            }
        }
    }

    if (IsNonZeroPosition(snapshot.Position))
        snapshot.OnScreen = sdk.WorldToScreen(snapshot.Position, snapshot.Screen);

    return snapshot;
}

bool ESP::IsAlive(const int health, const int lifeState) const
{
    if (health <= 0 || health > 200)
        return false;

    return lifeState == kAliveLifeStateA || lifeState == kAliveLifeStateB;
}

ImU32 ESP::GetBoxColor(const bool isVisible) const
{
    if (!config.Visuals.VisibleCheck)
        return ToImColor(config.Visuals.BoxColor);

    if (isVisible)
        return ToImColor(config.Visuals.BoxColorVisible);

    return IM_COL32(255, 255, 255, 255);
}

ImU32 ESP::ToImColor(const ImVec4& color) const
{
    return ImGui::ColorConvertFloat4ToU32(color);
}

std::string ESP::ReadPlayerName(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_iszPlayerName)
        return {};

    char nameBuffer[128]{};
    if (!mem.Read(controller + Offsets::Schema::m_iszPlayerName, nameBuffer, sizeof(nameBuffer)))
        return {};

    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    std::string name(nameBuffer);
    name.erase(std::remove_if(name.begin(), name.end(), [](char c)
    {
        const unsigned char value = static_cast<unsigned char>(c);
        return value < 0x20 && value != '\t';
    }), name.end());

    return name;
}

int ESP::ReadMoney(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_pInGameMoneyServices || !Offsets::Schema::m_iAccount)
        return 0;

    const uint64_t moneyServices = mem.Read<uint64_t>(controller + Offsets::Schema::m_pInGameMoneyServices);
    if (!moneyServices || !IsLikelyUserAddress(moneyServices))
        return 0;

    return mem.Read<int>(moneyServices + Offsets::Schema::m_iAccount);
}

int ESP::ReadWeaponId(const uint64_t pawn) const
{
    if (!pawn || !Offsets::Schema::m_pWeaponServices || !Offsets::Schema::m_hActiveWeapon)
        return 0;
    if (!Offsets::Schema::m_AttributeManager || !Offsets::Schema::m_Item || !Offsets::Schema::m_iItemDefinitionIndex)
        return 0;

    const uint64_t weaponServices = mem.Read<uint64_t>(pawn + Offsets::Schema::m_pWeaponServices);
    if (!weaponServices || !IsLikelyUserAddress(weaponServices))
        return 0;

    const uint32_t activeWeaponHandle = mem.Read<uint32_t>(weaponServices + Offsets::Schema::m_hActiveWeapon);
    if (!activeWeaponHandle)
        return 0;

    const uint64_t weapon = sdk.ResolveEntityFromHandle(activeWeaponHandle);
    if (!weapon || !IsLikelyUserAddress(weapon))
        return 0;

    const std::uint64_t itemDefinitionIndexAddress =
        weapon +
        static_cast<uint64_t>(Offsets::Schema::m_AttributeManager) +
        static_cast<uint64_t>(Offsets::Schema::m_Item) +
        static_cast<uint64_t>(Offsets::Schema::m_iItemDefinitionIndex);

    return static_cast<int>(mem.Read<std::uint16_t>(itemDefinitionIndexAddress));
}

std::string ESP::ReadWeaponName(const uint64_t pawn) const
{
    const int weaponId = ReadWeaponId(pawn);
    if (weaponId <= 0)
        return {};

    const std::string resolvedName = WeaponIdToName(weaponId);
    if (!resolvedName.empty())
        return resolvedName;

    return std::string(Localization::Pick("Weapon ", "武器 ")) + std::to_string(weaponId);
}

std::string ESP::ReadGrenadeType(const uint64_t pawn) const
{
    switch (ReadWeaponId(pawn))
    {
    case 43: return "Flash";
    case 44: return "HE";
    case 45: return "Smoke";
    case 46: return "Molotov";
    case 47: return "Decoy";
    case 48: return "Molotov";
    default: return "Unknown";
    }
}

void ESP::UpdateVisCheckState()
{
    ConsumeMapLoadResult();

    // Keep map/.opt status alive by default; VisibleCheck only controls usage, not loading state.
    m_VisCheckEnabled = true;

    const auto now = std::chrono::steady_clock::now();
    if (m_LastMapPoll.time_since_epoch().count() == 0 ||
        now - m_LastMapPoll >= std::chrono::milliseconds(1000))
    {
        m_LastPolledMapName = sdk.GetCurrentMapName();
        PerfDebug::RecordMapPoll(!m_LastPolledMapName.empty());
        m_LastMapPoll = now;
    }

    const std::string& mapName = m_LastPolledMapName;
    if (mapName.empty())
    {
        m_MapStatus = Localization::Pick("Map Status: (No Map)", "地图状态：（无地图）");
        return;
    }

    if (m_VisCheck && mapName == m_CurrentMapName)
    {
        m_MapStatus = BuildMapStatus(mapName, "Loaded");
        return;
    }

    if (!m_VisCheck && mapName == m_CurrentMapName && m_CurrentOptPath.empty())
    {
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    for (const PendingMapLoad& pending : m_PendingMapLoads)
    {
        if (pending.RequestId == m_ActiveMapRequestId && pending.MapName == mapName)
            return;
    }

    const std::string optPath = ResolveOptPath(mapName);
    if (optPath.empty())
    {
        m_VisCheck.reset();
        m_CurrentMapName = mapName;
        m_CurrentOptPath.clear();
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    RequestMapLoad(mapName, optPath);
}

void ESP::RequestMapLoad(const std::string& mapName, const std::string& optPath)
{
    PendingMapLoad pending{};
    pending.RequestId = ++m_NextMapRequestId;
    pending.MapName = mapName;
    pending.OptPath = optPath;
    pending.Future = std::async(std::launch::async, [optPath]()
    {
        auto visCheck = std::make_unique<VisCheck>(optPath);
        if (visCheck && visCheck->IsReady())
            return visCheck;

        return std::unique_ptr<VisCheck>{};
    });

    m_ActiveMapRequestId = pending.RequestId;
    m_MapStatus = BuildMapStatus(mapName, "Loading");
    m_PendingMapLoads.push_back(std::move(pending));
}

void ESP::ConsumeMapLoadResult()
{
    for (size_t index = 0; index < m_PendingMapLoads.size();)
    {
        PendingMapLoad& pending = m_PendingMapLoads[index];
        if (!pending.Future.valid())
        {
            m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
            continue;
        }

        if (pending.Future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            ++index;
            continue;
        }

        std::unique_ptr<VisCheck> loadedVisCheck = pending.Future.get();
        const bool requestIsCurrent = pending.RequestId == m_ActiveMapRequestId;

        if (requestIsCurrent)
        {
            if (loadedVisCheck)
            {
                m_VisCheck = std::move(loadedVisCheck);
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Loaded");
            }
            else
            {
                m_VisCheck.reset();
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Load Failed");
            }
        }

        m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
    }
}

std::string ESP::ResolveOptPath(const std::string& mapName) const
{
    if (mapName.empty())
        return {};

    const std::string fileName = mapName + ".opt";
    std::vector<std::filesystem::path> candidates{};
    candidates.reserve(64);

    auto appendCandidates = [&](std::filesystem::path base)
    {
        std::error_code ec{};
        for (int depth = 0; depth < 6 && !base.empty(); ++depth)
        {
            candidates.push_back(base / "maps" / fileName);
            candidates.push_back(base / "Maps" / fileName);
            candidates.push_back(base / "project-d" / "maps" / fileName);
            candidates.push_back(base / "project-d" / "Maps" / fileName);
            candidates.push_back(base / fileName);

            const std::filesystem::path parent = base.parent_path();
            if (parent == base || parent.empty())
                break;
            base = parent;
        }
    };

    std::error_code cwdError{};
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError)
        appendCandidates(cwd);

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        appendCandidates(std::filesystem::path(modulePath).parent_path());

    for (const std::filesystem::path& candidate : candidates)
    {
        if (candidate.empty())
            continue;

        std::error_code existsError{};
        if (std::filesystem::exists(candidate, existsError) && !existsError)
            return candidate.string();
    }

    return {};
}

std::string ESP::ResolveGrenadeDataPath(const std::string& mapName) const
{
    if (mapName.empty())
        return {};

    const std::string fileName = mapName + ".json";
    std::vector<std::filesystem::path> candidates{};
    candidates.reserve(64);

    auto appendCandidates = [&](std::filesystem::path base)
    {
        std::error_code ec{};
        for (int depth = 0; depth < 6 && !base.empty(); ++depth)
        {
            candidates.push_back(base / "GrenadeData" / fileName);
            candidates.push_back(base / "grenadedata" / fileName);
            candidates.push_back(base / "project-d" / "GrenadeData" / fileName);
            candidates.push_back(base / "project-d" / "grenadedata" / fileName);
            candidates.push_back(base / fileName);

            const std::filesystem::path parent = base.parent_path();
            if (parent == base || parent.empty())
                break;
            base = parent;
        }
    };

    std::error_code cwdError{};
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError)
        appendCandidates(cwd);

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        appendCandidates(std::filesystem::path(modulePath).parent_path());

    for (const std::filesystem::path& candidate : candidates)
    {
        if (candidate.empty())
            continue;

        std::error_code existsError{};
        if (std::filesystem::exists(candidate, existsError) && !existsError)
            return candidate.string();
    }

    return {};
}

std::string ESP::ResolveGrenadeDataWritePath(const std::string& mapName) const
{
    const std::string normalizedMap = NormalizeMapName(mapName);
    if (normalizedMap.empty())
        return {};

    if (const std::string existingPath = ResolveGrenadeDataPath(normalizedMap); !existingPath.empty())
        return existingPath;

    const std::string fileName = normalizedMap + ".json";
    std::vector<std::filesystem::path> candidateDirs{};
    candidateDirs.reserve(16);

    auto appendDirs = [&](const std::filesystem::path& base)
    {
        if (base.empty())
            return;

        candidateDirs.push_back(base / "GrenadeData");
        candidateDirs.push_back(base / "grenadedata");
        candidateDirs.push_back(base / "project-d" / "GrenadeData");
        candidateDirs.push_back(base / "project-d" / "grenadedata");
    };

    std::error_code cwdError{};
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError)
        appendDirs(cwd);

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        appendDirs(std::filesystem::path(modulePath).parent_path());

    for (const std::filesystem::path& dir : candidateDirs)
    {
        std::error_code ec{};
        if (dir.empty() || !std::filesystem::exists(dir, ec) || ec)
            continue;
        return (dir / fileName).string();
    }

    std::filesystem::path fallbackDir = cwdError ? std::filesystem::path("GrenadeData") : (cwd / "project-d" / "GrenadeData");
    std::error_code createEc{};
    std::filesystem::create_directories(fallbackDir, createEc);
    if (createEc)
        return (std::filesystem::path("GrenadeData") / fileName).string();

    return (fallbackDir / fileName).string();
}

bool ESP::LoadGrenadeMapFile(const std::string& filePath, GrenadeMapData& outMap, std::string& outError) const
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        outError = "open failed";
        return false;
    }

    const std::string sourceText{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
    if (!file.good() && !file.eof())
    {
        outError = "read failed";
        return false;
    }

    json root{};
    std::string parseError{};
    try
    {
        root = json::parse(sourceText);
    }
    catch (const std::exception& ex)
    {
        parseError = ex.what();

        try
        {
            const std::string repairedText = RepairMalformedGrenadeJsonText(sourceText);
            root = json::parse(repairedText);
            LOG_WARN("Grenade json repaired on load: {}", filePath);
        }
        catch (const std::exception& ex2)
        {
            outError = ex2.what();
            if (!parseError.empty())
                outError += std::string(" (orig: ") + parseError + ")";
            return false;
        }
    }

    if (!root.is_object() || !root.contains("grenades") || !root["grenades"].is_array())
    {
        outError = "invalid grenades array";
        return false;
    }

    outMap = {};
    if (root.contains("map_name") && root["map_name"].is_string())
        outMap.MapName = NormalizeMapName(root["map_name"].get<std::string>());
    if (outMap.MapName.empty())
        outMap.MapName = NormalizeMapName(std::filesystem::path(filePath).stem().string());

    const auto& grenadeList = root["grenades"];
    outMap.Spots.reserve(grenadeList.size());

    for (const auto& item : grenadeList)
    {
        if (!item.is_object())
            continue;

        Vector3 standPos{};
        Vector3 aimPos{};
        if (!ReadVectorField(item, "position", standPos) || !ReadVectorField(item, "aim_target", aimPos))
            continue;

        GrenadeSpot spot{};
        if (item.contains("id") && item["id"].is_number_integer())
            spot.Id = item["id"].get<int>();
        else
            spot.Id = static_cast<int>(outMap.Spots.size()) + 1;

        if (item.contains("type") && item["type"].is_string())
            spot.Type = item["type"].get<std::string>();
        if (spot.Type.empty())
            spot.Type = "Unknown";

        if (item.contains("name") && item["name"].is_string())
            spot.Name = item["name"].get<std::string>();
        if (spot.Name.empty())
            spot.Name = "Unnamed";

        if (item.contains("remark") && item["remark"].is_string())
            spot.Remark = item["remark"].get<std::string>();
        spot.Remark = TrimAscii(spot.Remark);

        std::string bracketRemark{};
        ExtractTrailingBracketRemark(spot.Name, bracketRemark);
        MergeRemarkSegment(spot.Remark, bracketRemark);
        if (spot.Name.empty())
            spot.Name = "Unnamed";

        const std::string rawThrowType = (item.contains("throw_type") && item["throw_type"].is_string())
            ? item["throw_type"].get<std::string>()
            : std::string("LB");
        spot.ThrowType = NormalizeThrowType(rawThrowType, spot.Name, &spot.Remark);

        spot.StandPos = standPos;
        spot.AimPos = aimPos;
        outMap.Spots.push_back(std::move(spot));
    }

    return true;
}

bool ESP::SaveGrenadeMapFile(const std::string& filePath, const GrenadeMapData& mapData, std::string& outError) const
{
    if (filePath.empty())
    {
        outError = "empty path";
        return false;
    }

    json root{};
    root["map_name"] = NormalizeMapName(mapData.MapName);
    if (root["map_name"].get<std::string>().empty())
        root["map_name"] = NormalizeMapName(std::filesystem::path(filePath).stem().string());

    root["grenades"] = json::array();
    for (const GrenadeSpot& spot : mapData.Spots)
    {
        std::string spotName = spot.Name.empty() ? std::string("Unnamed") : spot.Name;
        std::string spotRemark = TrimAscii(spot.Remark);
        std::string bracketRemark{};
        ExtractTrailingBracketRemark(spotName, bracketRemark);
        MergeRemarkSegment(spotRemark, bracketRemark);
        if (spotName.empty())
            spotName = "Unnamed";
        const std::string canonicalThrowType = NormalizeThrowType(
            spot.ThrowType.empty() ? std::string("LB") : spot.ThrowType,
            spotName,
            &spotRemark
        );

        json node{};
        node["id"] = spot.Id;
        node["type"] = spot.Type.empty() ? std::string("Unknown") : spot.Type;
        node["name"] = spotName;
        node["throw_type"] = canonicalThrowType;
        node["remark"] = spotRemark;
        node["position"] = {
            { "x", spot.StandPos.x },
            { "y", spot.StandPos.y },
            { "z", spot.StandPos.z }
        };
        node["aim_target"] = {
            { "x", spot.AimPos.x },
            { "y", spot.AimPos.y },
            { "z", spot.AimPos.z }
        };

        root["grenades"].push_back(std::move(node));
    }

    std::error_code createEc{};
    const std::filesystem::path outPath(filePath);
    if (!outPath.parent_path().empty())
        std::filesystem::create_directories(outPath.parent_path(), createEc);
    if (createEc)
    {
        outError = createEc.message();
        return false;
    }

    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open())
    {
        outError = "open write failed";
        return false;
    }

    out << root.dump(2);
    if (!out.good())
    {
        outError = "write failed";
        return false;
    }

    return true;
}

bool ESP::ReloadGrenadeMapFromDisk(const std::string& mapName, std::string& outStatus)
{
    const std::string normalizedMap = NormalizeMapName(mapName);
    if (normalizedMap.empty())
    {
        outStatus = Localization::Pick("Reload failed: empty map name", "重载失败：地图名为空");
        return false;
    }

    GrenadeMapData loadedMap{};
    loadedMap.MapName = normalizedMap;

    const std::string filePath = ResolveGrenadeDataPath(normalizedMap);
    if (!filePath.empty())
    {
        std::string error{};
        if (!LoadGrenadeMapFile(filePath, loadedMap, error))
        {
            outStatus = std::string(Localization::Pick("Reload failed: ", "重载失败：")) + error;
            return false;
        }
    }

    {
        std::lock_guard lock(m_GrenadeMutex);
        m_GrenadeMap = std::move(loadedMap);
        m_LoadedGrenadeMap = normalizedMap;
        m_LastGrenadeSelectedSpotId = 0;
        if (filePath.empty())
            m_GrenadeStatus = std::string(Localization::Pick("No grenade file found, using empty map cache: ", "未找到道具点位文件，使用空地图缓存")) + normalizedMap;
        else
            m_GrenadeStatus = std::string(Localization::Pick("Loaded grenade spots: ", "道具点位加载成功")) + std::to_string(m_GrenadeMap.Spots.size());
        outStatus = m_GrenadeStatus;
    }

    return true;
}

void ESP::EnsureGrenadeMapLoaded(const std::string& mapName)
{
    const std::string normalizedMap = NormalizeMapName(mapName);
    if (normalizedMap.empty())
    {
        std::lock_guard lock(m_GrenadeMutex);
        m_GrenadeMap = {};
        m_LoadedGrenadeMap.clear();
        m_LastGrenadeSelectedSpotId = 0;
        m_GrenadeStatus = Localization::Pick("No map loaded", "无地图加载");
        return;
    }

    {
        std::lock_guard lock(m_GrenadeMutex);
        if (m_LoadedGrenadeMap == normalizedMap)
            return;
    }

    std::string status{};
    if (!ReloadGrenadeMapFromDisk(normalizedMap, status))
        LOG_WARN("Failed to reload grenade map '{}': {}", normalizedMap, status);
}

void ESP::BuildGrenadeHelperSnapshot(
    const Vector3& localOrigin,
    const Vector3& localEye,
    const Vector3& localViewAngles,
    uint64_t localPawn,
    const std::string& heldGrenadeType,
    GrenadeHelperSnapshot& outHelper)
{
    outHelper = {};

    if (!config.Visuals.GrenadeHelper || !localPawn)
    {
        m_LastGrenadeSelectedSpotId = 0;
        return;
    }

    const std::string mapName = NormalizeMapName(m_LastPolledMapName.empty() ? sdk.GetCurrentMapName() : m_LastPolledMapName);
    if (mapName.empty())
    {
        m_LastGrenadeSelectedSpotId = 0;
        return;
    }

    EnsureGrenadeMapLoaded(mapName);

    GrenadeMapData mapSnapshot{};
    {
        std::lock_guard lock(m_GrenadeMutex);
        mapSnapshot = m_GrenadeMap;
    }

    if (mapSnapshot.Spots.empty())
    {
        m_LastGrenadeSelectedSpotId = 0;
        return;
    }

    const std::string currentGrenade = config.Visuals.GrenadeHelperManualTypeOverride
        ? GrenadeTypeLabelByIndex(std::clamp(config.Visuals.GrenadeHelperManualType, 0, 4))
        : heldGrenadeType;

    if (config.Visuals.GrenadeHelperFilterByWeapon && currentGrenade == "Unknown")
    {
        m_LastGrenadeSelectedSpotId = 0;
        return;
    }

    struct StandDrawItem
    {
        const GrenadeSpot* Spot = nullptr;
        Vector2 Screen{};
    };

    struct AimDrawItem
    {
        const GrenadeSpot* Spot = nullptr;
        Vector2 Screen{};
        float CrossDistanceSqr = FLT_MAX;
    };

    std::vector<StandDrawItem> standDrawItems{};
    std::vector<AimDrawItem> aimDrawItems{};
    standDrawItems.reserve(mapSnapshot.Spots.size());
    aimDrawItems.reserve(mapSnapshot.Spots.size());

    const auto grenadeTypeMatches = [&](const std::string& spotType) -> bool
    {
        const std::string spotLower = ToLowerAscii(spotType);
        const std::string currentLower = ToLowerAscii(currentGrenade);
        if (currentLower == "molotov")
            return spotLower == "molotov" || spotLower == "incendiary";
        return spotLower == currentLower;
    };

    const float pitchRad = localViewAngles.x * (math::PI / 180.0f);
    const float yawRad = localViewAngles.y * (math::PI / 180.0f);
    const Vector3 cameraForward{
        std::cos(pitchRad) * std::cos(yawRad),
        std::cos(pitchRad) * std::sin(yawRad),
        -std::sin(pitchRad)
    };

    const float maxStandDrawDistance = std::clamp(config.Visuals.GrenadeHelperMaxStandDrawDistance, 200.0f, 10000.0f);
    const float standTolerance = std::clamp(config.Visuals.GrenadeHelperStandTolerance, 5.0f, 250.0f);
    const float stickyStandTolerance = standTolerance * 1.35f;
    const float focusRadius = std::clamp(config.Visuals.GrenadeHelperFocusRadius, 5.0f, 50.0f);
    const float looseGuideDistance = std::clamp(config.Visuals.GrenadeHelperLooseGuideDistance, 50.0f, 12000.0f);

    const float maxStandDrawDistanceSqr = maxStandDrawDistance * maxStandDrawDistance;
    const float standToleranceSqr = standTolerance * standTolerance;
    const float stickyStandToleranceSqr = stickyStandTolerance * stickyStandTolerance;
    const float focusRadiusSqr = focusRadius * focusRadius;
    const float looseGuideDistanceSqr = looseGuideDistance * looseGuideDistance;

    const Vector2 cross{ ScreenCenter.x, ScreenCenter.y };
    const int previousSpotId = m_LastGrenadeSelectedSpotId;

    const GrenadeSpot* closestSpot = nullptr;
    float minCrossDistanceSqr = FLT_MAX;

    for (const GrenadeSpot& spot : mapSnapshot.Spots)
    {
        if (config.Visuals.GrenadeHelperFilterByWeapon && !grenadeTypeMatches(spot.Type))
            continue;

        const float standDistanceSqr = DistanceSquared3D(localOrigin, spot.StandPos);
        if (standDistanceSqr > maxStandDrawDistanceSqr)
            continue;

        const bool stickyCandidate = previousSpotId != 0 && previousSpotId == spot.Id;
        const float activeToleranceSqr = stickyCandidate ? stickyStandToleranceSqr : standToleranceSqr;
        const bool withinStandTolerance = standDistanceSqr <= activeToleranceSqr;

        const Vector3 toStand = spot.StandPos - localEye;
        const float forwardDot = Dot3(toStand, cameraForward);
        if (!withinStandTolerance && forwardDot < 0.0f)
            continue;

        if (config.Visuals.GrenadeHelperDrawStand)
        {
            Vector2 standScreen{};
            if (sdk.WorldToScreen(spot.StandPos, standScreen) && IsOnScreen(standScreen))
                standDrawItems.push_back({ &spot, standScreen });
        }

        if (!config.Visuals.GrenadeHelperDrawAim || standDistanceSqr > activeToleranceSqr)
            continue;

        Vector2 aimScreen{};
        if (!sdk.WorldToScreen(spot.AimPos, aimScreen))
            continue;

        if (!IsOnScreen(aimScreen))
            aimScreen = ClampToScreenEdge(aimScreen);

        const float crossDistanceSqr = DistanceSquared2D(aimScreen, cross);
        aimDrawItems.push_back({ &spot, aimScreen, crossDistanceSqr });

        if (crossDistanceSqr < minCrossDistanceSqr)
        {
            minCrossDistanceSqr = crossDistanceSqr;
            closestSpot = &spot;
        }
    }

    if (standDrawItems.empty() && aimDrawItems.empty())
    {
        m_LastGrenadeSelectedSpotId = 0;
        return;
    }

    const bool focusByCrosshair = !aimDrawItems.empty() && minCrossDistanceSqr <= focusRadiusSqr;
    const GrenadeSpot* selectedSpot = closestSpot;
    if (!selectedSpot && !standDrawItems.empty())
        selectedSpot = standDrawItems.front().Spot;

    outHelper.Valid = true;
    outHelper.Cross = cross;
    outHelper.SelectedSpotId = selectedSpot ? selectedSpot->Id : 0;

    if (config.Visuals.GrenadeHelperDrawStand)
    {
        struct StandCluster
        {
            Vector2 Center{};
            std::string Text{};
            int Count = 0;
        };

        auto appendStandText = [](StandCluster& cluster, const GrenadeSpot* spot)
        {
            if (!spot)
                return;

            std::string line = spot->Name;
            if (line.empty())
                return;

            if (!cluster.Text.empty())
                cluster.Text += "\n";
            cluster.Text += line;
        };

        std::vector<StandCluster> clusters{};
        const float clusterRadius = std::clamp(focusRadius * 0.65f, 14.0f, 34.0f);
        const float clusterRadiusSqr = clusterRadius * clusterRadius;
        clusters.reserve(standDrawItems.size());

        for (const StandDrawItem& item : standDrawItems)
        {
            if (focusByCrosshair && item.Spot != closestSpot)
                continue;

            int bestCluster = -1;
            float bestDistSqr = clusterRadiusSqr;
            for (int i = 0; i < static_cast<int>(clusters.size()); ++i)
            {
                const float distSqr = DistanceSquared2D(item.Screen, clusters[i].Center);
                if (distSqr <= bestDistSqr)
                {
                    bestDistSqr = distSqr;
                    bestCluster = i;
                }
            }

            if (bestCluster >= 0)
            {
                StandCluster& cluster = clusters[bestCluster];
                const int oldCount = cluster.Count;
                cluster.Count += 1;
                cluster.Center.x = (cluster.Center.x * oldCount + item.Screen.x) / static_cast<float>(cluster.Count);
                cluster.Center.y = (cluster.Center.y * oldCount + item.Screen.y) / static_cast<float>(cluster.Count);
                appendStandText(cluster, item.Spot);
                continue;
            }

            StandCluster cluster{};
            cluster.Center = item.Screen;
            cluster.Count = 1;
            appendStandText(cluster, item.Spot);
            clusters.push_back(std::move(cluster));
        }

        outHelper.StandItems.reserve(clusters.size());
        for (const StandCluster& cluster : clusters)
        {
            GrenadeStandRenderItem renderItem{};
            renderItem.Screen = cluster.Center;
            renderItem.Label = cluster.Text;
            outHelper.StandItems.push_back(std::move(renderItem));
        }
    }

    outHelper.AimItems.reserve(aimDrawItems.size());
    for (const AimDrawItem& item : aimDrawItems)
    {
        if (focusByCrosshair && item.Spot != closestSpot)
            continue;

        GrenadeAimRenderItem renderItem{};
        renderItem.Screen = item.Screen;
        renderItem.IsTarget = (selectedSpot && item.Spot == selectedSpot);
        renderItem.DrawGuide = (focusByCrosshair && renderItem.IsTarget) ||
            (!focusByCrosshair && renderItem.IsTarget && item.CrossDistanceSqr <= looseGuideDistanceSqr);

        renderItem.Label = item.Spot->Name;
        outHelper.AimItems.push_back(std::move(renderItem));
    }

    if (selectedSpot && focusByCrosshair)
    {
        std::string remark = TrimAscii(selectedSpot->Remark);
        const std::string throwType = NormalizeThrowType(
            selectedSpot->ThrowType.empty() ? std::string("LB") : selectedSpot->ThrowType,
            selectedSpot->Name,
            &remark
        );
        outHelper.TopHintTokens = BuildThrowHintTokens(ParseThrowTypeToKeys(throwType));
        outHelper.TopHintRemark = remark;
    }

    m_LastGrenadeSelectedSpotId = outHelper.SelectedSpotId;
}

std::string ESP::BuildMapStatus(const std::string& mapName, const char* suffix) const
{
    std::string status = Localization::Pick("Map Status: ", "地图状态 ");
    status += mapName.empty() ? Localization::Pick("(Unknown)", "(未知)") : (mapName + ".opt");
    status += " (";
    status += Localization::Localize(suffix);
    status += ")";
    return status;
}

bool ESP::CheckVisibility(const Vector3& src, const Vector3& dst) const
{
    if (!m_VisCheck)
        return false;

    constexpr float maxDistance = 5000.0f;
    constexpr float maxDistanceSqr = maxDistance * maxDistance;
    if (DistanceSquared3D(src, dst) > maxDistanceSqr)
        return false;

    const auto start = std::chrono::steady_clock::now();
    const bool isVisible = m_VisCheck->IsPointVisible(src, dst);
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    if (durationUs >= 0)
        PerfDebug::RecordVisCheck(static_cast<std::uint64_t>(durationUs), isVisible);

    return isVisible;
}

bool ESP::IsPawnVisibleCached(const uint64_t pawn) const
{
    if (!pawn)
        return false;

    std::lock_guard lock(m_RenderFrameMutex);
    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        if (player.Pawn == pawn)
            return player.IsVisible;
    }

    return false;
}

std::unordered_set<uint64_t> ESP::GetVisiblePawnSetSnapshot() const
{
    std::unordered_set<uint64_t> visiblePawns{};

    std::lock_guard lock(m_RenderFrameMutex);
    visiblePawns.reserve(m_RenderFrame.Players.size());
    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        if (player.Pawn != 0 && player.IsVisible)
            visiblePawns.insert(player.Pawn);
    }

    return visiblePawns;
}

std::vector<TriggerBoneSnapshot> ESP::GetTriggerBoneSnapshots() const
{
    std::vector<TriggerBoneSnapshot> snapshots{};

    auto slotFromBoneId = [](const int boneId) -> int
    {
        for (std::size_t i = 0; i < kTrackedBones.size(); ++i)
        {
            if (kTrackedBones[i] == boneId)
                return static_cast<int>(i);
        }

        return -1;
    };

    std::lock_guard lock(m_RenderFrameMutex);
    snapshots.reserve(m_RenderFrame.Players.size());

    for (const PlayerEspSnapshot& player : m_RenderFrame.Players)
    {
        TriggerBoneSnapshot snapshot{};
        snapshot.Pawn = player.Pawn;
        snapshot.Team = player.Team;
        snapshot.Health = player.Health;
        snapshot.LifeState = player.LifeState;
        snapshot.IsVisible = player.IsVisible;
        snapshot.BoxMin = player.BoxMin;
        snapshot.BoxMax = player.BoxMax;

        for (const BonePoint& bone : player.Bones)
        {
            const int slot = slotFromBoneId(bone.Index);
            if (slot < 0 || static_cast<std::size_t>(slot) >= TriggerBoneSnapshot::BoneCount)
                continue;

            snapshot.Bones[static_cast<std::size_t>(slot)] = bone;
            snapshot.BoneValid[static_cast<std::size_t>(slot)] = bone.OnScreen;
        }

        snapshots.push_back(std::move(snapshot));
    }

    return snapshots;
}

std::string ESP::GetSuggestedGrenadeMapName() const
{
    const std::string polledMap = NormalizeMapName(m_LastPolledMapName.empty() ? sdk.GetCurrentMapName() : m_LastPolledMapName);
    if (!polledMap.empty())
        return polledMap;

    std::lock_guard lock(m_GrenadeMutex);
    if (!m_LoadedGrenadeMap.empty())
        return m_LoadedGrenadeMap;

    return "de_dust2";
}

std::string ESP::GetGrenadeStatus() const
{
    std::lock_guard lock(m_GrenadeMutex);
    return m_GrenadeStatus;
}

std::vector<GrenadeSpotEditorRow> ESP::GetGrenadeSpotEditorRows() const
{
    std::vector<GrenadeSpotEditorRow> rows{};
    std::lock_guard lock(m_GrenadeMutex);
    rows.reserve(m_GrenadeMap.Spots.size());
    for (const GrenadeSpot& spot : m_GrenadeMap.Spots)
    {
        GrenadeSpotEditorRow row{};
        row.Id = spot.Id;
        row.TypeIndex = (std::max)(0, GrenadeTypeIndexByLabel(spot.Type));
        row.ThrowType = NormalizeThrowType(spot.ThrowType, spot.Name, nullptr);
        row.Remark = TrimAscii(spot.Remark);
        row.Name = spot.Name;
        row.StandPos = spot.StandPos;
        row.AimPos = spot.AimPos;
        rows.push_back(std::move(row));
    }
    return rows;
}

bool ESP::ReloadGrenadeSpots(const std::string& mapName, std::string& outStatus)
{
    return ReloadGrenadeMapFromDisk(mapName, outStatus);
}

bool ESP::SaveGrenadeSpotEditorRows(const std::string& mapName, const std::vector<GrenadeSpotEditorRow>& rows, std::string& outStatus)
{
    const std::string normalizedMap = NormalizeMapName(mapName);
    if (normalizedMap.empty())
    {
        outStatus = Localization::Pick("Save failed: empty map name", "保存失败：空地图名");
        return false;
    }

    GrenadeMapData mapData{};
    mapData.MapName = normalizedMap;
    mapData.Spots.reserve(rows.size());

    std::unordered_set<int> usedIds{};
    usedIds.reserve(rows.size() * 2 + 1);
    int nextId = 1;

    for (const GrenadeSpotEditorRow& row : rows)
    {
        int id = row.Id;
        if (id <= 0 || usedIds.contains(id))
        {
            while (usedIds.contains(nextId))
                ++nextId;
            id = nextId++;
        }
        usedIds.insert(id);

        GrenadeSpot spot{};
        spot.Id = id;
        spot.Type = GrenadeTypeLabelByIndex(std::clamp(row.TypeIndex, 0, 4));
        spot.Name = row.Name.empty() ? std::string("Unnamed") : row.Name;
        spot.Remark = TrimAscii(row.Remark);
        std::string bracketRemark{};
        ExtractTrailingBracketRemark(spot.Name, bracketRemark);
        MergeRemarkSegment(spot.Remark, bracketRemark);
        if (spot.Name.empty())
            spot.Name = "Unnamed";
        spot.ThrowType = NormalizeThrowType(
            row.ThrowType.empty() ? std::string("LB") : row.ThrowType,
            spot.Name,
            &spot.Remark
        );
        spot.StandPos = row.StandPos;
        spot.AimPos = row.AimPos;
        mapData.Spots.push_back(std::move(spot));
    }

    std::sort(mapData.Spots.begin(), mapData.Spots.end(), [](const GrenadeSpot& a, const GrenadeSpot& b)
    {
        return a.Id < b.Id;
    });

    const std::string filePath = ResolveGrenadeDataWritePath(normalizedMap);
    std::string error{};
    if (!SaveGrenadeMapFile(filePath, mapData, error))
    {
        outStatus = std::string(Localization::Pick("Save failed: ", "保存失败：")) + error;
        return false;
    }

    if (!ReloadGrenadeMapFromDisk(normalizedMap, outStatus))
        return false;

    outStatus = Localization::Pick("Spot list saved and reloaded", "点位列表保存成功并刷新");
    {
        std::lock_guard lock(m_GrenadeMutex);
        m_GrenadeStatus = outStatus;
    }
    return true;
}

bool ESP::RecordCurrentGrenadeSpot(
    const std::string& mapName,
    const std::string& spotName,
    const std::string& throwType,
    const std::string& remark,
    float recordDistance,
    bool manualTypeOverride,
    int manualTypeIndex,
    std::string& outStatus)
{
    std::string normalizedMap = NormalizeMapName(mapName);
    if (normalizedMap.empty())
        normalizedMap = GetSuggestedGrenadeMapName();
    if (normalizedMap.empty())
        normalizedMap = "de_dust2";

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn)
    {
        outStatus = Localization::Pick("Record failed: local player unavailable", "记录失败：本地玩家不可用");
        return false;
    }

    struct LocalRecordFields
    {
        Vector3 Origin{};
        Vector3 ViewOffset{};
    } local{};

    const auto scatter = mem.CreateScatterHandle();
    if (!scatter)
    {
        outStatus = Localization::Pick("Record failed: scatter allocation failed", "记录失败：Scatter分配失败");
        return false;
    }

    if (Offsets::Schema::m_vOldOrigin)
        mem.AddScatterReadRequest(scatter, core.LocalPawn + Offsets::Schema::m_vOldOrigin, &local.Origin, sizeof(local.Origin));
    if (Offsets::Schema::m_vecViewOffset)
        mem.AddScatterReadRequest(scatter, core.LocalPawn + Offsets::Schema::m_vecViewOffset, &local.ViewOffset, sizeof(local.ViewOffset));

    mem.ExecuteReadScatter(scatter);
    mem.CloseScatterHandle(scatter);

    Vector3 viewAngles{};
    if (Globals::ClientBase && Offsets::Client::dwViewAngles)
        mem.Read(Globals::ClientBase + Offsets::Client::dwViewAngles, &viewAngles, sizeof(viewAngles));

    const std::string grenadeType = manualTypeOverride
        ? GrenadeTypeLabelByIndex(std::clamp(manualTypeIndex, 0, 4))
        : ReadGrenadeType(core.LocalPawn);
    if (!IsUtilityGrenadeType(grenadeType))
    {
        outStatus = Localization::Pick("Record failed: current held item is not a utility grenade", "记录失败：目前手持非道具");
        return false;
    }

    Vector3 eyePos = local.Origin + Vector3{ 0.0f, 0.0f, 64.0f };
    if (std::abs(local.ViewOffset.x) > 0.001f || std::abs(local.ViewOffset.y) > 0.001f || std::abs(local.ViewOffset.z) > 0.001f)
        eyePos = local.Origin + local.ViewOffset;

    const float pitchRad = viewAngles.x * (math::PI / 180.0f);
    const float yawRad = viewAngles.y * (math::PI / 180.0f);
    const Vector3 forward{
        std::cos(pitchRad) * std::cos(yawRad),
        std::cos(pitchRad) * std::sin(yawRad),
        -std::sin(pitchRad)
    };

    const float finalDistance = std::clamp(recordDistance, 500.0f, 50000.0f);
    const Vector3 aimPos = eyePos + forward * finalDistance;

    std::string reloadStatus{};
    ReloadGrenadeMapFromDisk(normalizedMap, reloadStatus);

    GrenadeMapData mapData{};
    {
        std::lock_guard lock(m_GrenadeMutex);
        mapData = m_GrenadeMap;
    }
    if (NormalizeMapName(mapData.MapName) != normalizedMap)
    {
        mapData = {};
        mapData.MapName = normalizedMap;
    }

    int nextId = 1;
    for (const GrenadeSpot& spot : mapData.Spots)
        nextId = (std::max)(nextId, spot.Id + 1);

    GrenadeSpot newSpot{};
    newSpot.Id = nextId;
    newSpot.Type = grenadeType;
    newSpot.Name = spotName.empty() ? std::string("Unnamed") : spotName;
    newSpot.Remark = TrimAscii(remark);
    std::string bracketRemark{};
    ExtractTrailingBracketRemark(newSpot.Name, bracketRemark);
    MergeRemarkSegment(newSpot.Remark, bracketRemark);
    if (newSpot.Name.empty())
        newSpot.Name = "Unnamed";
    newSpot.ThrowType = NormalizeThrowType(
        throwType.empty() ? std::string("LB") : throwType,
        newSpot.Name,
        &newSpot.Remark
    );
    newSpot.StandPos = local.Origin;
    newSpot.AimPos = aimPos;
    mapData.Spots.push_back(std::move(newSpot));

    const std::string filePath = ResolveGrenadeDataWritePath(normalizedMap);
    std::string writeError{};
    if (!SaveGrenadeMapFile(filePath, mapData, writeError))
    {
        outStatus = std::string(Localization::Pick("Record failed: ", "记录失败：")) + writeError;
        return false;
    }

    std::string afterReloadStatus{};
    if (!ReloadGrenadeMapFromDisk(normalizedMap, afterReloadStatus))
    {
        outStatus = std::string(Localization::Pick("Record saved, but reload failed: ", "记录成功但刷新失败")) + afterReloadStatus;
        return false;
    }

    outStatus = std::string(Localization::Pick("Recorded spot: ", "记录点位：")) + mapData.Spots.back().Name;
    {
        std::lock_guard lock(m_GrenadeMutex);
        m_GrenadeStatus = outStatus;
    }
    return true;
}

bool ESP::DetectCurrentGrenadeTypeIndex(int& outTypeIndex) const
{
    outTypeIndex = -1;

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn)
        return false;

    const int index = GrenadeTypeIndexByLabel(ReadGrenadeType(core.LocalPawn));
    if (index < 0)
        return false;

    outTypeIndex = index;
    return true;
}

int ESP::GetCurrentGrenadeFocusedSpotId() const
{
    std::lock_guard lock(m_RenderFrameMutex);
    return m_RenderFrame.GrenadeHelper.SelectedSpotId;
}

void ESP::EnsureSamplerStarted()
{
    bool expected = false;
    if (!m_SamplerStarted.compare_exchange_strong(expected, true))
        return;

    std::thread([this]()
    {
        SamplerLoop();
    }).detach();
}

void ESP::UpdateRoundEpoch(const uint64_t localPawn, const bool localAlive)
{
    const auto now = std::chrono::steady_clock::now();
    if (m_LastRoundEpochTick.time_since_epoch().count() == 0)
        m_LastRoundEpochTick = now;

    bool shouldAdvanceEpoch = false;
    if (localPawn && m_LastRoundLocalPawn && localPawn != m_LastRoundLocalPawn)
    {
        shouldAdvanceEpoch = true;
    }
    else if (localAlive && !m_LastRoundLocalAlive)
    {
        if (now - m_LastRoundEpochTick > std::chrono::seconds(8))
            shouldAdvanceEpoch = true;
    }

    if (shouldAdvanceEpoch)
    {
        ++m_RoundEpoch;
        m_LastRoundEpochTick = now;
        m_PawnRuntimeCache.clear();
        m_LastFreezePeriod = false;
        m_LastFreezeEndTick = {};
    }

    if (localPawn)
        m_LastRoundLocalPawn = localPawn;
    m_LastRoundLocalAlive = localAlive;
}

void ESP::SamplerLoop()
{
    constexpr auto kSampleIntervalIdle = std::chrono::milliseconds(4); // 250Hz
    constexpr auto kSampleIntervalHot = std::chrono::milliseconds(2);  // 500Hz
    constexpr auto kHelperIdleInterval = std::chrono::milliseconds(17); // ~60Hz
    constexpr auto kHelperHotInterval = std::chrono::milliseconds(7);   // ~144Hz
    constexpr auto kOverrunYield = std::chrono::milliseconds(1);

    while (Globals::Running)
    {
        const auto cycleStart = std::chrono::steady_clock::now();

        RenderFrame sampledFrame{};
        sampledFrame.MapStatus = m_MapStatus;
        SampleFrame(sampledFrame);

        {
            std::lock_guard lock(m_RenderFrameMutex);
            std::swap(m_RenderFrame, sampledFrame);
        }

        const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - cycleStart
        ).count();
        if (sampleUs >= 0)
            PerfDebug::RecordEspSampleFrame(static_cast<std::uint64_t>(sampleUs));

        const bool boneTriggerHot =
            config.Aim.Trigger &&
            std::clamp(config.Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1) == Structs::TriggerDetect_BoneHitbox &&
            aim.IsTriggerHotkeyActiveVisual();
        const bool helperOnlyMode = m_GrenadeHelperOnlyMode.load(std::memory_order_relaxed);
        const bool helperHoldingUtility = m_GrenadeHelperHoldingUtility.load(std::memory_order_relaxed);
        const auto targetInterval = helperOnlyMode
            ? (helperHoldingUtility ? kHelperHotInterval : kHelperIdleInterval)
            : (boneTriggerHot ? kSampleIntervalHot : kSampleIntervalIdle);
        const auto elapsed = std::chrono::steady_clock::now() - cycleStart;
        if (elapsed < targetInterval)
            std::this_thread::sleep_for(targetInterval - elapsed);
        else
            std::this_thread::sleep_for(kOverrunYield);
    }
}

bool ESP::SampleFrame(RenderFrame& outFrame)
{
    UpdateVisCheckState();
    outFrame.MapStatus = m_MapStatus;

    const bool needVisibilityChecks = config.Aim.AimVisible || config.Visuals.VisibleCheck;
    const bool needTriggerBoneSampling =
        config.Aim.Trigger &&
        std::clamp(config.Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1) == Structs::TriggerDetect_BoneHitbox;
    const bool needEspSampling = config.Visuals.Enabled || needVisibilityChecks || needTriggerBoneSampling;
    const bool needGrenadeHelperSampling = config.Visuals.GrenadeHelper;
    m_GrenadeHelperOnlyMode.store(needGrenadeHelperSampling && !needEspSampling, std::memory_order_relaxed);
    if (!needGrenadeHelperSampling)
        m_GrenadeHelperHoldingUtility.store(false, std::memory_order_relaxed);
    if (!needEspSampling && !needGrenadeHelperSampling)
        return true;

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn || !core.EntityList)
        return true;

    std::string heldGrenadeType = "Unknown";
    if (needGrenadeHelperSampling)
        heldGrenadeType = ReadGrenadeType(core.LocalPawn);

    const bool isHoldingUtility = IsUtilityGrenadeType(heldGrenadeType);
    m_GrenadeHelperHoldingUtility.store(needGrenadeHelperSampling && isHoldingUtility, std::memory_order_relaxed);

    if (needGrenadeHelperSampling && !isHoldingUtility)
    {
        outFrame.GrenadeHelper = {};
        m_LastGrenadeSelectedSpotId = 0;
        if (!needEspSampling)
            return true;
    }

    struct LocalFields
    {
        int Team = 0;
        int Health = 0;
        int LifeState = 0;
        Vector3 Origin{};
        Vector3 ViewOffset{};
    } local{};

    const auto localScatter = mem.CreateScatterHandle();
    if (!localScatter)
        return false;

    if (Offsets::Schema::m_iTeamNum)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iTeamNum, &local.Team, sizeof(local.Team));
    if (Offsets::Schema::m_iHealth)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iHealth, &local.Health, sizeof(local.Health));
    if (Offsets::Schema::m_lifeState)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_lifeState, &local.LifeState, sizeof(local.LifeState));
    if (Offsets::Schema::m_vOldOrigin)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vOldOrigin, &local.Origin, sizeof(local.Origin));
    if (Offsets::Schema::m_vecViewOffset)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vecViewOffset, &local.ViewOffset, sizeof(local.ViewOffset));

    mem.ExecuteReadScatter(localScatter);
    mem.CloseScatterHandle(localScatter);

    Vector3 localViewOffset = { 0.0f, 0.0f, 64.0f };
    if (std::abs(local.ViewOffset.x) > 0.001f || std::abs(local.ViewOffset.y) > 0.001f || std::abs(local.ViewOffset.z) > 0.001f)
        localViewOffset = local.ViewOffset;

    const Vector3 localEyePosition = local.Origin + localViewOffset;
    Vector3 localViewAngles{};
    if (Globals::ClientBase && Offsets::Client::dwViewAngles)
        mem.Read(Globals::ClientBase + Offsets::Client::dwViewAngles, &localViewAngles, sizeof(localViewAngles));
    const int localTeam = local.Team;

    if (needGrenadeHelperSampling && isHoldingUtility)
    {
        BuildGrenadeHelperSnapshot(
            local.Origin,
            localEyePosition,
            localViewAngles,
            core.LocalPawn,
            heldGrenadeType,
            outFrame.GrenadeHelper
        );
    }
    else
    {
        outFrame.GrenadeHelper = {};
        m_LastGrenadeSelectedSpotId = 0;
    }

    if (!needEspSampling)
        return true;

    UpdateRoundEpoch(core.LocalPawn, IsAlive(local.Health, local.LifeState));
    const auto now = std::chrono::steady_clock::now();

    bool freezePeriod = false;
    bool freezeValid = false;
    if (Offsets::Client::dwGameRules && Offsets::Schema::m_bFreezePeriod)
    {
        const uint64_t gameRules = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGameRules);
        if (IsLikelyUserAddress(gameRules))
        {
            freezePeriod = mem.Read<bool>(gameRules + Offsets::Schema::m_bFreezePeriod);
            freezeValid = true;
        }
    }

    if (freezeValid)
    {
        if (freezePeriod)
        {
            if (!m_LastFreezePeriod &&
                (m_LastRoundEpochTick.time_since_epoch().count() == 0 ||
                    now - m_LastRoundEpochTick > std::chrono::seconds(8)))
            {
                ++m_RoundEpoch;
                m_LastRoundEpochTick = now;
                m_PawnRuntimeCache.clear();
            }

            m_LastFreezePeriod = true;
        }
        else if (m_LastFreezePeriod)
        {
            m_LastFreezePeriod = false;
            m_LastFreezeEndTick = now;
        }
    }

    const uint64_t controllerChunk = mem.Read<uint64_t>(core.EntityList + Offsets::EntityList::ListStart);
    if (!IsLikelyUserAddress(controllerChunk))
        return true;

    std::array<uint64_t, kMaxControllers + 1> controllerPointers{};
    if (const auto controllerScatter = mem.CreateScatterHandle())
    {
        for (int index = 1; index <= kMaxControllers; ++index)
        {
            const uint64_t entryAddress = controllerChunk + static_cast<uint64_t>(index) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(controllerScatter, entryAddress, &controllerPointers[index], sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(controllerScatter);
        mem.CloseScatterHandle(controllerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData> entities{};
    entities.reserve(kMaxControllers);

    for (int index = 1; index <= kMaxControllers; ++index)
    {
        const uint64_t controller = controllerPointers[index];
        if (!IsLikelyUserAddress(controller))
            continue;

        SampledEntityData data{};
        data.HandleIndex = index;
        data.Controller = controller;
        entities.push_back(data);
    }

    outFrame.ResolvedControllers = static_cast<std::uint32_t>(entities.size());
    if (entities.empty())
        return true;

    const uint32_t pawnHandlePrimaryOffset = Offsets::Schema::m_hPlayerPawn ? Offsets::Schema::m_hPlayerPawn : Offsets::Schema::m_hPawn;
    const uint32_t pawnHandleFallbackOffset = (Offsets::Schema::m_hPlayerPawn && Offsets::Schema::m_hPawn)
        ? Offsets::Schema::m_hPawn
        : 0;

    if (!pawnHandlePrimaryOffset)
        return true;

    std::vector<uint32_t> fallbackPawnHandles(entities.size(), 0);
    if (const auto pawnHandleScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            mem.AddScatterReadRequest(
                pawnHandleScatter,
                entities[i].Controller + pawnHandlePrimaryOffset,
                &entities[i].PawnHandle,
                sizeof(uint32_t)
            );

            if (pawnHandleFallbackOffset)
            {
                mem.AddScatterReadRequest(
                    pawnHandleScatter,
                    entities[i].Controller + pawnHandleFallbackOffset,
                    &fallbackPawnHandles[i],
                    sizeof(uint32_t)
                );
            }
        }

        mem.ExecuteReadScatter(pawnHandleScatter);
        mem.CloseScatterHandle(pawnHandleScatter);
    }
    else
    {
        return false;
    }

    std::vector<uint32_t> decodedHi(entities.size(), 0);
    std::vector<uint32_t> decodedLo(entities.size(), 0);
    std::unordered_set<uint32_t> uniqueHi{};

    for (size_t i = 0; i < entities.size(); ++i)
    {
        if (!entities[i].PawnHandle && pawnHandleFallbackOffset)
            entities[i].PawnHandle = fallbackPawnHandles[i];

        const uint32_t handleIndex = entities[i].PawnHandle & Offsets::EntityList::HandleMask;
        if (!handleIndex)
            continue;

        const uint32_t hi = handleIndex >> Offsets::EntityList::HandleHighShift;
        const uint32_t lo = handleIndex & Offsets::EntityList::HandleLowMask;
        decodedHi[i] = hi;
        decodedLo[i] = lo;
        uniqueHi.insert(hi);
    }

    std::unordered_map<uint32_t, uint64_t> chunkPointers{};
    chunkPointers.reserve(uniqueHi.size());
    for (const uint32_t hi : uniqueHi)
        chunkPointers.emplace(hi, 0ULL);

    if (!chunkPointers.empty())
    {
        if (const auto chunkScatter = mem.CreateScatterHandle())
        {
            for (auto& [hi, chunkPointer] : chunkPointers)
            {
                const uint64_t chunkAddress = core.EntityList + Offsets::EntityList::ListStart + static_cast<uint64_t>(hi) * Offsets::EntityList::ChunkStride;
                mem.AddScatterReadRequest(chunkScatter, chunkAddress, &chunkPointer, sizeof(uint64_t));
            }

            mem.ExecuteReadScatter(chunkScatter);
            mem.CloseScatterHandle(chunkScatter);
        }
        else
        {
            return false;
        }
    }

    if (const auto pawnPointerScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (!(entities[i].PawnHandle & Offsets::EntityList::HandleMask))
                continue;

            const auto chunkIt = chunkPointers.find(decodedHi[i]);
            if (chunkIt == chunkPointers.end())
                continue;

            const uint64_t chunkPointer = chunkIt->second;
            if (!IsLikelyUserAddress(chunkPointer))
                continue;

            const uint64_t pawnAddress = chunkPointer + static_cast<uint64_t>(decodedLo[i]) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(pawnPointerScatter, pawnAddress, &entities[i].Pawn, sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(pawnPointerScatter);
        mem.CloseScatterHandle(pawnPointerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData*> activeEntities{};
    activeEntities.reserve(entities.size());
    for (SampledEntityData& entity : entities)
    {
        if (!IsLikelyUserAddress(entity.Pawn))
            continue;
        if (entity.Pawn == core.LocalPawn)
            continue;
        activeEntities.push_back(&entity);
    }

    if (activeEntities.empty())
        return true;

    if (const auto pawnFieldScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (Offsets::Schema::m_iHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iHealth, &entity->Health, sizeof(entity->Health));
            if (Offsets::Schema::m_iTeamNum)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iTeamNum, &entity->Team, sizeof(entity->Team));
            if (Offsets::Schema::m_lifeState)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_lifeState, &entity->LifeState, sizeof(entity->LifeState));
            if (Offsets::Schema::m_iMaxHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iMaxHealth, &entity->MaxHealth, sizeof(entity->MaxHealth));
            if (Offsets::Schema::m_pGameSceneNode)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_pGameSceneNode, &entity->SceneNode, sizeof(entity->SceneNode));
            if (Offsets::Schema::m_vOldOrigin)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vOldOrigin, &entity->OldOrigin, sizeof(entity->OldOrigin));
            if (Offsets::Schema::m_vecViewOffset)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vecViewOffset, &entity->ViewOffset, sizeof(entity->ViewOffset));

            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            entity->RefreshStatus = runtimeCache.LastStatusRead.time_since_epoch().count() == 0 ||
                now - runtimeCache.LastStatusRead >= std::chrono::milliseconds(150);

            if (!entity->RefreshStatus)
            {
                entity->Armor = runtimeCache.Armor;
                entity->IsScoped = runtimeCache.IsScoped;
                entity->HasDefuser = runtimeCache.HasDefuser;
                entity->FlashDuration = runtimeCache.FlashDuration;
            }
            else
            {
                if (Offsets::Schema::m_ArmorValue)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_ArmorValue, &entity->Armor, sizeof(entity->Armor));
                if (Offsets::Schema::m_bIsScoped)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bIsScoped, &entity->IsScoped, sizeof(entity->IsScoped));
                if (Offsets::Schema::m_bHasDefuser)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bHasDefuser, &entity->HasDefuser, sizeof(entity->HasDefuser));
                if (Offsets::Schema::m_flFlashDuration)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_flFlashDuration, &entity->FlashDuration, sizeof(entity->FlashDuration));
            }
        }

        mem.ExecuteReadScatter(pawnFieldScatter);
        mem.CloseScatterHandle(pawnFieldScatter);
    }
    else
    {
        return false;
    }

    for (SampledEntityData* entity : activeEntities)
    {
        if (std::abs(entity->ViewOffset.x) <= 0.001f && std::abs(entity->ViewOffset.y) <= 0.001f && std::abs(entity->ViewOffset.z) <= 0.001f)
            entity->ViewOffset = { 0.0f, 0.0f, 64.0f };

        if (entity->RefreshStatus)
        {
            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            runtimeCache.Armor = (std::max)(0, entity->Armor);
            runtimeCache.IsScoped = entity->IsScoped;
            runtimeCache.HasDefuser = entity->HasDefuser;
            runtimeCache.FlashDuration = (std::max)(0.0f, entity->FlashDuration);
            runtimeCache.LastStatusRead = now;
        }

        if (!IsLikelyUserAddress(entity->SceneNode))
            entity->SceneNode = 0;
    }

    if (const auto sceneScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (!entity->SceneNode)
                continue;

            if (Offsets::Schema::m_vecAbsOrigin)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_vecAbsOrigin,
                    &entity->AbsOrigin,
                    sizeof(entity->AbsOrigin)
                );
                entity->HasAbsOrigin = true;
            }

            if (Offsets::Schema::m_modelState)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_modelState + Offsets::Layout::BoneArray,
                    &entity->BoneArray,
                    sizeof(entity->BoneArray)
                );
            }
        }

        mem.ExecuteReadScatter(sceneScatter);
        mem.CloseScatterHandle(sceneScatter);
    }
    else
    {
        return false;
    }

    outFrame.Players.clear();
    outFrame.Players.reserve(activeEntities.size());

    std::unordered_set<uint64_t> activeControllers{};
    std::unordered_set<uint64_t> activePawns{};
    activeControllers.reserve(activeEntities.size());
    activePawns.reserve(activeEntities.size());

    constexpr auto kMoneyPollInterval = std::chrono::seconds(1);
    constexpr auto kPostFreezeDuration = std::chrono::seconds(20);
    constexpr auto kEstimatedFreezeDuration = std::chrono::seconds(15); // fallback when freeze flag is unavailable.
    bool moneyWindowActive = false;
    if (m_LastRoundEpochTick.time_since_epoch().count() != 0)
    {
        if (freezeValid)
        {
            moneyWindowActive = freezePeriod ||
                (m_LastFreezeEndTick.time_since_epoch().count() != 0 &&
                    now - m_LastFreezeEndTick <= kPostFreezeDuration);
        }
        else
        {
            const auto roundElapsed = now - m_LastRoundEpochTick;
            moneyWindowActive = roundElapsed <= (kEstimatedFreezeDuration + kPostFreezeDuration);
        }
    }

    for (SampledEntityData* entity : activeEntities)
    {
        if (!IsAlive(entity->Health, entity->LifeState))
            continue;

        if (config.Visuals.TeamCheck && !config.Aim.AimFriendly && localTeam > 0 && entity->Team == localTeam)
            continue;

        activeControllers.insert(entity->Controller);
        activePawns.insert(entity->Pawn);

        ControllerIdentityCache& identityCache = m_ControllerIdentityCache[entity->Controller];
        if (identityCache.RoundEpoch != m_RoundEpoch)
        {
            identityCache.Name = ReadPlayerName(entity->Controller);
            identityCache.RoundEpoch = m_RoundEpoch;
            if (moneyWindowActive)
            {
                identityCache.Money = ReadMoney(entity->Controller);
                identityCache.LastMoneyRead = now;
            }
        }
        else if (moneyWindowActive &&
            (identityCache.LastMoneyRead.time_since_epoch().count() == 0 ||
                now - identityCache.LastMoneyRead >= kMoneyPollInterval))
        {
            identityCache.Money = ReadMoney(entity->Controller);
            identityCache.LastMoneyRead = now;
        }

        PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
        if (runtimeCache.WeaponName.empty() ||
            runtimeCache.LastWeaponRead.time_since_epoch().count() == 0 ||
            now - runtimeCache.LastWeaponRead >= std::chrono::milliseconds(500))
        {
            runtimeCache.WeaponName = ReadWeaponName(entity->Pawn);
            runtimeCache.LastWeaponRead = now;
        }

        PlayerEspSnapshot snapshot{};
        snapshot.Controller = entity->Controller;
        snapshot.Pawn = entity->Pawn;
        snapshot.SceneNode = entity->SceneNode;
        snapshot.BoneArray = IsLikelyUserAddress(entity->BoneArray) ? entity->BoneArray : 0;

        snapshot.Health = entity->Health;
        snapshot.MaxHealth = entity->MaxHealth > 0 ? entity->MaxHealth : 100;
        snapshot.Team = entity->Team;
        snapshot.LifeState = entity->LifeState;

        snapshot.Armor = runtimeCache.Armor;
        snapshot.IsScoped = runtimeCache.IsScoped;
        snapshot.HasDefuser = runtimeCache.HasDefuser;
        snapshot.FlashDuration = runtimeCache.FlashDuration;

        snapshot.Origin = entity->HasAbsOrigin ? entity->AbsOrigin : entity->OldOrigin;
        snapshot.EyePosition = snapshot.Origin + entity->ViewOffset;

        snapshot.Name = identityCache.Name;
        snapshot.Money = identityCache.Money;
        snapshot.ShowMoney = moneyWindowActive;
        snapshot.WeaponName = runtimeCache.WeaponName;

        bool hasBoxData = false;
        const bool needBoneData = (config.Visuals.Bones || config.Aim.TriggerHitboxDebug || needTriggerBoneSampling) && snapshot.BoneArray;
        if (needBoneData)
            hasBoxData = BuildBoneData(snapshot.BoneArray, snapshot);

        if (!hasBoxData)
        {
            Vector2 screenHead{};
            Vector2 screenFeet{};
            snapshot.HeadPosition = snapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };

            if (!sdk.WorldToScreen(snapshot.HeadPosition, screenHead) || !sdk.WorldToScreen(snapshot.Origin, screenFeet))
                continue;

            const float boxHeight = std::abs(screenFeet.y - screenHead.y);
            if (boxHeight < 4.0f)
                continue;

            const float boxWidth = boxHeight * 0.45f;
            snapshot.BoxMin = ImVec2(screenFeet.x - boxWidth * 0.5f, screenHead.y);
            snapshot.BoxMax = ImVec2(screenFeet.x + boxWidth * 0.5f, screenFeet.y);
        }

        if (needVisibilityChecks && m_VisCheckEnabled)
            snapshot.IsVisible = CheckVisibility(localEyePosition, snapshot.HeadPosition);

        outFrame.Players.push_back(std::move(snapshot));
    }

    for (auto it = m_ControllerIdentityCache.begin(); it != m_ControllerIdentityCache.end();)
    {
        if (activeControllers.find(it->first) == activeControllers.end())
            it = m_ControllerIdentityCache.erase(it);
        else
            ++it;
    }

    for (auto it = m_PawnRuntimeCache.begin(); it != m_PawnRuntimeCache.end();)
    {
        if (activePawns.find(it->first) == activePawns.end())
            it = m_PawnRuntimeCache.erase(it);
        else
            ++it;
    }

    if (config.Visuals.C4)
    {
        constexpr auto kC4IntervalIdle = std::chrono::microseconds(16667);   // 60Hz
        constexpr auto kC4IntervalPlanted = std::chrono::microseconds(10000); // 100Hz

        const bool cacheValid = m_LastC4Sample.time_since_epoch().count() != 0;
        const auto targetInterval = m_C4Cache.Planted ? kC4IntervalPlanted : kC4IntervalIdle;

        if (!cacheValid || (now - m_LastC4Sample) >= targetInterval)
        {
            m_C4Cache = ReadC4Snapshot();
            m_LastC4Sample = now;
        }

        outFrame.C4 = m_C4Cache;
    }
    else
    {
        m_C4Cache = C4Snapshot{};
        m_LastC4Sample = {};
        outFrame.C4 = C4Snapshot{};
    }

    return true;
}

void ESP::Render(ImDrawList* drawList)
{
    const auto renderStart = std::chrono::steady_clock::now();
    EnsureSamplerStarted();

    std::uint32_t resolvedControllers = 0;
    std::uint32_t drawnPlayers = 0;
    const auto publishPerf = [&]()
    {
        const auto renderUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - renderStart
        ).count();

        if (renderUs >= 0)
        {
            PerfDebug::RecordEspFrame(
                static_cast<std::uint64_t>(renderUs),
                resolvedControllers,
                drawnPlayers
            );
        }
    };

    if (!drawList)
    {
        publishPerf();
        return;
    }

    const bool renderEsp = config.Visuals.Enabled;
    const bool renderGrenadeHelper = config.Visuals.GrenadeHelper;
    if (!renderEsp && !renderGrenadeHelper)
    {
        publishPerf();
        return;
    }

    RenderFrame frame{};
    {
        std::lock_guard lock(m_RenderFrameMutex);
        frame = m_RenderFrame;
    }

    resolvedControllers = frame.ResolvedControllers;
    drawnPlayers = static_cast<std::uint32_t>(frame.Players.size());

    if (renderEsp)
    {
        RenderWatermark(drawList);
        float leftHudY = config.Visuals.Watermark ? 34.0f : 12.0f;

    const ImVec2 statusPos(12.0f, leftHudY);
    const char* mapStatus = frame.MapStatus.empty() ? Localization::Pick("Map Status: (Waiting)", "地图状态：（等待中）") : frame.MapStatus.c_str();
    drawList->AddText(statusPos, IM_COL32(210, 210, 210, 255), mapStatus);
    leftHudY += ImGui::GetFontSize() + 2.0f;

    if (config.Aim.Aimbot || config.Aim.Trigger)
    {
        const bool aimbotHotkeyActive = aim.IsAimbotHotkeyActiveVisual();
        const bool aimbotHasTarget = aim.HasAimbotTargetVisual();
        const bool triggerHotkeyActive = aim.IsTriggerHotkeyActiveVisual();
        const bool triggerHasTarget = aim.HasTriggerTargetVisual();

        const char* aimbotState = Localization::Pick("OFF", "关闭");
        ImU32 aimbotColor = IM_COL32(180, 180, 180, 255);
        if (config.Aim.Aimbot)
        {
            aimbotState = Localization::Pick("READY", "就绪");
            aimbotColor = IM_COL32(220, 220, 220, 255);
            if (aimbotHotkeyActive)
            {
                aimbotState = Localization::Pick("HOTKEY", "热键触发");
                aimbotColor = IM_COL32(255, 220, 120, 255);
            }
            if (aimbotHasTarget)
            {
                aimbotState = Localization::Pick("LOCK", "锁定中");
                aimbotColor = IM_COL32(120, 255, 155, 255);
            }
        }

        const char* triggerState = Localization::Pick("OFF", "关闭");
        ImU32 triggerColor = IM_COL32(180, 180, 180, 255);
        if (config.Aim.Trigger)
        {
            triggerState = Localization::Pick("READY", "就绪");
            triggerColor = IM_COL32(220, 220, 220, 255);
            if (triggerHotkeyActive)
            {
                triggerState = Localization::Pick("HOTKEY", "热键触发");
                triggerColor = IM_COL32(255, 220, 120, 255);
            }
            if (triggerHasTarget)
            {
                triggerState = Localization::Pick("HIT", "击中");
                triggerColor = IM_COL32(120, 255, 155, 255);
            }
        }

        char aimbotStatusLine[64]{};
        char triggerStatusLine[64]{};
        std::snprintf(aimbotStatusLine, sizeof(aimbotStatusLine), Localization::Pick("Aimbot: %s", "自瞄: %s"), aimbotState);
        std::snprintf(triggerStatusLine, sizeof(triggerStatusLine), Localization::Pick("Trigger: %s", "扳机: %s"), triggerState);
        drawList->AddText(ImVec2(12.0f, leftHudY), aimbotColor, aimbotStatusLine);
        leftHudY += ImGui::GetFontSize() + 2.0f;
        drawList->AddText(ImVec2(12.0f, leftHudY), triggerColor, triggerStatusLine);
        leftHudY += ImGui::GetFontSize() + 2.0f;
    }

    if (config.Aim.Aimbot && config.Aim.DrawFov)
    {
        float radius = aim.GetCurrentFovRadiusPx();
        if (radius <= 0.01f)
            radius = (std::max)(2.0f, (config.Aim.WeaponProfiles[Structs::AimWeapon_Rifle].Fov / 180.0f) * ScreenCenter.x);

        drawList->AddCircle(
            ImVec2(ScreenCenter.x, ScreenCenter.y),
            radius,
            ToImColor(config.Aim.AimbotFovColor),
            96,
            1.3f
        );

        char fovText[64]{};
        const bool aimbotHasTarget = aim.HasAimbotTargetVisual();
        const bool aimbotHotkeyActive = aim.IsAimbotHotkeyActiveVisual();
        const char* aimState = aimbotHasTarget ? Localization::Pick("LOCK", "锁定") : (aimbotHotkeyActive ? Localization::Pick("HOTKEY", "热键") : Localization::Pick("IDLE", "空闲"));
        std::snprintf(fovText, sizeof(fovText), Localization::Pick("FOV %.1f px [%s]", "FOV %.1f 像素 [%s]"), radius, aimState);
        drawList->AddText(
            ImVec2(ScreenCenter.x + radius + 8.0f, ScreenCenter.y - ImGui::GetFontSize() * 0.5f),
            ToImColor(config.Aim.AimbotFovColor),
            fovText
        );
    }

    if (config.Aim.TriggerHitboxDebug)
    {
        const int detectMode = std::clamp(
            aim.GetCurrentTriggerDetectMode(),
            0,
            static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1
        );
        const float unifiedRadius = std::clamp(config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
        const float hitboxScale = std::clamp(config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
        const float hitboxAddPx = std::clamp(config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
        const float headBaseRadius = std::clamp(config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);
        const float bodyRadius = std::clamp(unifiedRadius * hitboxScale + hitboxAddPx, 0.5f, 80.0f);
        const float headRadius = std::clamp(headBaseRadius * hitboxScale + hitboxAddPx, 0.5f, 100.0f);
        char triggerDebugText[128]{};
        std::snprintf(
            triggerDebugText,
            sizeof(triggerDebugText),
            Localization::Pick("Trigger Debug: %s | Body %.1f px | Head %.1f px", "扳机调试: %s | 身体 %.1f 像素 | 头部 %.1f 像素"),
            Localization::Localize(Structs::TriggerDetectModeNames[detectMode]),
            bodyRadius,
            headRadius
        );

        drawList->AddText(
            ImVec2(12.0f, leftHudY),
            ToImColor(config.Aim.TriggerHitboxDebugColor),
            triggerDebugText
        );
        leftHudY += ImGui::GetFontSize() + 2.0f;
    }

        for (const PlayerEspSnapshot& player : frame.Players)
            RenderPlayer(drawList, player);

        if (config.Visuals.C4)
            RenderC4(drawList, frame.C4);
    }

    if (renderGrenadeHelper)
        RenderGrenadeHelper(drawList, frame.GrenadeHelper);

    publishPerf();
}
