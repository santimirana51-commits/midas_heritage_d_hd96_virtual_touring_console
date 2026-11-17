#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace nova::graviton {

enum class MonitorSourceType : int {
    MainLR = 0,
    Aux = 1,
    Matrix = 2,
    TwoTrackInput = 3,
    Playback = 4,
    Oscillator = 5
};

struct MonitorStereoSample {
    double left = 0.0;
    double right = 0.0;
};

inline MonitorStereoSample selectMonitorSource(
    MonitorSourceType type, int index, double mainLeft, double mainRight,
    const std::array<double, 96>& aux,
    const std::array<double, 24>& matrix,
    double twoTrackLeft, double twoTrackRight,
    double playbackLeft, double playbackRight, double oscillator) noexcept {
    MonitorStereoSample result{};
    switch (type) {
    case MonitorSourceType::MainLR:
        result = {mainLeft, mainRight};
        break;
    case MonitorSourceType::Aux:
        if (index >= 0 && index < static_cast<int>(aux.size()))
            result = {aux[static_cast<size_t>(index)],
                      aux[static_cast<size_t>(index)]};
        break;
    case MonitorSourceType::Matrix:
        if (index >= 0 && index < static_cast<int>(matrix.size()))
            result = {matrix[static_cast<size_t>(index)],
                      matrix[static_cast<size_t>(index)]};
        break;
    case MonitorSourceType::TwoTrackInput:
        result = {twoTrackLeft, twoTrackRight};
        break;
    case MonitorSourceType::Playback:
        result = {playbackLeft, playbackRight};
        break;
    case MonitorSourceType::Oscillator:
        result = {oscillator, oscillator};
        break;
    }
    result.left = sanitize(result.left);
    result.right = sanitize(result.right);
    return result;
}

class MonitorEq final {
public:
    static constexpr size_t kBands = 4;

    MonitorEq() noexcept {
        for (size_t band = 0; band < kBands; ++band) {
            frequencyHz[band].store(defaultFrequency(band),
                                    std::memory_order_relaxed);
            gainDb[band].store(0.0f, std::memory_order_relaxed);
            q[band].store(0.7f, std::memory_order_relaxed);
            enabled[band].store(false, std::memory_order_relaxed);
        }
    }

    void prepare(double sampleRate) noexcept {
        const double sr = std::clamp(sampleRate, 1000.0, 768000.0);
        const bool srChanged = (std::abs(sr - cachedSr) > 0.5);
        cachedSr = sr;
        for (size_t band = 0; band < kBands; ++band) {
            const double frequency = std::clamp(
                static_cast<double>(frequencyHz[band].load(
                    std::memory_order_relaxed)),
                20.0, sr * 0.45);
            const double gain = std::clamp(
                static_cast<double>(gainDb[band].load(
                    std::memory_order_relaxed)),
                -18.0, 18.0);
            const double quality = std::clamp(
                static_cast<double>(q[band].load(std::memory_order_relaxed)),
                0.1, 10.0);
            if (srChanged ||
                std::abs(frequency - cachedFreq[band]) > 1.0e-3 ||
                std::abs(gain - cachedGain[band]) > 1.0e-3 ||
                std::abs(quality - cachedQ[band]) > 1.0e-3) {
                cachedFreq[band] = frequency;
                cachedGain[band] = gain;
                cachedQ[band] = quality;
                const auto coeffs = makePeakingEq(sr, frequency, gain, quality);
                filtersLeft[band].setCoefficients(coeffs);
                filtersRight[band].setCoefficients(coeffs);
            }
        }
    }

    void process(double& left, double& right) noexcept {
        for (size_t band = 0; band < kBands; ++band) {
            if (!enabled[band].load(std::memory_order_relaxed))
                continue;
            left = filtersLeft[band].process(left);
            right = filtersRight[band].process(right);
        }
    }

    static float defaultFrequency(size_t band) noexcept {
        constexpr std::array<float, kBands> defaults{100.0f, 500.0f,
                                                     2000.0f, 8000.0f};
        return defaults[std::min(band, kBands - 1)];
    }

    std::array<std::atomic<float>, kBands> frequencyHz{};
    std::array<std::atomic<float>, kBands> gainDb{};
    std::array<std::atomic<float>, kBands> q{};
    std::array<std::atomic<bool>, kBands> enabled{};

private:
    double cachedSr = 0.0;
    std::array<double, kBands> cachedFreq{};
    std::array<double, kBands> cachedGain{};
    std::array<double, kBands> cachedQ{};
    std::array<Biquad, kBands> filtersLeft{};
    std::array<Biquad, kBands> filtersRight{};
};

} // namespace nova::graviton
