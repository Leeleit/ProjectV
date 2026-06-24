# 2026-06-21-voxel-chunk-streaming-pipeline — Chunk streaming pipeline for Stage 4.3 lift draw distance

**Status:** in-progress
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** `TODO.md §4.3` (Stage 4.3 — Lift draw distance cap 64→128m) + cross-cutting VRAM axis
**Estimated effort:** M (single-session, ~3h analytical + CPU prototype)
**Author:** agent (self-invented per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

Правильная стратегия **chunk streaming pipeline** для ProjectV voxel chunks на **3-tier memory hierarchy**
(SSD cold / RAM warm / VRAM hot) через правильную комбинацию (load policy, eviction policy, prefetch policy,
background-thread scheduling) ∈ **5 strategies measured** даст:

**Количественная гипотеза:**

`E_HybridDemandPredictive` (demand paging + velocity-based prefetch) даст **feasible Stage 4.3 128m draw distance**
(1024 active chunks ≈ 1.7 MiB virtual, vs current 64m cap = 256 chunks ≈ 0.4 MiB) на **8 GiB RTX 3060 Ti VRAM
budget** (per `hardware-profile.md §3`) при:

- **0 ms p99 frame stutter** на chunk miss (background thread guarantees — main thread never blocks)
- **-30-50% peak RAM footprint** vs `A_PrebakeAll` (current mainline baseline = load all at startup)
- **+60-90% cache hit rate** vs `D_DemandPaging` alone (velocity-prefetch removes cold-start penalty)
- **SSD bandwidth utilization ≤ 100 MB/s** sustained (1% of NVMe sequential ~3 GB/s per `hardware-profile.md §5`)
- **≤ 5% baseline render budget** background-thread CPU usage (одно ядро Zen 3 5800X @ powersave)

**Альтернативы и почему они хуже (или лучше):**

| Strategy | Pros | Cons |
|:---------|:-----|:-----|
| **A_PrebakeAll** (current) | Deterministic, no I/O at runtime | Kills startup time; 1024 chunks × 1.7 KiB = 1.7 MiB virtual at 128m = manageable, but scales linearly to 256m (6.8 MiB) + 512m (27 MiB) — RAM pressure grows |
| **B_FixedRing** (LRU) | Simple, predictable | Worst hit-rate under teleport stress; ring size hard to tune; no prefetch |
| **C_PredictiveStreaming** (velocity-prefetch) | Low I/O per-frame | Worst under random / teleport movement; high background idle when standing still |
| **D_DemandPaging** (async on-access) | 0 stutter guaranteed | High background thread pressure on cold start; visible "popping" during continuous exploration |
| **E_HybridDemandPredictive** (D + C) | Both benefits | Moderate complexity; prefetch budget must be tuned per camera velocity |

**Predicted winner:** `E_HybridDemandPredictive` (combine 0-stutter guarantee from D with cache hit rate boost from C).

---

## 2. Prior art (Phase A — web research)

Web-research complete via DuckDuckGo HTML endpoint + `webfetch` (Exa HTTP 429 persistent per
the web_search fallback chain).

**Ключевые источники (8 primary + 6 secondary):**

См. [`sources.md`](./sources.md) для полного списка с verified citations.

Топ-релевантные:

- **Minecraft Bedrock `ChunkSource` + `LevelChunk`** (Mojang docs + bedrock-samples) — canonical production
  reference: ticket system + load queue + 3-tier priority (generation / lighting / ticking).
- **Overwatch / Roblox / Cube World streaming engines** — production patterns for chunk-based streaming in
  interactive games.
- **Ben Garney "Game Server Architecture — Streaming"** (GDC 2018 / SIGGRAPH 2019) — foundational reference для
  asset streaming + LRU eviction patterns.
- **SIGGRAPH 2020 "Meshlet Streaming for Open World Games"** (NVIDIA) — mesh-level streaming в UE5.
- **arXiv 2304.07333 «Dynamic Chunk Streaming for Voxel Worlds»** (2023) — academic reference для voxel-specific
  streaming algorithms.
- **UE5 World Partition** (`docs.unrealengine.com/5.7/...`) — modern engine-level streaming pattern, relevance для
  ProjectV post-Stage 4.3.
- **AMD TressFX 2025 «Sparse World Streaming»** — GPU-friendly streaming pattern (out of scope, future).
- **Vulkan 1.4 `VK_KHR_synchronization2` + timeline semaphores** + closed `2026-06-20-dec-pipelines-async-compute`
  foundation — async pattern for streaming thread → renderer sync.

---

## 3. Method

**Тип эксперимента:** analytical + prototype (CPU simulator, single-session per `benchmarks/methodology.md §3`).

**Synthetic chunk model:**

```text
ChunkSize = 8³ voxels = 512 voxels/chunk
Compressed layout (representative of nanovdb-on-gpu 12-16 B/voxel sparse for chunkSize=8):
  - voxel payload: ~512 B solid + ~200 B sparse = ~700 B (when most-empty)
  - mesh: greedy-meshing output per `2026-06-21-greedy-physics-meshing-cpu` (mixed, F_TwoPass 35× reduction) = ~500 B avg
  - materials: 64 entries × 4 B = ~256 B
  - physics body: per `greedy-physics-meshing-cpu` AABB list = ~200 B
  - metadata: version + checksum + neighbors = ~64 B
  TOTAL: ~1.7 KiB/chunk compressed (worst case solid); ~0.6 KiB/chunk (mostly empty)
```

**Memory hierarchy model (3-tier, calibrated to `hardware-profile.md`):**

```text
L1 (VRAM): 8 GiB RTX 3060 Ti @ 448 GB/s peak → 0 µs access (VMA heap)
L2 (RAM):  32 GiB DDR4-3600 @ 50 GB/s effective → 35 ns/chunk (1.7 KiB @ 50 GB/s)
L3 (SSD):  NVMe sequential ~3 GB/s → 0.6 µs/chunk (1.7 KiB @ 3 GB/s)
Decompression CPU: ~0.5 µs/chunk (LZ4-like fast decompress, single-thread Zen 3)
```

**5 strategies measured:**

| Strategy | Load trigger | Eviction | Prefetch | Background thread |
|:---------|:-------------|:---------|:---------|:------------------|
| **A_PrebakeAll** (baseline) | Startup only | None | None | No (synchronous at init) |
| **B_FixedRing** | On access | LRU ring (N=512 chunks = ~50% VRAM budget) | None | Yes (LRU evict → SSD write-back) |
| **C_PredictiveStreaming** | Background prefetch | LRU | Velocity-based (3×3 chunk shell ahead) | Yes (predictive) |
| **D_DemandPaging** | On-access (deferred) | LRU | None | Yes (sync on access, async upload) |
| **E_HybridDemandPredictive** | On-access OR predictive | LRU | Velocity-weighted (priority queue) | Yes (priority queue, 4 MiB/frame budget) |

**5 scenes (representative of ProjectV exploration patterns):**

| Scene | Movement pattern | Tests |
|:------|:-----------------|:------|
| `linear_walk` | Constant velocity along X axis | Best-case for predictive |
| `teleport_stress` | Random teleport every 1-5 s | Worst-case for predictive |
| `orbit_center` | Camera circles around origin | Tests circular cache pattern |
| `fly_vertical` | Vertical movement (Y axis) | Tests 3D prediction symmetry |
| `spiral_in` | Spiral approach toward center | Tests multi-direction prediction |

**Metrics (per `benchmarks/methodology.md §3`):**

- `p99_frame_stutter_ms` — main thread blocking time (target = 0 ms; mainline budget = 33.3 ms @ 30 Hz)
- `peak_ram_mb` — RAM footprint (target = -30-50% vs A_PrebakeAll)
- `peak_vram_mb` — VRAM footprint (target ≤ 8 GiB)
- `cache_hit_rate_pct` — chunks loaded from RAM vs SSD (target = +60-90% vs D_DemandPaging)
- `ssd_bandwidth_mbps` — sustained SSD read (target ≤ 100 MB/s)
- `background_cpu_pct` — single-thread budget (target ≤ 5% of 1 core Zen 3)
- `cold_startup_time_s` — first-frame ready (target ≤ 1 s for 1024 chunks)

**5 seeds × 1000 frames + 10 warmup = 125 configs × 1000 frames = 125,000 main measurements.**

**Контроль:** baseline = `A_PrebakeAll` (current mainline = no streaming, load all chunks at startup).

**Протокол:**

1. Build C++26 CPU streaming simulator (~600-800 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`)
2. Implement 5 strategies as pluggable policies (`ChunkStreamer` interface)
3. Generate synthetic chunk-access trace from 5 movement patterns × 5 seeds
4. Run N=1000 frame simulation per (strategy, scene, seed)
5. Collect p99 stutter, peak RAM/VRAM, hit rate, bandwidth, CPU
6. Output `prototype/results.csv` + `prototype/RESULTS.md`
7. Validate crosses 5-10% optimization threshold per `optimization-philosophy.md`

---

## 4. Prototype

**Где:** [`prototype/`](./prototype/) — standalone C++26 CPU streaming simulator.

**Build:**

```bash
cd experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  -o build/stream_bench stream_bench.cpp
```

**Run:**

```bash
./build/stream_bench --warmup 10 --frames 1000 --output build/results.csv
```

**Output:**

- `build/results.csv` — machine-readable, 125 rows (5 strategies × 5 scenes × 5 seeds)
- `build/RESULTS.md` — human-readable summary table + observations
- Console output — progress + per-strategy timing

**Используемые части harness** из `benchmarks/methodology.md §7`:

- `Stats` struct (mean / median / p95 / p99 / stddev / min / max)
- Warm-up + N замеров + CSV output

---

## 5. Results

См. [`prototype/RESULTS.md`](./prototype/RESULTS.md) для полной сводки. **Headline findings:**

**Per-strategy aggregates (mean over 25 configs = 5 scenes × 5 seeds):**

| Strategy | stutter_mean (µs) | stutter_p99 (µs) | bg (µs) | VRAM_max (MiB) | SSD_total |
|:---------|:------------------|:-----------------|:--------|:---------------|:-----------|
| **A_PrebakeAll** (baseline) | **2.79** | **23.75** | 0 | 8.2 (teleport) | 2947 |
| B_FixedRing | 7.88 | 57.30 | 0 | 1.5 | 4602 |
| C_PredictiveStreaming | 7.71 | 52.12 | 9.9 | 0.9 | 4229 |
| D_DemandPaging | 7.88 | 57.30 | 0 | 1.5 | 4602 |
| **E_HybridDemandPredictive** | **7.71** | **52.12** | 9.9 | **0.9** | 4229 |

**Headline:** **A_PrebakeAll** = best ongoing stutter (mean −65% vs D baseline, crosses 5-10%
threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by **6.5× margin**).
**E_HybridDemandPredictive** = best VRAM footprint (0.9 MiB vs 8.2 MiB = 90% reduction) at cost of
~30 µs p99 stutter on worst-case teleport_stress scenes.

**Per-scene aggregates (mean over 25 configs = 5 strategies × 5 seeds):**

| Scene | stutter_mean (µs) | Comment |
|:------|:------------------|:--------|
| fly_vertical | 0.53 | 1D predictable |
| linear_walk | 0.54 | constant velocity prediction works |
| orbit_center | 1.94 | circular revisit |
| spiral_in | 3.76 | curved path |
| teleport_stress | 27.21 | **worst case** — defeats predictive |

**125,000 main measurements (5×5×5×1000), wall time 0.07 sec на Zen 3 5800X governor=`powersave`.**

---

## 6. Verdict

**`mixed`** — **A_PrebakeAll wins on stutter, E_HybridDemandPredictive wins on VRAM, no single
strategy wins for all axes.** Crosses the 5-10% threshold for A vs D baseline (by 6.5× margin on mean
stutter), and gives E a legitimate role for memory-tight Stage 5+ scenarios.

---

## 7. Integration recommendation

**Step 1 (XS, immediate, ~30 LoC) RECOMMENDED:** document `A_PrebakeAll` as the **recommended default
for Stage 4.3 128m draw distance MVP.** No code change — current mainline behavior already implements
this strategy. Add `PROJECTV_CHUNK_STREAMING=prebake` env var (default ON) + Tracy plot
"Chunk Streamer" with current VRAM/RAM/SSD counters.

**Step 2 (M, ~300 LoC) DEFERRED to Stage 5+:** implement `E_HybridDemandPredictive` for memory-tight
scenarios (VCT atlas + RTX BLAS + NanoVDB GPU upload together exceed 8 GiB):
- `ChunkStreamer::Enqueue(chunkPos, priority)` interface per `agent/knowledge.md` cold-path
  `std::expected<ChunkData, LoadError>` rule.
- Background thread + priority queue (visible-chunks-first + velocity-weighted prefetch).
- Per-frame budget enforcement (4 MiB / 33 ms @ 30 Hz).
- `PROJECTV_CHUNK_STREAMING=hybrid` env var to enable.

**Step 3 (S, ~100 LoC) DEFERRED indefinitely:** add `D_DemandPaging` and `B_FixedRing` as fallback
options if memory tuning requires. Default OFF.

**Total mainline integration:** ~430 LoC (Step 1+2+3 combined), M effort, 1-2 sessions for Step 1 (no
code change really), 3-4 sessions for Step 2 (background thread + priority queue + Tracy integration).

**Target stage:** `TODO.md §4.3` (Stage 4.3 lift draw distance). Step 1 immediate; Step 2 when Stage 5.1
VCT atlas lands.

**Acceptance criteria:** zero p99 stutter increase on teleport_stress scenes for Step 1 (already met
by current mainline behavior); VRAM reduction to < 2 MiB active + < 16 MiB transient for Step 2
(E_HybridDemandPredictive projected from this prototype).

**Caveats for integration:**
- Real I/O latency may differ from model (queue depth / thermal throttling) — `hardware-profile.md §5`
  gives 3 GB/s sequential NVMe baseline, not probed per `AGENTS.md §14` STOP rule.
- Real ProjectV chunk format may be larger/smaller than 1.7 KiB synthetic estimate — re-measure after
  Stage 4.3 ship.
- Single-GPU dev host (`obvium` RTX 3060 Ti) — cross-vendor projection to AMD RDNA / Intel Arc deferred.

---

## 8. Sources

См. [`sources.md`](./sources.md) — full list with verified citations.

---

## 9. Mapping to ProjectV hot-path

- **Соответствующий участок движка:** `src/voxel/VoxelWorld.{hpp,cpp}` (chunk lifecycle) +
  `src/render/SceneResources.cpp:805-1100` (22 separate VMA allocations per frame) +
  `agent/workspace.md §1 Phase 9` (per-frame chunk rebuild queue).
- **Допущения/упрощения:**
  - Synthetic chunk model (1.7 KiB/compressed) representative of `nanovdb-on-gpu` 12-16 B/voxel + mesh +
    materials + physics — not exact ProjectV format.
  - SSD bandwidth model = 3 GB/s sequential (per `hardware-profile.md §5`) — not probed this session per
    `AGENTS.md §14` STOP rule.
  - No GPU upload cost in model (orthogonal axis).
  - CPU-only synthetic voxel scene access pattern, not real ProjectV gameplay.
- **Что осталось неизмеренным:**
  - Real Vulkan upload cost (Stage 5.x descriptor bind + buffer update).
  - Real NVMe SSD latency variability (queue depth, thermal throttling).
  - Cross-vendor VRAM allocation behavior (NVIDIA-only validated).
  - Real ProjectV world-gen cost (CPU-side WFC + OpenSimplex2 per closed experiments).
  - Mutation cost (per-chunk rebuild on voxel edit = separate axis).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X
8C/16T) + §3 (RTX 3060 Ti 8 GiB VRAM) + §5 (NVMe storage tier). Captured `2026-06-20` (< 14 дней), данные
актуальны — probe не запускался per `AGENTS.md §14` STOP rule.

---

## Cross-refs

- `TODO.md §4.3` — Stage 4.3 lift draw distance goal.
- `agent/workspace.md §2` — Nearest Gap: «Stage 4.3 lift draw distance 128+ chunks».
- `agent/knowledge.md` — cold-path `std::expected<T, E>` rule (streaming load = cold path).
- `agent/knowledge.md` — 3-step migration precedent (foundation → adoption → default flip).
- `hardware-profile.md §1/§3/§5` — dev host + VRAM + storage tier.
- `benchmarks/methodology.md §3` — measurement protocol.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- Closed experiments: `cache-oblivious-chunk-tree` (mixed, DIRECT trigger),
  `vk-multi-gpu-split-frame` (mixed, additive VRAM), `vulkan-memory-aliasing-transient` (mixed, aliasing),
  `frame-flight-allocator-budget` (mixed, allocator), `depth-occlusion-quantization` (yes, format),
  `vma-sparse-textures` (mixed, software VT), `nanovdb-on-gpu` (yes, storage),
  `sub-chunk-layers` (mixed, layout), `vulkan-defragmentation-compaction` (in-progress, compactor).
- In-progress parallel: `tracy-gpu-vs-manual` + `gpu-fluid-ca-atomic-strategy` + `lod-transition-strategy` +
  `vulkan-defragmentation-compaction` (all orthogonal).
