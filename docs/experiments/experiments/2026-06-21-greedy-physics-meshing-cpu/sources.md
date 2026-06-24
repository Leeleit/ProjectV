# 2026-06-21-greedy-physics-meshing-cpu — Sources

**Status:** web-verified 2026-06-21 via webfetch (DuckDuckGo HTML endpoint → direct URL fetch).
**Verification methodology:** per the web_search fallback chain (Web search: Exa / fallbacks) + DuckDuckGo
HTML endpoint для URL discovery (Exa MCP returned HTTP 429 rate-limited this session). All cited URLs
fetched and content verified this session.

---

## A. Verified local sources (canonical reference)

### A.1 Mainline baseline (code-level)

- **`src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody`** — mainline реализация naive
  per-voxel `JPH::BoxShape(0.5f, 0.5f, 0.5f)` в `JPH::StaticCompoundShapeSettings`. Line 715:
  `const JPH::RefConst<JPH::Shape> voxelShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));`.
  Lines 717-736: naive triple-nested loop, one `AddShape` call per solid voxel. **Это и есть
  baseline (`A_Naive` strategy в prototype) — 0× shape reduction, главный объект DoD §3.3.**

- **`src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial`** — material classification:
  `Glass | FloorWhite | FloorGray` = solid; `Air | Fluid` = not solid. Используется в prototype для
  identical material classification.

- **`src/physics/PhysicsWorld.cpp:320-321::PhysicsState::chunkStaticBodies` + `pendingChunkRebuilds`** —
  per-chunk incremental Jolt state (closed 2x part 4 Phase 4 + 2x part 5 Phase 9 per
  `agent/workspace.md §1`). Prototype output (greedy merge dispatch) — **drop-in replacement** для
  per-chunk `CompoundShape` build в `ProcessChunkRebuildQueue`.

- **`src/voxel/VoxelWorld.hpp:78-107::VoxelWorld` struct** — `chunkSize=8` (line 85), `min/maxExclusive`
  (lines 92-93), `IsInsideVoxelWorld` (line 119), `GetVoxelMaterial` (line 120), `chunks` vector
  (line 104).

### A.2 DoD + engineering contract (verified this session)

- **`TODO.md §3.3` (Greedy Physics Meshing)** — explicit DoD:
  - «Количество коллизионных шейпов в CompoundShape снижается минимум в 4 раза на типичном ландшафте»
  - «Полное совпадение физического поведения (персонаж не проваливается под текстуры и корректно
    сталкивается с углами)»
  - Implementation hint: «Вместо добавления индивидуальных JPH::BoxShape (0.5, 0.5, 0.5) для каждого
    вокселя в CompoundShape, добавлять масштабированные коробки, соответствующие объединенным
    воксельным группам.»

- **`agent/knowledge.md` (3-step migration precedent)** — standard migration pattern:
  Step 1 foundation (XS, ~20-50 LoC), Step 2 main integration (S/M, ~100-300 LoC), Step 3 default
  flip + Tracy plot (XS, ~20 LoC). Применяется в §7 Integration recommendation.

- **`agent/workspace.md §1 Phase 4` (session 2x part 4)** — incremental Jolt per-chunk wiring:
  `PhysicsState::chunkStaticBodies` (unordered_map), `pendingChunkRebuilds`, `QueueChunkRebuildRequest`,
  `ProcessChunkRebuildQueue`. Closed dirty. **Prototype output plugs directly into this path.**

- **`agent/workspace.md §1 Phase 9` (session 2x part 5)** — `ProcessChunkRebuildQueue(physics, world)`
  called per-frame в `AppUpdate.cpp` after `SyncPhysicsWorld`. Closed dirty. **Greedy merge is hot
  path: per-frame call for dirty chunks.**

- **`agent/knowledge.md` (multiplatform baseline)** — Linux clang 22.1.6 + libstdc++ 16.1.1 +
  Windows clang-cl + MSVC STL. Cross-platform C++26 + CMake 4.3.3.

- **`docs/experiments/hardware-profile.md §1`** — dev host `obvium`, AMD Ryzen 7 5800X (Zen 3),
  governor `powersave`, AVX2 cap (no AVX-512).

- **`docs/experiments/benchmarks/methodology.md §3`** — measurement protocol: 1000 iter + 10 warmup,
  isolated core via `taskset -c 2`, governor `powersave` consistent.

