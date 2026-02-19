// Standalone validation test — no external dependencies
// The repo uses Google Test; this file is for quick verification.

#include "lob/order_book.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace lob;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << " [" << __FILE__ << ":" << __LINE__ << "]\n"; \
        ++tests_failed; \
    } else { \
        ++tests_passed; \
    } \
} while(0)

void test_order_pool() {
    std::cout << "  OrderPool...\n";
    OrderPool pool(100);
    TEST_ASSERT(pool.capacity() == 100, "capacity");
    TEST_ASSERT(pool.size() == 0, "initial size");

    Order* o1 = pool.allocate();
    TEST_ASSERT(o1 != nullptr, "allocate returns non-null");
    TEST_ASSERT(pool.size() == 1, "size after alloc");

    pool.deallocate(o1);
    TEST_ASSERT(pool.size() == 0, "size after dealloc");

    // Reuse
    Order* o2 = pool.allocate();
    TEST_ASSERT(o1 == o2, "memory reuse");

    // Exhaust
    OrderPool small(2);
    small.allocate();
    small.allocate();
    bool threw = false;
    try { small.allocate(); } catch (...) { threw = true; }
    TEST_ASSERT(threw, "throws on exhaustion");
}

void test_price_level() {
    std::cout << "  PriceLevel...\n";
    Order orders[3];
    for (int i = 0; i < 3; ++i) {
        orders[i].reset();
        orders[i].id = static_cast<OrderId>(i + 1);
        orders[i].quantity = 100;
    }

    PriceLevel level(10000);
    TEST_ASSERT(level.empty(), "empty initially");

    level.add_order(&orders[0]);
    TEST_ASSERT(!level.empty(), "not empty after add");
    TEST_ASSERT(level.front() == &orders[0], "front is first added");
    TEST_ASSERT(level.total_quantity == 100, "quantity tracking");
    TEST_ASSERT(level.order_count == 1, "order count");

    level.add_order(&orders[1]);
    level.add_order(&orders[2]);
    TEST_ASSERT(level.front() == &orders[0], "FIFO: front stays");
    TEST_ASSERT(level.total_quantity == 300, "total quantity 3 orders");

    // Remove middle
    level.remove_order(&orders[1]);
    TEST_ASSERT(level.front()->next == &orders[2], "middle removal links");
    TEST_ASSERT(level.order_count == 2, "count after remove");
    TEST_ASSERT(level.total_quantity == 200, "quantity after remove");
}

void test_add_and_query() {
    std::cout << "  Add and query...\n";
    OrderBook book(10000);

    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 200);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 150);
    book.add_order(Side::Sell, OrderType::Limit, to_price(102.00), 250);

    TEST_ASSERT(book.best_bid() == to_price(100.00), "best bid");
    TEST_ASSERT(book.best_ask() == to_price(101.00), "best ask");
    TEST_ASSERT(book.spread() == to_price(1.00), "spread");
    TEST_ASSERT(book.total_orders() == 4, "total orders");
    TEST_ASSERT(book.bid_levels() == 2, "bid levels");
    TEST_ASSERT(book.ask_levels() == 2, "ask levels");
    TEST_ASSERT(book.volume_at_price(Side::Buy, to_price(100.00)) == 200, "volume at bid");
}

void test_exact_match() {
    std::cout << "  Exact match...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    TEST_ASSERT(result.status == OrderStatus::Filled, "exact match fills");
    TEST_ASSERT(result.filled_quantity == 100, "filled qty");
    TEST_ASSERT(result.trades.size() == 1, "one trade");
    TEST_ASSERT(result.trades[0].price == to_price(100.00), "trade price");
    TEST_ASSERT(book.total_orders() == 0, "book empty after match");
}

void test_partial_fill() {
    std::cout << "  Partial fill...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    TEST_ASSERT(result.status == OrderStatus::PartiallyFilled, "partial fill status");
    TEST_ASSERT(result.filled_quantity == 50, "partial filled qty");
    TEST_ASSERT(result.remaining_quantity == 50, "remaining rests");
    TEST_ASSERT(book.total_orders() == 1, "one order rests");
    TEST_ASSERT(book.best_bid() == to_price(100.00), "remaining buy on bid");
}

void test_price_time_priority() {
    std::cout << "  Price-time priority...\n";
    OrderBook book(10000);

    auto r1 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    auto r2 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 50);

    TEST_ASSERT(result.trades.size() == 1, "one trade");
    TEST_ASSERT(result.trades[0].sell_order_id == r1.order_id, "FIFO: first order matched");
    TEST_ASSERT(book.total_orders() == 1, "second order remains");
}

void test_price_priority() {
    std::cout << "  Price priority...\n";
    OrderBook book(10000);

    auto r100 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 50);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 50);

    TEST_ASSERT(result.trades[0].price == to_price(100.00), "best price first");
    TEST_ASSERT(result.trades[0].sell_order_id == r100.order_id, "correct order matched");
}

