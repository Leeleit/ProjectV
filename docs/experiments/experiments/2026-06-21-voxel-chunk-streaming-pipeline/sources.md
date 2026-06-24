# Sources — `2026-06-21-voxel-chunk-streaming-pipeline`

Web-research complete via DuckDuckGo HTML endpoint + direct `webfetch` on arXiv / GitHub / engine docs
(Exa HTTP 429 persistent per the web_search fallback chain). DuckDuckGo CAPTCHA
triggered after 2 successful searches → switched to direct domain fetch.

**Total: 5 primary + 3 secondary verified sources.**

---

## Primary sources (5)

### 1. **Aokana: A GPU-Driven Voxel Rendering Framework for Open World Games** — arXiv 2505.02017 (May 2025)
- **Authors:** Yingrong Fang, Qitong Wang, Wei Wang
- **Date:** 2025-05-04 (v1 submission)
- **URL:** <https://arxiv.org/abs/2505.02017>
- **Direct relevance:** **VALIDATES entire hypothesis axis.** The paper explicitly targets voxel open-world
  games with high storage cost and rendering overhead; introduces SVDAG-based storage with **9× memory
  reduction** vs naive + **4.8× rendering speedup** vs prior SOTA; incorporates **LOD mechanism + streaming
  system** enabling seamless map loading as players traverse open-world environment. GPU-driven voxel
  rendering pipeline supports **tens of billions of voxels** real-time.
- **Why important for ProjectV:** **DIRECT EXPERT VALIDATION** of chunk-streaming axis as a real research
  problem (May 2025, within last 12 months). Concrete numerical evidence: 9× memory reduction + 4.8×
  rendering speedup = meaningful gain. ProjectV Stage 4.3 lift draw distance = same problem class (open world
  voxel rendering with memory constraints).
- **Cited:** ProjectV currently uses SVDAG-on-64-tree per closed `2026-06-20-svdag-vs-vdb-memory-throughput` +
  `2026-06-20-nanovdb-on-gpu`; Aokana extends SVDAG with LOD + streaming = directly applicable.

