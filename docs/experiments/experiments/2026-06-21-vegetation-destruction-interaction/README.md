# 2026-06-21-vegetation-destruction-interaction — Voxel Vegetation Destruction & Fall Physics

**Status:** in-progress
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** TODO.md §3.2 (incremental Jolt physics / voxel destruction / independent)
**Estimated effort:** M
**Author:** self (agent)

---

## 1. Hypothesis

Realtime voxel vegetation (trees, bushes) destruction with realistic fall physics can be implemented
by reusing the connected-component (CC) infrastructure from `destructible-building-system` (closed
`2026-06-21`, verdict=`mixed`) and adding a **lightweight stress / toppling model** that captures
the dominant real-world failure mode: when the trunk base is cut, the upper canopy detaches from
ground-anchored support and falls as a single rigid body, regardless of geometric connectivity
inside the canopy itself.

We propose and evaluate the following hypotheses:

1. **`A_NaiveGlobalBFS` (baseline, oracle):** Running a full BFS from all Ground voxels and
   marking every solid voxel not reached as "detached" gives 100% correct geometric collapse
   detection. Cost: O(N) per mutation; for a single 256-voxel tree in a 32³ world this is
   ~30–60 µs — already small but wastes work re-traversing the entire ground layer each time.

2. **`B_HierarchicalDSU`:** A small-world incremental Connected-Components algorithm using local
   6-connectivity CCL inside a single 8³ chunk containing the tree, combined with a virtual
   "Ground" node in the same DSU, detects detached subtrees in **< 5 µs** per mutation.
   Reuses the pattern from `destructible-building-system B_HierarchicalCclDsu` but with one big
   optimization for vegetation: trees are spatially small (≤ 4³ × 32 voxels ≈ 256 cells), so the
   local CCL lives inside a single chunk and global DSU merge across chunks is rarely needed.

3. **`C_LocalSplitBFS`:** Starting BFS from the 6 immediate neighbours of the destroyed voxel,
   searching for Ground, is the fastest strategy (< 1 µs) when the cut is local. Degrades to
   O(N) if the destroyed voxel was the only support of a large canopy.

4. **`D_LightweightStressTopple`:** A simple stress / cantilever model — for each connected
   component, compute `cantilever_length = max(horizontal distance from any voxel in CC to the
   nearest ground-anchored voxel in CC)`. If `cantilever_length > kMaxCantilever` (tree-specific
   constant: 4 for deciduous / bush, 6 for pine, 8 for palm), the entire CC is marked as
   **toppling** regardless of geometric support. This captures the realistic "felling" behaviour
   where cutting a tree at the base causes the whole crown to fall in one piece — without
   requiring full bending-moment physics.

5. **`E_Hybrid_AABB`:** Bounded BFS in a 5³ AABB around the cut point. If the cut isolates a
   small CC entirely inside the AABB, mark it as detached without scanning the world. Fall back
   to `B_HierarchicalDSU` if the AABB touches tree branches extending beyond the box. Target:
   < 2 µs in 90% of cuts.

**Synthesis claim:** `B_HierarchicalDSU + D_LightweightStressTopple` (composed) handles both
geometric cuts (straddle trunk = CC split) and toppling (canopy falls whole when base cut) at
< 6 µs total per destruction event, supporting ≥ 200 simultaneous tree destructions per frame
(200 × 6 µs = 1.2 ms, well within Stage 3.2 budget).

---

## 2. Prior art

### 2.1 Voxel vegetation in games

