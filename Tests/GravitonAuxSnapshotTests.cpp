#include "../Source/Routing/GravitonAuxRoutingSnapshot.h"
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

using namespace nova::graviton;

using ChannelSnap = AuxRoutingSnapshot<4, 4>;
using MixerSnap = MixerAuxRoutingSnapshot<144, 4, 4>;
using MixerQueue = AuxRoutingSnapshotQueue<MixerSnap, 4>;
using MixerPublisher = AuxRoutingSnapshotPublisher<MixerSnap, 4>;

static_assert(std::is_trivially_copyable_v<ChannelSnap>, "Channel snapshot must be trivially copyable");
static_assert(std::is_trivially_copyable_v<MixerSnap>, "Mixer snapshot must be trivially copyable");

static MixerSnap makeMixerSnapshot(std::uint64_t generation, std::size_t activeSendsPerChannel)
{
    MixerSnap snap{};
    snap.generation = generation;
    for (std::size_t c = 0; c < 144; ++c)
    {
        auto& ch = snap.channels[c];
        ch.generation = generation;
        for (std::size_t a = 0; a < 4; ++a)
        {
            if (a < activeSendsPerChannel)
            {
                ch.sendGains[a] = 0.1 + static_cast<double>((c + a + generation) % 7) * 0.05;
                ch.sendPreFader[a] = ((c + a) % 2) == 0;
            }
            else
            {
                ch.sendGains[a] = 0.0;
                ch.sendPreFader[a] = false;
            }
        }
        ch.rebuildRoutes();
    }
    return snap;
}

