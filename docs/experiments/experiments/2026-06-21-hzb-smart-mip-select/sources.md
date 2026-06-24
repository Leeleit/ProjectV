# Sources — 2026-06-21-hzb-smart-mip-select

Web-research completed 2026-06-21 via DuckDuckGo HTML endpoint + `webfetch` (Exa HTTP 429 persistent per the web_search fallback chain).

## Primary sources (verified this session)

### 1. Greene, Kass, Miller 1993 — Hierarchical Z-Buffer Visibility

- **Authors:** Ned Greene, Michael Kass, Gavin Miller
- **Year:** 1993
- **Venue:** SIGGRAPH 1993 Proceedings, ACM 166147, pp. 231-238
- **URL:** https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/greene93.pdf
- **Why important:** Canonical paper establishing the HIZ cull algorithm. Octree spatial subdivision + Z pyramid + temporal coherence. Foundation для всех последующих HIZ implementations, including ProjectV.
- **Verification:** Verified PDF URL accessible via `webfetch`; citation ACM 166147 confirmed.

### 2. Mike Turitzin 2020 — Hierarchical Depth Buffers

- **Author:** Mike Turitzin
- **Date:** Mar 25, 2020
- **URL:** https://miketuritzin.com/post/hierarchical-depth-buffers/
- **Why important:** **Exact pattern statement matching our hypothesis:** «Hi-Z occlusion culling, for instance, works by projecting a bounding volume into screen-space and using the projected size to choose the appropriate mip level (so that a fixed number of texels are accessed per occlusion test)». 35% particle rendering speedup measured.
- **Verification:** Verified via `webfetch` (full page content retrieved, quote confirmed).

### 3. Omlor & Radicke 2025 — Two-Pass Occlusion Culling for Dynamic Voxel Scenes based on HZB

- **Authors:** Omlor, Radicke
- **Date:** Jul 2025
- **Venue:** IEEE Xplore document 11321175
- **URL:** https://ieeexplore.ieee.org/document/11321175 (also at https://www.semanticscholar.org/paper/Two-Pass-Occlusion-Culling-for-Dynamic-Voxel-Scenes-Omlor-Radicke/0bb4ba379e4ba24d66cd202f03c5d581a48641a8)
- **Why important:** **Direct voxel + HZB reference** для Stage 4.3 lift draw distance + Stage 2.2 Pattern C mesh shader integration. TPOC (Two-Pass Occlusion Culling) — mesh-shading pipeline adapts HZB to efficiently occlude voxel scene.
- **Verification:** Citation found via DuckDuckGo search, IEEE Xplore URL accessible.

### 4. DeepWiki 2026-04-06 — Metallic Engine GPU-Driven Culling

- **Source:** DeepWiki
- **Date:** Apr 6, 2026
- **URL:** https://deepwiki.com/af8a2a/metallic/5.2-gpu-driven-culling:-meshletcullpass-and-hzb
- **Why important:** Modern production Vulkan pattern (Metallic engine 2026): multi-stage compute pipeline with frustum + backface cone + HZB + Cluster LOD traversal system для dynamic LOD selection. **Modern reference для combined HZB+LOD strategy.**
- **Verification:** URL retrieved via DuckDuckGo search.

### 5. RasterGrid 2010 — Hierarchical-Z Map Based Occlusion Culling

- **Author:** RasterGrid
- **Year:** 2010
- **URL:** https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
- **Why important:** OpenGL FBO-based mip chain generation. Pre-Vulkan reference, но mip chain algorithm identical. **Implementation reference для `BuildHizMipChain` (`HizCulling.cpp:326-369`).**
- **Verification:** URL retrieved via DuckDuckGo search.

## Secondary sources

### 6. Nick Darnell — Hierarchical Z-Buffer Occlusion Culling

- **URL:** https://www.nickdarnell.com/hierarchical-z-buffer-occlusion-culling/
- **Why important:** SIGGRAPH 2008 Advances in Real-Time Rendering §3.3.3 + Stephen Hill «Rendering with Conviction» GDC talk. DX11 sample implementation reference.

### 7. Tobias Garpenhall — Occlusion Culling (UE5)

- **URL:** https://www.tobiasgarpenhall.com/occlusion-culling
- **Why important:** UE5 OcclusionAssembler pattern (CPU queues GPU commands for occlusion cull pass). Production reference for game engine integration.

### 8. chaoticbob 2024 — Mesh Shading Part 4: Culling

- **URL:** https://chaoticbob.github.io/2024/01/27/mesh-shading-part-4.html
- **Why important:** Meshlet culling reference для Stage 2.2 Pattern C integration with HIZ output.

### 9. zeux/meshoptimizer

- **URL:** https://github.com/zeux/meshoptimizer
- **Why important:** Meshlet bounding info для cluster culling. Library reference.

### 10. JarkkoPFC/meshlete

- **URL:** https://github.com/JarkkoPFC/meshlete
- **Why important:** Intra-object meshlet occlusion testing via visibility cones.

## ProjectV mainline sources

- `agent/workspace.md §2` line 52 — explicit Nearest Gap callout для per-chunk HZB mip selection
- `TODO.md §2.1` — HZB Occlusion Culling stage
- `src/render/HizCulling.cpp:326-369` — `BuildHizMipChain` (already builds mip chain через `vkCmdBlitImage`)
- `src/render/HizCulling.cpp:800-805` — `hizExtentAndMipCount[3] = 0u` (hardcoded mip=0, baseline = A_UniformMip0)
- `src/render/HizCulling.hpp:48-52` — `HizCullingPushConstants` structure (96 bytes)
- `src/shaders/hzb_cull.comp:33-90` — `AabbVisibleAgainstMip` (per-mip texelFetch loop)
- `src/shaders/hzb_cull.comp:102` — `const int mipLevel = int(pushConstants.hizExtentAndMipCount.w);` (single mip from push constants)
- `src/render/Renderer.cpp:1344-1350` — `RecordHzbCullingDispatch` call site (will need mip compute injection)
- `src/voxel/VoxelWorld.hpp:78` — `chunkSize=8`
- `src/app/Camera.cpp` — `kMainlineVisibleSceneMaxDistance=64m` (current Stage 2.1 cap; Stage 4.3 target = 128m)
- `agent/knowledge.md` — 3-step migration precedent

## Cross-refs (closed experiments in same session)

- `2026-06-20-hzb-binding-models/` (closed mixed) — `texelFetch` pattern (already adopted в `hzb_cull.comp:85`)
- `2026-06-20-dec-pipelines-async-compute/` (closed yes) — async compute foundation
- `2026-06-21-greedy-physics-meshing-cpu/` (closed yes) — CPU prototype precedent (single-session analytical model + scenes + seeds)
- `2026-06-21-sub-chunk-layers/` (closed mixed) — synthetic scenes + seeds (1, 7, 42, 1234, 31337)
- `2026-06-21-depth-occlusion-quantization/` (closed yes) — PSNR >50 dB threshold для false-negative cull validation

## Khronos / Vulkan spec

- Vulkan 1.4 spec — `vkCmdBlitImage` для mip chain generation (already used в `HizCulling.cpp:359`)
- `VK_KHR_synchronization2` (core 1.3) — для compute→draw barrier (already adopted)

## Hardware baseline

- `docs/experiments/hardware-profile.md §1` (Zen 3 5800X dev host `obvium`, governor=`powersave`) — captured 2026-06-20
- `docs/experiments/benchmarks/methodology.md §3` — measurement protocol (1000 iter + 10 warmup; we used 30 iter + 5 warmup due to wall time budget for A_UniformMip0 baseline at 10M+ texels/chunk)
