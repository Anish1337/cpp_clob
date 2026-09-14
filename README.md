# Limit Order Book & Matching Engine (C++23)

A limit-only central limit order book and matching engine for learning trading
systems, data structures, memory ownership, and performance measurement. The
engine uses only the C++ standard library. Tests use Catch2; optional benchmarks
use Google Benchmark.

## Build and run

Requires CMake 3.20+ and a compiler/standard library supporting C++23, including
`std::println` and `std::ranges::to`. Run from this project directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/examples/example_basic
```

The first test build downloads Catch2 v3.5.0. To build only the library and example
without downloading test dependencies, configure with `-DBUILD_TESTING=OFF`.

The manual build needs no external libraries:

```bash
bash build.sh
./build_manual/example_basic
```

## API and matching rules

```cpp
lob::MatchingEngine engine;
auto sell = engine.submit_order(1, lob::Side::Sell, 100, 10);
auto buy = engine.submit_order(2, lob::Side::Buy, 100, 5);
// sell == New; buy == Filled. Sell order 1 remains with quantity 5 available.
auto trades = engine.get_trades(); // One trade; drains the internal trade queue.
```

Every submission is a limit order: `submit_order(id, side, price, quantity)`.
There is no order-type enum or argument. A marketable limit executes against the
best eligible resting prices; any remainder rests at its own limit price.

- Bids execute against the lowest ask at or below their limit.
- Asks execute against the highest bid at or above their limit.
- Orders at the same price execute FIFO, using linked-list insertion order.
- Each trade executes at the resting order's price.
- Partial fills update both orders' quantities, statuses, and level totals.
- Fully filled orders are removed. Submission returns `Filled`,
  `PartiallyFilled`, `New`, or `Rejected` as appropriate.
- Zero-quantity submissions and duplicate live IDs are rejected. An ID can be
  reused after its order leaves the book.
- Prices are signed integer ticks; quantities are unsigned integers. Level
  quantity overflow is rejected. `spread()` returns no value when a side is
  empty or the difference cannot be represented by the price type.

`get_order_book()` exposes a const view. `get_order(id)` returns a non-owning
pointer that becomes invalid when that order is removed or the book is destroyed.
Use engine methods to mutate a matched book. A standalone `OrderBook` supports
storage operations but does not execute crossing orders itself.

### Cancellation and modification

`cancel_order(id)` removes an open order and returns whether it existed.

`modify_order(id, new_price, new_quantity)` interprets `new_quantity` as the
**total quantity including previous fills**. For an order originally sized 10
with 6 filled, a new total of 9 leaves 3 available.

- A total below the filled quantity is rejected without modifying the order.
- A total equal to filled quantity cancels the remaining order, including a
  reduction to zero for an unfilled order.
- Same-price reductions and unchanged requests retain FIFO priority.
- Quantity increases or price changes lose priority and join the level's tail.
- Fill history and partial-fill status are preserved across modifications.
- The engine immediately matches a modified order if its new price crosses.

### Trades and callbacks

All executions are recorded, including when no callback is installed.
`get_trades()` transfers accumulated records to the caller and empties the queue.
Each record contains both order IDs, maker price, executed quantity, and a
monotonic-clock timestamp.

An optional constructor callback receives trades after the matching operation
has left the book consistent. Notifications use a separate batch, so callbacks
may inspect the book or drain trade records. A throwing callback propagates its
exception after matching has committed; it does not roll back trades. Nested
mutations from callbacks are not an intended event-ordering API.

## Structure and architecture

```text
include/types.hpp                     Prices, quantities, orders, statuses, trades
include/order_book.hpp                Ordered levels, FIFO links, and ID lookup
include/matching_engine.hpp           Limit submission, replacement, trade events
include/allocator/slab_allocator.hpp  Template pool implementation
src/order_book.cpp                    Storage, depth, cancellation, modification
src/matching_engine.cpp               Shared buy/sell matching loop
src/slab_allocator.cpp                Allocator header compilation unit
examples/basic_example.cpp            Submission and depth demonstration
tests/test_order_book.cpp            Storage and quantity validation tests
tests/test_matching_engine.cpp       Matching regressions and reference model
tests/test_allocator.cpp             Allocation, reuse, ownership, alignment
benchmarks/                          Allocator, book, and matching workloads
```

`OrderBook` owns the live orders and price levels. Bid levels use a descending
`std::map`; ask levels use an ascending map. Each level holds an intrusive doubly
linked list with orders appended at the tail. An `std::unordered_map` maps IDs to
orders for average constant-time lookup. Level totals track remaining quantity,
not original quantity.

`MatchingEngine` coordinates matching and trade recording. It uses one loop for
both sides to keep fill accounting consistent. Incoming orders are initially
registered in the book, so every execution adjusts the level totals on both
sides. Callbacks run after filled orders have been removed.

The slab allocator obtains aligned storage in blocks and reuses released slots
through a free list. Its slab-size argument is in **bytes**, not object count.
Undersized slabs are rejected and alignment accounts for the allocated type.
The template requires objects large enough for a free-list pointer, with nothrow
default construction and trivial destruction. Allocation returns null if later
slab growth fails; initial allocation failure throws.

The allocator and book are neither copyable nor movable. Disabling moves avoids
aliasing raw slab pointers and makes the ownership contract explicit. The engine
is consequently noncopyable and nonmovable too.

This implementation is **single-threaded**. It is not a concurrent lock-free
allocator. Reusing a slot avoids allocating that order's storage, but new slabs,
map entries, hash entries, and trade vectors can still allocate. Allocation
failures may throw; submissions are not transactional across an entire multi-fill
match. No production throughput or zero-allocation guarantee is claimed.

## Tests

Catch2 supplies assertions and the test entry point; CTest discovers and runs the
cases. Coverage includes:

- FIFO and price priority, maker pricing, multi-level and partial fills.
- Accurate depth, order statuses, and trade recording on both sides.
- Repeated trade draining and callbacks observing consistent book state.
- Fill-preserving replacements, crossing replacements, and priority changes.
- Duplicate IDs, invalid quantities, depth overflow, and signed spread overflow.
- Slab reuse, growth, over-aligned objects, and disabled copy/move ownership.
- 1,500 seeded submissions, modifications, and cancellations checked against an
  independent simple matcher, including trades, quantities, statuses, and depth.

For GCC/Clang sanitizer checks, use a separate build:

```bash
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-sanitize -j 4
ctest --test-dir build-sanitize --output-on-failure
```

## Benchmarks

Benchmarks are optional and download Google Benchmark v1.8.3 if needed:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build-release -j 4
./build-release/benchmarks/benchmarks
```

