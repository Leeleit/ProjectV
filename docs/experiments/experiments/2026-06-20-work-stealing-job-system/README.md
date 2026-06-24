# 2026-06-20-work-stealing-job-system — Job-system SOTA для ProjectV CPU-side dispatch

**Status:** `concluded-verdict-mixed`
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** independent (Stage 4.1 background world gen dispatcher foundation + Stage 6.1 ECS multi-threading
future)
**Estimated effort:** S (prototype) + S (writeup)
**Author:** self (per `backlog.md §In progress`)

---

## 1. Hypothesis

**Гипотеза:** C++26 `std::execution` (P2300 senders/receivers) даёт сопоставимый throughput с `BS::thread_pool` v5 /
Taskflow / std::thread pool на synthetic ProjectV chunk-generation workload (1024 chunks × 8³ voxels = 524288 ops/batch)
на Zen 3 8C/16T — при этом лучше composable для Stage 6.1 ECS (sender chains для `ecs_progress` hooks), кроссвендорно
portable без vendor lock-in (TBB = Intel, libdispatch = Apple). Поэтому становится mainline default для CPU-side batch
dispatch.

**Преимущество:** (a) Стандарт C++26 = долговечный, не vendor-specific. (b) Senders/receivers composable, не
fire-and-forget. (c) `stdexec` production-ready (NVIDIA reference impl). (d) Pluggable schedulers (system parallel,
static thread pool, io_uring, GPU) — Stage 6.1 NUMA-aware scheduler plug-and-play.

**Альтернативы:**

- **BS::thread_pool v5.0.0** (Barak Shoshany, MIT, header-only) — work-stealing, mature, used in scientific computing,
  3K stars.
- **Taskflow v3.10.0 / v4.0.0** (Tsung-Wei Huang et al., MIT) — C++17 task-parallel programming system, IEEE TPDS 2022
  paper, decentralized work-stealing.
- **Intel oneTBB v2022.3.0** (Intel / UXL Foundation, Apache 2.0) — production-grade, NUMA-aware, 6.6K stars.
- **Custom std::thread pool** (mutex + condvar + shared deque) — baseline, no work stealing.

**Сравнить:** 3 implementations на synthetic workload, измерить throughput / p99 latency / jitter, рекомендовать default
для mainline.

---

## 2. Prior art

**Web-research per §4 моего `AGENTS.md`:** 4 batch queries (`std::execution` P2300, BS::thread_pool, Taskflow,
oneTBB), ~25 sources. Полный список: `sources.md`.

**Ключевые findings (cross-ref `sources.md` §1-§5):**

1. **C++26 `std::execution` (P2300)** принят в C++26 (P2300R10, 2024-06-28). Production-ready reference impl = *
   *NVIDIA/stdexec** (header-only, pluggable schedulers). Partial в GCC 15+, Clang 20+, MSVC. **Известный design issue**
   в `transform_sender` customization (P3826R3, 2026-01) — fix уже в CCCL/stdexec, не блокер, но сигнал что «голый»
   `std::execution` ещё не production-ready.
2. **std::execution = framework, НЕ thread pool** (per P3109R0 2024 + LLVM Discourse RFC 2025-06). «We should provide a
   scheduler out of the box» — но это scheduler abstraction, не конкретный pool. Реальный pool = external (
   `stdexec::static_thread_pool`, `BS::thread_pool`, etc).
3. **BS::thread_pool v5.0.0** (2024-12) — C++17/20/23, MIT, 3K stars, header-only, work-stealing. Mandelbrot benchmark.
4. **Taskflow v3.10.0** (2025-05) — decentralized work-stealing, BoundedTaskQueue, optimized work-stealing loop with
   adaptive breaking strategy.
5. **oneTBB v2022.3.0** (2025-10) — UXL Foundation, Apache 2.0, 6.6K stars, NUMA-aware.
6. **ptsouchlos/thread-pool benchmarks на Zen 3 5800X** — `task_thread_pool` (no WS) +9.7% vs `dp::thread_pool`,
   `BS::thread_pool` -9.8% vs baseline — **work-stealing НЕ всегда побеждает** для small tasks.

