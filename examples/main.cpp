#include "lob/order_book.hpp"
#include <iostream>
#include <iomanip>

using namespace lob;

void print_book(const OrderBook& book) {
    std::cout << "\n=== Order Book ===\n";

    auto asks = book.ask_depth(5);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  ASK " << std::fixed << std::setprecision(2)
                  << to_double(it->first) << "  |  " << it->second << "\n";
    }

    std::cout << "  --------------------\n";

    auto bids = book.bid_depth(5);
    for (const auto& [price, qty] : bids) {
        std::cout << "  BID " << std::fixed << std::setprecision(2)
                  << to_double(price) << "  |  " << qty << "\n";
    }

    std::cout << "  Spread: " << to_double(book.spread()) << "\n";
    std::cout << "  Orders: " << book.total_orders()
              << "  Trades: " << book.total_trades()
              << "  Volume: " << book.total_volume() << "\n";
}

int main() {
    OrderBook book(100000);

    // Register trade callback
    book.set_trade_callback([](const Trade& t) {
        std::cout << "[TRADE] Price=" << std::fixed << std::setprecision(2)
                  << to_double(t.price) << " Qty=" << t.quantity
                  << " Buy#" << t.buy_order_id << " Sell#" << t.sell_order_id << "\n";
    });

    // Build an order book
    std::cout << "--- Adding resting orders ---\n";
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 500);
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.50), 300);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 200);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.50), 150);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 400);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.50), 250);

    print_book(book);

    // Aggressive buy that sweeps multiple levels
    std::cout << "\n--- Aggressive buy: 200 @ 101.00 ---\n";
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 200);
    std::cout << "Filled: " << result.filled_quantity
              << "  Remaining: " << result.remaining_quantity
              << "  Trades: " << result.trades.size() << "\n";

    print_book(book);

    // Market sell
    std::cout << "\n--- Market sell: 400 ---\n";
    result = book.add_order(Side::Sell, OrderType::Market, 0, 400);
    std::cout << "Filled: " << result.filled_quantity
              << "  Remaining: " << result.remaining_quantity << "\n";

    print_book(book);

    return 0;
}
