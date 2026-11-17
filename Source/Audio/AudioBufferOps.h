#pragma once

#include <array>
#include <cstddef>

namespace nova
{
// Small allocation-free operations shared by the live device callback and tests.
// Buffers are borrowed from the audio device; these helpers never own memory.
inline void clearOutputBuffers(float* const* output, int numOutputs, int numSamples) noexcept
{
    if (output == nullptr || numOutputs <= 0 || numSamples <= 0)
        return;

    for (int channel = 0; channel < numOutputs; ++channel)
        if (output[channel] != nullptr)
            for (int sample = 0; sample < numSamples; ++sample)
                output[channel][sample] = 0.0f;
}

// Map a 0-based physical output index to the compact callback channel index.
// Returns false when the physical output is inactive, out of range, or null.
template <std::size_t MappingSize>
inline bool writeMappedOutputSample(float* const* output, int numOutputs,
                                    const std::array<int, MappingSize>& physicalToCallback,
                                    int physicalOutput, int sampleIndex, int numSamples,
                                    float value) noexcept
{
    if (output == nullptr || physicalOutput < 0
        || physicalOutput >= static_cast<int>(MappingSize)
        || sampleIndex < 0 || sampleIndex >= numSamples)
        return false;

    const int callbackIndex = physicalToCallback[static_cast<std::size_t>(physicalOutput)];
    if (callbackIndex < 0 || callbackIndex >= numOutputs || output[callbackIndex] == nullptr)
        return false;

    output[callbackIndex][sampleIndex] = value;
    return true;
}
} // namespace nova
