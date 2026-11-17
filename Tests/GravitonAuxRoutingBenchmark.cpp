#include "../Source/Mixer/GravitonBusIntegration.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace nova::graviton;

namespace
{
template <std::size_t Channels, std::size_t AuxCount, std::size_t MaxRoutes>
void runCase(std::size_t activeRoutes, std::size_t iterations)
{
    std::array<Sample, Channels> pre{};
    std::array<Sample, Channels> post{};
    std::array<std::array<Sample, AuxCount>, Channels> gains{};
    std::array<std::array<bool, AuxCount>, Channels> taps{};
    std::array<std::array<ActiveAuxRoute, MaxRoutes>, Channels> routes{};
    std::array<std::size_t, Channels> routeCounts{};
    for (std::size_t ch = 0; ch < Channels; ++ch)
    {
        pre[ch] = 0.05 + static_cast<Sample>(ch % 11) * 0.003;
        post[ch] = pre[ch] * 0.73;
        routeCounts[ch] = activeRoutes < AuxCount ? activeRoutes : AuxCount;
        for (std::size_t r = 0; r < routeCounts[ch]; ++r)
        {
            const std::size_t aux = (r * 7 + ch * 3) % AuxCount;
            const Sample gain = 0.1 + static_cast<Sample>((r + ch) % 9) * 0.07;
            gains[ch][aux] = gain;
            taps[ch][aux] = ((r + ch) % 2) == 0;
            routes[ch][r] = ActiveAuxRoute{aux, gain, taps[ch][aux]};
        }
    }

    volatile Sample sink = 0.0;
    std::array<Sample, AuxCount> denseAux{};
    std::array<Sample, AuxCount> sparseAux{};
    std::array<Sample, AuxCount> adaptiveAux{};
    Sample denseL = 0.0, denseR = 0.0, sparseL = 0.0, sparseR = 0.0;
    Sample adaptiveL = 0.0, adaptiveR = 0.0;
    AuxRoutingMode selectedMode = AuxRoutingMode::Dense;
    const auto denseStart = std::chrono::steady_clock::now();
    for (std::size_t it = 0; it < iterations; ++it)
    {
        denseAux.fill(0.0); denseL = denseR = 0.0;
        for (std::size_t ch = 0; ch < Channels; ++ch)
            accumulateChannelToBuses(denseL, denseR, denseAux, pre[ch], post[ch],
                0.7071067811865476, 0.7071067811865476, gains[ch], taps[ch]);
        sink = sink + denseL + denseR + denseAux[it % AuxCount];
    }
    const auto denseEnd = std::chrono::steady_clock::now();
    const auto sparseStart = std::chrono::steady_clock::now();
    for (std::size_t it = 0; it < iterations; ++it)
    {
        sparseAux.fill(0.0); sparseL = sparseR = 0.0;
        for (std::size_t ch = 0; ch < Channels; ++ch)
            accumulateChannelToBusesSparse(sparseL, sparseR, sparseAux, pre[ch], post[ch],
                0.7071067811865476, 0.7071067811865476, routes[ch], routeCounts[ch]);
        sink = sink + sparseL + sparseR + sparseAux[it % AuxCount];
    }
    const auto sparseEnd = std::chrono::steady_clock::now();
    const auto adaptiveStart = std::chrono::steady_clock::now();
    for (std::size_t it = 0; it < iterations; ++it)
    {
        adaptiveAux.fill(0.0); adaptiveL = adaptiveR = 0.0;
        for (std::size_t ch = 0; ch < Channels; ++ch)
            selectedMode = accumulateChannelToBusesAdaptive(adaptiveL, adaptiveR,
                adaptiveAux, pre[ch], post[ch], 0.7071067811865476,
                0.7071067811865476, gains[ch], taps[ch], routes[ch], routeCounts[ch]);
        sink = sink + adaptiveL + adaptiveR + adaptiveAux[it % AuxCount];
    }
    const auto adaptiveEnd = std::chrono::steady_clock::now();
    const double denseNs = std::chrono::duration<double, std::nano>(denseEnd - denseStart).count() / iterations;
    const double sparseNs = std::chrono::duration<double, std::nano>(sparseEnd - sparseStart).count() / iterations;
    const double adaptiveNs = std::chrono::duration<double, std::nano>(adaptiveEnd - adaptiveStart).count() / iterations;
    double maxError = std::max({std::abs(denseL - sparseL), std::abs(denseR - sparseR),
                                std::abs(denseL - adaptiveL), std::abs(denseR - adaptiveR)});
    for (std::size_t i = 0; i < AuxCount; ++i)
    {
        maxError = std::max(maxError, std::abs(denseAux[i] - sparseAux[i]));
        maxError = std::max(maxError, std::abs(denseAux[i] - adaptiveAux[i]));
    }
    std::cout << std::setw(7) << Channels << std::setw(9) << activeRoutes
              << std::setw(12) << std::fixed << std::setprecision(1) << denseNs
              << std::setw(12) << sparseNs << std::setw(12) << adaptiveNs
              << std::setw(10) << (adaptiveNs > 0.0 ? denseNs / adaptiveNs : 0.0)
              << std::setw(9) << (selectedMode == AuxRoutingMode::Sparse ? "sparse" : "dense")
              << std::setw(14) << std::scientific << maxError << '\n';
    if (!std::isfinite(static_cast<double>(sink)) || maxError > 1e-10)
        std::exit(2);
}
}

int main(int argc, char** argv)
{
    std::size_t iterations = 2000;
    if (argc > 1)
    {
        const auto parsed = std::strtoull(argv[1], nullptr, 10);
        if (parsed > 0 && parsed <= 10000000) iterations = static_cast<std::size_t>(parsed);
    }
    std::cout << "Graviton AUX dense-vs-sparse routing microbenchmark (synthetic)\n"
              << "144 channels max, 96 AUX, iterations: " << iterations << '\n'
              << "Adaptive speedup = dense time / adaptive time; timings are machine-dependent.\n"
              << "Channels Routes dense-ns sparse-ns adaptive-ns speedup mode max-error\n";
    for (std::size_t routes : {4u, 8u, 16u, 32u, 96u})
    {
        runCase<64, 96, 96>(routes, iterations);
        runCase<144, 96, 96>(routes, iterations);
    }
    return 0;
}
