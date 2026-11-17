#pragma once

#include "Effects/FlayrTuneEngine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nova::graviton {

// ==============================================================================
// OPENLIVE_REFACTORED — Vocal Pitch Correction Engine (Flayr Tune + Evo DSP)
// Powered by tune::Engine (FlayrTuneEngine.h):
//   - FFT-accelerated YIN pitch detector (4 input ranges)
//   - 13 musical scales + 12-note Removed Mask + Krumhansl-Schmuckler Key Learn
//   - TD-PSOLA resynthesis (formant-preserving) + Ultra-Low-Latency Live Mode
//   - Retune Speed, Humanize, Expression (Flex-Tune), Natural Vibrato,
//     Synthetic Vibrato, Transpose, Throat Length, Tracking, Mix
//   - Lock-free DisplayPoint ring for real-time scrolling pitch graph
// ==============================================================================
class VocalPitchCorrector final {
public:
    static constexpr int kNumScales = static_cast<int>(tune::kScaleNames.size()); // 13

    VocalPitchCorrector() noexcept {
        params.liveMode = true; // Default to low-latency live mode for live console
        params.formantPreserve = true;
        params.scale = 1; // Major
        params.inputType = 1; // Alto / Tenor
        params.retuneMs = 20.0f;
        params.humanize = 0.20f;
        params.mix = 1.0f;
        engine.prepare(sampleRate, 2);
        engine.setParams(params);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && sr != sampleRate) {
            sampleRate = sr;
            engine.prepare(sr, 2);
            engine.setParams(params);
        }
    }

    void reset() noexcept {
        engine.reset();
    }

    // Core Pitch Correction Parameters
    void setKey(int k) noexcept { params.key = std::clamp(k, 0, 11); paramsDirty.store(true, std::memory_order_relaxed); }
    int getKey() const noexcept { return params.key; }

    void setScale(int s) noexcept { params.scale = std::clamp(s, 0, kNumScales - 1); paramsDirty.store(true, std::memory_order_relaxed); }
    int getScale() const noexcept { return params.scale; }

    void setInputType(int t) noexcept { params.inputType = std::clamp(t, 0, 3); paramsDirty.store(true, std::memory_order_relaxed); }
    int getInputType() const noexcept { return params.inputType; }

    void setRetuneSpeedMs(double ms) noexcept { params.retuneMs = static_cast<float>(std::clamp(ms, 0.0, 400.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getRetuneSpeedMs() const noexcept { return params.retuneMs; }

    void setHumanizePct(double pct) noexcept { params.humanize = static_cast<float>(std::clamp(pct, 0.0, 100.0) * 0.01); paramsDirty.store(true, std::memory_order_relaxed); }
    double getHumanizePct() const noexcept { return params.humanize * 100.0; }

    void setExpressionPct(double pct) noexcept { params.expression = static_cast<float>(std::clamp(pct, 0.0, 100.0) * 0.01); paramsDirty.store(true, std::memory_order_relaxed); }
    double getExpressionPct() const noexcept { return params.expression * 100.0; }

    void setNaturalVibratoDb(double db) noexcept { params.naturalVibratoDb = static_cast<float>(std::clamp(db, -12.0, 12.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getNaturalVibratoDb() const noexcept { return params.naturalVibratoDb; }

    void setDetuneCents(double cents) noexcept {
        detuneCents = std::clamp(cents, -50.0, 50.0);
        params.concertA = static_cast<float>(440.0 * std::pow(2.0, detuneCents / 1200.0));
        paramsDirty.store(true, std::memory_order_relaxed);
    }
    double getDetuneCents() const noexcept { return detuneCents; }

    void setTranspose(double st) noexcept { params.transpose = static_cast<float>(std::clamp(st, -12.0, 12.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getTranspose() const noexcept { return params.transpose; }

    void setFormantPreserve(bool enabled) noexcept { params.formantPreserve = enabled; paramsDirty.store(true, std::memory_order_relaxed); }
    bool getFormantPreserve() const noexcept { return params.formantPreserve; }

    void setThroatLengthPct(double pct) noexcept {
        throatPct = std::clamp(pct, -50.0, 50.0);
        params.throat = static_cast<float>(std::pow(2.0, throatPct / 100.0));
        paramsDirty.store(true, std::memory_order_relaxed);
    }
    double getThroatLengthPct() const noexcept { return throatPct; }

    void setTrackingPct(double pct) noexcept { params.tracking = static_cast<float>(std::clamp(pct, 0.0, 100.0) * 0.01); paramsDirty.store(true, std::memory_order_relaxed); }
    double getTrackingPct() const noexcept { return params.tracking * 100.0; }

    void setVibRateHz(double hz) noexcept { params.vibRateHz = static_cast<float>(std::clamp(hz, 0.5, 12.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getVibRateHz() const noexcept { return params.vibRateHz; }

    void setVibDepthCents(double cents) noexcept { params.vibDepthCents = static_cast<float>(std::clamp(cents, 0.0, 150.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getVibDepthCents() const noexcept { return params.vibDepthCents; }

    void setVibDelayMs(double ms) noexcept { params.vibDelayMs = static_cast<float>(std::clamp(ms, 0.0, 2000.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getVibDelayMs() const noexcept { return params.vibDelayMs; }

    void setLiveMode(bool live) noexcept { params.liveMode = live; paramsDirty.store(true, std::memory_order_relaxed); }
    bool getLiveMode() const noexcept { return params.liveMode; }

    void setMix(double m) noexcept { params.mix = static_cast<float>(std::clamp(m, 0.0, 1.0)); paramsDirty.store(true, std::memory_order_relaxed); }
    double getMix() const noexcept { return params.mix; }

    void setRemovedMask(uint16_t mask) noexcept { params.removedMask = mask & 0x0FFFu; paramsDirty.store(true, std::memory_order_relaxed); }
    uint16_t getRemovedMask() const noexcept { return params.removedMask; }
    void toggleRemovedPitchClass(int pc) noexcept {
        if (pc >= 0 && pc < 12) {
            params.removedMask ^= static_cast<uint16_t>(1u << pc);
            paramsDirty.store(true, std::memory_order_relaxed);
        }
    }

    // Auto Key Learning (Krumhansl-Schmuckler)
    void beginKeyLearn() noexcept { engine.beginKeyLearn(); }
    void endKeyLearn() noexcept { engine.endKeyLearn(); }
    bool isLearningKey() const noexcept { return engine.isLearningKey(); }
    bool applyLearnedKey() noexcept {
        const auto hist = engine.getKeyHistogram();
        int k = 0;
        bool minor = false;
        if (tune::estimateKey(hist.data(), k, minor)) {
            setKey(k);
            setScale(minor ? 2 : 1); // Natural Minor or Major
            return true;
        }
        return false;
    }

    // Display & Telemetry
    bool popDisplayPoint(tune::DisplayPoint& pt) noexcept { return engine.popDisplay(pt); }

    float getDetectedMidi() const noexcept { return engine.getLastInNote(); }
    float getTargetMidi() const noexcept {
        const float t = engine.getLastTargetNote();
        return (t >= 0.0f) ? t : engine.getLastOutNote();
    }
    float getCorrectionCents() const noexcept {
        if (!engine.isLastVoiced()) return 0.0f;
        const float tgt = getTargetMidi();
        return (tgt - engine.getLastInNote()) * 100.0f;
    }
    float getVoicedConfidence() const noexcept { return engine.isLastVoiced() ? 1.0f : 0.0f; }
    int getLatencySamples() const noexcept { return engine.getLatencySamples(params.liveMode); }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        if (paramsDirty.exchange(false, std::memory_order_relaxed)) {
            engine.setParams(params);
        }
        float fL = std::isfinite(inOutL) ? static_cast<float>(inOutL) : 0.0f;
        float fR = std::isfinite(inOutR) ? static_cast<float>(inOutR) : 0.0f;
        float* chans[2] = { &fL, &fR };
        engine.process(chans, 2, 1);
        inOutL = std::isfinite(fL) ? static_cast<double>(fL) : 0.0;
        inOutR = std::isfinite(fR) ? static_cast<double>(fR) : 0.0;
    }

private:
    tune::Engine engine;
    tune::Params params;
    std::atomic<bool> paramsDirty{ false };
    double sampleRate = 48000.0;
    double detuneCents = 0.0;
    double throatPct = 0.0;
};

} // namespace nova::graviton
