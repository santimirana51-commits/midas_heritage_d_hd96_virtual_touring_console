#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>

namespace nova::graviton {

// Multi-writer/control-thread publication with a bounded, lock-free audio read.
// The writer mutex is never acquired by load(), so the audio callback cannot wait.
template <typename T>
class AtomicParameterSnapshot final {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Audio parameter snapshots must be trivially copyable");
    static constexpr size_t kWordCount =
        (sizeof(T) + sizeof(uint32_t) - 1u) / sizeof(uint32_t);
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "32-bit atomics must be lock-free for audio publication");

public:
    AtomicParameterSnapshot() noexcept {
        for (auto &word : words_)
            word.store(0u, std::memory_order_relaxed);
    }

    void publish(const T &value) noexcept {
        std::lock_guard<std::mutex> lock(writerMutex_);
        std::array<uint32_t, kWordCount> packed{};
        std::memcpy(packed.data(), &value, sizeof(T));
        version_.fetch_add(1u, std::memory_order_acq_rel); // odd: write in progress
        for (size_t i = 0; i < kWordCount; ++i)
            words_[i].store(packed[i], std::memory_order_relaxed);
        version_.fetch_add(1u, std::memory_order_release); // even: complete
    }

    bool load(T &destination) const noexcept {
        for (int attempt = 0; attempt < 3; ++attempt) {
            const uint32_t before = version_.load(std::memory_order_acquire);
            if ((before & 1u) != 0u)
                continue;
            std::array<uint32_t, kWordCount> packed{};
            for (size_t i = 0; i < kWordCount; ++i)
                packed[i] = words_[i].load(std::memory_order_relaxed);
            const uint32_t after = version_.load(std::memory_order_acquire);
            if (before == after && (after & 1u) == 0u) {
                std::memcpy(&destination, packed.data(), sizeof(T));
                return true;
            }
        }
        return false; // caller retains its last coherent snapshot
    }

private:
    mutable std::mutex writerMutex_;
    std::array<std::atomic<uint32_t>, kWordCount> words_{};
    std::atomic<uint32_t> version_{0u};
};

} // namespace nova::graviton
