#pragma once

#include "Mixer/MixMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace nova
{
constexpr int kTestLogicalChannels = 144;
constexpr int kTestVisibleAux = 4;
constexpr int kTestVisibleMatrix = 2;

// Deterministic block-level harness for regression tests. It models the routing
// invariants without opening a hardware device or constructing the GUI.
struct AudioBlockTestConfig
{
    std::array<int, kTestLogicalChannels> sourceForLogical {};
    std::array<float, kTestLogicalChannels> gainDb {};
    std::array<float, kTestLogicalChannels> pan {};
    std::array<bool, kTestLogicalChannels> muted {};
    std::array<bool, kTestLogicalChannels> solo {};
    std::array<std::array<float, kTestVisibleAux>, kTestLogicalChannels> auxSendDb {};
    std::array<std::array<bool, kTestVisibleAux>, kTestLogicalChannels> auxPre {};
    std::array<float, kTestVisibleAux> auxMasterDb {};
    std::array<float, kTestVisibleMatrix> matrixMasterDb {};
    float mainMasterDb = 0.0f;

    AudioBlockTestConfig() noexcept
    {
        sourceForLogical.fill(-1);
        gainDb.fill(0.0f);
        pan.fill(0.0f);
        muted.fill(false);
        solo.fill(false);
        for (auto& sends : auxSendDb) sends.fill(-120.0f);
        for (auto& sends : auxPre) sends.fill(false);
        auxMasterDb.fill(0.0f);
        matrixMasterDb.fill(0.0f);
    }
};

struct AudioBlockTestResult
{
    std::array<float, 2> main {};
    std::array<float, kTestVisibleAux> aux {};
    std::array<float, kTestVisibleMatrix> matrix {};
};

inline AudioBlockTestResult processAudioBlockTestSample(
    const AudioBlockTestConfig& config,
    const std::array<float, 64>& physicalInputs,
    int physicalInputCount) noexcept
{
    AudioBlockTestResult result;
    bool anySolo = false;
    for (bool isSolo : config.solo)
        anySolo = anySolo || isSolo;

    std::array<float, kTestVisibleAux> auxPreSum {};
    std::array<float, kTestVisibleAux> auxPostSum {};
    for (int channel = 0; channel < kTestLogicalChannels; ++channel)
    {
        const int source = config.sourceForLogical[static_cast<size_t>(channel)];
        if (source < 0 || source >= physicalInputCount
            || source >= static_cast<int>(physicalInputs.size()))
            continue;

        const float raw = sanitizeSample(physicalInputs[static_cast<size_t>(source)]);
        const float gated = applyChannelMuteSolo(raw,
            config.muted[static_cast<size_t>(channel)], anySolo,
            config.solo[static_cast<size_t>(channel)]);
        if (gated == 0.0f)
            continue;

        const float preFader = gated;
        const float postFader = preFader * linearGainFromDb(config.gainDb[static_cast<size_t>(channel)]);
        const auto panGains = equalPowerPanGains(config.pan[static_cast<size_t>(channel)]);
        result.main[0] += postFader * panGains.left;
        result.main[1] += postFader * panGains.right;

        for (int aux = 0; aux < kTestVisibleAux; ++aux)
        {
            const auto a = static_cast<size_t>(aux);
            const float sendDb = config.auxSendDb[static_cast<size_t>(channel)][a];
            if (!std::isfinite(sendDb) || sendDb <= -59.9f)
                continue;
            const float sendSource = config.auxPre[static_cast<size_t>(channel)][a] ? preFader : postFader;
            const float send = sendSource * linearGainFromDb(sendDb);
            if (config.auxPre[static_cast<size_t>(channel)][a])
                auxPreSum[a] += send;
            else
                auxPostSum[a] += send;
        }
    }

    const float mainGain = linearGainFromDb(config.mainMasterDb);
    result.main[0] = sanitizeSample(result.main[0] * mainGain);
    result.main[1] = sanitizeSample(result.main[1] * mainGain);
    for (int aux = 0; aux < kTestVisibleAux; ++aux)
        result.aux[static_cast<size_t>(aux)] = sanitizeSample(
            (auxPreSum[static_cast<size_t>(aux)] + auxPostSum[static_cast<size_t>(aux)])
            * linearGainFromDb(config.auxMasterDb[static_cast<size_t>(aux)]));

    // Match the current project's fixed matrix conventions:
    // Matrix 1 = mono sum of Main; Matrix 2 = mono sum of mastered AUX buses.
    result.matrix[0] = sanitizeSample((result.main[0] + result.main[1]) * 0.5f
        * linearGainFromDb(config.matrixMasterDb[0]));
    float auxSum = 0.0f;
    for (float value : result.aux) auxSum += value;
    result.matrix[1] = sanitizeSample(auxSum * 0.25f
        * linearGainFromDb(config.matrixMasterDb[1]));
    return result;
}
}
