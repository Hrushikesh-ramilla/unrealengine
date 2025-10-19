#pragma once

// ---------------------------------------------------------------------------
// GammaFlow - ring_buffer.hpp
// Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
//
// Design rationale:
//   - Exactly one writer thread and one reader thread operate concurrently.
//   - Memory ordering uses acquire/release semantics on the head/tail indices.
//   - Buffer capacity is always a power of two so modular indexing
//     reduces to a bitwise AND, avoiding expensive integer division.
//   - Head and tail are placed on separate cache lines to eliminate false
//     sharing between the producer and consumer cores.
// ---------------------------------------------------------------------------

#include <atomic>
#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace gammaflow {

template <typename T, std::size_t Capacity>
class SPSCRingBuffer {
    static_assert(Capacity > 1,
                  "Capacity must be > 1");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two for fast modular indexing");

public:
    SPSCRingBuffer() noexcept
        : head_{0}
        , tail_{0}
        , buffer_{} {}

    // Non-copyable, non-movable.
    SPSCRingBuffer(const SPSCRingBuffer&)            = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&)                 = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&)      = delete;

    // -- Producer API (single writer thread only) ----------------------------

    bool try_push(const T& item) noexcept {
        const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next_tail    = increment(current_tail);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // -- Consumer API (single reader thread only) ----------------------------

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const std::size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T item = buffer_[current_head];
        head_.store(increment(current_head), std::memory_order_release);
        return item;
    }

    /// Compile-time capacity.
    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] static constexpr std::size_t max_size() noexcept {
        return Capacity - 1;
    }

private:
    static constexpr std::size_t mask_ = Capacity - 1;

    [[nodiscard]] static constexpr std::size_t increment(std::size_t idx) noexcept {
        return (idx + 1) & mask_;
    }

    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    std::array<T, Capacity> buffer_;
};

} // namespace gammaflow