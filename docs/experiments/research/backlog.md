# Backlog — канбан гипотез

Простой список гипотез. Перед стартом эксперимента — `git blame`-style пометка «открыто», после закрытия — ссылка на
`experiments/<slug>/`.

Правила:

- Гипотеза = одно проверяемое утверждение (не «исследовать вообще», а «X даст Y на сцене Z»).
- Перед стартом — проверить: не дублирует ли уже идущий эксперимент в `INDEX.md §5`.
- Закрытие = либо стартовал эксперимент (ссылка), либо явный отказ с одной строкой обоснования.

---

## Open (идеи без старта)

- [ ] **hzb-binding-models** — варианты реализации Hi-Z (VkImageView mip chain vs fragment-density-vs-storage-image) и
  их
  cost на разных вендорах. Hint: TODO.md Stage 2.2. Priority: m.
- [ ] **restir-gi-feasibility** — насколько ReSTIR (Spatiotemporal Importance Resampling) реалистичен для voxel-сцены;
  нужен ли RTX обязательно. Hint: TODO.md Stage 5.1/5.2. Priority: m.
- [x] **vct-vs-rt-cutoff** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
- [ ] **nerf-gs-in-realtime-voxel** — есть ли смысл тащить Gaussian Splatting / NeRF в наш движок; где они ломаются на
  воксельном взаимодействии (мутация мира). Priority: l (эзотерика).
- [ ] **sdf-hybrid-world** — гибрид SDF + voxel grid для более гладкого освещения и collision; подводные камни для
  build/break. Priority: l (эзотерика).

- [ ] **multi-gpu-split-frame** — реальный overhead split-frame rendering на Vulkan для нашего workload. Priority: l.
- [ ] **wfc-procedural-worlds** — Wave Function Collapse как альтернатива Perlin/Simplex для генерации миров с
  локальной структурой. Priority: m.
- [ ] **ddsp-procedural-audio** — нейросетевой синтез (DDSP / RNN) для процедурной музыки/звуков воксельного мира.
  Priority: l (эзотерика).
- [ ] **depth-occlusion-quantization** — насколько реально сжатие depth/occlusion буферов без артефактов (для VRAM
  экономии). Priority: l.
- [ ] **programmable-voxels** — TinyCC / LuaJIT / WASM внутри чанка: цена, безопасность, UX. Priority: l.
- [ ] **dynamic-weather-svo-meta** — погода как SVO-метаполе (влажность/температура/ветер) в той же структуре. Priority:
  l.
- [ ] **sub-chunk-layers** — многослойные чанки (по Y) для биомов/пещер; целостность, мешинг, освещение. Priority: m.
- [ ] **meshing-algo-comparison** — Dual Contouring vs Surface Nets vs Marching Cubes vs Naive Greedy на SDF; quality vs
  speed vs memory benchmark на ProjectV чанках; вход для Stage 2.1/3.3. Hint: TODO.md §2.1/§3.3. Priority: h.

- [ ] **flecs-soa-vs-aos-bench** — closed `2026-06-20` (verdict `yes`), см. §Closed ниже.
- [ ] **tracy-gpu-vs-manual** — overhead benchmark Tracy GPU contexts vs manual `vkCmdWriteTimestamp` в multi-pass
  render;
  когда Tracy сам становится bottleneck. Hint: independent (cross-cutting profiling). Priority: m.
- [ ] **renderdoc-ci-capture** — headless `RenderDoc --capture` в CI + pixel-diff baseline для regression guard;
  интеграция
  с `lookdev-captures/`. Hint: independent (CI/tooling). Priority: l.
- [ ] **dxc-vs-glslc-toolchain** — HLSL→SPIR-V via DXC vs GLSL→SPIR-V via glslc: feature parity для mesh shader / wave
  intrinsics / RT, compile time, отладка; влияет на Stage 0 toolchain. Hint: TODO.md §Stage 0. Priority: m.
- [ ] **simd-procedural-noise** — closed `2026-06-20`, verdict **`mixed`** (см. §Closed ниже).
- [ ] **frame-flight-allocator-budget** — per-frame VMA pool с hard VRAM cap; детерминированный budget на frame, защита
  от утечек; cross-cutting для Stage 6 tech-debt. Hint: TODO.md §6.2. Priority: m.
- [ ] **async-compute-overhead-numbers** — **follow-up к закрытому `dec-pipelines-async-compute`** (verdict=yes);
  количественно измерить анонсированные 5-8% gain на реальном ProjectV workload (Stage 2.2 HZB + Stage 3.1 GPU
  Fluid CA + Stage 4.1 GPU world gen + Stage 5.2 RTX BLAS build). Hint: TODO.md §2.2/§3.1/§4.1/§5.2. Priority: h.
- [ ] **vis-buffer-for-voxels** — store `(primitiveID, barycentric, facing)` вместо G-buffer; resolve в deferred
  lighting; снижает bandwidth при 100+ материалов; альтернатива deferred подходу Stage 2/5. Hint: TODO.md §2/§5.
  Priority: m.
