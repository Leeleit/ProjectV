# STATUS — 2026-06-21-renderdoc-ci-capture

**Status:** concluded-verdict-mixed
**Phase:** A (scaffold) → B (web-research) → C (prototype + measurements) → D (close) ✅.
**Closed:** `2026-06-21` (single session ~3-4h).
**Anti-duplicate sentinel:** clean per `AGENTS.md §13.7`.

---

## Verdict

**`mixed`** — D_PixelDiffBaseline + E_SelectiveCaptureRange recommended as CI regression pair;
C_TriggeredOnError recommended as production fallback; B_AlwaysOnLayer never рекомендуется;
A_NoCapture = baseline.

**Headline numbers** (125,000 main measurements, Zen 3 5800X governor=`powersave` per
`hardware-profile.md §1`, wall time <1 sec):

| Strategy                 | CPU overhead | Capture MB / 1k frames | Capture rate | Notes                                  |
|:-------------------------|-------------:|-----------------------:|-------------:|:---------------------------------------|
| A_NoCapture (baseline)   | 0.00 %       | 0 MB                   | 0 %          | No layer loaded                        |
| B_AlwaysOnLayer          | 0.77 %       | 117,534 MB (117 GB)    | 100 %        | **NEVER production** (impractical disk) |
| C_TriggeredOnError       | 0.05 %       | 70 MB                  | 0.06 %       | Production fallback (rare captures)    |
| **D_PixelDiffBaseline**  | **0.12 %**   | **1,128 MB**           | **0.96 %**   | **CI primary (golden compare)**        |
| **E_SelectiveCaptureRange** | **0.09 %**| **1,175 MB**           | **1 %**      | **Spike isolation (first 10 frames)**  |

**Crosses `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold?**
- **CPU overhead:** все strategies below threshold (max 1.21% for B on stress_voxel).
  RenderDoc capture overhead is **NOT a perf bottleneck** → orthogonal.
- **Disk cost:** B = **impractical** (117 GB per 1000 frames = 12.7 TB per 30-min session @ 60 fps).
  D/E/C = manageable (70 MB — 1.17 GB per 1000 frames).

---

## Phase log

### Phase A — Scaffold + reservation ✅
- ✅ Anti-duplicate sentinel clean per `AGENTS.md §13.7` (rg renderdoc = only cross-refs in
  `tracy-gpu-vs-manual/README.md` + `dec-pipelines-async-compute/README.md:257` + `pipeline_overlap_analysis.md:314`;
  `ls lookdev-captures/` пусто; `ls 2026-06-21-renderdoc*` пусто).
- ✅ Reservation зафиксирована в `research/backlog.md §In progress` per §13.2 (prepended before
  `2026-06-21-eye-tracked-foveated` reservation).
- ✅ Experiment folder created: `experiments/2026-06-21-renderdoc-ci-capture/` + `prototype/` subfolder.
- ✅ README.md scaffold + fully populated per `experiments/_TEMPLATE/README.md` (8 mandatory sections + §9 mapping).
- ✅ STATUS.md created (this file).
- ✅ INDEX.md §5 Active entry (prepended before `hzb-smart-mip-select`).

### Phase B — Web research ✅
- ✅ RenderDoc 1.44 official docs fetched (`renderdoc.org/docs/` + Vulkan Support + In-application API +
  Quick Start).
- ✅ `renderdoccmd` CLI + `rdc-cli` PyPI (2026-06-04) + `renderdog-automation` Rust crate (2026-05-03).
- ✅ CI/CD patterns: `vision-regression-kit`, Glint3D CI issue #6 (SSIM ≥ 0.995 threshold).
- ✅ PSNR/SSIM formulas per Akenine-Möller / Wang 2004 (canonical references).
- ✅ ProjectV cross-refs verified: `src/debug/ProfilingGpu.hpp:14,161,203` + `VulkanBootstrap.cpp:592` +
  `VulkanDebug.cpp:9` + `agent/knowledge.md`.
- ✅ 12 Vulkan passes enumerated per `agent/knowledge.md` + `TODO.md §Stage 0-6` + Stage 5.x planned.
- ✅ 26 primary + secondary references в `sources.md`.

### Phase C — Prototype + measurements ✅
- ✅ Standalone C++26 CPU analytical harness `prototype/capture_overhead_bench.cpp` (~620 LoC).
- ✅ Build green, **0 warnings** per Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`.
- ✅ 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup = **125,000 main measurements** per
  `benchmarks/methodology.md §3`.
