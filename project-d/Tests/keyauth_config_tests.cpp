#include <cstdlib>
#include <iostream>
#include <string>

#include "Security/KeyAuthConfig.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(const std::string& actual, const std::string& expected, const std::string& message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << " actual=" << actual << " expected=" << expected << '\n';
        return false;
    }

    void SetEnvValue(const char* name, const char* value)
    {
        _putenv_s(name, value);
    }
}

int main()
{
    SetEnvValue("PROJECTD_KEYAUTH_ENABLED", "");
    SetEnvValue("PROJECTD_KEYAUTH_API_BASE", "");
    SetEnvValue("PROJECTD_KEYAUTH_APP_NAME", "");
    SetEnvValue("PROJECTD_KEYAUTH_OWNER_ID", "");
    SetEnvValue("PROJECTD_KEYAUTH_APP_SECRET", "");
    SetEnvValue("PROJECTD_KEYAUTH_LICENSE_KEY", "");
    SetEnvValue("PROJECTD_KEYAUTH_HARDWARE_ID", "");

    bool ok = true;

    const auto emptyConfig = Security::KeyAuthConfig::FromEnvironment();
    ok &= ExpectTrue(!emptyConfig.Enabled, "empty env should default KeyAuth to disabled");
    ok &= ExpectEqual(emptyConfig.ApiBase, "https://keyauth.win/api/1.3/", "empty env should use KeyAuth default api base");
    ok &= ExpectTrue(!emptyConfig.HasRequiredFields(), "empty env should be incomplete");

    SetEnvValue("PROJECTD_KEYAUTH_ENABLED", "true");
    SetEnvValue("PROJECTD_KEYAUTH_API_BASE", "https://keyauth.win/api/1.3/");
    SetEnvValue("PROJECTD_KEYAUTH_APP_NAME", "project-d");
    SetEnvValue("PROJECTD_KEYAUTH_OWNER_ID", "owner-id");
    SetEnvValue("PROJECTD_KEYAUTH_APP_SECRET", "app-secret");
    SetEnvValue("PROJECTD_KEYAUTH_LICENSE_KEY", "license-key");
    SetEnvValue("PROJECTD_KEYAUTH_HARDWARE_ID", "hwid-123");

    const auto populatedConfig = Security::KeyAuthConfig::FromEnvironment();
    ok &= ExpectTrue(populatedConfig.Enabled, "enabled env should enable KeyAuth");
    ok &= ExpectEqual(populatedConfig.AppName, "project-d", "app name should come from env");
    ok &= ExpectEqual(populatedConfig.OwnerId, "owner-id", "owner id should come from env");
    ok &= ExpectEqual(populatedConfig.AppSecret, "app-secret", "app secret should come from env");
    ok &= ExpectEqual(populatedConfig.LicenseKey, "license-key", "license key should come from env");
    ok &= ExpectEqual(populatedConfig.HardwareId, "hwid-123", "hardware id should come from env");
    ok &= ExpectTrue(populatedConfig.HasRequiredFields(), "full env should satisfy required fields");

    if (!ok)
        return 1;

    std::cout << "[PASS] keyauth_config_tests\n";
    return 0;
}
