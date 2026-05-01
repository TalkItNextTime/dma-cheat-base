#include <Features/ESP/EspFrameSyncModel.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>

namespace
{
    using Clock = std::chrono::steady_clock;

    void test_frame_timing_age_and_sample_duration()
    {
        const auto base = Clock::time_point{} + std::chrono::seconds(10);

        EspFrameSyncModel::FrameTiming timing{};
        timing.FrameId = 7;
        timing.SampleStart = base;
        timing.SampleEnd = base + std::chrono::microseconds(1200);
        timing.PublishTime = base + std::chrono::microseconds(1500);

        const EspFrameSyncModel::FrameTimingStats stats =
            EspFrameSyncModel::EvaluateFrameTiming(timing, base + std::chrono::microseconds(4100));

        assert(stats.FrameId == 7);
        assert(stats.SampleUs == 1200);
        assert(stats.PublishLatencyUs == 1500);
        assert(stats.FrameAgeUs == 2600);
    }

    void test_stale_tracker_counts_same_frame_reuse()
    {
        EspFrameSyncModel::StaleFrameTracker tracker{};

        auto first = EspFrameSyncModel::UpdateStaleFrameTracker(tracker, 10);
        assert(!first.ReusedPreviousFrame);
        assert(first.ConsecutiveReuseCount == 0);

        auto second = EspFrameSyncModel::UpdateStaleFrameTracker(tracker, 10);
        assert(second.ReusedPreviousFrame);
        assert(second.ConsecutiveReuseCount == 1);

        auto third = EspFrameSyncModel::UpdateStaleFrameTracker(tracker, 10);
        assert(third.ReusedPreviousFrame);
        assert(third.ConsecutiveReuseCount == 2);

        auto fourth = EspFrameSyncModel::UpdateStaleFrameTracker(tracker, 11);
        assert(!fourth.ReusedPreviousFrame);
        assert(fourth.ConsecutiveReuseCount == 0);
    }

    void test_cold_budget_runs_due_tasks_until_budget_exhausted()
    {
        const auto base = Clock::time_point{} + std::chrono::seconds(20);
        EspFrameSyncModel::ColdBudget budget{ std::chrono::microseconds(2500) };

        EspFrameSyncModel::ColdTaskState weapon{};
        weapon.LastRun = base - std::chrono::milliseconds(600);
        weapon.Interval = std::chrono::milliseconds(500);
        weapon.EstimatedCost = std::chrono::microseconds(1200);

        EspFrameSyncModel::ColdTaskState spectator{};
        spectator.LastRun = base - std::chrono::milliseconds(250);
        spectator.Interval = std::chrono::milliseconds(200);
        spectator.EstimatedCost = std::chrono::microseconds(1800);

        EspFrameSyncModel::ColdTaskState radar{};
        radar.LastRun = base - std::chrono::milliseconds(100);
        radar.Interval = std::chrono::milliseconds(500);
        radar.EstimatedCost = std::chrono::microseconds(800);

        assert(EspFrameSyncModel::ShouldRunColdTask(weapon, budget, base));
        EspFrameSyncModel::MarkColdTaskRun(weapon, budget, base, std::chrono::microseconds(1300));
        assert(weapon.LastRun == base);
        assert(budget.Used == std::chrono::microseconds(1300));

        assert(!EspFrameSyncModel::ShouldRunColdTask(spectator, budget, base));
        assert(!EspFrameSyncModel::ShouldRunColdTask(radar, budget, base));
    }
}

int main()
{
    test_frame_timing_age_and_sample_duration();
    test_stale_tracker_counts_same_frame_reuse();
    test_cold_budget_runs_due_tasks_until_budget_exhausted();
    return 0;
}
