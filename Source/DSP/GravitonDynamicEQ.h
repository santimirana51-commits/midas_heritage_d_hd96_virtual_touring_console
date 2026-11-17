#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace nova::graviton {

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapter 16 + TDR Nova Parallel Dynamic EQ Engine
// High-precision Orfanidis / Regalia-Mitra topology with continuous per-sample
// modulation, parallel W-Band + SPLIT slaving, Serial HPF/LPF, M/S (Sum/Diff),
// External Sidechain (EXT SC), Equal Loudness (EL) Auto-Gain/Bypass,
// GR Delta audition, and Band Solo.
// ==============================================================================

enum class DynamicEqMode : int {
    Above = 0, // Compresses/attenuates or boosts signals ABOVE threshold
    Below = 1  // Expands/attenuates or lifts signals BELOW threshold
};

enum class DynamicEqBandShape : int {
    LowShelf = 0,
    Bell = 1,
    HighShelf = 2
};

enum class DynamicEqStereoMode : int {
    Stereo = 0,   // Linked L/R detector, applied to both L and R
    Sum = 1,      // Mid (L+R)/2 only
    Diff = 2,     // Side (L-R)/2 only
    Left = 3,     // Left channel only
    Right = 4,    // Right channel only
    DualMono = 5  // Independent L and R detectors & filters
};

namespace dyn_detail {
inline double sanitizeSample(double x) noexcept {
    if (!std::isfinite(x)) return 0.0;
    return std::clamp(x, -16.0, 16.0);
}

inline double dbToGain(double db) noexcept {
    return std::pow(10.0, db * 0.05);
}

inline double computeDynamicGainDb(double envDb,
                                   DynamicEqMode mode,
                                   double thresholdDb,
                                   double rangeDb,
                                   double ratio,
                                   double kneeDb) noexcept {
    const double safeRatio = std::max(1.0, ratio);
    const double halfKnee = std::max(0.0, kneeDb) * 0.5;
    double dynGainDb = 0.0;

    if (mode == DynamicEqMode::Above) {
        double excess = 0.0;
        if (halfKnee > 0.001) {
            if (envDb <= thresholdDb - halfKnee) {
                excess = 0.0;
            } else if (envDb >= thresholdDb + halfKnee) {
                excess = envDb - thresholdDb;
            } else {
                const double diff = envDb - thresholdDb + halfKnee;
                excess = (diff * diff) / (4.0 * halfKnee);
            }
        } else {
            excess = std::max(0.0, envDb - thresholdDb);
        }

        if (excess > 0.0) {
            const double slope = (1.0 - (1.0 / safeRatio));
            const double rawDelta = slope * excess;
            dynGainDb = (rangeDb < 0.0) ? -std::min(-rangeDb, rawDelta)
                                        : std::min(rangeDb, rawDelta);
        }
    } else {
        double deficit = 0.0;
        if (halfKnee > 0.001) {
            if (envDb >= thresholdDb + halfKnee) {
                deficit = 0.0;
            } else if (envDb <= thresholdDb - halfKnee) {
                deficit = thresholdDb - envDb;
            } else {
                const double diff = thresholdDb - envDb + halfKnee;
                deficit = (diff * diff) / (4.0 * halfKnee);
            }
        } else {
            deficit = std::max(0.0, thresholdDb - envDb);
        }

        if (deficit > 0.0) {
            const double slope = (1.0 - (1.0 / safeRatio));
            const double rawDelta = slope * deficit;
            dynGainDb = (rangeDb < 0.0) ? -std::min(-rangeDb, rawDelta)
                                        : std::min(rangeDb, rawDelta);
        }
    }
    return dynGainDb;
}
} // namespace dyn_detail

// ==============================================================================
// Serial Input HPF / LPF (6, 12, 18, 24 dB/oct Butterworth cascade)
// ==============================================================================
class SerialSlopeFilter final {
public:
    void reset() noexcept {
        for (auto& st : stages) st.reset();
        z1 = 0.0;
        z2 = 0.0;
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && sr != sampleRate) {
            sampleRate = sr;
            cachedFreq = -1.0;
        }
    }

    // slopeIndex: 0=6dB, 1=12dB, 2=18dB, 3=24dB
    double process(double x, bool enabled, bool isHighPass, double freqHz, int slopeIndex) noexcept {
        if (!enabled) return x;
        const int clampedSlope = std::clamp(slopeIndex, 0, 3);
        const double clampedFreq = std::clamp(freqHz, 15.0, sampleRate * 0.45);
        updateCoeffsIfNeeded(isHighPass, clampedFreq, clampedSlope);

        double y = x;
        if (clampedSlope == 0 || clampedSlope == 2) {
            // 1st-order section
            const double out1 = b0_1 * y + b1_1 * z1 - a1_1 * z2;
            z1 = y;
            z2 = dyn_detail::sanitizeSample(out1);
            y = out1;
        }
        const int numBiquads = (clampedSlope == 0) ? 0 : (clampedSlope <= 2 ? 1 : 2);
        for (int i = 0; i < numBiquads; ++i) {
            y = stages[static_cast<size_t>(i)].process(y);
        }
        return dyn_detail::sanitizeSample(y);
    }

