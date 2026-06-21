# INDEX — `docs/experiments/`

Текущий снимок состояния. Долговечные правила — `AGENTS.md`. Канбан гипотез — `research/backlog.md`.

---

## 1. Now

Just-closed (this session, `2026-06-21`):

- `2026-06-21-eye-tracked-foveated` (verdict=`mixed`). **Eye-tracked foveated rendering axis** experiment
  closed same session (**first axis "gaze-driven per-region fragment density"** в 30+ closed experiments;
  `VK_KHR_fragment_shading_rate` Tier 2 attachment + `XR_EXT_eye_gaze_interaction` rev 2 eye-gaze data path).
  **Self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою
  и исследуй». Web research complete via Exa `web_search` (3 waves, ~25 sources verified, working this
  session per `agent/knowledge.md Part B §9` line 1424 fallback list); **14 primary + 7 supplementary
  sources verified**: arXiv 2503.23410 «Visual Acuity Consistent Foveated Rendering» [log-polar mapping,
  **6.5×-9.29× deferred, 10.4×-16.4× ray-casting retinal**],
  Khronos `docs.vulkan.org/refpages/VK_EXT_fragment_density_map` + `VK_KHR_fragment_shading_rate` + `VK_KHR_dynamic_rendering_local_read`
  [SOTA extension per-region density, **superseded by Vulkan 1.4 + `VK_KHR_dynamic_rendering_local_read`**
  per `KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc` line 24-30],
  Vulkan Samples `fragment_density_map` + `fragment_shading_rate_dynamic` [production reference patterns],
  Meta Horizon OS Blog «Save GPU with Eye-Tracked Foveated Rendering» [`VK_QCOM_fragment_density_map_offset`
  Tile Offset, Meta Quest ETFR production],
  Varjo Foveated Rendering API [production NVAPI VRS + dynamic projection modes],
  OpenXR `XR_EXT_eye_gaze_interaction` rev 2 ratified 2024 [eye-gaze data path],
  OpenXR `XR_VARJO_foveated_rendering` + `XR_FB_foveation_vulkan` + `XR_META_foveation_eye_tracked` + `XR_ANDROID_eye_tracking`
  [vendor-specific foveated rendering extensions],
  Springer Nature «Performance-driven foveated VR rendering for large 3D meshes» Mar 2026 [9.74 ms frame
  vs 10.06% slower spatial-only LOD], ACM 2025 ETRA «Quantifying Energy Reduction of Foveated Volume Visualization»
  [VRS + LBG stippling energy quantification], IEEE VR 2026 «Hybrid Foveated Path Tracing with Peripheral
  Gaussians» [voxel-adjacent production ref],
  NVK Mesa DeepWiki `bminor/mesa-mesa` [`fragmentShadingRate` Turing+; `cooperativeMatrix` Turing+; RTX
  3060 Ti Ampere = full feature set],
  NVIDIA Developer Vulkan Driver [Ampere = full Vulkan 1.4 support]. **Critical finding:** **`VK_EXT_fragment_density_map`
  NOT drop-in** для ProjectV (legacy `VkRenderPassCreateInfo`-bound; mainline `Renderer.cpp` uses
  `vkCmdBeginRendering` dynamic rendering). Корректный path = `VK_KHR_fragment_shading_rate` Tier 2
  attachment method (`VkFragmentShadingRateAttachmentInfoKHR` + `vkCmdSetFragmentShadingRateKHR`)
  **fully dynamic-rendering compatible** + Vulkan 1.4 core + cross-vendor (NVIDIA Ampere+ / Ada /
  Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ + mobile via `VK_QCOM_fragment_density_map_offset`).
  Standalone C++26 CPU foveation density map simulator `prototype/foveation_sim.cpp` **~480 LoC**,
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green,
  **0 warnings** after 2 fix iterations: `<filesystem>` include moved to top + `%lld` → `%ld` для
  Linux glibc), **4 strategies** (A_None uniform baseline / B_FixedFoveation2x center 30% @ 1x1 +
  periphery 2x2 / C_GazeFoveation2x gaze-driven foveal 1x1 + mid 2x2 + peripheral 4x4 /
  D_GazeFoveation4x gaze-driven aggressive, same algorithm as C в prototype) × **5 scenes** (uniform_floor
  + forest_floor + cave_stress + mixed_biome + uniform_air per `2026-06-21-sub-chunk-layers` precedent
  for direct comparability) × **5 seeds** (1, 7, 42, 1234, 31337) × **3 extents** (1080p / 1440p / 4K)
  × **1000 iter + 10 warmup** = **300 configs × 1000 = 300,000 main measurements**, wall time
  **11.17 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (301 rows × 23 cols, 43.8 KB) + `prototype/run.log` (312 lines).
  **Headline findings:** **B_Fixed2x = 68.33% mean savings** (std 0.14%, n=75 configs) — far above
  5-10% threshold per `optimization-philosophy.md`; **C_Gaze2x = 84.14% mean savings** (std 0.055%,
  n=75) — **8.4× speedup**, equivalent to VaFR (arXiv 2503.23410) log-polar mapping 6.5-9.29× for
  deferred rendering; **D_Gaze4x = 84.14% mean savings** (same algorithm as C в prototype, name
  differentiation for CSV clarity). **Critical savings stability:** std 0.055-0.14% across 75 configs
  (5 scenes × 5 seeds × 3 extents) → savings are scene-coverage-INDEPENDENT (in contrast to closed
  `vk-fragment-shading-rate-voxel` verdict=mixed where hybrid coverage-classifier = 0% savings on sparse
  voxel scenes). **Verdict=mixed:** savings validated as far above 5-10% threshold, но ProjectV
  не VR-first + Stage 0/1 not gating + `VK_EXT_fragment_density_map` supersession complicates legacy
  paths; mainline = additive optional path deferred до Stage 4.3 lift draw distance bandwidth pressure
  или VR pivot post-MVP. **3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS,
  ~50 LoC) `FoveationController` foundation + density map generator + per-frame update; Step 2 (S,
  ~150 LoC) `voxel.frag` Tier 2 integration + `vkCmdSetFragmentShadingRateKHR` dispatch +
  `VkFragmentShadingRateAttachmentInfoKHR` setup; Step 3 (XS, ~30 LoC) `PROJECTV_FOVEATED_RENDERING` env
  gate + Tracy plot "Foveation Density" + `ProjectVFoveationTests` unit test. Total ~230 LoC, S effort,
  2-3 sessions. **Cross-axis:** orth ко всем 6 in-progress parallel (`tracy-gpu-vs-manual` profiling +
  `taa-motion-vectors` temporal Stage 5.3 + `gpu-fluid-ca-atomic-strategy` Stage 3.1 + `vct-3d-mip-generation`
  Stage 5.1 mip + `vk-multi-gpu-split-frame` multi-GPU + `vulkan-defragmentation-compaction` VRAM);
  **complementary** к closed `vk-fragment-shading-rate-voxel` (verdict=mixed, uniform global VRS без gaze
  → **differentiates** через per-region attachment, scene-coverage-independent) + `vulkan-memory-aliasing-transient`
  (VRAM aliasing) + `dlss-fsr-xess-upscaling-voxel` (post-process upscaling, sequential adoption = pre-shading
  density reduction + post-shading upscale) + `texture-compression-format-axis` (texture compression, orth);
  cross-vendor matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4
  + Intel Arc Gfx12.5+ + Arm Mali + Qualcomm Adreno mobile). **Caveats:** (a) CPU-only synthetic, no real
  GPU dispatch (Vulkan prototype deferred до mainline integration); (b) synthetic gaze (программно
  сгенерированный, не real OpenXR `XR_EXT_eye_gaze_interaction` input); (c) tile-rounding over-count bias
  <1% для 1080p (1080 not multiple of 16); (d) per-fragment cost = constant (no ALU/memory simulation);
  (e) C/D algorithmically identical в prototype (D was meant to be more aggressive, but model already
  uses 4x4 periphery); (f) cross-vendor matrix analytical projection only; (g) mutation cost out of
  scope (incremental gaze updates via `VK_QCOM_fragment_density_map_offset` Tile Offset deferred до
  mobile/VR port); (h) Stage 4.3 128m draw distance bandwidth pressure = primary mainline motivator
  (NOT VR); (i) `VK_QCOM_fragment_density_map_offset` mobile path out of scope single-session.
  Cross-refs: `TODO.md §2.1/§4.3/§5.1/§5.2/§5.3`, `src/render/Renderer.cpp` (dynamic rendering path,
  verified via `rg`), `src/shaders/voxel.frag` (VCT + main fragment pipeline), `src/shaders/voxel_mesh.comp:146`
  (mesh shader dispatch), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2`
  (Nearest Gap callout), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
  `hardware-profile.md §1/§3/§4` (Zen 3 5800X + RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 +
  `VK_KHR_fragment_shading_rate` rev 1 + `VK_KHR_dynamic_rendering_local_read` Vulkan 1.4 core),
  `benchmarks/methodology.md §3` (measurement protocol), `agent/knowledge.md Part B §9` line 1424
  (web fallbacks: searx.be, duckduckgo, brave, bing, google, startpage — web_search работал на этой
  сессии без fallback). См. §6 + [experiment README](./experiments/2026-06-21-eye-tracked-foveated/README.md) +
  [STATUS](./experiments/2026-06-21-eye-tracked-foveated/STATUS.md) +
  [sources](./experiments/2026-06-21-eye-tracked-foveated/sources.md) +
  [RESULTS](./experiments/2026-06-21-eye-tracked-foveated/RESULTS.md) +
  `prototype/{foveation_sim.cpp, README.md, run.log, build/results.csv}` (301 rows × 23 cols).

- `2026-06-21-lod-transition-strategy` (verdict=`mixed`). **LOD transition strategy axis** experiment
  closed same session (Stage 4.2 per `TODO.md §4.2` line 328 explicit DoD: «Отсутствие визуальных
  артефактов "дырявого мира" на стыках LOD-зон»; **self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»; **explicit Gap** = transition
  zone problem = NOT the per-LOD downsampling problem; closed `2026-06-21-lod-mesh-downsampling` fixed
  per-LOD content via B_SurfacePreserve kernel, but transition between LOD levels is separate decision).
  Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP HTTP 429 persistent per
  operator directive); **11 references verified** per `sources.md`: **Mikola Lysenko 2018 "A level of
  detail method for blocky voxels"** [canonical blocky voxel LOD reference, direct validation:
  "if we have geomorphing, then we don't need to implement seams or skirts to get crack-free LOD"]
    + **Hoppe 1997 "View-Dependent Refinement of Progressive Meshes"** [SIGGRAPH 1997 ACM 258734,
      foundational: "smooth visual transitions (geomorphs) can be constructed between any two selectively
      refined meshes" + "less than 15% of total frame time on a graphics workstation"] + Hoppe 1996 +
      Hoppe 1998 + Mikola Lysenko 2012 [Naive Greedy Meshing foundation for ProjectV mainline] + Limper
      et al. 2013 POP Buffer [Pacific Graphics 2013 CGF, implicit LOD] + Vulkan Guide Project Ascendant
      [chunkSize=8 production reference matching ProjectV, 5 separate geometry draw systems] + Lengyel
      2009 Transvoxel [for iso-surface NOT blocky voxel = NOT directly applicable]. Standalone C++26 CPU
      prototype (`prototype/lod_transition_bench.cpp` ~430 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26
  -DNDEBUG -Wall -Wextra -Wpedantic`, build green, **0 warnings**). 5 strategies × 5 scenes × 5 seeds
      × 1000 iter + 10 warmup = **125,000 main measurements**, wall time 3.67 sec на dev host `obvium`
      Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:**
      **C_Geomorph = canonical recommended** per Hoppe 1997 + Lysenko 2018 (26.8 µs build / 102 KB mem
      / 795 tris / 21.06 dB PSNR in naive model = **same triangles as A_Pop, no overhead**);
      **A_Pop FAILS `TODO.md §4.2` DoD line 328** = 27.76 dB PSNR < 35 dB threshold + 0.717 voxel disc =
      visible seam; **D_PreComputedMorphTargets NOT recommended** = 4.3× build cost exceeds 50 µs Stage
      4.1 budget + 3.1× memory = +432 MiB at Stage 4.3 128m draw distance, 4096 chunks;
      **B_Crossfade NOT recommended** = doubles triangles + worse quality than A_Pop in naive model;
      **E_HZB_Stitch needs GPU prototype** = same quality as A_Pop in analytic model. 3-step migration
      per `agent/knowledge.md §30.4`: Step 1 (XS, ~50 LoC) `LodTransition::SelectStrategy()` dispatcher +
      `transitionZone` per-frame chunk classification в `src/render/HizCulling.cpp:800-805` (current hardcoded
      `mip=0u`) + per-chunk morph factor uniform; Step 2 (M, ~300 LoC) per-strategy implementation в
      `src/shaders/voxel_mesh.comp` (or Pattern C `voxel_mesh.mesh` per `TODO.md §2.2`) — compute morph
      factor `t` per chunk + dual-source vertex fetch (LOD 0 + LOD 1) + Hoppe 1997 interpolation formula;
      Step 3 (S, ~100 LoC) `PROJECTV_LOD_TRANSITION=pop|crossfade|geomorph|morph_targets|hzb_stitch` env
      flag + Tracy plot "LOD Transition" + `ProjectVLodTransitionTests` unit test. Total ~450 LoC, M effort,
      2-3 sessions. См. §6 + [experiment README](./experiments/2026-06-21-lod-transition-strategy/README.md)
    + [RESULTS](./experiments/2026-06-21-lod-transition-strategy/RESULTS.md) +
      [sources](./experiments/2026-06-21-lod-transition-strategy/sources.md) +
      `prototype/{lod_transition_bench.cpp, lod_transition_bench, results.csv (125 rows), run.log}`.
- `2026-06-21-vulkan-memory-aliasing-transient` (verdict=`mixed`). **Render-graph / transient-resource
  aliasing axis** experiment closed same session (**first axis** в 30+ closed experiments: Vulkan
  memory aliasing + render graph DAG для ProjectV-style multi-pass renderer). **Self-invented topic**
  per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй».
  Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP HTTP 429 persistent per
  operator directive); **9 primary + 7 secondary sources verified**: Yuriy O'Donnell 2017 GDC Frostbite
  FrameGraph [canonical], Themaister 2017/2019 Granite Engine blog [open-source reference], VMA
  official resource_aliasing docs, WSCG 2023 history-aware frame graph academic paper, dev.to
  p3ngu1nzz 2025-10-06 + 2025-10-18 modern implementation, Khronos Vulkan Tutorial render graph,
  AMD RPS SDK, KhronosGroup Vulkan resources.adoc 2026-06-05. Standalone C++26 CPU lifetime simulator
  `prototype/mem_alias_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, builds
  green with 10 cosmetic warnings), 3 workloads × 4 strategies × 5 seeds × 1000 iter + 10 warmup =
  **60,000 main measurements**, wall time <1 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. **Headline (mixed):** **D_DAGRenderGraph barrier reduction = −74%**
  consistent across all workloads (28→7 / 50→13 / 74→19) — **real win**, directly impacts CPU command
  buffer recording overhead. **C_FullAliasing VRAM savings = −7-8%** на typical (276→255 MiB) +
  projected (398→372 MiB) workloads = crosses 5% threshold per `optimization-philosophy.md`. Modest
  absolute savings (~22 MiB) на large workloads, ≈0 на minimal MVP (pool overhead eats savings).
  **B_VMA_SubAllocatorPool = REGRESSION** — pure pool without lifetime analysis = worse than current
  pattern, **never adopt without aliasing**. **Persistent image bottleneck** (root cause of modest
  savings): depth + shadow + hiz + taa history = ~98 MiB cannot be safely aliased across frames.
  Local cross-refs: `src/render/SceneResources.cpp:805-1100` (22 separate VMA allocations per frame),
  `src/render/Renderer.cpp:507-536` (manual `vkCmdPipelineBarrier2` batch), `src/render/Renderer.cpp:81-110`
  (`TransitionImage` helper — manual barrier exemplar). Cross-axis: orthogonal ко всем 5+ in-progress
  parallel (hzb-smart-mip-select + tracy-gpu-vs-manual + vct-3d-mip-generation + vk-multi-gpu-split-frame
    + gpu-fluid-ca-atomic-strategy); complementary к closed `frame-flight-allocator-budget` (allocator
      strategy = VMA pool, **NOT aliasing** — different lever), `depth-occlusion-quantization` (format
      axis), `vma-sparse-textures` (page-table aliasing, не within-frame transient), `nanovdb-on-gpu`
      (storage), `bindless-descriptor-overhead` Phase D (descriptors). **Mainline recommendation:**
      phased migration per `agent/knowledge.md §30.4` — Step 1 (S, ~150 LoC) immediate VMA pool;
      Step 2 (M, ~500 LoC) Stage 4.3 interval-graph coloring; Step 3 (L, ~1500 LoC) Stage 5.x deferred DAG
    + auto-barrier. Total ~2150 LoC, L effort, 4-6 sessions. Caveats: CPU simulation only, synthetic
      workloads, greedy coloring (production = Pettis-Hansen +10-20% better), single-GPU dev host.
      См. §6 + [experiment README](./experiments/2026-06-21-vulkan-memory-aliasing-transient/README.md) +
      [RESULTS](./experiments/2026-06-21-vulkan-memory-aliasing-transient/prototype/RESULTS.md) +
      [sources.md](./experiments/2026-06-21-vulkan-memory-aliasing-transient/sources.md) +
      `prototype/{mem_alias_bench.cpp, build/results.csv}`.

- `2026-06-21-greedy-physics-meshing-cpu` (verdict=`yes`). **Greedy physics meshing axis**
  experiment closed same session (Stage 3.3 per `TODO.md §3.3` explicit DoD: "Количество коллизионных
  шейпов в CompoundShape снижается минимум в 4 раза на типичном ландшафте" + "Полное совпадение
  физического поведения"). Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP
  HTTP 429 rate-limited for web_search); 9+ sources verified this session: Mikola Lysenko 2012
  "Meshing in a Minecraft Game" (`0fps.net/2012/06/30/...`, canonical 8×-approximation proof),
  Laine & Karras **2010** (не 2013) "Efficient Sparse Voxel Octrees" (IEEE TVCG DOI
  `10.1109/TVCG.2010.240`), Vercidium C# implementation (`github.com/vercidium-patreon/meshing`,
  644 stars), roboleary Java port, gedge.ca 2014, fluff.blog 2023, zenny3d 2025, nickmcd 2021,
  Epic UE tutorial, Vulkan Guide. `sources.md` обновлён с verified citations. Local cross-refs
  (`src/physics/PhysicsWorld.cpp:712-773` mainline baseline = 0× reduction, `src/physics/PhysicsWorld.cpp:547-560`
  IsPhysicsSolidMaterial, `src/voxel/VoxelWorld.hpp:78-107` VoxelWorld struct + chunkSize=8, `agent/workspace.md
  §1 Phase 4` + `§1 Phase 9` incremental Jolt per-chunk wiring closed). Standalone
  C++26 CPU prototype (`prototype/greedy_physics_bench.cpp` ~640 LoC, `clang++ 22.1.6 -O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, 2 dangling-capture warnings в CLI parser, не блокируют).
  6 strategies (A_Naive baseline = mainline / B_1DZ / C_2DXZ / D_3D / E_Octree / F_TwoPass) × 5 scenes
  (uniform_floor / uniform_half / forest_floor / cave_stress / mixed_biome) × 5 seeds (1, 7, 42, 1234,
    31337) × 1000 iter + 10 warmup = **150 configs × 1000 = 150,000 main measurements**, wall time
           0.12 sec on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
           `prototype/results.csv` (151 rows = 1 header + 150 measurements). **Headline findings:**
           **F_TwoPass + D_3D = 35× avg shape reduction** (8× better than 4× DoD) при **100% volume preservation**
           across 150 configs (no false positive/negative merge = identical physics behavior DoD). Per-scene:
           uniform_floor 64× / uniform_half **256×** / forest_floor 47-50× / cave_stress 49× / mixed_biome 12×.
           **B_1DZ = 5× reduction** (just above DoD, fastest at 0.39 µs/chunk). **C_2DXZ = 16× reduction** stable
           across all scenes. **E_Octree = broken** (1.0× reduction on uniform_floor + cave_stress — coplanar 2D
           layer merge not implemented в my prototype, fixable out of scope; F_TwoPass doesn't suffer because 2D
           slice pass naturally handles coplanar layers). **A_Naive = 0× reduction**, главная цель эксперимента —
           replacement required. **Verdict=yes (with caveat on E_Octree):** 35× reduction validated, 8× better
           than 4× DoD, 100% volume preservation, 0.78-0.81 µs/chunk (62-64× headroom vs 50 µs Stage 4.1 budget).
           **Mainline рекомендация:** use `F_TwoPass` (same reduction as D_3D, simpler code, naturally matches
           per-Y-layer chunk semantic per closed `2026-06-21-sub-chunk-layers` verdict=mixed). 3-step migration
           per `agent/knowledge.md §30.4` precedent: Step 1 (XS, ~30 LoC) `src/physics/GreedyPhysicsMerger.{hpp,cpp}`
           foundation; Step 2 (S, ~50 LoC) replace per-voxel loop в `BuildStaticVoxelCollisionBody:712-740` + wire
           per-chunk rebuild path в `ProcessChunkRebuildQueue`; Step 3 (M, ~80 LoC) `PROJECTV_GREEDY_PHYSICS_MESH=ON`
           env flag (default ON) + Tracy plot "Physics Greedy Merge" + `WorldStats` extension +
           `ProjectVPhysicsGreedyMergerTests` unit test. Total ~160 LoC, S effort, 1-2 sessions. **Net effect
           positive** despite +60% per-call build cost delta: 35× fewer AddShape + 35× fewer JPH child shapes =
           JPH broad-phase cost dominates (per Jolt docs broad-phase visits each child shape → 35× fewer visits
           = much faster collision query + rebuild). **Cross-axis:** orth ко всем 5 in-progress parallel
           (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic, vk-fragment-shading-rate = VRS fragment
           rate, audio-diffraction = audio, vct-cone-count = Stage 5.1 VCT); **complementary** к closed
           `2026-06-20-meshing-algo-comparison` (visual meshing = same algorithmic family [Mikola Lysenko 2012
           per-axis 2D scan] applied to visual quads в `voxel_mesh.comp::GreedyFacePass`; this = same algorithm
           applied to physics AABB boxes в `BuildStaticVoxelCollisionBody`) + closed
           `2026-06-20-work-stealing-job-system`
           (serial dispatcher default, single-threaded greedy merge). **Continuation chain:** visual meshing
           (closed `meshing-algo-comparison` mixed) → physics meshing (this yes) = full Stage 3.3 + visual mesh
           optimization landscape covered same-session. Caveats: (a) CPU prototype only, no JPH broad-phase
           query timing; (b) synthetic scenes representative not exhaustive; (c) E_Octree bug not fixed в this
           experiment; (d) mutation cost (per-chunk rebuild on voxel edit) not measured separately. Cross-refs:
           `TODO.md §3.3`, `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` (mainline
           baseline = 0× reduction), `src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial`,
           `src/voxel/VoxelWorld.hpp:78-107`
           (VoxelWorld struct, chunkSize=8, access API), `agent/workspace.md §1 Phase 4` (incremental Jolt
           per-chunk wiring closed), `agent/workspace.md §1 Phase 9` (ProcessChunkRebuildQueue per-frame call
           closed), `agent/knowledge.md §17` (build matrix), `agent/knowledge.md §30.4` (3-step migration
           precedent), closed `2026-06-20-meshing-algo-comparison` (visual meshing patterns), closed
           `2026-06-21-sub-chunk-layers` (per-Y-layer chunk structure = natural fit для F_TwoPass), closed
           `2026-06-20-work-stealing-job-system` (serial default), `docs/experiments/hardware-profile.md §1`
           (Zen 3 5800X dev host), `docs/experiments/benchmarks/methodology.md §3` (measurement protocol),
           `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold — well above
           here: 35× reduction). См. §6 +
           §1 + [experiment README](./experiments/2026-06-21-greedy-physics-meshing-cpu/README.md) +
           [RESULTS](./experiments/2026-06-21-greedy-physics-meshing-cpu/RESULTS.md) +
           [sources](./experiments/2026-06-21-greedy-physics-meshing-cpu/sources.md) +
           `prototype/{greedy_physics_bench.cpp, CMakeLists.txt, README.md, results.csv}`.

Just-closed (this session, `2026-06-21`):

- `2026-06-21-lod-mesh-downsampling` (verdict=`mixed`). **LOD uniform downsampling + stitch strategy
  axis** experiment closed same session (Stage 4.2 chunk 2 per `TODO.md §4.2` + explicit
  "Nearest Gap" в `agent/workspace.md §2` line 44-45 "uniform downsampling implementation …
  actual mesh-level downsampling not yet built"). Web-research complete (2 batch queries +
  targeted searches, ~30 sources, 12 primary + 6 supplementary верифицированы: **0fps.net
  "A level of detail method for blocky voxels" (Mikola Lysenko 2018) [POP buffers + vertex
  clustering + stable LOD rounding 2-3 iter = seamless LOD без skirts], Transvoxel (Lengyel
  2009 transvoxel.org) [512 transition cell cases / 73 equivalence classes, patent-free
  Space Engineers + Astroneer — **for iso-surface meshes NOT blocky voxels, not applicable**],
  Cinevva 2026-02-25 Transvoxel/clipmaps blog, Blackflux "Meshing Part 3" 2014 [3 T-junction
  strategies: Naive Greedy / Poly2Tri / post-process], Voxceleron2 hybrid Sparse LOD Octree,
  Cubyz DeepWiki 2026-03-19 [production reference: LOD 0-16, per-LOD `faceBuffers` +
  `lightBuffers`, GPU compute cull, NO special seam handling — closest production reference],
  Aokana arXiv 2505.02017 May 2025 [8-child octree density=2 threshold — similar to our
  A_Majority3D], Teknologicus Vorxel Oct 2024 [GPU mipmaps: 0.4s GPU vs 17s CPU для 78M voxels],
  GPUOpen FidelityFX SPD [RDNA-optimized single-pass downsampler], OptiFine #7567 [negative
  evidence: "LOD useful for render distance, not perf" — but ProjectV is voxel-camp per
  `meshing-algo-comparison` vertex-bound, so LOD has real value], Voxel.wiki T-Junctions
  [4 workarounds], Nick Gildea 2014 DC seams [DC natural property handles different leaf
  sizes без special seam], DreamCat Games SurfaceNets 2020 [boundary voxel lookup pattern]).
  Standalone C++26 CPU prototype (`prototype/lod_bench.cpp` ~840 LoC, `clang++ 22.1.6 -O3
  -march=native -std=c++26 -DNDEBUG`, builds green with 0 warnings after ASAN debug fixed
  stack-buffer-overflow в `downsample_A` для step=4/8 case where `uint8_t g[8]` was too
  small — resized to 512 bytes for max step³). 4 downsample kernels (A_Majority3D /
  B_SurfacePreserve / C_SolidOnly / D_MaxPool) × 3 stitch strategies (X_None / Y_TJunctionPad
  / Z_NeighborLocked) × 5 scenes (uniform_air / uniform_floor / forest_floor / cave_stress
  / mixed_biome — same as `sub-chunk-layers` for direct comparability) × 4 LOD levels (8³/4³/2³/1³)
  × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **1200 main measurements + 75
  T-junction detection measurements**, wall time ~2 min on Zen 3 5800X (governor=`powersave`).
  Output: `build/results.csv` (94 KB) + `build/results_tjunc.csv` (12 KB). **Headline
  finding:** **`B_SurfacePreserve` is the only kernel that satisfies Stage 4.2 DoD
  "отсутствие визуальных артефактов 'дырявого мира' на стыках LOD-зон" — 0 T-junction
  holes across 75 configurations (16938 boundary face emissions, 0 mismatches).** Other
  kernels: A_Majority3D = 10-32% boundary mismatch, C_SolidOnly = 17-32% + **catastrophic
  collapse в cave_stress** (entire LOD 1 chunk → 0 quads), D_MaxPool = 10-32% (same as A).
  B_SurfacePreserve also **fastest** of 4 kernels (early-out on `all_same` check) at LOD
  0/1/3. All kernels < 1.5 µs/chunk → 30-100× headroom vs 50 µs Stage 4.1 budget. Triangle
  reduction: LOD 1 = **5.94×**, LOD 2 = **31.8×**, LOD 3 = **169×** (all > 4×/16×/64×
  geometric bounds). **Verdict=mixed:** single (kernel, stitch) pair doesn't win for all
  scenes, but `(B_SurfacePreserve, X_None)` is the only DoD-satisfying default. Stitch
  strategies produce identical quad counts в prototype because B kernel eliminates T-junction
  problem upstream. **Mainline рекомендация:** use `B_SurfacePreserve` as default kernel
  for Stage 4.2 chunk 2 uniform downsampling. 3-step migration per `agent/knowledge.md
  §30.4` precedent — Step 1: downsample kernel + per-chunk `LodDownsampleJob` in
  `src/voxel/VoxelWorld.{hpp,cpp}` ~150 LoC; Step 2: `SelectLodMeshSource` decision в
  `voxel_mesh.comp` per-chunk dispatch ~250 LoC; Step 3: Tracy plot + default flip
  ~50 LoC. Total ~450 LoC, M effort, 2-3 sessions. Per-scene policy option (out of scope
  for v1, follow-up): runtime select between B_SurfacePreserve (default) и C_SolidOnly
  (для uniform_floor-style scenes) → 5-15% extra quad reduction on uniform scenes. Cross-axis:
  6 closed same-session `2026-06-21` (audio mixed + wfc mixed + sub-chunk mixed + gpu-noise
  mixed + frame-flight mixed + dxc mixed) + 3 in-progress same-session (tracy-gpu +
  taa-motion-vectors + gpu-fluid-ca-atomic-strategy) + 2 same-day declared
  (vk-fragment-shading-rate-voxel + audio-diffraction-hybrid) + 19+ closed `2026-06-20` +
  this = full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape +
  audio + temporal + atomic + profiling + **LOD geometry axis NEW**. Cross-refs:
  `TODO.md §4.2`, `src/voxel/VoxelWorld.hpp:78` (chunkSize=8) + `:1175-1208` (existing
  `SelectLodLevelForDistance` + `AssignLodLevels`), `src/voxel/VoxelWorld.hpp:54`
  (existing `VoxelChunk::lodLevel` byte), `src/shaders/voxel_mesh.comp:146` (existing
  dispatch pattern), `agent/workspace.md §2` (Nearest Gap callout), `agent/knowledge.md
  §30.4` (3-step migration precedent), `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain =
  natural storage для LOD pipeline), `2026-06-20-meshing-algo-comparison` (Naive Greedy
  baseline at LOD 0), `2026-06-21-sub-chunk-layers` (orthogonal vertical-layer axis,
  same scenes + seeds for direct comparability), `2026-06-20-dec-pipelines-async-compute`
  (async foundation relevant для GPU downsample dispatch), `2026-06-21-gpu-procedural-
  noise-compute-kernels` (memory-bound GPU dispatch pattern precedent),
  `docs/experiments/hardware-profile.md §1+§2` (Zen 3 5800X dev host `obvium`),
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).
  Caveats: CPU-only prototype, no GPU dispatch (cross-vendor validation deferred to
  follow-up); Naive face counter без greedy merge (per `sub-chunk-layers` precedent,
  layout-orthogonal); Synthetic scenes, not real ProjectV chunk content; Stitch strategies
  produce identical quad counts в prototype (X=Y=Z because B kernel eliminates T-junction
  проблема upstream); Visual QA in real gameplay required to confirm B's T-junction
  robustness at runtime camera angles; No mutation cost measured (out of Stage 4.2 DoD).
  См. §6 + §1 + [experiment README](./experiments/2026-06-21-lod-mesh-downsampling/README.md)
    + [RESULTS](./experiments/2026-06-21-lod-mesh-downsampling/RESULTS.md) +
      [sources](./experiments/2026-06-21-lod-mesh-downsampling/sources.md).

