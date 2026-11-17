#include "../Source/Routing/GravitonMatrixRouting.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED check at line " << __LINE__ << ": " #cond << std::endl; \
            std::exit(1); \
        } \
    } while(0)

namespace
{
void testDefaultEquivalenceWithLegacy()
{
    std::cout << "[TEST] Matrix Default Equivalence with Legacy..." << std::endl;

    constexpr std::size_t AuxCount = 4;
    constexpr std::size_t MatrixCount = 2;

    nova::graviton::MatrixRoutingSnapshot<AuxCount, MatrixCount> snapshot;
    // Configure default Matrix 1: Main Mono sum (each 0.5 linear gain)
    snapshot.matrices[0].mainLeftGain = 0.5;
    snapshot.matrices[0].mainRightGain = 0.5;
    snapshot.matrices[0].masterGain = 1.0;
    snapshot.matrices[0].masterMuted = false;

    // Configure default Matrix 2: AUX 1..4 sum (each 1.0 linear gain)
    snapshot.matrices[1].mainLeftGain = 0.0;
    snapshot.matrices[1].mainRightGain = 0.0;
    for (std::size_t a = 0; a < AuxCount; ++a)
    {
        snapshot.matrices[1].auxSendGains[a] = 1.0;
        snapshot.matrices[1].auxSendMuted[a] = false;
    }
    snapshot.matrices[1].masterGain = 1.0;
    snapshot.matrices[1].masterMuted = false;
    snapshot.rebuildAllTaps();

    REQUIRE(snapshot.matrices[0].activeAuxTapCount == 0);
    REQUIRE(snapshot.matrices[1].activeAuxTapCount == 4);

    const double mainL = 0.4;
    const double mainR = 0.2;
    const std::array<double, 4> aux { 0.1, -0.05, 0.25, 0.02 };

    std::array<double, MatrixCount> matrixOutputs {};
    nova::graviton::processMatrixBusesAdaptive(matrixOutputs, mainL, mainR, aux, snapshot);

    const double expectedMatrix1 = 0.5 * (mainL + mainR);
    const double expectedMatrix2 = aux[0] + aux[1] + aux[2] + aux[3];

    REQUIRE(std::abs(matrixOutputs[0] - expectedMatrix1) < 1.0e-12);
    REQUIRE(std::abs(matrixOutputs[1] - expectedMatrix2) < 1.0e-12);
    std::cout << "  PASS: Default matrix routes match legacy formulas exactly." << std::endl;
}

void testArbitraryMatrixSendsAndMutes()
{
    std::cout << "[TEST] Arbitrary Matrix Sends and Mutes..." << std::endl;

    constexpr std::size_t AuxCount = 4;
    constexpr std::size_t MatrixCount = 2;

    nova::graviton::MatrixRoutingSnapshot<AuxCount, MatrixCount> snapshot;
    // Matrix 1: Only Main Left at -6 dB (~0.501187) and AUX 2 at unity (1.0)
    snapshot.matrices[0].mainLeftGain = 0.5;
    snapshot.matrices[0].mainRightGain = 0.0;
    snapshot.matrices[0].auxSendGains[1] = 1.0;
    snapshot.matrices[0].masterGain = 2.0;

    // Matrix 2: AUX 3 only, master muted
    snapshot.matrices[1].auxSendGains[2] = 0.75;
    snapshot.matrices[1].masterMuted = true;
    snapshot.rebuildAllTaps();

    REQUIRE(snapshot.matrices[0].activeAuxTapCount == 1);
    REQUIRE(snapshot.matrices[0].activeAuxTaps[0].auxIndex == 1);
    REQUIRE(snapshot.matrices[1].activeAuxTapCount == 1);

    const double mainL = 0.6;
    const double mainR = 0.9;
    const std::array<double, 4> aux { 0.1, 0.3, 0.5, 0.7 };

    std::array<double, MatrixCount> matrixOutputs {};
    nova::graviton::processMatrixBusesAdaptive(matrixOutputs, mainL, mainR, aux, snapshot);

    const double expectedM1 = (mainL * 0.5 + aux[1] * 1.0) * 2.0;
    REQUIRE(std::abs(matrixOutputs[0] - expectedM1) < 1.0e-12);
    REQUIRE(matrixOutputs[1] == 0.0); // Muted

    // Test individual AUX mute on Matrix 1
    snapshot.matrices[0].auxSendMuted[1] = true;
    snapshot.rebuildAllTaps();
    REQUIRE(snapshot.matrices[0].activeAuxTapCount == 0);

    nova::graviton::processMatrixBusesAdaptive(matrixOutputs, mainL, mainR, aux, snapshot);
    const double expectedM1AfterMute = (mainL * 0.5) * 2.0;
    REQUIRE(std::abs(matrixOutputs[0] - expectedM1AfterMute) < 1.0e-12);

    std::cout << "  PASS: Arbitrary matrix sends and mutes verified." << std::endl;
}

void testSanitizationAndGuards()
{
    std::cout << "[TEST] Matrix Sanitization and Guards..." << std::endl;

    constexpr std::size_t AuxCount = 4;
    constexpr std::size_t MatrixCount = 2;

    nova::graviton::MatrixRoutingSnapshot<AuxCount, MatrixCount> snapshot;
    snapshot.matrices[0].mainLeftGain = std::numeric_limits<double>::quiet_NaN();
    snapshot.matrices[0].mainRightGain = std::numeric_limits<double>::infinity();
    snapshot.matrices[0].auxSendGains[0] = 1.0;
    snapshot.matrices[0].masterGain = std::numeric_limits<double>::quiet_NaN();
    snapshot.rebuildAllTaps();

    const double mainL = std::numeric_limits<double>::quiet_NaN();
    const double mainR = std::numeric_limits<double>::infinity();
    const std::array<double, 4> aux { std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0 };

    std::array<double, MatrixCount> matrixOutputs {};
    nova::graviton::processMatrixBusesAdaptive(matrixOutputs, mainL, mainR, aux, snapshot);

    REQUIRE(std::isfinite(matrixOutputs[0]));
    REQUIRE(std::isfinite(matrixOutputs[1]));
    REQUIRE(matrixOutputs[0] == 0.0);
    REQUIRE(matrixOutputs[1] == 0.0);

    std::cout << "  PASS: Non-finite values safely recovered." << std::endl;
}

void testQueuePublishAndBackpressure()
{
    std::cout << "[TEST] Matrix Queue SPSC & Backpressure..." << std::endl;

    using Snap = nova::graviton::MatrixRoutingSnapshot<4, 2>;
    nova::graviton::MatrixRoutingSnapshotQueue<Snap, 4> queue;
    nova::graviton::MatrixRoutingSnapshotPublisher<Snap, 4> publisher(queue);

    Snap consumerSnapshot;
    REQUIRE(!queue.tryConsumeLatest(consumerSnapshot));

    publisher.staged().matrices[0].mainLeftGain = 0.25;
    REQUIRE(publisher.publishStaged());
    REQUIRE(publisher.staged().generation == 1);

    publisher.staged().matrices[0].mainLeftGain = 0.50;
    REQUIRE(publisher.publishStaged());
    REQUIRE(publisher.staged().generation == 2);

    publisher.staged().matrices[0].mainLeftGain = 0.75;
    REQUIRE(publisher.publishStaged());
    REQUIRE(publisher.staged().generation == 3);

    publisher.staged().matrices[0].mainLeftGain = 1.00;
    REQUIRE(publisher.publishStaged());
    REQUIRE(publisher.staged().generation == 4);

    // Queue is full (capacity 4): next publish must fail (backpressure)
    publisher.staged().matrices[0].mainLeftGain = 1.25;
    REQUIRE(!publisher.publishStaged());
    REQUIRE(publisher.hasPendingPublish());

    // Consumer consumes latest in O(1)
    REQUIRE(queue.tryConsumeLatest(consumerSnapshot));
    REQUIRE(consumerSnapshot.generation == 4);
    REQUIRE(consumerSnapshot.matrices[0].mainLeftGain == 1.00);

    // After consume, pending snapshot can retry and succeed
    REQUIRE(publisher.retryPending());
    REQUIRE(!publisher.hasPendingPublish());

    REQUIRE(queue.tryConsumeLatest(consumerSnapshot));
    REQUIRE(consumerSnapshot.generation == 5);
    REQUIRE(consumerSnapshot.matrices[0].mainLeftGain == 1.25);

    std::cout << "  PASS: Queue SPSC and backpressure verified." << std::endl;
}

void testMultiThreadedStress()
{
    std::cout << "[TEST] Matrix Multi-Threaded Stress Test..." << std::endl;

    using Snap = nova::graviton::MatrixRoutingSnapshot<4, 2>;
    nova::graviton::MatrixRoutingSnapshotQueue<Snap, 8> queue;

    std::atomic<bool> stopProducer { false };
    std::atomic<std::size_t> publishedCount { 0 };
    std::atomic<std::size_t> consumedCount { 0 };
    std::atomic<std::size_t> nonFiniteCount { 0 };

    constexpr std::size_t kIterations = 5000;

    std::thread producer([&]()
    {
        Snap staged;
        for (std::size_t i = 1; i <= kIterations; ++i)
        {
            staged.generation = i;
            staged.matrices[0].mainLeftGain = 0.1 * static_cast<double>(i % 10);
            staged.matrices[0].mainRightGain = 0.1 * static_cast<double>((i + 5) % 10);
            staged.matrices[1].auxSendGains[i % 4] = 0.25 * static_cast<double>((i % 4) + 1);
            staged.rebuildAllTaps();

            while (!queue.tryPublish(staged))
            {
                std::this_thread::yield();
            }
            publishedCount.fetch_add(1, std::memory_order_relaxed);
        }
        stopProducer.store(true, std::memory_order_release);
    });

    std::thread audioConsumer([&]()
    {
        Snap active;
        std::uint64_t lastGen = 0;
        std::array<double, 2> matrixOut {};
        const std::array<double, 4> aux { 0.2, 0.4, 0.6, 0.8 };

        while (!stopProducer.load(std::memory_order_acquire) || queue.pendingCount() > 0)
        {
            if (queue.tryConsumeLatest(active))
            {
                REQUIRE(active.generation >= lastGen);
                lastGen = active.generation;
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            }

            nova::graviton::processMatrixBusesAdaptive(matrixOut, 0.5, 0.5, aux, active);
            if (!std::isfinite(matrixOut[0]) || !std::isfinite(matrixOut[1]))
                nonFiniteCount.fetch_add(1, std::memory_order_relaxed);
        }
    });

    producer.join();
    audioConsumer.join();

    REQUIRE(publishedCount.load() == kIterations);
    REQUIRE(nonFiniteCount.load() == 0);
    std::cout << "  PASS: " << kIterations << " matrix snapshots safely published and processed without race or non-finite outputs." << std::endl;
}

} // namespace

int main()
{
    std::cout << "=== Graviton Matrix Routing Unit & Stress Tests ===" << std::endl;
    testDefaultEquivalenceWithLegacy();
    testArbitraryMatrixSendsAndMutes();
    testSanitizationAndGuards();
    testQueuePublishAndBackpressure();
    testMultiThreadedStress();
    std::cout << "ALL Graviton Matrix Routing tests passed successfully." << std::endl;
    return 0;
}
