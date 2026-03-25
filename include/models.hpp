#pragma once

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// NullRing â€” models.hpp
// Core data models for the risk analytics pipeline.
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#include "types.hpp"

#include <cstdint>
#include <array>
#include <chrono>
#include <string_view>

namespace nullring {

/// Nanosecond-precision timestamp for event ordering.
using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

/// Fixed-size instrument identifier (8 bytes, null-padded).
/// Using a fixed array avoids heap allocation and keeps the struct trivially
/// copyable for zero-cost serialization into shared memory / ring buffers.
using InstrumentId = std::array<char, 8>;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// RiskEvent
//
// Represents a single market event flowing through the risk pipeline.
// The struct is tightly packed (#pragma pack) to eliminate padding bytes,
// ensuring deterministic memory layout for:
//   - Binary serialization without an encoding layer
//   - Cache-line-friendly batching
//   - Memory-mapped I/O and IPC ring buffers
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct RiskEvent {
    /// Monotonically increasing event identifier.
    std::uint64_t id;

    /// Nanosecond-resolution event timestamp (epoch-relative).
    /// Stored as raw nanoseconds rather than chrono::time_point to guarantee
    /// a trivial, packed memory layout.
    std::int64_t timestamp_ns;

    /// Instrument symbol, e.g. "AAPL", "TSLA". Null-padded to 8 bytes.
    InstrumentId instrument;

    /// Trade / quote price in fixed-point representation (8 decimal places).
    Price price;

    /// Trade / quote quantity in fixed-point representation (4 decimal places).
    Quantity quantity;
};

// Compile-time layout assertions â€” fail fast if assumptions break.
static_assert(sizeof(RiskEvent) ==
    sizeof(std::uint64_t)   +   // id              â€” 8
    sizeof(std::int64_t)    +   // timestamp_ns    â€” 8
    sizeof(InstrumentId)    +   // instrument      â€” 8
    sizeof(Price)           +   // price           â€” 8
    sizeof(Quantity),           // quantity         â€” 8
    "RiskEvent must be tightly packed with no padding"
);

static_assert(std::is_trivially_copyable_v<RiskEvent>,
              "RiskEvent must be trivially copyable for zero-copy I/O");

} // namespace nullring
