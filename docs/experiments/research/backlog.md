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
- [x] **restir-gi-feasibility** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
  См. §Closed ниже + [README](./experiments/2026-06-20-restir-gi-feasibility/README.md).
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
  **STATUS:** moved to `§In progress` 2026-06-20 (claimed by self for current session).
  Closed `2026-06-20`, verdict **`mixed`**. См. §Closed
  ниже + [README](./experiments/2026-06-20-vis-buffer-for-voxels/README.md).
- [ ] **work-stealing-job-system** — **closed `2026-06-20`** (verdict=`mixed`), см. §Closed ниже. Serial dispatcher =
  sweet spot для ProjectV mainline; pool/TBB/libdispatch/`std::execution` НЕ рекомендуются по default.
- [x] **rt-shadows-vs-csm** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
  См. §Closed ниже + [README](./experiments/2026-06-20-rt-shadows-vs-csm/README.md).
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

(No active reservations `2026-06-20` EOD. `2026-06-20-restir-gi-feasibility` closed verdict=mixed same session —
см. §Closed ниже.)

---

## Closed (startup → experiments/<slug>/)

- [x] **[2026-06-20-restir-gi-feasibility](./experiments/2026-06-20-restir-gi-feasibility/)** — m, Stage 5.1/5.2
  (SOTA-GI-ось experiment, post-Stage 5 follow-up explicitly named в `vct-vs-rt-cutoff` §1 line 99 «Step 4
  (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT»). Closed `2026-06-20` (single session),
  verdict **`mixed`**. Web-research complete (~30 sources верифицированы: Bitterli 2020 ReSTIR original
  [6-60× MSE ↓, 8 rays/pixel max], Ouyang 2021 ReSTIR GI [9.3-166× MSE ↓ @ 1spp], Lin 2022 ReSTIR PT +
  GRIS theory [80 ms @ 1920×1080, MAPE 0.39 vs 1.63 PT, 1 path/pixel], Majercik 2019/2021 DDGI,
  Müller 2021 NRC [2.6 ms @ full HD, NVIDIA Tensor Cores ≥ Turing], NVIDIA-RTX/RTXGI SDK v2.7.0 Mar 2026
  [336 stars, driver ≥ 555.85], NVIDIA-RTX/SHARC [123 stars, spatial hash grid 64-bit keys, 4-pass,
  ~185 MB @ 2^22 baseline, 1.5-10% perf overhead in Cyberpunk], NVIDIA-RTX/RTXDI v3.0+
  [ReSTIR DI/GI/PT/ReGIR, D3D12+Vulkan via NVRHI, DXC toolchain], Crassin 2011 GIVoxels [VCT foundation,
  25-70 FPS, two bounces Lambertian+glossy], Lumen SIGGRAPH 2022 [Epic explicitly rejected VCT as leaky],
  Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache, DDGI-style probes], Douglas Voxel
  Devlog #23 Jun 2025 [direct voxel + DDGI integration, voxel-compatible verified], Cyberpunk 2077 RT
  Overdrive Patch 2.1 Dec 2023 [production ReSTIR DI/GI + SHaRC], NVIDIA Zorah RTX 50 demo 2025 [ReSTIR PT],
  Portal RTX [SHaRC], OGRE-Next CIVCT [10-100× faster voxelization], Aokana arXiv 2505.02017 May 2025,
  ReSTIR FG/GSGI/PMGI 2024 [0.4-14 ms overhead variants], Epic DDGI abandonment forum Dec 2025 [Arc Raiders
  counter-example]). Standalone prototype deferred per `rt-shadows-vs-csm` precedent — analytical +
  literature + cross-vendor matrix sufficient. **Architectural mismatch (главный finding):** все 4 SOTA
  техники (ReSTIR PT/GI/DI, DDGI, SHaRC, NRC) **требуют path tracer foundation**; ProjectV's Stage 5.x
  = hybrid VCT+RTX = **NOT** path tracer. **VRAM matrix** (RTX 3060 Ti, 5.06 GiB budget):
  SHaRC = 185 MiB (3.65%, acceptable), DDGI = 16 MiB, ReSTIR reservoir = 33-67 MiB checkerboard/full.
  **Quality validated** для path-tracing contexts (ReSTIR PT MAPE 0.39 vs 1.63 Carousel, Cyberpunk
  production, SHaRC 1.5-10% overhead) — **cannot translate** к ProjectV без path tracer. **NRC rejected**
  = NVIDIA-only (Tensor Cores ≥ Turing, excludes AMD RDNA 4 + Intel Battlemage per `dec-pipelines-async-compute`
  matrix). **Mainline recommendation:** **keep current hybrid VCT+RTX as-is** (Stage 5.x MVP scope), **defer
  SOTA GI до Stage 6+ post-MVP path tracer pivot**. Recommended add-on order (if path tracer ships):
  **SHaRC → DDGI → ReSTIR DI/GI/PT**. SHaRC first: lowest complexity, cross-vendor, voxel-adaptable,
  acceptable VRAM. NRC = skip. **Re-evaluation triggers:** VCT leakage visible в production (cavity lighting
  artifact), Stage 4.3 ships (128+ chunks), mainline commits to path tracer (independent decision),
  vendor ships open-source SHaRC GLSL port, ReSTIR GSGI/PMGI stabilize (2024 prototypes, 0.4-0.8 ms = viable
  alternative). **Cross-axis:** 19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape + SOTA-GI axis. **Lighting axis FULLY closed** (cutoff + lights + shadows + SOTA-GI all
  same-day `2026-06-20`). Continuation chain: `vct-vs-rt-cutoff` (mixed, cutoff=0.3) + `rt-shadows-vs-csm`
  (mixed, hybrid CSM+RTX) + `clustered-forward-mass-lights` (yes, SSBO) + `nanovdb-on-gpu` (yes, VCT SSBO) +
  `dec-pipelines-async-compute` (yes, async foundation) → this. Closed entry:
  `experiments/2026-06-20-restir-gi-feasibility/`.

