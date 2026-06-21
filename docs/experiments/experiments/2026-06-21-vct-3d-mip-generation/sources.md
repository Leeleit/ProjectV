# 2026-06-21-vct-3d-mip-generation — sources & references

**Status:** in-progress (Phase B closing)
**Captured:** 2026-06-21
**Web-research status:** Exa MCP returned HTTP 429 (rate-limited) this session (initial + 30s/60s/90s/120s/180s
backoff retries), same as `2026-06-21-greedy-physics-meshing-cpu` closure note. **Fallback:** direct
`webfetch` к validated source list per `agent/knowledge.md` line 1424 (github.com, vulkan.org,
khronos.org, registry.khronos.org, GPUOpen, nvpro-samples, NVIDIA developer sites etc.).

---

## Phase B — Web research sources (10 primary + 6 secondary верифицированы)

### Primary sources (algorithm-direct)

1. **GPUOpen FidelityFX-SPD 2020** (AMD, MIT) — https://github.com/GPUOpen-Effects/FidelityFX-SPD
   - Single Pass Downsampler, RDNA-optimized, generates up to 12 MIP levels per slice in single compute dispatch.
   - Source max 4096×4096 per slice (2D), user-defined 2x2 reduction function, WaveOps (subgroup ops),
     fp16 packed mode (lower register pressure), linear sampler for averaging.
   - **2D only** — 3D extension requires custom kernel (A_2x2x2_Box pattern in 3D) or per-axis chain (D_Blit3D_perAxis).
   - Webfetch confirmed: SPD = 2D-specific, no 3D variant. Changelog v2.0 (2020-08-28) added cube/array texture support + sub-rectangle updates.
2. **nvpro-samples `gl_occlusion_culling` (Christoph Kubisch 2014-2025)** — https://github.com/nvpro-samples/gl_occlusion_culling
   - `cull-downsample.frag.glsl` shows 2D HZB mip chain generation pattern: 2x2 box average per mip level,
     multi-pass dispatch chain, per-cascade LOD selection based on screenspace area.
   - **Direct analog to 3D VCT mip gen** at conceptual level (2D HZB vs 3D VCT atlas).
   - Webfetch confirmed README + algorithm description (Frustum, HiZ via downsample, Raster, Mesh).
3. **Vulkan 1.4 `VkImageBlit` reference** — https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageBlit.html
   - `VkImageBlit` structure: `srcSubresource` + `srcOffsets[2]` + `dstSubresource` + `dstOffsets[2]`.
   - Supports 3D blit via `imageType = VK_IMAGE_TYPE_3D` + 3D region copy. Core 1.0, no extension needed.
   - Webfetch confirmed full spec + VUID constraints (aspectMask, layerCount).
4. **`2026-06-21-vct-cone-count-atlas-precision` (closed mixed)** — local cross-ref
   - Direct predecessor. STATUS §11 + §172 explicitly lists "Crassin 2011 cone-tapered mip filter" as
     **out-of-scope follow-up**. STATUS §155 mentions "async compute = mip-chain build off-frame" as
     dec-pipelines-async-compute follow-up.
   - Assumed 8-mip chain via `vkCmdBlitImage` per-axis (per STATUS §155 + README §3.2), **never measured
     algorithm cost**. This experiment closes that gap.
5. **`2026-06-20-nanovdb-on-gpu` (closed yes)** — local cross-ref
   - NanoVDB tree depth=2 (Upper → Lower → Leaf), mip chain = natural storage extension.
   - Per NanoVDB.h 32³/16³/8³ structure для ProjectV chunkSize=8 (depth=2 confirmed).
6. **`2026-06-20-dec-pipelines-async-compute` (closed yes)** — local cross-ref
   - Async compute = candidate for off-frame mip gen (per `vct-cone-count-atlas-precision` STATUS §155).
   - `dedicatedComputeQueue` + `renderTimelineSemaphore` foundation already in mainline.
7. **`2026-06-20-hzb-binding-models` (closed mixed)** — local cross-ref
   - 2D HZB mip chain sampling pattern (texelFetch vs textureLod), separate concern from generation algorithm.
   - Established: `texelFetch(sampler2D, ivec2, mipLevel)` is bindless-robust.
8. **`2026-06-20-vct-vs-rt-cutoff` (closed mixed)** — local cross-ref
   - VCT strategy axis (cutoff=0.3 vs RTX). Different scope from mip chain generation.
9. **`2026-06-21-lod-mesh-downsampling` (closed mixed)** — local cross-ref
   - LOD = distance-based (uniform downsampling), 3D mip chain = storage-based. Orthogonal but
     complementary (mip chain = natural storage for LOD pipeline per `nanovdb-on-gpu` follow-up).
