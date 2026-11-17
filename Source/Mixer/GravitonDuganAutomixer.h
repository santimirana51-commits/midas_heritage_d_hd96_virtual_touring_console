#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>

namespace nova::graviton::detail {
template <typename T>
inline T limit(T minimum, T maximum, T value) noexcept {
    if (!std::isfinite(value)) return minimum;
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}
}

namespace nova::graviton {

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapter 28: Dan Dugan Speech Automixer Engine
// Standard Gain-Sharing Automixer for up to 32 Speech Channels across 3 Groups
// ==============================================================================
class DuganAutomixer final {
public:
    static constexpr size_t kMaxChannels = 32;
    static constexpr size_t kNumGroups = 3;

    struct ChannelParams {
        std::atomic<int> group{ 0 }; // 0 = Off, 1 = Group 1, 2 = Group 2, 3 = Group 3
        std::atomic<float> weightDb{ 0.0f }; // -12.0 to +12.0 dB
        std::atomic<float> currentGainDb{ 0.0f }; // Reported gain reduction/boost (dB) for UI meters
        std::atomic<bool> mute{ false };

        // Internal DSP envelope state
        double envelope = 0.0;
        double targetGain = 1.0;
        double smoothedGain = 1.0;

        // Time constants (in seconds, configurable per-channel or globally)
        double attackTimeMs = 5.0;    // Default: 5 ms (speech)
        double releaseTimeMs = 250.0; // Default: 250 ms (speech)
        double smoothingTimeMs = 15.0; // Default: 15 ms
    };

    struct GroupParams {
        std::atomic<bool> enabled{ true };
        std::atomic<bool> musicOverride{ false };
        std::atomic<float> musicOverrideDuckerDb{ -20.0f }; // -40.0 to 0.0 dB
        std::atomic<float> ambientFloorDb{ -60.0f }; // Noise floor baseline
    };

    DuganAutomixer() noexcept {
        reset();
    }

    void reset() noexcept {
        for (auto& ch : channels) {
            ch.group.store(0, std::memory_order_relaxed);
            ch.weightDb.store(0.0f, std::memory_order_relaxed);
            ch.currentGainDb.store(0.0f, std::memory_order_relaxed);
            ch.mute.store(false, std::memory_order_relaxed);
            ch.envelope = 0.0;
            ch.targetGain = 1.0;
            ch.smoothedGain = 1.0;
        }
        for (auto& grp : groups) {
            grp.enabled.store(true, std::memory_order_relaxed);
            grp.musicOverride.store(false, std::memory_order_relaxed);
            grp.musicOverrideDuckerDb.store(-20.0f, std::memory_order_relaxed);
            grp.ambientFloorDb.store(-60.0f, std::memory_order_relaxed);
        }
        sampleRate = 48000.0;
        updateTimeConstants();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
            sampleRate = sr;
            updateTimeConstants();
        }
    }

    ChannelParams& getChannel(size_t ch) noexcept {
        return channels[ch < kMaxChannels ? ch : 0];
    }
    const ChannelParams& getChannel(size_t ch) const noexcept {
        return channels[ch < kMaxChannels ? ch : 0];
    }

    GroupParams& getGroup(size_t grp) noexcept {
        return groups[grp < kNumGroups ? grp : 0];
    }
    const GroupParams& getGroup(size_t grp) const noexcept {
        return groups[grp < kNumGroups ? grp : 0];
    }

    // Process a single sample for a specific channel
    // In Dugan gain-sharing: g_k = L_k / L_total
    double processSample(size_t chIndex, double drySample, int activeGroup) noexcept {
        if (chIndex >= kMaxChannels || activeGroup <= 0 || activeGroup > static_cast<int>(kNumGroups)) {
            return drySample;
        }

        auto& ch = channels[chIndex];
        const auto grpIdx = static_cast<size_t>(activeGroup - 1);
        const auto& grp = groups[grpIdx];

        if (!grp.enabled.load(std::memory_order_relaxed) || ch.mute.load(std::memory_order_relaxed)) {
            return drySample;
        }

        // Fast envelope follower
        drySample = sanitize(drySample);
        const double absSig = std::abs(drySample);
        if (absSig > ch.envelope) {
            ch.envelope = sanitize(attackCoeff * ch.envelope + (1.0 - attackCoeff) * absSig);
        } else {
            ch.envelope = sanitize(releaseCoeff * ch.envelope + (1.0 - releaseCoeff) * absSig);
        }

        // Per-sample smooth gain interpolation
        ch.smoothedGain = sanitize(gainSmoothingCoeff * ch.smoothedGain + (1.0 - gainSmoothingCoeff) * ch.targetGain);

        return sanitize(drySample * ch.smoothedGain);
    }

