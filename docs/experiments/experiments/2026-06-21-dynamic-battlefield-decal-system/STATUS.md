# STATUS — 2026-06-21-dynamic-battlefield-decal-system

**Phase:** Phase 4/4 — RESULTS COMPLETE + sync (this file + README + sources.md + prototype).
**Date:** 2026-06-21.

## State

- **Claimed:** `2026-06-21` by self per `AGENTS.md §13.1` + sentinel §13.7.
- **Slug:** `2026-06-21-dynamic-battlefield-decal-system` (military sandbox axis — Tier 0 Foundation & Optimization).
- **Verdict:** `mixed` (D_AtlasIndirectLRU validated as best general-purpose; C_DBuffer best for <5k;
  B_ScreenSpace for transient; A_PerDecalMesh deprecated).
- **Status:** **closed `2026-06-21` (single session), verdict=`mixed`.**

## Completed phases

**Phase 1 (reservation + skeleton):** ✅ backlog claim + INDEX §5 entry + README skeleton + STATUS.md.

**Phase 2 (web-research):** ✅
- Frostbite GDC'09 Shadows & Decals — primary canonical (geometry shader + stream-out approach).
- The Surge 2 bindless deferred decals — primary production reference (D3D12/Vulkan bindless atlas).
- MJP DeferredTexturing — open-source reference implementation.
- Khronos Vulkan multi_draw_indirect sample — canonical GPU-driven indirect draw pattern.
- GPU Gems 2 Ch. 5 "Decal Applications" — canonical taxonomy (Mitchell 2005).
- Plus 5 Tier 2 supporting references (Zhytou 2025 GPU-Driven Pipeline, AMD GPUOpen GDC 2024 Work Graphs,
  EA GDC 2016 Compute Pipeline, Alex Tardif Bindless blog, PLAYERUNKNOWN GPU-Driven Instancing).
- Sources fully documented in `sources.md`.

**Phase 3 (prototype + benchmark):** ✅
- `prototype/decal_bench.cpp` ~300 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra
  -Wpedantic`, build green 0 warnings).
- 4 strategies (A_PerDecalMesh / B_ScreenSpace / C_DBuffer / D_AtlasIndirectLRU) × 3 distributions
  (uniform / clustered / sparse) × 5 decal_counts (1k/2k/5k/10k/20k) × 5 seeds × 1000 iter + 10 warmup.
- **303,000 main measurements**, wall time **0.021 sec** на Zen 3 5800X governor=`powersave`.
- Output: `prototype/build/results.csv` (301 rows) + `prototype/build/summary_means.csv` (60 rows).

**Phase 4 (results + integration recommendation):** ✅
- README §5 — results table + key findings + threshold analysis.
- README §6 — verdict `mixed`.
- README §7 — 3-step migration per `agent/knowledge.md §30.4` precedent (~750 LoC, M effort, 2-3 sessions).

## Headline findings

- **D_AtlasIndirectLRU ⭐ = recommended default** (3× faster than A baseline, 2.4× faster than B,
  fixed 4 MiB VRAM, persistent state).
- **C_DBuffer fastest GPU at low counts** (0.124-0.527 ms) but VRAM scales (2-8.78 MiB) + chunk edit
  invalidation complexity.
- **B_ScreenSpace competitive at moderate counts** (0.497 ms clustered 20k) but no persistence.
- **A_PerDecalMesh naive baseline** (2.63 ms uniform 20k = 7.9% of 30 Hz budget) — NOT recommended
  beyond 1k decals.
- All strategies cross 5-10% threshold per `optimization-philosophy.md` (66% reduction A→D uniform 20k).

## Next steps (pending operator decision)

- **Sync INDEX.md §5 → §6 transition** (move entry from Active to Recent closed) — will be done in
  this commit per §13.5.
- **Sync backlog.md §In progress → §Closed** — will be done in this commit per §13.5.
- **Mainline integration** — deferred до Stage 6+ military sandbox activation per operator 8x planning
  decision in `agent/workspace.md §2`.

## Blockers

- **None.** Slug available per sentinel §13.7. Topic не дублирует 50+ closed experiments (first decal-system axis).
- **No parallel conflicts** — all 4 in-progress experiments at start (flow-field, multi-res-broadphase,
  gpu-fluid-ca, biome-blend) closed same session.

## Build verification

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    -o build/decal_bench decal_bench.cpp
./build/decal_bench --output build/results.csv --summary build/summary_means.csv
# [decal_bench] wall time: 0.021 sec
# [decal_bench] wrote results.csv (301 rows)
# [decal_bench] wrote summary_means.csv
```
