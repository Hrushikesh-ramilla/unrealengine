// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// NullRing â€” engine.cpp
// Implementation of the GammaEngine consumer pipeline.
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#include "engine.hpp"

#include <utility>

namespace nullring {

// â”€â”€ Construction / Destruction â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

GammaEngine::GammaEngine(ResultCallback on_result)
    : on_result_{std::move(on_result)} {}

GammaEngine::~GammaEngine() {
    stop();
}

// â”€â”€ Lifecycle â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void GammaEngine::start() {
    if (running_.load(std::memory_order_acquire)) {
        return; // Already running.
    }

    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&GammaEngine::consumer_loop, this);
}

void GammaEngine::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return; // Already stopped.
    }

    running_.store(false, std::memory_order_release);

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool GammaEngine::running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

// â”€â”€ Producer API â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool GammaEngine::submit(const RiskEvent* event) noexcept {
    return ring_.try_push(event);
}

// â”€â”€ Statistics â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::uint64_t GammaEngine::events_processed() const noexcept {
    return processed_.load(std::memory_order_relaxed);
}

// â”€â”€ Consumer Loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//
// Hot path:
//   1. try_pop() from the SPSC ring buffer.
//   2. On success â†’ evaluate â†’ optional callback â†’ increment counter.
//   3. On empty   â†’ yield the thread to avoid 100% CPU burn.
//
// std::this_thread::yield() is preferred over a sleep because:
//   - It surrenders the time-slice without a minimum sleep granularity
//     (Windows Sleep(0) / Linux sched_yield).
//   - The OS can immediately re-schedule us if new data arrives, keeping
//     tail latency low.
//
// For sub-microsecond latency requirements, yield() can be replaced with
// a PAUSE intrinsic (_mm_pause) or a busy-spin, at the cost of CPU usage.

void GammaEngine::consumer_loop() {
    while (running_.load(std::memory_order_acquire)) {
        auto maybe_event = ring_.try_pop();

        if (maybe_event.has_value()) {
            const RiskEvent* event = *maybe_event;
            RiskResult result = evaluator_.evaluate(*event);

            if (on_result_) {
                on_result_(result);
            }

            processed_.fetch_add(1, std::memory_order_relaxed);
        } else {
            // Buffer is empty â€” yield to avoid aggressive CPU spinning.
            std::this_thread::yield();
        }
    }

    // Drain any remaining events after the stop signal.
    while (true) {
        auto maybe_event = ring_.try_pop();
        if (!maybe_event.has_value()) break;

        const RiskEvent* event = *maybe_event;
        RiskResult result = evaluator_.evaluate(*event);

        if (on_result_) {
            on_result_(result);
        }

        processed_.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace nullring
