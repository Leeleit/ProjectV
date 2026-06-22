# 2026-06-22-procedural-voxel-resource-deposits — Procedural resource deposit strategies for voxel terrain

**Status:** `concluded-verdict-mixed`
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22
**Stage link:** `independent` (precedes TODO.md §5.5 Resource nodes / harvestable entities)
**Estimated effort:** M
**Author:** agent

---

## 1. Hypothesis

Five procedural strategies for placing resource deposits (ore veins, mineral pockets) in voxel terrain can be compared on three axes: **speed** (time per chunk), **connectivity** (fewer components → more vein-like), and **geological plausibility** (seam proximity + cluster coherence). The hypothesis is that a hybrid approach (worm + seam) yields best plausibility at moderate cost, while simple uniform random gives worst geological results.

**Alternatives considered:**
- Pure noise thresholding (simple but no vein structure)
- Seam-only boundary detection (good for contact deposits like pegmatite, poor for uniform lithology)
- Voronoi biomes (consistent regional control, but fragmented)
- Perlin worms (geologically plausible veins, but coverage depends on worm count)

---

## 2. Prior art

- **Minecraft 1.17+ ore vein system** — `vein_toggle` + `ridged` + `gap` noise density functions placed in biome JSON; seams control ore generation at block-face contacts; Perlin-like noise for vein shape. Minecraft Wiki, 2021.
- **Minetest ore types** — `scatter`, `sheet`, `claylike`, `blob`, `vein`; vein uses 2D Perlin worms with thickness falloff. Minetest Lua API 5.9.
- **Cubyz OreGenerator** — Zig implementation; sparse 3D noise + flood-fill expand; separate per-biome density maps. Cubyz GitHub, 2023.
- **Nathan Reed (2010): Procedural ore distribution** — Poisson disc sampling + Perlin worms in 2D top-down view with 3D extrusion. https://www.reedbeta.com/blog/procedural-ore-deposits/
- **Iridescence (2014): Perlin worms 3D** — 3D worm path via Perlin gradient following; vein thickness via distance field. GDC talk.
- **Minecraft noise router** — `nether_vein` / `overworld_vein` density functions with control over rarity, size, disc distribution. Minecraft Cactus Configs, 2023.
- **GDC 2015: Procedural Content Generation in No Man's Sky** — voronoi biomes at planetary scale; resources assigned per-cell with falloff at edges.
- **Gonzalez & Patow (2023): A Procedural Method for Automatic Generation of Geological Ore Deposits** — structural controls (faults, folds) + perlin worms; layered mineralization.

---

## 3. Method

- **Type:** prototype + benchmark
- **Chunk size:** 8³ (512 voxels) — canonical ProjectV micro-chunk
- **5 strategies:**
  - **A_UniformRandom** — baseline: each non-air voxel has 12% chance of deposit
  - **B_SeamBoundary** — seam voxel (neighbor of different material) gets 40%; falloff via Manhattan BFS distance (≤2 voxels)
  - **C_PerlinWorm** — 1–3 worms with random-walk path, deposit at each step
  - **D_VoronoiBiome** — 6 Voronoi cells, deposit probability higher near cell edges
  - **E_Hybrid_WormPlusSeam** — seam nuclei (25%) + worm growth from seam sites + near-seam expansion (30% at dist≤1)
- **5 scenes:**
  - `uniform_stone` — single material (1), no seams
  - `stratified_3layer` — 3 material layers via noise + depth
  - `granite_pegmatite` — granite background with pegmatite blobs
  - `folded_metamorphic` — folding folds + noise, 3 lithologies
  - `volcanic_complex` — columnar basalt + cinder air pockets
- **5 seeds** per strategy×scene
- **50 iterations** per config (after 5 warmup) = 6250 total measurements
- **Metrics:**
  - time_per_call (ns) — wall-clock via `std::chrono::steady_clock`
  - deposit_count — voxels placed
  - components — 6-connectivity flood-fill count (lower = more connected)
  - plausibility — 0–1 formula based on seam proximity + cluster coherence
