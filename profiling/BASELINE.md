# Initial perf baseline

Collected on 2026-09-14 using the profiling workload added with this report.
This is a starting measurement, not an optimization result or a general claim
about trading-system throughput.

## Configuration

- CPU: AMD Ryzen 7 7435HS, 8 cores / 16 logical CPUs.
- Compiler: GNU C++ 16.2.1, C++23.
- Build: RelWithDebInfo (`-O2 -g -DNDEBUG`), frame pointers enabled, no sanitizers.
- Linux perf: 7.2.5-1.
- Workload: 200,000 batches, 100 makers across ten prices, one taker per batch;
  alternating buy/sell direction; 20.2 million submissions and 20 million trades.
- Scope: insertion, matching, timestamps, trade draining, validation, and process
  overhead. CPU affinity and frequency were not fixed.

```bash
bash scripts/profile.sh stat 200000 100
bash scripts/profile.sh record 200000 100
```

## Counter observations

Five stat repetitions all completed with checksum 20,000,000.

| Measurement | Reported mean |
| --- | ---: |
| Process elapsed time | 2.132 seconds |
| User-space cycles | 9.218 billion |
| User-space instructions | 25.923 billion |
| User-space branches | 5.886 billion |
| User-space branch misses | 17.156 million |
| Generic cache references | 13.439 million |
| Generic cache misses | 186,916 |

These totals imply approximately 2.81 instructions per cycle and a 0.29% branch
miss rate for this workload. Hardware events were multiplexed with approximately
83% running time. Perf reported elapsed-time variation of about 0.54%, but cache
reference/miss variation was about 13%/11%; avoid drawing small cache-effect
conclusions from these runs.

## Sampled hotspots

A separate 499 Hz user-space cycle recording collected roughly 1,000 samples
with zero lost samples. Selected self-overhead entries from `perf report
--no-children`:

| Symbol | Sampled self overhead |
| --- | ---: |
| `__vdso_clock_gettime` | 34.99% |
| Hash-table `find` | 6.95% |
| `MatchingEngine::match_order` | 6.40% |
| `OrderBook::get_price_level` | 6.31% |
| `OrderBook::add_order` | 4.30% |

Clock call stacks lead to both order timestamps and trade timestamps. This
suggests timestamp acquisition is worth investigating before assuming the slab
allocator is the main bottleneck. It does not establish that removing timestamps
would be a valid optimization: timestamp semantics must be preserved or explicitly
changed as part of a separate experiment. Longer repeated recordings would give
more confidence in the hotspot proportions.

Raw counter output is in [baseline-stat.txt](baseline-stat.txt). Full call stacks,
metadata, exact executable, source snapshot, and compilation commands were saved
under `build-perf/profile/` (ignored build output). Archive those directories
before collecting a replacement baseline. Re-run measurements on your own machine
rather than treating these numbers as portable performance guarantees.
