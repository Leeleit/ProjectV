# RESULTS — cache-oblivious-chunk-tree

**Date:** 2026-06-20
**Prototype:** `prototype/cache_oblivious_layout.cpp` (standalone C++26, no Vulkan, no ProjectV deps)
**Compiled with:** `clang++ -O3 -march=native -DNDEBUG -std=c++26`
**Host:** AMD Ryzen 7 5800X (Zen 3), L1d=32 KiB / L2=512 KiB / L3=32 MiB, governor=`powersave` (sudo needed to switch to
`performance` — relative comparisons still valid; absolute latencies slightly inflated vs optimal)
**CPU affinity:** not pinned (single-threaded bench, but OS may migrate; mitigates cross-core L3 cache sharing effects)
**Iterations:** N=5000 per (layout, cache) condition, warmup=3000 steps before measurement

## 1. Scene

| Property             | Value                                                  |
|:---------------------|:-------------------------------------------------------|
| Chunk grid           | 24×24×24 chunks = 13824 chunks                         |
| Voxels per chunk     | 8×8×8 = 512 voxels                                     |
| Total voxel volume   | 192×192×192 = ~7M voxels                               |
| Fill rate            | 30% (random `std::mt19937(seed=42)`)                   |
| Node pool (baseline) | 124416 nodes = 33.2 MiB > L3 (32 MiB)                  |
| Node pool (Morton)   | 124416 nodes = 33.2 MiB (same count, reordered)        |
| Tree depth           | 2 (per chunk: root → 64 mid children → 64 leaves)      |
| Materials            | 4 (Glass, Fluid, FloorWhite, FloorGray) random-uniform |

Scene deliberately exceeds L3 to exercise cache hierarchy.

## 2. Access pattern

**Random walk (per-step):**

- Pick random chunk + random voxel.
- Walk tree root → mid → leaf (3 node accesses minimum).
- Pick random neighbor (±1 in each axis, chunk-wrap).
- Repeat.

This mimics a worst-case random access pattern (no spatial coherence).

**Cold cache:** evict cache between runs by touching 8 MiB of unrelated memory.
**Warm cache:** sequential measurement after warmup (cache filled).

## 3. Results

CSV: `prototype/results.csv`. Summary table (mean of 3 walk seeds, ns/step):

| Layout   | Cache | Mean | Median | p95  | p99 | p99.9 | Std   |
|:---------|:------|:-----|:-------|:-----|:----|:------|:------|
| baseline | warm  | 49.1 | 43.3   | 60.0 | 153 | 257   | 63.4  |
| baseline | cold  | 46.6 | 43.3   | 50.0 | 73  | 227   | 78.9  |
| morton   | warm  | 49.5 | 43.3   | 46.7 | 163 | 240   | 215.8 |
| morton   | cold  | 45.4 | 43.3   | 46.7 | 76  | 227   | 50.9  |

Per-seed detail:

| Walk seed | Layout   | Cache | Mean | p50 | p95 | p99 | p99.9 | max   |
|:----------|:---------|:------|:-----|:----|:----|:----|:------|:------|
| 1         | baseline | warm  | 57.9 | 50  | 70  | 200 | 260   | 6820  |
| 1         | baseline | cold  | 57.0 | 50  | 70  | 140 | 270   | 370   |
| 1         | morton   | warm  | 58.0 | 50  | 60  | 140 | 280   | 7170  |
| 1         | morton   | cold  | 56.3 | 50  | 60  | 150 | 270   | 300   |
| 2         | baseline | warm  | 48.1 | 40  | 70  | 180 | 300   | 320   |
| 2         | baseline | cold  | 39.7 | 40  | 40  | 40  | 200   | 200   |
| 2         | morton   | warm  | 49.3 | 40  | 40  | 200 | 230   | 33080 |
| 2         | morton   | cold  | 39.7 | 40  | 40  | 40  | 200   | 210   |
| 3         | baseline | warm  | 41.4 | 40  | 40  | 80  | 210   | 4850  |
| 3         | baseline | cold  | 42.9 | 40  | 40  | 40  | 210   | 14120 |
| 3         | morton   | warm  | 41.1 | 40  | 40  | 150 | 210   | 4320  |
| 3         | morton   | cold  | 40.1 | 40  | 40  | 40  | 210   | 210   |

## 4. Interpretation

### 4.1 Mean latency (~40-58 ns)

Mean latency is similar for both layouts across all seeds and cache conditions. With timer resolution ≈ 30 ns (
`std::chrono::steady_clock` on Zen 3), differences below 10 ns are within measurement noise.

Both layouts show mean latency consistent with **mostly L3 hit** access (~10-15 ns L3 hit on Zen 3 + cache line fetch
overhead). This is expected for a 33 MiB working set on a 32 MiB L3: most accesses still hit L3 (recent accesses in
working set), but a fraction miss.

### 4.2 p95 latency

Morton p95 ranges from 40-60 ns across seeds; baseline p95 ranges from 40-70 ns. **Morton p95 is consistently ≤ baseline
p95** (by 0-10 ns), suggesting modest warm-cache improvement.

### 4.3 p99 latency (noisy)