- ✅ Wall time <1 sec на Zen 3 5800X.
- ✅ Output: `prototype/build/results.csv` (126 rows = 1 header + 125 configs).
- ✅ Production integration design proposals: `prototype/CMakeLists_design.md` + `prototype/gh_actions_design.md`.
- ✅ `prototype/README.md` with build + run instructions.
- ✅ `RESULTS.md` with aggregated analysis + interpretation + caveats + continuation chain.

### Phase D — Analysis + close ✅
- ✅ Results aggregated by strategy + per-scene (Python csv analysis).
- ✅ Verdict write-up: `mixed` (D + E recommended pair, C production fallback, B never, A baseline).
- ✅ Integration recommendation: 3-step migration per `agent/knowledge.md` precedent
  (~400 LoC, S-M effort, 2-3 sessions).
- ✅ `README.md` fully populated (8 sections + §9 mapping + §10 continuation).
- ✅ `STATUS.md` → status `concluded-verdict-mixed`.
- ✅ `INDEX.md §6 Recent closed update` + `INDEX.md §1 Now-closed entry` (single-pass sync per §13.5).
- ✅ `research/backlog.md §In progress` → `§Closed` (per §13.5).

---

## Caveats

- (a) **Analytical overhead model, not real `renderdoccmd` execution.** Binary not installed on dev host
  (`which renderdoccmd` → not found 2026-06-21). Production validation = mainline scope, не this experiment.
- (b) **GPU pass coverage = analytical from ProjectV source code** (`Renderer.cpp` pass list +
  `agent/knowledge.md` 5 sub-passes + `TODO.md §Stage 0-6` + Stage 5.x planned passes = 12 passes),
  not runtime capture.
- (c) **Per-pass CPU overhead model** — conservative analytical estimate based on RenderDoc Vulkan docs
  "low overhead while not capturing" + per-pass state model. Real numbers may differ ±50% per Phoronix.
- (d) **Cross-vendor CI matrix (Linux + Windows + macOS) not measured on dev host** (Linux only).
  RenderDoc Vulkan layer cross-vendor verified via docs; production validation = mainline scope.
- (e) **Mutation cost (per-edit capture regression) out of scope** — measured at scene level, not
  per-chunk edit granularity.
- (f) **AI/ML CI agents (self-healing CI per Harness 2026 + GitHub Copilot for CI 2025-2026)
  deferred to follow-up** — `rudybear/renderdoc-skill` Claude Code integration documented as future
  enhancement.
- (g) **Headless Vulkan (SwiftShader / Lavapipe) as CI fallback** not validated на dev host.
  Mesa Lavapipe supports Vulkan 1.4 per Mesa 26.2; production validation = mainline scope.

---

## Cross-references

- **ProjectV mainline:**
  - `agent/knowledge.md` — `PROJECTV_ENABLE_RENDERDOC_MARKERS` (existing integration)
  - `agent/knowledge.md` — `RecordGraphicsCommands` 5 sub-passes
  - `agent/knowledge.md` — build/verification contract
  - `agent/knowledge.md` — 3-step migration precedent
  - `src/debug/ProfilingGpu.hpp:14,161,203` — RenderDoc marker integration
  - `src/render/vulkan/VulkanBootstrap.cpp:592` — `VK_EXT_debug_utils` extension load
  - `src/render/vulkan/VulkanDebug.cpp:9` — debug utils integration
  - `TODO.md §Stage 0` — cross-cutting DoD «reproducibility»
  - `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold
  - `legacy/docs/philosophy/03_domain/04_testing-philosophy.md` — test coverage thresholds
  - `docs/experiments/hardware-profile.md §1+§3+§4+§6` — dev host baseline
  - `docs/experiments/benchmarks/methodology.md §3` — measurement protocol

- **Active parallel experiments:**
  - `2026-06-21-tracy-gpu-vs-manual` (live profiling, orthogonal — not CI axis)
  - `2026-06-21-eye-tracked-foveated` (gaze VRS, orthogonal)
  - `2026-06-21-vct-temporal-denoise-tensor-core` (tensor-core VCT denoise, orthogonal)
  - `2026-06-21-gpu-fluid-ca-atomic-strategy` (atomic, orthogonal)
  - `2026-06-21-vulkan-fps-pacing-wayland-prototype` (present pacing, orthogonal)
  - `2026-06-21-vulkan-defragmentation-compaction` (VRAM, orthogonal)
  - `2026-06-21-vk-multi-gpu-split-frame` (multi-GPU, orthogonal)

- **Closed experiments (complementary):**
  - `2026-06-20-dec-pipelines-async-compute` (RenderDoc async capture extension point per
    `agent/knowledge.md`)
  - `2026-06-20-vulkan-fps-pacing-vk-ext` (RenderDoc timeline alternative per §6 line 314)
