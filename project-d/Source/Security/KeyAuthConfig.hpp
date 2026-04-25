#pragma once

#include <cstdlib>
#include <string>

namespace Security
{
    struct KeyAuthConfig
    {
        bool Enabled = false;
        std::string ApiBase = "https://keyauth.win/api/1.3/";
        std::string AppName{};
        std::string OwnerId{};
        std::string AppSecret{};
        std::string LicenseKey{};
        std::string HardwareId{};

        [[nodiscard]] bool HasRequiredFields() const
        {
            return !AppName.empty() &&
                !OwnerId.empty() &&
                !AppSecret.empty() &&
                !LicenseKey.empty();
        }

        [[nodiscard]] static KeyAuthConfig FromEnvironment()
        {
            KeyAuthConfig config;
            config.Enabled = ParseBool(ReadEnvironment("PROJECTD_KEYAUTH_ENABLED"));

            const std::string apiBase = ReadEnvironment("PROJECTD_KEYAUTH_API_BASE");
            if (!apiBase.empty())
                config.ApiBase = apiBase;

            config.AppName = ReadEnvironment("PROJECTD_KEYAUTH_APP_NAME");
            config.OwnerId = ReadEnvironment("PROJECTD_KEYAUTH_OWNER_ID");
            config.AppSecret = ReadEnvironment("PROJECTD_KEYAUTH_APP_SECRET");
            config.LicenseKey = ReadEnvironment("PROJECTD_KEYAUTH_LICENSE_KEY");
            config.HardwareId = ReadEnvironment("PROJECTD_KEYAUTH_HARDWARE_ID");
            return config;
        }

    private:
        [[nodiscard]] static std::string ReadEnvironment(const char* name)
        {
            if (name == nullptr || *name == '\0')
                return {};

            const char* value = std::getenv(name);
            if (value == nullptr)
                return {};

            return value;
        }

        [[nodiscard]] static bool ParseBool(std::string value)
        {
            for (char& ch : value)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            return value == "1" || value == "true" || value == "yes" || value == "on";
        }
    };
}
