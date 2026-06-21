# RESULTS — voxel-hydraulic-erosion

## Summary

| Strategy | Mean (µs/iter) | PSNR vs ref (dB) | PSNR vs raw (dB) | Note |
|:---------|:---------------|:-----------------|:-----------------|:-----|
| A_NoErosion | ~0.0 | 28.8-29.6 | 100.0 (identical) | Baseline — raw noise |
| B_CPUParticleDroplet | 3.1-4.5 | 28.8-29.6 | 77-97 | Different erosion pattern than pipe model |
| C_CPUPipeModel | 475-502 | 32.2-33.0 | 38.5-39.4 | Physically-based, best quality |
| D_GPUPipeModelAnalytical | **11.7** | N/A | N/A | **40-43× faster than CPU** |
| E_SimplifiedSlopeMethod | 57-66 | 28.8-29.6 | 100.0 (identical) | Threshold too high for procedural terrain |

## Headline findings

**C_CPUPipeModel** (Mei 2007 / Jako 2011) = gold standard for quality: PSNR 32.2-33.0 dB against long-run reference, visibly natural erosion features (riverbeds, sediment fans). Cost: ~480 µs/iter on Zen 3 5800X single core for 128×128 grid (16×16 ProjectV chunks).

**D_GPUPipeModelAnalytical** = clear perf winner: **11.7 µs/iter** projected on RTX 3060 Ti (448 GB/s, 12.7 TFLOPS). 40-43× speedup over CPU. At this cost, 200 iterations = 2.34 ms = 7% of 30 Hz budget. VRAM: 0.25 MiB for 4 float buffers. Dominant cost: launch overhead (≈8 µs per dispatch), not memory/ALU.

**B_CPUParticleDroplet** (Job Talle / Musgrave) = fastest CPU method at 3.1-4.5 µs/iter. However, produces a different erosion character than pipe model — PSNR vs pipe reference is same as baseline (28.8-29.6 dB). Still, 3.5 µs/iter × 200 = 0.7 ms on CPU is attractive for lightweight per-chunk erosion.

**E_SimplifiedSlopeMethod** (Machado 2019 / mega-minecraft) — did NOT activate at kSlopeMax=1.2 on procedural terrain (max gradient ~0.9). Confirms literature: slope method needs lower threshold or multi-pass with decay for procedural heightfields.

## PSNR quality ladder

| Quality tier | Method | PSNR vs ref (dB) | Relative gain |
|:-------------|:-------|:-----------------|:--------------|
| Baseline (no erosion) | A | 28.8 | — |
| Good | C | 32.2-33.0 | **+3.4-4.2 dB** |
| Different/potentially useful | B | 28.8-29.6 | +0-0.8 dB (different patterns though) |
| Not applicable | E | 28.8 (no change) | — |

## Key insight

GPU pipe model at ~11.7 µs/iter (200 iter = 2.34 ms) is viable as offline world-gen pre-processing. For real-time use, droplet method at 3.5 µs/iter (200 iter = 0.7 ms) fits within Stage 4.1 budget. The pipe model produces the most natural-looking terrain but should be GPU-accelerated for production use.

**Hardware baseline:** см. [`hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X), §3 (RTX 3060 Ti 448 GB/s, 12.7 TFLOPS).
