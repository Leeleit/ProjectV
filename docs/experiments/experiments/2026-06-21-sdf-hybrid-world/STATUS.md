# 2026-06-21-sdf-hybrid-world — STATUS

**Phase:** D (experiment closing — all phases complete, ready to sync per §13.5)
**Status:** **concluded-verdict-mixed** (closed `2026-06-21` same session)
**Started:** 2026-06-21
**Closed:** 2026-06-21
**Last action:** 2026-06-21 — **Phase A + B + C + D COMPLETE in single session.**

- **Phase A (web-research):** 15 primary sources verified via `webfetch` + DuckDuckGo HTML
  fallback (Exa 429 persistent, see `sources.md` for full bibliography).
- **Phase B (prototype code):** 9 files, ~1300 LoC, C++26 CPU-only standalone.
- **Phase C (build + measurements):** `clang++ 22.1.6 -O3 -march=native` **build green, 0 warnings**.
  **600 measurements** collected: 5 scenes × 5 seeds × 4 encodings × 2 builds × 3 terms × N=1000.
  Wall time <60 sec on Zen 3 5800X (governor=`powersave`). Full data: `prototype/results.csv`
  (75 KB).
- **Phase D (verdict + integration):** **`mixed`** per §6. Key findings: BFS 2.4× faster than JFA
  on chunkSize=8, D_RLE gives -70% VRAM, T_VoxelDiscrete remains optimal (no SDF gain measured
  in v1 prototype). 3-step migration recommendation in §7.

**Blocker:** нет. **Next tick:** operator sync per §13.5 (backlog.md §Closed move, INDEX.md §6 entry).

## Headline

- **BFS is faster than JFA on chunkSize=8** (6.6 vs 16.0 µs/chunk) — counter to literature;
  BFS wins for **dense/small** chunks (narrow-band = ≤7 voxels from surface).
- **D_RLE_NoneSparse = 30% VRAM** of B_R8_1byte — validates OpenVDB 13.0.1 narrow-band pattern.
- **T_VoxelDiscrete is fastest AND highest PSNR** in this prototype — current mainline behavior
  preserved; no SDF-driven VCT quality gain measured.
- **SDF prototype does NOT validate Narkowicz 2022 anti-leak benefit** — likely due to simplified
  trilinear SDF + same-algorithm reference (not true ground truth). Defer to Stage 5.1 post-MVP.

## Cross-axis summary (final)

- Orthogonal к 4 in-progress parallel (tracy-gpu / dlss-fsr-xess / greedy-physics-meshing /
  gpu-fluid-ca-atomic); `vct-cone-count-atlas-precision` closed same session 2026-06-21.
- Complementary к 8 closed experiments (vct-vs-rt-cutoff / nanovdb-on-gpu / sub-chunk-layers /
  lod-mesh-downsampling / wfc-procedural-worlds / gpu-procedural-noise / meshing-algo-comparison §6
  closure / vct-cone-count-atlas-precision).
- First SDF-for-lighting+physics axis (orthogonal к `meshing-algo-comparison` which parked
  SDF-meshing axis для Stage 3.3+).

## Self-promotion l→m justified (retrospective)

Per `optimization-philosophy.md`: hypothesis was measurable + cross-axis (Stage 5.1 + 3.3) + low
integration risk. **Even though final verdict is mixed, the experiment delivered value:**
- BFS > JFA finding (Step 1 recommendation, immediate integration XS effort)
- D_RLE narrow-band validation (Step 2 future S effort)
- Identified prototype vs production gap for SDF anti-leak benefit (research direction for
  follow-up)

## Phase A: web-research summary (unchanged)

- **Exa (primary):** 429 persistent (rate-limited). Reported to operator as fallback triggered.
- **DuckDuckGo HTML (`html.duckduckgo.com/html/?q=...`):** WORKING via `webfetch`. 3 queries → 24 results
  total. Top results filtered to ~12 directly relevant.
- **Direct URL fetch:** WORKING. Verified 4 primary sources (Narkowicz 2022, RTSDF arXiv 2210.04449,
  UE5 Mesh Distance Fields docs, OpenVDB 13.0.1 docs).
- **Verified sources count:** 15 primary + 4 open-source + 4 industrial + 4 local corpus = **27 total
  references** в `sources.md`.

## Phase B: prototype build instructions (unchanged, operator may re-run)

```bash
cd docs/experiments/experiments/2026-06-21-sdf-hybrid-world/prototype
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --parallel
./sdf_hybrid_bench --scene cave_stress --seed 42 \
    --encoding B_R8_1byte --build J_JFA_GPU --term T_Hybrid --cones 6 --iters 1000
```

