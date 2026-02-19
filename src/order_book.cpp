#include "lob/order_book.hpp"
#include <algorithm>

namespace lob {

OrderBook::OrderBook(std::size_t pool_capacity)
    : pool_(pool_capacity) {
    orders_.reserve(pool_capacity / 2);
}

OrderResult OrderBook::add_order(Side side, OrderType type, Price price, Quantity quantity) {
    OrderResult result;

    Order* order = pool_.allocate();
    order->id = next_order_id();
    order->side = side;
    order->type = type;
    order->price = price;
    order->quantity = quantity;
    order->filled_quantity = 0;
    order->status = OrderStatus::Active;
    order->timestamp = next_timestamp();

    result.order_id = order->id;

    // Attempt to match against opposite side
    match_order(order, result);

    if (order->is_filled()) {
        // Fully filled — return to pool
        order->status = OrderStatus::Filled;
        result.status = OrderStatus::Filled;
        result.filled_quantity = order->filled_quantity;
        result.remaining_quantity = 0;
        pool_.deallocate(order);
    } else if (type == OrderType::Limit) {
        // Resting order — insert into book
        if (order->filled_quantity > 0) {
            order->status = OrderStatus::PartiallyFilled;
        }
        insert_into_book(order);
        orders_[order->id] = order;

        result.status = order->status;
        result.filled_quantity = order->filled_quantity;
        result.remaining_quantity = order->remaining();
    } else {
        // Unfilled market order — no resting, return to pool
        result.status = OrderStatus::Cancelled;
        result.filled_quantity = order->filled_quantity;
        result.remaining_quantity = order->remaining();
        pool_.deallocate(order);
    }

    return result;
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }

    Order* order = it->second;
    Price price = order->price;
    Side side = order->side;

    // Remove from price level
    auto& levels = (side == Side::Buy) ? bids_ : asks_;
    auto level_it = levels.find(price);
    if (level_it != levels.end()) {
        level_it->second.remove_order(order);
        if (level_it->second.empty()) {
            levels.erase(level_it);
        }
    }

    // Remove from lookup and return to pool
    orders_.erase(it);
    order->status = OrderStatus::Cancelled;
    pool_.deallocate(order);
    return true;
}

bool OrderBook::modify_order(OrderId order_id, Quantity new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }

    Order* order = it->second;

    // Reducing quantity preserves time priority
    if (new_quantity <= order->filled_quantity) {
        // Effectively a cancel
        return cancel_order(order_id);
    }

    if (new_quantity < order->quantity) {
        // Reduce: update the price level total
        Quantity old_remaining = order->remaining();
        order->quantity = new_quantity;
        Quantity new_remaining = order->remaining();

        auto& levels = (order->side == Side::Buy) ? bids_ : asks_;
        auto level_it = levels.find(order->price);
        if (level_it != levels.end()) {
            level_it->second.total_quantity -= (old_remaining - new_remaining);
        }
        return true;
    }

    if (new_quantity > order->quantity) {
        // Increase: loses time priority — cancel and re-add
        Side side = order->side;
        Price price = order->price;
        cancel_order(order_id);
        add_order(side, OrderType::Limit, price, new_quantity);
        return true;
    }

    return true;  // same quantity, no-op
}

// --- Matching Engine (hot path) ---

void OrderBook::match_order(Order* order, OrderResult& result) {
    if (order->side == Side::Buy) {
        match_against_asks(order, result);
    } else {
        match_against_bids(order, result);
    }
}

void OrderBook::match_against_asks(Order* order, OrderResult& result) {
    // Buy order matches against asks from lowest price upward
    auto it = asks_.begin();
    while (it != asks_.end() && order->remaining() > 0) {
        // Limit order: stop if ask price exceeds our limit
        if (order->type == OrderType::Limit && it->first > order->price) {
            break;
        }

        PriceLevel& level = it->second;
        Order* passive = level.front();

        while (passive && order->remaining() > 0) {
            Order* next_passive = passive->next;
            Quantity trade_qty = std::min(order->remaining(), passive->remaining());
            execute_trade(order, passive, trade_qty, result);

            if (passive->is_filled()) {
                level.remove_order(passive);
                orders_.erase(passive->id);
                passive->status = OrderStatus::Filled;
                pool_.deallocate(passive);
            }
            passive = next_passive;
        }

        if (level.empty()) {
            it = asks_.erase(it);
        } else {
            ++it;
        }
    }
}

