#pragma once

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// NullRing â€” types.hpp
// Fixed-point numeric types for deterministic, floating-point-free arithmetic.
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#include <cstdint>
#include <compare>
#include <ostream>
#include <string>
#include <stdexcept>
#include <type_traits>

namespace nullring {

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

    // â”€â”€ Constructors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    /// Default-construct to zero.
    constexpr FixedPoint() noexcept : raw_{0} {}

    /// Construct from an integer value (no fractional part).
    constexpr explicit FixedPoint(std::int64_t integer) noexcept
        : raw_{integer * scale} {}

    /// Construct from integer + fractional parts.
    /// Example: FixedPoint(123, 45000000) with Precision=8 represents 123.45.
    constexpr FixedPoint(std::int64_t integer, std::int64_t frac) noexcept
        : raw_{integer * scale + frac} {}

    /// Named constructor from the raw underlying value.
    [[nodiscard]] static constexpr FixedPoint from_raw(raw_type raw) noexcept {
        FixedPoint fp;
        fp.raw_ = raw;
        return fp;
    }

    // â”€â”€ Accessors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    [[nodiscard]] constexpr raw_type raw() const noexcept { return raw_; }

    /// Integer portion (truncated toward zero).
    [[nodiscard]] constexpr std::int64_t integer_part() const noexcept {
        return raw_ / scale;
    }

    /// Fractional portion as a scaled integer.
    [[nodiscard]] constexpr std::int64_t fractional_part() const noexcept {
        return raw_ % scale;
    }

    // â”€â”€ Arithmetic Operators â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    constexpr FixedPoint operator+(FixedPoint rhs) const noexcept {
        return from_raw(raw_ + rhs.raw_);
    }

    constexpr FixedPoint operator-(FixedPoint rhs) const noexcept {
        return from_raw(raw_ - rhs.raw_);
    }

    /// Multiplication: (a * b) / scale â€” uses wide arithmetic to avoid overflow.
    constexpr FixedPoint operator*(FixedPoint rhs) const noexcept {
#if defined(__GNUC__) || defined(__clang__)
        auto wide = static_cast<__int128>(raw_) * rhs.raw_;
        return from_raw(static_cast<raw_type>(wide / scale));
#else
        // MSVC: split into high/low 32-bit parts to avoid overflow.
        // For values within typical financial ranges this is safe.
        // Full 128-bit emulation can be added if needed.
        std::int64_t a_hi = raw_ / scale;
        std::int64_t a_lo = raw_ % scale;
        return from_raw(a_hi * rhs.raw_ + (a_lo * rhs.raw_) / scale);
#endif
    }

    /// Division: (a * scale) / b â€” uses wide arithmetic to avoid overflow.
    constexpr FixedPoint operator/(FixedPoint rhs) const {
        if (rhs.raw_ == 0) {
            throw std::domain_error("FixedPoint: division by zero");
        }
#if defined(__GNUC__) || defined(__clang__)
        auto wide = static_cast<__int128>(raw_) * scale;
        return from_raw(static_cast<raw_type>(wide / rhs.raw_));
#else
        // MSVC: split multiplication to prevent overflow.
        std::int64_t a_hi = raw_ / rhs.raw_;
        std::int64_t a_lo = raw_ % rhs.raw_;
        return from_raw(a_hi * scale + (a_lo * scale) / rhs.raw_);
#endif
    }

    constexpr FixedPoint& operator+=(FixedPoint rhs) noexcept {
        raw_ += rhs.raw_; return *this;
    }

    constexpr FixedPoint& operator-=(FixedPoint rhs) noexcept {
        raw_ -= rhs.raw_; return *this;
    }

    constexpr FixedPoint operator-() const noexcept {
        return from_raw(-raw_);
    }

    // â”€â”€ Comparison (C++20 three-way) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    constexpr auto operator<=>(const FixedPoint&) const noexcept = default;

    // â”€â”€ String Conversion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    [[nodiscard]] std::string to_string() const {
        auto int_part = integer_part();
        auto frac_part = fractional_part();
        if (frac_part < 0) frac_part = -frac_part;

        std::string frac_str = std::to_string(frac_part);
        // Pad with leading zeros to match precision width.
        while (static_cast<int>(frac_str.size()) < Precision) {
            frac_str = "0" + frac_str;
        }

        std::string sign = (raw_ < 0 && int_part == 0) ? "-" : "";
        return sign + std::to_string(int_part) + "." + frac_str;
    }

    friend std::ostream& operator<<(std::ostream& os, const FixedPoint& fp) {
        return os << fp.to_string();
    }

private:
    raw_type raw_;
};

// â”€â”€ Convenient Aliases â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

/// Price type â€” 8 decimal places (sub-cent granularity).
using Price    = FixedPoint<8>;

/// Quantity type â€” 4 decimal places (fractional share support).
using Quantity = FixedPoint<4>;

} // namespace nullring
