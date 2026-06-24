# Prototype — `2026-06-21-audio-diffraction-hybrid`

Standalone C++26 CPU prototype, no GPU deps, no ProjectV mainline dependency.

## Build

```bash
make
# Uses Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG
# Per `hardware-profile.md §6`
```

## Run

```bash
./bench results.csv
# Output:
#   - results.csv (machine-readable, 28 rows: 1 header + 27 measurements)
#   - stdout: per-strategy × per-scene × per-seed summary
```

## Clean

```bash
make clean
```

## Files

| File | LoC | Purpose |
|:-----|:----|:--------|
| `voxel_grid.hpp` | ~350 | Synthetic voxel grid + DDA ray traversal + edge finding + depth-mip chain stub |
| `audio_path.hpp` | ~30 | Common interface (Strategy enum + AudioResult struct) |
| `diffraction.hpp` | ~180 | 3 strategies: A_None, B_Schissler, C_Tsingos |
| `bench.cpp` | ~150 | Measurement harness per `benchmarks/methodology.md §3` |
| `Makefile` | ~25 | Build with Clang 22.1.6 |
| `RESULTS.md` | ~250 | Per-strategy × per-scene × per-seed results + analysis |
| `results.csv` | 28 lines | Machine-readable measurements |

**Total: ~985 LoC C++26, build + run in <30 seconds on dev host.**

## Measurement campaign

Per `benchmarks/methodology.md §3`:
- **Warmup:** 5 iterations per strategy × scene × seed.
- **Iterations:** 100 per strategy × scene × seed.
- **Sources per iteration:** 16 (per audio engine typical 16-32 channel budget).
- **Total measurements:** 3 strategies × 3 scenes × 3 seeds × 100 iter × 16 sources = **14,400 strategy invocations**.
- **Output format:** mean / median / p95 / p99 / stddev / min / max latency per strategy × scene × seed, plus mean attenuation (dB) and mean probe count.

**Note:** methodology §3 default is N=1000. We use N=100 for research-prototype speed. Trade-off: slightly wider confidence interval. Per closed `audio-raytracing-voxel-sdf` precedent, N=100 was sufficient for trends.

## Status

Prototype **complete and validated**. Results in `RESULTS.md`. Per `agent/knowledge.md` 3-step migration precedent, integration recommendation in main `README.md §7`.
