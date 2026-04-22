#include <Pch.hpp>
#include <Radar/ObservRadarProtocol.hpp>
#include <Radar/Radar.hpp>

#include <array>
#include <winhttp.h>

namespace
{
    struct RadarSettings
    {
        bool Enabled = false;
        std::string Host = "127.0.0.1";
        int StaticPort = 36364;
        int IngestPort = 36365;
        int PublishIntervalMs = 40;
        int HttpTimeoutMs = 3000;
        int ReconnectBaseMs = 500;
        int ReconnectMaxMs = 5000;
    };

    struct ParsedHttpUrl
    {
        std::wstring Host{};
        INTERNET_PORT Port = 0;
        std::wstring PathAndQuery = L"/";
        bool Secure = false;
    };

    std::string TrimAscii(const std::string& value)
    {
        const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
        std::size_t begin = 0;
        while (begin < value.size() && isSpace(static_cast<unsigned char>(value[begin])))
            ++begin;

        std::size_t end = value.size();
        while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1])))
            --end;

        return value.substr(begin, end - begin);
    }

    std::wstring Utf8ToWide(const std::string& input)
    {
        if (input.empty())
            return {};

        const int charsNeeded = MultiByteToWideChar(
            CP_UTF8,
            0,
            input.data(),
            static_cast<int>(input.size()),
            nullptr,
            0
        );
        if (charsNeeded <= 0)
            return {};

        std::wstring output(static_cast<std::size_t>(charsNeeded), L'\0');
        const int converted = MultiByteToWideChar(
            CP_UTF8,
            0,
            input.data(),
            static_cast<int>(input.size()),
            output.data(),
            charsNeeded
        );
        if (converted <= 0)
            return {};

        return output;
    }

    std::uint64_t EpochMsNow()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    RadarSettings SnapshotSettings()
    {
        RadarSettings settings{};
        settings.Enabled = config.Radar.Enabled;
        settings.Host = TrimAscii(config.Radar.Host);
        if (settings.Host.empty())
            settings.Host = "127.0.0.1";
        settings.StaticPort = std::clamp(config.Radar.StaticPort, 1, 65535);
        settings.IngestPort = std::clamp(config.Radar.IngestPort, 1, 65535);
        settings.PublishIntervalMs = std::clamp(config.Radar.PublishIntervalMs, 40, 2000);
        settings.HttpTimeoutMs = std::clamp(config.Radar.HttpTimeoutMs, 300, 20000);
        settings.ReconnectBaseMs = std::clamp(config.Radar.ReconnectBaseMs, 100, 30000);
        settings.ReconnectMaxMs = std::clamp(config.Radar.ReconnectMaxMs, settings.ReconnectBaseMs, 120000);
        return settings;
    }

    bool ParseHttpUrl(const std::string& rawUrl, ParsedHttpUrl& outParsed)
    {
        const std::wstring wideUrl = Utf8ToWide(rawUrl);
        if (wideUrl.empty())
            return false;

        std::array<wchar_t, 256> hostBuffer{};
        std::array<wchar_t, 4096> pathBuffer{};
        std::array<wchar_t, 4096> extraBuffer{};

        URL_COMPONENTSW components{};
        components.dwStructSize = sizeof(components);
        components.lpszHostName = hostBuffer.data();
        components.dwHostNameLength = static_cast<DWORD>(hostBuffer.size());
        components.lpszUrlPath = pathBuffer.data();
        components.dwUrlPathLength = static_cast<DWORD>(pathBuffer.size());
        components.lpszExtraInfo = extraBuffer.data();
        components.dwExtraInfoLength = static_cast<DWORD>(extraBuffer.size());

        if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
            return false;

        if (components.dwHostNameLength == 0 || components.nPort == 0)
            return false;

        outParsed.Host.assign(components.lpszHostName, components.dwHostNameLength);
        outParsed.Port = components.nPort;
        outParsed.Secure = components.nScheme == INTERNET_SCHEME_HTTPS;
        outParsed.PathAndQuery.assign(components.lpszUrlPath, components.dwUrlPathLength);
        if (components.dwExtraInfoLength > 0)
            outParsed.PathAndQuery.append(components.lpszExtraInfo, components.dwExtraInfoLength);

        if (outParsed.PathAndQuery.empty())
            outParsed.PathAndQuery = L"/";

        return true;
    }

    bool ParseWebSocketUrl(const std::string& rawUrl, ParsedHttpUrl& outParsed)
    {
        if (rawUrl.rfind("ws://", 0) == 0)
            return ParseHttpUrl("http://" + rawUrl.substr(5), outParsed);
        if (rawUrl.rfind("wss://", 0) == 0)
            return ParseHttpUrl("https://" + rawUrl.substr(6), outParsed);
        return false;
    }
}

