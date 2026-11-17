#pragma once

// Graviton Mk 2: allocation-free, double-precision DSP primitives.
// Coefficient design and parameter conversion belong on the control/setup path;
// sample-processing methods perform bounded arithmetic only.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace nova::graviton
{
using Sample = double;
inline constexpr int kEngineMajor = 2;
inline constexpr int kSamplePrecisionBits = 64;
inline constexpr const char* kEngineName = "Graviton Mk 2 Processing Engine";

inline constexpr Sample kSilenceThreshold = 1.0e-30;

inline Sample sanitize(Sample value) noexcept
{
    return std::isfinite(value) ? value : 0.0;
}

inline Sample clampFinite(Sample value, Sample low, Sample high, Sample fallback = 0.0) noexcept
{
    if (!std::isfinite(value)) value = fallback;
    if (!std::isfinite(low) || !std::isfinite(high) || low > high) return fallback;
    return std::clamp(value, low, high);
}

inline Sample dbToGain(Sample decibels) noexcept
{
    if (!std::isfinite(decibels)) return 0.0;
    if (decibels <= -600.0) return 0.0;
    return std::pow(10.0, std::clamp(decibels, -600.0, 120.0) / 20.0);
}

inline Sample gainToDb(Sample gain, Sample floorDb = -120.0) noexcept
{
    if (!std::isfinite(gain) || gain <= 0.0) return floorDb;
    return std::max(floorDb, 20.0 * std::log10(gain));
}

// Explicit conversion at the float-only device/output boundary. Keeps finite double
// accumulators from overflowing to +/-infinity when narrowed to float.
inline float toFloatBoundary(Sample value) noexcept
{
    if (!std::isfinite(value)) return 0.0f;
    constexpr Sample maxFloat = static_cast<Sample>(std::numeric_limits<float>::max());
    return static_cast<float>(std::clamp(value, -maxFloat, maxFloat));
}

struct StereoFrame
{
    Sample left = 0.0;
    Sample right = 0.0;
};

// One-pole, sample-by-sample linear gain ramp. No allocation, locks, or logging.
class GainSmoother
{
public:
    void prepare(Sample sampleRate, Sample rampMilliseconds, Sample initialGain = 1.0) noexcept
    {
        const Sample sr = std::isfinite(sampleRate) ? std::clamp(sampleRate, 1.0, 768000.0) : 48000.0;
        const Sample ms = std::isfinite(rampMilliseconds) ? std::clamp(rampMilliseconds, 0.0, 10000.0) : 10.0;
        rampSamples_ = std::max<Sample>(1.0, sr * ms / 1000.0);
        current_ = target_ = std::isfinite(initialGain) ? initialGain : 0.0;
        step_ = 0.0;
    }

    void setTargetGain(Sample target) noexcept
    {
        target_ = std::isfinite(target) ? target : 0.0;
        step_ = (target_ - current_) / rampSamples_;
        if (std::abs(target_ - current_) <= std::numeric_limits<Sample>::epsilon())
        {
            current_ = target_;
            step_ = 0.0;
        }
    }

    void setTargetDb(Sample targetDb) noexcept { setTargetGain(dbToGain(targetDb)); }

    Sample next() noexcept
    {
        if (step_ != 0.0)
        {
            const Sample remaining = target_ - current_;
            if (std::abs(remaining) <= std::abs(step_))
            {
                current_ = target_;
                step_ = 0.0;
            }
            else current_ += step_;
        }
        return sanitize(current_);
    }

    Sample current() const noexcept { return current_; }
    Sample target() const noexcept { return target_; }
    bool isSmoothing() const noexcept { return step_ != 0.0; }

private:
    Sample current_ = 1.0;
    Sample target_ = 1.0;
    Sample step_ = 0.0;
    Sample rampSamples_ = 480.0;
};

struct BiquadCoefficients
{
    Sample b0 = 1.0, b1 = 0.0, b2 = 0.0;
    Sample a1 = 0.0, a2 = 0.0;
};

inline BiquadCoefficients makePeakingEq(Sample sampleRate, Sample frequency,
                                         Sample gainDb, Sample q) noexcept
{
    BiquadCoefficients unity{};
    if (!std::isfinite(sampleRate) || !std::isfinite(frequency)
        || !std::isfinite(gainDb) || !std::isfinite(q)
        || sampleRate < 1000.0 || frequency <= 0.0 || q <= 0.0)
        return unity;

    const Sample sr = std::clamp(sampleRate, 1000.0, 768000.0);
    const Sample f = std::clamp(frequency, 1.0, sr * 0.499);
    const Sample g = std::clamp(gainDb, -48.0, 48.0);
    const Sample quality = std::clamp(q, 0.05, 50.0);
    const Sample A = std::pow(10.0, g / 40.0);
    const Sample omega = 2.0 * 3.14159265358979323846 * f / sr;
    const Sample alpha = std::sin(omega) / (2.0 * quality);
    const Sample cosine = std::cos(omega);
    const Sample a0 = 1.0 + alpha / A;
    if (!std::isfinite(a0) || std::abs(a0) < 1.0e-20) return unity;

    BiquadCoefficients c;
    c.b0 = (1.0 + alpha * A) / a0;
    c.b1 = (-2.0 * cosine) / a0;
    c.b2 = (1.0 - alpha * A) / a0;
    c.a1 = (-2.0 * cosine) / a0;
    c.a2 = (1.0 - alpha / A) / a0;
    if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2)
        || !std::isfinite(c.a1) || !std::isfinite(c.a2)) return unity;
    return c;
}