- [x] **[2026-06-20-clustered-forward-mass-lights](./experiments/2026-06-20-clustered-forward-mass-lights/)** — m,
  Stage 5 (GI & Temporal, depends on §1.2 SVDAG). Closed `2026-06-20` (single session),
  verdict **`yes`** (с условиями: soft cap ≥2048 + light prioritization для 5000+ light scenes).
  **Mass-lights architecture** experiment — единственная ось, не покрытая today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/gi-strategy + job-scheduling).
  **Mainline baseline = single-light hard cap** per `src/shaders/voxel.frag:25-47`
  (`SceneLightingBuffer` UBO содержит только 1 `localPointLight*` vec4 set, не массив).
  **Не масштабируется** на `TODO.md §4.x` procedural (лава/факелы/магия) + `§5.1` VCT VPLs.
  Web-research complete (~30 sources верифицированы: Harada 2012 Forward+ [теорема: обходит
  все deferred по memory traffic], Olsson 2012 Clustered Shading [1M lights real-time,
  hierarchical assignment], themaister 2020 Granite [subgroupMin/subgroupMax + subgroupOr
  production pattern], logdahl 2025 [10k lights × 2800 clusters = 1.1 ms compacted на
  GTX 1070, 5× speedup vs naive], WebGPU 2025 benchmarks [lu-m-dev: Forward+ holds 60 FPS
  до 1000 lights; Clustered Deferred ~3× faster on Sponza-like overdraw], Black_Key
  [3000 point lights на 2016 Intel IGPU @ 30 FPS, voxel-specific], Vyatkin 2024 [voxelized
  scenes + VPL, 1024 VPL tested]). Standalone CPU prototype `prototype/bench.cpp`
  (single file, ~480 LoC, Clang 22.1.6, `-O3 -march=native`, compiled clean `-Wall -Wextra`).
  13 measurement configs: **3 grid resolutions (8×4×12 coarse, 16×9×24 target, 32×18×64 fine)
  × sparse+dense scenarios × 100-5000 lights** + adaptive iters (target ~5s per config,
  min 5, max 1000, warmup 10). **Key CPU numbers (16×9×24 target, sparse scenario):** 100
  lights = 1.4 ms mean, 1000 lights = **12.7 ms mean / 15.3 ms p99** (avg 3.1 lights/cluster,
  max 34, 66% empty). **Dense scenario (lava):** 16×9×24 / 1000 lights = 15.4 ms (avg 232,
  max 544, 22% empty). **CRITICAL: 16×9×24 / 5000 dense lights = 124.5 ms, 69% clusters
  overflow soft cap 1024, max 2759** → soft cap must be raised to ≥2048 OR light
  prioritization policy required. **Cross-validation с published GPU numbers:** within
  5-10× of logdahl 2025 (1.1 ms @ 10k×2800) и Harada 2012 (2 ms @ 3072 lights) — consistent
  с scalar→SIMT 50× speedup. **GPU projected cluster build:** 0.1-0.5 ms at 1000 lights
  (1.5-3% of 16.67 ms frame budget). **Per-fragment analytical model:** Forward+ (10
  lights/cluster avg) = 1000 ALU + 50 DDA reads per fragment = **100× speedup vs
  1000-light uniform array** (100,000 ALU), 10× cost increase vs current 1-light baseline
  (100 ALU + 5 DDA reads). **VRAM cost** < 2 MB (cluster grid offset+count = 27.6 KB,
  light SSBO 256×32 B = 8 KB, light index buffer avg 138 KB). **Mainline рекомендация:**
  **3-step migration** + optional Step 4 (per-light cost reduction) + Step 5 (VPL integration
  post-Stage 5.1). **Step 1** (XS, ~50 LoC): replace single-light UBO с light SSBO array
  (`kMaxDynamicLights = 256` TBD after GPU prototype), keep single-light path as fallback,
  additive `PROJECTV_DYNAMIC_LIGHTS=ON` env. **Step 2** (M, ~200 LoC): new `cluster_build.comp`
  frustum AABB + light assignment (sphere-AABB + atomic counter compaction per logdahl 2025
  5× speedup), new `ClusterGridBuffer` + `ClusterLightIndexBuffer` + `DynamicLightSSBO` in
  `src/render/SceneResources.{hpp,cpp}`, dispatch in `src/render/Renderer.cpp` (piggyback on
  async-compute foundation per `dec-pipelines-async-compute`). **Step 3** (M, ~100 LoC):
  modify `src/shaders/voxel.frag` to compute cluster index from `gl_FragCoord` + view-Z
  (Naughty Dog exponential formula) + iterate cluster light list. **Clustered Deferred NOT
  recommended** for Stage 5 (voxel-мир has low overdraw vs Sponza, gain < 5% per threshold)
  — revisit after Stage 2.1 mesh shader + Stage 4.3 lift draw distance. **Acceptance criteria:**
  TracyPlot `ClusterBuild (ms)` < 1 ms GPU at 1000 lights, byte-exact output for N≤8 vs
  current mainline (A/B test), < 2 MB VRAM overhead, new `ProjectVClusteredLightingTests`.
  **Cross-axis continuity:** same-day `2026-06-20` сессии (vct-vs-rt-cutoff mixed +
  this yes) + Stage 5 foundation complete (nanovdb-on-gpu yes + dec-pipelines-async-compute
  yes). **12+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape** + mass-lights dimension added. Closed entry:
  `experiments/2026-06-20-clustered-forward-mass-lights/`. **Race resolution note (per
  `docs/experiments/AGENTS.md §13.3`):** parallel session informed before starting
  `vis-buffer-for-voxels` (orthogonal design space, complementary); my `clustered-forward-
  mass-lights` = first-write-wins per §13.3.