- **Control:** all strategies share same scene data per seed; harness overhead measured uniformly

---

## 4. Prototype

**Code:** `prototype/resource_bench.cpp` (~470 LoC C++26 standalone)

**Build & run:**
```bash
cd prototype
cmake -B build -DCMAKE_CXX_COMPILER=clang++-19
cmake --build build
./build/resource_bench > results.csv
```

**Output:** CSV with columns `strategy,scene,seed,iter,time_ns,count,components,plausibility`

**Dependencies:** C++26 standard library only (`<chrono>`, `<random>`, `<algorithm>`, `<span>`). Simplex noise implementation included.

**Methodology:**
- Warmup: 5 unmeasured calls per config before taking data
- Iterations: 50 timed calls, nano-precision timing
- Geology: 3D Simplex noise (2–4 octaves) for scene material distribution
- Component analysis: BFS 6-connectivity via ring-buffer queue on 512-voxel grid

---

## 5. Results

### Overall (all scenes, 6250 measurements)

| Strategy | Time (ns) | Count | Components | Plausibility |
|:---------|---------:|------:|:----------:|:------------:|
| A_UniformRandom | 4,121 | 52.0 | 36.5 | 0.000 |
| B_SeamBoundary | 9,714 | 88.2 | 25.1 | 0.249 |
| C_PerlinWorm | 1,701 | 9.5 | 4.8 | 0.195 |
| D_VoronoiBiome | 7,345 | 76.5 | 43.1 | 0.311 |
| E_Hybrid_WormPlusSeam | 12,127 | 119.7 | 17.3 | 0.427 |

### Per scene (seam-rich scenes only: folded_metamorphic, granite_pegmatite, stratified_3layer)

| Strategy | Time (ns) | Count | Components | Plausibility |
|:---------|---------:|------:|:----------:|:------------:|
| A_UniformRandom | 4,655 | 60.5 | 42.0 | 0.000 |
| B_SeamBoundary | 12,221 | 125.1 | 30.8 | 0.315 |
| C_PerlinWorm | 1,731 | 11.0 | 5.4 | 0.212 |
| D_VoronoiBiome | 8,741 | 93.4 | 49.4 | 0.312 |
| E_Hybrid_WormPlusSeam | 16,094 | 172.7 | 19.0 | 0.545 |

### Observations

- **C_PerlinWorm** is fastest (1.7µs) but produces few deposits (9.5 avg) — worms are too thin and short for 8³ coverage. Increasing worm count or thickness would close the gap.
- **E_Hybrid** has highest plausibility (0.43–0.55 on seam scenes) and best connectivity (17.3 components for 120 deposits — ~7 voxels per component). The two-phase design (seam nucleation + worm growth) creates coherent veins along contacts.
- **B_SeamBoundary** works well for scenes with seams (plausibility 0.32), but on uniform_stone it produces 0 deposits (no seams to seed from — expected and correct behavior).
- **D_VoronoiBiome** has high plausibility (0.31) but also highest fragmentation (43.1 components). The cell-edge bias creates disconnected pockets per region.
- **A_UniformRandom** scores 0 plausibility — the formula penalizes extreme fragmentation (37 components for 52 deposits, ~1.4 voxels per component).
- **volcanic_complex** scene has air pockets, reducing available voxels by ~30% — all strategies produce proportionally fewer deposits there.
- **uniform_stone** scene has no material boundaries, so seam-dependent strategies (B, E) produce near-zero deposits. This is geologically correct: contact-metamorphic deposits require contrasting lithologies.

---

## 6. Verdict

**mixed** — The hybrid approach (E) is clearly superior in geological plausibility and connectivity, but at 12µs per 8³ chunk it is 3× slower than the uniform baseline and 7× slower than Perlin worms. For a 32×32×32 region (512 micro-chunks), this adds ~6ms per generation pass — acceptable for worldgen background thread but too expensive for on-demand chunk generation during player interactions. A tiered approach is recommended: cheap Perlin worms for background generation, hybrid with seam-awareness for resource hotspots.

