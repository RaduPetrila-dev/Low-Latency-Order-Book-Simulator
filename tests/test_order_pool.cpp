#include <gtest/gtest.h>
#include "lob/order_pool.hpp"

using namespace lob;

TEST(OrderPoolTest, BasicAllocation) {
    OrderPool pool(100);
    EXPECT_EQ(pool.capacity(), 100u);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), 100u);

    Order* o = pool.allocate();
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.available(), 99u);
}

TEST(OrderPoolTest, AllocateAndDeallocate) {
    OrderPool pool(10);

    Order* o1 = pool.allocate();
    Order* o2 = pool.allocate();
    EXPECT_EQ(pool.size(), 2u);

    pool.deallocate(o1);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.available(), 9u);

    pool.deallocate(o2);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), 10u);
}

TEST(OrderPoolTest, ReusesMemory) {
    OrderPool pool(2);

    Order* o1 = pool.allocate();
    pool.deallocate(o1);

    Order* o2 = pool.allocate();
    // Should reuse the same slot
    EXPECT_EQ(o1, o2);
}

TEST(OrderPoolTest, ExhaustsCapacity) {
    OrderPool pool(3);

    pool.allocate();
    pool.allocate();
    pool.allocate();
    EXPECT_EQ(pool.available(), 0u);

    EXPECT_THROW(pool.allocate(), std::runtime_error);
}

TEST(OrderPoolTest, AllocatedOrderIsReset) {
    OrderPool pool(10);

    Order* o = pool.allocate();
    o->id = 42;
    o->price = 10000;
    o->quantity = 500;
    pool.deallocate(o);

    Order* o2 = pool.allocate();
    EXPECT_EQ(o2->id, 0u);
    EXPECT_EQ(o2->price, INVALID_PRICE);
    EXPECT_EQ(o2->quantity, 0u);
}
