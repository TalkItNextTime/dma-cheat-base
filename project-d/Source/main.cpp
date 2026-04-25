#include <Pch.hpp>

#include <Features.hpp>
#include <Overlay.hpp>
#include <Radar/Radar.hpp>
#include <Kmbox/StartupPolicy.hpp>
#include <StartupStatus.hpp>
#include <StartupBanner.hpp>
#include <auth.hpp>
#include <auth_guard.hpp>
#include <array>
#include <limits>
#include <memory>

namespace
{
    struct RuntimeInitResult
    {
        bool Success = false;
        std::string Message{};
    };

    struct ConsoleAuthResult
    {
        bool Success = false;
        std::string Message{};
    };

    struct KeyAuthCredentials
    {
        std::string Name{};
        std::string OwnerId{};
        std::string Version{};
        std::string Url{};
        std::string Path{};
    };

    struct ConsoleAuthSession
    {
        std::unique_ptr<KeyAuth::api> Api{};
        std::string OwnerId{};
        std::string Version{};
    };

    ConsoleAuthSession g_ConsoleAuthSession{};

    void ZeroString(std::string& value)
    {
        if (!value.empty())
            SecureZeroMemory(value.data(), value.size());
        value.clear();
    }

    std::string TrimWhitespace(std::string value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            value.clear();
            return value;
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        value = value.substr(first, last - first + 1);
        return value;
    }

    KeyAuthCredentials LoadKeyAuthCredentials()
    {
        return {
            skCrypt("Cosmic13's Application").decrypt(),
            skCrypt("nvphVPbURR").decrypt(),
            skCrypt("1.0").decrypt(),
            skCrypt("https://keyauth.win/api/1.3/").decrypt(),
            skCrypt("").decrypt()
        };
    }

    std::string FirstSubscriptionExpiry(const KeyAuth::api::userdata& userData)
    {
        if (userData.subscriptions.empty())
            return {};

        return userData.subscriptions.front().expiry;
    }

    void UpdateStartupStatusForAuth(const std::string& version, const std::string& expiryText)
    {
        StartupStatus::SetBuildText("Version: " + version, "版本: " + version);
        StartupStatus::SetExpiryText("Expires: " + expiryText, "到期: " + expiryText);
    }

    ConsoleAuthResult AuthenticateWithConsole()
    {
        KeyAuthCredentials credentials = LoadKeyAuthCredentials();
        UpdateStartupStatusForAuth(credentials.Version, "Pending authentication");

        LOG_INFO("Initializing KeyAuth console authentication");
        cout << "[KeyAuth] Initializing..." << '\n';

        g_ConsoleAuthSession.Api = std::make_unique<KeyAuth::api>(
            credentials.Name,
            credentials.OwnerId,
            credentials.Version,
            credentials.Url,
            credentials.Path);
        g_ConsoleAuthSession.Api->enable_secure_strings(true);
        g_ConsoleAuthSession.Api->init();

        if (!g_ConsoleAuthSession.Api->response.success)
        {
            const std::string failureMessage = g_ConsoleAuthSession.Api->response.message.empty() ? "Failed to initialize KeyAuth." : g_ConsoleAuthSession.Api->response.message;
            UpdateStartupStatusForAuth(credentials.Version, "Authentication failed");
            g_ConsoleAuthSession.Api.reset();
            ZeroString(credentials.Name);
            ZeroString(credentials.OwnerId);
            ZeroString(credentials.Version);
            ZeroString(credentials.Url);
            ZeroString(credentials.Path);
            return { false, failureMessage };
        }

        g_ConsoleAuthSession.OwnerId = credentials.OwnerId;
        g_ConsoleAuthSession.Version = credentials.Version;
        ZeroString(credentials.Name);
        ZeroString(credentials.OwnerId);
        ZeroString(credentials.Url);
        ZeroString(credentials.Path);

        while (true)
        {
            std::array<char, 256> licenseBuffer{};
            cout << "[KeyAuth] Enter license key: ";
            cin.getline(licenseBuffer.data(), static_cast<std::streamsize>(licenseBuffer.size()));

            if (cin.fail())
            {
                cin.clear();
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                SecureZeroMemory(licenseBuffer.data(), licenseBuffer.size());
                cout << "[KeyAuth] Input too long, please try again." << '\n';
                continue;
            }

            std::string licenseKey = TrimWhitespace(std::string(licenseBuffer.data()));
            SecureZeroMemory(licenseBuffer.data(), licenseBuffer.size());

            if (licenseKey.empty())
            {
                cout << "[KeyAuth] License key cannot be empty." << '\n';
                continue;
            }

            g_ConsoleAuthSession.Api->license(licenseKey, "");
            ZeroString(licenseKey);

            if (g_ConsoleAuthSession.Api->response.success)
            {
                const std::string expiryTimestamp = FirstSubscriptionExpiry(g_ConsoleAuthSession.Api->user_data);
                const std::string expiryDisplay = expiryTimestamp.empty() ? "Unknown" : KeyAuth::api::expiry_remaining(expiryTimestamp);
                UpdateStartupStatusForAuth(g_ConsoleAuthSession.Version, expiryDisplay);

                std::thread(checkAuthenticated, g_ConsoleAuthSession.OwnerId).detach();

                cout << "[KeyAuth] " << (g_ConsoleAuthSession.Api->response.message.empty() ? "Authentication successful." : g_ConsoleAuthSession.Api->response.message) << '\n';
                cout << "[KeyAuth] Subscription time remaining: " << expiryDisplay << '\n';
                return { true, {} };
            }

            const std::string failureMessage = g_ConsoleAuthSession.Api->response.message.empty() ? "License authentication failed." : g_ConsoleAuthSession.Api->response.message;
            UpdateStartupStatusForAuth(g_ConsoleAuthSession.Version, "Authentication failed");
            cout << "[KeyAuth] " << failureMessage << '\n';
        }
    }

