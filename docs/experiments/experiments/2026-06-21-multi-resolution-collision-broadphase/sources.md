# Sources — 2026-06-21-multi-resolution-collision-broadphase

Verified primary sources from web research 2026-06-21.

## Tier 1 — Direct API references + canonical algorithms

1. **Jolt Physics** (Jorrit Rouwe 2025) — https://jrouwe.github.io/JoltPhysics/
   BroadPhaseQuadTree, BroadPhaseLayer (Static/Moving), lock-free mostly. `mTimeBeforeSleep=0.5s`, `mPointVelocitySleepThreshold=0.03 m/s`, `mUseLargeIslandSplitter=true`, `mAllowSleeping=true` (defaults per `external/JoltPhysics/Jolt/Physics/PhysicsSettings.h`).

2. **Architecting Jolt Physics for Horizon Forbidden West** (GDC 2022, Jorrit Rouwe) — https://jrouwe.nl/architectingjolt/ArchitectingJoltPhysics_Rouwe_Jorrit_Notes.pdf
   Lock-free broadphase + lock-free island building; broadphase = singleton QuadTree.

3. **Jolt Multicore Scaling** (Rouwe 2024) — https://jrouwe.nl/jolt/JoltPhysicsMulticoreScaling.pdf
   4.9× speedup at 8 threads, 5.7× at 16 SMT threads. Stops scaling at ~16 cores (memory bus bottleneck).

4. **Rapier (Rust) BroadPhase** — https://docs.rs/shura/latest/shura/physics/rapier/geometry/struct.BroadPhase.html
   **Multi-SAP + hierarchical grid** — canonical reference for our hypothesis. SAPLayer per scale (1×1×1, 10×10×10, etc.); inter-layer interference via region AABB insertion into larger layer.

5. **PhysX 5.4 GPU Rigid Bodies** — https://nvidia-omniverse.github.io/PhysX/physx/5.4.0/docs/GPURigidBodies.html
   4 CPU broad-phase variants: SAP (default), MBP, ABP, PABP (parallel). GPU broad-phase = CUDA parallel SAP + ABP initial pair gen.

6. **PhysX 5.4 Broad-phase types** — https://physics-playground.github.io/PhysX5/physx/5.3.1/docs/RigidBodyCollision.html
   SAP "good for when many objects are sleeping". MBP = multi-box pruning. ABP = automatic box pruning. GPU broad-phase = fully parallel.

7. **Bullet btMultiSapBroadphase** — http://docs.ros.org/en/diamondback/api/bullet/html/classbtMultiSapBroadphase.html
   Multiple SAP broadphases with `btQuantizedBvh` top-level. Marked "research, NOT production" in current Bullet docs.

8. **Pierre Terdiman MultiSAP benchmarks (2007)** — https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=1329
   **20-76× faster than single SAP** for insertions; 8-32× faster than Bullet baseline.

9. **MERL TR97-23 Hierarchical Spatial Hash** (Brian Mirtich 1998) — https://www.merl.com/publications/docs/TR97-23.pdf
   Canonical hierarchical spatial hash; O(d log R) bucket checks.

10. **Ewha gSAP (2014)** — https://graphics.ewha.ac.kr/gSaP/
    GPU-based SAP; hybrid Subdivision+SAP can handle 900K objects at 60 fps on Core 2 Duo.

## Tier 2 — Sleeping island references

11. **Erin Catto Box2D Simulation Islands (2023)** — https://box2d.org/posts/2023/10/simulation-islands/
    Persistent islands 10× faster than DFS for sleep/wake tracking. Sleeping must be per-island, not per-body.

12. **Avian3D Persistent Islands PR #809 (2025)** — https://github.com/Jondolf/avian/pull/809
    10× faster vs DFS; based on Erin Catto's design. Cites Jolt's parallel union-find as alternative but "requires expensive sorting for determinism".

13. **H2.0 robot simulation with sleeping (NeurIPS 2021)** — https://proceedings.neurips.cc/paper_files/paper/2021/file/021bbc7ee20b71134d53e20206bd6feb-Paper.pdf
    Bullet island sleep system + kinematic base + static parts + sleeping state → ~1200% SPS speedup (1191 vs 100 SPS idle).

14. **raduacg game-mechanics-optimizations (2024)** — https://github.com/raduacg/game-mechanics-optimizations/blob/main/09_physics_sleeping_bodies.md
    Sleeping body 50-100× cheaper. 80% sleeping ratio → 8.5× speedup on 1000 objects.

15. **Bullet Multi-SAP vs DBVT discussion (2011)** — https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=6863
    "SAP does not handle varying object sizes well" — large static bodies force many false positives. Hybrid Subdivision+SAP = "between 20 and 76 times faster than single".

## Tier 3 — Local ProjectV references

16. **`src/physics/PhysicsWorld.cpp:124`** — `kMaxPhysicsBodies = 32` (current mainline MVP scale; military sandbox needs 1000-10000).
17. **`src/physics/PhysicsWorld.cpp:133-135`** — current 2 BroadPhase layers (Static + Moving).
18. **`src/physics/PhysicsWorld.cpp:808, 3049`** — `OptimizeBroadPhase()` called after bulk operations (batch pattern correctly used).
19. **`external/JoltPhysics/Jolt/Physics/PhysicsSettings.h:84,95,102,120,123`** — Jolt defaults: `mNumVelocitySteps=10`, `mTimeBeforeSleep=0.5s`, `mPointVelocitySleepThreshold=0.03`, `mUseLargeIslandSplitter=true`, `mAllowSleeping=true`. Mainline does NOT override any of these.
20. **`agent/workspace.md §1 Phase 4`** — Incremental Jolt per-chunk wiring (closed); builds on for larger bodies.

## Tier 4 — Cross-axis closed experiments

21. **`2026-06-21-tank-terrain-interaction-physics`** (closed, yes) — vehicle physics uses Jolt — validates Jolt choice.
22. **`2026-06-21-greedy-physics-meshing-cpu`** (closed, yes) — 35× reduction in JPH CompoundShape children via greedy meshing. Reduces broad-phase body count for static voxel terrain.
23. **`2026-06-21-incremental-light-propagation`** (closed, yes) — budget BFS for light propagation; not directly related to physics but similar "expensive computation under budget" pattern.
24. **`2026-06-20-work-stealing-job-system`** (closed) — job system for parallel physics dispatch.
25. **`2026-06-21-ecs-1m-entities-bottleneck`** (closed, yes) — Flecs ECS handles 1M+ entities; relevant for future military sandbox entity count.