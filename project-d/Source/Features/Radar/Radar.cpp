#include <Pch.hpp>
#include <Radar/Radar.hpp>

#include <array>
#include <optional>
#include <winhttp.h>

namespace
{
    struct RadarSettings
    {
        bool Enabled = false;
        std::string Host = "127.0.0.1";
        int StaticPort = 36364;
        int IngestPort = 36365;
        std::string AdminKey{};
        std::string RoomNamePrefix = "dma-match";
        int PublishIntervalMs = 40;
        int HttpTimeoutMs = 3000;
        int ReconnectBaseMs = 500;
        int ReconnectMaxMs = 5000;
        bool AutoDeleteRoomOnExit = true;
    };

    struct ParsedHttpUrl
    {
        std::wstring Host{};
        INTERNET_PORT Port = 0;
        std::wstring PathAndQuery = L"/";
        bool Secure = false;
    };

    struct HttpResponse
    {
        DWORD StatusCode = 0;
        std::string Body{};
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

    std::string WideToUtf8(const std::wstring& input)
    {
        if (input.empty())
            return {};

        const int bytesNeeded = WideCharToMultiByte(
            CP_UTF8,
            0,
            input.data(),
            static_cast<int>(input.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (bytesNeeded <= 0)
            return {};

        std::string output(static_cast<std::size_t>(bytesNeeded), '\0');
        const int converted = WideCharToMultiByte(
            CP_UTF8,
            0,
            input.data(),
            static_cast<int>(input.size()),
            output.data(),
            bytesNeeded,
            nullptr,
            nullptr
        );
        if (converted <= 0)
            return {};

        return output;
    }

    std::string PercentEncode(const std::string& value)
    {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string encoded{};
        encoded.reserve(value.size() * 3);

        for (const unsigned char ch : value)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '_' || ch == '.' || ch == '~')
            {
                encoded.push_back(static_cast<char>(ch));
                continue;
            }

            encoded.push_back('%');
            encoded.push_back(kHex[(ch >> 4) & 0x0F]);
            encoded.push_back(kHex[ch & 0x0F]);
        }

        return encoded;
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
        if (settings.Host.rfind("http://", 0) == 0)
            settings.Host = settings.Host.substr(7);
        else if (settings.Host.rfind("https://", 0) == 0)
            settings.Host = settings.Host.substr(8);
        while (!settings.Host.empty() && (settings.Host.back() == '/' || settings.Host.back() == '\\'))
            settings.Host.pop_back();
        if (settings.Host.empty())
            settings.Host = "127.0.0.1";

        settings.StaticPort = std::clamp(config.Radar.StaticPort, 1, 65535);
        settings.IngestPort = std::clamp(config.Radar.IngestPort, 1, 65535);
        settings.AdminKey = config.Radar.AdminKey;
        settings.RoomNamePrefix = TrimAscii(config.Radar.RoomNamePrefix);
        if (settings.RoomNamePrefix.empty())
            settings.RoomNamePrefix = "dma-match";

        settings.PublishIntervalMs = std::clamp(config.Radar.PublishIntervalMs, 40, 2000);
        settings.HttpTimeoutMs = std::clamp(config.Radar.HttpTimeoutMs, 300, 20000);
        settings.ReconnectBaseMs = std::clamp(config.Radar.ReconnectBaseMs, 100, 30000);
        settings.ReconnectMaxMs = std::clamp(config.Radar.ReconnectMaxMs, settings.ReconnectBaseMs, 120000);
        settings.AutoDeleteRoomOnExit = config.Radar.AutoDeleteRoomOnExit;
        return settings;
    }

    std::string BuildHttpBase(const RadarSettings& settings)
    {
        return "http://" + settings.Host + ":" + std::to_string(settings.StaticPort);
    }

