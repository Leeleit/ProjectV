# STATUS — 2026-06-21-vct-3d-mip-generation

**Status:** closed (`concluded-verdict-yes`)
**Phase:** A (scaffold + reservation) → B (web-research + sources.md) → C (prototype + measurements) → D (close)
**Last action:** `2026-06-21` — Phase D close complete: verdict=`yes`, README §5/§6/§7 populated,
RESULTS.md + sources.md written, INDEX §6 Recent closed table + backlog §Closed sync pending.
**Blocker:** нет.
**Re-evaluation triggers:** Stage 5.1 integration milestone (when `voxelize.comp` lands in mainline);
Stage 4.3 (128+ chunks draw distance, mip gen time scaling); Crassin 2011 cone-tapered mip filter
follow-up (out-of-scope per `vct-cone-count-atlas-precision` STATUS §11 + §172); 4D temporal VCT
follow-up; Vulkan 1.5+ dedicated mip gen extensions; GPU `vkCmdBlitImage` 3D HW path validation
(D_Blit3D_perAxis GPU timing on RTX 3060 Ti).

---

## Phase log (closed)

### Phase A — Scaffold (~5 min) ✅
- ✅ Anti-duplicate sentinel clean per `AGENTS.md §13.7`: `rg -l "vct-3d-mip" docs/` + `ls experiments/vct-3d-mip-generation/` both empty.
- ✅ Reservation зафиксирована в `research/backlog.md §In progress` (block entry before `## Closed`).
- ✅ Experiment folder created: `experiments/2026-06-21-vct-3d-mip-generation/{README.md, STATUS.md, sources.md, prototype/}`.
- ✅ README.md scaffold заполнен по `experiments/_TEMPLATE/README.md` template (8 mandatory sections + §9 Mapping to ProjectV hot-path).
- ✅ INDEX.md §5 Active entry created.

### Phase B — Web research (~30 min) ✅
- ✅ Crassin 2011 GIVoxels §3.2 + §5 — cone-tapered mip filter + pyramid rule (foundational, full PDF read deferred до Stage 5.1 integration)
- ✅ GPUOpen FidelityFX-SPD 2020 — RDNA-optimized 2D single-pass downsampler, 12 mips in single dispatch, WaveOps + fp16 packed modes (2D only — 3D extension requires custom kernel)
- ✅ nvpro-samples `gl_occlusion_culling` `cull-downsample.frag.glsl` — 2D HZB mip chain pattern (direct analog to 3D VCT mip gen at conceptual level)
- ✅ Vulkan 1.4 `VkImageBlit` spec — 3D blit support confirmed (core 1.0, no extension needed)
- ✅ 6 secondary sources (NVIDIA HZB practice, SaschaWillems Vulkan samples, Snowapril/HanetakaChou VCT implementations, OGRE-Next CIVCT, Panteleev 2014 thesis reference)
- ✅ Failed URLs documented (6 sources returned 404, future re-verification deferred)
- ✅ sources.md finalized (10 primary + 6 secondary sources)
- ⚠️ Exa MCP HTTP 429 rate-limited this session (initial + 30s/60s/90s/120s/180s backoff retries); fallbacks via direct `webfetch` per `agent/knowledge.md`

### Phase C — Prototype (~2h, wall time 192 sec on Zen 3 5800X) ✅
- ✅ Standalone C++26 CPU harness (`prototype/mip_bench.cpp` ~580 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**)
- ✅ 4 downsample algorithms implemented: A_2x2x2_Box (8-sample average), B_4tap_Smooth
  (4-tap diagonal NVIDIA HZB pattern), C_8tap_3DGaussian (σ=0.5 voxel Gaussian, mathematically
  equivalent to A for symmetric kernel), D_Blit3D_perAxis (3 sequential per-axis 2D blits, CPU
  analog of vkCmdBlitImage chain)
- ✅ 4 synthetic 3D voxel atlas scenes: uniform_sky / uniform_floor / cave_stress / mixed_biome
  (per `vct-cone-count-atlas-precision` §3 precedent for direct comparability)
