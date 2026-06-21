# Prototype — `2026-06-21-voxel-mutation-cost-characterization`

Standalone C++26 CPU mutation cost simulator. **NOT ProjectV mainline** — это self-contained research harness per `docs/experiments/AGENTS.md §2`.

## Build

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-voxel-mutation-cost-characterization/prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
        mutation_bench.cpp -o build/mutation_bench
```

Toolchain: Clang 22.1.6 (validated on dev host `obvium` per `hardware-profile.md §6`).

## Run

```bash
./build/mutation_bench build/results.csv
```

Output: `build/results.csv` (626 rows = 1 header + 625 configs × 5 strategies × 5 scenes × 5 mutation patterns × 5 seeds).

Wall time: ~155 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Last 75 configs (E_CopyOn+dedup × P5_StressBurst) dominate (~120 sec); remaining 550 configs ~35 sec.

## Architecture

### SVDAG-on-64-tree model (`svdag::VoxelSvdag64`, ~150 LoC)

Faithful to mainline `src/voxel/Sparse64Tree.hpp`:
- 4³ branching (`kNodeSide=4`, `kChildrenPerNode=64`)
- Per-chunk side=8 (`kChunkSide=8`, `kChunkSize=512 voxels`)
- Per-node COW via `refCount` + `MarkNodeUnique`
- Per-node dedup hash index via `unordered_multimap<uint64_t, uint32_t>` (опционально, E strategy)
- Collapse-to-homogeneous optimization (`CanCollapseToHomogeneous`)
- Per-node fillMask 64-bit bitmask

### Strategies (5)

| Strategy             | Описание                                                                       | Mainline equivalent                              |
|:---------------------|:-------------------------------------------------------------------------------|:-------------------------------------------------|
| A_NaiveInPlace       | Per-SetCell rebuild path через SetCellRecursive                              | Current mainline                                 |
| B_DirtyFlagDeferred  | Skip duplicate SetCell per chunk per frame (last write wins)                | Recommended Step 1 integration (~30 LoC)        |
| C_BatchCoalesce      | Group SetCells per chunk → rebuild chunk tree ONCE                            | mathijs727 GPU-SVDAG-Editing Phase 1 (NOT recommended) |
| D_DoubleBufferSwap   | Clone current tree to staging; mutations in staging; commit = swap pointer    | HashDAG Phase 1+2 pattern (recommended Step 2)  |
| E_CopyOnWriteSnapshot| Per-node COW + dedup hash table ON (PERFORMANCE DISASTER for gameplay)       | Mainline dedup ON (`PROJECTV_SPARSE_64_STORAGE=ON`) — NEVER for dynamic worlds |

### Scenes (5)

Synthetic 8³ chunks representative of ProjectV gameplay:
- **uniform_floor** — VoxelLab baseline analog
- **sparse_world** — Minetest SkyBlock analog
- **mixed_biome** — Biome transition analog
- **cave_stress** — Underground cave analog
- **stacked_solid** — Solid homogeneous tower (best case for collapse)

### Mutation patterns (5)

Per-frame batch sizes representative of gameplay:
- **P1_SingleClick** — 1 SetCell (single block place/break)
- **P2_FillOperation** — 64 SetCells (FillVoxelBox 4³ analog)
- **P3_MultiChunkBuild** — 64 across 8 chunks (line build)
- **P4_FloodFill** — ~128 (FillVoxelMaterial BFS analog)
- **P5_StressBurst** — 256 (cheat-script burst / GPU world gen)

## Files

```
prototype/
├── README.md            # этот файл
├── mutation_bench.cpp   # ~750 LoC — SVDAG model + 5 strategies + harness
└── build/
    ├── mutation_bench   # compiled binary
    └── results.csv      # 626 rows × 17 cols, 80 KB
```

## See also

- [`../README.md`](../README.md) — эксперимент README с гипотезой + integration recommendation
- [`../RESULTS.md`](../RESULTS.md) — подробный analysis + 10 sections
- [`../sources.md`](../sources.md) — 24 источника (Tier 1-5)
- [`../STATUS.md`](../STATUS.md) — final status + 5 phases complete