#pragma once

#include <algorithm>
#include <cmath>

namespace nova
{
struct PeakingEqCoefficients
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
};

inline PeakingEqCoefficients makePeakingEqCoefficients(float sampleRate,
                                                        float frequency,
                                                        float gainDb,
                                                        float q) noexcept
{
    PeakingEqCoefficients result;
    if (!std::isfinite(sampleRate) || !std::isfinite(frequency)
        || !std::isfinite(gainDb) || !std::isfinite(q)
        || sampleRate <= 0.0f || frequency <= 0.0f || q <= 0.0f)
        return result;

    const float safeFrequency = std::clamp(frequency, 1.0f, sampleRate * 0.45f);
    const float safeGainDb = std::clamp(gainDb, -18.0f, 18.0f);
    const float safeQ = std::clamp(q, 0.1f, 10.0f);
    constexpr float pi = 3.14159265358979323846f;
    const float A = std::pow(10.0f, safeGainDb / 40.0f);
    const float omega = 2.0f * pi * safeFrequency / sampleRate;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * safeQ);
    const float a0 = 1.0f + alpha / A;

    result.b0 = (1.0f + alpha * A) / a0;
    result.b1 = (-2.0f * cs) / a0;
    result.b2 = (1.0f - alpha * A) / a0;
    result.a1 = (-2.0f * cs) / a0;
    result.a2 = (1.0f - alpha / A) / a0;

    if (!std::isfinite(result.b0) || !std::isfinite(result.b1)
        || !std::isfinite(result.b2) || !std::isfinite(result.a1)
        || !std::isfinite(result.a2))
        return PeakingEqCoefficients{};
    return result;
}

inline float peakingEqMagnitudeAt(const PeakingEqCoefficients& c,
                                  float frequency, float sampleRate) noexcept
{
    if (!std::isfinite(frequency) || !std::isfinite(sampleRate)
        || sampleRate <= 0.0f || frequency < 0.0f)
        return 0.0f;

    constexpr float pi = 3.14159265358979323846f;
    const float w = 2.0f * pi * frequency / sampleRate;
    const auto realNum = c.b0 + c.b1 * std::cos(w) + c.b2 * std::cos(2.0f * w);
    const auto imagNum = -c.b1 * std::sin(w) - c.b2 * std::sin(2.0f * w);
    const auto realDen = 1.0f + c.a1 * std::cos(w) + c.a2 * std::cos(2.0f * w);
    const auto imagDen = -c.a1 * std::sin(w) - c.a2 * std::sin(2.0f * w);
    const float denominator = realDen * realDen + imagDen * imagDen;
    if (!std::isfinite(denominator) || denominator <= 1.0e-20f)
        return 0.0f;
    const float numerator = realNum * realNum + imagNum * imagNum;
    const float magnitude = std::sqrt(numerator / denominator);
    return std::isfinite(magnitude) ? magnitude : 0.0f;
}
}
