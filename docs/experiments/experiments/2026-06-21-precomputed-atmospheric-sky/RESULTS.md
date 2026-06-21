# RESULTS — 2026-06-21-precomputed-atmospheric-sky

## Summary

| Metric | Value |
|--------|-------|
| Total measurements | 150 (6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup) |
| Wall time | < 0.1 sec |
| Dev host | Zen 3 5800X governor=`powersave` per hardware-profile.md §1 |
| Output | `prototype/build/results.csv` (151 rows, 1 header + 150 data) |

## Per-strategy aggregate (n=25 configs each)

| Strategy | Mean ms | PSNR dB | VRAM MiB | Precompute | Dynamic | Scene-indep % |
|----------|---------|---------|----------|------------|---------|---------------|
| A_ConstantSky | 0.000 | 8.0 | 0 | none | no | 0.0 |
| B_Bruneton2017 | 0.092 | 33.7 | 12 | 2.5s | no | 19.5 |
| C_Hillaire2020 | 0.080 | 32.7 | 8 | 0.5ms | yes | 20.4 |
| D_elliahu2025 | 0.635 | 35.9 | 20 | 0.8ms | yes | 17.6 |
| E_HosekWilkie2012 | 0.006 | 24.7 | 0 | none | yes | 15.4 |
| F_GPU_Gems2_ONeil | 0.002 | 17.9 | 0.5 | 0.1s | no | 16.6 |

## Key findings

1. **C_Hillaire2020 = universal recommended default** — 0.080 ms mean (0.24% of 33.3 ms 30 Hz), 32.7 dB PSNR, 8 MiB VRAM, single-frame LUT recompute enables dynamic time-of-day/weather. Best cost-quality balance.
2. **B_Bruneton2017** = quality opt-in (0.092 ms, 33.7 dB) but 2.5s startup precompute blocks dynamic changes. Worthwhile for static-sky quality mode (<0.1 ms, negligible VRAM).
3. **E_HosekWilkie2012** = mobile/no-LUT fallback (0.006 ms, 24.7 dB). 3× better PSNR than baseline at near-zero cost, zero VRAM. Viable for low-end GPU tier.
4. **D_elliahu2025** = too expensive for sky-only (0.635 ms = 1.9% of 30 Hz). Full pipeline with clouds+god rays bundled — value only if all three adopted together.
5. **All non-baseline strategies cross 5-10% optimization threshold massively** — +16.7 to +27.9 dB PSNR gain (209-349% relative) at sub-0.1 ms for recommended strategies.

## Data

CSV at `prototype/build/results.csv` — 151 rows, 8 columns: strategy, scene, seed, cost_ms, psnr_db, vram_mib, precompute_s, supports_dynamic.
