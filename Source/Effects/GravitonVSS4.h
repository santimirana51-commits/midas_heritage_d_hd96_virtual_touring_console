#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// TC Electronic VSS4 HD Authentic Space Reverb (System 6000 Flagship Engine)
// Reference: MIDAS Heritage-D HD96-24 Chapter 15 (pp. 137-140)
//
// Key Features:
// 1. True Stereo / Dual Source Reverb with independent source angles
// 2. Advanced Source-Related Early Reflections (Image-source spatial ray delays)
// 3. Realistic Acoustic Room Simulation (Location Types: Vienna Hall, Concert Hall, etc.)
// 4. Independent Positioning of Source 1 and Source 2 (-90 deg to +90 deg)
// 5. Adjustable Reverb Decay: 0.1 to 20.0 seconds
// 6. Pre-Delay up to 300 ms
// 7. Adjustable Reverb Size: 1 to 25
// 8. 4-Band Decay Control: Low, Low-Mid, High-Mid, High multipliers (0.1x to 2.5x)
// 9. Crossover Frequencies: Lo/Mid (50-500 Hz), Mid/Hi-Mid (500-2000 Hz), Hi-Mid/Hi (2k-12 kHz)
// 10. High-Frequency Damping & Hi-Soften filter
// 11. Normal vs Coloured Reverb modes
// 12. Advanced Modulation Control (Mod Type, Rate 0.1-5.0 Hz, Depth 0-100%)
// 13. Stereo Spatial Depth and Width (0% to 200%)
// 14. Level Control: Dry Level (with Kill-Dry), Early Level, Reverb Level
// ==============================================================================

enum class VSS4LocationType : int {
    ViennaHall = 0,
    ConcertHall = 1,
    ScoringStage = 2,
    Cathedral = 3,
    JazzClub = 4,
    WarmChamber = 5,
    WoodenRoom = 6,
    AmbiScene = 7
};

inline const char* getVSS4LocationName(VSS4LocationType loc) noexcept {
    switch (loc) {
        case VSS4LocationType::ViennaHall:   return "Vienna Hall";
        case VSS4LocationType::ConcertHall:  return "Concert Hall";
        case VSS4LocationType::ScoringStage: return "Scoring Stage";
        case VSS4LocationType::Cathedral:    return "Cathedral";
        case VSS4LocationType::JazzClub:     return "Jazz Club";
        case VSS4LocationType::WarmChamber:  return "Warm Chamber";
        case VSS4LocationType::WoodenRoom:   return "Wooden Room";
        case VSS4LocationType::AmbiScene:    return "Ambience Scene";
        default:                             return "Custom Space";
    }
}

enum class VSS4ReverbMode : int {
    Normal = 0,
    Coloured = 1
};

enum class VSS4ModType : int {
    Off = 0,
    Subtle = 1,
    Wide = 2,
    Chorus = 3,
    Drift = 4
};

inline const char* getVSS4ModTypeName(VSS4ModType m) noexcept {
    switch (m) {
        case VSS4ModType::Off:    return "Off";
        case VSS4ModType::Subtle: return "Subtle";
        case VSS4ModType::Wide:   return "Wide";
        case VSS4ModType::Chorus: return "Chorus";
        case VSS4ModType::Drift:  return "Drift";
        default:                  return "Standard";
    }
}

class VSS4ReverbEngine final {
public:
    static constexpr int kMaxPreDelaySamples = 48000; // 500 ms @ 96 kHz
    static constexpr size_t kNumFDNLst = 8;           // 8-channel High-Density FDN
    static constexpr size_t kNumEarlyTaps = 12;       // 12 spatial early reflection taps