    // Update gain-sharing matrix across all active channels (called once per block)
    void updateGainSharing() noexcept {
        std::array<double, kNumGroups> groupSum{};
        std::array<int, kNumGroups> activeCount{};

        for (size_t g = 0; g < kNumGroups; ++g) {
            const double floorLinear = dbToGain(detail::limit(-90.0, -20.0,
                static_cast<double>(groups[g].ambientFloorDb.load(std::memory_order_relaxed))));
            groupSum[g] = floorLinear; // Baseline ambient floor
            activeCount[g] = 0;
        }

        // 1. Calculate weighted envelope sums per group
        for (size_t c = 0; c < kMaxChannels; ++c) {
            auto& ch = channels[c];
            const int g = ch.group.load(std::memory_order_relaxed);
            if (g >= 1 && g <= static_cast<int>(kNumGroups) && !ch.mute.load(std::memory_order_relaxed)) {
                const auto gIdx = static_cast<size_t>(g - 1);
                const double weightLinear = dbToGain(detail::limit(
                    -12.0, 12.0,
                    static_cast<double>(ch.weightDb.load(std::memory_order_relaxed))));
                const double weightedLevel = ch.envelope * weightLinear;
                groupSum[gIdx] += weightedLevel;
                activeCount[gIdx]++;
            }
        }

        // 2. Compute individual gain factors: g_k = L_k / L_total (bounded <= 1.0)
        for (size_t c = 0; c < kMaxChannels; ++c) {
            auto& ch = channels[c];
            const int g = ch.group.load(std::memory_order_relaxed);
            if (g >= 1 && g <= static_cast<int>(kNumGroups) && !ch.mute.load(std::memory_order_relaxed)) {
                const auto gIdx = static_cast<size_t>(g - 1);
                const auto& grp = groups[gIdx];

                if (!grp.enabled.load(std::memory_order_relaxed)) {
                    ch.targetGain = 1.0;
                    ch.smoothedGain = 1.0;
                    ch.currentGainDb.store(0.0f, std::memory_order_relaxed);
                    continue;
                }

                const double weightLinear = dbToGain(detail::limit(
                    -12.0, 12.0,
                    static_cast<double>(ch.weightDb.load(std::memory_order_relaxed))));
                const double weightedLevel = ch.envelope * weightLinear;
                const double total = std::max(groupSum[gIdx], 1.0e-7);

                // Dugan formula: targetGain = weightedLevel / total
                double target = detail::limit(0.0001, 1.0, weightedLevel / total);

                // Apply Music Override ducker if active on this group
                if (grp.musicOverride.load(std::memory_order_relaxed)) {
                    const double duckerLinear = dbToGain(detail::limit(
                        static_cast<double>(grp.musicOverrideDuckerDb.load(std::memory_order_relaxed)), -40.0, 0.0));
                    target *= duckerLinear;
                }

                ch.targetGain = target;
                const float gainDb = static_cast<float>(20.0 * std::log10(std::max(ch.smoothedGain, 1.0e-5)));
                ch.currentGainDb.store(gainDb, std::memory_order_relaxed);
            } else {
                ch.targetGain = 1.0;
                ch.smoothedGain = 1.0;
                ch.currentGainDb.store(0.0f, std::memory_order_relaxed);
            }
        }
    }

private:
    void updateTimeConstants() noexcept {
        // Use per-channel configurable time constants (defaults: 5ms attack, 250ms release, 15ms smoothing).
        // The coefficients are shared across all channels since they are set once per sample rate change.
        for (const auto& ch : channels) {
            const double attackSec = ch.attackTimeMs * 0.001;
            const double releaseSec = ch.releaseTimeMs * 0.001;
            const double smoothingSec = ch.smoothingTimeMs * 0.001;
            attackCoeff = std::exp(-1.0 / (sampleRate * attackSec));
            releaseCoeff = std::exp(-1.0 / (sampleRate * releaseSec));
            gainSmoothingCoeff = std::exp(-1.0 / (sampleRate * smoothingSec));
            // Only set from first channel; all channels share the same coefficients
            break;
        }
    }

    double sampleRate = 48000.0;
    double attackCoeff = 0.99;
    double releaseCoeff = 0.999;
    double gainSmoothingCoeff = 0.998;

    std::array<ChannelParams, kMaxChannels> channels;
    std::array<GroupParams, kNumGroups> groups;
};

} // namespace nova::graviton
