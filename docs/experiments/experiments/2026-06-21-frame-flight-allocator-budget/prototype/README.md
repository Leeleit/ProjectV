# Prototype — frame-flight-allocator-budget

Standalone Vulkan 1.4 harness. **NOT ProjectV mainline** — pure isolated research
in `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/`.

## What it tests

Four (five, including E) allocation strategies, all running the **same** per-frame
workload simulating ProjectV's planned Stage 2.x/3.x/5.x transient SSBO additions:

| Item                | Size      | Notes                                        |
|:--------------------|:----------|:---------------------------------------------|
| Material visual SSBO| 256 KiB   | persistent, mapped (host-visible)            |
| Cluster grid SSBO   | 27 KiB    | persistent, mapped (host-visible)            |
| N small SSBOs       | 256 B × N | PackedFace-like (N=64 default)               |
| NanoVDB transient   | 1 MiB     | per-frame on world edit                      |
| BLAS pool entry     | 4 MiB     | per-frame (Stage 5.2 RTX)                    |
| Image (HZB/VCT)     | 4 MiB     | R32G32_UINT, 1024×1024                       |
| World edit spike    | 8 MiB     | every 200 frames (default)                   |

Stress pass: 256 MiB spike every 50 frames (overflow test for hard cap).

## Strategies

| Letter | Strategy                | Hypothesis                                      |
|:-------|:------------------------|:------------------------------------------------|
| A      | Default VMA             | baseline (current ProjectV mainline behavior)   |
| B      | Budget tracking         | A + `EXT_MEMORY_BUDGET_BIT` + `WITHIN_BUDGET`   |
| C      | Linear pool (per-frame) | ring buffer pool created + destroyed each frame |
| D      | Double-buffered (per-frame)| C + `WITHIN_BUDGET` flag                       |
| E      | Pre-created ring buffer | ring buffer pool created once at startup        |

## Build

```bash
cd docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
ninja
```

Dependencies: vendored VMA 3.4.0 + volk from `ProjectV/external/`, system Vulkan loader
1.4.350. **No changes to ProjectV mainline build.**

## Run

```bash
./frame-flight-prototype
```

Output: human-readable summary to stdout + `results.csv` in current directory.
TracyPlot-friendly column names.

## Results

See `../README.md` §5 for the full analysis + tables. Highlights:

- A vs B: WITHIN_BUDGET adds <2% overhead at the cost of OOM protection
- C/D: per-frame pool recreate is 1000+ µs/frame — **not viable** for ProjectV
- E: pre-created ring buffer matches A in latency at the cost of 64 MiB peak VRAM
  (1.9% of 5.06 GiB driver budget per `hardware-profile.md` §3)
- Stress (256 MiB spike): D's 64 MiB pool returns 21 clean `OUT_OF_DEVICE_MEMORY`
  errors with `WITHIN_BUDGET_BIT` instead of OOM-thrashing
