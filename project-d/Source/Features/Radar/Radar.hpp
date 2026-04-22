#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct RadarRuntimeState
{
    bool enabled = false;
    bool connected = false;
    std::string staticUrl{};
    std::string webSocketUrl{};
    std::string lastError{};
    std::uint64_t lastPushEpochMs = 0;
};

class RadarBridge
{
public:
    static RadarBridge& Get();

    void Start();
    void Stop();

    void PublishRawGsi(std::string payload);
    RadarRuntimeState GetRuntimeState() const;

    void ForceReconnect();

private:
    struct Impl;

    RadarBridge();
    ~RadarBridge();

    RadarBridge(const RadarBridge&) = delete;
    RadarBridge& operator=(const RadarBridge&) = delete;

    void WorkerLoop();
    bool EnsureSession(int timeoutMs);
    void CloseSession();
    void CloseWebSocket();

    bool TryConnectWebSocket();
    bool TrySendPayload(const std::string& payload, std::uint16_t& outCloseCode);
    bool TryQueryCloseStatus(std::uint16_t& outCloseCode, std::string& outReason) const;

    void SetLastError(const std::string& message);
    void SetEndpoints(const std::string& staticUrl, const std::string& webSocketUrl);
    void SetConnected(bool connected);
    void SetLastPushNow();

private:
    std::atomic<bool> m_Running{ false };
    std::thread m_Worker{};

    std::atomic<bool> m_ForceReconnectRequested{ false };

    mutable std::mutex m_RuntimeMutex{};
    RadarRuntimeState m_RuntimeState{};

    mutable std::mutex m_PayloadMutex{};
    std::string m_PendingPayload{};
    std::uint64_t m_PendingPayloadSeq = 0;

    std::unique_ptr<Impl> m_Impl{};
};

inline RadarBridge& radarBridge = RadarBridge::Get();
