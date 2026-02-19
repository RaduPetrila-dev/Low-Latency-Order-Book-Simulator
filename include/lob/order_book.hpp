#pragma once

#include "types.hpp"
#include "order.hpp"
#include "price_level.hpp"
#include "order_pool.hpp"

#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>

namespace lob {

// Result of an order submission
struct OrderResult {
    OrderId order_id = 0;
    OrderStatus status = OrderStatus::New;
    Quantity filled_quantity = 0;
    Quantity remaining_quantity = 0;
    std::vector<Trade> trades;
};

// Callback types for market data events
using TradeCallback = std::function<void(const Trade&)>;

class OrderBook {
public:
    explicit OrderBook(std::size_t pool_capacity = 1'000'000);

    // Core operations
    OrderResult add_order(Side side, OrderType type, Price price, Quantity quantity);
    bool cancel_order(OrderId order_id);
    bool modify_order(OrderId order_id, Quantity new_quantity);

    // Market data queries — all O(1)
    Price best_bid() const;
    Price best_ask() const;
    Price spread() const;
    Quantity volume_at_price(Side side, Price price) const;
    std::uint32_t order_count_at_price(Side side, Price price) const;

    // Book state
    std::size_t total_orders() const { return orders_.size(); }
    std::size_t bid_levels() const { return bids_.size(); }
    std::size_t ask_levels() const { return asks_.size(); }
    bool empty() const { return orders_.empty(); }

    // Depth snapshot: returns (price, quantity) pairs from best to worst
    std::vector<std::pair<Price, Quantity>> bid_depth(std::size_t levels) const;
    std::vector<std::pair<Price, Quantity>> ask_depth(std::size_t levels) const;

    // Register trade callback
    void set_trade_callback(TradeCallback cb) { trade_callback_ = std::move(cb); }

    // Statistics
    std::uint64_t total_trades() const { return trade_count_; }
    std::uint64_t total_volume() const { return total_volume_; }

private:
    // Match an incoming order against the opposite side of the book
    void match_order(Order* order, OrderResult& result);
    void match_against_asks(Order* order, OrderResult& result);
    void match_against_bids(Order* order, OrderResult& result);

    // Execute a trade between two orders
    void execute_trade(Order* aggressive, Order* passive, Quantity qty, OrderResult& result);

    // Insert a resting order into the book
    void insert_into_book(Order* order);

    // Remove empty price levels
    void remove_price_level_if_empty(Side side, Price price);

    // Generate monotonic order IDs and timestamps
    OrderId next_order_id() { return ++next_id_; }
    std::uint64_t next_timestamp() { return ++timestamp_counter_; }

    // Price levels: bids sorted descending (rbegin = best), asks sorted ascending (begin = best)
    std::map<Price, PriceLevel> bids_;
    std::map<Price, PriceLevel> asks_;

    // O(1) order lookup by ID
    std::unordered_map<OrderId, Order*> orders_;

    // Memory pool: zero heap allocation on hot path
    OrderPool pool_;

    // Counters
    OrderId next_id_ = 0;
    std::uint64_t timestamp_counter_ = 0;
    std::uint64_t trade_count_ = 0;
    std::uint64_t total_volume_ = 0;

    // Optional callback
    TradeCallback trade_callback_;
};

}  // namespace lob
