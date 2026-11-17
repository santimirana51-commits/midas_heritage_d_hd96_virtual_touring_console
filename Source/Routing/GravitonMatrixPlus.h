#pragma once

#include "DSP/GravitonDSP.h"
#include "Effects/GravitonMasterX.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <string>

namespace nova::graviton {

// ==============================================================================
// Smart Acoustic Environmental Calculator (ISO 9613-1 / Cramer Model)
// Calculates speed of sound and converts distance <-> delay time with temperature
// and relative humidity compensation.
// ==============================================================================
struct AcousticEnvironment {
    double temperatureCelsius = 20.0; // -20.0 C to +50.0 C
    double relativeHumidityPct = 50.0; // 0% to 100% RH
    double airPressureHPa = 1013.25;    // Standard atmospheric pressure

    // Speed of sound in m/s: c = 331.3 * sqrt(1 + T / 273.15) * (1 + humidity_correction)
    double calculateSpeedOfSound() const noexcept {
        const double t = std::clamp(temperatureCelsius, -40.0, 60.0);
        const double rh = std::clamp(relativeHumidityPct, 0.0, 100.0);

        // Dry air velocity
        const double cDry = 331.3 * std::sqrt(1.0 + t / 273.15);

        // Saturation vapor pressure (Pa) via Tetens equation
        const double es = 611.21 * std::exp((17.502 * t) / (240.97 + t));

        // Humidity correction factor
        const double pPa = airPressureHPa * 100.0;
        const double humidityFactor = 1.0 + (0.16 * (rh * 0.01) * es) / pPa;

        return cDry * humidityFactor;
    }

    double calculateSpeedOfSoundFps() const noexcept {
        return calculateSpeedOfSound() * 3.28084; // m/s to ft/s
    }

    double distanceToDelayMs(double distanceMeters) const noexcept {
        const double c = calculateSpeedOfSound();
        return (c > 1.0) ? (std::max(0.0, distanceMeters) / c) * 1000.0 : 0.0;
    }

    double distanceFeetToDelayMs(double distanceFeet) const noexcept {
        return distanceToDelayMs(distanceFeet * 0.3048);
    }

    double delayMsToDistanceMeters(double delayMs) const noexcept {
        const double c = calculateSpeedOfSound();
        return (std::max(0.0, delayMs) * 0.001) * c;
    }

    double delayMsToDistanceFeet(double delayMs) const noexcept {
        return delayMsToDistanceMeters(delayMs) * 3.28084;
    }
};

// ==============================================================================
// Single Matrix Bus Speaker Alignment Channel
// Reference: MIDAS Heritage-D HD96-24 Chapter 29
//
// Key Features:
// - Up to 500.00 ms delay with sub-sample linear/fractional interpolation
// - Sub-millimeter spatial resolution (0.01 ms = ~3.4 mm)
// - Polarity invert (phase flip Ø)
// - Phase-alignment variable All-Pass Filter for sub-to-top array wavefront sync
// - Level trim gain (-24 dB to +12 dB) and mute
// - Live atomic peak meter
// ==============================================================================
class MatrixChannelAlignment final {
public:
    static constexpr size_t kBufferCapacity = 65536; // ~682 ms @ 96 kHz, ~1365 ms @ 48 kHz
    static constexpr size_t kBufferMask = kBufferCapacity - 1;

    MatrixChannelAlignment() noexcept {
        delayBuffer.assign(kBufferCapacity, 0.0);
        reset();
    }

