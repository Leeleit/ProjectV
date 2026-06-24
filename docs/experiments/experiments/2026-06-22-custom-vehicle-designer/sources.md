# Sources — 2026-06-22-custom-vehicle-designer

## A. Game references (voxel-based vehicle assembly)

### A.1 From the Depths (Brilliant Skies 2014–2026)
- Voxel-by-voxel vehicle assembly, each block = 1 m³ with material properties (armor, HP, buoyancy).
- Chassis → collision mesh → runtime physics via custom solver (not Jolt/PhysX).
- Precomputed buoyancy: per-block water displacement volume + center of buoyancy.
- **Collision representation:** compound of block-sized AABBs with optional hull smoothing (per-player setting "detailed collision").
- Source: fromthedepths.wiki.gg, FtD dev blogs.

### A.2 Stormworks: Build and Rescue (Sunfire Software 2018–2026)
- Modular vehicle assembly with physics, engines, electronics.
- Body panels + components approach (voxel-like grid but with body panels as contiguous surfaces).
- **Physics representation:** voxel-grid → convex decomposition → simplified collision hull.
- Soft-body physics via voxel-like grid (deformation on impact).
- Source: Steam Community guides, stormworks.fandom.com.

### A.3 Space Engineers (Keen Software House 2013–2026)
- Block-based ship/station building with deformable voxel terrain.
- VRAGE 2 engine: volumetric objects with mass, inertia, velocity.
- **Collision representation:** per-block OBB in a hierarchical compound; large/small grid with different block sizes (2.5 m / 0.5 m).
- Player builds by placing blocks on grid → physics shape created at weld time.
- Source: spaceengineerswiki.com, Keen Software House dev blogs.

### A.4 Trailmakers (Flashbulb 2018–2026)
- Snap-together vehicle building, pre-defined block shapes (not free voxel).
- Each block = pre-made collision mesh + physics properties.
- Source: trailmakers.fandom.com.

### A.5 Besiege (Spiderling 2015–2026)
- Medieval siege engine building, block-based assembly.
- Blocks have physical properties (mass, HP, material).
- Source: besiege.fandom.com, Spiderling dev blogs.

### A.6 Avorion (Boxelware 2017–2026)
- Voxel-based ship building with dynamic LOD (voxel → mesh for rendering).
- Collision: axis-aligned compound of BoxShapes + convex decomposition for complex forms.
- Source: avorion.net, Boxelware dev blogs.

### A.7 Foxhole (Siege Camp 2017–2026)
- Player-built vehicles as assembled components + voxel terrain.
- Not true voxel vehicle building — vehicles come pre-fabricated as whole units.
- Terrain is voxel-based (trenches, bunkers, craters).
- Source: foxhole.wiki.gg, Siege Camp dev streams.

## B. Physics engine references

