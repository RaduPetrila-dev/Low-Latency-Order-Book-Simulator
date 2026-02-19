#include "lob/order_book.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>
#include <numeric>
#include <cmath>

using namespace lob;
using Clock = std::chrono::high_resolution_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct LatencyStats {
    std::string name;
    std::size_t count;
    double mean_ns;
    double median_ns;
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
    double min_ns;
    double max_ns;
    double throughput;  // operations per second
};

LatencyStats compute_stats(const std::string& name, std::vector<double>& latencies) {
    std::sort(latencies.begin(), latencies.end());
    std::size_t n = latencies.size();

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);

    LatencyStats stats;
    stats.name = name;
    stats.count = n;
    stats.mean_ns = sum / static_cast<double>(n);
    stats.median_ns = latencies[n / 2];
    stats.p50_ns = latencies[static_cast<std::size_t>(n * 0.50)];
    stats.p90_ns = latencies[static_cast<std::size_t>(n * 0.90)];
    stats.p99_ns = latencies[static_cast<std::size_t>(n * 0.99)];
    stats.p999_ns = latencies[static_cast<std::size_t>(n * 0.999)];
    stats.min_ns = latencies.front();
    stats.max_ns = latencies.back();
    stats.throughput = 1'000'000'000.0 / stats.mean_ns;

    return stats;
}

void print_stats(const LatencyStats& s) {
    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  " << std::left << std::setw(25) << s.name
              << " n=" << std::setw(10) << s.count
              << " mean=" << std::setw(8) << s.mean_ns << "ns"
              << " p50=" << std::setw(8) << s.p50_ns << "ns"
              << " p90=" << std::setw(8) << s.p90_ns << "ns"
              << " p99=" << std::setw(8) << s.p99_ns << "ns"
              << " p99.9=" << std::setw(8) << s.p999_ns << "ns"
              << " | " << std::setprecision(2) << s.throughput / 1'000'000.0 << "M ops/sec\n";
}

void print_separator() {
    std::cout << std::string(130, '-') << "\n";
}

// --- Benchmarks ---

void bench_add_limit_orders(std::size_t n) {
    OrderBook book(n + 1000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<Price> price_dist(9000, 11000);  // $90.00 - $110.00
    std::uniform_int_distribution<Quantity> qty_dist(1, 1000);

    std::vector<double> latencies;
    latencies.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = price_dist(rng);
        // Keep bids below asks to avoid matching
        if (side == Side::Buy) price = std::min(price, Price(9999));
        else price = std::max(price, Price(10001));
        Quantity qty = qty_dist(rng);

        auto start = Clock::now();
        book.add_order(side, OrderType::Limit, price, qty);
        auto end = Clock::now();

        latencies.push_back(
            static_cast<double>(std::chrono::duration_cast<Nanoseconds>(end - start).count())
        );
    }

    auto stats = compute_stats("Add (no match)", latencies);
    print_stats(stats);
}

void bench_cancel_orders(std::size_t n) {
    OrderBook book(n + 1000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<Price> price_dist(9000, 11000);
    std::uniform_int_distribution<Quantity> qty_dist(1, 1000);

    // Pre-populate
    std::vector<OrderId> ids;
    ids.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = price_dist(rng);
        if (side == Side::Buy) price = std::min(price, Price(9999));
        else price = std::max(price, Price(10001));
        auto r = book.add_order(side, OrderType::Limit, price, qty_dist(rng));
        ids.push_back(r.order_id);
    }

    // Shuffle cancel order
    std::shuffle(ids.begin(), ids.end(), rng);

    std::vector<double> latencies;
    latencies.reserve(n);

    for (auto id : ids) {
        auto start = Clock::now();
        book.cancel_order(id);
        auto end = Clock::now();

        latencies.push_back(
            static_cast<double>(std::chrono::duration_cast<Nanoseconds>(end - start).count())
        );
    }

    auto stats = compute_stats("Cancel", latencies);
    print_stats(stats);
}

void bench_matching(std::size_t n) {
    OrderBook book(n * 2 + 1000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<Quantity> qty_dist(1, 100);

    // Build a book with sells at 100.01 - 100.10
    for (std::size_t i = 0; i < n; ++i) {
        Price ask_price = 10001 + static_cast<Price>(i % 10);
        book.add_order(Side::Sell, OrderType::Limit, ask_price, qty_dist(rng));
    }

    std::vector<double> latencies;
    latencies.reserve(n);

    // Send aggressive buy orders that will match
    for (std::size_t i = 0; i < n; ++i) {
        // Replenish liquidity
        Price ask_price = 10001 + static_cast<Price>(i % 10);
        book.add_order(Side::Sell, OrderType::Limit, ask_price, qty_dist(rng));

        Quantity qty = qty_dist(rng);
        auto start = Clock::now();
        book.add_order(Side::Buy, OrderType::Limit, Price(10010), qty);
        auto end = Clock::now();

        latencies.push_back(
            static_cast<double>(std::chrono::duration_cast<Nanoseconds>(end - start).count())
        );
    }

    auto stats = compute_stats("Match (aggressive buy)", latencies);
    print_stats(stats);
}

void bench_mixed_workload(std::size_t n) {
    // Realistic workload: ~60% add, ~30% cancel, ~10% aggressive match
    OrderBook book(n * 2 + 1000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> action_dist(0, 99);
    std::uniform_int_distribution<Price> price_dist(9900, 10100);
    std::uniform_int_distribution<Quantity> qty_dist(1, 500);

    std::vector<OrderId> active_ids;
    active_ids.reserve(n);

    std::vector<double> latencies;
    latencies.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        int action = action_dist(rng);

        auto start = Clock::now();

        if (action < 60 || active_ids.empty()) {
            // Add a resting order
            Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = price_dist(rng);
            if (side == Side::Buy) price = std::min(price, Price(9999));
            else price = std::max(price, Price(10001));
            auto r = book.add_order(side, OrderType::Limit, price, qty_dist(rng));
            if (r.status == OrderStatus::Active) {
                active_ids.push_back(r.order_id);
            }
        } else if (action < 90) {
            // Cancel random order
            std::uniform_int_distribution<std::size_t> idx_dist(0, active_ids.size() - 1);
            std::size_t idx = idx_dist(rng);
            book.cancel_order(active_ids[idx]);
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();
        } else {
            // Aggressive order that crosses the spread
            Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = (side == Side::Buy) ? Price(10100) : Price(9900);
            book.add_order(side, OrderType::Limit, price, qty_dist(rng));
        }

        auto end = Clock::now();
        latencies.push_back(
            static_cast<double>(std::chrono::duration_cast<Nanoseconds>(end - start).count())
        );
    }

    auto stats = compute_stats("Mixed workload", latencies);
    print_stats(stats);
}

int main() {
    constexpr std::size_t N = 1'000'000;

    std::cout << "\n";
    std::cout << "Low-Latency Order Book Benchmark\n";
    std::cout << "Operations: " << N << " per test\n";
    print_separator();

    bench_add_limit_orders(N);
    bench_cancel_orders(N);
    bench_matching(N);
    bench_mixed_workload(N);

    print_separator();
    std::cout << "\n";

    return 0;
}
