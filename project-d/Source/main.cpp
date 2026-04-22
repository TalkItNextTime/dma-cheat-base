#include <Pch.hpp>

#include <Features.hpp>
#include <Overlay.hpp>
#include <Radar/Radar.hpp>

int main()
{
    SetConsoleTitleA("Console - Debug");
    spdlog::set_level(spdlog::level::trace);

    cout << R"(
     _______ _______ _______ _______ _______ _______ 
    |\     /|\     /|\     /|\     /|\     /|\     /|
    | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
    | |   | | |   | | |   | | |   | | |   | | |   | |
    | |A  | | |W  | | |H  | | |A  | | |R  | | |E  | |
    | +---+ | +---+ | +---+ | +---+ | +---+ | +---+ |
    |/_____\|/_____\|/_____\|/_____\|/_____\|/_____\|
)" << '\n';

    if (!c_exception_handler::setup())
    {
        LOG_ERROR("Failed to setup Exception Handler");
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    if (!config.Init())
    {
        LOG_ERROR("Failed to initialize Config");
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    LOG_INFO("Config initialized, proceeding to KMBOX/DMA/SDK startup");

    if (config.Kmbox.Enabled)
    {
        LOG_INFO("Initializing KMBOX");
        if (Kmbox.InitDevice(config.Kmbox.Ip, config.Kmbox.Port, config.Kmbox.Uuid) == 0)
        {
            ProcInfo::KmboxInitialized = true;
        }
        else
        {
            LOG_ERROR("Failed to initialize KMBOX");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            return 1;
        }
    }
    else
    {
        ProcInfo::KmboxInitialized = false;
        LOG_INFO("KMBOX disabled in config");
    }

    LOG_INFO("Initializing DMA");
    if (!dma.Init())
    {
        LOG_ERROR("Failed to initialize DMA");
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    LOG_INFO("Initializing SDK");
    if (!sdk.Init())
    {
        LOG_ERROR("Failed to initialize SDK");
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

	LOG_INFO("Initializing feature threads");
	if (!features.Init())
    {
		LOG_ERROR("Failed to initialize Features");
		this_thread::sleep_for(chrono::seconds(5));
		return 1;
	}

    LOG_INFO("Starting radar bridge");
    radarBridge.Start();

    PerfDebug::SetVisDebugTick([]()
    {
        esp.UpdateVisCheckDebugOverlayFromDebugThread();
    });

    LOG_INFO("Creating overlay and ImGui");
    if (!overlay.Create())
    {
		LOG_ERROR("Failed to create Overlay");
        radarBridge.Stop();
		this_thread::sleep_for(chrono::seconds(5));
		return 1;
	}

    LOG_INFO("Initialization complete! Press INSERT to open the menu");

    while (overlay.shouldRun)
    {
        PerfDebug::SetDebugOptions(config.DebugEnabled, config.DebugPerf, config.DebugTrigger, config.DebugVisCheck, config.DebugAutowall);
        PerfDebug::SyncDebugThread();

        TIMER("Global render");

        overlay.StartRender();

        if (overlay.shouldRenderMenu)
            overlay.RenderMenu();

        ImDrawList* drawList = overlay.GetBackgroundDrawList();
        if (!drawList)
            continue;

        esp.Update(drawList);

        overlay.EndRender();
    }

	Globals::Running = false;
    radarBridge.Stop();
    PerfDebug::SetVisDebugTick({});
    PerfDebug::ShutdownDebugThread();
	overlay.Destroy();

    system("pause");
    return 0;
}