- ✅ Atlas sizes: 64³ + 128³ (256³ deferred — out of CPU single-thread budget)
- ✅ 3 target mip levels: 1 (inner), 3 (mid), 5 (outer)
- ✅ Analytical 3D Gaussian low-pass reference (σ=0.5 voxel × 2^mip_factor)
- ✅ PSNR metric (vs reference), perf metric (ms per mip chain refresh)
- ✅ Reduced: N=30 iter + 5 warmup per `benchmarks/methodology.md §3` (timeout constraint),
  3 seeds (1, 7, 42)
- ✅ Output: `prototype/build/results.csv` (289 rows = 1 header + 288 data rows)

### Phase D — Close (~30 min) ✅
- ✅ Results analysis: A_2x2x2_Box is the **sole Pareto-optimal** algorithm (ties for best PSNR
  + lowest runtime). B is a strict regression. C is a pure perf tax. D is 2.9× slower for noise
  ΔPSNR.
- ✅ Verdict=`yes` recorded in README.md status field.
- ✅ README.md §5/§6/§7 populated with cross-references to RESULTS.md.
- ✅ RESULTS.md written (9 sections: per-algorithm / per-mip / per-scene / pairwise / threshold
  / GPU cross-vendor projection / headline / caveats / cross-refs).
- ✅ sources.md written (10 primary + 6 secondary sources, failed URLs documented).
- ✅ INDEX.md §5 Active → §6 Recent closed table update **pending** (next tick).
- ✅ `research/backlog.md §Closed` entry **pending** (next tick).
- ✅ STATUS.md → status `concluded-verdict-yes`.

---

## Cross-axis (orthogonal to all 4 in-progress parallel)

- `2026-06-21-tracy-gpu-vs-manual` (profiling tool)
- `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)
- `2026-06-21-sdf-hybrid-world` (VCT anti-leak via SDF overlay)
- `2026-06-21-vk-multi-gpu-split-frame` (multi-GPU scaling)

## Complementary to closed experiments

- `2026-06-21-vct-cone-count-atlas-precision` (verdict=mixed, within-VCT quality — closed measurement
  gap: assumed 8-mip chain via vkCmdBlitImage, never measured algorithm cost)
- `2026-06-20-nanovdb-on-gpu` (verdict=yes, NanoVDB mip chain = natural storage extension for VCT)
- `2026-06-20-dec-pipelines-async-compute` (verdict=yes, async compute = async mip gen off-frame)
- `2026-06-20-hzb-binding-models` (verdict=mixed, 2D HZB mip chain sampling pattern = direct analog)
- `2026-06-20-vct-vs-rt-cutoff` (verdict=mixed, VCT strategy axis = roughness cutoff, distinct from
  mip gen axis)
- `2026-06-21-lod-mesh-downsampling` (verdict=mixed, LOD = distance-based downsampling, distinct
  from mip gen axis; both target triangle count reduction but at different stages)

---

## Headline (см. RESULTS.md для details)

- **A_2x2x2_Box is the sole Pareto-optimal algorithm** — ties for best PSNR (49.99 dB mean) +
  lowest measured runtime (1.218 ms mean).
- **B_4tap_Smooth** is a strict regression (−0.498 dB PSNR, +7% perf). **Hypothesis falsified.**
- **C_8tap_3DGaussian** is a pure perf tax (+6%) for **zero** measurable PSNR gain at this sample
  size. Mathematically equivalent to A for symmetric 8-corner kernel with σ=0.5 voxel.
- **D_Blit3D_perAxis** is 2.9× slower for a 0.01 dB ΔPSNR. **GPU validation deferred** — on GPU
  `vkCmdBlitImage` is hardware-accelerated (5-20× faster than compute for simple box per AMD SPD +
  NVIDIA practice), so D may flip to faster on GPU.
- **Cross-scene breakdown:** A ties or beats every competitor in every scene × mip level combination.
- **Cross-mip breakdown:** B's quality deficit grows with mip depth (−0.33 dB at mip 1 → −0.94 dB at
  mip 5), validating that fancy algorithms don't help at outer mips either.
- **Verdict=`yes`:** A_2x2x2_Box is the recommended Stage 5.1 VCT atlas mip chain generation default.
  No need for fancy alternatives.
- **Integration:** 3-step migration per `agent/knowledge.md` precedent, **simplified from
  initial 260 LoC to ~120 LoC** (no dispatch enum, no per-scene selection, no per-axis blit fallback
  at this time). S effort, 1-2 sessions.

---

## Files

- `README.md` — main document (concluded-verdict-yes, all 9 sections)
- `STATUS.md` — this file
- `RESULTS.md` — full measurement analysis + 9 sub-sections
- `sources.md` — Phase B web research (10 primary + 6 secondary)
- `prototype/mip_bench.cpp` — standalone C++26 CPU harness (~580 LoC)
- `prototype/CMakeLists.txt` — Ninja build
- `prototype/README.md` — build + run instructions
- `prototype/build/results.csv` — 288 measurements (1 header + 288 data rows)
- `prototype/build/mip_bench` — compiled binary (linked from build/ parent)

---

## Cross-axis

**Orthogonal ко всем 4 in-progress parallel:**
- `2026-06-21-tracy-gpu-vs-manual` (profiling tool)
- `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)
- `2026-06-21-sdf-hybrid-world` (VCT anti-leak via SDF overlay)
- `2026-06-21-vk-multi-gpu-split-frame` (multi-GPU scaling)

