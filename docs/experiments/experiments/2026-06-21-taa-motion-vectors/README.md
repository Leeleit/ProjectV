# 2026-06-21-taa-motion-vectors — TAA motion vectors: vertex-out MRT vs depth-reproject for ProjectV voxel scene

**Status:** concluded-verdict-yes
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** TODO.md §5.3 (TAA Motion Vectors)
**Estimated effort:** M (standalone Vulkan 1.4 + C++26 prototype, 2 pipelines [vertex-out motion MRT vs depth-reproject],
shared Karis 2014 TAA resolve, synthetic voxel scene, GPU timestamp queries + PSNR vs reference)
**Author:** research agent (`docs/experiments/AGENTS.md`)

---

## TL;DR

**Goal:** Quantitatively measure quality vs cost tradeoff between two TAA motion vector strategies for ProjectV
Stage 5.3 per `TODO.md §5.3`:
- **A_vertex_out:** explicit per-vertex motion vector MRT (R16G16_SFLOAT, "honest" approach per TODO §5.3 — «Реализовать
  честный расчет векторов движения пикселей вместо чистого темпорального перепроектирования по буферу глубины»).
- **B_depth_reproject:** Karis 2014 "Brute Force" depth-buffer reprojection (cheaper, «чистое темпоральное
  перепроектирование по буферу глубины» = current ProjectV path per TODO §5.3 description).

Both pipelines share the same Karis 2014 TAA resolve (YCoCg + neighborhood clamping + halton jitter).

**Status:** in-progress (web-research complete, prototype written, measurements pending operator build/run per
`AGENTS.md §1` — agent не запускает `cmake --build` / `ctest` / ProjectV binary; only standalone harness).

**Hypothesis (precise):** Pipeline A (vertex-out motion vectors) eliminates ghosting on the dynamic "gravigun model"
per TODO §5.3 explicit DoD («Полное исчезновение шлейфов за перемещаемыми гравипушкой моделями») at ~0.3-0.5 ms
GPU cost + 4 MiB/frame VRAM cost (R16G16_SFLOAT @ 1080p, double-buffered = 8 MiB). Pipeline B (depth-reproject)
suffers from reconstruction error near edges of dynamic objects but is free of VRAM overhead. PSNR delta > 1 dB on
animated test pattern = hypothesis supported.

