#pragma once

#include <algorithm>
#include <cmath>

namespace nova
{
// One-pole gain smoothing step. Caller owns state and invokes it on the audio thread.
// Invalid targets are treated as silence; invalid state is recovered to a finite value.
inline float smoothGainStep(float current, float target, float coefficient) noexcept
{
    if (!std::isfinite(target))
        target = 0.0f;
    if (!std::isfinite(current))
        current = target;
    if (!std::isfinite(coefficient))
        coefficient = 1.0f;

    coefficient = std::clamp(coefficient, 0.0f, 1.0f);
    const float next = current + (target - current) * coefficient;
    return std::isfinite(next) ? next : 0.0f;
}

inline float gainSmoothingCoefficient(float sampleRate, float timeSeconds) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0f)
        return 1.0f;
    if (!std::isfinite(timeSeconds) || timeSeconds <= 0.0f)
        return 1.0f;
    const float coefficient = 1.0f - std::exp(-1.0f / (timeSeconds * sampleRate));
    return std::isfinite(coefficient) ? std::clamp(coefficient, 0.0f, 1.0f) : 1.0f;
}
} // namespace nova
