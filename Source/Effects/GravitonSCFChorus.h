#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// NOVA HD — TC Electronic Stereo Chorus / Pitch Modulator / Flanger (SCF Gold)
// Hybrid BBD-Inspired 64-Bit Modulation Engine (10-Stage Pipeline, 0 Latency)
// ==============================================================================
enum class SCFMode : int {
    Chorus   = 0, // Classic Stereo Chorus: modulated delay + dry mix, Intensity = notch depth
    PitchMod = 1, // Pitch Modulation (P.M.): delay derivative pitch shift, Intensity = Chorus <-> Vibrato
    Flanger  = 2  // Dual-Feedback Comb Flanger: short delay sweep, Intensity = Dual Regeneration
};

class TCSCFProcessor final {
public:
    // 16384 samples supports > 85 ms delay at 192 kHz with power-of-two mask
    static constexpr int kMaxDelaySamples = 16384;
    static constexpr int kDelayMask = kMaxDelaySamples - 1;

    struct Preset {
        const char* category;
        const char* name;
        SCFMode mode;
        double speedHz;          // 0.1 - 10.0 Hz
        double widthDial;        // 0.0 - 6.0
        double intensityDial;    // 0.0 - 6.0
        double inputGainDb;      // 0.0 - 15.0 dB
        double stereoPhaseDeg;   // 0.0 - 180.0 deg (90 deg = classic quadrature stereo)
        double dualFbBlend;      // 0.0 - 1.0 (Tap B ratio in Dual Flanger)
        bool fbInvertPolarity;   // false = + Jet, true = - Hollow
        double bbdBandwidthHz;   // 6000.0 - 20000.0 Hz
        double softSatAmount;    // 0.0 - 1.0
        double companderAmount;  // 0.0 - 1.0
        double mix;              // 0.0 - 1.0
        const char* purpose;
    };

    TCSCFProcessor() noexcept {
        reset();
        loadPresetByIndex(0);
    }

    void setSampleRate(double sr) noexcept {
        const double clamped = (sr >= 44100.0 && sr <= 192000.0) ? sr : ((sr > 1000.0) ? sr : 48000.0);
        if (std::abs(clamped - sampleRate) > 1.0) {
            sampleRate = clamped;
            updateFilterCoefficients();
        }
    }
    double getSampleRate() const noexcept { return sampleRate; }

    void reset() noexcept {
        std::memset(delayBufL, 0, sizeof(delayBufL));
        std::memset(delayBufR, 0, sizeof(delayBufR));
        writePos = 0;

        feedbackSampleAL = 0.0;
        feedbackSampleAR = 0.0;
        feedbackSampleBL = 0.0;
        feedbackSampleBR = 0.0;

        fbDcX1L = fbDcY1L = 0.0;
        fbDcX1R = fbDcY1R = 0.0;
        fbDampStateL = fbDampStateR = 0.0;

        inCondStateL = inCondStateR = 0.0;
        compEnvInL = compEnvInR = 0.1;
        compEnvOutL = compEnvOutR = 0.1;

        bbdLp1L = bbdLp2L = 0.0;
        bbdLp1R = bbdLp2R = 0.0;

        // Continuous LFO phase state (never reset per sample)
        lfoPhaseL = 0.0;
        lfoPhaseR = (stereoPhaseDeg * std::numbers::pi) / 180.0;

        prevDelayMsL = 9.2;
        prevDelayMsR = 9.2;

        overloadEnv = 0.0;
        overloadFlag.store(false, std::memory_order_relaxed);
        meterInputPeak.store(0.0f, std::memory_order_relaxed);
        meterOutputPeak.store(0.0f, std::memory_order_relaxed);
        telemetryLfoL.store(0.0f, std::memory_order_relaxed);
        telemetryLfoR.store(0.0f, std::memory_order_relaxed);
        telemetryDelayMsL.store(9.2f, std::memory_order_relaxed);
        telemetryDelayMsR.store(9.2f, std::memory_order_relaxed);
        telemetryPitchCentsL.store(0.0f, std::memory_order_relaxed);
        telemetryPitchCentsR.store(0.0f, std::memory_order_relaxed);

        updateFilterCoefficients();
    }

    // ==========================================================================
    // 1. Classic SCF Main Controls (Speed, Width 0-6, Mode, Intensity 0-6, Input Gain, Overload)
    // ==========================================================================
    void setMode(SCFMode m) noexcept { mode = m; }
    SCFMode getMode() const noexcept { return mode; }

    // Speed: 0.1 - 10.0 Hz
    void setSpeed(double hz) noexcept { speedHz = std::clamp(hz, 0.1, 10.0); }
    double getSpeed() const noexcept { return speedHz; }

    // Width Dial: 0.0 - 6.0 (Classic SCF scale)
    void setWidthDial(double w) noexcept {
        widthDial = std::clamp(w, 0.0, 6.0);
        depth = widthDial / 6.0;
    }
    double getWidthDial() const noexcept { return widthDial; }

