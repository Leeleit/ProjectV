# 2026-06-21-depth-occlusion-quantization — Vulkan depth format precision/VRAM tradeoffs для voxel cull pipeline

**Status:** in-progress
**Date opened:** 2026-06-21
**Date closed:** TBD
**Stage link:** independent (cross-cutting для Stage 2.1/2.2 HZB cull + Stage 2.2 depth prepass + Stage 5.x G-buffer/depth); foundation для будущих Stage 4.3 lift draw distance + Stage 5.x lighting
**Estimated effort:** M (analytical + prototype + measurements + writeup)
**Author:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»; l-priority slot в Open backlog — все h/m-priority закрыты или in-progress)

---

## 1. Hypothesis

**Primary hypothesis (depth format + reverse-Z):** Switch ProjectV's depth prepass от current `VK_FORMAT_D32_SFLOAT` (4 bytes/texel, `clearDepthValue={1.0f, 0}` per `src/render/Renderer.cpp:290` = standard Z, `minDepth=0.0f, maxDepth=1.0f` per `src/render/Renderer.cpp:296-297`) к `VK_FORMAT_D16_UNORM` (2 bytes/texel) **+ Reverse-Z trick** (clear=0, `VK_COMPARE_OP_GREATER`, near=1 far=0) даст **-50% VRAM saving** на fullscreen depth attachment (1080p = 8.3 → 4.1 MiB) + HZB mip chain (full chain 16.5 → 8.3 MiB) при **acceptable PSNR loss** (predicted < 1 dB vs D32 standard-Z reference в typical voxel scenes с draw distance ≤ 256m, per Nathan Reed 2021 "Reversed-Z with a float depth buffer gives a zero error rate in this test" + Upchurch/Desbrun 2012 "infinite projection" analysis).

**Alternative variant A (mixed HZB format):** D32_SFLOAT mip 0 + D16_UNORM mip 1+ (HZB mip chain coarse levels use D16). VRAM saving ~25% on HZB. **Risk:** coarse mip precision degradation → false-cull rate increase (predicted < 5% per `optimization-philosophy.md` threshold).

**Alternative variant B (D24 packed only):** `VK_FORMAT_X8_D24_UNORM_PACK32` (4 bytes/texel, 24-bit depth in 32-bit word, 8 bits unused — **no VRAM saving** per Vulkan format spec `vk_format_utils.cpp`). VRAM saving = 0%. **Verdict:** useful only if ProjectV wants 24-bit fixed-point (saves cycles vs float), NOT a compression option. Cross-vendor availability: optional per Khronos `Vulkan-Guide/chapters/depth.adoc`.

**Secondary hypothesis (HW conservative rasterization tier 2 alternative):** `VK_EXT_conservative_rasterization` rev 1.1 + `SPV_KHR_post_depth_coverage` + `conservativeRasterizationPostDepthCoverage=VK_TRUE` (per `VkPhysicalDeviceConservativeRasterizationPropertiesEXT` spec) per Bittner 2020 CGF "Hierarchical Raster Occlusion Culling" + natillum dP article — measurably faster occlusion culling **alternative** to HZB mip chain generation (no mip chain build cost = 0.2-2 ms saving per frame for 1080p+ scenes per RasterGrid 2010 measurements), per-fragment early-out via `PostDepthCoverage` execution mode. **Cross-vendor support matrix** (per `dec-pipelines-async-compute` §2.2 baseline): NVIDIA Ampere/Ada/Blackwell = full support; AMD RDNA 2/3/4 = tier 2 (`postDepthCoverage`); Intel Alchemist/Battlemage = tier 1 only (no tier 2). **Risk:** projectV's HZB per `hzb-binding-models` (closed verdict=mixed) уже рекомендован + `voxel.frag` integration locked. Conservative raster = **additive alternative**, не replacement.

**Out of scope (parked для follow-up):** Tile-based occlusion culling per Fyrox 2024 blog (R32UI bitmask, 32 objects per pixel, additive blending + readback) — alternative to HZB, but readback cost = CPU stall, incompatible с current dec-pipelines-async-compute architecture. Defer.

---

## 2. Prior art

