# STATUS — cache-oblivious-chunk-tree

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-20 — concluded. Verdict `mixed` (см. README §6). Prototype harness validated; 3 walk
seeds × 4 conditions (baseline/morton × warm/cold) measured; mean latency similar (~40-60 ns), p99 inconsistent
across seeds, cold cache unaffected. Implementation cost low but measured benefit below 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Re-evaluation trigger: `TODO.md §4.3` (128+
chunks draw distance).
**Next tick:** по запросу оператора или при re-evaluation trigger.
**Blocker:** нет

---

## Progress log

- 2026-06-20 — открыт. Прочитан `sparse-64-tree-alternatives/README.md` (continuity source — verdict=yes for
  64-tree design; next open question is *layout*, not *compression*). Прочитан `benchmarks/methodology.md` для
  harness constraints. Captured host env (AMD Ryzen 7 5800X, clang 22.1.6, L1d 32 KiB, L2 512 KiB, L3 32 MiB,
  governor=powersave — sudo needed to switch to performance).
- 2026-06-20 — topic claim per §13.1 (anti-duplicate sentinel clean — no other experiment on this axis per
  INDEX.md §5). `backlog.md` + `INDEX.md` updated in single-pass per §13.5. Sync fix r1: prior svdag claim
  not persisted to §In progress; восстановлено.
- 2026-06-20 — web research complete (8 key sources, all verified year/author/context). Key findings:
  CORoBTS (Ondráček 2024) для theoretical basis, Morton > Hilbert для practical reorder (KIT 2023), Aokana
  2025 confirms per-chunk SVDAG = primary cache-friendliness fix (layout reorder = next-step, not primary).
- 2026-06-20 — prototype complete. `prototype/cache_oblivious_layout.cpp` (546 lines, standalone C++26). Builds
  synthetic scene 24³ chunks × 8³ voxels (~33 MiB node pool, exceeds L3 32 MiB). Two layouts: baseline
  (insertion order, mirrors Sparse64Tree semantics) + Morton (post-construction reorder, two-pass with full
  remap). Random-walk access pattern. CSV + Markdown table output.
- 2026-06-20 — bench complete. N=5000 trials per (layout, cache, walk_seed), 3 walk seeds. Mean latency similar
  (~40-60 ns), p99 inconsistent (noise-dominated by OS scheduler events), cold cache unaffected. Std deviation
  60-480 ns vs mean 40-60 ns — outliers dominate tail. Detailed analysis: `prototype/RESULTS.md`.
- 2026-06-20 — concluded. Verdict `mixed`. README.md fully filled (all 8 sections + §9 mapping). Integration
  recommendation defers to `TODO.md §4.3` re-evaluation. No mainline action.

---

## Notes

- **Scope:** standalone C++26 prototype в `prototype/cache_oblivious_layout.cpp`. **No mainline changes.**
- **Adjacency to active `svdag-vs-vdb-memory-throughput`:** разные оси одного Stage 1.x storage area. Svdag
  измеряет *memory + mutation throughput* двух storage *designs*. Cache-oblivious измеряет *traversal latency*
  двух storage *layouts* поверх того же design. Non-overlapping scope, no contention.
- **Measured vs predicted:** literature (arxiv 2603.06771) predicts 25-75% cache miss reduction, up to 50%
  runtime reduction. Measured: within noise for this synthetic random-walk workload. Likely reasons (per
  RESULTS.md §4.5): random-walk pattern doesn't exercise spatial coherence; 280 B node is large (5 cache
  lines, vs SoftwareSVO's 32 B half-line); timer resolution ~30 ns masks smaller differences.
- **Acceptance target (planned vs measured):**
    - Planned: `yes` if ≥ 5% p95 reduction; `mixed` if hot-cache only; `no` if overhead.
    - **Measured: `mixed` (actually, borderline `no`).** p95 reduction 0-22% warm, but p99 inconsistent
      (sometimes worse, sometimes better). Cold cache flat. Implementation cost low, but measured benefit
      not at 5% threshold for this prototype.
- **Bench methodology per `benchmarks/methodology.md`:** N=5000 trials (vs minimum 1000), warmup=3000 steps
  (vs minimum 10), 3 walk seeds (vs single). 8 conditions × 5000 = 40K measurements. **Не использованы:**
  warm-up ≥ 3 sec timer (used 3000 step warmup); CPU pinning; governor switch (sudo); 3 runs разное время
  суток (single session). Documented в `RESULTS.md §5`.
- **Hypothesis status:** *partial confirmation.* Theoretical basis sound (Bender et al. 2002, Ondráček
  CORoBTS 2024). Practical gain not realized for synthetic random walk on scene > L3. May improve for
  spatially-coherent access (real gameplay) and/or smaller node size (32 B per Daley 2015 — out of scope).
- **What I did NOT do:**
    - Не запускал ctest / ProjectV (per AGENTS.md §2).
    - Не модифицировал `src/voxel/Sparse64Tree.hpp` (read-only reference for current layout assumption).
    - Не реализовал van Emde Boas layout (only Morton) — flagged as future R&D in integration rec.
    - Не реализовал GPU SSBO upload path (out of scope; это Stage 2.1 territory).
    - Не измерил реальный VoxelLab scene structure (synthetic random fill only).
    - Не pinned CPU / не pinned governor (sandbox constraints).
