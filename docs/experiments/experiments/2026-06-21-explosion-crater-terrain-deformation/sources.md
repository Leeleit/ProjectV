# Sources — 2026-06-21-explosion-crater-terrain-deformation

Web-research via Exa `web_search` (3 waves, 16 results, this session `2026-06-21`).

## Tier 1 — Primary sources (cited in README §2)

1. **Teardown Developer Breaks Down Multiplayer and Voxel Destruction Tech — Dennis Gustafsson 80.lv** (2026-03-17)
   <https://80.lv/articles/teardown-developer-breaks-down-multiplayer-and-voxel-destruction-tech>
   - **Why important:** Production-grade voxel destruction at scale. Key claims:
     - "Teardown objects are represented and modeled as voxel volumes on a regular grid" — same approach
       as ProjectV (Sparse64Tree on 8³ grid per `src/voxel/VoxelWorld.hpp:78`).
     - SIMD + multithreading for destruction performance.
     - **Deterministic destruction commands for multiplayer sync** (NOT voxel data) — directly relevant
       for our cross-chunk crater propagation (commands replicate, voxel state derives).
   - **For our experiment:** validates voxel-on-regular-grid as the right pattern; provides order-of-magnitude
     for "10k+ explosion events per second" budget.

2. **Real-Time Craters Generation On Dynamic Terrains — SBGames 2024 paper** (2024-09-30)
   <https://sol.sbc.org.br/index.php/sbgames/article/view/32332>
   - **Why important:** **DIRECTLY relevant paper.** "An innovative technique for simulating crater
     deformations caused by explosions in large height map-based virtual terrains." **Stores crater
     information as variables in a compact GPU-based hash table; actual deformation computed via
     compute shaders each time a heightmap block is loaded to GPU.** Validates our hypothesis
     (compute-shader-based crater formation).
   - **For our experiment:** confirms **GPU compute shader** is the right strategy; **hash-table
     storage of crater parameters** (radius, origin, falloff curve) for late-bound deformation matches
     our hypothesis axis E (deferred deformation).

3. **BoxCutter - Realtime Voxel Destruction — Unity Asset Store** (latest 2026-05-14)
   <https://assetstore.unity.com/packages/tools/physics/boxcutter-realtime-voxel-destruction-331249>
   - **Why important:** Reference production asset (Bitwise Games). 5 fragmentation modes (Standard,
     Radial, Slab, Splinter, Cluster); Burst multithreaded; **KD-tree spatial acceleration**;
     **occlusion-aware greedy meshing**; compound BoxColliders.
   - **For our experiment:** validates **5 strategies axis** (we use 5 strategies too); KD-tree pattern
     (analogous to our C_BlockBased8x / D_BlockBased2x sub-block spatial partitioning); occlusion-aware
     approach matches Leon's Notes axis (Leon's cubemap bake = our B_AABBPreFilter).

4. **Voxels That Scale and Break — Leon's Notes** (2026-06-03)
   <https://leonsnotes.ca/2026/06/03/voxels-that-scale-and-break/>
   - **Why important:** **Critical insight: explosion must not punch through obstacles.** "A charge
     going off beside a steel pillar shouldn't scar the wall behind it." Solution: cast explosion as
     point light, bake depth shadow cubemap (6 × 32² ≈ 6K rays, <1 ms batched off-thread), damage
     cell only if sits nearer than occluder. **O(1) lookup per cell.**
   - **For our experiment:** This is the **occlusion-correctness** axis (criterion 3 in our hypothesis).
     Naive sphere-SDF carve WITHOUT occlusion will carve the wall behind an obstacle. Cubemap bake
     validates the right pattern. Our experiment's `E_RasterizedSphereMarch` strategy approximates
     this at the chunk level (column-wise sample at fixed directions).

5. **How beautiful voxels laid the way for Teardown's heist-y framework — Game Developer 2020-12-16**
   <https://www.gamedeveloper.com/design/how-beautiful-voxels-laid-the-way-for-i-teardown-s-i-heist-y-framework>
   - **Why important:** Detailed Teardown architecture: "thousands of smaller volumes" instead of one
     big volume; voxel vs voxel collision on CPU + GPU ray-march rendering; "I have a separate voxel
     structure just with the 3D occlusion data" for ray tracing.
   - **For our experiment:** validates the multi-volume model matches ProjectV's per-chunk 8³ structure;
     CPU collision + GPU rendering split applies to crater formation (CPU carve → mesh rebuild on GPU).

6. **Non-Destructive Destruction — Game Developer** (2022-06-17)
   <https://www.gamedeveloper.com/game-platforms/non-destructive-destruction>
   - **Why important:** SDF-based destruction (alternative to direct voxel-by-voxel). Stores mesh
     SDF in 3D texture, applies damage by creating sphere SDF, **subtracts** damage SDF from mesh
     SDF (Boolean), renders with ray-marching for inside visualization. Proposes **SDF subtraction
     as the right pattern for damage** (vs naive voxel delete).
   - **For our experiment:** **directly validates our hypothesis** (sphere-SDF subtraction). Our
     voxel chunk version: compute voxel center distance to explosion sphere origin; if less than
     radius, set to 0. This is the 1D analog of their 3D SDF subtract (per-voxel, not per-fragment).