private:
    struct Biquad {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double s1 = 0.0, s2 = 0.0;
        void reset() noexcept { s1 = 0.0; s2 = 0.0; }
        double process(double x) noexcept {
            const double y = b0 * x + s1;
            s1 = b1 * x - a1 * y + s2;
            s2 = b2 * x - a2 * y;
            return y;
        }
    };

    void updateCoeffsIfNeeded(bool isHp, double freq, int slope) noexcept {
        if (isHp == cachedHp && freq == cachedFreq && slope == cachedSlope && sampleRate == cachedSr) {
            return;
        }
        cachedHp = isHp;
        cachedFreq = freq;
        cachedSlope = slope;
        cachedSr = sampleRate;

        constexpr double kPi = 3.14159265358979323846;
        // 1st-order bilinear transform for 6 dB / 18 dB
        const double k = std::tan(kPi * freq / sampleRate);
        const double norm1 = 1.0 / (1.0 + k);
        if (isHp) {
            b0_1 = norm1;
            b1_1 = -norm1;
            a1_1 = (k - 1.0) * norm1;
        } else {
            b0_1 = k * norm1;
            b1_1 = k * norm1;
            a1_1 = (k - 1.0) * norm1;
        }

        // Biquad Q values for Butterworth: 12dB (Q=0.7071), 18dB (Q=1.0), 24dB (Q=0.5412, 1.3066)
        std::array<double, 2> qs{ 0.7071067811865476, 0.7071067811865476 };
        if (slope == 2) {
            qs[0] = 1.0;
        } else if (slope == 3) {
            qs[0] = 0.541196100146197;
            qs[1] = 1.3065629648763766;
        }

        const double omega = 2.0 * kPi * freq / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);

        for (size_t i = 0; i < 2; ++i) {
            const double alpha = sn / (2.0 * qs[i]);
            const double a0 = 1.0 + alpha;
            if (isHp) {
                stages[i].b0 = ((1.0 + cs) * 0.5) / a0;
                stages[i].b1 = -(1.0 + cs) / a0;
                stages[i].b2 = ((1.0 + cs) * 0.5) / a0;
            } else {
                stages[i].b0 = ((1.0 - cs) * 0.5) / a0;
                stages[i].b1 = (1.0 - cs) / a0;
                stages[i].b2 = ((1.0 - cs) * 0.5) / a0;
            }
            stages[i].a1 = (-2.0 * cs) / a0;
            stages[i].a2 = (1.0 - alpha) / a0;
        }
    }

    double sampleRate = 48000.0;
    bool cachedHp = true;
    double cachedFreq = -1.0;
    int cachedSlope = -1;
    double cachedSr = -1.0;
    double b0_1 = 1.0, b1_1 = 0.0, a1_1 = 0.0;
    double z1 = 0.0, z2 = 0.0;
    std::array<Biquad, 2> stages{};
};

// ==============================================================================
// Single Dynamic EQ Band (Bell / LowShelf / HighShelf + Internal/External SC)
// ==============================================================================
class DynamicEqBand final {
public:
    DynamicEqBand() noexcept {
        reset();
    }

