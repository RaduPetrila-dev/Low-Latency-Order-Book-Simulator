#pragma once

#include <cstdint>
#include <limits>

namespace lob {

// Fixed-point price representation: 1 unit = 0.01 (1 cent)
// Avoids floating-point arithmetic on the hot path
using Price = std::uint64_t;
using Quantity = std::uint64_t;
using OrderId = std::uint64_t;

// Price constants
constexpr Price PRICE_MULTIPLIER = 100;  // 2 decimal places
constexpr Price INVALID_PRICE = 0;
constexpr Price MAX_PRICE = std::numeric_limits<Price>::max();

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : std::uint8_t {
    Limit = 0,
    Market = 1
};

enum class OrderStatus : std::uint8_t {
    New = 0,
    Active = 1,
    PartiallyFilled = 2,
    Filled = 3,
    Cancelled = 4
};

// Inline conversion helpers
inline Price to_price(double p) {
    return static_cast<Price>(p * PRICE_MULTIPLIER + 0.5);
}

inline double to_double(Price p) {
    return static_cast<double>(p) / PRICE_MULTIPLIER;
}

}  // namespace lob
