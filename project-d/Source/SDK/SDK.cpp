#include <Pch.hpp>
#include <SDK.hpp>
#include <cstring>

namespace
{
    std::string ResolveOffsetFilePath(const std::string& fileName)
    {
        const std::vector<std::string> candidates = {
            "Offsets/" + fileName,
            "project-d/Offsets/" + fileName,
            "../project-d/Offsets/" + fileName,
            "../../project-d/Offsets/" + fileName,
            "../../../project-d/Offsets/" + fileName
        };

        for (const auto& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
                return candidate;
        }

        return {};
    }

    bool ReadJsonFile(const std::string& path, json& out)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        try
        {
            file >> out;
            return true;
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR("Failed to parse JSON '{}': {}", path, ex.what());
            return false;
        }
    }

    bool TryGetClientField(const json& clientDllJson, const char* className, const char* fieldName, uint32_t& out)
    {
        if (!clientDllJson.contains("client.dll"))
            return false;

        const auto& client = clientDllJson["client.dll"];
        if (!client.contains("classes"))
            return false;

        const auto& classes = client["classes"];
        if (!classes.contains(className))
            return false;

        const auto& cls = classes[className];
        if (!cls.contains("fields"))
            return false;

        const auto& fields = cls["fields"];
        if (!fields.contains(fieldName))
            return false;

        out = fields[fieldName].get<uint32_t>();
        return true;
    }

    std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool ReadAsciiString(uint64_t address, char* outBuffer, size_t outSize)
    {
        if (!address || !outBuffer || outSize == 0)
            return false;

        std::memset(outBuffer, 0, outSize);
        return mem.Read(address, outBuffer, outSize);
    }

    bool IsLikelyMapName(const std::string& value)
    {
        if (value.empty() || value.size() > 128)
            return false;

        for (const char c : value)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/' || c == '\\' || c == '.')
                continue;

            return false;
        }

        const std::string lower = ToLowerAscii(value);
        if (lower.find("de_") != std::string::npos ||
            lower.find("cs_") != std::string::npos ||
            lower.find("ar_") != std::string::npos ||
            lower.find("dm_") != std::string::npos ||
            lower.find("aim_") != std::string::npos)
        {
            return true;
        }

        const size_t separator = lower.find_last_of("/\\");
        const std::string leaf = separator == std::string::npos ? lower : lower.substr(separator + 1);
        if (leaf.size() >= 3 &&
            std::isalpha(static_cast<unsigned char>(leaf[0])) &&
            std::isalpha(static_cast<unsigned char>(leaf[1])) &&
            leaf[2] == '_')
        {
            return true;
        }

        return false;
    }

    bool IsLikelyUserAddress(const uint64_t address)
    {
        return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
    }

    std::string NormalizeMapString(std::string value)
    {
        if (value.empty())
            return {};

        const std::string lower = ToLowerAscii(value);

        if (lower.rfind("maps/", 0) == 0)
            value = value.substr(5);
        else if (lower.rfind("maps\\", 0) == 0)
            value = value.substr(5);

        const std::string lower2 = ToLowerAscii(value);
        constexpr const char* bspSuffix = ".bsp";
        constexpr size_t bspSuffixLen = 4;
        if (lower2.size() > bspSuffixLen && lower2.compare(lower2.size() - bspSuffixLen, bspSuffixLen, bspSuffix) == 0)
            value = value.substr(0, value.size() - bspSuffixLen);

        return value;
    }

    bool TryReadMapNameAtAddress(const uint64_t address, std::string& outMapName)
    {
        constexpr size_t kMapNameBufferSize = 128;
        char mapNameRaw[kMapNameBufferSize]{};

        if (!ReadAsciiString(address, mapNameRaw, sizeof(mapNameRaw)))
            return false;

        std::string mapName = NormalizeMapString(mapNameRaw);
        if (!IsLikelyMapName(mapName))
            return false;

        outMapName = std::move(mapName);
        return true;
    }

    bool TryReadMapNameFromCandidateBase(const uint64_t base, std::string& outMapName)
    {
        if (!base || !IsLikelyUserAddress(base))
            return false;

        const uint64_t fallback = Offsets::Engine2::dwNetworkGameClient_mapNameFallback;
        const uint64_t mapNameOffsets[] = {
            fallback - 0x10,
            fallback - 0x08,
            fallback,
            fallback + 0x08,
            fallback + 0x10,
            fallback + 0x18,
            fallback + 0x20,
            fallback + 0x28
        };

        for (const uint64_t mapNameOffset : mapNameOffsets)
        {
            if (TryReadMapNameAtAddress(base + mapNameOffset, outMapName))
                return true;

            const uint64_t mapNamePtr = mem.Read<uint64_t>(base + mapNameOffset);
            if (!IsLikelyUserAddress(mapNamePtr))
                continue;

            if (TryReadMapNameAtAddress(mapNamePtr, outMapName))
                return true;
        }

        return false;
    }

    bool TryReadMapNameFromGlobalVars(std::string& outMapName)
    {
        if (!Globals::ClientBase || !Offsets::Client::dwGlobalVars)
            return false;

        const uint64_t globalVars = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGlobalVars);
        if (!IsLikelyUserAddress(globalVars))
            return false;

        // Matches the approach in CheatiFrame: GlobalVars + 0x188 points to current map string.
        const uint64_t mapNamePtr = mem.Read<uint64_t>(globalVars + 0x188);
        if (!IsLikelyUserAddress(mapNamePtr))
            return false;

        return TryReadMapNameAtAddress(mapNamePtr, outMapName);
    }
}

