#pragma once

// ---------------------------------------------------------------------------
// GammaFlow - evaluator.hpp
// Zero-allocation fuzzy-rule risk evaluator.
// ---------------------------------------------------------------------------

#include "types.hpp"
#include "models.hpp"

#include <cstdint>

namespace gammaflow {

/// Risk tiers for human-readable classification.
enum class RiskTier : std::uint8_t {
    LOW      = 0,   //   0 - 249
    MEDIUM   = 1,   // 250 - 499
    HIGH     = 2,   // 500 - 749
    CRITICAL = 3,   // 750 - 1000
};

/// Result of a single risk evaluation -- tightly packed, trivially copyable.
#pragma pack(push, 1)
struct RiskResult {
    std::uint64_t event_id;     // Correlates back to the source RiskEvent.
    std::int32_t  score;        // [0, 1000] -- higher = riskier.
    RiskTier      tier;         // Derived classification.
};
#pragma pack(pop)

// RiskEvaluator - TODO: implement evaluate() with fuzzy scoring

class RiskEvaluator {
public:
    // -- Price thresholds (8 decimal places) ---------------------------------
    static constexpr Price penny_threshold = Price::from_raw(1'00000000LL);
    static constexpr Price low_price       = Price::from_raw(10'00000000LL);
    static constexpr Price mid_price       = Price::from_raw(100'00000000LL);
    static constexpr Price high_price      = Price::from_raw(1000'00000000LL);

    // -- Quantity thresholds (4 decimal places) ------------------------------
    static constexpr Quantity tiny_qty  = Quantity::from_raw(10'0000LL);
    static constexpr Quantity small_qty = Quantity::from_raw(100'0000LL);
    static constexpr Quantity large_qty = Quantity::from_raw(10000'0000LL);
    static constexpr Quantity huge_qty  = Quantity::from_raw(100000'0000LL);

    static constexpr std::int32_t WEIGHT_PRICE_COMPONENT = 5;
    static constexpr std::int32_t WEIGHT_QTY_COMPONENT   = 4;
};

} // namespace gammaflow