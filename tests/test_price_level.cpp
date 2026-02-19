#include <gtest/gtest.h>
#include "lob/price_level.hpp"

using namespace lob;

class PriceLevelTest : public ::testing::Test {
protected:
    Order orders[5];

    void SetUp() override {
        for (int i = 0; i < 5; ++i) {
            orders[i].reset();
            orders[i].id = static_cast<OrderId>(i + 1);
            orders[i].quantity = 100;
        }
    }
};

TEST_F(PriceLevelTest, EmptyLevel) {
    PriceLevel level(10000);
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, 0u);
    EXPECT_EQ(level.order_count, 0u);
    EXPECT_EQ(level.front(), nullptr);
}

TEST_F(PriceLevelTest, AddSingleOrder) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);

    EXPECT_FALSE(level.empty());
    EXPECT_EQ(level.total_quantity, 100u);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.front(), &orders[0]);
}

TEST_F(PriceLevelTest, FIFOOrdering) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);
    level.add_order(&orders[1]);
    level.add_order(&orders[2]);

    // Front should be the first order added
    EXPECT_EQ(level.front(), &orders[0]);
    EXPECT_EQ(level.front()->next, &orders[1]);
    EXPECT_EQ(level.front()->next->next, &orders[2]);
    EXPECT_EQ(level.total_quantity, 300u);
    EXPECT_EQ(level.order_count, 3u);
}

TEST_F(PriceLevelTest, RemoveHead) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);
    level.add_order(&orders[1]);

    level.remove_order(&orders[0]);
    EXPECT_EQ(level.front(), &orders[1]);
    EXPECT_EQ(level.total_quantity, 100u);
    EXPECT_EQ(level.order_count, 1u);
}

TEST_F(PriceLevelTest, RemoveTail) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);
    level.add_order(&orders[1]);

    level.remove_order(&orders[1]);
    EXPECT_EQ(level.front(), &orders[0]);
    EXPECT_EQ(level.front()->next, nullptr);
    EXPECT_EQ(level.order_count, 1u);
}

TEST_F(PriceLevelTest, RemoveMiddle) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);
    level.add_order(&orders[1]);
    level.add_order(&orders[2]);

    level.remove_order(&orders[1]);
    EXPECT_EQ(level.front(), &orders[0]);
    EXPECT_EQ(level.front()->next, &orders[2]);
    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.total_quantity, 200u);
}

TEST_F(PriceLevelTest, RemoveAllOrders) {
    PriceLevel level(10000);
    level.add_order(&orders[0]);
    level.add_order(&orders[1]);

    level.remove_order(&orders[0]);
    level.remove_order(&orders[1]);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity, 0u);
    EXPECT_EQ(level.order_count, 0u);
}

TEST_F(PriceLevelTest, QuantityTracksPartialFills) {
    PriceLevel level(10000);
    orders[0].quantity = 500;
    orders[0].filled_quantity = 200;  // remaining = 300

    level.add_order(&orders[0]);
    EXPECT_EQ(level.total_quantity, 300u);
}