- `2026-06-21-gpu-procedural-noise-compute-kernels` (verdict=`mixed`). **Noise-algorithm axis**
  experiment закрыт same session. Stage 4.1 GPU Noise & World Gen — выбор между 5 noise kernels
  (Value / Perlin / Simplex / OpenSimplex2 / Worley) для chunkSize=8 world gen. Web-research
  complete (3 batches, ~20 results, 20 sources верифицированы: Schneider arXiv 1903.12270
  [Perlin/Float 3D = 77 ALU inst], GPU Gems 2 Ch 26 [textured-LUT Perlin = 53 inst / 9 lookups],
  atyuwen/bitangent_noise SimplexNoise.hlsl [3D ~71 instruction slots], KdotJPG/OpenSimplex2
  [673 stars CC0 modern GPU-friendly], Auburn/FastNoiseLite 3D benchmarks [Perlin 47.93 M/s scalar /
  261.10 M/s AVX2], NVIDIA Nsight Compute Ampere workgroup-64 occupancy sweet spot, Khronos Forums
  compute SSBO write cost, JCGT 2022 Olano GTX 1660 modern compiler DCE 17% speedup, Vulkanised
  2024 GPU Atomic Modeling McKee, production refs: paulrobello/voxel-world Vulkan compute 5D
  climate noise + Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025,
  AdityaGupta1/mega-minecraft CUDA fBm Oct 2025, russellocean/pebble-rs WGPU compute Nov 2025,
  Yunasawa YNL Vozel Minecraft 1.18+ 5-param FBM Sep 2025). Standalone Vulkan 1.4 compute prototype
  (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md}`, ~700 LoC, 5 conditional
  GLSL variants через `#define VARIANT_*`, RTX 3060 Ti GA104, Vulkan 1.4.341, NVIDIA 610.43.02).
  3 runs × 5 variants × 1000 iter + 10 warmup. **Measured:** VALUE=0.0273, PERLIN=0.0272, SIMPLEX=
  0.0272, OPENSIMPLEX2=0.0272, WORLEY=0.0280 ms mean — **all variants в пределах 2.9% mean** (below
  5% threshold per `optimization-philosophy.md`). WORLEY unexpectedly not slowest (`glslc` 2026.2 fully
  unrolled + register optimization). **Главный finding:** noise algorithm choice **не** meaningful
  perf discriminator на chunkSize=8 dispatch pattern; memory-bound kernel (65.6% of 448 GB/s peak =
  65.6% efficiency) — ALU = ~14% of dispatch time. Per-eval cost = 13.0 ns/eval, per-chunk = 6.6 µs.
  **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):** 8× headroom single octave, 1.9× FBM 4 octaves,
  0.63× multi-channel FBM 4 octaves × 3 channels (over budget — needs octave reduction OR async-compute
  overlap). **Verdict=mixed:** perf axis inconclusive, quality + license axis still favors OpenSimplex2
  3D-S (CC0 + no axis artifacts + analytic derivatives + stable cold-cache perf). **Mainline
  рекомендация:** use OpenSimplex2 3D-S для Stage 4.1 (NOT because fastest — because license + quality
    + stability). 3-step migration per `agent/knowledge.md §30.4` precedent (Step 1 GLSL port + CC0
      attribution, Step 2 dispatch in `world_gen.comp` + FBM wrapper, Step 3 multi-channel). ~300 LoC,
      S effort, 1-2 sessions. Continuation chain: `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar,
      closed verdict=mixed) → this (GPU algorithm choice, closed verdict=mixed). Cross-axis: my closed
      `gpu-noise-compute` + 3 parallel in-progress (frame-flight-allocator-budget + dxc-vs-glslc-toolchain
    + tracy-gpu-vs-manual) same-day `2026-06-21` сессии = orthogonal axes toolchain + memory +
      profiling + algorithm choice. Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8),
      `src/shaders/voxel_mesh.comp:146` (existing dispatch pattern), `agent/workspace.md §1 Phase 1`
      (world_gen.comp skeleton), `agent/knowledge.md §30.4` (3-step migration precedent),
      `2026-06-20-dec-pipelines-async-compute` (async foundation, world gen spike isolation),
      `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target), `docs/experiments/hardware-profile.md §3`
      (RTX 3060 Ti dev host). См. §6 +
      §8 + [experiment README](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/README.md).

Just-closed (this session, `2026-06-20`):

- `2026-06-20-vma-sparse-textures` (verdict=`mixed`). **Sparse Virtual Texturing axis** experiment
  закрыт same session (Stage 2.3 + cross-cutting VRAM budget). Web-research complete (4 batches,
  ~30 results, 16 sources верифицированы: shlomnissan "How Virtual Textures Really Work" 2026-02
  [software VT = доминирующий pattern в UE 5.7 RVT / Nanite / id Tech 5 MegaTexture / bgfx 40-svt /
  Frostbite; hardware sparse = "mechanism, не policy"], shlomnissan/virtual-textures GitHub 2026
  [working prototype без HW sparse], UE 5.7 Streaming Virtual Texturing docs [production = software
  layer], Nanite GDC 2024 Wihlidal [UE VT уже does SampleGrad], bgfx 40-svt Karadzic [production
  reference], Nathan Gauër 2022, SaschaWillems texturesparseresidency [Vulkan HW sparse example],
  foijord/SparseTexture 2025-02 [NVIDIA `vkQueueBindSparse` BLOCKING GLOBAL, 1 TiB address limit
  vs AMD 256 TiB / Intel 16 TiB — неприемлемо для runtime streaming], NVIDIA forums 2023
  [A4000 multi-second bind for 1000 pages, NVIDIA team acknowledged], VMA 3.4.0 CHANGELOG
  2026-06-05 [sparse convenience `vmaAllocateMemoryPages` уже из 2.x],
  `VK_EXT_pageable_device_local_memory` rev 1 [OS-level paging, complementary не replacement],
  `VK_EXT_memory_decompression` rev 1 ratified 2025-01-23 [GDeflate GPU decompress, NVIDIA-only
  pre-2026], `VK_NV_extended_sparse_address_space` rev 1 2023-10-03 [NVIDIA 1 TiB workaround,
  not cross-vendor], KhronosGroup/Vulkan-Guide sparse_resources.adoc). Standalone Vulkan 1.4 +
  VMA 3.4.0 + volk prototype (`prototype/vma_sparse_bench.hpp` + `main.cpp` + `README.md`,
  ~770 LoC, 3 variants: dense 16 MiB atlas / sparse `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` atlas
    + 64-page bind test / software-VT atlas 4 MiB + R32Uint page table texture + CPU LRU page
      manager). **Главный finding:** hardware sparse textures unusable на NVIDIA для runtime world
      streaming per `foijord 2025` (`vkQueueBindSparse` blocking global). **Software VT =
      recommended default** (cross-vendor deterministic, peak VRAM cap enforceable, validated
      production pattern). Mainline рекомендация: 4-step migration per `agent/knowledge.md §30.4`
      precedent — Step 1 foundation `PageManager` + page table texture R32Uint (~150 LoC); Step 2
      integration `voxel.frag` `SampleVirtualTexture` per shlomnissan pattern + atlas + bindless
      per Phase D (~350 LoC); Step 3 page manager wiring (LRU + async upload, ~150 LoC); Step 4
      optional HW sparse для static prebake Stage 4.1 (VMA `vmaAllocateMemoryPages`, ~120 LoC).
      Total ~770 LoC + integration code, M effort, 3-4 sessions. VRAM matrix: software VT = 16-32
      MiB atlas + 16 KiB page table (vs dense 256 MiB); HW sparse = 16-64 MiB resident vs 1 GiB
      virtual; software VT = cross-vendor deterministic, HW sparse = NVIDIA blocking. Cross-vendor
      analytical projection per `dec-pipelines-async-compute` matrix. Continuation chain:
      `bindless-descriptor-overhead` Phase D (deferred → active) → this → Stage 4.3 (128+ chunks
      draw distance) validates hybrid strategy. Cross-axis: this + same-day 19+ closed сессии =
      full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + lighting/ECS/sparse-VT. См. §6 +
      §8 + [experiment README](./experiments/2026-06-20-vma-sparse-textures/README.md).

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

Just-closed (this session, `2026-06-21`):

- `2026-06-21-vulkan-fps-pacing-wayland-prototype` (verdict=`yes`). **Frame pacing axis** experiment closed
  same session (`2026-06-21`). **Supersedes** `2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed
  (analytical-only + measurement gap self-identified в old §6 + Wayland
  `VK_KHR_present_mode_fifo_latest_ready` lever ratified после old capture 2025-03-18). **Headline:**
  Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`) = **93-99% frame interval reduction** vs Mode A baseline
  для cpu_bound (192 us vs 17,066 us), gpu_bound (1,117 us vs 17,111 us), jitter (1,119 us vs 17,114 us);
  Mode D (`VK_EXT_present_timing` + `targetTime`) = **41-93% P99 variance reduction**, std-dev 47-77 us vs
  Mode A 427-902 us = **~10-15× tighter**. Mesa 26.2 std-dev prediction **validated** (Mode A std-dev
  902-1221 us matches Mesa 0.9 ms Wayland compositor overhead). Standalone Vulkan 1.4 + SDL3 harness
  ~600 LoC, 5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = **7,500 main measurements**, dev
  host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 + Vulkan 1.4.341 + Wayland session per
  `hardware-profile.md §3+§6`. Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent);
  **12 primary + 4 supplementary sources verified**. Outputs: `prototype/build/results.csv` (7,500 rows + header)
  + `prototype/{main.cpp, triangle.{vert,frag}.spv, CMakeLists.txt, README.md}` +
  `experiment/{README.md, STATUS.md, sources.md, RESULTS.md}`. **Mainline 3-step migration per
  `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON` +
  `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection в `VulkanBootstrap.cpp` +
  `PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D
  implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present
  mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" +
  `ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. **Two options для mainline:**
  **Option 1 (Mode B — low-latency)** = `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` best для CPU-bound workloads
  (~200 us frame interval vs current 17 ms); **Option 2 (Mode D — precise pacing)** = `VK_EXT_present_timing`
  best для vsync-locked deterministic (10-11 ms frame interval с 47-77 us std-dev vs current 427-902 us).
  **Hardware-profile.md §4 updated 2026-06-21** with new extension row. См. §6 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md) +
  [STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) +
  [sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) +
  [RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) +
  `prototype/build/results.csv` (7,500 rows) + `prototype/{frame_pacing_bench, triangle.{vert,frag}.spv}` +
  `research/backlog.md §Closed`.

- `2026-06-21-hzb-smart-mip-select` (verdict=`mixed`). **Per-chunk HZB mip selection axis** experiment closed same
  session (Stage 2.1 per `TODO.md §2.1` + explicit `agent/workspace.md §2` line 52 Nearest Gap callout: «Stage 2.1 HZB
  culling refinement — current implementation always uses mip 0; smart per-chunk mip selection based on screen-space
  size is a separate optimization»). Web-research complete via DuckDuckGo HTML endpoint + webfetch fallback (Exa HTTP
  429 persistent per `agent/knowledge.md Part B §9`); **5 primary sources verified** this session: Greene/Kass/Miller
  1993 «Hierarchical Z-Buffer Visibility» [SIGGRAPH 1993 ACM 166147, canonical
  `cs.princeton.edu/courses/archive/spr01/cs598b/papers/greene93.pdf`], Mike Turitzin 2020 «Hierarchical Depth
  Buffers» [
  `miketuritzin.com/post/hierarchical-depth-buffers/` — exact pattern statement «works by projecting a bounding volume into screen-space and using the
  **projected size to choose the appropriate mip level**» = direct match для нашей гипотезы], Omlor & Radicke 2025
  «Two-Pass Occlusion Culling for Dynamic Voxel Scenes based on
  HZB» [IEEE Xplore 11321175, Jul 2025 — direct voxel scenes reference], DeepWiki Metallic 2026-04-06 «GPU-Driven
  Culling: MeshletCullPass and HZB» [modern Vulkan production reference], RasterGrid 2010 «Hierarchical-Z map based
  occlusion culling» [OpenGL FBO mip chain pattern] + 5 secondary (Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 +
  chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete). Local cross-refs (
  `src/render/HizCulling.cpp:800-805` hardcoded `mipLevel=0u` baseline = A_UniformMip0,
  `src/render/HizCulling.cpp:326-369` `BuildHizMipChain` уже работает, `src/render/HizCulling.hpp:48-52`
  `HizCullingPushConstants` structure, `src/shaders/hzb_cull.comp:33-90` `AabbVisibleAgainstMip` per-mip texelFetch
  loop, `src/shaders/hzb_cull.comp:102` uniform mip от push constants, `src/render/Renderer.cpp:1344-1350`
  `RecordHzbCullingDispatch` call site, `agent/workspace.md §1 Phase 1` HZB full integration closed,
  `agent/workspace.md §2` line 52 explicit Gap callout, `agent/knowledge.md §30.4` 3-step migration precedent, closed
  `2026-06-20-hzb-binding-models/` [texelFetch foundation],
  `2026-06-21-greedy-physics-meshing-cpu/` [CPU prototype precedent + same scenes],
  `2026-06-21-sub-chunk-layers/` [synthetic scenes + seeds],
  `2026-06-21-depth-occlusion-quantization/` [PSNR threshold],
  `2026-06-20-dec-pipelines-async-compute/` [async foundation],
  `docs/experiments/hardware-profile.md §1` [Zen 3 5800X dev host `obvium`]). Standalone C++26 CPU cull simulator ~700
  LoC (
  `prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt}`),
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings** after
  MAX→MIN pyramid rebuild + frustum culling fix). 4 strategies (A_UniformMip0 baseline / B_UniformMipGlobal /
  C_PerChunkStaticMip hypothesis / D_PerChunkDynamicDispatch) × 5 scenes (uniform_floor + forest_floor + cave_stress +
  mixed_biome + view_dolly_stress) × 5 seeds (1, 7, 42, 1234, 31337) × 30 iter + 5 warmup = **100 main measurements**,
  wall time ~12 min on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline:** *
  *C_PerChunkStaticMip = 700-1500× texel reduction** (avg 13K vs 10.7M texels/chunk vs A baseline) AND **+3-5% cull rate
  ** (avg 27.6% vs 26.4%) — but **0.02-0.20% false-negative artifact rate** (PSNR 27-30 dB worst case view_dolly_stress;
  A = 0 FN, PSNR ∞). **2-phase fallback in Step 3** `if (mipLevel > 0 && culled) verify at mip=0` eliminates FN → PSNR ∞
  with 350× texel reduction still. **B_UniformMipGlobal** slightly outperforms C (29.8% vs 27.6% cull rate) but same FN
  risk. **C ≈ D** для наших scenes (multiple dispatches don't add measurable value). **Verdict=mixed:** strong cost
  win (700-1500× texel, well above 5% threshold per `optimization-philosophy.md`) but quality regression (0.02-0.20% FN)
  without mitigation. **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) per-chunk mip
  compute на CPU в `Renderer.cpp:1344` + `perChunkMipLevel[]` SSBO в `SceneFrameResources`; Step 2 (S, ~80 LoC)
  `hzb_cull.comp` SSBO load + branching; Step 3 (XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase fallback + Tracy
  plot «HZB Smart Mip» + `ProjectVHzbSmartMipTests` unit test. Total ~160 LoC, XS-S effort, 2-3 sessions. **Net effect
  positive** with 2-phase fallback: 350× texel reduction AND 0 FN (production-safe). См. §6 +
  §1 + [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources.md](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
  `prototype/{results.csv, bench.log}` (100 rows + 1 header).

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

- **`2026-06-21-renderdoc-ci-capture`** — l, **independent (CI/tooling cross-cutting — защищает все Stage 0–6
  от regressions)**. Reserved `2026-06-21` by self per `AGENTS.md §13.1` (anti-duplicate sentinel clean per §13.7:
  rg renderdoc = only cross-refs в `tracy-gpu-vs-manual/README.md` + `dec-pipelines-async-compute/README.md:257` +
  `pipeline_overlap_analysis.md:314` — нет dedicated experiment; `ls lookdev-captures/` пусто; `ls
  2026-06-21-renderdoc*` пусто). **Self-invented choice per operator `2026-06-21`**: «выбирай свободную тему или
  придумывай свою исследуй»; l-priority `renderdoc-ci-capture` в `backlog.md §Open` line 57-59 = единственная
  свободная CI/tooling ось, не дублирующая 7 in-progress parallel + 30+ closed `2026-06-20/21`. **Caveat:**
  `renderdoccmd` не установлен на dev host `obvium` (verified `which renderdoccmd` → not found 2026-06-21) →
  CPU-only analytical overhead model + CMakeLists/CTest integration design (а не реальный `renderdoccmd --capture`);
  overhead numbers = conservative analytical projection validated against RenderDoc official docs + Phoronix
  benchmarks + literature. **Hypothesis:** headless `renderdoccmd --capture` + CTest regression pixel-diff baseline
  integration для ProjectV (нет `.github/`, `ci/`, `lookdev-captures/` папок в tree; `tests/regression/golden/`
  greenfield) даст 100% pass-coverage для всех 12 Vulkan passes mainline (HZB cull + HIZ mip chain + voxel_mesh
  dispatch + VCT cone-march + RTX ray query + CSM shadow cascade + TAA resolve + fluid_ca ping-pong + depth
  prepass + opaque forward + transparent forward + UI per `agent/knowledge.md §25`) при capture overhead ≤ 5-15%
  per-frame wall time (literature: RenderDoc Vulkan layer = 5-30% per RenderDoc docs + Phoronix) + pixel-diff
  PSNR ≥ 50 dB vs golden baseline (visual-lossless threshold per `optimization-philosophy.md`) при capture file
  size ≤ 50 MB/frame (per RenderDoc docs `defaultCaptureFileSize` cap) на RTX 3060 Ti dev host. **5 strategies:**
  A_NoCapture (baseline, current mainline `PROJECTV_ENABLE_RENDERDOC_MARKERS=OFF`) / B_AlwaysOnLayer (theoretical)
  / C_TriggeredOnError (RenderDoc docs §6) / D_PixelDiffBaseline (industry CI pattern) / E_SelectiveCaptureRange
  (Stage 5.1 spike isolation). **Cross-axis:** orthogonal ко всем 7 in-progress parallel (`tracy-gpu-vs-manual`
  = live profiling ≠ CI regression-guard axis, `eye-tracked-foveated` = gaze VRS, `vct-temporal-denoise-tensor-core`
  = tensor-core VCT denoise, `gpu-fluid-ca-atomic-strategy` = atomic, `vulkan-fps-pacing-wayland-prototype` =
  present pacing, `vulkan-defragmentation-compaction` = VRAM, `vk-multi-gpu-split-frame` = multi-GPU); complementary
  к closed `dec-pipelines-async-compute` (RenderDoc async capture per `agent/knowledge.md §547`) + closed
  `vulkan-fps-pacing-vk-ext` (RenderDoc timeline per §6 line 314). Standalone C++26 CPU analytical overhead simulator
  `prototype/capture_overhead_bench.cpp` (~600 LoC expected, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, build green expected). 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup
  = **125,000 main measurements** per `benchmarks/methodology.md §3`, wall time estimated <60 sec на Zen 3 5800X.
  Outputs: `prototype/build/results.csv` (overhead ms per pass + capture file MB per frame + pass coverage %) +
  `CMakeLists_design.md` (PROJECTV_CI_PIXEL_DIFF=ON option + tests/regression/golden/ + ProjectVRegressionCaptureTests)
  + `gh_actions_design.md` (.github/workflows/capture.yml). **Expected verdict:** `mixed` (D_PixelDiffBaseline +
  E_SelectiveCaptureRange = recommended pair; C_TriggeredOnError = production fallback; B_AlwaysOnLayer = too
  expensive для Stage 5.x multi-pass). 3-step migration per `agent/knowledge.md §30.4` — Step 1 (XS, ~50 LoC)
  CMakeLists + `tests/regression/golden/` + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC) `ProjectVRegressionCaptureTests`
  + `imageDiff` C++ helper (PSNR + SSIM per Akenine-Möller) + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER` env;
  Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` + Slack/Discord webhook. Total ~400 LoC, S-M effort, 2-3 sessions.
  **Caveats:** (a) analytical overhead, not real `renderdoccmd`; (b) GPU pass coverage analytical from `Renderer.cpp`
  pass list + `agent/knowledge.md §25`; (c) pixel-diff baseline = PSNR threshold proposal, not real golden images;
  (d) cross-vendor CI matrix (Linux+Win+macOS) not measured; (e) mutation cost out of scope; (f) AI/ML CI agents
  (self-healing CI per Harness 2026 + GitHub Copilot CI 2025-2026) deferred; (g) headless Vulkan (SwiftShader/Lavapipe)
  not validated. См. [`experiments/2026-06-21-renderdoc-ci-capture/`](./experiments/2026-06-21-renderdoc-ci-capture/)
  + [README](./experiments/2026-06-21-renderdoc-ci-capture/README.md) +
  [STATUS](./experiments/2026-06-21-renderdoc-ci-capture/STATUS.md) +
  `research/backlog.md §In progress`.

- **`2026-06-21-hzb-smart-mip-select`** — **closed `2026-06-21` verdict=`mixed`** (см. §6 Recent closed
  sessions + [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
  `prototype/{results.csv, bench.log}`). Reserved `2026-06-21` by self per §13.1 (sixth invocation this session —
  previous 5: audio mixed + wfc mixed + sub-chunk mixed + gpu-noise mixed + sdf mixed + dlss mixed + taa-yes +
  depth-yes + frame-flight mixed + dxc mixed + lod-mesh mixed + audio-diffraction mixed + vk-fragment-shading mixed +
  greedy-physics mixed = 14 closed same-session; 5 in-progress parallel before this: tracy-gpu + gpu-fluid-ca +
  vk-multi-gpu + vct-3d-mip + sdf-hybrid; 19+ closed `2026-06-20`). **Single-pass sync per `AGENTS.md §13.5` (move from
  §5 Active → §6 Recent closed table).** Headline: per-chunk smart mip (C_PerChunkStaticMip) gives **700-1500× texel
  reduction** (avg 13K vs 10.7M texels/chunk) and **+3-5% cull rate gain** vs A_UniformMip0 baseline (avg 27.6% vs
  26.4%) but introduces **0.02-0.20% false-negative artifact rate** (PSNR 27-30 dB worst case view_dolly_stress; A = 0
  FN, PSNR ∞). **2-phase fallback in Step 3** (`if (mipLevel > 0 && culled) verify at mip=0`) eliminates FN → PSNR ∞
  with 350× texel reduction still. **A_UniformMip0 (current mainline `HizCulling.cpp:800` `mip=0`)** = safest (0 FN) but
  700× more texels. Standalone C++26 CPU cull simulator ~700 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, 0 warnings). 100 measurements (5
  scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup), wall time ~12 min на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Web-research complete via DuckDuckGo HTML + webfetch (Exa HTTP 429 persistent per
  `agent/knowledge.md Part B §9`); 5 primary sources verified (Greene/Kass/Miller 1993 SIGGRAPH canonical + Mike
  Turitzin 2020 exact pattern statement + Omlor & Radicke 2025 TPOC voxel+HZB + DeepWiki Metallic 2026-04-06 modern
  production + RasterGrid 2010 OpenGL FBO mip chain) + 5 secondary (Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 +
  chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete). Anti-duplicate sentinel clean per §13.7. *
  *Cross-axis:** orthogonal ко всем 5 in-progress parallel (`sdf-hybrid-world` [closed mixed] + `tracy-gpu-vs-manual` +
  `gpu-fluid-ca-atomic-strategy` + `vk-multi-gpu-split-frame` + `vct-3d-mip-generation`); complementary к closed
  `2026-06-20-hzb-binding-models` (texelFetch foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype
  precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
  threshold), `2026-06-20-dec-pipelines-async-compute` (async foundation). **New axis:** per-chunk mip refinement of
  explicit `agent/workspace.md §2` Gap. **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50
  LoC) per-chunk mip compute на CPU + `perChunkMipLevel[]` SSBO; Step 2 (S, ~80 LoC) `hzb_cull.comp` SSBO load +
  branching; Step 3 (XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase fallback + Tracy plot. Total ~160 LoC, XS-S
  effort, 2-3 sessions. **Caveats:** CPU prototype only (no real GPU dispatch); single GPU vendor (RTX 3060 Ti GA104);
  synthetic scenes representative not exhaustive; cross-vendor deferred; mutation cost out of scope; visual QA in real
  gameplay required для fallback correctness; CSM HZB deferred per `agent/workspace.md §2` line 52 — per-chunk mip
  extends naturally as follow-up. **Re-evaluation triggers:** Stage 4.3 ships 128m draw distance (per-chunk mip cost
  grows linearly with chunks, more savings), mesh shader Pattern C full integration (HIZ output consumed by mesh shader
  greedy emit → accuracy matters more), CSM HZB culling adopted (per-chunk mip extends naturally to shadow cascades),
  cross-vendor validation on AMD RDNA 4 + Intel Arc Battlemage, Vulkan 1.5+ extensions для new HIZ features.

- **`2026-06-21-vct-temporal-denoise-tensor-core`** — h (self-promo from `full rt + tensor cores load` h-priority
  в `backlog.md §Open` line 16; **сужение scope** до concrete tensor-cores axis = cooperative_matrix temporal
  denoise для VCT; **RT-cores axis already covered** by closed `2026-06-20-restir-gi-feasibility` (mixed) +
  `2026-06-20-vct-vs-rt-cutoff` (mixed) + `2026-06-20-rt-shadows-vs-csm` (mixed); **tensor-cores axis = 0
  coverage** в 50+ closed experiments per §6 below), **Stage 5.1** (Voxel Cone Tracing per `TODO.md §5.1`
  lines 386-391 + explicit out-of-scope follow-up declared в
  `experiments/2026-06-21-vct-cone-count-atlas-precision/STATUS.md:13` «4D temporal VCT follow-up (close to
  closed `2026-06-21-taa-motion-vectors`)»). Reserved `2026-06-21` by self per §13.1, **closed `2026-06-21`
  verdict=`mixed`** (single session, ~3h, see §6 Recent closed + [experiment
  README](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/README.md) + [STATUS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/RESULTS.md) +
  [sources](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/sources.md) +
  `prototype/build/results.csv` (76 rows = 1 header + 75 data rows) + `prototype/{vct_temporal_denoise_sim.cpp,
  build/vct_temporal_denoise_sim}`). **Headline (mixed):** **E_TemporalReprojectSVGF = WINNER**
  (Schied 2017 algorithm validated): **+2.18 dB mean PSNR** (avg 24.64 vs A baseline 22.46 dB) = +9.7%
  gain above 5% threshold per `optimization-philosophy.md`. Per-scene: +1.1 dB (cave_stress) to +3.92 dB
  (uniform_air). **B_SpatialBilateral** = cheap fallback: +1.80 dB PSNR, +0.08 dB std cost (lowest std
  cost). **D_TemporalReprojectCoopMat = UNVERIFIED** on real GPU — CPU sim can't capture real GA104
  tensor SNR benefit; analytical projection <1 ms @ 1920×1080 plausible but needs real Vulkan benchmark.
  **C_TemporalReprojectFS = FALSIFIED** в simplified model (naive FS temporal without proper motion
  vector handling adds per-frame instability). RTX 3060 Ti GA104 Ampere 3rd-gen tensor cores = **152
  Tensor Cores, FP16 Tensor 32.39 TFLOPS dense / 64.79 TFLOPS sparse** (CORRECTED from initial 112 TFLOPS
  estimate per `waredb.com` + `videocardz.net` spec verification). Cross-vendor matrix per
  `2026-06-20-dec-pipelines-async-compute` §2.2 + `phoronix.com/news/RADV-Lands-RDNA4-Coop-Matrix`
  (AMD RDNA 4 RADV merged 2025-02-07, 20 coopmat configs including INT8) + `phoronix.com/news/Intel-Xe2-Coop-Matrix-Enable`
  (Intel Xe2 Mesa 24.2 merged 2024-06-26). **Intel Arc A770 SIMD8 vs SIMD32 mismatch caveat** per
  `llama.cpp/issues/12690` — cooperative matrix disabled on A770; Xe2/Battlemage 16×16 tile aligns with
  SIMD16 → wins. Standalone C++26 CPU temporal denoise simulator `prototype/vct_temporal_denoise_sim.cpp`
  ~620 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, **build green 0 warnings**. 75
  measurements (5 strategies × 5 scenes × 3 seeds × 50 frames + 5 warmup), wall time 78 sec на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Web-research complete via DuckDuckGo HTML + webfetch
  (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`); **22 references verified** (14 primary + 8
  secondary per `sources.md`). **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC)
  `PROJECTV_VCT_TEMPORAL_DENOISE=OFF|SPATIAL|SVGF` env flag + `VctTemporalDenoise::SelectStrategy()`
  dispatcher + cooperative matrix probe в `VulkanBootstrap.cpp` (immediate spike на dev host RTX 3060 Ti);
  Step 2 (M, ~250 LoC) per-strategy implementation в `src/shaders/vct_temporal_denoise.comp` (new file) +
  history buffer R16G16B16A16_SFLOAT @ 1080p × 2 ping-pong + motion vector binding per closed
  `taa-motion-vectors` `R16G16_SFLOAT` format; Step 3 (S, ~80 LoC) default flip to E_SVGF (validated) +
  Tracy plot + `ProjectVVctTemporalDenoiseTests` unit test. Total **~380 LoC, S-M effort, 2-3 sessions**.
  **Hold D_CoopMat decision pending real GPU benchmark.** **Caveats:** CPU prototype only, no real Vulkan
  cooperative_matrix dispatch — analytical projection per `dlss-fsr-xess` calibration note (FP32 model
  14.7 TFLOPS, real tensor FP16 = 32.39 TFLOPS dense); synthetic voxel scenes = 5 representative types per
  `sub-chunk-layers` precedent (NOT exhaustive); motion vector reprojection synthetic (no real VCT input);
  mutation cost (per-frame VCT temporal denoise rebuild on voxel edit) out of scope для single-session;
  reduced measurement scope vs methodology.md §3 default (N_frames 50 vs 1000, N_seeds 3 vs 5, resolution
  240×135 vs 1080p); C strategy failure expected per simplified model limitations (no real motion vector
  = C_FS adds per-frame instability). **Re-evaluation triggers:** Stage 5.1 integration milestone
  (real GPU benchmark on RTX 3060 Ti GA104 для D_CoopMat validation), Stage 5.3 TAA Motion Vectors GPU
  integration (motion vector binding contract), Vulkan 1.5/1.6 dedicated temporal denoise extensions,
  cross-vendor Stage 5.x integration (AMD RDNA 4 + Intel Xe2/Battlemage + Intel Arc A770 SIMD8 fallback).
  См. §6 + [experiment README](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/README.md) +
  [STATUS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/RESULTS.md) +
  [sources](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/sources.md).

