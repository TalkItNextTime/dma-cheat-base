#include <iostream>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "SDK/Offsets.hpp"
#include "SDK/RuntimeOffsetResolver.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    template <typename T>
    bool ExpectEqual(const T& actual, const T& expected, const std::string& message)
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

    RuntimeOffsetResolver::OffsetValueMap clientOffsets{
        { "dwLocalPlayerController", 0x10 },
        { "dwLocalPlayerPawn", 0x20 },
        { "dwEntityList", 0x30 },
        { "dwViewMatrix", 0x40 },
        { "dwGameRules", 0x50 }
    };
    RuntimeOffsetResolver::OffsetValueMap engineOffsets{
        { "dwNetworkGameClient", 0x60 },
        { "dwNetworkGameClient_localPlayer", 0x70 }
    };

    Offsets::Client::dwWeaponC4 = 0xFFFF;
    Offsets::Engine2::dwNetworkGameClient_signOnState = 0xFFFF;
    RuntimeOffsetResolver::ApplyResolvedOffsets(clientOffsets, engineOffsets);

    ok &= ExpectEqual(Offsets::Client::dwLocalPlayerController, 0x10ull, "core controller offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Client::dwLocalPlayerPawn, 0x20ull, "core pawn offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Client::dwEntityList, 0x30ull, "entity list offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Client::dwViewMatrix, 0x40ull, "view matrix offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Client::dwGameRules, 0x50ull, "game rules offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Client::dwWeaponC4, 0x0ull, "missing client offsets should reset to zero instead of keeping stale values");
    ok &= ExpectEqual(Offsets::Engine2::dwNetworkGameClient, 0x60ull, "engine network client offset should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Engine2::dwNetworkGameClient_localPlayer, 0x70ull, "engine local player field should map from runtime dump result");
    ok &= ExpectEqual(Offsets::Engine2::dwNetworkGameClient_signOnState, 0x0ull, "missing engine offsets should reset to zero");

    RuntimeOffsetResolver::SchemaClassMap schemaClasses{
        { "CBasePlayerController", { { "m_hPawn", 0x10 }, { "m_iConnected", 0x20 } } },
        { "CCSPlayerController", { { "m_hPlayerPawn", 0x30 }, { "m_steamID", 0x40 } } }
    };

    const auto pawnHandle =
        RuntimeOffsetResolver::FindSchemaField(schemaClasses, { "CCSPlayerController", "CBasePlayerController" }, "m_hPawn");
    const auto steamId =
        RuntimeOffsetResolver::FindSchemaField(schemaClasses, { "CBasePlayerController", "CCSPlayerController" }, "m_steamID");
    const auto missing =
        RuntimeOffsetResolver::FindSchemaField(schemaClasses, { "C_CSPlayerPawn" }, "m_iHealth");

    ok &= ExpectTrue(pawnHandle.has_value(), "schema lookup should search fallback class candidates");
    ok &= ExpectEqual(pawnHandle.value_or(0), 0x10u, "schema lookup should return the first matching fallback field");
    ok &= ExpectTrue(steamId.has_value(), "schema lookup should continue into later candidate classes");
    ok &= ExpectEqual(steamId.value_or(0), 0x40u, "schema lookup should find fields in the later fallback class");
    ok &= ExpectTrue(!missing.has_value(), "schema lookup should report missing fields cleanly");

    ok &= ExpectTrue(
        RuntimeOffsetResolver::IsClientSchemaSource("client.dll", ""),
        "client schema scope should be accepted even when class module name is absent");
    ok &= ExpectTrue(
        RuntimeOffsetResolver::IsClientSchemaSource("schemasystem.dll", "client"),
        "client class module should be accepted even when discovered under another scope");
    ok &= ExpectTrue(
        !RuntimeOffsetResolver::IsClientSchemaSource("server.dll", ""),
        "non-client scope should be rejected when no client module tag exists");

    std::vector<std::uint8_t> sourceBytes(0x3000);
    for (size_t index = 0; index < sourceBytes.size(); ++index)
        sourceBytes[index] = static_cast<std::uint8_t>(index & 0xFF);

    std::vector<std::uint8_t> recoveredBytes;
    std::string readFailureDetail;
    const bool recoveredWithFallback = RuntimeOffsetResolver::ReadMemoryRangeWithFallback(
        0x5000,
        sourceBytes.size(),
        recoveredBytes,
        [&](const std::uint64_t address, void* buffer, const size_t size)
        {
            if (size > 0x1000)
                return false;

            const auto offset = static_cast<size_t>(address - 0x5000);
            std::memcpy(buffer, sourceBytes.data() + offset, size);
            return true;
        },
        readFailureDetail);

    ok &= ExpectTrue(recoveredWithFallback, "chunked range reader should recover when only smaller reads succeed");
    ok &= ExpectEqual(recoveredBytes.size(), sourceBytes.size(), "chunked range reader should preserve the full range length");
    ok &= ExpectTrue(recoveredBytes == sourceBytes, "chunked range reader should rebuild the original bytes exactly");
    ok &= ExpectTrue(readFailureDetail.empty(), "successful chunked range reads should not report an error");

    std::vector<std::uint8_t> sparseRecoveredBytes;
    std::string sparseReadDetail;
    const bool recoveredSparseModule = RuntimeOffsetResolver::ReadMemoryRangeWithFallback(
        0x9000,
        sourceBytes.size(),
        sparseRecoveredBytes,
        [&](const std::uint64_t address, void* buffer, const size_t size)
        {
            const auto offset = static_cast<size_t>(address - 0x9000);
            if (offset >= 0x1000 && offset < 0x2000)
                return false;

            std::memcpy(buffer, sourceBytes.data() + offset, size);
            return true;
        },
        sparseReadDetail,
        0x1000,
        0x1000,
        true);

    ok &= ExpectTrue(recoveredSparseModule, "range reader should tolerate unreadable pages when sparse reads are enabled");
    ok &= ExpectEqual(sparseRecoveredBytes.size(), sourceBytes.size(), "sparse range reader should preserve the full module size");
    ok &= ExpectTrue(
        std::equal(sparseRecoveredBytes.begin(), sparseRecoveredBytes.begin() + 0x1000, sourceBytes.begin()),
        "sparse range reader should preserve bytes before the unreadable page");
    ok &= ExpectTrue(
        std::all_of(sparseRecoveredBytes.begin() + 0x1000, sparseRecoveredBytes.begin() + 0x2000, [](const std::uint8_t value) { return value == 0; }),
        "sparse range reader should zero-fill unreadable pages");
    ok &= ExpectTrue(
        std::equal(sparseRecoveredBytes.begin() + 0x2000, sparseRecoveredBytes.end(), sourceBytes.begin() + 0x2000),
        "sparse range reader should preserve bytes after the unreadable page");
    ok &= ExpectTrue(
        sparseReadDetail.find("zero-filled unreadable module page") != std::string::npos,
        "sparse range reader should report that unreadable pages were zero-filled");

    if (!ok)
        return 1;

    std::cout << "[PASS] offset_dumper_tests\n";
    return 0;
}
