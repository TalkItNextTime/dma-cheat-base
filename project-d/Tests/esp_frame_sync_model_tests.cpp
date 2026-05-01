#include <Features/ESP/EspFrameSyncModel.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <type_traits>

namespace
{
    using Clock = std::chrono::steady_clock;

    template <typename T, typename = void>
    struct HasVisualBoneOnlyInterval : std::false_type {};

    template <typename T>
    struct HasVisualBoneOnlyInterval<T, std::void_t<decltype(&T::VisualBoneOnlyInterval)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasVisualBoneOnlySampling : std::false_type {};

    template <typename T>
    struct HasVisualBoneOnlySampling<T, std::void_t<decltype(&T::VisualBoneOnlySampling)>> : std::true_type {};

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

    void test_hot_sampler_backpressure_when_sampling_overruns_target()
    {
        EspFrameSyncModel::SamplerTimingPolicy policy{};
        policy.IdleInterval = std::chrono::microseconds(6000);
        policy.HotInterval = std::chrono::microseconds(2000);
        policy.HelperIdleInterval = std::chrono::microseconds(17000);
        policy.HelperHotInterval = std::chrono::microseconds(7000);
        policy.BackpressureCap = std::chrono::microseconds(17000);

        EspFrameSyncModel::SamplerTimingInput input{};
        input.HighRateAimSampling = true;
        input.SampleDuration = std::chrono::microseconds(5000);

        const auto interval = EspFrameSyncModel::ResolveSamplerInterval(input, policy);
        assert(interval == std::chrono::microseconds(6250));
    }

    void test_sampler_keeps_hot_interval_when_sampling_is_fast()
    {
        EspFrameSyncModel::SamplerTimingPolicy policy{};
        policy.IdleInterval = std::chrono::microseconds(6000);
        policy.HotInterval = std::chrono::microseconds(2000);
        policy.HelperIdleInterval = std::chrono::microseconds(17000);
        policy.HelperHotInterval = std::chrono::microseconds(7000);
        policy.BackpressureCap = std::chrono::microseconds(17000);

        EspFrameSyncModel::SamplerTimingInput input{};
        input.HighRateAimSampling = true;
        input.SampleDuration = std::chrono::microseconds(1200);

        const auto interval = EspFrameSyncModel::ResolveSamplerInterval(input, policy);
        assert(interval == std::chrono::microseconds(2000));
    }

    void test_visual_bone_sampling_keeps_idle_interval_for_freshness()
    {
        EspFrameSyncModel::SamplerTimingPolicy policy{};
        policy.IdleInterval = std::chrono::microseconds(6000);
        policy.HotInterval = std::chrono::microseconds(2000);
        policy.HelperIdleInterval = std::chrono::microseconds(17000);
        policy.HelperHotInterval = std::chrono::microseconds(7000);
        policy.BackpressureCap = std::chrono::microseconds(17000);

        EspFrameSyncModel::SamplerTimingInput input{};
        input.SampleDuration = std::chrono::microseconds(3000);

        const auto interval = EspFrameSyncModel::ResolveSamplerInterval(input, policy);
        assert(interval == std::chrono::microseconds(6000));
    }

    void test_sampler_policy_has_no_visual_bone_downshift()
    {
        assert(!HasVisualBoneOnlyInterval<EspFrameSyncModel::SamplerTimingPolicy>::value);
        assert(!HasVisualBoneOnlySampling<EspFrameSyncModel::SamplerTimingInput>::value);
    }

    void test_hot_sampling_uses_hot_interval()
    {
        EspFrameSyncModel::SamplerTimingPolicy policy{};
        policy.IdleInterval = std::chrono::microseconds(6000);
        policy.HotInterval = std::chrono::microseconds(2000);
        policy.HelperIdleInterval = std::chrono::microseconds(17000);
        policy.HelperHotInterval = std::chrono::microseconds(7000);
        policy.BackpressureCap = std::chrono::microseconds(17000);

        EspFrameSyncModel::SamplerTimingInput input{};
        input.HighRateAimSampling = true;
        input.SampleDuration = std::chrono::microseconds(1200);

        const auto interval = EspFrameSyncModel::ResolveSamplerInterval(input, policy);
        assert(interval == std::chrono::microseconds(2000));
    }

    void test_vis_debug_sampler_does_not_duplicate_debug_thread_work()
    {
        assert(!EspFrameSyncModel::ShouldBuildVisDebugInSampler(false, true, true));
        assert(!EspFrameSyncModel::ShouldBuildVisDebugInSampler(true, true, true));
        assert(EspFrameSyncModel::ShouldBuildVisDebugInSampler(true, true, false));
        assert(EspFrameSyncModel::ShouldBuildVisDebugInSampler(false, false, true));
    }

    void test_vis_debug_item_cap_is_bounded_for_render_thread()
    {
        assert(EspFrameSyncModel::ClampVisDebugMaxItems(16) == 32);
        assert(EspFrameSyncModel::ClampVisDebugMaxItems(700) == 700);
        assert(EspFrameSyncModel::ClampVisDebugMaxItems(5000) == 1000);
    }
}

int main()
{
    test_frame_timing_age_and_sample_duration();
    test_stale_tracker_counts_same_frame_reuse();
    test_cold_budget_runs_due_tasks_until_budget_exhausted();
    test_hot_sampler_backpressure_when_sampling_overruns_target();
    test_sampler_keeps_hot_interval_when_sampling_is_fast();
    test_visual_bone_sampling_keeps_idle_interval_for_freshness();
    test_sampler_policy_has_no_visual_bone_downshift();
    test_hot_sampling_uses_hot_interval();
    test_vis_debug_sampler_does_not_duplicate_debug_thread_work();
    test_vis_debug_item_cap_is_bounded_for_render_thread();
    return 0;
}