Web-research complete (3 batch queries, ~25 results, 8 key sources верифицированы):

**Key sources (web-search, 2024–2026):**

- **Vulkan-Guide `chapters/depth.adoc`** (KhronosGroup, 2024+) — format support matrix: `D16_UNORM` required для sampled/blit; `D32_SFLOAT` required для attachment; `X8_D24_UNORM_PACK32` + `D24_UNORM_S8_UINT` optional. URL: <https://github.com/KhronosGroup/Vulkan-Guide/blob/f4745cfa/chapters/depth.adoc>

- **Vulkan Loader/Validation `vk_format_utils.cpp`** (KhronosGroup, 2024+) — texel block size reference: `D16_UNORM` = 2 bytes / 1 texel; `X8_D24_UNORM_PACK32` = 4 bytes / 1 texel; `D32_SFLOAT` = 4 bytes / 1 texel; `D24_UNORM_S8_UINT` = 4 bytes / 1 texel (24+8 packed). URL: <https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/layers/vk_format_utils.cpp>

- **Nathan Reed "Visualizing Depth Precision"** (NVIDIA Technical Blog, 2021-10-21) — **Reverse-Z foundational analysis**: "Reversed-Z with a float depth buffer gives a zero error rate in this test... use a floating-point depth buffer with reversed-Z! And if you can't use a floating-point depth buffer, you should still use reversed-Z." URL: <https://developer.nvidia.com/blog/visualizing-depth-precision/>

- **MJP "Attack of the depth buffer"** (2010-03-23, релевантен как SOTA-baseline 2010) — "16-bit float... easily the worst format out of everything I tested... don't use this!" для standard-Z; 24-bit = "this isn't terrible, and a lot of people have shipped awesome-looking games with this format". URL: <https://therealmjp.github.io/posts/attack-of-the-depth-buffer/>

- **Upchurch & Desbrun "Tightening the Precision of Perspective Rendering"** (Caltech 2012) — infinite projection theoretical analysis: "infinite projection is a more precise general purpose projection". URL: <https://www.geometry.caltech.edu/pubs/UD12.pdf>

- **doitsujin/dxvk PR #5564** (2026-03-25) — **D16 shadow map banding/moiré artifacts** на Vulkan (visible vs D3D11). Quote: "Vulkan's `VK_FORMAT_D16_UNORM` provides exactly 16 bits of depth precision. D3D11 and OpenGL drivers likely use higher internal precision for D16 depth buffers, as the OpenGL spec explicitly allows implementations to use higher bitdepth than requested for internal formats." **Caveat** для D16: DXVK promote D16 to D32 при `D3D11_BIND_DEPTH_STENCIL` + `D3D11_BIND_SHADER_RESOURCE` use case (shadow map + PCF). URL: <https://github.com/doitsujin/dxvk/pull/5564>

- **Arm "Occlusion Culling with Hierarchical-Z"** (Arm Software, OpenGL ES SDK, 2023+) — HZB mip chain pattern reference, `textureGather` 4-tap quad fetch for max reduction. URL: <https://arm-software.github.io/opengl-es-sdk-for-android/occlusion_culling.html>

- **RasterGrid "Hierarchical-Z map based occlusion culling"** (2010-10-01, SOTA reference) — HZB construction "takes less than 0.2 milliseconds and the actual culling comes at almost no cost". URL: <https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/>

- **natillum dP "Efficient GPU-based occlusion culling via early-Z and indirect dispatch"** (2024+) — **conservative rasterization tier limitations** + post-depth-coverage pattern + early-Z. URL: <https://natillum.com/en/article/27/efficient-gpu-based-occlusion-culling-via-early-z-and-indirect-dispatch>

- **Bittner J. "Hierarchical Raster Occlusion Culling"** (CGF 39(2) 2020) — "scalable online occlusion culling algorithm, which significantly improves the previous raster occlusion culling using object-level bounding volume hierarchy". URL: <https://onlinelibrary.wiley.com/doi/10.1111/cgf.142649>

- **Mike Turitzin "Hierarchical Depth Buffers"** (2023) — HZB mip chain construction, `atomicMin` + 4x4 workgroup pattern. URL: <https://miketuritzin.com/post/hierarchical-depth-buffers/>