## Tier 2 — Secondary sources (background)

7. **Fire-Aalt/com.firealt.mesh-to-sdf — Unity GPU compute SDF generator** (2026)
   <https://github.com/Fire-Aalt/Runtime-MeshToSDF>
   - 2× faster than Unity's built-in SDF baker; GPU compute + quasi-random triangle sampling.

8. **Unity-Technologies/com.unity.demoteam.mesh-to-sdf** (real-time SDF)
   <https://github.com/Unity-Technologies/com.unity.demoteam.mesh-to-sdf/blob/main/README.md>
   - 5k triangle mesh in 32³ voxel volume: jump flood = 0.22 ms (RTX 3090), linear flood 8 iter = 0.18 ms (RTX 3090) / 0.21 ms (RTX 2080 Super).

9. **EmmetOT/IsoMesh — Unity SDF tools**
   <https://github.com/EmmetOT/IsoMesh>
   - SDF with min/subtract operations; surface nets / dual contouring back to mesh; GPU compute parallel.

10. **nexus-engine destruction-quickstart.md**
    <https://github.com/developerz-ai/nexus-engine/blob/main/docs/guides/recipes/destruction-quickstart.md>
    - "Crater terrain on explosion: nexus add nexus-deformable-heightmap; rocket impacts auto-dig craters." Confirms **crater-as-separate-axis** from destruction/debris.

11. **Teardown and Voxel-Based Rendering with Dennis Gustafsson — Software Engineering Daily** (2025-01-02)
    <https://softwareengineeringdaily.com/2025/01/02/teardown-and-voxel-based-rendering-with-dennis-gustafsson/>
    - 20-year history of voxel game development; Teardown tech deep-dive.

12. **Impressive Voxel Destruction System Made in Unity — 80.lv** (2025-11-11)
    <https://80.lv/articles/enjoy-chaos-with-thsi-voxel-destruction-system-made-in-unity>
    - Bitwise Games BoxCutter deep-dive: "All other objects have their momentum conserved" — physical correctness.

## Tier 3 — Background (open-source references)

13. **Minecraft Bukkit/mc-dev Explosion.java** (canonical BFS-flood explosion)
    - BFS-flood within radius + power decay + block resistance threshold. Naive O(n) per explosion.

14. **Gram-Schmidt voxel constraints — Purdue SIGGRAPH 2024** (cited in chunk-damage-fracture-model)
    - Breakable face-to-face voxel constraints for soft-body destruction.

15. **Overcentral — Nintendo Donkey Kong Bananza voxel physics** (2026-03-12)
    - Material-specific destruction properties (density, brittleness, friction); dynamic LOD.

16. **three-pinata Voronoi fracture library** (GitHub)
    - Voronoi cells as fracture patterns; pre-bake cache for performance.

## Cross-references inside ProjectV (closed experiments)

- **Closed `2026-06-21-chunk-damage-fracture-model`** [mixed, C_Greedy3D 2.88 µs, D_Voronoi 1.48 µs] —
  validates 8³ chunk scope: explosion leaves voxels connected (CCL 1 component always), so for
  **structural separation** requires cross-chunk damage. **Inheritance:** our experiment focuses on
  **carve void** (what's removed), not on debris generation (what remains as fragments).
- **Closed `2026-06-21-voxel-topology-analysis`** [yes, CCL 2.73 µs] — Union-Find CCL 26-conn for
  post-carve connectivity check (no floating islands); overhang detection 0.19 µs for ejecta origin.
- **Closed `2026-06-21-dynamic-battlefield-decal-system`** [mixed, D_AtlasIndirectLRU 0.886 ms GPU] —
  crater rim = scorch decal spawn point; flat ground = crater decal placement.
- **Closed `2026-06-21-ballistic-projectile-simulation`** [yes, B_TableLookup 14 ns] — bullet impact
  events trigger small craters (radius 0.5-1.5 voxels per bullet).
- **Closed `2026-06-21-mesh-shader-mega-instancing`** [mixed, C_AmplificationShader 62-544×] —
  ejecta particles (voxels flying from crater) can use amplification shader cull.
- **Closed `2026-06-21-vma-sparse-textures`** [mixed, software VT recommended] — crater sparse data
  (if persisting) as virtual texture page.
- **Closed `2026-06-21-voxel-mutation-cost-characterization`** [mixed, B_DirtyFlagDeferred −58%] —
  crater per-voxel mutation cost: 256 voxel-edits per crater; with dirty flag deferred = 1.74 µs.

## Open backlog cross-references

- **Open `destructible-building-system`** [Tier 1, h] — building structural damage uses same
  sphere-SDF + CCL; crater per-projectile, but building system = sustained damage.
- **Open `structural-collapse-cascade`** [Tier 1, h] — per-chunk CCL enables "unsupported voxels
  become debris" cascade; crater is the trigger event.
- **Open `component-vehicle-damage-model`** [Tier 1, h] — vehicle component hit mask + crater for
  vehicle damage (different geometry, but same SDF algorithm).
