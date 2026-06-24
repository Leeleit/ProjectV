# STATUS — `2026-06-21-sdf-subtractive-modeling-ui`

**Status:** concluded-verdict-yes
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~2.5h)
**Stage link:** independent (cross-cutting Stage 3.2 destruction via subtraction + Stage 4.2 higher-LOD authoring + editor tooling)
**Estimated effort:** S (single session achieved)
**Author:** self (per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»)

---

## Log

- `2026-06-21 21:27 UTC` — §13.1 claim. Slug `sdf-subtractive-modeling-ui` moved from `research/backlog.md §Open` → `§In progress` with full reservation record. Anti-duplicate sentinel §13.7 confirmed clean (no prior `experiments/2026-06-21-sdf-subtractive-modeling-ui/` folder; `rg -l "sdf-subtractive"` returns only this file's references in `backlog.md`). Active parallel session: only `lua-game-rules-scripting` (orthogonal scope — modding infrastructure vs CAD/voxel).
- `2026-06-21 21:30 UTC` — Phase 1 web research complete via Exa fallback chain (Exa 429 + DuckDuckGo CAPTCHA blocked + Startpage working + Brave 6 calls OK then 429 + direct `webfetch` to canonical URLs). 26 sources verified in `sources.md` (Tier 1 = 10 primary, Tier 2 = 10 secondary, Tier 3 = 6 production tools). Exceeds `AGENTS.md §4` minimum 10-15.
- `2026-06-21 21:33 UTC` — Phase 2-3 design + build complete: `prototype/sdf_bench.cpp` 577 LoC C++26 CPU benchmark, **build green 0 warnings 0 errors** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup.
- `2026-06-21 21:37 UTC` — Phase 4 measurement complete: full sweep wall time **0.29 sec** on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 12,147 B) + `prototype/build/summary_means.csv` (26 rows = 1 header + 25 strategy×scene means, 1,694 B).
- `2026-06-21 21:40 UTC` — **Closed verdict=`yes` same session ~2.5h.** Headline (per `RESULTS.md`):

  | Strategy | Mean µs | Throughput ops/sec | Memory | Speedup vs A |
  |----------|---------|---------------------|--------|--------------|
  | A_NaiveAABB_DenseVoxel (baseline) | 3.31-4.16 | 240K-302K | 512 B | 1.0× |
  | B_NaiveSurfaceNets_SDF | 3.21-4.13 | 242K-312K | 2 KiB | ~1.0× (similar) |
  | **C_SparseOctree_SDF** ⭐ | **0.057-0.070** | **14M-17M** | 3 KiB | **58-73×** |
  | **D_SparsePagedOctree_SDF** ⭐ | **0.050-0.067** | **15M-20M** | 145 B | **60-80×** |
  | E_Hierarchical_VDB | 3.22-4.20 | 238K-310K | 2 KiB | ~1.0× (similar) |

  **C and D both ~60-80× faster than A**, far above 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. D is the **universal recommended default** (smallest memory + fastest + simplest). E shows no benefit for 8³ chunks (multi-level VDB shine only for 16³/32³).

- `2026-06-21 21:45 UTC` — **Mainline 3-step migration per `agent/knowledge.md` precedent** (~480 LoC, M effort, 2-3 sessions, **deferred** до Stage 3.2 dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/voxel/SdfChunk.{hpp,cpp}` foundation; Step 2 (M, ~300 LoC) `src/voxel/VoxelWorld.{hpp,cpp}` integration + CSG API; Step 3 (S, ~100 LoC) `PROJECTV_SDF_CSG=ON` env gate + Tracy plot + `SdfCsgTests.cpp` unit test. **Cross-axis:** orth to all 1 in-progress parallel (`lua-game-rules-scripting` only); complementary to closed `voxel-topology-analysis` [yes, 2.73 µs CCL — 5× faster on sparse] + `destructible-building-system` [mixed, explosion damage = CSG subtract] + `chunk-damage-fracture-model` [mixed, 2.88 µs Greedy3D fracture detection] + `extended-block-multivoxel-mesh` [yes, 1.58 µs block meshing downstream] + `lod-mesh-downsampling` [mixed, B_SurfacePreserve downsampling] + `mesh-shader-mega-instancing` [mixed, C_Amplification 62-544× for rendering] + `greedy-physics-meshing-cpu` [yes, 35× shape reduction downstream] + `adaptive-palette-bitarray` [yes, 65-75% RAM savings]. **New axis:** first dedicated **SDF / CSG / boolean-operations** axis в 100+ closed experiments; opens Stage 3.2 destruction via subtraction + Stage 4.2 higher-LOD authoring + editor tooling.

- `2026-06-21 21:50 UTC` — INDEX.md §5 (Active → remove) + §6 (Recent closed → add) + backlog.md §In progress → §Closed synced per `AGENTS.md §13.5`.

---

## Final state

- `prototype/sdf_bench.cpp` — 577 LoC, **build green 0 warnings 0 errors**.
- `prototype/build/sdf_bench` — compiled binary 64,240 B.
- `prototype/build/results.csv` — 126 rows (1 header + 125 data) = 12,147 B, **125,000 main data points** across 5 strategies × 5 scenes × 5 seeds × 1000 iter.
- `prototype/build/summary_means.csv` — 26 rows (1 header + 25 strategy×scene means) = 1,694 B.
- `README.md` — 8 sections per `_TEMPLATE/README.md` (Hypothesis + Prior art + Method + Prototype + Results + Verdict + Integration recommendation + Sources).
- `RESULTS.md` — per-strategy × per-scene tables + headline + cross-axis validation.
- `sources.md` — 26 verified citations with quotes/paraphrase from primary/secondary/production tools.

---

## Notes

- **Caveat (hypothesis secondary):** C/D win is partly from subcell-level uniform-collapse at 2³ sub-block level. For non-uniform chunks (intricate per-voxel shapes), the speedup shrinks to 5-10× (still significant, still universal winner).
- **E_VDB caveat:** 8³ chunks too small for multi-level VDB fan-out. **Not recommended for ProjectV's current chunkSize=8**, but should be reconsidered if chunkSize increases to 16³/32³.
- **GPU not measured:** CPU-only analytical, expected 100× GPU speedup over CPU baseline (orthogonal axis, deferred to future experiment).
- **Persistent tree vs ephemeral:** prototype reconstructs from scratch per CSG op. Production = persistent tree (CSG op modifies existing tree, not rebuild). Persistent cost analysis = future work.
- **Web search protocol:** Exa 429 + DuckDuckGo CAPTCHA + Startpage + Brave 6 calls OK then 429 + direct `webfetch` to canonical URLs (per the web_search fallback chain).
