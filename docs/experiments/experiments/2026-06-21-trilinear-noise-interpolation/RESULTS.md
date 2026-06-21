# RESULTS — 2026-06-21-trilinear-noise-interpolation

## Aggregate summary

| Strategy         | Evals | Reduction | Speedup | PSNR mean | PSNR min | PSNR max | Match mean | Match min | Match max |
|:-----------------|:------|:----------|:--------|:----------|:---------|:---------|:-----------|:----------|:----------|
| A_PerVoxel (ref) | 512   | 1×        | 1.0×    | 100.00 dB | ∞        | ∞        | 1.00000    | 1.00000   | 1.00000   |
| **B_Trilerp_2**  | 8     | **64×**   | 51.6×   | **4.97 dB**| -4.01 dB | 17.05 dB | 0.56477    | 0.12109   | 1.00000   |
| **C_Trilerp_3**  | 27    | **19×**   | 12.6×   | **30.22 dB**| 19.66 dB | 38.56 dB | 0.99234    | 0.93555   | 1.00000   |
| **D_Trilerp_4**  | 64    | **8×**    | 6.7×    | **36.23 dB**| 23.54 dB | 45.21 dB | 0.99734    | 0.97461   | 1.00000   |
| E_Spline_2       | 64    | 8×        | 7.1×    | -20.76 dB | -43.03 dB| -3.95 dB  | 0.68398    | 0.47266   | 0.94727   |

## Per-scene detail (PSNR mean across 5 seeds)

| Scene          | B_Trilerp_2 | C_Trilerp_3 | D_Trilerp_4 | E_Spline_2 |
|:---------------|:------------|:------------|:------------|:-----------|
| flat_plains    | 14.82 dB    | 36.33 dB    | 43.79 dB    | -10.24 dB  |
| rolling_hills  | 4.51 dB     | 32.37 dB    | 37.75 dB    | -19.13 dB  |
| mountains      | -3.47 dB    | 27.17 dB    | 33.67 dB    | -22.15 dB  |
| cave_system    | 5.98 dB     | 20.86 dB    | 24.23 dB    | -37.26 dB  |
| island         | 2.96 dB     | 34.35 dB    | 41.71 dB    | -15.03 dB  |

## Key findings

1. **B_Trilerp_2 (2×2×2 → 64× reduction) REJECTED**: PSNR mean 4.97 dB, match rate 56%. Hypothesis of <1 dB PSNR loss fails dramatically. On complex scenes (mountains), PSNR is negative — trilerp from only 8 coarse points loses too much high-frequency detail.

2. **C_Trilerp_3 (3×3×3 → 19× reduction) RECOMMENDED**: PSNR 30.22 dB mean, >99% match rate, 12.6× speedup. Best quality-speed tradeoff. On flat_plains + hills, PSNR exceeds 32 dB. On cave_system, drops to 20 dB (high-frequency detail lost) but match rate remains >99%.

3. **D_Trilerp_4 (4×4×4 → 8× reduction) QUALITY MODE**: PSNR 36.23 dB mean, >99.7% match rate, 6.7× speedup. Best quality. Use when noise quality is critical.

4. **E_Spline_2 (Catmull-Rom with 2×2×2) REJECTED**: Cubic spline interpolation with only 8 coarse samples produces severe overshoot artifacts (negative PSNR). Catmull-Rom needs ≥4×4×4 sampling for stability. Spline interpolation not recommended for coarse-grid terrain gen at chunkSize=8.

5. **Web research validation**: KdotJPG's critique (GitHub Hopson97/open-builder issue #67) of trilinear interpolation artifacts is confirmed — coarse trilerp introduces visible grid artifacts on complex terrain. However, at 3×3×3 resolution the artifacts are largely masked by per-chunk natural variation.

## Caveats

- CPU-only benchmark on Zen 3 5800X (governor=powersave per `hardware-profile.md §1`)
- Noise = hash-based gradient noise with 4-octave FBM (representative of GPU noise workload)
- PSNR computed on density field (continuous), not on final binary voxel grid
- Binary match rate captures the actual visual outcome (solid vs air)
- Single chunk (8³), not multi-chunk with cross-boundary interpolation
- GPU dispatch cost (kernel launch, memory bandwidth) not modeled
- Catmull-Rom spline at 2×2×2 is under-sampled; proper spline would need 3×3×3+ neighborhood

## Raw data

See `prototype/build/results.csv` (125 rows + 1 header).