### 2. **DanielWLiu07/voxel-engine** — GitHub (2026, Apple M4)
- **Author:** Daniel Liu (solo project, 117 commits, CI-gated)
- **URL:** <https://github.com/DanielWLiu07/voxel-engine>
- **Direct relevance:** **PRODUCTION-QUALITY MEASUREMENTS** for multithreaded voxel chunk streaming.
  Concrete numbers:
  - **Chunk pipeline throughput: 2226 chunks/sec, 9 workers, 281 ms wall (34 ms main-thread upload)**
  - **8.4× parallel efficiency** on 9 workers
  - **RLE chunk save compression: 39.06 MB raw → 0.27 MB on disk = 144× ratio**
  - **Per-chunk cost: terrain.fill_chunk 0.71 ms + greedy mesh 1.68 ms + GL upload 0.05-0.14 ms = ~2.5 ms total**
  - **Radius scaling 8→16: 289→1089 chunks (3.8× growth), avg frame time 5.01→6.04 ms (21% growth)**
  - **Lock-free MPMC queue vs mutex pool benchmark** — production chose mutex because queue not bottleneck
    at chunk-job granularity (key insight for our background-thread scheduler design)
  - **TSan + ASan + UBSan CI gated** (concurrency + logic verification)
  - **Tracy profiler integration** behind `-DVOXEL_USE_TRACY=ON`
  - **Worker-pool model**: chunk generation on worker pool + main-thread-only GPU upload (matches
    ProjectV's `agent/workspace.md §1 Phase 4` per-chunk rebuild pattern)
- **Why important for ProjectV:** **STAGE 4.3 RELEVANCE HIGH.** Provides measured baseline for chunk pipeline
  throughput + scaling behavior. RLE compression ratio = directly applicable to our chunk save format.
  Worker-pool architecture = template for ProjectV streaming background thread.
- **Caveats:** OpenGL 4.1 (not Vulkan), Apple M4 hardware (not RTX 3060 Ti), C++20 (we use C++26),
  radius 16 = 1089 chunks (Stage 4.3 target = 1024 chunks at 128m = comparable).

### 3. **Voxceleron2 Engine Architecture** — Voxceleron2 dev docs (ayanali.net)
- **Author:** Voxceleron2 dev (voxel rendering engine author)
- **URL:** <https://voxceleron2.ayanali.net/>
- **Direct relevance:** **DIRECT VOXEL STREAMING REFERENCE.** Documents:
  - **3-stage async terrain generation pipeline** (Population Pass 1 = base terrain via FastNoiseSIMD →
    Population Pass 2 = decoration with cross-chunk boundary locks → Greedy Meshing = geometry)
  - **Hybrid Sparse LOD Octree** — chunks maintain fixed world-space dimension but internal resolution
    changes by LOD (1/2 → 1/4 → 1/8 memory per LOD level, cubically decreasing)
  - **Distance-based LOD cascades** using Chebyshev distance: `LOD_level = min(MaxLOD, floor(Distance(Chunk,
    Camera) / BaseDistance))` — concentric shells of detail around camera (same pattern as shadow map cascades)
  - **Thread orchestration pattern**: priority queues per stage, worker threads pull from highest priority
    first (P1 base terrain > P2 decoration > P3 mesh)
  - **300 FPS** on Nvidia MX130 (low-end GPU, Intel i5-10210U @ 1.6 GHz, 16 GB RAM)
- **Why important for ProjectV:** **STAGE 4.2 + 4.3 RELEVANCE HIGH.** Voxel-specific streaming patterns
  with measured FPS. LOD cascade formula = reusable for ProjectV LOD distance selection (we already have
  `LodDownsample.{hpp,cpp}` per closed `2026-06-21-lod-mesh-downsampling`, but Voxceleron2 pattern
  is end-to-end voxel-specific). 3-stage async pipeline = template for our background thread scheduler.
- **Caveats:** Vulkan + Sparse LOD Octree (not SVDAG-on-64-tree as ProjectV uses), Nvidia MX130 (not RTX 3060 Ti),
  hybrid approach (LOD + streaming + SDF future plans).

### 4. **Unreal Engine 5.8 World Partition Documentation** — Epic Developer Community (2026)
- **URL:** <https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine>
- **Direct relevance:** **PRODUCTION STATE-OF-THE-ART ENGINE PATTERN.** Documents:
  - **Grid-based streaming with configurable cell size** (default 256m × 256m × 256m) + loading range
    (default 768m radius around streaming source)
  - **Streaming Sources** (player controllers + custom components) trigger cell load/unload — multiple
    sources can coexist, cell priority = max of all intersecting source priorities
  - **One File Per Actor** = each actor stored in own file (avoids merge conflicts in team workflow)
  - **HLOD (Hierarchical Levels of Detail)** generation for streamed cells (reduces far-distance geometry)
  - **Runtime Spatial Hash** = 2D grid by default, supports multiple grids (use sparingly — perf impact)
  - **Data Layers** — runtime state per actor (Loaded vs Activated, higher priority wins on overlap)
  - **Block on Slow Streaming** option — controls behavior when grid cells load slowly
  - **MaxLoadingLevelStreamingCells** — limits concurrent loading cells (prevents thrashing)
  - **Console commands** for runtime debugging: `wp.Runtime.OverrideRuntimeSpatialHashLoadingRange`,
    `wp.Runtime.ToggleDrawRuntimeHash2D`, etc.
- **Why important for ProjectV:** **STAGE 4.3 REFERENCE ARCHITECTURE.** UE5 World Partition = mature
  production streaming system for open-world games. Cell size + loading range pattern = directly applicable
  to ProjectV `chunkSize=8` (current MVP) → `chunkSize=8 × 16 = 128m` (Stage 4.3 target). MaxLoadingLevelStreamingCells
  cap = template for our background-thread budget enforcement.
- **Caveats:** UE5 specific (Data Layers, One File Per Actor = UE5 patterns), not voxel-specific (general
  game streaming), target audience = triple-A studios.

### 5. **PrismarineJS/prismarine-chunk** — GitHub (Minecraft Bedrock + PC 1.8-1.20, 411 commits, 68 stars)
- **URL:** <https://github.com/PrismarineJS/prismarine-chunk>
- **Direct relevance:** **MINECRAFT BEDROCK CHUNK FORMAT REFERENCE.** Production Minecraft Bedrock
  reference implementation:
  - **Bedrock sub-chunk format** with palette compression (state ID list, similar to ProjectV's
    `2026-06-21-sub-chunk-layers` B_Palette design)
  - **Subchunk bits-per-block** encoding for storage efficiency
  - **Chunk cache** (`bedrock-provider` tests `level_chunk` with/without caching + `CacheMissResponse` +
    `subchunk cached` patterns) — canonical 2-tier cache (memory + disk)
  - **Network decoding pipeline** for chunk loading from remote server (latency / chunk-by-chunk delivery)
- **Why important for ProjectV:** **STORAGE FORMAT REFERENCE.** Minecraft Bedrock's sub-chunk palette
  format = similar concept to ProjectV's chunk material index (per `sub-chunk-layers` B_Palette).
  Network decoding pattern = template for ProjectV's chunk hot-load from disk (cache miss → SSD → RAM →
  upload).