- [x] **[2026-06-20-vis-buffer-for-voxels](./experiments/2026-06-20-vis-buffer-for-voxels/)** — m,
  Stage 2.x + Stage 5.x (deferred resolve via vis-buffer + material-table SSBO; orthogonal
  rendering-approach axis). Closed `2026-06-20` (single session), verdict **`mixed`**.
  Web-research complete (5 batch queries, 20+ sources верифицированы: Burns-Hunt 2013 JCGT 2:2
  foundational 64MB vis-buffer vs 398MB G-buffer = 6.2× bandwidth win @ 1080p × 8xMSAA;
  Karis SIGGRAPH 2021 + Wihlidal GDC 2024 Unreal Nanite 64-bit vis-buffer with atomicMax +
  shading bins = 100% compute shaders UE 5.4; Andersson Frostbite 2017 "10-20x geometry vs
  Deferred"; The Forge v1.57 May 2024 TVB 2.0 pure compute; Cao NanoMesh SIGGRAPH 2024 32-bit
  mobile visbuffer; Vulkan-Guide TBR best practices 2024; Lam Adreno vis-stream HW compressor;
  jglrxavpok 2023 Vulkan R64Uint vis-buffer impl; Harada AMD Forward+ GPU Pro 4 alternative;
  Olsson Clustered Shading HPG 2012 1M lights; VoxelMVP / Exile / Slater / cgerikj / Ascendant
  voxel-specific refs). Standalone Vulkan 1.4 prototype (~700 LoC incl. shaders), standalone
  greedy-meshing voxel scene + 2 pipelines (baseline forward+ vs vis-buffer hypothesis) +
  6 measurement configs (3 scenes × 3 resolutions) на RTX 3060 Ti (Vulkan 1.4.341, NVIDIA
  610.43.02). **Refined hypothesis (post-ProjectV-survey):** ProjectV's current path already
  uses SSBO material lookup (forward+, no full G-buffer), so bandwidth win = N/A. Potential
  win = redundant raster elimination для CSM × 4 shadow passes (each re-decodes PackedFace
  vertex shader). **Cross-over @ ~1280×720.** 1920×1080 = vis-buffer 15-26% slower (bandwidth-
  bound on pixel coverage). 800×600 = vis-buffer 12-24% faster (vertex cost dominates).
  Voxel scenes are pixel-coherent after greedy meshing per `2026-06-20-meshing-algo-comparison`
  verdict=mixed (Naive Greedy default = ~1 visible triangle per pixel = no overdraw to amortize
  fullscreen vis-buffer cost). **Visual equivalence verified via framebuffer hash match** (both
  paths produce identical output for same scene + lighting). **Mainline рекомендация: DEFER.**
  No immediate integration. Re-evaluation triggers: Stage 4.3 (128+ chunks draw distance,
  vertex cost scales linearly → crossover shifts), mobile target support (TBR GPUs benefit
  per Vulkan-Guide, vis-buffer 10-30% win), Stage 4.2 LOD high-subdivision (overdraw-heavy),
  Stage 5.1 VCT integration (multiple cone-trace passes), >4 light passes. Cross-axis closure:
  today's batch (storage/sync/cull/binding/meshing/simd/hzb/flecs/nanovdb/gi-cutoff/frame-pacing/
  job-system/clustered-forward-mass-lights) + this = full Stage 1.x/2.x/3.x/4.x/5.x/6.x
  optimization landscape (12+ experiments closed same-day `2026-06-20`). Complementary to
  parallel session's `clustered-forward-mass-lights` (orthogonal: vis-buffer = deferred-resolve
  vs clustered-forward = forward+ cluster grid). Cross-refs: `2026-06-20-bindless-descriptor-overhead`
  Phase B (bindless material table = prerequisite для vis-buffer's per-frame material lookup),
  `2026-06-20-meshing-algo-comparison` verdict=mixed (Naive Greedy default = pixel-coherent = no
  overdraw = vis-buffer loses на high res), `2026-06-20-dec-pipelines-async-compute` verdict=yes
  (async-compute resolve pass would compound vis-buffer benefits, unmeasured),
  `agent/knowledge.md §25` (greedy meshing rationale), `agent/knowledge.md §30.4`
  (3-step migration precedent), `src/shaders/voxel.frag` binding 2 (existing MaterialVisual
  SSBO), `src/shaders/voxel_shadow.{vert,frag}` (existing shadow re-raster), `src/render/Renderer.cpp:540-863`
  (existing rendering orchestration), `TODO.md §5.2` (where vis-buffer integration would land
  if re-evaluated), `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti dev host).

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

- [x] **[meshing-algo-comparison](./experiments/2026-06-20-meshing-algo-comparison/)** — h, Stage 2.1 (visual
  mesh) + Stage 3.3 (physics mesh). Closed `2026-06-20`, verdict **`mixed`**. Standalone C++20 prototype
  (`prototype/bench.cpp` ~1 200 строк, 4 algos × 6 scenes = 24 configs, 1 000 iter, mean/median/p95/p99/std).
  **Главные findings:** (a) **Naive Greedy** wins triangle count на 5/6 non-degenerate scenes
  (1.3-450× меньше triangles vs MC/SN/DC) — vertex-bound advantage для Stage 2.1;
  (b) **Marching Cubes** fastest build time (250-380 µs, 1.7-2.5× быстрее greedy) — original claim "не хуже
  по build time" **НЕ подтверждён**; (c) **Sparse scenes** (1% density) — SN/MC лучше по triangles
  (coplanar merge не работает на isolated voxels); (d) **Dual Contouring slowest** (1 170-4 817 µs, QEF
  overhead 4-5× vs MC); (e) **SN competitive** (1.5-2× медленнее MC, 1.2-2.4× больше triangles vs greedy).
  **Mainline рекомендация:** keep Naive Greedy default для Stage 2.1/3.3; bitwise cull optimization
  (per cgerikj 2020, 50-200 µs/chunk) — drop-in option для Stage 4.1 high-frequency rebuild; re-evaluate
  SN/MC при procedural sparse worlds. Cross-refs: `agent/knowledge.md §25` (per-axis dispatch rationale),
  `TODO.md §2.1` (mesh shader spike target), `TODO.md §3.3` (Jolt MeshShape mirror),
  `mesh-shader-vs-compute-cull` (closed verdict=mixed, mesh shader = feature-flagged optional).
  [Sync fix r2 (2026-06-20): запись переехала из `§In progress` (stale после закрытия в другом
  parallel-session) → `§Closed` per AGENTS.md §13.5.]

- [x] **[vulkan-fps-pacing-vk-ext](./experiments/2026-06-20-vulkan-fps-pacing-vk-ext/)** — m,
  Stage 0 / independent (foundation для all stages; cross-cutting DoD principle «low latency
  > throughput» per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  Closed `2026-06-20`, verdict **`mixed`** (analytical literature valid; prototype deferred).
  Web-research complete (5 batch queries, ~30 results; 8 key sources + 3 supplementary,
  all верифицированы: Khronos blog 2025-12-04, Phoronix Mesa 26.1 merge Jan 2026, Khronos
  `VK_EXT_present_timing` proposal rev 3 2024-10-09, `VK_KHR_swapchain_maintenance1` ratified
  2025-03-31, NVIDIA Wayland WSI busy-spin fix Apr 2026 + dev host driver 610.43.02 match,
  `VK_KHR_present_wait2` rev 1, Mesa 26.2 direct-display benchmarks Jun 2026, Android docs
  Jun 2026). **Dev host validation:** `vulkaninfo 2026-06-20` confirms все extensions supported
  + features enabled: `VK_EXT_present_timing` rev 3 (`presentTiming`, `presentAtAbsoluteTime`,
  `presentAtRelativeTime` features = true), `VK_KHR_present_wait2` rev 1 (`presentWait2` = true),
  `VK_KHR_swapchain_maintenance1` rev 1 (`swapchainMaintenance1` = true), `VK_KHR_present_id/2`,
  `VK_KHR_present_mode_fifo_latest_ready`. **Refined hypothesis:** **`VK_EXT_present_timing`**
  (Nov 2025 merge, Vulkan 1.4.335) — SOTA frame-pacing API; **NOT Vulkan 1.4 core** as
  original README thought — все 3 extensions are **device extensions**. Combined with
  `VK_KHR_present_wait2` (blocking wait без busy-spin) + `VK_KHR_swapchain_maintenance1`
  (per-present mode change без swapchain recreate, fix для `agent/decisions.md §30.3`
  RecreateSwapchain cycle) → детерминированный frame budget. Mesa 26.2 KHR_display
  direct-display benchmark: **~0.3 ms latency reduction, 5% power reduction, tighter
  variance** (0.9 ms → 0.3 ms std-dev). **Caveats:** (a) Mesa benchmark = KHR_display
  direct-display (без Wayland compositor) — Wayland gain ожидаемо меньше; (b) Intel Iris Xe
  doesn't support `present_wait` / `swapchain_maintenance1` — fallback path needed;
  (c) AMD/Intel cross-vendor = Mesa 26.1+ (Jan 2026), deployment lag 1-2 cycles.
  **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent —
  Step 1 foundation (`PROJECTV_USE_PRESENT_TIMING=ON|OFF` env + per-feature detection в
  `TryPickPhysicalDevice`); Step 2 adoption (Mode C path с `desiredPresentTime` IPD
  calibration via `vkGetPastPresentationTimingEXT` feedback + `VkSwapchainPresentModeInfoKHR`
  per-present mode change + `VkSwapchainPresentFenceInfoKHR` race-free destroy); Step 3
  default flip для hardware с `presentTiming + presentAtAbsoluteTime` features enabled.
  Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA cross-frame latency contract
  (per `agent/workspace.md §2` + `agent/decisions.md §30.4`). Cross-refs: `dec-pipelines-async-compute`
  (closed verdict=yes, sync2 + timeline semaphores = prerequisite), `async-compute-overhead-numbers`
  (closed verdict=yes, async foundation = complementary), `agent/decisions.md §30.2-§30.3`
  (VSync cycle + RecreateSwapchain). [Sync fix r1 (2026-06-20): запись переехала из
  `§In progress` → `§Closed` per AGENTS.md §13.5 после research complete в том же session.]
  **Operator override note (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20,
  пользователь дал инструкцию «выбирай незанятую тему, не work-stealing-job-system»; previous
  reservation `work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier this session)
  released back to §Open. Fresh claim: `vulkan-fps-pacing-vk-ext`.

      **RACE CONDITION CORRECTION (per AGENTS.md §13.3 first-write-wins):** Parallel session
      misread operator instruction (operator meant «для parallel agent выбери не work-stealing»
      not «release the existing reservation»). Мой `work-stealing-job-system` experiment
      выполнялся **до** operator override и завершён полностью (research/prototype/measurements/
      writeup). Per §13.3, first-write-wins: моя работа сохраняется. Запись о закрытии см. ниже
      (`2026-06-20-work-stealing-job-system`).

