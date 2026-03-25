// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// NullRing â€” test_structures.cpp
// Unit tests for ObjectPool and SPSCRingBuffer.
//
// Uses standard <cassert> for zero-dependency validation.
// Run via: cmake --build build && ctest --test-dir build --verbose
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#include "memory_pool.hpp"
#include "ring_buffer.hpp"
#include "models.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

// â”€â”€ Helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

/// Pretty-print a test result.
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                                                           \
    do {                                                                     \
        std::cout << "  [RUN ] " << (name) << std::flush;                    \
    } while (0)

#define PASS()                                                               \
    do {                                                                     \
        std::cout << " âœ“\n";                                                 \
        ++tests_passed;                                                      \
    } while (0)

#define ASSERT_TRUE(expr)                                                    \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "\n  [FAIL] Assertion failed: " #expr               \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n";       \
            ++tests_failed;                                                  \
            return;                                                          \
        }                                                                    \
    } while (0)

#define ASSERT_EQ(a, b)                                                      \
    do {                                                                     \
        if ((a) != (b)) {                                                    \
            std::cerr << "\n  [FAIL] " #a " != " #b                          \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n";       \
            ++tests_failed;                                                  \
            return;                                                          \
        }                                                                    \
    } while (0)

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// ObjectPool Tests
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void test_pool_basic_allocate() {
    TEST("ObjectPool â€” basic allocate returns non-null");

    nullring::ObjectPool<nullring::RiskEvent, 4> pool;
    auto* p = pool.allocate();
    ASSERT_TRUE(p != nullptr);

    PASS();
}

void test_pool_no_heap_allocation() {
    TEST("ObjectPool â€” allocations come from internal storage, not heap");

    nullring::ObjectPool<nullring::RiskEvent, 8> pool;

    // The pool's backing storage starts at storage_base().
    auto base = reinterpret_cast<std::uintptr_t>(pool.storage_base());

    // Each slot is cache-line aligned (64 bytes); total footprint â‰¤ 8 * 128
    // (generous upper bound accounting for alignment + atomic overhead).
    constexpr std::size_t max_footprint = 8 * 128;

    for (int i = 0; i < 8; ++i) {
        auto* p = pool.allocate();
        ASSERT_TRUE(p != nullptr);

        auto addr = reinterpret_cast<std::uintptr_t>(p);
        // Every returned pointer must fall within the pool's own storage.
        ASSERT_TRUE(addr >= base && addr < base + max_footprint);
    }

    PASS();
}

void test_pool_exhaustion() {
    TEST("ObjectPool â€” returns nullptr when exhausted");

    nullring::ObjectPool<nullring::RiskEvent, 2> pool;
    auto* a = pool.allocate();
    auto* b = pool.allocate();
    ASSERT_TRUE(a != nullptr);
    ASSERT_TRUE(b != nullptr);

    // Pool of size 2 is now exhausted.
    auto* c = pool.allocate();
    ASSERT_TRUE(c == nullptr);

    PASS();
}

void test_pool_deallocate_and_reuse() {
    TEST("ObjectPool â€” deallocate makes slot available again");

    nullring::ObjectPool<nullring::RiskEvent, 1> pool;
    auto* p = pool.allocate();
    ASSERT_TRUE(p != nullptr);
    ASSERT_TRUE(pool.allocate() == nullptr); // Exhausted.

    pool.deallocate(p);

    auto* q = pool.allocate();
    ASSERT_TRUE(q != nullptr); // Slot recycled.
    ASSERT_TRUE(q == p);       // Same address returned.

    PASS();
}