### B.1 Parry 0.25+ native `Voxels` shape (2025)
- Dedicated voxel collision shape type added in parry 0.15 → stable in parry 0.25+.
- **Neighbor tracking:** knows which voxel faces are internal (adjacent to other voxel) vs external (exposed to air).
- **No internal edge snagging:** smooth sliding across flat voxel surfaces.
- **Dynamic add/remove:** `set_voxel(key, is_filled)` updates neighborhood info for affected + neighbor voxels.
- **Multi-shape coordination:** `combine_voxel_states()` + `propagate_voxel_change()` for seamless boundaries between separate `Voxels` shapes.
- **Sparse storage:** memory efficiency for destructible objects.
- **Missing features (as of 2025-04):** collision with non-convex / compound shapes; shape-casting.
- **Rapier integration:** `Voxels` collider type (PR #823, 2025-04).
- Source: `dimforge/parry` PR #336 (2025-04-17), `parry3d` docs, `rapier3d` docs.

**Relevance to ProjectV:** Proof-of-concept that native voxel collider provides collision quality advantages (no internal-edge snagging). Jolt lacks this — ProjectV must implement via `StaticCompoundShape` + greedy merge (closed `greedy-physics-meshing-cpu`). This experiment tests whether vehicle-specific assembly strategies can close the gap.

### B.2 Jolt Physics — CompoundShape best practices
- **Jorrit Rouwé (2023, #446):** divide voxel world into chunks (e.g. 16×16×16), create `StaticCompoundShape` per chunk. Can reuse same `BoxShape(0.5)` instance across all blocks. `MutableCompoundShape` for frequent updates.
- **Jorrit Rouwé (2024, #1067):** for irregular/editable terrain, use regular grid of `StaticCompoundShape` tiles. On edit, recreate tile. ~1000 shapes per compound is fine; 10K is "a bit much".
- **Jorrit Rouwé (2025, #1801):** compound internal BVH does similar work to broad-phase. Compounds suffer less cache misses (tighter packing). No massive performance difference between 1 compound-of-1000 vs 1000 separate bodies.
- **Jolt 5.3.0 CompoundShape API:** `InnerShapeOffset`, tree-based leaf culling, `MustBeStatic()` for child shapes.
- **Vehicle API:** Jolt has built-in `WheeledVehicleController` + `TrackedVehicleController`. VehicleConstraint with per-wheel collision tester (ray/cylinder), separate from main compound shape.

### B.3 bepuphysics2 — Custom `Voxels` collidable
- C# physics engine with custom voxel collision shape registered via `NarrowPhase` + `SweepTask` registration pipeline.
- `Compound<Voxels>` pair registered with `NonconvexReduction` continuation.
- Source: `bepu/bepuphysics2` — `Demos/CustomVoxelCollidableDemo.cs`.

### B.4 Unity NonConvexMeshColliders — Voxel decomposition
- `VoxelCollider` fills mesh interior with voxel grid → generates `BoxCollider` per filled voxel.
- Optional greedy merge step combines adjacent voxels into larger boxes (same algorithm as `greedy-physics-meshing-cpu`).
- Source: `JohannHotzel/UnityNonConvexMeshColliders`.

### B.5 Rig My Ride — Automatic physics-based vehicle rigging (SCA 2025)
- End-to-end pipeline: polygon soup → text-based 2D image segmentation (GLIP + SAM) → wheel identification → cylinder fitting → joint parameter optimization → physics-based vehicle.
- Optimization: gradient-free minimization in physics simulation loop to refine wheel geometry + joint constraints.
- Supports cars, tricycles, lunar rovers, semi-trucks with 10 wheels.
- Source: Katz, Kry, Andrews — SCA 2025, DOI `10.1145/3747861`.

## C. Closed complementary experiments

| Slug | Verdict | Relevance |
|:-----|:--------|:----------|
| `greedy-physics-meshing-cpu` (2026-06-21) | yes | F_TwoPass: 35× shape reduction, 100% volume preservation, 0.78 µs/chunk. **Algorithm source for strategies C + E.** |
| `data-driven-vehicle-weapon-definitions` (2026-06-21) | mixed | JSON/TOML codegen schema for vehicle stats. Upstream: blueprint definition format consumed by this experiment. |
| `component-vehicle-damage-model` (2026-06-21) | yes | Per-module hit-testing (1.4 ns/shot B_BinnedGrid). Downstream: vehicle assembly must preserve module grid mapping. |
| `tank-terrain-interaction-physics` (2026-06-21) | yes | Physics hull for tank-terrain interaction (0.005 ms/veh). Cross-ref: vehicle collision shape drives hull behaviour. |
| `custom-weapon-modding` (2026-06-21) | open | Weapon attachment system. Parallel track, no direct overlap. |

## D. Technical concepts

### D.1 Greedy mesh (physics variant)
- Mikola Lysenko 2012 "Meshing in a Minecraft Game" (0fps.net) — foundational 2D greedy face-merging.
- Boksansky 2019 "Greedy Meshing" (Wicked Engine) — 3D voxel AABB merge for collision.
- Closed `greedy-physics-meshing-cpu`: applied to physics (AABB merge, not visual faces). F_TwoPass = 2D XZ per Y + vertical merge → 35× reduction.

### D.2 Sub-assembly / hierarchical compound
- Principle: divide vehicle into functional modules (hull, turret, engine, wheels). Each module = independent `StaticCompoundShape`. Vehicle = top-level compound of module compounds.
- Advantage: mutation rebuild is sub-module-scoped (change wheel → rebuild only wheel compound, not whole vehicle).
- Tradeoff: deeper BVH tree in Jolt compound (1-level vs 2-level). Negligible for <100 modules per Jorrit Rouwé.

### D.3 MutableCompoundShape for vehicle editing
- Jolt `MutableCompoundShape` allows add/shape/remove child shapes without full rebuild.
- Tradeoff: query performance degrades with edits (no BVH rebuild). Per Rouwé: "trading rebuild speed for query performance".
- For vehicles with infrequent editing (spawn-time assembly + occasional blueprint edits): rebuild from scratch (F_TwoPass) is acceptable.
- For in-game modification (player actively editing vehicle in world): MutableCompoundShape may be better.

### D.4 Cylinder/capsule shapes for wheels
- `JPH::CylinderShape(halfHeight, radius)` — for wheel voxels.
- Jolt's built-in `WheeledVehicleController` uses ray-cast collision testers, not cylinder shapes. But for collision with other objects, wheels need cylinder/capsule colliders.
- ProjectV may use Jolt's `TrackedVehicleController` for tanks (closed `tank-terrain-interaction-physics`).

## E. Hardware baseline

- **CPU:** AMD Ryzen 7 5800X (Zen 3, 8C/16T, 3.8 GHz base, 4.7 GHz boost)
- **GPU:** NVIDIA RTX 3060 Ti (GA104, 8 GiB GDDR6)
- **RAM:** 32 GiB DDR4-3600
- **OS:** Linux 6.14.0 (Fedora 42)
- **Compiler:** Clang 22.1.6 `-O3 -march=native -std=c++26`
- **Governor:** `powersave` (consistent with all closed experiments)
- Source: [`../../hardware-profile.md`](../../hardware-profile.md) §1, §3.

## F. Web search methodology

- Exa API returned HTTP 429 (rate limited) — used DuckDuckGo HTML fallback per the web_search fallback chain.
- DuckDuckGo → webfetch pages → verify facts via cross-referencing game wikis, GitHub repos, API docs.
- Date range: 2023–2026 (Jolt discussions), 2025 (Parry Voxels shape), 2025 (SCA Rig My Ride).
- Search queries used: "voxel vehicle assembly collision shape compound greedy", "Jolt Physics CompoundShape vehicle dynamic", "voxel collision detection Parry Rapier shape 2025", "Jolt Physics voxel world best practice compound shape".
