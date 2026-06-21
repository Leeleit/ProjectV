# RESULTS — 2026-06-21-tonemap-color-grading

## Setup

- **Harness:** Standalone C++26 CPU `prototype/tonemap_bench.cpp` ~240 LoC
- **Compiler:** GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG` (build green 0 errors)
- **Host:** Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`
- **Configs:** 9 strategies × 5 scenes × 5 seeds × 1000 iter = **225 configs × 1000 = 225,000 main measurements**
- **Output:** `prototype/build/results.csv` (226 rows = 1 header + 225 data)

## Scenes

| Scene | Description | HDR range |
|:------|:------------|:----------|
| uniform_floor | Brown diffuse, low range | 0-1 |
| forest_floor | Green-heavy vegetation | 0-1.5 |
| cave_stress | Sparse bright torch + dark walls | 0-50 |
| sunset_sky | Warm sun glow + sky | 0-20 |
| emissive_blocks | Saturated emissive RGB blocks | 0-10 |

## Strategies

| ID | Name | Approach |
|:---|:-----|:---------|
| A | LinearNoTonemap | clamp to [0,1] (current baseline) |
| B | ReinhardGlobal | `c/(1+c)` per channel |
| C | ReinhardLuminance | Luminance-adaptive |
| D | ACES Narkowicz | 2015 analytic fit |
| E | ACES 1.3 LUT | 33×33×33 + trilinear (reference) |
| F | UnrealFilmic | UE4/5 filmic curve |
| G | HableColorGrade | Uncharted 2 S-curve |
| H | Uchimura | GT parametric (3-segment) |
| I | HejlDawson | id Tech filmic (16× exposure) |

## PSNR vs ACES 1.3 LUT reference (dB)

| Strategy | uniform | forest | cave | sunset | emissive | mean |
|:---------|:-------:|:------:|:----:|:------:|:--------:|:----:|
| **F_UnrealFilmic** | **15.6** | **15.9** | **30.2** | **17.5** | **12.6** | **18.4** |
| D_ACES_Narkowicz | 9.6 | 10.6 | 21.7 | 10.4 | 9.7 | 12.4 |
| A_LinearNoTonemap | 7.4 | 8.7 | 22.4 | 8.8 | 9.6 | 11.4 |
| H_Uchimura | 7.4 | 8.6 | 21.4 | 8.7 | 9.4 | 11.1 |
| G_HableColorGrade | 5.4 | 5.7 | 21.9 | 7.4 | 9.4 | 10.0 |
| I_HejlDawson | 10.9 | 13.5 | 6.7 | 9.4 | 6.2 | 9.3 |
| C_ReinhardLuminance | 5.8 | 5.7 | 19.0 | 7.0 | 4.2 | 8.3 |
| B_ReinhardGlobal | 5.5 | 5.9 | 18.2 | 6.8 | -1.0 | 7.1 |

## Per-pixel cost (CPU, ns)

| Strategy | mean_ns | ns/px | vs baseline |
|:---------|:-------:|:-----:|:-----------:|
| B_ReinhardGlobal | 1539 | 3.0 | 0.49× |
| C_ReinhardLuminance | 1681 | 3.3 | 0.53× |
| F_UnrealFilmic | 1838 | 3.6 | 0.58× |
| D_ACES_Narkowicz | 1932 | 3.8 | 0.61× |
| I_HejlDawson | 2037 | 4.0 | 0.64× |
| G_HableColorGrade | 2852 | 5.6 | 0.90× |
| A_LinearNoTonemap | 3160 | 6.2 | 1.00× |
| E_ACES_1p3_LUT32 | 6465 | 12.6 | 2.05× |
| H_Uchimura | 19218 | 37.5 | 6.08× |

## Findings

### 1. UnrealFilmic is the universal quality winner

F_UnrealFilmic achieves the highest PSNR vs ACES 1.3 reference across ALL 5 scenes — including +30.2 dB on cave_stress HDR (6-8 dB clear of next competitor). It costs only 3.6 ns/px on CPU ≈ negligible on GPU (< 0.1 ms at 1080p). **Recommended default for ProjectV Stage 5.x.**

### 2. ACES Narkowicz is a solid secondary option

12.4 dB mean, consistent across all scenes (9.6-21.7 range). Performance is similar to UnrealFilmic (3.8 ns/px). Main weakness: oversaturates brights per the original blog post caveat. **Recommended as fallback or ACES-adjacent option.**

### 3. Reinhard variants NOT recommended

Both B (5.5-5.9 dB on non-HDR) and C (5.7-5.8 dB) wash out colors significantly. B catastrophically fails on emissive_blocks (-1.0 dB PSNR = negative means massive deviation from ACES). These fail the 5-10% perceptual threshold. **Skip Reinhard for ProjectV.**

### 4. HableColorGrade underperforms

G achieves 10.0 dB mean — lower than expected given its AAA pedigree (Uncharted 2). Likely due to tuning sensitivity: Hable's 6 parameters need scene-specific tuning that my defaults don't match. With per-scene optimization it would improve. **Not recommended as fixed default, consider with auto-exposure.**

### 5. Uchimura is 6× slower — no quality benefit

H_Uchimura at 37.5 ns/px is 6× slower than UnrealFilmic for LOWER PSNR (11.1 vs 18.4 dB). The 3-segment piecewise spline adds computation (pow + exp + smoothstep per channel) without quality benefit for voxel rendering. The GT7 Color Volume Mapping (which blends per-channel with UCS) is NOT implemented here — the standalone Uchimura curve is insufficient. **Skip for mainline.**

### 6. HejlDawson hardcodes extreme exposure

I_HejlDawson produces very bright output (mean 0.84/0.82/0.81 vs ACES LUT 0.56/0.50/0.52) due to 16× pre-exposure. This makes it fail on HDR scenes (6.7 dB cave_stress) but perform acceptably on mid-range (13.5 dB forest). The hardcoded 16× exposure is id Tech's design choice tied to their light unit scale — not transferable. **Skip unless light unit scale matched.**

### 7. LUT (ACES 1.3) is 2× cost for reference quality

E_ACES_1p3_LUT32 at 12.6 ns/px is 2× slower than UnrealFilmic. On GPU this would be ~0.25 ms vs ~0.1 ms for analytic fits — still far below the 5% threshold (1.67 ms at 30 Hz). **Adopt only if color-critical (cutscenes, tooling) or ACES pipeline required.** The 33³ LUT + trilinear is standard and well-supported on GPU.

### 8. ALL strategies are essentially free on GPU

The most expensive strategy (Uchimura, projected ~0.5-0.75 ms) is still < 2.5% of 30 Hz frame budget. The best (ReinhardGlobal, UnrealFilmic, projected < 0.1 ms) are < 0.3%. **Performance is NOT a differentiator for tonemapping on RTX 3060 Ti class hardware.** Decision should be purely based on visual quality.

## Caveats

1. **CPU-only harness** — real GPU cost differs (texture fetch latency vs ALU throughput; warp divergence for branches).
2. **Synthetic scenes** — 512 pixels per scene × 5 scenes = prototype-level, not real render output.
3. **PSR vs Hill fit LUT** — the reference is the author's ACES Hill approximation, not official AMPAS ACES 1.3 CTL.
4. **No temporal/adaptive exposure** — auto-exposure / eye adaptation significantly affects tonemapper behavior (especially Hable and Reinhard).
5. **Subjective quality ≠ PSNR** — PSNR measures pixel difference, not perceptual quality. Some operators may look better despite lower PSNR.
6. **No real HDR display** — SDR output assumed (Rec.709 sRGB). HLG/PQ HDR output transforms not tested.
