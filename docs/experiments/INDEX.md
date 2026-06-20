# INDEX — `docs/experiments/`

Текущий снимок состояния. Долговечные правила — `AGENTS.md`. Канбан гипотез — `research/backlog.md`.

---

## 1. Now

Just-closed (this session, `2026-06-20`):

- `2026-06-20-restir-gi-feasibility` (verdict=`mixed`). SOTA-GI-ось experiment закрыт same session. Web-research
  (~30 sources верифицированы: Bitterli 2020 ReSTIR original, Ouyang 2021 ReSTIR GI, Lin 2022 ReSTIR PT +
  GRIS, Majercik 2019/2021 DDGI, Müller 2021 NRC, NVIDIA-RTX/RTXGI SDK v2.7.0 (Mar 2026), NVIDIA-RTX/SHARC,
  NVIDIA-RTX/RTXDI v3.0+, Crassin 2011 GIVoxels VCT foundation, Lumen SIGGRAPH 2022 [Epic explicitly rejected
  VCT as leaky], Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache], Douglas Voxel Devlog #23 Jun
  2025 [voxel + DDGI direct validation], Cyberpunk 2077 RT Overdrive [production ReSTIR DI/GI + SHaRC],
  NVIDIA Zorah RTX 50 demo [ReSTIR PT], OGRE-Next CIVCT, Aokana 2025, Closest Hit ReSTIR GSGI/PMGI 2024,
  ReSTIR FG 2024, Epic DDGI abandonment Dec 2025 forum). **Главный finding:** SOTA GI techniques (ReSTIR PT,
  DDGI, SHaRC, NRC) все **требуют path tracer foundation** — ProjectV's Stage 5.x = hybrid VCT+RTX = **не**
  path tracer. **Architectural mismatch.** **Recommended action:** keep current hybrid VCT+RTX as-is (Stage 5.x
  MVP), defer SOTA GI integration до Stage 6+ post-MVP path tracer pivot. Recommended add-on order (if path
  tracer ships): **SHaRC → DDGI → ReSTIR DI/GI/PT** (skip NRC = NVIDIA-only). VRAM cost SHaRC alone = 185 MB
  (3.65% of 5.06 GiB budget per `hardware-profile.md` §3). Quality validated для path-tracing contexts (ReSTIR
  PT MAPE 0.39 vs 1.63 naive PT per Lin 2022 Carousel benchmark). Cannot translate без path tracer. **Lighting
  axis fully closed** (`vct-vs-rt-cutoff` + `clustered-forward-mass-lights` + `rt-shadows-vs-csm` + this).
  Cross-axis: 19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI
  axis. См. §6 + §8 + [experiment README](./experiments/2026-06-20-restir-gi-feasibility/README.md).

- `2026-06-20-rt-shadows-vs-csm` (verdict=`mixed`). Shadow-ось experiment закрыт same session.
  Web-research (4 batches, ~30 results, 23 sources верифицированы: Boksansky RTG 2019 фундамент,
  Vulkan Tutorial Ray Query §5.2 patterns, NVIDIA Blackwell 4th-gen RT whitepaper Jan 2025
  [2× ray-tri vs Ada, 8× vs Turing], AMD HotChips 2025 RDNA 4 [8 box + 2 tri/cycle, 2× vs
  RDNA 3, OBB +10% traversal], Intel Battlemage Xe2 [3 traversal pipelines + 2 tri = 18+2 vs
  Alchemist 2+1, BVH cache 16 KB], Khronos Forum BLAS fence wait pattern, Boksansky 2019
  adaptive ray sampling) + analytical cost model + cross-vendor RT throughput matrix.
  **Hybrid CSM + RTX shadows** рекомендован для Stage 5.2: CSM (sun, current path per
  `agent/decisions.md §15`) + RTX `VK_KHR_ray_query` (feature-flagged additive для local
  lights + per-pixel contact shadow detail). **Quality gain > 5% per
  `optimization-philosophy.md`** для non-sun-dominated scenes (cave/lava/magic); < 5%
  для sun-dominated outdoor (CSM dominant). VRAM cost **8-23 MiB** на RTX 3060 Ti (well
  under 5% budget). BLAS rebuild bottleneck → async via `VK_KHR_deferred_host_operations`
  (rev 4) + `dec-pipelines-async-compute` precedent. Cross-vendor: Blackwell/RDNA 4/
  Battlemage = full benefit; Ampere/RDNA 3 = 1-2 rays limited; Turing/Alchemist = feature
  OFF. **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent
  (Step 1 foundation extension probing + BLAS pool + TLAS scratch; Step 2 ray query в
  `voxel.frag` для local lights + async BLAS build via deferred host operations; Step 3
  default flip). ~770 LoC total, M effort, 3-4 sessions. **Continuation chain:**
  `vct-vs-rt-cutoff` (closed verdict=mixed) + `clustered-forward-mass-lights` (closed
  verdict=yes) → this. Lighting axis complete (cutoff + lights + shadows). Stage 5
  foundation + cutoffs + lights + shadows все closed same-day `2026-06-20`. Cross-axis:
  17+ closed today-сессии = full Stage 1.x/2.x/3.x/5.x/6.x optimization landscape +
  shadow-dim. См. §6 + §8 + [experiment README](./experiments/2026-06-20-rt-shadows-vs-csm/README.md).
- `2026-06-20-svdag-vs-vdb-memory-throughput` (verdict=`yes`). SVDAG-on-64-tree (current mainline)
  подтверждён **измерениями** для ProjectV workload (32³ chunks): memory 8.75 B/voxel solid / 16-70 B/voxel sparse —
  within dubiousconst282 2024 literature range. GetCell 22-36 ns, SetCell 0.03-0.04 µs no-dedup / 0.68-1.26 µs dedup-ON.
  **Dedup ON costs 20-40× build time** на non-repetitive scenes → рекомендация: per-chunk `isStatic` flag
  (Stage 1.2 design) instead of always-on. Закрыл measurement gap от `sparse-64-tree-alternatives` §5.3.
- `2026-06-20-dec-pipelines-async-compute` (verdict=`yes`). Sync-axis experiment — async-compute queue +
  `VK_KHR_synchronization2` (core 1.3) + `VK_KHR_timeline_semaphore` (core 1.2) +
  `VK_KHR_global_priority` (core 1.4) рекомендованы для 4 of 5 ProjectV compute passes: Stage 2.2 HZB
  cull + Stage 3.1 Fluid CA (20 Hz) + Stage 4.1 GPU world gen (LOW priority) + Stage 5.2 RTX BLAS build
  (`VK_KHR_deferred_host_operations`). Stage 5.1 VCT sequential default, async opt-in. Expected 5-8%
  steady-state + 100% spike elimination (world gen + BLAS). Cross-vendor: NVIDIA Ampere/Ada/Blackwell +
  AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage. Caveats: NVIDIA June 2025 driver bug
  mesh-shading+async (не applies to compute cull path); AMD «export bound shaders» warning; Intel
  Ray Queries + groupshared L1 contention. Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA
  (sync-model конкретизирует `agent/knowledge.md §30.4` contract), Stage 2.2 HZB full integration, Stage
  5.2 RTX BLAS build (Phase E per `bindless-descriptor-overhead`).