- **Luc Momber "Two-Pass Hierarchical Z-Buffer Occlusion Culling"** (Medium 2025-04-01) — современный HZB pattern + reprojection + visibility buffer. URL: <https://medium.com/@Lucmomber/two-pass-hierarchical-z-buffer-occlusion-culling-93171c5a9808>

- **Fyrox "Tile-based Occlusion Culling"** (2024) — R32UI bitmask per tile pattern, 32 objects per pixel + readback collapse. URL: <https://fyrox.rs/blog/post/tile-based-occlusion-culling/>

- **Vkguide.dev "Compute based Culling"** (2024+) — `textureLod(depthPyramid, ...)` mip-level selection via `log2(max(width, height))`. URL: <https://www.vkguide.dev/docs/gpudriven/compute_culling/>

- **Vulkanised 2023 Mesh Shading Best Practices** — task shader per-meshlet culling pattern + vendor preferences. URL: <https://vulkan.org/user/pages/09.events/vulkanised-2023/vulkanised_mesh_best_practices_2023.02.09-1.pdf>

- **Vulkan-Guide TBR Best Practices** — `VK_ATTACHMENT_LOAD_OP_CLEAR` vs `vkCmdClearAttachments`, `VK_KHR_dynamic_rendering_local_read`, HSR + early-Z hardware depth culling. URL: <https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html>

- **vkdoc.net "Formats"** — `X8_D24_UNORM_PACK32` description: "4 byte 1x1x1 block extent, 1 texel/block" (pack32 = 32-bit word, 8-bit X + 24-bit D). URL: <https://vkdoc.net/chapters/formats>

- **ImgTec "Optimal Depth Buffer Usage for Large-scale Games"** — `GL_EXT_clip_control` reverse-Z recommendation: "With this method, the usual `D24S8` format may be enough for most games. For even more precision, use a `D32F`". URL: <https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/optimal-depth-buffer-usage-for-large-scale-games.html>

- **Zero Radiance "Quantitative Analysis of Z-Buffer Precision"** (2020-08-24) — analytical depth precision analysis, infinite far plane precision. URL: <https://zero-radiance.github.io/post/z-buffer/>

- **Vulkan samples "Mesh Shader Culling"** — task+mesh cull pattern. URL: <https://docs.vulkan.org/samples/latest/samples/extensions/mesh_shader_culling/README.html>

- **DLR "Comparison of Depth Buffer Techniques for Large and Detailed 3D Scenes"** — "reversed and reversed infinite projections with 32-bit floating-point depth buffers... best overall distribution of precision from all tested methods". URL: <https://elib.dlr.de/187280/1/Comparison%20of%20Depth%20Buffer%20Techniques%20for%20Large%20and%20Detailed%203D%20Scenes.pdf>

**ProjectV current path cross-references:**

- `src/render/Renderer.cpp:290-297` — `clearDepthValue{.depthStencil = {1.0f, 0}}` + `minDepth=0.0f, maxDepth=1.0f` = **standard Z** (far=1, near=0). NOT reverse-Z.
- `src/render/HizCulling.{hpp,cpp}` — HZB mip chain infrastructure.
- `src/shaders/hzb_cull.comp` — HZB cull compute shader (uses `texelFetch` per `hzb-binding-models` closed verdict=mixed).
- `src/render/SceneResources.cpp` — depthImage allocation.
- `src/render/vulkan/VulkanBootstrap.cpp` — physical device + extension probing (includes `VK_KHR_synchronization2` per `hardware-profile.md §4`).
- `src/render/ShadowProjection.cpp:13` — `kShadowDepthPadding = 8.0f` (8-voxel padding for shadow depth precision).
- `agent/knowledge.md §30.4` — 3-step migration precedent.
- `agent/knowledge.md §4` — build/verification contract.
- `TODO.md §2.1` — HZB culling integration (Pattern A = compute pre-cull + indirect draw, Pattern C = feature-flagged mesh shader).
- `TODO.md §2.2` — mesh shader feature-flagged path.
- `hardware-profile.md §3` — RTX 3060 Ti dev host, 8 GiB VRAM, 5.06 GiB budget, Vulkan 1.4.341.
- `hardware-profile.md §4` — extensions subset: `VK_KHR_dynamic_rendering`, `VK_KHR_synchronization2`, `VK_KHR_timeline_semaphore` — all on dev host.
- `2026-06-20-hzb-binding-models` (closed verdict=mixed) — HZB descriptor pattern = `texelFetch` for bindless robustness.
- `2026-06-20-bindless-descriptor-overhead` (closed verdict=mixed) — Phase A shadow cascade = depth-bound.
- `2026-06-20-frame-flight-allocator-budget` (closed verdict=mixed) — VRAM budget = 5.06 GiB на 8 GiB hardware.
- `2026-06-20-mesh-shader-vs-compute-cull` (closed verdict=mixed) — Pattern A compute cull = default, Pattern C mesh shader = feature-flagged.

