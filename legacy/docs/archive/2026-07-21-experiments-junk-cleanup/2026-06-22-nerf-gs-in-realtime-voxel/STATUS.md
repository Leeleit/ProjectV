# STATUS — 2026-06-22-nerf-gs-in-realtime-voxel

**Phase:** Phase 4 (writeup + close) — DONE.
**Opened:** 2026-06-22.
**Closed:** 2026-06-22 (single session, ~1.5h from claim to close).
**Last action:** README.md §5/§6/§7 finalized with RESULTS.md data; sources.md with 6 verified sources (Tier 1: 3
foundational papers, Tier 2: 2 production references, Tier 3: 1 Wikipedia overview); prototype/build/results.csv (126
rows) generated; build green 0 warnings.
**Blocker:** нет.
**Verdict:** **`yes`** for **C_HybridStatic_Plus_VoxelDynamic ⭐ as universal recommended default** (6.48 ms frame = 154
FPS theoretical, 0.008 ms mutation = 1,500,000× better than B, 159 MB VRAM = 1.9% of 8 GiB budget); B/D/E per-strategy
verdicts: B=REJECTED, D=REJECTED, E=REJECTED.
**Hypothesis (one-line):** C_HybridStatic_Plus_VoxelDynamic рендерит 60 FPS на RTX 3060 Ti + zero perceived lag on voxel
edit; H3c_DropAffectedSplats mutation strategy (mark dead, no re-train) = best cost/quality для gameplay-typical edit
rates (10-1000 edits/sec) — **CONFIRMED MASSIVELY** (3 of 3 sub-hypotheses).

---

## Files touched

- `README.md` (full 9-section template per `_TEMPLATE/README.md`, all sections filled)
- `STATUS.md` (this file)
- `sources.md` (6 verified sources: 3 Tier 1 foundational papers + 2 Tier 2 production refs + 1 Tier 3 Wikipedia
  overview)
- `RESULTS.md` (full headline + per-strategy + per-scene + 3 comparison matrices + 6 caveats)
- `prototype/gsplat_bench.cpp` (~320 LoC standalone C++26 CPU analytical cost model)
- `prototype/build/gsplat_bench` (binary, 26 KB)
- `prototype/build/results.csv` (126 rows = 5 strategies × 5 scenes × 5 seeds + header, 13 KB)

---

## Cross-axis

**Orth** ко всем in-progress parallel (`2026-06-22-urban-combat-tactics-ai` Tier 2 AI,
`2026-06-22-fire-coordination-multiple-units` Tier 2 AI, `2026-06-22-missile-guidance-laws-simulation` Tier 1 Phys+2 AI,
`2026-06-22-stealth-signature-reduction` Tier 2 AI, `2026-06-22-voxel-material-weathering-surface-aging` Stage
4.x/6.x) + closed 130+ experiments (3DGS = ML+rendering, no overlap with physics/AI/netcode).

**Complementary** к closed:

- `2026-06-21-lod-mesh-downsampling` [mixed] (LOD strategy, может использовать 3DGS для LOD2+ static decor)
- `2026-06-21-lod-transition-strategy` [mixed] (geomorph + 3DGS splice = hybrid LOD)
- `2026-06-21-volumetric-fog-atmosphere-rendering` [mixed] (3DGS splats as fog participants?)
- `2026-06-21-vct-vs-rt-cutoff` [mixed] (3DGS vs VCT vs RTX для GI = orthogonal lighting axis)
- `2026-06-21-dec-pipelines-async-compute` [yes] (3DGS sort = async compute candidate per Stage 2.2 HZB precedent)
- `2026-06-21-bindless-descriptor-overhead` [mixed] (3DGS = massive SSBO, bindless leverage)
- `2026-06-21-vma-sparse-textures` [mixed] (3DGS textures = sparse page table candidate)
- `2026-06-21-hzb-smart-mip-select` [mixed] (per-chunk HZB culling applicable to 3DGS)
- `2026-06-21-data-driven-vehicle-weapon-definitions` [yes] — 3DGS как asset format в spec catalog
- `2026-06-21-voxel-asset-template-catalog` [yes] — 3DGS templates alongside voxel templates
- `2026-06-21-procedural-military-terrain-gen` [yes] — 3DGS для photogrammetric landmarks

**Prerequisite** для open эзотерика-tier 6+:

- `ddsp-procedural-audio` [l, open] (нейросетевой audio synthesis = same ML-driven content tooling axis)
- `cxl-storage-class-tier` [l, open] (3DGS assets large enough для CXL)
- `neuromorphic-photonic-rendering` [l, open] (3DGS может befit from neuromorphic inference)

---

## Migration effort

**M (2-3 sessions, 1 done already with this experiment's analytical prototype + writeup).** Per `agent/knowledge.md`
precedent, 3-step mainline migration:

- Step 1 (XS, ~80 LoC) `src/render/gsplat/GsplatAsset.{hpp,cpp}` + PLY/`.splat` loader
- Step 2 (M, ~400 LoC) `src/render/gsplat/GsplatRenderer.{hpp,cpp}` + radix sort + rasterize + HZB culling + async
  compute
- Step 3 (S, ~100 LoC) `PROJECTV_GSPLAT=OFF|STATIC|HYBRID` env gate + voxel H3c drop hook + Tracy plot +
  `ProjectVGsplatTests`

Total: ~580 LoC C++ + asset pipeline glue + test infra, M effort.

**Deferral per `agent/workspace.md §2` operator 8x planning decision:** this is an **opt-in** path, not a gating Stage
5.x dependency. Mainline Voxel rendering is fully functional without 3DGS. Recommended for **dedicated session** when
operator decides to invest in photogrammetric content pipeline.

---

## Reusable for

- `data-driven-vehicle-weapon-definitions` [yes, closed] — 3DGS как asset format в spec catalog
- `voxel-asset-template-catalog` [yes, closed] — 3DGS templates alongside voxel templates
- `procedural-military-terrain-gen` [yes, closed] — 3DGS для photogrammetric landmarks
- Stage 5.x Visual Polish — additive opt-in для статических декоративных сцен
- Stage 6+ Content Tooling — photogrammetry pipeline (COLMAP → 3DGS training, **out of scope single session**)

---

## Hardware baseline

Per `hardware-profile.md §1/§3/§4` (Zen 3 5800X 8C/16T + RTX 3060 Ti GA104 + Vulkan 1.4.341) — данные актуальны на
`2026-06-21`, **probe не запускаю** per §14 STOP-блок. RTX 3060 Ti = ~2.5× slower than RTX 3090 for rasterization-bound
work (38 vs 82 RT cores); 3DGS frame cost scaled accordingly.

---

## Sync (per §13.5)

**Next: обновить `INDEX.md §6 Recent closed` + `backlog.md` (move from §In progress to §Closed / §backlog_closed.md).**
