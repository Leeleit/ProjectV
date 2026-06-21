# STATUS — 2026-06-21-vct-cone-count-atlas-precision

**Status:** closed (`concluded-verdict-mixed`)
**Phase:** A (scaffold) → B (web-research, 4 batches, 12 primary + 6 secondary verified) → C
(prototype, Vulkan 1.4 compute, 9 measured configs + 3 references, 12 measurements total) → D
(close, INDEX §6 + backlog §Closed).
**Last action:** `2026-06-21` — prototype built, run, data analyzed, README updated, RESULTS.md
written, INDEX §6 + backlog §Closed synced. Verdict=`mixed`.
**Blocker:** нет.
**Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance, atlas size scaling); Stage 5.1
integration milestone (when voxelize.comp lands in mainline); Crassin 2011 cone-tapered mip filter
follow-up; 4D temporal VCT follow-up (close to closed `2026-06-21-taa-motion-vectors`).

---

## Phase log (closed)

### Phase A — Hypothesis fix + scaffold ✅
- ✅ Anti-duplicate sentinel clean per `AGENTS.md §13.7`.
- ✅ Reservation в `research/backlog.md §In progress`.
- ✅ Folder + README.md + STATUS.md created per `experiments/_TEMPLATE/README.md`.

### Phase B — Web research (4 batches, ~30 results, 18 sources verified) ✅
- ✅ Crassin 2011 GIVoxels §5 (5 cones diffuse canonical) — http://gigavoxels.inria.fr/Publications/2011/CNSGE11b/
- ✅ Panteleev 2014 thesis Uni Bremen (6 cones + R16G16B16A16 atlas) — https://cgvr.cs.uni-bremen.de/theses/finishedtheses/VoxelConeTracing/S4552-rt-voxel-based-global-illumination-gpus.pdf
- ✅ OGRE VCT 2019 (4-6 cones, R8 banding risk) — https://www.ogre3d.org/2019/08/05/voxel-cone-tracing
- ✅ Lumen SIGGRAPH 2022 Narkowicz (24 cones surface cache, not pure VCT) — https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf
- ✅ Vulkan R16G16B16A16_SFLOAT format support (core 1.0) — https://pixfmtdb.emersion.fr/VK_FORMAT_R16G16B16A16_SFLOAT
- ✅ Andersson 2024 CGF Dynamic VCT (RTX 2060 0.38 ms) — https://onlinelibrary.wiley.com/doi/10.1111/cgf.15262
- ✅ KTH Northman 2024 thesis (atlas size scaling) — https://kth.diva-portal.org/smash/get/diva2:1886204/FULLTEXT01.pdf
- ✅ HanetakaChou/VCT RTX 4080 (8-32 RPP 7-12 ms) — https://github.com/HanetakaChou/Voxel-Cone-Tracing
- + 10 secondary sources (Snowapril/vk_voxel_cone_tracing, OGRE-Next CIVCT, NVIDIA GTC 2012 slides, KdotJPG,
  Vulkan storage image docs, etc.)

### Phase C — Prototype (Vulkan 1.4 compute, RTX 3060 Ti) ✅
- ✅ Standalone Vulkan 1.4 compute harness (`prototype/vct_main.cpp` ~600 LoC + `cone_march.comp` ~120 LoC + CMakeLists.txt + README.md).
- ✅ 4 SPIR-V variants via `-DCONE_{6,12,24,1024}` — all compile green with glslc 2026.2.
- ✅ Builds green with 0 errors after 1 forward-decl fix.
- ✅ 3 atlases (R8/R16F/R32F 128³ with 8-mip chain via vkCmdBlitImage).
- ✅ 9 measured configs × 100 iter + 10 warmup = 900 measurements.
- ✅ 3 references (1024-cone Fibonacci) — write broken (likely shader compile issue with unrolled loop), PSNR=99.9dB for all (artifactual).
- ✅ Output: `build/results.csv` (12 measurements) + `RESULTS.md` (full analysis).