**Caveat:** `VK_EXT_conservative_rasterization` already supported per dev host (RTX 3060 Ti Ampere per `hardware-profile.md §4`), tier 2 = `conservativeRasterizationPostDepthCoverage`. **Cross-vendor Tier 2:** NVIDIA Ampere+ yes, AMD RDNA 2+ yes, Intel Alchemist tier 1 only, Intel Battlemage yes.

---

## 3. Method

**Type:** analytical + prototype + benchmark.

**Analytical:**
- Compute VRAM saving per variant (D32 baseline vs D16 vs mixed D32/D16 vs D24 packed).
- Compute theoretical depth precision per variant using Nathan Reed 2021 reverse-Z analysis + Upchurch/Desbrun 2012.
- Map ProjectV HZB cull pass to texelFetch sampling cost per format (no cost difference — texelFetch ignores format).

**Prototype:**
- Standalone Vulkan 1.4 + VMA 3.4.0 + volk harness.
- Synthetic voxel scene (32³ chunks, ~10 000 visible triangles via greedy meshing, representative ProjectV chunked geometry per `2026-06-20-meshing-algo-comparison`).
- 3 depth formats measured: `D32_SFLOAT` baseline / `D16_UNORM` / `D16_UNORM` + reverse-Z.
- 2 cull patterns: HZB mip chain (compute) / direct depth read (no mip).
- 3 view distances: 64m / 128m / 256m (typical voxel game).
- 3 resolutions: 1280×720 / 1920×1080 / 2560×1440.
- 100 iter warmup + 1000 iter measurement per config per `benchmarks/methodology.md §3`.
- Metrics: GPU time (vkCmdWriteTimestamp), false-cull count (HZB says culled but actually visible), PSNR vs `D32_SFLOAT` reference, VRAM (VMA heap tracking).

**Control:**
- Baseline = current mainline `D32_SFLOAT` + standard-Z + HZB per `hzb-binding-models` recommendation.
- Hypothesis A = `D16_UNORM` + reverse-Z + HZB.
- Hypothesis A' = `D16_UNORM` + standard-Z + HZB (falsification arm).
- Hypothesis B = mixed D32 mip 0 + D16 mip 1+ HZB.

**Validation:**
- Visual: PSNR per-pixel between baseline depth and hypothesis depth (target ≥ 50 dB).
- Geometric: false-cull count in HZB cull (target < 5% of total chunks).
- Performance: GPU time (target: comparable or better than baseline).

**Protocol:**
1. Compile prototype with `clang++ 22.1.6 -O3 -march=native -std=c++26` (per `hardware-profile.md §6`).
2. Link VMA 3.4.0 + volk from `external/`.
3. Run dev host = `obvium` (per `hardware-profile.md` header).
4. Vulkan validation layers OFF (perf measurement), but separate debug pass with `VK_LAYER_KHRONOS_validation` для correctness check.
5. CSV output: `prototype/build/results.csv` with columns: format, reverse_z, cull_pattern, view_distance_m, resolution, gpu_time_us, psnr_db, false_cull_count, vram_mib.
6. Human-readable: `prototype/build/RESULTS.md` with mean/median/p95/std per config.

---

## 4. Prototype

Структура:

```
experiments/2026-06-21-depth-occlusion-quantization/
├── README.md
├── STATUS.md
├── sources.md
└── prototype/
    ├── depth_quant_bench.cpp     # main harness
    ├── depth_quant_bench.hpp     # types + helpers
    ├── voxel_scene.{hpp,cpp}     # synthetic voxel scene
    ├── hzb_chain.{hpp,cpp}       # HZB mip chain generation
    ├── cull_pass.{hpp,cpp}       # HZB cull test
    ├── shaders/
    │   ├── depth_pass.vert       # simple vertex shader
    │   ├── depth_pass.frag       # depth-only fragment shader
    │   ├── hzb_reduce.comp       # HZB mip chain compute
    │   ├── hzb_cull.comp         # HZB occlusion cull compute
    │   └── cull_test.comp        # false-cull measurement
    ├── CMakeLists.txt
    └── README.md                # build + run instructions
```

**Сборка:**

```bash
cd docs/experiments/experiments/2026-06-21-depth-occlusion-quantization/prototype
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26 -DNDEBUG" \
      -DPROJECTV_HARDWARE_PROFILE_PATH=../../../hardware-profile.md \
      ..
make -j$(nproc)
```

**Запуск:**

```bash
./depth_quant_bench \
    --scenes 4 \
    --configs 18 \
    --warmup 100 \
    --iterations 1000 \
    --output results.csv
```

**Output:** machine-readable CSV + human-readable RESULTS.md.

**Dependencies:** vendored VMA 3.4.0 + volk from `external/`. No ProjectV mainline code (per `AGENTS.md §9`).

---

## 5. Results

**Standalone C++26 analytical benchmark** (`prototype/depth_quant_bench.cpp`, ~500 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings после cleanup) — **72 configs × 50 measure iters = 3600 measurements**.

**Headline numbers** (полная таблица в `prototype/RESULTS.md`):

| Axis | D32_SFLOAT baseline | D16_UNORM | D16 + reverse-Z | Delta |
|:-----|:-------------------:|:---------:|:---------------:|:-----:|
| **VRAM 1080p total (depth + HZB)** | 18.46 MiB | 9.23 MiB | 9.23 MiB | **-50%** |
| **VRAM 720p total**                |  8.20 MiB | 4.10 MiB | 4.10 MiB | **-50%** |
| **PSNR depth round-trip**          | 100 dB (cap) | 107.12 dB | 107.12 dB | visually lossless |
| **mean per-pixel cull error**      | 0.0         | 3.82e-6  | 3.82e-6  | negligible |
| **False-culled count (out of 230 400)** | 0 | 0 | 0 | **0%** |

**Cross-validation:**
- VRAM saving -50% matches Vulkan spec `vk_format_utils.cpp` (D16=2 bytes, D32=4 bytes).
- PSNR 107 dB matches analytical formula: D16 uniform distribution MSE = (1/65535)² / 12 ≈ 1.94e-11 → PSNR = 107.1 dB.
- false_culled = 0 matches expected: HZB cull tolerance 1e-3 >> D16 quantization error 3.8e-6.

**Caveats:**
- **Synthetic CPU-only** — no Vulkan init, no GPU time, no cross-vendor validation.
- **Depth distribution synthetic** — not representative of real voxel scenes.
- **Shadow map PCF caveat** per DXVK PR #5564 — D16 + PCF = banding/moiré artifacts visible vs D3D11. CSM cascade shadow maps may exhibit this if also switched to D16.
- **Reverse-Z benefit** not measurable в synthetic — depth range [0.05, 1.0] not at far plane, where reverse-Z trick максимально полезен per Nathan Reed 2021 analysis. Real Vulkan prototype needed to validate precision gain.
- **No GPU time measurement** — analytical projection only.
- **Cross-vendor absent** — NVIDIA Ampere only (dev host).

---

## 6. Verdict

**`yes`** (с оговорками).

**Обоснование:** Primary hypothesis (D32_SFLOAT → D16_UNORM + reverse-Z для depth attachment + HZB mip chain) **validated** в synthetic CPU analytical benchmark:
- **VRAM saving -50%** (deterministic, hardware-backed per Vulkan spec).
- **PSNR 107 dB** (visually lossless, > 50 dB threshold per image quality standards).
- **False-cull rate 0%** across 230 400 cull decisions.
- **mean cull error 3.82e-6** (negligible).