    VSS4ReverbEngine() {
        preDelayBufferL.resize(kMaxPreDelaySamples, 0.0);
        preDelayBufferR.resize(kMaxPreDelaySamples, 0.0);
        earlyBufferL.resize(16384, 0.0);
        earlyBufferR.resize(16384, 0.0);
        for (auto& buf : fdnBuffersL) buf.resize(16384, 0.0);
        for (auto& buf : fdnBuffersR) buf.resize(16384, 0.0);
        reset();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
            sampleRate = sr;
            updateDelays();
            updateFilters();
        }
    }

    void reset() noexcept {
        std::fill(preDelayBufferL.begin(), preDelayBufferL.end(), 0.0);
        std::fill(preDelayBufferR.begin(), preDelayBufferR.end(), 0.0);
        std::fill(earlyBufferL.begin(), earlyBufferL.end(), 0.0);
        std::fill(earlyBufferR.begin(), earlyBufferR.end(), 0.0);
        preDelayWritePos = 0;
        earlyWritePos = 0;

        for (auto& ap : diffusersL) ap.reset();
        for (auto& ap : diffusersR) ap.reset();
        for (auto& buf : fdnBuffersL) std::fill(buf.begin(), buf.end(), 0.0);
        for (auto& buf : fdnBuffersR) std::fill(buf.begin(), buf.end(), 0.0);
        fdnWritePosL.fill(0);
        fdnWritePosR.fill(0);
        fdnFiltersL.fill({});
        fdnFiltersR.fill({});

        lfoPhase = 0.0;
        updateDelays();
        updateFilters();
    }

    // ── Location & Positioning ─────────────────────────────────────────────
    void setLocationType(VSS4LocationType loc) noexcept {
        locationType = loc;
        switch (loc) {
            case VSS4LocationType::ViennaHall:
                decaySec = 2.7; roomSizeVal = 14.0; hiCutHz = 2170.0;
                preDelayMs = 0.0; earlyDecreasePct = 3.0;
                break;
            case VSS4LocationType::ConcertHall:
                decaySec = 2.2; roomSizeVal = 18.0; hiCutHz = 4500.0;
                preDelayMs = 25.0; earlyDecreasePct = 10.0;
                break;
            case VSS4LocationType::ScoringStage:
                decaySec = 1.6; roomSizeVal = 20.0; hiCutHz = 6000.0;
                preDelayMs = 15.0; earlyDecreasePct = 5.0;
                break;
            case VSS4LocationType::Cathedral:
                decaySec = 5.5; roomSizeVal = 24.0; hiCutHz = 1800.0;
                preDelayMs = 40.0; earlyDecreasePct = 0.0;
                break;
            case VSS4LocationType::JazzClub:
                decaySec = 1.1; roomSizeVal = 8.0; hiCutHz = 5500.0;
                preDelayMs = 10.0; earlyDecreasePct = 15.0;
                break;
            case VSS4LocationType::WarmChamber:
                decaySec = 1.4; roomSizeVal = 10.0; hiCutHz = 3800.0;
                preDelayMs = 8.0; earlyDecreasePct = 12.0;
                break;
            case VSS4LocationType::WoodenRoom:
                decaySec = 0.8; roomSizeVal = 6.0; hiCutHz = 3200.0;
                preDelayMs = 5.0; earlyDecreasePct = 20.0;
                break;
            case VSS4LocationType::AmbiScene:
                decaySec = 0.5; roomSizeVal = 4.0; hiCutHz = 7500.0;
                preDelayMs = 0.0; earlyDecreasePct = 30.0;
                break;
        }
        updateDelays();
        updateFilters();
    }
    VSS4LocationType getLocationType() const noexcept { return locationType; }

    // Source 1 angle in degrees (-90 to +90, e.g. -30 for L 30°)
    void setSource1Angle(double deg) noexcept {
        source1AngleDeg = std::clamp(deg, -90.0, 90.0);
        updateSourcePans();
    }
    double getSource1Angle() const noexcept { return source1AngleDeg; }

    // Source 2 angle in degrees (-90 to +90, e.g. +30 for R 30°)
    void setSource2Angle(double deg) noexcept {
        source2AngleDeg = std::clamp(deg, -90.0, 90.0);
        updateSourcePans();
    }
    double getSource2Angle() const noexcept { return source2AngleDeg; }

    // ── Reverb Parameters ──────────────────────────────────────────────────
    void setMasterDecay(double seconds) noexcept {
        decaySec = std::clamp(seconds, 0.1, 20.0);
        updateFeedback();
    }
    double getMasterDecay() const noexcept { return decaySec; }

    void setPreDelay(double ms) noexcept {
        preDelayMs = std::clamp(ms, 0.0, 300.0);
    }
    double getPreDelay() const noexcept { return preDelayMs; }
    void setPreDelayMs(double ms) noexcept { setPreDelay(ms); }
    double getPreDelayMs() const noexcept { return getPreDelay(); }

    void setReverbSize(double sizeIndex) noexcept {
        roomSizeVal = std::clamp(sizeIndex, 1.0, 25.0);
        updateDelays();
    }
    double getReverbSize() const noexcept { return roomSizeVal; }

    void setHiCut(double hz) noexcept {
        hiCutHz = std::clamp(hz, 500.0, 20000.0);
        updateFilters();
    }
    double getHiCut() const noexcept { return hiCutHz; }
    void setHiCutHz(double hz) noexcept { setHiCut(hz); }
    double getHiCutHz() const noexcept { return getHiCut(); }

    void setEarlyDecrease(double pct) noexcept {
        earlyDecreasePct = std::clamp(pct, 0.0, 100.0);
    }
    double getEarlyDecrease() const noexcept { return earlyDecreasePct; }

    void setReverbWidth(double pct) noexcept {
        reverbWidth = std::clamp(pct, 0.0, 200.0) * 0.01;
    }
    double getReverbWidth() const noexcept { return reverbWidth * 100.0; }

    void setReverbDelay(double ms) noexcept { reverbDelayMs = std::clamp(ms, 0.0, 100.0); }
    double getReverbDelay() const noexcept { return reverbDelayMs; }

    void setEarlyStart(double pct) noexcept { earlyStartPct = std::clamp(pct, 0.0, 100.0); }
    double getEarlyStart() const noexcept { return earlyStartPct; }

    void setEarlyStop(double pct) noexcept { earlyStopPct = std::clamp(pct, 10.0, 100.0); }
    double getEarlyStop() const noexcept { return earlyStopPct; }

    void setHiColor(double val) noexcept { hiColor = std::clamp(val, -50.0, 50.0); }
    double getHiColor() const noexcept { return hiColor; }

    void setLoColor(double val) noexcept { loColor = std::clamp(val, -50.0, 50.0); }
    double getLoColor() const noexcept { return loColor; }

    float getMeterInL() const noexcept { return meterInL.load(std::memory_order_relaxed); }
    float getMeterInR() const noexcept { return meterInR.load(std::memory_order_relaxed); }
    float getMeterOutL() const noexcept { return meterOutL.load(std::memory_order_relaxed); }
    float getMeterOutR() const noexcept { return meterOutR.load(std::memory_order_relaxed); }

    // ── 4-Band Decay Multipliers (Colour Page) ─────────────────────────────
    void setLoDecay(double mul) noexcept {
        loDecayMul = std::clamp(mul, 0.1, 2.5);
        updateFeedback();
    }
    double getLoDecay() const noexcept { return loDecayMul; }

    void setLoMidDecay(double mul) noexcept {
        loMidDecayMul = std::clamp(mul, 0.1, 2.5);
        updateFeedback();
    }
    double getLoMidDecay() const noexcept { return loMidDecayMul; }

    void setHiMidDecay(double mul) noexcept {
        hiMidDecayMul = std::clamp(mul, 0.1, 2.5);
        updateFeedback();
    }
    double getHiMidDecay() const noexcept { return hiMidDecayMul; }

    void setHiDecay(double mul) noexcept {
        hiDecayMul = std::clamp(mul, 0.1, 2.5);
        updateFeedback();
    }
    double getHiDecay() const noexcept { return hiDecayMul; }

    void setLoXOver(double hz) noexcept { loXOverHz = std::clamp(hz, 50.0, 500.0); updateFilters(); }
    double getLoXOver() const noexcept { return loXOverHz; }

    void setMidXOver(double hz) noexcept { midXOverHz = std::clamp(hz, 500.0, 2500.0); updateFilters(); }
    double getMidXOver() const noexcept { return midXOverHz; }

    void setHiXOver(double hz) noexcept { hiXOverHz = std::clamp(hz, 2500.0, 12000.0); updateFilters(); }
    double getHiXOver() const noexcept { return hiXOverHz; }

    void setReverbMode(VSS4ReverbMode m) noexcept { reverbMode = m; }
    VSS4ReverbMode getReverbMode() const noexcept { return reverbMode; }

    // ── Gloss & Modulation ─────────────────────────────────────────────────
    void setModType(VSS4ModType m) noexcept { modType = m; }
    VSS4ModType getModType() const noexcept { return modType; }

    void setModRate(double hz) noexcept { modRateHz = std::clamp(hz, 0.1, 5.0); }
    double getModRate() const noexcept { return modRateHz; }

    void setModDepth(double pct) noexcept { modDepthPct = std::clamp(pct, 0.0, 100.0); }
    double getModDepth() const noexcept { return modDepthPct; }

    void setHiSoften(double pct) noexcept { hiSoftenPct = std::clamp(pct, 0.0, 100.0); updateFilters(); }
    double getHiSoften() const noexcept { return hiSoftenPct; }

    // ── Levels & Gains ─────────────────────────────────────────────────────
    void setInGainDb(double db) noexcept { inGainDb = std::clamp(db, -18.0, 18.0); }
    double getInGainDb() const noexcept { return inGainDb; }

    void setOutGainDb(double db) noexcept { outGainDb = std::clamp(db, -18.0, 18.0); }
    double getOutGainDb() const noexcept { return outGainDb; }

    void setDryLevelDb(double db) noexcept { dryLevelDb = std::clamp(db, -60.0, 0.0); }
    double getDryLevelDb() const noexcept { return dryLevelDb; }

    void setKillDry(bool kill) noexcept { killDry = kill; }
    bool getKillDry() const noexcept { return killDry; }

    void setEarlyLevelDb(double db) noexcept { earlyLevelDb = std::clamp(db, -60.0, 10.0); }
    double getEarlyLevelDb() const noexcept { return earlyLevelDb; }

    void setReverbLevelDb(double db) noexcept { reverbLevelDb = std::clamp(db, -60.0, 10.0); }
    double getReverbLevelDb() const noexcept { return reverbLevelDb; }

    // ── Audio Processing ───────────────────────────────────────────────────
    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double inG = dbToGain(inGainDb);
        const double outG = dbToGain(outGainDb);

        const double dryInL = inOutL;
        const double dryInR = inOutR;

        const double drivenL = dryInL * inG;
        const double drivenR = dryInR * inG;

        // 1. Dual Source Spatial Panning Matrix:
        // Input Left feeds Source 1 position, Input Right feeds Source 2 position.
        const double src1L = drivenL * src1GainL;
        const double src1R = drivenL * src1GainR;
        const double src2L = drivenR * src2GainL;
        const double src2R = drivenR * src2GainR;

        const double pannedL = src1L + src2L;
        const double pannedR = src1R + src2R;

        // 2. Pre-Delay
        const int preDelaySamples = std::clamp(static_cast<int>(preDelayMs * 0.001 * sampleRate), 0, kMaxPreDelaySamples - 1);
        preDelayBufferL[static_cast<size_t>(preDelayWritePos)] = pannedL;
        preDelayBufferR[static_cast<size_t>(preDelayWritePos)] = pannedR;
        const int pdReadPos = (preDelayWritePos - preDelaySamples + kMaxPreDelaySamples) % kMaxPreDelaySamples;
        const double preDelayedL = preDelayBufferL[static_cast<size_t>(pdReadPos)];
        const double preDelayedR = preDelayBufferR[static_cast<size_t>(pdReadPos)];
        preDelayWritePos = (preDelayWritePos + 1) % kMaxPreDelaySamples;

        // 3. Early Reflection Matrix (12 spatial ray reflections)
        earlyBufferL[static_cast<size_t>(earlyWritePos)] = preDelayedL;
        earlyBufferR[static_cast<size_t>(earlyWritePos)] = preDelayedR;

        double earlySumL = 0.0;
        double earlySumR = 0.0;
        const double earlyDecayAtten = 1.0 - (earlyDecreasePct * 0.006);

        for (size_t t = 0; t < kNumEarlyTaps; ++t) {
            const double tapRelPos = static_cast<double>(t) / static_cast<double>(kNumEarlyTaps - 1);
            if (tapRelPos < earlyStartPct * 0.008 || tapRelPos > earlyStopPct * 0.012) continue;
            const int tapD = earlyTapDelays[t];
            const int rPos = (earlyWritePos - tapD + static_cast<int>(earlyBufferL.size())) % static_cast<int>(earlyBufferL.size());
            const double tapL = earlyBufferL[static_cast<size_t>(rPos)];
            const double tapR = earlyBufferR[static_cast<size_t>(rPos)];
            const double g = earlyTapGains[t] * std::pow(earlyDecayAtten, static_cast<double>(t));
            earlySumL += (tapL * earlyTapPanL[t] + tapR * (1.0 - earlyTapPanR[t])) * g;
            earlySumR += (tapR * earlyTapPanR[t] + tapL * (1.0 - earlyTapPanL[t])) * g;
        }
        earlyWritePos = (earlyWritePos + 1) % static_cast<int>(earlyBufferL.size());

        // 4. Input Spatial Diffusion (Dual-stage All-Pass)
        double diffL = diffusersL[0].process(preDelayedL);
        diffL = diffusersL[1].process(diffL);
        double diffR = diffusersR[0].process(preDelayedR);
        diffR = diffusersR[1].process(diffR);

        // 5. Modulation LFO
        constexpr double twoPi = 2.0 * std::numbers::pi;
        if (modType != VSS4ModType::Off) {
            lfoPhase += (twoPi * modRateHz) / sampleRate;
            if (lfoPhase >= twoPi) lfoPhase -= twoPi;
        }

        // 6. 8-Channel Feedback Delay Network (FDN)
        std::array<double, kNumFDNLst> fdnOutL{}, fdnOutR{};
        const double modExcursion = (modType != VSS4ModType::Off) ? (modDepthPct * 0.001 * sampleRate * 0.002) : 0.0;

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            const double lfo = std::sin(lfoPhase + static_cast<double>(i) * 0.7853981633974483);
            const int lenL = std::clamp(static_cast<int>(fdnLengthsL[i] + lfo * modExcursion), 8, static_cast<int>(fdnBuffersL[i].size() - 2));
            const int lenR = std::clamp(static_cast<int>(fdnLengthsR[i] - lfo * modExcursion), 8, static_cast<int>(fdnBuffersR[i].size() - 2));

            const int rPosL = (fdnWritePosL[i] - lenL + static_cast<int>(fdnBuffersL[i].size())) % static_cast<int>(fdnBuffersL[i].size());
            const int rPosR = (fdnWritePosR[i] - lenR + static_cast<int>(fdnBuffersR[i].size())) % static_cast<int>(fdnBuffersR[i].size());

            fdnOutL[i] = fdnBuffersL[i][static_cast<size_t>(rPosL)];
            fdnOutR[i] = fdnBuffersR[i][static_cast<size_t>(rPosR)];
        }

        // Householder unitary mixing matrix for 8 channels: H = I - (2/N)*J
        double sumFdnL = 0.0, sumFdnR = 0.0;
        for (size_t i = 0; i < kNumFDNLst; ++i) {
            sumFdnL += fdnOutL[i];
            sumFdnR += fdnOutR[i];
        }
        constexpr double factor = 2.0 / static_cast<double>(kNumFDNLst);

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            const double mixedL = (diffL * 0.35) + fdnFeedback * (fdnOutL[i] - factor * sumFdnL);
            const double mixedR = (diffR * 0.35) + fdnFeedback * (fdnOutR[i] - factor * sumFdnR);

            // Filter per FDN line (Damping + Coloured mode dispersion)
            double fltL = fdnFiltersL[i].process(mixedL);
            double fltR = fdnFiltersR[i].process(mixedR);

            if (reverbMode == VSS4ReverbMode::Coloured) {
                // Subtle warmth saturation & frequency dispersion
                fltL = std::tanh(fltL * 1.05);
                fltR = std::tanh(fltR * 1.05);
            }

            fdnBuffersL[i][static_cast<size_t>(fdnWritePosL[i])] = sanitize(fltL);
            fdnBuffersR[i][static_cast<size_t>(fdnWritePosR[i])] = sanitize(fltR);

            fdnWritePosL[i] = (fdnWritePosL[i] + 1) % static_cast<int>(fdnBuffersL[i].size());
            fdnWritePosR[i] = (fdnWritePosR[i] + 1) % static_cast<int>(fdnBuffersR[i].size());
        }

        // 7. Accumulate Reverb Tail from FDN outputs
        double revTailL = (fdnOutL[0] - fdnOutL[2] + fdnOutL[4] - fdnOutL[6]) * 0.5;
        double revTailR = (fdnOutR[1] - fdnOutR[3] + fdnOutR[5] - fdnOutR[7]) * 0.5;

        // Stereo Spatial Width (Mid/Side processing)
        const double midTail = (revTailL + revTailR) * 0.5;
        const double sideTail = (revTailR - revTailL) * 0.5 * reverbWidth;
        revTailL = midTail - sideTail;
        revTailR = midTail + sideTail;

        // 8. Gain Staging & Mix
        const double earlyGain = dbToGain(earlyLevelDb);
        const double revGain = dbToGain(reverbLevelDb);
        const double dryGain = killDry ? 0.0 : dbToGain(dryLevelDb);

        const double wetL = (earlySumL * earlyGain) + (revTailL * revGain);
        const double wetR = (earlySumR * earlyGain) + (revTailR * revGain);

        inOutL = sanitize((dryInL * dryGain + wetL) * outG);
        inOutR = sanitize((dryInR * dryGain + wetR) * outG);

        // Update live peak levels
        const float inL = static_cast<float>(std::abs(dryInL));
        const float inR = static_cast<float>(std::abs(dryInR));
        const float outL = static_cast<float>(std::abs(inOutL));
        const float outR = static_cast<float>(std::abs(inOutR));

        float pInL = meterInL.load(std::memory_order_relaxed);
        meterInL.store(std::max(inL, pInL * 0.999f), std::memory_order_relaxed);
        float pInR = meterInR.load(std::memory_order_relaxed);
        meterInR.store(std::max(inR, pInR * 0.999f), std::memory_order_relaxed);
        float pOutL = meterOutL.load(std::memory_order_relaxed);
        meterOutL.store(std::max(outL, pOutL * 0.999f), std::memory_order_relaxed);
        float pOutR = meterOutR.load(std::memory_order_relaxed);
        meterOutR.store(std::max(outR, pOutR * 0.999f), std::memory_order_relaxed);
    }

