# 2026-06-22-voxel-navmesh-graph-generation — Voxel-to-Navmesh / Navigation Graph Generation

> **Self-invented topic per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй».
> **§13.7 sentinel clean** — `rg "navmesh|recast|detour|navigation.*graph|waypoint"` over `INDEX.md` + `experiments/` + `backlog.md` + `backlog_closed.md` = 0 dedicated experiments.
> **First dedicated voxel→navmesh / navigation-graph generation axis** в 170+ closed experiments.

## 1. Hypothesis

5-стратегийное сравнение ∈ {A_NaiveVoxelGrid_3DBool, B_WalkableHeightfield_2D, C_RecastStyle_PolyMeshContour, D_VoxelSurfaceGraph, E_Hybrid3D_RegionGraph} даст:

- **H1 cost:** все 5 strategies <100 µs/chunk generation time (= 0.3% of 30 Hz budget для 4096 chunks at 0.1 Hz update = 0.41% = well within 5% threshold).
- **H2 quality:** cross-chunk seamless graph для 2.5D (stairs/ramps/jumps) + 3D (multi-level) navigation; **C (Recast) = reference quality**; **E = most complete** (doorway detection + jump links + multi-floor support).
- **H3 storage:** <2 KiB/chunk для всех strategies; A=64 B/chunk smallest (1 bit/voxel), C=1-2 KiB/chunk largest (polygon mesh), D=32-128 B/chunk, E=0.5-1.5 KiB/chunk.

**Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold:** ожидаем cross-over A→D или A→E (Recast per chunk is O(n²) for n walkable cells; sparse graph O(k) for k = surface features = 10-100× reduction).

**Voxel chunk size:** 8³ per `agent/knowledge.md` (chunk_size=8 voxels, chunk_volume=512 voxels/chunk, 8 chunks = 32m world axis, typical chunk = 8m × 8m × 8m).

**Navigation graph usage:**
- **Pathfinding input:** `flow-field-pathfinding-10k-units` [closed yes, BFS 19.8 µs at 64²] uses 2D grid per Y-level — assumes navmesh. This experiment = **producer**.
- **AI routing:** `squad-fire-team-command` [closed], `urban-combat-tactics-ai` [closed, room graph], `medical-evacuation-chain` [open], `drone-swarm-ai` [open, h Tier 2], `formation-flight-wingman` [open] — all consume navmesh.
- **Cover query:** `cover-system-terrain-adaptive` [closed mixed] uses direct voxel query (avoids navmesh) — **orth** axis.

## 2. Prior art (web-research)

> **Status:** in progress (Phase 1).

**Canonical references (Tier 1):**
- **Recast & Detour** (Mikko Mononen, GitHub mononen/recastnavigation **8.6k★**, BSD-licensed, used in Unity/Unreal/Source Engine/3D engine AAA production). Industry-standard 5-step pipeline:
  1. **Voxelization:** rasterize input geometry (triangle mesh) into voxel cells.
  2. **Filter:** mark walkable cells (slope, height, special surfaces).
  3. **Regions:** build connected regions (BFS/flood-fill on walkable cells).
  4. **Contours:** generate simple polygons from region boundaries (ear clipping + simplification).
  5. **Poly mesh:** triangulate contours → final convex polygon mesh.
- **Detour** = pathfinding layer on top of Recast poly mesh: A*, JPS (jump point search), NavMeshQuery API with `findStraightPath` / `findRandomPoint` / `findPolysAroundCircle` / `findNearestPoly` / `slicedFindPath`.
- **Wikipedia "Navigation mesh"** + **Wikipedia "Recast"** + **Wikipedia "Detour"** + Mononen 2009 GDC presentation (if findable).
- **HatLink/VoxelNavigation** (Unity Asset Store) — voxel-based 3D pathfinding without traditional navmesh. Direct production reference for voxel-native alternative.

