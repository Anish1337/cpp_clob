#include <benchmark/benchmark.h>
#include "order_book.hpp"
#include <random>
#include <vector>

static void BM_AddOrder(benchmark::State& state) {
    lob::OrderBook book;
    std::mt19937 gen(42);
    std::uniform_int_distribution<lob::Price> price_dist(90, 110);
    std::uniform_int_distribution<lob::Quantity> qty_dist(1, 100);
    
    lob::OrderId id = 1;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            book.add_order(id++, lob::Side::Buy, price_dist(gen), qty_dist(gen))
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AddOrder)->Unit(benchmark::kMicrosecond);

static void BM_AddOrder_WithManyLevels(benchmark::State& state) {
    lob::OrderBook book;
    std::mt19937 gen(42);
    std::uniform_int_distribution<lob::Price> price_dist(90, 90 + state.range(0));
    std::uniform_int_distribution<lob::Quantity> qty_dist(1, 100);
    
    lob::OrderId id = 1;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            book.add_order(id++, lob::Side::Buy, price_dist(gen), qty_dist(gen))
        );
    }
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_AddOrder_WithManyLevels)
    ->RangeMultiplier(2)->Range(10, 1000)->Complexity(benchmark::oNLogN);

static void BM_BestBidAsk(benchmark::State& state) {
    lob::OrderBook book;
    
    // Pre-populate with orders
    const std::size_t num_orders = state.range(0);
    for (lob::OrderId id = 1; id <= num_orders; ++id) {
        (void)book.add_order(id, (id % 2 == 0) ? lob::Side::Buy : lob::Side::Sell,
                      100 + (id % 20), 10);
    }
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.best_bid());
        benchmark::DoNotOptimize(book.best_ask());
    }
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_BestBidAsk)->Arg(100)->Arg(1000)->Arg(10000)->Unit(benchmark::kNanosecond);

static void BM_CancelOrder(benchmark::State& state) {
    lob::OrderBook book;
    
    // Pre-populate
    const std::size_t num_orders = state.range(0);
    std::vector<lob::OrderId> ids;
    for (lob::OrderId id = 1; id <= num_orders; ++id) {
        (void)book.add_order(id, lob::Side::Buy, 100, 10);
        ids.push_back(id);
    }
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<std::size_t> dist(0, ids.size() - 1);
    
    std::size_t idx = 0;
    for (auto _ : state) {
        if (idx >= ids.size()) {
            state.PauseTiming();
            // Re-populate if we've cancelled everything
            for (lob::OrderId id = 1; id <= num_orders; ++id) {
                (void)book.add_order(id, lob::Side::Buy, 100, 10);
            }
            idx = 0;
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(book.cancel_order(ids[idx++]));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CancelOrder)->Arg(100)->Arg(1000)->Arg(10000)->Unit(benchmark::kMicrosecond);

static void BM_ModifyOrder(benchmark::State& state) {
    lob::OrderBook book;
    
    // Pre-populate
    const std::size_t num_orders = state.range(0);
    for (lob::OrderId id = 1; id <= num_orders; ++id) {
        (void)book.add_order(id, lob::Side::Buy, 100, 10);
    }
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<lob::Price> price_dist(95, 105);
    std::uniform_int_distribution<lob::Quantity> qty_dist(5, 15);
    
    lob::OrderId id = 1;
    for (auto _ : state) {
        if (id > num_orders) id = 1;
        benchmark::DoNotOptimize(
            book.modify_order(id++, price_dist(gen), qty_dist(gen))
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ModifyOrder)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

static void BM_GetLevels(benchmark::State& state) {
    lob::OrderBook book;
    
    // Pre-populate with many price levels
    for (lob::OrderId id = 1; id <= 1000; ++id) {
        (void)book.add_order(id, lob::Side::Buy, 100 + (id % 50), 10);
    }
    
    for (auto _ : state) {
        auto levels = book.get_levels(lob::Side::Buy, state.range(0));
        benchmark::DoNotOptimize(levels);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetLevels)->Arg(5)->Arg(10)->Arg(20)->Unit(benchmark::kMicrosecond);

static void BM_DepthAtPrice(benchmark::State& state) {
    lob::OrderBook book;
    
    // Pre-populate
    for (lob::OrderId id = 1; id <= 1000; ++id) {
        (void)book.add_order(id, lob::Side::Buy, 100 + (id % 20), 10);
    }
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.depth_at_price(lob::Side::Buy, 100));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DepthAtPrice)->Unit(benchmark::kNanosecond);

