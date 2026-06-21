# 2026-06-21-voxel-mutation-cost-characterization — STATUS

**Status:** concluded-verdict-mixed → CLOSED
**Phase:** All phases complete (1-5 done)

---

## Phase 1: Context + reservation (done)

- [x] Прочитал `docs/experiments/AGENTS.md`, `INDEX.md`, `research/backlog.md`, `hardware-profile.md`, `experiments/_TEMPLATE/README.md`, `benchmarks/methodology.md`
- [x] Anti-duplicate sentinel `§13.7`: clean — `rg "mutation.cost|mutation-cost|dirty.flag|chunk.mutation"` = only gap mentions в 3 closed experiments (`svdag-vs-vdb-memory-throughput`, `greedy-physics-meshing-cpu`, `voxel-chunk-streaming-pipeline`); no in-progress, no dedicated experiment folder, no reservation
- [x] Прочитал mainline `src/voxel/Sparse64Tree.hpp` (full, 570 LoC) — `SetCellRecursive:523-567` + `MarkNodeUnique:468-481` + `DedupPass:501-521`
- [x] Прочитал `src/voxel/VoxelWorld.hpp` (full, 160 LoC) — `VoxelChunk:47-69` + `VoxelWorld:89-108` + `pendingChunkRebuildIndices:106` + `editVersion:104`
- [x] Прочитал `src/voxel/VoxelWorld.cpp:1022-1100` — `MarkVoxelChunkDirty`, `MarkVoxelRegionDirty`, `SetVoxelMaterial`
- [x] Прочитал `src/voxel/VoxelInteraction.cpp` (full, 448 LoC) — call sites: `ApplyClassicInteraction:284+293`, `ApplyPaintInteraction:318+325`, `ApplyEraseInteraction:344`
- [x] Created README.md with reservation per `§13.2`
- [x] Claim в `research/backlog.md §In progress` (synced in INDEX.md §5 Active)

## Phase 2: Web-research (done)

- [x] Phyronnaz/HashDAG verified — Carreil 2020 TUDelft, MIT, 157 stars, persistent SVDAG with SlabHash + DyCuckoo
- [x] mathijs727/GPU-SVDAG-Editing verified — Pacific Graphics 2024, GPU editing back-end
- [x] Aokana arXiv:2505.02017 verified — Fang/Wang/Wang 2025-05-04, RTX 3060 Ti dev host, 2-4× HashDAG
- [x] dubiousconst282 2024-10-03 verified — direct SVDAG-on-64-tree edit pattern reference
- [x] Driscoll/Sarnak/Sleator/Tarjan 1989 verified — foundational persistent data structures
- [x] Sarnak/Tarjan 1986 verified — planar point location
- [x] sources.md complete — 24 sources (5 primary + 6 Tier 2 + 8 Tier 3 + 5 Tier 4 + 5 Tier 5)

## Phase 3: Prototype (done)

- [x] Standalone C++26 CPU prototype `prototype/mutation_bench.cpp` ~750 LoC
- [x] 5 strategies (A_NaiveInPlace + B_DirtyFlagDeferred + C_BatchCoalesce + D_DoubleBufferSwap + E_CopyOnWriteSnapshot)
- [x] 5 scenes × 5 mutation patterns × 5 seeds × N=1000 iter
- [x] Build green per `clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (0 warnings)
- [x] Output `prototype/build/results.csv` (626 rows = 1 header + 625 configs, 80 KB)

## Phase 4: Run + collect results (done)

- [x] Compile + run + collect CSV: **625,000 main measurements**, wall time 155 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`
- [x] `RESULTS.md` written with table + analysis (10 sections)

## Phase 5: Close (done)

- [x] `STATUS.md` → `concluded-verdict-mixed` (this file)
- [x] `INDEX.md` updated with §6 entry (sync §13.5)
- [x] `research/backlog.md §Closed` sync (move from §In progress)
- [x] Final `Integration recommendation` в README §7 — 3-step migration per `agent/knowledge.md §30.4`

---

## Headline

- **A_NaiveInPlace baseline** = 16 ns/edit (P5_StressBurst ÷ 256 edits). **NOT bottleneck** — mesh + physics rebuild dominate per closed `2026-06-21-greedy-physics-meshing-cpu` (~50 µs/chunk).
- **B_DirtyFlagDeferred** = **−58% on burst** → recommended Step 1 mainline integration.
- **D_DoubleBufferSwap** = **−45% on burst** → recommended Step 2 (atomic snapshot, Stage 1.3 async).
- **C_BatchCoalesce** = +81% (regression) → not recommended.
- **E_CopyOn+dedup** = +80,650% (catastrophic) → **NEVER for gameplay**, verify dedup OFF for dynamic chunks.

**Verdict:** `mixed` — 2 of 5 strategies cross 5% threshold per `optimization-philosophy.md`, recommended for mainline integration; 3 of 5 do not (1 regression + 1 catastrophic + baseline already fast).

**Integration recommendation:** 3-step migration ~100 LoC across 3 sessions, additive (no breaking API changes), default OFF for backward compat. Criteria: TracyPlot «Mutation Latency» −45% on burst, per-edit SVDAG < 10 ns, 0 false voxels.

---

## Last action

`Phase 5` complete: STATUS.md written, README.md updated with all 8 sections + §9 mapping, RESULTS.md written, sources.md written, sync §13.5 in INDEX.md + backlog.md pending.

**Next:** Sync INDEX.md §6 + backlog.md §Closed.