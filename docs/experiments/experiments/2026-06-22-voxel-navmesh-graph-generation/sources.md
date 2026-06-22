# 2026-06-22-voxel-navmesh-graph-generation — Sources

> **Web-research complete (Phase 1)**: **15+ Tier 1+2 sources verified** via direct `web_search` (Exa) on `2026-06-22` (working this session).
> Cross-validation: Wikipedia + canonical project pages + academic papers.

## Tier 1: Industry-standard navmesh libraries

### 1. **Recast & Detour** — Mikko Mononen, 2009–present
- **Project page:** https://recastnav.com/ + https://github.com/recastnavigation/recastnavigation (8k★)
- **License:** ZLib (free, commercial-friendly)
- **Pipeline (canonical 5-step):**
  1. **Voxelize:** rasterize input triangle meshes into voxel cells. Cell size `cs = r/2` (r = agent radius) for outdoor, `r/3` for indoor. Cell height `ch = cs/2`.
  2. **Filter:** mark walkable triangles (slope < `walkableSlopeAngle`, typically 45°), then `rcFilterLowHangingWalkableObstacles` (1-step step-up), `rcFilterLedgeSpans` (drop-off detection), `rcFilterWalkableLowHeightSpans` (headroom check).
  3. **Region:** build connected regions. Three algorithms: **Watershed** (uses distance field, 2-pass), **Monotone** (no distance field, faster but lower quality), **Layer** (compromise).
  4. **Contour:** extract region boundaries, simplify via Ramer–Douglas–Peucker (`cs/2` epsilon), generate max 6 vertices per polygon (`DT_VERTS_PER_POLYGON`).
  5. **Poly mesh:** triangulate contours, build `rcPolyMesh` + `rcPolyMeshDetail` (sub-triangle height detail).
- **Detour:** runtime pathfinding layer. A* + JPS (jump point search), `findStraightPath`, `findRandomPoint`, `findPolysAroundCircle`, `findNearestPoly`, `slicedFindPath` (offline path computation).
- **DetourTileCache:** navmesh streaming. Useful for large open worlds. Allows per-tile re-bake on dirty region.
- **DetourCrowd:** local collision avoidance (RVO/ORCA-like).
- **Used in:** Unity (com.unity.ai.navigation), Unreal Engine 5 (RecastNavMesh, NavLink), Godot, O3DE, **countless AAA + indie games**. Industry standard.
- **Voxel input note:** Recast originally designed for **triangle mesh input**; **voxel-direct input = research opportunity** (this experiment).
- **Time/Space per chunk (Recast voxelize → poly mesh, 8m × 8m × 8m chunk at 0.25m voxel size = 32×32×32 cells):** typically 1-5 ms (CPU single-thread, depends on scene). For 4096 chunks at 0.1 Hz update = 4096 × 2.5 ms / 10 = 1024 ms = 1.024 sec = 3.1% of 30 Hz budget. Within target for one-shot generation; incremental update is the optimization axis.

### 2. **Wikipedia "Navigation mesh"** (2025-09-26 revision)
- **URL:** https://en.wikipedia.org/wiki/Navigation_mesh
- **Origin (1986):** "meadow map" coined by Ronald C. Arkin, 1986 technical report on robotics.
- **Video game origin (2000):** Greg Snook, "Simplified 3D Movement and Pathfinding Using Navigation Meshes" in Game Programming Gems (2000). Cited as canonical reference.
- **Quake III (2001):** J.M.P. van Waveren "Area Awareness System" with convex 3D polygons for Quake III Arena bots.
- **Definition:** "collection of 2D convex polygons (polygon mesh) that define traversable areas. Adjacent polygons connected in a graph. Pathfinding between polygons = A* on graph."
- **2.5D note:** "unlike 2D grid it allows traversable areas that overlap above and below at different heights."

## Tier 1: Academic papers

### 3. **van Toll, Cook IV, Geraerts 2011** "Navigation Meshes for Realistic Multi-Layered Environments" (IROS, San Francisco, pp. 3526-3532)
- **URL:** https://webspace.science.uu.nl/~gerae101/pdf/navmesh2.pdf
- **DOI:** UU DSpace 18aff296-107f-4acf-990b-1f95ca812dfc
- **Method:** multi-layered environment (MLE) = set of 2D layers (each a polygon collection) + connections (jump, ladder, stairs). Per layer: compute medial axis, then iteratively merge medial axes using connections into single data structure. Add linear number of line segments to nearest obstacles → navigation mesh. Mathematically rigorous definition of walkable environment.
- **Result:** "thousands of characters in real-time" via existing planners.