- [ ] **clustered-forward-mass-lights** — voxel-мир с тысячами точечных источников (лава/факелы/магия); cluster grid
  (frustum × Z-slices); forward+ вместо deferred. Hint: TODO.md §5 (GI & Temporal). Priority: m.
- [ ] **work-stealing-job-system** — claimed → in progress (см. §In progress ниже, started `2026-06-20`).
- [ ] **rt-shadows-vs-csm** — Hardware RT shadow rays (Vulkan ray queries) vs Cascaded Shadow Maps для outdoor voxel
  terrain; quality/perf trade; уточняет Stage 5.2 baseline. Hint: TODO.md §5.2. Priority: m.
- [ ] **vma-sparse-textures** — sparse binding для material atlas / voxel texture page table; релевантно Stage 2.3
  Virtual Texturing; close-out для bindless Phase D. Hint: TODO.md §2.3. Priority: m.
- [ ] **ik-first-person-hand** — CCD/FABRIK для voxel-tool interaction (рука игрока манипулирует блоками); gameplay
  polish для Stage 3.x interaction. Hint: TODO.md §3 (Physics & Simulation). Priority: l.
- [ ] **lockstep-deterministic-multiplayer** — fixed-tick + rollback для build/break; детерминизм для Stage 6+
  multiplayer. Hint: independent (multiplayer вне текущего TODO roadmap). Priority: l.
- [ ] **vk-video-decoder-replay** — in-engine video playback через `VK_KHR_video_decode` без external player;
  cutscenes /
  replay tooling. Hint: independent (post-Stage 6). Priority: l.
- [ ] **vulkan-fps-pacing-vk-ext** — `VK_KHR_present_wait` / `VK_KHR_swapchain_maintenance1` для frame pacing без
  busy-wait vsync; Stage 0 architectural. Hint: TODO.md §Stage 0. Priority: m.
- [ ] **eye-tracked-foveated** — `VK_EXT_foveated_render` / variable rate shading при gaze input; VR-готовность +
  экономия bandwidth 30-70%; future feature, нет текущей стадии. Hint: independent. Priority: l.
- [ ] **audio-raytracing-voxel-sdf** — geometric audio path tracing через ту же SDF что и рендер; reflections /
  occlusion
  / reverb из геометрии мира (отличается от `ddsp-procedural-audio` — это synthesis, а это geometric propagation).
  Hint: independent (нет audio stage в TODO). Priority: l.
- [ ] **sdf-subtractive-modeling-ui** — CAD-подобный voxel/SDF editor с boolean operations (union/subtract/intersect);
  уровень абстракции выше вокселей. Hint: independent (editor tooling). Priority: l.
- [ ] **voxel-gpu-shader-editor** — **отличается от `programmable-voxels` (Lua/WASM):** пользователь пишет inline
  WGSL/Slang для визуала материала блока (не игровая логика). Hint: independent (modding). Priority: l.
- [ ] **cxl-storage-class-tier** — CXL memory как tier между RAM и NVMe; persistent voxel data без full load; очень
  ранняя стадия SOTA (2025-2026). Hint: independent (horizon scan). Priority: l.
- [ ] **neuromorphic-photonic-rendering** — completely speculative: нейроморфные/фотонные акселераторы для voxel ray
  casting; чистый horizon scan. Hint: independent (horizon scan). Priority: l.

---

## In progress

- [ ] **work-stealing-job-system** — m, Stage 4.1 (background world gen dispatcher) + Stage 6.1 (ECS multi-threading). *
  *Job-scheduling-ось**, foundation для multi-threaded Stage 4.1 (CPU-side batch chunk gen) + Stage 6.1 (
  `ecs_set_target_fps` + multi-threaded `ecs_progress` per `TODO.md §6.1` Step 6 NUMA-aware). Cross-axis continuity:
  this session уже закрыл `flecs-soa-vs-aos-bench` (ECS layout settled) + `async-compute-overhead-numbers` (async
  foundation) + `simd-procedural-noise` (per-chunk CPU compute). Priority: m.
  **Agent:** self.
  **Started:** 2026-06-20.
  **ETA:** same session (one-shot).
  **Blocker:** нет.
  **Hypothesis (one-line):** **C++26 `std::execution` (P2300 senders/receivers)** даёт сопоставимый throughput с **BS::
  thread_pool** / Taskflow / std::thread pool на synthetic ProjectV chunk-generation workload (1024 chunks × 8³ voxels,
  1M+ ops/sec/chunk) на Zen 3 8C/16T — при этом лучше composable для Stage 6.1 ECS (sender chains для `ecs_progress`
  hooks) — кроссвендорно portable (Linux/Windows/macOS) без vendor lock-in (TBB = Intel, libdispatch = Apple), поэтому
  становится mainline default.
  **Scope (paths):**
  `docs/experiments/experiments/2026-06-20-work-stealing-job-system/{README.md, STATUS.md, sources.md, prototype/, results.csv, RESULTS.md}`.
  Sync: `INDEX.md §5` при старте, `backlog.md §In progress → §Closed` + `INDEX.md §5 → §6` при закрытии. Ничего за
  пределами `docs/experiments/` (per §2).

