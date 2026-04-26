#include <Pch.hpp>

#include <Features.hpp>
#include <ESP/SoundEsp.hpp>
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

    std::filesystem::path GetExecutableDirectory()
    {
        std::array<char, MAX_PATH> modulePath{};
        const DWORD size = GetModuleFileNameA(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (size == 0 || size >= modulePath.size())
            return std::filesystem::current_path();

        return std::filesystem::path(modulePath.data()).parent_path();
    }

    std::filesystem::path LicenseCachePath()
    {
        return GetExecutableDirectory() / "keyauth_license.txt";
    }

    std::string LoadCachedLicenseKey()
    {
        std::ifstream input(LicenseCachePath(), std::ios::in | std::ios::binary);
        if (!input)
            return {};

        std::string value;
        std::getline(input, value);
        return TrimWhitespace(std::move(value));
    }

    void SaveCachedLicenseKey(const std::string& licenseKey)
    {
        std::ofstream output(LicenseCachePath(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (output)
            output << licenseKey << '\n';
    }

    void DeleteCachedLicenseKey()
    {
        std::error_code ignored{};
        std::filesystem::remove(LicenseCachePath(), ignored);
    }

    std::string ReadMaskedLine()
    {
        std::string value{};
        while (true)
        {
            const int ch = _getch();
            if (ch == '\r' || ch == '\n')
            {
                cout << '\n';
                break;
            }

            if (ch == 3)
            {
                value.clear();
                cout << '\n';
                break;
            }

            if (ch == '\b')
            {
                if (!value.empty())
                {
                    value.pop_back();
                    cout << "\b \b";
                }
                continue;
            }

            if (ch == 0 || ch == 0xE0)
            {
                (void)_getch();
                continue;
            }

            if (std::isprint(static_cast<unsigned char>(ch)) && value.size() < 255)
            {
                value.push_back(static_cast<char>(ch));
                cout << '*';
            }
        }

        return TrimWhitespace(std::move(value));
    }

    int ConsoleWidth()
    {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
            return 80;

        const int width = info.srWindow.Right - info.srWindow.Left + 1;
        return width > 0 ? width : 80;
    }

    void PrintCenteredLine(const std::string& text, const int width, const int displayWidth = -1)
    {
        const int effectiveWidth = displayWidth >= 0 ? displayWidth : static_cast<int>(text.size());
        const int padding = (std::max)(0, (width - effectiveWidth) / 2);
        cout << std::string(static_cast<std::size_t>(padding), ' ') << text << '\n';
    }

    void ShowFarewellConsole()
    {
        system("cls");

        const int width = ConsoleWidth();
        cout << "\n\n\n\n";
        PrintCenteredLine("期待下次相遇", width, 12);
        cout << '\n';
        PrintCenteredLine("UNTIL WE MEET AGAIN", width);
        cout << "\n\n";
        this_thread::sleep_for(chrono::milliseconds(2500));
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

        std::string cachedLicenseKey = LoadCachedLicenseKey();
        while (true)
        {
            cout << "[KeyAuth] Enter license key: ";
            std::string licenseKey{};
            if (!cachedLicenseKey.empty())
            {
                licenseKey = cachedLicenseKey;
                cout << std::string(licenseKey.size(), '*') << '\n';
                ZeroString(cachedLicenseKey);
            }
            else
            {
                licenseKey = ReadMaskedLine();
            }

            if (licenseKey.empty())
            {
                cout << "[KeyAuth] License key cannot be empty." << '\n';
                continue;
            }

            g_ConsoleAuthSession.Api->license(licenseKey, "");

            if (g_ConsoleAuthSession.Api->response.success)
            {
                SaveCachedLicenseKey(licenseKey);
                ZeroString(licenseKey);
                const std::string expiryTimestamp = FirstSubscriptionExpiry(g_ConsoleAuthSession.Api->user_data);
                const std::string expiryDisplay = expiryTimestamp.empty() ? "Unknown" : KeyAuth::api::expiry_remaining(expiryTimestamp);
                UpdateStartupStatusForAuth(g_ConsoleAuthSession.Version, expiryDisplay);

                std::thread(checkAuthenticated, g_ConsoleAuthSession.OwnerId).detach();

                cout << "[KeyAuth] " << (g_ConsoleAuthSession.Api->response.message.empty() ? "Authentication successful." : g_ConsoleAuthSession.Api->response.message) << '\n';
                cout << "[KeyAuth] Subscription time remaining: " << expiryDisplay << '\n';
                return { true, {} };
            }

            const std::string failureMessage = g_ConsoleAuthSession.Api->response.message.empty() ? "License authentication failed." : g_ConsoleAuthSession.Api->response.message;
            ZeroString(licenseKey);
            DeleteCachedLicenseKey();
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
        cout << "[Startup] Initializing SDK..." << '\n';
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
    sdk.Shutdown();
    esp.Shutdown();
    soundEsp.Shutdown();
    features.Shutdown();
    radarBridge.Stop();
    PerfDebug::SetVisDebugTick({});
    PerfDebug::ShutdownDebugThread();
	overlay.Destroy();

    ShowFarewellConsole();
    return 0;
}