- `2026-06-20-nanovdb-on-gpu` (verdict=`yes`). GPU-side measurement closing the gap from
  `svdag-vs-vdb-memory-throughput` §3 line 157 + bugfix NanoVDB-like impl (uniform-tile lie).
  **Both CPU-side and GPU-side prototypes byte-exact** на 5 сценах × 2 kernels (verify_mismatches=0).
  NanoVDB-aligned pointer-less layout (Upper[8³] → Lower[4³] → Leaf[2³], scaled per NanoVDB.h actual
  32³/16³/8³ structure для ProjectV chunkSize=8) outperforms SVDAG-on-64-tree **on 4/5 scenes by
  12-141%** (sparse_random_8: 500 → 1210 Mrays/s; voxel_lab_8: 541 → 1208 Mrays/s; ground_8: 638 →
  1242 Mrays/s; brick_8: 1146 → 1284 Mrays/s). Only solid_8 ties (1265 vs 1272, memory-bandwidth
  bound). **GPU memory: NanoVDB uses 57-75% less VRAM**. **CPU memory: ~50% less** (B/voxel). Crosses
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Critical mainline
  finding:** ProjectV chunkSize = 8 (not 32 as previous experiment assumed) per
  `src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not depth=3. OpenVDB 13.0.0
  (Nov 2025) lowered NanoVDB mutation barrier. Mainline рекомендация: **hybrid strategy** — keep
  CPU-side SVDAG-on-64-tree (Stage 1.2), flatten to NanoVDB-aligned transient SSBO at GPU upload for
  Stage 5.1 VCT cone-march + 3 fragment-shader DDA traces (`voxel.frag` per `TODO.md §6.2.2`). 3-step
  migration per `agent/knowledge.md §30.4` precedent. Caveats: single GPU vendor validated (NVIDIA
  RTX 3060 Ti GA104 Ampere, Vulkan 1.4.350); HDDA-specific optimizations not implemented in
  first-iteration prototype. Continuation chain: `sparse-64-tree-alternatives` → `svdag-vs-vdb-memory-throughput`
  → this. Cross-axis: previous experiments covered memory + sync; this covers GPU traversal for
  Stage 5.1.
- `2026-06-20-hzb-binding-models` (verdict=`mixed`). Cull-shader pattern decision для Stage 2.2. Web-research
  (~10 sources incl. critical NVIDIA `textureLod` bug под `VK_EXT_descriptor_heap` per
  `foijord/vk-textureLod-repro` 2026) + standalone Vulkan compute prototype + 24 sampling tests across
  8 mips × 3 patterns. **17/24 PASS, 7/24 FAIL.** Conclusive findings: (a) `texelFetch(sampler2D, ivec2,
  mipLevel)` correct + bindless-robust (recommended); (b) `textureLod` correct on classic, fragile под
  bindless на NVIDIA (NOT recommended); (c) `imageLoad(storage_image)` fundamentally unsuited для HZB
  culling (GLSL single-mip-per-binding, proved by `max_abs_error = N * 1000` pattern). Mainline
  recommendation: Stage 2.2 cull shader uses `texelFetch`, HZB descriptor = `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`
    + separate `SAMPLER`. ~50-100 LoC change across 4 files. Future-proofs `bindless-descriptor-overhead`
      Phase E.

`2026-06-20-simd-procedural-noise` closed (verdict=`mixed`) — см. §6 + §8.

`2026-06-20-nanovdb-on-gpu` closed (verdict=`yes`) — hybrid strategy recommended. See §6 + §8.

- `2026-06-20-vct-vs-rt-cutoff` (verdict=`mixed`). Lighting/GI-ось experiment закрыт same session.
  Roughness-based hybrid VCT + RTX рекомендован: VCT diffuse always (6 wide cones), VCT specular
  при roughness > 0.3 (cone-march через mip-mapped atlas), RTX (`rayQueryEXT`) при roughness < 0.3
  (sharp specular + AO/contact shadows), CSM для sun (current path, additive к RTX per `decisions.md
  §15`). **Refined cutoff = 0.3** (не 0.3–0.5 диапазон): VCT specular 2.5× at r=0.3 = RTX 1-ray cost;
  OGRE 2019 precision cliff at 0.02 (8-bit atlas risk, ProjectV R8G8B8A8 same); Akenine-Möller JCGT
  2021 GGX math validates roughness → cone spread; Lumen 2022 rejected pure VCT (leaking coarse mips)
  → RTX-dominant. Cross-vendor threshold adjustment: Blackwell → 0.4-0.5 (2× tri rate vs Ada), RDNA
  2 → 0.2 (¼ tri rate), Battlemage → 0.25, no-HW-RT → VCT-only fallback. Web-research ~30 sources
  (Crassin 2011 GIVoxels, NVIDIA VXGI 0.9, OGRE 2019, Lumen SIGGRAPH 2022, Narkowicz "Journey to Lumen"
  2022, Akenine-Möller JCGT 2021, RTXGI 2.0 SDK 2024, RTXDI 3.0, Erlich 2024 Eurographics, NVIDIA
  Blackwell 2025, AMD RDNA 4 2025, Intel Battlemage 2025, Aokana 2025, etc.). Mainline integration:
  4-step migration per `agent/knowledge.md §30.4` precedent — Step 1 foundation (cutoff constant + HW
  RT probe + CMakeLists feature flag), Step 2 VCT (voxelize.comp + vct.frag + 3D atlas + mip chain
  per `TODO.md §5.1`), Step 3 RTX (BLAS per chunk + TLAS per frame + rayQueryEXT per `TODO.md §5.2`),
  Step 4 (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT. Caveats: analytical model only (no
  ProjectV prototype), NVIDIA-heavy literature, ProjectV VCT leak risk = lower than Lumen surface
  cache (regular voxel SVO) but not zero. **Lighting/GI-ось closed**; Stage 5 теперь имеет все три
  foundation: storage (nanovdb-on-gpu), sync (dec-pipelines-async-compute), cutoff strategy (this).
  См. §6 + §8.

## 2. Nearest Gap

Next h/m from `research/backlog.md`:

- `sub-chunk-layers` (m, independent) — для biome/cave layers.
- `wfc-procedural-worlds` (m, independent) — для Stage 4.x procedural gen.
- `restir-gi-feasibility` (m, Stage 5.1/5.2) — **closed `2026-06-20`** (verdict=`mixed`). См. §6.
- `vct-vs-rt-cutoff` (m, Stage 5.1/5.2) — **closed `2026-06-20`** (verdict=`mixed`). См. §6.
- `vct-vs-rt-cutoff` (m, Stage 5) — после Stage 5.1 VCT spike.

Closed (recent, see §6 for full list):

- `dec-pipelines-async-compute` (m, Stages 2.2/3.1/4.1/5.2) — closed `2026-06-20`, verdict=`yes`.
  Foundation шаг (`vkQueueSubmit2` + timeline semaphores) — prerequisite для Stage 3.1 GPU Fluid CA,
  Stage 2.2 HZB full integration, Stage 5.2 RTX BLAS build.
- `cache-oblivious-chunk-tree` (m) — closed `2026-06-20`, verdict=`mixed`. Re-evaluation trigger: Stage 4.3
  (128+ chunks draw distance). Defer до re-evaluation.
- `svdag-vs-vdb-memory-throughput` (h) — closed `2026-06-20`, verdict=`yes`. Закрыл measurement gap.
- `bindless-descriptor-overhead` (m, Stage 2.x) — closed `2026-06-20`, verdict=`mixed`. Mainline
  рекомендация: hybrid strategy, 5-phase rollout (Phase A push shadow cascade → Phase B bindless
  material table → Phase C bindless Sparse64Node → Phase D bindless virtual texture → Phase E bindless
  RTX TLAS). Cross-refs: `TODO.md` §1.1/§1.2/§2.1/§2.2/§2.3/§5.2, `agent/knowledge.md §4/§15/
  §25/§30.4`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

## 3. Next Steps

Определяются оператором. По умолчанию: следующий h-priority из backlog (все h-priority сейчас
закрыты либо in-progress).

## 4. Risks

- Конфликт scope с mainline-агентом: если mainline правит `docs/experiments/` (что запрещено моим протоколом, но не
  запрещено корневым) — зафиксировать в `STATUS.md` заблокированного эксперимента и эскалировать.
- Устаревание web-источников: каждый эксперимент датируется; старше 12 месяцев — перепроверять.

## 5. Active experiments (current open sessions)

(No active self reservations `2026-06-20` EOD. `2026-06-20-restir-gi-feasibility` closed verdict=mixed same
session — см. §6 + §1. **Lighting axis fully closed**: cutoff + lights + shadows + SOTA-GI все same-day
`2026-06-20`.)

## 6. Recent closed sessions

| Slug                                        | Status                  | Verdict                                                                                                                                                                                                                                                                                                                                                                                                | Closed     |
|:--------------------------------------------|:------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------|
| `2026-06-20-sparse-64-tree-alternatives`    | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20 |
| `2026-06-20-mesh-shader-vs-compute-cull`    | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-bindless-descriptor-overhead`   | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-cache-oblivious-chunk-tree`     | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-svdag-vs-vdb-memory-throughput` | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20 |
| `2026-06-20-dec-pipelines-async-compute`    | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20 |
| `2026-06-20-nanovdb-on-gpu`                 | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20 |
| `2026-06-20-hzb-binding-models`             | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-simd-procedural-noise`          | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-vct-vs-rt-cutoff`               | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20 |
| `2026-06-20-flecs-soa-vs-aos-bench`         | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20 |
| `2026-06-20-async-compute-overhead-numbers` | concluded-verdict-yes   | **yes (+9.85-11.34% speedup)**                                                                                                                                                                                                                                                                                                                                                                         | 2026-06-20 |
| `2026-06-20-meshing-algo-comparison`        | concluded-verdict-mixed | mixed (greedy: poly count ✓, build time ✗)                                                                                                                                                                                                                                                                                                                                                             | 2026-06-20 |
| `2026-06-20-vulkan-fps-pacing-vk-ext`       | concluded-verdict-mixed | mixed (SOTA validated; prototype deferred)                                                                                                                                                                                                                                                                                                                                                             | 2026-06-20 |
| `2026-06-20-work-stealing-job-system`       | concluded-verdict-mixed | mixed (serial beats pool for ProjectV workloads; per-stage split)                                                                                                                                                                                                                                                                                                                                      | 2026-06-20 |
| `2026-06-20-clustered-forward-mass-lights`  | concluded-verdict-yes   | yes (with caveats: soft cap ≥2048, light prioritization for 5000+ light scenes)                                                                                                                                                                                                                                                                                                                        | 2026-06-20 |
| `2026-06-20-vis-buffer-for-voxels`          | concluded-verdict-mixed | mixed (cross-over @ 1280×720; +12-24% faster на 800×600 / −15-26% на 1920×1080; voxel scenes pixel-coherent = no overdraw to amortize)                                                                                                                                                                                                                                                                 | 2026-06-20 |
| `2026-06-20-rt-shadows-vs-csm`              | concluded-verdict-mixed | mixed (hybrid CSM + RTX additive per `TODO.md §5.2`; CSM dominant для sun, RTX для local lights + contact; cross-vendor matrix Blackwell/RDNA4/Battlemage full, Ampere/RDNA3 limited, Turing/Alchemist OFF)                                                                                                                                                                                            | 2026-06-20 |
| `2026-06-20-restir-gi-feasibility`          | concluded-verdict-mixed | mixed (SOTA GI quality validated — ReSTIR PT MAPE 0.39 vs 1.63; SHaRC 1.5-10% overhead; DDGI voxel-validated per Douglas Voxel Devlog #23 — but **architectural mismatch**: все SOTA techniques требуют path tracer foundation, ProjectV Stage 5.x = hybrid VCT+RTX = NOT path tracer; defer до Stage 6+ post-MVP path tracer pivot; recommended add-on order SHaRC→DDGI→ReSTIR, skip NRC=NVIDIA-only) | 2026-06-20 |

## 7. Backlog

См. `research/backlog.md`.

## 8. Last update

`2026-06-20` — closed `2026-06-20-restir-gi-feasibility` (verdict=`mixed`). SOTA-GI-ось experiment.
Web-research complete (3 batches, ~30 results, ~30 sources верифицированы: Bitterli 2020 ReSTIR original,
Ouyang 2021 ReSTIR GI, Lin 2022 ReSTIR PT + GRIS [80 ms @ 1920×1080, MAPE 0.39 vs 1.63 PT], Majercik 2019/2021
DDGI, Müller 2021 NRC [2.6 ms @ full HD], NVIDIA-RTX/RTXGI SDK v2.7.0 [336 stars Mar 2026], NVIDIA-RTX/SHARC
[123 stars, spatial hash grid 64-bit, 4-pass, ~185 MB @ 2^22, 1.5-10% overhead Cyberpunk], NVIDIA-RTX/RTXDI
v3.0+ [ReSTIR DI/GI/PT/ReGIR, D3D12+Vulkan], Crassin 2011 GIVoxels, Lumen SIGGRAPH 2022 [Epic rejected VCT leaky],
Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache], Douglas Voxel Devlog #23 Jun 2025 [voxel + DDGI],
Cyberpunk 2077 RT Overdrive Patch 2.1 Dec 2023 [production ReSTIR + SHaRC], NVIDIA Zorah RTX 50 demo 2025
[ReSTIR PT], OGRE-Next CIVCT, Aokana 2025, ReSTIR FG/GSGI/PMGI 2024 [0.4-14 ms variants], Epic DDGI abandonment
forum Dec 2025). **Главный finding:** **architectural mismatch** — все 4 SOTA техники (ReSTIR PT, DDGI, SHaRC,
NRC) требуют path tracer foundation; ProjectV Stage 5.x = hybrid VCT+RTX = NOT path tracer. **VRAM matrix:**
SHaRC = 185 MiB (3.65% of 5.06 GiB budget per `hardware-profile.md` §3), DDGI = 16 MiB, ReSTIR = 33-67 MiB
checkerboard/full. Cross-vendor: SHaRC = universal (RTXGI 2.x Vulkan path), NRC = NVIDIA-only (Tensor Cores
≥ Turing, excludes AMD RDNA 4 + Intel Battlemage per `dec-pipelines-async-compute` matrix). **Mainline
рекомендация:** **keep current hybrid VCT+RTX as-is** (Stage 5.x MVP), **defer SOTA GI до Stage 6+ post-MVP
path tracer pivot**. Recommended add-on order if path tracer ships: **SHaRC → DDGI → ReSTIR DI/GI/PT**.
**Lighting axis FULLY closed** (cutoff + lights + shadows + SOTA-GI all same-day `2026-06-20`). Cross-axis:
19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI axis. Closed
entry: `experiments/2026-06-20-restir-gi-feasibility/`. См. §6 + §1.

