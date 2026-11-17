#pragma once

#include "Mixer/GravitonBusIntegration.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace nova::graviton
{
// A complete, fixed-size AUX routing configuration for a single channel.
// Build snapshots off the audio thread, then publish them through AuxRoutingSnapshotQueue.
template <std::size_t AuxCount, std::size_t RouteCapacity>
struct AuxRoutingSnapshot
{
    std::array<Sample, AuxCount> sendGains{};
    std::array<bool, AuxCount> sendPreFader{};
    std::array<ActiveAuxRoute, RouteCapacity> routes{};
    std::size_t routeCount = 0;
    std::uint64_t generation = 0;

    void rebuildRoutes(Sample minGain = 0.0001) noexcept
    {
        routeCount = 0;
        for (std::size_t i = 0; i < AuxCount; ++i)
        {
            const Sample g = sendGains[i];
            if (g > minGain && routeCount < RouteCapacity)
            {
                routes[routeCount++] = ActiveAuxRoute{ i, g, sendPreFader[i] };
            }
        }
    }

    bool valid() const noexcept
    {
        if (routeCount > RouteCapacity)
            return false;
        for (std::size_t i = 0; i < routeCount; ++i)
        {
            if (routes[i].auxIndex >= AuxCount)
                return false;
        }
        return true;
    }
};

// Mixer-wide AUX routing configuration snapshot across all channels.
// Keeps the entire mixer routing state block-synchronous, lock-free, and allocation-free.
template <std::size_t Channels, std::size_t AuxCount, std::size_t RouteCapacityPerChannel = AuxCount>
struct MixerAuxRoutingSnapshot
{
    std::array<AuxRoutingSnapshot<AuxCount, RouteCapacityPerChannel>, Channels> channels{};
    std::uint64_t generation = 0;

    bool valid() const noexcept
    {
        for (std::size_t i = 0; i < Channels; ++i)
        {
            if (!channels[i].valid())
                return false;
        }
        return true;
    }
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

// Single-producer/single-consumer bounded queue. The producer is typically the
// control/UI thread; the consumer is the audio thread. No locks, allocations,
// waits, or retries occur in push/pop. A full queue rejects the new snapshot;
// the caller can retry on a later control update. Do not use with multiple
// producers or multiple consumers.
template <typename Snapshot, std::size_t Capacity = 4>
class AuxRoutingSnapshotQueue
{
    static_assert(Capacity >= 2, "Queue capacity must be at least two snapshots");
    static_assert(std::is_trivially_copyable_v<Snapshot>,
                  "Snapshots must remain trivially copyable for bounded handoff");

public:
    bool tryPublish(const Snapshot& snapshot) noexcept
    {
        if (!snapshot.valid())
            return false;

        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= Capacity)
            return false;

        slots_[head % Capacity] = snapshot;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool tryConsume(Snapshot& destination) noexcept
    {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto head = head_.load(std::memory_order_acquire);
        if (tail == head)
            return false;

        destination = slots_[tail % Capacity];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Real-time helper: consumes all pending snapshots up to the latest one in O(1) time.
    // Leaves destination unchanged and returns false if no new snapshot was published.
    bool tryConsumeLatest(Snapshot& destination) noexcept
    {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto head = head_.load(std::memory_order_acquire);
        if (tail == head)
            return false;

        const auto newest = head - 1;
        destination = slots_[newest % Capacity];
        tail_.store(head, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    std::size_t pendingCount() const noexcept
    {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return head >= tail ? (head - tail) : 0;
    }

private:
    std::array<Snapshot, Capacity> slots_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// Manages staged snapshot building and publication on the control/producer thread.
// Handles queue saturation gracefully by retaining the uncommitted snapshot in staged_
// and setting hasPending_ = true until the consumer drains the queue.
template <typename Snapshot, std::size_t QueueCapacity = 4>
class AuxRoutingSnapshotPublisher
{
public:
    using QueueType = AuxRoutingSnapshotQueue<Snapshot, QueueCapacity>;

    explicit AuxRoutingSnapshotPublisher(QueueType& queue) noexcept
        : queue_(queue) {}

    Snapshot& staged() noexcept { return staged_; }
    const Snapshot& staged() const noexcept { return staged_; }

    bool publishStaged() noexcept
    {
        staged_.generation = ++currentGeneration_;
        if (queue_.tryPublish(staged_))
        {
            hasPending_ = false;
            return true;
        }
        hasPending_ = true;
        return false;
    }

    bool publish(const Snapshot& snapshot) noexcept
    {
        staged_ = snapshot;
        return publishStaged();
    }

    bool retryPending() noexcept
    {
        if (!hasPending_)
            return true;
        if (queue_.tryPublish(staged_))
        {
            hasPending_ = false;
            return true;
        }
        return false;
    }

    bool hasPending() const noexcept { return hasPending_; }
    std::uint64_t currentGeneration() const noexcept { return currentGeneration_; }

private:
    QueueType& queue_;
    Snapshot staged_{};
    std::uint64_t currentGeneration_{0};
    bool hasPending_{false};
};

} // namespace nova::graviton