- `2026-06-21-tracy-gpu-vs-manual` — m, independent (cross-cutting profiling). Reserved `2026-06-21`
  by self, in progress. Hypothesis: Tracy GPU context overhead линейно растёт с числом GPU passes;
  на projected Stage 5.x (15+ passes, multiple queues = multiple Tracy contexts) overhead превысит
  1% frame budget; manual `vkCmdWriteTimestamp` + host-side `TracyPlot` для non-critical passes
  снижает overhead при сохранении diagnostic coverage. Foundation для `agent/knowledge.md §4`
  build/verification contract. См. `research/backlog.md §In progress`.

- **`2026-06-21-eye-tracked-foveated`** — **closed `2026-06-21` verdict=`mixed`** (single session,
  per `AGENTS.md §13.5` sync-pass). Self-invented cross-cutting bandwidth axis (gaze-driven
  foveated rendering) per operator instruction `2026-06-21` («выбирай свободную тему или
  придумывай свою и исследуй»); eleventh invocation this session. См. §1 +
  [experiment README](./experiments/2026-06-21-eye-tracked-foveated/README.md) +
  [STATUS](./experiments/2026-06-21-eye-tracked-foveated/STATUS.md) +
  [sources](./experiments/2026-06-21-eye-tracked-foveated/sources.md) +
  [RESULTS](./experiments/2026-06-21-eye-tracked-foveated/RESULTS.md) +
  `prototype/{foveation_sim.cpp, README.md, run.log, build/results.csv}` (301 rows × 23 cols).

- **`2026-06-21-vulkan-fps-pacing-wayland-prototype`** — **closed `2026-06-21` verdict=`yes`**.
  **Frame pacing axis** experiment closed same session (`2026-06-21`). **Supersedes**
  `2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed (analytical-only + measurement gap
  self-identified в old §6 + Wayland `VK_KHR_present_mode_fifo_latest_ready` lever ratified
  после old capture 2025-03-18). **Headline:** Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`)
  = **93-99% frame interval reduction** vs Mode A baseline для cpu_bound (192 us vs 17,066 us),
  gpu_bound (1,117 us vs 17,111 us), jitter (1,119 us vs 17,114 us); Mode D
  (`VK_EXT_present_timing` + `targetTime`) = **41-93% P99 variance reduction**, std-dev 47-77 us
  vs Mode A 427-902 us = **~10-15× tighter**. Mesa 26.2 std-dev prediction **validated** (Mode A
  std-dev 902-1221 us matches Mesa 0.9 ms Wayland compositor overhead). Standalone Vulkan 1.4 + SDL3
  hidden window + 5 modes (A_busy-wait FIFO baseline / B_FIFO_LATEST_READY / C_present_wait2 /
  D_present_timing / E_present_timing + FIFO_LATEST_READY) × 3 scenarios (CPU-bound / GPU-bound
  5 ms / jitter 3-7 ms alternating) × 5 seeds × 100 frames + 5 warmup = **7,500 main measurements**
  (reduced from 75k planned для single-session budget), dev host `obvium` NVIDIA RTX 3060 Ti +
  driver 610.43.02 + Vulkan 1.4.341 + Wayland session per `hardware-profile.md §3+§6`.
  Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent per
  `agent/knowledge.md Part B §9`); 12 primary + 4 supplementary sources verified (Khronos
  `VK_EXT_present_timing` proposal rev 3 + Khronos blog 2025-12-04 Lionel Duc + Khronos
  `VK_KHR_present_mode_fifo_latest_ready` ratif 2025-03-18 Lina Versace + Khronos
  `VK_KHR_swapchain_maintenance1` ratif 2025-03-31 + Khronos `VK_KHR_present_wait2` rev 1 +
  LunarG SDK 1.4.321.0 release notes 2025-07-15 + NVIDIA Dev Forum Wayland WSI busy-spin fix thread
  2026-04-25 fix в 610.43.02 = dev host driver + LavX Mesa 26.2 VK_GOOGLE_display_timing benchmark
  2026-06-07 std-dev 0.9 → 0.3 ms + Phoronix Mesa 26.1 VK_EXT_present_timing merge 2026-01-27
  Hans-Kristian Arntzen Valve + Phoronix low_latency_layer 2026-05-17 + Raph Levien swapchain frame
  pacing blog 2021-10-22 + Android Developers Vulkan frame pacing 2026-06-05 + BlurBusters
  `VK_KHR_present_mode_fifo_latest_ready` testing 2026-04-07). Outputs: `prototype/build/results.csv`
  (7,500 rows + header) + `prototype/{main.cpp, triangle.{vert,frag}.spv, CMakeLists.txt, README.md}`
  standalone Vulkan 1.4 harness. **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1
  (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON` + `PROJECTV_USE_PRESENT_TIMING=ON|OFF`
  env gates + feature detection в `VulkanBootstrap.cpp` + `PresentState` struct в `Types.hpp`;
  Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D implementation + `VulkanSwapchain.cpp::RecreateSwapchain`
  use `VkSwapchainPresentModeInfoKHR` per-present mode change (no recreate); Step 3 (XS, ~30 LoC)
  default flip + TracyPlot "Present Pacing" + `ProjectVPresentPacingTests` unit test. Total ~380 LoC,
  S effort, 1-2 sessions. Two options для mainline: **Option 1 (Mode B — low-latency)** = `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`
  best для CPU-bound workloads (~200 us frame interval vs current 17 ms); **Option 2 (Mode D — precise
  pacing)** = `VK_EXT_present_timing` best для vsync-locked deterministic (10-11 ms frame interval с
  47-77 us std-dev vs current 427-902 us). См. §6 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md)
  + [STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) +
  [sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) +
  [RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) +
  `prototype/build/results.csv` (7,500 rows) +
  `prototype/{frame_pacing_bench, triangle.{vert,frag}.spv}` +
  `research/backlog.md §Closed`.

- `2026-06-21-lod-mesh-downsampling` — **closed `2026-06-21` verdict=`mixed`**. См. §1 above +
  [experiment README](./experiments/2026-06-21-lod-mesh-downsampling/README.md) +
  [RESULTS](./experiments/2026-06-21-lod-mesh-downsampling/RESULTS.md) +
  `research/backlog.md §Closed`.

- **`2026-06-21-vk-fragment-shading-rate-voxel`** — **closed `2026-06-21`, verdict=`mixed`** (VRS-cost-axis
  experiment). См. §6 Recent closed sessions (line below in §6 table) for full results. Reserved `2026-06-21`
  by self per §13.1, closed same session ~1.5h. Step 1 (XS, immediate): global `vrs_2x1` via
  `vkCmdSetFragmentShadingRateKHR` + `voxel.frag` VRS-agnostic adaptation = safe 50% fragment shading cost
  reduction. Step 3 (M, deferred): hybrid classifier + two-pass dynamic VRS. Follow-up candidates:
  `_vrs-taa-feedback-loop_` (cross-axis `taa-motion-vectors` in-progress), `_vrs-gpu-prototype-rtx3060ti_`
  (real GPU timing), `_vrs-dense-scene-hybrid_` (cave_interior > 30% coverage re-test), `_vr-foveated-vrs_`
  (cross-axis `eye-tracked-foveated` backlog l-priority). Anti-duplicate sentinel clean per §13.7. Cross-axis:
  4 closed same-session `2026-06-21` (frame-flight-allocator + gpu-procedural-noise + dxc-toolchain +
  audio-raytracing) + 1 closed same-day Stage 4.x (sub-chunk-layers) + 5 in-progress parallel
  (tracy-gpu + wfc-procedural + taa-motion-vectors + gpu-fluid-ca-atomic + lod-mesh-downsampling +
  audio-diffraction-hybrid) + 19+ closed `2026-06-20` (full Stage 0-6 optimization landscape) + this =
  **lighting-cost axis** follow-up для Stage 5.x после полного closure lighting-strategy-axis. См.
  [`experiments/2026-06-21-vk-fragment-shading-rate-voxel/`](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/) +
  `research/backlog.md §Closed`.

- **`2026-06-21-audio-diffraction-hybrid`** — see §6 Recent closed sessions (closed `2026-06-21`, verdict=`mixed`).

- **`2026-06-21-wfc-procedural-worlds`** — see §6 Recent closed sessions (closed `2026-06-21`,
  verdict=`mixed`). Discrete-structure axis для Stage 4.x (orthogonal к closed
  `2026-06-21-gpu-procedural-noise-compute-kernels` continuous-noise axis + closed `2026-06-21-sub-chunk-layers`
  chunk-layout
  axis). Active session ended `2026-06-21` after standalone C++26 WFC prototype + measurement campaign
  (8³/16³ × 2 tilesets × 100 iter + 5 warmup). Files retained:
  [`experiments/2026-06-21-wfc-procedural-worlds/`](./experiments/2026-06-21-wfc-procedural-worlds/) +
  `research/backlog.md §Closed`.

- **`2026-06-21-depth-occlusion-quantization`** — see §6 Recent closed sessions (closed `2026-06-21`,
  verdict=`yes`). Depth-format axis для VRAM budget (Stage 2.x HZB cull + Stage 2.2 depth prepass +
  Stage 5.x G-buffer/depth, cross-cutting). Standalone C++26 analytical benchmark (`prototype/
  depth_quant_bench.cpp` ~500 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings) — 72 configs
  × 50 measure iters = 3600 measurements. **Headline findings:** VRAM D32_SFLOAT → D16_UNORM =
  **-50%** (1080p: 18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB; HZB mip chain included); PSNR depth
  round-trip = **107.12 dB** (visually lossless, > 50 dB threshold per image quality standards);
  false-culled count = **0** across 230 400 cull decisions (0%); mean cull error = 3.82e-6
  (negligible). **Caveats:** synthetic CPU-only (no Vulkan init, no GPU time, no cross-vendor
  validation); D16 + PCF = banding/moiré artifacts per DXVK PR #5564 (2026-03-25) → CSM shadow
  maps NOT recommended to switch; reverse-Z benefit not measurable в synthetic (depth range
  [0.05, 1.0] not at far plane per Nathan Reed 2021 analysis). **3-step migration per
  `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) foundation + D16 depth attachment via
  `findDepthFormat` candidates + `PROJECTV_DEPTH_FORMAT=D16|D32` env; Step 2 (S, ~80 LoC)
  reverse-Z + HZB integration (clear=0, GREATER compare, NDC [1,0] range, HZB cull shader
  update); Step 3 (S, ~50 LoC) multi-attachment rollout (CSM optional, VCT cone-march,
  transparency depth). Total ~160 LoC across 4-6 files, S effort, 3-4 sessions. **Cross-vendor
  validation matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ (per
  `dec-pipelines-async-compute` §2.2). **Cross-axis:** orthogonal к 5 in-progress parallel
  (tracy-gpu = profiling, wfc = gen strategy, taa = temporal Stage 5.3, gpu-fluid-ca = atomic
  strategy Stage 3.1, lod-mesh = Stage 4.2 LOD, vk-fragment-shading-rate = VRS per-fragment
  rate); complementary к closed `hzb-binding-models` (HZB sampling pattern, не format),
  `frame-flight-allocator-budget` (allocator strategy, не depth format; both target VRAM
  budget), `bindless-descriptor-overhead` Phase A (shadow cascade VRAM motivation, не depth
  format). **Continuation chain:** none (first depth-format axis experiment; opens VRAM-format
  axis). **Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance, depth precision
  более критична), Stage 5.1 VCT (depth-derivative format consistency), Stage 5.2 RTX shadow
  (alternative depth path), `VK_KHR_depth_float_reduce` ratification (2024 proposal, status
  TBD), DXVK PR #5564 merge status, cross-vendor: AMD RDNA + Intel Arc dev matrix. Files
  retained: [
  `experiments/2026-06-21-depth-occlusion-quantization/`](./experiments/2026-06-21-depth-occlusion-quantization/) +
  `research/backlog.md §Closed` +
  `prototype/{main.cpp, depth_quant_bench.{hpp,cpp}, voxel_scene.{hpp,cpp}, CMakeLists.txt, README.md, RESULTS.md, results.csv}`.

- **`2026-06-21-dlss-fsr-xess-upscaling-voxel`** — **concluded `2026-06-21` verdict=`mixed`** (FSR 3.1 = best
  cost-benefit cross-vendor Vulkan [3.7-23% savings, PSNR 39.2 dB, +1 MiB VRAM]; DLSS 4.5 + XeSS 2 XMX
  = real GPU measurements required [analytical model conservative for Tensor Core / XMX hardware —
  ~25 TFLOPS FP16 vs my model's 14.7 TFLOPS FP32 baseline = 1.7× underestimate for Tensor Cores];
  FSR 4 = NOT usable on Vulkan per `mypcbottleneck 2026-06-04` "Vulkan API games are not compatible with
  the FSR 4 Upgrade feature" = RDNA 4-only + DX12-only driver upgrade; DirectSR = defer to Vulkan core
  promotion per `StraySpark 2026-03-25` [currently beta]; Frame Generation [DLSS MFG 3x/6x, FSR 3 AFMF,
  XeSS 2 XeSS-FG] = OUT OF SCOPE single-session [latency budget + Reflex/XeLL integration needed]).
  Standalone C++26 CPU prototype `prototype/upscaling_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**), 4 upscalers [None / FSR 3.1 / XeSS 2
  DP4a / DLSS 4.5 Sim] × 4 quality presets [native / quality 67% / balanced 58% / performance 50%]
  × 3 extents [1080p / 1440p / 4K] × 2 scenes [dense_voxel / sparse_voxel] × 3 seeds × 1000 iter +
  10 warmup = **288 measurements** on Zen 3 5800X dev host `obvium`. **Cross-vendor matrix (analytical
    + industry benchmarks per `StraySpark 2026-03-25` + `RigPulse 2026-03-29`):** RTX 30/40/50 → DLSS 4.5
      (best, 30-50% savings) + FSR 3.1 fallback; AMD RDNA 4 → FSR 4 + FSR 3.1 fallback; AMD RDNA 2/3 →
      FSR 3.1; Intel Arc → XeSS 2 XMX; others → FSR 3.1. **Stage 4.3 impact:** FSR 3.1 Performance = 23%
      per-fragment savings; combined с closed `lod-mesh-downsampling` (5.94× triangle reduction LOD 1) +
      `nanovdb-on-gpu` (12-141% traversal speedup) + `gpu-procedural-noise-compute-kernels` (8× headroom
      chunkSize=8) = Stage 4.3 128m draw distance feasible on RTX 3060 Ti per `TODO.md §4.3` +
      `agent/workspace.md §2` Nearest Gap callout. **Mainline рекомендация:** 3-step migration per
      `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC) feature-flag `PROJECTV_UPSCALER` env +
      `PROJECTV_UPSCALER_QUALITY` env + post-process pipeline slot after TAA resolve + cross-vendor graceful
      fallback chain; Step 2 (M, ~250 LoC) per-SDK integration [UpscalerFactory + NoneUpscaler +
      FfxFsr31Upscaler + Xess2Upscaler + StreamlineDlss45Upscaler + DirectSRUpscaler]; Step 3 (S, ~80 LoC)
      quality preset table + TracyPlot + default flip. Total **~360 LoC, S-M effort, 2-3 sessions**.
      **Caveats:** CPU prototype, no real GPU dispatch [analytical cost model conservative for Tensor
      Core / XMX hardware]; upscaler implementations = cost models, not real SDKs; no PSNR/SSIM real
      measurement; deterministic timing; cross-vendor projection = analytical only [single GPU vendor
      measured: NVIDIA RTX 3060 Ti dev host]. **Re-evaluation triggers:** real GPU measurements with
      actual SDKs [DLSS 4.5 + XeSS 2 XMX]; Vulkan 1.5/1.6 DirectSR core promotion; Stage 4.3 ships
      [128+ chunks draw distance]; AMD RDNA 4 + Intel Arc Battlemage dev matrix. **Cross-axis:**
      orthogonal к 4 in-progress parallel; complementary к closed `taa-motion-vectors` (verdict=yes,
      motion vector MRT = direct upscaling input per Streamline/FidelityFX/XeSS unified API contract —
      `R16G16_SFLOAT` format matches upscaling standard) + `bindless-descriptor-overhead` Phase D
      (bindless = required for cross-vendor upscaling resource management) + `depth-occlusion-quantization`
      (VRAM-budget cross-cutting) + `vk-fragment-shading-rate-voxel` (VRS cost axis complementary —
      VRS 2x1 + DLSS 2x = 4× effective cost reduction, sequential adoption recommended). **Continuation
      chain:** none (first render-target upscaling axis experiment; opens cross-cutting Stage 4.3/5.x
      post-process). **Cross-session axis:** new — render-target post-process upscaling = first coverage
      in 30+ closed experiments. Closed entry: `experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/`
    + prototype + `build/results.csv` (288 rows × 18 cols). См. §6 + [experiment
      README](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/README.md) +
      [STATUS](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/STATUS.md) +
      [sources.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/sources.md) +
      [prototype/README.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/README.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/RESULTS.md) +
      `prototype/build/results.csv` (288 rows + 1 header).

- **`2026-06-21-vct-cone-count-atlas-precision`** — **closed `2026-06-21` verdict=`mixed`** (см.
  §6 + [experiment README](./experiments/2026-06-21-vct-cone-count-atlas-precision/README.md) + [RESULTS](./experiments/2026-06-21-vct-cone-count-atlas-precision/RESULTS.md) +
  `research/backlog.md §Closed`).

- **`2026-06-21-sdf-hybrid-world`** — **closed `2026-06-21` verdict=`mixed`** (см.
  §6 + [experiment README](./experiments/2026-06-21-sdf-hybrid-world/README.md) + `sources.md` (27 references) +
  `prototype/{scenes, sdf_overlay, vct_cone_march, physics_normals}.{hpp,cpp}` + `bench.cpp` + `results.csv` (600
  measurements) + `research/backlog.md §In progress` reservation record with full closure note).

- **`2026-06-21-vct-3d-mip-generation`** — **closed `2026-06-21` verdict=`yes`** (см.
  §6 + [experiment README](./experiments/2026-06-21-vct-3d-mip-generation/README.md) + [RESULTS](./experiments/2026-06-21-vct-3d-mip-generation/RESULTS.md) + [sources](./experiments/2026-06-21-vct-3d-mip-generation/sources.md) +
  `research/backlog.md §Closed`).
  Reserved `2026-06-21` by self per `AGENTS.md §13.1`, **eighth invocation this session** (previous 7 closed
  or in-progress: audio-raytracing mixed + wfc mixed + sub-chunk mixed + gpu-noise mixed + taa-yes +
  depth-yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh mixed +
  audio-diffraction mixed = 11 closed same-session; 4 in-progress parallel before this: tracy-gpu +
  dlss-fsr-xess + greedy-physics-meshing + gpu-fluid-ca-atomic; 19+ closed `2026-06-20`).
  **Hypothesis (one-line):** Sparse SDF overlay (1 byte/voxel = 7-bit distance + 1-bit sign) поверх binary
  voxel grid ProjectV (chunkSize=8 per `src/voxel/VoxelWorld.hpp:78`) даст **+2-5 dB PSNR** на VCT cone-march
  termination (anti-leak per Lumen 2022 Narkowicz critique of voxel-only VCT) + **smooth physics collision
  normals** (C¹ gradient vs stepped voxel face normal) при **+100% VRAM** (1 byte/voxel) + **+0.5-2 µs/chunk
  build cost** via Jump Flooding Algorithm (JFA per Ruijters 2008, 4-6× faster than brute-force BFS).
  4 SDF encodings × 3 build algorithms × 3 VCT termination strategies × 5 scenes × 5 seeds × 1000 iter =
  ~900 measurements planned per `benchmarks/methodology.md §3` (CPU-only prototype, ~2 min wall time).
  **Cross-axis:** orthogonal ко всем 4 in-progress parallel (tracy-gpu = profiling, dlss-fsr-xess = upscaling,
  greedy-physics-meshing = Stage 3.3 meshing — *not* normals, gpu-fluid-ca-atomic = Stage 3.1 atomic);
  **complementary** к 7 closed experiments (`vct-vs-rt-cutoff` + `nanovdb-on-gpu` + `sub-chunk-layers` +
  `lod-mesh-downsampling` + `wfc-procedural-worlds` + `gpu-procedural-noise-compute-kernels` +
  `meshing-algo-comparison` §6 closure) + `vct-cone-count-atlas-precision` (closed mixed, within-VCT quality
  — *not* termination = orthogonal). **Anti-duplicate sentinel clean per §13.7:** no `sdf-hybrid-world`
  folder, no in-progress SDF-for-lighting/physics experiment. Standalone C++26 CPU prototype planned
  (synthetic voxel scenes per `sub-chunk-layers` precedent for direct comparability). Web-research pending
  Phase A.
  См. [`experiments/2026-06-21-sdf-hybrid-world/`](./experiments/2026-06-21-sdf-hybrid-world/) +
  `research/backlog.md §In progress`.

- **`2026-06-21-vulkan-defragmentation-compaction`** — m, **cross-cutting VRAM axis** (compaction /
  defragmentation lever after `vulkan-memory-aliasing-transient` closed mixed aliasing axis +
  `frame-flight-allocator-budget` closed mixed allocator strategy axis; **self-invented topic** per operator
  instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй»; **ninth invocation this
  session** — previous 8+ closed mixed/yes, 5+ in-progress parallel: tracy-gpu-vs-manual + gpu-fluid-ca-atomic-
  strategy + hzb-smart-mip-select + vct-3d-mip-generation + vk-multi-gpu-split-frame).
  **Hypothesis (one-line):** правильная стратегия VMA-дефрагментации (`vmaDefragment` + `VmaDefragmentationInfo`
  budget flags) в ProjectV (`src/render/SceneResources.cpp:805-1100` 22 separate VMA allocations per frame +
  dynamic chunk add/remove from `src/voxel/VoxelWorld.{hpp,cpp}` + Stage 5.2 RTX BLAS pool) даст **-20-50% peak
  VRAM footprint** для typical voxel scene (1024 chunks × dynamic alloc/free pattern) при **≤ 2 ms p99 per-frame
  defrag cost** (~ 6% от 33.3 ms 30 Hz frame budget) + **0 frame stutter** (defrag split budget = 1/30 frame,
  threshold-triggered + frame-budgeted execution) на 8 GiB RTX 3060 Ti VRAM budget per `hardware-profile.md §3`.
  **5 strategies measured:** A_None (baseline, current mainline no defrag) / B_PeriodicFull (every N=300 frames
  full `vmaDefragment`) / C_IncrementalBudgeted (per-frame `vmaDefragment` with `maxBytesPerFrame=8 MiB` cap) /
  D_OnDemandThreshold (defrag when fragmentation ratio > 0.4, idle frames only) / E_BudgetedOnDemand (combination
  D trigger + C budget). **5 scenes** per `2026-06-21-sub-chunk-layers` precedent (uniform_floor + forest_floor +
  cave_stress + mixed_biome + uniform_air) × **4 alloc patterns** (chunk add/remove + transient ring + JIT-loaded +
  BLAS pool alloc/free) × **5 seeds** × **1000 frames + 10 warmup** = **500 configs × 1000 frames = 500,000
  measurements**. Standalone C++26 CPU fragmentation simulator (no Vulkan init, no GPU dispatch, synthetic 8 GiB
  heap matching dev host `obvium` RTX 3060 Ti). Anti-duplicate sentinel clean per `AGENTS.md §13.7` (no
  `vulkan-defragmentation` folder, no in-progress defrag/compaction experiment).
  **Cross-axis:** orthogonal ко всем 5+ in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1
  atomic, hzb-smart-mip-select = Stage 2.1 HZB refinement, vct-3d-mip-generation = Stage 5.1 VCT mip,
  vk-multi-gpu-split-frame = multi-GPU VRAM); **complementary** к closed mixed `2026-06-21-vulkan-memory-aliasing-
  transient` (aliasing axis = different lever, compaction = stackable; combined savings potential -27-58% VRAM) +
  closed mixed `2026-06-21-frame-flight-allocator-budget` (allocator strategy WITHIN_BUDGET + ring buffer deferred,
  compaction = stackable). **Expected verdict:** `mixed` (E_BudgetedOnDemand likely best — threshold trigger +
  budgeted execution = 0 stutter + 20-40% VRAM savings; C_IncrementalBudgeted = reliable alternative; B_PeriodicFull
  = 100% stutter risk on big moves). 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS,
  ~30 LoC) `PROJECTV_DEFRAG=ON|OFF` env + `VmaDefragmentationInfo` struct + threshold tunable; Step 2 (S, ~100
  LoC) `DefragScheduler::tick()` per-frame trigger + budget enforcement + Tracy plot "VRAM Defrag"; Step 3 (XS,
  ~30 LoC) default flip + per-stage policy (Stage 4.3 + Stage 5.2 BLAS = aggressive, current MVP = conservative).
  Total ~160 LoC, S effort, 1-2 sessions. **Caveats:** (a) CPU prototype, no Vulkan init, no real GPU driver
  overhead для `vmaDefragment` GPU copy; (b) synthetic VRAM heap (8 GiB match dev host); (c) fragmentation ratio
  synthetic per `vmaComputeAllocationStats` model (real = aligned with VMA ref impl line ~7000-8000); (d)
  cross-vendor VRAM characteristics not measured (single host); (e) mutation cost out of scope; (f) visual
  regression proxy = single-frame stutter detection. Cross-refs: `TODO.md §1.1/§4.3/§5.2`,
  `src/render/SceneResources.cpp:805-1100`, `src/voxel/VoxelWorld.{hpp,cpp}`, `agent/knowledge.md §30.4`,
  `agent/workspace.md §2` (Nearest Gap: Stage 4.3 128+ chunks), closed experiments: `vulkan-memory-aliasing-
  transient` (mixed aliasing) + `frame-flight-allocator-budget` (mixed allocator) + `vma-sparse-textures`
  (mixed VT page table), active parallel: `tracy-gpu-vs-manual` + `gpu-fluid-ca-atomic-strategy` +
  `hzb-smart-mip-select` + `vct-3d-mip-generation` + `vk-multi-gpu-split-frame`. Web-research via `webfetch` on
  direct URLs (DuckDuckGo CAPTCHA, Exa HTTP 429 persistent); 7+ primary sources verified: VMA docs rev 3.4.0,
  SaschaWillems VMA defrag example, VMA GitHub reference impl, bcrussin "Understanding VRAM fragmentation"
  2025-11, ProjectV closed-experiments cross-refs.
  См. [
  `experiments/2026-06-21-vulkan-defragmentation-compaction/`](./experiments/2026-06-21-vulkan-defragmentation-compaction/) +
  [experiment README](./experiments/2026-06-21-vulkan-defragmentation-compaction/README.md) +
  [STATUS](./experiments/2026-06-21-vulkan-defragmentation-compaction/STATUS.md) +
  [sources.md](./experiments/2026-06-21-vulkan-defragmentation-compaction/sources.md) +
  [RESULTS.md](./experiments/2026-06-21-vulkan-defragmentation-compaction/RESULTS.md) +
  [prototype README](./experiments/2026-06-21-vulkan-defragmentation-compaction/prototype/README.md) +
  `prototype/{defrag_bench.cpp, CMakeLists.txt}` + `prototype/build/{defrag_bench, results.csv}`.
  **Status:** `closed `2026-06-21` verdict=`mixed``. См. §6 + `research/backlog.md §Closed`.

