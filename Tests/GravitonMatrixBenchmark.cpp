#include "../Source/Routing/GravitonMatrixRouting.h"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

namespace
{
template <std::size_t AuxCount, std::size_t MatrixCount>
void benchmarkMatrix(std::size_t iterations)
{
    nova::graviton::MatrixRoutingSnapshot<AuxCount, MatrixCount> snapshot;
    for (std::size_t m = 0; m < MatrixCount; ++m)
    {
        snapshot.matrices[m].mainLeftGain = 0.7071;
        snapshot.matrices[m].mainRightGain = 0.7071;
        for (std::size_t a = 0; a < std::min<std::size_t>(AuxCount, 8); ++a)
        {
            snapshot.matrices[m].auxSendGains[a] = 0.5;
        }
        snapshot.matrices[m].masterGain = 1.0;
        snapshot.matrices[m].rebuildActiveTaps();
    }

    std::array<double, AuxCount> auxBuffer {};
    for (std::size_t a = 0; a < AuxCount; ++a)
        auxBuffer[a] = 0.1 * static_cast<double>((a % 5) + 1);

    std::array<double, MatrixCount> outputs {};

    // Warm-up
    for (std::size_t i = 0; i < 500; ++i)
        nova::graviton::processMatrixBusesAdaptive(outputs, 0.4, 0.3, auxBuffer, snapshot);

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i)
    {
        nova::graviton::processMatrixBusesAdaptive(outputs, 0.4, 0.3, auxBuffer, snapshot);
    }
    const auto end = std::chrono::steady_clock::now();

    const double elapsedNs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    const double nsPerSample = elapsedNs / static_cast<double>(iterations);

    std::cout << std::setw(8) << AuxCount
              << std::setw(10) << MatrixCount
              << std::setw(16) << std::fixed << std::setprecision(1) << nsPerSample
              << std::setw(16) << (outputs[0] > 0.0 ? "valid" : "zero")
              << std::endl;
}
} // namespace

int main(int argc, char** argv)
{
    std::size_t iterations = 50000;
    if (argc > 1)
        iterations = static_cast<std::size_t>(std::max(1000, std::atoi(argv[1])));

    std::cout << "Graviton Matrix Routing Microbenchmark (synthetic, no device I/O)\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "     AUX    Matrix    ns/sample (avg)      Status\n";

    benchmarkMatrix<4, 2>(iterations);
    benchmarkMatrix<8, 4>(iterations);
    benchmarkMatrix<16, 8>(iterations);
    benchmarkMatrix<24, 12>(iterations);
    benchmarkMatrix<96, 24>(iterations);

    return 0;
}

