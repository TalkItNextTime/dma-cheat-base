#include <iostream>
#include <string>
#include <string_view>

#include "StartupBanner.hpp"

namespace
{
    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
}

int main()
{
    const std::string_view banner = StartupBanner::Text;
    bool ok = true;

    ok &= ExpectTrue(!banner.empty(), "startup banner should not be empty");
    ok &= ExpectTrue(banner.find(PROJECT_D_STARTUP_STATUS_BUILD_TEXT_ZH) == std::string_view::npos, "startup banner should not show build text");
    ok &= ExpectTrue(banner.find(PROJECT_D_STARTUP_STATUS_EXPIRY_TEXT_ZH) == std::string_view::npos, "startup banner should not show expiry text");

    if (!ok)
        return 1;

    std::cout << "[PASS] startup_banner_tests\n";
    return 0;
}
