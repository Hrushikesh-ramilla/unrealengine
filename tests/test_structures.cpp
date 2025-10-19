// ---------------------------------------------------------------------------
// GammaFlow â€” test_structures.cpp
// Unit tests for ObjectPool
// ---------------------------------------------------------------------------

#include "../include/memory_pool.hpp"
#include "../include/ring_buffer.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

// â”€â”€ Helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct TestPayload {
    std::uint64_t a;
    std::uint64_t b;
    std::uint64_t c;
};

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]\n"; \
            ++tests_failed; \
        } else { \
            ++tests_passed; \
        } \
    } while (0)

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// ObjectPool Tests
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

static void test_pool_basic_allocation() {
    gammaflow::ObjectPool<TestPayload, 4> pool;
    auto* p1 = pool.allocate();
    auto* p2 = pool.allocate();
    auto* p3 = pool.allocate();
    auto* p4 = pool.allocate();
    ASSERT_TRUE(p1 != nullptr, "pool alloc 1");
    ASSERT_TRUE(p2 != nullptr, "pool alloc 2");
    ASSERT_TRUE(p3 != nullptr, "pool alloc 3");
    ASSERT_TRUE(p4 != nullptr, "pool alloc 4");
    ASSERT_TRUE(p1 != p2 && p2 != p3 && p3 != p4, "pool allocs unique");
}

static void test_pool_exhaustion() {
    gammaflow::ObjectPool<TestPayload, 2> pool;
    auto* p1 = pool.allocate();
    auto* p2 = pool.allocate();
    auto* p3 = pool.allocate();
    ASSERT_TRUE(p1 != nullptr, "pool exhaust alloc 1");
    ASSERT_TRUE(p2 != nullptr, "pool exhaust alloc 2");
    ASSERT_TRUE(p3 == nullptr, "pool exhausted returns nullptr");
}

static void test_pool_reuse_after_dealloc() {
    gammaflow::ObjectPool<TestPayload, 2> pool;
    auto* p1 = pool.allocate();
    auto* p2 = pool.allocate();
    ASSERT_TRUE(pool.allocate() == nullptr, "pool full before dealloc");
    pool.deallocate(p1);
    auto* p3 = pool.allocate();
    ASSERT_TRUE(p3 != nullptr, "pool realloc after dealloc");
    ASSERT_TRUE(p3 == p1, "pool reuses same slot");
}

static void test_pool_cache_line_alignment() {
    gammaflow::ObjectPool<TestPayload, 8> pool;
    auto* p1 = pool.allocate();
    auto* p2 = pool.allocate();
    auto addr1 = reinterpret_cast<std::uintptr_t>(p1);
    auto addr2 = reinterpret_cast<std::uintptr_t>(p2);
    ASSERT_TRUE(addr1 % 64 == 0, "pool slot 1 aligned to 64 bytes");
    ASSERT_TRUE(addr2 % 64 == 0, "pool slot 2 aligned to 64 bytes");
    ASSERT_TRUE((addr2 - addr1) >= 64, "pool slots on separate cache lines");
}

static void test_pool_capacity() {
    gammaflow::ObjectPool<TestPayload, 16> pool;
    ASSERT_TRUE(pool.capacity() == 16, "pool capacity() matches template arg");
}

int main() {
    std::cout << "\n=== GammaFlow Unit Tests ===\n\n";

    std::cout << "-- ObjectPool Tests --\n";
    test_pool_basic_allocation();
    test_pool_exhaustion();
    test_pool_reuse_after_dealloc();
    test_pool_cache_line_alignment();
    test_pool_capacity();

    std::cout << "\nResults: " << tests_passed << " passed, "
              << tests_failed << " failed\n\n";

    return tests_failed > 0 ? 1 : 0;
}