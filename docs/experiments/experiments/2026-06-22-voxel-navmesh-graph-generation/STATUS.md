# 2026-06-22-voxel-navmesh-graph-generation — Status

**Agent:** self.
**Started:** 2026-06-22.
**Phase:** 4 closed (single session, ~2h: claim → web-research → prototype → benchmark → RESULTS.md → close).
**Blocker:** нет.
**Verdict:** `concluded-verdict-yes` for `B_WalkableHeightfield_2D` ⭐ as universal recommended default. `concluded-verdict-mixed` per strategy for C/D/E (use case specific). `concluded-verdict-no` for A.

**Sentinel §13.7 clean** — `rg "navmesh|recast|detour|navigation.*graph|waypoint"` over `INDEX.md` + `experiments/` + `backlog.md` + `backlog_closed.md` = 0 dedicated experiments. Only cross-refs:
- `voxel-topology-analysis/README.md:189` — "AI pathfinding: cross-chunk connectivity graph for A*" (potential use, NOT dedicated)
- `cover-system-terrain-adaptive/sources.md:24` — "HatLink/VoxelNavigation" as web reference
- `flow-field-pathfinding-10k-units/{README,STATUS}.md` — assumes navmesh exists, does NOT generate
- `urban-combat-tactics-ai/sources.md:30` — UE5 NavMesh mention as production reference
- `ls experiments/2026-06-22-*` = 43 folders, 0 navmesh-related.

**First dedicated voxel-to-navmesh / navigation-graph generation axis** в 170+ closed experiments; cross-cuts Stage 2.x culling (visible-area cull via navmesh) + Stage 3.x interaction (doorway/jump-link detection) + Stage 4.1/4.2 world gen (navmesh on procedural terrain) + Stage 4.2 chunk 1 meshing (blocker-fill logic) + Stage 5.1 visibility (replaces chunk-level visibility check) + Stage 6+ military sandbox [AI navigation per Warno/HOI4/SupCom/BellumGare/Squad precedent] + Tier 2 AI (pathfinding graph input).

**Hypothesis (one-line):** 5-стратегийное сравнение ∈ {A_NaiveVoxelGrid_3DBool (1 bit/voxel), B_WalkableHeightfield_2D (1 byte/XZ-column, no 3D), C_RecastStyle_PolyMeshContour (industry-standard voxelize→region→contour→poly), D_VoxelSurfaceGraph (sparse adjacency graph, ~32-128 B/chunk), E_Hybrid3D_RegionGraph (region + doorway/jump-link + cross-chunk adj)} даст:
- **H1 cost:** все 5 strategies <100 µs/chunk generation time (= 0.3% of 30 Hz budget для 4096 chunks at 0.1 Hz update = 0.41% = well within 5% threshold)
- **H2 quality:** cross-chunk seamless graph для 2.5D (stairs/ramps/jumps) + 3D (multi-level) navigation; C (Recast) reference quality; E most complete (doorway detection + jump links)
- **H3 storage:** <2 KiB/chunk для всех strategies; A=64 B/chunk smallest (1 bit/voxel), C=1-2 KiB/chunk largest (polygon mesh), D=32-128 B/chunk, E=0.5-1.5 KiB/chunk

**5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** cross-over A→D or A→E expected (Recast per chunk is O(n²) for n walkable cells; sparse graph O(k) for k = surface features = 10-100× reduction).