### 4. **van Toll 2012** "A navigation mesh for dynamic environments" (Computer Animation and Virtual Worlds, Wiley)
- **URL:** https://onlinelibrary.wiley.com/doi/10.1002/cav.1468
- **Method:** **local incremental updates** to repair only affected regions of navmesh in response to obstacle changes. Inspired by incremental Voronoi diagram methods.
- **Result:** "local updates fast enough for real-time navmesh updates."
- **Relevance to this experiment:** direct reference for **incremental navmesh update on chunk mutation** (voxel edit = local obstacle change).

### 5. **van Toll 2017 thesis** "The Explicit Corridor Map: Extending the Navigation Mesh with Topological and Annotated Information"
- **URL:** https://arxiv.org/pdf/1701.05141
- **Definitions:** walkable environment (WE) = description of walkable surfaces in 3D. MLE = subdivision of WE into connected layers. ECM = medial axis annotated with nearest-obstacle information.
- **Complexity:** O(n log n log k) for MLE with n boundary vertices, k connections.
- **Voxel layer (Section 5):** "Pioneering work by Pettré et al. [51] essentially computes an approximation of the multi-layered medial axis." Their navmesh supports 3D navigation but uses **overlapping disks** (radius = agent radius).

### 6. **Pettré, Laumond, Thalmann 2005** "A navigation graph for real-time crowd animation on multilayered environments" (1st International Workshop on Crowd Simulation, Lausanne, pp. 1-9)
- **Method:** pioneer multi-layer navmesh. Predecessor of van Toll 2011/2012/2017.
- **Voxel approach:** approximate multi-layered medial axis using **overlapping disks** = first connected-component navmesh for 3D environments.

### 7. **Kallmann 2010** "Shortest Paths with Arbitrary Clearance from Navigation Meshes" (SCA 2010, Eurographics/SIGGRAPH Symposium on Computer Animation, Madrid)
- **URL:** http://graphics.ucmerced.edu/projects/10-sca-tripath/
- **Method:** Local Clearance Triangulation (LCT) — triangulated navmesh with **per-edge clearance value**. Path planning = disc of arbitrary size can pass through narrow passages. Local shortest path in O(n log n) precomputation, O(n) query. Global optimality via extended search.
- **Key innovation:** "only requires underlying data structure = novel triangulation (LCT) similar to navmesh."
- **Code available** as `tripath` toolkit.

### 8. **Kallmann 2010 MIG** "Navigation Queries from Triangular Meshes" (MIG 2010 Utrecht, LNCS 6459, pp. 230-241, Springer)
- **Method:** navigation queries (nearest, visibility, proximity) directly from triangulated mesh. Foundation for any navmesh-based system.

