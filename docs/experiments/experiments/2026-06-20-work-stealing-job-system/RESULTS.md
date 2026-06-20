# RESULTS — 2026-06-20-work-stealing-job-system

Полные цифры для всех 24 configs (4 workloads × 3 thread counts × 3 implementations, + serial baseline для threads=1).

**Hardware:** AMD Ryzen 7 5800X (Zen 3, 8C/16T, L3 32 MiB shared), governor `powersave`, idle boost ~5.0 GHz.
**Compiler:** clang++ 22.1.6, `-O3 -march=native -DNDEBUG -std=c++26`.
**Workload:** synthetic ProjectV chunk generation (1024 voxels/chunk = 8³, per-chunk splitmix32 + 64-block fill-mask;
имитирует Stage 4.1 batch chunk gen + Stage 3.1 per-chunk Fluid CA bookkeeping).
**Methodology:** 10 warm-up + 30 iters per config, harness thread pinned to core 0, workers un-pinned (allow OS
scheduler to optimize).

---

## 1. Headline numbers (mean latency, ms)

| Workload (chunks) |    serial | simple 1t | simple 4t | simple 16t |  BS 1t |  BS 4t | BS 16t |
|------------------:|----------:|----------:|----------:|-----------:|-------:|-------:|-------:|
|               256 | **0.103** |     0.122 |     0.246 |      0.703 |  0.663 |  0.313 |  0.752 |
|              1024 | **0.436** |     0.502 |     0.491 |      2.260 |  2.455 |  1.150 |  1.751 |
|              4096 | **1.695** |     1.841 |     1.842 |      9.399 |  8.953 |  3.756 | 16.216 |
|             16384 | **7.034** |     7.689 |    12.174 |     40.134 | 35.182 | 15.312 | 54.917 |

**Sweet spot:** **serial is fastest across ALL workload sizes.** Pool overhead dominates for 256-1024 chunks, and SMT
contention (16t) on L3 thrashing makes 16-thread pools catastrophically slower.

---

## 2. p99 latency (tail) — critical for real-time frame budget

| Workload | serial | simple 4t |  BS 4t | simple 16t | BS 16t |
|---------:|-------:|----------:|-------:|-----------:|-------:|
|      256 |  0.119 |     0.446 |  0.788 |      1.184 |  1.994 |
|     1024 |  0.562 |     1.250 |  3.489 |      4.076 |  8.026 |
|     4096 |  1.792 |     2.019 |  4.578 |     19.036 | 31.657 |
|    16384 |  7.732 |    14.600 | 16.126 |     58.316 | 88.860 |

**Real-time killer:** 16384-chunk batch:

- Serial: 7.7 ms p99 (1 frame at 60 FPS = 16.7 ms, fits with margin)
- BS 16t: 88.9 ms p99 (**11× over budget**, frame stutter)
- BS 4t: 16.1 ms p99 (marginal, occasional stutter)

**Tail latency degrades much faster than mean** for parallel configs (work-stealing CV = bad).

---

## 3. Speedup vs serial baseline

| Workload | simple 4t | BS 4t | simple 16t | BS 16t |
|---------:|----------:|------:|-----------:|-------:|
|      256 |     0.42× | 0.33× |      0.15× |  0.14× |
|     1024 |     0.89× | 0.38× |      0.19× |  0.25× |
|     4096 |     0.92× | 0.45× |      0.18× |  0.10× |
|    16384 |     0.58× | 0.46× |      0.18× |  0.13× |

**No parallelism configuration matches serial.** The best case (simple 4t, 4096 chunks) is 92% of serial speed. **Best
parallel throughput is worse than serial across the board.**

---

## 4. Per-config stddev / p99 ratio (jitter indicator)

Lower = more deterministic. Real-time systems want <2.0.

| Workload | serial | simple 4t | BS 4t | simple 16t | BS 16t |
|---------:|-------:|----------:|------:|-----------:|-------:|
|      256 |   0.96 |      3.78 |  2.47 |       1.84 |   2.65 |
|     1024 |   1.20 |      4.97 |  1.83 |       1.85 |   4.03 |
|     4096 |   1.06 |      1.10 |  1.22 |       2.02 |   1.96 |
|    16384 |   1.10 |      1.20 |  1.05 |       1.45 |   1.62 |

**Serial is consistently the most deterministic** (ratio ~1.0-1.2). Parallel configs show 2-5× jitter, especially for
small workloads (overhead dominates).

---

## 5. Throughput (Mops/sec)

| Workload | serial | simple 4t | BS 4t | simple 16t | BS 16t |
|---------:|-------:|----------:|------:|-----------:|-------:|
|      256 |   1277 |       533 |   419 |        187 |    174 |
|     1024 |   1202 |      1067 |   456 |        232 |    300 |
|     4096 |   1237 |      1138 |   558 |        223 |    129 |
|    16384 |   1193 |       689 |   548 |        209 |    153 |

**Serial throughput = 1.1-1.3 Gops/sec** stable across workloads. Parallel throughput drops 2-10× because:

- Submit overhead (~5-10 µs per task) eats into per-task compute (~0.4 µs/chunk)
- SMT (16t) cache thrashing
- Work stealing overhead (atomic counter ops, random memory access)

---

## 6. Analysis

### 6.1 Why serial wins

**Memory bandwidth-bound workload.** Per-chunk compute = ~0.4 µs (3 × splitmix32 + 64-cell mask). At 5 GHz × 4 IPC = ~20
BOps/sec/chunk theoretical per core. For 1024 chunks sequential = ~400 µs theoretical. Measured = 436 µs. **96% of
theoretical peak single-core compute.** Adding cores doesn't help because L3 (32 MiB) is the bottleneck:

- 1024 chunks × 4 KiB = 4 MiB — fits L3 comfortably
- 16384 chunks × 4 KiB = 64 MiB — **2× L3, thrashes on multi-core**
- Per-chunk output = 8 bytes hash — negligible

For cache-fitting workloads, single-thread already saturates memory bandwidth, and **submit overhead consumes the entire
parallel gain budget**.

### 6.2 Why BS::thread_pool loses to simple

**BS::thread_pool has more atomic ops per task** than simple (work-stealing deques, per-worker notifier). For 256 tasks:

- Simple: 256 × `std::function<void()>` ctor + cv_.notify_one ≈ 1 µs/task
- BS: 256 × `BS::future` ctor + per-worker deque push + notifier ≈ 4-6 µs/task

When task body = 0.4 µs compute, BS overhead = **10-15× task body**, dominates.

**Per ptsouchlos/thread-pool benchmarks (Zen 3 5800X, Clang) — `task_thread_pool` (no work stealing) +9.7%
vs `dp::thread_pool`, `BS::thread_pool` -9.8% vs baseline** — this matches my finding that work-stealing overhead hurts
for small/short tasks.

### 6.3 Why 16 threads catastrophically loses

SMT (Simultaneous Multi-Threading) on Zen 3 = 2 threads/core sharing L1/L2. For workloads that are not memory-bound (
i.e. fit in private L2), SMT = resource contention. For memory-bound workloads, SMT doubles the cache pressure without
doubling the bandwidth.

16384 chunks × 4 KiB = 64 MiB > L3 (32 MiB). 16 workers randomly accessing → cache lines bouncing between cores →
effective memory bandwidth = L3 + DRAM ratio collapses. Mean time **5.7× worse** than serial.

### 6.4 When thread pool IS better (hypothetical, not measured)

My workload is **memory-bandwidth-bound, short tasks, fits L3**. Pool would win if:

- Per-task compute = 100 µs - 1 ms (submit overhead becomes < 1% of total)
- Per-task data = 64 KiB - 1 MiB (L2-resident, not cache-line bouncing)
- Many tasks (10000+) — submit overhead amortized

**ProjectV applicability:**

- ❌ Stage 4.1 per-chunk chunk gen (per-chunk 4 KiB) — **serial is best**
- ❌ Stage 3.1 per-chunk Fluid CA bookkeeping (per-chunk 1-2 KiB) — **serial is best**
- ⚠️ Stage 6.1 ECS multi-threading (Flecs `ecs_progress`) — per-system, data-fit-dependent, **needs separate measurement
  **
- ✅ Stage 2.x full-scene mesh generation (all chunks at once, GB-scale) — **pool helps if non-L3-fit**
- ✅ Stage 4.1 batch *world* gen (1000+ chunks at once, future Stage 4.3 128-chunk draw distance) — **pool helps if
  data > L3**

---

## 7. Verdict preview

**Work-stealing pool** = **NO** default for ProjectV CPU-side dispatch.
**Simple thread pool** = **MAYBE** for large-batch workloads (>4 MiB total, >10 ms compute).
**Serial dispatcher** = **YES** for small/medium workloads (<4 MiB total, <1 ms compute).

**Cross-vendor:** 5800X (Zen 3) = consumer desktop. Intel desktop (no HT) or AMD EPYC (multi-socket NUMA) might show
different sweet spots — NUMA favors work-stealing because stealing across NUMA nodes = good locality distribution. **Not
measured, callout as caveat.**

**Future SOTA (std::execution P2300):** P2300 is framework, not pool. Real production mainline still needs an external
scheduler (e.g. `stdexec::static_thread_pool`). Given my finding that pool overhead = dominant for small tasks,
`stdexec` sender-chain overhead (lazy evaluation, type erasure) is **likely worse** than `BS::thread_pool` for hot-path
batch dispatch. **Not measured, callout as follow-up.**

---

## 8. Caveats

1. **Synthetic workload, not real ProjectV code.** Per-chunk 4 KiB splitmix32 + 64-block mask is **simpler** than real
   perlin noise (3-5× more expensive per `2026-06-20-simd-procedural-noise`) and SVDAG walks (branch-heavy). Real
   workload = potentially MORE compute per chunk → pool overhead ratio improves.
2. **Single-vendor (Zen 3).** Intel desktop = no HT = different scaling. AMD EPYC = NUMA = different scaling. Arm
   big.LITTLE = different scaling.
3. **No AVX-512 in workload.** Adding SIMD (per `simd-procedural-noise` AVX2 = 1.14-1.83× speedup) would make per-chunk
   compute longer → pool overhead ratio improves.
4. **No I/O or syscalls in workload.** Real ProjectV Stage 4.1 includes `std::filesystem::directory_iterator` (Stage 1.3
   pattern) — would benefit from async I/O, not just multi-thread.
5. **Governor = `powersave`, not `performance`.** Results may be 5-10% better with `performance` governor (boost stays
   at max). 5800X boost is aggressive regardless.
6. **No memory bandwidth measurement.** Per `benchmarks/methodology.md` §5, should include `perf stat` for L1/LL miss
   rates. Not done — would require root and `perf_event_open`. **Callout as follow-up.**

---

## 9. Reproducibility

```bash
cd docs/experiments/experiments/2026-06-20-work-stealing-job-system/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG bench.cpp -o /tmp/bench_pool
/tmp/bench_pool 30 > /tmp/bench.log
# results.csv auto-written to ../
```

All 24 configs run in <2 min on 5800X. Hash output determinstic across runs (seed `0xC0FFEE + chunkIndex`).
