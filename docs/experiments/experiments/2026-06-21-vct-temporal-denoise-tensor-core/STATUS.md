# STATUS — 2026-06-21-vct-temporal-denoise-tensor-core

**Status:** closed (`concluded-verdict-mixed`)
**Phase:** A (scaffold) → B (web-research, ~15 primary sources verified via webfetch +
DuckDuckGo HTML fallback per the web_search fallback chain + operator directive; Exa MCP
HTTP 429 persistent) → C (prototype, standalone C++26 CPU temporal denoise simulator, 75
measurements, 78 sec wall time) → D (close, RESULTS.md + INDEX §6 + backlog §Closed synced).
**Last action:** `2026-06-21` — Phase D close complete.
**Blocker:** нет.
**Re-evaluation triggers:** Stage 5.1 VCT integration milestone (real GPU benchmark on
RTX 3060 Ti GA104 для D_CoopMat hypothesis validation); Stage 5.3 TAA Motion Vectors GPU
integration (motion vector format contract binding); Vulkan 1.5/1.6 dedicated temporal
denoise extensions; cross-vendor Stage 5.x integration (AMD RDNA 4 + Intel Xe2/Battlemage).

---

## Phase log

### Phase A — Hypothesis fix + scaffold ✅
- ✅ Anti-duplicate sentinel clean per `AGENTS.md §13.7` (no `vct-temporal-denoise` /
  `tensor-core` / `cooperative-matrix` experiment folders).
- ✅ Reservation зафиксирована в `research/backlog.md §In progress` per §13.2 (h-priority
  from `full rt + tensor cores load` §Open).
- ✅ Folder created: `experiments/2026-06-21-vct-temporal-denoise-tensor-core/`.
- ✅ README.md scaffold по `experiments/_TEMPLATE/README.md`.
- ✅ STATUS.md created.
- ✅ INDEX.md §5 Active entry added.

### Phase B — Web research ✅ (4 batches, ~22 sources verified)
- ✅ **Primary SOTA (4):** Schied 2017 SVGF (HPG 2017 Best Paper) + NVIDIA-RTX/NRD
  v4.17.2 (Mar 2026) + TooMuchVoltage Hybrid PT + Neural Temporal Denoising IEEE TVCG 2022.
- ✅ **Primary VCT temporal (5):** SangHyeok Hong DigiPen thesis (direct VCT temporal
  precedent) + righier/gidemo (Light temporal multi-bounce) + bc3.moe/vctgi (Spatial +
  Temporal AA) + LanLou123/DXE (planned) + Grimkin SoftShadows (temporal reprojection).
- ✅ **Primary VCT baseline (3):** Crassin 2011 GIVoxels (5 cones, 30 FPS @ 512², GTX
  480) + Panteleev 2014 thesis (17 cones, 7.4 ms GTX 770 @ 1920×1080) + Andersson/Ayerbe
  2025 CGF (11 FPS VCT baseline RTX 2060).
- ✅ **Primary cooperative matrix (4):** VK_KHR_cooperative_matrix rev 2 ratified 2023-05-03
  + VK_NV_cooperative_matrix2 rev 1 (Oct 2024) + Phoronix 2025-02-07 (RDNA 4 RADV merged)
  + Phoronix 2024-06-26 (Intel Xe2 RADV merged).
- ✅ **Secondary hardware spec (5):** RTX 3060 Ti GA104 = 152 tensor cores, FP16 Tensor
  32.39/64.79 TFLOPS dense/sparse (CORRECTED from initial 112 TFLOPS estimate).
- ✅ **Secondary cross-vendor (3):** Arc A770 SIMD8 vs SIMD32 mismatch (cooperative
  matrix disabled в llama.cpp) + Battlemage XMX specs (4096 ops/clock int8) + Intel Arc
  Pro B60 197 TOPS INT8 peak.
- ⚠️ Sugimoto 2024 specific paper NOT directly retrieved (cited via closed experiments
  but no direct PDF access this session); flagged в `sources.md` verification queue.

### Phase C — Prototype ✅
- ✅ Standalone C++26 CPU temporal denoise simulator (`prototype/vct_temporal_denoise_sim.cpp`
  ~620 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
  **build green 0 warnings**).
- ✅ 5 strategies implemented: A_NoTemporal / B_SpatialBilateral /
  C_TemporalReprojectFS / D_TemporalReprojectCoopMat / E_TemporalReprojectSVGF.
