#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace nova::audio {

// Lock-free counters written by the single real-time callback and read by UI.
struct RealtimeAudioHealthMonitor {
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<std::uint64_t> deadlineMisses{0};
    std::atomic<std::uint64_t> dropouts{0};
    std::atomic<std::uint64_t> longestCallbackNs{0};
    std::atomic<double> lastCallbackMs{0.0};
    std::atomic<double> lastBudgetMs{0.0};
    std::atomic<double> maxLoadPercent{0.0};

    void record(std::uint64_t elapsedNs, double budgetMs) noexcept {
        callbacks.fetch_add(1, std::memory_order_relaxed);
        const double elapsedMs = static_cast<double>(elapsedNs) / 1.0e6;
        lastCallbackMs.store(elapsedMs, std::memory_order_relaxed);
        lastBudgetMs.store(budgetMs, std::memory_order_relaxed);
        const double load = budgetMs > 0.0 ? elapsedMs * 100.0 / budgetMs : 0.0;
        double old = maxLoadPercent.load(std::memory_order_relaxed);
        while (load > old && !maxLoadPercent.compare_exchange_weak(
            old, load, std::memory_order_relaxed, std::memory_order_relaxed)) {}
        auto longest = longestCallbackNs.load(std::memory_order_relaxed);
        while (elapsedNs > longest && !longestCallbackNs.compare_exchange_weak(
            longest, elapsedNs, std::memory_order_relaxed, std::memory_order_relaxed)) {}
        if (budgetMs > 0.0 && elapsedMs > budgetMs) {
            deadlineMisses.fetch_add(1, std::memory_order_relaxed);
            // A callback that overruns its entire block period is a missed
            // delivery deadline. This is a software dropout-risk counter,
            // not proof that the driver emitted silence.
            dropouts.fetch_add(1, std::memory_order_relaxed);
        }
    }
    void reset() noexcept {
        callbacks.store(0); deadlineMisses.store(0); dropouts.store(0);
        longestCallbackNs.store(0); lastCallbackMs.store(0.0);
        lastBudgetMs.store(0.0); maxLoadPercent.store(0.0);
    }
};

class CallbackDeadlineScope final {
public:
    CallbackDeadlineScope(RealtimeAudioHealthMonitor& monitor, int frames,
                          double sampleRate) noexcept
        : monitor_(monitor), start_(Clock::now()),
          budgetMs_(sampleRate > 0.0 && frames > 0
              ? 1000.0 * static_cast<double>(frames) / sampleRate : 0.0) {}
    ~CallbackDeadlineScope() noexcept {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start_).count();
        monitor_.record(static_cast<std::uint64_t>(elapsed), budgetMs_);
    }
private:
    using Clock = std::chrono::steady_clock;
    RealtimeAudioHealthMonitor& monitor_;
    Clock::time_point start_;
    double budgetMs_;
};

struct PhysicalLoopbackMetrics {
    std::atomic<std::uint64_t> packets{0}, frames{0}, discontinuities{0}, silentPackets{0};
    std::atomic<int> sampleRate{0}, channels{0};
    std::atomic<float> rmsDbfs{-120.0f}, peakDbfs{-120.0f};
    std::atomic<bool> active{false};

    void reset() noexcept {
        packets.store(0); frames.store(0); discontinuities.store(0); silentPackets.store(0);
        sampleRate.store(0); channels.store(0); rmsDbfs.store(-120.0f); peakDbfs.store(-120.0f);
        active.store(false);
    }
    void processInterleaved(const float* samples, unsigned channelCount,
                            unsigned frameCount, unsigned rate, bool discontinuity,
                            bool silent) noexcept {
        if (!samples || channelCount == 0 || frameCount == 0) return;
        double sum = 0.0; float peak = 0.0f;
        const std::uint64_t count = static_cast<std::uint64_t>(channelCount) * frameCount;
        for (std::uint64_t i = 0; i < count; ++i) {
            const float x = samples[i];
            const float a = std::abs(x);
            if (a > peak) peak = a;
            sum += static_cast<double>(x) * x;
        }
        const auto toDb = [](float x) noexcept {
            return x > 1.0e-6f ? 20.0f * std::log10(x) : -120.0f;
        };
        rmsDbfs.store(toDb(static_cast<float>(std::sqrt(sum / static_cast<double>(count)))), std::memory_order_relaxed);
        peakDbfs.store(toDb(peak), std::memory_order_relaxed);
        packets.fetch_add(1, std::memory_order_relaxed);
        frames.fetch_add(frameCount, std::memory_order_relaxed);
        if (discontinuity) discontinuities.fetch_add(1, std::memory_order_relaxed);
        if (silent) silentPackets.fetch_add(1, std::memory_order_relaxed);
        sampleRate.store(static_cast<int>(rate), std::memory_order_relaxed);
        channels.store(static_cast<int>(channelCount), std::memory_order_relaxed);
        active.store(true, std::memory_order_release);
    }
};
}
