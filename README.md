# ðŸš€ NullRing

### Ultra-Low Latency C++20 Execution Engine

*Deterministic â€¢ Cache-Aware â€¢ Hardware-Constrained Execution*

---

<p align="center">

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey)
![Architecture](https://img.shields.io/badge/Architecture-SPSC%20Lock--Free-green)
![Latency](https://img.shields.io/badge/Median%20Latency-~92ns--142ns-red)
![Determinism](https://img.shields.io/badge/Deterministic-Yes-brightgreen)
![Allocations](https://img.shields.io/badge/Allocations-Zero%20Hot%20Path-orange)

</p>

---

---

## ðŸ”¬ Abstract

NullRing is a deterministic, ultra-low latency C++20 execution pipeline designed for high-frequency trading environments. It processes streaming data in **sub-200 nanoseconds**, exploring the practical limits of user-space performance on modern x86 architectures.

The system is engineered by systematically eliminating all avoidable abstraction overhead and aligning execution with:

* CPU cache hierarchy (L1/L2/L3)
* MESI cache coherency protocol
* Inter-core data transfer latency
* OS scheduling and interrupt behavior

> NullRing operates at the boundary where latency is no longer a software problem, but a function of cache coherency physics and system-level interruptions.

---

## ðŸ§  Overview

NullRing is not a throughput-optimized system.
It is a **deterministic latency pipeline** designed to answer:

> *What is the minimum achievable latency of a user-space system when all software overhead is removed?*

The result:

* **~92ns lower-bound execution (unfenced pipeline floor)**
* **~142ns median deterministic latency (fully fenced, aligned, measured)**
* **~2â€“18Âµs tail latency (OS + hardware interrupt domain)**

Execution is reduced to:

> **cache line ownership transfer + L1-resident computation**

---

## ðŸ§  Design Philosophy

NullRing is built on a hardware-first philosophy:

> Modern latency is dominated by microarchitectural behavior, not algorithmic complexity.

### This enforces strict constraints:

* âŒ No dynamic memory allocation (`new`, `malloc`)

* âŒ No locks, mutexes, or kernel primitives

* âŒ No syscalls in the hot path

* âŒ No scheduler dependence during steady-state

* âœ… Cache-line aligned data structures

* âœ… Explicit memory ordering (`acquire/release`)

* âœ… Core affinity and isolation

* âœ… Branch prediction-aware execution design

---

## ðŸ—ï¸ System Architecture

<p align="center">
  <img src="assets/architecture.png" alt="NullRing System Architecture" width="100%">
</p>

NullRing follows a strictly isolated dual-core execution topology:

* **Core 2 â†’ Producer**
* **Core 3 â†’ Consumer**

No syscalls or kernel transitions occur in the hot path.

---

## ðŸ” End-to-End Data Flow

```text
Producer (Core 2)
    â†“
Store â†’ Cache Line enters Modified (MESI)
    â†“
SPSC Ring Buffer (Cache-Aligned, Lock-Free)
    â†“
Cache Line Ownership Transfer (MESI, ~50â€“150 cycles)
    â†“
Consumer (Core 3, Acquire Load)
    â†“
RiskEvaluator::evaluate()
    â†“
Branchless + Predictable Hybrid Compute
```

---

## âš™ï¸ Core Components

---

### ðŸ§© 1. SPSC Ring Buffer

#### Structural Properties

* Capacity: **65536 (2Â¹â¶)**
* Strict Single-Producer Single-Consumer model
* Wait-free on the hot path
* No contention, no locking

---

#### Zero-Cost Index Wrapping

```cpp
next = (idx + 1) & mask_;
```

* Eliminates integer division (`idiv`)
* Compiles to single-cycle bitwise AND

---

#### Cache Line Isolation (Critical)

```cpp
struct alignas(64) PaddedEvent {
    nullring::RiskEvent event;
    std::uint64_t enqueue_tsc;
    char padding[64 - sizeof(nullring::RiskEvent) - sizeof(std::uint64_t)];
};
```

* Exactly **64 bytes per event**
* One event = one physical cache line
* Prevents false sharing completely

---

#### Head / Tail Separation

```cpp
alignas(64) std::atomic<std::size_t> head_{0};
alignas(64) std::atomic<std::size_t> tail_{0};
```

* Eliminates MESI invalidation contention
* Producer and Consumer never fight for the same cache line

---

#### Memory Ordering Model

| Operation      | Ordering               |
| -------------- | ---------------------- |
| Producer write | `memory_order_release` |
| Consumer read  | `memory_order_acquire` |

Guarantee:

> Writes by producer become visible to consumer without full memory fencing overhead.

---

### ðŸ§  2. Risk Evaluator (Hybrid Compute Pipeline)

NullRing deliberately avoids both:

* full branching (misprediction risk)
* fully branchless everywhere (wasteful for predictable cases)

Instead, it uses a hybrid model.

---

#### âœ” Structural Path (Predictable Branching)

```cpp
if (event.quantity.raw() <= 0 || event.price.raw() <= 0) {
    return RiskResult{event.id, 0, RiskTier::LOW};
}
```

* Near-zero misprediction
* Fast-path dominates execution

---

#### âœ” Algorithmic Path (Branchless Arithmetic)

```cpp
std::int32_t price_score =
      (event.price < penny_threshold) * 400
    + (event.price >= penny_threshold && event.price < low_price) * 300;
```

* Deterministic execution
* No pipeline flushes
* Exploits superscalar ALU parallelism

---

### ðŸ”„ 3. Inter-Core Communication (MESI Physics)

The dominant cost in NullRing:

> Cache line ownership transfer between CPU cores

#### Flow:

1. Producer writes â†’ cache line enters **Modified (M)** state
2. Consumer attempts read â†’ invalidation + transfer triggered
3. Ownership migrates â†’ consumer reads locally

#### Latency:

* ~50â€“150 cycles depending on topology

This is the **true bottleneck** â€” not computation.

---

## ðŸ§  Memory Layout Evolution (Critical Optimization Journey)

---

### âŒ Initial Approach

```cpp
#pragma pack(push, 1)
```

#### Problems:

* Misaligned memory access
* Split loads across cache boundaries
* Undefined behavior risk
* Pipeline penalties on x86

---

### âœ… Final Approach

* Natural alignment restored
* Explicit padding introduced
* Compiler allowed to optimize layout safely

```cpp
static_assert(sizeof(PaddedEvent) == 64, 
              "PaddedEvent must exactly fill one cache line");
```

---

### ðŸ“Œ Outcome

> Alignment correctness achieved while preserving latency characteristics, eliminating misaligned load penalties and improving architectural validity.

---

## ðŸ§® Latency Budget Breakdown

**Median Execution (~92ns â€“ 142ns â‰ˆ 300â€“450 cycles)**

| Component                | Cycles   |
| ------------------------ | -------- |
| `__rdtscp` serialization | ~30â€“50   |
| Cache coherency transfer | ~50â€“150  |
| Cache hierarchy movement | ~80â€“120  |
| Compute (branchless ALU) | ~100â€“150 |

---

## ðŸ“Š Benchmark Results

```text
Median (p50):        ~92 ns â€“ 142 ns
p95:                 ~162 ns
p99:                 ~172 ns
p99.9:               ~2.35 Âµs
Min:                 ~82 ns
Max:                 ~82.23 Âµs
```

---

## ðŸ“ˆ Interpretation

* **92ns Floor**
  Lower bound of unfenced pipeline execution (no serialization barriers)

* **142ns Median**
  Fully fenced (`__rdtscp`), cache-aligned, deterministic execution baseline

* **p99 (~172ns)**
  Stable execution under minimal interference

* **p99.9 (~2.35Âµs â€“ 18Âµs)**
  Boundary where OS and hardware interrupts dominate

---

## âš ï¸ Tail Latency & System Boundary

### Sources of Jitter

* System Management Interrupts (SMI)
* Windows Deferred Procedure Calls (DPC)
* Timer interrupts
* Hypervisor scheduling ticks

---

### Critical Insight

> Tail latency is not caused by NullRing â€” it is imposed by the execution environment.

This represents the **hard boundary of Windows user-space determinism**.

---

## ðŸ§ª OS-Level Optimizations (Windows)

* Thread affinity:

  * Core 2 â†’ Producer
  * Core 3 â†’ Consumer

* `REALTIME_PRIORITY_CLASS`

* `THREAD_PRIORITY_TIME_CRITICAL`

* Disabled priority boosting

* Hardware spin-wait:

```cpp
_mm_pause();
```

* Memory pinning:

  * `VirtualLock()`
  * Expanded working set (â‰¥128MB)

* Pre-warming:

  * Ring buffer
  * TLB
  * Cache lines

---

## âš–ï¸ Determinism vs Throughput

| Metric              | NullRing Choice |
| ------------------- | ---------------- |
| Throughput          | ~50M+ events/sec |
| Determinism         | âœ…                |
| Tail Predictability | âœ…                |

---

### Tradeoffs

* SPSC instead of MPMC â†’ zero contention
* No batching â†’ minimum latency
* Busy spin â†’ higher CPU usage, zero scheduler involvement

---

## ðŸš§ System Limits

NullRing has reached:

> **Hardware-bound latency regime**

Further improvements are constrained by:

* Cache coherency latency (inter-core fabric)
* OS interrupt model
* CPU microarchitecture

---

## ðŸš€ Future Work: Linux RT Migration

To break the OS latency barrier:

### Required:

* `PREEMPT_RT`
* `isolcpus`
* `nohz_full`
* `rcu_nocbs`

---

### Expected Results

| Metric | Target   |
| ------ | -------- |
| Median | ~130 ns  |
| p99.9  | < 500 ns |

---

## â–¶ï¸ Build & Run

```bash
cmake -B build
cmake --build build --config Release
.\build\Release\latency_bench.exe
```

---

## ðŸ“ Project Structure

```text
nullring/
â”œâ”€â”€ include/
â”‚   â”œâ”€â”€ models.hpp
â”‚   â”œâ”€â”€ ring_buffer.hpp
â”‚   â”œâ”€â”€ types.hpp
â”‚   â””â”€â”€ evaluator.hpp
â”œâ”€â”€ benchmarks/
â”‚   â””â”€â”€ latency_bench.cpp
â”œâ”€â”€ assets/
â”‚   â””â”€â”€ architecture.png
â”œâ”€â”€ CMakeLists.txt
â””â”€â”€ README.md
```

---

## ðŸ Conclusion

NullRing is not simply an optimized program.

It is a **hardware-constrained execution experiment** demonstrating:

* Lock-free, allocation-free design
* Cache-aware memory structuring
* Deterministic execution under real-world system constraints

---

## ðŸ“Œ Final Statement

> NullRing represents the boundary where software optimization ends, and hardware physics begins.

---
