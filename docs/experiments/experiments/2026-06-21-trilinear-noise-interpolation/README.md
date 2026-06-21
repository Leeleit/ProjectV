# 2026-06-21-trilinear-noise-interpolation — Trilinear interpolation from coarse noise grid for terrain generation

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 4.1 (world generation), independent
**Estimated effort:** S
**Author:** self (derived from Minecraft 1.12 source analysis)

---

## 1. Hypothesis

Minecraft 1.12's `ChunkGeneratorOverworld.java:95-162` generates terrain from a coarse 5×33×5 noise grid, then trilinearly interpolates to 16×256×16. This avoids evaluating 7 noise octaves at every voxel (16×256×16 = 65,536 evaluations) — instead evaluating only 5×33×5 = 825 times and interpolating. This is a 79× reduction in noise evaluations.

**Hypothesis:** For ProjectV's chunkSize=8 world gen, generating noise at 2×2×2 coarse resolution (8 evaluations per chunk) and trilinearly interpolating to 8×8×8 (512 voxels) reduces noise compute by 64× while maintaining visual quality within 1 dB PSNR vs per-voxel evaluation. Combined with the OpenSimplex2 noise choice from closed `gpu-procedural-noise-compute-kernels`, this optimizes the GPU compute path.

**Alternatives:** per-voxel noise (current assumed), octree-based coarse-to-fine, wavelet compression of noise field.

---

## 2. Prior art

- **Minecraft 1.12 `ChunkGeneratorOverworld.java:95-162`** — 5×33×5 coarse grid + trilinear interpolation to 16×256×16.
- **Minecraft 1.12 `ChunkGeneratorOverworld.java:260-291`** — 5×5 biome weight blending with distance-based falloff.
- **closed `2026-06-21-gpu-procedural-noise-compute-kernels`** — OpenSimplex2 3D-S chosen for quality+license; noise algorithm not a perf discriminator at chunkSize=8 (memory-bound).
- **closed `2026-06-21-sub-chunk-layers`** — per-Y-layer chunk structure; coarse grid aligns with layer boundaries.

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 terrain profiles (flat_plains, rolling_hills, mountains, cave_system, island)
- **Metrics:** per-chunk gen time (µs), PSNR vs per-voxel reference, visual artifact count (flat faces, grid lines)
- **Baseline:** per-voxel noise evaluation (OpenSimplex2 3D-S, 4 octaves FBM)
- **Strategies:**
  - A_PerVoxel: full noise at every voxel (baseline, 512 evaluations)
  - B_Coarse2x2x2: noise at 2×2×2 grid (8 evaluations) + trilinear interpolate
  - C_Coarse3x3x3: noise at 3×3×3 grid (27 evaluations) + trilinear interpolate
  - D_Coarse4x4x4: noise at 4×4×4 grid (64 evaluations) + trilinear interpolate

---

## 4. Prototype

Standalone C++26 CPU harness measuring interpolation quality vs cost.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/trilinear_noise_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 4.1 (GPU noise compute kernels)
- **Конкретные изменения:** `src/shaders/noise_kernels.comp` — add coarse-grid generation + interpolation pass.
- **Подход:** generate noise at coarse grid points in first dispatch; interpolate in second pass (or single-pass with shared memory for 2×2×2 neighborhood).
- **Риски:** visible grid artifacts on terrain surfaces; interpolation smoothing losing detail on steep terrain.
- **Критерии приёмки:** >32× noise evaluation reduction; <1 dB PSNR vs per-voxel reference; no visible grid lines on terrain.
- **Зависимости:** closed `gpu-procedural-noise-compute-kernels` (OpenSimplex2 choice).
- **Estimated effort:** S (~150 LoC, 1 session)

---

## 8. Sources

- Minecraft 1.12 `ChunkGeneratorOverworld.java:95-162, 260-291` (local source)
- closed `2026-06-21-gpu-procedural-noise-compute-kernels` (noise algorithm choice)
- closed `2026-06-21-sub-chunk-layers` (chunk layout)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/shaders/noise_kernels.comp` — GPU noise dispatch (per `agent/workspace.md §1 Phase 1`).
- **Assumptions:** chunkSize=8; OpenSimplex2 3D-S; 4-octave FBM; GPU compute dispatch (not CPU).
- **Unmeasured:** GPU memory bandwidth for coarse grid storage; shared memory usage for interpolation.