bool SDK::LoadOffsets()
{
    const std::string offsetsPath = ResolveOffsetFilePath("offsets.json");
    const std::string clientDllPath = ResolveOffsetFilePath("client_dll.json");

    if (offsetsPath.empty() || clientDllPath.empty())
    {
        LOG_ERROR("Offsets files not found. Expected 'Offsets/offsets.json' and 'Offsets/client_dll.json'.");
        return false;
    }

    json offsetsJson;
    json clientDllJson;

    if (!ReadJsonFile(offsetsPath, offsetsJson) || !ReadJsonFile(clientDllPath, clientDllJson))
        return false;

    if (!offsetsJson.contains("client.dll"))
    {
        LOG_ERROR("Invalid offsets.json format: missing 'client.dll' section.");
        return false;
    }

    const auto& client = offsetsJson["client.dll"];
    Offsets::Client::dwLocalPlayerController = client.value("dwLocalPlayerController", 0ULL);
    Offsets::Client::dwLocalPlayerPawn = client.value("dwLocalPlayerPawn", 0ULL);
    Offsets::Client::dwEntityList = client.value("dwEntityList", 0ULL);
    Offsets::Client::dwGameEntitySystem = client.value("dwGameEntitySystem", 0ULL);
    Offsets::Client::dwGameEntitySystem_highestEntityIndex = client.value("dwGameEntitySystem_highestEntityIndex", 0ULL);
    Offsets::Client::dwGameRules = client.value("dwGameRules", 0ULL);
    Offsets::Client::dwPlantedC4 = client.value("dwPlantedC4", 0ULL);
    Offsets::Client::dwGlobalVars = client.value("dwGlobalVars", 0ULL);
    Offsets::Client::dwViewAngles = client.value("dwViewAngles", 0ULL);
    Offsets::Client::dwViewMatrix = client.value("dwViewMatrix", 0ULL);

    if (offsetsJson.contains("engine2.dll"))
    {
        const auto& engine = offsetsJson["engine2.dll"];
        Offsets::Engine2::dwNetworkGameClient = engine.value("dwNetworkGameClient", 0ULL);
        Offsets::Engine2::dwNetworkGameClient_localPlayer = engine.value("dwNetworkGameClient_localPlayer", 0ULL);
        Offsets::Engine2::dwNetworkGameClient_signOnState = engine.value("dwNetworkGameClient_signOnState", 0ULL);
        Offsets::Engine2::dwNetworkGameClient_maxClients = engine.value("dwNetworkGameClient_maxClients", 0ULL);
    }
    else
    {
        LOG_WARN("offsets.json missing 'engine2.dll' section. Current map auto-detection may be unavailable.");
    }

    bool schemaComplete = true;

    auto loadSchema = [&](const char* className, const char* fieldName, uint32_t& target)
    {
        const bool ok = TryGetClientField(clientDllJson, className, fieldName, target);
        schemaComplete = schemaComplete && ok;

        if (!ok)
            LOG_WARN("Missing schema field {}::{} in client_dll.json", className, fieldName);
    };

    loadSchema("CBasePlayerController", "m_hPawn", Offsets::Schema::m_hPawn);
    loadSchema("CCSPlayerController", "m_hPlayerPawn", Offsets::Schema::m_hPlayerPawn);
    loadSchema("CBasePlayerController", "m_iszPlayerName", Offsets::Schema::m_iszPlayerName);
    loadSchema("CCSPlayerController", "m_pInGameMoneyServices", Offsets::Schema::m_pInGameMoneyServices);
    loadSchema("CCSPlayerController_InGameMoneyServices", "m_iAccount", Offsets::Schema::m_iAccount);
    loadSchema("C_BaseEntity", "m_iHealth", Offsets::Schema::m_iHealth);
    loadSchema("C_BaseEntity", "m_iMaxHealth", Offsets::Schema::m_iMaxHealth);
    loadSchema("C_BaseEntity", "m_iTeamNum", Offsets::Schema::m_iTeamNum);
    loadSchema("C_BaseEntity", "m_lifeState", Offsets::Schema::m_lifeState);
    loadSchema("C_BaseEntity", "m_pGameSceneNode", Offsets::Schema::m_pGameSceneNode);
    loadSchema("CGameSceneNode", "m_vecAbsOrigin", Offsets::Schema::m_vecAbsOrigin);
    loadSchema("C_BasePlayerPawn", "m_pWeaponServices", Offsets::Schema::m_pWeaponServices);
    loadSchema("C_BasePlayerPawn", "m_pObserverServices", Offsets::Schema::m_pObserverServices);
    loadSchema("C_BasePlayerPawn", "m_vOldOrigin", Offsets::Schema::m_vOldOrigin);
    loadSchema("C_BaseModelEntity", "m_vecViewOffset", Offsets::Schema::m_vecViewOffset);
    loadSchema("CSkeletonInstance", "m_modelState", Offsets::Schema::m_modelState);
    loadSchema("C_CSPlayerPawn", "m_ArmorValue", Offsets::Schema::m_ArmorValue);
    loadSchema("C_CSPlayerPawn", "m_bIsScoped", Offsets::Schema::m_bIsScoped);
    loadSchema("C_CSPlayerPawnBase", "m_flFlashDuration", Offsets::Schema::m_flFlashDuration);
    loadSchema("C_CSPlayerPawn", "m_iIDEntIndex", Offsets::Schema::m_iIDEntIndex);
    loadSchema("CPlayer_ObserverServices", "m_iObserverMode", Offsets::Schema::m_iObserverMode);
    loadSchema("CPlayer_ObserverServices", "m_hObserverTarget", Offsets::Schema::m_hObserverTarget);
    loadSchema("C_CSPlayerPawn", "m_iShotsFired", Offsets::Schema::m_iShotsFired);
    loadSchema("C_CSPlayerPawn", "m_aimPunchAngle", Offsets::Schema::m_aimPunchAngle);
    loadSchema("CPlayer_WeaponServices", "m_hActiveWeapon", Offsets::Schema::m_hActiveWeapon);
    loadSchema("C_EconEntity", "m_AttributeManager", Offsets::Schema::m_AttributeManager);
    loadSchema("C_EconItemView", "m_iItemDefinitionIndex", Offsets::Schema::m_iItemDefinitionIndex);
    loadSchema("C_PlantedC4", "m_bBombTicking", Offsets::Schema::m_bBombTicking);
    loadSchema("C_PlantedC4", "m_nBombSite", Offsets::Schema::m_nBombSite);
    loadSchema("C_PlantedC4", "m_flC4Blow", Offsets::Schema::m_flC4Blow);
    loadSchema("C_PlantedC4", "m_flTimerLength", Offsets::Schema::m_flTimerLength);
    loadSchema("C_PlantedC4", "m_bBeingDefused", Offsets::Schema::m_bBeingDefused);
    loadSchema("C_PlantedC4", "m_flDefuseCountDown", Offsets::Schema::m_flDefuseCountDown);
    loadSchema("C_CSGameRules", "m_bFreezePeriod", Offsets::Schema::m_bFreezePeriod);

    if (!Offsets::HasCore())
    {
        LOG_ERROR("Core offsets are incomplete after JSON load.");
        return false;
    }

    LOG_INFO(
        "Loaded core offsets from JSON: controller=0x{:X}, pawn=0x{:X}, entity=0x{:X}, view=0x{:X}",
        Offsets::Client::dwLocalPlayerController,
        Offsets::Client::dwLocalPlayerPawn,
        Offsets::Client::dwEntityList,
        Offsets::Client::dwViewMatrix
    );

    if (!schemaComplete)
        LOG_WARN("Schema offsets partially loaded. Core layer remains available.");

    return true;
}

