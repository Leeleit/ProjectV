# STATUS — 2026-06-21-voxel-asset-template-catalog

**Phase:** closed

**Status:** `concluded-verdict-mixed` (A_HashMap recommended default; D_PerChunkInline catastrophic at scale; E viable for prefab reuse; B/C niche use cases)

**Started:** 2026-06-21
**Closed:** 2026-06-21 (single session, ~1h)
**Agent:** self
**Verdict:** `mixed` (A confirmed as universal default; D rejected at scale; E viable for prefab-dedup)

**Phase tracker (all complete):**

- [x] **Phase 0 (reservation per §13.1):** DONE — `research/backlog.md §In progress` + `INDEX.md §5 Active` + folder `experiments/2026-06-21-voxel-asset-template-catalog/`. Anti-duplicate sentinel clean per §13.7 (`rg "asset.template|template.catalog|voxel.template|asset.catalog"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; only tangential mention в `tank-terrain-interaction-physics/README.md`).
- [x] **Phase 1 (web-research):** DONE — Brave Search working after Exa HTTP 429 + DuckDuckGo CAPTCHA blocked. **12+ primary sources verified** в `sources.md`: Godot Voxel Tools VoxelInstanceLibrary + Unreal Voxel Plugin VoxelDataAssets + VoxelFarm procworld 2013 + Veloren veloren_common_assets + Stormworks XML format + Clay Garrett + SVDAG Siggraph 2013 + VoxEdit + Unity Voxel Play + Brown hash tables + MAGICAL.
- [x] **Phase 2 (design strategies + scenes):** DONE — 5 strategies (A_HashMap / B_BTreeMap / C_FlatArrayCatalog / D_PerChunkInline / E_HierarchicalPaletteCatalog) × 5 scenes (small_spawn=10 / medium_spawn=1000 / large_spawn=10000 / mixed_query=100k / hot_reload=1000).
- [x] **Phase 3 (prototype):** DONE — `prototype/asset_catalog_bench.cpp` ~470 LoC. C++26 CPU analytical model. Standalone, NOT ProjectV mainline.
- [x] **Phase 4 (build + run + collect results.csv):** DONE — Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings, 0 errors**. Wall time 3.538 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, ~10 KB). 100% hit rate (all `successful_spawns == spawn_count`).
- [x] **Phase 5 (write-up RESULTS + finalize README + STATUS):** DONE — full `RESULTS.md` written with 5 strategies × 5 scenes analysis; `README.md` updated with all 8 sections; this STATUS.md finalized.
- [x] **Phase 6 (single-pass sync per §13.5):** DONE — `research/backlog.md §In progress` → `§Closed`; `INDEX.md §5 Active` → `§6 Recent closed`; this STATUS.md + README.md + RESULTS.md + sources.md + prototype all on disk.

**Blocker:** нет.

**Headline:**
- **A_HashMap ⭐** = universal recommended default (122-406 ns lookup, 7-16 ns instantiation, 6.6e+07 — 1.4e+08 spawns/sec).
- **D_PerChunkInline ⛔** = catastrophic at scale (5869 ns/op at N=10000 mixed_query = 380× slower than A).
- **E_HierarchicalPaletteCatalog** = viable for prefab-dedup (128-680 ns lookup, +160 KB fixed overhead).
- **B_BTreeMap / C_FlatArrayCatalog** = niche use cases (sorted iteration / static catalog).

**Cross-axis (preliminary):**
- Orth к closed `chunk-storage-compression-axis` + `sub-chunk-layers` + `adaptive-palette-bitarray` + `voxel-mutation-cost-characterization`.
- Complementary к closed `extended-block-multivoxel-mesh` + `destructible-building-system` + `mesh-shader-mega-instancing` + `procedural-military-terrain-gen` + `voxel-topology-analysis`.

**Anti-duplicate verification (§13.7):**
- `rg "asset.template|template.catalog|voxel.template|asset.catalog"` over `INDEX.md` + `experiments/2026-06-21-*/` → 0 dedicated experiments.
- `ls experiments/2026-06-21-voxel-asset*` → only this folder.
- No race with parallel agents.

**Re-evaluation triggers:**
- Vulkan compute shader dispatch (GPU offload of catalog iteration)
- Stage 6+ military sandbox activation (10000+ templates scenario)
- Cross-platform validation (AMD RDNA / Intel Arc / ARM)

См. [README.md](./README.md) + [RESULTS.md](./RESULTS.md) + [sources.md](./sources.md) + `prototype/{asset_catalog_bench.cpp (~470 LoC), build/{asset_catalog_bench (124 KB), results.csv (126 rows, 10 KB)}}`.