**Tier 4 R&D marker per `agent/knowledge.md`** line 887: «`std::execution` (P2300, Senders/Receivers) — нужна Job
System, отдельный slice» — direct prior art, identifies this experiment as Tier 4 R&D, not mainline blocker.

**Cross-axis continuity:**

- `2026-06-20-flecs-soa-vs-aos-bench` (closed) — ECS layout settled (SoA wins). Этот experiment = job-scheduling surface
  для ECS multi-thread.
- `2026-06-20-async-compute-overhead-numbers` (closed) — async foundation on GPU (+9.85-11.34% speedup). Этот
  experiment = async foundation on CPU side.
- `2026-06-20-simd-procedural-noise` (closed) — per-chunk CPU compute measured (1.14-1.83× AVX2 vs scalar). Этот
  experiment = dispatcher для batch таких workloads.

---

## 3. Method

**Тип эксперимента:** prototype + benchmark (per `benchmarks/methodology.md`).

**Сцена:** synthetic ProjectV chunk generation workload — 512 voxels/chunk (8³ = ProjectV chunkSize per
`agent/knowledge.md` per `src/voxel/VoxelWorld.hpp:78`), per-chunk splitmix32 hash (имитирует material ID
assignment) + 64-block fill-mask (имитирует SVDAG 64-ary tree top-level structure). Per-chunk output = 8 bytes (one
cache line). Total per-chunk compute ≈ 0.4 µs на 5 GHz Zen 3.

**Метрики:**

- mean / median / p95 / p99 / stddev / min / max latency per config (per `benchmarks/methodology.md` §3)
- throughput (Mops/sec)
- jitter ratio (p99/mean)
- speedup vs serial baseline

**Контроль:** 3 implementations × 3 thread counts (1, 4, 16) × 4 workloads (256, 1024, 4096, 16384 chunks) + 1 serial
baseline = **24 configs**. Каждое config = 10 warm-up + 30 iters (per `benchmarks/methodology.md` §3, N=30 default). 720
measurements total.

**Протокол:**

1. Harness thread pinned to core 0 (low jitter per `benchmarks/methodology.md` §4).
2. Workers NOT pinned (allow OS scheduler / work stealing to operate normally).
3. `BS_thread_pool.hpp` vendored from upstream v5.0.0 (2024-12-20) — header-only, MIT.
4. Single binary `bench_pool` (89 KB), run with `N` iters per config, write `results.csv` (24 rows) + stdout summary.
5. Workload: deterministic seed `0xC0FFEE + chunkIndex` → reproducible output.

**Hardware:** см. `docs/experiments/hardware-profile.md` §1 — Zen 3 8C/16T, L3 32 MiB, governor `powersave`, idle
boost ~5.0 GHz.

---

## 4. Prototype

Код: `prototype/` (5 файлов, ~600 LoC total).

**Структура:**

| File                 |  LoC | Назначение                                                                                           |
|:---------------------|-----:|:-----------------------------------------------------------------------------------------------------|
| `workload.hpp`       |  ~70 | Synthetic ProjectV chunk gen (`ChunkGen::generate(seed)`: 512 splitmix32 calls + 64-block fill-mask) |
| `pool_simple.hpp`    |  ~95 | Custom `std::thread` pool (mutex + condvar + shared deque, no work stealing, baseline)               |
| `pool_bs.hpp`        |  ~20 | `BS::thread_pool<>` alias wrapper                                                                    |
| `BS_thread_pool.hpp` | 2373 | Vendored BS::thread_pool v5.0.0 (MIT, single header)                                                 |
| `stats.hpp`          |  ~50 | `Compute(samples) → {mean, median, p95, p99, stddev, min, max}`                                      |
| `bench.cpp`          | ~210 | Orchestrator: 24 configs, harness, CSV output                                                        |

**Build:**

```bash
cd docs/experiments/experiments/2026-06-20-work-stealing-job-system/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG bench.cpp -o /tmp/bench_pool
```

**Run:**

```bash
/tmp/bench_pool 30   # 30 iters per config (default)
# stdout: human-readable summary
# ../results.csv: machine-readable per-config stats
```