bool SDK::Init()
{
    Globals::Running = true;

    if (!LoadOffsets())
    {
        LOG_WARN("Offset initialization is incomplete. SDK core update may stay paused.");
    }

    if (!Offsets::HasCore())
    {
        LOG_WARN("Core offsets are not configured. Check Offsets/offsets.json before using feature logic.");
        m_LoggedMissingOffsets = true;
    }
    else
    {
        RefreshCoreCache();
    }

    InitUpdateSdk();

    return true;
}

void SDK::InitUpdateSdk()
{
    thread([this]()
    {
        while (Globals::Running)
        {
            if (!Offsets::HasCore())
            {
                if (!m_LoggedMissingOffsets)
                {
                    LOG_WARN("Core offsets missing, SDK update loop paused.");
                    m_LoggedMissingOffsets = true;
                }

                this_thread::sleep_for(chrono::milliseconds(500));
                continue;
            }

            RefreshCoreCache();
            this_thread::sleep_for(chrono::milliseconds(2));
        }
    }).detach();
}

bool SDK::RefreshCoreCache()
{
    if (!Offsets::HasCore() || !Globals::ClientBase)
        return false;

    CoreCache updated{};

    updated.LocalController = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwLocalPlayerController);
    updated.LocalPawn = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwLocalPlayerPawn);
    updated.EntityList = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwEntityList);

    Matrix viewMatrix{};
    if (!mem.Read(Globals::ClientBase + Offsets::Client::dwViewMatrix, &viewMatrix, sizeof(viewMatrix)))
        return false;

    updated.ViewMatrix = viewMatrix;
    updated.IsValid = updated.EntityList != 0;

    {
        lock_guard lock(m_CoreMutex);
        m_CoreCache = updated;
    }

    Globals::ViewMatrix = updated.ViewMatrix;
    return updated.IsValid;
}

