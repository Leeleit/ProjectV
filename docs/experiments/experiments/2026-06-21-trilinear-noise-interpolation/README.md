# 2026-06-21-trilinear-noise-interpolation — Trilinear interpolation from coarse noise grid for terrain generation

**Status:** `concluded-verdict-mixed`
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** Stage 4.1 (world generation), independent
**Estimated effort:** S
**Author:** self (derived from Minecraft 1.12 source analysis)

---

## 1. Hypothesis

Minecraft 1.12's `ChunkGeneratorOverworld.java:95-162` generates terrain from a coarse 5×33×5 noise grid, then trilinearly interpolates to 16×256×16. This reduces noise evaluations by 79×.

**Hypothesis:** For ProjectV's chunkSize=8 world gen, generating noise at 2×2×2 coarse resolution (8 evaluations per chunk) and trilinearly interpolating to 8×8×8 (512 voxels) reduces noise compute by 64× while maintaining visual quality within 1 dB PSNR vs per-voxel evaluation.

**Alternatives:** per-voxel noise (current assumed), octree-based coarse-to-fine, wavelet compression of noise field.

---

## 2. Prior art

- **Minecraft 1.12 `ChunkGeneratorOverworld.java:95-162`** — 5×33×5 coarse grid + trilinear interpolation to 16×256×16 (79× reduction).
- **Notch blog 2011** "Terrain Generation Part 1" — describes coarse-grid noise sampling for terrain.
- **KdotJPG (Hopson97/open-builder issue #67, 2020)** — critique of trilinear interpolation: "Trilerp can tarnish the results of even the best noise generator or terrain formula, because it breaks the continuity of slopes and introduces a visible grid pattern."
- **Reddit r/VoxelGameDev (2025)** — analysis of Minecraft Beta terrain: broken Perlin noise + trilerp created distinctive artifacts that defined the "Minecraft look."
- **Cinevva 2026** — modern approach: replace procedural noise with per-chunk heightmap buffers, sampled via bilinear interpolation on GPU.
- **closed `gpu-procedural-noise-compute-kernels`** — OpenSimplex2 3D-S chosen for quality+license; noise algorithm not a perf discriminator at chunkSize=8 (memory-bound).
- **closed `sub-chunk-layers`** — per-Y-layer chunk structure; coarse grid aligns with layer boundaries.
- **OpenSimplex2 (KdotJPG 2019)** — docs recommend `noise3_ImproveXZ` for 3D terrain with Y vertical.

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU harness)
- **Scene:** 5 terrain profiles (flat_plains, rolling_hills, mountains, cave_system, island)
- **Metrics:** per-chunk density evaluation time (µs), PSNR vs per-voxel reference (dB), binary voxel match rate (solid vs air)
- **Baseline:** per-voxel noise evaluation (hash-based gradient noise, 4-octave FBM)
- **Strategies:**
  - A_PerVoxel: full noise at every voxel (baseline, 512 evaluations)
  - B_Trilerp_2: noise at 2×2×2 grid (8 evaluations) + trilinear interpolate
  - C_Trilerp_3: noise at 3×3×3 grid (27 evaluations) + trilinear interpolate
  - D_Trilerp_4: noise at 4×4×4 grid (64 evaluations) + trilinear interpolate
  - E_Spline_2: noise at 2×2×2 grid (64 evaluations with borders) + Catmull-Rom cubic interpolate

- **Hardware baseline:** Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. GCC 16.1.1 `-O3 -march=native -std=c++26`.

---

## 4. Prototype

