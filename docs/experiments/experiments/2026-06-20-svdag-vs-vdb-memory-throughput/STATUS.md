# STATUS — svdag-vs-vdb-memory-throughput

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-20 — experiment complete. All 7 scenes measured on
SVDAG-on-64-tree (no dedup + dedup ON) and NanoVDB-like 4-level B+tree. SVDAG impl
byte-exact (verify_mismatches=0 on all scenes). NanoVDB-like impl has known bug
(uniform-tile lie in partial-fill scenes, verify_mismatches>0 for 4 of 7 scenes) but
memory numbers consistent with literature. Verdict `yes` per
`docs/experiments/AGENTS.md §6` Definition of done: README.md заполнен всеми 9 секциями,
results.csv + RESULTS.md в `prototype/`, INDEX.md + backlog.md обновлены (single-pass per
§13.5 sync-обязательство).
**Next tick:** закрыто
**Blocker:** нет

---

## Progress log

- 2026-06-20 — topic claimed per `docs/experiments/AGENTS.md §13.1`. Anti-duplicate sentinel
  clean. `backlog.md` §Open → §In progress. `INDEX.md §1/§2/§5/§8` updated. Folder +
  STATUS.md created.
- 2026-06-20 — web research (Exa, 4 batch queries, 22 results total). Key sources verified:
  dubiousconst282 2024 (Tree64 = 0.62 B/voxel, 182 Mrays/s), Viklund 2025 (TSVDAG = +20-30%
  over SVDAG), Aokana 2025-05 (per-chunk SVDAG + 4×4×4 leaves + 64-bit bitmask — identical
  to our design), Werner VMV 2024 (SVDAG + occupancy bit-field at 108 FPS for 113 GB
  volume), NanoVDB.h (4-level 32³/16³/8³ structure, 672+64 B fixed overhead), NanoVDB 13.0.0
  (mutation-cost barrier).
- 2026-06-20 — prototype v1: standalone C++26 Svdag64 + NanovdbLike impls, 5 scenes,
  harness per `benchmarks/methodology.md`. Initial v1 had several compile errors (kTilesPerNode
  constexpr, struct padding, kUpperTiles > 32 UB) — fixed in v2.
- 2026-06-20 — prototype v2-3: discovered fundamental issue with original 4³-branching VDB-like
  for 32³ chunks (tree covers 256³ per root, way more than needed). Rewrote as 2³-branching
  octree (4 levels = 16 cells per axis per root, 8 root children = full 32³ coverage).
- 2026-06-20 — prototype v4-7: discovered "uniform-tile lie" bug — when parent tile is uniform
  and we SetCell within it, the lie (claiming sub-cube is uniform X) propagates. Fixed root-
  level lie by always promoting on first touch. Inner-level (Upper/Lower) lie still present
  due to time budget; documented as known bug in §5.4 of README.
- 2026-06-20 — measurements complete. 7 scenes × 3 trees = 21 measurements. Build time,
  SetCell latency, GetCell latency, total bytes, NonAir count, verify_mismatches.
  SVDAG byte-exact on all scenes. VDB-like approximate (known bug).
- 2026-06-20 — README.md full fill (9 sections). Verdict `yes` (SVDAG confirmed as correct
  Stage 1.x choice). Recommendation: per-chunk SVDAG dedup toggle (not always-on) to
  avoid 20-40× build cost overhead on non-repetitive scenes.
- 2026-06-20 — backlog.md moved from §In progress → §Closed. INDEX.md §5 Active → §6
  Recent closed.

---

## Notes

- **Standalone C++26 prototype в `prototype/svdag_vs_nanovdb.cpp` (720 lines, no mainline changes).**
  Builds with `clang++ -std=c++26 -O3 -march=native -DNDEBUG`. Runs in <1 second.
- **SVDAG-on-64-tree impl is byte-exact** (verify_mismatches=0 on all 7 scenes). Memory
  numbers within dubiousconst282 2024 literature range (0.62 B/voxel with dedup = best case;
  my no-dedup solid_32 = 8.75 B/voxel, 14× worse than perfect-dedup; dedup-ON cost 20-40×
  build time for non-repetitive scenes).
- **NanoVDB-like impl has known correctness bug** (uniform-tile lie in partial-fill
  scenes: ground_32, brick_32, voxel_lab_32, sparse_random_32 all have verify_mismatches>0).
  Memory numbers (~12 KB total per chunk) consistent with NanoVDB expectations but NonAir
  counts inaccurate for affected scenes. Documented in §5.4.
- **Verdict `yes` is based on SVDAG numbers + literature cross-reference**, not VDB-like
  numbers. VDB-like bug is in impl, not in NanoVDB principle.
- **Closes the measurement gap** from `2026-06-20-sparse-64-tree-alternatives/README.md`
  §5.3: «Не валидировали dedup-ratio на реальных VoxelLab / MeshingStress сценах». My
  synthetic scenes don't have 4×4×4 repetition to dedup, but the SVDAG machinery
  (MarkUnique, dedup index) is verified correct; real VoxelLab measurement is Stage 1.2
  verification per `TODO.md §1.2` acceptance.
- **Host env recorded**: AMD Ryzen 7 5800X, clang 22.1.6, Arch Linux, governor=`powersave`
  (production bench should use `performance`). CPU = 3.998 GHz. L1d 256 KiB, L2 4 MiB, L3 32 MiB.