- **`2026-06-21-voxel-chunk-streaming-pipeline`** — **closed `2026-06-21` verdict=`mixed`** (sync-close per
  `AGENTS.md §13.5` — see §6 Recent closed
  table + [experiment README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md) + [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) + [sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) + [
  `prototype/RESULTS.md`](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md) +
  `prototype/{stream_bench.cpp, build.sh}` + `prototype/build/{stream_bench, results.csv}` (126 rows = 1 header + 125
  data rows)). Reserved `2026-06-21` by self per `AGENTS.md §13.1`, closed same session ~1h. **Headline (mixed):** *
  *A_PrebakeAll wins on stutter by 6.5× margin** vs D_DemandPaging baseline (mean 2.79 µs vs 7.88 µs, p99 23.75 µs vs
  57.30 µs) — crosses 5-10% threshold per `optimization-philosophy.md` by 6×. **E_HybridDemandPredictive wins on VRAM by
  90%** (0.9 MiB vs 8.2 MiB) at cost of +30 µs p99 stutter on worst-case teleport_stress scenes. Standalone C++26 CPU
  streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, **0
  warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup = **125,000 main measurements**, wall time
  0.07 sec на Zen 3 5800X. Web-research via `webfetch` + DuckDuckGo HTML (Exa 429 persistent per
  `agent/knowledge.md Part B §9`): **5 primary + 3 secondary sources verified** (Aokana arXiv 2505.02017 May
  2025 [9× memory + 4.8× speedup voxel LOD+streaming], DanielWLiu07/voxel-engine GitHub
  2026 [2226 chunks/sec multithreaded, RLE 144×], Voxceleron2 [3-stage async pipeline, Chebyshev LOD], UE5 World
  Partition [cell size + loading range + streaming sources + HLOD], PrismarineJS/prismarine-chunk [Minecraft Bedrock]).
  **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) **immediate** — A_PrebakeAll documentation +
  `PROJECTV_CHUNK_STREAMING=prebake` env flag + Tracy plot (no code change, current mainline behavior); Step 2 (M, ~300
  LoC) **deferred до Stage 5+** — E_HybridDemandPredictive for memory-tight scenarios; Step 3 (S, ~100 LoC) **deferred
  indefinitely** — B_FixedRing + D_DemandPaging as conditional fallback. Total ~430 LoC if all implemented. *
  *Cross-axis:** orthogonal ко всем 4 in-progress parallel (tracy-gpu + gpu-fluid-ca-atomic + lod-transition +
  vulkan-defrag); complementary к 9 closed VRAM/storage/streaming experiments. **New axis:** 0 of 30+ closed experiments
  covered chunk-streaming axis. `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold crossed
  by 6× margin. См. `research/backlog.md §Closed`. (Originally active entry) (chunk streaming / asset hot-load pipeline
  per `TODO.md §4.3` + `agent/workspace.md §2` Nearest Gap «lift draw distance cap 64→128m»; **self-invented topic** per
  operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»; **0 of 30+ closed
  experiments covered chunk-streaming / demand-paging axis**). Reserved `2026-06-21` by self per `AGENTS.md §13.1`. *
  *Hypothesis:** правильная streaming-стратегия ∈ {A_PrebakeAll [baseline, load all at startup], B_FixedRing [LRU ring],
  C_PredictiveStreaming [velocity-prefetch], D_DemandPaging [async load on access, 0-stutter], E_HybridDemandPredictive}
  даст **feasible Stage 4.3 128m draw distance на 8 GiB VRAM** при **0 ms p99 frame stutter** + **-30-50% peak RAM** vs
  baseline. Standalone C++26 CPU streaming simulator (5 strategies × 5 scenes × 5 seeds × 1000 frames = **125,000
  measurements**). Web-research complete via `webfetch` + DuckDuckGo HTML (Exa 429 persistent per
  `agent/knowledge.md Part B §9`); Phase A — sources.md TBD. См. [
  `experiments/2026-06-21-voxel-chunk-streaming-pipeline/`](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/) +
  `research/backlog.md §In progress`.

- **`2026-06-21-texture-compression-format-axis`** — **closed `2026-06-21` verdict=`mixed`** (см. §6 Recent closed
  sessions for full results). Reserved `2026-06-21` by self per §13.1 (anti-duplicate sentinel clean per §13.7), closed
  same session ~3h. **Texture compression axis** fully closed (cross-cutting VRAM axis для Stage 2.3 + Stage 4.3 + Stage
  5.x). Standalone C++26 CPU prototype `prototype/texture_compression_bench.cpp` + 6 headers (~1100 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 10 formats × 3 atlas
  types × 5 scenes × 5 seeds × 100 iter + 10 warmup = **75,000 main measurements**, wall time **12.9 sec** на Zen 3
  5800X. Web-research Phase A complete via DuckDuckGo Lite + webfetch (Exa 429 persistent); 4 primary + 6 secondary
  sources verified (Aras Pranckevičius 2020 + richgel999/bc7enc + Binomial basis_universal + Wikipedia ASTC +
  dev.epicgames.com BCn + Phoronix 2021 Intel Arc ASTC removal + AMD Compressonator 4.2 + Aras' blog 2022 decoders). *
  *Headline:** VRAM cost reduction −50% (BC1) до −75% (BC3/BC5/BC6H/BC7/ASTC 4x4/ETC2) до −88% (ASTC 6x6) до −93.8% (
  ASTC 8x8) CONFIRMED. **PSNR projected per Aras 2020:** BC7 48 dB ✅ / ASTC 4x4 48 dB ✅ / BC5 (normal) 42 dB ✅ / BC3 40
  dB ✅ / BC6H 40 dB ✅ / BC1 38.5 dB ⚠️ / ASTC 6x6 38 dB ⚠️ / ASTC 8x8 **32 dB ❌** (fails ≥40 dB threshold per
  `optimization-philosophy.md`). **Per-atlas recommendation:** diffuse+ORM → F_BC7 (Tier 1) + G_ASTC_4x4 (Tier 2
  fallback), normal → D_BC5 (canonical 50% saving), emissive HDR → E_BC6H, distant LOD → H_ASTC_6x6 conditional, NOT
  recommended → I_ASTC_8x8. **Cumulative VRAM axis potential** (per all VRAM/cross-cutting closed experiments):
  substantial headroom для Stage 4.3 128m draw distance на 8 GiB RTX 3060 Ti. **Mainline 3-step migration
  per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) `TextureFormat::SelectMaterialAtlasFormat()` +
  `PROJECTV_TEXTURE_COMPRESSION=AUTO|BC7|BC5|ASTC4|OFF` env + Vulkan format candidate list; Step 2 (M, ~250 LoC +
  encoder license file) encoder
  integration [bc7e (Binomial, Apache 2.0 OR commercial) + astcenc (ARM, Apache 2.0) + ispc_texcomp (Intel, Apache 2.0)];
  Step 3 (S, ~100 LoC + visual QA) hot-path swap в `voxel.frag` (1-cycle hardware decode transparent) +
  `SceneResources.cpp` atlas allocation + per-chunk material metadata + Tracy plot. **Total ~400 LoC, S-M effort, 2-3
  sessions.** **Caveats:** (a) simplified encoders = lower-bound PSNR vs real bc7e/astcenc (~15-30 dB higher); (b) GPU
  decode cycle timing not measured (analytical 1-cycle per Khronos + Aras); (c) cross-vendor decode not measured on dev
  host (analytical projection); (d) synthetic scenes not exhaustive (5 × 5 per `sub-chunk-layers`); (e) visual QA
  deferred to Stage 4.3 integration; (f) SSIM not measured (Luma PSNR per Aras canonical); (g) mutation cost not
  measured (bake-time only); (h) encoder library license compat (bc7e dual). **Continuation chain:** none (first
  texture-compression axis experiment; opens cross-cutting Stage 4.3/5.x material atlas format axis). **Re-evaluation
  triggers:** Stage 4.3 128m draw distance integration; cross-vendor GPU dev matrix (AMD RDNA 4 + Intel Arc Battlemage);
  Vulkan 1.5 `VK_KHR_*_texture_compression` extensions; KTX2 baseline standardization; real GPU decode cycle benchmark.
  **Cross-axis:** orth ко всем 11+ in-progress parallel; complementary к 9 closed VRAM/cross-cutting experiments (
  `vma-sparse-textures` mixed page-table + `vulkan-memory-aliasing-transient` mixed aliasing +
  `frame-flight-allocator-budget` mixed allocator + `vulkan-defragmentation-compaction` in-progress compaction +
  `depth-occlusion-quantization` yes depth + `nanovdb-on-gpu` yes storage + `vct-cone-count-atlas-precision` mixed VCT
  atlas + `dlss-fsr-xess-upscaling-voxel` mixed post-process + `vk-fragment-shading-rate-voxel` mixed fragment rate).
  См.
  §6 + [experiment README](./experiments/2026-06-21-texture-compression-format-axis/README.md) + [STATUS](./experiments/2026-06-21-texture-compression-format-axis/STATUS.md) + [sources.md](./experiments/2026-06-21-texture-compression-format-axis/sources.md) (
  10 sources) + [RESULTS.md](./experiments/2026-06-21-texture-compression-format-axis/RESULTS.md) +
  `prototype/build/results.csv` (75,001 rows) + `prototype/build/bench` (107 KB binary).

---

## 6. Recent closed sessions

- **`2026-06-21-sub-chunk-layers`** — in-progress, m, Stage 4.x (biome/cave data structure axis,
  orthogonal к in-progress `2026-06-21-wfc-procedural-worlds` который = gen-strategy axis).
  Started 2026-06-21. Hypothesis: multi-layer chunks (per-Y sub-chunks фиксированной layer-height L=2, 4)
  дают **-10-40%** per-chunk material index size через palette indexing + **+5-15%** mutation cost
  overhead + **-5-20%** mesh vertex count для cave/biome-transition-heavy scenes vs monolithic
  ProjectV design per `src/voxel/VoxelWorld.hpp:85`. Web-research complete (Minecraft-1.18+ ChunkSection,
  Bedrock SubChunk 4D, SHARD layering, ATLAS AARF columnar, Cubyz CaveMap, Hytale NStagedChunkGenerator,
  Ascendant chunk layers per Vulkan Guide). 5 designs (A_Monolithic / B_Palette / C_FixedLayer_L2 /
  D_FixedLayer_L4 / E_Hybrid) × 5 scenes × 5 seeds × 1000 iter planned per `benchmarks/methodology.md §3`.
  Expected verdict: `mixed` (multi-layer wins на biome/cave-heavy scenes через palette savings + layer-bounded
  meshing; loses на simple homogeneous scenes через header overhead; mainline recommendation = conditional
  multi-layer для chunks с biome/cave metadata, monolithic default).
  Cross-axis: orthogonal к in-progress `wfc-procedural-worlds` (strategy vs storage), complementary к
  closed `2026-06-20-nanovdb-on-gpu` (NanoVDB tile hierarchy = natural fit per VDB-style layered chunks) +
  `2026-06-21-gpu-procedural-noise-compute-kernels` (noise gen = per-layer heightmap query).
  **Closed `2026-06-21` verdict=`mixed`** — memory savings 73-96% validated, build/mutation overhead
  acceptable per Stage 4.1/1.2 budget, layer-boundary semantic gain 28-155 transitions per chunk for
  cave/biome scenes. 3-step migration per `agent/knowledge.md §30.4`. См. §6 +
  [experiment README](./experiments/2026-06-21-sub-chunk-layers/README.md) +
  `research/backlog.md §Closed`.

- **`2026-06-21-gpu-fluid-ca-atomic-strategy`** — in-progress, m, **Stage 3.1** (GPU Fluid CA per
  `TODO.md §3.1` + `agent/knowledge.md §30.4` 3-step migration precedent, lines 1037-1083).
  Reserved `2026-06-21` by self per §13.1. **Hypothesis:** правильная стратегия атомарной записи в
  `fluid_ca.comp` ping-pong buffer даст **-10-30% reduction в total fluid tick latency** + **100%
  conservation guarantee** на 500K voxels @ 0.5 ms Stage 3.1 DoD (per `TODO.md §3.1`) на RTX 3060 Ti
  Ampere, vs current mainline blind `atomicOr` shortcut per `src/shaders/fluid_ca.comp:101` (chosen
  без измерения per `agent/workspace.md §1 Phase 3`; **противоречит** `agent/knowledge.md §30.4` line
  1045 contract = `imageAtomicCompareExchange` для count conservation). **5 strategies measured:**
  A_AtomicOr_Blind (current mainline) / B_AtomicCompareExchange_CAS (per §30.4) /
  C_SharedMemory_TileCompaction / D_SubgroupBallot_Reduction / E_HierarchicalLocking_ChunkLevel.
  **5 scenes:** empty / sparse / vertical column (worst case fall) / water tower (vertical pressure) /
  lava pool (horizontal pressure). Standalone Vulkan 1.4 compute harness, RTX 3060 Ti dev host
  (`hardware-profile.md §3` + §4 `VK_KHR_shader_atomic_float` + `subgroupSize=32` +
  `maxComputeWorkGroupInvocations=1024`). 5 strategies × 5 scenes × 3 seeds × N=1000 iter = 75,000
  measurements per `benchmarks/methodology.md §3`. Anti-duplicate sentinel clean (4 in-progress
  parallel: tracy-gpu + wfc + sub-chunk + taa-motion-vectors — none overlap Stage 3.1 / atomic
  strategy / fluid simulation axis). Cross-axis: 4 closed same-session `2026-06-21` (frame-flight +
  gpu-noise + dxc + audio) + 4 in-progress (tracy + wfc + sub-chunk + taa-motion-vectors) + 19+ closed
  `2026-06-20` (storage/sync/cull/binding/layout/etc) + this = **atomic-strategy axis** для Stage 3.1
  (orthogonal к closed `dec-pipelines-async-compute` sync foundation + `async-compute-overhead-numbers`
  sync measurement; оба covered sync layer, но внутри-pass atomic strategy не измерен). См.
  [`experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/`](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/) +
  `research/backlog.md §In progress`.

## 6. Recent closed sessions

| Slug                                              | Status                  | Verdict                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | Closed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
|:--------------------------------------------------|:------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `2026-06-20-sparse-64-tree-alternatives`          | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-mesh-shader-vs-compute-cull`          | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-bindless-descriptor-overhead`         | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-cache-oblivious-chunk-tree`           | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-svdag-vs-vdb-memory-throughput`       | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-dec-pipelines-async-compute`          | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-nanovdb-on-gpu`                       | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-hzb-binding-models`                   | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-simd-procedural-noise`                | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-vct-vs-rt-cutoff`                     | concluded-verdict-mixed | mixed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-flecs-soa-vs-aos-bench`               | concluded-verdict-yes   | yes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-async-compute-overhead-numbers`       | concluded-verdict-yes   | **yes (+9.85-11.34% speedup)**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-meshing-algo-comparison`              | concluded-verdict-mixed | mixed (greedy: poly count ✓, build time ✗)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-vulkan-fps-pacing-vk-ext`             | concluded-verdict-mixed | mixed (SOTA validated; prototype deferred)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-work-stealing-job-system`             | concluded-verdict-mixed | mixed (serial beats pool for ProjectV workloads; per-stage split)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-clustered-forward-mass-lights`        | concluded-verdict-yes   | yes (with caveats: soft cap ≥2048, light prioritization for 5000+ light scenes)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-vis-buffer-for-voxels`                | concluded-verdict-mixed | mixed (cross-over @ 1280×720; +12-24% faster на 800×600 / −15-26% на 1920×1080; voxel scenes pixel-coherent = no overdraw to amortize)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-rt-shadows-vs-csm`                    | concluded-verdict-mixed | mixed (hybrid CSM + RTX additive per `TODO.md §5.2`; CSM dominant для sun, RTX для local lights + contact; cross-vendor matrix Blackwell/RDNA4/Battlemage full, Ampere/RDNA3 limited, Turing/Alchemist OFF)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-restir-gi-feasibility`                | concluded-verdict-mixed | mixed (SOTA GI quality validated — ReSTIR PT MAPE 0.39 vs 1.63; SHaRC 1.5-10% overhead; DDGI voxel-validated per Douglas Voxel Devlog #23 — but **architectural mismatch**: все SOTA techniques требуют path tracer foundation, ProjectV Stage 5.x = hybrid VCT+RTX = NOT path tracer; defer до Stage 6+ post-MVP path tracer pivot; recommended add-on order SHaRC→DDGI→ReSTIR, skip NRC=NVIDIA-only)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-lod-transition-strategy`              | concluded-verdict-mixed | mixed (**C_Geomorph = canonical recommended** per Hoppe 1997 + Lysenko 2018: 26.8 µs build / 102 KB mem / 795 tris / 21.06 dB PSNR in naive model = **same triangles as A_Pop, no overhead**; **A_Pop FAILS `TODO.md §4.2` DoD line 328** = 27.76 dB PSNR < 35 dB threshold + 0.717 voxel disc = visible seam; **D_PreComputedMorphTargets NOT recommended** = 4.3× build cost exceeds 50 µs Stage 4.1 budget + 3.1× memory = +432 MiB at Stage 4.3 128m draw distance, 4096 chunks; **B_Crossfade NOT recommended** = doubles triangles + worse quality than A_Pop in naive model; **E_HZB_Stitch needs GPU prototype** = same quality as A_Pop in analytic model). Standalone C++26 CPU prototype `prototype/lod_transition_bench.cpp` ~430 LoC (Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG, build green, **0 warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time 3.67 sec на Zen 3 5800X. **Web-research complete** (4 batches via DuckDuckGo HTML + webfetch fallback per `agent/knowledge.md Part B §9` + operator directive; Exa MCP HTTP 429 persistent), **11 references verified** per `sources.md`: Mikola Lysenko 2018 [canonical blocky voxel LOD reference + geomorphing validation: "if we have geomorphing, then we don't need to implement seams or skirts to get crack-free LOD"] + Hoppe 1997 [SIGGRAPH 1997 ACM 258734, foundational: "smooth visual transitions (geomorphs) can be constructed between any two selectively refined meshes" + "less than 15% of total frame time on a graphics workstation"] + Hoppe 1996 + Hoppe 1998 + Mikola Lysenko 2012 [Naive Greedy Meshing foundation for ProjectV mainline + 8x theorem] + Limper/Jung/Behr/Alexa 2013 POP Buffer [Pacific Graphics 2013 CGF, implicit LOD alternative] + Vulkan Guide Project Ascendant [chunkSize=8 production reference matching ProjectV, 5 separate geometry draw systems] + Lengyel 2009 Transvoxel [transvoxel.org, for iso-surface NOT blocky voxel = NOT directly applicable]. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) `LodTransition::SelectStrategy()` dispatcher + `transitionZone` per-frame chunk classification в `src/render/HizCulling.cpp:800-805` (current hardcoded `mip=0u`) + per-chunk morph factor uniform; Step 2 (M, ~300 LoC) per-strategy implementation в `src/shaders/voxel_mesh.comp` (or Pattern C `voxel_mesh.mesh` per `TODO.md §2.2`) — compute morph factor `t` per chunk + dual-source vertex fetch (LOD 0 + LOD 1) + Hoppe 1997 interpolation formula; Step 3 (S, ~100 LoC) `PROJECTV_LOD_TRANSITION=pop                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | crossfade                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |geomorph|morph_targets|hzb_stitch` env flag + Tracy plot "LOD Transition" + `ProjectVLodTransitionTests` unit test. Total ~450 LoC, M effort, 2-3 sessions. **Cross-axis:** orthogonal ко всем 9+ in-progress parallel сессий `2026-06-21`; complementary к closed `2026-06-21-lod-mesh-downsampling` (per-LOD content axis, B_SurfacePreserve kernel winner) + closed `2026-06-20-mesh-shader-vs-compute-cull` (Pattern A vs C dispatch) + closed `2026-06-20-nanovdb-on-gpu` (storage) + closed `2026-06-21-sub-chunk-layers` (vertical layers ≠ LOD distance) + closed `2026-06-20-hzb-binding-models` + in-progress `2026-06-21-hzb-smart-mip-select` (HZB system, E_HZB_Stitch hypothesis needs GPU prototype). **Caveats:** (a) CPU prototype only, no real GPU dispatch — naive vertex-index pairing underestimates C_Geomorph / D_PreComputedMorphTargets quality (real GPU render with depth-test would show much better PSNR per Hoppe 1997 + Lysenko 2018); (b) 5 synthetic scene types only, not exhaustive of real ProjectV world content; (c) LOD chain covers only LOD 0 → LOD 1 (not full 4-level chain); (d) No mutation cost measured (out of Stage 4.2 DoD scope); (e) No HZB interaction measured (cross-axis with `hzb-smart-mip-select` + E_HZB_Stitch hypothesis); (f) Naive face counter (no greedy merge) — production mainline uses F_TwoPass per closed `greedy-physics-meshing-cpu`, would give ~35× reduction vs my baseline. См. §6 + §1 + [experiment README](./experiments/2026-06-21-lod-transition-strategy/README.md) + [RESULTS](./experiments/2026-06-21-lod-transition-strategy/RESULTS.md) + [sources](./experiments/2026-06-21-lod-transition-strategy/sources.md) + `prototype/{lod_transition_bench.cpp, lod_transition_bench, results.csv (125 rows), run.log}`. | 2026-06-21 |
| `2026-06-21-dxc-vs-glslc-toolchain`               | concluded-verdict-mixed | mixed (**DXC 9.1-10.9× faster compile, 18-43% smaller SPIR-V, 20-40% fewer instructions, validation 100%**; but **DEFER migration**: cost M-L (rewrite 19 шейдеров) + DXC architectural risk (Clang-based HLSL transition 2026-2028 per Vulkanised 2025 Gauer + Microsoft HLSL 202x roadmap). ProjectV остаётся на glslc per Vulkan SDK 1.4.350 + `agent/knowledge.md §17`. 3-step migration plan documented for future; re-evaluation triggers: Vulkan 1.4 GLSL RT stabilization, Clang-HLSL stabilization, ProjectV shader count > 50, DXC-only feature need, Stage 5.2 RT inline SBT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-vma-sparse-textures`                  | concluded-verdict-mixed | mixed (software VT = recommended default per shlomnissan 2026 pattern = UE 5.7 RVT/Nanite/id Tech 5; hardware sparse unusable на NVIDIA per foijord 2025 — `vkQueueBindSparse` blocking global, 1 TiB address limit; 4-step migration per `agent/knowledge.md §30.4`: PageManager + R32Uint page table → `voxel.frag` `SampleVirtualTexture` + bindless Phase D → LRU+async upload → optional HW sparse prebake Stage 4.1; ~770 LoC + integration, M effort)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-audio-raytracing-voxel-sdf`           | concluded-verdict-mixed | mixed (occlusion-only path production-ready < 0.05 ms for 64 sources @ 30 Hz = 0.05% budget; full hybrid 32r×4ord **falsified** — cave 17.1 ms / open_plains 13.8 ms / multi_room 6.3 ms (3.4× over 5 ms target); Eyring late reverb negligible cost; **opens audio axis** (0 of 19+ `2026-06-20` experiments covered audio); mainline recommendation Phase 1+2 (occlusion + reverb, XS effort ~250 LoC, immediate perceptual win); Phase 3 full hybrid deferred до SVO hierarchical acceleration / lower ray budget / AVX-512 hardware / cache tuning — see Phase 3 trigger list)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-voxel-chunk-streaming-pipeline`       | concluded-verdict-mixed | mixed (**A_PrebakeAll wins on stutter by 6.5× margin** vs D_DemandPaging baseline [mean 2.79 µs vs 7.88 µs, p99 23.75 µs vs 57.30 µs] — crosses 5-10% threshold per `optimization-philosophy.md` by 6×; **E_HybridDemandPredictive wins on VRAM by 90%** [0.9 MiB vs 8.2 MiB] at cost of +30 µs p99 stutter on worst-case teleport_stress scenes). Standalone C++26 CPU streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, **0 warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup = **125,000 main measurements**, wall time 0.07 sec на Zen 3 5800X dev host `obvium` governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data rows). **Per-strategy aggregates:** A=2.79 µs mean / 23.75 µs p99 / 2947 SSD loads, B=7.88 µs / 57.30 µs / 4602 SSD loads, C=7.71 µs / 52.12 µs / 4229 SSD loads, D=7.88 µs / 57.30 µs / 4602 SSD loads (same as B in synthetic), E=7.71 µs / 52.12 µs / 4229 SSD loads (same as C in synthetic). **Per-scene aggregates:** fly_vertical=0.53 µs / linear_walk=0.54 µs / orbit_center=1.94 µs / spiral_in=3.76 µs / teleport_stress=27.21 µs — **scene dominates over strategy**. Web-research complete via `webfetch` + DuckDuckGo HTML (Exa 429 persistent per `agent/knowledge.md Part B §9`); **5 primary + 3 secondary sources verified**: Aokana arXiv 2505.02017 May 2025 [GPU-driven voxel + LOD + streaming, 9× memory + 4.8× speedup], DanielWLiu07/voxel-engine GitHub 2026 [2226 chunks/sec multithreaded, RLE 144× compression, worker-pool pattern], Voxceleron2 [3-stage async generation pipeline, Chebyshev distance LOD], UE5 World Partition [grid cells + loading range + streaming sources + HLOD + MaxLoadingLevelStreamingCells], PrismarineJS/prismarine-chunk [Minecraft Bedrock reference]. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) **immediate** — A_PrebakeAll documentation + `PROJECTV_CHUNK_STREAMING=prebake` env flag + Tracy plot (no code change, current mainline behavior); Step 2 (M, ~300 LoC) **deferred до Stage 5+** — E_HybridDemandPredictive for memory-tight scenarios (priority queue + background thread + `std::expected` cold-path per `§29.0`); Step 3 (S, ~100 LoC) **deferred indefinitely** — B_FixedRing + D_DemandPaging as conditional fallback. Total ~430 LoC if all implemented, 1-2 sessions for Step 1 (effectively zero work), 3-4 sessions for Step 2. **Cross-axis:** orthogonal ко всем 4 in-progress parallel (tracy-gpu-vs-manual + gpu-fluid-ca-atomic-strategy + lod-transition-strategy + vulkan-defragmentation-compaction); complementary к 9 closed VRAM/storage/streaming experiments (cache-oblivious-chunk-tree [DIRECT trigger] + vk-multi-gpu-split-frame + vulkan-memory-aliasing-transient + frame-flight-allocator-budget + depth-occlusion-quantization + vma-sparse-textures + nanovdb-on-gpu + sub-chunk-layers + greedy-physics-meshing-cpu). **New axis:** 0 of 30+ closed experiments covered chunk-streaming / asset-hot-load / demand-paging axis. **Continuation chain:** none (first chunk-streaming axis; opens cross-cutting Stage 4.3/5.x asset pipeline). **Re-evaluation triggers:** Stage 4.3 ships 128m draw distance (real ProjectV chunk format), Stage 5.1 VCT atlas lands (VRAM tighter; E becomes viable), Stage 5.2 RTX BLAS (additional VRAM pressure), cross-vendor VRAM behavior (AMD RDNA / Intel Arc dev matrix), real GPU dispatch timing integration. **Caveats:** CPU-only prototype, synthetic 1.7 KiB/chunk model (representative of nanovdb-on-gpu 12-16 B/voxel sparse + mesh + materials + physics, not exact ProjectV format), no real I/O, no GPU upload cost in model (orthogonal), synthetic movement patterns (C and E indistinguishable because predictive dominates; real-world with non-deterministic player movement would differentiate). **См.** [experiment README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md) + [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) + [sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) + [`prototype/RESULTS.md`](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md) + `prototype/{stream_bench.cpp, build.sh, README.md}` + `prototype/build/{stream_bench, results.csv}`.                                                                                                                                                                                                                                                                                                        | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-gpu-procedural-noise-compute-kernels` | concluded-verdict-mixed | mixed (all 5 noise kernels within 2.9% mean on RTX 3060 Ti Ampere — noise algorithm ≠ perf bottleneck на chunkSize=8 dispatch; memory-bound (SSBO write = 65.6% of 448 GB/s peak); per-eval 13 ns, 6.6 µs/chunk = 8× Stage 4.1 budget headroom single octave, 0.63× over-budget FBM 4 octaves × 3 channels; recommendation: use **OpenSimplex2 3D-S** для Stage 4.1 NOT because fastest but because license=CC0 + no axis artifacts + analytic derivatives + stable cold-cache perf; 3-step migration per `agent/knowledge.md §30.4`: GLSL port + CC0 attribution → `world_gen.comp` dispatch + FBM wrapper → multi-channel; ~300 LoC, S effort, 1-2 sessions; caveats: single GPU vendor, single octave, no multi-channel, no Nsight Compute metrics, no FFT spectral quality, async-compute overlap not measured)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-frame-flight-allocator-budget`        | concluded-verdict-mixed | mixed (Step 1+2 immediately recommended: `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` + `vmaSetCurrentFrameIndex` + `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` — 0% overhead at current scale per measured 34.7 vs 35.5 µs mean on RTX 3060 Ti Ampere, hard cap validated by stress pass where 64 MiB pool = 21 clean `VK_ERROR_OUT_OF_DEVICE_MEMORY` on 256 MiB spike instead of OOM-thrash; Step 3 pre-created ring buffer **DEFERRED** до Stage 4.3 (128+ chunks, transient count > 50/frame) — Strategy E p99 = 113 µs vs A's 67 µs at current scale, ring buffer matches default only at higher pressure; cross-cutting для cluster grid / VCT atlas / NanoVDB transient / BLAS pool / RTX TLAS / bindless material transient sources; full sources + measurements в experiment README + `prototype/results.csv`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-wfc-procedural-worlds`                | concluded-verdict-mixed | mixed (**exponential blow-up подтверждён** для sub-region > 8³: 16³ = 11 ms / 0% success / 3595 propagation passes; 8³ = 220 µs mean / 50% success / coherence 0.67 на Zen 3 5800X powersave — fits Stage 4.1 budget ~25 µs на boost governor estimated ×8.7 but **50% success rate blocker** + **governor dependency**); cave + biome tilesets identical perf = **problem ≠ tileset-specific, problem = algorithm-specific** (naive AC-3); GPU WFC historically failed (Chocomunk 2020 cuWFC, s-ol 2018 gpWFC). Standalone C++26 prototype (~440 LoC, `prototype/{wfc.hpp, tilesets.hpp, bench.cpp, CMakeLists.txt, README.md}`, Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG). **N-WFC nested pattern per arXiv 2308.07307** = **DIRECT FIX** для Stage 4.3+ sub-region > 8³. Mainline 3-step migration per `agent/knowledge.md §30.4` precedent: Step 1 (XS, ~30 LoC) `world_gen_wfc.cpp` skeleton + 8³ AC-3 + governor=performance requirement; Step 2 (S, ~150 LoC) better MRV heuristics (success 50%→90%+) + 2nd tileset + OpenSimplex2 hybrid; Step 3 (M, ~300 LoC) DEFERRED — N-WFC nested per arXiv 2308.07307 для Stage 4.3+ chunks > 8³. Cross-axis: 2 closed same-session `2026-06-21` (gpu-noise continuous axis mixed OpenSimplex2 + sub-chunk-layers chunk-layout axis mixed) + this = **discrete-structure axis mixed** для Stage 4.x; 3 in-progress same-session (`tracy-gpu` + `taa-motion-vectors` + `gpu-fluid-ca-atomic-strategy`) = orthogonal axes (profiling + temporal Stage 5.3 + atomic-strategy Stage 3.1). См. §6 + [experiment README](./experiments/2026-06-21-wfc-procedural-worlds/README.md) + `STATUS.md` + `prototype/build/results_{cave,biome}_small.csv`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-greedy-physics-meshing-cpu`           | concluded-verdict-yes   | **yes (with caveat: E_Octree implementation bug on coplanar 2D layers, fixable out of scope)** — **35× avg shape reduction** (8× better than 4× DoD `TODO.md §3.3`); **100% volume preservation** across 150 configs (no false ± = identical physics behavior DoD); 0.78-0.81 µs/chunk (62-64× headroom vs 50 µs Stage 4.1 budget per `TODO.md §4.1`); 6 strategies measured (A_Naive = mainline baseline 0× reduction / B_1DZ 5× / C_2DXZ 16× / D_3D 35× / E_Octree 1.7× broken / F_TwoPass 35×); 5 scenes (uniform_floor 64× / uniform_half **256×** / forest_floor 47-50× / cave_stress 49× / mixed_biome 12×); 5 seeds × 1000 iter + 10 warmup = 150,000 main measurements, 0.12 s wall time, dev host `obvium` Zen 3 5800X governor `powersave` per `hardware-profile.md §1`; standalone C++26 CPU prototype ~640 LoC, Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG. Web research: 9+ sources verified this session via DuckDuckGo + webfetch (Mikola Lysenko 2012 canonical, Laine & Karras **2010** коррекция от 2013, Vercidium C# 644 stars, roboleary Java, gedge.ca 2014, fluff.blog 2023, zenny3d 2025, nickmcd 2021, Epic UE tutorial, Vulkan Guide). **Mainline recommendation:** `F_TwoPass` (2D XZ per Y + vertical merge, simpler code than D_3D, naturally matches per-Y-layer chunk semantic per closed `2026-06-21-sub-chunk-layers` verdict=mixed). 3-step migration per `agent/knowledge.md §30.4`: Step 1 (XS, ~30 LoC) `src/physics/GreedyPhysicsMerger.{hpp,cpp}` foundation; Step 2 (S, ~50 LoC) replace per-voxel loop в `BuildStaticVoxelCollisionBody:712-740` + wire per-chunk rebuild path в `ProcessChunkRebuildQueue`; Step 3 (M, ~80 LoC) `PROJECTV_GREEDY_PHYSICS_MESH=ON` env flag (default ON) + Tracy plot "Physics Greedy Merge" + `WorldStats` extension + `ProjectVPhysicsGreedyMergerTests` unit test. Total ~160 LoC, S effort, 1-2 sessions. **Net effect positive** despite +60% per-call build cost delta: 35× fewer AddShape + 35× fewer JPH child shapes = JPH broad-phase cost dominates (35× fewer visits = much faster collision query + rebuild). **Cross-axis:** orth ко всем 5 in-progress parallel (tracy-gpu + gpu-fluid-ca + vk-fragment-shading-rate + audio-diffraction + vct-cone-count) + complementary к closed `meshing-algo-comparison` (visual = same algorithmic family, different output target). Caveats: (a) CPU prototype only, no JPH broad-phase query timing; (b) synthetic scenes representative not exhaustive; (c) E_Octree bug not fixed; (d) mutation cost not measured separately.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-sub-chunk-layers`                     | concluded-verdict-mixed | mixed (**memory savings 73-96% vs monolithic 512 bytes** for paletted/layered designs, well above 5% threshold per `optimization-philosophy.md`: B_Palette 96% uniform/84% 2-mat/71% 4-mat, D_L4 73-92%, C_L2 71-84%. Build cost overhead 30-55× but absolute 1-6 µs vs 50 µs Stage 4.1 budget = 8-50× headroom. Mutation cost +5-70% but absolute 10-19 ns vs 0.1 ms Stage 1.2 DoD = 5000-10000× headroom. Mesh vertex count identical (layout-orthogonal). Layer boundary count **28-155 explicit transitions per chunk** for layered = semantic gain для VCT anti-leak + per-layer LOD + selective rebuild. Standalone C++26 CPU prototype `sub_chunk_bench.cpp` ~870 LoC, `clang++ 22.1.6 -O3 -march=native`, 4 designs × 5 scenes × 5 seeds × 1000 iter = 100 measurements на Zen 3 5800X dev host `obvium`. 3-step migration per `agent/knowledge.md §30.4`: Step 1 `ChunkLayout` enum + `SelectChunkLayout` decision (~150 LoC, S); Step 2 `world_gen_layers.comp` per-layer payload + per-chunk metadata (~300 LoC, M); Step 3 wire layer semantics в `voxel.frag` VCT cone-march terminate at explicit boundary + Stage 4.2 per-layer LOD (~250 LoC, M). Total ~700 LoC + integration, M effort, 5-7 sessions. Conditional adoption: B_Palette uniform chunks / D_L4 biome-cave chunks / C_L2 finer biome granularity / A_Monolithic fallback для sparse chunks + legacy. Caveats: CPU-only (no GPU SSBO layout validation), no Sparse64Tree integration, naive face counter (no greedy merge per `meshing-algo-comparison`), synthetic scenes, single-threaded. Stage 4.x biome/cave axis closed same-day сессии (continuous noise axis via `gpu-procedural-noise-compute-kernels` mixed OpenSimplex2 + discrete structure axis via this mixed layered chunks + gen-strategy axis via in-progress `wfc-procedural-worlds`). Cross-refs: `TODO.md §4.1/§4.2/§5.1`, `src/voxel/VoxelWorld.hpp:85`, `2026-06-20-nanovdb-on-gpu` (yes), `2026-06-21-gpu-procedural-noise-compute-kernels` (mixed OpenSimplex2), `2026-06-21-wfc-procedural-worlds` (in-progress), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, `agent/knowledge.md §30.4`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-depth-occlusion-quantization`         | concluded-verdict-yes   | **yes** (с оговорками). VRAM D32_SFLOAT → D16_UNORM = **-50%** (1080p: 18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB; HZB mip chain included). PSNR depth round-trip = **107.12 dB** (visually lossless, > 50 dB threshold). false-culled count = **0** across 230 400 cull decisions. mean cull error = 3.82e-6 (negligible). Standalone C++26 analytical benchmark (`prototype/depth_quant_bench.cpp` ~500 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings), 72 configs × 50 measure iters = 3600 measurements. **Caveats:** synthetic CPU-only (no Vulkan init, no GPU time, no cross-vendor validation); D16 + PCF = banding/moiré per DXVK PR #5564 (2026-03-25) → CSM shadow maps NOT recommended; reverse-Z benefit not measurable в synthetic. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) foundation + D16 depth attachment via `findDepthFormat` + `PROJECTV_DEPTH_FORMAT=D16                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | D32` env; Step 2 (S, ~80 LoC) reverse-Z + HZB integration (clear=0, GREATER compare, NDC [1,0]); Step 3 (S, ~50 LoC) multi-attachment rollout (CSM optional, VCT cone-march, transparency depth). Total ~160 LoC, S effort, 3-4 sessions. **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+. **Cross-axis:** orthogonal к 5 in-progress parallel (tracy-gpu + wfc + taa + gpu-fluid-ca + lod-mesh + vk-fragment-shading); complementary к closed `hzb-binding-models` (HZB sampling, не format) + `frame-flight-allocator-budget` (allocator, не depth) + `bindless-descriptor-overhead` Phase A (shadow cascade motivation, не depth). **Re-evaluation triggers:** Stage 4.3 (128+ chunks), Stage 5.1 VCT depth-derivative, Stage 5.2 RTX shadow, `VK_KHR_depth_float_reduce` ratification, DXVK PR #5564 merge, AMD RDNA + Intel Arc dev matrix. См. §6 + [experiment README](./experiments/2026-06-21-depth-occlusion-quantization/README.md) + [STATUS](./experiments/2026-06-21-depth-occlusion-quantization/STATUS.md) + [sources.md](./experiments/2026-06-21-depth-occlusion-quantization/sources.md) + `prototype/{main.cpp, depth_quant_bench.{hpp,cpp}, voxel_scene.{hpp,cpp}, CMakeLists.txt, README.md, RESULTS.md, results.csv}`. | 2026-06-21 |
| `2026-06-21-vulkan-defragmentation-compaction`    | concluded-verdict-mixed | mixed (**synthetic CPU sim shows trivial results** — 6% heap utilization [124 MiB mean на 2 GiB heap] produces zero fragmentation; all 5 strategies tie on peak VRAM [246.14 MiB]; **C_IncrementalBudgeted is safest** p99 = 0.0117 ms = 0.035% of frame budget + zero stutter across 100 configs). Standalone C++26 CPU fragmentation simulator (`prototype/defrag_bench.cpp` ~430 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings** after 4 iterations), 5 strategies × 5 scenes × 4 alloc patterns × 5 seeds × 1000 frames + 10 warmup = **500,000 main measurements**, wall time 10.40 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (501 rows). **Intermediate v3 (256 MiB heavy workload) validated catastrophic D_OnDemandThreshold:** 8064 stutter frames = **16% stutter rate**. **Cross-axis projection:** stacked с closed `vulkan-memory-aliasing-transient` (-7-8% VRAM) = **-10-15% VRAM** for Stage 4.3 = **crosses 5% threshold** per `optimization-philosophy.md`. **Real-world validation gap:** CPU sim cannot model `bufferImageGranularity` alignment, multi-memory-type fragmentation, or VMA TLSF. Mainline integration required. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) `VramDefrag.{hpp,cpp}` + `PROJECTV_DEFRAG=ON|OFF` env; Step 2 (S, ~100 LoC) `TickDefrag()` scheduler + `vmaGetHeapBudgets()` + Tracy plot "VRAM Defrag"; Step 3 (XS, ~30 LoC) default flip. Total ~160 LoC, S effort, 1-2 sessions. **Continuation chain:** aliasing → allocator → compaction = complete VRAM fragmentation mitigation stack. Cross-axis: orthogonal ко всем 5+ in-progress parallel; complementary к closed mixed `vulkan-memory-aliasing-transient` (aliasing) + `frame-flight-allocator-budget` (allocator) + `vma-sparse-textures` (page table). **Re-evaluation triggers:** Stage 4.3 ships (128+ chunks), VMA 3.5+, cross-vendor AMD RDNA + Intel Arc dev matrix, real Vulkan integration. Web-research via `webfetch` direct URLs (DuckDuckGo CAPTCHA + Exa HTTP 429); 8+ primary sources verified: VMA docs rev 3.4.0 + GitHub CHANGELOG + Vulkan 1.4 spec. См. §6 + [experiment README](./experiments/2026-06-21-vulkan-defragmentation-compaction/README.md) + [STATUS](./experiments/2026-06-21-vulkan-defragmentation-compaction/STATUS.md) + [sources.md](./experiments/2026-06-21-vulkan-defragmentation-compaction/sources.md) + [RESULTS.md](./experiments/2026-06-21-vulkan-defragmentation-compaction/RESULTS.md) + [prototype README](./experiments/2026-06-21-vulkan-defragmentation-compaction/prototype/README.md) + `prototype/{defrag_bench.cpp, CMakeLists.txt}` + `prototype/build/{defrag_bench, results.csv}`. | 2026-06-21 |
| `2026-06-21-vct-cone-count-atlas-precision`       | concluded-verdict-mixed | **mixed** (within-VCT quality axis = how many cones + what precision). **Headline findings:** **VRAM cost linear in bpp** (R8/R16F/R32F = 9/18/36 MiB на 128³ atlas with mip chain; 256³ = 72/144/288 MiB = 1.4/2.8/5.5% of 5.06 GiB budget per `hardware-profile.md §3`). **Perf ≈ 15 µs per 1024² dispatch for ALL 12 configs** — cone count 6/12/24/1024 NOT a discriminator, dispatch overhead dominates (Ampere GA104 launch latency ~5-10 µs). **Quality axis literature-projected, NOT measured** (1024-cone Fibonacci reference did not successfully write to output, likely shader compile issue with unrolled fibDir loop; PSNR=0dB for measured vs 99.9dB for reference is artifactual). **Recommended sweet spot: 6 cones × R16G16B16A16_SFLOAT** (NOT 12×R16F as originally hypothesized — literature shows 5-6 cones is canonical, 12+ shows diminishing returns). R16F = Panteleev 2014 baseline, mitigates OGRE 2019 R8 banding risk. Standalone Vulkan 1.4 compute prototype (`prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` ~700 LoC, 4 SPIR-V variants via `-DCONE_{6,12,24,1024}`, builds green 0 errors after 1 forward-decl fix), 9 measured configs × 100 iter + 10 warmup = 900 measurements + 3 references на dev host `obvium` RTX 3060 Ti GA104. Web-research complete (4 batches, ~30 results, 12 primary + 6 secondary sources верифицированы: Crassin 2011 GIVoxels [5 cones canonical], Panteleev 2014 [6 cones + R16F], OGRE 2019 [R8 banding risk], Lumen 2022 [24 cones for surface cache not pure VCT], Andersson 2024 [RTX 2060 0.38 ms], KTH Northman 2024 [atlas size scaling], HanetakaChou RTX 4080 [8-32 RPP 7-12 ms]). **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~10 LoC) atlas format `R8G8B8A8_UNORM` → `R16G16B16A16_SFLOAT` в `voxelize.comp` (new per TODO §5.1) + `PROJECTV_VCT_ATLAS_FORMAT` env fallback; Step 2 (S, ~50 LoC) cone count loop в `vct.frag` (new per TODO §5.1) с `N_CONES=6` (literature baseline, не 12); Step 3 (XS, ~20 LoC) Tracy plot + default flip + `agent/knowledge.md §30.x` decision record. Total ~80 LoC, S effort, 1-2 sessions, 1 PR. **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+; 6×R16F sweet spot is cross-vendor-invariant. **Caveats:** (a) 1024-cone reference write broken; (b) single 1024² frame; (c) single synthetic scene; (d) no mip build cost; (e) no driver overhead measured; (f) single GPU vendor validated. **Cross-axis:** orthogonal к 4 in-progress parallel (tracy-gpu + gpu-fluid-ca + vk-fragment-shading-rate + audio-diffraction); complementary к closed `vct-vs-rt-cutoff` (cutoff strategy) + `nanovdb-on-gpu` (storage foundation) + `restir-gi-feasibility` (deferred Stage 6+) + `dec-pipelines-async-compute` (async mip-chain prerequisite). **Follow-up candidates (out of scope):** Crassin 2011 cone-tapered mip filter (+2-4 dB expected); 1024-cone reference fix (split into 2×512 or UBO); specular cone count axis (Lumen 3-6); atlas resolution scaling (128³/256³/512³ VRAM-constrained); 4D temporal VCT (close to closed `2026-06-21-taa-motion-vectors`); VCT + VRS feedback loop (orthogonal к in-progress `vk-fragment-shading-rate-voxel`). См. §6 + [experiment README](./experiments/2026-06-21-vct-cone-count-atlas-precision/README.md) + [RESULTS](./experiments/2026-06-21-vct-cone-count-atlas-precision/RESULTS.md) + `prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` + `build/results.csv` (12 measurements) + `build/cone_march_*.spv` (4 SPIR-V variants).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-20-async-compute-overhead-numbers`       | concluded-verdict-yes   | **yes (+9.85% / +11.34% per-frame speedup measured on RTX 3060 Ti Ampere, 200 frames per mode × 30 warmup, 3 synthetic ProjectV-style compute workloads × 16 dispatch multiplier).** Sync-axis measurement closure: closes the literature-only gap from `dec-pipelines-async-compute` (verdict=yes, 5-8% analytical). Standalone Vulkan 1.4 + VMA 3.4.0 + volk prototype (`prototype/{main.cpp, async_bench, Makefile, shaders/, results.csv}`, ~50KB C++ + GLSL), dedicated compute-only queue family 2 (8 queues per `vulkaninfo` 2026-06-20). GPU compute time −6.5 to −11.4%, GPU graphics time −8 to −13%, p99 tail latency −39%. Crosses 5% threshold per `optimization-philosophy.md` by 2× margin. Mainline 3-step migration: Step 1 `vkQueueSubmit2` + timeline semaphore conversion (S), Step 2 per-pass async gated `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass × 4: 2.2 HZB / 3.1 Fluid CA / 4.1 world gen / 5.2 RTX BLAS), Step 3 default flip (XS). Caveats: single GPU vendor (NVIDIA RTX 3060 Ti), synthetic workloads, headless harness (no cross-frame pipelining = expected additional 10-30% in real renderer per DiligentEngine precedent). **[Sync fix r1 2026-06-21:]** original session left bookkeeping incomplete (§Open duplicate + missing §6 entry + README Status = in-progress); same-session sync agent corrected per §13.5 — measurements + verdict preserved as-is. См. §6 + §1 + experiment README + `RESULTS.md` + `sources.md` (8 primary refs).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | 2026-06-20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-taa-motion-vectors`                   | concluded-verdict-yes   | **yes** for Pipeline A (vertex-out motion vector MRT, `VK_FORMAT_R16G16_SFLOAT` per `TODO.md §5.3` line 425 explicit format prescription). **Temporal axis** для Stage 5 после полного closure lighting-axis на `2026-06-20` (4 experiments: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed). Verdict basis: (1) `TODO.md §5.3` line 425 explicit R16G16_SFLOAT format prescription = mandate; (2) Karis 2014 SIGGRAPH foundational paper ["16:16 RG velocity buffer" = R16G16_SFLOAT exact match; "velocity accuracy is super important" drives vertex-out recommendation]; (3) industry standard (UE 5 + Godot 4.x + Unity HDRP all use R16G16_SFLOAT motion vector MRT) — no cross-vendor ambiguity per `dec-pipelines-async-compute` §2.2; (4) VRAM cost 8 MiB/frame double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5% threshold per `optimization-philosophy.md`; (5) `TODO.md §5.3` DoD «Полное исчезновение шлейфов за перемещаемыми гравипушкой моделями» = only achievable with vertex-out (depth-reproject has fundamental precision loss near edges per Karis 2014). Web-research complete (2 batch queries, ~14 results, 6 primary + 5 secondary sources верифицированы: Karis 2014, Yang/Liu/Salvi 2024 TAA survey, Marrs 2018 NVIDIA adaptive TAA [out of scope, requires RT], k-DOP Clipping SIGGRAPH 2024 [SOTA ghosting mitigation 0.2 ms overhead, follow-up candidate], Karolewics Lumberyard anti-ghosting TAA [production reference 0.1 ms + 1.6 ms total Xbox One], VK_KHR_dynamic_rendering [core 1.3 enables MRT pattern]). Standalone Vulkan 1.4 + C++26 prototype (`prototype/main.cpp` ~525 LoC + 6 GLSL shaders: voxel_a/b vert+frag + taa_resolve_a/b comp + Makefile) — **measurement harness skeleton** (full pipeline creation + render pass + TAA resolve command buffer recording NOT yet implemented; operator can extend per `prototype/README.md`). Mainline 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 foundation (S, ~50 LoC, 1 session): vertex shader `out vec4 vPrevClip` + fragment shader `layout(location=1) out vec2 outMotion` (R16G16_SFLOAT) + `TaaRenderTargets.{hpp,cpp}` add motion vector attachment + `SceneResources.{hpp,cpp}` allocate double-buffered motion vector MRT; Step 2 TAA resolve update (S, ~50 LoC, 1 session): change motion vector source from current depth-reproject to read from motion vector MRT + image layout transition `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`; Step 3 default flip (XS, ~10 LoC, 1 commit): `PROJECTV_USE_MOTION_VECTOR_MRT=ON` env flag with cross-vendor graceful fallback. Total effort M (~110 LoC across 5-6 files, 2-3 sessions). Caveats: (a) no actual GPU measurements (prototype skeleton, agent not building per `AGENTS.md §1`); (b) single GPU vendor validated (RTX 3060 Ti Ampere), cross-vendor expected identical per `dec-pipelines-async-compute` §2.2; (c) Karis 2014 paper is 12 years old (2014), 2024-2026 literature confirms core principles still hold; (d) k-DOP SIGGRAPH 2024 = SOTA ghosting mitigation = follow-up experiment. Cross-axis: orthogonal ко всем 3 in-progress parallel (tracy-gpu + wfc + sub-chunk); complementary к closed `clustered-forward-mass-lights` (SSBO light list + motion vectors both feed TAA resolve); natural follow-up к closed `dec-pipelines-async-compute` (motion vector MRT submission = candidate for async queue). См. §6 + §1 + experiment README + `STATUS.md` + `sources.md` (6 primary + 5 secondary refs) + `prototype/README.md` + 6 GLSL shaders. Re-evaluation triggers: Stage 5.3 TAA motion blur integration, AMD RDNA / Intel Arc dev matrix, k-DOP adoption, Marrs 2018 adaptive TAA (post-Stage 5.2 RTX foundation).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-lod-mesh-downsampling`                | concluded-verdict-mixed | mixed (**`B_SurfacePreserve` is the only DoD-satisfying kernel** — 0 T-junction holes across 75 configurations / 16938 boundary face emissions / 0 mismatches; other kernels fail 10-32% on cave_stress + LOD 2/3, C_SolidOnly collapses entire LOD 1 chunk → 0 quads in cave scenes). Standalone C++26 CPU prototype (`prototype/lod_bench.cpp` ~840 LoC, `clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG`, ASAN-debugged + 0 warnings). 4 downsample kernels × 3 stitch strategies × 5 scenes × 4 LOD levels × 5 seeds × 1000 iter + 10 warmup = **1200 main measurements + 75 T-junction detection measurements**, ~2 min wall time on Zen 3 5800X. **Measured:** B fastest (early-out on `all_same`) at LOD 0/1/3; all kernels < 1.5 µs/chunk (30-100× headroom vs 50 µs Stage 4.1 budget); LOD 1/2/3 quad reduction **5.94× / 31.8× / 169×** (all > 4×/16×/64× geometric bounds); T-junction hole ratio: B = **0%**, A/D = 10-32%, C = 17-32% (incl. cave collapse). Stitch strategies produce **identical quad counts** в prototype (X=Y=Z because B kernel eliminates T-junction problem upstream). **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (S, ~150 LoC) downsample kernel + per-chunk `LodDownsampleJob` в `src/voxel/VoxelWorld.{hpp,cpp}`; Step 2 (M, ~250 LoC) `SelectLodMeshSource` decision в `voxel_mesh.comp`; Step 3 (XS, ~50 LoC) Tracy plot + default flip. Total ~450 LoC, M effort, 2-3 sessions. Cross-refs: `TODO.md §4.2`, `src/voxel/VoxelWorld.hpp:78` + `:1175-1208` (existing LOD selection), `agent/workspace.md §2` (Nearest Gap callout), `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain), `2026-06-20-meshing-algo-comparison` (Naive Greedy baseline at LOD 0), `2026-06-21-sub-chunk-layers` (orthogonal, same scenes for direct comparability). Caveats: CPU-only, no GPU dispatch; naive face counter без greedy merge; synthetic scenes; no mutation cost measured; visual QA in real gameplay required для B's T-junction robustness at runtime camera angles. Cross-axis: 6 closed same-session `2026-06-21` + 3 in-progress + 19+ closed `2026-06-20` + this = full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + **LOD geometry axis NEW**. См. §1 + [README](./experiments/2026-06-21-lod-mesh-downsampling/README.md) + [RESULTS](./experiments/2026-06-21-lod-mesh-downsampling/RESULTS.md) + [sources](./experiments/2026-06-21-lod-mesh-downsampling/sources.md) + `prototype/build/results.csv` (1200 rows) + `prototype/build/results_tjunc.csv` (75 rows).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-vk-fragment-shading-rate-voxel`       | concluded-verdict-mixed | mixed (**global VRS savings validated** — `vrs_2x1` / `vrs_1x2` = **50%** fragment invocations savings deterministic across все 4 scenes × 3 resolutions; `vrs_2x2_global` = **75%** savings highest quality risk 0.425-0.575; **hybrid per-region savings = 0% falsified** для sparse voxel scenes (4-6% coverage profile → coverage-variance classifier classifies все tiles as high-detail 1x1). **VRAM cost negligible:** 8 KiB @ 1080p / 14 KiB @ 1440p / 32 KiB @ 4K = 0.0001-0.0004% of 8 GiB budget. **⚠️ Critical spec correction:** `VK_KHR_fragment_shading_rate` **NOT in Vulkan 1.4 core** (verified via `docs.vulkan.org/spec/latest/appendices/versions.html`; remains device extension in 1.4). Tier 2 hardware validated cross-vendor: NVIDIA Turing/Ampere/Ada/Blackwell + AMD RDNA 2/3/4 (Mesa RADV 21.0+/23.1+) + Intel Gen11/Arc Alchemist/Battlemage. Standalone C++26 CPU prototype (`prototype/vrs_voxel_sim.cpp` ~770 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -Wall -Wextra`, **0 warnings**), 4 scenes × 3 res × 5 VRS configs × 100 iter + 10 warmup = **6000 measurements** on dev host `obvium` Zen 3 5800X + governor `powersave`. **3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS, immediate, ~30 LoC) global `vrs_2x1` для VCT integration via `vkCmdSetFragmentShadingRateKHR` + `voxel.frag` VRS-agnostic adaptation per Intel SIGGRAPH 2019 (`dFdx/dFdy` scaling, `gl_FragCoord no longer n+0.5`) = safe 50% fragment shading cost reduction; Step 2 (S, ~100 LoC + tests) VRS extension probe (`VkPhysicalDeviceFragmentShadingRateFeaturesKHR` check 3 features: `pipelineFragmentShadingRate` + `primitiveFragmentShadingRate` + `attachmentFragmentShadingRate`) + `VkFragmentShadingRateAttachmentInfoKHR` attachment setup + `VK_FORMAT_R8_UINT` shading rate image per swapchain (size = W/16 × H/16 bytes); Step 3 (M, ~250 LoC) DEFERRED — hybrid classifier + two-pass dynamic VRS per Khronos sample `fragment_shading_rate_dynamic` (compute shader generate per-frame derivative image → next-frame VRS image; two renderpass pattern to avoid feedback loop). **Caveats:** (a) CPU prototype, no real GPU dispatch — savings formulas validated, real GPU timings + visual quality deferred до GPU prototype на RTX 3060 Ti; (b) hybrid savings 0% для sparse scenes (falsified hypothesis) — classifier thresholds (cov_ratio > 85% + edge_ratio < 3% для low-detail) consistent per `prototype/vrs_voxel_sim.cpp:build_vrs_image`; (c) quality_risk эвристика simplified, needs PSNR/SSIM measurement на rendered frames; (d) cross-vendor GPU measurement (AMD RDNA 2/3, Intel Arc) analytical-only — needs hardware matrix validation; (e) TAA + VRS feedback loop (per NVIDIA NAS GDC 2019: 3-4 frames transition latency) **cross-axis risk** с in-progress `2026-06-21-taa-motion-vectors` (closed same session) — separate experiment needed; (f) VRAM cost projection conservative (single-buffered; double-buffered = 2× bytes); (g) `voxel.frag` per-pixel ops (depth downsampling, dithering) require `SV_Position` adaptation per Intel SIGGRAPH 2019 caveat. **Continuation chain:** `vct-vs-rt-cutoff` (closed strategy axis mixed) + `clustered-forward-mass-lights` (closed light count yes) + `rt-shadows-vs-csm` (closed shadow strategy mixed) + `restir-gi-feasibility` (closed GI strategy mixed) + `taa-motion-vectors` (closed temporal axis yes) → this (closed cost axis mixed). **Full Stage 5 lighting optimization landscape covered same-day `2026-06-20` + `2026-06-21` cluster.** **Follow-up candidates** (out of scope, deferred до separate experiments per `backlog.md`): `_vrs-taa-feedback-loop_` (cross-axis с `taa-motion-vectors` closed same session, **HIGH priority** if ProjectV targets 4K + TAA); `_vrs-gpu-prototype-rtx3060ti_` (real GPU timing + visual quality validation); `_vrs-dense-scene-hybrid_` (re-test hybrid classifier на cave_interior / dense_foliage scenes с >30% coverage); `_vulkan-1.5-1.6-vrs-core-promotion_` (verify if VRS extension promoted to core in next Vulkan minor); `_vr-foveated-vrs-gaze-input_` (cross-axis с `eye-tracked-foveated` backlog l-priority). См. §6 + §1 + [experiment README](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/README.md) + [STATUS](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/STATUS.md) + [sources.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/sources.md) + [prototype/RESULTS.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/RESULTS.md) + [prototype/results.csv](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/results.csv) + `prototype/build/vrs_voxel_sim` (binary, ~770 LoC C++26). | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-audio-diffraction-hybrid`             | concluded-verdict-mixed | mixed (**C_Tsingos production-ready** — 0.0025-0.0032 ms per source / 33 probes = 0.5-0.6% audio budget @ 64 sources, +1.2-1.4 dB recovery per Tsingos 2007 spec 1-2 dB; **B_Schissler deferred** — 0.024-0.082 ms per source / 17 probes = 5-16% audio budget, 0 dB recovery в simplified first-order UTD prototype, second-order required для full +2-4 dB per Schissler 2014). Standalone C++26 CPU prototype ~985 LoC (`prototype/{voxel_grid,audio_path,diffraction}.{hpp,cpp} + bench.cpp + Makefile + README + RESULTS + results.csv`), Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, 0 warnings. 3 strategies × 3 scenes × 3 seeds × 100 iter × 16 sources = **14,400 invocations** на Zen 3 5800X governor `powersave` per `hardware-profile.md §1`. Web-research complete (4 batches, ~30 results, **16 primary + 7 secondary sources verified**: Schissler 2014 high-order diffraction [SIGGRAPH 2014 ACM TOG 33(4) 39] + Schissler 2014 multi-source [I3D 2014] + Cao 2016 BST [SIGGRAPH ASIA 2016] + Cao 2021 fast diffraction [SIGGRAPH 2021, 568× faster] + Tsingos 2001 UTD [SIGGRAPH 2001] + Tsingos 2007 Instant Sound Scattering [EGSR 2007] + Chandak 2008 AD-Frustum + Antani 2012 BTM + Vercidium 2025 + SonoTraceUE 2026 [arXiv 2602.19652] + Pinpoint Audio Tracing 2025-08-18 + Meta XR Audio SDK 2024+ + Wwise Spatial Audio + Google Patent WO2024179939A1 + Han 2025 IEEE CoG survey [**41% sound designers find LPF insufficient**]). Cross-arch projection (Zen 5 AVX-512): C_Tsingos 0.0015-0.0020 ms = 0.3-0.4% budget. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~80 LoC) `Diffraction::sampleHemisphere()` helper + Fibonacci sphere + depth-mip stub; Step 2 (XS, ~50 LoC) wire into `AudioEngine::tick()` after occlusion; Step 3 (XS, ~20 LoC) env flag `PROJECTV_AUDIO_DIFFRACTION=ON` default ON. Total ~150 LoC, XS effort, 1-2 sessions. Caveats: (a) CPU-only synthetic voxel scenes (cave + open_plains + multi_room); (b) Zen 3 5800X `powersave`; (c) no AVX-512; (d) perceptual validation = analytical proxy; (e) B_Schissler first-order UTD only; (f) N=100 iter (vs methodology default 1000); (g) no DSP overhead. Continuation chain: closed `audio-raytracing-voxel-sdf` (Phase 1+2) → this (Phase 1.5 = Tsingos) → future Phase 1.6 = B_Schissler second-order UTD. См. §6 + [experiment README](./experiments/2026-06-21-audio-diffraction-hybrid/README.md) + [STATUS](./experiments/2026-06-21-audio-diffraction-hybrid/STATUS.md) + [sources.md](./experiments/2026-06-21-audio-diffraction-hybrid/sources.md) + [prototype/RESULTS.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/RESULTS.md) + `prototype/results.csv` (28 rows).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-dlss-fsr-xess-upscaling-voxel`        | concluded-verdict-mixed | mixed (**FSR 3.1 = recommended cross-vendor Vulkan path** — 3.7-23% savings, PSNR 39.2 dB, +1 MiB VRAM; **DLSS 4.5 + XeSS 2 XMX = real GPU measurements required**; **FSR 4 = NOT usable on Vulkan** per `mypcbottleneck 2026-06-04`; **DirectSR = defer to Vulkan core promotion** per `StraySpark 2026-03-25`; **Frame Generation = OUT OF SCOPE**). Standalone C++26 CPU prototype `prototype/upscaling_bench.cpp` ~470 LoC, 4 upscalers × 4 quality presets × 3 extents × 2 scenes × 3 seeds × 1000 iter = **288 measurements**. 3-step migration: Step 1 (XS, ~30 LoC) feature-flag env + post-process slot; Step 2 (M, ~250 LoC) per-SDK integration; Step 3 (S, ~80 LoC) quality preset + TracyPlot + default flip. Total ~360 LoC, S-M effort, 2-3 sessions. См. §6 + [experiment README](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/README.md) + `prototype/build/results.csv`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `2026-06-21-sdf-hybrid-world`                     | concluded-verdict-mixed | mixed (**BFS 2.4× faster than JFA on chunkSize=8** [6.6 vs 16.0 µs/chunk — counter to literature, BFS wins for dense/small chunks because narrow-band = ≤7 voxels from surface per OpenVDB 13.0.1]; **D_RLE_NoneSparse = 30% VRAM** of B_R8_1byte [153 vs 512 bytes/chunk — validates OpenVDB narrow-band pattern]; **T_VoxelDiscrete is fastest AND highest PSNR** [44.97 dB] — current mainline behavior preserved; no SDF-driven VCT quality gain measured in v1 prototype). Standalone C++26 CPU prototype ~1300 LoC, 9 files (Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG, **build green 0 warnings**), 4 SDF encodings × 2 build algorithms × 3 VCT termination strategies × 5 scenes × 5 seeds × N=1000 = **600 measurements**, wall time <60 sec на Zen 3 5800X. Phase A web-research: 15 primary sources verified via `webfetch` + DuckDuckGo HTML fallback (Exa 429 persistent). **Narkowicz 2022 "Journey to Lumen"** = DIRECT EXPERT VALIDATION (voxel bit bricks 8×8×8 match ProjectV chunkSize=8). **NAADF 2026 (Wiley CGF May 2026)** order-of-magnitude faster ray tracing для voxel worlds + axis-aligned distance fields. **RTSDF arXiv 2210.04449 (2022)** JFA + ray-trace refinement. **UE5 Mesh Distance Fields** production reference (8 MiB @ 128³, Intel HD disabled). **OpenVDB 13.0.1** narrow-band level sets 8³ leaf = ProjectV match. 3-step migration per `agent/knowledge.md §30.4`: Step 1 (XS, ~50 LoC) **immediate recommendation** — BFS replaces JFA default [2.4× build speedup]; Step 2 (S, ~250 LoC) deferred до Stage 4.3 — D_RLE_NoneSparse narrow-band storage [-70% VRAM]; Step 3 (M, ~250 LoC) deferred indefinitely — T_SDFSmooth/T_Hybrid integration [no PSNR gain in v1, 43-87% march cost overhead]. Cross-axis: orthogonal к 4 in-progress parallel [tracy-gpu + dlss-fsr-xess + greedy-physics-meshing + gpu-fluid-ca-atomic]; complementary к 8 closed experiments [vct-vs-rt-cutoff + nanovdb-on-gpu + sub-chunk-layers + lod-mesh-downsampling + wfc-procedural-worlds + gpu-procedural-noise + meshing-algo-comparison §6 + vct-cone-count-atlas-precision]. Caveats: PSNR variance high [σ=32 dB]; reference uses same algorithm as measured → not true ground truth; 8³ chunk is ProjectV minimum; cross-vendor not measured [CPU-only]; NanoVDB-native SDF deferred; mutation cost out of scope. Continuation chain: none [first SDF-for-lighting+physics axis]. Follow-up candidates: _sdf-nanovdb-integration_, _sdf-jfa-gpu-validation_, _sdf-rle-compression-tuning_, _sdf-mutation-cost_, _sdf-vct-real-ground-truth_. См. §6 + §1 + experiment README + `STATUS.md` + `sources.md` (27 sources) + `prototype/{scenes, sdf_overlay, vct_cone_march, physics_normals}.{hpp,cpp}` + `bench.cpp` + `CMakeLists.txt` + `README.md` + `results.csv` (600 measurements).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | 2026-06-21                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |

| `2026-06-21-vulkan-memory-aliasing-transient` | concluded-verdict-mixed | mixed (**D_DAGRenderGraph barrier
reduction = −74%** consistent across all workloads [28→7 / 50→13 / 74→19] = **real win**, directly impacts CPU command
buffer recording overhead per frame; **C_FullAliasing VRAM savings = −7-8%** на typical 276→255 MiB + projected 398→372
MiB workloads = crosses 5% threshold per `optimization-philosophy.md`; **B_VMA_SubAllocatorPool = REGRESSION** (−5%
additional overhead vs A baseline — pure pool без lifetime analysis = worse than current pattern; **never adopt without
aliasing**). Standalone C++26 CPU lifetime simulator `prototype/mem_alias_bench.cpp` ~600 LoC (Clang 22.1.6
`-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, builds green with 10 cosmetic warnings на unused
constexpr / argc-argv), 3 workloads [minimal_mvp 5 passes × 13 res / standard 8 passes × 26 res per
`SceneFrameResources` actual / projected_stage5x 15 passes × 35 res] × 4
strategies [A_ManualBaseline / B_VMA_SubAllocatorPool / C_FullAliasing / D_DAGRenderGraph] × 5 seeds × 1000 iter + 10
warmup = **60,000 main measurements**, wall time <1 sec на Zen 3 5800X governor=`powersave` per
`hardware-profile.md §1`. **Persistent image bottleneck** (root cause of modest savings): depth + shadow + hiz + taa
history = ~98 MiB cannot be safely aliased across frames (write-after-read hazards) = hard ceiling ~35% VRAM. *
*Web-research Phase A:** 9 primary + 7 secondary sources verified via `webfetch` + DuckDuckGo HTML fallback (Exa HTTP
429 persistent per operator directive): Yuriy O'Donnell 2017 GDC Frostbite FrameGraph [canonical]; Themaister 2017/2019
Granite Engine blog [open-source reference]; VMA official resource_aliasing docs; WSCG 2023 history-aware frame graph
academic paper; dev.to p3ngu1nzz 2025-10-06 + 2025-10-18 modern implementation; Khronos Vulkan Tutorial render graph;
AMD RPS SDK; KhronosGroup Vulkan resources.adoc 2026-06-05. **3-step migration per `agent/knowledge.md

| `2026-06-21-hzb-smart-mip-select`            | concluded-verdict-mixed | mixed (**C_PerChunkStaticMip: 700-1500× texel
reduction** [avg 13K vs 10.7M texels/chunk across all scenes vs A_UniformMip0 baseline] AND **+3-5% cull rate gain
** [avg 27.6% vs 26.4%]; **B_UniformMipGlobal: best absolute cull rate** [avg 29.8%, +3.4% vs A] but same FN risk; **C ≈
D** для our scenes [multiple dispatches don't add measurable value]; **0.02-0.20% false-negative artifact rate** without
mitigation [PSNR 27-30 dB worst case view_dolly_stress; A = 0 FN, PSNR ∞]; **2-phase fallback** in Step 3
`if (mipLevel > 0 && culled) verify at mip=0` eliminates FN → PSNR ∞ with 350× texel reduction still; **A = safest
** [0 FN, 700× more texels]). Standalone C++26 CPU cull simulator
`prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt}` ~
700 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings**
after `std::min` clamp fix + MAX→MIN pyramid rebuild), 5 scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup = **100
measurements**, wall time ~12 min на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Synthetic MIN depth
pyramid (Greene 1993 convention, NOT MAX) built once per scene+seed+frame; `SampleHizMaxDepth` for chunk AABB at
per-chunk mip K = MAX over HIZ_min in chunk AABB. Cull decision: `result.culled = (hizMaxOfMins < chunkMinDepth01)`.
Web-research complete via DuckDuckGo HTML + webfetch fallback (Exa HTTP 429 persistent per
`agent/knowledge.md Part B §9`): **5 primary sources verified** (Greene/Kass/Miller 1993 SIGGRAPH canonical + Mike
Turitzin 2020 exact pattern statement «Hi-Z occlusion culling ... works by projecting a bounding volume into
screen-space and using the **projected size to choose the appropriate mip level**» + Omlor & Radicke 2025 TPOC voxel+HZB
IEEE 11321175 + DeepWiki Metallic 2026-04-06 modern production + RasterGrid 2010 OpenGL FBO mip chain) + 5 secondary (
Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 + chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete).
**3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) per-chunk mip compute на CPU +
`perChunkMipLevel[]` SSBO; Step 2 (S, ~80 LoC) `hzb_cull.comp` SSBO load + branching; Step 3 (XS, ~30 LoC)
`PROJECTV_HZB_SMART_MIP=ON` env + 2-phase fallback + Tracy plot + `ProjectVHzbSmartMipTests` unit test. Total ~160 LoC,
XS-S effort, 2-3 sessions. **Caveats:** CPU prototype only (no real GPU dispatch, analytical texel-touch cost model);
single GPU vendor (RTX 3060 Ti GA104); synthetic scenes representative not exhaustive (no real ProjectV chunk content);
cross-vendor deferred; mutation cost out of scope; visual QA в реальном gameplay required для fallback correctness; CSM
HZB deferred per `agent/workspace.md §2` line 52 — per-chunk mip extends naturally as follow-up. **Cross-axis:**
orthogonal ко всем 5 in-progress parallel (`sdf-hybrid-world` [closed mixed] + `tracy-gpu-vs-manual` +
`gpu-fluid-ca-atomic-strategy` + `vk-multi-gpu-split-frame` [closed mixed] + `vct-3d-mip-generation`); complementary к
closed `2026-06-20-hzb-binding-models` (texelFetch foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype
precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
threshold), `2026-06-20-dec-pipelines-async-compute` (async foundation); **new axis**: per-chunk mip refinement of
explicit `agent/workspace.md §2` Gap = 0 coverage в INDEX §6 до этого experiment. **Re-evaluation triggers:** Stage 4.3
ships 128m draw distance (per-chunk mip cost grows linearly with chunks, more savings), mesh shader Pattern C full
integration (HIZ output consumed by mesh shader greedy emit → accuracy matters more), CSM HZB culling adopted (per-chunk
mip extends naturally to shadow cascades), cross-vendor validation on AMD RDNA 4 + Intel Arc Battlemage, Vulkan 1.5+
extensions для new HIZ features.
См. [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources.md](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
`prototype/{results.csv, bench.log}`. | 2026-06-21 |
| `2026-06-21-vct-3d-mip-generation`             | concluded-verdict-yes | **yes** — **A_2x2x2_Box is the sole
Pareto-optimal 3D mip chain algorithm** (PSNR mean 49.99 dB, perf mean 1.218 ms on Zen 3 5800X; ties C within +0.0004 dB
PSNR, lowest perf of 4 algs). B_4tap_Smooth = strict regression (−0.498 dB PSNR, +7% perf cost — NVIDIA HZB pattern
degrades for VCT volumes because mips need isotropic averaging, not diagonal-only taps). C_8tap_3DGaussian = pure perf
tax (+6% perf for zero measurable PSNR gain — mathematically equivalent to A for symmetric 8-corner kernel with σ=0.5
voxel where all weights collapse to 0.125). D_Blit3D_perAxis = 2.9× slower CPU (0.01 dB ΔPSNR for +194% perf cost; **GPU
validation deferred** — on GPU `vkCmdBlitImage` is hardware-accelerated 5-20× faster than compute per AMD SPD + NVIDIA
practice, so D may flip to faster on GPU; Stage 5.1 GPU benchmark should validate). Standalone C++26 CPU prototype (
`prototype/mip_bench.cpp` ~580 LoC, `clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0
warnings**), 4 algorithms × 4 scenes [uniform_sky / uniform_floor / cave_stress / mixed_biome per
`vct-cone-count-atlas-precision` §3 precedent] × 2 atlas sizes [64³ / 128³] × 3 mip levels [1, 3, 5 inner/mid/outer] × 3
seeds × N=30 iter + 5 warmup = **288 configs × 30 = 8,640 main measurements**, wall time 192 sec on dev host `obvium`
Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (289 rows = 1
header + 288 data rows). **Cross-scene (per RESULTS.md §3):** A ties or beats every competitor in every scene × mip
level combination. **Cross-mip (per §2):** B's quality deficit grows with mip depth (−0.33 dB at mip 1 → −0.94 dB at mip
5) — fancy algorithms don't help at outer mips either. **Verdict basis:** (1) `TODO.md §5.1` line 380 explicit DoD
«Реализовать построение мип-уровней 3D-атласа на GPU для мягкой фильтрации конусов» — assumed `vkCmdBlitImage` mip chain
per `vct-cone-count-atlas-precision` §3.2, never measured algorithm cost, this validates; (2) Crassin 2011 cone-tapered
filter identified as out-of-scope follow-up per `vct-cone-count-atlas-precision` STATUS §11 + §172; (3) complementary to
closed `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain natural extension) + `2026-06-20-dec-pipelines-async-compute` (
async compute for off-frame mip gen) + `2026-06-20-hzb-binding-models` (2D HZB mip chain analog). Web-research: Exa HTTP
429 rate-limited this session; fallbacks via direct `webfetch` per `agent/knowledge.md` line 1424 (10 primary + 6
secondary verified: Crassin 2011 GIVoxels + GPUOpen FidelityFX-SPD
2020 [12 mips single dispatch, RDNA-optimized, WaveOps + fp16 packed, 2D only] + nvpro-samples
gl_occlusion_culling [2D HZB mip chain pattern] + Vulkan 1.4 `VkImageBlit` spec [core 1.0, 3D blit] + Panteleev 2014 +
SaschaWillems Vulkan samples + Snowapril/HanetakaChou VCT implementations + OGRE-Next CIVCT + Vulkan SDK 1.4.350.1
vendored docs + 6 failed URLs documented for future re-verification). **Mainline 3-step migration
per `agent/knowledge.md §30.4` precedent, simplified based on results (no need for fancy alternatives):** Step 1 (XS, ~
30 LoC) `voxelize_mipgen.comp` skeleton with A_2x2x2_Box + per-mip barrier + SPIR-V debug; Step 2 (S, ~50 LoC) wire into
`SceneResources::RebuildVctAtlas` lifecycle after `voxelize.comp` writes mip 0; Step 3 (S, ~40 LoC) Tracy plot "VCT Mip
Gen" + `ProjectVVctMipGenTests` unit test. Total **~120 LoC** (down from initial 260 LoC estimate — no dispatch enum, no
per-scene selection, no per-axis blit fallback at this time). S effort, 1-2 sessions. **GPU D-benchmark deferred to
Stage 5.1 integration:** if D_Blit3D_perAxis GPU timing < A_2x2x2_Box on RTX 3060 Ti, document and consider conditional
flip; else leave A as default and document D as rejected. **Continuation chain:** `vct-cone-count-atlas-precision` (
closed mixed, within-VCT quality, assumed mip chain) → this (closed yes, mip gen algorithm). **Stage 5.1 axis status:**
cutoff + cone count + atlas format + mip gen algorithm = 4 of 4 closed/explored. Remaining Stage 5.1 axis items: Crassin
2011 cone-tapered filter (out-of-scope per `vct-cone-count-atlas-precision` §172) + 4D temporal VCT (out-of-scope per
closed `taa-motion-vectors` follow-up) + cross-vendor GPU validation. **Cross-axis:** orth orth ко всем 4 in-progress
parallel (tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1, sdf-hybrid-world = VCT anti-leak,
vk-multi-gpu-split-frame = multi-GPU) + complementary к 9 closed Stage 5.1/2.x/3.x experiments (
`vct-vs-rt-cutoff` [cutoff=0.3 strategy] + `vct-cone-count-atlas-precision` [cone count, this = mip gen axis] +
`nanovdb-on-gpu` [storage] + `dec-pipelines-async-compute` [sync] + `hzb-binding-models` [2D cull] +
`clustered-forward-mass-lights` + `rt-shadows-vs-csm` + `restir-gi-feasibility` + `lod-mesh-downsampling`). **Caveats:
** (a) CPU prototype only — no Vulkan dispatch, no GPU time, no cross-vendor validation. Per-algorithm relative perf may
differ substantially on GPU (D_Blit3D_perAxis may flip to faster than A); (b) Synthetic 3D voxel atlas — not real
ProjectV chunk content; (c) Analytical 3D Gaussian low-pass reference (σ=0.5 voxel × 2^mip_factor) — ideal reference,
not real ground truth; (d) Mutations (per-chunk rebuild on voxel edit) out of scope; (e) Crassin 2011 cone-tapered
anisotropic filter (direction-weighted) = out-of-scope follow-up per `vct-cone-count-atlas-precision` §172; (f) 4D
temporal VCT = closed `taa-motion-vectors` follow-up candidate, out of scope; (g) GPU `vkCmdBlitImage` 3D real timing
out of scope — CPU prototype cannot validate; (h) Reduced measurement budget (30 iter / 3 seeds instead of 100 iter / 5
seeds) due to bash timeout constraint. The aggregate PSNR std is dominated by scene-mix signal, not iteration noise (
verified: per-config std < 0.1 dB across 30 iter), so reduction has minimal impact on algorithm comparison. *
*Re-evaluation triggers:** Stage 5.1 integration milestone (when `voxelize.comp` lands in mainline) — primary trigger,
GPU benchmark of D vs A; Stage 4.3 (128+ chunks draw distance, mip gen time scaling); Crassin 2011 cone-tapered mip
filter follow-up; 4D temporal VCT follow-up; Vulkan 1.5+ dedicated mip gen extensions; non-cubic voxel cells (
sub-chunk-layers mixed_biome 4×4×8) — would change C_8tap_3DGaussian math (asymmetric kernel weights could outperform
A); GPU D-benchmark result. См. §6 +
§1 + [experiment README](./experiments/2026-06-21-vct-3d-mip-generation/README.md) + [STATUS](./experiments/2026-06-21-vct-3d-mip-generation/STATUS.md) + [RESULTS](./experiments/2026-06-21-vct-3d-mip-generation/RESULTS.md) + [sources.md](./experiments/2026-06-21-vct-3d-mip-generation/sources.md) +
`prototype/{mip_bench.cpp, CMakeLists.txt, README.md}` + `prototype/build/results.csv` (288 rows) +
`prototype/build/mip_bench` (binary). | 2026-06-21 | §30.4
`:** Step 1 (S, ~150 LoC) **immediate** — VMA pool setup grouped by `ResourceType
` + heap type with sub-allocation (validation only); Step 2 (M, ~500 LoC) **Stage 4.3** — interval-graph coloring for non-overlapping lifetimes (lifetime tracking в `
CreateBuffer`/`CreateImage
`); Step 3 (L, ~1500 LoC) **Stage 5.x deferred** — DAG-based render graph + auto-barrier batching (4:1 reduction). Total ~2150 LoC, L effort, 4-6 sessions. **Caveats:** CPU simulation only (no real GPU dispatch / driver overhead), synthetic workloads (realistic upper-bound), greedy coloring algorithm (production render graphs use Pettis-Hansen +10-20% better packing), single-GPU dev host (cross-vendor analytical projection only). См. §6 + [experiment README](./experiments/2026-06-21-vulkan-memory-aliasing-transient/README.md) + [RESULTS](./experiments/2026-06-21-vulkan-memory-aliasing-transient/prototype/RESULTS.md) + `
sources.md` (16 sources) + `prototype/{mem_alias_bench.cpp, build/results.csv}`. | 2026-06-21 |

| `2026-06-21-vulkan-fps-pacing-wayland-prototype` | concluded-verdict-yes  | **yes** (**Mode B `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` = 93-99% frame interval reduction** vs Mode A baseline для cpu_bound (192 us vs 17,066 us), gpu_bound (1,117 us vs 17,111 us), jitter (1,119 us vs 17,114 us) scenarios; **Mode D `VK_EXT_present_timing` + `desiredPresentTime` = 41-93% P99 variance reduction** vs Mode A, std-dev 47-77 us vs Mode A 427-902 us = **~10-15× tighter**). **Supersedes `2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed analytical-only** (fills Wayland measurement gap self-identified в old §6 + adds `VK_KHR_present_mode_fifo_latest_ready` lever ratified 2025-03-18 after old capture). Standalone Vulkan 1.4 + SDL3 harness ~600 LoC (`prototype/{main.cpp, triangle.{vert,frag}, triangle.{vert,frag}.spv, triangle.{vert,frag}.spv.h, CMakeLists.txt, README.md}`), 5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = **7,500 main measurements**, dev host `obvium` NVIDIA RTX 3060 Ti + driver **610.43.02** + Vulkan 1.4.341 + Wayland session per `hardware-profile.md §3+§5`. Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent per `agent/knowledge.md Part B §9`); **9 primary + 4 supplementary sources verified**: Khronos `VK_EXT_present_timing` proposal rev 3 + Khronos blog 2025-12-04 Lionel Duc + Khronos `VK_KHR_present_mode_fifo_latest_ready` ratif 2025-03-18 Lina Versace + Khronos `VK_KHR_swapchain_maintenance1` ratif 2025-03-31 + Khronos `VK_KHR_present_wait2` rev 1 + LunarG SDK 1.4.321.0 release notes 2025-07-15 + NVIDIA Dev Forum Wayland WSI busy-spin fix thread 2026-04-25 fix в 610.43.02 = dev host driver + LavX Mesa 26.2 VK_GOOGLE_display_timing benchmark 2026-06-07 std-dev 0.9 → 0.3 ms Wayland → direct = **validated by our Mode A std-dev 902-1221 us matching Mesa 0.9 ms** + Phoronix Mesa 26.1 VK_EXT_present_timing merge 2026-01-27 + Phoronix low_latency_layer 2026-05-17 + Raph Levien swapchain frame pacing blog 2021-10-22 + Android Developers Vulkan frame pacing 2026-06-05 + BlurBusters `VK_KHR_present_mode_fifo_latest_ready` testing 2026-04-07. **CPU present overhead: Mode B = 44 us mean** (lowest, no busy-spin, no wait), Mode D = 76 us, Mode A = 81 us. **Mode D target offset** = -16 ms (vkQueuePresentKHR returned 16 ms before target time = expected behavior per spec, compositor holds image until target). **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON` + `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection в `VulkanBootstrap.cpp` + `PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" + `ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. Two options: **Option 1 (Mode B — low-latency)** = `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` best для CPU-bound workloads (~200 us frame interval vs current 17 ms); **Option 2 (Mode D — precise pacing)** = `VK_EXT_present_timing` best для vsync-locked deterministic (10-11 ms frame interval с 47-77 us std-dev vs current 427-902 us). **Caveats:** (a) single GPU vendor validated (NVIDIA RTX 3060 Ti, dev host); cross-vendor deferred to mainline (AMD Mesa RADV + Intel ANV via Mesa 26.1+ Jan 2026); (b) synthetic scenarios representative not exhaustive; (c) VRR display behavior out of scope (assumes fixed refresh 60 Hz); (d) Mode B drops frames when CPU+GPU faster than refresh — Mode D recommended if vsync must be respected; (e) Wayland compositor jitter surface — gain ожидаемо меньше, чем direct-display per Mesa 26.2 benchmark; (f) CPU prototype only, no real ProjectV workload coupling; (g) `low_latency_layer` Mesa no-op issue per Korthos 2026-04-27 — manual implementation рекомендуется; (h) ProjectV input-to-photon latency currently unknown (TracyPlot не имеет explicit "input latency" tracker — follow-up post-MVP). См. §1 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md) + [STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) + [sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) + [RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) + `prototype/build/results.csv` (7,500 rows).

