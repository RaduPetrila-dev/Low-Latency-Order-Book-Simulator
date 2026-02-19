#pragma once

#include "order.hpp"

namespace lob {

// Doubly-linked list of orders at a single price point.
// All operations O(1). No heap allocation.
struct PriceLevel {
    Price price = INVALID_PRICE;
    Quantity total_quantity = 0;
    std::uint32_t order_count = 0;
    Order* head = nullptr;  // oldest order (first to execute)
    Order* tail = nullptr;  // newest order

    PriceLevel() = default;
    explicit PriceLevel(Price p) : price(p) {}

    bool empty() const { return head == nullptr; }

    // O(1) — append order to tail (FIFO: oldest at head executes first)
    void add_order(Order* order) {
        order->prev = tail;
        order->next = nullptr;
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
        total_quantity += order->remaining();
        ++order_count;
    }

    // O(1) — remove order from anywhere in the list
    void remove_order(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }
        total_quantity -= order->remaining();
        --order_count;
        order->prev = nullptr;
        order->next = nullptr;
    }

    // O(1) — peek at the oldest order (front of queue)
    Order* front() const { return head; }
};

}  // namespace lob