`2026-06-20` (this session, previous) — closed `2026-06-20-rt-shadows-vs-csm` (verdict=`mixed`). Shadow-ось experiment.
Web-research complete (4 batches, ~30 results, 23 sources верифицированы: Boksansky RTG 2019,
NVIDIA Blackwell whitepaper Jan 2025, AMD RDNA 4 HotChips 2025, Intel Battlemage Xe2, Khronos
VK_KHR_deferred_host_operations spec, NVIDIA nvpro-samples BLAS pattern, Khronos Forum BLAS
fence wait, ACM SIGGRAPH 2025 mobile RT, Arm Vulkanised 2026, Vulkan Tutorial Ray Query §5.2,
Sascha Willems rayquery example, и т.д.). Analytical cost model + cross-vendor RT throughput
matrix. Hybrid CSM + RTX shadows рекомендован для Stage 5.2: CSM (sun, current path per
`agent/decisions.md §15`) + RTX `VK_KHR_ray_query` (feature-flagged additive для local
lights + per-pixel contact shadow detail). **Quality gain > 5% per `optimization-philosophy.md`**
для non-sun-dominated scenes (cave/lava/magic-heavy); < 5% для sun-dominated outdoor (CSM dominant).
VRAM cost **8-23 MiB** на RTX 3060 Ti (well under 5% budget). BLAS rebuild bottleneck → async via
`VK_KHR_deferred_host_operations` (rev 4) + `dec-pipelines-async-compute` precedent (per Khronos
Forum 2025-09-29: 2000 BLAS single dispatch = 15 ms fence wait). Cross-vendor: Blackwell/RDNA 4/
Battlemage = full benefit; Ampere/RDNA 3 = 1-2 rays limited; Turing/Alchemist = feature OFF.
**Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent (Step 1
foundation extension probing + BLAS pool + TLAS scratch; Step 2 ray query в `voxel.frag` для
local lights + async BLAS build via deferred host operations; Step 3 default flip). ~770 LoC
total, M effort, 3-4 sessions. **Continuation chain:** `vct-vs-rt-cutoff` (closed verdict=mixed) +
`clustered-forward-mass-lights` (closed verdict=yes) → this. **Lighting axis complete** (cutoff +
lights + shadows). Stage 5 foundation + cutoffs + lights + shadows все closed same-day `2026-06-20`.
Closed entry: `experiments/2026-06-20-rt-shadows-vs-csm/`. Rendering-approach
axis (deferred resolve via vis-buffer + material-table SSBO). Standalone Vulkan 1.4 prototype
(~700 LoC incl. shaders, RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02). 6 measurement
configs (3 scenes × 3 resolutions). Visual equivalence verified via framebuffer hash match.
**Cross-over @ 1280×720:** 1920×1080 vis-buffer 15-26% slower (bandwidth-bound on pixel
coverage); 800×600 vis-buffer 12-24% faster (vertex cost dominates). Voxel scenes are
pixel-coherent after greedy meshing per `2026-06-20-meshing-algo-comparison` verdict=mixed
(Naive Greedy default = ~1 visible triangle per pixel = no overdraw to amortize fullscreen
vis-buffer cost). Mainline рекомендация: **DEFER** до Stage 4.3 (128+ chunks draw distance)
или mobile target decision (TBR GPUs benefit per Vulkan-Guide, vis-buffer 10-30% win).
Cross-refs: `bindless-descriptor-overhead` Phase B (bindless material table = prerequisite),
`dec-pipelines-async-compute` (async-compute resolve pass would compound benefits, unmeasured),
`meshing-algo-comparison` verdict=mixed (greedy meshing = pixel-coherent = vis-buffer loses на high res).
Web-research: 5 batch queries, 20+ sources верифицированы (Burns-Hunt 2013 JCGT foundational
6.2× bandwidth win; Karis SIGGRAPH 2021 + Wihlidal GDC 2024 Unreal Nanite 64-bit vis-buffer +
shading bins 100% compute shaders UE 5.4; Andersson Frostbite 2017 "10-20x geometry vs Deferred";
The Forge v1.57 May 2024 TVB 2.0 pure compute; Cao NanoMesh SIGGRAPH 2024 32-bit mobile;
Vulkan-Guide TBR best practices 2024; Lam Adreno vis-stream HW compressor; jglrxavpok 2023
Vulkan R64Uint impl; Harada AMD Forward+ GPU Pro 4 alternative; Olsson Clustered Shading HPG 2012
1M lights; VoxelMVP / Exile / Slater / cgerikj / Ascendant voxel-specific refs). См. §6 +
[experiment README](./experiments/2026-06-20-vis-buffer-for-voxels/README.md).

