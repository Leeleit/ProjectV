# Sources — 2026-06-21-lod-mesh-downsampling

> Полный список источников для эксперимента. Каждая ссылка верифицирована
> (открыт первоисточник, проверены год/авторы/контекст) per `AGENTS.md §5.3` +
> `docs/experiments/AGENTS.md §4`.

## Project-internal sources (cross-refs only, не дублировать)

- `agent/workspace.md §2` — Nearest Gap explicit callout for Stage 4.2 chunk 2.
- `agent/knowledge.md` — 3-step migration precedent.
- `src/voxel/VoxelWorld.hpp:78` — `chunkSize = 8`.
- `src/voxel/VoxelWorld.cpp:1175-1208` — `SelectLodLevelForDistance` + `AssignLodLevels`.
- `src/voxel/VoxelWorld.hpp:54` — `VoxelChunk::lodLevel` byte field.
- `src/voxel/VoxelWorld.cpp:207-253` — `QueueChunkRebuildRequest` + per-chunk rebuild queue.
- `TODO.md §4.2` — Stage 4.2 DoD (uniform downsampling + T-junction stitch).
- `docs/experiments/2026-06-20-meshing-algo-comparison` — Naive Greedy baseline at LOD 0.
- `docs/experiments/2026-06-20-nanovdb-on-gpu` — NanoVDB mip chain (LOD storage candidate).
- `docs/experiments/2026-06-21-sub-chunk-layers` — orthogonal vertical-layer axis
  (same scenes + seeds + Material enum for direct comparability).
- `docs/experiments/2026-06-20-cache-oblivious-chunk-tree` — cache-line sweet spot for
  chunkSize=8 working set at LOD 1 (2 KiB → fits L1d).
- `docs/experiments/2026-06-20-simd-procedural-noise` — splitmix32 + Perlin helpers pattern
  (separate copy, не shared).
- `docs/experiments/2026-06-20-dec-pipelines-async-compute` — async-compute foundation
  (relevant for GPU downsample dispatch).
- `docs/experiments/2026-06-21-gpu-procedural-noise-compute-kernels` — memory-bound GPU
  dispatch pattern (LOD downsample expected same behavior).
- `docs/experiments/hardware-profile.md §1/§2` — CPU/RAM data captured 2026-06-20.
- `docs/experiments/benchmarks/methodology.md §3` — measurement protocol.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% gain threshold.

## External sources — Phase A (downsampler kernels + stitch strategies)

### Primary

- **0fps.net "A level of detail method for blocky voxels"** (Mikola Lysenko, 2018-03-03) —
  POP buffers (Progressively Ordered Primitive) + vertex clustering + **stable LOD
  rounding** (2-3 iterations per vertex shader, gives seamless LOD without skirts).
  URL: https://0fps.net/2018/03/03/a-level-of-detail-method-for-blocky-voxels/
  Key finding: "If we have geomorphing, then we don't need to implement seams or
  skirts to get crack-free LOD." Applicable as a Vertex Shader pattern for the
  higher-LOD neighbor's mesh.

- **Transvoxel Algorithm (Lengyel 2009)** — gold standard for iso-surface voxel LOD;
  512 transition cell cases (73 equivalence classes), 4² boundary face sampling,
  patent-free (used in Space Engineers + Astroneer).
  URL: https://transvoxel.org/
  Key finding: **NOT applicable to ProjectV's Naive Greedy pipeline** — Transvoxel
  is for Marching Cubes-style iso-surface meshes, not blocky voxels.

- **Cinevva Blog "Building an open world in the browser, part 9: Transvoxel started
  with a scaffold"** (2026-02-25) — confirms Transvoxel patent-free status, lists 3
  seam-fix strategies (Transvoxel, geomorphing, skirt geometry).
  URL: https://app.cinevva.com/blog/2026-02-25-open-world-browser-part-09-transvoxel-first-cut.html
  Key finding: For blocky voxels, the standard approach is to **expand faces** (our
  `Y_TJunctionPad`), **don't create T-vertices** (our `Z_NeighborLocked`), or
  **post-process shader fill** (rejected for our use case).

- **Blackflux "Meshing in Voxel Engines – Part 3"** (2014-03-02) — three T-junction
  strategies: Naive Greedy (accept), Poly2Tri (CPU poly2tri.org library), post-process
  shader (z-buffer).
  URL: https://blackflux.wordpress.com/2014/03/02/meshing-in-voxel-engines-part-3/
  Key finding: Naive Greedy is "blazing fast" and produces "sufficient low" triangle
  count, but T-junctions need to be handled. Poly2Tri rejected for our use case
  (overhead); post-process deferred.

