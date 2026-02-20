#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include "../Vector.h"
#include "VisCheck.h"

class VisCheckRuntime
{
public:
    VisCheckRuntime() = default;
    ~VisCheckRuntime();

    VisCheckRuntime(const VisCheckRuntime&) = delete;
    VisCheckRuntime& operator=(const VisCheckRuntime&) = delete;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void UpdateMap(const std::string& mapName);
    bool IsMapLoaded() const;

    bool IsPointVisible(const Vector& src, const Vector& dst, float maxDistance = 5000.0f) const;
    std::string GetStatusText() const;

private:
    struct LoadResult
    {
        uint64_t requestId = 0;
        std::string mapName{};
        std::string optPath{};
        std::unique_ptr<VisCheck> visCheck{};
        bool loaded = false;
    };

    struct PendingLoad
    {
        uint64_t requestId = 0;
        std::string mapName{};
        std::future<LoadResult> future{};
    };

    void ConsumeReadyLoadsLocked() const;

    std::string NormalizeMapName(std::string mapName) const;
    std::string BuildStatusText(const std::string& mapName, const char* suffix) const;
    std::string ResolveOptPath(const std::string& mapName) const;

private:
    mutable std::shared_mutex mutex_{};
    mutable std::string currentMapName_{};
    mutable std::string currentOptPath_{};
    mutable std::string statusText_ = "Map Status: (Disabled)";
    mutable std::unique_ptr<VisCheck> visCheck_{};
    mutable std::vector<PendingLoad> pendingLoads_{};
    mutable std::atomic<uint64_t> nextRequestId_{ 0 };
    mutable std::atomic<uint64_t> activeRequestId_{ 0 };
    mutable std::string loadingMapName_{};
    bool enabled_ = false;
};
