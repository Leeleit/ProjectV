# Sources — 2026-06-20-clustered-forward-mass-lights

Полный список источников с аннотациями. Cross-refs из `README.md §2` указывают
на соответствующие секции.

---

## S1. Оригинальные работы (SOTA basis)

### S1.1 Harada, McKee, Yang — "Forward+: Bringing Deferred Lighting to the Next Level"

- **Conference:** Eurographics 2012 — Short Papers, pp. 5-8
- **DOI:** `10.2312/conf/EG2012/short/005-008`
- **URLs:**
    - [https://diglib.eg.org/items/1db2c4c6-dcab-42ea-8c0a-6805d781759e](https://diglib.eg.org/items/1db2c4c6-dcab-42ea-8c0a-6805d781759e)
    - [https://takahiroharada.wordpress.com/wp-content/uploads/2015/04/forward_plus.pdf](https://takahiroharada.wordpress.com/wp-content/uploads/2015/04/forward_plus.pdf)
- **Год:** 2012
- **Ключевое:**
    - Оригинальная формулировка Forward+ метода (compute light culling + forward shading).
    - **Теоретическое доказательство** что Forward+ обходит все deferred варианты по
      memory traffic (Total_diff > 0 если avg lights per tile M < 15×(1+(1+L)×T)/T).
      При L=8 bytes per light → M < 135 — **всегда выполняется** в реальных scenes.
    - Implementation: **DX11c**, AMD Leo demo с 3072 lights.
    - Сравнение с **compute-based deferred** (Andersson 2011) — Forward+ экономит
      bandwidth (нет G-buffer write).
- **Cross-ref:** `README.md §2.1`, `agent/knowledge.md` — нет (pre-existing).

### S1.2 Olsson, Billeter, Assarsson — "Clustered Deferred and Forward Shading"

- **Conference:** HPG '12 (High Performance Graphics 2012), pp. 87-96
- **ISBN:** 978-3-905674-41-5
- **DOI:** `10.2312/EGGH/HPG12/087-096`
- **URL:
  ** [https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf](https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf)
- **Год:** 2012
- **Ключевое:**
    - Вводит **clustered shading** (3D froxels) как generalization of Forward+ (2D tiles).
    - **View samples группируются в 3D clusters** с fixed max extent.
    - **Per-cluster back-face culling** используя normal info.
    - **Hierarchical light assignment** — enables real-time **1M lights**.
    - Более uniform performance vs tile shading (нет depth-discontinuity degenerate case).
- **Cross-ref:** `README.md §2.1`, `sparse-64-tree-alternatives` (концептуально
  similar — hierarchical spatial data structure).

### S1.3 Harada, McKee, Yang — "Forward+: A Step Toward Film-Style Shading in Real Time"

- **Source:** GPU Pro 4, Chapter 5, A K Peters/CRC Press
- **URL:
  ** [https://www.oreilly.com/library/view/gpu-pro-4/9781466567443/chapter-33.html](https://www.oreilly.com/library/view/gpu-pro-4/9781466567443/chapter-33.html)
- **Год:** 2013
- **Ключевое:** production-quality guide, демо-ориентированная глава из
  GPU Pro 4 (Wolfgang Engel, ed.). Содержит DX11c implementation details
  и trade-offs.

---

## S2. Production reference implementations (2016-2026)

### S2.1 WindyDarian/Vulkan-Forward-Plus-Renderer (2016)

- **URL:
  ** [https://github.com/WindyDarian/Vulkan-Forward-Plus-Renderer](https://github.com/WindyDarian/Vulkan-Forward-Plus-Renderer)
- **Год:** 2016-11
- **Ключевое:** SIGGRAPH 2016 final project, **early Vulkan Forward+ implementation**.
  3 steps: depth prepass (no fragment shader), light culling (compute), final
  shading (uses cluster light list). Tile size 16×16.
- **Cross-ref:** `README.md §2.2` — earliest Vulkan reference.

### S2.2 Pezcode/Cluster (bgfx-based)

- **URL:** [https://github.com/pezcode/Cluster/](https://github.com/pezcode/Cluster/)
- **Год:** active 2019+
- **Ключевое:**
    - Cross-API (OpenGL, DX11/12, Vulkan via bgfx).
    - **Logarithmic depth partition** (id Tech 6 formula).
    - Compute shader cluster generation + light culling.
    - Tested on GTX 1070.
    - Поддерживает clustered forward + clustered deferred.

### S2.3 themaister — "Clustered Shading Evolution in Granite" (2020)

- **URL:
  ** [https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/](https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/)
- **Год:** 2020-01-10
- **Ключевое:**
    - **Production SOTA** (Granite engine, real shipping code).
    - **subgroupMin/subgroupMax** для uniform Z-range across subgroup.
    - **subgroupOr** для combining light bitmasks (replaces shared memory approach).
    - **Conservative sphere/spot rasterization** для cluster culling.
    - 32 lights per cluster bitmask + 32-bit type_mask for spot vs point.
- **Cross-ref:** `README.md §2.2` — лучший production pattern reference.

### S2.4 logdahl.net — "27'000 dragons and 10'000 lights" (2025)

- **URL:** [https://logdahl.net/p/gpu-driven](https://logdahl.net/p/gpu-driven)
- **Год:** 2025-04
- **Ключевое:**
    - **GTX 1070** (older GPU), still 60 FPS at 27k dragons + 10k lights.
    - **Naive cluster assignment: 6 ms** (10k lights × 2800 clusters).
    - **Compacted: 1.1 ms**, 164 KB cluster item memory (vs 3.1 MB naive).
    - **AABB-sphere intersection** + atomic counter for range reservation.
- **Cross-ref:** `README.md §2.2` + §2.4 (compaction pattern).

### S2.5 Silver-will/Black_Key (Voxel engine, 2024+)

- **URL:** [https://github.com/Silver-will/Black_Key](https://github.com/Silver-will/Black_Key)
- **Год:** 2024-07+
- **Ключевое:**
    - **Voxel-specific** движок: VCT (voxel cone tracing GI) + **Clustered Forward
      Shading** + GPU frustum/occlusion culling.
    - **3000 point lights на 2016 Intel IGPU = 30 FPS** — direct relevance для
      ProjectV (voxel + clustered forward + low-end hardware).
    - Vulkan 1.3, bindless, deferred rendering fallback.
- **Cross-ref:** `README.md §2.2` + §2.5 — **прямой analog** для ProjectV.

### S2.6 amrhmorsy — "Tiled Light Culling in Vulkan" (2026)

- **URL:** [https://amrhmorsy.github.io/blog/2026/LightCulling/](https://amrhmorsy.github.io/blog/2026/LightCulling/)
- **Год:** 2026-04
- **Ключевое:** recent tutorial-style walkthrough: naive tiled (2D) → 2.5D (2D + Z slices).
  Compute shader code snippets for min/max depth reduction, cell index computation.
- **Cross-ref:** `README.md §2.2` — recent implementation reference.

### S2.7 easimer.net — "Clustered Shading" (2026-01)

- **URL:
  ** [https://easimer.net/homepage/2026/01/11/clustered-shading.html](https://easimer.net/homepage/2026/01/11/clustered-shading.html)
- **Год:** 2026-01-11
- **Ключевое:**
    - **Bitmask representation** (32-bit per cluster, max 32 lights per cluster).
    - Slang shaders (cross-API: Vulkan/WebGPU/D3D12).
    - **Atomic counter** for light index array slot allocation.
    - `clusterMasks[idxCluster]` lookup + `findLSB` iteration в surface shader.
- **Cross-ref:** `README.md §2.2` + §2.4.

### S2.8 kabarsa01/VulkanRender

- **URL:** [https://github.com/kabarsa01/VulkanRender](https://github.com/kabarsa01/VulkanRender)
- **Год:** 2020-01
- **Ключевое:**
    - **Clustered deferred rendering**, 5-pass pipeline (Z-prepass, compute cluster,
      GBuffer, LightVisibility, Deferred lighting, RTGI).
    - **Overall grid 32×32×64** with exponential depth slicing.
    - **256 lights per cluster**, 1024 lights total cap.
    - Hardware RT shadows + RTGI (RDNA+).
- **Cross-ref:** `README.md §2.2`.

### S2.9 HTMA2024/ClusterLighting (Unity, 2026-03)

- **URL:** [https://github.com/HTMA2024/ClusterLighting](https://github.com/HTMA2024/ClusterLighting)
- **Год:** 2026-03
- **Ключевое:** Unity Built-in RP, **3D scene space** grid (not view-frustum
  clusters), baked at edit time. Two-stage intersection (coarse AABB + sphere-AABB).
  Flattens light data into per-cell inlined layout.
- **Cross-ref:** `README.md §2.2` — alternative **scene-space** approach (vs view-frustum).

---

## S3. WebGPU benchmarks (2025, SOTA comparative numbers)

### S3.1 lu-m-dev/WebGPU-forward-and-clustered-deferred-shading

- **URL:
  ** [https://github.com/lu-m-dev/WebGPU-forward-and-clustered-deferred-shading](https://github.com/lu-m-dev/WebGPU-forward-and-clustered-deferred-shading)
- **Год:** 2025-10
- **Ключевое:**
    - **Naive: 41 FPS @ 100 lights → 1-4 FPS @ 1000-2000** (Sponza scene).
    - **Forward+: 60 FPS up to ~700-1000 lights, 49 FPS @ 1000, 12 FPS @ 5000**.
    - **Clustered Deferred: 60 FPS up to ~2000 lights, 45 FPS @ 5000**.
    - **3× advantage of Clustered Deferred over Forward+ on Sponza** (overdraw-bound).
- **Cross-ref:** `README.md §2.2` — **главный quantitative reference**.

### S3.2 CIS5650 Fall 2025 — Project4 WebGPU Forward+ Clustered Deferred

- **URL:
  ** [https://github.com/CIS5650-Fall-2025/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred](https://github.com/CIS5650-Fall-2025/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred)
- **Год:** 2025-10
- **Ключевое:**
    - **Per-light performance table**:
        - 250 lights: 33 ms naive, 12-16 ms Forward+, <7 ms Clustered Deferred (-79%).
        - 500: 66 / 22-29 / 9-10 (-85%).
        - 1000: 143 / 45-48 / 13-15 (-90%).
        - 2000: 250 / 63-77 / 18-20 (-92%).
        - 4000: ~500 / 71-91 / 20-22 (-95%).
    - Forward+ scaling: **20.0 ms per 1000 lights** (at 4000 lights).
    - Clustered Deferred scaling: **6.2 ms per 1000 lights** (3× advantage).
- **Cross-ref:** `README.md §2.2` — **detailed scaling numbers**.

### S3.3 seabiscuit-iv/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred

- **URL:
  ** [https://github.com/seabiscuit-iv/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred](https://github.com/seabiscuit-iv/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred)
- **Год:** 2025-10
- **Ключевое:**
    - **Cluster resolution sensitivity:** 32×16×64 optimal (16:9 aspect, more Z-depth).
    - Forward+ sweet spot ~16:9 grid matching screen aspect + extra Z slices.
- **Cross-ref:** `README.md §2.2`.

### S3.4 vismaychuriwala/WebGPU-Forward-Plus-and-Clustered-Deferred

- **URL:
  ** [https://github.com/vismaychuriwala/WebGPU-Forward-Plus-and-Clustered-Deferred](https://github.com/vismaychuriwala/WebGPU-Forward-Plus-and-Clustered-Deferred)
- **Год:** 2025-10
- **Ключевое:**
    - **16×9×24 grid (3,456 clusters), 1024 lights per cluster cap**.
    - **G-buffer compression: 64 bits/pixel = 6× memory reduction**.
    - Grid size sweet spot: 16×9×24 (42 ms), 8×4×12 coarser (77 ms — too coarse),
      32×18×24 finer (41 ms — diminishing returns).
- **Cross-ref:** `README.md §2.2` + §2.4 (memory layout).

### S3.5 SiCoYu/Vulkan_ForwardPlus_Render

- **URL:** [https://github.com/SiCoYu/Vulkan_ForwardPlus_Render](https://github.com/SiCoYu/Vulkan_ForwardPlus_Render)
- **Год:** active
- **Ключевое:** Vulkan Forward+ benchmark таблица:

  | Scene | Lights | Forward+ ms | Forward ms | Forward+ FPS | Forward FPS |
    |:------|:-------|:-----------|:-----------|:-------------|:------------|
  | Sponza 1000 small | 1000 | 4.91 | 264.82 | 203.66 | 3.78 |
  | Sponza 1000 large | 1000 | 21.98 | 268.26 | 45.5 | 3.73 |
  | Sponza 20000 small | 20000 | 54.83 | Crashed | 18.23 | N/A |
  | Rungholt 1000 | 1000 | 24.59 | 641.06 | 40.67 | 1.56 |

  **Light radius critical** (large lights = many tiles covered, slower).
- **Cross-ref:** `README.md §2.2`.

### S3.6 Additional WebGPU benchmarks

- [terskayl/WebGPU-Forward-Plus-and-Clustered-Deferred-Rasterizer](https://github.com/terskayl/WebGPU-Forward-Plus-and-Clustered-Deferred-Rasterizer) —
  2025-10, workgroup size optimization (1×1 → 8×8 → значительный gain).
- [Calvin-Lieu/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred](https://github.com/Calvin-Lieu/Project4-WebGPU-Forward-Plus-and-Clustered-Deferred) —
  2025-10, fork of CIS5650, **3× Forward+ → Clustered Deferred gap подтверждён**.
- [printer83mph/CIS5650-Project4-WebGPU-Forward-Plus-and-Clustered-Deferred](https://github.com/printer83mph/CIS5650-Project4-WebGPU-Forward-Plus-and-Clustered-Deferred) —
  2025-10, 25× performance gain over naive.

---

## S4. Cross-vendor + subgroup optimization

### S4.1 Khronos — Vulkan Subgroup Tutorial

- **URL:
  ** [https://www.khronos.org/blog/vulkan-subgroup-tutorial](https://www.khronos.org/blog/vulkan-subgroup-tutorial)
- **Год:** 2018-03
- **Ключевое:**
    - `subgroupBallot`, `subgroupBallotBitCount`, `subgroupBallotExclusiveBitCount` —
      для efficient cluster culling.
    - **NVIDIA subgroup=32, AMD=64** (1 warp = 32 invocations на Ampere).
    - На NVIDIA с 32-wide subgroup — 1/32 atomics cost vs naive.
- **Cross-ref:** `README.md §2.3` + `agent/knowledge.md` + `hardware-profile.md §3`.

### S4.2 NVIDIA — Vulkan Update GTC 2019

- **URL:
  ** [https://developer.download.nvidia.com/video/gputechconf/gtc/2019/presentation/s9909-nvidia-vulkan-features-update.pdf](https://developer.download.nvidia.com/video/gputechconf/gtc/2019/presentation/s9909-nvidia-vulkan-features-update.pdf)
- **Год:** 2019-03
- **Ключевое:** task shader cluster culling pattern,
  `subgroupBallotExclusiveBitCount` для prefix-sum output indexing, partitioned
  subgroup operations.
- **Cross-ref:** `README.md §2.3`.

### S4.3 Vulkan subgroup ballot spec (Khronos)

- **URL:
  ** [https://github.com/KhronosGroup/Vulkan-Docs/blob/master/appendices/VK_EXT_shader_subgroup_ballot.txt](https://github.com/KhronosGroup/Vulkan-Docs/blob/master/appendices/VK_EXT_shader_subgroup_ballot.txt)
- **Ключевое:** `VK_KHR_shader_subgroup_ballot` extension (Vulkan 1.1+ core).
  `OpSubgroupBallotKHR`, `subgroupBallotARB` GLSL built-in.
- **Cross-ref:** `README.md §2.3` + `hardware-profile.md §4`.

### S4.4 Timur — "What is NGG and shader culling on AMD RDNA GPUs"

- **URL:** [https://timur.hu/blog/2022/what-is-ngg](https://timur.hu/blog/2022/what-is-ngg)
- **Год:** 2022-06
- **Ключевое:** RDNA NGG primitive shader, shader-based culling.
  Bonus optimization для RDNA, не applicable напрямую к cluster build.
- **Cross-ref:** `README.md §2.3` — bonus reference.

### S4.5 NVIDIA vk_tessellated_clusters (clusters_cull.comp.glsl)

- **URL:
  ** [https://github.com/nvpro-samples/vk_tessellated_clusters/blob/main/shaders/clusters_cull.comp.glsl](https://github.com/nvpro-samples/vk_tessellated_clusters/blob/main/shaders/clusters_cull.comp.glsl)
- **Ключевое:** NVIDIA production cluster culling shader, subgroup ballot
  pattern для cluster visibility output.
- **Cross-ref:** `README.md §2.3`.

---

## S5. Voxel-specific literature (ProjectV analog)

### S5.1 Vyatkin et al. — "A method for deferred rendering of a set of dynamic point light sources of voxelized scenes in real time"

- **Journal:** Programming and Computer Software, vol. 51, no. 3, 2025
- **URL:
  ** [https://manmiljournal.ru/0132-3470/article/view/688128](https://manmiljournal.ru/0132-3470/article/view/688128)
- **Год:** 2024-2025
- **Ключевое:**
    - **Voxelized scenes** + dynamic point lights (VPL = virtual point lights).
    - Reflective shadow maps (selected by significance, not per-texel).
    - Ray marching для indirect illumination.
    - **1024 VPL** tested across 4 scenes (Figs 5-8).
    - Conclusion: thousands of point lights real-time using GPU.
- **Cross-ref:** `README.md §2.2` + `TODO.md §5.1` VCT analog.

### S5.2 Timethy Hyman — "Voxel based Volumetric Fog"

- **URL:
  ** [https://timethy.com/projects/02-voxel-based-volmetric-fog/](https://timethy.com/projects/02-voxel-based-volmetric-fog/)
- **Год:** 2026-01
- **Ключевое:**
    - **Froxel-based** (frustum-aligned voxel) data structure.
    - **Exponential depth distribution** (Naughty Dog formula).
    - GPU-side froxel grid generation per frame.
    - Phase: scattering (Henyey-Greenstein) + accumulation integration.
    - Temporal reprojection (3D) для cross-frame reuse.
- **Cross-ref:** `README.md §2.2` + `TODO.md §5.1` VCT (froxel structure).

### S5.3 MatejGomboc/tron-grid (froxel volumetric fog)

- **URL:** [https://github.com/MatejGomboc/tron-grid](https://github.com/MatejGomboc/tron-grid)
- **Ключевое:** **320×180×64 froxel grid**, logarithmic depth slicing. 3 compute
  passes (density injection, spatial+temporal filter, raymarch composite).
  TLAS shadow ray, Henyey-Greenstein, mesh shader rendering.
- **Cross-ref:** `README.md §2.2` — production froxel reference.

---

## S6. Cluster memory layout (Pezcode/bgfx + logdahl)

### S6.1 Index list layout (vismaychuriwala 2025)

- **Layout:** `clusterLightGridBuffer` = 2×u32 per cluster (offset, count) +
  `clusterLightIndexBuffer` = up to 1024 indices per cluster.
- **Best for:** >32 lights per cluster.
- **VRAM (16×9×24 grid, avg 10 lights):** ~138 KB indices + 27.6 KB grid = **165 KB**.

### S6.2 Bitmask layout (easimer 2026, 32 lights max per cluster)

- **Layout:** 1×u32 per cluster + global light index array (32 lights max).
- **Best for:** small/medium light counts.
- **VRAM (16×9×24 grid):** ~14 KB bitmask + 128 B index array (32×u32) = **~14 KB**.
- **Limitation:** max 32 lights per cluster, **over 32 → lights dropped** (visual artifact).

### S6.3 Compaction (logdahl 2025)

- `atomicAdd` для range reservation + full compaction pass.
- 5× faster than naive, 5× less memory.
- Per-cluster hard cap = 1024, overflow → lost lights (rare в реальных scenes).

### S6.4 Hierarchical assignment (Olsson 2012)

- Multi-level grid (cluster of clusters).
- Enables **1M lights** real-time.
- Out of scope для ProjectV (1000+ lights = 1000× меньше).

---

## S7. ProjectV cross-refs (internal)

- `TODO.md §5.1` VCT — VPL output = primary use case для Forward+ в ProjectV.
- `TODO.md §5.2` RTX shadows — additive per-light cost.
- `TODO.md §4.x` procedural — lava, biomes = dynamic lights.
- `src/shaders/voxel.frag:374-587` — current per-light cost (5 DDA + PBR).
- `src/shaders/voxel.frag:88-117` — `DDA_BODY` macro (template).
- `src/render/SceneResources.{hpp,cpp}` — new SSBO + new compute pipeline.
- `src/render/Renderer.cpp` — new dispatch before voxel.frag.
- `agent/knowledge.md` — build/verification contract (Tracy metric, ≥5% threshold).
- `agent/knowledge.md` — Linux baseline (Clang 22.1.6, libstdc++ 16).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- `docs/experiments/hardware-profile.md §1` + §3 — Zen 3 5800X + RTX 3060 Ti.
- `docs/experiments/2026-06-20-async-compute-overhead-numbers/` — sync foundation
  (vkQueueSubmit2 + timeline semaphores) for cluster build compute pass.

---

## S8. Cross-axis continuation chain (ProjectV experiments closed 2026-06-20)

Этот experiment — **продолжение** lighting/GI axis:

- `2026-06-20-vct-vs-rt-cutoff` (closed mixed) — strategy для high/low roughness.
- `2026-06-20-nanovdb-on-gpu` (closed yes) — storage foundation для VCT.
- `2026-06-20-dec-pipelines-async-compute` (closed yes) — async compute для cluster
  build pass (per-frame on GPU).
- `2026-06-20-hzb-binding-models` (closed mixed) — texelFetch pattern релевантен
  для cluster grid texture (если выбрать texture-based cluster storage).

Этот experiment добавляет **mass-lights dimension** (orthogonal to existing lighting
strategy axis).
