#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace PerfDebug
{
    namespace detail
    {
        inline void UpdateMax(std::atomic<std::uint64_t>& target, const std::uint64_t value)
        {
            std::uint64_t current = target.load(std::memory_order_relaxed);
            while (current < value &&
                !target.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed))
            {
            }
        }
    }

    struct IntervalCounters
    {
        std::atomic<std::uint64_t> OverlayFrames{ 0 };
        std::atomic<std::uint64_t> OverlayFrameUsSum{ 0 };
        std::atomic<std::uint64_t> OverlayFrameUsMax{ 0 };

        std::atomic<std::uint64_t> EspFrames{ 0 };
        std::atomic<std::uint64_t> EspFrameUsSum{ 0 };
        std::atomic<std::uint64_t> EspFrameUsMax{ 0 };
        std::atomic<std::uint64_t> EspControllers{ 0 };
        std::atomic<std::uint64_t> EspPlayersDrawn{ 0 };

        std::atomic<std::uint64_t> EspSampleFrames{ 0 };
        std::atomic<std::uint64_t> EspSampleUsSum{ 0 };
        std::atomic<std::uint64_t> EspSampleUsMax{ 0 };
        std::atomic<std::uint64_t> EspFrameAgeSamples{ 0 };
        std::atomic<std::uint64_t> EspFrameAgeUsSum{ 0 };
        std::atomic<std::uint64_t> EspFrameAgeUsMax{ 0 };
        std::atomic<std::uint64_t> EspSamplePublishUsSum{ 0 };
        std::atomic<std::uint64_t> EspSamplePublishUsMax{ 0 };
        std::atomic<std::uint64_t> EspStaleFrameReuses{ 0 };
        std::atomic<std::uint64_t> EspStaleFrameReuseMax{ 0 };
        std::atomic<std::uint64_t> EspColdTaskRuns{ 0 };
        std::atomic<std::uint64_t> EspColdTaskUsSum{ 0 };
        std::atomic<std::uint64_t> EspColdTaskUsMax{ 0 };
        std::atomic<std::uint64_t> EspColdTaskSkips{ 0 };
        std::atomic<std::uint64_t> OverlayFrameCapWaits{ 0 };
        std::atomic<std::uint64_t> OverlayFrameCapWaitUsSum{ 0 };
        std::atomic<std::uint64_t> OverlayFrameCapWaitUsMax{ 0 };

        std::atomic<std::uint64_t> VisChecks{ 0 };
        std::atomic<std::uint64_t> VisHits{ 0 };
        std::atomic<std::uint64_t> VisCheckUsSum{ 0 };
        std::atomic<std::uint64_t> VisCheckUsMax{ 0 };

        std::atomic<std::uint64_t> MapPolls{ 0 };
        std::atomic<std::uint64_t> MapHits{ 0 };

        std::atomic<std::uint64_t> TriggerHotkeyDowns{ 0 };
        std::atomic<std::uint64_t> TriggerTimeoutChecks{ 0 };
        std::atomic<std::uint64_t> TriggerTimeoutHits{ 0 };
        std::atomic<std::uint64_t> TriggerTimeoutAgeUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerTimeoutAgeUsMax{ 0 };
        std::atomic<std::uint64_t> TriggerScans{ 0 };
        std::atomic<std::uint64_t> TriggerScanUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerScanUsMax{ 0 };
        std::atomic<std::uint64_t> TriggerScanCandidates{ 0 };
        std::atomic<std::uint64_t> TriggerSnapshotFrames{ 0 };
        std::atomic<std::uint64_t> TriggerSnapshotEmptyFrames{ 0 };
        std::atomic<std::uint64_t> TriggerFallbackScans{ 0 };
        std::atomic<std::uint64_t> TriggerCycles{ 0 };
        std::atomic<std::uint64_t> TriggerCycleUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerCycleUsMax{ 0 };
        std::atomic<std::uint64_t> TriggerPreFireGateHits{ 0 };
        std::atomic<std::uint64_t> TriggerFires{ 0 };
        std::atomic<std::uint64_t> TriggerTimeoutFires{ 0 };
        std::atomic<std::uint64_t> TriggerDetectFires{ 0 };
        std::atomic<std::uint64_t> TriggerFireHotkeyAgeUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerFireHotkeyAgeUsMax{ 0 };
        std::atomic<std::uint64_t> TriggerDecisionUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerDecisionUsMax{ 0 };
        std::atomic<std::uint64_t> TriggerSendUsSum{ 0 };
        std::atomic<std::uint64_t> TriggerSendUsMax{ 0 };

        std::atomic<std::uint64_t> FlickAutowallChecks{ 0 };
        std::atomic<std::uint64_t> FlickAutowallPass{ 0 };
        std::atomic<std::uint64_t> FlickAutowallQueryFail{ 0 };
        std::atomic<std::uint64_t> FlickAutowallPenetrable{ 0 };
        std::atomic<std::uint64_t> FlickAutowallRequirementPass{ 0 };
        std::atomic<std::uint64_t> FlickAutowallDamageMilliSum{ 0 };
        std::atomic<std::uint64_t> FlickAutowallDamageMilliMax{ 0 };
        std::atomic<std::uint64_t> FlickAutowallRequirementMilliSum{ 0 };
        std::atomic<std::uint64_t> FlickAutowallRequirementMilliMax{ 0 };
    };

    inline IntervalCounters Counters{};
    inline std::atomic<bool> DebugEnabled{ false };
    inline std::atomic<bool> DebugPerf{ false };
    inline std::atomic<bool> DebugTrigger{ false };
    inline std::atomic<bool> DebugVisCheck{ false };
    inline std::atomic<bool> DebugAutowall{ false };
    inline std::atomic<bool> DebugThreadStopRequested{ false };
    inline std::mutex DebugThreadMutex{};
    inline std::thread DebugThread{};
    inline std::mutex VisDebugTickMutex{};
    inline std::function<void()> VisDebugTick{};

    inline bool IsPerfCollectionEnabled()
    {
        return DebugEnabled.load(std::memory_order_relaxed) && DebugPerf.load(std::memory_order_relaxed);
    }

    inline bool IsTriggerCollectionEnabled()
    {
        return DebugEnabled.load(std::memory_order_relaxed) && DebugTrigger.load(std::memory_order_relaxed);
    }

    inline bool IsVisDebugEnabled()
    {
        return DebugEnabled.load(std::memory_order_relaxed) && DebugVisCheck.load(std::memory_order_relaxed);
    }

    inline bool IsAutowallCollectionEnabled()
    {
        return DebugEnabled.load(std::memory_order_relaxed) && DebugAutowall.load(std::memory_order_relaxed);
    }

    inline void RecordOverlayFrame(const std::uint64_t frameUs)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.OverlayFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.OverlayFrameUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.OverlayFrameUsMax, frameUs);
    }

    inline void RecordEspFrame(const std::uint64_t frameUs, const std::uint32_t controllerCount, const std::uint32_t playersDrawn)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.EspFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.EspFrameUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspFrameUsMax, frameUs);
        Counters.EspControllers.fetch_add(controllerCount, std::memory_order_relaxed);
        Counters.EspPlayersDrawn.fetch_add(playersDrawn, std::memory_order_relaxed);
    }

    inline void RecordEspSampleFrame(const std::uint64_t frameUs)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.EspSampleFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.EspSampleUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspSampleUsMax, frameUs);
    }

    inline void RecordEspFrameTiming(
        const std::uint64_t frameAgeUs,
        const std::uint64_t sampleToPublishUs,
        const bool reusedPreviousFrame,
        const std::uint64_t consecutiveReuseCount)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.EspFrameAgeSamples.fetch_add(1, std::memory_order_relaxed);
        Counters.EspFrameAgeUsSum.fetch_add(frameAgeUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspFrameAgeUsMax, frameAgeUs);
        Counters.EspSamplePublishUsSum.fetch_add(sampleToPublishUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspSamplePublishUsMax, sampleToPublishUs);

        if (reusedPreviousFrame)
        {
            Counters.EspStaleFrameReuses.fetch_add(1, std::memory_order_relaxed);
            detail::UpdateMax(Counters.EspStaleFrameReuseMax, consecutiveReuseCount);
        }
    }

    inline void RecordEspColdTask(const std::uint64_t durationUs)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.EspColdTaskRuns.fetch_add(1, std::memory_order_relaxed);
        Counters.EspColdTaskUsSum.fetch_add(durationUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspColdTaskUsMax, durationUs);
    }

    inline void RecordEspColdTaskSkip()
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.EspColdTaskSkips.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordOverlayFrameCapWait(const std::uint64_t waitUs)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.OverlayFrameCapWaits.fetch_add(1, std::memory_order_relaxed);
        Counters.OverlayFrameCapWaitUsSum.fetch_add(waitUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.OverlayFrameCapWaitUsMax, waitUs);
    }

    inline void RecordVisCheck(const std::uint64_t durationUs, const bool isVisible)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.VisChecks.fetch_add(1, std::memory_order_relaxed);
        Counters.VisCheckUsSum.fetch_add(durationUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.VisCheckUsMax, durationUs);

        if (isVisible)
            Counters.VisHits.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordMapPoll(const bool hasMap)
    {
        if (!IsPerfCollectionEnabled())
            return;

        Counters.MapPolls.fetch_add(1, std::memory_order_relaxed);
        if (hasMap)
            Counters.MapHits.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerHotkeyDown()
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerHotkeyDowns.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerTimeoutCheck(const std::uint64_t hotkeyAgeUs, const bool timeoutHit)
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerTimeoutChecks.fetch_add(1, std::memory_order_relaxed);
        Counters.TriggerTimeoutAgeUsSum.fetch_add(hotkeyAgeUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerTimeoutAgeUsMax, hotkeyAgeUs);
        if (timeoutHit)
            Counters.TriggerTimeoutHits.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerScan(const std::uint64_t scanUs, const bool hasCandidate)
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerScans.fetch_add(1, std::memory_order_relaxed);
        Counters.TriggerScanUsSum.fetch_add(scanUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerScanUsMax, scanUs);
        if (hasCandidate)
            Counters.TriggerScanCandidates.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerSnapshotFrame(const bool isEmpty)
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerSnapshotFrames.fetch_add(1, std::memory_order_relaxed);
        if (isEmpty)
            Counters.TriggerSnapshotEmptyFrames.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerFallbackScan()
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerFallbackScans.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerCycle(const std::uint64_t cycleUs)
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerCycles.fetch_add(1, std::memory_order_relaxed);
        Counters.TriggerCycleUsSum.fetch_add(cycleUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerCycleUsMax, cycleUs);
    }

    inline void RecordTriggerPreFireGate()
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerPreFireGateHits.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordTriggerFire(
        const std::uint64_t hotkeyAgeUs,
        const std::uint64_t decisionUs,
        const std::uint64_t sendUs,
        const bool byTimeout)
    {
        if (!IsTriggerCollectionEnabled())
            return;

        Counters.TriggerFires.fetch_add(1, std::memory_order_relaxed);
        Counters.TriggerFireHotkeyAgeUsSum.fetch_add(hotkeyAgeUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerFireHotkeyAgeUsMax, hotkeyAgeUs);
        Counters.TriggerDecisionUsSum.fetch_add(decisionUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerDecisionUsMax, decisionUs);
        Counters.TriggerSendUsSum.fetch_add(sendUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.TriggerSendUsMax, sendUs);

        if (byTimeout)
            Counters.TriggerTimeoutFires.fetch_add(1, std::memory_order_relaxed);
        else
            Counters.TriggerDetectFires.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordFlickAutowallCheck(
        const bool canPenetrate,
        const bool passRequirement,
        const bool queryFailed,
        const float damage,
        const float requiredDamage)
    {
        if (!IsAutowallCollectionEnabled())
            return;

        Counters.FlickAutowallChecks.fetch_add(1, std::memory_order_relaxed);
        if (canPenetrate)
            Counters.FlickAutowallPenetrable.fetch_add(1, std::memory_order_relaxed);
        if (passRequirement)
            Counters.FlickAutowallRequirementPass.fetch_add(1, std::memory_order_relaxed);
        if (canPenetrate && passRequirement)
            Counters.FlickAutowallPass.fetch_add(1, std::memory_order_relaxed);
        if (queryFailed)
            Counters.FlickAutowallQueryFail.fetch_add(1, std::memory_order_relaxed);

        const float clampedDamage = std::clamp(damage, 0.0f, 10000.0f);
        const std::uint64_t damageMilli = static_cast<std::uint64_t>(std::llround(static_cast<double>(clampedDamage) * 1000.0));
        Counters.FlickAutowallDamageMilliSum.fetch_add(damageMilli, std::memory_order_relaxed);
        detail::UpdateMax(Counters.FlickAutowallDamageMilliMax, damageMilli);

        const float clampedRequirement = std::clamp(requiredDamage, 0.0f, 10000.0f);
        const std::uint64_t requirementMilli = static_cast<std::uint64_t>(std::llround(static_cast<double>(clampedRequirement) * 1000.0));
        Counters.FlickAutowallRequirementMilliSum.fetch_add(requirementMilli, std::memory_order_relaxed);
        detail::UpdateMax(Counters.FlickAutowallRequirementMilliMax, requirementMilli);
    }

    inline void LogInterval(double intervalSeconds);

    inline void SetDebugOptions(const bool enabled, const bool perfEnabled, const bool triggerEnabled, const bool visCheckEnabled = false, const bool autowallEnabled = false)
    {
        DebugEnabled.store(enabled, std::memory_order_relaxed);
        DebugPerf.store(enabled && perfEnabled, std::memory_order_relaxed);
        DebugTrigger.store(enabled && triggerEnabled, std::memory_order_relaxed);
        DebugVisCheck.store(enabled && visCheckEnabled, std::memory_order_relaxed);
        DebugAutowall.store(enabled && autowallEnabled, std::memory_order_relaxed);
    }

    inline void SetVisDebugTick(std::function<void()> tick)
    {
        std::lock_guard lock(VisDebugTickMutex);
        VisDebugTick = std::move(tick);
    }

    inline void InvokeVisDebugTick()
    {
        if (!IsVisDebugEnabled())
            return;

        std::function<void()> callback{};
        {
            std::lock_guard lock(VisDebugTickMutex);
            callback = VisDebugTick;
        }

        if (!callback)
            return;

        try
        {
            callback();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Vis debug tick failed: {}", e.what());
        }
        catch (...)
        {
            LOG_ERROR("Vis debug tick failed: unknown exception");
        }
    }

    inline void StartDebugThreadIfNeeded()
    {
        std::lock_guard lock(DebugThreadMutex);
        if (DebugThread.joinable())
            return;

        DebugThreadStopRequested.store(false, std::memory_order_relaxed);
        DebugThread = std::thread([]()
        {
            constexpr auto logInterval = std::chrono::seconds(1);
            constexpr auto visTickInterval = std::chrono::milliseconds(33);
            auto lastLogAt = std::chrono::steady_clock::now();
            auto lastVisTickAt = lastLogAt;

            while (!DebugThreadStopRequested.load(std::memory_order_relaxed))
            {
                const auto now = std::chrono::steady_clock::now();

                if (now - lastVisTickAt >= visTickInterval)
                {
                    InvokeVisDebugTick();
                    lastVisTickAt = now;
                }

                if (now - lastLogAt >= logInterval)
                {
                    const double elapsedSeconds = std::chrono::duration<double>(now - lastLogAt).count();
                    LogInterval(elapsedSeconds);
                    lastLogAt = now;
                }

                if (DebugThreadStopRequested.load(std::memory_order_relaxed))
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            const auto now = std::chrono::steady_clock::now();
            const double elapsedSeconds = std::chrono::duration<double>(now - lastLogAt).count();
            LogInterval(elapsedSeconds);
        });
    }

    inline void StopDebugThreadIfNeeded()
    {
        std::thread worker{};
        {
            std::lock_guard lock(DebugThreadMutex);
            if (!DebugThread.joinable())
                return;

            DebugThreadStopRequested.store(true, std::memory_order_relaxed);
            worker = std::move(DebugThread);
        }

        if (worker.joinable())
            worker.join();
    }

    inline void SyncDebugThread()
    {
        if (DebugEnabled.load(std::memory_order_relaxed))
            StartDebugThreadIfNeeded();
        else
            StopDebugThreadIfNeeded();
    }

    inline void ShutdownDebugThread()
    {
        StopDebugThreadIfNeeded();
    }

    inline void LogInterval(const double intervalSeconds = 1.0)
    {
        const double safeIntervalSeconds = intervalSeconds > 0.0 ? intervalSeconds : 1.0;

        const std::uint64_t overlayFrames = Counters.OverlayFrames.exchange(0, std::memory_order_relaxed);
        const std::uint64_t overlayFrameUsSum = Counters.OverlayFrameUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t overlayFrameUsMax = Counters.OverlayFrameUsMax.exchange(0, std::memory_order_relaxed);

        const std::uint64_t espFrames = Counters.EspFrames.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espFrameUsSum = Counters.EspFrameUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espFrameUsMax = Counters.EspFrameUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espControllers = Counters.EspControllers.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espPlayersDrawn = Counters.EspPlayersDrawn.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espSampleFrames = Counters.EspSampleFrames.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espSampleUsSum = Counters.EspSampleUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espSampleUsMax = Counters.EspSampleUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espFrameAgeSamples = Counters.EspFrameAgeSamples.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espFrameAgeUsSum = Counters.EspFrameAgeUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espFrameAgeUsMax = Counters.EspFrameAgeUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espSamplePublishUsSum = Counters.EspSamplePublishUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espSamplePublishUsMax = Counters.EspSamplePublishUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espStaleFrameReuses = Counters.EspStaleFrameReuses.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espStaleFrameReuseMax = Counters.EspStaleFrameReuseMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espColdTaskRuns = Counters.EspColdTaskRuns.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espColdTaskUsSum = Counters.EspColdTaskUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espColdTaskUsMax = Counters.EspColdTaskUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t espColdTaskSkips = Counters.EspColdTaskSkips.exchange(0, std::memory_order_relaxed);
        const std::uint64_t overlayFrameCapWaits = Counters.OverlayFrameCapWaits.exchange(0, std::memory_order_relaxed);
        const std::uint64_t overlayFrameCapWaitUsSum = Counters.OverlayFrameCapWaitUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t overlayFrameCapWaitUsMax = Counters.OverlayFrameCapWaitUsMax.exchange(0, std::memory_order_relaxed);

        const std::uint64_t visChecks = Counters.VisChecks.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visHits = Counters.VisHits.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visCheckUsSum = Counters.VisCheckUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visCheckUsMax = Counters.VisCheckUsMax.exchange(0, std::memory_order_relaxed);

        const std::uint64_t mapPolls = Counters.MapPolls.exchange(0, std::memory_order_relaxed);
        const std::uint64_t mapHits = Counters.MapHits.exchange(0, std::memory_order_relaxed);

        const std::uint64_t triggerHotkeyDowns = Counters.TriggerHotkeyDowns.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerTimeoutChecks = Counters.TriggerTimeoutChecks.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerTimeoutHits = Counters.TriggerTimeoutHits.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerTimeoutAgeUsSum = Counters.TriggerTimeoutAgeUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerTimeoutAgeUsMax = Counters.TriggerTimeoutAgeUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerScans = Counters.TriggerScans.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerScanUsSum = Counters.TriggerScanUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerScanUsMax = Counters.TriggerScanUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerScanCandidates = Counters.TriggerScanCandidates.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerSnapshotFrames = Counters.TriggerSnapshotFrames.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerSnapshotEmptyFrames = Counters.TriggerSnapshotEmptyFrames.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerFallbackScans = Counters.TriggerFallbackScans.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerCycles = Counters.TriggerCycles.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerCycleUsSum = Counters.TriggerCycleUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerCycleUsMax = Counters.TriggerCycleUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerPreFireGateHits = Counters.TriggerPreFireGateHits.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerFires = Counters.TriggerFires.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerTimeoutFires = Counters.TriggerTimeoutFires.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerDetectFires = Counters.TriggerDetectFires.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerFireHotkeyAgeUsSum = Counters.TriggerFireHotkeyAgeUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerFireHotkeyAgeUsMax = Counters.TriggerFireHotkeyAgeUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerDecisionUsSum = Counters.TriggerDecisionUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerDecisionUsMax = Counters.TriggerDecisionUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerSendUsSum = Counters.TriggerSendUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t triggerSendUsMax = Counters.TriggerSendUsMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallChecks = Counters.FlickAutowallChecks.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallPass = Counters.FlickAutowallPass.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallQueryFail = Counters.FlickAutowallQueryFail.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallPenetrable = Counters.FlickAutowallPenetrable.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallRequirementPass = Counters.FlickAutowallRequirementPass.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallDamageMilliSum = Counters.FlickAutowallDamageMilliSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallDamageMilliMax = Counters.FlickAutowallDamageMilliMax.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallRequirementMilliSum = Counters.FlickAutowallRequirementMilliSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t flickAutowallRequirementMilliMax = Counters.FlickAutowallRequirementMilliMax.exchange(0, std::memory_order_relaxed);

        if (overlayFrames == 0 && espFrames == 0 && espSampleFrames == 0 && espFrameAgeSamples == 0 &&
            overlayFrameCapWaits == 0 && espColdTaskRuns == 0 && espColdTaskSkips == 0 &&
            visChecks == 0 && mapPolls == 0 &&
            triggerHotkeyDowns == 0 && triggerTimeoutChecks == 0 && triggerScans == 0 && triggerFires == 0 &&
            triggerSnapshotFrames == 0 && triggerFallbackScans == 0 && triggerPreFireGateHits == 0 &&
            flickAutowallChecks == 0)
            return;

        const double overlayFps = static_cast<double>(overlayFrames) / safeIntervalSeconds;
        const double overlayAvgMs = overlayFrames ? (static_cast<double>(overlayFrameUsSum) / static_cast<double>(overlayFrames)) / 1000.0 : 0.0;
        const double overlayMaxMs = static_cast<double>(overlayFrameUsMax) / 1000.0;

        const double espAvgMs = espFrames ? (static_cast<double>(espFrameUsSum) / static_cast<double>(espFrames)) / 1000.0 : 0.0;
        const double espMaxMs = static_cast<double>(espFrameUsMax) / 1000.0;
        const double controllersPerFrame = espFrames ? static_cast<double>(espControllers) / static_cast<double>(espFrames) : 0.0;
        const double playersPerFrame = espFrames ? static_cast<double>(espPlayersDrawn) / static_cast<double>(espFrames) : 0.0;
        const double sampleAvgMs = espSampleFrames ? (static_cast<double>(espSampleUsSum) / static_cast<double>(espSampleFrames)) / 1000.0 : 0.0;
        const double sampleMaxMs = static_cast<double>(espSampleUsMax) / 1000.0;
        const double frameAgeAvgMs = espFrameAgeSamples ? (static_cast<double>(espFrameAgeUsSum) / static_cast<double>(espFrameAgeSamples)) / 1000.0 : 0.0;
        const double frameAgeMaxMs = static_cast<double>(espFrameAgeUsMax) / 1000.0;
        const double samplePublishAvgMs = espFrameAgeSamples ? (static_cast<double>(espSamplePublishUsSum) / static_cast<double>(espFrameAgeSamples)) / 1000.0 : 0.0;
        const double samplePublishMaxMs = static_cast<double>(espSamplePublishUsMax) / 1000.0;
        const double coldTaskAvgMs = espColdTaskRuns ? (static_cast<double>(espColdTaskUsSum) / static_cast<double>(espColdTaskRuns)) / 1000.0 : 0.0;
        const double coldTaskMaxMs = static_cast<double>(espColdTaskUsMax) / 1000.0;
        const double overlayWaitAvgMs = overlayFrameCapWaits ? (static_cast<double>(overlayFrameCapWaitUsSum) / static_cast<double>(overlayFrameCapWaits)) / 1000.0 : 0.0;
        const double overlayWaitMaxMs = static_cast<double>(overlayFrameCapWaitUsMax) / 1000.0;

        const double visAvgMs = visChecks ? (static_cast<double>(visCheckUsSum) / static_cast<double>(visChecks)) / 1000.0 : 0.0;
        const double visMaxMs = static_cast<double>(visCheckUsMax) / 1000.0;

        const bool logPerf = DebugPerf.load(std::memory_order_relaxed);
        const bool logTrigger = DebugTrigger.load(std::memory_order_relaxed);
        const bool logAutowall = DebugAutowall.load(std::memory_order_relaxed);
        if (!logPerf && !logTrigger && !logAutowall)
            return;

        const double triggerTimeoutAgeAvgMs = triggerTimeoutChecks ? (static_cast<double>(triggerTimeoutAgeUsSum) / static_cast<double>(triggerTimeoutChecks)) / 1000.0 : 0.0;
        const double triggerTimeoutAgeMaxMs = static_cast<double>(triggerTimeoutAgeUsMax) / 1000.0;
        const double triggerScanAvgMs = triggerScans ? (static_cast<double>(triggerScanUsSum) / static_cast<double>(triggerScans)) / 1000.0 : 0.0;
        const double triggerScanMaxMs = static_cast<double>(triggerScanUsMax) / 1000.0;
        const double triggerCycleAvgMs = triggerCycles ? (static_cast<double>(triggerCycleUsSum) / static_cast<double>(triggerCycles)) / 1000.0 : 0.0;
        const double triggerCycleMaxMs = static_cast<double>(triggerCycleUsMax) / 1000.0;
        const double triggerFireAgeAvgMs = triggerFires ? (static_cast<double>(triggerFireHotkeyAgeUsSum) / static_cast<double>(triggerFires)) / 1000.0 : 0.0;
        const double triggerFireAgeMaxMs = static_cast<double>(triggerFireHotkeyAgeUsMax) / 1000.0;
        const double triggerDecisionAvgMs = triggerFires ? (static_cast<double>(triggerDecisionUsSum) / static_cast<double>(triggerFires)) / 1000.0 : 0.0;
        const double triggerDecisionMaxMs = static_cast<double>(triggerDecisionUsMax) / 1000.0;
        const double triggerSendAvgMs = triggerFires ? (static_cast<double>(triggerSendUsSum) / static_cast<double>(triggerFires)) / 1000.0 : 0.0;
        const double triggerSendMaxMs = static_cast<double>(triggerSendUsMax) / 1000.0;
        const double flickAutowallDamageAvg = flickAutowallChecks
            ? static_cast<double>(flickAutowallDamageMilliSum) / static_cast<double>(flickAutowallChecks) / 1000.0
            : 0.0;
        const double flickAutowallDamageMax = static_cast<double>(flickAutowallDamageMilliMax) / 1000.0;
        const double flickAutowallRequirementAvg = flickAutowallChecks
            ? static_cast<double>(flickAutowallRequirementMilliSum) / static_cast<double>(flickAutowallChecks) / 1000.0
            : 0.0;
        const double flickAutowallRequirementMax = static_cast<double>(flickAutowallRequirementMilliMax) / 1000.0;

        if (logPerf)
        {
            LOG_INFO(
                "[perf dbg] overlay fps={:.1f} frame={:.2f}/{:.2f}ms cap_wait={:.2f}/{:.2f}ms | esp draw={:.2f}/{:.2f}ms ctr={:.1f} draw={:.1f} | esp sample={:.2f}/{:.2f}ms age={:.2f}/{:.2f}ms pub={:.2f}/{:.2f}ms stale={}/{} | cold run/skip={}/{} time={:.2f}/{:.2f}ms | vis hit={}/{} time={:.2f}/{:.2f}ms | map={}/{}",
                overlayFps,
                overlayAvgMs,
                overlayMaxMs,
                overlayWaitAvgMs,
                overlayWaitMaxMs,
                espAvgMs,
                espMaxMs,
                controllersPerFrame,
                playersPerFrame,
                sampleAvgMs,
                sampleMaxMs,
                frameAgeAvgMs,
                frameAgeMaxMs,
                samplePublishAvgMs,
                samplePublishMaxMs,
                espStaleFrameReuses,
                espStaleFrameReuseMax,
                espColdTaskRuns,
                espColdTaskSkips,
                coldTaskAvgMs,
                coldTaskMaxMs,
                visHits,
                visChecks,
                visAvgMs,
                visMaxMs,
                mapHits,
                mapPolls
            );
        }

        if (logTrigger)
        {
            LOG_INFO(
                "[trigger dbg] hk_down={} | timeout hit/check={}/{} age={:.2f}/{:.2f}ms | scan cand/scan={}/{} time={:.2f}/{:.2f}ms | snapshot empty/all={}/{} fallback={} prefire_gate={} | cycle={:.2f}/{:.2f}ms | fire={} timeout={} detect={} | fire_age={:.2f}/{:.2f}ms decision={:.2f}/{:.2f}ms send={:.2f}/{:.2f}ms",
                triggerHotkeyDowns,
                triggerTimeoutHits,
                triggerTimeoutChecks,
                triggerTimeoutAgeAvgMs,
                triggerTimeoutAgeMaxMs,
                triggerScanCandidates,
                triggerScans,
                triggerScanAvgMs,
                triggerScanMaxMs,
                triggerSnapshotEmptyFrames,
                triggerSnapshotFrames,
                triggerFallbackScans,
                triggerPreFireGateHits,
                triggerCycleAvgMs,
                triggerCycleMaxMs,
                triggerFires,
                triggerTimeoutFires,
                triggerDetectFires,
                triggerFireAgeAvgMs,
                triggerFireAgeMaxMs,
                triggerDecisionAvgMs,
                triggerDecisionMaxMs,
                triggerSendAvgMs,
                triggerSendMaxMs
            );
        }

        if (logAutowall)
        {
            LOG_INFO(
                "[autowall dbg] flick check={} pen={} req_pass={} pass={} query_fail={} | dmg={:.2f}/{:.2f} req={:.2f}/{:.2f}",
                flickAutowallChecks,
                flickAutowallPenetrable,
                flickAutowallRequirementPass,
                flickAutowallPass,
                flickAutowallQueryFail,
                flickAutowallDamageAvg,
                flickAutowallDamageMax,
                flickAutowallRequirementAvg,
                flickAutowallRequirementMax
            );
        }
    }
}