struct RadarBridge::Impl
{
    RadarSettings Settings{};

    HINTERNET Session = nullptr;
    HINTERNET WebSocket = nullptr;
    std::string ConnectedWebSocketUrl{};

    std::uint64_t LastSentSeq = 0;
    std::chrono::steady_clock::time_point LastSendTick{};
};

RadarBridge& RadarBridge::Get()
{
    static RadarBridge instance;
    return instance;
}

RadarBridge::RadarBridge() :
    m_Impl(std::make_unique<Impl>())
{
}

RadarBridge::~RadarBridge()
{
    Stop();
}

void RadarBridge::Start()
{
    bool expected = false;
    if (!m_Running.compare_exchange_strong(expected, true))
        return;

    m_Worker = std::thread([this]()
    {
        WorkerLoop();
    });
}

void RadarBridge::Stop()
{
    if (!m_Running.exchange(false))
        return;

    if (m_Worker.joinable())
        m_Worker.join();
}

void RadarBridge::PublishRawGsi(std::string payload)
{
    if (payload.empty())
        return;

    std::lock_guard lock(m_PayloadMutex);
    m_PendingPayload = std::move(payload);
    ++m_PendingPayloadSeq;
}

RadarRuntimeState RadarBridge::GetRuntimeState() const
{
    std::lock_guard lock(m_RuntimeMutex);
    return m_RuntimeState;
}

void RadarBridge::ForceReconnect()
{
    m_ForceReconnectRequested.store(true);
}

bool RadarBridge::EnsureSession(const int timeoutMs)
{
    if (m_Impl->Session)
    {
        WinHttpSetTimeouts(m_Impl->Session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        return true;
    }

    m_Impl->Session = WinHttpOpen(
        L"project-d-radar/2.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!m_Impl->Session)
        return false;

    if (!WinHttpSetTimeouts(m_Impl->Session, timeoutMs, timeoutMs, timeoutMs, timeoutMs))
    {
        WinHttpCloseHandle(m_Impl->Session);
        m_Impl->Session = nullptr;
        return false;
    }

    return true;
}

void RadarBridge::CloseSession()
{
    if (!m_Impl->Session)
        return;

    WinHttpCloseHandle(m_Impl->Session);
    m_Impl->Session = nullptr;
}

void RadarBridge::CloseWebSocket()
{
    if (!m_Impl->WebSocket)
        return;

    (void)WinHttpWebSocketClose(m_Impl->WebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    WinHttpCloseHandle(m_Impl->WebSocket);
    m_Impl->WebSocket = nullptr;
    m_Impl->ConnectedWebSocketUrl.clear();
    SetConnected(false);
}

void RadarBridge::SetLastError(const std::string& message)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.lastError = message;
}

void RadarBridge::SetEndpoints(const std::string& staticUrl, const std::string& webSocketUrl)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.staticUrl = staticUrl;
    m_RuntimeState.webSocketUrl = webSocketUrl;
}

void RadarBridge::SetConnected(const bool connected)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.connected = connected;
}

void RadarBridge::SetLastPushNow()
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.lastPushEpochMs = EpochMsNow();
}