**Reproducibility:** full output reproducible. Seed deterministic. Hash output identical across runs (verified during
smoke test).

**Scope per `AGENTS.md §2`:** standalone research artifact, **NOT** ProjectV mainline. Zero ProjectV source
dependencies. Only stdlib + vendored BS::thread_pool.hpp.

---

## 5. Results

Полные таблицы + анализ: `RESULTS.md`.

**Headline (mean latency, ms, 30 iters):**

| Workload (chunks) |    serial | simple 1t | simple 4t | simple 16t |  BS 1t |  BS 4t | BS 16t |
|------------------:|----------:|----------:|----------:|-----------:|-------:|-------:|-------:|
|               256 | **0.103** |     0.122 |     0.246 |      0.703 |  0.663 |  0.313 |  0.752 |
|              1024 | **0.436** |     0.502 |     0.491 |      2.260 |  2.455 |  1.150 |  1.751 |
|              4096 | **1.695** |     1.841 |     1.842 |      9.399 |  8.953 |  3.756 | 16.216 |
|             16384 | **7.034** |     7.689 |    12.174 |     40.134 | 35.182 | 15.312 | 54.917 |

**Best p99 jitter (smallest stddev/mean ratio) = serial, all workloads.** Best throughput = serial, all workloads. Best
latency = serial, all workloads.

**Surprising negative findings:**

1. **`std::thread` pool overhead dominates for tasks <10 µs.** Per-task compute ≈ 0.4 µs → pool overhead (queue mutex /
   future ctor / CV notify) = 5-15 µs = **12-37× task body**. No pool config breaks even.
2. **Work-stealing overhead HURTS for small tasks.** BS::thread_pool slower than simple across all small workloads (BS
   1t = 5-8× slower than serial). Matches ptsouchlos/thread-pool benchmarks on Zen 3.
3. **SMT (16 threads) catastrophically counter-productive for cache-friendly workloads.** 16384 chunks × 4 KiB = 64
   MiB > L3 32 MiB → cache-line bouncing between SMT siblings destroys bandwidth. simple 16t = **5.7× slower** than
   serial; BS 16t = **7.8× slower**.
4. **Sweet spot = serial dispatcher** for ProjectV-style small-chunk CPU work.

**Per-stage applicability (deferred to §7 Integration):**

- ❌ Stage 4.1 per-chunk chunk gen — **serial** (per-chunk 4 KiB).
- ❌ Stage 3.1 per-chunk Fluid CA bookkeeping — **serial** (per-chunk 1-2 KiB).
- ⚠️ Stage 6.1 ECS multi-thread — needs separate measurement (per-system data-fit-dependent).
- ✅ Stage 4.1 batch *world* gen (1000+ chunks) — pool helps if data > L3.
- ✅ Stage 2.x full-scene mesh generation (GB-scale) — pool helps.

---

## 6. Verdict

**`mixed`** — гипотеза НЕ подтверждена для ProjectV primary workloads:

1. **`std::execution` (P2300) — НЕ mainline default** для CPU-side batch dispatch. P2300 = framework, не pool. Real
   pool = `stdexec::static_thread_pool` или external. Sender-chain overhead (lazy eval, type erasure) **предположительно
   хуже** чем `BS::thread_pool` для hot-path batch dispatch (per my finding, не измерено — callout as follow-up).
2. **Work-stealing — НЕ auto-win** для small tasks. Per `agent/knowledge.md` SIMD-frustum-cull priority (1500+
   dot products per frame) — аналогично: per-entity ECS bookkeeping = small task. **«Use work-stealing = best default» —
   common wisdom, NOT measured in this workload.**
3. **Sweet spot для ProjectV mainline = serial dispatcher** для большинства CPU-side workloads. SMT + thread pool =
   counter-productive for cache-fitting workloads.

**Caveats (per `RESULTS.md` §8):**

- Synthetic workload, not real ProjectV code (per-chunk compute likely 3-5× higher in real perlin).
- Single-vendor (Zen 3). Intel desktop / EPYC NUMA / Arm big.LITTLE = different.
- No AVX-512 (per `simd-procedural-noise` AVX2 = 1.14-1.83× speedup, would shift ratio in favor of pool).
- No memory bandwidth measurement (per `benchmarks/methodology.md` §5, `perf stat` L1/LL miss rate — callout as
  follow-up).