- **Voxel.wiki "T-Junctions"** — canonical 4 workarounds for the T-junction problem.
  URL: https://voxel.wiki/wiki/t-junction/
  Key finding: (1) expand faces (our `Y_TJunctionPad`), (2) fill pixel gaps in
  post-process, (3) don't create T-vertices (our `Z_NeighborLocked` — cleanest), (4)
  directly raytrace.

- **Nick Gildea "Dual Contouring: Seams & LOD for Chunked Terrain"** (2014-09) —
  DC's natural property of handling different leaf sizes without special seam
  handling.
  URL: http://ngildea.blogspot.com/2014/09/dual-contouring-chunked-terrain.html
  Key finding: For DC, "the differently sized leaf nodes in each octree" naturally
  resolve seams. Not applicable to Naive Greedy but validates the "look at neighbor's
  faces" pattern.

- **Smooth Voxel Mapping: a Technical Deep Dive on Real-time Surface Nets and Texturing**
  (DreamCat Games, 2020-08-01) — SurfaceNets + boundary voxel lookup pattern.
  URL: https://bonsairobo.medium.com/smooth-voxel-mapping-a-technical-deep-dive-on-real-time-surface-nets-and-texturing-ef06d0f8ca14
  Key finding: "When meshing a chunk, you look not only at the voxels in a chunk, but
  all adjacent voxels as well. This will make sure we don't miss any cubes, and the
  meshes will calculate identical surface points on the boundary." This is the
  theoretical basis for our `Z_NeighborLocked` strategy.

### Supplementary

- **GPU Gems 2 Ch 2 "Terrain Geometry Clipmaps"** (Losasso & Hoppe 2004) —
  concentric rings, ring k is twice the area at half vertex resolution.
- **bpodwinski/TerrainCDLODBabylonJs** (Feb 2025) — modern GLSL/TypeScript CDLOD port.
  URL: https://github.com/bpodwinski/TerrainCDLODBabylonJs
- **Transvoxel-XNA: Voxel Terrain with Level of Detail** (BinaryConstruct, 2012-02-01) —
  XNA port of Transvoxel, validates production usage.
  URL: https://www.binaryconstruct.com/posts/transvoxel-xna/
- **Dexyfex "Voxels and Seamless LOD Transitions"** (2016-07-14) — octree-based
  voxel LOD with edge-blending factor.
  URL: https://dexyfex.com/2016/07/14/voxels-and-seamless-lod-transitions/

## External sources — Phase B (production voxel LOD systems)

### Primary

- **Cubyz DeepWiki "Chunk Meshing and LOD"** (2026-03-19) — production reference
  for ProjectV-style voxel LOD.
  URL: https://deepwiki.com/PixelGuys/Cubyz/4.2-chunk-meshing-and-lod
  Key findings:
  - **LOD 0-16** (1×1×1 to 16×16×16 voxels per mesh element)
  - Per-LOD `faceBuffers` + `lightBuffers` (separate per LOD level)
  - `getLodFromDistanceAndSize` function — distance-based LOD assignment
  - GPU compute cull (`fillIndirectBuffer.comp`) + `glMultiDrawElementsIndirect`
  - **No special seam handling** — relies on Naive Greedy at the lower-LOD buffer
    being meshed independently. (Cubyz author's choice — works for them because the
    cave density is high enough that the boundary mismatch is rarely visible.)
  - Closest production reference to ProjectV's needs.

- **Voxceleron2 (ayanali.net)** — hybrid Sparse LOD Octree architecture.
  URL: https://voxceleron2.ayanali.net/
  Key finding: "Chunks maintain a fixed world-space dimension (CHUNK_SIZE) but the
  internal voxel resolution changes based on a Level of Detail (LOD) index."
  `LOD_level = floor(Distance / BaseDistance)`. LOD 0 = raw voxels, LOD 1 = 1/2
  resolution (1/8th memory), etc. **LOD only works with Sparse Octree**, not
  fixed-grid. Future reference for Stage 4.3 if mainline moves to octree.

- **Aokana: A GPU-Driven Voxel Rendering Framework for Open World Games**
  (arXiv 2505.02017, May 2025) — 8-child octree aggregation with **density=2
  threshold** (if ≥2 of 8 children are non-empty, create parent voxel with average
  color).
  URL: https://dl.acm.org/doi/10.1145/3728299
  Key finding: This is **similar to our A_Majority3D** but with explicit density=2
  threshold. Direct production precedent for GPU LOD aggregation.

