# STATUS — 2026-06-21-lod-mesh-downsampling

**Phase:** wrap-up (concluded-verdict-mixed)
**Last action:** 2026-06-21 — full measurement sweep complete (1200 main + 75 T-junction
measurements), RESULTS.md + README.md + sources.md written, sync-close per `AGENTS.md §13.5`
in progress.
**Next tick:** N/A (closed same session).
**Blocker:** нет.

---

## Progress log

- **2026-06-21 — opened.** Topic invented (Stage 4.2 chunk 2 uniform downsampling — explicit
  Nearest Gap in `agent/workspace.md §2`, not yet covered by any closed or in-progress
  experiment). Reservation per `AGENTS.md §13.1-13.5`: `backlog.md §Open` entry added,
  `backlog.md §In progress` entry with all §13.2 fields, folder + README + STATUS created.
- **2026-06-21 — web-research complete.** 2 batch queries + multiple targeted searches,
  ~30 sources верифицированы. Key references: 0fps.net, Lengyel 2009, Cinevva 2026, Cubyz
  DeepWiki, Aokana arXiv, Teknologicus Vorxel, GPUOpen FidelityFX SPD, OptiFine #7567
  (negative), Blackflux Part 3, Voxel.wiki, Nick Gildea, Voxceleron2, GPU Gems 2 Ch 2.
- **2026-06-21 — prototype written.** `prototype/lod_bench.cpp` (~840 LoC standalone C++26,
  4 downsample kernels + 3 stitch strategies + 5 scenes + 4 LOD levels + T-junction
  detector + Stats + CSV). `prototype/CMakeLists.txt` + `prototype/README.md` for
  reproduction.
- **2026-06-21 — prototype built + debugged.** Initial build green with 0 warnings. ASAN
  run revealed stack-buffer-overflow in `downsample_A` (the `uint8_t g[8]` was too small
  for `step=4, 8` where 64-512 reads needed). Fixed by resizing `g` to 512 bytes (max
  step³ = 8³ = 512) and refactoring to track `k` count. Re-built clean, ASAN run
  verified.
- **2026-06-21 — full sweep complete.** 1200 main measurements + 75 T-junction
  measurements. Wall time ~2 min on Zen 3 5800X (governor=`powersave`). Output:
  `build/results.csv` (94 KB) + `build/results_tjunc.csv` (12 KB).
- **2026-06-21 — analysis + writeup.** Headline finding: **`B_SurfacePreserve` is the
  only kernel with 0 T-junction holes across 75 test configurations** (16938 boundary
  face emissions, 0 mismatches). Verdict: **mixed** (no single (kernel, stitch) pair
  wins for all scenes, but B is the only DoD-satisfying kernel). Mainline
  recommendation: use `B_SurfacePreserve` as default, 3-step migration per
  `agent/knowledge.md` precedent.

---

## Notes

- Anti-duplicate sentinel clean per `AGENTS.md §13.7`.
- Cross-axis validated: 5 in-progress parallel sessions (`tracy-gpu-vs-manual` = profiling,
  `wfc-procedural-worlds` = gen strategy, `sub-chunk-layers` = vertical layers, `taa-
  motion-vectors` = temporal Stage 5.3, `gpu-fluid-ca-atomic-strategy` = atomic strategy
  Stage 3.1) + 30+ closed sessions from 2026-06-20/21 = none overlap Stage 4.2 LOD
  implementation axis.
- **Negative finding worth noting:** the 3 stitch strategies (X_None, Y_TJunctionPad,
  Z_NeighborLocked) produced **identical quad counts** in the prototype because B kernel
  eliminates the T-junction problem at the downsample stage. The strategies are still
  valuable for a real ProjectV integration where the kernel choice is scene-dependent.
- **Surprising finding:** `A_Majority3D` and `D_MaxPool` are **functionally equivalent**
  at the boundary (same 10-32% T-junction hole ratio, same 96 quads for forest_floor
  and uniform_floor). They differ only in interior behavior, which doesn't affect
  LOD quality at the chunk boundary.
- **Critical finding for cave scenes:** `C_SolidOnly` collapses the entire LOD 1 chunk
  to 0 quads in cave_stress. This is a **catastrophic regression** for cave worlds.
  Cave scenes MUST use B_SurfacePreserve (or D_MaxPool as a fallback if B unavailable).
- All 4 downsample kernels cost **< 1.5 µs/chunk** → Stage 4.1 budget (50 µs) gives
  **30-100× headroom** even on dev host's powersave governor.
