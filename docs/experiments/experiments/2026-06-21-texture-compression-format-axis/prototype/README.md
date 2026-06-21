# Prototype — `2026-06-21-texture-compression-format-axis`

Standalone C++26 CPU texture compression harness. **NOT ProjectV mainline** — это research tool для измерения VRAM cost + encode time + PSNR quality для 10 texture compression форматов против synthetic voxel material atlases.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-texture-compression-format-axis/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    texture_compression_bench.cpp -o build/bench
```

Build verification (per `AGENTS.md §1` research workflow — agent not building, operator can verify):
- Toolchain: Clang 22.1.6 per `agent/knowledge.md §17` + `hardware-profile.md §6`.
- Build green with **0 warnings** (per `2026-06-21-texture-compression-format-axis/STATUS.md`).
- Output binary: `build/bench` (107 KB on dev host `obvium` Zen 3 5800X per `hardware-profile.md §1`).

## Run

```bash
./build/bench 100
```

- First arg: iterations per measurement (default 100; range tested: 50-100).
- Output:
  - `build/results.csv` — 75,001 rows (header + 75,000 measurements) per `benchmarks/methodology.md §3`.
  - stderr summary: mean encode_us + compressed_bytes + psnr per (format, atlas_type).

Wall time: ~12.9 seconds для 100 iter × 75,000 measurements на dev host `obvium`.

## Files

| File | LoC | Purpose |
|:-----|:----|:--------|
| `texture_compression_bench.cpp` | ~250 | main harness — generates scenes, runs measurement loop, writes CSV |
| `scenes.hpp` | ~200 | 5 synthetic voxel material atlas scenes per `2026-06-21-sub-chunk-layers` precedent |
| `texture_formats.hpp` | ~110 | 10 format specs (bpp + block size + Aras 2020 PSNR projection + VRAM cost model) |
| `psnr.hpp` | ~70 | Luma + RGB PSNR computation per Aras 2020 methodology |
| `encoder_uncompressed.hpp` | ~25 | pass-through reference (PSNR = inf) |
| `encoder_bc1.hpp` | ~180 | simplified BC1 (DXT1) — 4-color mode, max/min endpoint fit, RGB+1bit alpha |
| `encoder_bc3.hpp` | ~150 | simplified BC3 (DXT5) = BC1 RGB + 8-level alpha block |
| `encoder_bc5.hpp` | ~120 | simplified BC5 — 2× BC1 for X/Y normal channels (Z reconstructed in shader) |
| `encoder_bc7.hpp` | ~280 | simplified BC7 mode 6 — single subset, 4-bit indices, 7.7.7.1 endpoints |
| `encoder_stub.hpp` | ~80 | analytical VRAM cost + projected PSNR (AWGN noise model) для BC6H, ASTC, ETC2 |
| **Total** | **~1100** | |

## Methodology

Per `benchmarks/methodology.md §3`:
- **Warm-up:** 10 iterations (uncompressed only, pre-measurement).
- **Iterations:** 100 per measurement (reduced from default 1000 due to 75k measurement budget; per-config std < 0.1 µs validated для sub-microsecond measurements).
- **Scenes:** 5 (uniform_diffuse, biome_pbr, cave_roughness, metal_emissive, mixed_stress) per `2026-06-21-sub-chunk-layers` precedent.
- **Seeds:** 5 (1, 7, 42, 1234, 31337).
- **Metrics:** encode_us, compressed_bytes, PSNR RGB + Luma per `texture_compression_bench.cpp:130-145`.
- **Output:** `build/results.csv` (one row per measurement; header + 75,000 data rows).

## Limitations

Per `RESULTS.md §6 Quality validation gap`:

| Caveat | Severity | Mitigation |
|:-------|:---------|:-----------|
| Simplified encoders give lower-bound PSNR | medium | Integrate bc7e (Binomial, Apache 2.0 OR commercial) + astcenc (ARM, Apache 2.0) + ispc_texcomp (Intel, Apache 2.0) в Stage 4.3 integration |
| GPU hardware decode cycle timing not measured | medium | Per Khronos + Aras 2020 = 1-cycle decode documented for all Tier 1/2 hardware |
| Cross-vendor decode benchmark not measured on dev host | medium | Single-GPU dev host `obvium` RTX 3060 Ti (Tier 1); analytical projection for Tier 2 |
| Synthetic scenes representative not exhaustive | low | 5 scenes × 5 seeds per `sub-chunk-layers` precedent |
| Visual QA in real ProjectV gameplay not performed | low | Stage 4.3 integration visual QA |
| SSIM / perceptual metrics not measured | low | Luma PSNR per Aras 2020 canonical methodology; SSIM deferred |
| Mutation cost (per-chunk material change → re-encode) not measured | low | Atlas encode happens at chunk-load / asset-bake, NOT per-frame |

## Cross-references

- [`../README.md`](../README.md) — hypothesis + verdict + integration recommendation.
- [`../RESULTS.md`](../RESULTS.md) — full measurement results + interpretation.
- [`../sources.md`](../sources.md) — Phase A web research with verified citations.
- [`../STATUS.md`](../STATUS.md) — final closure status.
- [`../../benchmarks/methodology.md`](../../benchmarks/methodology.md) — measurement protocol.
- [`../../hardware-profile.md`](../../hardware-profile.md) — dev host baseline (Zen 3 5800X + RTX 3060 Ti).