void test_pool_cache_line_alignment() {
    TEST("ObjectPool â€” each slot is 64-byte aligned");

    nullring::ObjectPool<nullring::RiskEvent, 4> pool;
    for (int i = 0; i < 4; ++i) {
        auto* p = pool.allocate();
        ASSERT_TRUE(p != nullptr);
        ASSERT_TRUE(reinterpret_cast<std::uintptr_t>(p) % 64 == 0);
    }

    PASS();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// SPSCRingBuffer Tests
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void test_ring_basic_push_pop() {
    TEST("RingBuffer â€” basic push / pop round-trip");

    nullring::SPSCRingBuffer<int, 4> rb;
    ASSERT_TRUE(rb.empty());

    ASSERT_TRUE(rb.try_push(42));
    ASSERT_TRUE(!rb.empty());

    auto val = rb.try_pop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(*val, 42);
    ASSERT_TRUE(rb.empty());

    PASS();
}

void test_ring_fifo_order() {
    TEST("RingBuffer â€” FIFO ordering preserved");

    nullring::SPSCRingBuffer<int, 8> rb;
    for (int i = 0; i < 7; ++i) {       // max_size = 7
        ASSERT_TRUE(rb.try_push(i * 10));
    }

    for (int i = 0; i < 7; ++i) {
        auto val = rb.try_pop();
        ASSERT_TRUE(val.has_value());
        ASSERT_EQ(*val, i * 10);
    }

    PASS();
}

void test_ring_full_returns_false() {
    TEST("RingBuffer â€” try_push returns false when full");

    nullring::SPSCRingBuffer<int, 4> rb; // max_size = 3
    ASSERT_TRUE(rb.try_push(1));
    ASSERT_TRUE(rb.try_push(2));
    ASSERT_TRUE(rb.try_push(3));

    // Buffer should be full now (3 items = Capacity - 1).
    ASSERT_TRUE(!rb.try_push(4));

    PASS();
}

void test_ring_empty_returns_nullopt() {
    TEST("RingBuffer â€” try_pop returns nullopt when empty");

    nullring::SPSCRingBuffer<int, 4> rb;
    auto val = rb.try_pop();
    ASSERT_TRUE(!val.has_value());

    PASS();
}

void test_ring_wrap_around() {
    TEST("RingBuffer â€” correct behavior after index wrap-around");

    nullring::SPSCRingBuffer<int, 4> rb; // max_size = 3

    // Fill â†’ drain â†’ refill to force the indices past Capacity.
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(rb.try_push(cycle * 100 + i));
        }
        for (int i = 0; i < 3; ++i) {
            auto val = rb.try_pop();
            ASSERT_TRUE(val.has_value());
            ASSERT_EQ(*val, cycle * 100 + i);
        }
    }

    PASS();
}

void test_ring_with_risk_event_pointers() {
    TEST("RingBuffer â€” works with RiskEvent* payloads");

    nullring::RiskEvent event{};
    event.id = 1001;
    event.timestamp_ns = 123456789;
    event.price = nullring::Price(99, 95000000);  // 99.95
    event.quantity = nullring::Quantity(50);

    nullring::SPSCRingBuffer<nullring::RiskEvent*, 4> rb;
    ASSERT_TRUE(rb.try_push(&event));

    auto val = rb.try_pop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ((*val)->id, 1001u);
    ASSERT_EQ((*val)->timestamp_ns, 123456789);

    PASS();
}

void test_ring_spsc_concurrent() {
    TEST("RingBuffer â€” concurrent SPSC producer/consumer");

    constexpr int N = 100'000;
    nullring::SPSCRingBuffer<int, 1024> rb;

    // Producer thread.
    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!rb.try_push(i)) {
                // Spin until space is available.
            }
        }
    });

    // Consumer thread â€” verify FIFO ordering.
    int expected = 0;
    bool order_ok = true;
    std::thread consumer([&] {
        while (expected < N) {
            auto val = rb.try_pop();
            if (val.has_value()) {
                if (*val != expected) {
                    order_ok = false;
                    break;
                }
                ++expected;
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(expected, N);
    ASSERT_TRUE(order_ok);

    PASS();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Main
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

int main() {
    std::cout << "\nâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n"
              << " NullRing â€” Data Structure Tests\n"
              << "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n\n";

    std::cout << "â”€â”€ ObjectPool â”€â”€\n";
    test_pool_basic_allocate();
    test_pool_no_heap_allocation();
    test_pool_exhaustion();
    test_pool_deallocate_and_reuse();
    test_pool_cache_line_alignment();

    std::cout << "\nâ”€â”€ SPSCRingBuffer â”€â”€\n";
    test_ring_basic_push_pop();
    test_ring_fifo_order();
    test_ring_full_returns_false();
    test_ring_empty_returns_nullopt();
    test_ring_wrap_around();
    test_ring_with_risk_event_pointers();
    test_ring_spsc_concurrent();

    std::cout << "\nâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n"
              << " Results: " << tests_passed << " passed, "
              << tests_failed << " failed\n"
              << "â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n\n";

    return tests_failed > 0 ? 1 : 0;
}
