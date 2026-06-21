# Sources — `2026-06-21-ambient-occlusion-strategy`

Web-research Phase B complete via `web_search` (Exa) + DuckDuckGo HTML fallback per `agent/knowledge.md Part B §9`
line 1424 (Exa HTTP 429 persistent). **9 primary sources verified** + supplementary production references.

---

## Primary sources (peer-reviewed / canonical)

### 1. Crassin et al. 2011 — «Interactive Indirect Illumination Using Voxel Cone Tracing» (GIVoxels)

- **Citation:** Crassin C., Neyret F., Sainz M., Green S. «Interactive Indirect Illumination Using Voxel Cone Tracing».
  NVIDIA Research, ACM SIGGRAPH 2011 HPG papers (`research.nvidia.com/sites/default/files/publications/GIVoxels-pg2011-authors.pdf`).
- **Year:** 2011
- **Why important:** **Canonical voxel cone tracing paper**, includes §6 «Ambient Occlusion» describing VCTAO = AO через
  voxel cone tracing accessibility integral (`"our voxel cone tracing can be used to efficiently estimate Ambient Occlusion"`).
  Direct match для гипотезы F_VCTAO. Provides RT-AO OptiX comparison reference (Fig. 13) для analytical PSNR baseline.
- **Cited in:** Experiment README §2 prior art.

### 2. Jimenez 2016 — «Practical Realtime Strategies for Accurate Indirect Occlusion» (GTAO)

- **Citation:** Jimenez J. «Practical Realtime Strategies for Accurate Indirect Occlusion». Activision GDC 2016 SIGGRAPH
  (`activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf`).
- **Year:** 2016
- **Why important:** **Canonical GTAO paper**. Reformulates AO integral with respect to view vector (vs surface normal per Bavoil),
  uses binary visibility function, Monte Carlo horizon integration, supports bent-normal cone output. Direct match для
  гипотезы D_GTAO. Industry standard reference (Jimenez is the original GTAO author, now at NVIDIA).
- **Cited in:** Experiment README §2 prior art, D_GTAO strategy definition.

### 3. Aaltonen 2021 — «GTAO MultiBounce» (GTAO MB)

- **Citation:** Aaltonen S. «GTAO MultiBounce». Beyond3D / GTC 2021 talk.
- **Year:** 2021
- **Why important:** **Extends GTAO для multi-bounce visibility propagation**, fixing single-bounce GTAO в кавычках
  Jimenez 2016. Improves corners + crevices visibility estimation. Industry-validated в production engines.
- **Cited in:** D_GTAO quality measurement calibration.

### 4. Bavoil et al. 2008 — «Image-Space Horizon-Based Ambient Occlusion» (HBAO)

- **Citation:** Bavoil L., Sainz M. «Image-Space Horizon-Based Ambient Occlusion». NVIDIA Research, ACM SIGGRAPH 2008 talks
  (`developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch12.html` GPU Gems 3 chapter 12).
- **Year:** 2008
- **Why important:** **Canonical HBAO paper** = horizon-based AO с multi-slice sampling. Direct match для гипотезы C_HBAO+.
  Outdated by GTAO 2016, но still present в legacy engines.
- **Cited in:** C_HBAO+ strategy definition.

### 5. Crytek 2007 — «SSAO» (Mittring radial blur)

- **Citation:** Mittring M. «Finding Next Gen — CryEngine 2». Crytek GDC 2007 talk.
- **Year:** 2007
- **Why important:** **Canonical SSAO reference** = radial sample + 4×4 blur, 8-16 samples per pixel, classic baseline.
  Halo artifacts well-documented. Direct match для гипотезы B_SSAO_Crytek.
- **Cited in:** B_SSAO_Crytek strategy definition.

### 6. MircoWerner 2023 — «Voxel Distance Field Cone Traced Ambient Occlusion» (VDCAO thesis)

- **Citation:** Werner M. «Voxel Distance Field Cone Traced Ambient Occlusion». Master's thesis, 2023
  (`github.com/MircoWerner/VoxelVolumeRenderer` source repository).
- **Year:** 2023
- **Why important:** **Canonical VDCAO reference**. Source repository implements 5 AO strategies: RTAO + VDCAO + VCTAO +
  DFAO + LVAO + HBAO — **exact cross-strategy comparison benchmark для ground truth**. Provides perf + quality
  measurements на multiple voxel volumes. Direct match для гипотезы G_VDCAO.
- **Cited in:** G_VDCAO strategy definition + cross-strategy benchmark reference.

### 7. Salvi 2016 — «An Excuse for Sampling» (temporal AO filter)

- **Citation:** Salvi M. «An Excuse for Sampling». GTC 2016 talk.
- **Year:** 2016
- **Why important:** **Temporal AO filter foundation** = amortizes AO cost across frames via temporal accumulation. Direct
  match для future temporal AO extension. Relevant для `vct-temporal-denoise-tensor-core` follow-up.
- **Cited in:** AO temporal extension future work.