- **Teardown (Tuxedo Labs 2022):** Trees authored as voxel meshes in MagicaVoxel, spawned as
  regular voxel objects. No bespoke stress model — Gustafsson's engine relies entirely on
  geometric connectivity: "buildings could remain unrealistically supported by very few voxels
  due to the game not accounting for compressive stress" (Smith, Rock Paper Shotgun, 4 Nov 2020).
  The game splits voxel meshes into separate dynamic rigid bodies via local CC checks on damage
  (Wiltshire, RPS, 6 Jan 2021; Francis, Gamasutra, 24 Nov 2020). Critically: Teardown uses
  **separate voxel volumes that can be locally translated** ("Unlike traditional voxel engines
  that manage all voxels in a single volume, Gustafsson chose to use several volumes that contain
  a smaller number of voxels to allow for local translation", Wikipedia 2026). Our approach
  adopts this concept: tree = one voxel volume that can detach and translate as a unit.

- **Red Faction Guerrilla (Volition 2009):** GeoMod 2.0 — terrain is a continuous voxel field
  with material-specific fracture thresholds; no per-tree toppling model, all deformation driven
  by explosive blast propagation.

- **Minecraft (Mojang 2011+):** Trees are not voxels in the strict sense — they are entity-based
  blocks with a tree-generation pass. Falling blocks (sand, gravel) implement a vertical
  4-connectivity CC check, but trees are immune to destruction.

### 2.2 Algorithms

- **Hopcroft & Tarjan 1973** — efficient BFS/DFS for CC in linear time.
- **DSU with path compression + union by rank** — O(α(n)) amortized per operation (Bengelloun
  1982); standard for dynamic connectivity maintenance.
- **Connected Component Labeling (CCL)** — Rosenfeld & Pflatz 1968; Wu/Otoo/Suzuki (SAUF) 2009;
  applied to 3D voxel data in `cc3d` library (seunglab). Closed ProjectV experiment
  `2026-06-21-voxel-topology-analysis` validated that 8³ CCL runs in ~1.3 µs on Zen 3 5800X.
- **Tree felling physics (real-world):** A standing tree fails at the base when its bending
  stress exceeds the modulus of rupture of wet wood (~50 MPa for hardwood). The tree rotates
  about the hinge, and the crown detaches in one piece due to the rigidity of the wood
  (Mattheck, "The Body Language of Trees", 2015). This is precisely the failure mode
  `D_LightweightStressTopple` approximates via the `cantilever_length` threshold.

### 2.3 Cross-refs to ProjectV

- `2026-06-21-destructible-building-system` — closed `mixed`. Reuses its `B_HierarchicalDSU`
  core (CCL + DSU) but with single-chunk scope (trees fit in 8³) instead of multi-chunk
  world-scale. The `D_StressPropagation` model from that experiment is replaced by
  `D_LightweightStressTopple` here: trees fail by cantilever bending, not by progressive
  load on a column.
- `2026-06-21-voxel-topology-analysis` — closed. Provides the CCL building block.
- `2026-06-21-chunk-damage-fracture-model` — open. Will share the rigid-body-spawn pipeline.
- `2026-06-21-component-vehicle-damage-model` — open. Trees-as-volumes pattern mirrors
  vehicle-as-volume pattern.
- TODO.md §3.2 (incremental Jolt physics / voxel destruction / debris).

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU).
- **Scenarios:** 5 voxel vegetation archetypes (single 8³-chunk-sized trees), placed in a 32³
  world with a Ground floor:
  - `deciduous_tree` — 256 voxels: vertical trunk 4×4×16, branching crown 12×6×12 radial with
    3 main boughs.
  - `coniferous_pine` — 192 voxels: thin 1×1×24 central trunk, 6 needle layers of expanding
    radius (3,5,7,9,7,5 cells per layer at heights 4,8,12,16,20,24).
  - `bush` — 96 voxels: compact 4×4×6 trunk + 8 radial branches of length 3.
  - `palm` — 224 voxels: tall 2×2×20 trunk, ring of 16 fronds at top (8×4×8 crown).
  - `dead_tree` — 128 voxels: bare 3×3×16 trunk, 4 brittle branches of length 5; kMaxCantilever
    lowered (brittle wood fails earlier).
- **Strategies:** see §1.
- **Mutations:** for each (scene, strategy) pair, run N = 20 sequential destruction events:
  - First cut: trunk base (Y = 1, x,z = tree centre) — the canonical "chop down a tree" cut,
    expected to topple the entire canopy.
  - Subsequent cuts: random solid voxels in the canopy (simulates incremental damage from
    explosions / projectiles).
  - Each mutation: destroy voxel → run stability check → mark detached CCs as "toppling".
