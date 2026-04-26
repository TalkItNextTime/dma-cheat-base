#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Offsets.hpp"

class Memory;

namespace RuntimeOffsetResolver
{
    using OffsetValueMap = std::unordered_map<std::string, std::uint64_t>;
    using SchemaFieldMap = std::unordered_map<std::string, std::uint32_t>;
    using SchemaClassMap = std::unordered_map<std::string, SchemaFieldMap>;

    inline std::string NormalizeSchemaModuleName(std::string_view moduleName)
    {
        if (moduleName.empty())
            return {};

        std::string normalized(moduleName);
        if (const auto terminator = normalized.find('\0'); terminator != std::string::npos)
            normalized.erase(terminator);

        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });

        if (normalized == "client" || normalized == "client.dll")
            return "client.dll";

        if (normalized.find(".dll") == std::string::npos &&
            normalized.find('.') == std::string::npos)
        {
            normalized += ".dll";
        }

        return normalized;
    }

    inline bool IsClientSchemaSource(std::string_view scopeName, std::string_view moduleName)
    {
        return NormalizeSchemaModuleName(scopeName) == "client.dll" ||
               NormalizeSchemaModuleName(moduleName) == "client.dll";
    }

    template <typename ReadFn>
    inline bool ReadMemoryRangeWithFallback(
        const std::uint64_t baseAddress,
        const size_t size,
        std::vector<std::uint8_t>& buffer,
        ReadFn&& readBlock,
        std::string& errorDetail,
        const size_t initialChunkSize = 0x100000,
        const size_t minimumChunkSize = 0x1000,
        const bool zeroFillUnreadablePages = false)
    {
        errorDetail.clear();
        buffer.clear();

        if (size == 0)
            return true;

        buffer.resize(size);

        size_t offset = 0;
        const size_t firstChunkSize = (std::max)(initialChunkSize, minimumChunkSize);

        while (offset < size)
        {
            const size_t remaining = size - offset;
            size_t chunkSize = (std::min)(firstChunkSize, remaining);
            bool chunkRead = false;

            while (chunkSize != 0)
            {
                if (readBlock(baseAddress + offset, buffer.data() + offset, chunkSize))
                {
                    offset += chunkSize;
                    chunkRead = true;
                    break;
                }

                if (chunkSize <= minimumChunkSize || chunkSize == 1)
                    break;

                chunkSize = (std::max)(minimumChunkSize, chunkSize / 2);
                if (chunkSize > remaining)
                    chunkSize = remaining;
            }

            if (chunkRead)
                continue;

            if (zeroFillUnreadablePages && remaining >= minimumChunkSize)
            {
                const size_t fillSize = (std::min)(minimumChunkSize, remaining);
                std::fill(buffer.begin() + static_cast<std::ptrdiff_t>(offset),
                    buffer.begin() + static_cast<std::ptrdiff_t>(offset + fillSize),
                    0);

                if (!errorDetail.empty())
                    errorDetail += "; ";

                std::ostringstream stream;
                stream << "zero-filled unreadable module page at offset +0x" << std::hex << offset
                       << " (address 0x" << (baseAddress + offset) << ")";
                errorDetail += stream.str();

                offset += fillSize;
                continue;
            }

            std::ostringstream stream;
            stream << "read failed at module offset +0x" << std::hex << offset
                   << " (address 0x" << (baseAddress + offset)
                   << ", remaining 0x" << remaining << ")";
            errorDetail = stream.str();
            buffer.clear();
            return false;
        }

        return true;
    }

    struct ResolveReport
    {
        bool SchemaInitialized = false;
        bool SchemaComplete = true;
        std::string SchemaErrorMessage;
        std::vector<std::string> MissingRequiredSchemaFields;
    };

    inline std::uint64_t ReadResolvedOffset(const OffsetValueMap& offsets, const std::string_view name)
    {
        const auto it = offsets.find(std::string(name));
        return it != offsets.end() ? it->second : 0ull;
    }

    inline void ApplyResolvedOffsets(
        const OffsetValueMap& clientOffsets,
        const OffsetValueMap& engineOffsets,
        const OffsetValueMap& soundSystemOffsets = {})
    {
        Offsets::Client::dwLocalPlayerController = ReadResolvedOffset(clientOffsets, "dwLocalPlayerController");
        Offsets::Client::dwLocalPlayerPawn = ReadResolvedOffset(clientOffsets, "dwLocalPlayerPawn");
        Offsets::Client::dwEntityList = ReadResolvedOffset(clientOffsets, "dwEntityList");
        Offsets::Client::dwGameEntitySystem = ReadResolvedOffset(clientOffsets, "dwGameEntitySystem");
        Offsets::Client::dwGameEntitySystem_highestEntityIndex = ReadResolvedOffset(clientOffsets, "dwGameEntitySystem_highestEntityIndex");
        Offsets::Client::dwGameRules = ReadResolvedOffset(clientOffsets, "dwGameRules");
        Offsets::Client::dwPlantedC4 = ReadResolvedOffset(clientOffsets, "dwPlantedC4");
        Offsets::Client::dwWeaponC4 = ReadResolvedOffset(clientOffsets, "dwWeaponC4");
        Offsets::Client::dwGlobalVars = ReadResolvedOffset(clientOffsets, "dwGlobalVars");
        Offsets::Client::dwViewAngles = ReadResolvedOffset(clientOffsets, "dwViewAngles");
        Offsets::Client::dwViewMatrix = ReadResolvedOffset(clientOffsets, "dwViewMatrix");

        Offsets::Engine2::dwNetworkGameClient = ReadResolvedOffset(engineOffsets, "dwNetworkGameClient");
        Offsets::Engine2::dwNetworkGameClient_localPlayer = ReadResolvedOffset(engineOffsets, "dwNetworkGameClient_localPlayer");
        Offsets::Engine2::dwNetworkGameClient_signOnState = ReadResolvedOffset(engineOffsets, "dwNetworkGameClient_signOnState");
        Offsets::Engine2::dwNetworkGameClient_maxClients = ReadResolvedOffset(engineOffsets, "dwNetworkGameClient_maxClients");

        Offsets::SoundSystem::dwSoundSystem = ReadResolvedOffset(soundSystemOffsets, "dwSoundSystem");
        Offsets::SoundSystem::dwSoundSystem_engineViewData = ReadResolvedOffset(soundSystemOffsets, "dwSoundSystem_engineViewData");
    }

    inline std::optional<std::uint32_t> FindSchemaField(
        const SchemaClassMap& classes,
        const std::initializer_list<std::string_view> classNames,
        const std::string_view fieldName)
    {
        for (const std::string_view className : classNames)
        {
            const auto classIt = classes.find(std::string(className));
            if (classIt == classes.end())
                continue;

            const auto fieldIt = classIt->second.find(std::string(fieldName));
            if (fieldIt != classIt->second.end())
                return fieldIt->second;
        }

        return std::nullopt;
    }

    inline std::string FormatSchemaRequirement(
        const std::initializer_list<std::string_view> classNames,
        const std::string_view fieldName)
    {
        std::string result;
        bool first = true;

        for (const std::string_view className : classNames)
        {
            if (!first)
                result += " | ";

            result += className;
            first = false;
        }

        result += "::";
        result += fieldName;
        return result;
    }

    inline void AssignRequiredSchemaField(
        const SchemaClassMap& classes,
        ResolveReport& report,
        const std::initializer_list<std::string_view> classNames,
        const std::string_view fieldName,
        std::uint32_t& target)
    {
        if (const auto value = FindSchemaField(classes, classNames, fieldName); value.has_value())
        {
            target = *value;
            return;
        }

        target = 0;
        report.SchemaComplete = false;
        report.MissingRequiredSchemaFields.push_back(FormatSchemaRequirement(classNames, fieldName));
    }

    inline void AssignOptionalSchemaField(
        const SchemaClassMap& classes,
        const std::initializer_list<std::string_view> classNames,
        const std::string_view fieldName,
        std::uint32_t& target)
    {
        if (const auto value = FindSchemaField(classes, classNames, fieldName); value.has_value())
        {
            target = *value;
            return;
        }

        target = 0;
    }

    inline bool ApplyResolvedSchemas(const SchemaClassMap& classes, ResolveReport& report)
    {
        report.SchemaComplete = true;
        report.MissingRequiredSchemaFields.clear();

        AssignRequiredSchemaField(classes, report, { "CBasePlayerController" }, "m_hPawn", Offsets::Schema::m_hPawn);
        AssignRequiredSchemaField(classes, report, { "CCSPlayerController" }, "m_hPlayerPawn", Offsets::Schema::m_hPlayerPawn);
        AssignOptionalSchemaField(classes, { "CCSPlayerController" }, "m_iCompTeammateColor", Offsets::Schema::m_iCompTeammateColor);
        AssignOptionalSchemaField(classes, { "CCSPlayerController" }, "m_bPawnHasDefuser", Offsets::Schema::m_bPawnHasDefuser);
        AssignOptionalSchemaField(classes, { "CCSPlayerController" }, "m_bPawnHasHelmet", Offsets::Schema::m_bPawnHasHelmet);
        AssignOptionalSchemaField(classes, { "CCSPlayerController", "CBasePlayerController" }, "m_iConnected", Offsets::Schema::m_iConnected);
        AssignOptionalSchemaField(classes, { "CCSPlayerController", "CBasePlayerController" }, "m_steamID", Offsets::Schema::m_steamID);
        AssignRequiredSchemaField(classes, report, { "CBasePlayerController" }, "m_iszPlayerName", Offsets::Schema::m_iszPlayerName);
        AssignRequiredSchemaField(classes, report, { "CCSPlayerController" }, "m_pInGameMoneyServices", Offsets::Schema::m_pInGameMoneyServices);
        AssignRequiredSchemaField(classes, report, { "CCSPlayerController_InGameMoneyServices" }, "m_iAccount", Offsets::Schema::m_iAccount);
        AssignRequiredSchemaField(classes, report, { "C_BaseEntity" }, "m_iHealth", Offsets::Schema::m_iHealth);
        AssignRequiredSchemaField(classes, report, { "C_BaseEntity" }, "m_iMaxHealth", Offsets::Schema::m_iMaxHealth);
        AssignRequiredSchemaField(classes, report, { "C_BaseEntity" }, "m_iTeamNum", Offsets::Schema::m_iTeamNum);
        AssignRequiredSchemaField(classes, report, { "C_BaseEntity" }, "m_lifeState", Offsets::Schema::m_lifeState);
        AssignRequiredSchemaField(classes, report, { "C_BaseEntity" }, "m_pGameSceneNode", Offsets::Schema::m_pGameSceneNode);
        AssignRequiredSchemaField(classes, report, { "CGameSceneNode" }, "m_vecAbsOrigin", Offsets::Schema::m_vecAbsOrigin);
        AssignRequiredSchemaField(classes, report, { "C_BasePlayerPawn" }, "m_pWeaponServices", Offsets::Schema::m_pWeaponServices);
        AssignRequiredSchemaField(classes, report, { "C_BasePlayerPawn" }, "m_pObserverServices", Offsets::Schema::m_pObserverServices);
        AssignRequiredSchemaField(classes, report, { "C_BasePlayerPawn" }, "m_vOldOrigin", Offsets::Schema::m_vOldOrigin);
        AssignRequiredSchemaField(classes, report, { "C_BaseModelEntity" }, "m_vecViewOffset", Offsets::Schema::m_vecViewOffset);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawnBase", "C_CSPlayerPawn" }, "m_angEyeAngles", Offsets::Schema::m_angEyeAngles);
        AssignRequiredSchemaField(classes, report, { "CSkeletonInstance" }, "m_modelState", Offsets::Schema::m_modelState);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawn" }, "m_ArmorValue", Offsets::Schema::m_ArmorValue);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawn" }, "m_bIsScoped", Offsets::Schema::m_bIsScoped);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawnBase" }, "m_flFlashDuration", Offsets::Schema::m_flFlashDuration);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawnBase" }, "m_flFlashMaxAlpha", Offsets::Schema::m_flFlashMaxAlpha);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawnBase" }, "m_flFlashOverlayAlpha", Offsets::Schema::m_flFlashOverlayAlpha);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawn" }, "m_iIDEntIndex", Offsets::Schema::m_iIDEntIndex);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawn" }, "m_iShotsFired", Offsets::Schema::m_iShotsFired);
        AssignRequiredSchemaField(classes, report, { "C_CSPlayerPawn" }, "m_pAimPunchServices", Offsets::Schema::m_pAimPunchServices);
        AssignRequiredSchemaField(classes, report, { "CCSPlayer_AimPunchServices" }, "m_predictableBaseAngle", Offsets::Schema::m_predictableBaseAngle);
        AssignRequiredSchemaField(classes, report, { "CCSPlayer_AimPunchServices" }, "m_unpredictableBaseAngle", Offsets::Schema::m_unpredictableBaseAngle);
        AssignRequiredSchemaField(classes, report, { "CPlayer_ObserverServices" }, "m_iObserverMode", Offsets::Schema::m_iObserverMode);
        AssignRequiredSchemaField(classes, report, { "CPlayer_ObserverServices" }, "m_hObserverTarget", Offsets::Schema::m_hObserverTarget);
        AssignRequiredSchemaField(classes, report, { "CPlayer_WeaponServices" }, "m_hActiveWeapon", Offsets::Schema::m_hActiveWeapon);
        AssignOptionalSchemaField(classes, { "CPlayer_WeaponServices" }, "m_hMyWeapons", Offsets::Schema::m_hMyWeapons);
        AssignRequiredSchemaField(classes, report, { "C_EconEntity" }, "m_AttributeManager", Offsets::Schema::m_AttributeManager);
        AssignRequiredSchemaField(classes, report, { "C_AttributeContainer" }, "m_Item", Offsets::Schema::m_Item);
        AssignRequiredSchemaField(classes, report, { "C_EconItemView" }, "m_iItemDefinitionIndex", Offsets::Schema::m_iItemDefinitionIndex);
        AssignOptionalSchemaField(classes, { "C_BasePlayerWeapon", "C_CSWeaponBase", "CWeaponBaseItem" }, "m_iClip1", Offsets::Schema::m_iClip1);
        AssignRequiredSchemaField(classes, report, { "C_CSWeaponBase" }, "m_bInReload", Offsets::Schema::m_bInReload);
        AssignOptionalSchemaField(classes, { "C_BaseEntity", "CBasePlayerWeapon", "C_CSWeaponBase" }, "m_hOwnerEntity", Offsets::Schema::m_hOwnerEntity);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawn" }, "m_bInBuyZone", Offsets::Schema::m_bInBuyZone);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawn" }, "m_bHasDefuser", Offsets::Schema::m_bHasDefuser);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawn" }, "m_bHasHelmet", Offsets::Schema::m_bHasHelmet);
        AssignOptionalSchemaField(classes, { "C_Team" }, "m_iScore", Offsets::Schema::m_iScore);
        AssignOptionalSchemaField(classes, { "C_Team" }, "m_szTeamname", Offsets::Schema::m_szTeamname);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_bBombTicking", Offsets::Schema::m_bBombTicking);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_bBombDefused", Offsets::Schema::m_bBombDefused);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_nBombSite", Offsets::Schema::m_nBombSite);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_flC4Blow", Offsets::Schema::m_flC4Blow);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_flTimerLength", Offsets::Schema::m_flTimerLength);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_flDefuseLength", Offsets::Schema::m_flDefuseLength);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_bBeingDefused", Offsets::Schema::m_bBeingDefused);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_flDefuseCountDown", Offsets::Schema::m_flDefuseCountDown);
        AssignOptionalSchemaField(classes, { "C_PlantedC4" }, "m_hBombDefuser", Offsets::Schema::m_hBombDefuser);
        AssignRequiredSchemaField(classes, report, { "C_PlantedC4" }, "m_vecC4ExplodeSpectatePos", Offsets::Schema::m_vecC4ExplodeSpectatePos);
        AssignRequiredSchemaField(classes, report, { "C_C4" }, "m_bStartedArming", Offsets::Schema::m_bStartedArming);
        AssignRequiredSchemaField(classes, report, { "C_C4" }, "m_bIsPlantingViaUse", Offsets::Schema::m_bIsPlantingViaUse);
        AssignRequiredSchemaField(classes, report, { "C_C4" }, "m_fArmedTime", Offsets::Schema::m_fArmedTime);
        AssignRequiredSchemaField(classes, report, { "C_CSGameRules" }, "m_bFreezePeriod", Offsets::Schema::m_bFreezePeriod);
        AssignOptionalSchemaField(classes, { "C_CSGameRules" }, "m_gamePhase", Offsets::Schema::m_gamePhase);
        AssignOptionalSchemaField(classes, { "C_CSGameRules" }, "m_timeUntilNextPhaseStarts", Offsets::Schema::m_timeUntilNextPhaseStarts);
        AssignOptionalSchemaField(classes, { "CCSPlayerController" }, "m_recentKillQueue", Offsets::Schema::m_recentKillQueue);
        AssignOptionalSchemaField(classes, { "C_SoundEventEntity" }, "m_onSoundFinished", Offsets::Schema::m_onSoundFinished);
        AssignOptionalSchemaField(classes, { "C_SoundEventEntity" }, "m_iszSoundName", Offsets::Schema::m_iszSoundName);
        AssignOptionalSchemaField(classes, { "C_CSPlayerPawn" }, "m_flEmitSoundTime", Offsets::Schema::m_flEmitSoundTime);
        AssignOptionalSchemaField(classes, { "C_CSWeaponBase" }, "m_nLastEmptySoundCmdNum", Offsets::Schema::m_nLastEmptySoundCmdNum);

        return report.SchemaComplete;
    }

    bool Resolve(Memory& memory, ResolveReport& report, std::string& errorMessage);
}
