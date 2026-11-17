#pragma once
#include "Mixer/MixMath.h"
#include <array>
#include <cmath>

namespace nova
{
// Shared, allocation-free bus finalization primitives used by the production
// MixerEngine callback and unit tests. Inputs to matrix mixing are already
// post-master AUX and post-master stereo Main samples.
inline float applyBusMaster(float sample, float masterGain, bool muted) noexcept
{
    if (muted || !std::isfinite(sample) || !std::isfinite(masterGain))
        return 0.0f;
    return sanitizeSample(sample * std::clamp(masterGain, 0.0f, 16.0f));
}

struct VisibleMatrixOutput
{
    float matrix1 = 0.0f; // mono sum of Main L/R, then Matrix 1 master
    float matrix2 = 0.0f; // sum of mastered AUX 1-4, then Matrix 2 master
};

inline VisibleMatrixOutput mixVisibleMatrixOutputs(
    float mainLeft, float mainRight,
    const std::array<float, 4>& masteredAux,
    float matrix1Master, bool matrix1Muted,
    float matrix2Master, bool matrix2Muted) noexcept
{
    float auxSum = 0.0f;
    for (float sample : masteredAux)
        auxSum = sanitizeSample(auxSum + sanitizeSample(sample));

    const float mainMono = sanitizeSample(0.5f *
        (sanitizeSample(mainLeft) + sanitizeSample(mainRight)));
    return {
        applyBusMaster(mainMono, matrix1Master, matrix1Muted),
        applyBusMaster(auxSum, matrix2Master, matrix2Muted)
    };
}
} // namespace nova