void OrderBook::match_against_bids(Order* order, OrderResult& result) {
    // Sell order matches against bids from highest price downward
    auto it = bids_.rbegin();
    while (it != bids_.rend() && order->remaining() > 0) {
        // Limit order: stop if bid price is below our limit
        if (order->type == OrderType::Limit && it->first < order->price) {
            break;
        }

        PriceLevel& level = it->second;
        Order* passive = level.front();

        while (passive && order->remaining() > 0) {
            Order* next_passive = passive->next;
            Quantity trade_qty = std::min(order->remaining(), passive->remaining());
            execute_trade(order, passive, trade_qty, result);

            if (passive->is_filled()) {
                level.remove_order(passive);
                orders_.erase(passive->id);
                passive->status = OrderStatus::Filled;
                pool_.deallocate(passive);
            }
            passive = next_passive;
        }

        if (level.empty()) {
            // Erase via base iterator (standard reverse_iterator erase pattern)
            bids_.erase(std::next(it).base());
            it = bids_.rbegin();
        } else {
            ++it;
        }
    }
}

void OrderBook::execute_trade(Order* aggressive, Order* passive, Quantity qty, OrderResult& result) {
    aggressive->filled_quantity += qty;
    passive->filled_quantity += qty;

    // Update passive's price level total quantity
    auto& levels = (passive->side == Side::Buy) ? bids_ : asks_;
    auto level_it = levels.find(passive->price);
    if (level_it != levels.end()) {
        level_it->second.total_quantity -= qty;
    }

    Trade trade;
    trade.price = passive->price;  // trade at passive (resting) order's price
    trade.quantity = qty;
    trade.timestamp = timestamp_counter_;

    if (aggressive->side == Side::Buy) {
        trade.buy_order_id = aggressive->id;
        trade.sell_order_id = passive->id;
    } else {
        trade.buy_order_id = passive->id;
        trade.sell_order_id = aggressive->id;
    }

    ++trade_count_;
    total_volume_ += qty;
    result.trades.push_back(trade);

    if (trade_callback_) {
        trade_callback_(trade);
    }
}

void OrderBook::insert_into_book(Order* order) {
    auto& levels = (order->side == Side::Buy) ? bids_ : asks_;
    auto [it, inserted] = levels.try_emplace(order->price, order->price);
    it->second.add_order(order);
}

// --- Market Data Queries ---

Price OrderBook::best_bid() const {
    if (bids_.empty()) return INVALID_PRICE;
    return bids_.rbegin()->first;
}

Price OrderBook::best_ask() const {
    if (asks_.empty()) return INVALID_PRICE;
    return asks_.begin()->first;
}

Price OrderBook::spread() const {
    Price bid = best_bid();
    Price ask = best_ask();
    if (bid == INVALID_PRICE || ask == INVALID_PRICE) return INVALID_PRICE;
    return ask - bid;
}

Quantity OrderBook::volume_at_price(Side side, Price price) const {
    const auto& levels = (side == Side::Buy) ? bids_ : asks_;
    auto it = levels.find(price);
    if (it == levels.end()) return 0;
    return it->second.total_quantity;
}

std::uint32_t OrderBook::order_count_at_price(Side side, Price price) const {
    const auto& levels = (side == Side::Buy) ? bids_ : asks_;
    auto it = levels.find(price);
    if (it == levels.end()) return 0;
    return it->second.order_count;
}

std::vector<std::pair<Price, Quantity>> OrderBook::bid_depth(std::size_t levels) const {
    std::vector<std::pair<Price, Quantity>> depth;
    depth.reserve(levels);
    std::size_t count = 0;
    for (auto it = bids_.rbegin(); it != bids_.rend() && count < levels; ++it, ++count) {
        depth.emplace_back(it->first, it->second.total_quantity);
    }
    return depth;
}

std::vector<std::pair<Price, Quantity>> OrderBook::ask_depth(std::size_t levels) const {
    std::vector<std::pair<Price, Quantity>> depth;
    depth.reserve(levels);
    std::size_t count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < levels; ++it, ++count) {
        depth.emplace_back(it->first, it->second.total_quantity);
    }
    return depth;
}

}  // namespace lob
