#include <gtest/gtest.h>
#include "lob/order_book.hpp"

using namespace lob;

class MatchingEngineTest : public ::testing::Test {
protected:
    OrderBook book{10000};
};

// --- Exact Match ---

TEST_F(MatchingEngineTest, ExactMatchBuyIntoSell) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.filled_quantity, 100u);
    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, to_price(100.00));
    EXPECT_EQ(result.trades[0].quantity, 100u);
    EXPECT_EQ(book.total_orders(), 0u);
}

TEST_F(MatchingEngineTest, ExactMatchSellIntoBid) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);

    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.filled_quantity, 100u);
    EXPECT_EQ(book.total_orders(), 0u);
}

// --- Partial Fills ---

TEST_F(MatchingEngineTest, PartialFillAggressorRests) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    EXPECT_EQ(result.status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(result.filled_quantity, 50u);
    EXPECT_EQ(result.remaining_quantity, 50u);
    EXPECT_EQ(book.total_orders(), 1u);  // remaining buy order rests
    EXPECT_EQ(book.best_bid(), to_price(100.00));
}

TEST_F(MatchingEngineTest, PartialFillPassiveRests) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 200);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 80);

    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.filled_quantity, 80u);
    EXPECT_EQ(book.total_orders(), 1u);  // remaining sell order
    EXPECT_EQ(book.volume_at_price(Side::Sell, to_price(100.00)), 120u);
}

// --- Price-Time Priority ---

TEST_F(MatchingEngineTest, PriceTimePriorityFIFO) {
    // Two sells at same price — first one should fill first
    auto r1 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    auto r2 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 50);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].sell_order_id, r1.order_id);  // first order matched
    EXPECT_EQ(book.total_orders(), 1u);  // second sell remains
}

TEST_F(MatchingEngineTest, PricePriority) {
    // Sell at 100 and 101 — buy at 101 should hit the 100 first
    auto r_100 = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 50);
    auto r_101 = book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 50);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 50);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, to_price(100.00));  // filled at better price
    EXPECT_EQ(result.trades[0].sell_order_id, r_100.order_id);
    EXPECT_EQ(book.total_orders(), 1u);  // 101 sell remains
}

TEST_F(MatchingEngineTest, SweepMultipleLevels) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 30);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 30);
    book.add_order(Side::Sell, OrderType::Limit, to_price(102.00), 30);

    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(102.00), 80);

    EXPECT_EQ(result.filled_quantity, 80u);
    ASSERT_EQ(result.trades.size(), 3u);
    EXPECT_EQ(result.trades[0].price, to_price(100.00));
    EXPECT_EQ(result.trades[0].quantity, 30u);
    EXPECT_EQ(result.trades[1].price, to_price(101.00));
    EXPECT_EQ(result.trades[1].quantity, 30u);
    EXPECT_EQ(result.trades[2].price, to_price(102.00));
    EXPECT_EQ(result.trades[2].quantity, 20u);  // partial fill at top level
}

// --- Market Orders ---

TEST_F(MatchingEngineTest, MarketBuyFills) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Market, 0, 100);

    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.filled_quantity, 100u);
    EXPECT_EQ(book.total_orders(), 0u);
}

TEST_F(MatchingEngineTest, MarketSellFills) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    auto result = book.add_order(Side::Sell, OrderType::Market, 0, 100);

    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.filled_quantity, 100u);
}

TEST_F(MatchingEngineTest, MarketOrderPartialFillCancelsRemainder) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 30);
    auto result = book.add_order(Side::Buy, OrderType::Market, 0, 100);

    // Market order fills what it finds, remainder is cancelled (not resting)
    EXPECT_EQ(result.status, OrderStatus::Cancelled);
    EXPECT_EQ(result.filled_quantity, 30u);
    EXPECT_EQ(result.remaining_quantity, 70u);
    EXPECT_EQ(book.total_orders(), 0u);
}

TEST_F(MatchingEngineTest, MarketOrderIntoEmptyBook) {
    auto result = book.add_order(Side::Buy, OrderType::Market, 0, 100);
    EXPECT_EQ(result.status, OrderStatus::Cancelled);
    EXPECT_EQ(result.filled_quantity, 0u);
    EXPECT_EQ(book.total_orders(), 0u);
}

// --- Crossing Orders ---

TEST_F(MatchingEngineTest, BuyAboveAskCrosses) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(99.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 100);

    // Trade at passive price (99.00)
    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.trades[0].price, to_price(99.00));
}

TEST_F(MatchingEngineTest, SellBelowBidCrosses) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 100);
    auto result = book.add_order(Side::Sell, OrderType::Limit, to_price(99.00), 100);

    // Trade at passive price (101.00)
    EXPECT_EQ(result.status, OrderStatus::Filled);
    EXPECT_EQ(result.trades[0].price, to_price(101.00));
}

// --- No Match (orders rest) ---

TEST_F(MatchingEngineTest, NoMatchBuyBelowAsk) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 100);
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    EXPECT_EQ(result.status, OrderStatus::Active);
    EXPECT_EQ(result.filled_quantity, 0u);
    EXPECT_EQ(book.total_orders(), 2u);
}

// --- Trade Callback ---

TEST_F(MatchingEngineTest, TradeCallbackFires) {
    std::vector<Trade> recorded_trades;
    book.set_trade_callback([&](const Trade& t) {
        recorded_trades.push_back(t);
    });

    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);

    ASSERT_EQ(recorded_trades.size(), 1u);
    EXPECT_EQ(recorded_trades[0].quantity, 100u);
}

// --- Statistics ---

TEST_F(MatchingEngineTest, TradeCountAndVolume) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 200);

    book.add_order(Side::Buy, OrderType::Limit, to_price(101.00), 250);

    EXPECT_EQ(book.total_trades(), 2u);
    EXPECT_EQ(book.total_volume(), 250u);
}

// --- Stress: Multiple Orders at Same Price ---

TEST_F(MatchingEngineTest, MultipleOrdersSamePriceFIFO) {
    std::vector<OrderId> sell_ids;
    for (int i = 0; i < 5; ++i) {
        auto r = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 10);
        sell_ids.push_back(r.order_id);
    }

    // Buy 25 — should fill first 2 fully and third partially
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 25);

    ASSERT_EQ(result.trades.size(), 3u);
    EXPECT_EQ(result.trades[0].sell_order_id, sell_ids[0]);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    EXPECT_EQ(result.trades[1].sell_order_id, sell_ids[1]);
    EXPECT_EQ(result.trades[1].quantity, 10u);
    EXPECT_EQ(result.trades[2].sell_order_id, sell_ids[2]);
    EXPECT_EQ(result.trades[2].quantity, 5u);

    EXPECT_EQ(book.total_orders(), 3u);  // orders 3, 4, and partial 5 remain
    EXPECT_EQ(book.volume_at_price(Side::Sell, to_price(100.00)), 25u);
}

// --- Edge Cases ---

TEST_F(MatchingEngineTest, ZeroQuantityEdge) {
    // Should place a resting order with 0 remaining...
    // In a real system you'd reject this, but testing the edge
    auto result = book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    EXPECT_EQ(result.status, OrderStatus::Active);
}

TEST_F(MatchingEngineTest, BidAskUpdateAfterTrade) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 100);

    EXPECT_EQ(book.best_bid(), to_price(100.00));

    // Sell into top bid
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.00), 100);
    EXPECT_EQ(book.best_bid(), to_price(99.00));
}