    // Normalized Depth (0.0 - 1.0) kept for backwards compatibility with existing rack calls
    void setDepth(double d) noexcept {
        if (d > 1.0001) {
            setWidthDial(d);
        } else {
            depth = std::clamp(d, 0.0, 1.0);
            widthDial = depth * 6.0;
        }
    }
    double getDepth() const noexcept { return depth; }

    // Intensity Dial: 0.0 - 6.0 (Mode-dependent function: Chorus notch, P.M. blend, Flanger regen)
    void setIntensityDial(double iDial) noexcept {
        intensityDial = std::clamp(iDial, 0.0, 6.0);
        intensity = (intensityDial / 6.0) * 0.95;
    }
    double getIntensityDial() const noexcept { return intensityDial; }

    // Normalized Intensity (0.0 - 0.95) kept for backwards compatibility
    void setIntensity(double i) noexcept {
        if (i > 1.0001) {
            setIntensityDial(i);
        } else {
            intensity = std::clamp(i, 0.0, 0.95);
            intensityDial = (intensity / 0.95) * 6.0;
        }
    }
    double getIntensity() const noexcept { return intensity; }

    // Input Gain: 0.0 to +15.0 dB (Preamp gain stage)
    void setInputGainDb(double db) noexcept {
        inputGainDb = std::clamp(db, 0.0, 15.0);
        inputGainLin = dbToGain(inputGainDb);
    }
    double getInputGainDb() const noexcept { return inputGainDb; }

    // Overload Indicator (LED telemetry)
    bool isOverloadActive() const noexcept { return overloadFlag.load(std::memory_order_relaxed); }
    float getOverloadLevel() const noexcept { return overloadMeter.load(std::memory_order_relaxed); }

    // ==========================================================================
    // 2. NOVA HD Stereo & Hybrid BBD Coloration Controls
    // ==========================================================================
    // Stereo Phase Offset between Left and Right LFOs (0 to 180 deg; 90 deg = Quadrature Stereo)
    void setStereoPhaseDeg(double deg) noexcept {
        stereoPhaseDeg = std::clamp(deg, 0.0, 180.0);
        stereoWidth = stereoPhaseDeg / 180.0;
    }
    double getStereoPhaseDeg() const noexcept { return stereoPhaseDeg; }

    // Backwards-compatible Stereo Width (0.0 - 1.0 maps to 0 - 180 deg phase offset)
    void setStereoWidth(double w) noexcept {
        stereoWidth = std::clamp(w, 0.0, 1.0);
        stereoPhaseDeg = 180.0 * stereoWidth;
    }
    double getStereoWidth() const noexcept { return stereoWidth; }

    // Dual Flanger Feedback Network controls (Tap B blend, Polarity, Cross-Coupling)
    void setDualFeedbackBlend(double blend) noexcept { dualFbBlend = std::clamp(blend, 0.0, 1.0); }
    double getDualFeedbackBlend() const noexcept { return dualFbBlend; }

    void setFeedbackInvertPolarity(bool invert) noexcept { fbInvertPolarity = invert; }
    bool getFeedbackInvertPolarity() const noexcept { return fbInvertPolarity; }

    void setFeedbackCrossCoupling(double cross) noexcept { fbCrossCoupling = std::clamp(cross, 0.0, 0.50); }
    double getFeedbackCrossCoupling() const noexcept { return fbCrossCoupling; }

    // Hybrid BBD Character controls
    void setBbdBandwidthHz(double hz) noexcept {
        bbdBandwidthHz = std::clamp(hz, 6000.0, 20000.0);
        updateFilterCoefficients();
    }
    double getBbdBandwidthHz() const noexcept { return bbdBandwidthHz; }

    void setSoftSaturation(double amt) noexcept { softSatAmount = std::clamp(amt, 0.0, 1.0); }
    double getSoftSaturation() const noexcept { return softSatAmount; }

    void setCompanderAmount(double amt) noexcept { companderAmount = std::clamp(amt, 0.0, 1.0); }
    double getCompanderAmount() const noexcept { return companderAmount; }

    void setAnalogNoiseEnabled(bool en) noexcept { analogNoiseEnabled = en; }
    bool getAnalogNoiseEnabled() const noexcept { return analogNoiseEnabled; }

    void setOutputStereoInvert(bool inv) noexcept { outputStereoInvert = inv; }
    bool getOutputStereoInvert() const noexcept { return outputStereoInvert; }

    void setOutputGainDb(double db) noexcept {
        outputGainDb = std::clamp(db, -12.0, 12.0);
        outputGainLin = dbToGain(outputGainDb);
    }
    double getOutputGainDb() const noexcept { return outputGainDb; }

    void setMix(double m) noexcept { mix = std::clamp(m, 0.0, 1.0); }
    double getMix() const noexcept { return mix; }

    void setPower(bool on) noexcept { powerOn = on; }
    bool getPower() const noexcept { return powerOn; }