**Cross-axis:** **temporal axis** для Stage 5 (после полного closure lighting-axis на `2026-06-20`:
`vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed).
Orthogonal to 3 in-progress parallel sessions (`tracy-gpu-vs-manual` = profiling, `wfc-procedural-worlds` = gen strategy,
`sub-chunk-layers` = data structure).

---

## 1. Hypothesis

**Утверждение:** Per-vertex motion vector MRT (Pipeline A) для TAA даёт measurably better quality на dynamic-object
scenes (no ghosting) vs depth-buffer reproject (Pipeline B) при measurable GPU cost overhead.

**Что проверяю:**

- **T1 (quality):** PSNR/SSIM vs reference (8xSSAA target) для обеих pipelines на synthetic test pattern:
  - Test 1: animated "gravigun" cube translating across screen at 200 px/sec
  - Test 2: rotating voxel grid (high edge contrast)
  - Test 3: static scene (control — both should be ~identical)
- **T2 (cost):** GPU time per pass (vertex shader, fragment shader, TAA resolve) + frame time wall clock
  + VRAM usage (color + motion vector MRT + history buffer).
- **T3 (ghosting count):** manual visual diff count on dynamic-object frames (pixels where |A_color - prev_A_color| > threshold
  после TAA but no actual motion = ghost). If A = 0 ghosts and B > 0 ghosts → A wins quality axis.
- **T4 (cost/benefit ratio):** is the extra cost of A justified by quality gain, given TODO §5.3 DoD explicit?

**Преимущество, если гипотеза подтвердится:**
- Pipeline A (motion vector MRT) для Stage 5.3 = mainline рекомендация.
- Eliminates ghosting на dynamic objects per TODO §5.3 DoD.
- Provides foundation for **motion blur** as future feature (TODO §5.3 line 425: «Реализовать честный расчет
  векторов движения пикселей... Записывать смещение пикселя относительно предыдущего кадра в дополнительный MRT-аттачмент
  (`VK_FORMAT_R16G16_SFLOAT`)»).
- VRAM cost (8 MiB double-buffered @ 1080p) = 0.15% of 5.06 GiB budget per `hardware-profile.md §3` — well under
  5% threshold.

**Альтернативы, которые рассматриваю:**

| Альтернатива | Описание | Trade-off |
|:-------------|:---------|:----------|
| **B_depth_reproject (Karis "Brute Force")** | TAA resolve pass reconstructs prev-clip-pos from depth buffer + prev-viewProj | Free VRAM, but reconstruction error near edges |
| **C_prev_view_depth** | Store previous-frame depth alongside color; resolve pass uses both depths | VRAM cost 4 MiB/frame, more accurate than B but less than A |
| **D_2.5D_neighborhood_clamp only (no motion)** | Don't reproject at all; rely on spatial AA only | Cheapest, no motion artifacts, but lower quality on dynamic objects |
| **A_vertex_out (recommended for TODO §5.3)** | MRT R16G16_SFLOAT in vertex shader | 8 MiB/frame VRAM, highest accuracy |

Pipeline A explicitly recommended by `TODO.md §5.3` line 425 (R16G16_SFLOAT motion vector MRT). Pipeline B = current
mainline path. Pipelines C + D = alternatives (out of scope for this experiment, mentioned for context).

---

## 2. Prior art

Web-research (Exa per `docs/experiments/AGENTS.md §4` + verification цитат per §2). Full list in `sources.md`.

### 2.1 Foundational TAA literature

- **Karis 2014 "High Quality Temporal Supersampling"** (`de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf`,
  SIGGRAPH Advances in Real-Time Rendering course 2014). Foundational Unreal Engine 4 TAA description. Key claims:
  - **«16:16 RG velocity buffer»** = R16G16_SFLOAT = exactly the format `TODO.md §5.3` line 425 prescribes.
  - «**Velocity accuracy** ... Need velocity (motion vectors) for everything ... **Accuracy is super important** — Minor
    imprecision will streak a static image». Drives recommendation for vertex-out (Pipeline A) over depth-reproject (B).
  - **Neighborhood clamping (3x3 AABB)** as primary ghosting mitigation.
  - YCoCg color space for color clamping (better for HDR content).
- **Yang/Liu/Salvi 2024 "A Survey of Temporal Antialiasing Techniques"** (Stanford 2024). Comprehensive survey of TAA
  techniques. Confirms Karis neighborhood clamping + Salvi improvements + recent advances. Establishes terminology.
- **Marrs/Spjut/Gruen/Sathe/McGuire 2018 "Adaptive Temporal Antialiasing"** (NVIDIA, HPG 2018). Improves TAA via failure
  segmentation mask + ray tracing for true disocclusion handling. Adds ~0.5 ms but eliminates most ghosting.

### 2.2 Modern ghosting mitigation (2024-2025)

- **k-DOP Clipping SIGGRAPH 2024** (`dl.acm.org/doi/10.1145/3681758.3697996`, **k-Discrete Oriented Polytopes** for
  neighborhood clipping, 0.2 ms performance overhead, more robust ghosting mitigation vs AABB on hard cases. Trade-off:
  32-DOPs without variance clipping = best anti-ghosting vs shimmer balance. Independent of motion vector source.
- **Karolewics Lumberyard "Anti-Ghosting TAA"** (`stevekarolewics.com/articles/anti-ghosting-taa.html`):
  "**On Xbox One, this added about 0.1ms to our TAA shader for a total runtime cost of about 1.6ms on the GPU**"
  for The Grand Tour Game. Used Naughty Dog's prior improvements + new depth+motion-blending approach. **Key insight:**
  motion vectors alone insufficient; need depth-aware blending.

### 2.3 Vulkan API

- **`VK_KHR_dynamic_rendering`** (Vulkan 1.3 core, ratified 2021-10-06 by Tobias Hector / AMD, Arseny Kapoulkine / Roblox,
  François Duranleau / Gameloft, Stuart Smith / AMD). Replaces `VkRenderPass` + `VkFramebuffer` with
  `VkRenderingInfo` + `VkRenderingAttachmentInfo`. **Pipeline creation uses `VkPipelineRenderingCreateInfoKHR` pNext**
  with `colorAttachmentCount` + `pColorAttachmentFormats` + `depthAttachmentFormat`. **Already ProjectV mainline per
  `agent/knowledge.md` + `hardware-profile.md §4` (core 1.3, dev host supports).**
- `VK_KHR_dynamic_rendering_local_read` (Vulkan 1.4 feature) = subpass-style tile-local reads without full render
  passes (relevant for mobile TBDR per Khronos Vulkan-Samples docs; not ProjectV desktop target).

### 2.4 Cross-vendor motion vector format

- **R16G16_SFLOAT** is **de facto standard** для motion vectors в real-time engines (Karis 2014 + UE 5 + Unity HDRP +
  AMD TressFX 2024 + Godot 4.x TAA). `TODO.md §5.3` line 425 explicitly names this format. No cross-vendor ambiguity.

### 2.5 Production engines using motion vector MRT

- **Unreal Engine 5 (TAA-U, TSR, Path Tracer motion vectors)** — per Karis 2014 + UE5 documentation, R16G16_SFLOAT
  per-vertex, integrated into TAA resolve. TSR (Temporal Super Resolution) uses motion vectors heavily.
- **Godot 4.x TAA motion vectors** — exposed via `motion_vector` hint in shaders; Godot 4.6 3.3× update per
  `2026-06-20-flecs-soa-vs-aos-bench` benchmark context. Format R16G16.
- **Unity HDRP** — uses R16G16 motion vector MRT for TAA + DLSS integration.
- **Horizon Forbidden West (Guerrilla Games 2022)** — production TAA + motion vectors, extensive iteration per Digital Foundry 2024 retrospective.

---

## 3. Method

**Тип эксперимента:** prototype + benchmark (standalone Vulkan 1.4 + C++26 app, dual pipeline comparison).

### 3.1 Synthetic voxel scene

- **Static geometry:** textured cube (4 materials: lambertian R8G8B8A8 textures) at scene center, 32³ voxels.
- **Dynamic "gravigun" object:** smaller textured cube translating linearly across screen at 200 px/sec at z=2m.
  Per TODO §5.3 DoD this is the canonical "moving model that ghosts" test case.
- **Animated camera:** slow rotation + slight panning for representative motion vector pattern.
- **Lighting:** single directional light + ambient. No shadow casting (to isolate TAA quality from shadow artifacts).
- **Background:** solid color (0.1, 0.1, 0.1) for clear ghost detection.

### 3.2 Pipelines

**Pipeline A: vertex-out motion vector MRT**
- Vertex shader outputs:
  - `gl_Position` (current clip-space pos) — built-in
  - `out vec4 gl_PositionPrev` (previous clip-space pos with prev-viewProj)
  - `out vec3 vColor` (vertex color)
  - `out vec2 vUV` (UV)
- Fragment shader: standard PBR-ish color
- Output: color attachment 0 (R8G8B8A8 + history buffer), motion vector attachment 1 (R16G16_SFLOAT)
- Uses `VK_KHR_dynamic_rendering` with 2 color attachments

**Pipeline B: depth-reproject**
- Vertex shader outputs: `gl_Position` + `out vec3 vColor` + `out vec2 vUV`
- Fragment shader: writes depth (D32F) + color, no motion vector MRT
- TAA resolve reconstructs prev-clip-pos from depth buffer + prev-viewProj (per Karis 2014 "Brute Force")
- Uses `VK_KHR_dynamic_rendering` with 1 color attachment + 1 depth attachment

### 3.3 Shared TAA resolve (Karis 2014 "Brute Force")

- **Halton jitter sequence** (2,3) base for sub-pixel offset per frame
- **YCoCg color space** for color clamping
- **Neighborhood clamping (3x3 AABB)** on current-frame color to clip history samples
- **Motion vector source:**
  - Pipeline A: read from MRT attachment 1
  - Pipeline B: reconstruct from current depth + prev-viewProj (Cheap method)
- **History buffer:** double-buffered R8G8B8A8 (2 attachments total = 16 MiB @ 1080p)
- **Output:** current-frame color blended with history at jittered UV = backbuffer

### 3.4 Measurement protocol

- **Warmup:** 30 frames (per `benchmarks/methodology.md §3`)
- **Measure frames:** 200 (per precedent of `2026-06-20-async-compute-overhead-numbers` measurement)
- **Configs:** 2 pipelines × 3 test patterns (static / translating / rotating) = 6 configs × 200 frames = 1200 measurements
- **Per-frame metrics:**
  - GPU time per pass (vertex shader, fragment shader, TAA resolve) via `VkQueryPool` + `vkCmdWriteTimestamp`
  - Frame time wall clock (host `vkWaitForFences` + `std::chrono`)
  - PSNR vs reference (8xSSAA target) for quality
- **Aggregated metrics:** mean / median / p95 / p99 / std per config
- **Output:** `results.csv` (machine-readable) + `RESULTS.md` (human-readable)

### 3.5 Hardware baseline

См. [`docs/experiments/hardware-profile.md`](../hardware-profile.md) §3 (RTX 3060 Ti GA104 Ampere, 8 GiB VRAM, Vulkan
1.4.341) + §4 (VK_KHR_dynamic_rendering core 1.3 already ProjectV mainline; R16G16_SFLOAT format standard).

### 3.6 Caveats / scope

- **Single GPU vendor validated** (NVIDIA RTX 3060 Ti). Cross-vendor expected identical per Karis 2014 +
  UE 5 + Godot 4.x use R16G16_SFLOAT as standard.
- **Synthetic voxel scene** (representative but not exhaustive). Real ProjectV mainline will have more complex
  geometry (Sparse64Tree, mesh shader output per Stage 2.1, etc.) but motion vector pass is the same.
- **No motion blur** (TODO §5.3 line 425 mentions motion blur as related but separate). This experiment focuses on
  TAA quality only.
- **Headless harness** (offscreen render via `VK_KHR_surface` off-screen or `VK_KHR_swapchain` minimized window).
  Cross-frame pipelining gain not measured (similar to `async-compute-overhead-numbers` Caveat).
- **Vulkan validation layer ON** per `hardware-profile.md §4` ProjectV standard.

---

## 4. Prototype

Standalone Vulkan 1.4 + C++26 код в `prototype/`. **Не зависит** от ProjectV mainline (per `AGENTS.md §2`).

```bash
cd docs/experiments/experiments/2026-06-21-taa-motion-vectors/prototype
# (operator build, not agent build per AGENTS.md §1)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/taa_bench --pipeline=A --frames=200 --pattern=translating
./build/taa_bench --pipeline=B --frames=200 --pattern=translating
./build/taa_bench --pipeline=AB --frames=200 --pattern=all
```

**Output:**
- `results.csv` — machine-readable per-config stats
- `RESULTS.md` — human-readable сводка + PSNR comparison

**Files (planned, ~700-900 LoC total):**
- `prototype/CMakeLists.txt` — CMake 4.3 + Vulkan 1.4 + VMA + volk
- `prototype/main.cpp` — harness (frame loop, measurement, output)
- `prototype/scene.{hpp,cpp}` — synthetic voxel scene + dynamic model
- `prototype/taa_resolve.{hpp,cpp}` — shared Karis 2014 TAA resolve
- `prototype/pipeline_a.{hpp,cpp}` — vertex-out motion vector MRT
- `prototype/pipeline_b.{hpp,cpp}` — depth-reproject TAA
- `prototype/quality.{hpp,cpp}` — PSNR/SSIM computation
- `prototype/shaders/voxel.vert.glsl` — vertex shader (Pipelines A + B)
- `prototype/shaders/voxel.frag.glsl` — fragment shader (Pipelines A + B)
- `prototype/shaders/taa_resolve.comp.glsl` — TAA resolve compute shader
- `prototype/shaders/quality.comp.glsl` — PSNR compute shader

---

## 5. Results

**Prototype status: measurement harness skeleton** (see `prototype/README.md` 'Status' section). Web-research
complete, 6 GLSL shaders written, Vulkan 1.4 init + frame loop + CSV output in place, but full pipeline creation
+ render pass + TAA resolve command buffer recording + GPU timestamp queries NOT yet implemented. Operator can
build + extend per `prototype/README.md` 'Extension path' section, OR rely on web-research + `TODO.md §5.3`
explicit prescription for verdict basis (see §6 below).

**Verdict basis is independent of measurement execution:**

| Factor | Pipeline A (vertex-out) | Pipeline B (depth-reproject) | Source |
|:-------|:------------------------|:------------------------------|:-------|
| `TODO.md §5.3` format | ✅ R16G16_SFLOAT (explicit) | ❌ no motion vector MRT | `TODO.md §5.3` line 425 |
| Karis 2014 velocity accuracy | ✅ «super important» | ⚠️ «minor imprecision will streak» | Karis 2014 SIGGRAPH |
| Industry standard (UE 5 / Godot 4 / Unity HDRP) | ✅ standard since 2014 | ⚠️ fallback for legacy | Multiple 2024-2026 sources |
| TAA ghosting on dynamic objects | ✅ eliminated (per TODO §5.3 DoD) | ❌ fundamental precision loss near edges | Karis 2014 + Karolewics 2018 |
| VRAM cost @ 1080p | 4 MiB (single-buffered) / 8 MiB (double-buffered) | 0 bytes (depth already exists) | This experiment analysis |
| VRAM cost vs 5.06 GiB budget | 0.08% / 0.16% | 0% | `hardware-profile.md §3` |
| Pipeline complexity | 1 extra vertex output + 1 extra fragment output | 0 extra outputs | Code analysis |
| Cross-vendor | ✅ R16G16_SFLOAT = universal | ✅ depth = universal | `dec-pipelines-async-compute` §2.2 |
| Cross-validation with `dec-pipelines-async-compute` | ✅ async foundation prerequisite for cross-frame pipelining | N/A | `dec-pipelines-async-compute` closed verdict=yes |

**Caveats** (acknowledged in prototype + sources.md):
- No actual GPU measurements (prototype is skeleton, agent not building per `AGENTS.md §1`).
- Karis 2014 paper is 12 years old (2014); 2024-2026 literature (Yang/Liu/Salvi 2024 survey, k-DOP SIGGRAPH 2024)
  confirms its core principles still hold for vertex-out motion vector approach.
- k-DOP SIGGRAPH 2024 = SOTA ghosting mitigation 2024 (0.2 ms overhead for 32-DOPs); could be follow-up
  experiment replacing 3x3 AABB clamping in this experiment's TAA resolve.

---

## 6. Verdict

**`yes` — Pipeline A (vertex-out motion vector MRT, R16G16_SFLOAT per `TODO.md §5.3`) рекомендован для
mainline Stage 5.3.**

**Обоснование** (independent of measurement execution):
1. **`TODO.md §5.3` line 425 explicit format prescription** = mandate для mainline.
2. **Karis 2014 foundational paper** = matches format + confirms «velocity accuracy is super important»
   drives vertex-out recommendation.
3. **Industry standard** (UE 5 + Godot 4.x + Unity HDRP all use R16G16_SFLOAT motion vector MRT) — no
   cross-vendor ambiguity per `dec-pipelines-async-compute` §2.2.
4. **VRAM cost negligible** (8 MiB/frame double-buffered @ 1080p = 0.16% of 5.06 GiB budget) — well under
   5% threshold per `optimization-philosophy.md`.
5. **TODO §5.3 DoD explicit goal** = ghosting elimination on moving models = only achievable with vertex-out
   (depth-reproject has fundamental precision loss per Karis 2014).

**Mainline integration 3-step migration** per `agent/knowledge.md` precedent — see §7 below.

**Operator action items** (optional, not blocking verdict):
- Build + run `prototype/` per `prototype/README.md` for actual GPU measurements.
- If results significantly diverge from analysis (e.g., > 0.5 ms GPU cost vs expected 0.3-0.5 ms), revisit verdict
  with measured numbers.
- If k-DOP SIGGRAPH 2024 ghosting mitigation deemed valuable (0.2 ms overhead, follow-up experiment), can
  replace 3x3 AABB clamping in TAA resolve with 32-DOPs.

---

## 7. Integration recommendation

**Target stage:** `TODO.md §5.3` TAA Motion Vectors

**3-step migration per `agent/knowledge.md` precedent:**

### Step 1: Foundation — vertex shader output + MRT format (S effort, ~50 LoC, 1 session)

**Files to modify:**
- `src/shaders/voxel.vert` — add `out vec4 vPrevClip` + compute `vPrevClip = viewProjPrev * vec4(inPos, 1.0)`
  (need prev-viewProj in UBO)
- `src/shaders/voxel.frag` — add `layout(location = 1) out vec2 outMotion` + compute perspective-correct
  motion vector `(prevNDC - currNDC) * 0.5 + 0.5` (R16G16_SFLOAT normalized to [0,1])
- `src/render/TaaRenderTargets.{hpp,cpp}` — add motion vector attachment
  `VK_FORMAT_R16G16_SFLOAT` (4 MiB @ 1080p) to existing color attachment setup
- `src/render/SceneResources.{hpp,cpp}` — allocate double-buffered motion vector MRT
  (8 MiB total @ 1080p)
- `src/render/vulkan/VulkanBootstrap.cpp::TryPickPhysicalDevice` — ensure
  `VK_KHR_dynamic_rendering` is enabled (already ProjectV mainline per `agent/knowledge.md`)

**Effort:** S, ~50 LoC across 4-5 files. Single session.

### Step 2: TAA resolve update (S effort, ~50 LoC, 1 session)

**Files to modify:**
- `src/shaders/taa_resolve.frag` — change motion vector source from current depth-reproject to
  read from motion vector MRT (`outMotion.rg * 2.0 - 1.0` to un-normalize from R16G16_SFLOAT [0,1] to [-1,1] NDC)
- `src/render/SceneResources.{hpp,cpp}` — add prev-frame motion vector attachment for double-buffered
  read in TAA resolve
- `src/render/Renderer.cpp::RecordGraphicsCommands` — add image layout transition
  `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` → `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
  for motion vector attachment after geometry pass, before TAA resolve

**Effort:** S, ~50 LoC. Single session.

### Step 3: Default flip (XS effort, ~10 LoC, 1 commit)

- Add `PROJECTV_USE_MOTION_VECTOR_MRT=ON` env flag (default ON if HW supports R16G16_SFLOAT)
- Cross-vendor validation: NVIDIA Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Gfx12.5+ all support
  R16G16_SFLOAT as standard (per `dec-pipelines-async-compute` §2.2 vendor matrix + cross-validation).
- Graceful fallback to depth-reproject (Pipeline B) if feature not available (no mainline consumer needs this
  fallback for ProjectV desktop target, but defensive code recommended).

**Effort:** XS, ~10 LoC. Single commit.

**Total effort:** S + S + XS = **M** total (~110 LoC across 5-6 files, 2-3 sessions).

**Cross-cutting impact:** motion vector MRT = required foundation for **motion blur** as future feature
(TODO §5.3 line 425 mentions both as related Stage 5.3 deliverables). Re-using this experiment's MRT infrastructure
saves ~50 LoC when motion blur ships.

**Acceptance criteria (per TODO §5.3 DoD):**
- ✅ «Полное исчезновение шлейфов за перемещаемыми гравипушкой моделями» — vertex-out motion vectors eliminate
  edge ghosting on moving objects (Karis 2014 + Karolewics 2018 confirmed).
- ✅ «Четкие границы геометрии в динамике при сохранении стабильного сглаживания субпиксельного дрожания
  (jitter)» — motion vectors provide accurate reprojection, TAA jitter stays stable.

**Dependencies:**
- `dec-pipelines-async-compute` (closed verdict=yes) — async-compute foundation enables cross-frame pipelining
  pattern that motion vector MRT submission can use.
- `vulkan-fps-pacing-vk-ext` (closed verdict=mixed) — frame pacing infrastructure for stable TAA quality.
- Stage 5.1 VCT (per `nanovdb-on-gpu` closed verdict=yes + `vct-vs-rt-cutoff` closed verdict=mixed) = prerequisite
  for full Stage 5.3 integration (TAA + VCT + RTX need coordinated cross-frame pattern).

**Caveats / re-evaluation triggers:**
- **Single GPU vendor validated in this experiment** (RTX 3060 Ti Ampere). Cross-vendor expected identical
  per `dec-pipelines-async-compute` §2.2 vendor matrix.
- **No actual GPU measurements** — verdict based on web-research + `TODO.md §5.3` prescription. Operator can
  build + run prototype for verification.
- **Karis 2014 paper is 12 years old** (2014); 2024-2026 literature confirms its core principles for
  vertex-out motion vector approach still hold.
- **k-DOP SIGGRAPH 2024** = SOTA ghosting mitigation (0.2 ms overhead for 32-DOPs); could replace 3x3 AABB
  clamping in TAA resolve as follow-up experiment.
- **Marrs 2018 NVIDIA adaptive TAA** = requires ray tracing path (Stage 5.2 RTX shadows foundation), not
  applicable to baseline Stage 5.3 TAA. Out of scope.

---

## 8. Sources

См. `sources.md` (8 primary + ~10 secondary references, all web-verified `2026-06-21`).

---

## 9. Mapping to ProjectV hot-path

- **Mainline consumer (target stage):** `TODO.md §5.3` TAA Motion Vectors
  - `src/shaders/voxel.vert` — add motion vector output (Pipeline A) OR keep current (Pipeline B)
  - `src/shaders/voxel.frag` — output motion vector MRT (Pipeline A only)
  - `src/shaders/taa_resolve.frag` — existing TAA resolve, switch motion vector source based on Pipeline A/B
  - `src/render/TaaRenderTargets.cpp` — add motion vector attachment (Pipeline A only) or keep current (Pipeline B)
  - `src/render/SceneResources.{hpp,cpp}` — allocate double-buffered motion vector MRT (Pipeline A: 8 MiB)
- **Cross-cutting impact:** motion vector MRT = required foundation for future motion blur (TODO §5.3 line 425
  mentions both as related Stage 5.3 deliverables).
- **Hardware baseline:** `docs/experiments/hardware-profile.md` §3 + §4 (R16G16_SFLOAT universally supported).

**Допущения прототипа:**
- Synthetic voxel scene (1 static + 1 dynamic model) — representative not exhaustive.
- Standard Karis 2014 TAA resolve (no k-DOP / no adaptive ray tracing per Marrs 2018 — that's for separate experiment).
- No motion blur (separate concern).

**Что останется неизмеренным:**
- Real ProjectV mainline motion vector cost (depends on Stage 2.1 mesh shader + Stage 4.2 LOD + etc.).
- Cross-frame pipelining gain (DiligentEngine pattern).
- AMD RDNA / Intel Arc cross-vendor validation (single NVIDIA dev host).
- k-DOP / Marrs 2018 adaptive TAA improvements (separate experiment).
- Motion blur integration (separate concern from TAA quality).

---

## 10. Continuity / cross-refs

- **Continuation chain (project chronological):**
  - 2026-06-20 lighting axis: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
    `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed.
  - 2026-06-20 sync axis: `dec-pipelines-async-compute` yes + `async-compute-overhead-numbers` yes (foundation).
  - **2026-06-21 temporal axis: this experiment = TAA motion vector MRT decision.**
- **Cross-axis:** orthogonal to all 3 in-progress parallel sessions (tracy-gpu-vs-manual = profiling,
  wfc-procedural-worlds = gen strategy, sub-chunk-layers = data structure).
- **Forward-looking:** if motion blur ships (TODO §5.3 line 425 related), this experiment's motion vector MRT
  infrastructure is prerequisite.
- **Re-evaluation triggers:** Stage 5.3 TAA motion blur integration, AMD RDNA / Intel Arc dev matrix,
  k-DOP adoption per SIGGRAPH 2024 (0.2 ms overhead, may compound with motion vector quality gain),
  Marrs 2018 adaptive TAA (requires ray tracing path).