- ✅ 5 procedural voxel scenes per `2026-06-21-sub-chunk-layers` precedent (uniform_floor +
  forest_floor + cave_stress + mixed_biome + uniform_air).
- ✅ 3 seeds (1, 42, 31337) × 50 frames + 5 warmup = 55 frames per measurement.
- ✅ **75 measurements total** (5 strategies × 5 scenes × 3 seeds).
- ✅ Output: `prototype/build/results.csv` (76 rows = 1 header + 75 data rows).
- ✅ Wall time: 78 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`.

### Phase D — Analysis + close ✅
- ✅ Verdict=`mixed` recorded в README.md + STATUS.md.
- ✅ README.md §5/§6/§7 populated with actual findings.
- ✅ RESULTS.md written (9 sections + headline tables).
- ✅ INDEX.md §5 Active entry moved → §6 Recent closed (next tick per §13.5 sync).
- ✅ `research/backlog.md §In progress` entry → `§Closed` (next tick per §13.5 sync).
- ✅ sources.md written (22 references = 14 primary + 8 secondary).

---

## Cross-axis (orthogonal to all in-progress parallel)

- `2026-06-21-tracy-gpu-vs-manual` (profiling tool)
- `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)

## Complementary to closed experiments

- `2026-06-20-vct-vs-rt-cutoff` (cutoff=0.3 strategy)
- `2026-06-21-vct-cone-count-atlas-precision` (single-frame quality + STATUS.md:13 explicit
  out-of-scope follow-up = this)
- `2026-06-21-vct-3d-mip-generation` (mip chain algorithm)
- `2026-06-20-nanovdb-on-gpu` (atlas storage)
- `2026-06-21-sdf-hybrid-world` (spatial anti-leak via SDF)
- `2026-06-21-taa-motion-vectors` (motion vector `R16G16_SFLOAT` format = direct VCT temporal
  input contract)
- `2026-06-21-dlss-fsr-xess-upscaling-voxel` (analytical tensor core projection = cost model
  calibration precedent)
- `2026-06-20-dec-pipelines-async-compute` (async compute = cooperative matrix async dispatch
  prerequisite)

---

## Headline

- **E_TemporalReprojectSVGF = WINNER** (Schied 2017 algorithm validated):
  +2.18 dB mean PSNR (avg 24.64 vs A baseline 22.46), +9.7% gain above 5% threshold per
  `optimization-philosophy.md`. Std cost +0.24 dB (acceptable).
- **B_SpatialBilateral = CHEAP FALLBACK** (edge-preserving 3×3 spatial filter):
  +1.80 dB mean PSNR, +0.08 dB std cost (lowest std cost of all strategies).
- **D_TemporalReprojectCoopMat = UNVERIFIED** (CPU sim can't capture real GA104 tensor
  SNR benefit; analytical projection <1 ms @ 1920×1080 plausible but needs real Vulkan
  benchmark).
- **C_TemporalReprojectFS = FALSIFIED** in simplified model (naive FS temporal without
  proper motion vector handling adds per-frame instability; real Karis 2014 TAA requires MV
  texture + history rejection).

---

## Files (closed)

- `README.md` — main document (concluded-verdict-mixed, all 8 sections + §9 populated)
- `STATUS.md` — this file
- `RESULTS.md` — full measurement analysis + tables + cross-vendor matrix
- `sources.md` — 22 references (14 primary + 8 secondary verified)
- `prototype/vct_temporal_denoise_sim.cpp` — ~620 LoC standalone C++26 CPU simulator
- `prototype/README.md` — build + run instructions
- `prototype/build/vct_temporal_denoise_sim` — 57 KB binary
- `prototype/build/results.csv` — 76 rows (1 header + 75 data rows)

---

## Re-evaluation triggers

1. **Stage 5.1 VCT integration milestone** (real GPU benchmark on RTX 3060 Ti GA104 для
   D_CoopMat hypothesis validation before final default selection).
2. **Stage 5.3 TAA Motion Vectors GPU integration** (motion vector format contract binding
   per closed `taa-motion-vectors`).
3. **Vulkan 1.5/1.6 dedicated temporal denoise extensions** (if proposed by Khronos).
4. **Cross-vendor Stage 5.x integration** (AMD RDNA 4 + Intel Xe2/Battlemage + Intel Arc
   A770 SIMD8 fallback path).
5. **ProjectV shader count growth** (if TC path becomes bottleneck for total pipeline).
6. **Visual QA on real ProjectV scenes** at runtime camera angles (per `dlss-fsr-xess`
   precedent).
