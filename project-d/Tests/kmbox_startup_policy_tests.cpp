#include <iostream>
#include <string>

#include "Kmbox/StartupPolicy.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectFalse(const bool condition, const std::string& message)
    {
        return ExpectTrue(!condition, message);
    }
}

int main()
{
    bool ok = true;

    const auto failedInit = StartupPolicy::EvaluateKmboxInitialization(true, 1);
    ok &= ExpectTrue(failedInit.ContinueStartup, "KMBOX init failure should not abort startup");
    ok &= ExpectFalse(failedInit.KmboxInitialized, "KMBOX init failure should leave KMBOX disconnected");
    ok &= ExpectTrue(failedInit.EmitWarning, "KMBOX init failure should emit a warning");

    const auto successfulInit = StartupPolicy::EvaluateKmboxInitialization(true, 0);
    ok &= ExpectTrue(successfulInit.ContinueStartup, "KMBOX init success should continue startup");
    ok &= ExpectTrue(successfulInit.KmboxInitialized, "KMBOX init success should mark KMBOX connected");
    ok &= ExpectFalse(successfulInit.EmitWarning, "KMBOX init success should not emit a warning");

    const auto disabledInit = StartupPolicy::EvaluateKmboxInitialization(false, -1);
    ok &= ExpectTrue(disabledInit.ContinueStartup, "disabled KMBOX should continue startup");
    ok &= ExpectFalse(disabledInit.KmboxInitialized, "disabled KMBOX should stay disconnected");
    ok &= ExpectFalse(disabledInit.EmitWarning, "disabled KMBOX should not emit a warning");

    if (!ok)
        return 1;

    std::cout << "[PASS] kmbox_startup_policy_tests\n";
    return 0;
}