    RuntimeInitResult InitializeRuntime()
    {
        LOG_INFO("Config initialized, proceeding to KMBOX/DMA/SDK startup");

        if (config.Kmbox.Enabled)
        {
            LOG_INFO("Initializing KMBOX");
            const auto kmboxDecision = StartupPolicy::EvaluateKmboxInitialization(
                true,
                Kmbox.InitDevice(config.Kmbox.Ip, config.Kmbox.Port, config.Kmbox.Uuid));
            ProcInfo::KmboxInitialized = kmboxDecision.KmboxInitialized;

            if (kmboxDecision.EmitWarning)
                LOG_WARN("Failed to initialize KMBOX, continuing startup without KMBOX support");
        }
        else
        {
            ProcInfo::KmboxInitialized = false;
            LOG_INFO("KMBOX disabled in config");
        }

        LOG_INFO("Initializing DMA");
        if (!dma.Init())
            return { false, "Failed to initialize DMA." };

        LOG_INFO("Initializing SDK");
        if (!sdk.Init())
            return { false, "Failed to initialize SDK." };

        LOG_INFO("Initializing feature threads");
        if (!features.Init())
            return { false, "Failed to initialize feature threads." };

        LOG_INFO("Starting radar bridge");
        radarBridge.Start();

        PerfDebug::SetVisDebugTick([]()
        {
            esp.UpdateVisCheckDebugOverlayFromDebugThread();
        });

        return { true, "Protected runtime initialized." };
    }
}

int main()
{
    SetConsoleTitleA("Console - Debug");
    spdlog::set_level(spdlog::level::trace);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    cout << StartupBanner::Text << '\n';

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

    const ConsoleAuthResult authResult = AuthenticateWithConsole();
    if (!authResult.Success)
    {
        LOG_ERROR("{}", authResult.Message);
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    LOG_INFO("Creating overlay and ImGui");
    if (!overlay.Create())
    {
        LOG_ERROR("Failed to create Overlay");
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    const RuntimeInitResult runtimeResult = InitializeRuntime();
    if (!runtimeResult.Success)
    {
        LOG_ERROR("{}", runtimeResult.Message);
        overlay.Destroy();
        this_thread::sleep_for(chrono::seconds(5));
        return 1;
    }

    LOG_INFO("Initialization complete! Press INSERT to open the menu");

    while (overlay.shouldRun)
    {
        overlay.StartRender();

        PerfDebug::SetDebugOptions(config.DebugEnabled, config.DebugPerf, config.DebugTrigger, config.DebugVisCheck, config.DebugAutowall);
        PerfDebug::SyncDebugThread();

        TIMER("Global render");

        if (overlay.shouldRenderMenu)
            overlay.RenderMenu();

        ImDrawList* drawList = overlay.GetBackgroundDrawList();
        if (drawList != nullptr)
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