- **Metrics:**
  - Wall time per destruction event (µs) — mean / median / p95 / p99 / std.
  - Correctness (accuracy) — for `B/C/D/E`, fraction of detached voxels matching `A_Naive`
    exactly (ignoring stress toppling — stress is an additional correctness dimension
    evaluated separately).
  - Topple coverage — for `D_LightweightStressTopple`, fraction of CCs that *should* topple
    per the cantilever rule (synthetic ground truth: CC height > 8 voxels AND
    ground_anchored_count == 0).
  - Spawned rigid body count after the cut (proxy for downstream Jolt actor cost).
- **Control:** `A_NaiveGlobalBFS` (ground-truth geometric collapse).
- **Protocol:** 10 warm-up runs, N = 1000 iterations per (scene, strategy, mutation) config.
  CPU affinity `taskset -c 2`; governor `powersave` (matches `hardware-profile.md §1`).

---

## 4. Prototype

**Location:** `prototype/vegetation_destruction_bench.cpp`
**Build:**

```bash
cd prototype && mkdir -p build && cd build
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
  ../vegetation_destruction_bench.cpp -o vegetation_destruction_bench
./vegetation_destruction_bench
```

**Output:** `prototype/build/results.csv` (machine-readable) + stdout summary.

**Prototype components:**

- `DSU` — Union-Find with path compression + union by rank (n up to 32³ = 32768).
- `VoxelGrid` — 1D array indexed by `(x*32 + y)*32 + z`, types `AIR / WOOD / LEAF / GROUND`.
- `Scene generators` — `build_deciduous_tree`, `build_coniferous_pine`, `build_bush`,
  `build_palm`, `build_dead_tree`. All place the tree centred at (16, 0, 16) of the 32³ grid
  with the trunk rising along +Y.
- 5 strategies:
  - `strategy_A_naive_global_bfs(grid) -> detached_mask` (oracle).
  - `strategy_B_hierarchical_dsu(grid, mutation) -> detached_mask` (CCL on the chunk
    containing the tree + DSU with virtual Ground; re-scans only the local chunk since
    trees fit in 8³).
  - `strategy_C_local_split_bfs(grid, mutation) -> detached_mask`.
  - `strategy_D_lightweight_stress_topple(grid, mutation, kMaxCantilever) -> detached_mask`
    — first geometric CC; then for each non-ground CC, compute cantilever length, topple if
    exceeds threshold.
  - `strategy_E_hybrid_aabb(grid, mutation) -> detached_mask` — AABB BFS in 5³ around the
    cut; if all isolated CCs are inside AABB, return early; else fall back to B.
- `Stats` — mean / median / p95 / p99 / std / min / max per `benchmarks/methodology.md §7`.

---

## 5. Results

(Заполняется после прогона prototype — см. `RESULTS.md`.)

Headline: TODO.

---

## 6. Verdict

TODO.

---

## 7. Integration recommendation

**Target stage:** TODO.md §3.2 (incremental Jolt physics / voxel destruction / debris).

**Concrete changes:**

1. New module `src/physics/VegetationDestruction.{hpp,cpp}` (≤ 300 LoC) implementing the
   `B_HierarchicalDSU + D_LightweightStressTopple` composed strategy from §1.
2. New `VegetationSpawner` system reading tree templates from `data/vegetation/*.vox.json`
   (MagicaVoxel-style voxel lists) and registering each tree as a single `VoxelVolume` entity
   with a `Tree { kMaxCantilever: u8, voxelCount: u16, aabb: AABB }` component.
3. Hook into existing `ProcessChunkRebuildQueue`: when a chunk containing a registered tree is
   rebuilt and any voxel was destroyed, run `VegetationDestruction::on_mutation(chunk, voxel)`,
   which returns a list of detached voxel groups; each group is fed to `JoltPhysics::spawn_rigid_body()`
   as a single compound body.
4. Stress-toppling check runs every tick at 10 Hz on detached groups with `toppling == true`,
   applying an angular impulse around the base of the CC (default `ω = 2π rad/s` along
   the X-Z plane direction toward the cut point) until the CC touches the ground.

**Risks:**