    // ==========================================================================
    // 3. Real-Time UI Telemetry Getters
    // ==========================================================================
    float getMeterInputPeak() const noexcept { return meterInputPeak.load(std::memory_order_relaxed); }
    float getMeterOutputPeak() const noexcept { return meterOutputPeak.load(std::memory_order_relaxed); }
    float getTelemetryLfoL() const noexcept { return telemetryLfoL.load(std::memory_order_relaxed); }
    float getTelemetryLfoR() const noexcept { return telemetryLfoR.load(std::memory_order_relaxed); }
    float getTelemetryDelayMsL() const noexcept { return telemetryDelayMsL.load(std::memory_order_relaxed); }
    float getTelemetryDelayMsR() const noexcept { return telemetryDelayMsR.load(std::memory_order_relaxed); }
    float getTelemetryPitchCentsL() const noexcept { return telemetryPitchCentsL.load(std::memory_order_relaxed); }
    float getTelemetryPitchCentsR() const noexcept { return telemetryPitchCentsR.load(std::memory_order_relaxed); }

    // ==========================================================================
    // 4. Factory Presets (12 Curated TC SCF Gold Presets across Chorus, P.M., Flanger)
    // ==========================================================================
    static const std::vector<Preset>& getFactoryPresets() {
        static const std::vector<Preset> kPresets = {
            // --- CHORUS PRESETS ---
            { "Chorus", "1. SCF Gold Classic Chorus", SCFMode::Chorus,
              0.85, 3.8, 3.4, 2.0, 90.0, 0.25, false, 14500.0, 0.25, 0.55, 0.50,
              "Authentic 1980s TC SCF Gold stereo chorus with 90-deg quadrature width" },
            { "Chorus", "2. Lush 12-String Shimmer", SCFMode::Chorus,
              1.45, 4.4, 4.5, 3.5, 110.0, 0.35, false, 16000.0, 0.30, 0.60, 0.58,
              "Deep multi-notch BBD chorus for acoustic & clean electric guitar" },
            { "Chorus", "3. Warm Rhodes & Synth Pad", SCFMode::Chorus,
              0.42, 4.8, 3.8, 1.5, 120.0, 0.30, false, 12500.0, 0.40, 0.65, 0.52,
              "Slow, wide analog BBD dimension for electric pianos and synth strings" },
            { "Chorus", "4. Vocal Silk Doubler", SCFMode::Chorus,
              0.65, 2.6, 2.8, 1.0, 95.0, 0.15, false, 15500.0, 0.20, 0.45, 0.38,
              "Subtle stereo widening for lead and backing vocals without pitch wobble" },

            // --- PITCH MODULATION (P.M.) PRESETS ---
            { "Pitch Mod", "5. SCF Pitch Mod Ensemble", SCFMode::PitchMod,
              1.10, 3.6, 2.8, 2.0, 90.0, 0.20, false, 15000.0, 0.25, 0.50, 0.65,
              "Balanced blend of pitch modulation and chorus without comb-filter coloration" },
            { "Pitch Mod", "6. True Pitch Vibrato", SCFMode::PitchMod,
              4.20, 3.2, 6.0, 3.0, 0.0, 0.0, false, 14000.0, 0.35, 0.55, 1.00,
              "100% pure pitch-modulated vibrato driven by delay derivative dD(t)/dt" },
            { "Pitch Mod", "7. Slow Tape Detune Drift", SCFMode::PitchMod,
              0.28, 5.2, 1.8, 2.5, 105.0, 0.15, false, 13000.0, 0.45, 0.60, 0.55,
              "Organic pitch drift and wide stereo thickening for guitars and keys" },
            { "Pitch Mod", "8. Rotary Shimmer Vibrato", SCFMode::PitchMod,
              5.80, 2.9, 4.6, 4.0, 90.0, 0.25, false, 13500.0, 0.40, 0.60, 0.85,
              "Fast Leslie-inspired pitch modulation with warm BBD saturation" },

            // --- FLANGER PRESETS ---
            { "Flanger", "9. Classic SCF Jet Flanger", SCFMode::Flanger,
              0.35, 4.8, 4.6, 3.5, 90.0, 0.45, false, 15000.0, 0.35, 0.60, 0.55,
              "Sweeping comb-filter jet flange with Dual Feedback network (+ polarity)" },
            { "Flanger", "10. Hollow Tube Flanger (-)", SCFMode::Flanger,
              0.50, 4.2, 4.4, 2.5, 90.0, 0.50, true, 14000.0, 0.30, 0.55, 0.52,
              "Inverted-polarity hollow comb regeneration for woodwind-like resonances" },
            { "Flanger", "11. Metallic Chord Sweep", SCFMode::Flanger,
              1.25, 3.4, 5.2, 4.5, 120.0, 0.65, false, 16500.0, 0.45, 0.65, 0.60,
              "High-intensity dual-tap metallic flanger with cross-coupled stereo feedback" },
            { "Flanger", "12. Subtle Drum & Bass Comb", SCFMode::Flanger,
              0.22, 2.8, 2.6, 1.5, 75.0, 0.30, false, 13500.0, 0.25, 0.50, 0.40,
              "Controlled slow comb movement for bass guitar, overheads, and percussion" }
        };
        return kPresets;
    }

