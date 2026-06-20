# STATUS — 2026-06-20-vis-buffer-for-voxels

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20 (single session)
**Phase:** Done.

---

## Summary

Hypothesis: vis-buffer даст 5-10× bandwidth win + unbounded material capacity для voxel deferred-shading.
**Refined after ProjectV survey:** vis-buffer не даст bandwidth win потому что ProjectV уже использует
SSBO material lookup (forward+, no full G-buffer). Potential win = redundant raster elimination для
CSM × 4 shadow passes (each re-decodes PackedFace vertex shader).

**Verdict: `mixed`** — не рекомендуется для ProjectV's **current** Stage 2.x/5.x at 1920×1080.
Cross-over at ~1280×720. **Re-evaluate at Stage 4.3** (128+ chunks) + mobile target decision.

---

## Timeline

- **2026-06-20 (start)**: Claimed per §13.1 (anti-duplicate check + reservation lifecycle).
- **2026-06-20 (research)**: 5 web-search queries, 15+ sources верифицированы (Burns-Hunt 2013,
  Nanite SIGGRAPH 2021 + GDC 2024, Frostbite Andersson 2017, The Forge v1.57, Vulkan-Guide TBR,
  Adreno vis-stream HW, voxel-specific VoxelMVP/Exile/Slater/Ascendant).
- **2026-06-20 (prototype)**: Standalone Vulkan 1.4 app, ~700 LoC (src/ + shaders/). RTX 3060 Ti,
  Vulkan 1.4.341, driver 610.43.02. Two pipelines (baseline forward+ vs vis-buffer hypothesis).
  3 scenes × 3 resolutions = 6 measurement configurations. Visual equivalence verified via
  framebuffer hash match.
- **2026-06-20 (close)**: README + sources.md + STATUS.md written; backlog.md + INDEX.md updated.

## Key findings

- **Cross-over at ~1280×720** for 4-8³ chunk scenes (~3K-25K quads).
- **1920×1080**: vis-buffer 15-26% **slower** than baseline (bandwidth-bound on pixel coverage).
- **800×600**: vis-buffer 12-24% **faster** (vertex cost dominates for low pixel count).
- **Voxel scenes are pixel-coherent after greedy meshing** = ~1 visible triangle per pixel = no
  overdraw to amortize fullscreen vis-buffer cost.
- **Hash match** confirms visual equivalence (both paths produce identical output framebuffer).

## Cross-axis closure

Today's batch covered: storage (`svdag-vs-vdb`), layout (`cache-oblivious`), sync (`dec-pipelines`),
cull (`mesh-shader-vs-compute-cull`), binding (`bindless-descriptor-overhead`), meshing
(`meshing-algo-comparison`), simd (`simd-procedural-noise`), hzb (`hzb-binding-models`), flecs
(`flecs-soa-vs-aos-bench`), nanovdb (`nanovdb-on-gpu`), gi-cutoff (`vct-vs-rt-cutoff`),
frame-pacing (`vulkan-fps-pacing-vk-ext`), job-system (`work-stealing-job-system`).
**This experiment closes rendering-approach axis** (deferred resolve via vis-buffer + material table).

## Cross-references

- **`2026-06-20-clustered-forward-mass-lights`** (parallel session) — orthogonal design space
  (forward+ cluster grid vs deferred vis-buffer). EXCLUDED из моего scope per operator.
- **`2026-06-20-bindless-descriptor-overhead`** (closed verdict=mixed) — Phase B = bindless
  material table = prerequisite для vis-buffer's per-frame material lookup.
- **`2026-06-20-meshing-algo-comparison`** (closed verdict=mixed, Naive Greedy default) —
  voxel scenes with greedy meshing = pixel-coherent = low-overdraw = **loses** for vis-buffer.
- **`2026-06-20-dec-pipelines-async-compute`** (closed verdict=yes) — async-compute resolve
  pass would compound vis-buffer benefits (deferred compute overlap with main render).
- **`agent/knowledge.md §25`** — greedy meshing rationale (per-axis dispatch).
- **`agent/knowledge.md §30.4`** — 3-step migration precedent.

## Anti-ritual notes

- Не дублировал hardware data в README — cross-ref на `hardware-profile.md` §3 + §4.
- Не использовал hardware-probe (RTX 3060 Ti data уже в `hardware-profile.md`, captured 2026-06-20).
- Не путал scope с parallel session's `clustered-forward-mass-lights` (orthogonal, complementary).

## Risks

- **Single-vendor validation** (NVIDIA RTX 3060 Ti) — mainline re-test on AMD RDNA + Intel Arc
  required для cross-vendor claims.
- **Headless harness** (no swapchain) — systematic under-estimate vs real renderer + swapchain latency.
- **Synthetic scene** (procedural ground + columns) — not real VoxelLab. Mesh quality + lighting
  preview not validated against production.
- **Simplified resolve** (full GGX per pixel) — production would split into main + AO + shadow
  passes with simpler per-pixel work.
- **No async compute** — resolve pass could overlap main render per `dec-pipelines-async-compute`,
  but unmeasured.

## Re-evaluation triggers

- **Stage 4.3** (128+ chunks draw distance per `TODO.md §4.3`) — vertex cost scales linearly with
  chunks, pixel cost constant → crossover shifts toward vis-buffer winning.
- **Mobile target support** (Apple/Android) — TBR GPUs benefit from on-chip tile memory,
  vis-buffer 10-30% win per Vulkan-Guide TBR best practices.
- **Stage 4.2** LOD high-subdivision — overdraw-heavy at distance, vis-buffer wins.
- **Stage 5.1** VCT integration — multiple cone-trace passes per pixel = fullscreen resolves
  dominated by lighting compute, not memory bandwidth.
- **More light passes** than current 4 CSM + 1 AO + 1 point — more redundant raster savings.
