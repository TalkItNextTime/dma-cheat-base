#include <Pch.hpp>
#include "AutowallEngine.hpp"

#include <regex>

namespace
{
    bool ReadTextFile(const std::filesystem::path& path, std::string& outText)
    {
        std::ifstream in(path);
        if (!in.is_open())
            return false;

        outText.assign(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>());
        return !outText.empty();
    }

    bool ExtractListBlocks(const std::string& text, const char* listName, std::vector<std::string>& outBlocks)
    {
        outBlocks.clear();
        if (!listName)
            return false;

        const std::string marker = std::string(listName) + " =";
        const std::size_t markerPos = text.find(marker);
        if (markerPos == std::string::npos)
            return false;

        const std::size_t listBegin = text.find('[', markerPos);
        if (listBegin == std::string::npos)
            return false;

        int listDepth = 1;
        int blockDepth = 0;
        std::size_t blockStart = std::string::npos;
        for (std::size_t i = listBegin + 1; i < text.size(); ++i)
        {
            const char ch = text[i];

            if (ch == '[')
            {
                ++listDepth;
                continue;
            }
            if (ch == ']')
            {
                --listDepth;
                if (listDepth <= 0)
                    break;
                continue;
            }

            if (ch == '{')
            {
                if (blockDepth == 0)
                    blockStart = i;
                ++blockDepth;
                continue;
            }

            if (ch == '}' && blockDepth > 0)
            {
                --blockDepth;
                if (blockDepth == 0 && blockStart != std::string::npos)
                {
                    outBlocks.emplace_back(text.substr(blockStart, i - blockStart + 1));
                    blockStart = std::string::npos;
                }
            }
        }

        return !outBlocks.empty();
    }

    bool ExtractStringField(const std::string& block, const char* key, std::string& outValue)
    {
        if (!key)
            return false;

        const std::regex pattern(std::string(key) + "\\s*=\\s*\"([^\"]+)\"");
        std::smatch match{};
        if (!std::regex_search(block, match, pattern) || match.size() < 2)
            return false;

        outValue = match[1].str();
        return !outValue.empty();
    }