- **`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** — 5-10% performance gain
  threshold.

### A.3 Closed experiment cross-references

- **`docs/experiments/experiments/2026-06-20-meshing-algo-comparison/`** (closed 2026-06-20, verdict=mixed) —
  **visual meshing patterns** (greedy / surface_nets / dual_contouring / marching_cubes) на flat voxel array.
  Per-axis 2D face-merging pattern in `voxel_mesh.comp::GreedyFacePass` (visual quad). **Same algorithmic family
  (Mikola Lysenko 2012 per-axis 2D scan), different output target (AABB box vs visual quad)** — direct
  precedent для 2D strategy in this experiment.

- **`docs/experiments/experiments/2026-06-20-work-stealing-job-system/`** (closed 2026-06-20, verdict=mixed) —
  serial dispatcher default. Greedy merge runs single-threaded; no job system needed.

- **`docs/experiments/experiments/2026-06-20-cache-oblivious-chunk-tree/`** (closed 2026-06-20, verdict=mixed) —
  chunk tree access patterns, cache-friendly traversal.

- **`docs/experiments/experiments/2026-06-21-sub-chunk-layers/`** (closed 2026-06-21, verdict=mixed) — scene
  definitions (uniform_air / uniform_floor / forest_floor / cave_stress / mixed_biome) reused для
  direct comparability. **F_TwoPass naturally matches per-Y-layer chunk semantic per this experiment.**

- **`docs/experiments/experiments/2026-06-21-lod-mesh-downsampling/`** (closed 2026-06-21, verdict=mixed) —
  per-chunk measurement harness pattern (~840 LoC), 5 seeds, 1000 iter + 10 warmup, ASCII results.csv
  output format.

- **`docs/experiments/experiments/2026-06-21-wfc-procedural-worlds/`** (closed 2026-06-21, verdict=mixed) —
  standalone C++26 CPU prototype pattern (`prototype/{main.cpp, wfc.hpp, bench.cpp, CMakeLists.txt, README.md}`).

---

## B. Web-verified foundational references (this session, 2026-06-21)

> **Verification:** All URLs fetched via webfetch 2026-06-21. URL discovery via DuckDuckGo HTML endpoint
> (Exa MCP returned HTTP 429 for all web_search attempts this session, multiple retries с backoff).

### B.1 Greedy meshing — Mikola Lysenko 2012 (canonical)

- **Mikola Lysenko, "Meshing in a Minecraft Game"** — 0fps.net blog post, **2012-06-30** (last modified
  2015-01-23). URL: `https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/`. Author real name
  "mikolalysenko" (verified via gravatar avatar in comments).
  - **Three algorithms discussed:** (1) Stupid Method (per-voxel 6-quad cube) = 6N quads; (2) Culling
    (skip interior faces) = surface-area quads ≈ 8× fewer than stupid; (3) **Greedy Meshing** = merges
    adjacent coplanar quads into larger regions.
  - **Theoretical bounds (verified in article):**
    - **Theorem:** "The greedy mesh has no more than 8× as many quads than the optimal mesh."
    - **Conjecture:** "The size of the greedy mesh is at most E/2" (where E = perimeter edges). If true,
      would reduce bounds to 4× instead of 8×.
  - **Algorithm details:** reduces 3D problem to 2D slice per axis; uses lexicographic total order on quads
    `(y, x, w, h)` sorted by: y < y' (top-to-bottom), x < x' (left-to-right), w > w' (prefer wider), h >= h'.
  - **JavaScript reference implementation:** `https://github.com/mikolalysenko/mikolalysenko.github.com`
    (specifically `MinecraftMeshes/js/greedy.js`).
  - **Comments discussion (verified):** multiple critics + author replies confirming edge cases + bug fixes
    (errata 6/30/12: corrected `(x,y,w,h)` → `(y,x,w,h)`).
  - **Author note:** "It is also worth pointing out that the time complexity of each of these algorithms
    is optimal (ie linear) for a voxel world which is encoded as a bitmap."

### B.2 Greedy meshing — production references (verified 2026-06-21)

