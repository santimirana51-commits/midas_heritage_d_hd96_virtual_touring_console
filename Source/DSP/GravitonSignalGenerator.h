#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <numbers>

namespace nova::graviton {

enum class GeneratorType : int {
    Sine = 0,
    PinkNoise = 1,
    WhiteNoise = 2
};

// 64-bit real-time Signal Generator for console alignment, acoustic measurement, and bus testing.
class SignalGenerator final {
public:
    constexpr SignalGenerator() noexcept = default;

    void reset(double sampleRate = 48000.0) noexcept {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        phase = 0.0;
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0;
        rngState = 0x853c49e6748fea9bULL;
    }

    void setSampleRate(double sampleRate) noexcept {
        if (sampleRate > 1000.0)
            currentSampleRate = sampleRate;
    }

    // Generate a single 64-bit float audio sample
    double renderSample(GeneratorType type, double frequencyHz, double gainLinear) noexcept {
        if (gainLinear <= 1.0e-5)
            return 0.0;

        double raw = 0.0;
        switch (type) {
            case GeneratorType::Sine: {
                const double safeFreq = std::clamp(frequencyHz, 20.0, 20000.0);
                raw = std::sin(phase * 2.0 * std::numbers::pi);
                phase += safeFreq / currentSampleRate;
                if (phase >= 1.0)
                    phase -= std::floor(phase);
                break;
            }
            case GeneratorType::WhiteNoise: {
                raw = nextUniformRandom();
                break;
            }
            case GeneratorType::PinkNoise: {
                const double white = nextUniformRandom();
                // Paul Kellet's refined 7-stage filter approximation for -3 dB/octave pink noise
                b0 = 0.99886 * b0 + white * 0.0555179;
                b1 = 0.99332 * b1 + white * 0.0750759;
                b2 = 0.96900 * b2 + white * 0.1538520;
                b3 = 0.86650 * b3 + white * 0.3104856;
                b4 = 0.55000 * b4 + white * 0.5329522;
                b5 = -0.7616 * b5 - white * 0.0168980;
                const double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
                b6 = white * 0.115926;
                // Scale pink noise output to roughly normalize peak/RMS to unit range
                raw = pink * 0.15;
                break;
            }
        }

        const double output = raw * gainLinear;
        return std::isfinite(output) ? std::clamp(output, -2.0, 2.0) : 0.0;
    }

private:
    // Fast high-entropy xorshift64 generator yielding uniform double in [-1.0, 1.0]
    double nextUniformRandom() noexcept {
        rngState ^= rngState >> 12;
        rngState ^= rngState << 25;
        rngState ^= rngState >> 27;
        const uint64_t val = rngState * 0x2545F4914F6CDD1DULL;
        // Map 53 bits to [0.0, 1.0) then scale to [-1.0, 1.0]
        const double unit = static_cast<double>(val >> 11) * (1.0 / 9007199254740992.0);
        return unit * 2.0 - 1.0;
    }

    double currentSampleRate = 48000.0;
    double phase = 0.0;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, b3 = 0.0, b4 = 0.0, b5 = 0.0, b6 = 0.0;
    uint64_t rngState = 0x853c49e6748fea9bULL;
};

} // namespace nova::graviton