    void reset() noexcept {
        s1 = 0.0;
        s2 = 0.0;
        scS1 = 0.0;
        scS2 = 0.0;
        envelope = 0.0;
        smoothedGain = 1.0;
        smoothedStaticGain = 1.0;
        currentDynGainDb.store(0.0f, std::memory_order_relaxed);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && sr != sampleRate) {
            sampleRate = sr;
            cachedFrequency = -1.0;
            cachedAttackMs = -1.0;
            cachedReleaseMs = -1.0;
        }
    }

    // Legacy serial processSample (preserved for backwards compatibility)
    double processSample(double x,
                         double frequencyHz,
                         double q,
                         double staticGainDb,
                         bool dynEnabled,
                         DynamicEqMode mode,
                         double thresholdDb,
                         double rangeDb,
                         double ratio,
                         double kneeDb,
                         double attackMs,
                         double releaseMs) noexcept {
        const double delta = processParallelDelta(
            x, x, false, true, frequencyHz, q, DynamicEqBandShape::Bell,
            staticGainDb, dynEnabled, mode, thresholdDb, rangeDb, ratio,
            kneeDb, attackMs, releaseMs, 0.0, false);
        return x + delta;
    }

    // Parallel band delta contribution: returns (g - 1) * y_band(x).
    // When slavedToWBand is true (SPLIT == false), uses wBandEnvDb instead of own envelope.
    double processParallelDelta(double x,
                                double scSignal,
                                bool useExternalSc,
                                bool splitIndependent,
                                double frequencyHz,
                                double q,
                                DynamicEqBandShape shape,
                                double staticGainDb,
                                bool dynEnabled,
                                DynamicEqMode mode,
                                double thresholdDb,
                                double rangeDb,
                                double ratio,
                                double kneeDb,
                                double attackMs,
                                double releaseMs,
                                double wBandEnvDb,
                                bool grDeltaOnly,
                                double* outFilteredBand = nullptr) noexcept {
        if (!std::isfinite(x)) x = 0.0;
        if (!std::isfinite(scSignal)) scSignal = 0.0;

        updateCoefficientsIfNeeded(frequencyHz, q, shape);
        updateTimeConstants(attackMs, releaseMs);

        // 1. Audio bandpass / shelf-delta extraction
        const double y_band = b0 * x + s1;
        s1 = b1 * x - a1 * y_band + s2;
        s2 = b2 * x - a2 * y_band;

        if (outFilteredBand != nullptr) {
            *outFilteredBand = y_band;
        }

        // 2. Sidechain detector signal (internal audio band or external sidechain band)
        double y_sc = y_band;
        if (useExternalSc) {
            y_sc = b0 * scSignal + scS1;
            scS1 = b1 * scSignal - a1 * y_sc + scS2;
            scS2 = b2 * scSignal - a2 * y_sc;
        } else {
            scS1 = s1;
            scS2 = s2;
        }

        // 3. Ballistics Envelope Detector
        const double absDet = std::abs(y_sc);
        if (absDet > envelope) {
            envelope = attackCoeff * envelope + (1.0 - attackCoeff) * absDet;
        } else {
            envelope = releaseCoeff * envelope + (1.0 - releaseCoeff) * absDet;
        }

        // 4. Dynamic Gain Calculation
        double dynGainDb = 0.0;
        if (dynEnabled) {
            const double envDb = splitIndependent
                ? (20.0 * std::log10(std::max(envelope, 1.0e-5)))
                : wBandEnvDb;
            dynGainDb = dyn_detail::computeDynamicGainDb(
                envDb, mode, thresholdDb, rangeDb, ratio, kneeDb);
        }

        currentDynGainDb.store(static_cast<float>(dynGainDb), std::memory_order_relaxed);

        // 5. Total Effective Band Gain: static EQ + dynamic offset
        const double totalGainDb = std::clamp(staticGainDb + dynGainDb, -24.0, 24.0);
        const double targetLinear = dyn_detail::dbToGain(totalGainDb);
        smoothedGain = gainSmoothingCoeff * smoothedGain + (1.0 - gainSmoothingCoeff) * targetLinear;

        const double staticLinear = dyn_detail::dbToGain(std::clamp(staticGainDb, -24.0, 24.0));
        smoothedStaticGain = gainSmoothingCoeff * smoothedStaticGain + (1.0 - gainSmoothingCoeff) * staticLinear;

        if (grDeltaOnly) {
            return (smoothedGain - smoothedStaticGain) * y_band;
        }
        return (smoothedGain - 1.0) * y_band;
    }

    float getDynamicGainDb() const noexcept {
        return currentDynGainDb.load(std::memory_order_relaxed);
    }

private:
    void updateCoefficientsIfNeeded(double freq, double q, DynamicEqBandShape shape) noexcept {
        const double clampedFreq = std::clamp(freq, 20.0, sampleRate * 0.45);
        const double clampedQ = std::clamp(q, 0.1, 10.0);

        if (clampedFreq == cachedFrequency && clampedQ == cachedQ &&
            shape == cachedShape && sampleRate == cachedSampleRate) {
            return;
        }

        cachedFrequency = clampedFreq;
        cachedQ = clampedQ;
        cachedShape = shape;
        cachedSampleRate = sampleRate;

        constexpr double kPi = 3.14159265358979323846;
        const double omega = 2.0 * kPi * clampedFreq / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);

        if (shape == DynamicEqBandShape::LowShelf) {
            // 2nd-order 0dB-passband LP so x + (g - 1)*y_lp forms a smooth parallel low shelf
            const double alpha = sn / (2.0 * std::clamp(clampedQ, 0.35, 2.0));
            const double a0 = 1.0 + alpha;
            b0 = ((1.0 - cs) * 0.5) / a0;
            b1 = (1.0 - cs) / a0;
            b2 = ((1.0 - cs) * 0.5) / a0;
            a1 = (-2.0 * cs) / a0;
            a2 = (1.0 - alpha) / a0;
        } else if (shape == DynamicEqBandShape::HighShelf) {
            // 2nd-order 0dB-passband HP so x + (g - 1)*y_hp forms a smooth parallel high shelf
            const double alpha = sn / (2.0 * std::clamp(clampedQ, 0.35, 2.0));
            const double a0 = 1.0 + alpha;
            b0 = ((1.0 + cs) * 0.5) / a0;
            b1 = -(1.0 + cs) / a0;
            b2 = ((1.0 + cs) * 0.5) / a0;
            a1 = (-2.0 * cs) / a0;
            a2 = (1.0 - alpha) / a0;
        } else {
            // Audio EQ Cookbook BPF with 0 dB peak gain
            const double alpha = sn / (2.0 * clampedQ);
            const double a0 = 1.0 + alpha;
            b0 = alpha / a0;
            b1 = 0.0;
            b2 = -alpha / a0;
            a1 = (-2.0 * cs) / a0;
            a2 = (1.0 - alpha) / a0;
        }
    }

    void updateTimeConstants(double attackMs, double releaseMs) noexcept {
        const double safeAtt = std::clamp(attackMs, 0.1, 200.0);
        const double safeRel = std::clamp(releaseMs, 5.0, 2000.0);

        if (safeAtt != cachedAttackMs || safeRel != cachedReleaseMs) {
            cachedAttackMs = safeAtt;
            cachedReleaseMs = safeRel;
            attackCoeff = std::exp(-1.0 / (sampleRate * (safeAtt * 0.001)));
            releaseCoeff = std::exp(-1.0 / (sampleRate * (safeRel * 0.001)));
            gainSmoothingCoeff = std::exp(-1.0 / (sampleRate * 0.005));
        }
    }

    double sampleRate = 48000.0;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1 = 0.0, s2 = 0.0;
    double scS1 = 0.0, scS2 = 0.0;
    double envelope = 0.0;
    double smoothedGain = 1.0;
    double smoothedStaticGain = 1.0;
    std::atomic<float> currentDynGainDb{ 0.0f };

    double cachedFrequency = -1.0;
    double cachedQ = -1.0;
    DynamicEqBandShape cachedShape = DynamicEqBandShape::Bell;
    double cachedSampleRate = -1.0;
    double cachedAttackMs = 10.0;
    double cachedReleaseMs = 100.0;
    double attackCoeff = 0.99;
    double releaseCoeff = 0.999;
    double gainSmoothingCoeff = 0.995;
};

