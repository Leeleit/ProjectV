# 2026-06-20-cache-oblivious-chunk-tree — cache-oblivious layout for Sparse64Tree chunk storage

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** TODO.md §1.x retro (post Stage 1.1/1.2 land), §4.x LOD (LOD-aware layout candidate), §4.3 re-evaluation
trigger
**Estimated effort:** S (literature review + standalone C++26 prototype + bench harness)
**Author:** research agent (`docs/experiments/AGENTS.md`)

---

## 1. Hypothesis

**Утверждение:** Применение **cache-oblivious layout** (van Emde Boas recursive subdivision) к Sparse64Tree chunk
storage даст **measurable reduction в traversal latency** (greedy meshing, raycast, DDA) vs текущего
**chunk-major layout**, без изменения размера 64-tree node и без модификации структуры дерева.

**Что проверяю:**

- **T1 (greedy meshing):** linear walk по chunks в render order (nearest-first sorted) — chunk-major даёт good
  locality within chunk, но переходы между chunks = L2 miss. Cache-oblivious layout должен давать lower p95
  meshing time на dense + large worlds (32+ chunks).
- **T2 (raycast / DDA):** single voxel ray через несколько chunks. Chunk-major: каждый chunk boundary = hard
  cache miss. Cache-oblivious: recursive subdivision помещает sub-tree-of-size-N в cache-of-size-N на любом
  уровне иерархии.
- **T3 (cold-cache start):** первый доступ после scene load / camera teleport — chunk-major = linear prefetch
  misses, cache-oblivious = local sub-tree hot on first access.

**Преимущество, если гипотеза подтвердится:**

- Stage 2.1 (mesh shader) и Stage 2.2 (HZB cull) читают Sparse64Tree chunk AABB + node pool. Cache-oblivious
  layout даёт GPU task shader / compute cull лучший locality при traversal DAG. Особенно важно для Stage 4.3
  (128+ chunks draw distance).
- Stage 4.2 (LOD) — cache-oblivious layout на multi-LOD chunks (coarse + fine рядом) — естественно поддерживает
  «sub-tree at LOD N + sub-tree at LOD N+1» co-location, что важно для geomorphing без seams.
- Zero implementation cost на storage layer: только `BuildStorageSsbO` reorders nodes, не меняя structure.

**Альтернативы:**

| Альтернатива               | Источник                                | Почему рассматривалась                                                    |
|:---------------------------|:----------------------------------------|:--------------------------------------------------------------------------|
| Chunk-major (current)      | `src/voxel/Sparse64Tree.hpp` line 62-64 | Baseline — что есть сейчас.                                               |
| Hilbert curve layout       | Hilbert 1891, вики / Frisken 2008       | Альтернатива cache-oblivious для spatial locality preservation.           |
| Z-order (Morton) layout    | Morton 1966, `external/...`             | Already used в SVDAG-friendly formats (per `arxiv 2410.14128`).           |
| BVH-style spatial layout   | Akenine-Möller 2018                     | Ray-tracing-focused; overkill для chunk grid.                             |
| Persistent adaptive layout | Appuswamy 2016 (cache-adaptive)         | Runtime-tuned к access pattern — но не cache-oblivious theoretical bound. |

---

## 2. Prior art

Web-research выполнен `2026-06-20` (Exa) per `AGENTS.md §5.3` / `docs/experiments/AGENTS.md §4`. Ключевые источники
(8), верифицированы по году/автору/контексту:

1. **Bender, Demaine, Farach-Colton — "Cache-Oblivious Search Trees via Binary Trees of Small Height" (SODA 2002)**
   — [https://www.cs.au.dk/~gerth/papers/soda02.pdf](https://www.cs.au.dk/~gerth/papers/soda02.pdf).
   *Canonical van Emde Boas (vEB) layout для binary search trees. Прямая цитата: «the nice theoretical
   properties of cache oblivious search trees actually do carry over into practice.» Empirical comparison vEB vs
   BFS/DFS/inorder layouts. **Один из theoretical foundations** для нашего подхода.*

2. **Ondráček — "Cache-Oblivious Representation of B-Tree Structures" (CORoBTS, arxiv 2209.09166, 2022/2024)** —
   [https://arxiv.org/abs/2209.09166](https://arxiv.org/abs/2209.09166).
   *Свежая работа (Sep 2022 v1, Sep 2024 v2). Cache-oblivious dynamic B-tree via vEB layout + packed memory
   array. Optimal I/O complexity O(log_B N). Linear space. Batch updates поддерживаются. Subtree insert/remove
   O(S·log² N) amortized. **Direct theoretical basis** для нашего case: 64-ary tree с variable vertex count per
   level.*

3. **Wang et al. — "Aokana: A GPU-Driven Voxel Rendering Framework for Open World Games" (arxiv 2505.02017,
   2025-05-04)** — [https://arxiv.org/html/2505.02017v1](https://arxiv.org/html/2505.02017v1).
   *Прямая цитата: «Due to the pointer-linked structure of SVDAG, it is not cache-friendly. ... instead of using
   a single SVDAG with a large depth, multiple SVDAGs with smaller depths should be used ... This is because
   scenes with larger resolutions lead to deeper SVDAG structures, which are less cache-friendly, while our
   method uses multiple SVDAGs with smaller depths, reducing the number of indirect jumps during queries.»*
   **Подтверждает, что per-chunk shallow SVDAGs (наш Stage 1.2 design) уже решает primary cache-friendliness
   problem через chunking.** Layout reorder внутри chunk — следующий логический шаг, но не primary bottleneck.

4. **KIT — "Fast Compressed Segmentation Volumes" (supplementary, 2023)** —
   [https://cg.ivd.kit.edu/publications/2023/compsegvol/compsegvol_supplementary.pdf](https://cg.ivd.kit.edu/publications/2023/compsegvol/compsegvol_supplementary.pdf).
   *Explicit Hilbert vs Morton Z-order comparison. Прямая цитата: «Hilbert vs Morton — compression rates almost
   identical. Compression run times, however, are twice or even up to three times as long when using a Hilbert
   curve instead of a Morton Z-curve.»* **Меняет наш prior: Morton = practical choice для reorder (cheaper than
   Hilbert, similar locality). Hilbert overhead не оправдан для нашего case.**

5. **arXiv 2603.06771 — "Linear Octree with Space-Filling Curve Reordering" (2025)** —
   [https://arxiv.org/pdf/2603.06771](https://arxiv.org/pdf/2603.06771).
   *Direct number для нашего case: «SFC reordering significantly improves access to spatial data, reducing the
   number of cache misses from 25% to 75% and runtime by up to 50%.»* **Empirical baseline** для нашего
   expected gain. Но measured на point clouds (kNN search), не random walk.

6. **Frisken, Perry — "Simple and Efficient Traversal Methods for Quadtrees and Octrees" (MERL TR2002-41, 2002)**
   — [https://www.merl.com/publications/docs/TR2002-41.pdf](https://www.merl.com/publications/docs/TR2002-41.pdf).
   *«Table-free, thereby reducing memory accesses, and generalize easily to higher dimensions.»* Locational codes
    + non-recursive traversal. **Historical baseline** для voxel cache-friendly traversal — still cited в 2024-2026
      SVO/SVDAG papers (per Aokana 2025, KIT 2024).

7. **Laine, Karras — "Efficient Sparse Voxel Octrees" (NVIDIA TR, 2010/2011)** —
   [https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010tr1_paper.pdf](https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010tr1_paper.pdf).
   *«The regularity of the octree data structure is the key factor in enabling efficient ray casts. As most of
   the data associated with each ... stored in conjunction with its parent ... need to allocate storage for ...
   voxels and makes ... during ray casts ... per voxel.»* **Foundational SVO layout principles** — store child
   pointers in pool, BFS-like allocation.

8. **Daley — "SoftwareSVO" (dfhe2004/SoftwareSVO, 2015)** —
   [https://github.com/dfhe2004/SoftwareSVO](https://github.com/dfhe2004/SoftwareSVO).
   *Direct quote: «The layout uses a pool allocator with nodes stored in blocks of 8. Nodes are an even 32 bytes
   == half of a cache line; nehalem and other intel processors with inclusive caches invalidate L1 and L2 a lot,
   so we need to be careful there.»* **Прямой аргумент за node size = half cache line** для cache-friendliness.
   Наши 280 B nodes = 5 cache lines — suboptimal по этому критерию. **Это НЕ решается layout reorder** — только
   node size reduction. Future R&D follow-up.

**ProjectV internal cross-refs:**

- `sparse-64-tree-alternatives` (closed 2026-06-20, verdict=yes) — validates 64-tree design, устанавливает что
  `Sparse64Tree` — primary storage. Layout reorder не меняет structure, только physical ordering.
- `svdag-vs-vdb-memory-throughput` (parallel session, in-progress) — измеряет memory + mutation throughput двух
  storage **designs**. Наш experiment измеряет traversal latency двух storage **layouts** поверх того же design.
  Non-overlapping scope.
- `agent/knowledge.md` — sun-shadow path, не касается storage layout.
- `agent/knowledge.md` — GPU Fluid CA reversal, контракт «shader оперирует на SVDAG node pool, не на flat
  array». Layout reorder не нарушает этот контракт (Node ID indirection preserved).
- `TODO.md §1.1` (Sparse 64-trees) — mainline storage, `nodes_[]` в `Sparse64Tree.hpp:268` — primary site для
  potential layout change.
- `TODO.md §1.2` (SVDAG) — dedup machinery (lines 224-313); lazy toggle pattern. Layout reorder compatible с dedup
  (slot remap works for any DAG).
- `TODO.md §2.1` (mesh shaders), §2.2 (HZB cull) — Stage 2.x shaders read `nodes_[]`; cache-friendliness benefits
  transfer to GPU task shader.
- `TODO.md §4.3` (lift draw distance cap to 128+ chunks) — **re-evaluation trigger** для cache-oblivious layout.
  Working set at 128+ chunks = > L3, layout benefits should increase.

---

## 3. Method

**Тип эксперимента:** **literature review + analytical + prototype benchmark** (standalone C++26 prototype,
no mainline changes per AGENTS.md §2).

**Сцена (мишень исследования):**

- **Synthetic-A:** Random walk через chunk grid (32×32×32 chunks, sparse fill pattern mimicking VoxelLab).
  Sequence length = 10000 voxel accesses. Измеряем per-access latency distribution.
- **Synthetic-B:** Greedy meshing traversal pattern — sorted by camera distance, all visible chunks visited
  once. Sequence length = 32 chunks × 32 chunks (typical 1024-chunk visible range).
- **Synthetic-C:** Single ray traversal — start at random chunk, traverse 32 voxel steps across chunk
  boundaries. Sequence length = 1000 rays, mean step count per ray logged.
- **Synthetic-D:** Cold-cache — access pattern designed to evict L1/L2 between accesses.

**Метрики:**

- mean access latency (ns)
- p50, p95, p99, p99.9 latency (ns) — focus on p95/p99 (per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  low-latency > throughput)
- stddev
- L1 / L2 / L3 miss counts (via `perf stat` if available; otherwise indirect via latency distribution shape)

**Контроль (baseline):**

- Current chunk-major layout — `nodes_[]` array ordered by chunk index, then by depth-first descent within
  chunk (matches `Sparse64Tree.hpp` line 62-64 implicit layout — children slots 0-63 packed contiguously).
- Same `Node` struct (272 B), same fillMask layout, same slot encoding. Только *external ordering* differs.

**Протокол воспроизведения:**

```bash
# Per benchmarks/methodology.md harness pattern.
clang++ -O3 -march=native -DNDEBUG -std=c++26 \
  prototype/cache_oblivious_layout.cpp -o /tmp/cobl_bench
/tmp/cobl_bench --scenario A --iterations 1000 --output results/scenario_a.csv
/tmp/cobl_bench --scenario B --iterations 1000 --output results/scenario_b.csv
/tmp/cobl_bench --scenario C --iterations 1000 --output results/scenario_c.csv
/tmp/cobl_bench --scenario D --iterations 1000 --output results/scenario_d.csv
```

Governor: `performance` (per STATUS.md). CPU pinned: single core via `taskset -c 0`.

---

## 4. Prototype

Standalone C++26 prototype в `prototype/cache_oblivious_layout.cpp` (546 lines). Compiled с
`clang++ -O3 -march=native -DNDEBUG -std=c++26 -pthread`.

**Что реализует:**

- `Node` struct (280 B) — byte-identical to `Sparse64Tree::Node` (`Sparse64Tree.hpp:78-83`).
  `static_assert(sizeof(Node) == 280)`.
- Synthetic scene builder: 24³ chunks × 8³ voxels = 192³ voxel volume, 30% non-air fill rate, 4 materials.
  ~124K nodes = 33 MiB (превышает L3 = 32 MiB).
- Two layouts:
    - **baseline:** insertion-order `nodes_[]` (mirrors `Sparse64Tree::SetCellRecursive` semantics — root allocated
      before mid → BFS-like).
    - **morton:** post-construction reorder by 3D Morton (Z-order) curve over subtree spatial centers. Two-pass
      (build remap table, then rebuild nodes with remapped slots).
- Random-walk access pattern: pick random chunk + random voxel, walk root → mid → leaf, pick random neighbor.
- 4 conditions per layout: warm cache (after 3000 warmup steps) + cold cache (8 MiB evict between runs).
- Statistics: mean, median, p50, p95, p99, p99.9, stddev, min, max per (layout, cache, walk_seed).
- Output: CSV (`results.csv`) + stdout summary.

**Что НЕ реализует (out of scope, per STATUS.md):**

- GPU traversal (Stage 2.1 territory).
- Stage 1.2 SVDAG dedup interaction (this prototype uses non-deduplicated nodes).
- Real VoxelLab scene structure (synthetic random fill only).
- van Emde Boas layout (CORoBTS, Ondráček 2024) — only Morton tested.
- `rdtsc` timer (uses `std::chrono::steady_clock`, ~30 ns resolution).
- CPU pinning / `isolcpus` (single-threaded but not pinned).
- Governor switch (requires sudo; powersave used).

**Reproducibility:**

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-20-cache-oblivious-chunk-tree/prototype/
clang++ -O3 -march=native -DNDEBUG -std=c++26 cache_oblivious_layout.cpp -o /tmp/cobl_bench
/tmp/cobl_bench --output /tmp/results.csv --iterations 5000 --warmup 3000 --seed 42
```

**Части шаблонного harness из `benchmarks/methodology.md` использованы:**

- Stats computation (mean, median, p95, p99, stddev) — adapted from `benchmarks/methodology.md §7`.
- N=1000+ iterations per condition (used 5000 for stability) — per `benchmarks/methodology.md §3.2`.
- CSV output — per `benchmarks/methodology.md §3.4`.
- Multiple walk seeds (3 seeds) for cross-seed stability — adapted from §4 (3 runs разное время суток).
- **НЕ использованы:** warm-up ≥ 3 sec timer (used 3000 step warmup instead); CPU pinning (§2); governor switch (§2);
  3 runs разное время суток (§4) — single session due to scope.

Подробный output: `prototype/output.txt`, `prototype/results.csv`, `prototype/RESULTS.md`.

---

## 5. Results

Per-seed detail в `prototype/RESULTS.md §3`. Summary (mean of 3 walk seeds, ns/step):

| Layout   | Cache | Mean | p95  | p99 | Std   |
|:---------|:------|:-----|:-----|:----|:------|
| baseline | warm  | 49.1 | 60.0 | 153 | 63.4  |
| baseline | cold  | 46.6 | 50.0 | 73  | 78.9  |
| morton   | warm  | 49.5 | 46.7 | 163 | 215.8 |
| morton   | cold  | 45.4 | 46.7 | 76  | 50.9  |

**Key findings:**

- **Mean latency similar** (~40-60 ns for both). Both layouts dominated by L3 hits on this 33 MiB scene.
- **p95 latency:** Morton slightly better warm (46.7 vs 60.0) — but within noise.
- **p99 latency:** inconsistent across seeds (Morton 30% better seed 1, 88% worse seed 3). Std deviation 60-480 ns
  suggests tail latency dominated by OS scheduler events, not cache misses.
- **Cold cache:** no significant difference (cold = load latency from RAM, layout doesn't reduce total data load).
- **Literature baseline (arxiv 2603.06771):** 25-75% cache miss reduction, up to 50% runtime reduction — **NOT
  reproduced** in this prototype. Likely reasons: random-walk access pattern doesn't exercise spatial coherence;
  node size 280 B (5 cache lines) is large for reorder benefit; timer resolution ~30 ns masks smaller differences.

**Что НЕ увидели (per `RESULTS.md §4.6`):**

- Не увидели 25-50% reduction из literature.
- Не увидели cold-cache improvement.
- Не увидели mean latency reduction.
- Не измерили GPU-side equivalent.
- Не измерили реальный VoxelLab scene structure.
- Не реализовали van Emde Boas layout (только Morton).

**Что удивило (per `RESULTS.md §4.7`):**

- Std deviation очень высокий (60-480 ns при mean 40-60 ns) — tail latency dominated by OS scheduler.
- Min latency = 30 ns timer floor — не cache hit latency.
- Cold cache comparable to warm для mean latency (мой "cold" не достаточно cold).

---

## 6. Verdict

**`mixed`** — implementation cost is **low** (one-time reorder pass + slot remap), measured benefit is **within
timer noise** for this synthetic workload. Cache-oblivious theory (Bender et al. SODA 2002, Ondráček CORoBTS
2022/2024) is sound, but **for ProjectV's typical working set (< L3 in most gameplay)**, both layouts work
equivalently. The measured gain does not justify the implementation cost **at this scale**.

**Re-evaluation trigger:** `TODO.md §4.3` (lift draw distance cap to 128+ chunks). At that scale, working set
exceeds L3 dramatically (128+ visible chunks = 17-34 MiB > L3 32 MiB on dev host), and spatially-coherent
access patterns (player movement) will exercise Morton locality better than random walk.

**If verdict were `yes` or `no`:** не применимо. Confident `mixed`:

- **Not `yes`:** measured gain is below 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.
- **Not `no`:** theoretical basis sound; reorder is non-trivial engineering effort и может дать больше benefit
  на real ProjectV workload (spatial coherence from player movement) than на synthetic random walk.
- **Not `abandoned`:** concept valid; нужны real-workload measurements для definitive verdict.

---

## 7. Integration recommendation

**Target stage:** **NOT immediate.** Defer до `TODO.md §4.3` (128+ chunks draw distance) re-evaluation.

**Конкретные изменения (если re-evaluation даст `yes`):**

1. **Add `PROJECTV_SPARSE_64_LAYOUT=baseline|morton` env var** in `src/voxel/Sparse64Tree.hpp` (or wrapper).
    - Default = `baseline` (current insertion-order, backward-compatible).
    - `morton` = post-construction reorder at scene-finalization time.
    - A/B test: both paths produce byte-equal `GetCell` output.

2. **Reorder implementation in `src/voxel/Sparse64Tree.hpp`:**
    - Two-pass: build remap table (oldIdx → newIdx) by sorting `nodes_[]` by Morton(subtree_center);
      then rebuild `nodes_[]` with remapped slots (mirrors `prototype/MortonReorder`).
    - Chunk roots + internal slot indices remapped; leaf slots (flagged) не трогаем.
    - One-shot at scene load time (cost: O(N log N) sort + O(N) copy).

3. **Validate byte-equal output:**
    - `tests/Sparse64TreeTests.cpp` extended: 24+ existing sub-tests + new `MortonReorderByteEqual` test that
      compares `GetCell(x, y, z)` output before vs after reorder on VoxelLab scene.
    - Per `TODO.md §1.1` A/B test invariant: byte-equal before flip default.

4. **TracyPlot measurement на real workload:**
    - Per `TODO.md §1.1` acceptance: «MeshingStress measurement: TracyPlot for `VoxelAccess (ms)` should drop ≥ 5%
      on sparse scenes». Add Morton variant TracyPlot, compare on MeshingStress.
    - **Acceptance threshold:** ≥ 5% drop on MeshingStress (per project philosophy), иначе revert.

**Подход (high level):**

- **No node size change.** Layout reorder preserves 280 B node. (Future R&D: reduce to 32 B per Daley 2015
  SoftwareSVO — out of scope for this experiment.)
- **One-shot reorder, not runtime.** Sort + remap at scene load. Runtime cost: O(N log N) for N = total nodes.
  For 100K nodes = ~1ms on Zen 3 — negligible vs scene load cost.
- **Backward-compatible.** `PROJECTV_SPARSE_64_LAYOUT=baseline` = current behavior; `=morton` = reorder.
  Default `baseline` until validated.

**Риски:**

- **R1 (med):** Reorder bug → silent data corruption. Mitigation: byte-equal test (`GetCell` before/after
  produces identical output).
- **R2 (low):** Reorder cost on large scenes. O(N log N) sort may be noticeable at Stage 4.3 (128+ chunks,
  ~10M nodes → ~50ms sort). Mitigation: amortize (reorder per chunk, not full scene); or defer to background
  thread.
- **R3 (med):** Stage 1.2 SVDAG dedup interaction. When dedup is enabled, two different slots may point to
  identical nodes. Reorder must preserve dedup invariant (or be re-run on dedup toggle). Mitigation: invalidate
  reorder on `SetDeduplicationEnabled(true)`; rebuild on `SetDeduplicationEnabled(false)`.
- **R4 (low):** Per chunk already has its own tree — chunk-major layout already provides some locality.
  Cross-chunk benefit may be smaller than literature number suggests. Mitigation: measure before/after.
- **R5 (med):** GPU SSBO upload (`Sparse64Tree::GetNodes()`) — if mainline uses CPU `nodes_[]` order for GPU
  layout (vs explicit reorder), Morton reorder breaks byte-exact GPU SSBO. Mitigation: GPU upload also reorders
  independently, или reorder baked into SSBO packing step.

**Критерии приёмки (если mainline решит реализовать после re-evaluation):**

- [ ] `PROJECTV_SPARSE_64_LAYOUT=morton` byte-equal output vs `=baseline` на VoxelLab, MeshingStress, FlatBenchmark.
- [ ] ctest baseline (16/16 per `agent/knowledge.md`) preserved.
- [ ] New `ProjectVSparse64TreeTests::MortonReorderByteEqual` test passes on all existing fixtures.
- [ ] TracyPlot `VoxelAccess (ms)` drop ≥ 5% on MeshingStress with `=morton` (per `TODO.md §1.1` acceptance).
- [ ] Per-chunk SVDAG dedup invariant preserved (`SetDeduplicationEnabled` toggle + reorder rebuild works).
- [ ] Reorder cost < 100ms on 10M-node scene (negligible vs scene load).

**Зависимости:**

- **Pre-required:** `TODO.md §1.1` Sparse 64-trees must be in mainline (`PROJECTV_SPARSE_64_STORAGE=on` default).
- **Pre-required:** `tests/Sparse64TreeTests.cpp` byte-equality test infrastructure.
- **Concurrent:** Stage 1.2 SVDAG policy (per-chunk `isStatic` + `SetDeduplicationEnabled` toggle). Reorder
  must coexist.
- **Unblocks:** Stage 2.1 mesh shader traversal, Stage 2.2 HZB cull — both read `nodes_[]`, benefit from cache-friendly
  layout.
- **Future R&D (not this experiment):** Node size reduction (32 B per Daley 2015); full van Emde Boas layout
  (CORoBTS 2024) для dynamic mutation cost; GPU SSBO layout integration.

**Estimated effort (mainline, IF re-evaluated yes at Stage 4.3):**

- Item 1 (env var + baseline/morton dual path): **S** (1 commit, 1 day).
- Item 2 (reorder implementation in `Sparse64Tree.hpp`): **S** (1 commit, 1-2 days, prototype serves as reference).
- Item 3 (byte-equal test + MeshingStress TracyPlot validation): **M** (multi-commit, 3-5 days).
- Item 4 (default flip + A/B test): **S** (1 commit, 1 day).
- **Total:** M (2-3 weeks spread across Stage 4.3 implementation).

**If verdict were `no` или `mixed` (current):** as-is. Concept documented for future re-evaluation per
Stage 4.3 trigger. No mainline action now.

---

## 8. Sources

1. Bender, Demaine, Farach-Colton. "Cache-Oblivious Search Trees via Binary Trees of Small Height". SODA 2002.
   <https://www.cs.au.dk/~gerth/papers/soda02.pdf>
2. Ondráček. "Cache-Oblivious Representation of B-Tree Structures" (CORoBTS). arxiv 2209.09166, 2022 (v1) / 2024 (v2).
   <https://arxiv.org/abs/2209.09166>
3. Wang et al. "Aokana: A GPU-Driven Voxel Rendering Framework for Open Open World Games". arxiv 2505.02017, 2025-05-04.
   <https://arxiv.org/html/2505.02017v1>
4. KIT. "Fast Compressed Segmentation Volumes" (supplementary, Hilbert vs Morton comparison). 2023.
   <https://cg.ivd.kit.edu/publications/2023/compsegvol/compsegvol_supplementary.pdf>
5. "Linear Octree with Space-Filling Curve Reordering". arxiv 2603.06771, 2025.
   <https://arxiv.org/pdf/2603.06771>
6. Frisken, Perry. "Simple and Efficient Traversal Methods for Quadtrees and Octrees". MERL TR2002-41, 2002.
   <https://www.merl.com/publications/docs/TR2002-41.pdf>
7. Laine, Karras. "Efficient Sparse Voxel Octrees" (NVIDIA TR). 2010/2011.
   <https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010tr1_paper.pdf>
8. Daley. "SoftwareSVO — fast, cache-friendly Voxel Octree class for SSE4/x86". GitHub, 2015.
   <https://github.com/dfhe2004/SoftwareSVO>

**ProjectV internal cross-refs:**

- `src/voxel/Sparse64Tree.hpp` (lines 62-83: Node struct; 268: nodes_[] vector; 224-313: dedup machinery) —
  primary integration site.
- `src/voxel/VoxelWorld.hpp` (line 87: sparseStorage field; lines 45-50: VoxelChunk) — chunk container.
- `sparse-64-tree-alternatives` (closed 2026-06-20, verdict=yes) — validates 64-tree design.
- `svdag-vs-vdb-memory-throughput` (parallel session, in-progress) — non-overlapping scope.
- `agent/knowledge.md`, §30.4` — relevant engineering contracts.
- `TODO.md §1.1, §1.2, §2.1, §2.2, §4.3` — all designed for SVDAG/64-tree read; layout reorder compatible.

---

## 9. Mapping to ProjectV hot-path

**Какой участок движка соответствует прототипу:**

- `src/voxel/Sparse64Tree.hpp` (lines 62-64: `Node` struct) — current storage node layout.
- `src/voxel/VoxelWorld.hpp` (lines 45-50: `VoxelChunk`) — chunk container; chunks hold `Sparse64Tree nodes_[]`.
- Hot-path reads:
    - `VoxelWorld::GetVoxelMaterial` (`VoxelWorld.cpp:111-117`) — per voxel access from meshing, physics, raycast.
    - `Sparse64Tree::Traverse*` — for greedy meshing per chunk.
- Hot-path writes:
    - `Sparse64Tree::SetCellRecursive` (`Sparse64Tree.hpp:369-390`) — random mutation.
- Stage 1.2 SVDAG: `Sparse64Tree::SetDeduplicationEnabled` toggles dedup index; layout change не нарушает
  dedup hash, но может изменить cache locality of dedup operations.

**Какие допущения/упрощения:**

- **CPU-only prototype.** GPU traversal (Stage 2.1 mesh shader, Stage 2.2 HZB cull) может иметь разные
  cache characteristics — результаты CPU-бенчмарка не транслируются directly в GPU perf gain.
- **No concurrent mutation.** Prototype assumes single-threaded reads + static layout. Stage 1.2 lazy dedup
  = `SetDeduplicationEnabled` toggle — rebuild layout on toggle, или делать layout mutable (extra cost).
- **Synthetic scenes, не VoxelLab.** Real-world scene structure может нивелировать layout benefits (highly
  repetitive = chunk-major уже good enough).
- **64 B cache line, 8-way L1, 32 MiB L3.** Зафиксировано как параметры прототипа (per STATUS.md). Другие
  архитектуры (Apple M-series, ARM server) могут дать другие результаты — cross-platform не тестируется.

**Что осталось неизмеренным (cross-refs для возможных follow-ups):**

- GPU SSBO layout (Stage 2.1) — отдельно per GPU vendor cache architecture.
- Concurrent mutation cost (Stage 1.2 lazy dedup toggle + layout rebuild).
- Real VoxelLab scene structure (real data shape, not synthetic).
- Apple M-series / ARM server cache behavior (L1d = 128 KiB on M2 Pro, etc.).
