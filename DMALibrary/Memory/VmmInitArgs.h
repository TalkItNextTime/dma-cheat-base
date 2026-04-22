#pragma once

#include <vector>

namespace dma_vmm
{
    inline std::vector<const char*> BuildInitializationArgs(const bool debug)
    {
        std::vector<const char*> args{
            "",
            "-device",
            "fpga://algo=0",
            "-waitinitialize"
        };

        if (debug)
        {
            args.push_back("-v");
            args.push_back("-printf");
        }

        return args;
    }
}
