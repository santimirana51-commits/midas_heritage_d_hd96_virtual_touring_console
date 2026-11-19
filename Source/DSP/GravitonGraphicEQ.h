#pragma once

#include "DSP/GravitonDSP.h"
#include "DSP/GravitonAtomicParameterSnapshot.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <mutex>

namespace nova::graviton {

// Phase 77: 31-Band Graphic Equaliser (Klark Teknik DN370 Emulation).
// GUI/control writers publish a coherent snapshot; the audio thread alone
// updates and owns all biquad coefficients and delay state.
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
        Biquad hpf, lpf, notch1, notch2;
        void reset() noexcept {
            for (auto& b : bands) b.reset();
            hpf.reset(); lpf.reset(); notch1.reset(); notch2.reset();
        }
    };

    struct Parameters {
        std::array<float, kNumBands> bandGainDb{};
        uint64_t revision = 1;
        bool proportionalQ = true;
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

    GraphicEQ() noexcept {
        controlParams_ = Parameters{};
        publishControlParams();
        audioParams_ = controlParams_;
        updateAllCoefficients(audioParams_);
        audioRevision_ = audioParams_.revision;
    }

    // Call only while the audio callback is stopped.
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        leftChannel.reset();
        rightChannel.reset();
        controlParams_ = Parameters{};
        ++controlParams_.revision;
        publishControlParams();
        audioParams_ = controlParams_;
        cachedSampleRate_ = 48000.0;
        updateAllCoefficients(audioParams_);
        audioRevision_ = audioParams_.revision;
    }

    // Audio-device lifecycle callback; no allocation or locking.
    void setSampleRate(double sr) noexcept {
        if (std::isfinite(sr) && sr > 1000.0 &&
            std::abs(sr - cachedSampleRate_) > 1.0) {
            cachedSampleRate_ = sr;
            updateAllCoefficients(audioParams_);
        }
    }

    void setBandGain(size_t band, float gainDb) noexcept {
        if (band >= kNumBands) return;
        std::lock_guard<std::mutex> lock(controlMutex_);
        const float value = std::isfinite(gainDb)
            ? std::clamp(gainDb, -12.0f, 12.0f) : 0.0f;
        if (std::abs(controlParams_.bandGainDb[band] - value) > 0.01f) {
            controlParams_.bandGainDb[band] = value;
            publishControlParams();
        }
    }

    float getBandGain(size_t band) const noexcept {
        if (band >= kNumBands) return 0.0f;
        std::lock_guard<std::mutex> lock(controlMutex_);
        return controlParams_.bandGainDb[band];
    }

    void setProportionalQ(bool value) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        if (controlParams_.proportionalQ != value) {
            controlParams_.proportionalQ = value;
            publishControlParams();
        }
    }
    bool getProportionalQ() const noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        return controlParams_.proportionalQ;
    }

    void setHpf(bool enabled, double freqHz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.hpfEnabled = enabled;
        controlParams_.hpfFreq = std::isfinite(freqHz)
            ? std::clamp(freqHz, 20.0, 500.0) : 20.0;
        publishControlParams();
    }
    void setLpf(bool enabled, double freqHz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.lpfEnabled = enabled;
        controlParams_.lpfFreq = std::isfinite(freqHz)
            ? std::clamp(freqHz, 2000.0, 20000.0) : 20000.0;
        publishControlParams();
    }
    void setNotch1(bool enabled, double freqHz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.notch1Enabled = enabled;
        controlParams_.notch1Freq = std::isfinite(freqHz)
            ? std::clamp(freqHz, 20.0, 2000.0) : 100.0;
        publishControlParams();
    }
    void setNotch2(bool enabled, double freqHz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.notch2Enabled = enabled;
        controlParams_.notch2Freq = std::isfinite(freqHz)
            ? std::clamp(freqHz, 200.0, 20000.0) : 1000.0;
        publishControlParams();
    }
    void setBypassed(bool value) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.bypassed = value;
        publishControlParams();
    }
    bool isBypassed() const noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        return controlParams_.bypassed;
    }

    void flatten() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlParams_.bandGainDb.fill(0.0f);
        publishControlParams();
    }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        Parameters latest{};
        if (publishedParams_.load(latest) && latest.revision != audioRevision_) {
            audioParams_ = latest;
            updateAllCoefficients(audioParams_);
            audioRevision_ = audioParams_.revision;
        }
        const auto& p = audioParams_;
        if (p.bypassed) return;

        double l = inOutL, r = inOutR;
        if (p.hpfEnabled) {
            l = leftChannel.hpf.process(l);
            r = rightChannel.hpf.process(r);
        }
        if (p.lpfEnabled) {
            l = leftChannel.lpf.process(l);
            r = rightChannel.lpf.process(r);
        }
        if (p.notch1Enabled) {
            l = leftChannel.notch1.process(l);
            r = rightChannel.notch1.process(r);
        }
        if (p.notch2Enabled) {
            l = leftChannel.notch2.process(l);
            r = rightChannel.notch2.process(r);
        }
        for (size_t i = 0; i < kNumBands; ++i) {
            if (std::abs(p.bandGainDb[i]) > 0.01f) {
                l = leftChannel.bands[i].process(l);
                r = rightChannel.bands[i].process(r);
            }
        }
        inOutL = sanitize(l);
        inOutR = sanitize(r);
    }

private:
    void publishControlParams() noexcept {
        ++controlParams_.revision;
        publishedParams_.publish(controlParams_);
    }

    void updateBandCoefficients(size_t band, const Parameters& p) noexcept {
        const double f0 = kIsoFrequencies[band];
        const double gainDb = static_cast<double>(p.bandGainDb[band]);
        const double q = p.proportionalQ
            ? (1.414 * (1.0 + 0.12 * std::abs(gainDb))) : 2.87;
        const auto coeffs = makePeakingEq(cachedSampleRate_, f0, gainDb, q);
        leftChannel.bands[band].setCoefficients(coeffs);
        rightChannel.bands[band].setCoefficients(coeffs);
    }

    void updateAllCoefficients(const Parameters& p) noexcept {
        for (size_t i = 0; i < kNumBands; ++i)
            updateBandCoefficients(i, p);
        const auto hpf = makeButterworthHPF(cachedSampleRate_, p.hpfFreq, 0.7071067811865475);
        const auto lpf = makeButterworthLPF(cachedSampleRate_, p.lpfFreq, 0.7071067811865475);
        const auto n1 = makePeakingEq(cachedSampleRate_, p.notch1Freq, -17.0, 8.0);
        const auto n2 = makePeakingEq(cachedSampleRate_, p.notch2Freq, -17.0, 8.0);
        leftChannel.hpf.setCoefficients(hpf); rightChannel.hpf.setCoefficients(hpf);
        leftChannel.lpf.setCoefficients(lpf); rightChannel.lpf.setCoefficients(lpf);
        leftChannel.notch1.setCoefficients(n1); rightChannel.notch1.setCoefficients(n1);
        leftChannel.notch2.setCoefficients(n2); rightChannel.notch2.setCoefficients(n2);
    }

    mutable std::mutex controlMutex_;
    Parameters controlParams_{};
    AtomicParameterSnapshot<Parameters> publishedParams_;
    Parameters audioParams_{};
    uint64_t audioRevision_ = 0;
    ChannelFilters leftChannel, rightChannel;
    double cachedSampleRate_ = 48000.0;
};

} // namespace nova::graviton