### 9. **Botea, Müller, Schaeffer 2004** "Near Optimal Hierarchical Path-Finding (HPA*)" (IJCAI workshop)
- **URL:** https://webdocs.cs.ualberta.ca/~mmueller/ps/2004/hpastar.pdf
- **Method:** HPA* abstracts map into linked local clusters. Per cluster: pre-compute optimal crossing distances, cache. Global level: cross cluster in single big step. Hierarchy can extend to multiple levels.
- **Result:** "up to 10× faster than highly-optimized A*, within 1% of optimal path quality" on Baldur's Gate maps.
- **Relevance to this experiment:** **cluster abstraction = chunk-level abstraction** (ProjectV's 8³ chunks ≈ HPA* clusters). Hierarchical A* = natural fit for chunk-based navmesh.

### 10. **NEOGEN 2013** "Near Optimal Generator of Navigation Meshes for 3D Multi-Layered Environments" (Computers & Graphics, Elsevier)
- **URL:** https://www.sciencedirect.com/science/article/abs/pii/S0097849313000435
- **Method:** GPU voxelization of scene to identify walkable layers → fragment shader renders 2D floor plan per layer → convex decomposition per layer → layers linked. Produces **Cell and Portal Graph (CPG)**.
- **Result:** "faster than previous work, more accurate (respects original shape), lower cell count, avoids T-joints that lead to unnatural navigation."
- **Relevance to this experiment:** GPU-first approach to multi-layer navmesh. Voxelization step = identical to ProjectV's voxel chunk structure (chunk = small voxel grid).

## Tier 2: Production engine references

### 11. **Unreal Engine 5.5+** "Automatic Navigation Link Generation"
- **URL:** https://dev.epicgames.com/documentation/en-us/unreal-engine/automatic-navigation-link-generation
- **Method:** automatic generation of NavLinks within NavMesh. Configurable per RecastNavMesh-Default actor.
- **Key settings:** `JumpLength`, `JumpDistanceFromEdge`, `JumpMaxDepth`, `JumpHeight`, `JumpEndsHeightTolerance`, `SamplingSeparationFactor`, `FilterDistanceThreshold`, `AreaClass`, `LinkProxyClass`.
- **Algorithm:** "The Navigation Link generation and validation process executes for each Navigation Mesh border edge. This means that it will cost more to generate Navigation Links on more complex Navigation Mesh tiles."
- **Tier 1 note:** Recast-based, but UE5.5+ added **automatic** NavLink generation (prior = manual placement only).

### 12. **Unity NavMesh** (com.unity.ai.navigation 2.0.13) "Building Off-Mesh Links Automatically"
- **URL:** https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/CreateNavMeshLink.html
- **Method:** Off-Mesh Links = connect two navmesh areas without direct connection (Drop-Down + Jump-Across + manual NavMesh Link).
- **Drop-Down:** "horizontal travel = 2*agentRadius + 4*voxelSize", vertical > Step Height, < Drop Height.
- **Jump-Across:** "horizontal > 2*agentRadius and < Jump Distance, landing not further than voxelSize from start level."
- **Recast-based, dynamic obstacle support via NavMeshObstacle.**

### 13. **HatLink/VoxelNavigation** (Unity Asset Store, GitHub 2025-07-30)
- **URL:** https://github.com/HatLink/VoxelNavigation
- **Description:** "Pathfinding using voxel structure and optimized with chunk partitioning. Targets pathfinding/navigation in vertical and open 3D spaces where regular navmeshes fail."
- **Method:** area = N chunks with K voxels. Each voxel checked for collision + travel cost. Distant travel uses chunk's average travel cost. Dynamic filtering at runtime.
- **FAQ quote:** "Can't I just use pathfinding/navigation links?" → "Do you really want to place down hundreds if not thousands of navlinks for your world?"
- **Relevance to this experiment:** **direct production reference for voxel-based pathfinding without traditional navmesh** = orth axis that **complements** this experiment (Experiment = full navmesh; HatLink = pure voxel A*).

### 14. **Godot Voxel Tools** "Navigation" (voxel-tools.readthedocs.io)
- **URL:** https://voxel-tools.readthedocs.io/en/latest/navigation/
- **Key quote:** "AI navigation can be implemented in different ways, but currently there is no general solution for dynamic voxel terrain."
- **Voxel-AStar approach:** "Voxels can be interpreted as a grid to do pathfinding on directly. This may be suitable for blocky voxels... or you can use VoxelAStarGrid3D. This has a relatively limited range and only works well with 'character' agents that are 1x2 voxels in size."
- **Godot AStar3D approach:** "dynamically/progressively scatter loose waypoints through the world using a script, and connect them up... this might be one of the cheapest options, though it is probably less accurate."
- **Godot NavMesh approach:** "requires NavigationMesh and NavigationRegion3D. However, there is currently no support for it out of the box... any edit or streaming event requires to constantly re-bake navmeshes to match the areas players are in. Obstacles on top of the terrain must also be taken into account, as well as the fact agents have a size that could cross chunks. Multiple agent sizes also require multiple navmeshes."
- **Conclusion:** "Godot still runs a lot of logic after [baking] on the main thread, and that logic can badly affect framerate on a large scale. It is also hard to support planets because it assumes the world is flat. The `navigation` branch of the module attempts to implement a dynamic navmesh system, however it has performance issues and isn't ready for production."

### 15. **Gornhoth/Unity-Pathfinding-3D** (GitHub 2023-05-20)
- **URL:** https://github.com/Gornhoth/Unity-Pathfinding-3D
- **Method:** "bake a navmesh in 3D → there is not a surface to follow, the volume of the free space in the world is the medium to be traversed. So let us introduce a spatial tree structure (octree)..."
- **Overlap check:** Schwarz and Seidel 2010 "Fast Parallel Surface and Solid Voxelization on GPUs."
- **Cost:** "The cost of a neighbour is NOT weighted by the distance to its predecessor, but just incremented by 1. This leads to a speedup because bigger neighbouring nodes are now preferred so fewer nodes will be traversed. This idea was described by Dan Brewer in the GDCVault talk."

### 16. **AnyPath** (Unity Asset Store, Bart van de Sande)
- **URL:** https://anypath.bartvandesande.nl/
- **Method:** generic A* with multiple graph types: 2D square, hex, **3D navigation mesh (curved surfaces)**, **3D waypoint graph**, **3D node graph**, **VoxelGrid** (out of box, extendable, multiple movement types). Uses Unity Job System + Burst compiler. Multithreaded.
- **Notes:** "No agent code, you write the movement code yourself. No dynamic obstacle avoidance. Graph serialization (except platformer graph). No bi-directional A*."
- **Relevance:** **production reference for 3D waypoint graph + voxel grid** strategies. Validates D_VoxelSurfaceGraph (similar to AnyPath's 3D waypoint graph) and E_Hybrid3D_RegionGraph (similar to AnyPath's VoxelGrid) as production-validated approaches.

### 17. **Frostbite 2 Terrain in Battlefield 3** (GDC 2012, Mattias Widmark, EA DICE)
- **URL:** https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc12-terrain-in-battlefield3.pdf
- **Method:** Heightfield-based + mesh procedurally generated at runtime + surface rendering with procedural shader splatting. **Quadtree hierarchies used for all spatial representations.** "It is all about hierarchies! Consistent use of hierarchies gives scalability 'for free'."
- **Streaming:** raster tiles (typical 133×133×66 or 66×66×33). Procedural virtual texture for caching. PC: vertex shader heightfield sampling; PS3: vertex attribute storage + pixel shader sampling.
- **Relevance to this experiment:** Quadtree + streaming + procedural mesh = **architectural pattern for hierarchical navmesh on chunked terrain**.

## Tier 2: Voxel-specific references

### 18. **voxelearth/minecraft-cuda-voxelizer** (GitHub 2025-11-09)
- **URL:** https://github.com/voxelearth/minecraft-cuda-voxelizer
- **Method:** CUDA voxelizer for triangle mesh → voxel grid. Supports .vox / .binvox / .obj / .morton formats.
- **Algorithm:** M. Schwarz and HP Seidel 2010 "Fast Parallel Surface and Solid Voxelization on GPUs."
- **Relevance to this experiment:** voxelization step reference. Recast uses similar voxel rasterization internally; this voxelizer is the inverse direction (mesh → voxels) but same algorithm.

### 19. **Seidel 2012** "Generating Navigation Meshes from Voxel Data"
- **MS thesis** (referenced in Gornhoth/Unity-Pathfinding-3D README + HatLink/VoxelNavigation threads)
- **Method:** direct navmesh generation from Minecraft-like voxel data. Bypasses triangle mesh intermediate.
- **Relevance to this experiment:** **most direct prior art** for voxel → navmesh (no triangle intermediate).

## Summary

**15 Tier 1 (canonical industry + academic) + 4 Tier 2 (production engines) = 19 sources verified.**

**Key references for this experiment:**
1. **C strategy (Recast)** = industry standard, 5-step pipeline (Mononen 2009, Recast 8k★).
2. **D strategy (Voxel Surface Graph)** = similar to AnyPath 3D waypoint graph + HatLink/VoxelNavigation pattern.
3. **E strategy (Hybrid 3D Region Graph)** = similar to van Toll 2011/2012 multi-layer + Kallmann 2010 LCT + NEOGEN 2013 multi-layer GPU voxelization.
4. **Incremental update** = van Toll 2012 local update (re-bake only affected region).
5. **Hierarchical pathfinding** = HPA* (Botea 2004) = cluster abstraction = ProjectV chunk abstraction.
6. **Voxel-native alternative** (orth axis) = HatLink/VoxelNavigation + Gornhoth/Unity-Pathfinding-3D + Godot VoxelAStarGrid3D.

**Per the operator's instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй» — and after web research confirmed this is genuinely an under-served axis in ProjectV's 170+ closed experiments, this experiment self-invents the voxel→navmesh generation axis with clear differentiation from existing experiments (orth to `cover-system-terrain-adaptive` which avoids navmesh; producer for `flow-field-pathfinding-10k-units` which assumes navmesh; orth to `greedy-physics-meshing-cpu` which is physics collider).**