    std::string BuildIngestFallbackUrl(const RadarSettings& settings, const std::string& token)
    {
        return "ws://" + settings.Host + ":" + std::to_string(settings.IngestPort) + "?token=" + PercentEncode(token);
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

    bool HttpRequest(
        HINTERNET session,
        const ParsedHttpUrl& parsed,
        const wchar_t* method,
        const std::vector<std::wstring>& headers,
        const std::string& body,
        int timeoutMs,
        HttpResponse& outResponse)
    {
        if (!session || !method)
            return false;

        if (!WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs))
            return false;

        HINTERNET connect = WinHttpConnect(session, parsed.Host.c_str(), parsed.Port, 0);
        if (!connect)
            return false;

        const DWORD requestFlags = parsed.Secure ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(
            connect,
            method,
            parsed.PathAndQuery.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            requestFlags
        );
        if (!request)
        {
            WinHttpCloseHandle(connect);
            return false;
        }

        bool success = true;
        for (const std::wstring& header : headers)
        {
            if (!WinHttpAddRequestHeaders(
                request,
                header.c_str(),
                static_cast<DWORD>(header.size()),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                success = false;
                break;
            }
        }

        const DWORD payloadSize = static_cast<DWORD>(body.size());
        if (success)
        {
            success = WinHttpSendRequest(
                request,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                payloadSize,
                payloadSize,
                0
            ) == TRUE;
        }

        if (success)
            success = WinHttpReceiveResponse(request, nullptr) == TRUE;

        if (success)
        {
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            success = WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX
            ) == TRUE;
            if (success)
                outResponse.StatusCode = statusCode;
        }

        if (success)
        {
            outResponse.Body.clear();
            for (;;)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    success = false;
                    break;
                }

                if (available == 0)
                    break;

                std::string chunk(static_cast<std::size_t>(available), '\0');
                DWORD read = 0;
                if (!WinHttpReadData(request, chunk.data(), available, &read))
                {
                    success = false;
                    break;
                }

                if (read == 0)
                    break;

                outResponse.Body.append(chunk.data(), static_cast<std::size_t>(read));
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        return success;
    }
}

struct RadarBridge::Impl
{
    RadarSettings Settings{};

    HINTERNET Session = nullptr;
    HINTERNET WebSocket = nullptr;

    std::string RoomToken{};

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

void RadarBridge::ForceCreateRoom()
{
    m_ForceCreateRequested.store(true);
}

void RadarBridge::ForceReconnectIngest()
{
    m_ForceReconnectRequested.store(true);
}

void RadarBridge::ForceDeleteRoom()
{
    m_ForceDeleteRequested.store(true);
}

bool RadarBridge::EnsureSession(const int timeoutMs)
{
    if (m_Impl->Session)
    {
        WinHttpSetTimeouts(m_Impl->Session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        return true;
    }

    m_Impl->Session = WinHttpOpen(
        L"project-d-radar/1.0",
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
    SetIngestConnected(false);
}

void RadarBridge::SetLastError(const std::string& message)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.lastError = message;
}

void RadarBridge::SetRoomState(
    const bool roomCreated,
    const bool ingestConnected,
    const std::string& roomId,
    const std::string& watchUrl,
    const std::string& ingestUrl)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.roomCreated = roomCreated;
    m_RuntimeState.ingestConnected = ingestConnected;
    m_RuntimeState.roomId = roomId;
    m_RuntimeState.watchUrl = watchUrl;
    m_RuntimeState.ingestUrl = ingestUrl;
}

void RadarBridge::SetIngestConnected(const bool connected)
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.ingestConnected = connected;
}

void RadarBridge::SetLastPushNow()
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.lastPushEpochMs = EpochMsNow();
}

void RadarBridge::ClearRuntimeRoomState()
{
    std::lock_guard lock(m_RuntimeMutex);
    m_RuntimeState.roomCreated = false;
    m_RuntimeState.ingestConnected = false;
    m_RuntimeState.roomId.clear();
    m_RuntimeState.watchUrl.clear();
    m_RuntimeState.ingestUrl.clear();
    m_Impl->RoomToken.clear();
}

