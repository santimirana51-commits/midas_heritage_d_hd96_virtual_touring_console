#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace nova::graviton {

// Phase 77: 31-Band Graphic Equaliser (Klark Teknik DN370 Emulation)
// Reference: MIDAS Heritage-D HD96-24 Chapter 14 (Graphic EQ)
class GraphicEQ final {
public:
    static constexpr size_t kNumBands = 31;
    static constexpr std::array<double, kNumBands> kIsoFrequencies = {
        20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0,
        200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0,
        2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0,
        20000.0
    };

    struct ChannelFilters {
        std::array<Biquad, kNumBands> bands;
        Biquad hpf;
        Biquad lpf;
        Biquad notch1;
        Biquad notch2;

        void reset() noexcept {
            for (auto& b : bands) b.reset();
            hpf.reset();
            lpf.reset();
            notch1.reset();
            notch2.reset();
        }
    };

    GraphicEQ() noexcept {
        reset();
    }

    void reset() noexcept {
        leftChannel.reset();
        rightChannel.reset();
        cachedSampleRate = 48000.0;
        isProportionalQ = true;
        hpfEnabled = false;
        hpfFreq = 20.0;
        lpfEnabled = false;
        lpfFreq = 20000.0;
        notch1Enabled = false;
        notch1Freq = 100.0;
        notch2Enabled = false;
        notch2Freq = 1000.0;
        bypassed = false;
        bandGainDb.fill(0.0f);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
            cachedSampleRate = sr;
            updateAllCoefficients();
        }
    }

    void setBandGain(size_t band, float gainDb) noexcept {
        if (band < kNumBands) {
            const float clamped = std::clamp(gainDb, -12.0f, 12.0f);
            if (std::abs(bandGainDb[band] - clamped) > 0.01f) {
                bandGainDb[band] = clamped;
                updateBandCoefficients(band);
            }
        }
    }

    float getBandGain(size_t band) const noexcept {
        return (band < kNumBands) ? bandGainDb[band] : 0.0f;
    }

    void setProportionalQ(bool propQ) noexcept {
        if (isProportionalQ != propQ) {
            isProportionalQ = propQ;
            updateAllCoefficients();
        }
    }

    bool getProportionalQ() const noexcept { return isProportionalQ; }

    void setHpf(bool enabled, double freqHz) noexcept {
        hpfEnabled = enabled;
        hpfFreq = std::clamp(freqHz, 20.0, 500.0);
        updateHpfCoefficients();
    }

    void setLpf(bool enabled, double freqHz) noexcept {
        lpfEnabled = enabled;
        lpfFreq = std::clamp(freqHz, 2000.0, 20000.0);
        updateLpfCoefficients();
    }

    void setNotch1(bool enabled, double freqHz) noexcept {
        notch1Enabled = enabled;
        notch1Freq = std::clamp(freqHz, 20.0, 2000.0);
        updateNotch1Coefficients();
    }

    void setNotch2(bool enabled, double freqHz) noexcept {
        notch2Enabled = enabled;
        notch2Freq = std::clamp(freqHz, 200.0, 20000.0);
        updateNotch2Coefficients();
    }

    void setBypassed(bool bypass) noexcept { bypassed = bypass; }
    bool isBypassed() const noexcept { return bypassed; }

    void flatten() noexcept {
        bandGainDb.fill(0.0f);
        updateAllCoefficients();
    }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        if (bypassed) return;

        double l = inOutL;
        double r = inOutR;

        // Filters: HPF & LPF
        if (hpfEnabled) {
            l = leftChannel.hpf.process(l);
            r = rightChannel.hpf.process(r);
        }
        if (lpfEnabled) {
            l = leftChannel.lpf.process(l);
            r = rightChannel.lpf.process(r);
        }

        // Notch filters (-17 dB narrow notches)
        if (notch1Enabled) {
            l = leftChannel.notch1.process(l);
            r = rightChannel.notch1.process(r);
        }
        if (notch2Enabled) {
            l = leftChannel.notch2.process(l);
            r = rightChannel.notch2.process(r);
        }

        // 31-Band graphic EQ filters
        for (size_t i = 0; i < kNumBands; ++i) {
            if (std::abs(bandGainDb[i]) > 0.01f) {
                l = leftChannel.bands[i].process(l);
                r = rightChannel.bands[i].process(r);
            }
        }

        inOutL = sanitize(l);
        inOutR = sanitize(r);
    }

private:
    void updateBandCoefficients(size_t band) noexcept {
        const double f0 = kIsoFrequencies[band];
        const double gainDb = static_cast<double>(bandGainDb[band]);
        // DN370 Proportional-Q: bandwidth tightens proportionally as boost/cut increases
        const double q = isProportionalQ
            ? (1.414 * (1.0 + 0.12 * std::abs(gainDb)))
            : 2.87; // Constant-Q 1/3-octave standard

        const auto coeffs = makePeakingEq(cachedSampleRate, f0, gainDb, q);
        leftChannel.bands[band].setCoefficients(coeffs);
        rightChannel.bands[band].setCoefficients(coeffs);
    }

    void updateAllCoefficients() noexcept {
        for (size_t i = 0; i < kNumBands; ++i)
            updateBandCoefficients(i);
        updateHpfCoefficients();
        updateLpfCoefficients();
        updateNotch1Coefficients();
        updateNotch2Coefficients();
    }

    void updateHpfCoefficients() noexcept {
        const auto coeffs = makeButterworthHPF(cachedSampleRate, hpfFreq, 0.7071067811865475);
        leftChannel.hpf.setCoefficients(coeffs);
        rightChannel.hpf.setCoefficients(coeffs);
    }

    void updateLpfCoefficients() noexcept {
        const auto coeffs = makeButterworthLPF(cachedSampleRate, lpfFreq, 0.7071067811865475);
        leftChannel.lpf.setCoefficients(coeffs);
        rightChannel.lpf.setCoefficients(coeffs);
    }

    void updateNotch1Coefficients() noexcept {
        // -17 dB surgical notch with Q = 8.0
        const auto coeffs = makePeakingEq(cachedSampleRate, notch1Freq, -17.0, 8.0);
        leftChannel.notch1.setCoefficients(coeffs);
        rightChannel.notch1.setCoefficients(coeffs);
    }

    void updateNotch2Coefficients() noexcept {
        const auto coeffs = makePeakingEq(cachedSampleRate, notch2Freq, -17.0, 8.0);
        leftChannel.notch2.setCoefficients(coeffs);
        rightChannel.notch2.setCoefficients(coeffs);
    }

    ChannelFilters leftChannel;
    ChannelFilters rightChannel;
    double cachedSampleRate = 48000.0;
    std::array<float, kNumBands> bandGainDb{};
    bool isProportionalQ = true;
    bool hpfEnabled = false;
    double hpfFreq = 20.0;
    bool lpfEnabled = false;
    double lpfFreq = 20000.0;
    bool notch1Enabled = false;
    double notch1Freq = 100.0;
    bool notch2Enabled = false;
    double notch2Freq = 1000.0;
    bool bypassed = false;
};

} // namespace nova::graviton