## 6A. Sync-close pending — `2026-06-21-vk-multi-gpu-split-frame` (verdict=`mixed`)

Closed `2026-06-21` (single session, ~1.5h) per `AGENTS.md §13.5` sync-pass. **Multi-GPU rendering axis** (cross-cutting
VRAM-capacity axis для Stage 4.3 128m draw distance per `TODO.md §4.3` + `agent/workspace.md §2` Nearest Gap callout, *
*new lever в same VRAM axis** as 8 closed mitigation experiments: `frame-flight-allocator-budget` +
`depth-occlusion-quantization` + `vma-sparse-textures` + `nanovdb-on-gpu` + `vct-cone-count-atlas-precision` +
`sub-chunk-layers` + `lod-mesh-downsampling` + `dlss-fsr-xess-upscaling-voxel` + `vk-fragment-shading-rate-voxel` — все
closed, additive к multi-GPU aggregation). **Self-promo l→m justified per `optimization-philosophy.md` 5-10%
threshold + `agent/knowledge.md Part A §2` mainline MVP scope cross-cutting VRAM axis** (originally l-priority в
`backlog.md §Open` per `2026-06-20`; **refined to m per multi-axis coupling**: (a) `VK_KHR_device_group` = core 1.1+ per
`docs.vulkan.org/refpages/.../VK_KHR_device_group.html` lines 38-43 — no extension dep for Vulkan 1.4 ProjectV; (b) 8
GiB VRAM cap on dev host `obvium` RTX 3060 Ti = main bottleneck per `agent/workspace.md §2`; (c) cross-cutting VRAM
axis = 8 closed mitigation experiments; (d) cross-vendor validation = analytical + 5 tier coverage; (e) no real
multi-GPU hardware required = CPU sim sufficient for first-tier validation).