Matching benchmarks replenish liquidity outside each timed submission and check
that every iteration actually executes its expected trades. The FIFO benchmark
also checks execution order. They report executed trades as processed items, not
incoming submissions. Callback overhead is excluded because no callback is set;
trade recording is included. Setup/validation timing is excluded, but their cache
effects still influence results. Other benchmark names describe operations whose
surrounding workload can also contribute to timing; compare like-for-like runs.

For `L` price levels, ID lookup is average O(1), price-level lookup is O(log L),
and best bid/ask access is O(1). Cancellation includes a level lookup, so it is
O(log L) overall. Matching visits individual resting orders and performs level
lookups and removals; it is not simply O(number of price levels).

Performance numbers should include compiler, build flags, hardware, workload,
and repetitions. Previous headline throughput figures have been removed because
the old matching workloads exhausted liquidity or measured duplicate rejection.

## Project direction

The current scope is correct limit matching, cancellation, replacement, useful
regression tests, and reproducible performance experiments. Visualization,
protocol handling, and further optimizations can be added after measuring this
baseline.

## Linux perf profiling

The project includes a deterministic profiling workload and a script for counter
collection and sampled call stacks. Neither requires Catch2 or Google Benchmark.
Install Linux `perf` separately if it is not already available.

```bash
# Five repeated measurements of CPU and cache/branch counters.
bash scripts/profile.sh stat

# Sample user-space CPU cycles and produce a text hotspot report.
bash scripts/profile.sh record
```

Both commands build `build-perf/clob_profile` with `RelWithDebInfo` and frame
pointers. They default to 200,000 batches of 100 resting orders, distributed
across ten tick prices. Each batch submits one crossing order that fills all
resting orders; buy and sell directions alternate. The workload drains trades,
validates the empty book and trade count, and checks total executed quantity.
It performs 20,200,000 submissions and 20,000,000 executions per default run.

You can change the workload size:

```bash
bash scripts/profile.sh stat 100000 1000
bash scripts/profile.sh record 100000 1000
```

Arguments are batches (1–1,000,000) and orders per batch (1–10,000). A batch with
fewer than ten orders uses fewer price levels. IDs are reused only after the
previous batch has completed. No callbacks are installed.

Outputs are saved separately in `build-perf/profile/stat/` and
`build-perf/profile/record/`. Each run saves workload output, machine/commit
metadata, CMake configuration, compilation commands, a source archive, and a
copy of the executable. Stat mode adds `stat.txt`; record mode adds `perf.data`
and `report.txt`. Repeating the same mode overwrites that mode's previous output;
copy its directory elsewhere if you want to retain a baseline.

To explore the recorded call stacks interactively:

```bash
perf report -i build-perf/profile/record/perf.data
```

**Measurement scope:** these profiles include the entire process: insertion,
matching, map/hash/pool operations, trade timestamps, trade storage/draining,
validation, and startup/destruction. Google Benchmark's `PauseTiming()` does not
pause perf counters. This separate executable makes that distinction explicit;
its elapsed time is not isolated matching latency and contains no percentile
latency measurement.

Interpret the outputs as follows:

- Cycles and instructions help compare CPU work for an identical workload.
  Instructions divided by cycles gives aggregate IPC, not a standalone speed score.
- Branch misses divided by branches estimates branch-miss rate.
- Generic cache references/misses depend on the CPU's event mapping; do not label
  them L1 misses or infer cache efficiency from one run.
- Perf may multiplex hardware counters. Check their time-running percentages and
  repeatability, especially before comparing small differences.
- The text report uses `--no-children`, so percentages show sampled self overhead.
  Use call stacks to investigate callers; samples alone do not prove that a
  proposed change will improve performance.

For an optimization experiment, keep workload, compiler flags, and machine fixed;
archive the baseline, change one implementation detail, rerun correctness tests,
and collect a new profile. Profiling instrumentation, frequency scaling, CPU
migration, and other processes affect measurements. Longer runs and optional CPU
affinity can improve repeatability. Sanitizer builds are for correctness checks,
not performance comparisons.

The script requests user-space events. Some environments restrict performance
counters or do not expose the requested hardware events. If perf reports access
denied or unsupported counters, that run is not a usable result; the script does
not change kernel permissions or silently substitute different events.

An [initial measured baseline](profiling/BASELINE.md) records the workload,
machine, counters, and sampled hotspots. It identifies timestamp acquisition as
an investigation target; no performance improvement is claimed yet.
