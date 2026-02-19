#pragma once

#include "types.hpp"
#include <chrono>

namespace lob {

// Intrusive doubly-linked list order node.
// No dynamic allocation per order — pointers managed by PriceLevel.
struct Order {
    OrderId id = 0;
    Price price = INVALID_PRICE;
    Quantity quantity = 0;
    Quantity filled_quantity = 0;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    OrderStatus status = OrderStatus::New;

    // Intrusive list pointers (managed by PriceLevel)
    Order* prev = nullptr;
    Order* next = nullptr;

    // Timestamp for price-time priority verification
    std::uint64_t timestamp = 0;

    Quantity remaining() const { return quantity - filled_quantity; }
    bool is_filled() const { return filled_quantity >= quantity; }

    void reset() {
        id = 0;
        price = INVALID_PRICE;
        quantity = 0;
        filled_quantity = 0;
        side = Side::Buy;
        type = OrderType::Limit;
        status = OrderStatus::New;
        prev = nullptr;
        next = nullptr;
        timestamp = 0;
    }
};

// Trade execution record
struct Trade {
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price price;
    Quantity quantity;
    std::uint64_t timestamp;
};

}  // namespace lob