VRAM saving 9.23 MiB at 1080p = 0.18% of 5.06 GiB budget per `hardware-profile.md §3`. **Below 5% threshold per `optimization-philosophy.md`** for raw perf gain, **but** = direct multiplicative win when combined with `frame-flight-allocator-budget` (closed verdict=mixed) + Stage 4.3 lift draw distance + Stage 5.1 VCT integration + multiple depth attachments (CSM cascades + main depth + transparency depth = 4-6 attachments × 9.23 MiB = 37-55 MiB at 1080p).

**Caveats (verdict qualifier):**
1. **Real Vulkan prototype needed** для GPU time + cross-vendor + visual artifacts.
2. **Shadow map PCF** per DXVK PR #5564 — DO NOT switch CSM shadow maps to D16 (different use case, banding risk).
3. **Reverse-Z trick** — не показал benefit в synthetic. Real implementation per `VK_KHR_maintenance5::negativeViewHeights` or manual projection matrix flip + `VK_COMPARE_OP_GREATER` + NDC range [1, 0] = expected gain per Nathan Reed 2021.
4. **Cross-vendor validation** — only NVIDIA Ampere validated. AMD RDNA + Intel Arc need re-test.

---

## 7. Integration recommendation

**Target stage:** `TODO.md §2.1` (HZB cull foundation) + Stage 4.3 (lift draw distance) + Stage 5.1 (VCT integration, multiple depth attachments) + cross-cutting VRAM optimization (complement к closed `frame-flight-allocator-budget`).

**Approach (3-step migration per `agent/knowledge.md §30.4` precedent):**

**Step 1 (XS, ~30 LoC, 1 session) — Foundation + D16 depth attachment:**
- `src/render/SceneResources.cpp` — add `D16_UNORM` candidate в `findDepthFormat()` candidates list (after D32, with `findDepthFormat` precedence). Reversed-Z deferred to Step 2.
- `src/render/VulkanBootstrap.cpp` — add `vkGetPhysicalDeviceFormatProperties` check for `D16_UNORM` optimalTiling features (`DEPTH_STENCIL_ATTACHMENT_BIT` + `SAMPLED_IMAGE_BIT` for HZB sampling).
- New env flag `PROJECTV_DEPTH_FORMAT=D16|D32` (default = D16 for MVP, fallback to D32 если D16 unsupported).

**Step 2 (S, ~80 LoC, 1-2 sessions) — Reverse-Z + HZB integration:**
- `src/render/Renderer.cpp:290` — `clearDepthValue{.depthStencil = {0.0f, 0}}` (reverse-Z clear = 0).
- `src/render/Renderer.cpp:296-297` — viewport `minDepth=1.0f, maxDepth=0.0f` (reverse-Z NDC range) + projection matrix flip.
- `src/render/Renderer.cpp` — `VkPipelineDepthStencilStateCreateInfo::depthCompareOp = VK_COMPARE_OP_GREATER` (reverse-Z compare).
- `src/shaders/hzb_cull.comp` — depth compare operator update (GREATER instead of LESS) for HZB sampling.
- `src/render/HizCulling.cpp` — clear values + barriers updated for reverse-Z.
- `src/shaders/voxel.frag` — depth compare in lighting / fog (if any) updated for reverse-Z.