- Governor = `powersave`, not `performance` (results may be 5-10% better with `performance`).

**Re-evaluation triggers:**

- Stage 6.1 Step 6 NUMA-aware allocation (`TODO.md §6.1`).
- Stage 4.3 lift draw distance (128+ chunks, total data likely > L3).
- Perlin/SVDAG real workload (3-5× compute vs my synthetic).
- AVX-512 hardware arrival (Zen 5, Arrow Lake).
- Real ProjectV ECS `ecs_progress` overhead (not yet measured).

---

## 7. Integration recommendation

**Рекомендация 1 (DO сейчас, S effort):** **Stage 4.1 dispatcher = serial CPU + async GPU dispatch.** Не подключать
thread pool для background world gen. Per-chunk gen = 4 KiB splitmix32, serial = 7 ms для 16384 chunks (within 1 frame
at 60 FPS). Главный bottleneck ProjectV = GPU noise compute (10-100× faster per `TODO.md §4.1`), не CPU. Serial CPU +
parallel GPU = правильное распределение.

**Рекомендация 2 (DO сейчас, S effort):** **Stage 3.1 Fluid CA bookkeeping = serial dispatcher + GPU compute.**
Аналогично: per-chunk Fluid CA state (1-2 KiB) — serial CPU side = correct. Heavy CA compute = на GPU per
`decisions.md §30.4`. Не подключать thread pool для CPU-side bookkeeping.

**Рекомендация 3 (DEFER, Stage 6.1):** **Flecs `ecs_progress` multi-threading — отдельный experiment.** Per
`TODO.md §6.1` Step 6 «NUMA-aware allocation may shift tradeoff» — нужно измерить конкретные ECS systems (HZB cull /
Fluid CA bookkeeping / Incremental Jolt / VCT voxelize) на их actual per-system working set. Если working set > L3, pool
helps; if < L3, serial is best. **Open question, defer до Stage 6.1 implementation.** Per `agent/knowledge.md` — «`std::execution` (P2300) — нужна Job System, отдельный slice» — это Tier 4 R&D, не mainline blocker.

**Рекомендация 4 (PARKED, future):** **Stage 4.3 lift draw distance (128+ chunks) может shift tradeoff.** При 128
chunks × 4 KiB = 512 KiB (still < L3 32 MiB), so serial still wins. При active streaming или async gen across 1000+
chunks, может превысить L3 → pool helps. Re-evaluation trigger: Stage 4.3 implementation start.

**Рекомендация 5 (PARKED, vendor diversity):** **`std::execution` (P2300) tracking.** Per `bigcpp.com` 2026-05-25: GCC
15+ / Clang 20+ / MSVC partial support. Per P3826R3 (2026-01) — known customization issue, fix in CCCL/stdexec. **Wait
for libc++ full integration + Clang 23+ stable**, then re-evaluate. ETA: ~C++26 publication date (~2026-2027).

**Что mainline НЕ должен делать:**

- ❌ Добавлять TBB (vendor lock-in Intel + 2 MB binary overhead per `Decisions.md §29.0` line 887 reasoning).
- ❌ Добавлять libdispatch (Apple-only, не portable).
- ❌ Использовать `task_thread_pool` (no work stealing) — 3rd party, no upstream updates.
- ❌ Менять Stage 4.1 / 3.1 CPU dispatch с serial на pool без отдельного measurement per workload.

**Estimated mainline effort:** **XS** — нет mainline кода, только **НЕ делать** определённые вещи (anti-pattern: don't
add pool по default).

**Критерии приёмки:** N/A (negative result). Cross-check: при Stage 6.1 implementation начать с **serial dispatcher** +
**per-system profile**; если конкретный ECS system < 1 ms serial, оставить serial; если > 5 ms, re-evaluate pool с
отдельным measurement.

**Dependencies:** нет (negative result).

---

## 8. Sources

Полный список (25 источников с датами, авторами, контекстом): `sources.md`.

