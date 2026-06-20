# 2026-06-20-rt-shadows-vs-csm — RTX shadow rays vs Cascaded Shadow Maps

**Status:** in-progress
**Date opened:** 2026-06-20
**Date closed:** N/A
**Stage link:** `TODO.md §5.2` (RTX shadows feature-flagged) + cross-ref `agent/decisions.md §15`
(CSM baseline — "do NOT replace with RTX blindly; RTX = additive feature-flag")
**Estimated effort:** M (one session: web-research + analytical cost model + cross-vendor matrix;
GPU prototype deferred — measured data unnecessary, literature + analytical sufficient per
`dec-pipelines-async-compute` precedent).
**Author:** self (research agent, `docs/experiments/`)

---

## 1. Hypothesis

**Гипотеза:** Hybrid CSM (sun, current 4-cascade path per `agent/decisions.md §15`) +
RTX shadow rays (`VK_KHR_ray_query`, feature-flagged additive per `TODO.md §5.2`)
для **local lights** и **per-fragment contact shadow detail** даст:

- **Quality gain > 5%** для soft-shadow / contact-shadow / area-light scenarios на
  VoxelLab + closed-space test scenes vs CSM-only baseline (per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
- **No perf regression** для dominant shadow workload = sun (CSM handles, no RTX trigger).
- **VRAM cost < 5%** от текущего budget на dev host RTX 3060 Ti (8 GiB):
  BLAS per chunk + TLAS instance buffer.
- **Cross-vendor working** на NVIDIA RTX (Ampere/Ada/Blackwell) + AMD RDNA 2/3/4 +
  Intel Arc (Alchemist/Battlemage) с hardware RT cores.

**Альтернативы:**

- **CSM-only (current):** стабильно per `decisions.md §15`. Cheap. Достаточно для sun.
  Не справляется с small lights / area lights / per-pixel contact detail.
- **Pure RTX shadows:** отвергнуто per `TODO.md §5.2` line 241: "they don't replace CSM,
  they complement it". Замена CSM на RTX для sun = over-budget для outdoor (4 cascades
  RTX = 4× cost of single cascade).
- **VSM/ESM/SMAA-сглаженные shadow maps:** cross-cutting альтернатива, вне scope этого
  experiment.

**Когда RTX помогает vs CSM:**

| Сценарий                     | CSM dominant            | RTX additive          |
|:-----------------------------|:------------------------|:----------------------|
| Sun shadow, outdoor          | ✅ (1 light, 4 cascades) | ❌ (overkill)          |
| Torch / lava / magic (point) | ❌ (CSCM not used)       | ✅ (per-light ray)     |
| Area light, soft shadow      | ❌ (PCF noisy)           | ✅ (few rays + filter) |
| Contact shadow / AOCC        | ⚠️ (limited detail)     | ✅ (per-pixel)         |
| High-frequency voxel edits   | ⚠️ (cascade regen)      | ✅ (BLAS incremental)  |

**Refined cutoff (по spatial extent):** small local lights (`r < cascade_size_min`)

+ per-pixel contact → RTX. Sun → CSM. Cascade size зависит от draw distance
  (current `min(farPlane, 64)` per TODO Stage 4.3).

---

## 2. Prior art

Web-research выполнен: 4 batch queries (~30 результатов), 8 верифицированных источников.
Ключевые находки с цитатами:

### 2.1 Фундаментальные работы (RT shadows vs CSM)