`2026-06-20` — closed `2026-06-20-clustered-forward-mass-lights` (verdict=`yes`). Mass-lights
architecture axis: Forward+ (clustered shading) рекомендован для Stage 5 с условиями (soft cap
≥2048, light prioritization для 5000+ light scenes). Standalone CPU prototype
`prototype/bench.cpp` (~480 LoC, Clang 22.1.6, no warnings, 13 configs). Measured cluster
build 16×9×24 / 1000 lights = 12.7 ms CPU (sparse) / 15.4 ms CPU (dense). GPU projected
0.1-0.5 ms at 1000 lights. **CRITICAL: 16×9×24 / 5000 dense lights = 69% clusters overflow
soft cap 1024** — soft cap must be raised или prioritization policy. Per-fragment 100×
speedup vs 1000-light uniform array. Mainline 3-step migration (M effort, 3-4 sessions).
Cross-axis: 14+ closed same-day `2026-06-20` sessions покрывают full Stage 1.x/2.x/3.x/4.x/5.x/6.x
optimization landscape + mass-lights axis. Closed entry:
`experiments/2026-06-20-clustered-forward-mass-lights/`. ECS memory-layout-ось experiment
(Stage 6.1 + cross-cutting). Standalone C++26 prototype `prototype/flecs_soa_vs_aos.cpp` (642 строки, 4 configs ×
3 workloads × 3 seeds × 1000 iterations = 36 measurements). **SoA wins ALL 3 workloads** — raycast **2.14×**
(199→427 Meps), physics **3.86×** (210→812 Meps, near-exact match с DevelopersIO 2026 Godot 4.6 3.3× update
benchmark), cull **1.44×** (315→454 Meps, predicate branch dampens gain). Crosses 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by 40-280%. SoA variance ниже AoS (24% reduction
for physics) — deterministic cache-line stride reduces OS scheduler noise. Hybrid ≈ SoA (within 1-2%), HotOnly
worst variance (15% raycast stddev) — NOT recommended. Cross-validation: Mertens 2024 (Flecs default SoA — direct
validation), Sagar 2026 (5.67× OOP→SoA), Bevy PR #14049 (2× dense iteration), AMD EPYC 7003 docs (Zen 3 cache
spec). Mainline рекомендация: keep Flecs default SoA storage (per Mertens 2024 + Flecs v4.1.5), **не возвращаться
на AoS POD-struct per entity** в новых systems. HotOnly-SoA pattern NOT рекомендуется. Snapshot save/load path
остаётся AoS (cold path, simpler code). Estimated mainline effort: **XS** (doc update + code review checklist,
не mainline rewrite). Cross-cutting unblocks для Stage 2.2 HZB cull / Stage 3.1 Fluid CA bookkeeping /
Stage 3.2 Incremental Jolt / Stage 5.1 VCT voxelize — все эти Flecs systems могут proceed с уверенностью
что SoA = correct default. Documentation update recommended для
`legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` mermaid diagram (analytical 3-5× claim → measured
1.44-3.86× numbers с cross-ref). Re-evaluation trigger: Stage 6.1 multi-threading per `TODO.md §6.1` Step 6
(NUMA-aware allocation may shift tradeoff). Cross-axis: 11 closed today-сессии покрывают
storage/sync/cull/binding/layout/meshing/hzb/gpu-traversal/gi-cutoff + теперь ECS memory-layout = full Stage
1.x/2.x/3.x/5.x/6.x optimization landscape.