SDK::CoreCache SDK::GetCoreCache() const
{
    lock_guard lock(m_CoreMutex);
    return m_CoreCache;
}

std::string SDK::GetCurrentMapName() const
{
    if (!Globals::Engine2Base || !Offsets::Engine2::dwNetworkGameClient)
    {
        std::string mapNameFromGlobalVars{};
        if (TryReadMapNameFromGlobalVars(mapNameFromGlobalVars))
            return mapNameFromGlobalVars;
        return {};
    }

    std::string mapNameFromGlobalVars{};
    if (TryReadMapNameFromGlobalVars(mapNameFromGlobalVars))
        return mapNameFromGlobalVars;

    const uint64_t networkClientAddress = Globals::Engine2Base + Offsets::Engine2::dwNetworkGameClient;
    const uint64_t networkClientPtr = mem.Read<uint64_t>(networkClientAddress);
    const uint64_t networkClientPtr2 = IsLikelyUserAddress(networkClientPtr)
        ? mem.Read<uint64_t>(networkClientPtr)
        : 0;

    std::vector<uint64_t> candidates{};
    candidates.reserve(4);
    candidates.push_back(networkClientAddress);

    if (IsLikelyUserAddress(networkClientPtr))
        candidates.push_back(networkClientPtr);

    if (IsLikelyUserAddress(networkClientPtr2))
        candidates.push_back(networkClientPtr2);

    std::vector<uint64_t> uniqueCandidates{};
    uniqueCandidates.reserve(candidates.size());
    for (const uint64_t candidate : candidates)
    {
        if (!candidate || !IsLikelyUserAddress(candidate))
            continue;

        if ((std::find)(uniqueCandidates.begin(), uniqueCandidates.end(), candidate) != uniqueCandidates.end())
            continue;

        uniqueCandidates.push_back(candidate);
    }

    for (const uint64_t candidateBase : uniqueCandidates)
    {
        if (Offsets::Engine2::dwNetworkGameClient_signOnState)
        {
            const int signOnState = mem.Read<int>(candidateBase + Offsets::Engine2::dwNetworkGameClient_signOnState);
            if (signOnState < 0 || signOnState > 12)
                continue;
        }

        std::string mapName{};
        if (TryReadMapNameFromCandidateBase(candidateBase, mapName))
            return mapName;
    }

    return {};
}