    bool ExtractUintField(const std::string& block, const char* key, std::uint32_t& outValue)
    {
        if (!key)
            return false;

        const std::regex pattern(std::string(key) + "\\s*=\\s*([0-9]+)");
        std::smatch match{};
        if (!std::regex_search(block, match, pattern) || match.size() < 2)
            return false;

        try
        {
            const std::uint64_t parsed = std::stoull(match[1].str());
            if (parsed > std::numeric_limits<std::uint32_t>::max())
                return false;
            outValue = static_cast<std::uint32_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ExtractFloatField(const std::string& block, const char* key, float& outValue)
    {
        if (!key)
            return false;

        const std::regex pattern(std::string(key) + "\\s*=\\s*([-+]?[0-9]*\\.?[0-9]+)");
        std::smatch match{};
        if (!std::regex_search(block, match, pattern) || match.size() < 2)
            return false;

        try
        {
            outValue = std::stof(match[1].str());
            return std::isfinite(outValue);
        }
        catch (...)
        {
            return false;
        }
    }

    std::filesystem::path ResolveAutowallDataFile(const char* fileName)
    {
        if (!fileName || *fileName == '\0')
            return {};

        std::vector<std::filesystem::path> candidates{};
        candidates.reserve(48);

        auto appendCandidatesFromBase = [&](std::filesystem::path base)
        {
            for (int depth = 0; depth < 8 && !base.empty(); ++depth)
            {
                candidates.push_back(base / "Data" / "autowall" / fileName);
                candidates.push_back(base / "project-d" / "Data" / "autowall" / fileName);

                const std::filesystem::path parent = base.parent_path();
                if (parent == base || parent.empty())
                    break;
                base = parent;
            }
        };

        std::error_code cwdEc{};
        const std::filesystem::path cwd = std::filesystem::current_path(cwdEc);
        if (!cwdEc)
            appendCandidatesFromBase(cwd);

        char modulePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        {
            const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
            appendCandidatesFromBase(exeDir);
        }

        // Dev/repo fallback: resolve relative to this source file location.
        // __FILE__ => .../project-d/Source/Features/Aimbot/AutowallEngine.cpp
        {
            std::error_code fileEc{};
            std::filesystem::path sourceDir = std::filesystem::path(__FILE__).parent_path();
            if (!sourceDir.empty())
            {
                for (int i = 0; i < 5 && !sourceDir.empty(); ++i)
                {
                    candidates.push_back(sourceDir / "Data" / "autowall" / fileName);
                    candidates.push_back(sourceDir / "project-d" / "Data" / "autowall" / fileName);

                    const std::filesystem::path parent = sourceDir.parent_path();
                    if (parent == sourceDir || parent.empty())
                        break;
                    sourceDir = parent;
                }
            }
        }

        for (const std::filesystem::path& candidate : candidates)
        {
            std::error_code existsEc{};
            if (std::filesystem::exists(candidate, existsEc) && !existsEc)
                return candidate;
        }

        return {};
    }
}

AutowallEngine::AutowallEngine()
{
    m_WeaponTable = {
        { 1, { 53.0f, 0.81f, 140.0f } }, // Deagle
        { 2, { 38.0f, 0.75f, 110.0f } }, // Dual
        { 3, { 20.0f, 0.91f, 100.0f } }, // Five-SeveN
        { 4, { 28.0f, 0.91f, 100.0f } }, // Glock
        { 7, { 36.0f, 0.98f, 200.0f } }, // AK47
        { 8, { 28.0f, 0.96f, 180.0f } }, // AUG
        { 9, { 115.0f, 0.99f, 250.0f } }, // AWP
        { 10, { 30.0f, 0.96f, 170.0f } }, // Famas
        { 11, { 80.0f, 0.98f, 220.0f } }, // G3SG1
        { 13, { 30.0f, 0.98f, 180.0f } }, // Galil
        { 14, { 38.0f, 0.97f, 240.0f } }, // M249
        { 16, { 33.0f, 0.97f, 200.0f } }, // M4A4
        { 17, { 29.0f, 0.96f, 130.0f } }, // Mac10
        { 19, { 26.0f, 0.97f, 130.0f } }, // P90
        { 23, { 27.0f, 0.96f, 130.0f } }, // MP5-SD
        { 24, { 35.0f, 0.96f, 130.0f } }, // UMP45
        { 25, { 20.0f, 0.70f, 90.0f } }, // XM1014
        { 26, { 27.0f, 0.96f, 120.0f } }, // Bizon
        { 27, { 30.0f, 0.70f, 95.0f } }, // MAG7
        { 28, { 35.0f, 0.97f, 240.0f } }, // Negev
        { 29, { 32.0f, 0.70f, 95.0f } }, // Sawed-Off
        { 30, { 33.0f, 0.94f, 110.0f } }, // Tec9
        { 32, { 35.0f, 0.93f, 110.0f } }, // P2000
        { 33, { 29.0f, 0.97f, 130.0f } }, // MP7
        { 34, { 26.0f, 0.97f, 130.0f } }, // MP9
        { 35, { 26.0f, 0.70f, 90.0f } }, // Nova
        { 36, { 38.0f, 0.90f, 120.0f } }, // P250
        { 38, { 80.0f, 0.98f, 220.0f } }, // SCAR20
        { 39, { 30.0f, 0.98f, 200.0f } }, // SG553
        { 40, { 88.0f, 0.98f, 220.0f } }, // SSG08
        { 60, { 38.0f, 0.97f, 200.0f } }, // M4A1-S
        { 61, { 35.0f, 0.92f, 110.0f } }, // USP-S
        { 63, { 31.0f, 0.90f, 105.0f } }, // CZ75
        { 64, { 86.0f, 0.98f, 200.0f } }, // R8
    };

    m_DefaultWeapon = { 36.0f, 0.98f, 160.0f };
    m_DefaultMaterial = { "default", 0.5f, 0.5f };
    m_MaterialsLoaded = LoadMaterialTables();
}

float AutowallEngine::Distance3D(const Vector3& a, const Vector3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::string AutowallEngine::ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const AutowallEngine::WeaponPenetrationData& AutowallEngine::ResolveWeapon(const int weaponId) const
{
    const auto it = m_WeaponTable.find(weaponId);
    if (it == m_WeaponTable.end())
        return m_DefaultWeapon;
    return it->second;
}

const AutowallEngine::MaterialPenetrationData& AutowallEngine::ResolveMaterial(const std::uint32_t materialHash) const
{
    const auto it = m_MaterialsByHash.find(materialHash);
    if (it == m_MaterialsByHash.end())
        return m_DefaultMaterial;
    return it->second;
}

bool AutowallEngine::LoadMaterialTables()
{
    const std::filesystem::path propertiesPath = ResolveAutowallDataFile("surfaceproperties.txt");
    const std::filesystem::path gamePath = ResolveAutowallDataFile("surfaceproperties_game.txt");
    if (propertiesPath.empty() || gamePath.empty())
    {
        LOG_WARN("Autowall material files not found, using fallback defaults.");
        return false;
    }

    std::string propertiesText{};
    std::string gameText{};
    if (!ReadTextFile(propertiesPath, propertiesText) || !ReadTextFile(gamePath, gameText))
    {
        LOG_WARN("Autowall material files could not be read, using fallback defaults.");
        return false;
    }

    std::vector<std::string> propertyBlocks{};
    std::vector<std::string> gameBlocks{};
    if (!ExtractListBlocks(propertiesText, "SurfacePropertiesList", propertyBlocks) ||
        !ExtractListBlocks(gameText, "SurfacePropertiesList", gameBlocks))
    {
        LOG_WARN("Autowall material data malformed, using fallback defaults.");
        return false;
    }

    std::unordered_map<std::uint32_t, std::string> nameByHash{};
    nameByHash.reserve(propertyBlocks.size());
    for (const std::string& block : propertyBlocks)
    {
        std::string name{};
        std::uint32_t hash = 0u;
        if (!ExtractStringField(block, "surfacePropertyName", name))
            continue;
        if (!ExtractUintField(block, "m_nameHash", hash))
            continue;
        nameByHash[hash] = name;
    }

    std::unordered_map<std::string, MaterialPenetrationData> gameDataByName{};
    gameDataByName.reserve(gameBlocks.size());
    for (const std::string& block : gameBlocks)
    {
        std::string name{};
        if (!ExtractStringField(block, "surfacePropertyName", name))
            continue;

        MaterialPenetrationData data{};
        data.Name = name;
        data.DistanceModifier = 0.5f;
        data.DamageModifier = 0.5f;
        (void)ExtractFloatField(block, "bulletPenetrationDistanceModifier", data.DistanceModifier);
        (void)ExtractFloatField(block, "bulletPenetrationDamageModifier", data.DamageModifier);
        gameDataByName[ToLowerAscii(name)] = data;
    }

    m_MaterialsByHash.clear();
    m_MaterialsByHash.reserve(nameByHash.size() + 4u);

    const auto defaultIt = gameDataByName.find("default");
    if (defaultIt != gameDataByName.end())
        m_DefaultMaterial = defaultIt->second;

    for (const auto& [hash, name] : nameByHash)
    {
        const auto it = gameDataByName.find(ToLowerAscii(name));
        if (it != gameDataByName.end())
            m_MaterialsByHash[hash] = it->second;
        else
            m_MaterialsByHash[hash] = MaterialPenetrationData{ name, m_DefaultMaterial.DistanceModifier, m_DefaultMaterial.DamageModifier };
    }

    m_MaterialsByHash[0u] = m_DefaultMaterial;
    LOG_INFO(
        "Autowall material table loaded: {} entries (source=disk, files='{}','{}')",
        m_MaterialsByHash.size(),
        propertiesPath.string(),
        gamePath.string());
    return !m_MaterialsByHash.empty();
}

AutowallEngine::Result AutowallEngine::Evaluate(
    const Vector3& shooter,
    const Vector3& target,
    const int weaponId,
    const std::vector<VisCheck::PenetrationSegment>& segments) const
{
    constexpr float kMinPenetrationDamage = 0.01f;

    const WeaponPenetrationData& weapon = ResolveWeapon(weaponId);
    const float totalDistance = (std::max)(0.0f, Distance3D(shooter, target));
    float currentDamage = (std::max)(0.0f, weapon.BaseDamage);

    if (currentDamage <= kMinPenetrationDamage || totalDistance <= 0.001f)
        return { 0.0f, false };

    float traveledDistance = 0.0f;
    for (const VisCheck::PenetrationSegment& segment : segments)
    {
        const float entryDistance = std::clamp(segment.entryDistance, 0.0f, totalDistance);
        const float exitDistance = std::clamp(segment.exitDistance, 0.0f, totalDistance);
        if (exitDistance <= entryDistance)
            continue;

        const float stepDistance = (std::max)(0.0f, entryDistance - traveledDistance);
        currentDamage *= std::pow(weapon.RangeModifier, stepDistance / 500.0f);
        if (currentDamage <= kMinPenetrationDamage)
            return { 0.0f, false };

        const MaterialPenetrationData& entryMaterial = ResolveMaterial(segment.entryMaterialHash);
        const MaterialPenetrationData& exitMaterial = ResolveMaterial(segment.exitMaterialHash);

        float combinedPenModifier = (entryMaterial.DistanceModifier + exitMaterial.DistanceModifier) * 0.5f;
        if (segment.entryMaterialHash == segment.exitMaterialHash)
        {
            const std::string lowerName = ToLowerAscii(entryMaterial.Name);
            if (lowerName.find("cardboard") != std::string::npos ||
                lowerName.find("wood") != std::string::npos)
            {
                combinedPenModifier = 3.0f;
            }
            else if (lowerName.find("plastic") != std::string::npos)
            {
                combinedPenModifier = 2.0f;
            }
        }

        if (combinedPenModifier <= 0.0f)
            return { 0.0f, false };

        // Segment thickness is already in Source world units; no extra unit conversion here.
        const float insideDistance = (std::max)(0.0f, segment.thickness);
        const float modifier = (std::max)(0.0f, 1.0f / combinedPenModifier);
        const float lostDamage = (std::max)(
            ((modifier * insideDistance * insideDistance) / 24.0f) +
            ((currentDamage * 0.18f) + (std::max)(3.75f / weapon.PenetrationPower, 0.0f) * 3.0f * modifier),
            0.0f);

        if (lostDamage > currentDamage)
            return { 0.0f, false };

        currentDamage -= lostDamage;
        if (currentDamage <= kMinPenetrationDamage)
            return { 0.0f, false };

        traveledDistance = exitDistance;
    }

    const float remainingDistance = (std::max)(0.0f, totalDistance - traveledDistance);
    currentDamage *= std::pow(weapon.RangeModifier, remainingDistance / 500.0f);
    currentDamage = (std::max)(currentDamage, 0.0f);
    return { currentDamage, currentDamage > kMinPenetrationDamage };
}
