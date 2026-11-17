#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace nova::graviton
{
// Compact active tap from an AUX bus into a specific matrix.
struct ActiveMatrixAuxTap
{
    std::size_t auxIndex = 0;
    Sample gain = 0.0;
};

// Routing and gain configuration for a single Matrix bus.
template <std::size_t AuxCount, std::size_t MaxAuxTaps = AuxCount>
struct MatrixBusSnapshot
{
    static_assert(MaxAuxTaps >= AuxCount, "MaxAuxTaps must be >= AuxCount");
    Sample mainLeftGain = 0.0;
    Sample mainRightGain = 0.0;
    bool mainMuted = false;
    std::array<Sample, AuxCount> auxSendGains {};
    std::array<bool, AuxCount> auxSendMuted {};
    std::array<ActiveMatrixAuxTap, MaxAuxTaps> activeAuxTaps {};
    std::size_t activeAuxTapCount = 0;
    Sample masterGain = 1.0;
    bool masterMuted = false;

    constexpr MatrixBusSnapshot() noexcept
    {
        auxSendGains.fill(0.0);
        auxSendMuted.fill(false);
    }

    void rebuildActiveTaps(Sample minGain = 1.0e-5) noexcept
    {
        activeAuxTapCount = 0;
        for (std::size_t a = 0; a < AuxCount; ++a)
        {
            if (auxSendMuted[a])
                continue;
            const Sample g = clampFinite(auxSendGains[a], 0.0, 16.0, 0.0);
            if (g >= minGain && activeAuxTapCount < MaxAuxTaps)
            {
                activeAuxTaps[activeAuxTapCount++] = ActiveMatrixAuxTap { a, g };
            }
        }
    }
};

// Snapshot of the entire matrix routing grid across all Matrix buses.
template <std::size_t AuxCount, std::size_t MatrixCount, std::size_t MaxAuxTaps = AuxCount>
struct MatrixRoutingSnapshot
{
    std::array<MatrixBusSnapshot<AuxCount, MaxAuxTaps>, MatrixCount> matrices {};
    std::uint64_t generation = 0;

    constexpr MatrixRoutingSnapshot() noexcept = default;

    void rebuildAllTaps(Sample minGain = 1.0e-5) noexcept
    {
        for (auto& matrix : matrices)
            matrix.rebuildActiveTaps(minGain);
    }
};

// Process arbitrary matrix bus sends in 64-bit double precision.
template <std::size_t AuxCount, std::size_t MatrixCount, std::size_t MaxAuxTaps>
inline void processMatrixBusesAdaptive(
    std::array<Sample, MatrixCount>& matrixOutputs,
    Sample mainLeft,
    Sample mainRight,
    const std::array<Sample, AuxCount>& masteredAux,
    const MatrixRoutingSnapshot<AuxCount, MatrixCount, MaxAuxTaps>& snapshot) noexcept
{
    const Sample sanitizedMainL = sanitize(mainLeft);
    const Sample sanitizedMainR = sanitize(mainRight);

    for (std::size_t m = 0; m < MatrixCount; ++m)
    {
        const auto& bus = snapshot.matrices[m];
        if (bus.masterMuted)
        {
            matrixOutputs[m] = 0.0;
            continue;
        }

        Sample sum = 0.0;
        if (!bus.mainMuted)
        {
            const Sample gL = clampFinite(bus.mainLeftGain, 0.0, 16.0, 0.0);
            const Sample gR = clampFinite(bus.mainRightGain, 0.0, 16.0, 0.0);
            if (gL > 0.0)
                sum += sanitizedMainL * gL;
            if (gR > 0.0)
                sum += sanitizedMainR * gR;
        }

        // Adaptive accumulation: iterate active non-zero AUX taps
        const std::size_t tapCount = std::min(bus.activeAuxTapCount, MaxAuxTaps);
        for (std::size_t t = 0; t < tapCount; ++t)
        {
            const auto& tap = bus.activeAuxTaps[t];
            if (tap.auxIndex < AuxCount)
            {
                const Sample auxSample = sanitize(masteredAux[tap.auxIndex]);
                sum += auxSample * tap.gain;
            }
        }

        const Sample master = clampFinite(bus.masterGain, 0.0, 16.0, 0.0);
        matrixOutputs[m] = sanitize(sum * master);
    }
}

// Lock-free single-producer single-consumer queue for Matrix snapshots.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
template <typename Snapshot, std::size_t Capacity = 4>
class MatrixRoutingSnapshotQueue final
{
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    MatrixRoutingSnapshotQueue() noexcept
    {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }

    bool tryPublish(const Snapshot& snapshot) noexcept
    {
        const std::size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const std::size_t currentRead = readIndex.load(std::memory_order_acquire);
        if (currentWrite - currentRead >= Capacity)
            return false;

        buffer[currentWrite & mask] = snapshot;
        writeIndex.store(currentWrite + 1, std::memory_order_release);
        return true;
    }

    bool tryConsumeLatest(Snapshot& destination) noexcept
    {
        const std::size_t currentRead = readIndex.load(std::memory_order_relaxed);
        const std::size_t currentWrite = writeIndex.load(std::memory_order_acquire);
        if (currentRead >= currentWrite)
            return false;

        destination = buffer[(currentWrite - 1) & mask];
        readIndex.store(currentWrite, std::memory_order_release);
        return true;
    }

    std::size_t pendingCount() const noexcept
    {
        const std::size_t r = readIndex.load(std::memory_order_relaxed);
        const std::size_t w = writeIndex.load(std::memory_order_relaxed);
        return w >= r ? (w - r) : 0;
    }

private:
    static constexpr std::size_t mask = Capacity - 1;
    alignas(64) std::array<Snapshot, Capacity> buffer {};
    alignas(64) std::atomic<std::size_t> writeIndex { 0 };
    alignas(64) std::atomic<std::size_t> readIndex { 0 };
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// Publisher helper with staged snapshot and retry support.
template <typename Snapshot, std::size_t Capacity = 4>
class MatrixRoutingSnapshotPublisher final
{
public:
    explicit MatrixRoutingSnapshotPublisher(MatrixRoutingSnapshotQueue<Snapshot, Capacity>& targetQueue) noexcept
        : queue(targetQueue) {}

    Snapshot& staged() noexcept { return stagedSnapshot; }
    const Snapshot& staged() const noexcept { return stagedSnapshot; }

    bool publishStaged() noexcept
    {
        stagedSnapshot.generation++;
        if (queue.tryPublish(stagedSnapshot))
        {
            hasPending = false;
            return true;
        }
        hasPending = true;
        return false;
    }

    bool retryPending() noexcept
    {
        if (!hasPending)
            return true;
        if (queue.tryPublish(stagedSnapshot))
        {
            hasPending = false;
            return true;
        }
        return false;
    }

    bool hasPendingPublish() const noexcept { return hasPending; }

private:
    MatrixRoutingSnapshotQueue<Snapshot, Capacity>& queue;
    Snapshot stagedSnapshot {};
    bool hasPending = false;
};

} // namespace nova::graviton