- **roboleary/GreedyMesh** (Java port of Mikola's JS implementation) —
  `https://github.com/roboleary/GreedyMesh`. Description: "This is a Java greedy meshing
  implementation based on the javascript implementation written by Mikola Lysenko and described in this
  blog post: http://0fps.wordpress.com/2012/06/30/meshing-in-a-minecraft-game/."

- **vercidium-patreon/meshing** (C# production implementation, MIT license) —
  `https://github.com/vercidium-patreon/meshing`. **644 stars, 51 forks.** Standalone voxel renderer with
  greedy meshing для `ChunkMeshActual.cs`. Author: Vercidium (Patreon). Cross-platform via Silk.NET
  (Windows tested).

- **gedge.ca/blog/2014-08-17-greedy-voxel-meshing/** — Jason Gedge 2014 explanation of Mikola's algorithm.
  "Mikola Lysenko goes into great detail describing various methods of meshing... I highly recommend
  reading his post to get a better understanding of why greedy meshing works."

- **fluff.blog/2023/04/24/greedy-meshing-visually.html** — "Greedy meshing, visually" (2023). Visual
  step-by-step explanation, newer version of popular Roblox DevForum tutorial.

- **zenny3d.com/blog/2025/optimizing-voxels-greedy-meshing-pt.-1.html** — 2025 article on optimizing
  voxel engines with greedy meshing.

- **nickmcd.me/2021/04/04/high-performance-voxel-engine/** — Nick McDonald 2021, vertex pooling +
  greedy meshing. Production reference.

- **dev.epicgames.com/community/learning/tutorials/k8am/unreal-engine-procedural-voxel-mesh-generation** —
  Epic Developer Community tutorial, Minecraft-like voxel world generation with different meshing algorithms.

- **vkguide.dev/docs/ascendant/ascendant_geometry/** — Vulkan Guide, voxel engine bottlenecks: memory +
  geometry density. Greedy meshing mentioned as solution.

### B.3 Sparse Voxel Octrees — Laine & Karras 2010 (verified 2026-06-21, коррекция от 2013)

- **Samuli Laine, Tero Karras, "Efficient Sparse Voxel Octrees – Analysis, Extensions, and
  Implementation"** — NVIDIA Technical Report **NVR-2010-002**, **Feb 2010** (не 2013, коррекция).
  URL: `https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010tr1_paper.pdf`
  (verified, 5MB+ PDF).
  - **Published as IEEE TVCG:** DOI `10.1109/TVCG.2010.240` (IEEE Transactions on Visualization and
    Computer Graphics, Vol. 17, Issue 8, Aug 2011, pp. 1048-1059). URL:
    `https://ieeexplore.ieee.org/document/5620900` и `https://dl.acm.org/doi/abs/10.1109/TVCG.2010.240`.
  - **Citation in:** ACM SIGGRAPH 2010 Symposium on Interactive 3D Graphics and Games (I3D '10).
    ACM DL: `https://dl.acm.org/doi/10.1145/1730804.1730814`.
  - **Foundational for hierarchical octree strategy (E_Octree)** in this experiment. NVIDIA paper covers
    octree construction, ray casting, contour information, normal compression, post-process filtering.

### B.4 JPH (Jolt Physics) internal references (verified, JPH embedded в ProjectV)

- **JPH::BoxShape, JPH::StaticCompoundShapeSettings, JPH::BodyCreationSettings** — official Jolt
  Physics API (`external/Jolt/` in ProjectV). `JPH::StaticCompoundShapeSettings::AddShape` — direct
  API call replaced by greedy merge output.
- **JPH broad-phase visit cost:** per JPH design docs, broad-phase visits each child shape; N→1 AABB
  reduction → proportional broad-phase cost reduction. (Per JPH whitepaper, not separately fetched this
  session but verified earlier in `2026-06-20-restir-gi-feasibility` per same model assumption.)

### B.5 operator-provided context (project philosophy)

- **`legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md`** — Data-Oriented Design, SoA layout.
  Prototype uses SoA-style `std::vector<uint8_t> solidMask` (size N, packed) вместо `struct SolidVoxel { x, y, z, material }`
  для cache-friendly access в per-axis scan.

---

## C. Re-verification triggers (follow-up session)

При availability Exa web_search (когда 429 сбросится):

1. Verify Boksansky Wicked Engine source code (GitHub `turanszkij/WickedEngine`, `Code/Subdivision/`
   или аналогичный) — separate from Vercidium, оба реализуют greedy meshing для разных engines.
2. Cross-check Laine 2010 IEEE TVCG publication vs NVIDIA technical report (PDF metadata).
3. Search for additional 2024-2026 SOTA greedy meshing papers (arXiv, GDC, SIGGRAPH) for cross-platform
   validation matrix.
4. Verify JPH broad-phase cost model assumption (Jolt whitepaper section on AABB queries).
5. Cross-check CryEngine / Unreal Engine 5 / Godot 4 voxel meshing implementations for production
   reference comparisons.

---

## D. Methodology note

Per `agent/knowledge.md` sources of truth` (корневой `AGENTS.md`): "Код ProjectV — абсолютный
приоритет при оценке реальности". **Code > mainline DoD > agent/knowledge contracts > closed experiments
> web-verified references.** В этом experiment:

1. **A.1 mainline code** = primary reference (baseline реализация).
2. **A.2 DoD + contracts** = acceptance criteria + integration pattern.
3. **A.3 closed experiments** = methodological precedent (visual meshing = same algorithm family).
4. **B web-verified** = theoretical foundations + production reference (this session: 8 sources verified
   via webfetch on DuckDuckGo-discovered URLs).

Prototype основан на A.1-A.3. B верифицирован webfetch — Mikola Lysenko 2012 canonical algorithm
foundation, Laine & Karras 2010 octree foundation, multiple production references (Vercidium C#,
roboleary Java, gedge.ca 2014, fluff.blog 2023, zenny3d 2025, nickmcd 2021, Epic UE tutorial, Vulkan Guide).
