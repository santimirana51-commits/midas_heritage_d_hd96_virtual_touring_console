#pragma once

#include "Audio/AudioBufferOps.h"
#include "Mixer/MixMath.h"
#include <algorithm>
#include <array>
#include <cstddef>

namespace nova
{
// Physical outputs 0/1 are Main L/R.
// When dualOutput4Ch is true and activeOutputCount >= 4, physical outputs 2/3 are Monitor L/R.
// Otherwise, physical outputs 2..5 are AUX 1..4, followed by Matrix 1..2.
// Additional buses use the remaining physical outputs in AUX-then-Matrix order.
template <std::size_t AuxCount, std::size_t MatrixCount>
inline void routeBusOutputSample(
    float* const* output, int numOutputs,
    const std::array<int, 64>& physicalToCallback, int activeOutputCount,
    int sampleIndex, int numSamples, float mainLeft, float mainRight,
    const std::array<float, AuxCount>& aux,
    const std::array<float, MatrixCount>& matrix,
    float monLeft = 0.0f, float monRight = 0.0f,
    bool dualOutput4Ch = false) noexcept
{
    if (output == nullptr || numOutputs <= 0 || sampleIndex < 0 || sampleIndex >= numSamples)
        return;

    if (activeOutputCount == 1)
    {
        if (output[0] != nullptr)
            output[0][sampleIndex] = sanitizeSample(0.5f * (mainLeft + mainRight));
        return;
    }

    (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                   0, sampleIndex, numSamples, sanitizeSample(mainLeft));
    (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                   1, sampleIndex, numSamples, sanitizeSample(mainRight));

    if (dualOutput4Ch && activeOutputCount >= 4)
    {
        // 4-Channel Mode: Out 0/1 = Master L/R, Out 2/3 = Monitor L/R
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       2, sampleIndex, numSamples, sanitizeSample(monLeft));
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       3, sampleIndex, numSamples, sanitizeSample(monRight));

        int physicalOutput = 4;
        for (std::size_t i = 0; i < aux.size(); ++i, ++physicalOutput)
            (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                           physicalOutput, sampleIndex, numSamples,
                                           sanitizeSample(aux[i]));
        for (std::size_t i = 0; i < matrix.size(); ++i, ++physicalOutput)
            (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                           physicalOutput, sampleIndex, numSamples,
                                           sanitizeSample(matrix[i]));
        return;
    }

    const std::size_t legacyAuxCount = std::min<std::size_t>(aux.size(), 4);
    for (std::size_t i = 0; i < legacyAuxCount; ++i)
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       static_cast<int>(i) + 2, sampleIndex, numSamples,
                                       sanitizeSample(aux[i]));
    const std::size_t legacyMatrixCount = std::min<std::size_t>(matrix.size(), 2);
    for (std::size_t i = 0; i < legacyMatrixCount; ++i)
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       static_cast<int>(i) + 6, sampleIndex,
                                       numSamples, sanitizeSample(matrix[i]));

    int physicalOutput = 8;
    for (std::size_t i = legacyAuxCount; i < aux.size(); ++i, ++physicalOutput)
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       physicalOutput, sampleIndex, numSamples,
                                       sanitizeSample(aux[i]));
    for (std::size_t i = legacyMatrixCount; i < matrix.size(); ++i, ++physicalOutput)
        (void) writeMappedOutputSample(output, numOutputs, physicalToCallback,
                                       physicalOutput, sampleIndex, numSamples,
                                       sanitizeSample(matrix[i]));
}

inline void routeBusOutputSample(
    float* const* output, int numOutputs,
    const std::array<int, 64>& physicalToCallback, int activeOutputCount,
    int sampleIndex, int numSamples, float mainLeft, float mainRight,
    const std::array<float, 4>& aux, float matrix1, float matrix2,
    float monLeft = 0.0f, float monRight = 0.0f,
    bool dualOutput4Ch = false) noexcept
{
    routeBusOutputSample(output, numOutputs, physicalToCallback,
                         activeOutputCount, sampleIndex, numSamples, mainLeft,
                         mainRight, aux, std::array<float, 2>{matrix1, matrix2},
                         monLeft, monRight, dualOutput4Ch);
}
} // namespace nova
