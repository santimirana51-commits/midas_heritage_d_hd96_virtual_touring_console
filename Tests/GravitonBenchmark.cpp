#include "../Source/DSP/GravitonDSP.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

using nova::graviton::Sample;
using nova::graviton::StereoFrame;

namespace
{
struct TimingStats
{
    double medianNsPerAudioSample = 0.0;
    double p95NsPerAudioSample = 0.0;
    double p99NsPerAudioSample = 0.0;
};

double percentileSorted(const std::vector<double>& sorted, double percentile)
{
    if (sorted.empty())
        return 0.0;
    const auto index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(sorted.size())) - 1.0);
    return sorted[std::min(index, sorted.size() - 1)];
}

template <typename T, std::size_t Channels, std::size_t AuxCount>
TimingStats runCase(std::size_t blockSize, int blocks, int batches)
{
    static_assert(std::is_floating_point_v<T>);
    std::array<T, Channels> pre{};
    std::array<T, Channels> post{};
    std::array<T, Channels> panL{};
    std::array<T, Channels> panR{};
    std::array<T, Channels> faderGain{};
    std::array<std::array<T, AuxCount>, Channels> sendGain{};
    std::array<std::array<bool, AuxCount>, Channels> sendPre{};
    std::array<T, AuxCount> aux{};
    volatile T sink = T{0};

    for (std::size_t ch = 0; ch < Channels; ++ch)
    {
        faderGain[ch] = static_cast<T>(std::pow(T{10}, static_cast<T>(-static_cast<int>(ch % 18)) / T{20}));
        const T panPosition = static_cast<T>(ch % 101) / T{100};
        panL[ch] = static_cast<T>(std::sqrt(T{1} - panPosition));
        panR[ch] = static_cast<T>(std::sqrt(panPosition));
        for (std::size_t a = 0; a < AuxCount; ++a)
        {
            const int db = (a % 7 == 0) ? 0 : -6 - static_cast<int>((ch + a) % 24);
            sendGain[ch][a] = (a % 7 == 0)
                ? T{0}
                : static_cast<T>(std::pow(T{10}, static_cast<T>(db) / T{20}));
            sendPre[ch][a] = ((ch + a) % 2) == 0;
        }
    }

    std::vector<double> batchNsPerSample;
    batchNsPerSample.reserve(static_cast<std::size_t>(batches));
    T phase = T{0};
    T mainL = T{0};
    T mainR = T{0};

    for (int batch = 0; batch < batches; ++batch)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < blocks; ++block)
        {
            mainL = T{0};
            mainR = T{0};
            aux.fill(T{0});
            for (std::size_t sample = 0; sample < blockSize; ++sample)
            {
                phase += static_cast<T>(0.013);
                for (std::size_t ch = 0; ch < Channels; ++ch)
                {
                    const T amplitude = static_cast<T>(0.05) +
                        static_cast<T>(ch % 13) * static_cast<T>(0.002);
                    pre[ch] = amplitude * std::sin(phase + static_cast<T>(ch) * static_cast<T>(0.07));
                    post[ch] = pre[ch] * faderGain[ch];
                    mainL += post[ch] * panL[ch];
                    mainR += post[ch] * panR[ch];

                    for (std::size_t a = 0; a < AuxCount; ++a)
                    {
                        const T source = sendPre[ch][a] ? pre[ch] : post[ch];
                        if (sendGain[ch][a] > T{0})
                            aux[a] += source * sendGain[ch][a];
                    }
                }
            }
            sink = static_cast<T>(sink + mainL + mainR + aux[static_cast<std::size_t>(block) % AuxCount]);
        }
        const auto end = std::chrono::steady_clock::now();
        const double elapsedNs = std::chrono::duration<double, std::nano>(end - start).count();
        const double samples = static_cast<double>(blocks) * static_cast<double>(blockSize);
        batchNsPerSample.push_back(elapsedNs / samples);
    }

    if (!std::isfinite(static_cast<double>(sink)))
        return {};

    std::sort(batchNsPerSample.begin(), batchNsPerSample.end());
    return {
        percentileSorted(batchNsPerSample, 0.50),
        percentileSorted(batchNsPerSample, 0.95),
        percentileSorted(batchNsPerSample, 0.99)
    };
}

template <std::size_t Channels>
void printCase(std::size_t blockSize, int blocks, int batches)
{
    constexpr std::size_t auxCount = 96;
    const auto f = runCase<float, Channels, auxCount>(blockSize, blocks, batches);
    const auto d = runCase<double, Channels, auxCount>(blockSize, blocks, batches);
    const double ratio = f.medianNsPerAudioSample > 0.0
        ? d.medianNsPerAudioSample / f.medianNsPerAudioSample : 0.0;
    const double audioMs = static_cast<double>(blockSize) / 48000.0 * 1000.0;
    const double doubleBlockMs = d.medianNsPerAudioSample * static_cast<double>(blockSize) / 1.0e6;
    const double headroom = doubleBlockMs > 0.0 ? audioMs / doubleBlockMs : 0.0;

    std::cout << std::setw(7) << Channels
              << std::setw(7) << blockSize
              << std::setw(12) << std::fixed << std::setprecision(2) << f.medianNsPerAudioSample
              << std::setw(12) << d.medianNsPerAudioSample
              << std::setw(10) << ratio
              << std::setw(12) << d.p95NsPerAudioSample
              << std::setw(12) << d.p99NsPerAudioSample
              << std::setw(12) << headroom << "x\n";
}
}

int main(int argc, char** argv)
{
    int blocks = 30;
    int batches = 5;
    if (argc > 1)
    {
        const int requested = std::atoi(argv[1]);
        if (requested >= 1 && requested <= 100000)
            blocks = requested;
    }
    if (argc > 2)
    {
        const int requested = std::atoi(argv[2]);
        if (requested >= 3 && requested <= 100)
            batches = requested;
    }

    std::cout << "Graviton Mk 2 precision benchmark (synthetic, no device I/O)\n";
    std::cout << "48 kHz assumption; 96 AUX; blocks/batch: " << blocks
              << "; batches: " << batches << "\n";
    std::cout << "float/double are arithmetic-path comparisons, not whole-app performance claims.\n";
    std::cout << "Channels Block  float-ns/s  double-ns/s  D/F-ratio  double-p95  double-p99  headroom\n";

    for (const std::size_t blockSize : { 64u, 128u, 256u, 512u })
    {
        printCase<8>(blockSize, blocks, batches);
        printCase<32>(blockSize, blocks, batches);
        printCase<64>(blockSize, blocks, batches);
        printCase<144>(blockSize, blocks, batches);
        std::cout << '\n';
    }
    return 0;
}
