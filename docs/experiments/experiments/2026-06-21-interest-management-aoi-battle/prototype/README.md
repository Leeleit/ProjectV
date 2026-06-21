# aoi_bench prototype

Standalone C++26 CPU prototype for grid-based AOI (Area of Interest) netcode simulation.

**Maps to ProjectV:** see [`../README.md` §9](../README.md) — corresponds to netcode layer
needed for Stage 6+ military sandbox multiplayer (not in current TODO Stage 1-6).

## Build

```bash
cd prototype/build
cmake .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Expected: Clang 22.1.6 or GCC 16.x, no warnings on `-Wall -Wextra -Wpedantic`.

## Run

```bash
./aoi_bench
```

Outputs:
- `aoi_bench` stdout — human-readable summary table
- `aoi_bench_results.csv` — per-config mean/median/p95/p99/std (machine-readable)

Wall time target: < 30 sec on Zen 3 5800X.

## Strategies

- **A_FullBroadcast** — every player gets every entity update. O(N²) per tick. Baseline.
- **B_GridAOI_NoTiering** — grid AOI, 9-grid lookup, single 200 m range, all in-range at 20 Hz.
- **C_GridAOI_3Tier** — 3 tiers: critical 200m@20Hz / peripheral 500m@5Hz / ambient>500m@1Hz.
- **D_GridAOI_3Tier_Priority** — C + per-object priority queue.
- **E_GridAOI_3Tier_KNN_BackCull** — C + KNN variable radius + back cull via rotation.
- **F_GridAOI_3Tier_Batched** — C + packet batching.

## Network / GPU

CPU-only analytical prototype. Bandwidth estimates from per-tick byte count + 30 Hz, no protocol
overhead modeling. Real-world deployment will need actual netcode + packet scheduling.
