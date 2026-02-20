#include "VisCheckRuntime.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <vector>

#include "VisCheck.h"

namespace
{
    std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }
}

VisCheckRuntime::~VisCheckRuntime() = default;

void VisCheckRuntime::ConsumeReadyLoadsLocked() const
{
    for (size_t i = 0; i < pendingLoads_.size();)
    {
        PendingLoad& pending = pendingLoads_[i];
        if (!pending.future.valid())
        {
            pendingLoads_.erase(pendingLoads_.begin() + static_cast<long long>(i));
            continue;
        }

        if (pending.future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            ++i;
            continue;
        }

        LoadResult result = pending.future.get();
        pendingLoads_.erase(pendingLoads_.begin() + static_cast<long long>(i));

        if (result.requestId != activeRequestId_.load())
            continue;

        if (result.loaded && result.visCheck)
        {
            visCheck_ = std::move(result.visCheck);
            currentMapName_ = result.mapName;
            currentOptPath_ = result.optPath;
            statusText_ = BuildStatusText(result.mapName, "Loaded");
        }
        else
        {
            visCheck_.reset();
            currentMapName_ = result.mapName;
            currentOptPath_ = result.optPath;
            statusText_ = result.optPath.empty()
                ? BuildStatusText(result.mapName, "Not Found")
                : BuildStatusText(result.mapName, "Load Failed");
        }
    }
}

void VisCheckRuntime::SetEnabled(bool enabled)
{
    std::unique_lock lock(mutex_);

    ConsumeReadyLoadsLocked();

    if (enabled_ == enabled)
        return;

    enabled_ = enabled;
    if (!enabled_)
    {
        visCheck_.reset();
        currentMapName_.clear();
        currentOptPath_.clear();
        loadingMapName_.clear();
        activeRequestId_.store(0);
        statusText_ = "Map Status: (Disabled)";
    }
    else
    {
        statusText_ = "Map Status: (Waiting Map)";
    }
}

bool VisCheckRuntime::IsEnabled() const
{
    std::shared_lock lock(mutex_);
    return enabled_;
}

void VisCheckRuntime::UpdateMap(const std::string& mapName)
{
    const std::string normalizedMapName = NormalizeMapName(mapName);

    std::unique_lock lock(mutex_);
    ConsumeReadyLoadsLocked();

    if (!enabled_)
    {
        statusText_ = "Map Status: (Disabled)";
        return;
    }

    if (normalizedMapName.empty())
    {
        visCheck_.reset();
        currentMapName_.clear();
        currentOptPath_.clear();
        loadingMapName_.clear();
        activeRequestId_.store(0);
        statusText_ = "Map Status: (No Map)";
        return;
    }

    if ((normalizedMapName == currentMapName_ && visCheck_) ||
        (normalizedMapName == loadingMapName_ && activeRequestId_.load() != 0))
        return;

    const std::string optPath = ResolveOptPath(normalizedMapName);
    if (optPath.empty())
    {
        visCheck_.reset();
        currentMapName_ = normalizedMapName;
        currentOptPath_.clear();
        loadingMapName_.clear();
        activeRequestId_.store(0);
        statusText_ = BuildStatusText(normalizedMapName, "Not Found");
        return;
    }

    const uint64_t requestId = nextRequestId_.fetch_add(1) + 1;
    activeRequestId_.store(requestId);
    loadingMapName_ = normalizedMapName;
    statusText_ = BuildStatusText(normalizedMapName, "Loading");

    PendingLoad pending{};
    pending.requestId = requestId;
    pending.mapName = normalizedMapName;
    pending.future = std::async(std::launch::async, [requestId = pending.requestId, normalizedMapName, optPath]() mutable {
        LoadResult result{};
        result.requestId = requestId;
        result.mapName = normalizedMapName;
        result.optPath = optPath;

        auto nextVisCheck = std::make_unique<VisCheck>(optPath);
        if (nextVisCheck && nextVisCheck->IsReady())
        {
            result.loaded = true;
            result.visCheck = std::move(nextVisCheck);
        }

        return result;
    });

    pendingLoads_.push_back(std::move(pending));
}

bool VisCheckRuntime::IsMapLoaded() const
{
    std::unique_lock lock(mutex_);
    ConsumeReadyLoadsLocked();

    return enabled_ && static_cast<bool>(visCheck_);
}

bool VisCheckRuntime::IsPointVisible(const Vector& src, const Vector& dst, float maxDistance) const
{
    std::unique_lock lock(mutex_);
    ConsumeReadyLoadsLocked();

    if (!enabled_)
        return true;
    if (!visCheck_)
        return false;

    const float maxDistanceSqr = maxDistance * maxDistance;
    const float worldDistanceSqr = src.CalcDis2Point3DSqr(dst);
    if (worldDistanceSqr > maxDistanceSqr)
        return false;

    return visCheck_->IsPointVisible(src, dst);
}

std::string VisCheckRuntime::GetStatusText() const
{
    std::unique_lock lock(mutex_);
    ConsumeReadyLoadsLocked();
    return statusText_;
}

std::string VisCheckRuntime::NormalizeMapName(std::string mapName) const
{
    if (mapName.rfind("maps/", 0) == 0)
        mapName = mapName.substr(5);
    if (mapName.rfind("maps\\", 0) == 0)
        mapName = mapName.substr(5);

    const std::string lower = ToLowerAscii(mapName);
    constexpr const char* kBspSuffix = ".bsp";
    constexpr size_t kBspSuffixLen = 4;
    if (lower.size() > kBspSuffixLen && lower.compare(lower.size() - kBspSuffixLen, kBspSuffixLen, kBspSuffix) == 0)
        mapName = mapName.substr(0, mapName.size() - kBspSuffixLen);

    return mapName;
}

std::string VisCheckRuntime::BuildStatusText(const std::string& mapName, const char* suffix) const
{
    std::string text = "Map Status: ";
    text += mapName.empty() ? "(Unknown)" : (mapName + ".opt");
    text += " (";
    text += suffix;
    text += ")";
    return text;
}

std::string VisCheckRuntime::ResolveOptPath(const std::string& mapName) const
{
    if (mapName.empty())
        return {};

    const std::string optFileName = mapName + ".opt";
    std::vector<std::filesystem::path> candidates{};

    std::error_code ec{};
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec)
    {
        candidates.push_back(cwd / "maps" / optFileName);
        candidates.push_back(cwd / optFileName);
        candidates.push_back(cwd.parent_path() / "maps" / optFileName);
    }

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
    {
        std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
        candidates.push_back(exeDir / "maps" / optFileName);
        candidates.push_back(exeDir.parent_path() / "maps" / optFileName);
    }

    for (const auto& candidate : candidates)
    {
        if (candidate.empty())
            continue;

        std::error_code existsEc{};
        if (std::filesystem::exists(candidate, existsEc) && !existsEc)
            return candidate.string();
    }

    return {};
}
