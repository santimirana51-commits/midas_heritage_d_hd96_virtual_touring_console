#pragma once

#include <cmath>

namespace nova
{
inline float envelopeCoefficient(float sampleRate, float timeMs) noexcept
{
    if (!std::isfinite(sampleRate) || !std::isfinite(timeMs)
        || sampleRate <= 0.0f || timeMs <= 0.0f)
        return 0.0f;

    const float exponent = -1.0f / (timeMs * 0.001f * sampleRate);
    const float coefficient = std::exp(exponent);
    return std::isfinite(coefficient) ? (coefficient < 0.0f ? 0.0f : (coefficient > 1.0f ? 1.0f : coefficient)) : 0.0f;
}
}
