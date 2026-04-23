#pragma once

namespace OffsetInitializationPolicy
{
    struct UpdateLoopDecision
    {
        bool AttemptReload = false;
        bool RefreshCoreCache = false;
        bool EmitPausedWarning = false;
        int SleepMilliseconds = 2;
    };

    inline UpdateLoopDecision EvaluateUpdateLoop(
        const bool hasCoreOffsets,
        const bool schemaInitialized,
        const int millisecondsSinceLastRetry)
    {
        if (!hasCoreOffsets)
        {
            return {
                true,
                false,
                true,
                500
            };
        }

        if (!schemaInitialized && millisecondsSinceLastRetry >= 500)
        {
            return {
                true,
                true,
                false,
                2
            };
        }

        return {
            false,
            true,
            false,
            2
        };
    }
}
