#include <iostream>
#include <string>

#include "SDK/OffsetInitializationPolicy.hpp"

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

    const auto missingCore = OffsetInitializationPolicy::EvaluateUpdateLoop(false, false, 0);
    ok &= ExpectTrue(missingCore.AttemptReload, "missing core offsets should trigger automatic reload attempts");
    ok &= ExpectTrue(!missingCore.RefreshCoreCache, "missing core offsets should not refresh the core cache");
    ok &= ExpectTrue(missingCore.EmitPausedWarning, "missing core offsets should emit a paused warning");
    ok &= ExpectEqual(missingCore.SleepMilliseconds, 500, "missing core offsets should use the retry interval");

    const auto waitingSchemaNoRetry = OffsetInitializationPolicy::EvaluateUpdateLoop(true, false, 200);
    ok &= ExpectTrue(!waitingSchemaNoRetry.AttemptReload, "schema warmup should respect the retry interval");
    ok &= ExpectTrue(waitingSchemaNoRetry.RefreshCoreCache, "schema warmup should keep core cache updates running");
    ok &= ExpectTrue(!waitingSchemaNoRetry.EmitPausedWarning, "schema warmup should not emit paused warnings when core is already ready");
    ok &= ExpectEqual(waitingSchemaNoRetry.SleepMilliseconds, 2, "schema warmup should keep the fast core refresh interval");

    const auto waitingSchemaRetry = OffsetInitializationPolicy::EvaluateUpdateLoop(true, false, 500);
    ok &= ExpectTrue(waitingSchemaRetry.AttemptReload, "schema warmup should retry once the retry interval elapses");
    ok &= ExpectTrue(waitingSchemaRetry.RefreshCoreCache, "schema warmup retry should still keep core refresh enabled");
    ok &= ExpectTrue(!waitingSchemaRetry.EmitPausedWarning, "schema warmup retry should stay quiet after core is available");
    ok &= ExpectEqual(waitingSchemaRetry.SleepMilliseconds, 2, "schema warmup retry should keep the fast core refresh interval");

    const auto readyCore = OffsetInitializationPolicy::EvaluateUpdateLoop(true, true, 2000);
    ok &= ExpectTrue(!readyCore.AttemptReload, "fully initialized offsets should not trigger reload attempts");
    ok &= ExpectTrue(readyCore.RefreshCoreCache, "fully initialized offsets should keep core cache updates running");
    ok &= ExpectTrue(!readyCore.EmitPausedWarning, "fully initialized offsets should not emit paused warnings");
    ok &= ExpectEqual(readyCore.SleepMilliseconds, 2, "fully initialized offsets should use the fast update interval");

    if (!ok)
        return 1;

    std::cout << "[PASS] offset_initialization_policy_tests\n";
    return 0;
}