**Phase A (web-research) summary:**
- 15 primary sources verified via `webfetch` + DuckDuckGo HTML fallback (Exa 429 persistent)
- **Narkowicz 2022 "Journey to Lumen"** — DIRECT EXPERT VALIDATION: voxel VCT leaks → global distance
  field + voxel bit bricks (8×8×8) = production-proven anti-leak path
- **NAADF 2026 Wiley CGF May 2026** — order-of-magnitude faster ray tracing для voxel worlds +
  axis-aligned distance fields
- **RTSDF arXiv 2210.04449 (2022)** — voxel + JFA + ray-trace refinement = direct analog
- **UE5 Mesh Distance Fields (Epic 5.8)** — production reference; **Intel HD disabled** (cross-vendor
  note)
- **OpenVDB 13.0.1** — narrow-band level sets; 8³ leaf = ProjectV chunkSize match

**Phase B (prototype code) summary:**
- `prototype/scenes.{hpp,cpp}` (220 LoC) — 5 scene generators per `sub-chunk-layers` precedent
- `prototype/sdf_overlay.{hpp,cpp}` (320 LoC) — JFA + BFS + Adaptive SDF generation
- `prototype/vct_cone_march.{hpp,cpp}` (380 LoC) — 3 termination strategies + 1024-cone reference
- `prototype/physics_normals.{hpp,cpp}` (180 LoC) — collision normal estimation
- `prototype/bench.cpp` (270 LoC) — measurement harness per `methodology.md §3`
- `prototype/CMakeLists.txt` + `prototype/README.md` — build/run instructions

**Blocker:** нет
**Next tick:** Phase C — operator runs `cmake --build` + sweep script, collects `results.csv`.
Phase D — анализ, verdict, integration recommendation per `AGENTS.md §6` DoD.
**ETA:** Phase C ~30 min (per `wfc` precedent); Phase D ~30 min (this session).

## Cross-axis summary

- Orthogonal к 4 in-progress parallel (tracy-gpu / dlss-fsr-xess / greedy-physics-meshing /
  gpu-fluid-ca-atomic); `vct-cone-count-atlas-precision` closed same session 2026-06-21.
- Complementary к 8 closed experiments (vct-vs-rt-cutoff / nanovdb-on-gpu / sub-chunk-layers /
  lod-mesh-downsampling / wfc-procedural-worlds / gpu-procedural-noise / meshing-algo-comparison §6
  closure / vct-cone-count-atlas-precision).
- First SDF-for-lighting+physics axis (orthogonal к `meshing-algo-comparison` which parked
  SDF-meshing axis для Stage 3.3+).

## Self-promotion l→m justified

Per `optimization-philosophy.md`: measurable hypothesis (VCT PSNR + collision normal smoothness) + strong
cross-axis (Stage 5.1 + Stage 3.3) + low integration risk (additive data, drop-in termination/normal
calculation) + CPU-only analytical scope (single-session per `wfc` precedent). **Narkowicz 2022
explicitly validates approach = production-proven, not unproven research direction.**

## Web research summary (Phase A)

- **Exa (primary):** 429 persistent (rate-limited). Reported to operator as fallback triggered.
- **DuckDuckGo HTML (`html.duckduckgo.com/html/?q=...`):** WORKING via `webfetch`. 3 queries → 24 results
  total. Top results filtered to ~12 directly relevant.
- **Direct URL fetch:** WORKING. Verified 4 primary sources (Narkowicz 2022, RTSDF arXiv 2210.04449,
  UE5 Mesh Distance Fields docs, OpenVDB 13.0.1 docs).
- **Verified sources count:** 15 primary + 4 open-source + 4 industrial + 4 local corpus = **27 total
  references** в `sources.md`.
- **Coverage:** primary hypothesis (VCT anti-leak) + 4 orthogonal angles (build, encoding, termination,
  application) + cross-vendor (NVIDIA/AMD/Intel) + SOTA 2024-2026.

## Prototype build instructions (per `prototype/README.md`)

```bash
cd docs/experiments/experiments/2026-06-21-sdf-hybrid-world/prototype
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --parallel

# Single run:
./sdf_hybrid_bench --scene cave_stress --seed 42 \
    --encoding B_R8_1byte --build J_JFA_GPU --term T_Hybrid \
    --cones 6 --iters 1000

# Full sweep (5 scenes × 5 seeds × 4 encodings × 3 builds × 3 terms = 900 measurements):
# See prototype/README.md "Sweep" section.
```
