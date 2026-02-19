#include <gtest/gtest.h>
#include "lob/order_book.hpp"

using namespace lob;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book{10000};
};

// --- Basic Order Placement ---

TEST_F(OrderBookTest, AddBuyLimitOrder) {
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 50);
    EXPECT_EQ(result.status, OrderStatus::Active);
    EXPECT_EQ(result.remaining_quantity, 50u);
    EXPECT_EQ(result.filled_quantity, 0u);
    EXPECT_EQ(book.total_orders(), 1u);
    EXPECT_EQ(book.bid_levels(), 1u);
}

TEST_F(OrderBookTest, AddSellLimitOrder) {
    auto result = book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 30);
    EXPECT_EQ(result.status, OrderStatus::Active);
    EXPECT_EQ(book.total_orders(), 1u);
    EXPECT_EQ(book.ask_levels(), 1u);
}

TEST_F(OrderBookTest, MultipleBidLevels) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 200);
    book.add_order(Side::Buy, OrderType::Limit, to_price(98.00), 150);

    EXPECT_EQ(book.bid_levels(), 3u);
    EXPECT_EQ(book.best_bid(), to_price(100.00));
}

TEST_F(OrderBookTest, MultipleAskLevels) {
    book.add_order(Side::Sell, OrderType::Limit, to_price(101.00), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(102.00), 200);
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.50), 150);

    EXPECT_EQ(book.ask_levels(), 3u);
    EXPECT_EQ(book.best_ask(), to_price(100.50));
}

// --- Market Data Queries ---

TEST_F(OrderBookTest, SpreadCalculation) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.50), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.50), 100);

    EXPECT_EQ(book.spread(), to_price(1.00));
}

TEST_F(OrderBookTest, EmptyBookReturnsInvalidPrice) {
    EXPECT_EQ(book.best_bid(), INVALID_PRICE);
    EXPECT_EQ(book.best_ask(), INVALID_PRICE);
    EXPECT_EQ(book.spread(), INVALID_PRICE);
}

TEST_F(OrderBookTest, VolumeAtPrice) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 200);

    EXPECT_EQ(book.volume_at_price(Side::Buy, to_price(100.00)), 300u);
    EXPECT_EQ(book.volume_at_price(Side::Buy, to_price(99.00)), 0u);
}

TEST_F(OrderBookTest, OrderCountAtPrice) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 200);
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 50);

    EXPECT_EQ(book.order_count_at_price(Side::Buy, to_price(100.00)), 3u);
}

TEST_F(OrderBookTest, DepthSnapshot) {
    book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    book.add_order(Side::Buy, OrderType::Limit, to_price(99.00), 200);
    book.add_order(Side::Buy, OrderType::Limit, to_price(98.00), 300);

    auto depth = book.bid_depth(2);
    ASSERT_EQ(depth.size(), 2u);
    EXPECT_EQ(depth[0].first, to_price(100.00));
    EXPECT_EQ(depth[0].second, 100u);
    EXPECT_EQ(depth[1].first, to_price(99.00));
    EXPECT_EQ(depth[1].second, 200u);
}

// --- Cancel and Modify ---

TEST_F(OrderBookTest, CancelOrder) {
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    EXPECT_EQ(book.total_orders(), 1u);

    bool cancelled = book.cancel_order(result.order_id);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(book.total_orders(), 0u);
    EXPECT_EQ(book.bid_levels(), 0u);
}

TEST_F(OrderBookTest, CancelNonExistentOrder) {
    bool cancelled = book.cancel_order(99999);
    EXPECT_FALSE(cancelled);
}

TEST_F(OrderBookTest, ModifyReduceQuantity) {
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 500);
    bool modified = book.modify_order(result.order_id, 300);
    EXPECT_TRUE(modified);
    EXPECT_EQ(book.volume_at_price(Side::Buy, to_price(100.00)), 300u);
}

TEST_F(OrderBookTest, CancelRemovesPriceLevel) {
    auto r1 = book.add_order(Side::Buy, OrderType::Limit, to_price(100.00), 100);
    EXPECT_EQ(book.bid_levels(), 1u);

    book.cancel_order(r1.order_id);
    EXPECT_EQ(book.bid_levels(), 0u);
}