// ==============================================================================
// TDR Nova Parallel Dynamic EQ Configuration Struct (POD, Audio-Thread Safe)
// ==============================================================================
struct NovaParallelParams {
    // 4 Bands
    std::array<double, 4> freqs{ 100.0, 500.0, 2000.0, 7000.0 };
    std::array<double, 4> qs{ 1.0, 1.0, 1.0, 1.5 };
    std::array<double, 4> staticGainsDb{ 0.0, 0.0, 0.0, 0.0 };
    std::array<DynamicEqBandShape, 4> shapes{
        DynamicEqBandShape::Bell, DynamicEqBandShape::Bell,
        DynamicEqBandShape::Bell, DynamicEqBandShape::Bell
    };
    std::array<bool, 4> bandActive{ true, true, true, true };
    std::array<bool, 4> dynEnables{ false, false, false, false };
    std::array<bool, 4> splitIndependent{ true, true, true, true }; // false = slave to W-Band
    std::array<DynamicEqMode, 4> modes{
        DynamicEqMode::Above, DynamicEqMode::Above,
        DynamicEqMode::Above, DynamicEqMode::Above
    };
    std::array<double, 4> thresholdsDb{ -20.0, -20.0, -20.0, -20.0 };
    std::array<double, 4> rangesDb{ -6.0, -6.0, -6.0, -6.0 };
    std::array<double, 4> ratios{ 4.0, 4.0, 4.0, 4.0 };
    std::array<double, 4> kneesDb{ 3.0, 3.0, 3.0, 3.0 };
    std::array<double, 4> attacksMs{ 10.0, 10.0, 10.0, 10.0 };
    std::array<double, 4> releasesMs{ 100.0, 100.0, 100.0, 100.0 };
    std::array<DynamicEqStereoMode, 4> stereoModes{
        DynamicEqStereoMode::Stereo, DynamicEqStereoMode::Stereo,
        DynamicEqStereoMode::Stereo, DynamicEqStereoMode::Stereo
    };

    // Serial HPF / LPF
    bool hpfEnabled = false;
    double hpfFreqHz = 30.0;
    int hpfSlope = 1; // 0=6dB, 1=12dB, 2=18dB, 3=24dB
    bool lpfEnabled = false;
    double lpfFreqHz = 18000.0;
    int lpfSlope = 1;

    // W-Band (Wide-Band complement & master detector)
    bool wBandEnabled = false;
    DynamicEqMode wBandMode = DynamicEqMode::Above; // Above = compress, Below = expand
    double wBandThresholdDb = -20.0;
    double wBandRatio = 2.0;
    double wBandRangeDb = -6.0;
    double wBandKneeDb = 3.0;
    double wBandAttackMs = 10.0;
    double wBandReleaseMs = 100.0;
    double wBandGainDb = 0.0;

    // Global TDR Nova features
    bool bypassed = false;
    bool elBypass = false;       // Equal Loudness compensation
    bool extSidechain = false;   // INT SC (false) vs EXT SC (true)
    bool grDelta = false;        // Audition gain reduction delta
    int soloBandIndex = -1;      // -1 = off, 0..3 = band I..IV
    double eqGainDb = 0.0;       // Pre/post EQ trim
    double dryMix = 0.0;         // 0.0 = 100% wet, 1.0 = 100% dry
    double outGainDb = 0.0;      // Master output gain
};

