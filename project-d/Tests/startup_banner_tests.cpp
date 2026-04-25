#include <iostream>
#include <string>
#include <string_view>

#include "StartupBanner.hpp"

namespace
{
    bool ExpectEqual(const std::string_view actual, const std::string_view expected, const std::string& message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
}

int main()
{
    constexpr std::string_view expectedBanner = R"(██████╗ ██████╗  ███████╗ ███╗   ███╗ ██╗ ██████╗     ██████╗ ███████╗   ████████═╗
██╔════╝ ██╔═══██╗██╔════╝ ████╗ ████║ ██║ ██╔════╝    ██╔════╝██╔════╝   ╚════███╔╝
██║      ██║   ██║███████╗ ██╔████╔██║ ██║ ██║         ██║     ███████╗     ███╔═╝  
██║      ██║   ██║╚════██║ ██║╚██╔╝██║ ██║ ██║         ██║     ╚════██║    ███ ╔╝   
╚██████╗ ╚██████╔╝███████║ ██║ ╚═╝ ██║ ██║ ╚██████╗    ╚██████╗███████║   ████████╗ 
 ╚═════╝  ╚═════╝ ╚══════╝ ╚═╝     ╚═╝ ╚═╝  ╚═════╝     ╚═════╝╚══════╝   ╚═══════╝ 

  版本: 开发版                                                         到期: 永不)";

    bool ok = true;
    ok &= ExpectEqual(StartupBanner::Text, expectedBanner, "startup banner should match menu status text");

    if (!ok)
        return 1;

    std::cout << "[PASS] startup_banner_tests\n";
    return 0;
}