Standalone C++26 CPU harness: `prototype/trilinear_noise_bench.cpp` ~390 LoC.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/trilinear_noise_bench
```

Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 5 strategies × 5 scenes × 5 seeds).

---

## 5. Results

| Strategy       | Evals | Reduction | Speedup  | PSNR mean | PSNR min | Match mean | Match min |
|:---------------|:------|:----------|:---------|:----------|:---------|:-----------|:----------|
| A_PerVoxel     | 512   | 1×        | 1.0×     | 100.00 dB | ∞        | 1.00000    | 1.00000   |
| **B_Trilerp_2**| 8     | **64×**   | **51.6×**| **4.97 dB**| -4.01 dB | 0.56477    | 0.12109   |
| **C_Trilerp_3**| 27    | **19×**   | **12.6×**| **30.22 dB**| 19.66 dB | 0.99234    | 0.93555   |
| **D_Trilerp_4**| 64    | **8×**    | **6.7×** | **36.23 dB**| 23.54 dB | 0.99734    | 0.97461   |
| E_Spline_2     | 64    | 8×        | 7.1×     | -20.76 dB | -43.03 dB| 0.68398    | 0.47266   |

**Key observations:**

- **B_Trilerp_2 (64× reduction) FAILS** — PSNR mean 5 dB (hypothesis was <1 dB). On complex terrain (mountains), PSNR goes negative and match rate drops to 12.5%. The coarse 2×2×2 grid loses all high-frequency detail.

- **C_Trilerp_3 (19× reduction) RECOMMENDED** — PSNR 30 dB, match rate >99%, 12.6× speedup. Best quality-speed tradeoff. Validates KdotJPG's critique (grid artifacts visible on cave_system at 20 dB) but at 30+ dB the artifacts are masked by natural variation.

- **D_Trilerp_4 (8× reduction) QUALITY MODE** — PSNR 36 dB, match rate >99.7%, 6.7× speedup. Near-perfect quality for critical scenes.

- **E_Spline_2 (Catmull-Rom) REJECTED** — Cubic interpolation with only 2×2×2 coarse grid produces severe overshoot artifacts. Undersampled for Catmull-Rom.

---

## 6. Verdict

`mixed` — Hypothesis is **partially confirmed**: coarse-grid noise interpolation is a valid optimization (12-51× speedup), but the <1 dB PSNR target for 64× reduction (B_Trilerp_2) is **not achievable**. Acceptable quality requires 3×3×3 (19× reduction, PSNR 30 dB) or 4×4×4 (8× reduction, PSNR 36 dB).

---

## 7. Integration recommendation

- **Target stage:** Stage 4.1 (GPU noise compute kernels)
- **Конкретные изменения:** `src/shaders/noise_kernels.comp` — add coarse-grid generation + trilinear interpolation pass.
- **Подход:** Use 3×3×3 coarse grid (27 noise evaluations per 8³ chunk) with trilinear interpolation as default. This gives 12× speedup with 30 dB PSNR (>99% binary match rate). Optionally expose 4×4×4 quality mode for critical scenes.
  - Stage 1: coarse grid noise dispatch (27 evals instead of 512)
  - Stage 2: trilinear interpolation in shared memory or second pass
  - Stage 3: `PROJECTV_NOISE_COARSE_GRID=NONE|3X3X3|4X4X4` env gate
- **Риски:** visible grid artifacts on high-frequency cave systems (PSNR 20 dB on cave_system). Scene-adaptive quality may be needed. KdotJPG's spline recommendation (avoid trilerp) is noted but 3×3×3 trilerp is acceptable for Stage 4.1 MVP.
- **Критерии приёмки:** >10× noise eval reduction; >99% binary match rate on outdoor scenes; >95% on cave systems.
- **Зависимости:** closed `gpu-procedural-noise-compute-kernels` (OpenSimplex2 choice). GPU memory bandwidth for coarse grid storage.
- **Estimated effort:** S (~150 LoC, 1 session)
- **Re-evaluation triggers:** if GPU world gen becomes ALU-bound (not memory-bound as per closed experiment), the 12× reduction becomes critical. If quality QA reveals visible grid lines, switch to 4×4×4 quality mode or adopt per-voxel evaluation with early-out optimization (KdotJPG's alternative approach).

---

## 8. Sources

1. Minecraft 1.12 `ChunkGeneratorOverworld.java:95-162, 260-291` (local source)
2. Notch blog 2011 "Terrain Generation Part 1" — `notch.tumblr.com/post/3746989361/terrain-generation-part-1`
3. KdotJPG 2020 "Suggestion: Don't use trilinear interpolation for terrain generation" — `github.com/Hopson97/open-builder/issues/67`
4. Reddit r/VoxelGameDev 2025 "The Secret to Minecraft Beta's Famous Terrain: Broken Perlin Noise?" — `reddit.com/r/VoxelGameDev/comments/1fbryci/`
5. Cinevva 2026 "Building an open world in the browser, part 13" — `app.cinevva.com/blog/2026-04-13-open-world-browser-part-13-terrain-sculpting.html`
6. Cinevva 2026 "Landscape Generation with Dynamic LOD and Streaming" — `app.cinevva.com/guides/landscape-generation-browser.html`
7. KdotJPG 2019 OpenSimplex2 — `github.com/KdotJPG/OpenSimplex2` (CC0)
8. closed `2026-06-21-gpu-procedural-noise-compute-kernels` (noise algorithm choice)
9. closed `2026-06-21-sub-chunk-layers` (chunk layout)
10. Nornagon 2021 "Terrain generation in Minecraft" — `blog.nornagon.net/terrain-generation-in-minecraft/`
11. GameDev SE 2016 "How to smooth low-res sampling of noise for voxel terrain" — `gamedev.stackexchange.com/questions/124543/`
12. InfiniteDiffusion SIGGRAPH 2026 — `xandergos.github.io/terrain-diffusion/`

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/shaders/noise_kernels.comp` — GPU noise dispatch (per `agent/workspace.md §1 Phase 1`).
- **Assumptions:** chunkSize=8; OpenSimplex2 3D-S; 4-octave FBM; GPU compute dispatch (not CPU).
- **Unmeasured:** GPU memory bandwidth for coarse grid storage; shared memory usage for interpolation; multi-chunk dispatch amortization; cross-chunk boundary continuity.
- **Open questions:** Does GPU compute at chunkSize=8 benefit from the 12× noise eval reduction? The closed `gpu-procedural-noise-compute-kernels` found noise is memory-bound at chunkSize=8 (65.6% of 448 GB/s). If ALU is not the bottleneck, the speedup may not translate to GPU as directly as CPU.