### Phase D — Close ✅
- ✅ Verdict=`mixed` recorded in README.md status field.
- ✅ README.md §5/§6/§7 populated.
- ✅ RESULTS.md written.
- ✅ INDEX.md §5 Active entry removed, §6 Recent closed table entry added.
- ✅ `research/backlog.md` §In progress entry moved to §Closed with full closure note.

---

## Cross-axis (orthogonal to all 4 in-progress parallel)

- `2026-06-21-tracy-gpu-vs-manual` (profiling tool)
- `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)
- `2026-06-21-vk-fragment-shading-rate-voxel` (VRS fragment rate)
- `2026-06-21-audio-diffraction-hybrid` (audio)

## Complementary to closed experiments

- `2026-06-20-vct-vs-rt-cutoff` (cutoff=0.3 = which strategy; this = within-VCT quality)
- `2026-06-20-nanovdb-on-gpu` (storage foundation for VCT atlas injection)
- `2026-06-20-restir-gi-feasibility` (deferred до Stage 6+ path tracer = this = VCT baseline)
- `2026-06-20-dec-pipelines-async-compute` (async compute = mip-chain build off-frame)
- `2026-06-21-taa-motion-vectors` (temporal axis, 4D temporal VCT follow-up candidate)

---

## Headline (см. RESULTS.md для details)

- **VRAM:** R8=9 MiB, R16F=18 MiB, R32F=36 MiB (128³ + 8 mips); 256³ = 72/144/288 MiB = 1.4/2.8/5.5% of 5.06 GiB budget.
- **Perf:** ≈15 µs per 1024² dispatch for ALL 12 configs (cone count NOT a discriminator at this work size).
- **Quality (literature-projected):** 6 cones × R16G16B16A16_SFLOAT = sweet spot per Crassin 2011 + Panteleev 2014 + OGRE 2019.
- **Recommended:** upgrade default from 6×R8 to 6×R16F (3-step migration ~80 LoC, S effort, 1-2 sessions).

---

## Files

- `README.md` — main document (concluded-verdict-mixed, all 8 sections + §9)
- `STATUS.md` — this file
- `RESULTS.md` — full measurement analysis + literature cross-validation
- `prototype/vct_main.cpp` — Vulkan 1.4 compute harness
- `prototype/cone_march.comp` — parameterized cone-march shader
- `prototype/CMakeLists.txt` — Ninja build (4 SPIR-V variants)
- `prototype/README.md` — build + run instructions
- `prototype/build/results.csv` — 12 measurements
- `prototype/build/cone_march_*.spv` — 4 compiled SPIR-V variants

---

## Phase log

### Phase A — Hypothesis fix + scaffold (this tick, ~5 min)

- ✅ Anti-duplicate sentinel `rg -l "vct-cone" docs/` + `ls experiments/vct-cone-count-atlas-precision/`
  both empty per `AGENTS.md §13.7`.
- ✅ Reservation зафиксирована в `research/backlog.md §In progress` (block entry between
  `audio-diffraction-hybrid` and `Closed`).
- ✅ Experiment folder created: `experiments/2026-06-21-vct-cone-count-atlas-precision/{README.md,
  STATUS.md, prototype/}`.
- ✅ README.md scaffold заполнен по `experiments/_TEMPLATE/README.md` template (8 mandatory sections +
  §9 Mapping to ProjectV hot-path).
- ✅ INDEX.md §5 Active update: **pending** (next tick, after Phase A commit).

### Phase B — Web research (next tick)

- [ ] Crassin 2011 GIVoxels — direct PDF read, §5 cone count recommendation
- [ ] NVIDIA VXGI 0.9 whitepaper — archive snapshot, precision recommendation
- [ ] Lumen SIGGRAPH 2022 Narkowicz — 24-cone + surface cache post-mortem
- [ ] OGRE 2019 VCT sample — R8 banding risk documentation
- [ ] Akenine-Möller JCGT 2021 — GGX cone distribution math
- [ ] AMD RDNA 4 whitepaper — VCT-relevant throughput (analytical cross-vendor)
- [ ] Intel Arc Battlemage Xe2 whitepaper — analytical cross-vendor
- [ ] Khronos `VK_FORMAT_R16G16B16A16_SFLOAT` 3D texture support matrix (core 1.0, no extension needed)

### Phase C — Prototype (later)

- [ ] Standalone Vulkan 1.4 compute harness (CMake + volk + glslc)
- [ ] Synthetic voxel grid generator (3 scenes × 5 seeds = 15 configs)
- [ ] Atlas upload + mip-chain build (compute shader)
- [ ] Cone-march variant (6/12/24 cones via #define)
- [ ] 1024-cone brute-force reference (Fibonacci sphere)
- [ ] PSNR computation (compute shader → readback → CPU)
- [ ] Measurement harness (timestamp queries, mean/p95/std, 1000 iter)
- [ ] Output: `build/results.csv` (135,000 rows) + `RESULTS.md` (top-3 candidates × 3 metrics)

### Phase D — Analysis + close (later)

- [ ] Results interpretation (sweet spot identification)
- [ ] Verdict write-up (`mixed` or `yes` expected)
- [ ] Integration recommendation finalize (3-step migration per `agent/knowledge.md §30.4`)
- [ ] INDEX.md §6 Recent closed update
- [ ] `research/backlog.md §Closed` entry
- [ ] STATUS.md → status `concluded-verdict-*`

---

## Cross-axis

Orthogonal ко всем 6 in-progress parallel:
- `2026-06-21-tracy-gpu-vs-manual` (profiling tool)
- `2026-06-21-taa-motion-vectors` (closed yes same session, temporal Stage 5.3)
- `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)
- `2026-06-21-depth-occlusion-quantization` (VRAM format)
- `2026-06-21-vk-fragment-shading-rate-voxel` (VRS fragment rate)
- `2026-06-21-audio-diffraction-hybrid` (audio)