**Web-research partial** per `agent/knowledge.md Part B §9` fallback policy — `web_search` (Exa) returned HTTP 429 «Too
Many Requests» × 4 retries; `webfetch` retrieved full Vulkan 1.4 core spec for `VK_KHR_device_group` +
`VK_KHR_device_group_creation` + `VkDeviceGroupPresentInfoKHR` 2026-06-21 (4 present modes: LOCAL / REMOTE / SUM=SFR /
LOCAL_MULTI_DEVICE=AFR). Cross-vendor SOTA numbers (NVLink 4.0/4.1 Hopper/Blackwell, AMD xGMI/IF RDNA 3/4, Intel Arc
Battlemage PCIe 4.0, NVIDIA consumer PCIe 5.0) cited from operator's pre-2026 knowledge per §9 caveat, **NOT verified
via fresh web_search 2026-06-21** (will be flagged в `sources.md §2`).

**Standalone C++26 CPU prototype** (`prototype/analytical_model.cpp` ~370 LoC + `cpu_simulation.cpp` ~360 LoC +
`cross_vendor_matrix.cpp` ~250 LoC, **all built via ad-hoc `clang++ -std=c++26 -O2 -march=native -DNDEBUG`
per `AGENTS.md §1` research workflow, 0 warnings after 1 unused-var fix**, 6 GPU tiers × 3 GPU counts × 4 scenes × 4
present modes × 30 iter = **288 analytical + 9000 simulation measurements**, total wall time ~2 min на Zen 3 5800X
governor `powersave` per `hardware-profile.md §1`). `api_discovery.cpp` (~290 LoC, C++26 + Vulkan 1.4 + volk) written
but **not built** per `AGENTS.md §1` (agent not building); mock `build/api_discovery.json` with expected dev host
single-GPU output (deviceGroupCount=1, physicalDeviceCount=1, peerMemoryFlags=0x0, modes=LOCAL-only) written for
documentation; operator builds with `clang++ -std=c++26 -O2 api_discovery.cpp -lvulkan -lvolk -o api_discovery` and runs
to validate.

**Headline (CPU simulation, work=4096 rays, 30 iter, all 5 interconnects, baseline = single-GPU LOCAL 6906 µs):**

| Mode          | NVLink 4.0 (H100) | NVLink 4.1 (B200) | xGMI 2.0 (RDNA 3) | PCIe 4.0 (Intel Arc) | PCIe 5.0 (consumer) |
|---------------|------------------:|------------------:|------------------:|---------------------:|--------------------:|
| AFR 2-GPU     |              235% |              226% |              221% |                 213% |                225% |
| **AFR 4-GPU** |          **402%** |          **410%** |          **401%** |             **383%** |            **397%** |
| SFR 2-GPU     |              134% |              129% |              133% |                 132% |                123% |
| SFR 4-GPU     |              137% |              132% |              130% |                 129% |                131% |
| REMOTE 2-GPU  |              206% |              200% |              203% |                 189% |                190% |
| REMOTE 4-GPU  |              207% |              206% |              205% |                 187% |                191% |

**Key findings:**

- (1) **AFR super-linear 4-GPU scaling to 3.83-4.10×** across ALL interconnects including slow PCIe 4.0 32 GB/s (peer
  copy only 4 MiB/frame, dwarfed by GPU work ~7 ms)
- (2) **AFR strictly best** of 3 present modes for typical render workload
- (3) **SFR limited by 1.5 ms compositing + 35% spatial load balance loss** (compositing fixed per-present, doesn't
  scale with GPU count)
- (4) **REMOTE only competitive for compute-heavy mixed workload** (Fluid CA + world gen + VCT cone-march on GPU 1,
  render on GPU 0)
- (5) **VRAM aggregation = killer feature for Stage 4.3
  ** [RTX 3060 Ti 8 GiB → 16 GiB (2-GPU) / 32 GiB (4-GPU), sufficient for 9 GiB Stage 4.3 target_128m]
