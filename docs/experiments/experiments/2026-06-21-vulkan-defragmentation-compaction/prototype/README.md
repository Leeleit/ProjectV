# defrag_bench prototype — standalone C++26 CPU fragmentation simulator

Standalone CPU simulator for VMA defragmentation strategies. NOT part of
ProjectV mainline. Designed per `docs/experiments/benchmarks/methodology.md`
for self-contained reproducible benchmark.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-vulkan-defragmentation-compaction/prototype
mkdir -p build
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    -o build/defrag_bench defrag_bench.cpp
# Or use CMake:
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Compiler: Clang 22.1.6 per `hardware-profile.md §6` and
`agent/knowledge.md`.

## Run

```bash
./build/defrag_bench
# Writes build/results.csv with 500 rows.
# Wall time ~10 sec on Zen 3 5800X per hardware-profile.md §1.
```

## What it measures

For each combination of {strategy × scene × alloc_pattern × seed}:

- **peak_vram_mib**: peak bytes-in-use over measurement window (lower is better
  when defrag extends usable heap).
- **heap_used_mean_mib**: mean bytes-in-use (workload utilization).
- **frag_ratio_mean**: mean fragmentation ratio (1 - largest_contiguous_free /
  total_free, 0..1).
- **defrag_p99_ms**: 99th percentile of per-frame simulated defrag cost.
- **stutter_frames**: number of frames where defrag cost exceeds 2 ms threshold
  (= 6% of 33.3 ms frame budget @ 30 Hz).
- **alloc_failure_rate**: fraction of allocation attempts that fail (no
  contiguous hole of required size).

## Strategies modeled

| Name                  | Behavior                                            | VMA equivalent |
|:----------------------|:----------------------------------------------------|:---------------|
| A_None                | Baseline, no defrag (current mainline)              | —              |
| B_PeriodicFull        | Full defrag every 300 frames                        | `vmaBeginDefragmentation` + `maxBytesPerPass=SIZE_MAX` + period scheduling |
| C_IncrementalBudgeted | Per-frame 8 MiB defrag budget                       | `vmaBeginDefragmentationPass` with `maxBytesPerPass=8 MiB` cap |
| D_OnDemandThreshold   | Full defrag when frag_ratio > 0.4                   | `vmaBeginDefragmentation` triggered by `vmaCalculateStatistics` > threshold |
| E_BudgetedOnDemand    | 8 MiB defrag when frag_ratio > 0.4                  | D with `maxBytesPerPass=8 MiB` cap |

## Limitations

CPU simulation, NOT real VMA integration. See
[`RESULTS.md`](../RESULTS.md) for detailed analysis of why synthetic sim
shows trivial results and what production integration would require.

See [`README.md`](../README.md) §9 "Mapping to ProjectV hot-path" for which
mainline files would be touched by integration.