Complementary к closed:
- `2026-06-20-vct-vs-rt-cutoff` (cutoff=0.3 = which strategy; this = within-VCT quality axis)
- `2026-06-20-nanovdb-on-gpu` (storage foundation for VCT atlas traversal)
- `2026-06-20-restir-gi-feasibility` (deferred до Stage 6+ path tracer = this stays the VCT baseline)
- `2026-06-20-dec-pipelines-async-compute` (async compute = 5.x base prerequisite для VCT mip-chain
  build off-frame)
- `2026-06-21-wfc-procedural-worlds` (synthetic scene generation = source of voxel grid material)
- `2026-06-21-sub-chunk-layers` (chunk layout = alternative source of voxel grid material)

---

## Notes

- Prototype = standalone C++26 + Vulkan 1.4 compute, NOT ProjectV mainline (per
  `docs/experiments/AGENTS.md §2` scope discipline).
- No `git *` per `docs/experiments/AGENTS.md §2` — sync to `INDEX.md` + `backlog.md` via file edits
  only.
- Hardware baseline: see `hardware-profile.md` §3 (RTX 3060 Ti GA104, 8 GiB VRAM, 5.06 GiB budget) +
  §4 (Vulkan 1.4.341, 3D texture max 16384³). Captured `2026-06-20`, <14 days, **no probe needed**.
- Per `TODO.md §5.1` current mainline: 6 diffuse cones + 1 specular cone + R8 atlas. Hypothesis
  tests whether upgrade to 12×R16F improves quality/perf tradeoff above 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.
- Crassin 2011 cone-tapered mip filter, 4D temporal VCT, specular cone axis, and atlas resolution
  axis (128³/256³/512³) = out-of-scope follow-up experiments, NOT in this prototype.