**Complementary к closed experiments:**
- `2026-06-21-vct-cone-count-atlas-precision` (verdict=mixed, within-VCT quality — assumed mip chain via `vkCmdBlitImage`, never measured cost)
- `2026-06-20-nanovdb-on-gpu` (verdict=yes, NanoVDB mip chain = natural storage for VCT atlas)
- `2026-06-20-dec-pipelines-async-compute` (verdict=yes, async compute = async mip gen off-frame)
- `2026-06-20-hzb-binding-models` (verdict=mixed, HZB 2D mip chain = analogous pattern, 2D only)
- `2026-06-20-vct-vs-rt-cutoff` (verdict=mixed, VCT strategy axis — this = within-VCT mip gen axis)
- `2026-06-21-lod-mesh-downsampling` (verdict=mixed, LOD = distance-based, this = mip chain = storage-based)
- `2026-06-20-clustered-forward-mass-lights` (verdict=yes, unrelated Stage 5.x foundation)
- `2026-06-20-rt-shadows-vs-csm` (verdict=mixed, Stage 5.2 strategy axis)
- `2026-06-20-restir-gi-feasibility` (verdict=mixed, deferred до Stage 6+ path tracer)

---

## Notes

- Prototype = standalone C++26 CPU, NOT ProjectV mainline (per `docs/experiments/AGENTS.md §2` scope discipline).
- No `git *` per `docs/experiments/AGENTS.md §2` — sync to `INDEX.md` + `backlog.md` via file edits only.
- Hardware baseline: see `hardware-profile.md §1` (Zen 3 5800X dev host `obvium`, governor=`powersave`,
  no AVX-512 per §1) + §3 (RTX 3060 Ti GA104 Ampere, 8 GiB VRAM, 5.06 GiB budget) + §4
  (Vulkan 1.4.341). Captured `2026-06-20`, <14 days, **no probe needed**.
- Per `TODO.md §5.1` current mainline: 6 diffuse cones + 1 specular cone + R8 atlas + **mip chain
  generation method TBD** (this experiment validates the algorithm choice).
- Crassin 2011 cone-tapered mip filter, 4D temporal VCT, anisotropy-aware filtering, and atlas
  resolution axis (64³/128³/256³) = out-of-scope follow-up experiments, NOT in this prototype.
- This experiment = 4th in Stage 5.1 axis (after vct-vs-rt-cutoff + vct-cone-count + vct-cone-count-atlas-precision).
  Stage 5.1 axis has 1 more natural axis (cone-tapered mip filter) before VCT is "fully validated".
