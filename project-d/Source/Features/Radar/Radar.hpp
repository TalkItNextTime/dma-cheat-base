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
    bool roomCreated = false;
    bool ingestConnected = false;
    std::string roomId{};
    std::string watchUrl{};
    std::string ingestUrl{};
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

    void ForceCreateRoom();
    void ForceReconnectIngest();
    void ForceDeleteRoom();

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

    bool TryCreateRoom();
    bool TryDeleteRoom(const std::string& roomId);
    bool TryConnectIngest();
    bool TrySendPayload(const std::string& payload, std::uint16_t& outCloseCode);
    bool TryQueryCloseStatus(std::uint16_t& outCloseCode, std::string& outReason) const;

    void SetLastError(const std::string& message);
    void SetRoomState(
        bool roomCreated,
        bool ingestConnected,
        const std::string& roomId,
        const std::string& watchUrl,
        const std::string& ingestUrl);
    void SetIngestConnected(bool connected);
    void SetLastPushNow();
    void ClearRuntimeRoomState();

private:
    std::atomic<bool> m_Running{ false };
    std::thread m_Worker{};

    std::atomic<bool> m_ForceCreateRequested{ false };
    std::atomic<bool> m_ForceReconnectRequested{ false };
    std::atomic<bool> m_ForceDeleteRequested{ false };

    mutable std::mutex m_RuntimeMutex{};
    RadarRuntimeState m_RuntimeState{};

    mutable std::mutex m_PayloadMutex{};
    std::string m_PendingPayload{};
    std::uint64_t m_PendingPayloadSeq = 0;

    std::unique_ptr<Impl> m_Impl{};
};

inline RadarBridge& radarBridge = RadarBridge::Get();