int main()
{
    std::cout << "Starting Graviton AUX routing snapshot tests...\n";

    // 1. Channel snapshot rebuildRoutes and validity
    {
        ChannelSnap ch{};
        ch.sendGains = { 0.5, 0.0, 0.8, 0.0 };
        ch.sendPreFader = { true, false, false, false };
        ch.rebuildRoutes();
        assert(ch.valid());
        assert(ch.routeCount == 2);
        assert(ch.routes[0].auxIndex == 0 && std::abs(ch.routes[0].gain - 0.5) < 1e-9 && ch.routes[0].preFader == true);
        assert(ch.routes[1].auxIndex == 2 && std::abs(ch.routes[1].gain - 0.8) < 1e-9 && ch.routes[1].preFader == false);

        // Invalid aux index test
        ch.routes[0].auxIndex = 4;
        assert(!ch.valid());
    }

    // 2. Mixer snapshot validity
    {
        auto mixerSnap = makeMixerSnapshot(1, 2);
        assert(mixerSnap.valid());
        // Invalidate one channel route
        mixerSnap.channels[10].routes[0].auxIndex = 99;
        assert(!mixerSnap.valid());
    }

    // 3. Queue bounded saturation and tryConsumeLatest
    {
        MixerQueue queue;
        MixerSnap out{};
        assert(!queue.tryConsume(out));
        assert(!queue.tryConsumeLatest(out));

        const auto snap1 = makeMixerSnapshot(1, 1);
        const auto snap2 = makeMixerSnapshot(2, 2);
        const auto snap3 = makeMixerSnapshot(3, 3);
        const auto snap4 = makeMixerSnapshot(4, 4);
        const auto snap5 = makeMixerSnapshot(5, 1);

        assert(queue.tryPublish(snap1));
        assert(queue.tryPublish(snap2));
        assert(queue.tryPublish(snap3));
        assert(queue.tryPublish(snap4));
        // Queue capacity is 4: 5th publish must fail without overwriting unread data
        assert(!queue.tryPublish(snap5));
        assert(queue.pendingCount() == 4);

        // tryConsumeLatest should jump directly to newest published (snap4) and drain all 4 slots
        assert(queue.tryConsumeLatest(out));
        assert(out.generation == 4);
        assert(queue.pendingCount() == 0);
        assert(!queue.tryConsume(out));
        assert(!queue.tryConsumeLatest(out));

        // Now slots are free: publish snap5 should succeed
        assert(queue.tryPublish(snap5));
        assert(queue.tryConsume(out));
        assert(out.generation == 5);
        assert(!queue.tryConsume(out));
    }

    // 4. AuxRoutingSnapshotPublisher staging and saturation backpressure
    {
        MixerQueue queue;
        MixerPublisher publisher(queue);
        assert(!publisher.hasPending());

        publisher.staged() = makeMixerSnapshot(0, 2);
        assert(publisher.publishStaged());
        assert(publisher.currentGeneration() == 1);
        assert(!publisher.hasPending());

        // Publish until queue fills
        for (int i = 0; i < 3; ++i)
        {
            assert(publisher.publishStaged());
        }
        assert(queue.pendingCount() == 4);

        // Next publish should fail because queue is full, setting hasPending = true
        assert(!publisher.publishStaged());
        assert(publisher.hasPending());
        assert(publisher.currentGeneration() == 5);

        // Consumer drains latest
        MixerSnap consumerSlot{};
        assert(queue.tryConsumeLatest(consumerSlot));
        assert(consumerSlot.generation == 4);
        assert(queue.pendingCount() == 0);

        // retryPending should now succeed and clear hasPending
        assert(publisher.retryPending());
        assert(!publisher.hasPending());
        assert(queue.pendingCount() == 1);

        assert(queue.tryConsume(consumerSlot));
        assert(consumerSlot.generation == 5);
        assert(queue.pendingCount() == 0);
    }

    // 5. Mathematical equivalence: Dense vs Snapshot-Adaptive
    {
        const auto snap = makeMixerSnapshot(42, 3);
        std::array<Sample, 4> denseAux{};
        std::array<Sample, 4> adaptiveAux{};
        Sample denseL = 0.0, denseR = 0.0;
        Sample adaptiveL = 0.0, adaptiveR = 0.0;

        for (std::size_t c = 0; c < 144; ++c)
        {
            const auto& ch = snap.channels[c];
            const Sample pre = 0.45 + static_cast<Sample>(c % 7) * 0.02;
            const Sample post = 0.35 + static_cast<Sample>(c % 5) * 0.01;
            const Sample panL = 0.7071;
            const Sample panR = 0.7071;

            accumulateChannelToBuses(denseL, denseR, denseAux, pre, post,
                                    panL, panR, ch.sendGains, ch.sendPreFader);
            accumulateChannelToBusesAdaptive(adaptiveL, adaptiveR, adaptiveAux, pre, post,
                                            panL, panR, ch.sendGains, ch.sendPreFader,
                                            ch.routes, ch.routeCount);
        }

        assert(std::abs(denseL - adaptiveL) < 1e-12);
        assert(std::abs(denseR - adaptiveR) < 1e-12);
        for (std::size_t a = 0; a < 4; ++a)
        {
            assert(std::abs(denseAux[a] - adaptiveAux[a]) < 1e-12);
        }
    }

    // 6. Cross-Thread Stress & Real-Time Simulation
    // Thread 1: Control/UI Thread (Producer) rapidly modifying routing and publishing
    // Thread 2: Audio Thread (Consumer) running audio callback blocks and processing DSP
    {
        MixerQueue threadedQueue;
        std::atomic<bool> audioRunning{true};
        std::atomic<std::uint64_t> audioBlocksProcessed{0};
        std::atomic<double> maxObservedDifference{0.0};
        std::atomic<bool> nonFiniteDetected{false};

        constexpr std::size_t kIterations = 2000;

        std::thread audioThread([&]() {
            MixerSnap activeSnapshot = makeMixerSnapshot(0, 0);
            std::array<Sample, 4> auxSums{};
            Sample mainL = 0.0, mainR = 0.0;

            while (audioRunning.load(std::memory_order_relaxed))
            {
                // Real-time audio callback block start
                if (threadedQueue.tryConsumeLatest(activeSnapshot))
                {
                    assert(activeSnapshot.valid());
                }

                auxSums.fill(0.0);
                mainL = mainR = 0.0;

                // Process 128 audio samples in block
                for (int s = 0; s < 128; ++s)
                {
                    for (std::size_t c = 0; c < 144; ++c)
                    {
                        const auto& ch = activeSnapshot.channels[c];
                        const Sample pre = 0.25 * std::sin(static_cast<double>(s + c) * 0.05);
                        const Sample post = pre * 0.8;
                        const Sample panL = 0.7071;
                        const Sample panR = 0.7071;

                        accumulateChannelToBusesAdaptive(mainL, mainR, auxSums, pre, post,
                                                        panL, panR, ch.sendGains, ch.sendPreFader,
                                                        ch.routes, ch.routeCount);
                    }
                }

                if (!std::isfinite(mainL) || !std::isfinite(mainR))
                    nonFiniteDetected.store(true);
                for (auto a : auxSums)
                {
                    if (!std::isfinite(a))
                        nonFiniteDetected.store(true);
                }

                audioBlocksProcessed.fetch_add(1, std::memory_order_relaxed);
                // Simulate audio callback interval (~2.6 ms for 128 samples @ 48kHz)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });

        // Producer thread
        MixerPublisher publisher(threadedQueue);
        for (std::uint64_t i = 1; i <= kIterations; ++i)
        {
            publisher.staged() = makeMixerSnapshot(i, static_cast<std::size_t>((i % 4) + 1));
            while (!publisher.publishStaged())
            {
                // When queue is full, yield/wait briefly simulating control thread retry
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                publisher.retryPending();
            }
        }

        // Flush any remaining pending
        while (publisher.hasPending())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            publisher.retryPending();
        }

        // Let audio run a bit more to consume final state
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        audioRunning.store(false);
        audioThread.join();

        assert(!nonFiniteDetected.load());
        assert(audioBlocksProcessed.load() > 50);
        std::cout << "Cross-thread stress test passed: " << kIterations
                  << " snapshot publications across " << audioBlocksProcessed.load()
                  << " audio callback blocks.\n";
    }

    std::cout << "ALL Graviton AUX Routing Snapshot tests passed successfully.\n";
    return 0;
}