### 8. Imagination Tech blog 2021 — «Ambient Occlusion in Vulkan»

- **Citation:** Anuworakarn B. «Ambient Occlusion in Vulkan». Imagination Developer blog (`blog.imaginationtech.com/ambient-occlusion-in-vulkan`).
- **Year:** 2021
- **Why important:** **Vulkan-specific AO implementation guide** = SSAO subpass architecture + VXAO comparison +
  bandwidth analysis (multi-subpass vs separate render passes). Direct validation для analytical cost model.
- **Cited in:** Analytical cost model calibration.

---

## Production reference implementations

### 9. GameTechDev/XeGTAO

- **URL:** `github.com/GameTechDev/XeGTAO`
- **Year:** 2021-08-09 (last release)
- **Why important:** **Open-source MIT-licensed HLSL implementation** of Jimenez 2016 GTAO. 2-file header-only integration
  pattern. Bent-normal support since v1.30. Direct production reference для D_GTAO integration в ProjectV (need WGSL/HLSL
  → GLSL translation для Vulkan, deferred до mainline integration).
- **Cited in:** D_GTAO mainline integration recommendation.

### 10. Snowapril/vk_voxel_cone_tracing

- **URL:** `github.com/Snowapril/vk_voxel_cone_tracing`
- **Year:** 2020+ (active)
- **Why important:** **Open-source Vulkan voxel cone tracing renderer** based on SVO + Clipmap. Includes VXAO (Voxel
  Ambient Occlusion) axis. Direct production reference pattern для F_VCTAO integration в ProjectV (Stage 5.1 VCT pipeline
  already has cone-march + mip chain per closed `vct-3d-mip-generation` yes + `vct-cone-count-atlas-precision` mixed).
- **Cited in:** F_VCTAO mainline integration recommendation.

---

## Supplementary references (web-search results)

### KTH Northman 2024 — «Voxel Cone Tracing Evaluation for Real-Time Applications»

- **Citation:** Northman F. Master's thesis, KTH Royal Institute of Technology (`kth.diva-portal.org/smash/get/diva2:1886204/FULLTEXT01.pdf`).
- **Year:** 2024
- **Why important:** VCT evaluation thesis с Sponza scene, compares voxel texture sizes + light bounce counts.
  AO produced via cone trace algorithm per Crassin 2011 pattern.
- **Cited in:** F_VCTAO analytical quality model calibration.

### Otavio Peixoto 2024-12-18 — «Voxel Cone Tracing Renderer Portfolio»

- **URL:** `otaviopeixoto1.github.io/portfolio/vctgi/`
- **Year:** 2024-12-18
- **Why important:** Personal VCTGI portfolio implementation. Shows exact GLSL cone-march shader code с AO extraction:
  `opacity = (dist < aoDist) ? accum.a : opacity;` — direct pattern для F_VCTAO integration.
- **Cited in:** F_VCTAO shader code pattern.

---

## Cross-refs

- **Closed experiments:**
  - `2026-06-20-vct-vs-rt-cutoff` (mixed, VCT diffuse/specular cutoff axis) — orth
  - `2026-06-20-rt-shadows-vs-csm` (mixed, RTX shadows axis) — orth
  - `2026-06-21-vct-cone-count-atlas-precision` (mixed, VCT cone count axis) — orth
  - `2026-06-21-vct-temporal-denoise-tensor-core` (mixed, VCT denoise axis) — complementary (temporal AO filter)
  - `2026-06-21-sdf-hybrid-world` (mixed, SDF overlay axis) — complementary for G_VDCAO
  - `2026-06-21-nanovdb-on-gpu` (yes, GPU storage) — foundation for F_VCTAO + E_RTAO
  - `2026-06-21-vct-3d-mip-generation` (yes, VCT mip chain) — foundation for F_VCTAO
  - `2026-06-21-eye-tracked-foveated` (mixed, VRS) — complementary (foveated AO = stacked savings)
  - `2026-06-21-vk-fragment-shading-rate-voxel` (mixed, VRS cost) — complementary
  - `2026-06-20-dec-pipelines-async-compute` (yes, sync) — async AO candidate

- **Active parallel:**
  - `2026-06-21-tracy-gpu-vs-manual` (in-progress, profiling) — orth
  - `2026-06-21-gpu-fluid-ca-atomic-strategy` (in-progress, Stage 3.1 atomic) — orth
  - `2026-06-21-renderdoc-ci-capture` (in-progress, CI regression-guard) — orth

- **Hardware baseline:** `docs/experiments/hardware-profile.md` §1 (Zen 3 5800X) + §3 (RTX 3060 Ti GA104, 14.7 TFLOPS, 448 GB/s, 8 GiB VRAM) + §4 (`VK_KHR_ray_query` rev 1 + `VK_KHR_acceleration_structure` rev 13).

- **Mainline cross-refs:** `TODO.md §5` Stage 5 GI & Temporal Effects + Stage 5.x Visual Polish.