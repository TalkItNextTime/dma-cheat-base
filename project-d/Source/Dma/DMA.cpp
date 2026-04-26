#include "Pch.hpp"
#include "DMA.hpp"

bool DMA::Init()
{
    if (!mem.Init(GAME_NAME))
    {
        LOG_ERROR("Failed to initialize DMA");
        return 1;
    }

    Globals::ClientBase = mem.GetBaseDaddy(CLIENT_DLL);
    if (!Globals::ClientBase || Globals::ClientBase == NULL)
    {
        LOG_ERROR("Failed to get ClientBase");
        return false;
    }

    Globals::Engine2Base = mem.GetBaseDaddy(ENGINE2_DLL);
    if (!Globals::Engine2Base || Globals::Engine2Base == NULL)
    {
        LOG_WARN("Failed to get Engine2Base, map dependent features may be unavailable");
    }

    if (!mem.GetKeyboard()->InitKeyboard())
    {
        LOG_WARN(
            "Failed to initialize DMA Keyboard (reason: {}), falling back to host GetAsyncKeyState",
            mem.GetKeyboard()->GetLastInitFailure());
    }

    if (!mem.FixCr3())
    {
        LOG_ERROR("Failed to fix CR3");
		return false;
    }

    ProcInfo::DmaInitialized = true;

    return true;
}

bool DMA::RefreshGameBases(const bool reinitializeProcess)
{
    if (reinitializeProcess && !mem.Init(GAME_NAME))
    {
        Globals::ClientBase = 0;
        Globals::Engine2Base = 0;
        ProcInfo::DmaInitialized = false;
        return false;
    }

    const uint64_t clientBase = mem.GetBaseDaddy(CLIENT_DLL);
    if (!clientBase)
    {
        Globals::ClientBase = 0;
        Globals::Engine2Base = 0;
        ProcInfo::DmaInitialized = false;
        return false;
    }

    Globals::ClientBase = clientBase;
    Globals::Engine2Base = mem.GetBaseDaddy(ENGINE2_DLL);
    ProcInfo::DmaInitialized = true;
    return true;
}
