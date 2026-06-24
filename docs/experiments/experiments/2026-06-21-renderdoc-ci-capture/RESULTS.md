# RESULTS — 2026-06-21-renderdoc-ci-capture

> **Captured:** 2026-06-21 (single session).
> **Prototype:** `prototype/capture_overhead_bench.cpp` (standalone C++26 CPU analytical model,
> Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings**).
> **Output:** `prototype/build/results.csv` (126 lines = 1 header + 125 configs × 1000 frames
> = **125,000 main measurements** per `docs/experiments/benchmarks/methodology.md §3`).
> **Hardware:** dev host `obvium` Zen 3 5800X governor=`powersave` per
> [`docs/experiments/hardware-profile.md`](../hardware-profile.md) §1. Wall time <1 sec.

---

## 1. Aggregated by strategy (mean over 25 configs × 1000 frames each)

| Strategy                 | avg_cpu_overhead | avg_capture_MB/1k_frames | avg_capture_rate | avg_capture_file_KB (when captured) |
|:-------------------------|-----------------:|-------------------------:|-----------------:|------------------------------------:|
| A_NoCapture              | 0.00 %           | 0 MB                     | 0.00 %           | 0 KB                                |
| B_AlwaysOnLayer          | 0.77 %           | 117,534 MB (≈117 GB)     | 100.00 %         | 120,355 KB ≈ 117.5 MB               |
| C_TriggeredOnError       | 0.05 %           | 70 MB                    | 0.06 %           | 48,142 KB ≈ 47.0 MB                 |
| D_PixelDiffBaseline      | 0.12 %           | 1,128 MB                 | 0.96 %           | 120,355 KB ≈ 117.5 MB               |
| E_SelectiveCaptureRange  | 0.09 %           | 1,175 MB                 | 1.00 %           | 120,355 KB ≈ 117.5 MB               |

## 2. Per scene × strategy (CPU overhead mean %)

| Strategy                 | full_voxel | minimal_voxel | stress_voxel | synthetic_golden | typical_voxel |
|:-------------------------|-----------:|--------------:|-------------:|-----------------:|--------------:|
| A_NoCapture              | 0.00       | 0.00          | 0.00         | 0.00             | 0.00          |
| B_AlwaysOnLayer          | 1.21       | 0.31          | 1.21         | 0.51             | 0.63          |
| C_TriggeredOnError       | 0.05       | 0.05          | 0.05         | 0.05             | 0.05          |
| D_PixelDiffBaseline      | 0.13       | 0.12          | 0.13         | 0.12             | 0.12          |
| E_SelectiveCaptureRange  | 0.08       | 0.08          | 0.08         | 0.08             | 0.08          |

## 3. Pass count per scene

| Scene             | active_passes | description                                     |
|:------------------|--------------:|:------------------------------------------------|
| minimal_voxel     | 3             | voxel_mesh + opaque_forward + ui_debug          |
| typical_voxel     | 8             | depth + hzb + voxel_mesh + csm + opaque + taa + ui |
| full_voxel        | 12            | All Stage 0-6 + planned Stage 5.x passes       |
| stress_voxel      | 12            | full_voxel + giant SSBO allocations (2× cost)   |
| synthetic_golden  | 5             | voxel_mesh + csm + opaque + taa + ui (golden)  |

## 4. Key findings

### 4.1 CPU overhead — well below 5-10% threshold per `optimization-philosophy.md`

- **A_NoCapture (baseline):** 0.00 % (reference).
- **B_AlwaysOnLayer:** 0.77 % mean (max 1.21 % on full_voxel). **Below 5% threshold even for capture every frame**.
- **C/D/E:** 0.05–0.12 % mean. **All negligible**. Always-on layer interception cost is minimal per
  RenderDoc Vulkan docs ("low performance overhead while not capturing").
- **Stress voxel (giant SSBOs >1 GB) doubles resource copy cost but NOT proportionally overhead** —
  per-pass CPU overhead model is dominated by per-vkCmd* cost, not per-byte copy.

### 4.2 Capture file size — the real bottleneck