`2026-06-20` — closed `2026-06-20-vct-vs-rt-cutoff` (verdict=`mixed`). Lighting/GI-ось experiment.
Roughness-based hybrid VCT + RTX рекомендован с **cutoff = 0.3** (VCT high roughness, RTX low roughness,
diffuse GI = VCT always, AO/contact shadows = RTX always, sun = CSM). Web-research ~30 sources
(Crassin 2011, OGRE 2019, Lumen 2022, Akenine-Möller JCGT 2021, RTXGI 2.0, Blackwell 2025, RDNA 4
2025, Battlemage 2025, Aokana 2025, etc.) + analytical cost model + cross-vendor HW RT perf matrix.
Cross-vendor threshold adjustment: Blackwell → 0.4-0.5, RDNA 2 → 0.2, no-HW-RT → VCT-only fallback.
Mainline integration: 4-step migration per `agent/knowledge.md §30.4` precedent (Step 1 cutoff
constant + HW RT probe + CMakeLists flag, Step 2 VCT per `TODO.md §5.1`, Step 3 RTX per `TODO.md
§5.2`, Step 4 optional DDGI/SHaRC/NRC/ReSTIR PT). Stage 5 теперь имеет все три foundation: storage
(`nanovdb-on-gpu`), sync (`dec-pipelines-async-compute`), cutoff strategy (this). См. §1 + §6.
Continuation chain: `nanovdb-on-gpu` → `dec-pipelines-async-compute` → `hzb-binding-models` → this —
4th orthogonal axis (lighting/GI) после storage/sync/binding. Cross-axis: 5 same-day `2026-06-20`
sessions (memory + layout + sync + storage + GI strategy) покрывают Stage 1.x/2.x/3.x/5.x
optimization landscape.