void test_sweep_multiple_levels() {
    std::cout << "  Sweep multiple levels...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 30);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 30);
    book.add_order(Side::Sell, OrderType::Limit, to_price(102.00), 30);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(102.00), 80);

    TEST_ASSERT(result.filled_quantity == 80, "sweep filled 80");
    TEST_ASSERT(result.trades.size() == 3, "three trades across levels");
    TEST_ASSERT(result.trades[0].price == to_price(100.00), "first level");
    TEST_ASSERT(result.trades[1].price == to_price(101.00), "second level");
    TEST_ASSERT(result.trades[2].price == to_price(102.00), "third level");
    TEST_ASSERT(result.trades[2].quantity == 20, "partial at top");
}

void test_market_orders() {
    std::cout << "  Market orders...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Market, 0, 100);
    TEST_ASSERT(result.status == OrderStatus::Filled, "market buy fills");
    TEST_ASSERT(result.filled_quantity == 100, "market buy qty");

    // Market into empty book
    result = book.add_order(Side::Buy, OrderType::Market, 0, 50);
    TEST_ASSERT(result.status == OrderStatus::Cancelled, "market into empty cancels");
    TEST_ASSERT(result.filled_quantity == 0, "no fill in empty book");

    // Partial market
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 30);
    result = book.add_order(Side::Buy, OrderType::Market, 0, 100);
    TEST_ASSERT(result.filled_quantity == 30, "partial market fill");
    TEST_ASSERT(result.remaining_quantity == 70, "market remainder");
    TEST_ASSERT(result.status == OrderStatus::Cancelled, "partial market cancelled");
}

void test_cancel() {
    std::cout << "  Cancel...\n";
    OrderBook book(10000);

    auto r = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    TEST_ASSERT(book.total_orders() == 1, "order placed");

    bool ok = book.cancel_order(r.order_id);
    TEST_ASSERT(ok, "cancel succeeds");
    TEST_ASSERT(book.total_orders() == 0, "book empty");
    TEST_ASSERT(book.bid_levels() == 0, "level removed");

    ok = book.cancel_order(99999);
    TEST_ASSERT(!ok, "cancel non-existent fails");
}

void test_modify() {
    std::cout << "  Modify...\n";
    OrderBook book(10000);

    auto r = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 500);
    bool ok = book.modify_order(r.order_id, 300);
    TEST_ASSERT(ok, "modify succeeds");
    TEST_ASSERT(book.volume_at_price(Side::Buy, to_price(100.00)) == 300, "volume reduced");
}

void test_crossing_orders() {
    std::cout << "  Crossing orders...\n";
    OrderBook book(10000);

    // Buy above ask
    book.add_order(Side::Sell, OrderType::Limit, to_price(99.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 100);
    TEST_ASSERT(result.trades[0].price == to_price(99.00), "trade at passive price (ask)");

    // Sell below bid
    book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 100);
    result = book.add_order(Side::Sell, OrderType::Limit, to_price(99.00), 100);
    TEST_ASSERT(result.trades[0].price == to_price(101.00), "trade at passive price (bid)");
}

void test_trade_callback() {
    std::cout << "  Trade callback...\n";
    OrderBook book(10000);
    int callback_count = 0;

    book.set_trade_callback([&](const Trade&) { ++callback_count; });

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    TEST_ASSERT(callback_count == 1, "callback fires once");
}

void test_statistics() {
    std::cout << "  Statistics...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 200);
    book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 250);

    TEST_ASSERT(book.total_trades() == 2, "trade count");
    TEST_ASSERT(book.total_volume() == 250, "total volume");
}

void test_depth_snapshot() {
    std::cout << "  Depth snapshot...\n";
    OrderBook book(10000);

    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(102.00), 200);
    book.add_order(Side::Sell, OrderType::Limit, to_price(103.00), 300);

    auto depth = book.ask_depth(2);
    TEST_ASSERT(depth.size() == 2, "depth limit respected");
    TEST_ASSERT(depth[0].first == to_price(101.00), "best ask first");
    TEST_ASSERT(depth[1].first == to_price(102.00), "second ask");
}

void test_bid_update_after_trade() {
    std::cout << "  Best bid/ask update after trade...\n";
    OrderBook book(10000);

    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    TEST_ASSERT(book.best_bid() == to_price(100.00), "best bid before trade");

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    TEST_ASSERT(book.best_bid() == to_price(99.00), "best bid shifts down");
}

int main() {
    std::cout << "\n=== Order Book Validation Tests ===\n\n";

    test_order_pool();
    test_price_level();
    test_add_and_query();
    test_exact_match();
    test_partial_fill();
    test_price_time_priority();
    test_price_priority();
    test_sweep_multiple_levels();
    test_market_orders();
    test_cancel();
    test_modify();
    test_crossing_orders();
    test_trade_callback();
    test_statistics();
    test_depth_snapshot();
    test_bid_update_after_trade();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n\n";

    return tests_failed > 0 ? 1 : 0;
}
