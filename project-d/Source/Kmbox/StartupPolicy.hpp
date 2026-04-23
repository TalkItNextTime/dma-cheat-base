#pragma once

namespace StartupPolicy
{
    struct KmboxInitializationDecision
    {
        bool ContinueStartup = true;
        bool KmboxInitialized = false;
        bool EmitWarning = false;
    };

    inline KmboxInitializationDecision EvaluateKmboxInitialization(const bool enabled, const int initResult)
    {
        if (!enabled)
            return {};

        if (initResult == 0)
        {
            return {
                true,
                true,
                false
            };
        }

        return {
            true,
            false,
            true
        };
    }
}
