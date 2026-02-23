#pragma once

#include <cstdint>

namespace Offsets
{
    namespace Client
    {
        // Loaded at runtime from Offsets/offsets.json.
        inline std::uint64_t dwLocalPlayerController = 0x0;
        inline std::uint64_t dwLocalPlayerPawn = 0x0;
        inline std::uint64_t dwEntityList = 0x0;
        inline std::uint64_t dwGameEntitySystem = 0x0;
        inline std::uint64_t dwGameEntitySystem_highestEntityIndex = 0x0;
        inline std::uint64_t dwGameRules = 0x0;
        inline std::uint64_t dwPlantedC4 = 0x0;
        inline std::uint64_t dwWeaponC4 = 0x0;
        inline std::uint64_t dwGlobalVars = 0x0;
        inline std::uint64_t dwViewAngles = 0x0;
        inline std::uint64_t dwViewMatrix = 0x0;
    }

    namespace Engine2
    {
        // Loaded at runtime from Offsets/offsets.json.
        inline std::uint64_t dwNetworkGameClient = 0x0;
        inline std::uint64_t dwNetworkGameClient_localPlayer = 0x0;
        inline std::uint64_t dwNetworkGameClient_signOnState = 0x0;
        inline std::uint64_t dwNetworkGameClient_maxClients = 0x0;

        // Not exported by dumper in some builds, keep a safe fallback.
        inline constexpr std::uint64_t dwNetworkGameClient_mapNameFallback = 0x248;
    }

    namespace EntityList
    {
        // CS2 handle decode:
        // index = handle & 0x7FFF
        // hi    = index >> 9
        // lo    = index & 0x1FF
        inline constexpr std::uint32_t HandleMask = 0x7FFF;
        inline constexpr std::uint32_t HandleHighShift = 9;
        inline constexpr std::uint32_t HandleLowMask = 0x1FF;

        // Entity list addressing constants.
        inline constexpr std::uint64_t ListStart = 0x10;
        inline constexpr std::uint64_t ChunkStride = 0x8;
        inline constexpr std::uint64_t EntryStride = 0x70;
    }

    namespace Schema
    {
        // Base fields loaded from client_dll.json.
        inline std::uint32_t m_hPawn = 0;
        inline std::uint32_t m_hPlayerPawn = 0;
        inline std::uint32_t m_iszPlayerName = 0;
        inline std::uint32_t m_pInGameMoneyServices = 0;
        inline std::uint32_t m_iAccount = 0;
        inline std::uint32_t m_iHealth = 0;
        inline std::uint32_t m_iMaxHealth = 0;
        inline std::uint32_t m_iTeamNum = 0;
        inline std::uint32_t m_lifeState = 0;
        inline std::uint32_t m_pGameSceneNode = 0;
        inline std::uint32_t m_pWeaponServices = 0;
        inline std::uint32_t m_pObserverServices = 0;
        inline std::uint32_t m_vecAbsOrigin = 0;
        inline std::uint32_t m_vOldOrigin = 0;
        inline std::uint32_t m_vecViewOffset = 0;
        inline std::uint32_t m_modelState = 0;
        inline std::uint32_t m_ArmorValue = 0;
        inline std::uint32_t m_bIsScoped = 0;
        inline std::uint32_t m_flFlashDuration = 0;
        inline std::uint32_t m_flFlashMaxAlpha = 0;
        inline std::uint32_t m_flFlashOverlayAlpha = 0;
        inline std::uint32_t m_iIDEntIndex = 0;
        inline std::uint32_t m_iShotsFired = 0;
        inline std::uint32_t m_aimPunchAngle = 0;
        inline std::uint32_t m_iObserverMode = 0;
        inline std::uint32_t m_hObserverTarget = 0;
        inline std::uint32_t m_hActiveWeapon = 0;
        inline std::uint32_t m_AttributeManager = 0;
        inline std::uint32_t m_Item = 0;
        inline std::uint32_t m_iItemDefinitionIndex = 0;
        inline std::uint32_t m_bInReload = 0;
        inline std::uint32_t m_bHasDefuser = 0;
        inline std::uint32_t m_bBombTicking = 0;
        inline std::uint32_t m_bBombDefused = 0;
        inline std::uint32_t m_nBombSite = 0;
        inline std::uint32_t m_flC4Blow = 0;
        inline std::uint32_t m_flTimerLength = 0;
        inline std::uint32_t m_flDefuseLength = 0;
        inline std::uint32_t m_bBeingDefused = 0;
        inline std::uint32_t m_flDefuseCountDown = 0;
        inline std::uint32_t m_vecC4ExplodeSpectatePos = 0;
        inline std::uint32_t m_bFreezePeriod = 0;
    }

    namespace Layout
    {
        // In CS2 model state keeps a pointer to the packed bone data.
        inline constexpr std::uint32_t BoneArray = 0x80;
        inline constexpr std::uint32_t BoneStride = 0x20;
    }

    inline bool HasCore()
    {
        return Client::dwLocalPlayerController != 0 &&
               Client::dwLocalPlayerPawn != 0 &&
               Client::dwEntityList != 0 &&
               Client::dwViewMatrix != 0;
    }
}
