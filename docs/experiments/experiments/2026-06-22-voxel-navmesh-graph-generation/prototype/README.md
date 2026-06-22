# prototype/

Standalone C++26 CPU prototype для `2026-06-22-voxel-navmesh-graph-generation` experiment.

**Изолирован от mainline** (per `AGENTS.md §1`): standalone C++26, no external dependencies beyond standard library + POSIX `<chrono>` / `<random>`.

## Build

```bash
cd docs/experiments/experiments/2026-06-22-voxel-navmesh-graph-generation/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
        -o build/navmesh_bench navmesh_bench.cpp
```

**Toolchain:** Clang 22.1.6 (per `hardware-profile.md §6`).

## Run

```bash
./build/navmesh_bench
```

## Output

- `build/results.csv` — 126 rows = 1 header + 125 data (5 strategies × 5 scenes × 5 seeds)
- `build/summary_means.csv` — 5 rows, per-strategy mean across 25 configs
- `build/run.log` — experiment metadata + wall time + iteration count

## What it measures

**5 strategies** ∈ {A_NaiveVoxelGrid_3DBool, B_WalkableHeightfield_2D, C_RecastStyle_PolyMeshContour, D_VoxelSurfaceGraph, E_Hybrid3D_RegionGraph} × **5 scenes** ∈ {open_terrain, sparse_rocks, dense_urban, stairs_ramp, destroyed_building} × **5 seeds** ∈ {1, 7, 42, 1234, 31337} × **1000 iter + 10 warmup = 125,000 main measurements**.

**Per measurement:**
- `gen_time_ns` — total chunk regeneration time (median across 1000 iter)
- `storage_bytes` — estimated bytes used for navmesh storage
- `waypoint_count` — number of nodes / regions
- `edge_count` — number of edges / connections
- `door_count` — vertical transitions (E strategy only)
- `query_time_ns` — A* per random source-target pair (mean across 100 pairs)
- `paths_found` — how many of 100 random pairs found a path

## Architecture

**ProjectV chunk = 8×8×8 voxels** (per `agent/knowledge.md`). All strategies operate on a single 8³ chunk (512 voxels, 64 B walkable mask per byte-storage strategy).

**Per-strategy implementation:**
- A: `std::array<uint8_t, 512>` walkable mask + 3D A* on 6-connectivity
- B: `std::array<int8_t, 64>` top-walkable-Y per XZ column + 2D A* on 4-connectivity with step-up/down check
- C: voxelize → BFS regions → 1 quad per region → poly mesh + 2D A* on region graph (3D-proximity adjacency)
- D: 3×3 XZ local-max surface centers + sparse node graph + A* on edges (Manhattan distance ≤ 2 + same Y)
- E: per-Y-level BFS regions + doorway detection (step-up/down/jump) + region graph with doors

## Caveats

- CPU-only analytical cost model (no real JPH coupling, no Flecs ECS overhead, no real Vulkan dispatch, no real network/streaming).
- Simplified Recast (C) uses 1 quad per region + 2.5 voxel adjacency radius — loses much of real Recast quality.
- 2D A* on 8×8 = 64 cells (B) is representative; 3D A* on 8³ = 512 cells (A) is slower per query due to larger search space.
- All strategies implement full chunk regenerate; incremental update is mainline integration concern.
- Cross-chunk seam handling not modeled.
- Multi-floor 3D nav is out of scope (research-grade).
