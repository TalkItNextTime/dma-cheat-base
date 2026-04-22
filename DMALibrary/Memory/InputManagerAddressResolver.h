#pragma once

#include <cstdint>

namespace dma_keyboard
{
    enum class KeyboardInitFailure
    {
        None,
        MissingBuildNumber,
        MissingUbr,
        WinlogonNotFound,
        EatLookupFailed,
        EatVersionMismatch,
        ModuleInfoLookupFailed,
        PdbLoadFailed,
        PdbSymbolLookupFailed,
        AddressUnresolved
    };

    inline constexpr std::uint64_t kKernelAddressThreshold = 0x7FFFFFFFFFFFULL;

    inline constexpr const char* KeyboardInitFailureMessage(const KeyboardInitFailure failure)
    {
        switch (failure)
        {
        case KeyboardInitFailure::None:
            return "none";
        case KeyboardInitFailure::MissingBuildNumber:
            return "registry query failed: CurrentBuild";
        case KeyboardInitFailure::MissingUbr:
            return "registry query failed: UBR";
        case KeyboardInitFailure::WinlogonNotFound:
            return "winlogon.exe not found";
        case KeyboardInitFailure::EatLookupFailed:
            return "VMMDLL_Map_GetEATU failed";
        case KeyboardInitFailure::EatVersionMismatch:
            return "unexpected EAT map version";
        case KeyboardInitFailure::ModuleInfoLookupFailed:
            return "VMMDLL_Map_GetModuleFromNameW failed";
        case KeyboardInitFailure::PdbLoadFailed:
            return "VMMDLL_PdbLoad failed";
        case KeyboardInitFailure::PdbSymbolLookupFailed:
            return "VMMDLL_PdbSymbolAddress failed for gafAsyncKeyState";
        case KeyboardInitFailure::AddressUnresolved:
            return "gafAsyncKeyState address unresolved";
        default:
            return "unknown keyboard init failure";
        }
    }

    inline constexpr bool IsResolvedKernelAddress(const std::uint64_t address)
    {
        return address > kKernelAddressThreshold;
    }

    inline constexpr bool ShouldTryLegacyPdbLookup(
        const bool eatLookupSucceeded,
        const std::uint64_t eatAddress)
    {
        return !eatLookupSucceeded || !IsResolvedKernelAddress(eatAddress);
    }

    inline constexpr std::uint64_t ResolveLegacyGafAsyncKeyStateAddress(
        const std::uint64_t eatAddress,
        const std::uint64_t pdbAddress)
    {
        return IsResolvedKernelAddress(eatAddress) ? eatAddress : pdbAddress;
    }
}