- **Teknologicus Vorxel devlog "Voxel volume mipmaps generation via compute
  shader"** (2024-10-08) — voxel volume mipmaps via compute shader:
  0.4 sec on GPU vs 17 sec on CPU for 78M voxels.
  URL: https://teknologicus.itch.io/vorxel/devlog/812265/voxel-volume-mipmaps-generation-via-compute-shader
  Key finding: Direct production precedent for **GPU-side** LOD dispatch (per
  dispatching kernel, build mips). Confirms our `vkCmdDispatch` + workgroup pattern
  is production-realistic.

- **Leadwerks "GPU Voxel Downsampling with Compute Shaders"** — same compute-shader
  approach with 8x8x8 workgroups.
  URL: https://www.leadwerks.com/community/blogs/entry/2752-gpu-voxel-downsampling-with-compute-shaders/
  Key finding: "Compute shader offers the best performance" for voxel downsampling.
  8x8x8 workgroup is the production sweet spot.

- **GPUOpen FidelityFX SPD (Single Pass Downsampler)** — RDNA-optimized, up to 12
  mip levels in single dispatch, subgroup operations, fp16 packed mode.
  URL: https://github.com/GPUOpen-Effects/FidelityFX-SPD
  Key finding: Reference for GPU LOD dispatch pattern; ProjectV only needs 3 mip
  levels (LOD 0/1/2/3), simpler dispatch.

- **OptiFine Issue #7567 "[Optimization] Potential ideas for LOD"** (2023-10-16) —
  OptiFine author's negative evidence on LOD utility.
  URL: https://github.com/sp614x/optifine/issues/7567
  Key finding: "LOD is really only useful for having more render distance, not
  saving performance. I don't think it's good in OptiFine." — but ProjectV is in
  the voxel camp (per `meshing-algo-comparison` vertex-bound), so LOD has real
  value here.

### Supplementary

- **Cinevva "Building an open world in the browser, part 6: Clipmaps changed the
  plot"** (2026-02-25) — geometry clipmaps + geomorphing.
  URL: https://app.cinevva.com/blog/2026-02-25-open-world-browser-part-06-clipmaps.html
- **Cinevva "Building an open world in the browser, part 5: CDLOD"** — CDLOD
  with vertex morphing.
- **Southern & Gain "g-mesh" (2003)** — geomorph mesh for continuous LOD.
  URL: https://people.cs.uct.ac.za/~jgain/wp-content/papercite-data/pdf/southern2003.pdf
- **Tatarchuk "Progressive Buffers" (2006)** — view-dependent LOD with
  geomorphing.
  URL: https://advances.realtimerendering.com/s2006/Chapter1-Out-of-Core_Rendering_of_Large_Meshes_with_Progressive_Buffers.pdf
- **CDLOD (Strugar 2010)** — Continuous Distance-Dependent LOD.
- **Cuberact/godot-cuberact-planet-chunked-lod** (Mar 2026) — Godot port of CDLOD
  for planet rendering. Validates cross-engine CDLOD implementation.

## External sources — Phase C (ProjectV-relevant constraints)

- **Vulkan Guide "High-performance voxel and mesh rendering"** (vkguide.dev) —
  Ascendant engine uses **8×8×8 chunks** (matches ProjectV's chunkSize=8).
  URL: https://www.vkguide.dev/docs/ascendant/ascendant_geometry/
  Key finding: "A given chunk will be 1 'unit' of mesh generation, and 1 mesh to
  draw." Confirms chunkSize=8 is a production sweet spot for voxel engines.

- **SeanPP/VoxelMVP** (2026-02-16) — GPU-driven voxel renderer with
  MultiDrawIndirect + compute shader culling, ~6ms GPU frame time on 65 chunks
  radius.
  URL: https://github.com/SSeanPP/VoxelMVP
  Key finding: 8 bytes/vertex (two packed uints), 71,825 slots at 65 radius. Direct
  production reference for ProjectV's cull pattern.

- **nvpro-samples/vk_compute_mipmaps** (2021-07-24) — NVIDIA's
  pyramid-based mipmap generation reference for Vulkan.
  URL: https://github.com/nvpro-samples/vk_compute_mipmaps
  Key finding: "nvpro_pyramid" library — GLSL compute shader template for mipmap
  generation schedule. User completes the shader by defining reduction kernels.
  Direct reference for GPU LOD dispatch.
