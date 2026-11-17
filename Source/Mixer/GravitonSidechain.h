#pragma once

#include "DSP/GravitonDSP.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>

namespace nova::graviton {

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapter 17: Frequency-Conscious Key Sidechain
// Flexible Sidechain Key Filter (HPF, LPF, Bandpass, Notch) & External Key Routing
// ==============================================================================

enum class KeyFilterType : int {
    Bypass = 0,
    HPF = 1,
    LPF = 2,
    Bandpass = 3,
    Notch = 4
};

enum class KeySourceType : int {
    Internal = 0,
    External = 1
};

class SidechainKeyFilter final {
public:
    SidechainKeyFilter() noexcept {
        reset();
    }

    void reset() noexcept {
        s1 = 0.0;
        s2 = 0.0;
        b0 = 1.0;
        b1 = 0.0;
        b2 = 0.0;
        a1 = 0.0;
        a2 = 0.0;
        cachedType = KeyFilterType::Bypass;
        cachedFreq = -1.0;
        cachedQ = -1.0;
        cachedSampleRate = -1.0;
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
            sampleRate = sr;
            cachedFreq = -1.0; // Force recalculation
        }
    }

    double processSample(double input, KeyFilterType type, double freqHz, double q) noexcept {
        if (type == KeyFilterType::Bypass) {
            return sanitize(input);
        }

        updateCoefficientsIfNeeded(type, freqHz, q);

        // Direct Form II Transposed biquad
        const double out = b0 * input + s1;
        s1 = b1 * input - a1 * out + s2;
        s2 = b2 * input - a2 * out;

        return sanitize(out);
    }

private:
    void updateCoefficientsIfNeeded(KeyFilterType type, double freqHz, double qVal) noexcept {
        const double clampedFreq = std::clamp(freqHz, 20.0, sampleRate * 0.45);
        const double clampedQ = std::clamp(qVal, 0.1, 10.0);

        if (type == cachedType && clampedFreq == cachedFreq && clampedQ == cachedQ && sampleRate == cachedSampleRate) {
            return;
        }

        cachedType = type;
        cachedFreq = clampedFreq;
        cachedQ = clampedQ;
        cachedSampleRate = sampleRate;

        constexpr double pi = 3.14159265358979323846;
        const double omega = 2.0 * pi * clampedFreq / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * clampedQ);

        switch (type) {
            case KeyFilterType::HPF: {
                const double a0 = 1.0 + alpha;
                b0 = ((1.0 + cs) / 2.0) / a0;
                b1 = (-(1.0 + cs)) / a0;
                b2 = ((1.0 + cs) / 2.0) / a0;
                a1 = (-2.0 * cs) / a0;
                a2 = (1.0 - alpha) / a0;
                break;
            }
            case KeyFilterType::LPF: {
                const double a0 = 1.0 + alpha;
                b0 = ((1.0 - cs) / 2.0) / a0;
                b1 = (1.0 - cs) / a0;
                b2 = ((1.0 - cs) / 2.0) / a0;
                a1 = (-2.0 * cs) / a0;
                a2 = (1.0 - alpha) / a0;
                break;
            }
            case KeyFilterType::Bandpass: {
                // Constant 0 dB peak gain bandpass
                const double a0 = 1.0 + alpha;
                b0 = alpha / a0;
                b1 = 0.0;
                b2 = -alpha / a0;
                a1 = (-2.0 * cs) / a0;
                a2 = (1.0 - alpha) / a0;
                break;
            }
            case KeyFilterType::Notch: {
                const double a0 = 1.0 + alpha;
                b0 = 1.0 / a0;
                b1 = (-2.0 * cs) / a0;
                b2 = 1.0 / a0;
                a1 = (-2.0 * cs) / a0;
                a2 = (1.0 - alpha) / a0;
                break;
            }
            default: {
                b0 = 1.0; b1 = 0.0; b2 = 0.0;
                a1 = 0.0; a2 = 0.0;
                break;
            }
        }
    }

    double sampleRate = 48000.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1 = 0.0, s2 = 0.0;
    KeyFilterType cachedType = KeyFilterType::Bypass;
    double cachedFreq = -1.0;
    double cachedQ = -1.0;
    double cachedSampleRate = -1.0;
};

inline double processSidechainKeySample(
    SidechainKeyFilter& filter,
    KeySourceType sourceType,
    double channelSignal,
    double externalSignal,
    KeyFilterType filterType,
    double frequencyHz,
    double q) noexcept
{
    const double source = sourceType == KeySourceType::External
        ? externalSignal
        : channelSignal;
    return filter.processSample(sanitize(source), filterType, frequencyHz, q);
}

} // namespace nova::graviton