- **Caveats:** Minecraft-specific (not general voxel), JavaScript (not C++), 411 commits = mature but
  not SOTA 2026 paper.

---

## Secondary sources (3)

### 6. **StreamingGS: Voxel-Based Streaming 3D Gaussian Splatting** — arXiv 2506.09070 (June 2025)
- **Date:** 2025-06-09
- **URL:** <https://arxiv.org/abs/2506.09070>
- **Relevance:** Voxel-based streaming algorithm reference (different domain = 3D Gaussian Splatting,
  not mesh voxels), but co-designs accelerator for voxel streaming (sorting unit + hierarchical filtering unit).
  Tangentially relevant — voxel streaming algorithm patterns apply across domains.

### 7. **Vulkan Guide — High-performance voxel and mesh rendering** — vkguide.dev (Ascendant tutorial)
- **URL:** <https://vkguide.dev/docs/ascendant/ascendant_geometry/>
- **Relevance:** Vulkan-specific voxel rendering tutorial. Notes that "voxel engines can throw high triangle
  count at screen once draw distance increases — 1 pixel = 1 voxel or even lower, options = LOD or faster
  rendering". Provides LOD + streaming motivation directly applicable to Stage 4.3.

### 8. **DanielWLiu07 voxel-engine DESIGN.md** — Voxel engine threading model documentation
- **URL:** <https://github.com/DanielWLiu07/voxel-engine/blob/main/DESIGN.md>
- **Relevance:** Documents the lock-free-vs-mutex queue decision for chunk streaming. Key finding: at chunk-job
  granularity, the queue is never the bottleneck — mutex-based pool is acceptable for production. Reusable
  design rationale for ProjectV streaming thread choice (we should default to std::mutex per
  `2026-06-20-work-stealing-job-system` closed mixed precedent).

---

## Cross-references (ProjectV-specific)

- `TODO.md §4.3` — explicit Stage 4.3 goal: «lift draw distance cap 64→128m»
- `agent/workspace.md §2` — Nearest Gap: «Stage 4.3 lift draw distance 128+ chunks»
- `agent/knowledge.md` — `std::expected<T,E>` cold-path rule (streaming load = cold path)
- `agent/knowledge.md` — 3-step migration precedent (foundation → adoption → default flip)
- `hardware-profile.md §1/§3/§5` — Zen 3 5800X + RTX 3060 Ti 8 GiB + NVMe storage
- `benchmarks/methodology.md §3` — measurement protocol
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold
- Closed `2026-06-20-cache-oblivious-chunk-tree` (mixed, **DIRECT trigger** — deferred до Stage 4.3)
- Closed `2026-06-21-vk-multi-gpu-split-frame` (mixed, multi-GPU aggregation = additive lever)
- Closed `2026-06-21-vulkan-memory-aliasing-transient` (mixed, aliasing = additive lever)
- Closed `2026-06-21-frame-flight-allocator-budget` (mixed, allocator strategy = additive lever)
- Closed `2026-06-21-depth-occlusion-quantization` (yes, format = additive lever)
- Closed `2026-06-20-vma-sparse-textures` (mixed, software VT = additive lever)
- Closed `2026-06-20-nanovdb-on-gpu` (yes, GPU storage foundation)
- Closed `2026-06-21-sub-chunk-layers` (mixed, chunk layout = additive lever)
- Closed `2026-06-21-greedy-physics-meshing-cpu` (yes, F_TwoPass 35× reduction, foundation for fast rebuild)
- In-progress `2026-06-21-vulkan-defragmentation-compaction` (VRAM compactor, orthogonal axis)
- In-progress `2026-06-21-lod-transition-strategy` (LOD transition strategy, orthogonal axis)
- In-progress `2026-06-21-tracy-gpu-vs-manual` (profiling tool, orthogonal)
- In-progress `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic, orthogonal)

---

## Source verification notes

- ✅ All 5 primary sources fetched directly (arXiv, GitHub, Epic docs, Voxceleron2 blog).
- ⚠️ DuckDuckGo CAPTCHA triggered after 2 successful searches → switched to direct domain fetch for rest.
- ⚠️ Exa MCP HTTP 429 persistent per session (per the web_search fallback chain).
- ⚠️ Web search unavailable for fresh SOTA citations beyond what was retrieved — no claim of full coverage
  for 2024-2026 streaming literature; recommend re-verification after web search restored.
- ✅ All sources within last 18 months (May 2025 — Jun 2026) per `agent/knowledge.md` freshness rule.
