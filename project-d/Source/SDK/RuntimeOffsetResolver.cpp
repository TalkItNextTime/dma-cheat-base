#include <Pch.hpp>
#include "RuntimeOffsetResolver.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace RuntimeOffsetResolver
{
    namespace
    {
        enum class CaptureKind
        {
            RipRelative,
            ImmediateU32
        };

        struct PatternDefinition
        {
            const char* Name;
            const char* Pattern;
            size_t CaptureOffset;
            CaptureKind Capture;
        };

        template <typename T>
        struct UtlVector
        {
            std::int32_t Count;
            std::uint8_t Pad0[0x4];
            std::uint64_t Data;
        };

        struct TsListHead
        {
            std::uint64_t Next;
        };

        struct TsListBase
        {
            TsListHead Head;
        };

        struct UtlMemoryPool
        {
            std::int32_t BlockSize;
            std::int32_t BlocksPerBlob;
            std::uint32_t GrowMode;
            std::int32_t BlocksAllocated;
            std::int32_t PeakAllocated;
            std::uint16_t Alignment;
            std::uint16_t BlobCount;
            std::uint8_t Pad0[0x2];
            TsListBase FreeBlocks;
            std::uint8_t Pad1[0x20];
            std::uint64_t BlobHead;
            std::int32_t TotalSize;
            std::uint8_t Pad2[0xC];
        };

        template <typename T>
        struct UtlTsHashAllocatedBlob
        {
            std::uint64_t Next;
            std::uint8_t Pad0[0x8];
            std::uint64_t Data;
            std::uint8_t Pad1[0x18];
        };

        template <typename T, typename K = std::uint64_t>
        struct UtlTsHashFixedData
        {
            K Key;
            std::uint64_t Next;
            std::uint64_t Data;
        };

        template <typename T, typename K = std::uint64_t>
        struct UtlTsHashBucket
        {
            std::uint64_t AddLock;
            std::uint64_t First;
            std::uint64_t FirstUncommitted;
        };

        template <typename T, size_t BucketCount = 256, typename K = std::uint64_t>
        struct UtlTsHash
        {
            UtlMemoryPool EntryMem;
            UtlTsHashBucket<T, K> Buckets[BucketCount];
            bool NeedsCommit;
            std::uint8_t Pad0[0x3];
            std::int32_t ContentionCheck;
            std::uint8_t Pad1[0x8];
        };

        struct SchemaSystem
        {
            std::uint8_t Pad0[0x190];
            UtlVector<std::uint64_t> TypeScopes;
            std::uint8_t Pad1[0xE0];
            std::int32_t RegistrationCount;
        };

        struct SchemaClassFieldData
        {
            std::uint64_t Name;
            std::uint64_t Type;
            std::int32_t Offset;
            std::int32_t MetadataCount;
            std::uint64_t Metadata;
        };

        struct SchemaClassInfoData
        {
            std::uint64_t Base;
            std::uint64_t Name;
            std::uint64_t BinaryName;
            std::uint64_t ModuleName;
            std::int32_t Size;
            std::int16_t FieldCount;
            std::int16_t StaticMetadataCount;
            std::uint8_t Pad0[0x2];
            std::uint8_t Alignment;
            std::uint8_t HasBaseClass;
            std::int16_t TotalClassSize;
            std::int16_t DerivedClassSize;
            std::uint64_t Fields;
            std::uint8_t Pad1[0x8];
            std::uint64_t BaseClasses;
            std::uint64_t StaticMetadata;
            std::uint8_t Pad2[0x8];
            std::uint64_t TypeScope;
            std::uint64_t Type;
            std::uint8_t Pad3[0x10];
        };

        struct SchemaSystemTypeScope
        {
            std::uint8_t Pad0[0x8];
            char Name[256];
            std::uint64_t GlobalScope;
            std::uint8_t Pad1[0x450];
            UtlTsHash<SchemaClassInfoData> ClassBindings;
        };

        static_assert(sizeof(UtlMemoryPool) == 0x60);
        static_assert(sizeof(UtlTsHash<SchemaClassInfoData>) == 0x1870);
        static_assert(sizeof(SchemaSystem) == 0x288);
        static_assert(sizeof(SchemaClassFieldData) == 0x20);
        static_assert(sizeof(SchemaClassInfoData) == 0x78);
        static_assert(sizeof(SchemaSystemTypeScope) == 0x1DD0);

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::string ReadInlineCString(const char* buffer, const size_t capacity)
        {
            if (!buffer || capacity == 0)
                return {};

            const auto terminator = (std::find)(buffer, buffer + capacity, '\0');
            return std::string(buffer, terminator);
        }

        std::vector<int> ParsePattern(const std::string_view pattern)
        {
            std::vector<int> bytes;
            std::stringstream stream{ std::string(pattern) };
            std::string token;

            while (stream >> token)
            {
                if (token == "?" || token == "??")
                {
                    bytes.push_back(-1);
                    continue;
                }

                bytes.push_back(std::stoi(token, nullptr, 16));
            }

            return bytes;
        }

        std::optional<std::uint64_t> ScanPatternValue(
            const std::vector<std::uint8_t>& moduleBytes,
            const PatternDefinition& definition)
        {
            const std::vector<int> pattern = ParsePattern(definition.Pattern);
            if (pattern.empty() || moduleBytes.size() < pattern.size())
                return std::nullopt;

            for (size_t start = 0; start + pattern.size() <= moduleBytes.size(); ++start)
            {
                bool matched = true;

                for (size_t i = 0; i < pattern.size(); ++i)
                {
                    if (pattern[i] >= 0 && moduleBytes[start + i] != static_cast<std::uint8_t>(pattern[i]))
                    {
                        matched = false;
                        break;
                    }
                }

                if (!matched)
                    continue;

                if (definition.CaptureOffset + sizeof(std::int32_t) > pattern.size())
                    return std::nullopt;

                std::int32_t rawValue = 0;
                std::memcpy(&rawValue, moduleBytes.data() + start + definition.CaptureOffset, sizeof(rawValue));

                if (definition.Capture == CaptureKind::ImmediateU32)
                    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(rawValue));

                const auto target = static_cast<std::int64_t>(start + definition.CaptureOffset + sizeof(rawValue)) + rawValue;
                if (target < 0)
                    return std::nullopt;

                return static_cast<std::uint64_t>(target);
            }

            return std::nullopt;
        }

        bool ReadModuleBytes(
            Memory& memory,
            const char* moduleName,
            std::uint64_t& moduleBase,
            std::vector<std::uint8_t>& moduleBytes,
            std::string& errorMessage)
        {
            moduleBase = memory.GetBaseDaddy(moduleName);
            const std::uint64_t moduleSize = memory.GetBaseSize(moduleName);

            if (!moduleBase || !moduleSize)
            {
                errorMessage = std::string("module not loaded: ") + moduleName;
                return false;
            }

            std::string readFailureDetail;
            if (!ReadMemoryRangeWithFallback(
                moduleBase,
                static_cast<size_t>(moduleSize),
                moduleBytes,
                [&](const std::uint64_t address, void* buffer, const size_t size)
                {
                    return memory.Read(address, buffer, size);
                },
                readFailureDetail,
                0x100000,
                0x1000,
                true))
            {
                errorMessage = std::string("failed to read module bytes: ") + moduleName;
                if (!readFailureDetail.empty())
                    errorMessage += " (" + readFailureDetail + ")";
                return false;
            }

            if (!readFailureDetail.empty())
                LOG_WARN("Module {} snapshot contains unreadable page(s): {}", moduleName, readFailureDetail);

            return true;
        }

        std::string ReadCString(Memory& memory, const std::uint64_t address, const size_t maxLength = 128)
        {
            if (!address || maxLength == 0)
                return {};

            std::string buffer;
            buffer.reserve(maxLength);

            for (size_t index = 0; index < maxLength; ++index)
            {
                char value = '\0';
                if (!memory.Read(address + index, &value, sizeof(value)))
                    break;

                if (value == '\0')
                    break;

                buffer.push_back(value);
            }

            return buffer;
        }

        template <typename T>
        std::vector<std::uint64_t> EnumerateTsHashPointers(Memory& memory, const UtlTsHash<T>& hash)
        {
            std::vector<std::uint64_t> result;
            std::unordered_set<std::uint64_t> seen;

            const size_t allocatedLimit = hash.EntryMem.BlocksAllocated > 0
                ? static_cast<size_t>(hash.EntryMem.BlocksAllocated)
                : 0u;

            for (const auto& bucket : hash.Buckets)
            {
                std::uint64_t nodeAddress = bucket.FirstUncommitted;

                while (nodeAddress)
                {
                    const auto node = memory.Read<UtlTsHashFixedData<T>>(nodeAddress);
                    if (node.Data && seen.insert(node.Data).second)
                        result.push_back(node.Data);

                    if (allocatedLimit != 0 && result.size() >= allocatedLimit)
                        break;

                    nodeAddress = node.Next;
                }

                if (allocatedLimit != 0 && result.size() >= allocatedLimit)
                    break;
            }

            const size_t freeLimit = hash.EntryMem.PeakAllocated > 0
                ? static_cast<size_t>(hash.EntryMem.PeakAllocated)
                : 0u;

            std::uint64_t blobAddress = hash.EntryMem.FreeBlocks.Head.Next;
            size_t freeCount = 0;

            while (blobAddress && (freeLimit == 0 || freeCount < freeLimit))
            {
                const auto blob = memory.Read<UtlTsHashAllocatedBlob<T>>(blobAddress);
                if (blob.Data && seen.insert(blob.Data).second)
                    result.push_back(blob.Data);

                blobAddress = blob.Next;
                ++freeCount;
            }

            return result;
        }

        bool ResolveCoreOffsets(
            Memory& memory,
            OffsetValueMap& clientOffsets,
            OffsetValueMap& engineOffsets,
            OffsetValueMap& soundSystemOffsets,
            std::string& errorMessage)
        {
            std::uint64_t clientBase = 0;
            std::vector<std::uint8_t> clientBytes;
            if (!ReadModuleBytes(memory, "client.dll", clientBase, clientBytes, errorMessage))
                return false;

            constexpr PatternDefinition clientPatterns[] = {
                { "dwEntityList", "48 89 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC", 3, CaptureKind::RipRelative },
                { "dwGameEntitySystem", "48 8B 1D ?? ?? ?? ?? 48 89 1D ?? ?? ?? ?? 4C 63 B3", 3, CaptureKind::RipRelative },
                { "dwGameEntitySystem_highestEntityIndex", "FF 81 ?? ?? ?? ?? 48 85 D2", 2, CaptureKind::ImmediateU32 },
                { "dwGameRules", "F6 C1 01 0F 85 ?? ?? ?? ?? 4C 8B 05 ?? ?? ?? ?? 4D 85", 12, CaptureKind::RipRelative },
                { "dwGlobalVars", "48 89 15 ?? ?? ?? ?? 48 89 42", 3, CaptureKind::RipRelative },
                { "dwLocalPlayerController", "48 8B 05 ?? ?? ?? ?? 41 89 BE", 3, CaptureKind::RipRelative },
                { "dwPlantedC4", "48 8B 15 ?? ?? ?? ?? 41 FF C0 48 8D 4C 24 ?? 44 89 05 ?? ?? ?? ??", 3, CaptureKind::RipRelative },
                { "dwViewMatrix", "48 8D 0D ?? ?? ?? ?? 48 C1 E0 06", 3, CaptureKind::RipRelative },
                { "dwWeaponC4", "48 8B 15 ?? ?? ?? ?? 48 8B 5C 24 ?? FF C0 89 05 ?? ?? ?? ?? 48 8B C6 48 89 34 EA 80 BE", 3, CaptureKind::RipRelative }
            };

            for (const auto& definition : clientPatterns)
            {
                if (const auto value = ScanPatternValue(clientBytes, definition); value.has_value())
                    clientOffsets.emplace(definition.Name, *value);
            }

            if (const auto csgoInput = ScanPatternValue(
                clientBytes,
                { "dwCSGOInput", "48 89 05 ?? ?? ?? ?? 0F 57 C0 0F 11 05", 3, CaptureKind::RipRelative });
                csgoInput.has_value())
            {
                if (const auto viewAnglesOffset = ScanPatternValue(
                    clientBytes,
                    { "dwViewAnglesOffset", "F2 42 0F 10 84 28 ?? ?? ?? ??", 6, CaptureKind::ImmediateU32 });
                    viewAnglesOffset.has_value())
                {
                    clientOffsets["dwViewAngles"] = *csgoInput + *viewAnglesOffset;
                }
            }

            if (const auto prediction = ScanPatternValue(
                clientBytes,
                { "dwPrediction", "48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54", 3, CaptureKind::RipRelative });
                prediction.has_value())
            {
                if (const auto localPawnOffset = ScanPatternValue(
                    clientBytes,
                    { "dwLocalPlayerPawnOffset", "4C 39 B6 ?? ?? ?? ?? 74 ?? 44 88 BE", 3, CaptureKind::ImmediateU32 });
                    localPawnOffset.has_value())
                {
                    clientOffsets["dwLocalPlayerPawn"] = *prediction + *localPawnOffset;
                }
            }

            std::uint64_t engineBase = 0;
            std::vector<std::uint8_t> engineBytes;
            if (!ReadModuleBytes(memory, "engine2.dll", engineBase, engineBytes, errorMessage))
                return false;

            constexpr PatternDefinition enginePatterns[] = {
                { "dwNetworkGameClient", "48 89 3D ?? ?? ?? ?? FF 87", 3, CaptureKind::RipRelative },
                { "dwNetworkGameClient_localPlayer", "42 8B 94 D3 ?? ?? ?? ?? 5B 49 FF E3 32 C0 5B C3", 4, CaptureKind::ImmediateU32 },
                { "dwNetworkGameClient_maxClients", "8B 81 ?? ?? ?? ?? C3 ?? ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? C3 ?? ?? ?? ?? ?? 8B 81", 2, CaptureKind::ImmediateU32 },
                { "dwNetworkGameClient_signOnState", "44 8B 81 ?? ?? ?? ?? 48 8D 0D", 3, CaptureKind::ImmediateU32 }
            };

            for (const auto& definition : enginePatterns)
            {
                if (const auto value = ScanPatternValue(engineBytes, definition); value.has_value())
                    engineOffsets.emplace(definition.Name, *value);
            }

            const std::filesystem::path dumpCandidates[] = {
                std::filesystem::current_path() / "project-d" / "Offsets" / "offsets.json",
                std::filesystem::current_path() / "Offsets" / "offsets.json",
                std::filesystem::current_path().parent_path() / "project-d" / "Offsets" / "offsets.json"
            };

            for (const auto& dumpPath : dumpCandidates)
            {
                std::ifstream stream(dumpPath);
                if (!stream.good())
                    continue;

                try
                {
                    const json offsetsJson = json::parse(stream);
                    if (offsetsJson.contains("soundsystem.dll") && offsetsJson["soundsystem.dll"].is_object())
                    {
                        const auto& soundJson = offsetsJson["soundsystem.dll"];
                        if (soundJson.contains("dwSoundSystem"))
                            soundSystemOffsets["dwSoundSystem"] = soundJson["dwSoundSystem"].get<std::uint64_t>();
                        if (soundJson.contains("dwSoundSystem_engineViewData"))
                            soundSystemOffsets["dwSoundSystem_engineViewData"] = soundJson["dwSoundSystem_engineViewData"].get<std::uint64_t>();
                    }
                }
                catch (...)
                {
                    soundSystemOffsets.clear();
                }

                break;
            }

            ApplyResolvedOffsets(clientOffsets, engineOffsets, soundSystemOffsets);

            if (!Offsets::HasCore())
            {
                errorMessage = "runtime core offset scan is incomplete";
                return false;
            }

            return true;
        }

        bool ReadClientSchemaClasses(Memory& memory, SchemaClassMap& classes, std::string& errorMessage)
        {
            std::uint64_t schemaModuleBase = 0;
            std::vector<std::uint8_t> schemaModuleBytes;
            if (!ReadModuleBytes(memory, "schemasystem.dll", schemaModuleBase, schemaModuleBytes, errorMessage))
                return false;

            const auto schemaSystemOffset = ScanPatternValue(
                schemaModuleBytes,
                { "SchemaSystem", "4C 8D 35 ?? ?? ?? ?? 0F 28 45", 3, CaptureKind::RipRelative });

            if (!schemaSystemOffset.has_value())
            {
                errorMessage = "failed to locate SchemaSystem";
                return false;
            }

            const auto schemaSystem = memory.Read<SchemaSystem>(schemaModuleBase + *schemaSystemOffset);
            if (schemaSystem.RegistrationCount == 0 || !schemaSystem.TypeScopes.Data || schemaSystem.TypeScopes.Count <= 0)
            {
                errorMessage = "schema system is not initialized";
                return false;
            }

            for (std::int32_t scopeIndex = 0; scopeIndex < schemaSystem.TypeScopes.Count; ++scopeIndex)
            {
                const std::uint64_t scopePointer =
                    memory.Read<std::uint64_t>(schemaSystem.TypeScopes.Data + static_cast<std::uint64_t>(scopeIndex) * sizeof(std::uint64_t));
                if (!scopePointer)
                    continue;

                const auto scope = memory.Read<SchemaSystemTypeScope>(scopePointer);
                const std::string scopeName = ReadInlineCString(scope.Name, sizeof(scope.Name));
                for (const std::uint64_t classPointer : EnumerateTsHashPointers(memory, scope.ClassBindings))
                {
                    if (!classPointer)
                        continue;

                    const auto binding = memory.Read<SchemaClassInfoData>(classPointer);
                    const std::string className = ReadCString(memory, binding.Name);
                    if (className.empty())
                        continue;

                    const std::string moduleName = ReadCString(memory, binding.ModuleName);
                    if (!IsClientSchemaSource(scopeName, moduleName) || !binding.Fields || binding.FieldCount <= 0)
                        continue;

                    auto& fields = classes[className];
                    for (std::int32_t fieldIndex = 0; fieldIndex < binding.FieldCount; ++fieldIndex)
                    {
                        const std::uint64_t fieldAddress =
                            binding.Fields + static_cast<std::uint64_t>(fieldIndex) * sizeof(SchemaClassFieldData);
                        const auto field = memory.Read<SchemaClassFieldData>(fieldAddress);
                        const std::string fieldName = ReadCString(memory, field.Name);
                        if (fieldName.empty())
                            continue;

                        fields[fieldName] = static_cast<std::uint32_t>(field.Offset);
                    }
                }
            }

            if (classes.empty())
            {
                errorMessage = "no client schema classes were discovered";
                return false;
            }

            return true;
        }
    }

    bool Resolve(Memory& memory, ResolveReport& report, std::string& errorMessage)
    {
        OffsetValueMap clientOffsets;
        OffsetValueMap engineOffsets;
        OffsetValueMap soundSystemOffsets;
        report.SchemaInitialized = false;
        report.SchemaComplete = false;
        report.SchemaErrorMessage.clear();
        report.MissingRequiredSchemaFields.clear();

        if (!ResolveCoreOffsets(memory, clientOffsets, engineOffsets, soundSystemOffsets, errorMessage))
        {
            ApplyResolvedOffsets({}, {});
            ResolveReport resetReport;
            ApplyResolvedSchemas({}, resetReport);
            return false;
        }

        SchemaClassMap schemaClasses;
        if (!ReadClientSchemaClasses(memory, schemaClasses, errorMessage))
        {
            ResolveReport resetReport;
            ApplyResolvedSchemas({}, resetReport);
            report.SchemaInitialized = false;
            report.SchemaComplete = false;
            report.SchemaErrorMessage = errorMessage;
            return true;
        }

        report.SchemaInitialized = true;
        report.SchemaErrorMessage.clear();
        ApplyResolvedSchemas(schemaClasses, report);
        return true;
    }
}