inline BiquadCoefficients makeButterworthHPF(Sample sampleRate, Sample cutoffHz,
                                            Sample q = 0.70710678118654752440) noexcept
{
    BiquadCoefficients unity{};
    if (!std::isfinite(sampleRate) || !std::isfinite(cutoffHz) || !std::isfinite(q)
        || sampleRate < 1000.0 || cutoffHz <= 0.0 || q <= 0.0)
        return unity;

    const Sample sr = std::clamp(sampleRate, 1000.0, 768000.0);
    const Sample f = std::clamp(cutoffHz, 5.0, sr * 0.499);
    const Sample quality = std::clamp(q, 0.05, 50.0);
    const Sample omega = 2.0 * 3.14159265358979323846 * f / sr;
    const Sample sn = std::sin(omega);
    const Sample cs = std::cos(omega);
    const Sample alpha = sn / (2.0 * quality);
    const Sample a0 = 1.0 + alpha;
    if (!std::isfinite(a0) || std::abs(a0) < 1.0e-20) return unity;

    BiquadCoefficients c;
    c.b0 = ((1.0 + cs) * 0.5) / a0;
    c.b1 = (-(1.0 + cs)) / a0;
    c.b2 = ((1.0 + cs) * 0.5) / a0;
    c.a1 = (-2.0 * cs) / a0;
    c.a2 = (1.0 - alpha) / a0;
    if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2)
        || !std::isfinite(c.a1) || !std::isfinite(c.a2)) return unity;
    return c;
}

inline BiquadCoefficients makeButterworthLPF(Sample sampleRate, Sample cutoffHz,
                                            Sample q = 0.70710678118654752440) noexcept
{
    BiquadCoefficients unity{};
    if (!std::isfinite(sampleRate) || !std::isfinite(cutoffHz) || !std::isfinite(q)
        || sampleRate < 1000.0 || cutoffHz <= 0.0 || q <= 0.0)
        return unity;

    const Sample sr = std::clamp(sampleRate, 1000.0, 768000.0);
    const Sample f = std::clamp(cutoffHz, 5.0, sr * 0.499);
    const Sample quality = std::clamp(q, 0.05, 50.0);
    const Sample omega = 2.0 * 3.14159265358979323846 * f / sr;
    const Sample sn = std::sin(omega);
    const Sample cs = std::cos(omega);
    const Sample alpha = sn / (2.0 * quality);
    const Sample a0 = 1.0 + alpha;
    if (!std::isfinite(a0) || std::abs(a0) < 1.0e-20) return unity;

    BiquadCoefficients c;
    c.b0 = ((1.0 - cs) * 0.5) / a0;
    c.b1 = (1.0 - cs) / a0;
    c.b2 = ((1.0 - cs) * 0.5) / a0;
    c.a1 = (-2.0 * cs) / a0;
    c.a2 = (1.0 - alpha) / a0;
    if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2)
        || !std::isfinite(c.a1) || !std::isfinite(c.a2)) return unity;
    return c;
}