**Academic references (Tier 1):**
- **van Toll, Pettre, Raupp Musse 2012** "Navigation Meshes for Realistic Multi-Layered Environments" (IEEE TVCG) — multi-floor navmesh generation.
- **van Toll, Cook, Geraerts 2011** "A navigation mesh based on vertical visibility" (Motion in Games) — vertical visibility for multi-floor support.
- **Pettre, Laumond, Thalmann 2005** "A navigation graph for real-time crowd animation on multilayered environments" (IEEE TVCG) — early multi-layer navmesh.
- **Kallmann 2010** "Navigation Planning with Realistic Walking" (JVCA / Motion in Games) — jump links, climbing, dynamic obstacles.
- **Marzotto, Zorzi, Burgard 2022** "Path Planning in Virtual Worlds" (survey).
- **Seidel 2012** "Generating Navigation Meshes from Voxel Data" (M.S. thesis, generates navmesh from Minecraft-like voxel data).
- **Botea, Müller, Schaeffer 2004** "Near Optimal Hierarchical Path-Finding" (HPA*) — hierarchical pathfinding for sparse graph optimization.

**Production engine references (Tier 2):**
- **Unreal Engine 5 NavMesh** (Recast-based, with NavMeshModifierVolume, dynamic obstacles, NavLink for jump links, NavLink for ladders, NavMesh generation in editor + runtime).
- **Unity NavMeshComponent** (Recast-based, runtime NavMeshSurface baking, NavMeshAgent, NavMeshObstacle, NavMeshLink for jumps).
- **Godot NavigationServer** (Recast-based, NavigationRegion + NavigationObstacle + NavigationLink).
- **Source Engine Navigation Meshes** (Valve, Mononen Recast + custom, used in Half-Life 2, CS:GO, L4D).
- **CryEngine AI/Navigation** (Crytek, custom navmesh + navgraph + cover system + tactical waypoints).
- **Frostbite Navigation** (EA DICE, 2006-2020 GDC presentations: voxel-based navmesh generation, region decomposition, contour, polygon mesh).
- **Killzone 2 Navigation** (Verweij 2009 GDC, voxel-based navmesh with cover points).
- **Halo Wars Navigation** (Bungie, hybrid navmesh + waypoint graph + tactical considerations).
- **Killzone 3 Tactical AI** (van der Sterren 2011 GDC AI Summit, NavMesh + Squad tactics).
- **Wing Commander / Supreme Commander navigation** (Gas Powered Games, custom navgraph + influence maps).

**Local collision avoidance (complement to navmesh):**
- **RVO2 / ORCA** (Unreal Engine 4 Crowd Manager paper 2014, Reciprocal n-Body Collision Avoidance).
- **OpenSteer** (Reynolds 2004, canonical steering behaviors library).

**Alternative approaches (orth to navmesh):**
- **PowerMesh** (H楼上 Xbox 360 multi-core navmesh).
- **Path graph / waypoint graph** (Valve AI Director, simpler than navmesh, hand-authored waypoints).
- **Voxel-based pathfinding without navmesh** (HatLink/VoxelNavigation, 3D A* on voxel grid).

## 3. Method

**5 strategies (orth on 2.5D/3D support, storage, and update cost axes):**

### A_NaiveVoxelGrid_3DBool (baseline = 1 bit/voxel walkable mask)

Direct 3D bool grid per chunk. 8³ = 512 bits = **64 B/chunk**.
- Walkable check: 1 BFS step on all 6 neighbors per voxel.
- Pathfinding cost: 3D A* directly on voxel grid (8³ × 3D = expensive).
- Storage: O(512) bits = 64 B/chunk.
- Quality: low (no surface features, no doorways, no jump links).
- Pros: trivially correct, no information loss.
- Cons: massive pathfinding cost (3D A* on 512 cells × 6 neighbors = 3072 per query).
- Update cost: 64 B regenerated per chunk edit = trivial.

### B_WalkableHeightfield_2D (1 byte/XZ-column, no 3D)

