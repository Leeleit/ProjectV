# Prototype — `gpu-fluid-ca-atomic-strategy` benchmark

Standalone Vulkan 1.4 compute harness для измерения 5 atomic strategies для GPU Fluid CA на
RTX 3060 Ti dev host (per `docs/experiments/hardware-profile.md §3`).

---

## Build

```bash
cd docs/experiments/experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/prototype
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

**Требования:**

- Clang 22.1.6+ (per `agent/knowledge.md` Linux baseline)
- Vulkan SDK 1.4.350+ (system, per `hardware-profile.md §6`)
- `glslc` 2026.2+ (system, per `hardware-profile.md §6`)
- VMA 3.4.0 + volk (vendored in ProjectV `external/`)
- CMake 3.28+ + Ninja

CMake автоматически подхватывает vendored VMA + volk из `../../../../external/`.

---

## Run

```bash
# Default: 5 scenes × 5 strategies × 3 seeds × N=1000 + warmup=30
./atomic_bench --scene=all --strategy=all --frames=1000 --warmup=30 --csv=results.csv

# Single scene + single strategy
./atomic_bench --scene=vertical_column --strategy=0 --frames=1000

# Dry run: empty scene control
./atomic_bench --scene=empty --strategy=0 --frames=10
```

**Output:** `results.csv` (machine-readable, 1 row per config).

---

## Scenarios (5 scene types)

| # | Name | Dimensions | Fluid cells | Purpose |
|---|------|------------|-------------|---------|
| 0 | `empty` | 64×64×64 | 0 | Control, no contention |
| 1 | `sparse` | 64×64×64 | ~4096 (~1% density, random) | Low contention baseline |
| 2 | `vertical_column` | 64×1×1 | 64 (column) | **Worst case fall** (all cells claim cell below) |
| 3 | `water_tower` | 8×32×8 | 2048 (dense block) | Vertical pressure (fall + spread at base) |
| 4 | `lava_pool` | 32×4×32 | 4096 (horizontal slab) | Horizontal pressure (spread contention) |

---

## Strategies (5 atomic strategies)

| # | Name | Description | Reference |
|---|------|-------------|-----------|
| 0 | `A_AtomicOr_Blind` | blind `atomicOr` без CAS check | `src/shaders/fluid_ca.comp:101` (current mainline) |
| 1 | `B_CAS` | `atomicCompSwap` loop (Air→Fluid) | `agent/knowledge.md` contract |
| 2 | `C_SharedMem` | per-workgroup shared mem, sequential compaction | per `WebGPU Atomic Contention 2026` + `FLIP MDPI 2026` |
| 3 | `D_SubgroupBallot` | `subgroupBallot` + `subgroupExclusiveAdd` prefix sum | per `Vulkan Subgroup Tutorial` Khronos + `Prefix Sum WebGPU` Yamasaki |
| 4 | `E_HierLock` | coarse-grained per-chunk atomic lock, sequential within chunk | per `AMD RDNA Performance Guide 2023` + fallback pattern |

---

## Per-config metrics

- **mean / median / p95 / p99 / stddev / min / max** tick latency (microseconds)
- **conservation_violations** (1 if `|fluid_after - fluid_before| > 1`; 0 = correct)
- **fluid_before / fluid_after** (cell counts)
- **atomic_ops_count** (cumulative across N iterations, from `fluidStats.atomicOpsCount` SSBO)

---

## Caveats

1. **Single GPU vendor validated** (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02).
   Cross-vendor matrix via literature (`sources.md` Tier 2).
2. **Synthetic scenes representative, not exhaustive** — 5 scenes cover main contention patterns
   (worst case fall, vertical pressure, horizontal pressure, sparse baseline, empty control).
3. **64³ max grid** (single chunk for our scenes) — Stage 4.3 128+ chunk world = 8-64 MiB grid
   size, extrapolation needed.
4. **Strategy C (SharedMemory) и Strategy D (SubgroupBallot) — single-dispatch prototype**.
   Full implementation требует 2 dispatches (claim allocation + writeback) для optimal
   compaction. Current single-dispatch variant не раскрывает full potential.
5. **Strategy E (HierarchicalLocking) — simplified single-lock per chunk**. For multi-chunk
   scenes, lock array would be per-chunk (1 lock per chunk).
6. **Per-tick upload overhead** included in timing (zeroing destination buffer + stats per tick).
   In real impl, zeroing = pipeline barrier + clear, not upload.

---

## Files

```
prototype/
├── CMakeLists.txt       # Build system
├── README.md            # This file
├── main.cpp             # C++26 harness (VMA + volk + Vulkan 1.4)
├── harness.hpp          # Vulkan context, buffer management, stats
├── scenes.hpp           # 5 scene generators
├── bench.hpp            # Stats + GLSL subprocess compile
├── strategies.comp      # 5 GLSL strategies (via #define STRATEGY_ID 0..4)
└── results/             # Output directory (created at build time)
    ├── results.csv      # Per-config metrics (after run)
    └── RESULTS.md       # Human-readable summary (after run, written by hand)
```