// ==============================================================================
// 4-Band Dynamic Parametric EQ Processor (Serial Legacy + TDR Nova Parallel)
// ==============================================================================
class DynamicEqEngine final {
public:
    static constexpr size_t kNumBands = 4;

    DynamicEqEngine() noexcept {
        reset();
    }

    void reset() noexcept {
        for (auto& band : bands) band.reset();
        for (auto& band : bandsR) band.reset();
        hpfL.reset();
        hpfR.reset();
        lpfL.reset();
        lpfR.reset();
        wBandEnvL = 0.0;
        wBandEnvR = 0.0;
        wBandSmoothedGainL = 1.0;
        wBandSmoothedGainR = 1.0;
        smoothedDryMix = 0.0;
        smoothedOutGain = 1.0;
        smoothedEqGain = 1.0;
        inRmsSq = 1.0e-6;
        outRmsSq = 1.0e-6;
        smoothedElGain = 1.0;
        wBandDynGainDb.store(0.0f, std::memory_order_relaxed);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0) {
            sampleRate = sr;
            for (auto& band : bands) band.setSampleRate(sr);
            for (auto& band : bandsR) band.setSampleRate(sr);
            hpfL.setSampleRate(sr);
            hpfR.setSampleRate(sr);
            lpfL.setSampleRate(sr);
            lpfR.setSampleRate(sr);
            wBandCachedAtt = -1.0;
            wBandCachedRel = -1.0;
            rmsCoeff = std::exp(-1.0 / (sr * 0.300));
            paramSmoothCoeff = std::exp(-1.0 / (sr * 0.005));
        }
    }

    DynamicEqBand& getBand(size_t index) noexcept {
        return bands[index < kNumBands ? index : 0];
    }
    const DynamicEqBand& getBand(size_t index) const noexcept {
        return bands[index < kNumBands ? index : 0];
    }
    float getWBandDynamicGainDb() const noexcept {
        return wBandDynGainDb.load(std::memory_order_relaxed);
    }

    // Legacy serial 4-band processing (kept intact for existing tests & callers)
    double processSample(double input,
                         const std::array<double, 4>& freqs,
                         const std::array<double, 4>& qs,
                         const std::array<double, 4>& staticGainsDb,
                         const std::array<bool, 4>& dynEnables,
                         const std::array<DynamicEqMode, 4>& modes,
                         const std::array<double, 4>& thresholdsDb,
                         const std::array<double, 4>& rangesDb,
                         const std::array<double, 4>& ratios,
                         const std::array<double, 4>& kneesDb,
                         const std::array<double, 4>& attacksMs,
                         const std::array<double, 4>& releasesMs) noexcept {
        double signal = input;
        for (size_t b = 0; b < kNumBands; ++b) {
            signal = bands[b].processSample(signal,
                                            freqs[b],
                                            qs[b],
                                            staticGainsDb[b],
                                            dynEnables[b],
                                            modes[b],
                                            thresholdsDb[b],
                                            rangesDb[b],
                                            ratios[b],
                                            kneesDb[b],
                                            attacksMs[b],
                                            releasesMs[b]);
        }
        return signal;
    }

    // TDR Nova Parallel Mono Channel Processing
    double processParallelSample(double input,
                                 double extSidechain,
                                 const NovaParallelParams& p) noexcept {
        const double safeIn = dyn_detail::sanitizeSample(input);
        const double scIn = p.extSidechain ? dyn_detail::sanitizeSample(extSidechain) : safeIn;

        updateWBandTimeConstants(p.wBandAttackMs, p.wBandReleaseMs);

        // 1. Serial Input HPF / LPF
        double filtered = hpfL.process(safeIn, p.hpfEnabled, true, p.hpfFreqHz, p.hpfSlope);
        filtered = lpfL.process(filtered, p.lpfEnabled, false, p.lpfFreqHz, p.lpfSlope);

        // 2. W-Band Envelope Detector (runs on filtered or ext sidechain)
        const double wDet = std::abs(p.extSidechain ? scIn : filtered);
        if (wDet > wBandEnvL) {
            wBandEnvL = wBandAttCoeff * wBandEnvL + (1.0 - wBandAttCoeff) * wDet;
        } else {
            wBandEnvL = wBandRelCoeff * wBandEnvL + (1.0 - wBandRelCoeff) * wDet;
        }
        const double wEnvDb = 20.0 * std::log10(std::max(wBandEnvL, 1.0e-5));

        double wDynDb = 0.0;
        if (p.wBandEnabled) {
            wDynDb = dyn_detail::computeDynamicGainDb(
                wEnvDb, p.wBandMode, p.wBandThresholdDb,
                p.wBandRangeDb, p.wBandRatio, p.wBandKneeDb);
        }
        wBandDynGainDb.store(static_cast<float>(wDynDb), std::memory_order_relaxed);
        const double wTotalLin = dyn_detail::dbToGain(std::clamp(p.wBandGainDb + wDynDb, -24.0, 24.0));
        wBandSmoothedGainL = paramSmoothCoeff * wBandSmoothedGainL + (1.0 - paramSmoothCoeff) * wTotalLin;

        // 3. Parallel Band Summing + Spectral Complement W-Band
        double bandSumDelta = 0.0;
        double sumActiveBandFilt = 0.0;
        double soloOut = 0.0;
        for (size_t b = 0; b < kNumBands; ++b) {
            double bandFiltered = 0.0;
            const double effStatic = p.bandActive[b] ? p.staticGainsDb[b] : 0.0;
            const bool effDyn = p.bandActive[b] && p.dynEnables[b];
            const DynamicEqMode effMode = p.splitIndependent[b] ? p.modes[b] : p.wBandMode;
            const double effRatio = p.splitIndependent[b] ? p.ratios[b] : p.wBandRatio;
            const double effAtt = p.splitIndependent[b] ? p.attacksMs[b] : p.wBandAttackMs;
            const double effRel = p.splitIndependent[b] ? p.releasesMs[b] : p.wBandReleaseMs;

            const double delta = bands[b].processParallelDelta(
                filtered, scIn, p.extSidechain, p.splitIndependent[b],
                p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                effAtt, effRel, wEnvDb, p.grDelta, &bandFiltered);

            if (p.bandActive[b]) {
                bandSumDelta += delta;
                sumActiveBandFilt += bandFiltered;
            }
            if (p.soloBandIndex == static_cast<int>(b)) {
                soloOut = bandFiltered;
            }
        }

        const double compSignal = filtered - sumActiveBandFilt;
        double wet = 0.0;
        if (p.soloBandIndex >= 0 && p.soloBandIndex < static_cast<int>(kNumBands)) {
            wet = soloOut;
        } else if (p.grDelta) {
            const double wDelta = (wBandSmoothedGainL - dyn_detail::dbToGain(std::clamp(p.wBandGainDb, -24.0, 24.0))) * compSignal;
            wet = wDelta + bandSumDelta;
        } else {
            wet = compSignal * wBandSmoothedGainL + sumActiveBandFilt + bandSumDelta;
        }

        // 4. EQ Gain, Equal Loudness (EL) tracking, Dry Mix & Output Gain
        const double targetEqGain = dyn_detail::dbToGain(std::clamp(p.eqGainDb, -24.0, 24.0));
        const double targetDry = std::clamp(p.dryMix, 0.0, 1.0);
        const double targetOutGain = dyn_detail::dbToGain(std::clamp(p.outGainDb, -24.0, 24.0));

        smoothedEqGain = paramSmoothCoeff * smoothedEqGain + (1.0 - paramSmoothCoeff) * targetEqGain;
        smoothedDryMix = paramSmoothCoeff * smoothedDryMix + (1.0 - paramSmoothCoeff) * targetDry;
        smoothedOutGain = paramSmoothCoeff * smoothedOutGain + (1.0 - paramSmoothCoeff) * targetOutGain;

        double processed = (wet * smoothedEqGain) * (1.0 - smoothedDryMix) + safeIn * smoothedDryMix;
        processed *= smoothedOutGain;

        // Track RMS for Equal Loudness (EL) Bypass / Auto-Gain
        inRmsSq = rmsCoeff * inRmsSq + (1.0 - rmsCoeff) * (safeIn * safeIn);
        outRmsSq = rmsCoeff * outRmsSq + (1.0 - rmsCoeff) * (processed * processed);
        const double targetEl = std::clamp(std::sqrt(inRmsSq / std::max(outRmsSq, 1.0e-12)), 0.25, 4.0);
        smoothedElGain = rmsCoeff * smoothedElGain + (1.0 - rmsCoeff) * targetEl;

        if (p.bypassed) {
            // EL Bypass matches bypassed level to processed output loudness when enabled
            return p.elBypass ? dyn_detail::sanitizeSample(safeIn / std::max(smoothedElGain, 0.25))
                              : safeIn;
        }

        if (p.elBypass && p.soloBandIndex < 0 && !p.grDelta) {
            processed *= smoothedElGain;
        }
        return dyn_detail::sanitizeSample(processed);
    }

    // TDR Nova Parallel Stereo Processing (supports Stereo, Sum/Mid, Diff/Side, Left, Right, DualMono)
    void processParallelStereoSample(double inL,
                                     double inR,
                                     double extScL,
                                     double extScR,
                                     const NovaParallelParams& p,
                                     double& outL,
                                     double& outR) noexcept {
        const double safeL = dyn_detail::sanitizeSample(inL);
        const double safeR = dyn_detail::sanitizeSample(inR);
        const double scL = p.extSidechain ? dyn_detail::sanitizeSample(extScL) : safeL;
        const double scR = p.extSidechain ? dyn_detail::sanitizeSample(extScR) : safeR;

        updateWBandTimeConstants(p.wBandAttackMs, p.wBandReleaseMs);

        // 1. Serial Input HPF / LPF on L & R
        double filtL = hpfL.process(safeL, p.hpfEnabled, true, p.hpfFreqHz, p.hpfSlope);
        filtL = lpfL.process(filtL, p.lpfEnabled, false, p.lpfFreqHz, p.lpfSlope);

        double filtR = hpfR.process(safeR, p.hpfEnabled, true, p.hpfFreqHz, p.hpfSlope);
        filtR = lpfR.process(filtR, p.lpfEnabled, false, p.lpfFreqHz, p.lpfSlope);

        // 2. W-Band Stereo Linked Envelope Detector
        const double wDet = std::max(std::abs(p.extSidechain ? scL : filtL),
                                     std::abs(p.extSidechain ? scR : filtR));
        if (wDet > wBandEnvL) {
            wBandEnvL = wBandAttCoeff * wBandEnvL + (1.0 - wBandAttCoeff) * wDet;
        } else {
            wBandEnvL = wBandRelCoeff * wBandEnvL + (1.0 - wBandRelCoeff) * wDet;
        }
        const double wEnvDb = 20.0 * std::log10(std::max(wBandEnvL, 1.0e-5));

        double wDynDb = 0.0;
        if (p.wBandEnabled) {
            wDynDb = dyn_detail::computeDynamicGainDb(
                wEnvDb, p.wBandMode, p.wBandThresholdDb,
                p.wBandRangeDb, p.wBandRatio, p.wBandKneeDb);
        }
        wBandDynGainDb.store(static_cast<float>(wDynDb), std::memory_order_relaxed);
        const double wTotalLin = dyn_detail::dbToGain(std::clamp(p.wBandGainDb + wDynDb, -24.0, 24.0));
        wBandSmoothedGainL = paramSmoothCoeff * wBandSmoothedGainL + (1.0 - paramSmoothCoeff) * wTotalLin;

        // Mid / Side projections
        const double filtMid = 0.5 * (filtL + filtR);
        const double filtSide = 0.5 * (filtL - filtR);
        const double scMid = 0.5 * (scL + scR);
        const double scSide = 0.5 * (scL - scR);

        double sumDeltaL = 0.0;
        double sumDeltaR = 0.0;
        double sumBandFiltL = 0.0;
        double sumBandFiltR = 0.0;
        double soloL = 0.0;
        double soloR = 0.0;

        for (size_t b = 0; b < kNumBands; ++b) {
            const double effStatic = p.bandActive[b] ? p.staticGainsDb[b] : 0.0;
            const bool effDyn = p.bandActive[b] && p.dynEnables[b];
            const DynamicEqMode effMode = p.splitIndependent[b] ? p.modes[b] : p.wBandMode;
            const double effRatio = p.splitIndependent[b] ? p.ratios[b] : p.wBandRatio;
            const double effAtt = p.splitIndependent[b] ? p.attacksMs[b] : p.wBandAttackMs;
            const double effRel = p.splitIndependent[b] ? p.releasesMs[b] : p.wBandReleaseMs;
            const auto stMode = p.stereoModes[b];

            double dL = 0.0;
            double dR = 0.0;
            double sOutL = 0.0;
            double sOutR = 0.0;

            if (stMode == DynamicEqStereoMode::Sum) {
                double fBand = 0.0;
                const double dMid = bands[b].processParallelDelta(
                    filtMid, scMid, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &fBand);
                dL = dMid;
                dR = dMid;
                sOutL = fBand;
                sOutR = fBand;
            } else if (stMode == DynamicEqStereoMode::Diff) {
                double fBand = 0.0;
                const double dSide = bands[b].processParallelDelta(
                    filtSide, scSide, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &fBand);
                dL = dSide;
                dR = -dSide;
                sOutL = fBand;
                sOutR = -fBand;
            } else if (stMode == DynamicEqStereoMode::Left) {
                dL = bands[b].processParallelDelta(
                    filtL, scL, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutL);
            } else if (stMode == DynamicEqStereoMode::Right) {
                dR = bandsR[b].processParallelDelta(
                    filtR, scR, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutR);
            } else if (stMode == DynamicEqStereoMode::DualMono) {
                dL = bands[b].processParallelDelta(
                    filtL, scL, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutL);
                dR = bandsR[b].processParallelDelta(
                    filtR, scR, p.extSidechain, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutR);
            } else {
                // Linked Stereo: use max(|scL|, |scR|) as sidechain detector for both L and R
                const double linkedSc = (std::abs(scL) >= std::abs(scR)) ? scL : scR;
                dL = bands[b].processParallelDelta(
                    filtL, linkedSc, true, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutL);
                dR = bandsR[b].processParallelDelta(
                    filtR, linkedSc, true, p.splitIndependent[b],
                    p.freqs[b], p.qs[b], p.shapes[b], effStatic, effDyn, effMode,
                    p.thresholdsDb[b], p.rangesDb[b], effRatio, p.kneesDb[b],
                    effAtt, effRel, wEnvDb, p.grDelta, &sOutR);
            }

            if (p.bandActive[b]) {
                sumDeltaL += dL;
                sumDeltaR += dR;
                sumBandFiltL += sOutL;
                sumBandFiltR += sOutR;
            }
            if (p.soloBandIndex == static_cast<int>(b)) {
                soloL = sOutL;
                soloR = sOutR;
            }
        }

        const double compL = filtL - sumBandFiltL;
        const double compR = filtR - sumBandFiltR;
        double wetL = 0.0;
        double wetR = 0.0;
        if (p.soloBandIndex >= 0 && p.soloBandIndex < static_cast<int>(kNumBands)) {
            wetL = soloL;
            wetR = soloR;
        } else if (p.grDelta) {
            const double wStatic = dyn_detail::dbToGain(std::clamp(p.wBandGainDb, -24.0, 24.0));
            wetL = (wBandSmoothedGainL - wStatic) * compL + sumDeltaL;
            wetR = (wBandSmoothedGainL - wStatic) * compR + sumDeltaR;
        } else {
            wetL = compL * wBandSmoothedGainL + sumBandFiltL + sumDeltaL;
            wetR = compR * wBandSmoothedGainL + sumBandFiltR + sumDeltaR;
        }

        const double targetEqGain = dyn_detail::dbToGain(std::clamp(p.eqGainDb, -24.0, 24.0));
        const double targetDry = std::clamp(p.dryMix, 0.0, 1.0);
        const double targetOutGain = dyn_detail::dbToGain(std::clamp(p.outGainDb, -24.0, 24.0));

        smoothedEqGain = paramSmoothCoeff * smoothedEqGain + (1.0 - paramSmoothCoeff) * targetEqGain;
        smoothedDryMix = paramSmoothCoeff * smoothedDryMix + (1.0 - paramSmoothCoeff) * targetDry;
        smoothedOutGain = paramSmoothCoeff * smoothedOutGain + (1.0 - paramSmoothCoeff) * targetOutGain;

        double procL = ((wetL * smoothedEqGain) * (1.0 - smoothedDryMix) + safeL * smoothedDryMix) * smoothedOutGain;
        double procR = ((wetR * smoothedEqGain) * (1.0 - smoothedDryMix) + safeR * smoothedDryMix) * smoothedOutGain;

        const double inEnergy = 0.5 * (safeL * safeL + safeR * safeR);
        const double outEnergy = 0.5 * (procL * procL + procR * procR);
        inRmsSq = rmsCoeff * inRmsSq + (1.0 - rmsCoeff) * inEnergy;
        outRmsSq = rmsCoeff * outRmsSq + (1.0 - rmsCoeff) * outEnergy;
        const double targetEl = std::clamp(std::sqrt(inRmsSq / std::max(outRmsSq, 1.0e-12)), 0.25, 4.0);
        smoothedElGain = rmsCoeff * smoothedElGain + (1.0 - rmsCoeff) * targetEl;

        if (p.bypassed) {
            if (p.elBypass) {
                const double invEl = 1.0 / std::max(smoothedElGain, 0.25);
                outL = dyn_detail::sanitizeSample(safeL * invEl);
                outR = dyn_detail::sanitizeSample(safeR * invEl);
            } else {
                outL = safeL;
                outR = safeR;
            }
            return;
        }

        if (p.elBypass && p.soloBandIndex < 0 && !p.grDelta) {
            procL *= smoothedElGain;
            procR *= smoothedElGain;
        }

        outL = dyn_detail::sanitizeSample(procL);
        outR = dyn_detail::sanitizeSample(procR);
    }

