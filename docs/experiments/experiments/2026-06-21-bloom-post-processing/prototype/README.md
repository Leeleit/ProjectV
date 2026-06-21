# Bloom benchmark prototype

Standalone C++26 CPU analytical cost model for bloom post-processing strategies on RTX 3060 Ti @ 1080p.

## Build & run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bloom_bench
```

## Output

- `results.csv` — 151 rows (header + 150 measurements): 6 strategies × 5 scenes × 5 seeds × 1000 iter

## Strategies

| ID | Name | Description |
|----|------|-------------|
| A | NoBloom | Baseline (current mainline) |
| B | GaussianPyramid | 5-level gaussian pyramid (12 passes) |
| C | KawaseDual | Dual Kawase filter (10 passes) |
| D | SeparableLattice | Separable 9-tap lattice blur (3 passes) |
| E | LensDirtComposite | Gaussian + lens dirt overlay (13 passes) |
| F | AdaptiveThreshold | Variance-adaptive + Kawase dual (1-11 passes) |

## Scenes

| Scene | Bright fraction | Description |
|-------|---------------:|-------------|
| uniform_floor | 5% | Low dynamic range |
| forest_floor | 15% | Mixed diffuse + highlights |
| cave_stress | 8% | Dark + lava patches |
| lava_pool | 40% | Large emissive area |
| emissive_cluster | 25% | Many small emissive blocks |

## Hardware

Calibrated for RTX 3060 Ti GA104 Ampere (38 SMs, 1.665 GHz, 448 GB/s) per `hardware-profile.md §3`.
