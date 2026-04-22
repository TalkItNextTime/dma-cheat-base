#include <cstdint>
#include <iostream>
#include <string>

#include "Memory/InputManagerAddressResolver.h"
#include "Memory/VmmInitArgs.h"

namespace
{
    bool ExpectEqual(const std::uint64_t actual, const std::uint64_t expected, const char* const message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: 0x" << std::hex << expected << "\n"
                  << "  actual  : 0x" << actual << std::dec << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    constexpr std::uint64_t kInvalidEatAddress = 0x0;
    constexpr std::uint64_t kResolvedEatAddress = 0xFFFF90403B001000ull;
    constexpr std::uint64_t kResolvedPdbAddress = 0xFFFF90403B0498A0ull;

    ok &= ExpectEqual(
        dma_keyboard::ResolveLegacyGafAsyncKeyStateAddress(kInvalidEatAddress, kResolvedPdbAddress),
        kResolvedPdbAddress,
        "旧版 Windows 在 EAT 失败但 PDB 成功时，必须使用 PDB 解析出的 gafAsyncKeyState 地址");
    ok &= ExpectEqual(
        dma_keyboard::ResolveLegacyGafAsyncKeyStateAddress(kResolvedEatAddress, kResolvedPdbAddress),
        kResolvedEatAddress,
        "旧版 Windows 在 EAT 已成功时，不应被 PDB 结果覆盖");
    ok &= ExpectEqual(
        dma_keyboard::ShouldTryLegacyPdbLookup(false, kInvalidEatAddress),
        true,
        "旧版 Windows 在 EAT 查询直接失败时，仍然必须继续尝试 PDB");
    ok &= ExpectEqual(
        dma_keyboard::KeyboardInitFailureMessage(dma_keyboard::KeyboardInitFailure::PdbLoadFailed) != nullptr,
        true,
        "键盘初始化失败原因必须能被格式化为可输出日志");
    {
        const auto args = dma_vmm::BuildInitializationArgs(false);
        bool foundWaitInitialize = false;
        for (const auto arg : args)
        {
            if (std::string(arg) == "-waitinitialize")
            {
                foundWaitInitialize = true;
                break;
            }
        }
        ok &= ExpectEqual(
            foundWaitInitialize,
            true,
            "VMM 初始化参数必须包含 -waitinitialize，避免符号相关 API 在异步初始化完成前调用");
    }

    if (!ok)
        return 1;

    std::cout << "[PASS] dma_keyboard_init_tests\n";
    return 0;
}
