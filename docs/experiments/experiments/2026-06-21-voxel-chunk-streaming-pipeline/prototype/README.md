# Prototype README — `2026-06-21-voxel-chunk-streaming-pipeline`

Standalone C++26 CPU streaming simulator. **NOT** ProjectV mainline. Synthetic voxel world (4096 chunks
max, 1.7 KiB/compressed per chunk) + 3-tier memory hierarchy (VRAM 0 µs / RAM 35 ns / SSD 0.6 µs).

---

## Build

```bash
cd experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  -o build/stream_bench stream_bench.cpp
```

Or with the helper script:

```bash
bash build.sh
```

Compiler: `clang++ 22.1.6` per `hardware-profile.md §1` / `agent/knowledge.md` Linux baseline.

---

## Run

Default (5×5×5 configs, 10 warmup + 1000 frames):

```bash
./build/stream_bench
# → build/results.csv (125 rows)
```

Custom (verbose):

```bash
./build/stream_bench --warmup 10 --frames 1000 --output build/results.csv --verbose
```

---

## Output

- `build/results.csv` — machine-readable, 126 rows (1 header + 125 data rows).
  Columns: `strategy, scene, seed, stutter_us_mean, stutter_us_p99, stutter_us_max, bg_us_mean,
  bg_us_p99, vram_mb_mean, vram_mb_max, ram_mb_mean, ram_mb_max, ssd_loads_total,
  chunks_loaded_total, frames`.
- `RESULTS.md` — human-readable summary tables + observations.
- Console progress (with `--verbose`): per-config line with key metrics.

---

## Methodology

Per `docs/experiments/benchmarks/methodology.md §3`:
- **Warm-up:** 10 frames (results discarded).
- **Measurements:** 1000 frames per config.
- **Seeds:** 5 (1, 7, 42, 1234, 31337) — xoshiro256**-equivalent (MT19937) per C++26 std::random.
- **N configs:** 5 strategies × 5 scenes × 5 seeds = 125 configs.
- **Total main measurements:** 125 × 1000 = **125,000**.

**Movement pattern generators (5 scenes):**
- `linear_walk` — constant velocity along +X axis.
- `teleport_stress` — random teleport every 1-5 seconds (30-150 frames @ 30 Hz).
- `orbit_center` — camera circles around (8, 8, 8) at constant radius 6.
- `fly_vertical` — vertical movement along +Y axis.
- `spiral_in` — spiral toward center with decreasing radius.

---

## Code structure

`stream_bench.cpp` single file, ~700 LoC:
- `Stats` harness (mean / median / p95 / p99 / stddev / min / max) per `benchmarks/methodology.md §7`.
- `ChunkId` (xyz tuple + hash) + `Chunk` (tier + last access + priority).
- `MemoryTiers` (3-tier latency/capacity model).
- `TierStore` (3-tier cache with LRU eviction).
- `Strategy` interface + 5 implementations (A_PrebakeAll / B_FixedRing / C_PredictiveStreaming /
  D_DemandPaging / E_HybridDemandPredictive).
- Movement pattern generators + `RunSimulation` harness.
- CSV output via `std::format`.

---

## Caveats

- **CPU simulator only** — no real I/O, no Vulkan, no GPU dispatch.
- **Synthetic chunk model** = 1.7 KiB/compressed (representative of `nanovdb-on-gpu` 12-16 B/voxel +
  mesh + materials + physics).
- **16×16×16 world = 4096 chunks** = Stage 4.3 128m draw distance scale.
- **No mutation cost** (per-chunk rebuild on voxel edit) — separate axis.
- **No GPU upload cost** — orthogonal axis.
- **C and E show identical metrics** in this prototype because predictive prefetch dominates; demand
  paging path of E not exercised in synthetic movement patterns. Real-world difference = how well
  prediction matches actual player decisions.

See [`RESULTS.md`](./RESULTS.md) for full analysis + caveats.
