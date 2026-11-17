#include "../Source/DSP/GravitonDSP.h"
#include "../Source/Mixer/GravitonBusIntegration.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>

using namespace nova::graviton;

int main()
{
    constexpr std::size_t channels = 144;
    constexpr std::size_t auxCount = 96;
    constexpr int blocks = 400;
    constexpr int blockSize = 128;

    // Model a high-capacity mixer using fixed storage and repeatable inputs.
    // This is a deterministic arithmetic stress test, not a real-time benchmark.
    std::array<Sample, auxCount> auxSums{};
    Sample mainLeft = 0.0;
    Sample mainRight = 0.0;
    std::array<Sample, channels> pre{};
    std::array<Sample, channels> post{};
    std::array<StereoFrame, channels> pan{};
    std::array<Sample, auxCount> sendGain{};
    std::array<bool, auxCount> sendPre{};

    for (std::size_t a = 0; a < auxCount; ++a)
    {
        sendGain[a] = (a % 7 == 0) ? 0.0 : dbToGain(-6.0 - static_cast<Sample>(a % 18));
        sendPre[a] = (a % 2) == 0;
    }

    Sample phase = 0.0;
    for (int block = 0; block < blocks; ++block)
    {
        mainLeft = 0.0;
        mainRight = 0.0;
        auxSums.fill(0.0);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            phase += 0.013;
            for (std::size_t ch = 0; ch < channels; ++ch)
            {
                const Sample amplitude = 0.05 + static_cast<Sample>(ch % 13) * 0.002;
                pre[ch] = amplitude * std::sin(phase + static_cast<Sample>(ch) * 0.07);
                post[ch] = pre[ch] * dbToGain(-static_cast<Sample>(ch % 24));
                const Sample panPos = static_cast<Sample>(ch % 101) / 100.0;
                // Bounded linear pan gains are sufficient to stress the routing accumulator.
                pan[ch] = StereoFrame{ 1.0 - panPos, panPos };
                const auto& selected = (ch % 2 == 0) ? pre : post;
                for (std::size_t a = 0; a < auxCount; ++a)
                {
                    const Sample source = sendPre[a] ? selected[ch] : post[ch];
                    auxSums[a] = sanitize(auxSums[a] + source * sendGain[a]);
                }
                mainLeft = sanitize(mainLeft + post[ch] * pan[ch].left);
                mainRight = sanitize(mainRight + post[ch] * pan[ch].right);
            }
            assert(std::isfinite(mainLeft) && std::isfinite(mainRight));
            for (const auto value : auxSums) assert(std::isfinite(value));
        }
    }

    // Stress large/small cancellation in double precision and ensure invalid data is contained.
    StereoFrame cancellation{};
    accumulate(cancellation, 1.0e12, -1.0e12);
    accumulate(cancellation, 0.125, -0.125);
    accumulate(cancellation, -1.0e12, 1.0e12);
    assert(std::abs(cancellation.left - 0.125) < 1e-12);
    assert(std::abs(cancellation.right + 0.125) < 1e-12);

    std::array<Sample, 4> matrixAux { auxSums[0], auxSums[1], auxSums[2], auxSums[3] };
    const auto matrix = mixMatrixOutputs(mainLeft, mainRight, matrixAux, 1.0, false, 1.0, false);
    assert(std::isfinite(matrix.matrix1) && std::isfinite(matrix.matrix2));
    assert(toFloatBoundary(matrix.matrix1) == toFloatBoundary(matrix.matrix1));

    std::cout << "Graviton scalability stress tests passed: " << channels
              << " channels, " << auxCount << " AUX buses, " << blocks
              << " blocks x " << blockSize << " samples.\n";
    return 0;
}