// First-order high-pass filter with double-precision coefficient and delay state.
// Configure outside the sample loop; process() performs bounded arithmetic only.
class OnePoleHighPass
{
public:
    bool prepare(Sample sampleRate, Sample cutoffHz) noexcept
    {
        if (!std::isfinite(sampleRate) || !std::isfinite(cutoffHz)
            || sampleRate < 1000.0 || cutoffHz <= 0.0)
        {
            coefficient_ = 0.0;
            reset();
            return false;
        }
        const Sample sr = std::clamp(sampleRate, 1000.0, 768000.0);
        const Sample cutoff = std::clamp(cutoffHz, 1.0, sr * 0.45);
        coefficient_ = std::exp(-2.0 * 3.14159265358979323846 * cutoff / sr);
        if (!std::isfinite(coefficient_)) coefficient_ = 0.0;
        return true;
    }

    Sample process(Sample input) noexcept
    {
        const Sample x = sanitize(input);
        const Sample y = coefficient_ * (previousOutput_ + x - previousInput_);
        previousInput_ = flush(x);
        previousOutput_ = flush(sanitize(y));
        return previousOutput_;
    }

    void reset() noexcept { previousInput_ = previousOutput_ = 0.0; }
    Sample coefficient() const noexcept { return coefficient_; }

private:
    static Sample flush(Sample value) noexcept
    {
        return std::abs(value) < kSilenceThreshold ? 0.0 : value;
    }
    Sample coefficient_ = 0.0;
    Sample previousInput_ = 0.0;
    Sample previousOutput_ = 0.0;
};

// Direct Form I with double-precision state; suitable for real-time sample loops.
class Biquad
{
public:
    void setCoefficients(const BiquadCoefficients& c) noexcept
    {
        if (std::isfinite(c.b0) && std::isfinite(c.b1) && std::isfinite(c.b2)
            && std::isfinite(c.a1) && std::isfinite(c.a2)) coeffs_ = c;
        else coeffs_ = {};
    }

    Sample process(Sample input) noexcept
    {
        const Sample x = sanitize(input);
        Sample y = coeffs_.b0 * x + coeffs_.b1 * x1_ + coeffs_.b2 * x2_
                 - coeffs_.a1 * y1_ - coeffs_.a2 * y2_;
        if (!std::isfinite(y)) y = 0.0;
        x2_ = flush(x1_); x1_ = flush(x);
        y2_ = flush(y1_); y1_ = flush(y);
        return y;
    }

    void reset() noexcept { x1_ = x2_ = y1_ = y2_ = 0.0; }
    const BiquadCoefficients& coefficients() const noexcept { return coeffs_; }

private:
    static Sample flush(Sample value) noexcept { return std::abs(value) < kSilenceThreshold ? 0.0 : value; }
    BiquadCoefficients coeffs_{};
    Sample x1_ = 0.0, x2_ = 0.0, y1_ = 0.0, y2_ = 0.0;
};

// Double-precision one-pole smoother for production channel-fader state.
// The coefficient is computed once per block; this function is bounded arithmetic.
inline Sample smoothGainStep(Sample current, Sample target, Sample coefficient) noexcept
{
    if (!std::isfinite(target)) target = 0.0;
    if (!std::isfinite(current)) current = target;
    if (!std::isfinite(coefficient)) coefficient = 1.0;
    coefficient = std::clamp(coefficient, 0.0, 1.0);
    return sanitize(current + (target - current) * coefficient);
}

// One-pole smoothing coefficient computed in double precision. Calculate once per
// audio block from the active sample rate; use smoothGainStep() per sample.
inline Sample gainSmoothingCoefficient(Sample sampleRate, Sample timeSeconds) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0
        || !std::isfinite(timeSeconds) || timeSeconds <= 0.0)
        return 1.0;
    const Sample sr = std::clamp(sampleRate, 1.0, 768000.0);
    const Sample time = std::clamp(timeSeconds, 1.0e-6, 60.0);
    const Sample coefficient = 1.0 - std::exp(-1.0 / (time * sr));
    return std::isfinite(coefficient) ? std::clamp(coefficient, 0.0, 1.0) : 1.0;
}

