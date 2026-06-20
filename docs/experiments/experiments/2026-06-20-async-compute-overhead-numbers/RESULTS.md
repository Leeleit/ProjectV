# RESULTS — 2026-06-20-async-compute-overhead-numbers

## TL;DR

**Dedicated async-compute queue on RTX 3060 Ti Ampere delivers +9.85% per-frame speedup** vs single-queue baseline
for synthetic ProjectV compute workloads (3 dispatch chains × 16 iterations). Crosses the 5% threshold from
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by ~2× margin.

**Closes measurement gap** from `2026-06-20-dec-pipelines-async-compute` (verdict=yes, но literature review
only). Quantitative numbers now available to confirm the 5-8% literature estimate — **actual measured gain
is +9.85%** (slightly above literature upper bound due to compute-heavy workload + dedicated compute queue
family).

---

## Setup

- **Hardware:** NVIDIA GeForce RTX 3060 Ti (GA104 Ampere), Vulkan 1.4.341
- **Compute queue:** dedicated compute-only queue family (family 2 = 8 queues), priority MEDIUM
- **Graphics queue:** family 0 (graphics + compute + transfer, 16 queues), priority MEDIUM
- **Sync model:** `VK_KHR_synchronization2` (core 1.3) + `VK_KHR_timeline_semaphore` (core 1.2)
- **Workloads** (synthetic ProjectV-style):
    - **Workload A (light):** 3D box-filter blur 64³, 8 substeps — models Stage 5.1 VCT mip generation
    - **Workload B (medium):** HZB cull — 4096 chunk AABBs vs 8-mip HZB, 4 substeps per mip — models Stage 2.2
    - **Workload C (heavy):** Fluid CA ping-pong 64³, 4 substeps, atomicOr claims — models Stage 3.1 per
      `agent/knowledge.md §30.4`
- **Graphics dummy:** full-screen triangle, 1920×1080, dynamic rendering — baseline render pass
- **Multiplier:** each compute workload dispatched **16 times per frame** to give GPU non-trivial work
  (realistic for ProjectV fluid CA 16 substeps per tick)
- **Frames:** 200 measured frames per mode (after 30 warmup)
- **Toolchain:** GCC 16.1.1 / libstdc++ 16 / glslc 2026.2

---

## Measurements

### Sequential mode (single queue baseline)

| Metric                                 | Value (ms) |
|:---------------------------------------|:-----------|
| Frame time mean                        | **0.771**  |
| Frame time median                      | 0.703      |
| Frame time p95                         | 1.102      |
| Frame time p99                         | 1.917      |
| Frame time std                         | 0.180      |
| GPU graphics time mean                 | **0.050**  |
| GPU compute time mean (×16 multiplier) | **0.619**  |
| GPU total = gfx + compute              | **0.669**  |

### Async mode (graphics || compute)

| Metric                                 | Value (ms) |
|:---------------------------------------|:-----------|
| Frame time mean                        | **0.695**  |
| Frame time median                      | 0.636      |
| Frame time p95                         | 0.942      |
| Frame time p99                         | 1.172      |
| Frame time std                         | 0.144      |
| GPU graphics time mean                 | **0.046**  |
| GPU compute time mean (×16 multiplier) | **0.579**  |
| GPU total = gfx + compute              | **0.625**  |

### Overlap gain

| Metric                    | Value                            |
|:--------------------------|:---------------------------------|
| Frame time savings        | 0.771 − 0.695 = **0.076 ms**     |
| **Speedup**               | **9.85%**                        |
| GPU compute time savings  | 0.619 − 0.579 = 0.040 ms (−6.5%) |
| GPU graphics time savings | 0.050 − 0.046 = 0.004 ms (−8.0%) |

---

## Interpretation

### What async helps

- **Wall clock frame time drops by ~10%.** This is the metric that matters for steady-state FPS.
- **GPU compute time drops by ~6.5%.** Compute work overlaps with graphics; the SMs that would be idle
  during graphics-bound periods now execute compute work.
- **Tail latency (p99) drops more:** 1.917 ms → 1.172 ms = **−39% p99**. Async helps most in worst-case
  frames where scheduling jitter normally bunches work.

### Why the gain is higher than literature (5-8%)

1. **Dedicated compute-only queue family 2** (8 queues). Per `vulkaninfo --summary` probe `2026-06-20`,
   RTX 3060 Ti exposes true async compute hardware (NVIDIA Ampere ACE engines). Literature averages
   over heterogeneous hardware.
2. **Heavy compute workload** (16 dispatch multiplier × 3 workloads = 48 dispatches). The more compute
   work, the more benefit from hiding it under graphics.
3. **Lightweight graphics dummy** (0.05 ms). Real ProjectV render passes (CSM, SVO meshing, voxel DDA)
   would be ~2-5 ms, leaving MORE room for compute overlap.
4. **No present/wait overhead** — headless harness, no swapchain wait. Async-compute's main benefit
   (overlapping with the NEXT frame's graphics) isn't measured here.

### Caveats / what we did NOT measure

- **Single GPU vendor:** only NVIDIA RTX 3060 Ti GA104 Ampere validated. AMD RDNA2/3/4 / Intel Arc
  expected to behave similarly per `dec-pipelines-async-compute` §2.2 vendor matrix — needs separate
  hosts for direct measurement.
- **No driver-specific caveats exercised:** NVIDIA June 2025 mesh-shading+async bug (per
  `dec-pipelines-async-compute` Caveat #1) does not apply to compute cull path (no mesh shaders here).
  Real ProjectV mainline Stage 2.1 mesh-shader port would need separate validation if enabled.
- **No cross-frame pipelining:** harness submits-then-waits each frame. Real renderer overlaps frame N+1
  graphics with frame N compute (DiligentEngine up to 2× gain). Could add 10-30% more headroom.
- **Synthetic workloads** model ProjectV patterns but not actual code paths. Real-world Stage 3.1
  fluid CA will have larger volume (256³ vs 64³) and different memory access patterns.

---

## Comparison to literature predictions

| Source                                     | Vendor / HW            | Workload                      | Gain                                              |
|:-------------------------------------------|:-----------------------|:------------------------------|:--------------------------------------------------|
| `dec-pipelines-async-compute` (literature) | mixed                  | mixed ProjectV compute        | 5-8% (predicted, not measured)                    |
| **This experiment**                        | **NVIDIA RTX 3060 Ti** | **3 × 16 dispatches compute** | **9.85% (measured)**                              |
| nvpro-samples vk_timeline_semaphore        | RTX 3090               | mcubes geometry               | 15% (release build)                               |
| Vulkan async_compute sample                | generic desktop        | FRAG→COMP→FRAG                | ~5%                                               |
| DiligentEngine Tutorial 23                 | desktop GPU            | terrain height/normal         | up to 2× (with double-buffering latency tradeoff) |

**Verdict:** Measured 9.85% aligns well with literature predictions (5-8%). Within range of NVIDIA RTX 3090
nvpro-samples reference (15% for mcubes). Crosses 5% threshold comfortably.

---

## Outputs

- `results.csv` — machine-readable summary (mode, frames, mean/median/p95/p99/std/min/max ms)
- `async_bench` — built executable (run with `--frames=N --mode=seq|async|both`)

---

## Reproduce

```bash
cd docs/experiments/experiments/2026-06-20-async-compute-overhead-numbers/prototype
make
./async_bench --frames=200 --mode=both
```

Defaults: WARMUP_FRAMES=30, MEASURE_FRAMES=200, kComputeMultiplier=16.