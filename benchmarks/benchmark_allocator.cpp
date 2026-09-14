#include <benchmark/benchmark.h>
#include "allocator/slab_allocator.hpp"
#include "types.hpp"
#include <vector>

static void BM_AllocateOrder(benchmark::State& state) {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    std::vector<lob::Order*> orders;
    orders.reserve(state.iterations());
    
    for (auto _ : state) {
        auto* order = allocator.allocate();
        benchmark::DoNotOptimize(order);
        orders.push_back(order);
    }
    
    // Cleanup
    for (auto* order : orders) {
        allocator.deallocate(order);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AllocateOrder)->Unit(benchmark::kNanosecond);

static void BM_DeallocateOrder(benchmark::State& state) {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    std::vector<lob::Order*> orders;
    for (int i = 0; i < state.range(0); ++i) {
        orders.push_back(allocator.allocate());
    }
    
    std::size_t idx = 0;
    for (auto _ : state) {
        if (idx >= orders.size()) {
            state.PauseTiming();
            // Re-allocate if we've deallocated everything
            for (auto*& order : orders) {
                order = allocator.allocate();
            }
            idx = 0;
            state.ResumeTiming();
        }
        allocator.deallocate(orders[idx++]);
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeallocateOrder)->Arg(100)->Arg(1000)->Unit(benchmark::kNanosecond);

static void BM_AllocateDeallocateCycle(benchmark::State& state) {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    for (auto _ : state) {
        auto* order = allocator.allocate();
        benchmark::DoNotOptimize(order);
        allocator.deallocate(order);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AllocateDeallocateCycle)->Unit(benchmark::kNanosecond);

static void BM_AllocatorReuse(benchmark::State& state) {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    // Pre-allocate and deallocate to fill free list
    std::vector<lob::Order*> orders;
    for (int i = 0; i < 100; ++i) {
        orders.push_back(allocator.allocate());
    }
    for (auto* order : orders) {
        allocator.deallocate(order);
    }
    orders.clear();
    
    // Now benchmark reuse from free list
    for (auto _ : state) {
        auto* order = allocator.allocate();
        benchmark::DoNotOptimize(order);
        allocator.deallocate(order);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AllocatorReuse)->Unit(benchmark::kNanosecond);