private:
    void updateWBandTimeConstants(double attackMs, double releaseMs) noexcept {
        const double safeAtt = std::clamp(attackMs, 0.1, 200.0);
        const double safeRel = std::clamp(releaseMs, 5.0, 2000.0);
        if (safeAtt != wBandCachedAtt || safeRel != wBandCachedRel) {
            wBandCachedAtt = safeAtt;
            wBandCachedRel = safeRel;
            wBandAttCoeff = std::exp(-1.0 / (sampleRate * (safeAtt * 0.001)));
            wBandRelCoeff = std::exp(-1.0 / (sampleRate * (safeRel * 0.001)));
        }
    }

    double sampleRate = 48000.0;
    std::array<DynamicEqBand, kNumBands> bands{};
    std::array<DynamicEqBand, kNumBands> bandsR{};
    SerialSlopeFilter hpfL{}, hpfR{}, lpfL{}, lpfR{};

    double wBandEnvL = 0.0, wBandEnvR = 0.0;
    double wBandSmoothedGainL = 1.0, wBandSmoothedGainR = 1.0;
    double wBandCachedAtt = -1.0, wBandCachedRel = -1.0;
    double wBandAttCoeff = 0.99, wBandRelCoeff = 0.999;
    std::atomic<float> wBandDynGainDb{ 0.0f };

    double smoothedDryMix = 0.0;
    double smoothedOutGain = 1.0;
    double smoothedEqGain = 1.0;
    double paramSmoothCoeff = 0.995;

    double inRmsSq = 1.0e-6;
    double outRmsSq = 1.0e-6;
    double smoothedElGain = 1.0;
    double rmsCoeff = 0.99993;
};

} // namespace nova::graviton
