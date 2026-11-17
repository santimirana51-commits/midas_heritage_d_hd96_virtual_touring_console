#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <cstddef>

namespace nova::graviton
{
// Shared channel-to-bus routing primitive. It receives the channel's pre-fader
// and post-fader samples and sends each to its configured AUX tap while adding
// the post-fader signal to the stereo Main bus. All math remains double.
// Fixed-size std::array arguments keep this helper allocation-free and suitable
// for the real-time callback.
template <std::size_t AuxCount>
inline void accumulateChannelToBuses(
    Sample& mainLeft,
    Sample& mainRight,
    std::array<Sample, AuxCount>& auxSums,
    Sample preFaderSample,
    Sample postFaderSample,
    Sample panGainLeft,
    Sample panGainRight,
    const std::array<Sample, AuxCount>& sendGains,
    const std::array<bool, AuxCount>& sendPreFader) noexcept
{
    const Sample pre = sanitize(preFaderSample);
    const Sample post = sanitize(postFaderSample);
    const Sample panL = clampFinite(panGainLeft, 0.0, 1.0, 0.0);
    const Sample panR = clampFinite(panGainRight, 0.0, 1.0, 0.0);

    mainLeft = sanitize(sanitize(mainLeft) + post * panL);
    mainRight = sanitize(sanitize(mainRight) + post * panR);

    for (std::size_t i = 0; i < AuxCount; ++i)
    {
        const Sample gain = clampFinite(sendGains[i], 0.0, 16.0, 0.0);
        if (gain > 0.0)
            accumulateBus(auxSums[i], sendPreFader[i] ? pre : post, gain);
    }
}

// Compact representation of a configured, non-zero AUX send. Build/update this
// list when routing controls change, not in the audio callback. Fixed-capacity
// storage lets callers keep the route list allocation-free on the realtime path.
struct ActiveAuxRoute
{
    std::size_t auxIndex = 0;
    Sample gain = 0.0;
    bool preFader = false;
};

template <std::size_t AuxCount, std::size_t RouteCapacity>
inline void accumulateChannelToBusesSparse(
    Sample& mainLeft,
    Sample& mainRight,
    std::array<Sample, AuxCount>& auxSums,
    Sample preFaderSample,
    Sample postFaderSample,
    Sample panGainLeft,
    Sample panGainRight,
    const std::array<ActiveAuxRoute, RouteCapacity>& routes,
    std::size_t routeCount) noexcept
{
    const Sample pre = sanitize(preFaderSample);
    const Sample post = sanitize(postFaderSample);
    const Sample panL = clampFinite(panGainLeft, 0.0, 1.0, 0.0);
    const Sample panR = clampFinite(panGainRight, 0.0, 1.0, 0.0);

    mainLeft = sanitize(sanitize(mainLeft) + post * panL);
    mainRight = sanitize(sanitize(mainRight) + post * panR);

    const std::size_t boundedCount = routeCount < RouteCapacity ? routeCount : RouteCapacity;
    for (std::size_t i = 0; i < boundedCount; ++i)
    {
        const auto& route = routes[i];
        if (route.auxIndex >= AuxCount)
            continue;
        const Sample gain = clampFinite(route.gain, 0.0, 16.0, 0.0);
        if (gain > 0.0)
            accumulateBus(auxSums[route.auxIndex], route.preFader ? pre : post, gain);
    }
}

// Conservative density heuristic for choosing the helper path. Sparse routing
// is preferred when fewer than half of the AUX slots are active; dense routing
// avoids the overhead of walking a route list when most slots are populated.
// This is a policy helper only: callers must provide a route list that matches
// the dense gain/tap arrays for the same routing snapshot.
enum class AuxRoutingMode { Dense, Sparse };

inline AuxRoutingMode chooseAuxRoutingMode(std::size_t activeRouteCount,
                                           std::size_t auxCount) noexcept
{
    if (auxCount == 0 || activeRouteCount >= auxCount)
        return AuxRoutingMode::Dense;
    return activeRouteCount < (auxCount + 1) / 2
        ? AuxRoutingMode::Sparse : AuxRoutingMode::Dense;
}

template <std::size_t AuxCount, std::size_t RouteCapacity>
inline AuxRoutingMode accumulateChannelToBusesAdaptive(
    Sample& mainLeft,
    Sample& mainRight,
    std::array<Sample, AuxCount>& auxSums,
    Sample preFaderSample,
    Sample postFaderSample,
    Sample panGainLeft,
    Sample panGainRight,
    const std::array<Sample, AuxCount>& sendGains,
    const std::array<bool, AuxCount>& sendPreFader,
    const std::array<ActiveAuxRoute, RouteCapacity>& routes,
    std::size_t routeCount) noexcept
{
    const auto mode = chooseAuxRoutingMode(routeCount < RouteCapacity ? routeCount : RouteCapacity,
                                           AuxCount);
    if (mode == AuxRoutingMode::Sparse)
        accumulateChannelToBusesSparse(mainLeft, mainRight, auxSums,
            preFaderSample, postFaderSample, panGainLeft, panGainRight, routes, routeCount);
    else
        accumulateChannelToBuses(mainLeft, mainRight, auxSums,
            preFaderSample, postFaderSample, panGainLeft, panGainRight, sendGains, sendPreFader);
    return mode;
}

struct IntegratedBusFrame
{
    StereoFrame main{};
    Sample aux = 0.0;
    Sample matrixFromMain = 0.0;
    Sample matrixFromAux = 0.0;
};

// Small deterministic integration harness used by regression tests to model a
// two-channel -> stereo Main/AUX -> Matrix path without JUCE or device I/O.
inline IntegratedBusFrame integrateTwoChannelBusFrame(
    const std::array<Sample, 2>& preFader,
    const std::array<Sample, 2>& postFader,
    const std::array<StereoFrame, 2>& panGains,
    const std::array<Sample, 2>& auxSendGain,
    const std::array<bool, 2>& auxPreFader,
    Sample mainMaster,
    Sample auxMaster,
    Sample matrixMainMaster,
    Sample matrixAuxMaster,
    bool mainMuted = false,
    bool auxMuted = false,
    bool matrixMainMuted = false,
    bool matrixAuxMuted = false) noexcept
{
    IntegratedBusFrame result{};
    std::array<Sample, 1> auxSums{};
    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        const std::array<Sample, 1> gains { auxSendGain[channel] };
        const std::array<bool, 1> pre { auxPreFader[channel] };
        accumulateChannelToBuses(result.main.left, result.main.right, auxSums,
            preFader[channel], postFader[channel], panGains[channel].left,
            panGains[channel].right, gains, pre);
    }
    result.main.left = applyBusMaster(result.main.left, mainMaster, mainMuted);
    result.main.right = applyBusMaster(result.main.right, mainMaster, mainMuted);
    result.aux = applyBusMaster(auxSums[0], auxMaster, auxMuted);
    const std::array<Sample, 4> matrixAux { result.aux, 0.0, 0.0, 0.0 };
    const auto matrix = mixMatrixOutputs(result.main.left, result.main.right,
        matrixAux, matrixMainMaster, matrixMainMuted, matrixAuxMaster, matrixAuxMuted);
    result.matrixFromMain = matrix.matrix1;
    result.matrixFromAux = matrix.matrix2;
    return result;
}
} // namespace nova::graviton
