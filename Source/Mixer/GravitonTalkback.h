#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace nova::graviton {

enum class TalkbackSwitchMode : int {
    AutoLatchMomentary = 0, // Tap (<300ms) to Latch toggle, Hold (>=300ms) for Momentary PTT
    LatchOnly = 1,          // Traditional latching toggle on every click
    MomentaryOnly = 2       // Push-to-Talk active strictly while held down
};

class TalkbackEngine final {
public:
    static constexpr size_t kMaxAuxBuses = 96;
    static constexpr size_t kMaxMatrixBuses = 24;

    TalkbackEngine() noexcept {
        reset();
    }

    void reset() noexcept {
        gainDb.store(0.0f, std::memory_order_relaxed);
        hpfHz.store(120.0f, std::memory_order_relaxed);
        hpfEnabled.store(true, std::memory_order_relaxed);
        gateThresholdDb.store(-45.0f, std::memory_order_relaxed);
        gateEnabled.store(true, std::memory_order_relaxed);
        duckerDb.store(-20.0f, std::memory_order_relaxed);
        switchMode.store(static_cast<int>(TalkbackSwitchMode::AutoLatchMomentary), std::memory_order_relaxed);
        isTalking.store(false, std::memory_order_relaxed);

        for (size_t a = 0; a < kMaxAuxBuses; ++a) {
            auxDestinations[a].store(a < 16, std::memory_order_relaxed); // Default first 16 Auxes (IEMs)
        }
        for (size_t m = 0; m < kMaxMatrixBuses; ++m) {
            matrixDestinations[m].store(false, std::memory_order_relaxed);
        }
        destSoloA.store(false, std::memory_order_relaxed);
        destSoloB.store(true, std::memory_order_relaxed); // Monitor engineer booth
        destControlRoom.store(false, std::memory_order_relaxed);
        destMainLR.store(false, std::memory_order_relaxed);

        duckerSmoothedGain = 1.0;
        gateSmoothedGain = 0.0;
        hpfState = 0.0;
        talkbackPeak.store(0.0f, std::memory_order_relaxed);
    }

    // --- Destination Control Helpers ---
    void setAllAux(bool enabled) noexcept {
        for (size_t a = 0; a < kMaxAuxBuses; ++a) {
            auxDestinations[a].store(enabled, std::memory_order_relaxed);
        }
    }

    void setAuxBank(int bankIndex, bool enabled) noexcept {
        const size_t start = static_cast<size_t>(std::clamp(bankIndex, 0, 5)) * 16;
        const size_t end = std::min(kMaxAuxBuses, start + 16);
        for (size_t a = start; a < end; ++a) {
            auxDestinations[a].store(enabled, std::memory_order_relaxed);
        }
    }

    void setAllMatrix(bool enabled) noexcept {
        for (size_t m = 0; m < kMaxMatrixBuses; ++m) {
            matrixDestinations[m].store(enabled, std::memory_order_relaxed);
        }
    }

    bool isAuxTarget(size_t auxIdx) const noexcept {
        return (auxIdx < kMaxAuxBuses) && auxDestinations[auxIdx].load(std::memory_order_relaxed);
    }

    bool isMatrixTarget(size_t matIdx) const noexcept {
        return (matIdx < kMaxMatrixBuses) && matrixDestinations[matIdx].load(std::memory_order_relaxed);
    }

    bool isSoloATarget() const noexcept { return destSoloA.load(std::memory_order_relaxed); }
    bool isSoloBTarget() const noexcept { return destSoloB.load(std::memory_order_relaxed); }
    bool isControlRoomTarget() const noexcept { return destControlRoom.load(std::memory_order_relaxed); }
    bool isMainLRTarget() const noexcept { return destMainLR.load(std::memory_order_relaxed); }

    bool isActive() const noexcept { return isTalking.load(std::memory_order_relaxed); }

    // --- Switch & Footswitch Logic (MIDAS Chapter 32) ---
    void onButtonPressed(uint32_t timestampMs) noexcept {
        pressTimestampMs = timestampMs;
        const auto mode = static_cast<TalkbackSwitchMode>(switchMode.load(std::memory_order_relaxed));
        if (mode == TalkbackSwitchMode::LatchOnly) {
            const bool current = isTalking.load(std::memory_order_relaxed);
            isTalking.store(!current, std::memory_order_relaxed);
        } else {
            // AutoLatch or Momentary: activate immediately on press
            isTalking.store(true, std::memory_order_relaxed);
        }
    }

    void onButtonReleased(uint32_t timestampMs) noexcept {
        const auto mode = static_cast<TalkbackSwitchMode>(switchMode.load(std::memory_order_relaxed));
        if (mode == TalkbackSwitchMode::MomentaryOnly) {
            isTalking.store(false, std::memory_order_relaxed);
        } else if (mode == TalkbackSwitchMode::AutoLatchMomentary) {
            const uint32_t duration = timestampMs - pressTimestampMs;
            if (duration >= 300) {
                // Held down for >= 300ms -> acted as momentary push-to-talk, deactivate on release
                isTalking.store(false, std::memory_order_relaxed);
            }
            // If < 300ms, keep talking (latched)
        }
    }