Per-XZ-column heightfield (top walkable Y per column). 8×8 columns × 1 byte = **64 B/chunk**.
- Walkable check: Y-1 (drop-down) or Y+1 (step-up) per column.
- Pathfinding cost: 2D A* on 8×8 = 64 cells per chunk × 3D = 192 neighbors per query.
- Storage: O(64) bytes = 64 B/chunk.
- Quality: medium (no ramp/stair surface info, no doorway, no overhang).
- Pros: compact, fast pathfinding.
- Cons: 2.5D only (no multi-floor), no ramp slope.
- Update cost: O(64) = 64 B per chunk edit.

### C_RecastStyle_PolyMeshContour (industry-standard)

5-step Recast pipeline: voxelize → filter → regions → contours → poly mesh.
- Voxelization: 8³ = 512 cells. Cell size = 0.1m.
- Filter: walkable slope < 45° (assume agent slope limit), walkable height ≥ 1 (headroom), walkable climb ≤ 0.4m.
- Regions: BFS flood-fill on walkable cells.
- Contours: boundary walk + Douglas-Peucker simplification (epsilon = 0.1m).
- Poly mesh: triangle fan from contour centroid (no ear clipping, simplified for prototype).
- Storage: variable, ~1-2 KiB/chunk for typical 64×64m area with sparse contours.
- Quality: high (industry-standard 2.5D navmesh with contour simplification).
- Pros: well-tested, production-grade.
- Cons: expensive (O(n²) per chunk for n = walkable cells = ~10ms for 64×64 chunk).
- Update cost: full chunk regenerate, expensive.

### D_VoxelSurfaceGraph (sparse adjacency graph, ~32-128 B/chunk)

Center points of walkable surfaces + edges between adjacent surfaces.
- Center: per Y-level, find local maxima of walkable area (kernel 3×3).
- Edge: 2 walkable centers within ≤2 voxel distance + same Y level = bidirectional edge.
- Storage: O(k) where k = number of surface features = ~10-50 per chunk = 32-128 B/chunk.
- Quality: medium-high (capture open terrain + room connectivity well, struggles with dense urban).
- Pros: very compact, fast pathfinding (sparse A* on k nodes = O(k log k) = ~100 ns per query).
- Cons: less precise than navmesh, no surface area information.
- Update cost: O(k) per chunk edit = ~50-200 ns.

### E_Hybrid3D_RegionGraph (region + doorway/jump-link + cross-chunk adj)

Most complete: per-Y-level walkable regions + per-region doors (transition to adjacent Y-level via step-up, step-down, jump, ladder, or free-space if 3D enabled).
- Walkable region: connected component of walkable cells per Y-level.
- Door: per pair of adjacent Y-levels in same chunk or neighbor chunk, identify cells where 1-2 voxel vertical gap = step-up/down (≤0.4m), or 3-4 voxel gap = jump (only if E_HasJumpLinks enabled), or 1-3 voxel gap with ladder metadata = ladder.
- Cross-chunk: per chunk face, identify boundary edges (perimeter walkable cells) and link to neighbor chunk's boundary.
- Storage: O(r + d + c) where r = regions, d = doors, c = cross-chunk links = ~50-200 per chunk = 0.5-1.5 KiB/chunk.
- Quality: highest (supports stairs, ramps, jumps, ladders, cross-chunk seam, optional 3D multi-floor via "free" door type).
- Pros: most complete, supports all standard navmesh features.
- Cons: more complex implementation, more storage than A/B/D.
- Update cost: O(r + d) per chunk edit = ~200-1000 ns.

**5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.**

**5 scenes:**
1. **open_terrain** — flat 8×8 chunk, 100% walkable (no obstacles) — baseline.
2. **sparse_rocks** — flat 8×8 chunk with 5-20 isolated rock obstacles (1-2 voxel each).
3. **dense_urban** — 8×8 chunk with 50% building coverage, multi-story (Y-levels 0-3), doorways at 1-voxel-wide passages.
4. **stairs_ramp** — 8×8 chunk with 1 spiral staircase (4 levels, 2-voxel-wide steps) + 1 ramp (slope 30°, 8 voxels long) — tests stair/ramp detection.
5. **destroyed_building** — 8×8 chunk with partially destroyed building (60% mass, 20% debris, 5 doorway openings) — tests post-destruction navmesh rebuild.