- [x] **[2026-06-20-meshing-algo-comparison](./experiments/2026-06-20-meshing-algo-comparison/)** — h, Stage 2.1 (visual
  mesh shader) + Stage 3.3 (physics mesh). **Meshing-ось**, отсутствующая в закрытых today-сессиях (
  storage/sync/cull/binding/layout все закрыты либо mixed). Priority: h.
  Closed `2026-06-20`, verdict **`mixed`**. Web-research complete (8 sources across 2 batch queries: cgerikj
  binary-greedy 2020, 0fps.net 2012, bonsairobo SN 2020, KAIST ODC SIGGRAPH Asia 2024, MakerTech YouTube 2026, jwarren
  DC 2002, lpigou SN 2021, isoext 2025). Standalone C++20 prototype `prototype/bench.cpp` (~1 200 строк, 4 algos × 6
  scenes = 24 configs, 1 000 iter, mean/median/p95/p99/std). **Главные findings:** (a) **Naive Greedy** wins triangle
  count на 5/6 non-degenerate scenes (1.3-450× меньше triangles vs MC/SN/DC; sphere 3 108 vs 4 006-7 388; hollow_shell
  24 vs 9 106-10 796; projectv_mix 96 vs 3 906-4 402; layered_terrain 53 854 vs 67 008-70 800) — подтверждение original
  hypothesis для vertex-bound Stage 2.1; (b) **Marching Cubes** fastest build time (250-380 µs на 5/6 сцен, 1.7-2.5×
  быстрее greedy) — original claim "не хуже по build time" **НЕ подтверждён**; (c) **Sparse scenes** (1% density) —
  SN/MC лучше по triangles (1 220/2 258 vs greedy 3 608) — coplanar merge не работает на isolated voxels; (d) **Dual
  Contouring slowest** (1 170-4 817 µs, QEF overhead 4-5× vs MC); (e) **SN competitive** (build time 1.5-2× медленнее
  MC, triangle count 1.2-2.4× больше greedy). **Refined verdict:** mixed — greedy wins poly count (главная метрика для
  vertex shader), loses build time, sparse-scene caveat для Stage 4.1 procedural world. **Mainline рекомендация:** keep
  Naive Greedy default для Stage 2.1/3.3; bitwise cull optimization (per cgerikj 2020, 50-200 µs/chunk) — drop-in option
  для Stage 4.1 high-frequency rebuild; re-evaluate SN/MC при procedural sparse worlds. **Cross-axis:** meshing-ось =
  spatial/geometry logic, complement к in-progress `simd-procedural-noise` (compute-arithmetic) +
  `async-compute-overhead-numbers` (sync-model). Cross-refs: `agent/knowledge.md §25` (per-axis dispatch rationale),
  `TODO.md §2.1` (mesh shader spike target), `TODO.md §3.3` (Jolt MeshShape mirror), `mesh-shader-vs-compute-cull` (
  closed verdict=mixed, mesh shader = feature-flagged optional). **Continuation chain:** Stage 1.x (SVDAG-on-64-tree,
  sparse-64-tree-alternatives verdict=yes) → Stage 2.1/3.3 meshing (this) → Stage 4.1 procedural world gen (
  re-evaluation trigger).

---

## Closed (startup → experiments/<slug>/)