bool RadarBridge::TryCreateRoom()
{
    const RadarSettings& settings = m_Impl->Settings;
    if (settings.AdminKey.empty())
    {
        SetLastError("Radar.AdminKey is empty");
        return false;
    }

    const std::string url = BuildHttpBase(settings) + "/api/rooms";
    ParsedHttpUrl parsed{};
    if (!ParseHttpUrl(url, parsed))
    {
        SetLastError("Failed to parse room API URL");
        return false;
    }

    nlohmann::json payload{};
    payload["roomName"] = settings.RoomNamePrefix + "-" + std::to_string(EpochMsNow() / 1000ull);

    std::vector<std::wstring> headers{};
    headers.push_back(L"Content-Type: application/json\r\n");
    headers.push_back(Utf8ToWide("X-Bolt-Admin-Key: " + settings.AdminKey + "\r\n"));

    HttpResponse response{};
    if (!HttpRequest(m_Impl->Session, parsed, L"POST", headers, payload.dump(), settings.HttpTimeoutMs, response))
    {
        SetLastError("POST /api/rooms failed (transport)");
        return false;
    }

    if (response.StatusCode != 201)
    {
        if (response.StatusCode == 401)
            SetLastError("POST /api/rooms failed: 401 unauthorized");
        else if (response.StatusCode == 503)
            SetLastError("POST /api/rooms failed: 503 admin key not configured");
        else
            SetLastError("POST /api/rooms failed: HTTP " + std::to_string(response.StatusCode));
        return false;
    }

    const nlohmann::json parsedBody = nlohmann::json::parse(response.Body, nullptr, false);
    if (parsedBody.is_discarded())
    {
        SetLastError("POST /api/rooms returned invalid JSON");
        return false;
    }

    const std::string roomId = parsedBody.value("roomId", std::string{});
    const std::string watchUrl = parsedBody.value("watchUrl", std::string{});
    std::string ingestUrl = parsedBody.value("ingestUrl", std::string{});
    const std::string token = parsedBody.value("token", std::string{});

    if (ingestUrl.empty() && !token.empty())
        ingestUrl = BuildIngestFallbackUrl(settings, token);

    if (roomId.empty() || ingestUrl.empty())
    {
        SetLastError("POST /api/rooms missing roomId or ingestUrl");
        return false;
    }

    m_Impl->RoomToken = token;
    SetRoomState(true, false, roomId, watchUrl, ingestUrl);
    SetLastError(std::string{});
    LOG_INFO("[radar] created room id={} watch={}", roomId, watchUrl.empty() ? "<none>" : watchUrl);
    return true;
}

bool RadarBridge::TryDeleteRoom(const std::string& roomId)
{
    if (roomId.empty())
        return true;

    const RadarSettings& settings = m_Impl->Settings;
    if (settings.AdminKey.empty())
    {
        SetLastError("DELETE room skipped: Radar.AdminKey is empty");
        return false;
    }

    const std::string url = BuildHttpBase(settings) + "/api/rooms/" + roomId;
    ParsedHttpUrl parsed{};
    if (!ParseHttpUrl(url, parsed))
    {
        SetLastError("Failed to parse delete room URL");
        return false;
    }

    std::vector<std::wstring> headers{};
    headers.push_back(Utf8ToWide("X-Bolt-Admin-Key: " + settings.AdminKey + "\r\n"));

    HttpResponse response{};
    if (!HttpRequest(m_Impl->Session, parsed, L"DELETE", headers, {}, settings.HttpTimeoutMs, response))
    {
        SetLastError("DELETE /api/rooms/:id failed (transport)");
        return false;
    }

    if (response.StatusCode == 200 || response.StatusCode == 204 || response.StatusCode == 404)
    {
        SetLastError(std::string{});
        return true;
    }

    if (response.StatusCode == 401)
        SetLastError("DELETE /api/rooms/:id failed: 401 unauthorized");
    else if (response.StatusCode == 503)
        SetLastError("DELETE /api/rooms/:id failed: 503 admin key not configured");
    else
        SetLastError("DELETE /api/rooms/:id failed: HTTP " + std::to_string(response.StatusCode));
    return false;
}

