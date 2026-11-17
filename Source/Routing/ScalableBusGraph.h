#pragma once

#include "Mixer/MixMath.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace nova::bus_graph_detail {
template <typename T>
inline T limit(T minimum, T maximum, T value) noexcept {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}
}

namespace nova
{
// Capacity model for the scalable NOVA bus graph. This header is allocation-free
// and deliberately independent of JUCE so routing rules can be tested standalone.
inline constexpr int kBusGraphChannels = 144;
inline constexpr int kBusGraphAuxCount = 96;
inline constexpr int kBusGraphMatrixCount = 24;

enum class MatrixSourceKind : unsigned char
{
    MainMono,
    AuxBus,
    AuxRangeSum
};

struct MatrixSourceRoute
{
    MatrixSourceKind kind { MatrixSourceKind::MainMono };
    int sourceIndex { 0 };       // AUX index for AuxBus or first index for AuxRangeSum
    int sourceEndExclusive { 1 };// exclusive end for AuxRangeSum
    float masterGain { 1.0f };
    bool muted { false };
};

struct ScalableBusGraphConfig
{
    // Per-channel send gains are linear gains. PRE uses preFader; POST uses postFader.
    std::array<std::array<float, kBusGraphAuxCount>, kBusGraphChannels> sendGain {};
    std::array<std::array<bool, kBusGraphAuxCount>, kBusGraphChannels> sendPreFader {};
    std::array<float, kBusGraphAuxCount> auxMasterGain {};
    std::array<bool, kBusGraphAuxCount> auxMuted {};
    std::array<MatrixSourceRoute, kBusGraphMatrixCount> matrix {};

    ScalableBusGraphConfig() noexcept
    {
        for (auto& channel : sendGain) channel.fill(0.0f);
        auxMasterGain.fill(1.0f);
        auxMuted.fill(false);
    }
};

struct ScalableBusGraphInput
{
    std::array<float, kBusGraphChannels> preFader {};
    std::array<float, kBusGraphChannels> postFader {};
    float mainLeft { 0.0f };
    float mainRight { 0.0f };
};

struct ScalableBusGraphOutput
{
    std::array<float, kBusGraphAuxCount> aux {};
    std::array<float, kBusGraphMatrixCount> matrix {};
};

inline float finiteLinearGain(float gain) noexcept
{
    return std::isfinite(gain) ? bus_graph_detail::limit(0.0f, 16.0f, gain) : 0.0f;
}


// Linear-gain accumulation is the real-time primitive. Convert dB to linear
// once per channel/send per callback block, not once per sample.
inline void accumulateBusSendLinear(float& destination, float source, float gain) noexcept
{
    if (!std::isfinite(source) || !std::isfinite(gain) || gain <= 0.0f)
        return;
    const float contribution = source * bus_graph_detail::limit(0.0f, 3.1622777f, gain); // +10 dB maximum
    if (std::isfinite(contribution))
        destination = sanitizeSample(destination + contribution);
}

// Compatibility helper for tests and non-real-time callers already using dB.
inline void accumulateBusSend(float& destination, float source, float sendDb) noexcept
{
    if (!std::isfinite(sendDb) || sendDb <= -59.9f)
        return;
    const float boundedDb = bus_graph_detail::limit(-60.0f, 10.0f, sendDb);
    accumulateBusSendLinear(destination, source, std::pow(10.0f, boundedDb / 20.0f));
}

struct ScalableBusSendRoute
{
    // Field order keeps the fixed route record compact (typically 8 bytes).
    unsigned short channel = 0;
    unsigned char aux = 0;
    bool preFader = false;
    float gain = 0.0f;
};

// A fixed-capacity sparse execution plan. Build/update this plan on the control
// thread when routing changes, then process samples without heap allocation.
// Maximum route count is 144 * 96; inactive sends consume no processing time.
struct ScalableBusGraphPlan
{
    static constexpr std::size_t maxRoutes =
        static_cast<std::size_t>(kBusGraphChannels) * kBusGraphAuxCount;
    std::array<ScalableBusSendRoute, maxRoutes> routes {};
    std::size_t routeCount = 0;
    std::array<float, kBusGraphAuxCount> auxMasterGain {};
    std::array<bool, kBusGraphAuxCount> auxMuted {};
    std::array<MatrixSourceRoute, kBusGraphMatrixCount> matrix {};