// Equal-power pan gains. Trigonometry is intentionally outside per-channel
// sample loops by caching the result once per channel per block.
inline StereoFrame equalPowerPanGains(Sample pan) noexcept
{
    const Sample p = clampFinite(pan, -1.0, 1.0, 0.0);
    constexpr Sample quarterPi = 0.7853981633974483096156608458198757;
    return { std::cos((p + 1.0) * quarterPi), std::sin((p + 1.0) * quarterPi) };
}

inline void accumulate(StereoFrame& destination, Sample left, Sample right) noexcept
{
    destination.left = sanitize(destination.left + sanitize(left));
    destination.right = sanitize(destination.right + sanitize(right));
}

// Mono bus accumulation and master stage for AUX/Matrix internals. The caller
// converts to float only when handing samples to the existing JUCE/router boundary.
inline void accumulateBus(Sample& destination, Sample sample, Sample gain) noexcept
{
    if (!std::isfinite(sample) || !std::isfinite(gain) || gain <= 0.0) return;
    destination = sanitize(destination + sample * gain);
}

inline Sample applyBusMaster(Sample sample, Sample masterGain, bool muted) noexcept
{
    if (muted || !std::isfinite(sample) || !std::isfinite(masterGain)) return 0.0;
    return sanitize(sample * std::clamp(masterGain, 0.0, 16.0));
}

struct MatrixOutputs
{
    Sample matrix1 = 0.0; // mono sum of double-precision Main L/R, then master gain
    Sample matrix2 = 0.0; // sum of double-precision mastered AUX buses, then master gain
};

// Graviton Matrix bus core. Inputs and accumulation stay double until the
// existing float-only physical output/router boundary. Allocation-free.
inline MatrixOutputs mixMatrixOutputs(Sample mainLeft, Sample mainRight,
                                      const std::array<Sample, 4>& masteredAux,
                                      Sample matrix1Master, bool matrix1Muted,
                                      Sample matrix2Master, bool matrix2Muted) noexcept
{
    Sample auxSum = 0.0;
    for (Sample sample : masteredAux)
        auxSum = sanitize(auxSum + sanitize(sample));

    const Sample mainMono = sanitize(0.5 * (sanitize(mainLeft) + sanitize(mainRight)));
    return { applyBusMaster(mainMono, matrix1Master, matrix1Muted),
             applyBusMaster(auxSum, matrix2Master, matrix2Muted) };
}
// Phase 76: Variable Phase All-Pass Filter Engine (MIDAS Heritage-D Chapter 29)
// Provides continuous 0-90 or 0-180 degree acoustic phase adjustment with flat magnitude.
struct VariableAllPass {
    double x1_a = 0.0, y1_a = 0.0;
    double x1_b = 0.0, y1_b = 0.0;
    double coeff = 0.0;

    void update(double sampleRate, double freqHz, double degrees, double maxDegrees) noexcept {
        const double sr = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 48000.0;
        const double safeDeg = std::isfinite(degrees) ? degrees : 0.0;
        const double safeFreq = std::isfinite(freqHz) ? freqHz : 1000.0;
        const double normDeg = std::clamp(safeDeg / std::max(1.0, maxDegrees), 0.0, 1.0);
        // Vary effective center frequency to shift acoustic phase while keeping pure all-pass response
        const double f = std::clamp(safeFreq * std::pow(4.0, (normDeg - 0.5) * 2.0), 20.0, sr * 0.45);
        const double w0 = 2.0 * 3.14159265358979323846 * f / sr;
        const double tanHalf = std::tan(w0 * 0.5);
        coeff = sanitize((tanHalf - 1.0) / (tanHalf + 1.0));
    }

    double process(double in, double degrees) noexcept {
        in = sanitize(in);
        if (!std::isfinite(degrees) || degrees <= 0.001)
            return in;
        // Two 1st-order all-pass stages in cascade: pure all-pass with exactly 1.0 magnitude
        const double y_a = sanitize(coeff * in + x1_a - coeff * y1_a);
        x1_a = in;
        y1_a = y_a;

        const double y_b = sanitize(coeff * y_a + x1_b - coeff * y1_b);
        x1_b = y_a;
        y1_b = y_b;

        return y_b;
    }

    void reset() noexcept {
        x1_a = 0.0; y1_a = 0.0;
        x1_b = 0.0; y1_b = 0.0;
    }
};

} // namespace nova::graviton