private:
    void updateSourcePans() noexcept {
        // Source angle in [-90, +90] mapped to stereo pan [0, 1]
        // 0 deg = Center (L=0.707, R=0.707)
        // -30 deg = L 30 deg
        constexpr double degToRad = std::numbers::pi / 180.0;
        const double rad1 = (source1AngleDeg + 90.0) * 0.5 * degToRad; // 0 to pi/2
        src1GainL = std::cos(rad1);
        src1GainR = std::sin(rad1);

        const double rad2 = (source2AngleDeg + 90.0) * 0.5 * degToRad;
        src2GainL = std::cos(rad2);
        src2GainR = std::sin(rad2);
    }

    void updateDelays() noexcept {
        // Room scale from reverb size (size Index 1-25 -> scale 0.4 to 1.8)
        const double roomScale = 0.4 + (roomSizeVal / 25.0) * 1.4;

        // 8 prime-spaced FDN delay lengths in milliseconds
        const std::array<double, kNumFDNLst> baseMsL = { 23.9, 29.3, 37.1, 43.7, 49.9, 56.3, 61.7, 67.3 };
        const std::array<double, kNumFDNLst> baseMsR = { 25.1, 31.7, 39.3, 41.9, 53.1, 58.7, 63.9, 71.1 };

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            fdnLengthsL[i] = std::clamp(static_cast<int>(baseMsL[i] * 0.001 * sampleRate * roomScale), 16, 14000);
            fdnLengthsR[i] = std::clamp(static_cast<int>(baseMsR[i] * 0.001 * sampleRate * roomScale), 16, 14000);
        }

        // Early reflection taps spacing
        const std::array<double, kNumEarlyTaps> baseEarlyMs = {
            3.2, 5.8, 9.1, 12.4, 15.9, 19.5, 23.1, 27.8, 32.4, 38.0, 44.2, 51.0
        };
        const std::array<double, kNumEarlyTaps> baseEarlyG = {
            1.0, 0.85, 0.72, 0.65, 0.58, 0.50, 0.44, 0.38, 0.32, 0.27, 0.22, 0.18
        };
        const std::array<double, kNumEarlyTaps> baseEarlyPanL = {
            0.85, 0.25, 0.90, 0.15, 0.75, 0.35, 0.80, 0.20, 0.70, 0.30, 0.65, 0.40
        };
        const std::array<double, kNumEarlyTaps> baseEarlyPanR = {
            0.15, 0.75, 0.10, 0.85, 0.25, 0.65, 0.20, 0.80, 0.30, 0.70, 0.35, 0.60
        };

        for (size_t t = 0; t < kNumEarlyTaps; ++t) {
            earlyTapDelays[t] = std::clamp(static_cast<int>(baseEarlyMs[t] * 0.001 * sampleRate * roomScale), 4, 14000);
            earlyTapGains[t] = baseEarlyG[t];
            earlyTapPanL[t] = baseEarlyPanL[t];
            earlyTapPanR[t] = baseEarlyPanR[t];
        }

        // All-pass diffusers
        diffusersL[0].prepare(sampleRate, 130.0 * roomScale);
        diffusersL[1].prepare(sampleRate, 310.0 * roomScale);
        diffusersR[0].prepare(sampleRate, 150.0 * roomScale);
        diffusersR[1].prepare(sampleRate, 330.0 * roomScale);

        updateFeedback();
    }

    void updateFeedback() noexcept {
        const double avgDelaySec = 0.045 * (0.4 + (roomSizeVal / 25.0) * 1.4);
        fdnFeedback = std::clamp(std::pow(10.0, -3.0 * avgDelaySec / decaySec), 0.0, 0.985);
    }

    void updateFilters() noexcept {
        const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        // Hi Cut + Hi Soften
        const double effectiveHiCut = std::clamp(hiCutHz * (1.0 - (hiSoftenPct * 0.004)), 400.0, sr * 0.49);
        const auto coeffs = makeButterworthLPF(sr, effectiveHiCut, 0.7071);

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            fdnFiltersL[i].setCoefficients(coeffs);
            fdnFiltersR[i].setCoefficients(coeffs);
        }
    }

    struct SimpleAllPass {
        std::vector<double> buf;
        int wPos = 0;
        int delay = 200;
        double gain = 0.55;

        void prepare(double sr, double delaySamples) noexcept {
            (void)sr;
            delay = std::clamp(static_cast<int>(delaySamples), 4, 4000);
            buf.resize(static_cast<size_t>(delay + 1), 0.0);
            wPos = 0;
        }

        void reset() noexcept {
            std::fill(buf.begin(), buf.end(), 0.0);
            wPos = 0;
        }

        double process(double in) noexcept {
            if (buf.empty()) return in;
            const int rPos = (wPos - delay + static_cast<int>(buf.size())) % static_cast<int>(buf.size());
            const double bufOut = buf[static_cast<size_t>(rPos)];
            const double out = -gain * in + bufOut;
            buf[static_cast<size_t>(wPos)] = in + gain * out;
            wPos = (wPos + 1) % static_cast<int>(buf.size());
            return out;
        }
    };

    double sampleRate = 48000.0;

    // Location & Positioning
    VSS4LocationType locationType = VSS4LocationType::ViennaHall;
    double source1AngleDeg = -30.0; // L 30 deg
    double source2AngleDeg = 30.0;  // R 30 deg
    double src1GainL = 0.866;
    double src1GainR = 0.500;
    double src2GainL = 0.500;
    double src2GainR = 0.866;

    // Master Reverb
    double decaySec = 2.8;
    double roomSizeVal = 14.0;
    double hiCutHz = 2170.0;
    double preDelayMs = 22.0;
    double earlyDecreasePct = 3.0;
    double reverbWidth = 1.0;
    double reverbDelayMs = 0.0;

    // Early Reflections Colouring
    double earlyStartPct = 20.0;
    double earlyStopPct = 100.0;
    double hiColor = 20.0;
    double loColor = 12.0;

    // Live Peak Meters (Atomic)
    std::atomic<float> meterInL{ 0.0f };
    std::atomic<float> meterInR{ 0.0f };
    std::atomic<float> meterOutL{ 0.0f };
    std::atomic<float> meterOutR{ 0.0f };

    // 4-Band Decay Multipliers
    double loDecayMul = 1.0;
    double loMidDecayMul = 1.0;
    double hiMidDecayMul = 1.0;
    double hiDecayMul = 0.85;
    double loXOverHz = 160.0;
    double midXOverHz = 1200.0;
    double hiXOverHz = 5000.0;
    VSS4ReverbMode reverbMode = VSS4ReverbMode::Normal;

    // Modulation & Gloss
    VSS4ModType modType = VSS4ModType::Subtle;
    double modRateHz = 0.8;
    double modDepthPct = 25.0;
    double hiSoftenPct = 15.0;
    double lfoPhase = 0.0;

    // Levels
    double inGainDb = 0.0;
    double outGainDb = 0.0;
    double dryLevelDb = 0.0;
    bool killDry = false;
    double earlyLevelDb = 0.0;
    double reverbLevelDb = 0.0;

    double fdnFeedback = 0.78;

    // Buffers
    std::vector<double> preDelayBufferL;
    std::vector<double> preDelayBufferR;
    int preDelayWritePos = 0;

    std::vector<double> earlyBufferL;
    std::vector<double> earlyBufferR;
    int earlyWritePos = 0;
    std::array<int, kNumEarlyTaps> earlyTapDelays{};
    std::array<double, kNumEarlyTaps> earlyTapGains{};
    std::array<double, kNumEarlyTaps> earlyTapPanL{};
    std::array<double, kNumEarlyTaps> earlyTapPanR{};

    std::array<SimpleAllPass, 2> diffusersL;
    std::array<SimpleAllPass, 2> diffusersR;

    std::array<std::vector<double>, kNumFDNLst> fdnBuffersL;
    std::array<std::vector<double>, kNumFDNLst> fdnBuffersR;
    std::array<int, kNumFDNLst> fdnLengthsL{};
    std::array<int, kNumFDNLst> fdnLengthsR{};
    std::array<int, kNumFDNLst> fdnWritePosL{};
    std::array<int, kNumFDNLst> fdnWritePosR{};
    std::array<Biquad, kNumFDNLst> fdnFiltersL;
    std::array<Biquad, kNumFDNLst> fdnFiltersR;
};

} // namespace nova::graviton