bool RadarBridge::TryConnectIngest()
{
    RadarRuntimeState state = GetRuntimeState();
    std::string ingestUrl = state.ingestUrl;
    if (ingestUrl.empty() && !m_Impl->RoomToken.empty())
        ingestUrl = BuildIngestFallbackUrl(m_Impl->Settings, m_Impl->RoomToken);

    if (ingestUrl.empty())
    {
        SetLastError("ingestUrl is empty");
        return false;
    }

    ParsedHttpUrl parsed{};
    if (!ParseWebSocketUrl(ingestUrl, parsed))
    {
        SetLastError("Failed to parse ingest websocket URL");
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
        SetLastError("WinHttpConnect failed for ingest websocket");
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
        SetLastError("WinHttpOpenRequest failed for ingest websocket");
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
        if (statusCode == 401)
            SetLastError("ingest websocket rejected: 401");
        else if (statusCode == 404)
            SetLastError("ingest websocket rejected: 404 room not found");
        else if (statusCode != 0)
            SetLastError("ingest websocket rejected: HTTP " + std::to_string(statusCode));
        else
            SetLastError("ingest websocket connect failed");
        return false;
    }

    CloseWebSocket();
    m_Impl->WebSocket = webSocket;
    SetIngestConnected(true);
    SetLastError(std::string{});
    LOG_INFO("[radar] ingest websocket connected");
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
        SetLastError("ingest send failed, close=" + std::to_string(outCloseCode) + (reason.empty() ? "" : (" (" + reason + ")")));
    }
    else
    {
        SetLastError("ingest send failed, winhttp=" + std::to_string(status));
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

        {
            std::lock_guard lock(m_RuntimeMutex);
            m_RuntimeState.enabled = m_Impl->Settings.Enabled;
        }

        const auto now = std::chrono::steady_clock::now();

        if (m_ForceDeleteRequested.exchange(false))
        {
            const RadarRuntimeState state = GetRuntimeState();
            CloseWebSocket();
            if (!state.roomId.empty())
                (void)TryDeleteRoom(state.roomId);
            ClearRuntimeRoomState();
            currentBackoffMs = 0;
            nextRetryTick = now;
        }

        if (!m_Impl->Settings.Enabled)
        {
            CloseWebSocket();
            currentBackoffMs = 0;
            nextRetryTick = now;
            m_ForceCreateRequested.store(false, std::memory_order_relaxed);
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

        if (m_ForceCreateRequested.exchange(false))
        {
            const RadarRuntimeState state = GetRuntimeState();
            CloseWebSocket();
            if (!state.roomId.empty())
                (void)TryDeleteRoom(state.roomId);
            ClearRuntimeRoomState();
            currentBackoffMs = 0;
            nextRetryTick = now;
        }

        RadarRuntimeState state = GetRuntimeState();
        if (!state.roomCreated)
        {
            if (now < nextRetryTick)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            if (TryCreateRoom())
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

        state = GetRuntimeState();
        if (!state.ingestConnected)
        {
            if (now < nextRetryTick)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            if (TryConnectIngest())
            {
                currentBackoffMs = 0;
                nextRetryTick = now;
            }
            else
            {
                std::uint16_t closeCode = 0;
                std::string closeReason{};
                if (TryQueryCloseStatus(closeCode, closeReason) && (closeCode == 4001 || closeCode == 4004))
                {
                    SetLastError("ingest closed by server, recreating room");
                    ClearRuntimeRoomState();
                }

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
                    if (closeCode == 4001 || closeCode == 4004)
                        ClearRuntimeRoomState();

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

    m_Impl->Settings = SnapshotSettings();
    if (m_Impl->Settings.AutoDeleteRoomOnExit)
    {
        const RadarRuntimeState state = GetRuntimeState();
        if (EnsureSession(m_Impl->Settings.HttpTimeoutMs) && !state.roomId.empty())
            (void)TryDeleteRoom(state.roomId);
    }

    CloseWebSocket();
    ClearRuntimeRoomState();
    CloseSession();
}
