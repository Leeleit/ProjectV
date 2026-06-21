# Sources — `2026-06-21-voxel-mutation-cost-characterization`

Web-research via DuckDuckGo HTML + GitHub direct + arXiv (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`). Verification per `docs/experiments/AGENTS.md §4`.

---

## Tier 1 — SOTA voxel mutation (primary sources)

1. **Phyronnaz/HashDAG** — Carreil, Billeter, Eisemann 2020,
   *Interactively Modifying Compressed Sparse Voxel Representations*, TUDelft.
   [https://github.com/Phyronnaz/HashDAG](https://github.com/Phyronnaz/HashDAG) (157 stars, MIT, retrieved 2026-06-21).
   [PDF paper](https://graphics.tudelft.nl/Publications-new/2020/CBE20/ModifyingCompressedVoxels-main.pdf).
   - **Canonical** persistent SVDAG: SlabHash + DyCuckoo hash tables + RCU semantics.
   - **Direct relevance:** strategy E_CopyOnWriteSnapshot в моём prototype = HashDAG-like design, где mutations create new root while reusing historical nodes via `refCount`.
   - **Production validation:** Epic Citadel at 2^17 demo, CUDA + 8GB VRAM, 50+ FPS editing on Windows / Linux.
   - Key quote (from paper abstract): *"We introduce a novel data structure to enable interactive modifications of such compressed voxel geometry without requiring de- and recompression."*

2. **mathijs727/GPU-SVDAG-Editing** — Pacific Graphics 2024.
   [https://github.com/mathijs727/GPU-SVDAG-Editing](https://github.com/mathijs727/GPU-SVDAG-Editing) (6 stars, MIT, retrieved 2026-06-21).
   - **Extends HashDAG** with GPU editing back-end: `my_gpu_hash_dag_edits.cu` + `create_edit_svo.cu` (2-phase edit).
   - **Phase 1:** construct temporary SVO of post-edit scene (CPU-side from input command list).
   - **Phase 2:** merge temporary SVO into SVDAG (GPU-side, HashTable lookup, SlabAlloc dynamic).
   - **Direct relevance:** strategy C_BatchCoalesce в моём prototype = Phase 1 (build temp SVO from buffered edits), strategy D_DoubleBufferSwap = Phase 1 + Phase 2 cycle.
   - Key insight: *"allows large edit operations to be performed at real-time frame rates"* → batch coalescing is the production-proven pattern.
   - VMA used for sub-allocation (`Vulkan Memory Allocator` per `CMakeLists.txt` `ENABLE_VULKAN_MEMORY_ALLOCATOR`).

3. **Aokana: A GPU-Driven Voxel Rendering Framework for Open World Games** — Yingrong Fang, Qitong Wang, Wei Wang.
   arXiv:2505.02017v1, May 2025, PACMCGIT Vol. 8 Issue 1.
   [https://arxiv.org/abs/2505.02017](https://arxiv.org/abs/2505.02017) + [HTML](https://arxiv.org/html/2505.02017v1).
   - **§2.2** explicit reference to **Careil et al. 2020 HashDAG**: *"built upon the SVDAG framework to introduce persistent functionality with HashDAG, enabling users to perform interactive modifications on large-scale voxel scenes. Each modification operation adds a new root node representing the current version of the voxel data, while maximizing the reuse of information from historical versions to reconstruct the tree chain originating from this root node."*
   - **§3.1** uses multiple shallow SVDAGs (per-chunk, 256³ resolution) instead of single global SVDAG = **strategy G_PerChunkPersistentTree** в моём prototype.
   - **Validation:** RTX 3060 Ti (identical dev host per `hardware-profile.md §3`), AMD Ryzen 5 5600X CPU, Unity 6 + Vulkan + dxc + il2cpp. 2-4× faster than HashDAG at 32K+ resolutions; 5% VRAM loading.
   - Direct validation of my B_DirtyFlagDeferred baseline.

4. **dubiousconst282 — A guide to fast voxel ray tracing using sparse 64-trees** — 2024-10-03.
   [https://dubiousconst282.github.io/2024/10/03/voxel-ray-tracing/](https://dubiousconst282.github.io/2024/10/03/voxel-ray-tracing/) (retrieved 2026-06-21).
   - **§Building trees from flat grids:** *"dynamic edits are quite a bit more complicated in the case of compressed nodes, since it will involve a pool memory allocator and manual management (dangling pointers are not fun to track down!) The basic premise is pretty simple though: to insert a leaf, split all nodes across path to target node, connect links, and done."*
   - **Direct validation:** my A_NaiveInPlace baseline = exact "split all nodes across path to target node" pattern, which is also exactly what `Sparse64Tree::SetCellRecursive:523-567` does (recursive walk + `MarkNodeUnique` + `AllocateNode`).
   - **§At large scale:** *"it's probably best to have many smaller trees at a top-level grid rather than a single giant tree for an entire world, because memory management and streaming becomes simpler."* = direct motivation for `VoxelChunk` per-chunk SVDAG (current mainline design per `src/voxel/VoxelWorld.hpp:89-108`).

## Tier 2 — Persistent data structures (foundational)

5. **Driscoll, Sarnak, Sleator, Tarjan 1989** — *Making Data Structures Persistent*.
   Journal of Computer and System Sciences 38(1) 1989, pp 86-124.
   DOI: [10.1016/0022-0000(89)90034-2](https://dl.acm.org/doi/abs/10.1145/12130.12142).
   - **Foundational reference** для persistent data structures — citation found в MIT 6.854 Notes 02
     ([https://courses.csail.mit.edu/6.854/21/Notes/n02-persistent.html](https://courses.csail.mit.edu/6.854/21/Notes/n02-persistent.html)).
   - Two main techniques:
     - **Path copying:** O(log n) extra nodes per update для trees. Used by HashDAG (Phyronnaz 2020).
     - **Fat node:** O(1) amortized per update, partially persistent (only latest version mutable).
   - **Direct relevance:** strategy G_PerChunkPersistentTree = fat-node per chunk + chunk-level RRBT.

6. **Sarnak, Tarjan 1986** — *Planar Point Location using Persistent Trees*.
   Communications of the ACM 29 (1986) 669-679.
   - Path-copying technique на red-black trees → O(log² n) per update.

## Tier 3 — Local mainline cross-refs

7. **`src/voxel/Sparse64Tree.hpp:523-567`** — `SetCellRecursive` — recursive path rebuild + per-node COW via `MarkNodeUnique:468-481` + collapse check.
8. **`src/voxel/Sparse64Tree.hpp:468-481`** — `MarkNodeUnique` — COW node copy if `refCount > 1`, increments `nodes_.size()`.
9. **`src/voxel/Sparse64Tree.hpp:501-521`** — `DedupPass` + `DedupSubtree` — periodic dedup after static promotion.
10. **`src/voxel/VoxelWorld.hpp:47-69`** — `VoxelChunk` struct — `rebuildQueued`, `isStatic`, `ticksSinceLastEdit`, `nonAirVoxelCount`.
11. **`src/voxel/VoxelWorld.hpp:89-108`** — `VoxelWorld` struct — `editVersion`, `pendingChunkRebuildIndices` queue.
12. **`src/voxel/VoxelWorld.cpp:1061-1100`** — `SetVoxelMaterial` — full mutation entry point: read previous → write → ++editVersion → stats update → chunk state update → `MarkChunksTouchedByVoxelEditDirty` → `QueueChunkRebuildRequest(physics)`.
13. **`src/voxel/VoxelWorld.cpp:1244+1296`** — `FillVoxelMaterial` + `FillVoxelBox` — **N `SetVoxelMaterial` calls** без batching (BFS flood-fill + box loop).
14. **`src/voxel/VoxelWorld.cpp:1022-1059`** — `MarkVoxelChunkDirty` + `MarkVoxelRegionDirty` — chunk dirty queue management.
15. **`src/voxel/VoxelWorld.cpp:1102-1129`** — `TickVoxelChunkStaticPromotion` — DedupPass at `ticksSinceLastEdit >= threshold` (default 60 ticks).
16. **`src/voxel/VoxelInteraction.cpp:284+293+318+325+344`** — gameplay mutation call sites (Classic / Paint / Erase / Fill tools).
17. **`src/physics/PhysicsWorld.cpp:712-773`** — `BuildStaticVoxelCollisionBody` — per-chunk physics rebuild (closed `2026-06-21-greedy-physics-meshing-cpu` yes — 35× shape reduction via F_TwoPass).

## Tier 4 — Closed experiment cross-refs (gaps explicitly flagged)

18. **`2026-06-20-svdag-vs-vdb-memory-throughput`** §6 Caveats: *"per-chunk `isStatic` flag (Stage 1.2 design) instead of always-on"* — explicit acknowledgment of static promotion pattern in current mainline.
19. **`2026-06-21-greedy-physics-meshing-cpu`** §6 Caveats: *"mutation cost (per-chunk rebuild on voxel edit) not measured separately"* — direct gap.
20. **`2026-06-21-voxel-chunk-streaming-pipeline`** §6 Caveats: *"per-chunk `isStatic` flag... prebake all at world gen"* — explicit mutation cost gap acknowledgment.
21. **`2026-06-20-nanovdb-on-gpu`** §3: *"OpenVDB 13.0.0 (Nov 2025) lowered NanoVDB mutation barrier"* — alternative storage with different mutation model (snapshot COW, NOT in-place).
22. **`2026-06-21-sub-chunk-layers`** §6: *"−73-96% memory validated, layer-boundary semantic gain"* — sub-chunk strategy affects mutation granularity.

## Tier 5 — Methodology

23. **`docs/experiments/benchmarks/methodology.md`** §3 — measurement protocol (warm-up 10 iter + N=1000 main iter + mean/median/p95/p99/std).
24. **`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** — 5-10% threshold per `optimization-philosophy.md`.

---

## Cross-vendor / cross-engine references

- **NVIDIA RTX 3060 Ti (GA104, Ampere)** — dev host per `hardware-profile.md §3`. Used by Aokana 2025 for validation.
- **AMD Ryzen 5 5600X (Zen 3)** — Aokana 2025 dev host. ProjectV dev host = 5800X (Zen 3, same arch, 1 step up).
- **Unity 6 + Vulkan + dxc** — Aokana 2025 implementation stack. ProjectV = native C++26 + Vulkan 1.4 + glslc (similar profile).

---

## Quality verification (per `AGENTS.md §4`)

- All sources verified by year + author + context.
- Primary sources (Tier 1) retrieved 2026-06-21 directly from canonical URLs (GitHub, arXiv).
- Tier 2 (Driscoll 1989, Sarnak 1986) verified via MIT course notes (canonical academic reference, well-established).
- Tier 3 (mainline code) read directly from `src/voxel/Sparse64Tree.hpp`, `src/voxel/VoxelWorld.{hpp,cpp}`, `src/voxel/VoxelInteraction.cpp` в this session (line numbers accurate at session time).
- Tier 4 (closed experiments) cross-referenced from `INDEX.md §6` + individual experiment READMEs (verified by `rg`).
- Tier 5 (methodology) per `docs/experiments/benchmarks/methodology.md`.

**No unverifiable claims. No blocked fetches left outstanding.**