bool RadarBridge::TryConnectWebSocket()
{
    ObservRadarEndpointConfig endpoint{};
    endpoint.Host = m_Impl->Settings.Host;
    endpoint.StaticPort = m_Impl->Settings.StaticPort;
    endpoint.WebSocketPort = m_Impl->Settings.IngestPort;

    const std::string webSocketUrl = BuildObservRadarWebSocketUrl(endpoint);
    ParsedHttpUrl parsed{};
    if (!ParseWebSocketUrl(webSocketUrl, parsed))
    {
        SetLastError("Failed to parse radar websocket URL");
        return false;
    }

    if (!WinHttpSetTimeouts(
        m_Impl->Session,
        m_Impl->Settings.HttpTimeoutMs,
        m_Impl->Settings.HttpTimeoutMs,
        m_Impl->Settings.HttpTimeoutMs,
        m_Impl->Settings.HttpTimeoutMs))
    {
        SetLastError("WinHttpSetTimeouts failed before websocket connect");
        return false;
    }

    HINTERNET connect = WinHttpConnect(m_Impl->Session, parsed.Host.c_str(), parsed.Port, 0);
    if (!connect)
    {
        SetLastError("WinHttpConnect failed for radar websocket");
        return false;
    }

    const DWORD requestFlags = parsed.Secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        parsed.PathAndQuery.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        requestFlags
    );
    if (!request)
    {
        WinHttpCloseHandle(connect);
        SetLastError("WinHttpOpenRequest failed for radar websocket");
        return false;
    }

    bool success = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) == TRUE;
    if (success)
        success = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == TRUE;
    if (success)
        success = WinHttpReceiveResponse(request, nullptr) == TRUE;

    DWORD statusCode = 0;
    if (success)
    {
        DWORD statusSize = sizeof(statusCode);
        success = WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX
        ) == TRUE;
    }

    HINTERNET webSocket = nullptr;
    if (success && statusCode == 101)
        webSocket = WinHttpWebSocketCompleteUpgrade(request, 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);

    if (!webSocket)
    {
        if (statusCode != 0)
            SetLastError("radar websocket rejected: HTTP " + std::to_string(statusCode));
        else
            SetLastError("radar websocket connect failed");
        return false;
    }

    CloseWebSocket();
    m_Impl->WebSocket = webSocket;
    m_Impl->ConnectedWebSocketUrl = webSocketUrl;
    SetConnected(true);
    SetLastError(std::string{});
    LOG_INFO("[radar] websocket connected {}", webSocketUrl);
    return true;
}

bool RadarBridge::TrySendPayload(const std::string& payload, std::uint16_t& outCloseCode)
{
    outCloseCode = 0;
    if (!m_Impl->WebSocket || payload.empty())
        return false;

    const DWORD status = WinHttpWebSocketSend(
        m_Impl->WebSocket,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char*>(payload.data()),
        static_cast<DWORD>(payload.size())
    );
    if (status == NO_ERROR)
    {
        SetLastPushNow();
        return true;
    }

    std::string reason{};
    (void)TryQueryCloseStatus(outCloseCode, reason);
    if (outCloseCode != 0)
    {
        SetLastError("radar send failed, close=" + std::to_string(outCloseCode) + (reason.empty() ? "" : (" (" + reason + ")")));
    }
    else
    {
        SetLastError("radar send failed, winhttp=" + std::to_string(status));
    }

    CloseWebSocket();
    return false;
}

bool RadarBridge::TryQueryCloseStatus(std::uint16_t& outCloseCode, std::string& outReason) const
{
    outCloseCode = 0;
    outReason.clear();
    if (!m_Impl->WebSocket)
        return false;

    USHORT closeCode = 0;
    std::array<char, 256> reasonBuffer{};
    DWORD reasonLength = static_cast<DWORD>(reasonBuffer.size());
    const DWORD status = WinHttpWebSocketQueryCloseStatus(
        m_Impl->WebSocket,
        &closeCode,
        reasonBuffer.data(),
        reasonLength,
        &reasonLength
    );
    if (status != NO_ERROR)
        return false;

    outCloseCode = static_cast<std::uint16_t>(closeCode);
    outReason.assign(reasonBuffer.data(), reasonBuffer.data() + (std::min<std::size_t>)(reasonLength, reasonBuffer.size()));
    return true;
}