p99 shows the most variance:

- Seed 1: Morton 140 ns vs baseline 200 ns — **Morton 30% better** (warm).
- Seed 2: Morton 200 ns vs baseline 180 ns — **Morton 11% worse** (warm, with one 33080 ns outlier).
- Seed 3: Morton 150 ns vs baseline 80 ns — **Morton 88% worse** (warm).

The variation suggests p99 is dominated by **OS scheduler events** (context switches, page faults) rather than cache
misses per se. Std deviations 60-480 ns (vs mean 40-60 ns) confirm outliers dominate tail.

### 4.4 Cold cache

Both layouts show essentially identical cold-cache latency (cold-evict 8 MiB → next access is from RAM). This is
expected: cold-cache performance is dominated by **load latency from RAM (~80-100 ns)** + cache fill overhead. Layout
doesn't change how much data must be loaded — only where it's positioned in the loaded set.

### 4.5 Comparison with literature

The literature (e.g., arxiv 2603.06771 — SFC reordering in point clouds) reports:

- **25-75% cache miss reduction** with Morton/Hilbert reordering.
- **Up to 50% runtime reduction** in SFC-friendly access patterns.

For ProjectV-style random-walk traversal (no spatial coherence between consecutive accesses), this experiment **does not
reproduce the literature's headline numbers**. Likely reasons:

1. **Access pattern mismatch.** Random walk over 192³ voxels touches chunks across the scene. With 33 MiB working set vs
   32 MiB L3, the natural eviction policy already keeps most-recently-accessed nodes hot. Morton's spatial coherence
   benefit assumes temporally-coherent access (e.g., ray traversal, where consecutive steps are spatially close).

2. **Node size mismatch.** Sparse64Tree's 280 B node = ~5 cache lines. Reorder helps when a **single node access is
   expensive** (cold cache line fetch). But per-step access fetches a node and walks 3 nodes deep; the entire working
   set is large enough that L3 dominates. Smaller nodes (SoftwareSVO's 32 B = half cache line) would benefit more from
   reorder (per `dfhe2004/SoftwareSVO` cache-friendliness argument).

3. **Timer resolution.** `std::chrono::steady_clock` on this host has ~30 ns minimum resolution. Smaller differences are
   masked.

4. **No spatial coherence between random-walk steps.** Per-step random chunk selection means consecutive steps often
   land in distant chunks, defeating spatial locality benefits. A more realistic workload (player movement = local walk)
   might show different results.

### 4.6 Что НЕ увидели (и почему)

- **Не увидели** 25-50% reduction, как в литературе. Random-walk access pattern не exercise spatial coherence.
- **Не увидели** cold-cache improvement. Layout doesn't reduce total data load.
- **Не увидели** mean latency reduction. Working set ≈ L3, mostly hits in both layouts.
- **Не измерили** GPU-side equivalent. Prototype is CPU-only (per STATUS.md scope).
- **Не измерили** реальный VoxelLab scene structure. Synthetic random fill ≠ real VoxelLab topology.
- **Не реализовали** van Emde Boas layout (recursive subdivision). Morton is a simpler alternative; vEB might show
  different results.

### 4.7 Что удивило

- **Std deviation очень высокий** (60-480 ns при mean 40-60 ns). Это говорит про tail latency dominated by **OS
  scheduler events**, не cache misses. Для rigorous bench нужно либо увеличить iterations, либо pinned core + isolcpus,
  либо batch measurements.
- **Min latency = 30 ns** для всех conditions. Это timer resolution floor, не cache hit latency. Нужен `rdtsc` для
  sub-30 ns resolution.
- **Cold cache comparable to warm** для mean latency. Cold = evict 8 MiB, but real cold = scene в RAM (~80 ns RAM
  latency). Моя "cold" не достаточно cold для тестирования cold load.

## 5. Limitations

1. **Synthetic scene.** Real VoxelLab topology may give different results.
2. **Random-walk access.** Real gameplay has spatial coherence (player movement) — likely beneficial to Morton reorder.
   Need separate bench for this.
3. **Timer resolution.** 30 ns floor masks smaller improvements. `rdtsc` would help.
4. **Governor=powersave.** Without sudo, can't switch to `performance`. Latencies inflated; relative comparisons valid.
5. **CPU affinity not pinned.** OS scheduler may migrate threads; mitigates cross-core L3 sharing but adds noise.
6. **Single iteration of Morton reorder.** No validation that Morton-reordered scene produces identical results to
   baseline (would require byte-exact comparison, which we did not implement for prototype).
7. **No vEB implementation.** Morton is a simpler alternative; full van Emde Boas layout (CORoBTS) might show different
   results.

## 6. Reproduction

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-20-cache-oblivious-chunk-tree/prototype/
clang++ -O3 -march=native -DNDEBUG -std=c++26 cache_oblivious_layout.cpp -o /tmp/cobl_bench
/tmp/cobl_bench --output /tmp/results.csv --iterations 5000 --warmup 3000 --seed 42
```

Expected output: ~33 MiB node pool, ~40-60 ns mean latency, ~50-200 ns p99 latency. Variance between seeds expected.