- **Per-capture file size:** ~120 MB for full_voxel scenes (per-frame state + resource copies).
  - Largest single resource: `OpaqueForward` render target 8.3 MiB @ 1080p (RGBA8).
  - `FluidCaPingpong` 64 MiB SSBO copies (worst single pass).
  - `CsmShadow` 4 × 4 MiB depth attachments.
  - Per-vkCmd* 16 B state × ~5000 commands/frame ≈ 80 KB.
- **B_AlwaysOnLayer (100% rate):** 117 GB per 1000 frames = **117 MB/frame** = ~7 GB/min at 60 fps.
  - **PRACTICAL: impractical** для CI (storage + disk I/O).
- **C_TriggeredOnError (0.06% rate):** 70 MB per 1000 frames = **0.07 MB/frame avg**.
  - **EXCELLENT для production** (rare captures).
- **D_PixelDiffBaseline (0.96% rate):** 1.1 GB per 1000 frames = **1.1 MB/frame avg**.
  - **MANAGEABLE для CI** (golden image compare regression test, daily CI run).
- **E_SelectiveCaptureRange (1% rate):** 1.17 GB per 1000 frames = **1.17 MB/frame avg**.
  - **MANAGEABLE для spike isolation** (Stage 5.1 VCT cone-march first 10 frames).

### 4.3 Capture rate drives disk cost linearly

For a 30-min session @ 60 fps = 108,000 frames:
- B_AlwaysOnLayer: **12.7 TB** (impractical).
- C_TriggeredOnError: **7.6 GB** (fine for production).
- D_PixelDiffBaseline: **122 GB** (fine for daily CI with retention).
- E_SelectiveCaptureRange: **127 GB** (fine for weekly CI).

### 4.4 Cross-axis validation per `optimization-philosophy.md`

- **5-10% threshold для perf:** all strategies **below** threshold (max 1.21% for B on stress voxel).
  RenderDoc capture overhead is **not a perf bottleneck** — orthogonal ко всем other RenderDoc-related
  concerns (storage, CI integration, regression detection quality).
- **50 dB PSNR visual-lossless threshold:** PSNR measurement requires actual golden captures +
  `imageDiff` implementation (deferred to mainline integration; vision-regression-kit + Glint3D SSIM
  ≥ 0.995 patterns documented per `sources.md` for production reference).
- **Disk I/O threshold:** Practical concern только для B (100% rate). D/E at ~1% rate acceptable.

## 5. Recommended strategy combination

**D_PixelDiffBaseline + E_SelectiveCaptureRange = recommended pair**:

- **D_PixelDiffBaseline (always-on layer + 1% golden compare):** primary CI gate.
  - Captures 1 frame per 100 frames (~10 captures per second at 60 fps), each ~120 MB.
  - PSNR/SSIM compare vs `tests/regression/golden/*.png` golden images per `sources.md` Glint3D
    pattern (SSIM ≥ 0.995 threshold).
  - Catches shader regressions, descriptor layout changes, blend mode regressions, etc.
- **E_SelectiveCaptureRange (capture first N=10 frames of session):** spike isolation mode.
  - Triggered by `PROJECTV_CAPTURE_START_FRAME` env when Stage 5.1 VCT cone-march spikes detected.
  - Captures first 10 frames of next session for offline inspection.
  - ~120 MB × 10 = 1.2 GB per spike (one-time cost).

**C_TriggeredOnError = production fallback** (PV_ASSERT / NaN detection):

- Captures только на actual error (~0.06% rate based on Poisson model of rare errors).
- 0.05% overhead = **imperceptible** в production.
- Always-on layer minimal interception cost only.

**B_AlwaysOnLayer = NEVER рекомендуется** для production/CI:

- 117 GB на 1000 frames = **impractical** disk cost.
- Even at 0.77% CPU overhead, the disk I/O bottleneck overwhelms the gain.
- Theoretical reference only.

## 6. Integration cost analysis

Per `agent/knowledge.md` 3-step migration precedent:

### Step 1 (XS, ~50 LoC)
- CMakeLists.txt: `option(PROJECTV_CI_PIXEL_DIFF "Enable RenderDoc capture regression CI" OFF)`
- `tests/regression/golden/` directory convention (empty initially)
- `scripts/ci_capture.sh` shell wrapper around `renderdoccmd --capture`
- `src/debug/ProfilingGpu.hpp` — extend `PROJECTV_ENABLE_RENDERDOC_MARKERS` to support capture trigger

### Step 2 (M, ~250 LoC)
- `tests/ProjectVRegressionCaptureTests.cpp` — CTest target с `imageDiff` C++ helper
  (PSNR per Akenine-Möller formula + SSIM per Wang 2004 / Glint3D threshold pattern)
- 12 initial golden captures per Vulkan pass (`tests/regression/golden/{pass}_frame{n}.rdc`)
- `PROJECTV_CAPTURE_TRIGGER` env integration в `src/debug/ProfilingGpu.hpp`
- `renderdog-automation` Rust crate OR `rdc-cli` Python integration (CI script)

### Step 3 (S, ~100 LoC)
- `.github/workflows/capture.yml` — GitHub Actions workflow
- Self-hosted GPU runner vs headless Vulkan (Lavapipe / SwiftShader) matrix
- Slack/Discord webhook на regression detection (PSNR < 50 dB OR SSIM < 0.995)
- PR comment with imageDiff summary (per-region diff heatmap)

**Total ~400 LoC, S-M effort, 2-3 sessions** — matches hypothesis.

## 7. Caveats

- (a) **Analytical overhead model, not real `renderdoccmd` execution.** Binary not installed on dev host
  (`which renderdoccmd` → not found 2026-06-21). Production validation = mainline scope, не this experiment.
- (b) **GPU pass coverage = analytical from ProjectV source code** (`Renderer.cpp` pass list +
  `agent/knowledge.md` 5 sub-passes + TODO.md Stage 0-6 + Stage 5.x planned passes), not runtime
  capture. 12 passes enumerated in `prototype/capture_overhead_bench.cpp` enum class VkPass.
- (c) **Per-pass cost model** — CPU overhead % per pass is conservative analytical estimate based on
  RenderDoc Vulkan docs "low overhead while not capturing" + per-pass state model. Real numbers may
  differ ±50% per Phoronix benchmarks.
- (d) **Cross-vendor CI matrix (Linux + Windows + macOS) not measured on dev host** (Linux only).
  RenderDoc Vulkan layer cross-vendor verified via docs; production validation = mainline scope.
- (e) **Mutation cost (per-edit capture regression) out of scope** — measured at scene level, not
  per-chunk edit granularity.
- (f) **AI/ML CI agents (self-healing CI per Harness 2026 + GitHub Copilot for CI 2025-2026)
  deferred to Phase 4 follow-up** — could be future enhancement for automatic regression triage.
- (g) **Headless Vulkan (SwiftShader / Lavapipe) as CI fallback** not validated на dev host.
  Mesa Lavapipe supports Vulkan 1.4 per Mesa 26.2; production validation = mainline scope.
- (h) **Cross-references in `agent/knowledge.md`** list 5 sub-passes within
  `RecordGraphicsCommands` (shadow / meshing / taaResolve / debugOverlay / debugHud) — my analytical
  model groups these into `OpaqueForward` for simplicity. Future refinement can split into 5 sub-passes
  for finer-grained coverage analysis.

## 8. Continuation chain

None (first CI/tooling axis experiment). Opens **cross-cutting Stage 0 / cross-stage regression axis**
для 50+ closed experiments:

- Follow-up candidate: `_renderdoc-vulkan-1.4-extensions_` — explicit capture coverage для
  `VK_KHR_cooperative_matrix` (tensor-core VCT denoise per in-progress `2026-06-21-vct-temporal-denoise-tensor-core`)
  + `VK_KHR_fragment_shading_rate` (gaze VRS per in-progress `2026-06-21-eye-tracked-foveated`).
- Follow-up candidate: `_renderdoc-cross-vendor-ci_` — Linux + Windows + macOS GitHub Actions matrix.
- Follow-up candidate: `_renderdoc-ai-triage_` — AI agent analysis of regression captures (per
  Harness 2026 / GitHub Copilot CI patterns).
