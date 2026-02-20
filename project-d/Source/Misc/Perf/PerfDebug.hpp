#pragma once

#include <atomic>
#include <cstdint>

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

        std::atomic<std::uint64_t> VisChecks{ 0 };
        std::atomic<std::uint64_t> VisHits{ 0 };
        std::atomic<std::uint64_t> VisCheckUsSum{ 0 };
        std::atomic<std::uint64_t> VisCheckUsMax{ 0 };

        std::atomic<std::uint64_t> MapPolls{ 0 };
        std::atomic<std::uint64_t> MapHits{ 0 };
    };

    inline IntervalCounters Counters{};

    inline void RecordOverlayFrame(const std::uint64_t frameUs)
    {
        Counters.OverlayFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.OverlayFrameUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.OverlayFrameUsMax, frameUs);
    }

    inline void RecordEspFrame(const std::uint64_t frameUs, const std::uint32_t controllerCount, const std::uint32_t playersDrawn)
    {
        Counters.EspFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.EspFrameUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspFrameUsMax, frameUs);
        Counters.EspControllers.fetch_add(controllerCount, std::memory_order_relaxed);
        Counters.EspPlayersDrawn.fetch_add(playersDrawn, std::memory_order_relaxed);
    }

    inline void RecordEspSampleFrame(const std::uint64_t frameUs)
    {
        Counters.EspSampleFrames.fetch_add(1, std::memory_order_relaxed);
        Counters.EspSampleUsSum.fetch_add(frameUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.EspSampleUsMax, frameUs);
    }

    inline void RecordVisCheck(const std::uint64_t durationUs, const bool isVisible)
    {
        Counters.VisChecks.fetch_add(1, std::memory_order_relaxed);
        Counters.VisCheckUsSum.fetch_add(durationUs, std::memory_order_relaxed);
        detail::UpdateMax(Counters.VisCheckUsMax, durationUs);

        if (isVisible)
            Counters.VisHits.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RecordMapPoll(const bool hasMap)
    {
        Counters.MapPolls.fetch_add(1, std::memory_order_relaxed);
        if (hasMap)
            Counters.MapHits.fetch_add(1, std::memory_order_relaxed);
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

        const std::uint64_t visChecks = Counters.VisChecks.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visHits = Counters.VisHits.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visCheckUsSum = Counters.VisCheckUsSum.exchange(0, std::memory_order_relaxed);
        const std::uint64_t visCheckUsMax = Counters.VisCheckUsMax.exchange(0, std::memory_order_relaxed);

        const std::uint64_t mapPolls = Counters.MapPolls.exchange(0, std::memory_order_relaxed);
        const std::uint64_t mapHits = Counters.MapHits.exchange(0, std::memory_order_relaxed);

        if (overlayFrames == 0 && espFrames == 0 && espSampleFrames == 0 && visChecks == 0 && mapPolls == 0)
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

        const double visAvgMs = visChecks ? (static_cast<double>(visCheckUsSum) / static_cast<double>(visChecks)) / 1000.0 : 0.0;
        const double visMaxMs = static_cast<double>(visCheckUsMax) / 1000.0;

        LOG_INFO(
            "[perf dbg] overlay fps={:.1f} frame={:.2f}/{:.2f}ms | esp draw={:.2f}/{:.2f}ms ctr={:.1f} draw={:.1f} | esp sample={:.2f}/{:.2f}ms | vis hit={}/{} time={:.2f}/{:.2f}ms | map={}/{}",
            overlayFps,
            overlayAvgMs,
            overlayMaxMs,
            espAvgMs,
            espMaxMs,
            controllersPerFrame,
            playersPerFrame,
            sampleAvgMs,
            sampleMaxMs,
            visHits,
            visChecks,
            visAvgMs,
            visMaxMs,
            mapHits,
            mapPolls
        );
    }
}