**Per-scene measurements:**
- Generation time per chunk (µs/chunk).
- Storage per chunk (bytes/chunk).
- Pathfinding query time (ns/query, 1000 random source-target pairs).
- Quality metrics: graph connectivity (1 = fully connected, 0 = disconnected), waypoint count, doorway detection accuracy, ramp slope coverage.

## 4. Prototype

> **Status:** in progress (Phase 2 — pending).

Standalone C++26 CPU prototype `prototype/navmesh_bench.cpp` (planned ~600-800 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`).

Build directory: `prototype/build/` (isolated from mainline, per `AGENTS.md §1`).

**Build:**
```bash
cd docs/experiments/experiments/2026-06-22-voxel-navmesh-graph-generation/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -o build/navmesh_bench navmesh_bench.cpp
./build/navmesh_bench
```

**Output:**
- `prototype/build/results.csv` (126 rows = 1 header + 125 data)
- `prototype/build/summary_means.csv` (5 rows, per-strategy summary)
- `prototype/build/run.log` (timing + warmup log)

**Provisional measurement plan:** 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements; wall time < 30 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.

## 5. Results

> **Status:** pending (Phase 3 — after prototype complete).

## 6. Verdict

> **Status:** pending (Phase 3 — after prototype complete).

## 7. Integration recommendation

> **Status:** pending (Phase 3 — after prototype complete).

Per `agent/knowledge.md §30.4` precedent (3-step mainline migration):
- Step 1 (XS-S, ~80-150 LoC): `src/voxel/NavmeshChunk.{hpp,cpp}` + `NavmeshStrategy` enum + `PROJECTV_NAVMESH=OFF|NaiveGrid|HeightField|RecastStyle|SurfaceGraph|Hybrid3D` env gate (default `Hybrid3D`).
- Step 2 (M, ~400-600 LoC): per-strategy implementation + Flecs `NavmeshChunkComponent` + `NavmeshGenerationSystem` per 0.1-1 Hz tick + integration with `flow-field-pathfinding-10k-units` (consume navmesh as input) + `voxel-chunk-streaming-pipeline` (add/remove chunks).
- Step 3 (S, ~150-200 LoC): `ProjectVNavmeshTests` (5 unit + 5 integration) + Tracy plot "Navmesh Gen" + "Navmesh Query" + "Navmesh Storage" + `PROJECTV_NAVMESH_UPDATE_HZ=0.5` env (default 0.5 Hz = once per 2 sec) + `PROJECTV_NAVMESH_LOD=DETAIL|MEDIUM|COARSE` env (default DETAIL = per 8³ chunk; MEDIUM = per 16³; COARSE = per 32³).

**Deferred** до Stage 4.1/6+ per `agent/workspace.md §2` line 36 operator 8x planning decision.

## 8. Sources

> **Status:** pending (Phase 1 — web-research in progress).

Список появится после Phase 1 web-research.

---

**Cross-refs:**
- `agent/knowledge.md` §30.4 (3-step migration precedent)
- `agent/workspace.md` §2 (operator 8x planning, Stage 4.1/6+ deferral)
- `hardware-profile.md` §1 (Zen 3 5800X dev host, governor=`powersave`)
- `benchmarks/methodology.md` (measurement protocol)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold)
- `experiments/2026-06-21-flow-field-pathfinding-10k-units/README.md` (downstream consumer)
- `experiments/2026-06-21-voxel-topology-analysis/README.md` (upstream building block)
- `experiments/2026-06-21-cover-system-terrain-adaptive/sources.md` (orth axis: direct cover from voxel)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/README.md` (orth axis: physics collider)
- `experiments/2026-06-21-flood-fill-visgraph-culling/README.md` (BFS methodology precedent)
- `experiments/2026-06-22-urban-combat-tactics-ai/README.md` (downstream consumer)
- `TODO.md` (per chunk_size=8, cross-chunk connectivity precedent)