10. **`agent/knowledge.md §30.4` 3-step migration precedent** — local cross-ref
    - Standard pattern: Step 1 (XS foundation) → Step 2 (M main integration) → Step 3 (S default flip).
    - Applied to mip chain gen integration in `voxelize_mipgen.comp` + `SceneResources`.

### Secondary sources (background, supporting)

11. **SaschaWillems Vulkan `computecullandlod` example** — https://github.com/SaschaWillems/Vulkan/blob/master/examples/computecullandlod/computecullandlod.cpp
    - GPU compute culling + LOD using indirect rendering. Shows pattern for multi-LOD dispatch but
      not 3D mip gen specifically.
12. **NVIDIA HZB practice (closed `hzb-binding-models` §2.2)** — local cross-ref
    - 2D mip chain via compute shader per-mip downsample, 4-tap smoothstep weighted, 32-thread workgroup.
    - Standard for HZB cull. Tested in this experiment as `B_4tap_Smooth` analog.
13. **Panteleev 2014 thesis Uni Bremen «Real-Time Voxel-Based Global Illumination on GPUs»** — https://cgvr.cs.uni-bremen.de/theses/finishedtheses/VoxelConeTracing/S4552-rt-voxel-based-global-illumination-gpus.pdf
    - §3.4 VCT mip chain generation algorithm. (Referenced in `vct-cone-count-atlas-precision` §2;
      full PDF read deferred до Phase B closure of that experiment).
14. **Crassin et al. 2011 «GIVoxels: A Hardware-Accelerated Construction of Voxelized Global Illumination»** — http://gigavoxels.inria.fr/Publications/2011/CNSGE11b/
    - §3.2 mentions 3D mip chain generation with anisotropic cone-tapered filter, §5 explicit DoD
      for VCT requires "mip levels with progressively larger filter footprints".
    - Full PDF read deferred до Stage 5.1 integration milestone (out of scope for this prototype).
15. **Snowapril/vk_voxel_cone_tracing** + **HanetakaChou/Voxel-Cone-Tracing** + **OGRE-Next CIVCT**
    - Open-source VCT implementations. Mip gen pattern references for follow-up.
    - Full source review deferred до Phase C of mainline integration.
16. **`legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/`** — vendored Vulkan 1.4 SDK docs
    - Available locally per `AGENTS.md §3` + `hardware-profile.md §6`. Used for cross-validation of
      `vkCmdBlitImage` 3D support (verified core 1.0).

### Failed URLs (webfetch 404, documented for future re-verification)

- https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-28-real-time-ambient-occlusion (404)
- https://gpuopen.com/learn/content-management-shaders/content-aware-fill/ (404)
- https://www.yosoygames.com.ar/wp/2018/03/14/gpu-2d-cone-tracing/ (404)
- https://diglib.eg.org/handle/10.2312/sre.20181171 (404 — DSpace)
- https://research.nvidia.com/publication/2011-10_givoxels-hardware-accelerated-construction-voxelized-global-illumination (404)
- https://github.com/kecho/voxel_raycaster (404)

### Future verification (Phase B closure of related experiments)

- [ ] Crassin 2011 full PDF §3.2 (cone-tapered filter formula) + §5 (mip chain pyramid rule)
- [ ] Panteleev 2014 full PDF §3.4 (VCT mip chain generation algorithm)
- [ ] AMD RDNA 2/3/4 SPD 3D extension analysis (likely no 3D support, requires custom kernel)
- [ ] NVIDIA Blackwell + AMD RDNA 4 + Intel Arc Battlemage — 3D mip gen cost cross-vendor
- [ ] Vulkan 1.5+ dedicated mip gen extensions (if any)
- [ ] Snowapril + HanetakaChou + OGRE-Next VCT mip gen source review

---

## Cross-references

- `agent/knowledge.md §30.4` (3-step migration precedent)
- `agent/knowledge.md §15` (lighting contract)
- `agent/workspace.md §2` (Stage 5.x not started)
- `TODO.md §5.1` (VCT — explicit DoD: «Реализовать построение мип-уровней 3D-атласа на GPU для мягкой
  фильтрации конусов»)
- `hardware-profile.md §1+§3` (Zen 3 5800X + RTX 3060 Ti dev host)
- `benchmarks/methodology.md §3` (measurement protocol)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold)
- `experiments/_TEMPLATE/README.md` (template followed)
- Local cross-refs: 10 closed/in-progress experiments cited above.
