#include "EspFrameSyncModel.hpp"

#include <algorithm>

namespace EspFrameSyncModel
{
    namespace
    {
        std::uint64_t DurationUs(const Clock::duration duration)
        {
            if (duration <= Clock::duration::zero())
                return 0;

            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(duration).count()
            );
        }
    }

    FrameTimingStats EvaluateFrameTiming(const FrameTiming& timing, const Clock::time_point renderTime)
    {
        FrameTimingStats stats{};
        stats.FrameId = timing.FrameId;
        stats.SampleUs = DurationUs(timing.SampleEnd - timing.SampleStart);
        stats.PublishLatencyUs = DurationUs(timing.PublishTime - timing.SampleStart);
        stats.FrameAgeUs = DurationUs(renderTime - timing.PublishTime);
        return stats;
    }

    StaleFrameUpdate UpdateStaleFrameTracker(StaleFrameTracker& tracker, const std::uint64_t frameId)
    {
        StaleFrameUpdate update{};

        if (frameId != 0 && frameId == tracker.LastFrameId)
        {
            ++tracker.ConsecutiveReuseCount;
            update.ReusedPreviousFrame = true;
            update.ConsecutiveReuseCount = tracker.ConsecutiveReuseCount;
            return update;
        }

        tracker.LastFrameId = frameId;
        tracker.ConsecutiveReuseCount = 0;
        return update;
    }

    bool ShouldRunColdTask(const ColdTaskState& task, const ColdBudget& budget, const Clock::time_point now)
    {
        if (task.Interval > Clock::duration::zero() &&
            task.LastRun.time_since_epoch().count() != 0 &&
            now - task.LastRun < task.Interval)
        {
            return false;
        }

        if (budget.Limit <= std::chrono::microseconds::zero())
            return true;

        return budget.Used + std::max(task.EstimatedCost, std::chrono::microseconds::zero()) <= budget.Limit;
    }

    void MarkColdTaskRun(
        ColdTaskState& task,
        ColdBudget& budget,
        const Clock::time_point now,
        const std::chrono::microseconds actualCost)
    {
        task.LastRun = now;
        if (actualCost > std::chrono::microseconds::zero())
            budget.Used += actualCost;
    }

    std::chrono::microseconds ResolveSamplerInterval(
        const SamplerTimingInput& input,
        const SamplerTimingPolicy& policy)
    {
        std::chrono::microseconds targetInterval = input.HelperOnlyMode
            ? (input.HelperHoldingUtility ? policy.HelperHotInterval : policy.HelperIdleInterval)
            : (input.HighRateAimSampling ? policy.HotInterval : policy.IdleInterval);

        if (input.SampleDuration > std::chrono::microseconds::zero())
        {
            const auto adaptiveInterval = input.SampleDuration + input.SampleDuration / 4;
            if (adaptiveInterval > targetInterval)
            {
                targetInterval = policy.BackpressureCap > std::chrono::microseconds::zero()
                    ? std::min(adaptiveInterval, policy.BackpressureCap)
                    : adaptiveInterval;
            }
        }

        return targetInterval;
    }

    bool ShouldBuildVisDebugInSampler(
        const bool visualVisDebugEnabled,
        const bool debugEnabled,
        const bool debugVisCheckEnabled)
    {
        if (debugEnabled && debugVisCheckEnabled)
            return false;

        if (visualVisDebugEnabled)
            return true;

        return debugVisCheckEnabled;
    }

    int ClampVisDebugMaxItems(const int requestedItems)
    {
        return std::clamp(requestedItems, 32, 1000);
    }
}