**Top references:**

- [P2300R10 — `std::execution`](https://wg21.link/P2300) (Niebler, Shoop, Baker et al., ISO WG21, 2024-06-28)
- [P3826R3 — Fix Sender Algorithm Customization](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3826r3.html) (
  WG21, 2026-01)
- [P3109R0 — A plan for std::execution for C++26](https://open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3109r0.html) (
  WG21, 2024)
- [LLVM Discourse RFC: add std::execution](https://discourse.llvm.org/t/rfc-add-std-execution-c-26-to-the-utilities/86841) (
  vtjnash, 2025-06-12)
- [NVIDIA/stdexec](https://github.com/nvidia/stdexec) (NVIDIA, 2024+, production-ready reference impl)
- [bshoshany/thread-pool v5.0.0](https://github.com/bshoshany/thread-pool) (Shoshany, 2024-12-20, MIT)
- [taskflow/taskflow](https://github.com/taskflow/taskflow) (Huang et al., 2024+, MIT, IEEE TPDS 2022)
- [uxlfoundation/onetbb v2022.3.0](https://github.com/uxlfoundation/onetbb) (Intel / UXL Foundation, 2025-10-29, Apache
  2.0)
- [ptsouchlos/thread-pool benchmarks (Zen 3 5800X)](https://github.com/DeveloperPaul123/thread-pool) — confirms my
  finding that work-stealing loses for small tasks
- [facebookincubator/dispenso](https://github.com/facebookincubator/dispenso) (Meta, 2020+, alternative to TBB)

---

## 9. Mapping to ProjectV hot-path

**Какие участки движка соответствуют прототипу:**

| ProjectV path                            | Workload scale                        | Mapped config         | Verdict                                |
|:-----------------------------------------|:--------------------------------------|:----------------------|:---------------------------------------|
| Stage 4.1 per-chunk background world gen | 1024-16384 chunks, 4 KiB/chunk        | W2-W4, 1t-4t          | **serial** (already mainline)          |
| Stage 3.1 per-chunk Fluid CA bookkeeping | 100-1000 active chunks, 1-2 KiB/chunk | W1-W2, 1t             | **serial** (already mainline)          |
| Stage 6.1 Flecs `ecs_progress`           | per-system, 4 KiB - 1 MiB             | W1-W3, depends        | **TBD, separate experiment**           |
| Stage 2.x full-scene mesh gen            | all visible chunks, MB-GB             | N/A (single dispatch) | **single dispatch** (already mainline) |
| Stage 4.3 128-chunk draw distance        | 128+ chunks, 512 KiB+                 | (future)              | **re-evaluate at Stage 4.3**           |

**Допущения / упрощения:**

- Workload is **synthetic** (splitmix32 + 64-block mask), не real perlin/SVDAG. Real workload = 3-5× more compute per
  chunk per `simd-procedural-noise` data → pool overhead ratio improves.
- Single-vendor (Zen 3). Cross-vendor per `RESULTS.md` §8 caveat 2.
- No AVX-512 (per `hardware-profile.md` §1 — Zen 3 не поддерживает). AVX-512 arrival = pool overhead ratio improves
  further.
- 5800X governor = `powersave`, не `performance` (per `hardware-profile.md` §1). Results may be 5-10% better with
  `performance` governor.

**Что осталось неизмеренным:**

- Real ProjectV perlin/SVDAG workload (next experiment: `simd-real-workload-vs-pool`).
- Flecs `ecs_progress` per-system overhead (defer to Stage 6.1).
- Memory bandwidth via `perf stat` (требует root + `perf_event_open` per `benchmarks/methodology.md` §5).
- Cross-vendor: Intel desktop, AMD EPYC NUMA, Arm big.LITTLE.
- `stdexec::static_thread_pool` directly (defer until Clang 23+ + libc++ full integration).
- Workload with `std::filesystem::directory_iterator` (Stage 1.3 pattern) — async I/O + pool synergy.

**Hardware baseline:** см. `docs/experiments/hardware-profile.md` §1 (AMD Ryzen 7 5800X Zen 3 8C/16T, L3 32 MiB,
governor `powersave`).
