#pragma once

#include <chrono>
#include <cstdint>

namespace EspFrameSyncModel
{
    using Clock = std::chrono::steady_clock;

    struct FrameTiming
    {
        std::uint64_t FrameId = 0;
        Clock::time_point SampleStart{};
        Clock::time_point SampleEnd{};
        Clock::time_point PublishTime{};
    };

    struct FrameTimingStats
    {
        std::uint64_t FrameId = 0;
        std::uint64_t SampleUs = 0;
        std::uint64_t PublishLatencyUs = 0;
        std::uint64_t FrameAgeUs = 0;
    };

    struct StaleFrameTracker
    {
        std::uint64_t LastFrameId = 0;
        std::uint64_t ConsecutiveReuseCount = 0;
    };

    struct StaleFrameUpdate
    {
        bool ReusedPreviousFrame = false;
        std::uint64_t ConsecutiveReuseCount = 0;
    };

    struct ColdBudget
    {
        std::chrono::microseconds Limit{ 0 };
        std::chrono::microseconds Used{ 0 };
    };

    struct ColdTaskState
    {
        Clock::time_point LastRun{};
        std::chrono::steady_clock::duration Interval{};
        std::chrono::microseconds EstimatedCost{ 0 };
    };

    struct SamplerTimingPolicy
    {
        std::chrono::microseconds IdleInterval{ 0 };
        std::chrono::microseconds HotInterval{ 0 };
        std::chrono::microseconds HelperIdleInterval{ 0 };
        std::chrono::microseconds HelperHotInterval{ 0 };
        std::chrono::microseconds BackpressureCap{ 0 };
    };

    struct SamplerTimingInput
    {
        bool HelperOnlyMode = false;
        bool HelperHoldingUtility = false;
        bool HighRateAimSampling = false;
        std::chrono::microseconds SampleDuration{ 0 };
    };

    FrameTimingStats EvaluateFrameTiming(const FrameTiming& timing, Clock::time_point renderTime);
    StaleFrameUpdate UpdateStaleFrameTracker(StaleFrameTracker& tracker, std::uint64_t frameId);
    bool ShouldRunColdTask(const ColdTaskState& task, const ColdBudget& budget, Clock::time_point now);
    void MarkColdTaskRun(
        ColdTaskState& task,
        ColdBudget& budget,
        Clock::time_point now,
        std::chrono::microseconds actualCost);
    std::chrono::microseconds ResolveSamplerInterval(
        const SamplerTimingInput& input,
        const SamplerTimingPolicy& policy);
    bool ShouldBuildVisDebugInSampler(
        bool visualVisDebugEnabled,
        bool debugEnabled,
        bool debugVisCheckEnabled);
    int ClampVisDebugMaxItems(int requestedItems);
}