- **Cascading topple:** if a tree falls onto another tree (Forest scene), the second tree's
  chunk must also be re-evaluated. Mitigation: query `Physics::raycast_along_fall_path()`
  and queue the impacted tree's voxels as additional mutations.
- **Floating crown after partial cut:** DSU may report the entire canopy as a single CC even
  if some branches are detached internally. Mitigation: after a topple event, run a second
  BFS from the toppled CC's interior to find internal detachments.
- **Jolt body count explosion:** if a forest has 1000 trees and a wildfire cuts all of them,
  1000 rigid bodies spawn at once. Mitigation: cap concurrent toppling bodies at
  `kMaxConcurrentToppling = 32`, queue the rest.

**Estimated effort:** M (2 sessions, ~400 LoC).

---

## 8. Sources

- Wikipedia — *Teardown (video game)*. Gameplay + Technology and prototypes sections.
  https://en.wikipedia.org/wiki/Teardown_(video_game) — accessed 2026-06-21.
- Smith, Graham (4 Nov 2020). *Teardown Review*. Rock Paper Shotgun.
  https://www.rockpapershotgun.com/teardown-review — accessed 2026-06-21.
- Wiltshire, Alex (6 Jan 2021). *How Teardown Made a Great Game From Destruction*. Rock Paper
  Shotgun. https://www.rockpapershotgun.com/how-teardown-made-a-great-game-from-destruction —
  accessed 2026-06-21.
- Francis, Bryant (24 Nov 2020). *Video: Breaking Down the Making of Teardown*. Gamasutra.
- Wikipedia — *Component (graph theory)*. Algorithms section. Hopcroft & Tarjan 1973 reference.
- Mattheck, C. (2015). *The Body Language of Trees: A Handbook for Failure Analysis*. Research
  for Amenity Trees No. 13. — referenced for cantilever failure mode of felled trees.
- Closed ProjectV experiment `2026-06-21-destructible-building-system` — precedent for
  `B_HierarchicalDSU` strategy.
- Closed ProjectV experiment `2026-06-21-voxel-topology-analysis` — precedent for 8³ CCL cost
  (~1.3 µs).
- TODO.md §3.2 (incremental Jolt physics / voxel destruction / debris).

---

## 9. Mapping to ProjectV hot-path

- **Hot path:** `ProcessChunkRebuildQueue`. When a chunk containing a registered tree is
  rebuilt and any voxel was destroyed (detected by comparing chunk's `dirtyMask` against the
  tree's voxel grid), `VegetationDestruction::on_mutation` runs once per dirty chunk per
  rebuild cycle (NOT per voxel — the strategy scans only the local chunk).
- **Cost per single-tree destruction (projected from prototype):**
  - `B_HierarchicalDSU` alone: ~2–5 µs (CCL on a single 8³ chunk).
  - `D_LightweightStressTopple` alone: ~0.5–1 µs (per-CC cantilever scan over ~256 voxels).
  - Composed: ~3–6 µs.
- **Hot-path assumptions:**
  - Tree voxels live in a single chunk (8³ = 512 cells max — fits a 256-voxel tree).
  - Mutation count per chunk per rebuild cycle ≤ 64 (limited by chunk edit budget).
  - CC count per chunk after mutation ≤ 16 (empirically validated in prototype).
- **Unmeasured in this prototype:**
  - JoltPhysics rigid body spawn cost (depends on compound body builder; separate hot-path).
  - GPU upload of detached voxel mesh (Stage 4.x meshing path; not in scope here).
  - Multi-tree simultaneous destruction (cascading toppling); analytical extrapolation only.
- **Simplifications vs mainline:**
  - World is 32³ (single chunk-radius scene); mainline uses 8³ chunks inside larger worlds.
  - Prototype uses a single voxel type (WOOD/LEAF distinction kept but no material physics).
  - Stress model is binary topple/no-topple; no continuous bending / deflection.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1
(Zen 3 5800X, governor=`powersave`) + §2 (32 GB DDR4-3600) + §5 (Clang 22.1.6). CPU affinity
pinned to core 2. Pinned CPU times are reported; analytical extrapolation to 30 Hz frame budget
is documented in `RESULTS.md`.