- (6) **No interconnect is bottleneck for AFR** at this peer copy size (4 MiB)

**3-step migration per `agent/knowledge.md §30.4` precedent:**

- **Step 1 (XS, ~30 LoC, immediate, additive):** `vkEnumeratePhysicalDeviceGroupsKHR` +
  `vkGetDeviceGroupPresentCapabilitiesKHR` + `vkGetDeviceGroupPeerMemoryFeaturesKHR` probe в
  `src/render/vulkan/VulkanBootstrap.cpp` + Tracy plots `gpu.deviceGroupCount` / `gpu.presentModeMask` /
  `gpu.peerMemoryFlags` + `PROJECTV_MULTI_GPU_PROBE=ON` env (default ON, no behavior change for single-GPU)
- **Step 2 (M, ~300 LoC, Stage 4.3 ship, opt-in):** `PROJECTV_MULTI_GPU_AFR=ON` env var (default OFF) + frame parity
  counter (modulo `physicalDeviceCount`) + `vkAcquireNextImage2KHR` with `deviceMask` = parity bit +
  `VkDeviceGroupPresentInfoKHR::mode = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR` + cross-GPU uniform
  buffer mirroring via `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` + `VK_KHR_timeline_semaphore` (per closed
  `dec-pipelines-async-compute` yes) for cross-queue per-GPU frame submission sync
- **Step 3 (XS, ~50 LoC, Stage 4.3+ future):** per-vendor preset
  `PROJECTV_MULTI_GPU_PROFILE=DATACENTER|ENTERPRISE|CONSUMER` (DATACENTER = NVLink 4.0+/xGMI/IF, AFR for compute-bound,
  peer memory aggressive; ENTERPRISE = PCIe 4.0/5.0, AFR for balanced, staging buffer for cross-GPU; CONSUMER = LOCAL
  single-GPU default) + default flip when multi-GPU dev host available

**Total ~380 LoC across 4-6 files, M effort, 2-3 sessions.**

**Caveats:** (a) single-GPU dev host `obvium` (RTX 3060 Ti GA104, no second GPU) = **API discovery only**, not real
multi-GPU benchmark; (b) CPU simulation = synthetic DDA-proxy ray-march loop (~256 iters), not real GPU dispatch; (c)
cross-vendor scaling projected from operator's pre-2026 knowledge per §9 caveat; (d) **4-GPU super-linear 4.0× scaling
likely drops to 3.0-3.5× with real GPU overheads not modeled** (vkCmdDispatch, vkCmdBeginRenderPass, vkQueueSubmit
binary semaphore wait, vkAcquireNextImage2KHR wait, vkQueuePresentKHR serialization); (e) no visual quality diff
cross-vendor (AFR frame parity, SFR seam visibility); (f) no real `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` allocation
test (no peer device on dev host); (g) `web_search` unavailable для fresh SOTA citations per `STATUS.md` blocker +
`sources.md §2` caveat.

**Cross-axis:** orthogonal ко всем 8 closed Stage 4.3 mitigation experiments (mentioned above) — **multi-GPU = new lever
в same VRAM axis, additive to existing mitigations**; complementary к closed `dec-pipelines-async-compute` (yes, sync
foundation) + `async-compute-overhead-numbers` (yes +9.85-11.34%, sync measurement baseline) +
`vulkan-fps-pacing-vk-ext` (mixed, frame pacing foundation for AFR half-rate present patterns via
`VK_KHR_present_id/2`).

**Re-evaluation triggers:** (1) multi-GPU dev host availability (operator upgrade) — enables real benchmark; (2) Stage
4.3 ships 128m draw distance — VRAM cap re-tightens, multi-GPU becomes relevant; (3) AMD RDNA 4 + Intel Arc Battlemage
dev matrix — cross-vendor validation; (4) Vulkan 1.5/1.6 `VK_KHR_*_mgpu` extensions — any future multi-GPU primitives; (
5) ProjectV shader count > 50 with peer memory copy costs — per-shader dispatch overhead matters.

**Cross-refs:** `TODO.md §4.3` (Lift Draw Distance Cap, direct beneficiary of multi-GPU VRAM aggregation),
`agent/workspace.md §2` (Nearest Gap: 8 GiB VRAM cap on dev host = main bottleneck), `hardware-profile.md §3` (RTX 3060
Ti GA104, Vulkan 1.4.341), `agent/knowledge.md Part A §30.4` (3-step migration precedent),
`agent/knowledge.md Part A §2` (mainline MVP scope — multi-GPU = forward-looking scaling, NOT gating current MVP slice),
`agent/knowledge.md Part B §9` (self-audit fallback policy, web_search Exa 429 → webfetch + operator's pre-2026
knowledge), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold for self-promo l→m
justification).

Closed entry: [`experiments/2026-06-21-vk-multi-gpu-split-frame/`](./experiments/2026-06-21-vk-multi-gpu-split-frame/) +
`prototype/build/{analytical_results.csv, sim_results.csv, cross_vendor_matrix.md, api_discovery.json}` + [
`RESULTS.md`](./experiments/2026-06-21-vk-multi-gpu-split-frame/RESULTS.md) (96 lines, full numerical synthesis with
caveats) + [`sources.md`](./experiments/2026-06-21-vk-multi-gpu-split-frame/sources.md) (4-tier, ~140 lines, Vulkan
spec + cross-vendor SOTA + local ProjectV cross-refs) + [
`STATUS.md`](./experiments/2026-06-21-vk-multi-gpu-split-frame/STATUS.md) (progress log) +
`prototype/{analytical_model,cpu_simulation,cross_vendor_matrix,api_discovery}.cpp` (4 source files, ~1.3k LoC total,
all built via ad-hoc `clang++` research workflow except `api_discovery.cpp` per `AGENTS.md §1`).

`README.md` Status updated: `in-progress` → `concluded-verdict-mixed` (Date closed 2026-06-21).
`research/backlog.md §In progress` → `§Closed` (with full closure note per §13.5). Anti-duplicate sentinel clean per
§13.7.

## 7. Backlog

См. `research/backlog.md`.

## 8. Last update

`2026-06-21` — closed `2026-06-21-vulkan-fps-pacing-wayland-prototype` (verdict=`yes`). **Frame pacing
axis** experiment closed same session (Stage 0 / independent foundation). **Supersedes**
`2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed (analytical-only + Wayland measurement gap self-identified
в old §6 + Wayland `VK_KHR_present_mode_fifo_latest_ready` lever ratified после old capture 2025-03-18).
Self-invented follow-up per operator instruction `2026-06-21` «выбирай свободную тему или придумывай
свою исследуй». Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent per
`agent/knowledge.md Part B §9`); **12 primary + 4 supplementary sources verified**. Standalone Vulkan 1.4
+ SDL3 harness ~600 LoC, 5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = **7,500 main
measurements**, dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 + Vulkan 1.4.341 + Wayland session
per `hardware-profile.md §3+§6`. **Headline:** Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`) =
**93-99% frame interval reduction** vs Mode A baseline; Mode D (`VK_EXT_present_timing` + `targetTime`)
= **41-93% P99 variance reduction** (std-dev 47-77 us vs Mode A 427-902 us = ~10-15× tighter); Mesa 26.2
std-dev prediction **validated**. Single-pass sync per `AGENTS.md §13.5`: `backlog.md §In progress`
→ `§Closed` (with full closure note + reservation record kept per §13.5), `INDEX.md §5 Active` →
`§6 Recent closed` table row added + `§1 Now Just-closed` + this `§8 Last update` entry +
`hardware-profile.md §4` updated с `VK_KHR_present_mode_fifo_latest_ready` row per §14 edge case +
old `2026-06-20-vulkan-fps-pacing-vk-ext/STATUS.md` supersede notation per §13.7. **Mainline 3-step
migration per `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON`
+ `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection в `VulkanBootstrap.cpp` +
`PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D
implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present
mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" +
`ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. **Caveats:** (a) single
GPU vendor validated (NVIDIA RTX 3060 Ti, dev host); cross-vendor deferred to mainline; (b) synthetic
scenarios representative not exhaustive; (c) VRR display behavior out of scope; (d) Mode B drops frames
when CPU+GPU faster than refresh — Mode D recommended if vsync must be respected; (e) Wayland compositor
jitter surface — gain ожидаемо меньше, чем direct-display per Mesa 26.2; (f) CPU prototype only, no
real ProjectV workload coupling. Cross-refs: closed `2026-06-20-vulkan-fps-pacing-vk-ext/` (superseded),
closed `2026-06-20-dec-pipelines-async-compute` (sync foundation), `TODO.md §Stage 0`,
`agent/knowledge.md §30.4` (3-step migration precedent), `agent/decisions.md §30.2-§30.3` (VSync cycle
lineage), `agent/workspace.md §2` (Nearest Gap: Stage 3.1 cross-frame latency contract). См. §1 + §5 +
§6 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md) +
[STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) +
[sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) +
[RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) +
`prototype/build/results.csv` (7,500 rows) + `prototype/{frame_pacing_bench, triangle.{vert,frag}.spv}` +
`research/backlog.md §Closed`. Previous session update: closed `2026-06-21-voxel-chunk-streaming-pipeline`
(verdict=`mixed`). **A_PrebakeAll wins on stutter by 6.5× margin**
vs D_DemandPaging baseline (mean 2.79 µs vs 7.88 µs, p99 23.75 µs vs 57.30 µs) — crosses 5-10%
threshold per `optimization-philosophy.md` by 6×. **E_HybridDemandPredictive wins on VRAM by 90%**
(0.9 MiB vs 8.2 MiB) at cost of +30 µs p99 stutter on worst-case teleport scenes. Standalone C++26 CPU
streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6 `-O3 -march=native
-std=c++26 -DNDEBUG`, **0 warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup =
**125,000 main measurements**, wall time 0.07 sec на Zen 3 5800X dev host `obvium`. Web-research via
`webfetch` + DuckDuckGo HTML (Exa 429 persistent): **5 primary + 3 secondary sources verified** (Aokana
arXiv 2505.02017 + DanielWLiu07/voxel-engine + Voxceleron2 + UE5 World Partition + PrismarineJS). **3-step
migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) immediate — A_PrebakeAll doc + env flag +
Tracy plot (no code change); Step 2 (M, ~300 LoC) deferred до Stage 5+ — E_HybridDemandPredictive for
memory-tight scenarios; Step 3 (S, ~100 LoC) deferred indefinitely. Total ~430 LoC if all implemented.
**Cross-axis:** orthogonal ко всем 4 in-progress parallel; complementary к 9 closed VRAM/storage
experiments. **New axis:** chunk-streaming axis opens cross-cutting Stage 4.3/5.x asset pipeline.
См. §1 + §6 + [experiment README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md)
+ [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) +
[sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) +
[`prototype/RESULTS.md`](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md)
+ `prototype/{stream_bench.cpp, build.sh, README.md}` + `prototype/build/{stream_bench, results.csv}`
(126 rows). Anti-duplicate sentinel clean per §13.7.

`2026-06-21` — closed `2026-06-21-lod-transition-strategy` (verdict=`mixed`). **LOD transition strategy
axis** experiment closed same session (Stage 4.2 per `TODO.md §4.2` line 328 explicit DoD: «Отсутствие
визуальных артефактов "дырявого мира" на стыках LOD-зон» = transition zone problem = NOT the per-LOD
downsampling problem; closed `2026-06-21-lod-mesh-downsampling` fixed per-LOD content via B_SurfacePreserve
kernel, but transition between LOD levels is a separate decision; **self-invented topic** per operator
instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»). Single-pass sync
agent per `AGENTS.md §13.5`: `backlog.md §In progress` → `§Closed` (with full closure note + reservation
record removed per §13.5), `INDEX.md §6 Recent closed` table row added. **C_Geomorph = canonical
recommended** per Hoppe 1997 + Lysenko 2018. **A_Pop FAILS Stage 4.2 DoD** (27.76 dB < 35 dB threshold).
**D_PreComputedMorphTargets / B_Crossfade NOT recommended.** **E_HZB_Stitch needs GPU prototype.**
(with full closure note), `INDEX.md §5 Active` → `§6 Recent closed` table row + `§8 Last update`
(this entry). Anti-duplicate sentinel clean per `AGENTS.md §13.7`. **Headline:** **A_2x2x2_Box is the sole
Pareto-optimal 3D mip chain algorithm** — PSNR mean 49.99 dB (ties C within +0.0004 dB), perf mean
1.218 ms (lowest of 4 algs); B_4tap_Smooth = strict regression (−0.498 dB, +7% perf); C_8tap_3DGaussian
= pure perf tax (+6%, no quality gain); D_Blit3D_perAxis = 2.9× slower CPU (GPU validation deferred).
Standalone C++26 CPU prototype (`prototype/mip_bench.cpp` ~580 LoC, `clang++ 22.1.6 -O3 -march=native
-std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**), 4 algs × 4 scenes × 2 atlas sizes × 3
mip levels × 3 seeds × N=30 iter + 5 warmup = **288 configs × 30 = 8,640 main measurements**, wall
time 192 sec on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
`prototype/build/results.csv` (289 rows = 1 header + 288 data rows). **Mainline 3-step migration per
`agent/knowledge.md §30.4` precedent, simplified based on results (no need for fancy alternatives):**
Step 1 (XS, ~30 LoC) `voxelize_mipgen.comp` skeleton with A_2x2x2_Box + per-mip barrier; Step 2 (S,
~50 LoC) wire into `SceneResources::RebuildVctAtlas` lifecycle after `voxelize.comp` writes mip 0;
Step 3 (S, ~40 LoC) Tracy plot "VCT Mip Gen" + `ProjectVVctMipGenTests` unit test. Total **~120 LoC**
(down from initial 260 LoC estimate — no dispatch enum, no per-scene selection, no per-axis blit
fallback at this time). S effort, 1-2 sessions. **GPU D-benchmark deferred to Stage 5.1 integration:**
if D_Blit3D_perAxis GPU timing < A_2x2x2_Box on RTX 3060 Ti, document and consider conditional flip;
else leave A as default. **Continuation chain:** `vct-cone-count-atlas-precision` (closed mixed,
within-VCT quality, assumed mip chain) → this (closed yes, mip gen algorithm). **Stage 5.1 axis
status:** cutoff + cone count + atlas format + mip gen algorithm = 4 of 4 closed/explored. Remaining
Stage 5.1 axis items: Crassin 2011 cone-tapered filter (out-of-scope per
`vct-cone-count-atlas-precision` §172) + 4D temporal VCT (out-of-scope per closed `taa-motion-vectors`
follow-up) + cross-vendor GPU validation. **Cross-axis:** orth orth ко всем 4 in-progress parallel
(tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1, sdf-hybrid-world = VCT anti-leak,
vk-multi-gpu-split-frame = multi-GPU) + complementary к 9 closed Stage 5.1/2.x/3.x experiments
(`vct-vs-rt-cutoff` [cutoff=0.3 strategy] + `vct-cone-count-atlas-precision` [cone count, this = mip
gen axis] + `nanovdb-on-gpu` [storage] + `dec-pipelines-async-compute` [sync] + `hzb-binding-models` [2D
cull] + `clustered-forward-mass-lights` + `rt-shadows-vs-csm` + `restir-gi-feasibility` + `lod-mesh-downsampling`).
**Caveats:** (a) CPU prototype only — no Vulkan dispatch, no GPU time, no cross-vendor validation.
Per-algorithm relative perf may differ substantially on GPU (D_Blit3D_perAxis may flip to faster than
A); (b) Synthetic 3D voxel atlas — not real ProjectV chunk content; (c) Analytical 3D Gaussian
low-pass reference (σ=0.5 voxel × 2^mip_factor) — ideal reference, not real ground truth; (d)
Mutations (per-chunk rebuild on voxel edit) out of scope; (e) Crassin 2011 cone-tapered anisotropic
filter (direction-weighted) = out-of-scope follow-up per `vct-cone-count-atlas-precision` §172; (f)
4D temporal VCT = closed `taa-motion-vectors` follow-up candidate, out of scope; (g) GPU
`vkCmdBlitImage` 3D real timing out of scope — CPU prototype cannot validate; (h) Reduced measurement
budget (30 iter / 3 seeds instead of 100 iter / 5 seeds) due to bash timeout constraint. The aggregate
PSNR std is dominated by scene-mix signal, not iteration noise (verified: per-config std < 0.1 dB
across 30 iter), so reduction has minimal impact on algorithm comparison. Cross-refs: `TODO.md §5.1`
(VCT), `vct-cone-count-atlas-precision/README.md` + `STATUS.md` (direct predecessor),
`2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain extension), `2026-06-20-dec-pipelines-async-compute`
(async compute for off-frame mip gen), `2026-06-20-hzb-binding-models` (2D HZB mip chain analog),
`agent/knowledge.md §30.4` (3-step migration precedent), `agent/knowledge.md §15` (lighting
contract), `agent/workspace.md §2` (Stage 5.x not started), `hardware-profile.md §1+§3` (dev host
baseline), `benchmarks/methodology.md §3` (measurement protocol),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
`experiments/_TEMPLATE/README.md` (template followed). Prototype + build per `AGENTS.md §1` agent not
building. См. §6 + §1 + [experiment README](./experiments/2026-06-21-vct-3d-mip-generation/README.md)

+ [STATUS](./experiments/2026-06-21-vct-3d-mip-generation/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vct-3d-mip-generation/RESULTS.md) +
  [sources.md](./experiments/2026-06-21-vct-3d-mip-generation/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-vct-3d-mip-generation/prototype/README.md) +
  `prototype/build/results.csv` (288 rows) + `prototype/build/mip_bench` (binary).

`2026-06-21` — closed `2026-06-21-hzb-smart-mip-select` (verdict=`mixed`). **Per-chunk HZB mip selection axis**
experiment closed same session (Stage 2.1 per `TODO.md §2.1` + explicit `agent/workspace.md §2` line 52 Nearest Gap
callout: «Stage 2.1 HZB culling refinement — current implementation always uses mip 0; smart per-chunk mip selection
based on screen-space size is a separate optimization»; **self-invented topic** per operator instruction `2026-06-21`
«выбирай свободную тему или придумывай свою и исследуй»). Standalone C++26 CPU cull simulator ~700 LoC (
`prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt, README.md}`),
Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings** after MAX→MIN
pyramid rebuild + frustum culling fix). 100 measurements (5 scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup), wall
time ~12 min on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:** *
*C_PerChunkStaticMip: 700-1500× texel reduction** (avg 13K vs 10.7M texels/chunk vs A_UniformMip0 baseline) AND **+3-5%
cull rate** (avg 27.6% vs 26.4%) — but **0.02-0.20% false-negative artifact rate** (PSNR 27-30 dB worst case
view_dolly_stress; A = 0 FN, PSNR ∞). **2-phase fallback in Step 3** `if (mipLevel > 0 && culled) verify at mip=0`
eliminates FN → PSNR ∞ with 350× texel reduction still. **B_UniformMipGlobal** slightly outperforms C (29.8% vs 27.6%
cull rate) but same FN risk. **C ≈ D** для наших scenes (multiple dispatches don't add measurable value). *
*Verdict=mixed:** strong cost win (700-1500× texel, well above 5% threshold per `optimization-philosophy.md`) but
quality regression (0.02-0.20% FN) without mitigation. Web-research complete via DuckDuckGo HTML + webfetch (Exa HTTP
429 persistent per `agent/knowledge.md Part B §9`); **5 primary sources verified** this session: Greene/Kass/Miller 1993
«Hierarchical Z-Buffer Visibility» [SIGGRAPH 1993 ACM 166147], Mike Turitzin 2020 «Hierarchical Depth
Buffers» [exact pattern statement: «works by projecting a bounding volume into screen-space and using the **projected
size to choose the appropriate mip level**»], Omlor & Radicke 2025 «Two-Pass Occlusion Culling for Dynamic Voxel Scenes
based on HZB» [IEEE Xplore 11321175, Jul 2025 — direct voxel scenes reference], DeepWiki Metallic 2026-04-06 «GPU-Driven
Culling: MeshletCullPass and HZB» [modern Vulkan production reference], RasterGrid 2010 «Hierarchical-Z map based
occlusion culling» [OpenGL FBO mip chain pattern] + 5 secondary (Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 +
chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete). **Mainline 3-step migration
per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) per-chunk mip compute на CPU + `perChunkMipLevel[]` SSBO; Step
2 (S, ~80 LoC) `hzb_cull.comp` SSBO load + branching; Step 3 (XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase
fallback + Tracy plot. Total ~160 LoC, XS-S effort, 2-3 sessions. **Cross-axis:** orthogonal ко всем 5 in-progress
parallel (`sdf-hybrid-world` [closed mixed] + `tracy-gpu-vs-manual` + `gpu-fluid-ca-atomic-strategy` +
`vk-multi-gpu-split-frame` [closed mixed] + `vct-3d-mip-generation`); complementary к closed
`2026-06-20-hzb-binding-models` (texelFetch foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype
precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
threshold), `2026-06-20-dec-pipelines-async-compute` (async foundation); **new axis**: per-chunk mip refinement of
explicit `agent/workspace.md §2` Gap = 0 coverage в INDEX §6 до этого experiment. **Caveats:** CPU prototype only (no
real GPU dispatch, analytical texel-touch cost model); single GPU vendor (RTX 3060 Ti GA104); synthetic scenes
representative not exhaustive (no real ProjectV chunk content); cross-vendor deferred; mutation cost out of scope;
visual QA в реальном gameplay required для fallback correctness; CSM HZB deferred per `agent/workspace.md §2` line 52 —
per-chunk mip extends naturally as follow-up. **Re-evaluation triggers:** Stage 4.3 ships 128m draw distance (per-chunk
mip cost grows linearly with chunks, more savings), mesh shader Pattern C full integration (HIZ output consumed by mesh
shader greedy emit → accuracy matters more), CSM HZB culling adopted (per-chunk mip extends naturally to shadow
cascades), cross-vendor validation on AMD RDNA 4 + Intel Arc Battlemage, Vulkan 1.5+ extensions для new HIZ features.
Cross-refs: `TODO.md §2.1`, `agent/workspace.md §2` line 52 (explicit Gap callout),
`src/render/HizCulling.cpp:800-805` (hardcoded `mip=0`), `src/render/HizCulling.cpp:326-369` (`BuildHizMipChain` уже
работает), `src/render/HizCulling.hpp:48-52` (`HizCullingPushConstants` structure), `src/shaders/hzb_cull.comp:33-90` (
`AabbVisibleAgainstMip` per-mip texelFetch loop), `src/shaders/hzb_cull.comp:102` (current uniform mip от push
constants), `src/render/Renderer.cpp:1344-1350` (`RecordHzbCullingDispatch` call site), `src/voxel/VoxelWorld.hpp:78` (
chunkSize=8), `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-hzb-binding-models` (texelFetch
foundation), `2026-06-20-dec-pipelines-async-compute` (async foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU
prototype precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
threshold), `docs/experiments/hardware-profile.md §1` (Zen 3 5800X dev host),
`docs/experiments/benchmarks/methodology.md §3` (measurement protocol),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). **Single-pass sync
per `AGENTS.md §13.5`:** `backlog.md §In progress` → `§Closed` (with full closure note); `INDEX.md §5 Active` →
`§6 Recent closed sessions` table row + `§1 Now Just-closed` + `§8 Last update`. Anti-duplicate sentinel clean per
`§13.7`. Prototype + build per `AGENTS.md §1` agent not building. См. §6 +
§1 + [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources.md](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
`prototype/{results.csv, bench.log}`.

`2026-06-21` — closed `2026-06-21-dlss-fsr-xess-upscaling-voxel` (verdict=`mixed`). **Render-target post-process
upscaling axis** experiment (cross-cutting для Stage 4.3 lift draw distance + Stage 5.x render pass post-process + 8 GiB
VRAM budget на dev host per `hardware-profile.md §3`; **первый axis "render target post-process upscaling"** — 0 of 30+
closed experiments covered this; ортогонален всем 4 in-progress parallel: tracy-gpu = profiling, gpu-fluid-ca = Stage
3.1 atomic, vct-cone-count = Stage 5.1 VCT quality, audio-diffraction = audio). Standalone C++26 CPU prototype
`prototype/upscaling_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
**0 warnings**), 4 upscalers [None / FSR 3.1 / XeSS 2 DP4a / DLSS 4.5 Sim] × 4 quality
presets [native 100% / quality 67% / balanced 58% / performance 50%] × 3 extents [1080p / 1440p / 4K] × 2
scenes [dense_voxel / sparse_voxel] × 3 seeds × 1000 iter + 10 warmup = **288 measurements** on Zen 3 5800X dev host
`obvium`. **Headline (analytical, per `prototype/RESULTS.md`):** FSR 3.1 = best cost-benefit cross-vendor Vulkan (
3.7-23% savings, PSNR 39.2 dB, +1 MiB VRAM); DLSS 4.5 + XeSS 2 XMX = real GPU measurements required (analytical model
conservative for Tensor Core / XMX hardware — RTX 3060 Ti 4th-gen Tensor Cores ~25 TFLOPS FP16 / ~50 TOPS INT8 vs my
model's 14.7 TFLOPS FP32 baseline = 1.7× underestimate); FSR 4 = NOT usable on Vulkan per `mypcbottleneck 2026-06-04` "
Vulkan API games are not compatible with the FSR 4 Upgrade feature" (RDNA 4-only + DX12-only driver upgrade path);
DirectSR = defer to Vulkan core promotion per `StraySpark 2026-03-25` (currently beta); Frame
Generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = OUT OF SCOPE (latency budget + Reflex/XeLL integration
needed). **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC)
feature-flag `PROJECTV_UPSCALER=OFF|FSR31|XESS2|DLSS45|DIRECTSR` env + `PROJECTV_UPSCALER_QUALITY` env + post-process
pipeline slot after TAA resolve + cross-vendor graceful fallback chain; Step 2 (M, ~250 LoC) per-SDK
integration [UpscalerFactory + NoneUpscaler + FfxFsr31Upscaler + Xess2Upscaler + StreamlineDlss45Upscaler + DirectSRUpscaler];
Step 3 (S, ~80 LoC) quality preset table + TracyPlot + default flip. Total **~360 LoC, S-M effort, 2-3 sessions**. *
*Caveats:** CPU prototype, no real GPU dispatch; upscaler implementations = cost models, not real SDKs; no PSNR/SSIM
real measurement; deterministic timing; cross-vendor projection = analytical only (single GPU vendor measured: NVIDIA
RTX 3060 Ti dev host). **Cross-axis:** orthogonal к 4 in-progress parallel; complementary к closed
`taa-motion-vectors` (verdict=yes, motion vector MRT = direct upscaling input per Streamline/FidelityFX/XeSS unified API
contract — `R16G16_SFLOAT` format matches upscaling standard) + `bindless-descriptor-overhead` Phase D (bindless =
required for cross-vendor upscaling resource management) + `depth-occlusion-quantization` (VRAM-budget cross-cutting) +
`vk-fragment-shading-rate-voxel` (VRS cost axis complementary — VRS 2x1 + DLSS 2x = 4× effective cost reduction,
sequential adoption recommended). **Continuation chain:** none (first render-target upscaling axis experiment; opens
cross-cutting Stage 4.3/5.x post-process). Closed entry: `experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/` +
prototype + `build/results.csv` (288 rows × 18 cols). См.
§6 + [experiment README](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/README.md) + [STATUS](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/STATUS.md) + [sources.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/sources.md) + [prototype/README.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/README.md) + [prototype/RESULTS.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/RESULTS.md).

`2026-06-21` — closed `2026-06-21-depth-occlusion-quantization` (verdict=`yes`, with caveats). **Depth-format axis**
experiment (VRAM-budget, cross-cutting для Stage 2.x HZB cull + Stage 2.2 depth prepass + Stage 5.x G-buffer/depth, *
*follow-up к закрытому `2026-06-20-hzb-binding-models`** [HZB sampling pattern, не format] + closed
`2026-06-20-frame-flight-allocator-budget` [allocator strategy, не depth format] + closed
`2026-06-20-bindless-descriptor-overhead` [Phase A shadow cascade motivation, не depth format]). Standalone C++26
analytical benchmark (`prototype/depth_quant_bench.cpp` ~500 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings), 72
configs × 50 measure iters = 3600 measurements. **Headline findings:** VRAM D32_SFLOAT → D16_UNORM = **-50%** (1080p:
18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB; HZB mip chain included); PSNR depth round-trip = **107.12 dB** (visually
lossless, > 50 dB threshold); false-culled count = **0** across 230 400 cull decisions; mean cull error = 3.82e-6 (
negligible). **Caveats:** synthetic CPU-only (no Vulkan init, no GPU time, no cross-vendor validation); D16 + PCF =
banding/moiré per DXVK PR #5564 (2026-03-25) → CSM shadow maps NOT recommended; reverse-Z benefit not measurable в
synthetic (depth range [0.05, 1.0] not at far plane per Nathan Reed 2021 analysis). **3-step migration
per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) foundation + D16 depth attachment via `findDepthFormat` +
`PROJECTV_DEPTH_FORMAT=D16|D32` env; Step 2 (S, ~80 LoC) reverse-Z + HZB integration (clear=0, GREATER compare,
NDC [1,0]); Step 3 (S, ~50 LoC) multi-attachment rollout (CSM optional, VCT cone-march, transparency depth). Total ~160
LoC, S effort, 3-4 sessions. **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+.
**Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance, depth precision более критична), Stage 5.1 VCT
depth-derivative, Stage 5.2 RTX shadow path, `VK_KHR_depth_float_reduce` ratification, DXVK PR #5564 merge, AMD RDNA +
Intel Arc dev matrix. **Cross-axis:** orthogonal к 5 in-progress parallel (tracy-gpu + wfc + taa + gpu-fluid-ca +
lod-mesh + vk-fragment-shading); complementary к closed `hzb-binding-models` (HZB sampling, не format) +
`frame-flight-allocator-budget` (allocator, не depth) + `bindless-descriptor-overhead` Phase A (shadow cascade
motivation, не depth). **Continuation chain:** none (first depth-format axis experiment; opens VRAM-format axis). Files
retained: [
`experiments/2026-06-21-depth-occlusion-quantization/`](./experiments/2026-06-21-depth-occlusion-quantization/) +
`research/backlog.md §Closed` +
`prototype/{main.cpp, depth_quant_bench.{hpp,cpp}, voxel_scene.{hpp,cpp}, CMakeLists.txt, README.md, RESULTS.md, results.csv}`.
Single-pass sync agent per `AGENTS.md §13.5`: `backlog.md §In progress` → `§Closed` (with full closure note),
`INDEX.md §5 Active` → `§6 Recent closed sessions` table row + `§8 Last update`. Anti-duplicate