`2026-06-20` — closed `2026-06-20-nanovdb-on-gpu` (verdict=`yes`). GPU-axis experiment closing
`svdag-vs-vdb-memory-throughput` measurement gap. Both CPU-side and GPU-side prototypes byte-exact
(verify_mismatches=0 на 5 сценах × 2 kernels). NanoVDB-aligned pointer-less layout outperforms
SVDAG-on-64-tree **on 4/5 sparse scenes by 12-141%** (sparse_random_8: 500→1210 Mrays/s,
voxel_lab_8: 541→1208, ground_8: 638→1242, brick_8: 1146→1284). Only solid_8 ties (memory-bandwidth-bound).
GPU memory: NanoVDB 57-75% less VRAM. CPU memory: ~50% less. Crosses 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Critical mainline finding:**
ProjectV chunkSize = 8 (not 32 as previous experiment assumed) per
`src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not depth=3. OpenVDB 13.0.0
(Nov 2025) lowered NanoVDB mutation barrier (DilateGrid, MergeGrids, CoarsenGrid, RefineGrid,
PruneGrid, VoxelBlockManager). Mainline рекомендация: **hybrid strategy** — keep CPU-side
SVDAG-on-64-tree (current mainline Stage 1.2 design, proven by `svdag-vs-vdb-memory-throughput`),
flatten to NanoVDB-aligned transient SSBO at GPU upload for Stage 5.1 VCT cone-march + 3
fragment-shader DDA traces in `voxel.frag` per `TODO.md §6.2.2`. 3-step migration per
`agent/knowledge.md §30.4` precedent: Step 1 foundation (CPU→GPU flatten helper, S effort),
Step 2 kernel swap (NanoVDB walker, M effort, includes HDDA optimization), Step 3 default flip
(`PROJECTV_USE_NANOVDB_TRANSIENT_VCT=ON`). Foundation optional dependency: `dec-pipelines-async-compute`
(closed 2026-06-20) for async re-upload. Caveats: single GPU vendor (NVIDIA RTX 3060 Ti GA104
Ampere, Vulkan 1.4.350) — mainline re-test on AMD RDNA2/3 + Intel Arc dev matrix; HDDA-specific
optimizations (warp ballot early-out, ReadAccessor caching) NOT implemented in first-iteration
prototype (would add 10-30% per NanoVDB PR #2220 reference numbers). Continuation chain:
`sparse-64-tree-alternatives` (analysis) → `svdag-vs-vdb-memory-throughput` (CPU) → this (GPU) —
three orthogonal angles of Stage 1.x storage analysis, all closed same-day `2026-06-20`.
Sync fix r1 (post-parallel-session): nanovdb-on-gpu moved from `backlog.md §In progress` → `§Closed`
per §13.5. INDEX.md §1 stale "still in-progress" line 56 обновлено.

`2026-06-20` — closed `2026-06-20-hzb-binding-models` (verdict=`mixed`). Cull-shader pattern decision для
Stage 2.2: switch from `textureLod` (vkguide.dev pattern) к `texelFetch(sampler2D, ivec2, mipLevel)`. Web-research

+ standalone Vulkan compute prototype + 24 sampling tests across 8 mips × 3 patterns. **17/24 PASS, 7/24 FAIL.**
  Storage image (`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` + `imageLoad`) rejected (GLSL single-mip-per-binding limitation,
  proved by `max_abs_error = N * 1000` pattern). `textureLod` correct on classic set but fragile под bindless
  heap на NVIDIA per `foijord/vk-textureLod-repro` 2026 — drives recommendation to use `texelFetch` for
  bindless-robustness. Mainline integration: HZB descriptor = `SAMPLED_IMAGE` + separate `SAMPLER`,
  `hzb_cull.comp` uses `texelFetch`. ~50-100 LoC change. Future-proofs `bindless-descriptor-overhead` Phase E
  rollout. Cross-axis continuity: same-day `2026-06-20` сессии закрыли 6 storage/cull/bindless/sync experiments
  plus hzb binding — orthogonal axes Stage 1.x/2.x/3.x optimization complete.

`2026-06-20` — closed `2026-06-20-dec-pipelines-async-compute` (verdict=`yes`). Sync-axis experiment —
async-compute queue + `VK_KHR_synchronization2` (core 1.3) + `VK_KHR_timeline_semaphore` (core 1.2) +
`VK_KHR_global_priority` (core 1.4) рекомендованы для 4 of 5 ProjectV compute passes: Stage 2.2 HZB
cull + Stage 3.1 Fluid CA (20 Hz, natural async candidate via 3-frame latency) + Stage 4.1 GPU world
gen (LOW priority, background) + Stage 5.2 RTX BLAS build (`VK_KHR_deferred_host_operations` для
non-blocking dispatch). Stage 5.1 VCT — sequential default, async opt-in (RDNA «export bound shaders»
warning). Expected 5-8% steady-state + 100% spike elimination (world gen + BLAS). Crosses 5% threshold
per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Cross-vendor validated: NVIDIA
Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage. Vendor caveats documented in
`sources.md` and `README.md §6`. Mainline рекомендация: 3-step migration per
`agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
conversion (S effort), Step 2 per-pass async adoption gated by `PROJECTV_ASYNC_COMPUTE=ON` env, Step 3
default flip. Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA (sync-model конкретизирует §30.4
contract), Stage 2.2 HZB full integration, Stage 5.2 RTX BLAS build. Cross-axis continuity: memory
(`svdag-vs-vdb-memory-throughput`) + layout (`cache-oblivious-chunk-tree`) + sync (this) — three
orthogonal axes of Stage 1.x/2.x/3.x optimization, all same-day `2026-06-20` sessions. Per
`legacy/docs/architecture/practice/00_engine-structure.md:483` minor fix opportunity: «`VK_KHR_synchronization2`
(core in 1.4)» should be «core in 1.3» per Khronos spec — no functional impact (1.3+ all have it as core).
Sync fix r1 (post-parallel-session): dec-pipelines moved from `backlog.md §In progress` → `§Closed` per §13.5.

