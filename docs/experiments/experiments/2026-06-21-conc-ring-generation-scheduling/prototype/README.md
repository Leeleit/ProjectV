# Prototype: concentric ring generation scheduling benchmark

Standalone C++26 CPU harness simulating chunk generation scheduling across player movement patterns.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

## Run

```bash
./build/conc_ring_gen_bench > results.csv
```

## Output

CSV with columns: strategy,movement,seed,seq,completed,stalls,inner_done,inner_total,inner_pct,covered_pct

## Strategies

| ID | Name | Description |
|----|------|-------------|
| 0 | A_DistSorted | Distance-sorted (baseline) |
| 1 | B_ConcRing3 | 3-level concentric ring priority |
| 2 | C_ConcRing5 | 5-level concentric ring priority |
| 3 | D_SeqRings | Sequential ring phases (inner → outer) |

## Movement patterns

| ID | Name | Description |
|----|------|-------------|
| 0 | stationary | Player stays at origin |
| 1 | linear_walk | Player walks in straight line |
| 2 | teleport | Player teleports every 60 frames |
| 3 | circular | Player walks in circle |
| 4 | rand_walk | Random walk (normal distribution) |