    void loadPresetByIndex(int index) noexcept {
        const auto& all = getFactoryPresets();
        if (all.empty()) return;
        const int idx = std::clamp(index, 0, static_cast<int>(all.size()) - 1);
        currentPresetIndex = idx;
        const auto& p = all[static_cast<size_t>(idx)];

        setMode(p.mode);
        setSpeed(p.speedHz);
        setWidthDial(p.widthDial);
        setIntensityDial(p.intensityDial);
        setInputGainDb(p.inputGainDb);
        setStereoPhaseDeg(p.stereoPhaseDeg);
        setDualFeedbackBlend(p.dualFbBlend);
        setFeedbackInvertPolarity(p.fbInvertPolarity);
        setBbdBandwidthHz(p.bbdBandwidthHz);
        setSoftSaturation(p.softSatAmount);
        setCompanderAmount(p.companderAmount);
        setMix(p.mix);
    }

    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    // ==========================================================================
    // 5. 10-Stage Real-Time 64-Bit Stereo Processing Pipeline
    // ==========================================================================
    void processStereo(double& inOutL, double& inOutR) noexcept {
        inOutL = sanitize(inOutL);
        inOutR = sanitize(inOutR);

        if (!powerOn) {
            meterInputPeak.store(static_cast<float>(std::max(std::abs(inOutL), std::abs(inOutR))), std::memory_order_relaxed);
            meterOutputPeak.store(meterInputPeak.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return;
        }

        // ----------------------------------------------------------------------
        // STAGE 1: Input Gain (0 to +15 dB) & Overload Detection
        // ----------------------------------------------------------------------
        double xL = inOutL * inputGainLin;
        double xR = inOutR * inputGainLin;

        const double inPeak = std::max(std::abs(xL), std::abs(xR));
        meterInputPeak.store(static_cast<float>(inPeak), std::memory_order_relaxed);

        // Overload threshold around -1.2 dBFS (0.871 linear) as in BBD headroom
        if (inPeak > 0.871) {
            overloadEnv = 1.0;
        } else {
            overloadEnv *= overloadReleaseCoeff;
        }
        overloadFlag.store(overloadEnv > 0.15, std::memory_order_relaxed);
        overloadMeter.store(static_cast<float>(overloadEnv), std::memory_order_relaxed);

        // Soft preamp headroom protection when pushed into overload
        if (inPeak > 0.90) {
            xL = 0.98 * std::tanh(xL / 0.98);
            xR = 0.98 * std::tanh(xR / 0.98);
        }

        const double dryL = xL;
        const double dryR = xR;

        // ----------------------------------------------------------------------
        // STAGE 2: Input Conditioning (BBD Compander Compressor + Pre-Filtering)
        // ----------------------------------------------------------------------
        // Syllabic envelope follower for input companding
        const double absInL = std::max(1e-5, std::abs(xL));
        const double absInR = std::max(1e-5, std::abs(xR));
        compEnvInL += (absInL > compEnvInL ? compAttackCoeff : compReleaseCoeff) * (absInL - compEnvInL);
        compEnvInR += (absInR > compEnvInR ? compAttackCoeff : compReleaseCoeff) * (absInR - compEnvInR);

        // Compander compression law (reduces dynamic range entering BBD line)
        const double compExp = -0.22 * companderAmount;
        const double compGainInL = std::clamp(std::pow(compEnvInL + 0.02, compExp), 0.5, 2.2);
        const double compGainInR = std::clamp(std::pow(compEnvInR + 0.02, compExp), 0.5, 2.2);

        inCondStateL += inCondAlpha * (xL * compGainInL - inCondStateL);
        inCondStateR += inCondAlpha * (xR * compGainInR - inCondStateR);

        // Subtle high-frequency pre-emphasis before delay line
        const double condL = sanitize(inCondStateL + 0.12 * (xL * compGainInL - inCondStateL));
        const double condR = sanitize(inCondStateR + 0.12 * (xR * compGainInR - inCondStateR));

        // ----------------------------------------------------------------------
        // STAGE 3: Dual LFO Generator (Independent Left / Right + Phase Offset)
        // ----------------------------------------------------------------------
        constexpr double twoPi = 2.0 * std::numbers::pi;
        const double phaseOffsetRad = (stereoPhaseDeg * std::numbers::pi) / 180.0;
        const double lfoInc = (twoPi * speedHz) / sampleRate;

        // Continuous LFO phase accumulation
        lfoPhaseL += lfoInc;
        if (lfoPhaseL >= twoPi) lfoPhaseL -= twoPi;
        lfoPhaseR = lfoPhaseL + phaseOffsetRad;
        if (lfoPhaseR >= twoPi) lfoPhaseR -= twoPi;

        // Primary LFO(t) = sin(2*pi*f*t + phi)
        const double lfoL = std::sin(lfoPhaseL);
        const double lfoR = std::sin(lfoPhaseR);

        // Secondary quadrature LFO for Tap B (Dual Feedback / Multi-Notch Chorus)
        const double lfo2L = std::cos(lfoPhaseL * 1.0 + 0.52);
        const double lfo2R = std::cos(lfoPhaseR * 1.0 + 0.52);

        telemetryLfoL.store(static_cast<float>(lfoL), std::memory_order_relaxed);
        telemetryLfoR.store(static_cast<float>(lfoR), std::memory_order_relaxed);

        // ----------------------------------------------------------------------
        // STAGE 4 & 5: Modulated Delay Calculation & Cubic Hermite Interpolation
        // ----------------------------------------------------------------------
        const double normWidth = widthDial / 6.0; // 0.0 .. 1.0
        const double normIntensity = intensityDial / 6.0; // 0.0 .. 1.0

        double centerDelayMs = 9.2;
        double sweepDepthMs = 6.0 * normWidth;

        if (mode == SCFMode::Flanger) {
            // Short delay for comb filtering (sweeps down to ~0.15 ms)
            centerDelayMs = 1.35 + 0.85 * normWidth;
            sweepDepthMs = 1.20 * normWidth;
        } else if (mode == SCFMode::PitchMod) {
            // Modulated delay optimized for smooth derivative pitch shifting
            centerDelayMs = 7.2;
            sweepDepthMs = 4.6 * normWidth;
        } else {
            // Chorus: Intensity also deepens the BBD delay center slightly for richer notches
            centerDelayMs = 8.5 + 2.5 * normIntensity;
            sweepDepthMs = 5.8 * normWidth;
        }

        // Primary modulated delay D_1(t) = D_center + W * LFO(t)
        const double delayMs1L = std::clamp(centerDelayMs + sweepDepthMs * lfoL, 0.12, 40.0);
        const double delayMs1R = std::clamp(centerDelayMs + sweepDepthMs * lfoR, 0.12, 40.0);

        // Secondary modulated delay D_2(t) for Dual Flanger Feedback / Multi-Voice Chorus
        constexpr double kGoldenTapRatio = 0.61803398875;
        const double delayMs2L = std::clamp(centerDelayMs * kGoldenTapRatio + sweepDepthMs * 0.72 * lfo2L, 0.10, 40.0);
        const double delayMs2R = std::clamp(centerDelayMs * kGoldenTapRatio + sweepDepthMs * 0.72 * lfo2R, 0.10, 40.0);

        // Instantaneous pitch shift telemetry: PitchRatio(t) ≈ 1 - dD(t)/dt
        const double dtMs = 1000.0 / sampleRate;
        const double dDelayDtL = (delayMs1L - prevDelayMsL) / dtMs;
        const double dDelayDtR = (delayMs1R - prevDelayMsR) / dtMs;
        prevDelayMsL = delayMs1L;
        prevDelayMsR = delayMs1R;

        const double pitchRatioL = std::clamp(1.0 - dDelayDtL, 0.5, 2.0);
        const double pitchRatioR = std::clamp(1.0 - dDelayDtR, 0.5, 2.0);
        telemetryDelayMsL.store(static_cast<float>(delayMs1L), std::memory_order_relaxed);
        telemetryDelayMsR.store(static_cast<float>(delayMs1R), std::memory_order_relaxed);
        telemetryPitchCentsL.store(static_cast<float>(1200.0 * std::log2(pitchRatioL)), std::memory_order_relaxed);
        telemetryPitchCentsR.store(static_cast<float>(1200.0 * std::log2(pitchRatioR)), std::memory_order_relaxed);

        // ----------------------------------------------------------------------
        // STAGE 7 (Input Injection): Dual Flanger Feedback Network & Stability Matrix
        // y(t) = x(t) + g1 * y1(t - D1) + g2 * y2(t - D2)
        // ----------------------------------------------------------------------
        double g1 = 0.0;
        double g2 = 0.0;
        const double polA = fbInvertPolarity ? -1.0 : 1.0;
        const double polB = fbInvertPolarity ? 1.0 : -1.0; // Complementary polarity on Tap B

        if (mode == SCFMode::Flanger) {
            // Dual Flanger Feedback controlled by Intensity (0-6)
            const double totalFbBudget = 0.88 * normIntensity;
            g1 = totalFbBudget * (1.0 - 0.38 * dualFbBlend);
            g2 = totalFbBudget * (0.38 * dualFbBlend);
        } else if (mode == SCFMode::Chorus) {
            // Chorus: Intensity adds subtle BBD notch depth
            g1 = 0.22 * normIntensity;
            g2 = 0.08 * normIntensity * dualFbBlend;
        }
        // PitchMod has 0 feedback so pitch modulation stays pure without comb resonance

        // Cross-coupled Dual Feedback Matrix
        const double cross = fbCrossCoupling;
        const double rawFbL = polA * g1 * feedbackSampleAL
                            + polB * g2 * ((1.0 - cross) * feedbackSampleBL + cross * feedbackSampleBR);
        const double rawFbR = polA * g1 * feedbackSampleAR
                            + polB * g2 * ((1.0 - cross) * feedbackSampleBR + cross * feedbackSampleBL);

        // Feedback Stability Conditioning: 25 Hz DC blocker + high-frequency damping + tanh bound
        const double dcOutL = rawFbL - fbDcX1L + fbDcR * fbDcY1L;
        fbDcX1L = rawFbL;
        fbDcY1L = sanitize(dcOutL);

        const double dcOutR = rawFbR - fbDcX1R + fbDcR * fbDcY1R;
        fbDcX1R = rawFbR;
        fbDcY1R = sanitize(dcOutR);

        fbDampStateL += fbDampAlpha * (fbDcY1L - fbDampStateL);
        fbDampStateR += fbDampAlpha * (fbDcY1R - fbDampStateR);

        const double stableFbL = 0.94 * std::tanh(fbDampStateL / 0.94);
        const double stableFbR = 0.94 * std::tanh(fbDampStateR / 0.94);

        // Write into 64-bit circular delay buffer
        delayBufL[writePos] = sanitize(condL + stableFbL);
        delayBufR[writePos] = sanitize(condR + stableFbR);

        // Read Fractional Delay Taps via 4-Point Cubic Hermite Interpolation
        const double samp1L = (delayMs1L * 0.001) * sampleRate;
        const double samp1R = (delayMs1R * 0.001) * sampleRate;
        const double samp2L = (delayMs2L * 0.001) * sampleRate;
        const double samp2R = (delayMs2R * 0.001) * sampleRate;

        const double tap1L = readDelayHermite(delayBufL, writePos, samp1L);
        const double tap1R = readDelayHermite(delayBufR, writePos, samp1R);
        const double tap2L = readDelayHermite(delayBufL, writePos, samp2L);
        const double tap2R = readDelayHermite(delayBufR, writePos, samp2R);

        writePos = (writePos + 1) & kDelayMask;

        // Store feedback states for next sample
        feedbackSampleAL = tap1L;
        feedbackSampleAR = tap1R;
        feedbackSampleBL = tap2L;
        feedbackSampleBR = tap2R;

        // ----------------------------------------------------------------------
        // STAGE 6: Mode Engine (Chorus / Pitch Modulation / Flanger)
        // ----------------------------------------------------------------------
        double wetL = tap1L;
        double wetR = tap1R;

        if (mode == SCFMode::Chorus) {
            // Intensity (0-6) shapes chorus notch depth by blending Tap 1 and complementary Tap 2
            const double tap2Weight = 0.32 * normIntensity;
            wetL = tap1L * (1.0 - tap2Weight * 0.4) + tap2L * tap2Weight;
            wetR = tap1R * (1.0 - tap2Weight * 0.4) + tap2R * tap2Weight;
        } else if (mode == SCFMode::PitchMod) {
            // Pitch Modulation: pure derivative pitch shift from Tap 1 + subtle quadrature Tap 2 at low intensity
            const double chorusColor = (1.0 - normIntensity) * 0.25;
            wetL = tap1L * (1.0 - chorusColor) + tap2L * chorusColor;
            wetR = tap1R * (1.0 - chorusColor) + tap2R * chorusColor;
        } else {
            // Flanger: short comb tap1 + dual feedback tap2 contribution
            const double tap2Mix = 0.28 * dualFbBlend * normIntensity;
            wetL = tap1L + tap2L * tap2Mix;
            wetR = tap1R + tap2R * tap2Mix;
        }

        // ----------------------------------------------------------------------
        // STAGE 8: Hybrid BBD Coloration (Bandwidth Filter, Soft Sat, Compander Expand, Noise)
        // ----------------------------------------------------------------------
        // 8a. 2-Pole Cascaded BBD Reconstruction Low-Pass Filter
        bbdLp1L += bbdFilterAlpha * (wetL - bbdLp1L);
        bbdLp2L += bbdFilterAlpha * (bbdLp1L - bbdLp2L);
        bbdLp1R += bbdFilterAlpha * (wetR - bbdLp1R);
        bbdLp2R += bbdFilterAlpha * (bbdLp1R - bbdLp2R);

        double coloredL = bbdLp2L;
        double coloredR = bbdLp2R;

        // 8b. BBD Soft Saturation (subtle MN3007 even/odd harmonic transfer curve)
        if (softSatAmount > 0.001) {
            const double drive = 1.0 + 1.35 * softSatAmount;
            const double satL = std::tanh(coloredL * drive + 0.03 * softSatAmount * coloredL * coloredL) / std::tanh(drive);
            const double satR = std::tanh(coloredR * drive + 0.03 * softSatAmount * coloredR * coloredR) / std::tanh(drive);
            coloredL = coloredL * (1.0 - softSatAmount) + satL * softSatAmount;
            coloredR = coloredR * (1.0 - softSatAmount) + satR * softSatAmount;
        }

        // 8c. Compander De-Emphasis / Expander (restores dynamic punch after BBD line)
        const double absWetL = std::max(1e-5, std::abs(coloredL));
        const double absWetR = std::max(1e-5, std::abs(coloredR));
        compEnvOutL += (absWetL > compEnvOutL ? compAttackCoeff : compReleaseCoeff) * (absWetL - compEnvOutL);
        compEnvOutR += (absWetR > compEnvOutR ? compAttackCoeff : compReleaseCoeff) * (absWetR - compEnvOutR);

        const double expPower = 0.22 * companderAmount;
        const double expGainL = std::clamp(std::pow(compEnvInL + 0.02, expPower), 0.45, 2.2);
        const double expGainR = std::clamp(std::pow(compEnvInR + 0.02, expPower), 0.45, 2.2);
        coloredL *= expGainL;
        coloredR *= expGainR;

        // 8d. Optional Analog BBD Clock/Thermal Noise Floor (default OFF)
        if (analogNoiseEnabled) {
            noiseState = noiseState * 1664525u + 1013904223u;
            const double nL = (static_cast<double>(noiseState & 0xFFFFu) / 32768.0 - 1.0) * 0.000025;
            noiseState = noiseState * 1664525u + 1013904223u;
            const double nR = (static_cast<double>(noiseState & 0xFFFFu) / 32768.0 - 1.0) * 0.000025;
            coloredL += nL;
            coloredR += nR;
        }

        // ----------------------------------------------------------------------
        // STAGE 9: Stereo Mixer (Mode-Specific Intensity & Wet/Dry Blend)
        // ----------------------------------------------------------------------
        double outL = 0.0;
        double outR = 0.0;

        if (mode == SCFMode::PitchMod) {
            // In SCF Pitch Modulation mode:
            // Intensity (0-6) morphs from 50/50 Chorus+PitchMod (Intensity=0) to 100% Pure Vibrato (Intensity=6),
            // scaled by overall Mix control.
            const double vibratoBlend = 0.50 + 0.50 * normIntensity; // 0.5 -> 1.0
            const double effectiveWet = vibratoBlend * mix;
            const double effectiveDry = 1.0 - effectiveWet;
            outL = dryL * effectiveDry + coloredL * effectiveWet;
            outR = dryR * effectiveDry + coloredR * effectiveWet;
        } else if (mode == SCFMode::Chorus) {
            // In SCF Chorus mode:
            // Intensity (0-6) governs the depth of the chorus notches (wet injection ratio)
            const double chorusDepth = (0.18 + 0.82 * normIntensity) * mix;
            outL = dryL * (1.0 - chorusDepth * 0.5) + coloredL * chorusDepth;
            outR = dryR * (1.0 - chorusDepth * 0.5) + coloredR * chorusDepth;
        } else {
            // In SCF Flanger mode:
            // Comb filtering is strongest near 50/50 dry+wet; Mix controls wet balance
            outL = dryL * (1.0 - mix * 0.5) + coloredL * mix;
            outR = dryR * (1.0 - mix * 0.5) + (fbInvertPolarity ? -coloredR : coloredR) * mix;
        }

        // Classic SCF Stereo Output Phase Invert option on Right channel
        if (outputStereoInvert) {
            outR = -outR;
        }

        // ----------------------------------------------------------------------
        // STAGE 10: Output Stage & Sanitization
        // ----------------------------------------------------------------------
        inOutL = sanitize(outL * outputGainLin);
        inOutR = sanitize(outR * outputGainLin);

        const double outPeak = std::max(std::abs(inOutL), std::abs(inOutR));
        meterOutputPeak.store(static_cast<float>(outPeak), std::memory_order_relaxed);
    }

private:
    // 4-Point Cubic Hermite Fractional Delay Interpolator
    static double readDelayHermite(const double* buf, int wPos, double delaySamples) noexcept {
        const double clampedDelay = std::clamp(delaySamples, 1.0, static_cast<double>(kMaxDelaySamples - 4));
        const double readPos = static_cast<double>(wPos) - clampedDelay;
        const int iPos = static_cast<int>(std::floor(readPos));
        const double frac = readPos - static_cast<double>(iPos);

        const int i0 = (iPos - 1) & kDelayMask;
        const int i1 = iPos & kDelayMask;
        const int i2 = (iPos + 1) & kDelayMask;
        const int i3 = (iPos + 2) & kDelayMask;

        const double y0 = buf[i0];
        const double y1 = buf[i1];
        const double y2 = buf[i2];
        const double y3 = buf[i3];

        const double c0 = y1;
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

        return sanitize(((c3 * frac + c2) * frac + c1) * frac + c0);
    }

    void updateFilterCoefficients() noexcept {
        const double sr = std::max(8000.0, sampleRate);
        constexpr double twoPi = 2.0 * std::numbers::pi;

        // BBD Reconstruction Low-Pass Filter
        const double fc = std::clamp(bbdBandwidthHz, 4000.0, sr * 0.45);
        bbdFilterAlpha = 1.0 - std::exp(-twoPi * fc / sr);

        // Input conditioning anti-aliasing filter (~16 kHz)
        const double inFc = std::min(16500.0, sr * 0.44);
        inCondAlpha = 1.0 - std::exp(-twoPi * inFc / sr);

        // Feedback damping low-pass (~11.5 kHz) & 25 Hz DC blocker
        const double fbFc = std::min(11500.0, sr * 0.42);
        fbDampAlpha = 1.0 - std::exp(-twoPi * fbFc / sr);
        fbDcR = std::exp(-twoPi * 25.0 / sr);

        // Compander envelope attack (2 ms) and release (35 ms)
        compAttackCoeff  = 1.0 - std::exp(-1.0 / (0.002 * sr));
        compReleaseCoeff = 1.0 - std::exp(-1.0 / (0.035 * sr));

        // Overload LED hold decay (~220 ms)
        overloadReleaseCoeff = std::exp(-1.0 / (0.220 * sr));
    }

    // 64-bit Delay Buffers
    double delayBufL[kMaxDelaySamples]{};
    double delayBufR[kMaxDelaySamples]{};
    int writePos{0};

    // Dual Feedback Network States (Tap A & Tap B)
    double feedbackSampleAL{0.0};
    double feedbackSampleAR{0.0};
    double feedbackSampleBL{0.0};
    double feedbackSampleBR{0.0};

    // Stability Filter States
    double fbDcX1L{0.0}, fbDcY1L{0.0};
    double fbDcX1R{0.0}, fbDcY1R{0.0};
    double fbDampStateL{0.0}, fbDampStateR{0.0};
    double fbDcR{0.996};
    double fbDampAlpha{0.65};

    // Input Conditioning & Compander States
    double inCondStateL{0.0}, inCondStateR{0.0};
    double inCondAlpha{0.80};
    double compEnvInL{0.1}, compEnvInR{0.1};
    double compEnvOutL{0.1}, compEnvOutR{0.1};
    double compAttackCoeff{0.01};
    double compReleaseCoeff{0.001};

    // BBD Bandwidth Filter States
    double bbdLp1L{0.0}, bbdLp2L{0.0};
    double bbdLp1R{0.0}, bbdLp2R{0.0};
    double bbdFilterAlpha{0.75};

    // Independent L/R LFO States
    double lfoPhaseL{0.0};
    double lfoPhaseR{1.57079632679};
    double prevDelayMsL{9.2};
    double prevDelayMsR{9.2};

    // Classic SCF Parameters
    SCFMode mode{SCFMode::Chorus};
    double speedHz{0.85};           // 0.1 - 10.0 Hz
    double widthDial{3.8};          // 0.0 - 6.0
    double depth{3.8 / 6.0};        // 0.0 - 1.0 normalized mirror
    double intensityDial{3.4};      // 0.0 - 6.0
    double intensity{0.54};         // 0.0 - 0.95 normalized mirror
    double inputGainDb{2.0};        // 0.0 - 15.0 dB
    double inputGainLin{1.258925};

    // NOVA HD Extended Parameters
    double stereoPhaseDeg{90.0};    // 0.0 - 180.0 deg
    double stereoWidth{0.5};        // 0.0 - 1.0 mirror
    double dualFbBlend{0.35};       // 0.0 - 1.0
    bool fbInvertPolarity{false};   // false = + Jet, true = - Hollow
    double fbCrossCoupling{0.18};   // 0.0 - 0.50
    double bbdBandwidthHz{14500.0}; // 6000 - 20000 Hz
    double softSatAmount{0.25};     // 0.0 - 1.0
    double companderAmount{0.55};   // 0.0 - 1.0
    bool analogNoiseEnabled{false}; // default OFF
    bool outputStereoInvert{false};
    double outputGainDb{0.0};
    double outputGainLin{1.0};
    double mix{0.50};
    bool powerOn{true};
    int currentPresetIndex{0};
    double sampleRate{48000.0};

    // Overload & Telemetry State
    double overloadEnv{0.0};
    double overloadReleaseCoeff{0.9999};
    uint32_t noiseState{0x4A3B2C1Du};

    std::atomic<bool> overloadFlag{false};
    std::atomic<float> overloadMeter{0.0f};
    std::atomic<float> meterInputPeak{0.0f};
    std::atomic<float> meterOutputPeak{0.0f};
    std::atomic<float> telemetryLfoL{0.0f};
    std::atomic<float> telemetryLfoR{0.0f};
    std::atomic<float> telemetryDelayMsL{9.2f};
    std::atomic<float> telemetryDelayMsR{9.2f};
    std::atomic<float> telemetryPitchCentsL{0.0f};
    std::atomic<float> telemetryPitchCentsR{0.0f};
};

} // namespace nova::graviton
