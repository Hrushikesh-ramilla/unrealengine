#pragma once

// ---------------------------------------------------------------------------
// GammaFlow - memory_pool.hpp
// Lock-free, cache-aligned object pool with O(1) allocate / deallocate.
//
// Design rationale:
//   - Pre-allocates a fixed array of T objects to eliminate runtime heap calls.
//   - Uses a lock-free free-list (Treiber stack) built from atomic CAS so
//     multiple threads can allocate/deallocate concurrently without mutexes.
//   - Each slot is aligned to a 64-byte cache line to prevent false sharing
//     when adjacent slots are accessed by different CPU cores.
// ---------------------------------------------------------------------------

#include <atomic>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace gammaflow {

/// Sentinel value indicating the end of the free-list chain.
inline constexpr std::uint32_t POOL_NULL = UINT32_MAX;

// ---------------------------------------------------------------------------
// ObjectPool<T, Capacity>
// ---------------------------------------------------------------------------

template <typename T, std::size_t Capacity>
class ObjectPool {
    static_assert(Capacity > 0, "ObjectPool capacity must be > 0");
    static_assert(Capacity < POOL_NULL,
                  "Capacity must be < UINT32_MAX (reserved as sentinel)");

public:
    ObjectPool() noexcept {
        // Build the initial free-list: slot 0 -> 1 -> 2 -> ... -> POOL_NULL.
        for (std::uint32_t i = 0; i < Capacity - 1; ++i) {
            slots_[i].next.store(i + 1, std::memory_order_relaxed);
        }
        slots_[Capacity - 1].next.store(POOL_NULL, std::memory_order_relaxed);
        head_.store(0, std::memory_order_release);
    }

    // Non-copyable, non-movable
    ObjectPool(const ObjectPool&)            = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&)                 = delete;
    ObjectPool& operator=(ObjectPool&&)      = delete;

    /// Returns the compile-time capacity of the pool.
    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    /// Returns a pointer to the beginning of the backing storage (for tests).
    [[nodiscard]] const void* storage_base() const noexcept {
        return &slots_[0];
    }

    // TODO: implement allocate() and deallocate()

private:
    struct alignas(64) Slot {
        alignas(alignof(T)) unsigned char storage[sizeof(T)];
        std::atomic<std::uint32_t> next{POOL_NULL};
    };

    static_assert(sizeof(Slot) % 64 == 0,
                  "Slot must be a multiple of 64 bytes (cache-line aligned)");

    [[nodiscard]] T* object_ptr(std::uint32_t i) noexcept {
        return std::launder(reinterpret_cast<T*>(slots_[i].storage));
    }

    [[nodiscard]] std::uint32_t slot_index(const T* ptr) const noexcept {
        auto byte_ptr  = reinterpret_cast<const unsigned char*>(ptr);
        auto base_ptr  = reinterpret_cast<const unsigned char*>(&slots_[0]);
        return static_cast<std::uint32_t>(
            (byte_ptr - base_ptr) / sizeof(Slot));
    }

    std::array<Slot, Capacity> slots_;
    alignas(64) std::atomic<std::uint32_t> head_{0};
};

} // namespace gammaflow