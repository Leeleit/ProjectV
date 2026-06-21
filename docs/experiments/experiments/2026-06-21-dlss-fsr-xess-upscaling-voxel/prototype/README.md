# `prototype/upscaling_bench.cpp` — standalone C++26 CPU upscaling benchmark

**Status:** Phase B (analytical cost model + measurement harness) — built and run 2026-06-21.
**Build:** `clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic upscaling_bench.cpp -o build/upscaling_bench` (compiled clean, 0 warnings).
**Run:** `./build/upscaling_bench --output build/results.csv` (wrote 288 measurement rows in <1 sec on Zen 3 5800X).

---

## What it measures

Synthetic voxel scene post-process pipeline representative of ProjectV Stage 5.x (voxel MRT → TAA resolve → upscaling post-process → swapchain). Per-frame cost model:

- **Voxel pass cost:** `render_pixels × mean_voxel_touches × 25 ALU / 14.7 TFLOPS` (representative of voxel MRT + GI + AO)
- **TAA resolve cost:** `render_pixels × 30 ALU / 14.7 TFLOPS` (representative of YCoCg + history blend + CAS sharpening)
- **Upscale dispatch cost:** `(output_pixels × (upscaler.alu_per_pixel + tensor_core_ops × 4)) / 14.7 TFLOPS + output_pixels × lookups × 4 / 448 GB/s`
- **GPU cost ratio:** `total_frame_us / native_total_us` (where `native_total_us` = render at 100% extent with No upscaler)

**4 upscaler configurations (sourced from public benchmarks):**

| Upscaler       | ALU/pixel | Lookups | Tensor ops | VRAM Δ (MiB) | PSNR preservation | Dispatch count |
|:---------------|:----------|:--------|:-----------|:-------------|:------------------|:---------------|
| `None`         | 5         | 0       | 0          | 0            | 1.000 (∞ dB)      | 1              |
| `FSR 3.1`      | 50        | 4       | 0          | +1           | 0.965 (38-40 dB)  | 1              |
| `XeSS 2 DP4a`  | 200       | 2       | 2          | +18          | 0.955 (37-39 dB)  | 1              |
| `DLSS 4.5 Sim` | 500       | 3       | 50         | +32          | 0.985 (40-42 dB)  | 2              |

Sources per upscaler:
- `None`: identity copy, ~5 ALU baseline.
- `FSR 3.1`: Lanczos-like temporal upsampler, per `wccftech 2024-07-09` AMD FidelityFX SDK v1.1 + `StraySpark 2026-03-25` UE 5.7 benchmarks.
- `XeSS 2 DP4a`: simplified neural net via DP4a fallback, per Intel XeSS SDK 2.0 + `RigPulse 2026-03-29` + `gamerhardware 2026-03-29` benchmarks.
- `DLSS 4.5 Sim`: 2nd-gen transformer model, per `NVIDIA devblog 2026-01-14` (5× more compute vs 1st-gen) + `wccftech 2026-04-21` SDK 310.6.0 notes. Dispatch count = 2 (compute shader generate derivative + upscale), per `StraySpark 2026-03-25` DirectSR section.

**4 quality presets:** native (100%) / quality (67%) / balanced (58%) / performance (50%) per StraySpark 2026-03-25 + RigPulse 2026-03-29.

**3 extents:** 1080p (1920×1080) / 1440p (2560×1440) / 4K (3840×2160).

**2 scenes:** `dense_voxel` (mean voxel touches = 6.0, VCT-like fragment cost) / `sparse_voxel` (mean voxel touches = 1.5, geometry-bound).

**3 seeds × 1000 iter + 10 warmup per `benchmarks/methodology.md §3`.**

---

## Caveats (per `README.md §9`)

- **CPU prototype, no real GPU dispatch** — costs are analytical based on per-pixel ALU + memory bandwidth model with RTX 3060 Ti reference (14.7 TFLOPS / 448 GB/s from `hardware-profile.md §3`). Real GPU timings depend on dispatch latency, driver overhead, kernel launch cost.
- **Upscaler implementations = cost models, not real SDKs** — FSR 3.1 / XeSS 2 / DLSS 4.5 per-pixel costs sourced from public benchmarks; real SDK load via `dlopen` deferred to mainline integration prototype.
- **No PSNR/SSIM real measurement** — quality model is analytical from `psnr_preservation` parameter (calibrated to StraySpark 2026-03-25 + RigPulse 2026-03-29 industry data). Real PSNR/SSIM on rendered frames deferred to integration prototype + visual QA.
- **Deterministic timing measurements** — all seeds produce identical costs (synthetic scene affects colors, not timing). Seeds = 3 = redundant for cost; kept for future random-variation extension.
- **Cross-vendor projection = analytical only** — single GPU vendor measured (NVIDIA RTX 3060 Ti dev host); AMD RDNA 2/3/4 + Intel Arc Battlemage projected per published vendor benchmarks.

---

## Files

- `upscaling_bench.cpp` (~470 LoC) — single-file C++26 harness (per `vrs_voxel_sim.cpp` / `depth_quant_bench.cpp` / `sub_chunk_bench.cpp` precedent).
- `CMakeLists.txt` — optional (not required; `clang++` direct build works).
- `build/upscaling_bench` — compiled binary.
- `build/results.csv` — 288 measurements (4 upscalers × 4 presets × 3 extents × 2 scenes × 3 seeds = 288 + 1 header).
- `RESULTS.md` — headline findings + per-config breakdown.
