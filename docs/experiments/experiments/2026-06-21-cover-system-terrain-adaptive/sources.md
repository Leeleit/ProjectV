# Sources — cover-system-terrain-adaptive

## Tier 1 (primary, verified this session)

1. **GlassBeaver CoverSystem** (github, 2018-2023, 184★)
   `github.com/GlassBeaver/CoverSystem` — MIT licensed UE4 cover system used in Severed Steel. Navmesh edge-walking + 3D object scanning. Octree persistence.

2. **KieranCoppins Post-Navigation-System** (Unity, 2025)
   `github.com/KieranCoppins/Post-Navigation-System` — Open/cover post generation from navmesh. Naughty Dog hard-points inspiration. Zone management.

3. **Tactical Cover & Retreat AI System v2.0** (Unity Asset Store, Feb 2026)
   `assetstore.unity.com/packages/tools/behavior-ai/tactical-cover-retreat-ai-system-338916` — 15+ spot providers. Modular scoring (visibility, distance, survival, stealth, prediction).

4. **jfq520/CoverGenerator-UE4** (github, 2025)
   `github.com/jfq520/CoverGenerator-UE4` — Navmesh-based cover generation with crouch/stand/lean classification. EQS integration.

5. **Recited.io — Cover System Implementation** (2026-05-04)
   `recited.io/kb/ai-in-game-development/combat-and-decision-systems/cover-system-implementation/` — Automated detection + baking vs manual tagging.

6. **Arma Reforger — SCR_AIFindCover** (Bohemia Interactive, 2024)
   `arexplorer.zeroy.com/_s_c_r___a_i_find_cover_8c_source.html` — Production cover system: CoverQueryProperties with score-weighted selection (direction, distance, navmesh ray, visibility).

7. **HatLink/VoxelNavigation** (github, 2025)
   `github.com/HatLink/VoxelNavigation` — Voxel-based 3D pathfinding using chunks. Directly relevant: no navmesh dependency.

8. **darbycostello/Nav3D** (UE5, github, 2020-2026)
   `github.com/darbycostello/Nav3D` — Sparse Voxel Octree 3D navigation. Region identification, adjacency graphs, tactical queries, multi-criteria scoring.

9. **midgen/AeonixNavigation** (UE5, github, 2025)
   `github.com/midgen/AeonixNavigation` — SVO 3D pathfinding with A*/Theta*/Lazy Theta*. Dynamic modifier regions.

10. **arXiv 2605.21397** — "Validating Navmesh using Geometry: Voxel-Based Analysis with Prioritized Exploration" (2026)
    `arxiv.org/html/2605.21397v1` — Voxel walkable space reconstruction, flood-fill connectivity.

11. **closed `voxel-topology-analysis`** (2026-06-21, verdict=yes)
    `docs/experiments/experiments/2026-06-21-voxel-topology-analysis/` — Overhang detection 0.19 µs, CCL 2.73 µs.

12. **closed `flood-fill-visgraph-culling`** (2026-06-21, verdict=yes)
    `docs/experiments/experiments/2026-06-21-flood-fill-visgraph-culling/` — Occlusion BFS for visgraph.

## Tier 2 (secondary, supplementary)

13. **GameDev.net — Cover system in games** (2023-10-23)
    `gamedev.net/blogs/entry/2276748-cover-system-in-games/` — Navmesh edge-walking vs environment scanning.

14. **freeCodeCamp — How to build a real-time dynamic cover system in UE4** (2018-03-28)
    `freecodecamp.org/news/real-time-dynamic-cover-system-in-unreal-engine-4-eddb554eaefb/` — GlassBeaver tutorial.

15. **thelinuxcode.com — How to Build a Real-time Dynamic Cover System in UE4** (2024-12-24)
    `thelinuxcode.com/how-to-build-a-real-time-dynamic-cover-system-in-unreal-engine-4/` — Navmesh edge-walking, ledge detection, octree optimization.

16. **arXiv 2602.04130** — "Real-time pathfinding in heterogeneous 3D environments" (2026)
    Recast navmesh generation, density-aware crowd coordination.

## Tier 3 (ProjectV internal)

17. `agent/knowledge.md` — 3-step migration precedent
18. `docs/experiments/hardware-profile.md §1` — Zen 3 5800X dev host
19. `benchmarks/methodology.md §3` — measurement protocol
20. `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% perf threshold
