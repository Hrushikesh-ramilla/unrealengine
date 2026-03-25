#pragma once

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// NullRing â€” engine.hpp
// Core execution engine that ties the ring buffer, object pool, and risk
// evaluator into a continuously running consumer pipeline.
//
// Architecture:
//   Producer (user thread)           Consumer (background thread)
//   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”              â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
//   â”‚  submit(event)  â”‚â”€â”€â”€ SPSC â”€â”€â”€â–¶â”‚  poll â†’ evaluate     â”‚
//   â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜   RingBuf   â”‚  store RiskResult    â”‚
//                                    â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#include "ring_buffer.hpp"
#include "evaluator.hpp"
#include "models.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

namespace nullring {

/// Default ring buffer depth â€” 64K slots (must be power of two).
inline constexpr std::size_t DEFAULT_RING_CAPACITY = 65536;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// GammaEngine
//
// Owns:
//   â€¢ A lock-free SPSC ring buffer of RiskEvent pointers.
//   â€¢ A background consumer thread that continuously drains the buffer and
//     passes each event through the RiskEvaluator.
//   â€¢ An optional user-supplied callback invoked with every RiskResult.
//
// Thread model:
//   â€“ Exactly ONE producer thread calls submit().
//   â€“ Exactly ONE consumer thread (spawned internally) calls poll().
//   â€“ The ring buffer enforces the SPSC contract.
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

class GammaEngine {
public:
    /// Optional callback for processed results.
    using ResultCallback = std::function<void(const RiskResult&)>;

    /// Construct the engine.  Does NOT start the consumer thread until
    /// start() is called.
    explicit GammaEngine(ResultCallback on_result = nullptr);

    // Non-copyable, non-movable (owns a thread).
    GammaEngine(const GammaEngine&)            = delete;
    GammaEngine& operator=(const GammaEngine&) = delete;
    GammaEngine(GammaEngine&&)                 = delete;
    GammaEngine& operator=(GammaEngine&&)      = delete;

    /// Destructor â€” signals the consumer to stop and joins the thread.
    ~GammaEngine();

    // â”€â”€ Lifecycle â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    /// Spawn the background consumer thread.
    void start();

    /// Signal the consumer to stop and join the thread.
    void stop();

    /// Returns true if the consumer thread is running.
    [[nodiscard]] bool running() const noexcept;

    // â”€â”€ Producer API (single thread only) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    /// Enqueue a RiskEvent pointer for evaluation.
    /// Returns false if the ring buffer is full (back-pressure).
    bool submit(const RiskEvent* event) noexcept;

    // â”€â”€ Statistics â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    /// Number of events successfully evaluated by the consumer.
    [[nodiscard]] std::uint64_t events_processed() const noexcept;

private:
    /// Main consumer loop â€” runs on the background thread.
    void consumer_loop();

    // â”€â”€ Members â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    SPSCRingBuffer<const RiskEvent*, DEFAULT_RING_CAPACITY> ring_;
    RiskEvaluator                                           evaluator_;
    ResultCallback                                          on_result_;

    std::atomic<bool>        running_{false};
    std::atomic<std::uint64_t> processed_{0};
    std::thread              worker_;
};

} // namespace nullring