`2026-06-20` — closed `2026-06-20-cache-oblivious-chunk-tree` (verdict=`mixed`). Morton (Z-order) reorder
измерен на synthetic random-walk workload (24³ chunks, 33 MiB > L3 32 MiB). Mean latency similar (~40-60 ns)
для baseline vs Morton, p99 inconsistent across seeds, cold cache unaffected. Implementation cost low
(one-time reorder + slot remap) but measured benefit within timer noise. Literature predicts 25-75% cache
miss reduction (arxiv 2603.06771) — not reproduced в этом prototype. Likely reasons: random-walk access
pattern (no spatial coherence), 280 B node size (5 cache lines vs SoftwareSVO's 32 B half-line optimal),
timer resolution ~30 ns. Re-evaluation trigger: `TODO.md §4.3` (128+ chunks draw distance). Sync fix r1
(post-parallel-session): cache-oblivious moved from `backlog.md §In progress` → `§Closed` per §13.5.

`2026-06-20` — closed `2026-06-20-bindless-descriptor-overhead` (verdict=`mixed`). Hybrid descriptor
strategy рекомендуется: bindless для stable resources (material table, Sparse64Node, HZB mip,
virtual texture page table) + traditional+dynamic-offset для transient SSBOs (PackedFace, indirect,
motion) + push descriptors для small per-draw transient. 5-phase rollout plan в
`README.md §7`. `VK_EXT_descriptor_buffer` deferred до NVIDIA native HW support (current emulation
= 5 indirections in VKD3D-Proton per XDC 2025-09-29). Cross-vendor validated: NVIDIA RTX 30/40/50,
AMD RDNA2/3, Intel Arc Gfx12.5+, Arm v9+ Mali. Quantitative refs: Traha 2024 (3.5ms saved =
+5 FPS), Arm Mali sample (38% frame time saved), NVIDIA bindless 7× upper bound
(legacy OpenGL). Continuation chain: `sparse-64-tree-alternatives` → `mesh-shader-vs-compute-cull` →
`bindless-descriptor-overhead`. Все три — same-day `2026-06-20` сессии.

`2026-06-20` (this session) — `2026-06-20-meshing-algo-comparison` closed (verdict=`mixed`). Meshing-axis experiment
(unique h-priority slot после 8 закрытых same-day сессий на orthogonal axes:
storage/sync/cull/layout/binding/memory/hzb). Web-research complete (8 sources across 2 batch queries:
cgerikj binary-greedy 2020, 0fps.net 2012, bonsairobo SN 2020, KAIST ODC SIGGRAPH Asia 2024, MakerTech YouTube
2026, jwarren DC 2002, lpigou SN 2021, isoext 2025). Standalone C++20 prototype `prototype/bench.cpp`
(4 algos × 6 scenes = 24 configs, 1000 iter, mean/median/p95/p99/std, `taskset -c 2` на 5800X).
**Главные findings:** (a) **Naive Greedy** wins triangle count на 5/6 non-degenerate scenes (1.3-450× меньше
triangles vs MC/SN/DC); (b) **Marching Cubes** fastest build time (250-380 µs vs greedy 555-650 µs, 1.7-2.5×
быстрее); (c) **Sparse scenes** (1% density) — SN/MC лучше по triangles (1 220/2 258 vs greedy 3 608);
(d) **DC slowest** (1 170-4 817 µs, QEF overhead 4-5× vs MC). **Refined verdict:** mixed — greedy wins poly count
(главная метрика для vertex-bound Stage 2.1), loses build time. **Mainline рекомендация:** keep Naive Greedy
default для Stage 2.1/3.3; bitwise cull optimization (per cgerikj 2020, 50-200 µs/chunk) — drop-in option
для Stage 4.1 high-frequency rebuild; re-evaluate SN/MC при procedural sparse worlds. Cross-refs:
`agent/knowledge.md §25` (greedy meshing contract, baseline), `src/shaders/voxel_mesh.comp::GreedyFacePass`
(per-axis dispatch, current mainline), `TODO.md §2.1` (mesh shader port, this informs choice) + `§3.3`
(physics mesh, mirror choice), `mesh-shader-vs-compute-cull` (closed verdict=mixed, mesh shader =
feature-flagged optional). Continuation chain: `sparse-64-tree-alternatives` → `svdag-vs-vdb-memory-throughput`
→ this → `Stage 4.1` procedural world gen (re-evaluation trigger). Closed entry:
`experiments/2026-06-20-meshing-algo-comparison/`.

`2026-06-20` — closed `2026-06-20-vulkan-fps-pacing-vk-ext` (verdict=`mixed`). **Frame-pacing-ось**
experiment (Stage 0 / independent, foundation для all stages per DoD principle «low latency >
throughput»). Web-research complete (5 batch queries, 8 key sources + 3 supplementary, all
верифицированы: Khronos blog 2025-12-04, Phoronix Mesa 26.1 merge Jan 2026, Khronos
`VK_EXT_present_timing` proposal rev 3 2024-10-09, `VK_KHR_swapchain_maintenance1` ratified
2025-03-31, NVIDIA Wayland WSI busy-spin fix Apr 2026 + dev host driver 610.43.02 match,
`VK_KHR_present_wait2` rev 1, Mesa 26.2 direct-display benchmarks Jun 2026, Android docs
Jun 2026). **Dev host validation** via `vulkaninfo 2026-06-20`: все relevant extensions supported

+ features enabled — `VK_EXT_present_timing` rev 3 (`presentTiming` + `presentAtAbsoluteTime` +
  `presentAtRelativeTime` features = true), `VK_KHR_present_wait2` rev 1 (`presentWait2` = true),
  `VK_KHR_swapchain_maintenance1` rev 1 (`swapchainMaintenance1` = true), `VK_KHR_present_id/2`,
  `VK_KHR_present_mode_fifo_latest_ready`. **Refined hypothesis:** `VK_EXT_present_timing` (Nov 2025
  merge, Vulkan 1.4.335) — SOTA frame-pacing API; **NOT Vulkan 1.4 core** as original hypothesis
  thought — все 3 extensions are **device extensions**. Combined with `VK_KHR_present_wait2`
  (blocking wait без busy-spin) + `VK_KHR_swapchain_maintenance1` (per-present mode change без
  swapchain recreate, fix для `agent/decisions.md §30.3` RecreateSwapchain cycle) → детерминированный
  frame budget. Mesa 26.2 KHR_display direct-display benchmark: **~0.3 ms latency reduction, 5%
  power reduction, tighter variance** (0.9 ms → 0.3 ms std-dev). **Mixed потому что measured
  Wayland-specific p99 frame variance numbers отсутствуют** (Mesa benchmark на KHR_display
  direct-display, другие условия; Wayland compositor вносит дополнительный jitter). Intel Iris Xe
  **doesn't support** `present_wait` / `swapchain_maintenance1` — fallback path needed.
  **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1
  foundation (`PROJECTV_USE_PRESENT_TIMING=ON|OFF` env + per-feature detection в
  `TryPickPhysicalDevice`); Step 2 adoption (Mode C path с `desiredPresentTime` IPD calibration
  via `vkGetPastPresentationTimingEXT` feedback + `VkSwapchainPresentModeInfoKHR` per-present mode
  change + `VkSwapchainPresentFenceInfoKHR` race-free destroy); Step 3 default flip для hardware
  с `presentTiming + presentAtAbsoluteTime` features enabled. Foundation шаг = prerequisite для
  Stage 3.1 GPU Fluid CA cross-frame latency contract (per `agent/workspace.md §2` +
  `agent/decisions.md §30.4`). **Caveats:** (a) prototype deferred (analytical literature
  sufficient для integration recommendation); (b) cross-vendor = Mesa 26.1+ (Jan 2026), deployment
  lag 1-2 cycles; (c) AMD/Intel mainline re-test required (NVIDIA dev host only validated).
  **Operator override note (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20, пользователь дал
  инструкцию «выбирай незанятую тему, не work-stealing-job-system»; previous reservation
  `work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier this session) released back to
  `research/backlog.md §Open`. Fresh claim: `vulkan-fps-pacing-vk-ext`. Closed entry:
  `experiments/2026-06-20-vulkan-fps-pacing-vk-ext/`.

**RACE CONDITION CORRECTION (per `docs/experiments/AGENTS.md §13.3`):** Параллельный агент
misinterpreted operator instruction «выбирай не work-stealing-job-system» (в parallel session) как
«release the existing reservation». В реальности operator сказал parallel agent'у «выбери
другую тему для себя» (т.к. work-stealing-job-system уже был мной claim'нут в этой сессии через
first-write-wins). После operator override parallel agent взял vulkan-fps-pacing-vk-ext. Но
**мой work-stealing-job-system experiment уже был выполнен до override** — research/web-research/
prototype/results/writeup всё завершено. Per §13.3 first-write-wins, моя работа сохраняется

+ зафиксирована в §6 + §Closed separately. **Этот experiment re-recorded в §6**:
  `2026-06-20-work-stealing-job-system` (verdict=mixed, per `experiments/2026-06-20-work-stealing-job-system/`).

`2026-06-20` — closed `2026-06-20-work-stealing-job-system` (verdict=`mixed`). **Job-scheduling-ось**
experiment (Stage 4.1 dispatcher foundation + Stage 6.1 ECS multi-threading per `TODO.md`).
Web-research complete (4 batch queries, 25 sources верифицированы: P2300R10 2024-06-28,
P3826R3 2026-01, P3109R0 2024, LLVM Discourse 2025-06, NVIDIA/stdexec, BS::thread_pool v5.0.0
2024-12-20, Taskflow v3.10.0 2025-05 / v4.0.0 2026, oneTBB v2022.3.0 2025-10-29, Dispenso,
DagFlow, TooManyCooks, ptsouchlos/thread-pool benchmarks on Zen 3 5800X, arXiv 2407.15805).
Standalone C++26 prototype `prototype/bench.cpp` (6 файлов, ~750 LoC incl. vendored
`BS_thread_pool.hpp` v5.0.0 MIT). 2 implementations (custom simple std::thread pool + BS::thread_pool
work stealing) × 3 thread counts (1/4/16) × 4 workloads (256/1024/4096/16384 chunks) + serial
baseline = 24 configs × 30 iters = 720 measurements. **Surprising negative finding:**
**serial dispatcher — sweet spot для ProjectV mainline** (cache-fitting workload fits L3 32 MiB).
Work-stealing pool (BS::thread_pool) **проигрывает** simple pool'у для small tasks (BS 1t = 5-8×
slower than serial). Simple pool проигрывает serial для small workloads. SMT (16 threads)
**counter-productive** для cache-friendly workloads (simple 16t = 5.7× slower than serial;
BS 16t = 7.8× slower). p99 jitter: serial 1.0-1.2× mean, parallel 2-5× mean. **Per-stage split:**
❌ Stage 4.1 (4 KiB/chunk) = serial, ❌ Stage 3.1 (1-2 KiB/chunk) = serial, ⚠️ Stage 6.1 (ECS
per-system) = TBD separate experiment, ✅ Stage 4.3 (128+ chunks batch world gen) = re-evaluate.
**Mainline рекомендация:** не подключать thread pool / TBB / libdispatch / `std::execution`
по default. Per `legacy/docs/philosophy/01_foundation/05_decision-making.md» «if perf gain
< 5-10%, choose simple» — measured: pool overhead = 5-15× per-task compute = 12-37× waste.
Estimated mainline effort: **XS** (anti-pattern: «don't add pool по default»). Cross-axis
closure: today 12 experiments closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
landscape (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async + job-scheduling).
Re-evaluation triggers: Stage 6.1 Step 6 NUMA-aware, Stage 4.3 lift draw distance, AVX-512
hardware arrival (Zen 5), real perlin/SVDAG workload, `stdexec::static_thread_pool`
direct measurement when Clang 23+ + libc++ stable. Closed entry:
`experiments/2026-06-20-work-stealing-job-system/`.

`2026-06-20` — closed `2026-06-20-clustered-forward-mass-lights` (verdict=`yes`).
**Mass-lights architecture** experiment — единственная ось, не покрытая today-сессиями
(storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/gi-strategy + job-scheduling).
**Mainline baseline = single-light hard cap** per `src/shaders/voxel.frag:25-47` (`SceneLightingBuffer`
UBO содержит только 1 `localPointLight*` vec4 set, не массив). **Не масштабируется** на
`TODO.md §4.x` procedural (лава/факелы/магия) + `§5.1` VCT VPLs. Web-research complete
(~30 sources верифицированы: Harada 2012 Forward+ [теорема: обходит все deferred по memory
traffic], Olsson 2012 Clustered Shading [1M lights real-time, hierarchical assignment],
themaister 2020 Granite [subgroupMin/subgroupMax + subgroupOr production pattern],
logdahl 2025 [10k lights × 2800 clusters = 1.1 ms compacted на GTX 1070, 5× speedup vs naive],
WebGPU 2025 benchmarks [lu-m-dev: Forward+ holds 60 FPS до 1000 lights; Clustered Deferred
~3× faster on Sponza-like overdraw], Black_Key [3000 point lights на 2016 Intel IGPU
@ 30 FPS, voxel-specific], Vyatkin 2024 [voxelized scenes + VPL, 1024 VPL tested]). Standalone
CPU prototype `prototype/bench.cpp` (single file, ~480 LoC, Clang 22.1.6, `-O3 -march=native`)
**compiled clean** (`-Wall -Wextra` no warnings). 13 measurement configs: **3 grid
resolutions (8×4×12 coarse, 16×9×24 target, 32×18×64 fine) × sparse+dense scenarios ×
100-5000 lights** + adaptive iters (target ~5s per config, min 5, max 1000, warmup 10).
**Key CPU numbers (16×9×24 target, sparse scenario):** 100 lights = 1.4 ms mean, 1000 lights
= **12.7 ms mean / 15.3 ms p99** (avg 3.1 lights/cluster, max 34, 66% empty). **Dense scenario
(лава):** 16×9×24 / 1000 lights = 15.4 ms (avg 232, max 544, 22% empty). **CRITICAL: 16×9×24
/ 5000 dense lights = 124.5 ms, 69% clusters overflow soft cap 1024, max 2759** → soft cap
must be raised to ≥2048 OR light prioritization policy required. **Cross-validation с
published GPU numbers:** within 5-10× of logdahl 2025 (1.1 ms @ 10k×2800) и Harada 2012
(2 ms @ 3072 lights) — consistent с scalar→SIMT 50× speedup. **GPU projected cluster build:**
0.1-0.5 ms at 1000 lights (1.5-3% of 16.67 ms frame budget). **Per-fragment analytical model:**
Forward+ (10 lights/cluster avg) = 1000 ALU + 50 DDA reads per fragment = **100× speedup vs
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
**Cross-axis continuity:** 5 same-day `2026-06-20` sessions on lighting axis (vct-vs-rt-cutoff
mixed + this yes) + Stage 5 foundation complete (nanovdb-on-gpu yes + dec-pipelines-async-compute
yes). **12+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape**

+ mass-lights dimension added. Closed entry: `experiments/2026-06-20-clustered-forward-mass-lights/`.

## 9. Archive references

- `experiments/_TEMPLATE/README.md` — шаблон формата эксперимента.
- `benchmarks/methodology.md` — стандарт измерений.
- `AGENTS.md` — протокол.