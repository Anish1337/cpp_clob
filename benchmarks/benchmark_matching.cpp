#include <benchmark/benchmark.h>
#include "matching_engine.hpp"

// Each timed submission consumes a fresh book. Setup and validation are excluded.
static void BM_MatchLimitOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    const auto count = static_cast<lob::Quantity>(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        for (lob::OrderId id = 1; id <= count; ++id)
            (void)engine.submit_order(id, lob::Side::Sell, 100 + id % 10, 1);
        state.ResumeTiming();
        auto result = engine.submit_order(count + 1, lob::Side::Buy, 109, count);
        benchmark::DoNotOptimize(result);
        state.PauseTiming();
        const auto trades = engine.get_trades();
        if (result != lob::OrderStatus::Filled || trades.size() != count || engine.get_order_book().order_count() != 0)
            state.SkipWithError("Expected complete sweep of fresh liquidity");
        state.ResumeTiming();
        if (state.error_occurred()) break;
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_MatchLimitOrders)->Arg(10)->Arg(100)->Arg(1000);

static void BM_PriceTimePriority(benchmark::State& state) {
    lob::MatchingEngine engine;
    const auto count = static_cast<lob::Quantity>(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        for (lob::OrderId id = 1; id <= count; ++id)
            (void)engine.submit_order(id, lob::Side::Sell, 100, 1);
        state.ResumeTiming();
        benchmark::DoNotOptimize(engine.submit_order(count + 1, lob::Side::Buy, 100, count));
        state.PauseTiming();
        auto trades = engine.get_trades();
        bool valid = trades.size() == count && engine.get_order_book().order_count() == 0;
        for (std::size_t i = 0; i < trades.size(); ++i) valid = valid && trades[i].sell_order_id == i + 1;
        if (!valid) state.SkipWithError("FIFO sweep failed");
        state.ResumeTiming();
        if (state.error_occurred()) break;
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_PriceTimePriority)->Arg(10)->Arg(100)->Arg(1000);