    void reset() noexcept {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0);
        writePos = 0;
        peakMeter.store(0.0f, std::memory_order_relaxed);
        phaseAllPass.reset();
        updateFilter();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - cachedSampleRate.load(std::memory_order_relaxed)) > 1.0) {
            cachedSampleRate.store(sr, std::memory_order_relaxed);
            updateFilter();
        }
    }

    // Delay time in milliseconds (0.00 ms to 500.00 ms)
    void setDelayMs(double ms) noexcept {
        delayMs.store(std::clamp(ms, 0.0, 500.0), std::memory_order_relaxed);
    }
    double getDelayMs() const noexcept { return delayMs.load(std::memory_order_relaxed); }

    void setDelayEnabled(bool enabled) noexcept { delayEnabled.store(enabled, std::memory_order_relaxed); }
    bool isDelayEnabled() const noexcept { return delayEnabled.load(std::memory_order_relaxed); }

    // Polarity invert (Phase flip Ø)
    void setPolarityInvert(bool invert) noexcept { polarityInvert.store(invert, std::memory_order_relaxed); }
    bool getPolarityInvert() const noexcept { return polarityInvert.load(std::memory_order_relaxed); }

    // Crossover Phase Alignment All-Pass Filter
    void setAllPassEnabled(bool enabled) noexcept {
        allPassEnabled.store(enabled, std::memory_order_relaxed);
    }
    bool isAllPassEnabled() const noexcept { return allPassEnabled.load(std::memory_order_relaxed); }

    void setAllPassFrequency(double freqHz) noexcept {
        allPassFreqHz.store(std::clamp(freqHz, 20.0, 2500.0), std::memory_order_relaxed);
        updateFilter();
    }
    double getAllPassFrequency() const noexcept { return allPassFreqHz.load(std::memory_order_relaxed); }

    void setAllPassQ(double q) noexcept {
        allPassQ.store(std::clamp(q, 0.2, 5.0), std::memory_order_relaxed);
        updateFilter();
    }
    double getAllPassQ() const noexcept { return allPassQ.load(std::memory_order_relaxed); }

    // Gain Trim & Mute
    void setGainTrimDb(double db) noexcept { gainTrimDb.store(std::clamp(db, -24.0, 12.0), std::memory_order_relaxed); }
    double getGainTrimDb() const noexcept { return gainTrimDb.load(std::memory_order_relaxed); }

    void setMuted(bool mute) noexcept { muted.store(mute, std::memory_order_relaxed); }
    bool isMuted() const noexcept { return muted.load(std::memory_order_relaxed); }

    float getPeak() const noexcept { return peakMeter.load(std::memory_order_relaxed); }

    // Process a single audio sample through the Matrix Plus alignment chain
    double processSample(double input) noexcept {
        if (muted.load(std::memory_order_relaxed)) {
            peakMeter.store(0.0f, std::memory_order_relaxed);
            return 0.0;
        }

        double s = sanitize(input);

        // 1. Polarity Flip (Ø)
        if (polarityInvert.load(std::memory_order_relaxed)) {
            s = -s;
        }

        // 2. Sub-to-Top Crossover Phase Alignment All-Pass Filter
        if (allPassEnabled.load(std::memory_order_relaxed)) {
            s = phaseAllPass.process(s);
        }

        // 3. Write sample to delay ring buffer
        delayBuffer[writePos] = s;

        // 4. Fractional Delay with Sub-Sample Linear Interpolation
        double delayed = s;
        const double dMs = delayMs.load(std::memory_order_relaxed);
        if (delayEnabled.load(std::memory_order_relaxed) && dMs > 0.0001) {
            const double delaySamples = dMs * 0.001 * cachedSampleRate.load(std::memory_order_relaxed);
            const int intDelay = static_cast<int>(std::floor(delaySamples));
            const double fracDelay = delaySamples - static_cast<double>(intDelay);

            const size_t readPos0 = (writePos - static_cast<size_t>(intDelay)) & kBufferMask;
            const size_t readPos1 = (readPos0 - 1) & kBufferMask;

            const double s0 = delayBuffer[readPos0];
            const double s1 = delayBuffer[readPos1];

            delayed = s0 + fracDelay * (s1 - s0);
        }

        writePos = (writePos + 1) & kBufferMask;

        // 5. Gain Trim
        const double trimGain = dbToGain(gainTrimDb.load(std::memory_order_relaxed));
        const double out = sanitize(delayed * trimGain);

        // 6. Fast Attack / Slow Release Peak Metering
        const float absVal = static_cast<float>(std::abs(out));
        float currentPeak = peakMeter.load(std::memory_order_relaxed);
        currentPeak = std::max(absVal, currentPeak * 0.9992f);
        peakMeter.store(currentPeak, std::memory_order_relaxed);

        return out;
    }

private:
    void updateFilter() noexcept {
        const auto coeffs = makeAllPass2ndOrder(cachedSampleRate.load(std::memory_order_relaxed),
                                                allPassFreqHz.load(std::memory_order_relaxed),
                                                allPassQ.load(std::memory_order_relaxed));
        phaseAllPass.setCoefficients(coeffs);
    }

    std::atomic<double> cachedSampleRate{ 48000.0 };
    std::atomic<double> delayMs{ 0.0 };
    std::atomic<bool> delayEnabled{ false };
    std::atomic<bool> polarityInvert{ false };

    std::atomic<bool> allPassEnabled{ false };
    std::atomic<double> allPassFreqHz{ 85.0 }; // Typical Subwoofer to Line Array crossover (85 Hz)
    std::atomic<double> allPassQ{ 0.7071 };

    std::atomic<double> gainTrimDb{ 0.0 };
    std::atomic<bool> muted{ false };

    std::vector<double> delayBuffer;
    size_t writePos = 0;

    Biquad phaseAllPass;
    std::atomic<float> peakMeter{ 0.0f };
};