**Scope (paths):**
- `docs/experiments/experiments/2026-06-22-voxel-navmesh-graph-generation/{README.md,STATUS.md,sources.md,RESULTS.md}`
- `docs/experiments/experiments/2026-06-22-voxel-navmesh-graph-generation/prototype/` (standalone C++26 CPU prototype + build/)
- `docs/experiments/INDEX.md` (§5 Active → §6 Recent при закрытии)
- `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)

**Differentiation vs closed/in-progress experiments:**
- `2026-06-21-flow-field-pathfinding-10k-units` [closed yes, GPU compute steering] = **consumes** navmesh as input (assumes 2D grid per Y-level exists); does **NOT generate**. This experiment = **producer** (chunk→navmesh).
- `2026-06-21-greedy-physics-meshing-cpu` [closed yes, F_TwoPass 35× reduction] = **physics collider** (JPH StaticCompoundShape); **orth** axis (physics shape vs navigation graph).
- `2026-06-21-voxel-topology-analysis` [closed yes, 2.73 µs CCL] = **topology building block** (CCL, overhang, exposed classify); this experiment = **navigation downstream consumer** (CCL on walkable cells = room detection).
- `2026-06-21-cover-system-terrain-adaptive` [closed mixed] = **avoid navmesh** (extract cover directly from voxel geometry). **orth** axis (cover = static, AI direct query vs navmesh = persistent, AI pathfinding input).
- `2026-06-21-multi-resolution-collision-broadphase` [closed mixed, D_QuadTree 250-1300×] = **physics broadphase** spatial query; **orth** axis (physics vs navigation).
- `2026-06-22-urban-combat-tactics-ai` [closed, uses room graph + flow field] = **consumes** room graph; this experiment = **producer**.
- `2026-06-22-missile-guidance-laws-simulation` [closed, APN guidance] = **orth** (missile guidance ≠ AI navigation).
- `2026-06-22-squad-fire-team-command` [closed, squad BT] = **consumes** AI pathfinding (BT calls into pathfinding system); this = **producer**.
- `2026-06-22-medical-evacuation-chain` [open, MEDEVAC] = **consumes** AI pathfinding (MEDEVAC vehicles route via navmesh); this = **producer**.
- `2026-06-22-tech-tree-research-system` [closed, Tier 3 econ] = **orth** (no AI pathfinding).
- `2026-06-22-trench-fortification-construction` [closed, Tier 1 Phys] = **complementary** (trench = navmesh-blocking wall, triggers navmesh rebuild for affected chunks).
- `2026-06-22-voxel-material-weathering-surface-aging` [closed, Stage 4/6] = **orth** (visual material change, no navmesh effect).
- `2026-06-22-bridge-building-repair` [closed, Tier 1 Phys] = **complementary** (bridge = new navmesh, completes blocked path = incremental navmesh update).
- `2026-06-22-voxel-water-flow-ca` [closed, 3D CA water] = **complementary** (water = navmesh-blocking OR navmesh-allowing (amphibious units)).
- `2026-06-21-flood-fill-visgraph-culling` [closed yes, BFS traversal] = **complementary** (flood-fill = room detection pattern, BFS methodology).
- `2026-06-22-voxel-chunk-impostor-far-lod` [closed, far-LOD impostor] = **orth** (visual LOD, not AI).
- `2026-06-22-ddgi-probe-field-voxel-gi` [closed yes, Stage 5.5 DDGI] = **orth** (lighting, not AI).
- **0 of 170+ closed experiments cover voxel→navmesh generation specifically** — first dedicated axis.

**Cross-axis:** **orth** ко всем 18 in-progress parallel на `2026-06-22` (verified §13.7 sentinel + `ls experiments/`); **complementary** к closed:
- `flow-field-pathfinding-10k-units` [yes, **downstream consumer** — flow field generated from navmesh]
- `urban-combat-tactics-ai` [closed, **downstream consumer** — room graph = navmesh-derived subgraph]
- `squad-fire-team-command` [closed, **downstream consumer** — squad BT path calls]
- `medical-evacuation-chain` [open, **downstream consumer** — MEDEVAC routes via navmesh]
- `voxel-topology-analysis` [yes, **upstream** — CCL on walkable cells = room detection = room graph = navmesh component]
- `flood-fill-visgraph-culling` [yes, **methodology** — BFS for room connectivity]
- `cover-system-terrain-adaptive` [mixed, **orth axis** — cover direct from voxel = navmesh alternative for static queries]
- `greedy-physics-meshing-cpu` [yes, **orth axis** — physics collider vs navmesh]
- `ecs-1m-entities-bottleneck` [yes, **registry host** — Flecs stores navmesh entities]
- `bridge-building-repair` [closed, **incremental update trigger** — new bridge = add navmesh edge]
- `voxel-water-flow-ca` [closed, **incremental update trigger** — water level change = walkable area change]
- `trench-fortification-construction` [closed, **incremental update trigger** — trench = navmesh blocker]
- `voxel-chunk-streaming-pipeline` [closed yes, **lifecycle** — chunk load/unload = navmesh add/remove]
- `destructible-building-system` [closed mixed, **incremental update trigger** — destruction = navmesh change]
- `chunk-damage-fracture-model` [closed mixed, **incremental update trigger** — fracture = navmesh change]
- `vegetation-destruction-interaction` [closed yes, **incremental update trigger** — tree topple = navmesh change]
- `explosion-crater-terrain-deformation` [closed yes, **incremental update trigger** — crater = walkable area change]
- `after-action-report` [closed, **downstream consumer** — replay shows AI navigation paths]
- `lockstep-state-sync-hybrid-netcode` [closed mixed, **lockstep** — navmesh must be deterministic for lockstep]
- `mesh-shader-mega-instancing` [mixed, **visual host** — render navmesh edges for editor mode]
- `voxel-asset-template-catalog` [closed yes, **data host** — navmesh spec catalog]
- `data-driven-vehicle-weapon-definitions` [closed mixed, **data host** — vehicle navigation profile = per-vehicle navmesh queries]

**Prerequisite** для open:
- `drone-swarm-ai` [h Tier 2, swarm tactics — requires navmesh]
- `formation-flight-wingman` [m Tier 2, wingman formation — requires navmesh]
- `flocking-wildlife-ambient` [m Tier 5.x, animal herds — requires navmesh]
- `battlefield-npc-command` [m Tier 2, NPC player-issued orders — requires navmesh]
- `siege-assault-coordination-ai` [concept, siege AI — requires navmesh for assault path]
- `urban-combat-tactics-ai-extended` [follow-up, room-to-room AI — requires navmesh with doorway detection]

**Web-research next:** Recast & Detour (Mikko Mononen, canonical open-source navmesh, used in Unity/Unreal/Source Engine/3D engine AAA industry-standard) + Wikipedia "Navigation mesh" + Wikipedia "Recast" + Wikipedia "Detour" + Recast Navigation GitHub mononen/recastnavigation (8.6k★) + HatLink/VoxelNavigation (Unity Asset Store, voxel-based 3D pathfinding without navmesh = production reference for voxel-native alternative) + WingedEdge/Link generation algorithms + Mononen 2009 GDC presentation + van Toll et al. 2012 "Navigation Meshes for Realistic Multi-Layered Environments" + van Toll et al. 2011 "A navigation mesh based on vertical visibility" + Pettre et al. 2005 "A navigation graph for real-time crowd animation on multilayered environments" + Kallmann 2010 "Navigation Planning with Realistic Walking" (jump links, climbing, dynamic obstacles) + Recast voxelization step (Mononen 2009) + UE5 NavMesh (Recast-based) documentation + Unity NavMeshComponent (Recast-based) + Godot NavigationServer + Solmani Recast in Rust + CryEngine AI/Navigation system (Crytek, mature production) + Frostbite navigation (GDC presentations) + Killzone 2 navigation (Verweij 2009 GDC) + Halo Wars navigation (Bungie) + 1) Marzotto 2022 "Path Planning in Virtual Worlds" survey + 2) Open/RVO (Unreal Engine 4 paper, local collision avoidance layer on navmesh) + 3) PathFinding.js 12 algorithms + 4) A* and JPS (jump point search) for sparse graph optimization + 5) wikipedia "A* search algorithm" + 6) wikipedia "HPA*" (hierarchical pathfinding, 2004) + 7) opensteer, OpenSteerDotNet, RVO2 + 8) Mikro Mononen 2012 "Voxel-based Navigation Mesh" thesis chapter + 9) Seidel 2012 "Generating Navigation Meshes from Voxel Data".

**Phase 0:** ✅ folder created + STATUS.md init + backlog.md move to §In progress + INDEX.md §5 Active sync + README.md scaffold complete.

**Phase 1:** ✅ web-research complete → 19 sources verified в [`sources.md`](./sources.md) (15 Tier 1 industry + academic + 4 Tier 2 production engines).

**Phase 2:** ✅ prototype C++26 CPU standalone build complete → `prototype/navmesh_bench.cpp` ~660 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green (8 cosmetic warnings on unused const variables + 1 unused set).

**Phase 3:** ✅ benchmark + RESULTS.md complete → 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **2.6 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.

**Sync §13.5 complete** — см. §Closed entries в `backlog.md` + `backlog_closed.md` + `INDEX.md §6 Recent closed sessions`.

**Output:**
- [`prototype/build/navmesh_bench`](./prototype/build/navmesh_bench) (76 KB binary)
- [`prototype/build/results.csv`](./prototype/build/results.csv) (126 rows = 1 header + 125 data, 10.7 KB)
- [`prototype/build/summary_means.csv`](./prototype/build/summary_means.csv) (5 rows, per-strategy means, 517 B)
- [`prototype/build/run.log`](./prototype/build/run.log) (experiment metadata, 325 B)
- [`README.md`](./README.md) (scaffold + hypothesis + method)
- [`sources.md`](./sources.md) (19 verified sources: Recast&Detour 8k★, Wikipedia Navigation mesh, van Toll 2011/2012/2017, Pettré 2005, Kallmann 2010, Botea HPA* 2004, NEOGEN 2013, UE5 NavLink, Unity Off-Mesh Links, HatLink/VoxelNavigation, Voxel Tools, AnyPath, Frostbite 2 GDC 2012, voxelearth/voxelizer, Seidel 2012)
- [`RESULTS.md`](./RESULTS.md) (per-strategy + per-scene + hypothesis validation + 5-10% threshold + caveats + integration recommendation + final verdict)

**Caveats pre-empted:**
- CPU-only analytical cost model (no GPU compute, no Vulkan init, no Flecs ECS overhead, no real JPH coupling).
- 2.5D stair/ramp detection is voxel-pattern-based (closed `voxel-topology-analysis` precedent for overhang detection at 0.19 µs reusable).
- 3D multi-level (multi-floor via ladder/elevator/voxel-jump) is **out of scope** for first iteration (3D navmesh is research-grade, Recast is 2.5D-first; full 3D = follow-up).
- Cross-chunk seam handling simplified to "neighbor chunk shares boundary edge/navmesh face" (production would need async tile-based generation).
- Dynamic chunk update = **full chunk regenerate** (per `voxel-chunk-streaming-pipeline` precedent — dirty chunk → full rebuild). Incremental partial update (only changed voxels → navmesh patch) deferred to integration.
- Storage costs are upper bound (no compression; production could use run-length encoding on walkable cells).
- Generation cost assumes random-access to chunk voxels (real ProjectV mainline would batch chunks with `voxel_write_batch()`).
- No mobile/M1 GPU dispatch (CPU cost dominates on 1 thread; multi-threaded parallel = mainline integration).
