#include <Pch.hpp>
#include <SDK.hpp>
#include "OffsetInitializationPolicy.hpp"
#include "RuntimeOffsetResolver.hpp"
#include <cstring>

namespace
{
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

bool SDK::LoadOffsets(const bool suppressFailureLog)
{
    RuntimeOffsetResolver::ResolveReport report;
    std::string errorMessage;

    if (!RuntimeOffsetResolver::Resolve(mem, report, errorMessage))
    {
        if (!suppressFailureLog)
            LOG_ERROR("Automatic runtime offset dump failed: {}", errorMessage);
        return false;
    }

    LOG_INFO(
        "Loaded runtime offsets: controller=0x{:X}, pawn=0x{:X}, entity=0x{:X}, view=0x{:X}",
        Offsets::Client::dwLocalPlayerController,
        Offsets::Client::dwLocalPlayerPawn,
        Offsets::Client::dwEntityList,
        Offsets::Client::dwViewMatrix);

    m_SchemaInitialized = report.SchemaInitialized;

    if (!report.SchemaInitialized)
    {
        if (!suppressFailureLog)
            LOG_WARN("Runtime schema discovery is not ready yet: {}", report.SchemaErrorMessage);

        return Offsets::HasCore();
    }

    if (!report.SchemaComplete)
    {
        LOG_WARN(
            "Schema offsets partially loaded. Missing {} required field(s).",
            report.MissingRequiredSchemaFields.size());

        for (const auto& field : report.MissingRequiredSchemaFields)
            LOG_WARN("Missing runtime schema field {}", field);
    }

    return Offsets::HasCore();
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
        LOG_WARN("Core offsets are not configured. Automatic runtime dump did not produce a usable core set.");
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
    m_UpdateThread = thread([this]()
    {
        auto lastOffsetRetry = chrono::steady_clock::now() - chrono::milliseconds(500);
        auto lastBaseRefresh = chrono::steady_clock::now() - chrono::seconds(5);

        while (Globals::Running)
        {
            const auto now = chrono::steady_clock::now();
            if (now - lastBaseRefresh >= chrono::seconds(2))
            {
                lastBaseRefresh = now;
                RefreshGameBases(false);
            }

            const int millisecondsSinceLastRetry = static_cast<int>(
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - lastOffsetRetry).count());
            const auto decision = OffsetInitializationPolicy::EvaluateUpdateLoop(
                Offsets::HasCore(),
                m_SchemaInitialized.load(),
                millisecondsSinceLastRetry);

            if (decision.AttemptReload)
            {
                const bool hadCoreBefore = Offsets::HasCore();
                const bool hadSchemaBefore = m_SchemaInitialized.load();
                lastOffsetRetry = chrono::steady_clock::now();

                if (!LoadOffsets(true))
                {
                    if (!m_LoggedMissingOffsets && decision.EmitPausedWarning)
                    {
                        LOG_WARN("Core offsets missing, SDK update loop is retrying automatic runtime dump.");
                        m_LoggedMissingOffsets = true;
                    }

                    this_thread::sleep_for(chrono::milliseconds(decision.SleepMilliseconds));
                    continue;
                }

                if (!hadCoreBefore && Offsets::HasCore())
                    LOG_INFO("Runtime core offsets recovered in SDK update loop.");
                else if (!hadSchemaBefore && m_SchemaInitialized.load())
                    LOG_INFO("Runtime schema discovery recovered in SDK update loop.");

                m_LoggedMissingOffsets = false;
                RefreshCoreCache();
                this_thread::sleep_for(chrono::milliseconds(2));
                continue;
            }

            if (decision.RefreshCoreCache)
                RefreshCoreCache();

            this_thread::sleep_for(chrono::milliseconds(decision.SleepMilliseconds));
        }
    });
}

void SDK::Shutdown()
{
    if (m_UpdateThread.joinable())
        m_UpdateThread.join();
}

bool SDK::RefreshCoreCache()
{
    if (!RefreshGameBases(false))
        return false;

    if (!Offsets::HasCore() || !Globals::ClientBase)
        return false;

    CoreCache updated{};

    updated.LocalController = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwLocalPlayerController);
    updated.LocalPawn = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwLocalPlayerPawn);
    updated.EntityList = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwEntityList);

    Matrix viewMatrix{};
    if (!mem.Read(Globals::ClientBase + Offsets::Client::dwViewMatrix, &viewMatrix, sizeof(viewMatrix)))
    {
        RefreshGameBases(true);
        return false;
    }

    updated.ViewMatrix = viewMatrix;
    updated.IsValid = updated.EntityList != 0;

    {
        lock_guard lock(m_CoreMutex);
        m_CoreCache = updated;
    }

    Globals::ViewMatrix = updated.ViewMatrix;
    return updated.IsValid;
}

bool SDK::RefreshGameBases(const bool forceReinitialize)
{
    const auto now = chrono::steady_clock::now();
    if (!forceReinitialize &&
        m_LastBaseRefresh.time_since_epoch().count() != 0 &&
        now - m_LastBaseRefresh < chrono::milliseconds(750))
    {
        return Globals::ClientBase != 0;
    }
    m_LastBaseRefresh = now;

    const uint64_t oldClientBase = Globals::ClientBase;
    if (!dma.RefreshGameBases(forceReinitialize))
    {
        lock_guard lock(m_CoreMutex);
        m_CoreCache = {};
        return false;
    }

    if (oldClientBase && oldClientBase != Globals::ClientBase)
    {
        LOG_INFO(
            "Detected game module base change: client.dll 0x{:X} -> 0x{:X}",
            static_cast<unsigned long long>(oldClientBase),
            static_cast<unsigned long long>(Globals::ClientBase));
        LoadOffsets(true);

        lock_guard lock(m_CoreMutex);
        m_CoreCache = {};
    }

    return true;
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

    if (!Offsets::EntityList::IsHandleValid(handle))
        return 0;

    const uint32_t index = Offsets::EntityList::HandleIndex(handle);
    const uint32_t hi = index >> Offsets::EntityList::HandleHighShift;
    const uint32_t lo = index & Offsets::EntityList::HandleLowMask;

    const uint64_t listEntry = mem.Read<uint64_t>(
        entityList + Offsets::EntityList::ListStart + static_cast<uint64_t>(hi) * Offsets::EntityList::ChunkStride
    );
    if (!listEntry)
        return 0;

    return mem.Read<uint64_t>(listEntry + static_cast<uint64_t>(lo) * Offsets::EntityList::EntryStride);
}

uint64_t SDK::ResolveActiveWeaponFromPawn(uint64_t pawn) const
{
    const auto cache = GetCoreCache();
    return ResolveActiveWeaponFromPawn(pawn, cache.EntityList);
}

uint64_t SDK::ResolveActiveWeaponFromPawn(uint64_t pawn, uint64_t entityList) const
{
    if (!pawn || !entityList || !Offsets::Schema::m_pWeaponServices || !Offsets::Schema::m_hActiveWeapon)
        return 0;

    const std::uint64_t weaponServices = mem.Read<std::uint64_t>(pawn + Offsets::Schema::m_pWeaponServices);
    if (!weaponServices)
        return 0;

    const std::uint32_t activeWeaponHandle = mem.Read<std::uint32_t>(weaponServices + Offsets::Schema::m_hActiveWeapon);
    if (!Offsets::EntityList::IsHandleValid(activeWeaponHandle))
        return 0;

    return ResolveEntityFromHandle(activeWeaponHandle, entityList);
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