    void toggleLatched() noexcept {
        const bool current = isTalking.load(std::memory_order_relaxed);
        isTalking.store(!current, std::memory_order_relaxed);
    }

    // --- Real-time DSP Processing (Zero-Allocation, Lock-Free) ---
    double processSample(double micInputSample, double sampleRate, double& outDuckerGain) noexcept {
        const double sr = (sampleRate > 8000.0) ? sampleRate : 96000.0;
        const bool active = isTalking.load(std::memory_order_relaxed);

        // 1. Ducker Gain Envelope Smoothing
        // 5 ms attack (when talking begins), 200 ms release (when talking stops)
        const double targetDuckerDb = active ? std::clamp(static_cast<double>(duckerDb.load(std::memory_order_relaxed)), -60.0, 0.0) : 0.0;
        const double targetDuckerGain = dbToGain(targetDuckerDb);
        const double duckerTimeSec = active ? 0.005 : 0.200;
        const double duckerCoeff = std::exp(-1.0 / (sr * duckerTimeSec));
        duckerSmoothedGain = targetDuckerGain + duckerCoeff * (duckerSmoothedGain - targetDuckerGain);
        outDuckerGain = std::clamp(duckerSmoothedGain, 0.0, 1.0);

        if (!active && duckerSmoothedGain > 0.999) {
            talkbackPeak.store(0.0f, std::memory_order_relaxed);
            return 0.0;
        }

        // 2. Preamp Trim Gain
        const double trimLinear = dbToGain(std::clamp(static_cast<double>(gainDb.load(std::memory_order_relaxed)), -20.0, 40.0));
        double sig = micInputSample * trimLinear;

        // 3. High-Pass Filter (Swept 40 - 400 Hz 1st-order Butterworth)
        if (hpfEnabled.load(std::memory_order_relaxed)) {
            const double cutoff = std::clamp(static_cast<double>(hpfHz.load(std::memory_order_relaxed)), 40.0, 400.0);
            const double alpha = 1.0 / (1.0 + 2.0 * 3.14159265358979323846 * cutoff / sr);
            const double diff = sig - hpfState;
            sig = alpha * diff;
            hpfState += (1.0 - alpha) * diff;
        }

        // 4. Noise Gate / Expander (Attenuates background booth noise when engineer isn't speaking)
        if (gateEnabled.load(std::memory_order_relaxed)) {
            const double threshDb = static_cast<double>(gateThresholdDb.load(std::memory_order_relaxed));
            const double sigAbs = std::abs(sig);
            const double sigDb = (sigAbs > 1.0e-6) ? gainToDb(sigAbs) : -120.0;
            const double gateTarget = (sigDb >= threshDb) ? 1.0 : 0.0;
            const double gateCoeff = (gateTarget > gateSmoothedGain) ? std::exp(-1.0 / (sr * 0.002)) : std::exp(-1.0 / (sr * 0.080));
            gateSmoothedGain = gateTarget + gateCoeff * (gateSmoothedGain - gateTarget);
            sig *= gateSmoothedGain;
        }

        // 5. Peak Limiter (-1.0 dBFS ceiling with soft knee to protect stage musicians' ears)
        constexpr double limiterCeiling = 0.8912509381337456; // -1.0 dBFS
        if (std::abs(sig) > limiterCeiling) {
            sig = std::copysign(limiterCeiling + std::tanh(std::abs(sig) - limiterCeiling) * 0.08, sig);
        }

        const float peak = static_cast<float>(std::abs(sig));
        talkbackPeak.store(
            std::max(peak, talkbackPeak.load(std::memory_order_relaxed)),
            std::memory_order_relaxed);
        return sanitize(sig);
    }

    float getLivePeak() const noexcept {
        return talkbackPeak.load(std::memory_order_relaxed);
    }

    // --- Configuration Parameters ---
    std::atomic<float> gainDb{0.0f};
    std::atomic<float> hpfHz{120.0f};
    std::atomic<bool> hpfEnabled{true};
    std::atomic<float> gateThresholdDb{-45.0f};
    std::atomic<bool> gateEnabled{true};
    std::atomic<float> duckerDb{-20.0f};
    std::atomic<int> switchMode{static_cast<int>(TalkbackSwitchMode::AutoLatchMomentary)};
    std::atomic<bool> isTalking{false};

    std::array<std::atomic<bool>, kMaxAuxBuses> auxDestinations;
    std::array<std::atomic<bool>, kMaxMatrixBuses> matrixDestinations;
    std::atomic<bool> destSoloA{false};
    std::atomic<bool> destSoloB{true};
    std::atomic<bool> destControlRoom{false};
    std::atomic<bool> destMainLR{false};

private:
    uint32_t pressTimestampMs{0};
    double duckerSmoothedGain{1.0};
    double gateSmoothedGain{0.0};
    double hpfState{0.0};
    std::atomic<float> talkbackPeak{0.0f};
};

} // namespace nova::graviton
