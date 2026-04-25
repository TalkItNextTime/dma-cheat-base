#pragma once

#include <string>

#define PROJECT_D_STARTUP_STATUS_BUILD_TEXT_EN "Build: Developer"
#define PROJECT_D_STARTUP_STATUS_BUILD_TEXT_ZH "版本: 开发版"
#define PROJECT_D_STARTUP_STATUS_EXPIRY_TEXT_EN "Expires: Never"
#define PROJECT_D_STARTUP_STATUS_EXPIRY_TEXT_ZH "到期: 永不"

namespace StartupStatus
{
inline std::string BuildTextEn = PROJECT_D_STARTUP_STATUS_BUILD_TEXT_EN;
inline std::string BuildTextZh = PROJECT_D_STARTUP_STATUS_BUILD_TEXT_ZH;
inline std::string ExpiryTextEn = PROJECT_D_STARTUP_STATUS_EXPIRY_TEXT_EN;
inline std::string ExpiryTextZh = PROJECT_D_STARTUP_STATUS_EXPIRY_TEXT_ZH;

inline void SetBuildText(std::string english, std::string chinese)
{
    BuildTextEn = std::move(english);
    BuildTextZh = std::move(chinese);
}

inline void SetExpiryText(std::string english, std::string chinese)
{
    ExpiryTextEn = std::move(english);
    ExpiryTextZh = std::move(chinese);
}

inline const char* GetBuildText(const bool chinese)
{
    return chinese ? BuildTextZh.c_str() : BuildTextEn.c_str();
}

inline const char* GetExpiryText(const bool chinese)
{
    return chinese ? ExpiryTextZh.c_str() : ExpiryTextEn.c_str();
}
}