// ==============================================================================
// Matrix Plus & Speaker Delay Alignment Network Engine (24 Matrix Channels)
// Reference: MIDAS Heritage-D HD96-24 Chapter 29
// ==============================================================================
class MatrixPlusEngine final {
public:
    static constexpr size_t kNumMatrices = 24;

    MatrixPlusEngine() noexcept {
        reset();
    }

    void reset() noexcept {
        for (auto& ch : channels) ch.reset();
    }

    void setSampleRate(double sr) noexcept {
        for (auto& ch : channels) ch.setSampleRate(sr);
    }

    MatrixChannelAlignment& getChannel(size_t index) noexcept {
        return channels[index < kNumMatrices ? index : 0];
    }
    const MatrixChannelAlignment& getChannel(size_t index) const noexcept {
        return channels[index < kNumMatrices ? index : 0];
    }

    // Environmental Calculator
    AcousticEnvironment& getEnvironment() noexcept { return environment; }
    const AcousticEnvironment& getEnvironment() const noexcept { return environment; }

    void setTemperatureCelsius(double c) noexcept {
        environment.temperatureCelsius = std::clamp(c, -40.0, 60.0);
    }
    void setRelativeHumidity(double rh) noexcept {
        environment.relativeHumidityPct = std::clamp(rh, 0.0, 100.0);
    }

    // Apply distance to matrix channel using current environment speed of sound
    void applyDistanceMeters(size_t matrixIndex, double meters) noexcept {
        if (matrixIndex < kNumMatrices) {
            const double ms = environment.distanceToDelayMs(meters);
            channels[matrixIndex].setDelayMs(ms);
        }
    }

    void applyDistanceFeet(size_t matrixIndex, double feet) noexcept {
        if (matrixIndex < kNumMatrices) {
            const double ms = environment.distanceFeetToDelayMs(feet);
            channels[matrixIndex].setDelayMs(ms);
        }
    }

    // Preset configurations for typical touring sound reinforcement
    void applyTouringPreset(size_t matrixIndex, const std::string& presetName) noexcept {
        if (matrixIndex >= kNumMatrices) return;
        auto& ch = channels[matrixIndex];

        if (presetName == "FOH_MAIN_ARRAY") {
            ch.setDelayEnabled(false);
            ch.setDelayMs(0.0);
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(false);
            ch.setGainTrimDb(0.0);
        } else if (presetName == "SUBWOOFER_ARRAY") {
            ch.setDelayEnabled(true);
            applyDistanceMeters(matrixIndex, 1.8); // ~5.2 ms alignment to fly array
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(true);
            ch.setAllPassFrequency(80.0);
            ch.setAllPassQ(0.7071);
            ch.setGainTrimDb(0.0);
        } else if (presetName == "FRONTFILL") {
            ch.setDelayEnabled(true);
            applyDistanceMeters(matrixIndex, 4.5); // ~13.1 ms stage apron alignment
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(false);
            ch.setGainTrimDb(-3.0);
        } else if (presetName == "OUTFILL") {
            ch.setDelayEnabled(true);
            applyDistanceMeters(matrixIndex, 8.0); // ~23.3 ms
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(false);
            ch.setGainTrimDb(-2.0);
        } else if (presetName == "DELAY_TOWER_1") {
            ch.setDelayEnabled(true);
            applyDistanceMeters(matrixIndex, 35.0); // ~102 ms lawn/delay tower
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(false);
            ch.setGainTrimDb(0.0);
        } else if (presetName == "DELAY_TOWER_2") {
            ch.setDelayEnabled(true);
            applyDistanceMeters(matrixIndex, 70.0); // ~204 ms rear stadium tower
            ch.setPolarityInvert(false);
            ch.setAllPassEnabled(false);
            ch.setGainTrimDb(0.0);
        }
    }

    // Process a single sample for a specific matrix channel
    double processMatrixSample(size_t matrixIndex, double inputSample) noexcept {
        return (matrixIndex < kNumMatrices) ? channels[matrixIndex].processSample(inputSample) : inputSample;
    }

    // Process an entire frame of 24 matrix bus samples in-place
    template <size_t NumVisible>
    void processMatrixFrame(std::array<double, NumVisible>& matrixOutputs) noexcept {
        const size_t count = std::min(NumVisible, kNumMatrices);
        for (size_t m = 0; m < count; ++m) {
            matrixOutputs[m] = channels[m].processSample(matrixOutputs[m]);
        }
    }

private:
    std::array<MatrixChannelAlignment, kNumMatrices> channels;
    AcousticEnvironment environment;
};

} // namespace nova::graviton