void RadarBridge::WorkerLoop()
{
    std::uint32_t currentBackoffMs = 0;
    auto nextRetryTick = std::chrono::steady_clock::now();

    while (m_Running.load(std::memory_order_relaxed))
    {
        m_Impl->Settings = SnapshotSettings();

        ObservRadarEndpointConfig endpoint{};
        endpoint.Host = m_Impl->Settings.Host;
        endpoint.StaticPort = m_Impl->Settings.StaticPort;
        endpoint.WebSocketPort = m_Impl->Settings.IngestPort;

        const std::string staticUrl = BuildObservRadarStaticUrl(endpoint);
        const std::string webSocketUrl = BuildObservRadarWebSocketUrl(endpoint);
        SetEndpoints(staticUrl, webSocketUrl);

        {
            std::lock_guard lock(m_RuntimeMutex);
            m_RuntimeState.enabled = m_Impl->Settings.Enabled;
        }

        const auto now = std::chrono::steady_clock::now();

        if (!m_Impl->Settings.Enabled)
        {
            CloseWebSocket();
            currentBackoffMs = 0;
            nextRetryTick = now;
            m_ForceReconnectRequested.store(false, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            continue;
        }

        if (!EnsureSession(m_Impl->Settings.HttpTimeoutMs))
        {
            SetLastError("WinHTTP session initialization failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        if (m_ForceReconnectRequested.exchange(false))
        {
            CloseWebSocket();
            currentBackoffMs = 0;
            nextRetryTick = now;
        }

        if (!m_Impl->ConnectedWebSocketUrl.empty() && m_Impl->ConnectedWebSocketUrl != webSocketUrl)
        {
            CloseWebSocket();
            currentBackoffMs = 0;
            nextRetryTick = now;
        }

        RadarRuntimeState state = GetRuntimeState();
        if (!state.connected)
        {
            if (now < nextRetryTick)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            if (TryConnectWebSocket())
            {
                currentBackoffMs = 0;
                nextRetryTick = now;
            }
            else
            {
                currentBackoffMs = currentBackoffMs == 0
                    ? static_cast<std::uint32_t>(m_Impl->Settings.ReconnectBaseMs)
                    : (std::min)(
                        static_cast<std::uint32_t>(m_Impl->Settings.ReconnectMaxMs),
                        currentBackoffMs * 2u);
                nextRetryTick = now + std::chrono::milliseconds(currentBackoffMs);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (m_Impl->LastSendTick.time_since_epoch().count() == 0 ||
            now - m_Impl->LastSendTick >= std::chrono::milliseconds(m_Impl->Settings.PublishIntervalMs))
        {
            std::string payload{};
            std::uint64_t payloadSeq = 0;
            {
                std::lock_guard payloadLock(m_PayloadMutex);
                payload = m_PendingPayload;
                payloadSeq = m_PendingPayloadSeq;
            }

            if (!payload.empty() && payloadSeq != m_Impl->LastSentSeq)
            {
                std::uint16_t closeCode = 0;
                if (TrySendPayload(payload, closeCode))
                {
                    m_Impl->LastSentSeq = payloadSeq;
                    SetLastError(std::string{});
                }
                else
                {
                    currentBackoffMs = currentBackoffMs == 0
                        ? static_cast<std::uint32_t>(m_Impl->Settings.ReconnectBaseMs)
                        : (std::min)(
                            static_cast<std::uint32_t>(m_Impl->Settings.ReconnectMaxMs),
                            currentBackoffMs * 2u);
                    nextRetryTick = now + std::chrono::milliseconds(currentBackoffMs);
                }
            }

            m_Impl->LastSendTick = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    CloseWebSocket();
    CloseSession();
}