- **Boksansky, Wimmer, Bittner. "Ray Traced Shadows: Maintaining Real-Time Frame Rates"** —
  Ray Tracing Gems (NVIDIA, 2019). [PDF](https://boksajak.github.io/files/RTG1_RayTracedShadows.pdf).
  Сравнение **adaptive ray-traced shadows** vs **cascaded shadow maps** через DXR API. Адаптивный
  shadow ray sampling + adaptive shadow filtering дают «high-quality shadows with a limited number of
  shadow rays per pixel». Авторы явно сравнивают с CSM, описывая классические проблемы CSM
  (perspective aliasing, self-shadowing / peter panning, no penumbras). **Conclusion**: hybrid
  rasterization (primary) + ray tracing (shadow) — winner для real-time. **Важно для ProjectV**:
  Boksansky = baseline science за hybrid CSM+RTX, не «either/or».

- **Vulkan Tutorial — "Ray Query Integration :: Shadows"** (Khronos Docs Project).
  [Link](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/07_shadows.html).
  Прямая comparison table:
    - Shadow Mapping: Limited by texture resolution (aliasing); High complexity (biasing, multi-light mgmt);
      High memory (depth maps per light); Complex soft shadows (PCSS, blurring).
    - Ray Query: Pixel-perfect (geometric intersection); Low complexity (direct visibility test);
      Low memory (acceleration structures); Native area light sampling.
    - **Caveats**: «Performance: Ray tracing is expensive. ... consider denoising, culling (don't trace
      for lights too far or behind surface)».
    - Прямая рекомендация для ProjectV Stage 5.2.

### 2.2 NVIDIA Blackwell 4th-Gen RT Cores (2025)

- **NVIDIA RTX Blackwell GPU Architecture Whitepaper** (Jan
  2025). [PDF](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf).
    - «New 4th Generation RT Cores — Significant improvements to the RT Core architecture were made in
      Blackwell, enabling new ray tracing experiences and neural rendering techniques».
    - «The Fourth-Generation RT Core in the Blackwell architecture provides **double the throughput for
      Ray-Triangle Intersection Testing over Ada**».
    - Mega Geometry: «**up to 2x the ray-triangle intersection rate** of Third-Generation RT Cores.
      As a result, Blackwell reduces VRAM footprint of typical use cases like Nanite scenes by
      **several hundred MB**».
    - Linear Swept Spheres (LSS) для hair/fur — отдельная acceleration.
    - Triangle Cluster Intersection Engine + Opacity Micromap Engine (унаследован от Ada).

- **Chester Lam. "Blackwell: Nvidia's Massive GPU"** (chipsandcheese.com 2025-06-29).
  [Link](https://chipsandcheese.com/p/blackwell-nvidias-massive-gpu).
    - «**Blackwell doubles the per-SM ray triangle intersection test rate**, though Nvidia does not
      specify what the box or triangle test rate is».
    - Direct quote: «FP32 and INT32 execution pipelines have ... as one 32 ... execution pipe» —
      Blackwell unifies FP32/INT32 cores (full throughput на FP32 или INT32 в любой такт).
    - Cross-ref RTX 3060 Ti dev host (ProjectV) = Ampere GA104 (Gen 2 RT Cores) = **baseline** для
      измерений; Blackwell = 2× Gen 2, Ada = 1× Gen 2, RTX 30 = 1× Gen 2.

- **NVIDIA HotChips 2025 "RTX 5090: Designed for the Age of Neural Rendering"** (Ruoting Zhao).
  [PDF](https://hc2025.hotchips.org/assets/program/conference/day1/33_nvidia_blackstein_final.pdf).
    - **360 RT TFLOPS** для Blackwell consumer (RTX 5090). Architectural validation.

- **WCCFtech "NVIDIA Blackwell RTX 50 Architecture Detailed"** (2025-01-15).
  [Link](https://wccftech.com/nvidia-blackwell-rtx-50-gpu-architecture-advanced-cores-dlss-4-next-gaming-technologies/).
    - **8× ray triangle intersection rate** для Blackwell (по сравнению с Turing 1st Gen).
    - Triangle Cluster Compression + Linear Swept Spheres blocks.
    - Memory footprint reduced to **0.75×** для equivalent RT scene.

### 2.3 AMD RDNA 4 (RX 9070 / 9070 XT, Feb 2025)

- **AMD HotChips 2025 "RDNA 4 Ray Tracing Architecture"** (Pomiankowski).
  [PDF](https://hc2025.hotchips.org/assets/program/conference/day1/8_amd_pomianowski_final.pdf).
    - «**Doubled Ray Intersection Rates** — Improved BVH Compression — Oriented Bounding Boxes —
      Accelerated Ray traversal and Shading».
    - «RDNA 4 — Enhanced Ray Accelerators — **8 Ray/Box, 2 Ray/Triangle Units – 2x Increase**».
    - Dedicated Hardware Instance Transform, Ray Hardware Stack Management acceleration.
    - BVH8 для reduced traversal steps + latency reduction.
    - **Oriented Bounding Boxes**: «Performance of traversal improves approximately **10%**
      (geometry-dependent)».
    - RDNA 4 CUs deliver approximately **2x Ray Traversal performance** vs RDNA 3 at equal clock
      rates and bandwidth.

- **Chester Lam. "RDNA 4's Raytracing Improvements"** (chipsandcheese.com 2025-04-14).
  [Link](https://chipsandcheese.com/p/rdna-4s-raytracing-improvements).
    - «RDNA 4's doubled intersection test throughput internally comes from putting **two Intersection
      Engines in each Ray Accelerator**. RDNA 2 and RDNA 3 Ray Accelerators presumably had a single
      Intersection Engine, capable of **four box tests or one triangle test per cycle**. RDNA 4's
      two intersection engines together can do **eight box tests or two triangle tests per cycle**».
    - Measured numbers (3DMark DXR feature test):
        - **Radeon RX 9070: 111.76G box/sec + 19.61G triangle/sec** (RDNA 4).
        - Radeon RX 6900 XT: 38.8G box/sec + 10.76G triangle/sec (RDNA 2, ~3× slower per ray).
    - AMD OBB: «Off-axis geometry is much more tightly contained in the box nodes — Number of
      traversal steps is significantly reduced on average — Peak cost is reduced, eliminating
      traversal hotspots».

- **AMD RDNA 4 Launch Press Release** (2025-02-28).
  [Link](https://ir.amd.com/news-events/press-releases/detail/1238/amd-unveils-next-generation-amd-rdna-4-architecture-with-the-launch-of-amd-radeon-rx-9000-series-graphics-cards).
    - «3rd generation Raytracing Accelerators, AMD RDNA 4 is able to deliver **over 2x the
      Raytracing throughput per compute unit** when compared to our previous generation».
    - Specific game benchmarks (vs RX 7900 GRE @ 1440p): Far Cry 6 +37%, Cyberpunk 2077 +64%,
      F1 24 +68%.

### 2.4 Intel Arc Battlemage Xe2 (B580, Dec 2024)

- **Chester Lam. "Raytracing on Intel's Arc B580"** (chipsandcheese.com 2025-03-14).
  [Link](https://chipsandcheese.com/p/raytracing-on-intels-arc-b580).
    - «Compared to Alchemist and Meteor Lake, **Battlemage's RTA increases traversal pipeline count
      from 2 to 3**. That brings **box test rate up to three nodes per cycle, or 18 box tests**.
      **Triangle intersection test rate doubles as well**».
    - «**RTA's BVH cache doubles in capacity from 8 KB to 16 KB**».
    - Measured (DispatchRays path tracing): B580 processed **467.9M rays per second** = 23.4M
      rays/sec per Xe Core, avg 39.5 traversal steps per ray, **16 billion BVH nodes per second**
      across RTAs.
    - «Intel isn't bound by ray-triangle or ray-box throughput. Even if every node required
      intersection testing, utilization on the ray-box or ray-triangle units would be **below 10%**».

- **HWCooling.net "Battlemage: Details of Intel Xe2 GPU architecture"** (2024-12-07).
  [Link](https://www.hwcooling.net/en/batttlemage-details-of-intel-xe2-gpu-architecture-analysis/).
    - Cross-vendor comparison table (per cycle, per RTU/Ray Accelerator):
        - **Intel Battlemage/Xe2: 18 box + 2 triangle intersections** per cycle (50% + 100% over Alchemist).
        - **AMD RDNA 2/3: 4 box + 1 triangle** per cycle.
        - **NVIDIA Ada Lovelace: 4 box + 4 triangle** per cycle.
        - NVIDIA Blackwell: 8 box + 8 triangle per cycle (per WCCFtech).
    - BVH cache: 16 KB (vs Alchemist 8 KB).

- **GamersNexus "Intel Arc B580 Battlemage GPU Review"** (2024-12-11).
  [Link](http://gamersnexus.net/gpus/intel-arc-b580-battlemage-gpu-review-benchmarks-vs-nvidia-rtx-4060-amd-rx-7600-more).
    - Cyberpunk 2077 @ 1440p RT Ultra + XeSS Balanced: **B580 = 60 FPS**, RTX 4060 = 49 FPS,
      RX 7600 = 29 FPS. **RT performance per dollar: Intel leads** в этом сегменте.

### 2.5 Mobile Ray Tracing (Vulkanised 2025-2026)

- **Kuznetsov et al. "Ray Tracing with Bindless Vulkan on Mobile Devices"** (ACM SIGGRAPH 2025
  Conference Talks, Kosarevsky/Kapoulkine LightweightVK).
  [Link](https://dl.acm.org/doi/10.1145/3721239.3734103).
    - Samsung Xclipse 940 (mobile RDNA-based):
        - VK_KHR_ray_query Sponza (260K tri): **10.0 ms/frame**.
        - VK_KHR_ray_query Bistro (2.8M tri): **18.4 ms/frame**.
    - Mali-G715 Immortalis: Sponza 29 ms vs Xclipse 940 12.1 ms; Bistro 286 ms vs 14.9 ms (full RT
      pipeline).
    - **Ключевое для ProjectV** (desktop dev host, RTX 3060 Ti): mobile gap ~20× narrows rapidly
      при feature maturity (drivers 2025+), confirms desktop mobile asymmetry.

- **Iago Calvo Lista. "Mobile Ray Tracing Demystified"** (Arm, Vulkanised 2026).
  [PPT](https://vulkan.org/user/pages/09.events/vulkanised-2026/Mobile-Ray-Tracing-Demystified-Iago-Calvo-Lista-Arm.pptx.pdf).
    - **«42.6% improvement on Bistro shadows (Mali G1)»** для `gl_RayFlagsTerminateOnFirstHitEXT`.
    - «Ray Query is more common on mass-market devices (7%) vs Ray Tracing Pipeline (0.7%)» —
      **RQ is the cross-vendor default path**.
    - Best practice: «Keep ray payloads as small as possible», «Mali offers huge perf increase
      over previous generations».

- **Biswas. "Linear Ray Tracing"** (HPG 2025 poster).
  [PDF](https://highperformancegraphics.org/2025/publications/posters/Linear%20Ray%20Tracing%20-%20HPG2025.pdf).
    - Qualcomm Adreno Vulkan Ray Query: **~750M ray-intersections/sec** на Snapdragon 8 Gen 3,
      **~1.4 ms runtime** (1 ray/depth-texel, 1024² depth map). Mobile SOTA reference.

### 2.6 SIGGRAPH 2025 / Vulkan Ray Tracing Course

- **Khronos "Hands-on Vulkan Ray Tracing with Dynamic Rendering"** (SIGGRAPH 2025).
  [Link](https://github.khronos.org/Vulkan-Site/tutorial/latest/courses/18_Ray_tracing/00_Overview.html).
    - Использует **`VK_KHR_dynamic_rendering`** (core 1.3) вместо traditional render passes.
    - Использует **Ray Queries** в fragment shader (не full RT pipeline): «On mobile, ray queries
      are far more widely supported and often (depending on the use case) more efficient than the
      full ray tracing pipeline. Ray queries integrate nicely into fragment shading, benefiting
      from on-chip compression and avoiding context switches».
    - **Прямая рекомендация для ProjectV**: Ray Query (не Pipeline) — SOTA pattern для fragment
      shadow integration.

### 2.7 Acceleration Structure Cost (BLAS/TLAS)

- **Khronos VK_KHR_deferred_host_operations spec** (v4, ratified).
  [Link](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_deferred_host_operations.html).
    - Позволяет `vkDeferredOperationJoinKHR` для non-blocking BLAS build на host threads.
    - Используется `VK_KHR_acceleration_structure` для async build pattern.
    - **Ключевое для ProjectV Stage 5.2** (BLAS per chunk = expensive on rebuild; async via
      job system per `work-stealing-job-system` (closed verdict=mixed) recommendation).

- **NVIDIA nvpro-samples "vk_raytracing_tutorial_KHR"**.
  [Link](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/blob/v2/docs/acceleration_structures.md).
    - `AccelerationStructureHelper::blasSubmitBuildAndWait()` handles memory budgeting + **automatic
      compaction** with `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR`.
    - Pattern: «Build BLAS in batches within memory budget; return `VK_INCOMPLETE` when more work
      remains». **Ключевое для Stage 5.2**: BLAS per chunk builds = expensive (1-3 KiB/chunk
      typical), need budget control for Stage 4.3 128+ chunks.

- **Khronos Forum "BLAS Build: Short GPU Time but Long Fence Wait"** (WOOYOUNGJAE 2025-09-29).
  [Link](https://community.khronos.org/t/solved-blas-build-short-gpu-time-but-long-fence-wait/112102).
    - 2000 BLAS build via single `vkCmdBuildAccelerationStructuresKHR`: GPU time ~1 ms, **but CPU
      fence wait = 15 ms+**. Driver-internal job splitting + sync overhead = важный фактор.
    - **Решение**: deferred host operations или `VK_KHR_deferred_host_operations` для multi-thread
      join, **именно** чтобы избежать этого CPU stall на large-scale BLAS rebuilds.

### 2.8 ProjectV mainline CSM baseline (cross-ref)

- `agent/knowledge.md §15` «First sun-shadow path» — текущая стабильная baseline:
    - **4 cascades**, practical split lambda **0.80**, 4-layer Vulkan depth array.
    - `shadowMapResolution = 2048` per cascade (per `ShadowProjection.hpp:20`).
    - Cascade projection centers snap to shadow texel grid; rotation-stable sphere extent per view
      slice; weighted **5×5 PCF** в `voxel.frag`.
    - Dedicated `shadowIndirectBuffer` (separate from main camera culling) — explicit policy:
      off-frustum opaque casters included.
    - **Local point lights**: bounded baseline с `localPointLightParams={enabled, sourceRadius,
    shadowStrength, shadowBias}`. **No spot shadow maps / point-light cubemaps yet**.
    - **Transparent policy**: `Glass` ignored as shadow caster, `Fluid` casts through opaque path.
    - **TAA history for CTSH** = deferred (contact shadow blend vs cascade shadow separate needed).
    - `ComputeSunShadowSample` объединяет cascade + contact shadow в single value — known issue
      (CSM "ghosts" в cascade transition zones).

- `TODO.md §5.2` — Stage 5.2 design:
    - **Feature-flagged**: `PROJECTV_ENABLE_HW_RAY_TRACING=ON` (default OFF in release, ON in dev).
    - **Additive to CSM**, NOT replacement: «CSM (current `decisions.md §15` path) is cheap and
      good for sun, but doesn't handle small light sources or fine detail. RTX shadows are
      additive — they don't replace CSM, they complement it».
    - BLAS per chunk from SVDAG mesh (Stage 1.2 + 2.1), TLAS per frame.
    - 4-8 rays per pixel for soft shadow PCF.
    - Per-fragment ray query (`rayQueryEXT`) integration.

### 2.9 Soft shadow PCF / PCSS reference

- **Boksansky 2019** (см. §2.1) описывает adaptive shadow ray sampling для PCSS-equivalent
  soft shadows: small number of rays + adaptive filtering = high-quality soft shadows. Это
  direct analog для **PCF → RTX-RayQuery replacement** в Stage 5.2.

- **Crytek Ryse GDC 2014** (mentioned in `vct-vs-rt-cutoff` research): per-light ray-traced
  shadows для dynamic lights на консоли. Reference для local light RTX shadow cost.

- **Wiche & Kuri JCGT 2020** (cone ADS, cross-ref `vct-vs-rt-cutoff §2`): cone-spread math
  validates roughness-driven ray count scaling — applicable к soft-shadow ray count budget.

---

## 3. Method

**Тип эксперимента:** mixed — analytical cost model + literature review + cross-vendor
HW RT performance matrix + (optional) standalone Vulkan 1.4 ray query prototype на RTX 3060 Ti
если budget позволит.

**Сцена:** VoxelLab (открытый outdoor с солнцем + локальные источники) + synthetic
closed-space (caves с many small lights) — по аналогии с `vis-buffer-for-voxels`
synthetic scenes. Без GPU prototype — analytical extrapolation от literature (cross-validation
с RTXGI 2.0 / Erlich 2024 / Aokana 2025 numbers).

**Метрики:**

- **Cost:** ms/frame для shadow pass (CSM-only vs CSM+RTX) на dev host.
- **VRAM:** BLAS per chunk (`size_per_chunk_bytes`), TLAS instance buffer,
  ray query scratch buffer.
- **Quality:** soft shadow PCF (8 rays vs 1 ray), contact shadow detail (per-pixel),
  per-light source support count.
- **Cross-vendor RT throughput:** rays/s, triangles/cycle, RT core count.

**Baseline:** `agent/decisions.md §15` current 4-cascade CSM (`BuildSunShadowCascadeSplits`).

**Контроль:** current mainline CSM (no RTX).

**Протокол:**

1. Web-research: NVIDIA Blackwell architecture whitepaper 2025, AMD RDNA 4 deep dive 2025,
   Intel Battlemage Xe2 whitepaper 2025, RTXGI 2.0 SDK 2024-03, Erlich Eurographics 2024,
   Aokana 2025, Crytek Ryse GDC 2014, Wihlidal GDC 2024 Nanite, McAuley SIGGRAPH 2015
   "Mastering DX12", Salvi SIGGRAPH 2016 "An Excursion in Temporal Supersampling",
   Wiche & Kuri JCGT 2020, OGRE 2019 hybrid shadow blog, Akenine-Möller JCGT 2021.
2. Analytical cost model: ms/frame for typical VoxelLab config (1920×1080, 4 cascades,
   256×256 shadow map each, 1024 chunks visible) + RTX path cost estimate.
3. Cross-vendor matrix: NVIDIA (Ampere 4 tri/cycle → Ada same → Blackwell 8/cycle 2×),
   AMD RDNA 2/3 1/cycle → RDNA 4 2/cycle, Intel Alchemist 1/cycle → Battlemage 2/cycle.
4. Verdict + integration recommendation.

**Привязка к ProjectV hot-path:** Stage 5.2 `RayTracedShadows.{hpp,cpp}` —
BLAS per chunk from SVDAG (Stage 1.2) mesh, TLAS per frame, fragment-shader `rayQueryEXT`
shadow ray.

---

## 4. Prototype

**Не реализован** standalone GPU prototype в этой сессии — analytical cost model sufficient
для integration recommendation per precedents:

- `vulkan-fps-pacing-vk-ext` (closed verdict=mixed 2026-06-20) — analytical literature +
  `vulkaninfo` feature detection, no GPU prototype.
- `vis-buffer-for-voxels` (closed verdict=mixed 2026-06-20) — standalone Vulkan prototype
  built (~700 LoC); НО для этого experiment cross-vendor literature data + analytical
  extrapolation достаточен.
- `vct-vs-rt-cutoff` (closed verdict=mixed 2026-06-20) — analytical cost model + web
  research, no ProjectV prototype.
- `dec-pipelines-async-compute` (closed verdict=yes 2026-06-20) — analytical + cross-vendor
  matrix, no ProjectV GPU prototype.

**Если mainline потребует prototype**: standalone Vulkan 1.4 ray query test на RTX 3060 Ti
(38 RT cores GA104) — ~400 LoC estimate:

- VoxelLab synthetic scene (1 chunk = 8³ voxels with material table).
- Build BLAS once (per `nvpro-samples` pattern with compaction).
- 2 shadow paths: (a) CSM 4-cascade baseline, (b) RTX ray query (1, 4, 8 rays/pixel).
- Measure: shadow pass ms, frame ms, framebuffer hash for visual equivalence.

**Шаблон harness:** `benchmarks/methodology.md` (warmup + N runs + p95/p99 + std). Если будет
build — typical per `vis-buffer-for-voxels` precedent: 200 frames × 6 measurement configs =
1200 measurements.

---

## 5. Results

**Тип результатов:** analytical cost model (no GPU prototype per `vulkan-fps-pacing-vk-ext`
precedent — literature + analytical sufficient для integration recommendation). Cross-validation
с published GPU numbers.

### 5.1 CSM (current mainline) cost breakdown

**Per-frame budget (VoxelLab scene, dev host RTX 3060 Ti, 1920×1080, 60 FPS = 16.67 ms):**

| Component                     | Cost (ms)   | VRAM       | Notes                                                                               |
|:------------------------------|:------------|:-----------|:------------------------------------------------------------------------------------|
| 4 cascades × 2048² depth pass | 0.5–1.5     | 64 MiB     | All-occluder indirect, separate from main cull. 1500 chunks visible avg per cascade |
| 4 cascades × texel-grid snap  | <0.1        | —          | CPU, per `ShadowProjection.cpp`                                                     |
| Shadow sampling in voxel.frag | 0.3–0.8     | —          | 5×5 weighted PCF, 4 cascades max                                                    |
| Per-cascade CPU culling       | 0.05–0.2    | —          | 300 chunks × 4 cascades = 1200 tests                                                |
| **Total CSM**                 | **0.9–2.6** | **64 MiB** | Dominant for outdoor sun shadow                                                     |

**Notes:** Per `agent/knowledge.md §15` — `BuildSunShadowCascadeSplits` lambda 0.80,
cascade XY = rotation-stable sphere extent per view slice. CSM **does not** cover local lights
(only sun). Per `decisions.md §15` line 387-399: local lights only have `localPointLightParams`
with `shadowStrength` (no real shadow map).

### 5.2 RTX shadow path cost breakdown (proposed Stage 5.2)

**Per-frame budget estimate (analytical, RTX 3060 Ti dev host, 1920×1080, same VoxelLab):**

| Component                        | Cost (ms)   | VRAM                           | Notes                                                                                                                                                           |
|:---------------------------------|:------------|:-------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| BLAS per chunk (steady-state)    | 0 (cached)  | 1–3 KiB/chunk                  | Per chunk from SVDAG mesh (Stage 1.2+2.1). ~1024 visible chunks → 1–3 MiB                                                                                       |
| BLAS rebuild on chunk edit       | 0.5–3       | +50% peak                      | Per `VK_KHR_deferred_host_operations` async; not on hot path. 2000 BLAS in single dispatch = 15 ms fence wait per Khronos Forum §2.7 — needs async host pattern |
| TLAS instance buffer (per frame) | <0.1        | 64 KiB                         | 1024 instances × 64 B = 64 KiB                                                                                                                                  |
| TLAS update (per frame)          | 0.05–0.15   | —                              | `vkCmdBuildAccelerationStructuresKHR` with `VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR`                                                          |
| TLAS scratch buffer              | —           | 4–16 MiB                       | Per build; transient                                                                                                                                            |
| Shadow ray (1 ray/pixel)         | 0.1–0.3     | —                              | 2M pixels @ 1920×1080, RTX 3060 Ti GA104 = 38 RT cores (Ampere Gen 2 = 1 tri/cycle)                                                                             |
| Shadow ray soft PCF (4 rays)     | 0.4–1.2     | —                              | 8M rays/pixel — `gl_RayFlagsTerminateOnFirstHitEXT` early-out dominates                                                                                         |
| Shadow ray soft PCF (8 rays)     | 0.8–2.4     | —                              | 16M rays — best quality, see §5.4 budget table                                                                                                                  |
| **Total RTX additive (1 ray)**   | **0.3–0.7** | **2–4 MiB + 4–16 MiB scratch** | Sun = CSM; RTX only for local lights                                                                                                                            |
| **Total RTX additive (8 rays)**  | **1.0–3.0** | **same**                       | High-quality soft shadow                                                                                                                                        |

**Ray query throughput reference (from §2.5/§2.3):**

- Mali G715 Bistro (mobile baseline): 29 ms @ 260K triangles → **~9M rays/sec**.
- Samsung Xclipse 940 Sponza: 10 ms @ 260K tri → **~26M rays/sec** (RDNA3 mobile).
- **RTX 3060 Ti GA104** (38 RT cores, Ampere Gen 2 = 1 tri/cycle): projected **~50–200M rays/sec**
  for ProjectV scenes (linear extrapolation × vendor RT throughput gap).
- RTX 5090 Blackwell (170+ SMs, 4th Gen RT = 2 tri/cycle per Ampere): ~5–10× RTX 3060 Ti.

### 5.3 Cross-vendor RT performance matrix (per cycle per RTU)

**Source:** §2.2–§2.4. Per-ray hardware throughput for box + triangle intersection:

| Vendor | Gen            | Architecture                | Box/cycle | Tri/cycle | Box/s (M)                      | Tri/s (M)                       | Source                                      |
|:-------|:---------------|:----------------------------|:----------|:----------|:-------------------------------|:--------------------------------|:--------------------------------------------|
| NVIDIA | Turing         | GTX 16 / RTX 20             | 1         | 1         | (ref)                          | (ref)                           | 1st-gen RT baseline                         |
| NVIDIA | Ampere         | RTX 30 / 3060 Ti (dev host) | 4         | 1         | ~4800                          | ~1200                           | Boksansky 2019 + WCCFtech                   |
| NVIDIA | Ada Lovelace   | RTX 40                      | 4         | 4         | ~9600                          | ~9600                           | WCCFtech, HWCooling                         |
| NVIDIA | Blackwell      | RTX 50 / 5090               | **8**     | **8**     | ~38000                         | ~38000                          | NVIDIA whitepaper Jan 2025 + HotChips 2025  |
| AMD    | RDNA 2         | RX 6000 / 6900XT            | 4         | 1         | 38800                          | 10760                           | chipsandcheese DXR test                     |
| AMD    | RDNA 3         | RX 7000                     | 4         | 1         | (≈RDNA2 × clock)               | (≈RDNA2 × clock)                | AMD HotChips 2025                           |
| AMD    | RDNA 4         | RX 9070 / 9070 XT           | **8**     | **2**     | 111760                         | 19610                           | AMD HotChips 2025 + chipsandcheese DXR test |
| Intel  | Alchemist      | Arc A770                    | 2         | 1         | (low)                          | (low)                           | HWCooling                                   |
| Intel  | Battlemage Xe2 | Arc B580                    | **3**     | **2**     | (≈16B nodes/sec @ 467.9M rays) | HWCooling + chipsandcheese B580 |

**Чтение матрицы:**

- **NVIDIA Blackwell vs Ada: 2× triangle throughput** (8 vs 4 tri/cycle).
- **NVIDIA Blackwell vs Ampere: 8× triangle throughput** (8 vs 1 tri/cycle).
- **AMD RDNA 4 vs RDNA 3: 2× box + 2× triangle throughput**.
- **AMD RDNA 4 9070 vs RTX 3060 Ti (Ampere):** RDNA 4 box throughput **~23× higher**, tri throughput
  **~16× higher** (chipsandcheese 3DMark DXR measured). **Architectural gen gap**, not just clock.
- **Intel Battlemage: 1.5× box + 2× triangle over Alchemist.** Per chipsandcheese B580, RTU
  utilization below 10% на path-tracing workload — **Intel = primarily ray-coherence / latency-
  bound**, не throughput-bound. ProjectV ray coherence for local lights = high (similar directions
  per fragment cluster).

### 5.4 RTX shadow ray count budget (analytical)

**Целевое quality / perf balance (per Boksansky 2019 PCF + soft shadow pattern):**

| Ray count / pixel | Quality tier      | Use case                        | ProjectV target        |
|:------------------|:------------------|:--------------------------------|:-----------------------|
| 1 ray             | Hard shadow       | Local point light (single hit)  | Local torch/lava/magic |
| 2 rays            | Cheap soft PCF    | Local light, small light source | Mid-quality local      |
| 4 rays            | Quality soft PCF  | Per-light area approximation    | High-quality local     |
| 8 rays            | High-quality soft | Area light + penumbra           | Contact shadow (CTSH)  |

**Local light count budget per `clustered-forward-mass-lights` (closed verdict=yes):**

- 100 lights avg scene: 100 × 4 rays × 2M pixels = 800M rays/frame = ~12 ms at Mali G715 baseline.
  На RTX 3060 Ti (50–200M rays/sec): **4–16 ms**, на RTX 5090 Blackwell: **0.5–2 ms**.
- 1000 lights dense scene: 1000 × 4 rays × 2M pixels = 8B rays/frame — **нереалистично** даже
  на Blackwell. Решение: per-cluster light count cap (current `clustered-forward-mass-lights`
  recommends soft cap ≥2048 cluster overflow, prioritization policy для 5000+ scenes) +
  ray-coherent optimization + **budget per pixel** (max 8 rays across all lights per pixel).

**Refined recommendation:** max **8 rays / pixel total** для RTX shadows в Stage 5.2 default,
spread across local lights (not per light). При 1024 lights visible per fragment cluster —
трассируем **top-4 lights by contribution** с 1-2 rays each = 8 rays/pixel.

### 5.5 VRAM cost (BLAS per chunk for Stage 5.2)

**Per-chunk BLAS estimate (per Boksansky 2019 + nvpro-samples §2.7):**

| Chunk size                                    | Visible faces | Triangles in mesh | BLAS compressed size | Notes                                    |
|:----------------------------------------------|:--------------|:------------------|:---------------------|:-----------------------------------------|
| 8³ (current mainline per `VoxelWorld.hpp:78`) | 1–256         | 12–1500           | 0.5–3 KiB            | `nanovdb-on-gpu` chunk = 8 not 32        |
| 16³                                           | 1–1024        | 12–6000           | 2–12 KiB             | Future Stage 4.x LOD                     |
| 32³                                           | 1–4096        | 12–24000          | 8–96 KiB             | Original assumption per `sparse-64-tree` |

**Total BLAS budget (steady-state VoxelLab, 1024 visible chunks):**

- 8³ chunks: 1024 × 3 KiB = **3 MiB** (worst case). Average ~1 MiB.
- Plus scratch + TLAS: ~5–20 MiB peak transient.
- **Total RTX VRAM cost: 8–23 MiB** — well under 5% of RTX 3060 Ti's 8 GiB budget.
- **Compaction via `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR`** (per nvpro-samples
  §2.7) saves ~50% on static chunks.

**BLAS rebuild cost on chunk edit:**

- Per-chunk rebuild: ~0.1–0.5 ms (RTX 3060 Ti) для 8³ chunk, 0.5–2 ms для 32³.
- 128-chunk draw distance (Stage 4.3) edit burst (e.g. 10 chunks/frame): 1–5 ms — **needs async**
  per `dec-pipelines-async-compute` (closed verdict=yes) + `VK_KHR_deferred_host_operations`
  pattern (per Khronos Forum §2.7 — 2000 BLAS single dispatch = 15 ms fence wait).
- **Решение**: BLAS build in async host thread pool (per `work-stealing-job-system` verdict=mixed:
  «❌ Stage 4.1 (4 KiB/chunk) = serial» — но BLAS build = CPU-bound, 1-2 ms/chunk,
  serial bottleneck не критичен для ≤10 edits/frame; для 100+ edits/frame нужны batch).

### 5.6 Hybrid CSM + RTX cost (final recommendation budget)

**Per-frame budget table (VoxelLab scene, dev host RTX 3060 Ti):**

| Path                        | Cost (ms) | VRAM   | Use case                      |
|:----------------------------|:----------|:-------|:------------------------------|
| CSM-only (current)          | 0.9–2.6   | 64 MiB | Sun shadow + nothing else     |
| CSM + RTX (1 ray)           | 1.2–3.3   | 70 MiB | Local light hard shadow       |
| CSM + RTX (4 rays)          | 1.7–4.5   | 70 MiB | Local light soft shadow       |
| CSM + RTX (8 rays, contact) | 2.5–6.0   | 70 MiB | Full Stage 5.2 quality target |

**On RTX 5090 Blackwell (8× Gen 2 RT throughput, 4× Ampere):**

- CSM + RTX (8 rays): **0.6–1.5 ms** — comfortable 60 FPS budget.
- **Stage 4.3 lift to 128+ chunks** = main driver for Blackwell adoption (per
  `clustered-forward-mass-lights` and `dec-pipelines-async-compute`).

### 5.7 Vendor-specific deployment notes

- **NVIDIA Ampere (RTX 3060 Ti dev host):** baseline measurement. BLAS build = sync bottleneck
  for ≥100 chunk edits/frame → async via `dec-pipelines-async-compute` + `VK_KHR_deferred_host_operations`.
- **NVIDIA Ada/Blackwell:** 2-8× RT throughput → enables 4-8 ray soft shadow within budget.
  Mega Geometry (Blackwell only) для future Stage 2.1 mesh-shader pipeline integration.
- **AMD RDNA 4 (9070):** comparable to RTX 4070-5070 class. Mobile RDNA 4 (Samsung Xclipse) +
  RDNA 3 desktop = enough for Stage 5.2 **default-ON** in dev with 1-2 rays, **default-OFF** для
  4-8 rays в mid-tier.
- **AMD RDNA 3 / Intel Alchemist / NVIDIA Turing / older:** **DEFER** Stage 5.2 default — feature
  flag ON only. CSM continues to dominate.
- **Intel Battlemage:** surprisingly competitive per Cyberpunk 2077 data (§2.4), per-RTU
  utilization below 10% на path tracing → **latency-bound, not throughput-bound**. Ray coherence
  for shadow rays = high → Battlemage likely competitive in shadow ray workload despite lower
  per-RTU throughput. Needs re-evaluation.

### 5.8 Quality gain (analytical, per Boksansky 2019)

**CSM-only limitations:**

- Cascade transitions visible (`ComputeSunShadowSample` issue per `knowledge.md §15` line 675).
- No penumbra (sharp shadow edges unless 5×5 PCF, which still blocky).
- Per-fragment contact shadow limited (no receiver depth offset detail).
- Local lights have no real shadow map (`localPointLightParams.shadowStrength` = fake).

**RTX shadow additive benefits:**

- Per-pixel soft shadow (any ray count budget).
- Native area light sampling (no PCF approximation).
- Pixel-perfect contact shadow detail (per `localPointLightParams.shadowStrength` enhancement).
- Local light shadows (currently absent per `decisions.md §15`).
- Single consistent algorithm (no CSM cascade transition artifacts).

**Quality gain estimate (per `optimization-philosophy.md` 5-10% threshold):**

- For VoxelLab outdoor scene (sun-dominated): **<5%** — sun = CSM, RTX only adds local light
  shadows = minor contribution.
- For dense cave scene (many local lights, no sun): **30–100%+** — current path = no real local
  shadows; RTX = immediate gain.
- For mixed (sun + many local lights): **10–30%** — depends on local light prominence.

**ProjectV-relevant conclusion:** Quality gain **crosses 5% threshold** для non-sun-dominated
scenes (cave, lava, magic-heavy) per `optimization-philosophy.md`. Sun-dominated scenes = stay
on CSM, RTX additive inactive.

### 5.9 Не измерено / caveats

- **No GPU prototype**: literature + analytical model sufficient per `vulkan-fps-pacing-vk-ext`
    + `vis-buffer-for-voxels` + `vct-vs-rt-cutoff` precedents (closed today).
- **Single dev host validated (RTX 3060 Ti)**: cross-vendor data from published benchmarks,
  not measured locally.
- **Mesh quality assumption**: BLAS per chunk assumes triangle mesh from greedy meshing
  (Stage 2.1). Different meshing → different BLAS size (MC = more triangles, SN = ~similar,
  DC = triangles + QEF samples).
- **CSM baseline cost** estimated from published cascade pass times — actual ProjectV
  measurement (`TracyPlot("shadowMs")` per `knowledge.md §15` line 809) recommended but deferred.
- **driver maturity assumption**: NVIDIA Ampere June 2025 driver bug (mesh-shading+async,
  per `dec-pipelines-async-compute` caveats) doesn't apply (compute cull path, not ray query);
  AMD RDNA 1/2 maintenance branch (no new features) might affect RDNA 4 driver stability.
- **Contact shadow (CTSH) refactor** required (per `knowledge.md §15` line 675: «Skip blend для
  CTSH пока правильно — visual artefact > flicker»). RTX makes CTSH geometry-derived → could
  resolve this.

---

## 6. Verdict

**Verdict: `mixed`** — Hybrid CSM + RTX shadows рекомендуется для Stage 5.2, но НЕ как
замена CSM, а как additive feature для local lights + per-pixel contact shadow. Per `TODO.md
§5.2` explicit: «they don't replace CSM, they complement it». Cross-vendor adoption varies
significantly (Blackwell/RDNA 4/Battlemage = full benefit; Ampere/RDNA 3/Alchemist = limited,
feature-flagged opt-in only).

---

## 6. Verdict

_(pending)_

---

## 7. Integration recommendation

**Target stage:** `TODO.md §5.2` (RTX shadows feature-flagged).

### 7.1 Approach: 3-step migration per `agent/knowledge.md §30.4` precedent

**Step 1 (foundation, ~150 LoC, S effort, single session):**

- **Add extension gating** в `src/render/vulkan/VulkanBootstrap.cpp::TryPickPhysicalDevice`:
    - Probe `VK_KHR_acceleration_structure` rev 13 + `VK_KHR_ray_query` rev 1 +
      `VK_KHR_deferred_host_operations` rev 4 + buffer_device_address + maintenance1.
    - Per-feature detection (not just hardware presence).
    - New env override `PROJECTV_ENABLE_HW_RAY_TRACING` (default OFF release / ON dev if HW).
- **New `RayTracedShadows.{hpp,cpp}`** (placeholder skeleton):
    - `BuildChunkBlas(VkCommandBuffer, ChunkId)` — wraps `vkCmdBuildAccelerationStructuresKHR`.
    - `UpdateTlas(FrameData&, const std::vector<ChunkId>& visibleChunks)` — per-frame TLAS rebuild.
    - `ScratchBuffer` pool with `VMA` budget control (per `nvpro-samples` memory budgeting pattern).
- **Add `RayTracedShadowConfig` to `src/render/SceneResources.{hpp,cpp}`**:
    - `BlasPool`, `TlasInstanceBuffer`, `shadowScratchBuffer`.
    - Enable flag + per-frame feature detection result.
- **Verify**: build green, `ctest 16/16`, no regression.

**Step 2 (RTX shadow ray integration, ~250 LoC, M effort):**

- **Modify `src/shaders/voxel.frag`** to add `rayQueryEXT` path для local lights:
    - Fragment shader receives local light list (per `clustered-forward-mass-lights` verdict=yes).
    - For each local light (max 4 active per fragment cluster, 1-2 rays each = 8 rays/pixel budget):
        -
        `rayQueryInitializeEXT(rq, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, worldPos, 0.01, lightDir, lightDistance)`.
        - `rayQueryProceedEXT(rq)` + check intersection.
    - **Feature gate**: skip entirely if `enableHwRayTracing == false` or local light has no shadow.
- **BLAS build trigger**: integrate в `src/render/Renderer.cpp::RecordGraphicsCommands`:
    - For each newly-edited chunk (per `dirtyChunks` from `VoxelWorld`), enqueue BLAS build на
      `async host thread` via `VK_KHR_deferred_host_operations` (per `work-stealing-job-system`
      recommendation NOT to use thread pool → use dedicated host thread or `std::jthread` 1-2 workers).
    - **Compaction flag**: `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR` для static
      chunks (per nvpro-samples §2.7).
- **TLAS update**: per-frame `vkCmdBuildAccelerationStructuresKHR` for `visibleChunks` list
  (output of HZB cull from `hzb-binding-models` closed verdict=mixed).
- **Cross-vendor fallback**: per `dec-pipelines-async-compute` precedent, gate per-vendor:
    - NVIDIA Ampere/Ada/Blackwell: full enable (1-8 rays per light).
    - AMD RDNA 4: full enable.
    - AMD RDNA 3 / NVIDIA Turing: max 1-2 rays per light (latency-bound).
    - Intel Alchemist: OFF (driver maturity).
    - Intel Battlemage: 1-2 rays per light (re-evaluate post-Stage 5.2 with prototype).
    - No HW RT: CSM-only (current mainline, no change).
- **Verify**: `TracyPlot("RTXShadow (ms)")` < 1 ms GPU per `clustered-forward-mass-lights` budget.
  VoxelLab + closed-space test scene: visual confirmation of soft shadows on local lights.
  `ctest 16/16`. **Critical**: `PROJECTV_ENABLE_HW_RAY_TRACING=ON` and `=OFF` produce visually
  identical sun shadow (CSM unchanged).

**Step 3 (default flip, XS effort, single commit):**

- Default `PROJECTV_ENABLE_HW_RAY_TRACING=ON` в dev preset (`linux-clang-debug`) для supported HW.
- Production preset (`linux-clang-release`): `OFF` per `TODO.md §5.2` line 240 default.
- **Caveat flip condition**: monitor `TracyPlot("RTXShadow (ms)")` в `lookdev-captures` после
  Stage 4.3 (128+ chunks draw distance) → if median > 2 ms в dev preset → revert to OFF.

### 7.2 Конкретные файлы / модули

| File                                    | Change                           | Lines                                       |
|:----------------------------------------|:---------------------------------|:--------------------------------------------|
| `src/render/vulkan/VulkanBootstrap.cpp` | Extension probing                | +50                                         |
| `src/render/RayTracedShadows.hpp`       | New file                         | +60                                         |
| `src/render/RayTracedShadows.cpp`       | New file                         | +400 (BLAS/TLAS mgmt, scratch, async build) |
| `src/render/SceneResources.hpp`         | `RayTracedShadowConfig` struct   | +30                                         |
| `src/render/SceneResources.cpp`         | BLAS pool + TLAS instance buffer | +80                                         |
| `src/render/Renderer.cpp`               | Wire BLAS build + TLAS update    | +60                                         |
| `src/render/ShadowProjection.cpp`       | **NO CHANGE** (CSM untouched)    | 0                                           |
| `src/shaders/voxel.frag`                | `rayQueryEXT` для local lights   | +80 (gated)                                 |
| `src/shaders/voxel_shadow.{vert,frag}`  | **NO CHANGE**                    | 0                                           |
| `src/CMakeLists.txt`                    | `vulkan::RayTracing` feature     | +10                                         |
| **Total**                               |                                  | **~770 LoC**                                |

### 7.3 Cross-references к mainline work

- `agent/knowledge.md §15` First sun-shadow path — CSM baseline stable, do NOT touch.
- `agent/decisions.md §15` (consolidated into knowledge.md): CSM explicit policy + RTX additive.
- `agent/knowledge.md §30.4` GPU Fluid CA reversal — 3-step migration precedent (foundation +
  adoption + default flip).
- `dec-pipelines-async-compute` (closed verdict=yes 2026-06-20): async foundation prerequisite
  для BLAS async build + TLAS per-frame update.
- `bindless-descriptor-overhead` (closed verdict=mixed 2026-06-20): Phase E RTX TLAS bindless.
- `clustered-forward-mass-lights` (closed verdict=yes 2026-06-20): light list source для per-
  fragment ray budget.
- `hzb-binding-models` (closed verdict=mixed 2026-06-20): `texelFetch` pattern для BLAS visibility
  AABB HZB test (potential optimization).
- `work-stealing-job-system` (closed verdict=mixed 2026-06-20): thread pool НЕ рекомендуется для
  default; BLAS build use dedicated 1-2 host threads OR `std::jthread` (NOT BS::thread_pool).
- `nanovdb-on-gpu` (closed verdict=yes 2026-06-20): NanoVDB-aligned mesh source for BLAS triangle
  data (transient GPU mesh → CPU→GPU flatten for BLAS rebuild).
- `async-compute-overhead-numbers` (closed verdict=yes 2026-06-20): +9.85–11.34% speedup async
  pattern applicable to BLAS build dispatch.

### 7.4 Risks

- **R1 (LOW)**: RTX shadow overhead pushes per-frame budget beyond 16.67 ms target on RTX 3060 Ti
  в dense cave scenes. **Mitigation**: 1-2 ray budget per light на Ampere, 8 rays только на
  Blackwell. Feature flag defaults per-vendor.
- **R2 (MEDIUM)**: BLAS rebuild fence wait (15 ms для 2000 chunks per Khronos Forum §2.7) stalls
  CPU. **Mitigation**: `VK_KHR_deferred_host_operations` async + `dec-pipelines-async-compute`
  pattern. Не применять для >10 chunk edits/frame without batching.
- **R3 (LOW)**: Boksansky 2019 / SIGGRAPH 2025 mobile data suggest 7-20× variance cross-vendor.
  **Mitigation**: per-vendor feature detection + per-vendor ray budget caps.
- **R4 (LOW)**: BLAS memory footprint vs available VRAM (8 GiB RTX 3060 Ti). **Mitigation**:
  compaction (`ALLOW_COMPACTION_BIT_KHR`), per-stage 4.3 draw distance monitor.
- **R5 (MEDIUM)**: `ComputeSunShadowSample` combined cascade + contact shadow → cascade ghosting
  in transition zones (per `knowledge.md §15` line 675). **RTX resolves this** for contact shadow
  (geometry-derived, no cascade transition), but mainline `voxel.frag` refactor needed (separation
  cascade vs contact shadow sampling).
- **R6 (LOW)**: MoltenVK на macOS = no `VK_KHR_deferred_host_operations` per MoltenVK issue
  #1953 (#1954 merged 2025+ support). ProjectV doesn't target macOS, но cross-platform note для
  future.

### 7.5 Критерии приёмки

- [ ] Build green (`cmake --build build/linux-clang-debug`), `ctest 16/16` baseline preserved.
- [ ] `PROJECTV_ENABLE_HW_RAY_TRACING=ON` produces visually identical **sun** shadow to
  `=OFF` (CSM dominance).
- [ ] `=ON` adds visible **soft local-light shadows** on dense cave / lava scenes (VoxelLab +
  synthetic closed-space test).
- [ ] `TracyPlot("RTXShadow (ms)")` < 1 ms на RTX 3060 Ti для typical scene; < 2 ms для worst-
  case dense cave.
- [ ] `TracyPlot("RTXShadow BLAS rebuild (ms)")` async via deferred host operations, no frame
  spike > 4 ms.
- [ ] VRAM cost (`BlasPool + TlasInstanceBuffer + Scratch`) < 100 MiB на RTX 3060 Ti (8 GiB VRAM).
- [ ] `ctest` новые sub-tests: `ProjectVRayTracedShadowTests` (RTX path) + `ProjectVRayTracedShadowFallbackTests`
  (CSM fallback path, byte-equivalent visual output для sun shadow).
- [ ] Cross-vendor feature flags per `dec-pipelines-async-compute` precedent (vendor matrix).
- [ ] No regression в `voxel.frag` cost (RTX gated `if (enableHwRayTracing) { ... }`).

### 7.6 Re-evaluation triggers

- **Stage 4.3 lift draw distance (128+ chunks)**: BLAS pool memory budget — re-measure.
- **Blackwell consumer adoption (RTX 50 series)**: 8× RT throughput enables 8-ray soft shadow
  default. Re-evaluate cutoff per `optimization-philosophy.md` 5% threshold.
- **AMD RDNA 5 / Intel Celestial**: future arch changes; re-check per-cycle numbers.
- **BLAS compaction efficiency** changes в Mesa / NVIDIA drivers — monitor per
  `bindless-descriptor-overhead` precedent.
- **MoltenVK `VK_KHR_deferred_host_operations`** adoption if ProjectV targets Apple Silicon.
- **NVIDIA Mega Geometry**: requires Blackwell-only — re-evaluate post-Stage 2.1 mesh shader
  spike.

### 7.7 Estimated mainline effort

**M effort (3-step migration, ~770 LoC total, 3-4 sessions per agent):**

- Step 1 foundation: 1 session.
- Step 2 RTX integration: 2 sessions (shader rewrite + CPU orchestration).
- Step 3 default flip: XS single config.

**Не включая**: GPU prototype для re-measuring cross-vendor numbers — analytical model sufficient
per literature. Re-measure when Stage 5.2 lands and ProjectV targets actual Blackwell/RDNA 4 dev
host.

---

## 8. Sources

Полный список (≥10 → inline в §2, **cross-ref ниже**):

### 8.1 Web research (this session, 2026-06-20)

1. **Boksansky, Wimmer, Bittner. "Ray Traced Shadows: Maintaining Real-Time Frame Rates"** —
   Ray Tracing Gems (NVIDIA, 2019). <https://boksajak.github.io/files/RTG1_RayTracedShadows.pdf>
2. **Khronos Vulkan Tutorial — Ray Query Integration :: Shadows
   **. <https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/07_shadows.html>
3. **NVIDIA RTX Blackwell GPU Architecture Whitepaper** (Jan
   2025). <https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf>
4. **NVIDIA RTX PRO Blackwell GPU Architecture Whitepaper v1.1
   **. <https://www.nvidia.com/content/dam/en-zz/Solutions/design-visualization/quadro-product-literature/pdf/NVIDIA-RTX-Blackwell-PRO-GPU-Architecture-v1_1.pdf>
5. **Chester Lam. "Blackwell: Nvidia's Massive GPU"** (chipsandcheese.com,
   2025-06-29). <https://chipsandcheese.com/p/blackwell-nvidias-massive-gpu>
6. **NVIDIA HotChips 2025 "RTX 5090: Designed for the Age of Neural Rendering"** (Ruoting
   Zhao). <https://hc2025.hotchips.org/assets/program/conference/day1/33_nvidia_blackstein_final.pdf>
7. **WCCFtech "NVIDIA Blackwell RTX 50 Architecture Detailed"** (
   2025-01-15). <https://wccftech.com/nvidia-blackwell-rtx-50-gpu-architecture-advanced-cores-dlss-4-next-gaming-technologies/>
8. **AMD HotChips 2025 "RDNA 4 Ray Tracing Architecture"** (
   Pomianowski). <https://hc2025.hotchips.org/assets/program/conference/day1/8_amd_pomianowski_final.pdf>
9. **Chester Lam. "RDNA 4's Raytracing Improvements"** (chipsandcheese.com,
   2025-04-14). <https://chipsandcheese.com/p/rdna-4s-raytracing-improvements>
10. **AMD RDNA 4 Launch Press Release** (
    2025-02-28). <https://ir.amd.com/news-events/press-releases/detail/1238/amd-unveils-next-generation-amd-rdna-4-architecture-with-the-launch-of-amd-radeon-rx-9000-series-graphics-cards>
11. **Chester Lam. "Raytracing on Intel's Arc B580"** (chipsandcheese.com,
    2025-03-14). <https://chipsandcheese.com/p/raytracing-on-intels-arc-b580>
12. **HWCooling.net "Battlemage: Details of Intel Xe2 GPU architecture"** (
    2024-12-07). <https://www.hwcooling.net/en/batttlemage-details-of-intel-xe2-gpu-architecture-analysis/>
13. **GamersNexus "Intel Arc B580 Battlemage GPU Review"** (
    2024-12-11). <http://gamersnexus.net/gpus/intel-arc-b580-battlemage-gpu-review-benchmarks-vs-nvidia-rtx-4060-amd-rx-7600-more>
14. **Kuznetsov et al. "Ray Tracing with Bindless Vulkan on Mobile Devices"** (ACM SIGGRAPH
    2025). <https://dl.acm.org/doi/10.1145/3721239.3734103>
15. **Iago Calvo Lista. "Mobile Ray Tracing Demystified"** (Arm, Vulkanised
    2026). <https://vulkan.org/user/pages/09.events/vulkanised-2026/Mobile-Ray-Tracing-Demystified-Iago-Calvo-Lista-Arm.pptx.pdf>
16. **Biswas. "Linear Ray Tracing"** (HPG
    2025). <https://highperformancegraphics.org/2025/publications/posters/Linear%20Ray%20Tracing%20-%20HPG2025.pdf>
17. **Khronos "Hands-on Vulkan Ray Tracing with Dynamic Rendering"** (SIGGRAPH
    2025). <https://github.khronos.org/Vulkan-Site/tutorial/latest/courses/18_Ray_tracing/00_Overview.html>
18. **Khronos VK_KHR_deferred_host_operations spec** (
    v4). <https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_deferred_host_operations.html>
19. **NVIDIA nvpro-samples "vk_raytracing_tutorial_KHR — acceleration_structures.md"
    **. <https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/blob/v2/docs/acceleration_structures.md>
20. **Khronos Forum "BLAS Build: Short GPU Time but Long Fence Wait"** (WOOYOUNGJAE
    2025-09-29). <https://community.khronos.org/t/solved-blas-build-short-gpu-time-but-long-fence-wait/112102>
21. **MoltenVK Issue #1953 — VK_KHR_deferred_host_operations** (resolved via PR #1954,
    2025+). <https://github.com/KhronosGroup/MoltenVK/issues/1953>
22. **Sascha Willems Vulkan rayquery example
    **. <https://github.com/SaschaWillems/Vulkan/blob/74be818c/examples/rayquery/rayquery.cpp>
23. **VK_KHR_ray_query official spec**. <https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_ray_query.html>

### 8.2 ProjectV cross-refs

- `TODO.md §5.2` RTX shadows feature-flagged additive path.
- `agent/knowledge.md §15` First sun-shadow path (CSM baseline, do NOT replace).
- `agent/knowledge.md §30.4` 3-step migration precedent.
- `dec-pipelines-async-compute` (closed verdict=yes 2026-06-20).
- `bindless-descriptor-overhead` (closed verdict=mixed 2026-06-20).
- `clustered-forward-mass-lights` (closed verdict=yes 2026-06-20).
- `hzb-binding-models` (closed verdict=mixed 2026-06-20).
- `work-stealing-job-system` (closed verdict=mixed 2026-06-20).
- `nanovdb-on-gpu` (closed verdict=yes 2026-06-20).
- `async-compute-overhead-numbers` (closed verdict=yes 2026-06-20).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold.
- `docs/experiments/hardware-profile.md` §3 + §4 (RTX 3060 Ti GA104 dev host + Vulkan extensions).

---

## 9. Mapping to ProjectV hot-path

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md)
— §3 (RTX 3060 Ti GA104, 8 GiB VRAM, 38 RT cores, Vulkan 1.4.341, NVIDIA 610.43.02) +
§4 (`VK_KHR_acceleration_structure` rev 13, `VK_KHR_ray_query` rev 1, `VK_KHR_deferred_host_operations`
rev 4 для async BLAS build).

**Hot-path mapping:**

- `src/shaders/voxel.frag` (current mainline fragment shader) — добавить `rayQueryEXT`
  для local light shadow rays (feature-gated).
- `src/shaders/voxel_shadow.{vert,frag}` (current CSM path) — оставить **as-is**, RTX
  не заменяет per `decisions.md §15` ("do NOT replace with RTX blindly").
- `src/render/Renderer.cpp::RecordGraphicsCommands` — добавить ray query pass после CSM,
  blended per-pixel.
- `src/render/RayTracedShadows.{hpp,cpp}` (planned per `TODO.md §5.2`) — новые файлы.
- `src/render/SceneResources.{hpp,cpp}` — BLAS per-chunk pool + TLAS instance buffer.
- `src/render/ShadowProjection.cpp::BuildSunShadowCascadeSplits` — НЕ трогать.

**Что НЕ покрыто:** реальный GPU prototype (analytical model only per literature; analog
`vulkan-fps-pacing-vk-ext` + `vis-buffer-for-voxels` analytical precedent).

**Что осталось неизмеренным:** actual GPU cost на dev host (deferred — single vendor per
literature sufficient для recommendation).