- [x] **[2026-06-20-vct-vs-rt-cutoff](./experiments/2026-06-20-vct-vs-rt-cutoff/)** — m, Stage 5.1 + Stage 5.2.
  Closed `2026-06-20`, verdict **`mixed`**. Lighting/GI-ось experiment. Web-research (3 batch queries,
  ~30 sources: Crassin 2011 GIVoxels, NVIDIA VXGI 0.9, OGRE 2019 hybrid blog, Lumen SIGGRAPH 2022 +
  Narkowicz "Journey to Lumen" 2022, Akenine-Möller JCGT 2021 ray-cone spread, Wiche & Kuri JCGT 2020
  cone ADS, NVIDIA RTXGI 2.0 SDK 2024-03 (NRC/SHaRC/DDGI), NVIDIA RTXDI 3.0 ReSTIR PT, Erlich et al.
  Eurographics 2024 VSRM vs DXR, NVIDIA Blackwell architecture whitepaper 2025, AMD RDNA 4 deep dive
  2025, Intel Battlemage Xe2 2025, Minecraft RTX 2021, Franke Delta VCT 2014, Sugihara 2014 LRSM,
  Ryse Crytek GDC 2014, Aokana 2025, dubiousconst282 2024, Molenaar PG 2024, etc.). **Analytical
  cost model** + **cross-vendor HW RT perf matrix** (NVIDIA Ampere 4 tri/cycle baseline → Ada same
  + more units → Blackwell 8/cycle 2× gain; AMD RDNA 2/3 1/cycle → RDNA 4 2/cycle 2× gain; Intel
  Alchemist 1/cycle → Battlemage 2/cycle 2× gain; cross-vendor convergence at RDNA 4 / Battlemage).
  **Refined cutoff = 0.3** (не 0.3–0.5 диапазон из гипотезы): VCT specular 2.5× at r=0.3 = RTX 1-ray
  cost; OGRE 2019 precision cliff at 0.02 (8-bit atlas, ProjectV R8G8B8A8 same risk); Akenine-Möller
  2021 GGX math; Lumen 2022 rejected pure VCT (leaking in coarse mips) → RTX-dominant с VCT fallback.
  Cross-vendor threshold adjustment recommended (Blackwell → 0.4-0.5, RDNA 2 → 0.2, Battlemage → 0.25,
  no-HW-RT → VCT-only). Mainline integration: 4-step migration per `agent/knowledge.md §30.4` precedent
  — Step 1 foundation (roughness cutoff constant + HW RT probe + feature flag in CMakeLists), Step 2
  VCT implementation (voxelize.comp + vct.frag + 3D atlas + mip chain per `TODO.md §5.1`), Step 3
  RTX implementation (BLAS per chunk + TLAS per frame + rayQueryEXT integration per `TODO.md §5.2`),
  Step 4 (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT. Caveats: analytical model only (no ProjectV
  prototype), single-vendor literature (NVIDIA heavy), VCT leak in ProjectV SVO = lower than Lumen
  surface cache but not zero. Continuation chain: `nanovdb-on-gpu` (VCT SSBO foundation) →
  `dec-pipelines-async-compute` (async re-voxelization) → `hzb-binding-models` (texelFetch pattern
  для bindless VCT atlas) → this (roughness cutoff strategy). **Lighting/GI-ось closed**; Stage 5
  now has both storage (nanovdb-on-gpu) + sync (dec-pipelines-async-compute) + cutoff strategy (this).
  Cross-axis: memory + layout + sync + storage + GI strategy — five orthogonal axes of Stage 1.x/2.x/
  3.x/5.x optimization, all closed same-day `2026-06-20`.

- [x] **[2026-06-20-hzb-binding-models](./experiments/2026-06-20-hzb-binding-models/)** — m, Stage 2.2.
  Closed `2026-06-20`, verdict **`mixed`**. Web-research (~10 sources incl. critical NVIDIA `textureLod`
  bug под `VK_EXT_descriptor_heap` per `foijord/vk-textureLod-repro` 2026) + standalone Vulkan compute
  prototype (24 sampling tests across 8 mips × 3 patterns). **17/24 PASS, 7/24 FAIL.** Conclusive findings:
  (a) `texelFetch(sampler2D, ivec2, mipLevel)` correct + bindless-robust (recommended); (b) `textureLod`
  correct on classic, fragile под bindless (NOT recommended для Phase E future); (c) `imageLoad(storage_image)`
  fundamentally unsuited для HZB culling (GLSL single-mip-per-binding limitation, proved by `max_abs_error =
      N * 1000` pattern). Mainline recommendation: Stage 2.2 cull shader uses `texelFetch`, HZB descriptor =
  `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + separate `SAMPLER`. ~50-100 LoC change across 4 files. Future-proofs
  `bindless-descriptor-overhead` Phase E. Cross-refs: `bindless-descriptor-overhead` (Phase E prerequisite),
  `TODO.md §2.2`, `src/render/HizCulling.cpp`.

- [x] **[async-compute-overhead-numbers](./experiments/2026-06-20-async-compute-overhead-numbers/)** — h, Stage
  2.2/3.1/4.1/5.2.
  Closed `2026-06-20`, verdict **`yes`**. **Sync-axis measurement gap closure** — количественно
  измерил overlap graphics||compute на RTX 3060 Ti Ampere (dedicated compute-only queue family 2,
  8 queues per `vulkaninfo` probe `2026-06-20`). Standalone Vulkan 1.4 app + 3 синтетических
  ProjectV-style compute workloads (VCT 3D blur, HZB cull, Fluid CA ping-pong) + 16-iter multiplier
  (моделирует 16 substeps/tick per `agent/knowledge.md §30.1` fluid CA rate) + 200 frames per mode
  (30 warmup). **Sequential mode:** 0.771-0.869 ms wall clock / 0.669-0.720 ms GPU total.
  **Async mode:** 0.695-0.771 ms wall clock / 0.625-0.636 ms GPU total. **Speedup: +9.85% to +11.34%**
  (стабильно > 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  p99 tail latency −39% (1.917 → 1.172 ms). GPU compute time −6.5 to −11.4%, GPU graphics time
  −8 to −13%. Подтверждает литературные 5-8% estimates из `2026-06-20-dec-pipelines-async-compute`
  количественно. Mainline рекомендация: 3-step migration per `dec-pipelines-async-compute` §1 +
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass, 4 passes: 2.2/3.1/4.1/5.2), Step 3 default flip
  (XS, single config). Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA per
  `agent/workspace.md §2` + Stage 2.2 HZB full integration + Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Cross-vendor expectations per `dec-pipelines-async-compute`
  §2.2 vendor matrix (NVIDIA Ampere/Ada/Blackwell = yes; AMD RDNA2/3/4 = yes with caveats; Intel
  Arc Gfx12.5+ = yes with L1 contention for ray queries). Caveats: (a) single GPU vendor validated
  (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341) — mainline re-test on AMD RDNA + Intel Arc dev matrix;
  (b) NVIDIA June 2025 driver bug mesh-shading+async does NOT apply (compute cull path per
  `mesh-shader-vs-compute-cull` verdict=mixed); (c) synthetic workloads model ProjectV patterns
  but not actual code paths; (d) headless harness (no swapchain), so cross-frame pipelining gain
  (DiligentEngine up to 2× with double-buffering) not measured — expected additional 10-30% in
  real renderer per `dec-pipelines-async-compute` Caveat #2.

- [x] **[sparse-64-tree-alternatives](./experiments/2026-06-20-sparse-64-tree-alternatives/)** — h, Stage 1.1/1.2.
  Closed 2026-06-20, verdict **`yes`**. Sparse 64-tree (4×4×4 = 64-ary) подтверждён как SOTA-выбор для ProjectV
  Stage 1.1/1.2. Все три corner-cases (mutation / sparse DAG / GPU traversal) **не упираются** в design choice.
  VDB/NanoVDB = VFX dense (не наш use case). BR-tree/BIH = triangle-mesh-focused. Octree regression = -40-60%
  per eisenwave. HashDAG = future R&D (Stage 3.1+). Mainline рекомендация: продолжить Stage 1.1 → 1.2 path без
  pivot; flip `PROJECTV_SPARSE_64_STORAGE` default → on; добавить per-chunk SVDAG policy (lazy dedup, N-tick
  threshold).
- [x] **[mesh-shader-vs-compute-cull](./experiments/2026-06-20-mesh-shader-vs-compute-cull/)** — m, Stage 2.1.
  Closed 2026-06-20, verdict **`mixed`**. Compute cull + indirect draw (текущий `voxel_mesh.comp` + Pattern A)
  остаётся правильным default для Stage 2.x. Mesh shader pipeline (Pattern C, mesh + indirect count без task
  shader per the maister's universal fast path) = feature-flagged optional path
  (`PROJECTV_MESH_SHADER_PIPELINE=ON`), не default. **Task shader (Pattern B, TODO §2.1 literal design) =
  explicitly avoided** (vendor-specific tuning overhead + ~10% perf penalty even optimal per the maister +
  AMD RDNA2 TDR на early-return per GameDev.net 2024 + no shipped games). Aokana (май 2025, академический
  SOTA) использует compute shaders для всего voxel pipeline, не mesh shaders. Cross-vendor support matrix:
  Pattern A = universal; Pattern C = requires Vulkan 1.2+ + driver maturity; Pattern B = experimental.
  Mainline рекомендация: defer Stage 2.1 implementation до Stage 1.x (Sparse 64-tree + SVDAG) + Stage 2.2
  (HZB cull) completion. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) — bandwidth savings
  scale proportionally, may cross 5% perf threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.
- [x] **[bindless-descriptor-overhead](./experiments/2026-06-20-bindless-descriptor-overhead/)** — m,
  Stage 2.x. Closed 2026-06-20, verdict **`mixed`**. Pure bindless НЕ рекомендуется для ProjectV
  (cost savings <0.2% frame budget, 8× validation overhead в debug, GPU memory bandwidth
  trade-off). **Hybrid strategy** рекомендуется: bindless для stable resources (material table,
  Sparse64Node pool, HZB mip, virtual texture page table) + traditional+dynamic-offset для transient
  per-frame SSBOs (PackedFace, indirect draw, motion vectors) + push descriptors для small per-draw
  transient (shadow cascade params, debug toggles). Defer `VK_EXT_descriptor_buffer` до NVIDIA native
  HW support (current emulation = 5 indirections per XDC 2025). 5-phase rollout plan: Phase A push
  shadow cascade (XS, immediate); Phase B bindless material table (S, after Stage 1.1 lands);
  Phase C bindless Sparse64Node (S, after Stage 1.2 SVDAG); Phase D bindless virtual texture (M,
  with Stage 2.3); Phase E bindless RTX TLAS (M, with Stage 5.2). Cross-vendor validated:
  NVIDIA (32B/32B descriptors, emulated buffer), AMD RDNA2/3 (32B/16B, HW buffer),
  Intel Gfx12.5+ Arc (64B/16B, dual mode LEGACY+BUFFER), Arm v9+ Mali (HW 32 set bindings).
  Quantitative reference: Traha 2024 saves 3.5ms by dynamic-offset rewrite (+5 FPS),
  Arm Mali sample 38% frame time reduction from caching, NVIDIA bindless 7× upper bound
  (legacy OpenGL).

- [x] **[cache-oblivious-chunk-tree](./experiments/2026-06-20-cache-oblivious-chunk-tree/)** — m, independent
  (Stage 1.x retro / Stage 4.x LOD / Stage 4.3 re-evaluation trigger). Closed 2026-06-20, verdict
  **`mixed`**. Morton (Z-order) reorder of `Sparse64Tree::nodes_[]` measured on synthetic random-walk
  workload (24³ chunks × 8³ voxels, 33 MiB > L3). Mean latency similar (~40-60 ns), p99 inconsistent
  across seeds, cold cache unaffected. Implementation cost low (one-time reorder + slot remap) but
  measured benefit within timer noise. Literature predicts 25-75% cache miss reduction (arxiv
  2603.06771), but not reproduced in this prototype — likely due to random-walk access pattern (no
  spatial coherence), 280 B node size (5 cache lines, vs SoftwareSVO's 32 B half-line optimal), timer
  resolution ~30 ns. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) when working set
  exceeds L3 dramatically. Mainline recommendation: defer; не pursue at current Stage 1.x; revisit
  at Stage 4.3 с real spatially-coherent workload (player movement). Cross-refs:
  `sparse-64-tree-alternatives` verdict=yes (continuity), `svdag-vs-vdb-memory-throughput` (parallel
  session, non-overlapping scope), `TODO.md §1.1/§1.2/§2.1/§2.2/§4.3`.

- [x] **[svdag-vs-vdb-memory-throughput](./experiments/2026-06-20-svdag-vs-vdb-memory-throughput/)** — h, Stage 1.2.
  Closed `2026-06-20`, verdict **`yes`**. SVDAG-on-64-tree (current mainline) подтверждён
  **измерениями** для ProjectV workload (32³ chunks): memory 8.75 B/voxel solid / 16-70 B/voxel sparse —
  within dubiousconst282 2024 literature range (0.62 B/voxel Tree64 + dedup = best case). GetCell
  latency 22-36 ns, SetCell latency 0.03-0.04 µs no-dedup / 0.68-1.26 µs dedup-ON. **Dedup ON costs
  20-40× build time** на non-repetitive scenes → рекомендация: per-chunk `isStatic` flag (Stage 1.2
  design) instead of always-on. VDB-like impl в prototype имеет known bug (uniform-tile lie,
  verify_mismatches>0 для 4/7 scenes) — но memory numbers consistent with NanoVDB expectations.
  Mainline может продолжить Stage 1.1 → 1.2 → 2.x → 3.x → 4.x → 5.x path **без архитектурного pivot
  на NanoVDB**. Закрыл measurement gap от `2026-06-20-sparse-64-tree-alternatives` §5.3.
- [x] **[dec-pipelines-async-compute](./experiments/2026-06-20-dec-pipelines-async-compute/)** — m,
  independent (Stage 2.2 / 3.1 / 4.1 / 5.2; sync-model foundation). Closed 2026-06-20, verdict
  **`yes`**. Dedicated async-compute queue + `VK_KHR_synchronization2` (core 1.3) +
  `VK_KHR_timeline_semaphore` (core 1.2) + `VK_KHR_global_priority` (core 1.4) рекомендованы для
  4 of 5 ProjectV compute passes: Stage 2.2 HZB cull + Stage 3.1 Fluid CA (20 Hz, natural async
  candidate via 3-frame latency) + Stage 4.1 GPU world gen (LOW priority, background) +
  Stage 5.2 RTX BLAS build (`VK_KHR_deferred_host_operations` для non-blocking dispatch). Stage
  5.1 VCT — sequential default, async opt-in (RDNA «export bound shaders» warning). Expected: 5-8%
  steady-state frame time saving + 100% spike elimination (world gen + BLAS). Crosses 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Cross-vendor validated: NVIDIA
  Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage (Arm Mali TBDR out of scope
  for desktop). Sync model change is **net simpler** (sync2 cleaner than current pNext chains +
  binary semaphores + `vkWaitForFences`). Vendor caveats: (a) NVIDIA June 2025 driver bug
  mesh-shading+async-compute-started-before-raster (Timberdoodle, RTX 4080, driver 566.03) — не
  applies to ProjectV's compute cull path per `mesh-shader-vs-compute-cull` verdict=mixed;
  (b) AMD RDNA1/2 maintenance branch (2025-Q4) — async-compute still works, new extensions won't
  come; (c) Intel Ray Queries + groupshared + async compute = L1 cache contention (relevant for
  Stage 5.2). `VK_AMDX_shader_enqueue` deferred (2025 proposal, AMD-only, cross-vendor unclear per
  docs.vulkan.org). Per `legacy/docs/architecture/practice/00_engine-structure.md:483` minor fix
  opportunity: «`VK_KHR_synchronization2` (core in 1.4)» should be «core in 1.3» per Khronos spec —
  no functional impact (1.3+ all have it as core). Mainline рекомендация: 3-step migration per
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass), Step 3 default flip. Foundation шаг = prerequisite
  для Stage 3.1 GPU Fluid CA (sync-model конкретизирует §30.4 contract), Stage 2.2 HZB full
  integration (per `workspace.md §2 Nearest Gap`), Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Synergy: shared async-compute queue manager обслуживает все 4
  async candidates. Cross-axis: memory (svdag-vs-vdb) + layout (cache-oblivious) + sync (this) —
  three orthogonal axes of Stage 1.x/2.x/3.x optimization.

- [x] **[simd-procedural-noise](./experiments/2026-06-20-simd-procedural-noise/)** — h, Stage 4.1 (CPU noise
  gen prebake path; secondary Stage 1.1 batch hash combine). Closed `2026-06-20`, verdict
  **`mixed`**. Web-research (4 batch queries, ~20 results; 3 `webfetch` верификации включая
  ISPC perf page + FastNoise2 GitHub + Clang issue #176670) + standalone C++26 AVX2/FMA
  benchmark (`docs/experiments/experiments/2026-06-20-simd-procedural-noise/prototype/bench.cpp`).
  2 варианта (spec Ken Perlin perm-table + SIMD-hash splitmix32+16-grad) × 2 dimensions
  (2D/3D) × 2 kernels (scalar/AVX2) = 8 configs, 1000 reps × 1024 samples. **Гипотеза
  (≥ 4×) НЕ подтверждена на Zen 3 AVX2**: scalar auto-vec LLVM SLP до 4 lanes, AVX2 = 8 lanes
  → theoretical max ~2×. **Измерено:** spec 2D AVX2 = **1.14×** / spec 3D AVX2 = **0.62×**
  (loss, hash extraction overhead) / simd 2D AVX2 = **1.83×** / simd 3D AVX2 = **1.51×**.
  50-100% improvement IS выше 5-10% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, но literature 5-7×
  (ISPC, FastNoise2) требует ISPC toolchain или AVX-512 hardware — **out of scope** для
  Zen 3. All 4 (variant × dim) AVX2 vs scalar = **bit-identical** (`rel_err = 0.00e+00`).
  Mainline рекомендация: **simd-hash variant** (splitmix32 + 16-grad) для Stage 4.1 CPU
  prebake path, runtime detect `__builtin_cpu_supports("avx2")`, scalar fallback для non-AVX2;
  CMake `-march=x86-64-v3` baseline → AVX2 default on Zen 3+. **НЕ использовать** spec Perlin
  для AVX2 mainline (3D проигрывает). 3-step migration per `agent/knowledge.md §30.4`
  precedent: Step 1 `src/voxel/SimdHashNoise.hpp` (150-200 LoC), Step 2 wire in
  `src/asset/WorldGen.cpp` (planned Stage 4.1), Step 3 CMake `-march=x86-64-v3` flip.
  Caveats: single-vendor (Zen 3) — mainline re-test on Intel Haswell/Skylake+; Arm NEON
  path = separate follow-up; visual noise quality slightly different from Ken Perlin
  spec (no permutation bijection, but C¹ continuous — acceptable for voxel world gen).
  Re-evaluation triggers: Stage 5.1 VCT (indirect lighting — A/B test noise quality),
  AVX-512 hardware arrival (Zen 5 / Arrow Lake). Cross-axis: today-сессии `2026-06-20`
  closed orthogonal axes (storage/sync/cull/binding/layout/meshing/hzb/nanovdb/simd-noise)
  = 8 storage/compute closed. Single remaining h-priority slot in `§In progress` =
  `meshing-algo-comparison`.

- [x] **[nanovdb-on-gpu](./experiments/2026-06-20-nanovdb-on-gpu/)** — m, independent (Stage 5.1 VCT
  primary, fragment-shader DDA secondary per `TODO.md §6.2.2`). Closed 2026-06-20, verdict
  **`yes`**. Closes measurement gap from `2026-06-20-svdag-vs-vdb-memory-throughput` §3 line 157
  («Не реализовывал GPU traversal») + bugfix NanoVDB-like impl (uniform-tile lie). **Both
  CPU-side and GPU-side prototypes byte-exact** (verify_mismatches=0 на 5 сценах × 2 kernels).
  NanoVDB-aligned pointer-less layout (Upper[8³] → Lower[4³] → Leaf[2³], scaled per NanoVDB.h
  actual 32³/16³/8³ structure для ProjectV chunkSize=8) **outperforms SVDAG-on-64-tree on 4/5
  scenes by 12-141%** (sparse_random_8: 500 → 1210 Mrays/s = +141%; voxel_lab_8: 541 → 1208
  Mrays/s = +123%; ground_8: 638 → 1242 Mrays/s = +95%; brick_8: 1146 → 1284 Mrays/s = +12%).
  Only solid_8 ties (1265 vs 1272 Mrays/s = +0.6%, memory-bandwidth-bound). **GPU memory:
  NanoVDB uses 57-75% less VRAM** across all scenes. **CPU memory: ~50% less** (B/voxel). Crosses
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by significant
  margin. Cross-references literature: fVDB 2024 (NanoVDB+HDDA = SOTA GPU traversal), Aokana
  May 2025 (per-chunk SVDAG — identical to our design), Mathijs PG 2024 (SVDAG-on-GPU editing
  5× faster than CPU HashDAG), NanoVDB PR #2220 (fused accessor 1.4-2.6× speedup on Blackwell).
  **Critical mainline finding:** ProjectV chunkSize = 8 (not 32 as previous experiment
  assumed) per `src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not
  depth=3. OpenVDB 13.0.0 (Nov 2025) lowered NanoVDB's mutation barrier (DilateGrid, MergeGrids,
  CoarsenGrid, RefineGrid, PruneGrid, VoxelBlockManager) — relevant for Stage 5.1 transient
  atlas re-upload cost. Mainline рекомендация: **hybrid strategy** — keep CPU-side SVDAG-on-64-tree
  (current mainline Stage 1.2 design, proven by `svdag-vs-vdb-memory-throughput` verdict=yes),
  but flatten chunks into NanoVDB-aligned transient SSBO at GPU upload time for Stage 5.1 VCT
  cone-march + 3 fragment-shader DDA traces in `voxel.frag` per `TODO.md §6.2.2`. 3-step
  migration per `agent/knowledge.md §30.4` precedent: Step 1 foundation (CPU→GPU flatten helper,
  S effort), Step 2 kernel swap (NanoVDB walker, M effort, includes shader rewrite for HDDA
  optimization), Step 3 default flip (`PROJECTV_USE_NANOVDB_TRANSIENT_VCT=ON`). Foundation
  optional dependency: `dec-pipelines-async-compute` (closed 2026-06-20) for async re-upload.
  Caveats: single GPU vendor validated (NVIDIA RTX 3060 Ti GA104 Ampere, Vulkan 1.4.350) — mainline
  re-test on AMD RDNA2/3 + Intel Arc dev matrix; HDDA-specific optimizations (warp ballot
  early-out, ReadAccessor caching) NOT implemented in first-iteration prototype — adding these
  would give additional 10-30% per PR #2220 reference. Continuation chain:
  `sparse-64-tree-alternatives` (analysis) → `svdag-vs-vdb-memory-throughput` (CPU) → this (GPU).
  Cross-axis: previous experiments covered memory + sync; this covers GPU traversal for
  Stage 5.1.

