#pragma once

#include "Utilities/BusConfigPersistence.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace nova
{
// Triple-buffered, fixed-capacity publication of a scalable bus execution plan.
//
// Contract:
// - Exactly one control-thread publisher calls publish().
// - Exactly one audio thread calls activatePendingAtBlockBoundary() and
//   activePlan().
// - publish() performs validation and builds a plan off the audio thread.
// - The audio thread performs only bounded atomic operations at block boundary;
//   processing samples reads the immutable active plan without locks or allocation.
// - If both spare slots are occupied, publish() returns false; caller may retry.
class SafeBusGraphActivation
{
public:
    SafeBusGraphActivation() noexcept
    {
        ScalableBusGraphConfig defaults;
        (void) buildScalableBusGraphPlan(defaults, slots_[0].plan);
        slots_[0].generation = 0;
        slots_[0].state.store(State::Active, std::memory_order_relaxed);
        slots_[1].state.store(State::Free, std::memory_order_relaxed);
        slots_[2].state.store(State::Free, std::memory_order_relaxed);
    }

    SafeBusGraphActivation(const SafeBusGraphActivation&) = delete;
    SafeBusGraphActivation& operator=(const SafeBusGraphActivation&) = delete;

    bool publish(const ScalableBusGraphConfig& config) noexcept
    {
        // Validation may scan the full configuration; this method is control-thread only.
        if (!isValidBusConfig(config))
            return false;

        int slotIndex = -1;
        for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        {
            State expected = State::Free;
            if (slots_[static_cast<std::size_t>(i)].state.compare_exchange_strong(
                    expected, State::Writing, std::memory_order_acq_rel,
                    std::memory_order_relaxed))
            {
                slotIndex = i;
                break;
            }
        }
        if (slotIndex < 0)
            return false;

        auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
        if (!buildScalableBusGraphPlan(config, slot.plan))
        {
            slot.state.store(State::Free, std::memory_order_release);
            return false;
        }

        slot.generation = nextGeneration_.fetch_add(1, std::memory_order_relaxed) + 1;
        slot.state.store(State::Ready, std::memory_order_release);
        return true;
    }

    // Call once at the beginning of an audio block. Does not lock or allocate.
    // Returns true only when a new plan became active.
    bool activatePendingAtBlockBoundary() noexcept
    {
        int newestIndex = -1;
        std::uint64_t newestGeneration = 0;
        for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        {
            auto& slot = slots_[static_cast<std::size_t>(i)];
            if (slot.state.load(std::memory_order_acquire) == State::Ready
                && slot.generation >= newestGeneration)
            {
                newestGeneration = slot.generation;
                newestIndex = i;
            }
        }
        if (newestIndex < 0)
            return false;

        State expected = State::Ready;
        auto& next = slots_[static_cast<std::size_t>(newestIndex)];
        if (!next.state.compare_exchange_strong(expected, State::Active,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed))
            return false;

        const int previousIndex = activeIndex_;
        activeIndex_ = newestIndex;

        // Discard stale ready plans. A publisher can only claim Free slots.
        for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        {
            if (i == newestIndex || i == activeIndex_)
                continue;
            State ready = State::Ready;
            (void) slots_[static_cast<std::size_t>(i)].state.compare_exchange_strong(
                ready, State::Free, std::memory_order_acq_rel,
                std::memory_order_relaxed);
        }

        if (previousIndex != newestIndex)
            slots_[static_cast<std::size_t>(previousIndex)].state.store(State::Free,
                                                                        std::memory_order_release);
        return true;
    }

    const ScalableBusGraphPlan& activePlan() const noexcept
    {
        return slots_[static_cast<std::size_t>(activeIndex_)].plan;
    }

    std::uint64_t activeGeneration() const noexcept
    {
        return slots_[static_cast<std::size_t>(activeIndex_)].generation;
    }

private:
    enum class State : unsigned char { Free, Writing, Ready, Active };

    struct Slot
    {
        ScalableBusGraphPlan plan {};
        std::atomic<State> state { State::Free };
        // Written before Ready release-store; read after Ready acquire-load.
        std::uint64_t generation = 0;
    };

    std::array<Slot, 3> slots_ {};
    // activeIndex_ is audio-thread-owned after construction.
    int activeIndex_ = 0;
    std::atomic<std::uint64_t> nextGeneration_ { 0 };
};
} // namespace nova
