# INDEX — `docs/experiments/`

Текущий снимок состояния. Долговечные правила — `AGENTS.md`. Канбан гипотез — `research/backlog.md`.

---

## 1. Now

Just-closed (this session, `2026-06-20`):

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
- `restir-gi-feasibility` (m, Stage 5.1/5.2) — после Stage 5.x baseline established.
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

- `2026-06-20-vulkan-fps-pacing-vk-ext` (status: `in-progress`). Claimed per
  `docs/experiments/AGENTS.md §13.1` on `2026-06-20` after parking
  `2026-06-20-meshing-algo-comparison` (operator override per §13.6: пользователь дал
  инструкцию «выбирай незанятую тему»). Anti-duplicate sentinel clean. **Frame-pacing-ось**,
  отсутствующая в closed today-сессиях (storage/sync/cull/binding/layout/meshing/hzb/gi/
  noise/ecs все closed либо parked). Vulkan 1.4 core extensions `VK_KHR_present_wait`
  (was KHR extension pre-1.4, core in 1.4) + `VK_KHR_swapchain_maintenance1` (core 1.4).
  Hypothesis: present_wait убирает busy-wait overhead vs current FIFO busy-wait path;
  expected < 1 ms p99 frame variance gain + reduced CPU spin time. Cross-vendor:
  NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Alchemist/Battlemage.
  Stage 0 architectural foundation для Stage 3.1 GPU Fluid CA cross-frame latency contract
  per `agent/workspace.md §2` + `dec-pipelines-async-compute` Step 1 prerequisite.
  Scope: `docs/experiments/experiments/2026-06-20-vulkan-fps-pacing-vk-ext/`. ETA: same session.

- `2026-06-20-work-stealing-job-system` (status: `in-progress`). Claimed per `docs/experiments/AGENTS.md §13.1`.
  Anti-duplicate sentinel clean. **Job-scheduling-ось** — единственный h/m-priority в backlog, ещё не покрытый
  today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy/profiling-overhead-async все закрыты). Foundation
  для Stage 4.1 (background world gen dispatcher) + Stage 6.1 (ECS multi-threading `ecs_set_target_fps` +
  multi-threaded `ecs_progress` per `TODO.md §6.1` Step 6 NUMA-aware). Refined hypothesis: **C++26 `std::execution`
  (P2300 senders/receivers)** даёт сопоставимый throughput с **BS::thread_pool** / Taskflow / std::thread pool на
  synthetic ProjectV chunk-generation workload (1024 chunks × 8³ voxels, 1M+ ops/sec/chunk) на Zen 3 8C/16T, при
  этом лучше composable для Stage 6.1 ECS (sender chains для `ecs_progress` hooks), кроссвендорно portable без
  vendor lock-in (TBB = Intel, libdispatch = Apple). Tier 4 R&D per `agent/knowledge.md §29.0` line 887
  («`std::execution` — нужна Job System, отдельный slice»). Scope: только
  `docs/experiments/experiments/2026-06-20-work-stealing-job-system/`. ETA: same session (one-shot).

## 6. Recent closed sessions

| Slug                                        | Status                  | Verdict                        | Closed     |
|:--------------------------------------------|:------------------------|:-------------------------------|:-----------|
| `2026-06-20-sparse-64-tree-alternatives`    | concluded-verdict-yes   | yes                            | 2026-06-20 |
| `2026-06-20-mesh-shader-vs-compute-cull`    | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-bindless-descriptor-overhead`   | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-cache-oblivious-chunk-tree`     | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-svdag-vs-vdb-memory-throughput` | concluded-verdict-yes   | yes                            | 2026-06-20 |
| `2026-06-20-dec-pipelines-async-compute`    | concluded-verdict-yes   | yes                            | 2026-06-20 |
| `2026-06-20-nanovdb-on-gpu`                 | concluded-verdict-yes   | yes                            | 2026-06-20 |
| `2026-06-20-hzb-binding-models`             | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-simd-procedural-noise`          | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-vct-vs-rt-cutoff`               | concluded-verdict-mixed | mixed                          | 2026-06-20 |
| `2026-06-20-flecs-soa-vs-aos-bench`         | concluded-verdict-yes   | yes                            | 2026-06-20 |
| `2026-06-20-async-compute-overhead-numbers` | concluded-verdict-yes   | **yes (+9.85-11.34% speedup)** | 2026-06-20 |

## 7. Backlog

См. `research/backlog.md`.

## 8. Last update

`2026-06-20` — closed `2026-06-20-flecs-soa-vs-aos-bench` (verdict=`yes`). ECS memory-layout-ось experiment
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

`2026-06-20` (this session) — `2026-06-20-meshing-algo-comparison` in-progress. Meshing-axis experiment
(unique h-priority slot после 8 закрытых same-day сессий на orthogonal axes:
storage/sync/cull/layout/binding/memory/hzb).
Web-research complete (8 sources across 2 batch queries); prototype scaffold + 6-synthetic-scene harness
drafted; next tick = compile + run. Cross-refs: `agent/knowledge.md §25` (greedy meshing contract, baseline),
`src/shaders/voxel_mesh.comp::GreedyFacePass` (562 lines, per-axis dispatch в 6 passes, current mainline),
`TODO.md §2.1` (mesh shader port, this informs choice) + `§3.3` (physics mesh, mirror choice).
Duplicate `meshing-algo-comparison` entry в `research/backlog.md §Open` (line 63) удалён в r1 sync —
line 40 остаётся канонической Open записью, line 97 — reservation в §In progress.

## 9. Archive references

- `experiments/_TEMPLATE/README.md` — шаблон формата эксперимента.
- `benchmarks/methodology.md` — стандарт измерений.
- `AGENTS.md` — протокол.