---

## 7. Integration recommendation

- **Target stage:** Precedes `TODO.md §5.5` (Resource nodes / harvestable entities). This experiment is the foundation for choosing *where* to place resource nodes.
- **Concrete changes:**
  1. Implement **Strategy E (Hybrid)** as the primary deposit generator for `VoxelWorld` chunk finalization, but with configurable parameters:
     - Seam nucleation probability (default 0.25)
     - Worm count (default 2 per chunk)
     - Near-seam expansion radius (default 1 voxel)
  2. Add a **fast path (Strategy C, Perlin worms)** for background/LOD chunks where deposits are not visually resolved.
  3. Store deposit metadata per micro-chunk: an `std::bitset<512>` or `uint64_t[8]` bitmask for resource presence (2 bits per voxel → 4 resource types max).
  4. Wire the generation into the existing `VoxelWorld` chunk generation pipeline at material-finalization stage (after strata/erosion, before mesh building).
- **Risks:**
  - Seam detection assumes material boundaries are finalized — if terrain modification (mining, explosions) happens, the static deposit must be preserved independently of material adjacency.
  - Performance impact on worldgen: 12µs per 8³ chunk → ~75ms for a 16×16×16 region (256 micro-chunks). Acceptable for async generation but must not block render thread.
  - Resource type allocation per seed needs to be deterministic across chunk borders to avoid visible seams at chunk boundaries (voronoi-based resource typing mitigates this).
- **Dependencies:** `VoxelWorld` chunk generation must expose material-finalization callback. Resource type palette must be defined in world config.
- **Estimated effort:** M (2–3 days: integration into VoxelWorld + parameter tuning + border consistency)
- **Re-evaluation conditions:** If mainline chunk size changes (e.g., to 16³), re-run with CI benchmark. If GPU-based worldgen is added (compute shader), re-evaluate strategy complexity budget.

---

## 8. Sources

See `sources.md` for full list with annotations.

Key:
1. Minecraft 1.17 ore vein density functions — https://minecraft.wiki/w/Tutorials/Custom_ore_vein_generation
2. Minetest ore API — https://github.com/minetest/minetest/blob/master/doc/lua_api.md
3. Cubyz OreGenerator — https://github.com/Jai-A-2023/Cubyz/blob/main/src/world/gen/OreGenerator.zig
4. Nathan Reed — Procedural ore deposits — https://www.reedbeta.com/blog/procedural-ore-deposits/
5. Iridescence Perlin worms talk — https://www.youtube.com/watch?v=4O0gallXw1I
6. Minecraft noise router — https://github.com/gnembon/carpet-cm/blob/master/src/main/java/carpetcm/utils/CactusConfigs.java
7. No Man's Sky procedural generation (GDC 2015) — Innes, Sean — GDC Vault
8. Gonzalez & Patow (2023) — Procedural geological ore deposits — https://doi.org/10.1016/j.cag.2023.01.005

---

## 9. Mapping to ProjectV hot-path

**Hot-path segment:** VoxelWorld chunk material generation → resource deposit placement → mesh building. The deposit generation sits *between* geology finalization and mesh building.

**Differing from mainline:**
- Prototype uses synchronous CPU code; mainline would be parallel per chunk (job system).
- Prototype assumes 8³ single-material chunks; mainline Chunk uses 16³ with material 3D array.
- Component analysis is diagnostic-only; mainline does not need to compute components at generation time.

**Unmeasured:**
- Driver / scheduler overhead when parallelized across 32+ chunks
- Memory bandwidth cost of writing deposit bitmasks
- Interaction with subsequent meshing pass (seam-awareness could reduce mesh complexity near deposits)

**Hardware baseline:** [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — dev host `obvium`: AMD Ryzen 7 5700X (8C/16T), 32 GiB DDR4-3600, RTX 3060 Ti. CPU used for benchmark; single-threaded, no GPU involvement.
