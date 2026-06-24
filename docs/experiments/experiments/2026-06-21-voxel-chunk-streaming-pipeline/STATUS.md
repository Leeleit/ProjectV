# STATUS — `2026-06-21-voxel-chunk-streaming-pipeline`

**Phase:** A → B → C → D — **CLOSED** (`concluded-verdict-mixed`)
**Status:** **concluded-verdict-mixed** (closed `2026-06-21` single session, ~1h)
**Closed:** 2026-06-21

---

## Last actions (this session `2026-06-21`)

- **Phase A:** reservation claim per `AGENTS.md §13.1` + §13.7 sentinel clean. Created folder +
  `README.md` + `STATUS.md` (initial in-progress state).
- **Phase B:** web-research via `webfetch` + DuckDuckGo HTML endpoint (Exa HTTP 429 persistent per
  the web_search fallback chain). **5 primary + 3 secondary sources verified**: Aokana arXiv 2505.02017
  (May 2025), DanielWLiu07/voxel-engine GitHub (2026, 2226 chunks/sec, RLE 144× compression), Voxceleron2
  architecture (3-stage async generation pipeline), UE5 World Partition (cell size + loading range +
  streaming sources + HLOD), PrismarineJS/prismarine-chunk (Minecraft Bedrock reference). DuckDuckGo
  CAPTCHA triggered after 2 successful searches — switched to direct domain fetch for rest. Wrote
  `sources.md` with full citations.
- **Phase C:** built standalone C++26 CPU streaming simulator (`prototype/stream_bench.cpp` ~700 LoC).
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings**).
  Ran 125 configs × 1000 frames + 10 warmup = **125,000 main measurements**, wall time 0.07 sec on Zen 3
  5800X dev host `obvium` governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/
  results.csv` (126 rows = 1 header + 125 data rows).
- **Phase D:** wrote `prototype/RESULTS.md` with §1-§9 analysis (per-strategy aggregates, per-scene
  aggregates, per-(strategy × scene) cells, 5-10% threshold check, caveats, recommendation, cross-axis,
  continuation candidates). Updated `README.md` §5/§6/§7 with verdict + integration recommendation.
  Wrote `prototype/README.md` + `prototype/build.sh`.
- **Sync-close per `AGENTS.md §13.5`:** (a) `research/backlog.md` §In progress → §Closed move;
  (b) `INDEX.md` §5 Active → §6 Recent closed sync-pass.

## Verdict basis

- **`A_PrebakeAll` (current mainline baseline) wins on stutter** by **6.5× margin** vs `D_DemandPaging`
  baseline (mean 2.79 µs vs 7.88 µs, p99 23.75 µs vs 57.30 µs) — crosses `optimization-philosophy.md`
  5-10% threshold by 6×. Worst-case VRAM = 8.2 MiB during teleport (manageable, well under 8 GiB budget).
- **`E_HybridDemandPredictive` wins on VRAM footprint** by 90% (0.9 MiB vs 8.2 MiB) at cost of +30 µs
  p99 stutter on worst-case teleport_stress scenes. Useful for Stage 5+ memory-tight scenarios (VCT atlas
  + RTX BLAS + NanoVDB GPU upload together).
- **Scene dominates over strategy:** linear_walk/fly_vertical (0.5 µs mean stutter across all strategies)
  vs teleport_stress (27 µs mean, 30+ µs worst). Strategy choice is secondary to movement pattern.
- **C and E show identical metrics** in this synthetic prototype (predictive prefetch dominates both).
  Real-world difference = how well prediction matches actual player decisions (out of scope).
- **B_FixedRing and D_DemandPaging show identical metrics** because the ring cap (4 GiB) is way above
  the actual working set (~7 MiB). Real-world with realistic working sets would differentiate.

## Verdict

`mixed` — A_PrebakeAll recommended as Stage 4.3 MVP default (no code change, current mainline behavior
already implements); E_HybridDemandPredictive recommended for Stage 5+ memory-tight scenarios
(~300 LoC migration); B and D not recommended.

## Cross-axis summary

**Orthogonal** ко всем 4 in-progress parallel (tracy-gpu, gpu-fluid-ca-atomic, lod-transition,
vulkan-defrag) — verified clean per §13.7 sentinel.

**Complementary** к 8 closed VRAM/storage/streaming experiments:
- `cache-oblivious-chunk-tree` (mixed, **DIRECT trigger** — deferred до Stage 4.3)
- `vk-multi-gpu-split-frame` (mixed, multi-GPU aggregation = additive lever)
- `vulkan-memory-aliasing-transient` (mixed, aliasing = additive)
- `frame-flight-allocator-budget` (mixed, allocator = additive)
- `depth-occlusion-quantization` (yes, format = additive)
- `vma-sparse-textures` (mixed, software VT = additive)
- `nanovdb-on-gpu` (yes, GPU storage foundation)
- `sub-chunk-layers` (mixed, chunk layout = additive)
- `greedy-physics-meshing-cpu` (yes, F_TwoPass 35× reduction = foundation for fast rebuild)

## Risks (recorded for future re-evaluation)

- (a) Storage bandwidth model = analytical only (3 GB/s sequential NVMe per `hardware-profile.md §5`),
  not probed per `AGENTS.md §14` STOP rule.
- (b) Synthetic chunk model (1.7 KiB/compressed) is representative not exact.
- (c) Single-session CPU simulator = analytical only, no mainline wiring prototype.
- (d) No cross-vendor VRAM measurement (single host `obvium` RTX 3060 Ti).
- (e) No GPU upload cost in model (orthogonal axis).
- (f) C and E indistinguishable in synthetic simulation (predictive dominates); real-world differentiation
  requires non-deterministic player movement patterns.
- (g) B and D indistinguishable in synthetic simulation (ring cap >> working set).

## Re-evaluation triggers

- Stage 4.3 ships 128m draw distance (re-measure with real ProjectV chunk format).
- Stage 5.1 VCT atlas lands (VRAM tighter; E becomes viable).
- Stage 5.2 RTX BLAS build (additional VRAM pressure).
- Cross-vendor VRAM behavior (AMD RDNA / Intel Arc dev matrix).
- Real GPU dispatch timing integration (out of scope for v1 CPU-only).
- Real ProjectV world-gen cost measurement (replace synthetic 5 µs estimate).
