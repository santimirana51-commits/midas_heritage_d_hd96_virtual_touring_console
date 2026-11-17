#pragma once

#include <algorithm>
#include <cmath>

namespace nova
{
inline float linearGainFromDb(float db) noexcept
{
    if (!std::isfinite(db))
        return 0.0f;
    const float safeDb = std::clamp(db, -120.0f, 20.0f);
    const float gain = std::pow(10.0f, safeDb / 20.0f);
    return std::isfinite(gain) ? gain : 0.0f;
}

inline float clampPan(float pan) noexcept
{
    if (!std::isfinite(pan))
        return 0.0f;
    return std::clamp(pan, -1.0f, 1.0f);
}

struct StereoGains
{
    float left = 0.70710678f;
    float right = 0.70710678f;
};

inline StereoGains equalPowerPanGains(float pan) noexcept
{
    constexpr float pi = 3.14159265358979323846f;
    const float safePan = clampPan(pan);
    const float angle = (safePan + 1.0f) * pi * 0.25f;
    return { std::cos(angle), std::sin(angle) };
}

inline float sanitizeSample(float sample) noexcept
{
    return std::isfinite(sample) ? sample : 0.0f;
}

inline float applyChannelMuteSolo(float sample, bool muted, bool soloActive, bool channelSolo) noexcept
{
    if (muted || (soloActive && !channelSolo))
        return 0.0f;
    return sanitizeSample(sample);
}

inline float auxSendLevel(float postFaderSample, float sendDb, bool sendPreFader,
                          float preFaderSample) noexcept
{
    const float source = sendPreFader ? preFaderSample : postFaderSample;
    return sanitizeSample(source) * linearGainFromDb(sendDb);
}
}
