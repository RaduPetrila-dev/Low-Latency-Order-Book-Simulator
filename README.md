# Low-Latency Limit Order Book

A high-performance limit order book and matching engine written in C++17. Designed for sub-microsecond order processing with price-time priority matching.

## Performance

Benchmarked on 1,000,000 operations per test:

| Operation | Mean Latency | P50 | P99 | Throughput |
|-----------|-------------|-----|-----|------------|
| Add (no match) | ~250 ns | ~120 ns | ~1.8 μs | ~4M ops/sec |
| Cancel | ~750 ns | ~720 ns | ~1.2 μs | ~1.3M ops/sec |
| Match (aggressive) | ~320 ns | ~160 ns | ~1.5 μs | ~3.1M ops/sec |
| Mixed workload | ~280 ns | ~160 ns | ~1.0 μs | ~3.6M ops/sec |

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                      OrderBook                           │
│                                                          │
│  ┌─────────────────┐         ┌─────────────────┐        │
│  │   Bids (map)    │         │   Asks (map)    │        │
│  │  sorted desc    │         │  sorted asc     │        │
│  │                 │         │                 │        │
│  │ 100.00 ──► [O][O][O]     │ 101.00 ──► [O][O]       │
│  │  99.50 ──► [O][O]        │ 101.50 ──► [O]          │
│  │  99.00 ──► [O]           │ 102.00 ──► [O][O][O]    │
│  └─────────────────┘         └─────────────────┘        │
│                                                          │
│  ┌─────────────────┐         ┌─────────────────┐        │
│  │ Order Lookup    │         │   Order Pool    │        │
│  │ unordered_map   │         │  pre-allocated  │        │
│  │ O(1) by ID     │         │  O(1) alloc     │        │
│  └─────────────────┘         └─────────────────┘        │
└──────────────────────────────────────────────────────────┘
```

Each price level holds orders in a doubly-linked list (FIFO). The oldest order at a price executes first (time priority). The best price executes first across levels (price priority).

## Complexity Analysis

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Add order (no match) | O(log M) | M = number of price levels. Map insertion. |
| Add order (with match) | O(log M + K) | K = number of orders filled across levels |
| Cancel order | O(log M) | Hash lookup O(1) + map find O(log M) + list remove O(1) |
| Modify (reduce qty) | O(log M) | Preserves time priority |
| Modify (increase qty) | O(log M) | Loses time priority: cancel + re-add |
| Best bid/ask | O(1) | Map begin/rbegin are constant time |
| Volume at price | O(log M) | Map find |
| Order lookup by ID | O(1) | Hash table |
| Allocate/free order | O(1) | Pre-allocated memory pool |

M = number of distinct price levels in the book (typically 10-1000).

## Design Decisions

**Fixed-point integer prices.** All prices stored as `uint64_t` with 2 decimal places of precision (1 unit = $0.01). Eliminates floating-point comparison issues and avoids FPU latency on the hot path.

**Pre-allocated object pool.** All `Order` objects come from a contiguous memory pool. No `malloc`/`free` calls during order processing. The pool uses a free-list stack for O(1) allocation and deallocation.

**Intrusive doubly-linked lists.** Orders at each price level are stored in an intrusive linked list (prev/next pointers embedded in the `Order` struct). No separate node allocation. O(1) insert at tail, O(1) remove from any position.

**`std::map` for price levels.** Red-black tree provides O(log M) insert/erase and O(1) access to best bid (`rbegin`) and best ask (`begin`). For typical order books with 100-500 price levels, log M is around 7-9.

**`std::unordered_map` for order lookup.** O(1) amortized lookup by order ID for cancel and modify operations.

**Passive price execution.** Trades execute at the resting (passive) order's price, matching real exchange behavior.

**Market orders do not rest.** Unfilled market order volume is cancelled, not placed in the book.

## Project Structure

```
low-latency-order-book/
├── CMakeLists.txt          # Build system (CMake 3.16+, fetches Google Test)
├── include/lob/
│   ├── lob.hpp             # Convenience header
│   ├── types.hpp           # Price, Quantity, Side, OrderType definitions
│   ├── order.hpp           # Order struct with intrusive list pointers
│   ├── order_pool.hpp      # Pre-allocated memory pool
│   ├── price_level.hpp     # Doubly-linked list at a single price
│   └── order_book.hpp      # Matching engine interface
├── src/
│   └── order_book.cpp      # Matching engine implementation
├── tests/
│   ├── test_order_pool.cpp     # Google Test: memory pool
│   ├── test_price_level.cpp    # Google Test: linked list operations
│   ├── test_order_book.cpp     # Google Test: book state and queries
│   ├── test_matching_engine.cpp # Google Test: matching correctness
│   └── validate.cpp            # Standalone validation (no dependencies)
├── bench/
│   └── benchmark.cpp       # Latency benchmark with percentile reporting
├── examples/
│   └── main.cpp            # Interactive example with book visualisation
├── .gitignore
└── LICENSE
```

## Build

### With CMake (recommended)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./lob_tests

# Run benchmark
./lob_bench

# Run example
./lob_example
```

### Manual compilation

```bash
# Tests
g++ -std=c++17 -O0 -g -Wall -Wextra -Iinclude src/order_book.cpp tests/validate.cpp -o validate
./validate

# Benchmark
g++ -std=c++17 -O3 -march=native -DNDEBUG -Iinclude src/order_book.cpp bench/benchmark.cpp -o bench
./bench

# Example
g++ -std=c++17 -O2 -Iinclude src/order_book.cpp examples/main.cpp -o example
./example
```

## Usage

```cpp
#include "lob/lob.hpp"

using namespace lob;

int main() {
    OrderBook book(100000);  // pre-allocate pool for 100k orders

    // Optional: register trade callback
    book.set_trade_callback([](const Trade& t) {
        // handle trade event
    });

    // Add limit orders
    book.add_order(Side::Buy,  OrderType::Limit, to_price(99.50), 100);
    book.add_order(Side::Sell, OrderType::Limit, to_price(100.50), 200);

    // Aggressive order that crosses the spread
    auto result = book.add_order(Side::Buy, OrderType::Limit, to_price(100.50), 50);
    // result.trades contains execution details

    // Market order
    book.add_order(Side::Sell, OrderType::Market, 0, 75);

    // Cancel
    auto r = book.add_order(Side::Buy, OrderType::Limit, to_price(98.00), 500);
    book.cancel_order(r.order_id);

    // Query market data
    Price bid = book.best_bid();
    Price ask = book.best_ask();
    auto depth = book.bid_depth(5);  // top 5 bid levels

    return 0;
}
```

## Supported Order Types

- **Limit orders**: rest in the book if not immediately matchable
- **Market orders**: execute against available liquidity, unfilled remainder is cancelled

## Supported Operations

- **Add**: submit a new order (limit or market)
- **Cancel**: remove a resting order by ID
- **Modify**: reduce quantity (preserves time priority) or increase quantity (loses time priority)

## Testing

69 validation tests covering:
- Memory pool allocation and reuse
- FIFO ordering at each price level
- Price-time priority matching
- Partial fills on both aggressive and passive sides
- Multi-level sweeps
- Market order behavior (fill-or-cancel semantics)
- Cancel and modify correctness
- Best bid/ask tracking through trades
- Trade callback execution
- Crossing order execution at passive price

## License

MIT
