#pragma once

// ---------------------------------------------------------------------------
// GammaFlow - types.hpp
// Fixed-point numeric types for deterministic, floating-point-free arithmetic.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <compare>
#include <type_traits>

namespace gammaflow {

/// Compile-time scaling factor: 10^Precision.
/// Default precision of 8 decimal places covers sub-cent financial granularity.
template <int Precision = 8>
class FixedPoint {
    static_assert(Precision > 0 && Precision <= 18,
                  "Precision must be in [1, 18] to fit within int64_t");

public:
    /// The underlying integral representation.
    using raw_type = std::int64_t;

    /// Number of implied decimal digits.
    static constexpr int precision = Precision;

    /// The scaling factor (10^Precision), computed at compile time.
    static constexpr raw_type scale = []() constexpr {
        raw_type s = 1;
        for (int i = 0; i < Precision; ++i) s *= 10;
        return s;
    }();

    // -- Constructors --------------------------------------------------------

    /// Default-construct to zero.
    constexpr FixedPoint() noexcept : raw_{0} {}

    /// Construct from an integer value (no fractional part).
    constexpr explicit FixedPoint(std::int64_t integer) noexcept
        : raw_{integer * scale} {}

    /// Construct from integer + fractional parts.
    constexpr FixedPoint(std::int64_t integer, std::int64_t frac) noexcept
        : raw_{integer * scale + frac} {}

    /// Named constructor from the raw underlying value.
    [[nodiscard]] static constexpr FixedPoint from_raw(raw_type raw) noexcept {
        FixedPoint fp;
        fp.raw_ = raw;
        return fp;
    }

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] constexpr raw_type raw() const noexcept { return raw_; }

    /// Integer portion (truncated toward zero).
    [[nodiscard]] constexpr std::int64_t integer_part() const noexcept {
        return raw_ / scale;
    }

    /// Fractional portion as a scaled integer.
    [[nodiscard]] constexpr std::int64_t fractional_part() const noexcept {
        return raw_ % scale;
    }

    // -- Comparison (C++20 three-way) ----------------------------------------

    constexpr auto operator<=>(const FixedPoint&) const noexcept = default;

private:
    raw_type raw_;
};

} // namespace gammaflow