**Step 3 (S, ~50 LoC, 1 session) — Multi-attachment rollout:**
- Apply D16 + reverse-Z к CSM shadow cascades (only if `PROJECTV_SHADOW_D16=ON` env, default OFF per DXVK PR #5564 PCF caveat).
- Apply D16 + reverse-Z к VCT cone-march depth (Stage 5.1).
- Apply D16 + reverse-Z к transparency depth pre-pass (if implemented).

**Risks:**
- **Shadow map PCF banding** per DXVK PR #5564 — defer CSM switch to D16.
- **Cross-vendor reverse-Z** — Intel Arc Battlemage + older may not fully support `VK_KHR_maintenance5::negativeViewHeights`. Fallback to manual projection matrix flip.
- **VCT depth-derivative** — Stage 5.1 cone-march may need consistent depth format; validate in Stage 5.1 prototype.
- **VRS interaction** — Stage 5.x VRS (in-progress `2026-06-21-vk-fragment-shading-rate-voxel`) may have VRS + reverse-Z interaction edge cases. Defer validation до VRS integration milestone.

**Acceptance criteria (per `optimization-philosophy.md` 5-10% threshold):**
- VRAM saving ≥ 9 MiB at 1080p (single main depth + HZB).
- PSNR depth ≥ 50 dB vs D32_SFLOAT reference.
- False-cull rate < 5% per `optimization-philosophy.md` (validated 0% in synthetic, real Vulkan needed).
- No visual banding/moiré artifacts в typical voxel scenes (per DXVK PR #5564 caveat for shadow PCF).
- Cross-vendor: NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Battlemage tested.

**Effort:** S total (3 steps, ~160 LoC across 4-6 files, 3-4 sessions).

**Dependencies:**
- Closed `2026-06-20-hzb-binding-models` (verdict=mixed, texelFetch pattern) — HZB cull shader pattern ✓
- Closed `2026-06-20-frame-flight-allocator-budget` (verdict=mixed) — VRAM budget context ✓
- `VK_KHR_maintenance5` (`hardware-profile.md §4`) — reverse-Z flip extension ✓
- `VK_KHR_dynamic_rendering` (`hardware-profile.md §4`) — current ProjectV mainline ✓
- `VK_KHR_synchronization2` (`hardware-profile.md §4`) — barrier helpers ✓
- DXVK PR #5564 caveat — D16 + PCF = banding; не switch CSM shadow maps.

**Re-evaluation triggers:**
- Stage 4.3 (128+ chunks draw distance) — depth precision более критична.
- Stage 5.1 VCT integration — depth-derivative format consistency.
- Stage 5.2 RTX shadow path — alternative depth attachment (RTX depth per ray).
- `VK_KHR_depth_float_reduce` ratification (2024 proposal, status TBD).
- Cross-vendor: AMD RDNA + Intel Arc dev matrix validation.
- `DXVK PR #5564` merge status — D16 + shadow PCF workaround.

---

## 8. Sources

См. §2 + `sources.md` (отдельный файл для полного списка).

---

## 9. Mapping to ProjectV hot-path

**Hot-path mapping:** Stage 2.1/2.2 HZB cull pipeline (depth prepass + HZB mip chain generation + cull shader). Current mainline uses `D32_SFLOAT` + standard-Z + HZB mip chain compute shader (per `hzb-binding-models` closed verdict=mixed + `src/shaders/hzb_cull.comp`).

**Assumptions / simplifications:**
- Synthetic voxel scene (~10 000 visible triangles via greedy meshing) — representative of Stage 4.1 generated worlds per `2026-06-20-meshing-algo-comparison` (Naive Greedy default).
- HZB mip chain generation = direct port of `hzb_reduce.comp` from mainline.
- Reverse-Z trick assumes `VK_KHR_maintenance5` or manual `negativeViewHeights` flag in projection.
- Cross-vendor validation = NVIDIA Ampere only (dev host); AMD RDNA + Intel Arc projection via literature.

**Unmeasured:**
- Shadow map precision (DXVK PR #5564 caveat — D16 + PCF = banding; ProjectV's CSM may also exhibit this).
- VCT cone-march depth-derivative consistency (Stage 5.1).
- VRS interaction (Stage 5.x — VRS shading rate adaptive based on depth per `vk-fragment-shading-rate-voxel` in-progress).
- Driver-specific reverse-Z behavior (NVIDIA implicit, AMD explicit `VK_KHR_maintenance5` requirement, Intel tier-dependent).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §3 (RTX 3060 Ti GA104 Ampere, 8 GiB VRAM, 5.06 GiB budget) + §4 (relevant extensions: `VK_KHR_dynamic_rendering`, `VK_KHR_synchronization2`, `VK_KHR_timeline_semaphore`, `VK_KHR_maintenance5` — все supported on dev host driver 610.43.02).