    ScalableBusGraphPlan() noexcept
    {
        auxMasterGain.fill(1.0f);
        auxMuted.fill(false);
    }
};

inline bool buildScalableBusGraphPlan(
    const ScalableBusGraphConfig& config, ScalableBusGraphPlan& plan) noexcept
{
    plan.routeCount = 0;
    for (int a = 0; a < kBusGraphAuxCount; ++a)
    {
        plan.auxMasterGain[static_cast<std::size_t>(a)] =
            finiteLinearGain(config.auxMasterGain[static_cast<std::size_t>(a)]);
        plan.auxMuted[static_cast<std::size_t>(a)] = config.auxMuted[static_cast<std::size_t>(a)];
    }
    plan.matrix = config.matrix;
    for (int c = 0; c < kBusGraphChannels; ++c)
    {
        for (int a = 0; a < kBusGraphAuxCount; ++a)
        {
            const float gain = finiteLinearGain(
                config.sendGain[static_cast<std::size_t>(c)][static_cast<std::size_t>(a)]);
            if (gain <= 0.0f)
                continue;
            if (plan.routeCount >= plan.routes.size())
            {
                // Route count exceeds bus graph capacity (144 channels × 96 AUX buses = 13,824 routes).
                // Previously applied send gains will remain in effect until the next control-thread update.
                return false;
            }
            auto& route = plan.routes[plan.routeCount++];
            route.channel = static_cast<unsigned short>(c);
            route.aux = static_cast<unsigned char>(a);
            route.gain = gain;
            route.preFader = config.sendPreFader[static_cast<std::size_t>(c)][static_cast<std::size_t>(a)];
        }
    }
    return true;
}

inline ScalableBusGraphOutput processScalableBusGraphSample(
    const ScalableBusGraphPlan& plan, const ScalableBusGraphInput& input) noexcept
{
    ScalableBusGraphOutput output {};
    for (std::size_t i = 0; i < plan.routeCount; ++i)
    {
        const auto& route = plan.routes[i];
        const auto channel = static_cast<std::size_t>(route.channel);
        const auto aux = static_cast<std::size_t>(route.aux);
        const float source = route.preFader ? input.preFader[channel] : input.postFader[channel];
        output.aux[aux] = sanitizeSample(output.aux[aux] + sanitizeSample(source) * route.gain);
    }
    for (int a = 0; a < kBusGraphAuxCount; ++a)
    {
        const auto index = static_cast<std::size_t>(a);
        output.aux[index] = plan.auxMuted[index] ? 0.0f
            : sanitizeSample(output.aux[index] * finiteLinearGain(plan.auxMasterGain[index]));
    }

    const float mainMono = sanitizeSample(0.5f * (sanitizeSample(input.mainLeft) + sanitizeSample(input.mainRight)));
    for (int m = 0; m < kBusGraphMatrixCount; ++m)
    {
        const auto& route = plan.matrix[static_cast<std::size_t>(m)];
        float source = 0.0f;
        switch (route.kind)
        {
            case MatrixSourceKind::MainMono:
                source = mainMono;
                break;
            case MatrixSourceKind::AuxBus:
                if (route.sourceIndex >= 0 && route.sourceIndex < kBusGraphAuxCount)
                    source = output.aux[static_cast<std::size_t>(route.sourceIndex)];
                break;
            case MatrixSourceKind::AuxRangeSum:
            {
                const int first = bus_graph_detail::limit(0, kBusGraphAuxCount, route.sourceIndex);
                const int last = bus_graph_detail::limit(first, kBusGraphAuxCount, route.sourceEndExclusive);
                for (int a = first; a < last; ++a)
                    source = sanitizeSample(source + output.aux[static_cast<std::size_t>(a)]);
                break;
            }
        }
        output.matrix[static_cast<std::size_t>(m)] = route.muted ? 0.0f
            : sanitizeSample(source * finiteLinearGain(route.masterGain));
    }
    return output;
}

inline ScalableBusGraphOutput processScalableBusGraphSample(
    const ScalableBusGraphConfig& config, const ScalableBusGraphInput& input) noexcept
{
    ScalableBusGraphOutput output {};
    for (int a = 0; a < kBusGraphAuxCount; ++a)
    {
        float sum = 0.0f;
        for (int c = 0; c < kBusGraphChannels; ++c)
        {
            const float source = config.sendPreFader[static_cast<std::size_t>(c)][static_cast<std::size_t>(a)]
                ? input.preFader[static_cast<std::size_t>(c)]
                : input.postFader[static_cast<std::size_t>(c)];
            const float gain = finiteLinearGain(config.sendGain[static_cast<std::size_t>(c)][static_cast<std::size_t>(a)]);
            sum += sanitizeSample(source) * gain;
        }
        const float master = finiteLinearGain(config.auxMasterGain[static_cast<std::size_t>(a)]);
        output.aux[static_cast<std::size_t>(a)] = config.auxMuted[static_cast<std::size_t>(a)]
            ? 0.0f : sanitizeSample(sum * master);
    }

    const float mainMono = sanitizeSample(0.5f * (sanitizeSample(input.mainLeft) + sanitizeSample(input.mainRight)));
    for (int m = 0; m < kBusGraphMatrixCount; ++m)
    {
        const auto& route = config.matrix[static_cast<std::size_t>(m)];
        float source = 0.0f;
        switch (route.kind)
        {
            case MatrixSourceKind::MainMono:
                source = mainMono;
                break;
            case MatrixSourceKind::AuxBus:
                if (route.sourceIndex >= 0 && route.sourceIndex < kBusGraphAuxCount)
                    source = output.aux[static_cast<std::size_t>(route.sourceIndex)];
                break;
            case MatrixSourceKind::AuxRangeSum:
            {
                const int first = bus_graph_detail::limit(0, kBusGraphAuxCount, route.sourceIndex);
                const int last = bus_graph_detail::limit(first, kBusGraphAuxCount, route.sourceEndExclusive);
                for (int a = first; a < last; ++a)
                    source += output.aux[static_cast<std::size_t>(a)];
                break;
            }
        }
        const float master = finiteLinearGain(route.masterGain);
        output.matrix[static_cast<std::size_t>(m)] = route.muted ? 0.0f : sanitizeSample(source * master);
    }
    return output;
}
} // namespace nova