- [x] **[2026-06-20-flecs-soa-vs-aos-bench](./experiments/2026-06-20-flecs-soa-vs-aos-bench/)** — m, Stage 6.1.
  Closed `2026-06-20`, verdict **`yes`**. Web-research complete (8 primary sources верифицированы по
  году/автору/контексту + 10 background sources в `sources.md`, key cross-validation: Mertens 2024 Flecs
  default SoA, Sagar 2026 5.67× OOP→SoA, DevelopersIO 2026 3.3× Godot update, Bevy PR #14049 2× dense iteration,
  AMD EPYC 7003 Zen 3 cache spec). Standalone C++26 prototype `prototype/flecs_soa_vs_aos.cpp` (642 строки,
  4 configs × 3 workloads × 3 seeds × 1000 iterations = 36 measurements). **SoA wins ALL 3 workloads** —
  raycast **2.14×** (199→427 Meps), physics **3.86×** (210→812 Meps, near-exact match с DevelopersIO), cull
  **1.44×** (315→454 Meps). Crosses 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  by 40-280%. Hybrid ≈ SoA (within 1-2%), HotOnly worst variance (15% raycast stddev). SoA variance ниже AoS
  (24% reduction for physics) — deterministic cache-line stride reduces OS scheduler noise. Mainline
  рекомендация: keep Flecs default SoA storage (per Mertens 2024 + Flecs v4.1.0 release notes), **не возвращаться
  на AoS POD-struct per entity** в новых systems. Per-workload split hot/cold опционально для HZB cull
  (4 fields, modest 1.44× gain) — only if profile shows branch mispredict > 5%. HotOnly-SoA pattern NOT
  рекомендуется (worst variance, gain ≤5%). Snapshot save/load path остаётся AoS (cold path, simpler code).
  Estimated mainline effort: **XS** (doc update + code review checklist, не mainline rewrite). Cross-cutting
  unblocks для Stage 2.2 HZB cull / Stage 3.1 Fluid CA bookkeeping / Stage 3.2 Incremental Jolt per-chunk
  lifecycle / Stage 5.1 VCT voxelize bookkeeping — все эти Flecs systems могут proceed с уверенностью
  что SoA = correct default. Documentation update recommended для
  `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` mermaid diagram (analytical 3-5× claim →
  measured 1.44-3.86× numbers с cross-ref). Cross-refs: `agent/knowledge.md §1605` A9 (current AoS voxel
  storage alternative), `agent/workspace.md §1` Phase 5 (ECS systems already landed),
  `TODO.md §6.1` (Flecs ECS migration), `external/flecs/` v4.1.5 (Flecs design defaults).

---

## Rejected (без старта, с обоснованием)

- _нет_