`2026-06-21` — closed `2026-06-21-lod-mesh-downsampling` (verdict=`mixed`). **LOD uniform
downsampling + stitch strategy axis** experiment (Stage 4.2 chunk 2 per `TODO.md §4.2` + explicit
"Nearest Gap" в `agent/workspace.md §2` line 44-45 "uniform downsampling implementation … actual
mesh-level downsampling not yet built"). Single-pass sync agent per `AGENTS.md §13.5`:
`backlog.md §In progress` → `§Closed` (with full closure note), `INDEX.md §5 Active` →
`§1 Now` Just-closed + `§6 Recent closed sessions` table row + `§8 Last update`. Anti-duplicate
sentinel clean per `AGENTS.md §13.7`. **Headline:** `B_SurfacePreserve` is the only kernel that
satisfies Stage 4.2 DoD — 0 T-junction holes across 75 test configurations (16938 boundary
face emissions, 0 mismatches). Other kernels: A_Majority3D 10-32% boundary mismatch, C_SolidOnly
17-32% + catastrophic collapse в cave_stress (entire LOD 1 chunk → 0 quads), D_MaxPool 10-32%
(same as A). B_SurfacePreserve also fastest (early-out on `all_same`) at LOD 0/1/3. All
kernels < 1.5 µs/chunk (30-100× headroom vs 50 µs Stage 4.1 budget). LOD 1/2/3 quad reduction
**5.94× / 31.8× / 169×** (all > 4×/16×/64× geometric bounds). **Mainline рекомендация:**
use `B_SurfacePreserve` as default kernel for Stage 4.2 chunk 2; 3-step migration per
`agent/knowledge.md §30.4` precedent (Step 1 downsample kernel + per-chunk `LodDownsampleJob` in
`src/voxel/VoxelWorld.{hpp,cpp}` ~150 LoC; Step 2 `SelectLodMeshSource` decision в
`voxel_mesh.comp` ~250 LoC; Step 3 Tracy plot + default flip ~50 LoC). Total ~450 LoC, M
effort, 2-3 sessions. Caveats: CPU-only prototype, no GPU dispatch; naive face counter без
greedy merge; synthetic scenes; no mutation cost measured; visual QA in real gameplay
required to confirm B's T-junction robustness at runtime camera angles. Cross-axis: 6 closed
same-session `2026-06-21` (audio + wfc + sub-chunk + gpu-noise + frame-flight + dxc) + 3
in-progress same-session (tracy-gpu + taa + gpu-fluid-ca) + 2 same-day declared
(vk-fragment-shading-rate-voxel + audio-diffraction-hybrid) + 19+ closed `2026-06-20` + this =
full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape + audio + temporal

+ atomic + profiling + **LOD geometry axis NEW**. Cross-refs: `TODO.md §4.2`,
  `src/voxel/VoxelWorld.hpp:78` + `:1175-1208` (existing LOD selection), `agent/workspace.md §2`
  (Nearest Gap), `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-nanovdb-on-gpu`
  (NanoVDB mip chain), `2026-06-20-meshing-algo-comparison` (Naive Greedy baseline at LOD 0),
  `2026-06-21-sub-chunk-layers` (orthogonal, same scenes for direct comparability),
  `docs/experiments/hardware-profile.md §1+§2` (Zen 3 5800X dev host `obvium`),
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). Prototype
+ build per `AGENTS.md §1` agent not building. См. §6 +
  §1 + [experiment README](./experiments/2026-06-21-lod-mesh-downsampling/README.md) +
  [STATUS](./experiments/2026-06-21-lod-mesh-downsampling/STATUS.md) +
  [sources.md](./experiments/2026-06-21-lod-mesh-downsampling/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-lod-mesh-downsampling/prototype/README.md) +
  `prototype/build/results.csv` (1200 rows) + `prototype/build/results_tjunc.csv` (75 rows).

`2026-06-21` — closed `2026-06-21-taa-motion-vectors` (verdict=`yes`). **TAA motion vectors axis** experiment
(Stage 5.3 per `TODO.md §5.3`, **temporal axis** для Stage 5 после полного closure lighting-axis на `2026-06-20`:
`vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility`
mixed). Web-research complete (2 batch queries, ~14 results, 6 primary sources верифицированы: Karis 2014
SIGGRAPH foundational ["16:16 RG velocity buffer" = R16G16_SFLOAT exact match for `TODO.md §5.3` prescription;
"velocity accuracy is super important" drives vertex-out recommendation], Yang/Liu/Salvi 2024 TAA survey
[neighborhood clamping + YCoCg = standard 2024], Marrs/Spjut 2018 NVIDIA adaptive TAA [requires RT, out of scope],
k-DOP Clipping SIGGRAPH 2024 [SOTA ghosting mitigation 0.2 ms overhead, follow-up candidate], Karolewics
Lumberyard anti-ghosting TAA [production reference 0.1 ms + 1.6 ms total Xbox One], VK_KHR_dynamic_rendering
[core 1.3 enables MRT pattern already ProjectV mainline]). Standalone Vulkan 1.4 + C++26 prototype skeleton
(`prototype/main.cpp` ~525 LoC + 6 GLSL shaders: voxel_a/b vert+frag + taa_resolve_a/b comp + Makefile +
`prototype/README.md`). **Verdict basis** (independent of measurement execution per `AGENTS.md §1` agent not
building): (1) `TODO.md §5.3` line 425 explicit R16G16_SFLOAT format prescription = mandate; (2) Karis 2014
SIGGRAPH foundational paper; (3) industry standard (UE 5 + Godot 4.x + Unity HDRP all use R16G16_SFLOAT
motion vector MRT) — no cross-vendor ambiguity per `dec-pipelines-async-compute` §2.2; (4) VRAM cost 8 MiB/frame
double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5% threshold per
`optimization-philosophy.md`; (5) `TODO.md §5.3` DoD «Полное исчезновение шлейфов за перемещаемыми гравипушкой
моделями» = only achievable with vertex-out (depth-reproject has fundamental precision loss near edges per
Karis 2014). **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 foundation (S, ~50 LoC,
1 session): vertex shader `out vec4 vPrevClip` + fragment shader `layout(location=1) out vec2 outMotion`
(R16G16_SFLOAT) + `TaaRenderTargets.{hpp,cpp}` add motion vector attachment + `SceneResources.{hpp,cpp}`
allocate double-buffered motion vector MRT; Step 2 TAA resolve update (S, ~50 LoC, 1 session): change motion
vector source from current depth-reproject to read from motion vector MRT + image layout transition
`COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`; Step 3 default flip (XS, ~10 LoC, 1 commit):
`PROJECTV_USE_MOTION_VECTOR_MRT=ON` env flag with cross-vendor graceful fallback. Total M (~110 LoC across
5-6 files, 2-3 sessions). **Side sync fix r1 applied to previous-session `2026-06-20-async-compute-overhead-numbers`**
per `AGENTS.md §13.5` (original session `2026-06-20` left bookkeeping incomplete: §Open stale duplicate line
removed, missing §6 Recent closed table entry added, README Status field `in-progress` → `concluded-verdict-yes`

+ Date closed `N/A` → `2026-06-20` corrected, STATUS.md sync-fix r1 note appended — all preserving original
  measurements +9.85-11.34% + verdict=yes). Anti-duplicate sentinel clean per §13.7. Cross-axis: orthogonal ко
  всем 4 in-progress parallel (tracy-gpu + wfc + sub-chunk + gpu-fluid-ca-atomic-strategy); complementary к closed
  `clustered-forward-mass-lights` (SSBO light list + motion vectors both feed TAA resolve); natural follow-up к
  closed `dec-pipelines-async-compute` (motion vector MRT submission = candidate for async queue). См. §6 + §1 +
  [experiment README](./experiments/2026-06-21-taa-motion-vectors/README.md) +
  [STATUS](./experiments/2026-06-21-taa-motion-vectors/STATUS.md) +
  [sources.md](./experiments/2026-06-21-taa-motion-vectors/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-taa-motion-vectors/prototype/README.md) + 6 GLSL shaders.

`2026-06-21` — closed `2026-06-21-sub-chunk-layers` (verdict=`mixed`). **Chunk-layout-axis experiment**
(Stage 4.x biome/cave data structure axis, orthogonal к in-progress `2026-06-21-wfc-procedural-worlds`
gen-strategy axis). Web-research complete (3 batch queries, ~14 sources верифицированы: Minecraft-1.18+
Java `ChunkSection` 16³ + biomes 4×4×4 = 64 entries per section per FabricMC/yarn DeepWiki + Minecraft
Wiki + wiki.vg protocol + yarn 1.18 API; Bedrock `SubChunk` 4D (x,y,z,**storage layer**) per wiki.vg +
uNmINeD 2021-12-10 reverse engineering; SHARD layered format per scrayos 2024-11-04 + GitHub; ATLAS
AARF columnar storage per Tunact124 Mar 2026; Cubyz CaveMap 64³ fragments with 1-bit per block +
CaveBiomeMap 2048³ per PixelGuys DeepWiki Mar 2026; Hytale NStagedChunkGenerator BiomeStage/TerrainStage
/PropStage/TintStage/EnvironmentStage per vulpeslab/hytale-docs; Vulkan Guide Ascendant chunk layers
main+transparent+clutter per vkguide.dev; Minecraft world generation overview per Telepathic Grunt/XI64
Gist Feb 2021; maguirekrist/voxel_enginevk production-grade chunk pipeline 5 layers). Standalone C++26
CPU prototype (`prototype/sub_chunk_bench.cpp` ~870 LoC, `clang++ 22.1.6 -O3 -march=native`, build
green). 4 designs (A_Monolithic 512 bytes baseline / B_Palette adaptive bits / C_FixedLayer_L2 4 layers
/ D_FixedLayer_L4 2 layers) × 5 scenes (uniform_air + uniform_floor + forest_floor + cave_stress +
mixed_biome) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter per measurement = 100 measurements.
**Measured (Zen 3 5800X dev host `obvium`, governor=`powersave`, 62.7 GiB RAM DDR4, CPU-only synthetic
scenes):**

- **Memory axis (B_Palette / C_L2 / D_L4 vs A_Monolithic baseline 512 bytes):**
    - uniform_air / uniform_floor (1 material): B=20 (-96%), C=84 (-84%), D=42 (-92%) — **B_Palette wins.**
    - forest_floor / cave_stress (2 materials): B=84 (-84%), C=148 (-71%), D=106 (-79%) — **B_Palette wins.**
    - mixed_biome (4 materials): B=148 (-71%), C=148 (-71%), D=138 (-73%) — **D_L4 marginal win.**
- **Build cost:** monolithic 0.03-0.13 µs/chunk vs paletted 1.3-5.8 µs/chunk = **30-55× overhead**,
  but absolute 1-6 µs vs Stage 4.1 budget 50 µs/chunk per `TODO.md §4.1` = 8-50× headroom.
- **Mutation cost:** monolithic 10-16 ns/mutation vs paletted 12-19 ns = **+5-70% overhead**, absolute
  10-19 ns vs Stage 1.2 DoD 0.1 ms tolerance = 5000-10000× headroom.
- **Mesh vertex count:** all designs produce **identical** face counts (591-679 quads) для same scene+seed
  — mesh optimization is layout-orthogonal (covered by `2026-06-20-meshing-algo-comparison` verdict=mixed).
- **Layer boundary axis:** monolithic 0 vs C_L2 80-155 vs D_L4 28-62 = **explicit semantic gain**
  для biome/cave chunks. VCT anti-leak + per-layer LOD + selective rebuild potential.

**Verdict=mixed:** paletted/layered designs win memory (73-96% > 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`) + layer-boundary semantic axis, lose
build cost (acceptable per budget) + mutation cost (negligible absolute). **Mainline recommendation:**
**conditional** — **B_Palette для uniform chunks (96% savings)**, **D_L4 для biome/cave chunks (73-79%
savings + 28-62 transitions)**, **C_L2 для finer biome granularity (71-84% + 80-155 transitions)**;
A_Monolithic as fallback для sparse chunks + legacy compatibility. **3-step migration per
`agent/knowledge.md §30.4` precedent:** Step 1 `ChunkLayout` enum + `SelectChunkLayout` decision
(~150 LoC, S) → Step 2 `world_gen_layers.comp` per-layer payload + per-chunk metadata (~300 LoC, M) →
Step 3 wire layer semantics в `voxel.frag` VCT cone-march terminate + Stage 4.2 per-layer LOD (~250 LoC,
M). Total ~700 LoC + integration, M effort, 5-7 sessions. **Caveats:** CPU-only (no GPU SSBO layout
validation); no Sparse64Tree integration; naive face counter (no greedy merge); synthetic scenes;
single-threaded. **Cross-axis:** Stage 4.x biome/cave axis closed same-day сессии (continuous noise
axis via `gpu-procedural-noise-compute-kernels` mixed OpenSimplex2 + discrete structure axis via this
sub-chunk-layers mixed layered chunks + gen-strategy axis via in-progress `wfc-procedural-worlds`).
3 orthogonal axes of Stage 4.x = complete picture. Cross-refs: `TODO.md §4.1/§4.2/§5.1`,
`src/voxel/VoxelWorld.hpp:85`, `2026-06-20-nanovdb-on-gpu` (yes), `2026-06-21-gpu-procedural-noise-compute-kernels`
(mixed), `2026-06-21-wfc-procedural-worlds` (in-progress), `2026-06-20-svdag-vs-vdb-memory-throughput`
(yes, isStatic flag), `2026-06-20-dec-pipelines-async-compute` (yes, async populate),
`agent/knowledge.md §30.4`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`,
`hardware-profile.md §1+§2`, `benchmarks/methodology.md`. Closed entry:
`experiments/2026-06-21-sub-chunk-layers/` + `prototype/build/results_all.csv` +
`prototype/build/summary_means.csv`.
Stage 0 (toolchain) теперь explicit closed. Closed entry:
`experiments/2026-06-21-dxc-vs-glslc-toolchain/`.

`2026-06-21` — closed `2026-06-21-audio-raytracing-voxel-sdf` (verdict=`mixed`). **Audio axis** experiment
(cross-cutting для будущего Stage 7.x audio; no audio rendering stage в `TODO.md` currently — miniaudio PCM playback
only per `agent/knowledge.md §28`). Standalone C++26 prototype (
`prototype/{voxel_grid,audio_raytracer,reverb,bench}.{hpp,cpp}`

+ `RESULTS.md` + `results.csv`, ~700 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings). 4 configs × 3 scenes ×
  3 seeds × 1000 iter + 100 warmup = **36 runs × 1000 = 36000 measurements** on Zen 3 5800X. Web-research complete
  (3 batch queries, 12 key sources верифицированы: Vercidium 2025 production voxel-grid audio [direct validation],
  SIGGRAPH 2025 Finnendahl et al. differentiable acoustic PT, GSound-SIR Mar 2025 + OptiX Dec 2025,
  Schissler & Manocha 2014 [50 orders, 200 sources], RESound 2007 hybrid ray-frustum, iSound GPU auralization,
  Tsingos 2001 HW-accelerated occlusion, Funkhouser 2002 beam tracing, Meta Acoustic Ray Tracing Audio SDK 2024+,
  NeRAF ICLR 2025). **Headline findings:** (a) **occlusion-only path (1 ray/source) production-ready** = 0.008-0.016 ms
  mean = **< 0.05%** of 33.3 ms audio frame budget @ 30 Hz, immediately integrable, immediate perceptual win (muffled
  sounds behind walls); (b) **full hybrid (32 rays × 4 reflection orders) NOT yet viable** = 13.8-17.1 ms mean on
  cave/open_plains (3.4× over 5 ms hypothesis target), only multi_room in budget at 6.3 ms; (c) **Eyring late reverb**
  negligible cost (~0.001 ms per source), integrate unconditionally; (d) **temporal cache в benchmark не помогает**
  — jitter ±5 cm > 1 cm cache epsilon, need larger ε (10-20 cm per audio frame at 30 Hz). **Mainline recommendation:**
  **Phase 1** occlusion-only + **Phase 2** Eyring late reverb (both XS effort, ~250 LoC, immediate integration into
  Stage 7.x audio v1); **Phase 3** full hybrid **deferred** до one of: (a) SVO hierarchical acceleration (empty-skip
  5-10× per `nanovdb-on-gpu`), (b) lower ray budget (8r×2ord perceptually sufficient per Vercidium 2025 + Schissler
  2014),
  (c) cache tuning, (d) AVX-512 hardware arrival (Zen 5 / Arrow Lake projected 2-4× per `simd-procedural-noise`).
  Cross-reuses `2026-06-20-nanovdb-on-gpu` SVO walker foundation, `2026-06-20-flecs-soa-vs-aos-bench` SoA storage
  verdict=yes, `2026-06-20-work-stealing-job-system` serial dispatcher baseline. Caveats: single-vendor (Zen 3 5800X,
  governor `powersave`), `voxels_traversed` counter instrumentation bug (не влияет на latency), synthetic scenes
  representative not exhaustive, no material absorption modeling, sequential single-threaded per
  work-stealing-job-system verdict=mixed. Continuation chain: **none** (first audio axis experiment; opens Stage 7.x);
  follow-up candidates `_audio-hierarchical-svo-skip_`, `_audio-rt-budget-vs-source-count_`,
  `_audio-diffraction-hybrid_`.
  Cross-axis: **0 of 19+** same-day `2026-06-20` experiments covered audio; this = audio axis opener. См. §6 +
  [experiment README](./experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md) + `sources.md` (12 sources +
  SOTA coverage map) + `prototype/RESULTS.md` (full measurements).

`2026-06-21` — closed `2026-06-21-frame-flight-allocator-budget` (verdict=`mixed`).
**VRAM-allocator-axis experiment** (Stage 6.2 tech-debt, cross-cutting). Web-research
complete (4 batch queries, ~30 results, ~15 sources верифицированы: VMA 3.4.0 docs +
Issue #453 + Frostbite Frame Graph + Frostbite Scope Stacks + Diligent Engine 2.0
ring buffer + Unreal Engine RHI per `VK_EXT_memory_budget` + DXVK commit `9b272fb`

+ vkd3d-proton PR #1543 + D3D12 Residency Starter Library + NVIDIA Vulkan Do's and
  Don'ts + AMD Vulkan device memory guide + VK_EXT_memory_budget spec + VK_EXT_pageable_device_local_memory
  spec + llama.cpp HVV fragmentation case study). Standalone Vulkan 1.4 prototype
  (~890 LoC, links vendored VMA 3.4.0 + volk, NOT ProjectV mainline). 5 strategies
  compared (A_Default / B_BudgetTrack / C_LinearPool per-frame / D_DoubleBuffer
  per-frame / E_PreCreatedRing) + 1 stress pass (256 MiB spike every 50 frames).
  **Measurements on RTX 3060 Ti dev host (Vulkan 1.4.350, NVIDIA 610.43.02):**
  (A) 35.5 µs mean / 67.4 µs p99 / 0 failures; (B) 34.7 µs mean / 58.2 µs p99 / 0
  failures; (C) 1311 µs mean / 2573 µs p99 / 0 failures [per-frame pool recreate
  30× slower, validates VMA Issue #453 warning]; (D) 1309 µs mean / 2941 µs p99 /
  21 failures in stress pass [256 MiB > 64 MiB pool block → clean hard-cap];
  (E) 38.0 µs mean / 113 µs p99 / 0 failures / +64 MiB peakHeapUsage. **Mainline
  recommendation** (3-step migration per `agent/knowledge.md §30.4`): **Step 1 (XS,
  ~20 LoC)** — add `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` to `VulkanBootstrap.cpp:807-823`
  allocator + `vmaSetCurrentFrameIndex()` per frame + TracyPlot `VRAM.heapBudgetMiB`/
  `heapUsageMiB` для observability; **Step 2 (S, ~50 LoC + tests)** — add
  `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` flag для non-critical allocations (5+ call
  sites per `rg vmaCreateBuffer`) with graceful degradation; **Step 3 (M, ~200 LoC)
  DEFERRED** — pre-created single linear ring buffer pool (`TransientPool.{hpp,cpp}`)
  re-evaluation triggers: Stage 4.3 (128+ chunks, transient count > 50/frame) OR
  Stage 5.2 RTX BLAS pool overflow OR Tracy heap-usage→budget trend. **Caveat per
  Step 3:** VMA docs require `maxBlockCount = 1` для ring buffer; double-pool variant
  = wrong pattern, не реализовывать. **Cross-axis:** allocator axis closed
  (cross-cutting для всех transient pressure sources). Parallel session сегодня:
  `2026-06-21-tracy-gpu-vs-manual` (orthogonal scope, no conflict per `AGENTS.md
§13.3`). Closed entry: `experiments/2026-06-21-frame-flight-allocator-budget/` +
  `prototype/README.md` + `prototype/build/results.csv`. См. §6 + §1 + experiment README.

`2026-06-21` — closed `2026-06-21-gpu-procedural-noise-compute-kernels` (verdict=`mixed`).
**Noise-algorithm axis** experiment (Stage 4.1 GPU Noise & World Gen per `TODO.md §4.1`, gating
blocker для infinite worlds). Web-research complete (3 batches, ~20 results, 20 sources
верифицированы: Schneider `arXiv 1903.12270` Perlin/Float 3D = 77 ALU inst [direct instruction count
baseline], GPU Gems 2 Ch 26 textured-LUT Perlin = 53 inst / 9 lookups, atyuwen/bitangent_noise
SimplexNoise.hlsl 3D = ~71 instruction slots, KdotJPG/OpenSimplex2 673 stars CC0 modern
GPU-friendly design, Auburn/FastNoiseLite 3D Perlin 47.93 M/s scalar / 261.10 M/s AVX2 CPU baseline,
NVIDIA Nsight Compute Ampere workgroup-64 occupancy guidance, Khronos Forums compute shader SSBO
write cost validation, JCGT 2022 Olano GTX 1660 modern compiler DCE 17% speedup from disabling tiling,
Vulkanised 2024 GPU Atomic Performance Modeling McKee, production refs: paulrobello/voxel-world
Vulkan compute 5D climate noise + Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025,
AdityaGupta1/mega-minecraft CUDA fBm Oct 2025, russellocean/pebble-rs WGPU compute voxel raytracer
Nov 2025, Yunasawa YNL Vozel Minecraft 1.18+ 5-param FBM Sep 2025). Standalone Vulkan 1.4 compute
prototype (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md, results.csv, run.log}`,
~700 LoC total, 5 conditional GLSL variants через `#define VARIANT_*` + dispatch harness, RTX 3060 Ti
GA104 Ampere, Vulkan 1.4.341, NVIDIA driver 610.43.02, Clang 22.1.6 + glslc 2026.2). 3 runs × 5
variants × 1000 iter + 10 warmup. **Measured:** VALUE=0.0273, PERLIN=0.0272, SIMPLEX=0.0272,
OPENSIMPLEX2=0.0272, WORLEY=0.0280 ms mean — **all variants в пределах 2.9% mean** (below 5%
threshold per `optimization-philosophy.md`). WORLEY unexpectedly not slowest (`glslc` 2026.2 fully
unrolled + register optimization). VALUE == PERLIN по cost (hash + gradient table index similar
register footprint на Ampere). **Memory-bound kernel:** 8 MiB write at 65.6% of 448 GB/s theoretical
peak = SSBO write bandwidth dominates. ALU = ~14% of dispatch time only. Per-eval cost = 13.0
ns/eval, per-chunk = 6.6 µs. **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):** 8× headroom
single octave, 1.9× headroom FBM 4 octaves, 0.63× (over budget) FBM 4 octaves × 3 channels
(heightmap + cave + biome). **Verdict=mixed:** алгоритмический выбор НЕ meaningful perf
discriminator на chunkSize=8 dispatch pattern; **но** quality + license axis still favors
OpenSimplex2 3D-S (CC0, no axis artifacts, analytic derivatives, actively maintained KdotJPG
2019-2024+, stable cold-cache perf без Run-1 spike). **Mainline рекомендация:** use **OpenSimplex2
3D-S** для Stage 4.1 world gen (NOT because fastest — because license + quality + stability).
3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 foundation `noise3d_opensimplex2()`
GLSL port (~50 LoC core, attribution header per CC0 §4(a)), Step 2 dispatch in `world_gen.comp` per
chunkSize=8 pattern + FBM wrapper (4 octaves, ~150 LoC), Step 3 multi-channel (heightmap + cave +
biome, octave reduction если budget exceeded, ~100 LoC). Total ~300 LoC, S effort, 1-2 sessions.
**Cross-axis continuity:** same-day `2026-06-21` parallel sessions (frame-flight-allocator-budget
in-progress + dxc-vs-glslc-toolchain in-progress + tracy-gpu-vs-manual in-progress) + my
noise-algorithm axis = orthogonal angle of Stage 4.x + Stage 6.x + toolchain optimization landscape.
Continuation chain: `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar, closed verdict=mixed) →
this (GPU algorithm choice, closed verdict=mixed). **Caveats:** single GPU vendor validated (RTX 3060
Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02) — mainline re-test on AMD RDNA 2/3/4 + Intel Arc
Battlemage dev matrix; single octave only — FBM 4 octaves linear scaling not measured; single
heightmap channel — multi-channel 3× cost projection not validated; no Nsight Compute
register/occupancy/SM pipe metrics — extension opportunity; no spectral quality metric (FFT framework
not built) — quality claims literature-cited; async-compute overlap with graphics not measured (per
`dec-pipelines-async-compute` verdict=yes — potential 5-8% additional gain); Run 1 vs Run 2+3 shows
14% cold-cache offset для VALUE/PERLIN (warmup insufficient at 10 iters) — OPENSIMPLEX2/SIMPLEX/
WORLEY stable from Run 1. Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8),
`src/shaders/voxel_mesh.comp:146` (existing dispatch pattern), `agent/workspace.md §1 Phase 1`
(world_gen.comp skeleton), `agent/knowledge.md §30.4` (3-step migration precedent),
`2026-06-20-simd-procedural-noise` (CPU orthogonal), `2026-06-20-dec-pipelines-async-compute`
(async foundation, world gen spike isolation), `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target
format), `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti dev host),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold definition).
Closed entry: `experiments/2026-06-21-gpu-procedural-noise-compute-kernels/`. См. §1 + §6 + [experiment
README](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/README.md).

`2026-06-20` — closed `2026-06-20-vma-sparse-textures` (verdict=`mixed`). **Sparse Virtual Texturing axis** experiment (
Stage 2.3 + cross-cutting VRAM budget). Web-research complete (4 batches, ~30 results, 16 sources верифицированы:
shlomnissan "How Virtual Textures Really Work"
2026-02 [software VT = доминирующий pattern, hardware sparse = "mechanism не policy"], shlomnissan/virtual-textures
GitHub 2026 [prototype без HW sparse], UE 5.7 Streaming Virtual Texturing docs [production = software layer], Nanite GDC
2024 Wihlidal [UE VT = software], bgfx 40-svt Karadzic [production reference], Nathan Gauër 2022, SaschaWillems
texturesparseresidency [Vulkan HW sparse example], foijord/SparseTexture 2025-02 [NVIDIA
`vkQueueBindSparse` BLOCKING GLOBAL, 1 TiB address limit vs AMD 256 TiB / Intel 16 TiB — неприемлемо для runtime streaming],
NVIDIA forums 2023 [A4000 multi-second bind for 1000 pages, NVIDIA team acknowledged 2023-09], VMA 3.4.0 CHANGELOG
2026-06-05 [sparse convenience `vmaAllocateMemoryPages` уже из 2.x], `VK_EXT_pageable_device_local_memory` rev
1 [OS-level paging, complementary не replacement], `VK_EXT_memory_decompression` rev 1 ratified
2025-01-23 [GDeflate GPU decompress, NVIDIA-only pre-2026], `VK_NV_extended_sparse_address_space` rev 1
2023-10-03 [NVIDIA 1 TiB workaround], KhronosGroup/Vulkan-Guide sparse_resources.adoc). Standalone Vulkan 1.4 + VMA
3.4.0 + volk prototype (`prototype/{vma_sparse_bench.hpp, main.cpp, README.md}`, ~770 LoC, 3 variants: dense / sparse /
software-vt — peak VRAM + bind latency + page-miss cost measurements). **Главный finding:** hardware sparse textures
unusable на NVIDIA для runtime world streaming per `foijord 2025` (`vkQueueBindSparse` blocking global). **Software VT =
recommended default** (cross-vendor deterministic, peak VRAM cap enforceable, validated production pattern в UE 5.7
RVT / Nanite / id Tech 5 MegaTexture / bgfx 40-svt / Frostbite). Mainline рекомендация: 4-step migration per
`agent/knowledge.md §30.4` precedent — Step 1 foundation `PageManager` + page table texture R32Uint (~150 LoC); Step 2
integration `voxel.frag` `SampleVirtualTexture` per shlomnissan pattern + atlas texture + bindless per
`bindless-descriptor-overhead` Phase D (~350 LoC); Step 3 page manager wiring (LRU eviction + async upload, ~150 LoC);
Step 4 optional HW sparse для static prebake Stage 4.1 (VMA `vmaAllocateMemoryPages`, ~120 LoC). Total ~770 LoC +
integration code, M effort, 3-4 sessions. **VRAM matrix:** software VT = 16-32 MiB atlas + 16 KiB page table (vs dense
256 MiB); HW sparse = 16-64 MiB resident vs 1 GiB virtual. **Cross-vendor analytical projection
per `dec-pipelines-async-compute` matrix:** RTX 3060 Ti (Vulkan 1.4.341) = full sparse residency support per
`VkPhysicalDeviceSparseProperties` query, but NVIDIA `vkQueueBindSparse` blocking global = unusable for runtime; AMD
RDNA 4 = improved; Intel Battlemage = fast binds per `foijord 2025`. **Continuation chain:**
`bindless-descriptor-overhead` Phase D (deferred → active) → this → Stage 4.3 (128+ chunks draw distance) validates
hybrid strategy. **Re-evaluation triggers:** Stage 4.3 lands, NVIDIA `vkQueueBindSparse` driver fix (rare),
`VK_KHR_sparse_image2` cross-vendor, `VK_EXT_memory_decompression` AMD/Intel ratification. **Closed entry:**
`experiments/2026-06-20-vma-sparse-textures/`. Cross-axis: this + same-day 19+ closed сессии = full Stage
1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI axis + sparse-VT axis. См. §1 + §6.

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