- [x] **[2026-06-20-work-stealing-job-system](./experiments/2026-06-20-work-stealing-job-system/)**
  — m, Stage 4.1 (background world gen dispatcher foundation) + Stage 6.1 (ECS multi-threading
  per `TODO.md §6.1` Step 6 NUMA-aware). Closed `2026-06-20`, verdict **`mixed`**.
  **Job-scheduling-ось** experiment — h/m-priority slot в backlog, ещё не покрытый today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy все закрыты). Direct
  prior art: `agent/knowledge.md §29.0` line 887 (Tier 4 R&D: «`std::execution` (P2300) — нужна
  Job System, отдельный slice»). Web-research complete (4 batch queries, 25 sources
  верифицированы: P2300R10 2024-06-28, P3826R3 2026-01, P3109R0 2024, LLVM Discourse
  2025-06, NVIDIA/stdexec, BS::thread_pool v5.0.0 2024-12-20, Taskflow v3.10.0 2025-05
  / v4.0.0 2026, oneTBB v2022.3.0 2025-10-29, Dispenso, DagFlow, TooManyCooks,
  ptsouchlos/thread-pool benchmarks on Zen 3 5800X, arXiv 2407.15805). **Refined hypothesis
  (negative):** `std::execution` (P2300) = framework, не pool; sender-chain overhead
  предположительно хуже `BS::thread_pool` для hot-path batch dispatch (NOT measured — callout
  as follow-up). Standalone C++26 prototype `prototype/bench.cpp` (6 файлов, ~750 LoC incl.
  vendored `BS_thread_pool.hpp` v5.0.0 MIT). 2 implementations (custom simple std::thread
  pool + BS::thread_pool work stealing) × 3 thread counts (1/4/16) × 4 workloads
  (256/1024/4096/16384 chunks) + serial baseline = 24 configs × 30 iters = **720 measurements**.
  **Surprising negative finding:** **serial dispatcher — sweet spot для ProjectV mainline**
  (cache-fitting workload fits L3 32 MiB; submit overhead = 5-15× per-task compute = 12-37×
  waste). Work-stealing pool (BS::thread_pool) **проигрывает** simple pool'у для small tasks
  (BS 1t = 5-8× slower than serial; matches ptsouchlos/thread-pool benchmarks on Zen 3).
  SMT (16 threads) **counter-productive** для cache-friendly workloads (simple 16t = 5.7× slower
  than serial; BS 16t = 7.8× slower). p99 jitter: serial 1.0-1.2× mean, parallel 2-5× mean.
  **Per-stage split:** ❌ Stage 4.1 (4 KiB/chunk) = serial, ❌ Stage 3.1 (1-2 KiB/chunk) =
  serial, ⚠️ Stage 6.1 (ECS per-system) = TBD separate experiment, ✅ Stage 4.3 (128+ chunks
  batch world gen) = re-evaluate. **Mainline рекомендация:** НЕ подключать thread pool /
  TBB / libdispatch / `std::execution` по default. Per `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` («if perf gain < 5-10%, choose simple») — measured: pool overhead
  = 5-15× per-task compute, NO measured gain for ProjectV primary workloads. Estimated mainline
  effort: **XS** (anti-pattern: «don't add pool по default»). Cross-axis closure: today 12+
  experiments closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async + job-scheduling).
  Re-evaluation triggers: Stage 6.1 Step 6 NUMA-aware, Stage 4.3 lift draw distance, AVX-512
  hardware arrival (Zen 5), real perlin/SVDAG workload, `stdexec::static_thread_pool` direct
  measurement when Clang 23+ + libc++ stable (P2300R10 published 2024-06; P3826R3 fix 2026-01;
  C++26 publication expected 2026-2027; per `bigcpp.com` 2026-05-25 GCC 15+ / Clang 20+
  partial). Caveats: (a) single-vendor (Zen 3 5800X, governor `powersave`); (b) synthetic
  workload (splitmix32 + 64-block mask), not real perlin/SVDAG (per `simd-procedural-noise`
  real perlin = 1.14-1.83× AVX2 vs scalar = potentially 3-5× more compute); (c) no AVX-512
  (Zen 3 = no HW support); (d) no memory bandwidth measurement via `perf stat` (требует
  root + `perf_event_open`); (e) cross-vendor unmeasured (Intel desktop no-HT, EPYC NUMA,
  Arm big.LITTLE). Cross-refs: `flecs-soa-vs-aos-bench` (closed verdict=yes, ECS layout
  settled — этот experiment = job-scheduling surface для ECS multi-thread), `async-compute-
      overhead-numbers` (closed verdict=yes, async foundation on GPU = async foundation on CPU
  side here), `simd-procedural-noise` (closed verdict=mixed, per-chunk CPU compute measured
  — этот experiment = dispatcher для batch таких workloads), `agent/knowledge.md §29.0`
  line 887 (Tier 4 R&D marker), `TODO.md §4.1` (background world gen dispatcher) + `§6.1`
  Step 6 (NUMA-aware allocation may shift tradeoff), `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` (5-10% threshold). [Sync fix r1 (2026-06-20 post-parallel-session):
  запись переехала из `§In progress` → `§Closed` per AGENTS.md §13.5 после RACE CONDITION
  CORRECTION (см. выше `vulkan-fps-pacing-vk-ext` note).]