uint64_t SDK::ResolveEntityFromHandle(uint32_t handle) const
{
    const auto cache = GetCoreCache();
    return ResolveEntityFromHandle(handle, cache.EntityList);
}

uint64_t SDK::ResolveEntityFromHandle(uint32_t handle, uint64_t entityList) const
{
    if (!entityList)
        return 0;

    const uint32_t index = handle & Offsets::EntityList::HandleMask;
    if (!index)
        return 0;

    const uint32_t hi = index >> Offsets::EntityList::HandleHighShift;
    const uint32_t lo = index & Offsets::EntityList::HandleLowMask;

    const uint64_t listEntry = mem.Read<uint64_t>(
        entityList + Offsets::EntityList::ListStart + static_cast<uint64_t>(hi) * Offsets::EntityList::ChunkStride
    );
    if (!listEntry)
        return 0;

    return mem.Read<uint64_t>(listEntry + static_cast<uint64_t>(lo) * Offsets::EntityList::EntryStride);
}

uint64_t SDK::ResolvePawnFromController(uint64_t controller) const
{
    if (!controller || (!Offsets::Schema::m_hPawn && !Offsets::Schema::m_hPlayerPawn))
        return 0;

    uint32_t pawnHandle = 0;
    if (Offsets::Schema::m_hPlayerPawn)
        pawnHandle = mem.Read<uint32_t>(controller + Offsets::Schema::m_hPlayerPawn);
    if (!pawnHandle && Offsets::Schema::m_hPawn)
        pawnHandle = mem.Read<uint32_t>(controller + Offsets::Schema::m_hPawn);

    if (!pawnHandle)
        return 0;

    return ResolveEntityFromHandle(pawnHandle);
}

bool SDK::ReadBasicEntityState(uint64_t entity, int& health, int& team, int& lifeState) const
{
    if (!entity || !Offsets::Schema::m_iHealth || !Offsets::Schema::m_iTeamNum || !Offsets::Schema::m_lifeState)
        return false;

    health = mem.Read<int>(entity + Offsets::Schema::m_iHealth);
    team = mem.Read<int>(entity + Offsets::Schema::m_iTeamNum);
    lifeState = mem.Read<int>(entity + Offsets::Schema::m_lifeState);

    return true;
}

bool SDK::WorldToScreen(const Vector3& WorldPos, Vector2& ScreenPos, const Matrix& Matrix)
{
    float w = Matrix[3][0] * WorldPos.x + Matrix[3][1] * WorldPos.y + Matrix[3][2] * WorldPos.z + Matrix[3][3];

    if (w < 0.001f) {
        return false;
    }

    float inv_w = 1.0f / w;

    float ndc_x = (Matrix[0][0] * WorldPos.x + Matrix[0][1] * WorldPos.y + Matrix[0][2] * WorldPos.z + Matrix[0][3]) * inv_w;
    float ndc_y = (Matrix[1][0] * WorldPos.x + Matrix[1][1] * WorldPos.y + Matrix[1][2] * WorldPos.z + Matrix[1][3]) * inv_w;

    float screen_x = ScreenCenter.x + (ndc_x * ScreenCenter.x);
    float screen_y = ScreenCenter.y - (ndc_y * ScreenCenter.y);

    ScreenPos.x = screen_x;
    ScreenPos.y = screen_y;

    return true;
}