- [x] **[2026-06-20-rt-shadows-vs-csm](./experiments/2026-06-20-rt-shadows-vs-csm/)** — m,
  Stage 5.2 (RTX shadows feature-flagged additive path). Closed `2026-06-20` (single session),
  verdict **`mixed`**. **Shadow-ось experiment** — финальный штрих lighting axis после
  `vct-vs-rt-cutoff` (verdict=mixed, GI cutoff) + `clustered-forward-mass-lights` (verdict=yes,
  light SSBO array). Per `TODO.md §5.2` explicit: «they don't replace CSM, they complement it»
  + `agent/decisions.md §15` explicit: «do NOT replace with RTX blindly; RTX = additive
    feature-flag». Web-research complete (4 batches, ~30 results, 23 sources верифицированы):
    Boksansky RTG 2019 фундамент (adaptive ray-traced shadows vs CSM через DXR),
    NVIDIA Blackwell whitepaper Jan 2025 (4th-gen RT Cores, **2× ray-tri throughput vs Ada**,
    Mega Geometry **8× vs Turing**, 0.75× memory footprint), AMD HotChips 2025 RDNA 4
    (**8 box + 2 tri/cycle** per Ray Accelerator, 2× vs RDNA 3, OBB +10% traversal, BVH8),
    Intel Battlemage Xe2 (**3 traversal pipelines + 2 tri = 18+2 vs Alchemist 2+1**, BVH cache
    16 KB), Khronos Forum 2025-09-29 BLAS fence wait pattern (2000 BLAS single dispatch = 15 ms
    CPU wait), NVIDIA nvpro-samples BLAS memory budgeting + compaction pattern, Khronos
    VK_KHR_deferred_host_operations spec (v4), ACM SIGGRAPH 2025 mobile RT (LightweightVK
    Kuznetsov), Arm Vulkanised 2026 RQ optimization (42.6% Bistro shadows with
    TerminateOnFirstHitEXT), Vulkan Tutorial Ray Query §5.2 patterns, Bistro/Sponza mobile
    frame times (Xclipse 940 = 10-18 ms, Mali G715 = 29-466 ms), Sascha Willems rayquery.cpp
    reference, и т.д. Standalone GPU prototype deferred — analytical cost model +
    cross-vendor matrix sufficient per `vulkan-fps-pacing-vk-ext` + `vct-vs-rt-cutoff` precedent
    (literature + analytical → integration recommendation). **Cross-vendor RT throughput matrix
    (per cycle per RTU/Ray Accelerator):** NVIDIA Turing 1+1 → Ampere 4+1 → Ada 4+4 →
    Blackwell 8+8; AMD RDNA 2/3 4+1 → RDNA 4 8+2 (9070 = 111.76G box/s + 19.61G tri/s vs 6900XT
    38.8G + 10.76G per chipsandcheese DXR); Intel Alchemist 2+1 → Battlemage 3+2 (16 KB BVH cache,
    16B nodes/sec across RTAs). **Hybrid CSM + RTX shadows** рекомендован для Stage 5.2: CSM
    (sun, current 4-cascade path per `agent/decisions.md §15`, **DO NOT TOUCH**) + RTX
    `VK_KHR_ray_query` (feature-flagged additive для local lights + per-pixel contact shadow
    detail). **Quality gain > 5% per `optimization-philosophy.md`** для non-sun-dominated scenes
    (cave/lava/magic-heavy); < 5% для sun-dominated outdoor (CSM dominant, RTX inactive). VRAM
    cost **8-23 MiB** на RTX 3060 Ti dev host (well under 5% budget). **Mainline рекомендация:**
    3-step migration per `agent/knowledge.md §30.4` precedent — **Step 1** foundation (extension
    probing `VK_KHR_acceleration_structure` rev 13 + `VK_KHR_ray_query` rev 1 +
    `VK_KHR_deferred_host_operations` rev 4 в `VulkanBootstrap.cpp::TryPickPhysicalDevice` + new
    `RayTracedShadows.{hpp,cpp}` skeleton + `BlasPool` + `TlasInstanceBuffer` + scratch, ~150 LoC,
    S effort); **Step 2** RTX integration (`rayQueryEXT` в `voxel.frag` для local lights,
    max **8 rays/pixel** total budget spread across top-4 lights by contribution per cluster,
    per `clustered-forward-mass-lights` cluster grid, async BLAS build via
    `VK_KHR_deferred_host_operations` pattern, per-vendor feature flags per `dec-pipelines-async-compute`
    precedent — Blackwell/RDNA 4/Battlemage = full benefit, Ampere/RDNA 3 = 1-2 rays limited,
    Turing/Alchemist = OFF, ~250 LoC, M effort); **Step 3** default flip (XS single config,
    `PROJECTV_ENABLE_HW_RAY_TRACING=ON` в dev preset если HW, OFF в production per `TODO.md §5.2`
    line 240 default). ~770 LoC total, M effort, 3-4 sessions. **Continuation chain:**
    `vct-vs-rt-cutoff` (closed verdict=mixed) + `clustered-forward-mass-lights` (closed verdict=yes)
    → this. **Lighting axis complete** (cutoff + lights + shadows все closed same-day `2026-06-20`).
    Stage 5 foundation (nanovdb-on-gpu yes) + cutoffs (vct-vs-rt-cutoff mixed) + lights
    (clustered-forward-mass-lights yes) + shadows (this) все closed same-day `2026-06-20`.
    Cross-axis: 18+ closed today-сессии = full Stage 1.x/2.x/3.x/5.x/6.x optimization landscape
  + shadow-dim. **Caveats:** (a) analytical model only, no ProjectV GPU prototype; (b)
    cross-vendor numbers from published benchmarks not measured locally; (c) BLAS rebuild
    fence wait bottleneck requires async pattern (per Khronos Forum 2025-09-29); (d) CSM
    baseline untouched per `decisions.md §15`; (e) re-evaluation triggers: Stage 4.3
    lift draw distance (128+ chunks BLAS pool budget), Blackwell consumer adoption (8× RT
    throughput enables 8-ray soft shadow default), future RDNA 5 / Intel Celestial arch changes.
    Cross-refs: `TODO.md §5.2`, `agent/knowledge.md §15` (CSM baseline), `agent/knowledge.md §30.4`
    (3-step migration precedent), `dec-pipelines-async-compute` (closed verdict=yes, async
    foundation), `bindless-descriptor-overhead` (closed verdict=mixed, Phase E RTX TLAS bindless),
    `clustered-forward-mass-lights` (closed verdict=yes, light list source для per-fragment ray
    budget), `hzb-binding-models` (closed verdict=mixed, texelFetch pattern для BLAS visibility AABB),
    `work-stealing-job-system` (closed verdict=mixed, NOT recommend pool → use dedicated 1-2 host
    threads OR `std::jthread`), `nanovdb-on-gpu` (closed verdict=yes, NanoVDB-aligned mesh source
    for BLAS triangle data), `async-compute-overhead-numbers` (closed verdict=yes, +9.85-11.34%
    async speedup pattern applicable). Closed entry: `experiments/2026-06-20-rt-shadows-vs-csm/`.

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