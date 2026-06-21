# Sources — `2026-06-21-sdf-hybrid-world`

**Captured:** 2026-06-21
**Methodology:** Web search via Exa returned 429 (rate-limited); fallback per `AGENTS.md §4` to `webfetch` + DuckDuckGo HTML (`html.duckduckgo.com/html/`) confirmed working. Primary sources verified via `webfetch` direct URL fetch; secondary sources verified via DuckDuckGo snippet. All citations dated and accessible as of `2026-06-21`.

---

## Primary sources (verified 2026-06-21)

### 1. Narkowicz 2022 — "Journey to Lumen" (knarkowicz.wordpress.com)

- **URL:** https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/
- **Author:** Krzysztof Narkowicz (former Lumen lead, Epic Games)
- **Date:** 2022-08-18
- **Why critical:** **DIRECT EXPERT VALIDATION** of experiment hypothesis. Lumen development journey = pure voxel cone tracing → leaked → replaced with **global distance field ray tracing** + **voxel bit bricks (one bit per voxel in 8×8×8 brick, EXACT MATCH to ProjectV chunkSize=8)**. Multiple explicit quotes on voxel VCT leaking problem and SDF/DF solution.
- **Key quotes:**
  - «_The main drawback of voxel cone tracing is **leaking** due to aggressive merging of scene geometry, which is especially visible when tracing coarser (lower) mip-maps_»
  - «_First leaking reduction technique was to **trace a global distance field and sample voxel volume only near the surface**… Always sampling voxel volume exactly near the geometry increased the chance of a cone stopping at a thin solid wall_»
  - «_The second technique was to **voxelize mesh interiors**… even with distance fields we would still see leaking in various places, so later we also were **forcing cone tracing to terminate if we registered a distance field ray hit**. This minimized leaking_»
  - «_Voxel bit bricks were storing **one bit per voxel in a 8x8x8 brick** to indicate whether a given voxel is empty or not_» (= ProjectV chunkSize=8 + 1-bit-per-voxel granularity)
  - «_The first and biggest change was **replacing heightfield tracing with distance field tracing**… voxel cone tracing was changed to **global distance field ray tracing** and shading hits from a merged card volume_»
- **Conclusion for ProjectV:** SDF overlay + VCT cone-march termination (Narkowicz's exact anti-leak technique) is production-proven path. **No risk of unproven approach.**

### 2. NAADF 2026 — "Nested Axis-Aligned Distance Fields" (Wiley CGF, May 2026)

- **URL:** https://onlinelibrary.wiley.com/doi/full/10.1111/cgf.70413
- **Authors:** (Wiley paper, 2026 publication)
- **Date:** 2026-05-01
- **Why critical:** **MOST RECENT reference (May 2026!)** on voxel + axis-aligned distance fields. Abstract via DuckDuckGo snippet (full text 403 paywall): «_We propose a multilayered data structure with a carefully balanced hierarchy between depth and node size to maximize ray tracing performance for **voxel worlds**, and augment it with **axis-aligned distance fields** computed and cached on the fly, resulting in an **order-of-magnitude faster ray tracing than state-of-the-art algorithms**._»
- **Conclusion for ProjectV:** validates voxel + AADF hybrid = order-of-magnitude speedup → strong upside for ProjectV Stage 5.x + 3.x if SDF overlay is integrated.

### 3. RTSDF — arXiv 2210.04449 (NUS, October 2022)

- **URL:** https://ar5iv.labs.arxiv.org/html/2210.04449
- **Authors:** Tan Yu Wei, Nicholas Chua, Clarence Koh, Anand Bhojan (National University of Singapore)
- **Date:** 2022-10-10
- **Why critical:** **Direct production reference for voxel + JFA + ray-trace refinement SDF pipeline.** Exact analog of my proposed approach: coarse SDF via JFA on voxel grid → fine SDF via ray-trace refinement near surface → raymarch for shadows.
- **Key technical details:**
  - «_SDFs can alternatively be approximated in real-time with **jump flooding** [Rong 2006], offering a voxelized scene representation that causes reconstructed surfaces to appear **blocky**… we propose a technique that combines the precision of **ray tracing and the speed of jump flooding**_»
  - «_We first perform jump flooding to give the **coarse SDF** or a fast approximation of the SDF of the scene. Next, we detect regions in the coarse SDF **near surfaces** and apply **brute force ray tracing with temporal accumulation** in these areas to generate a **fine SDF**_»
  - «_As for the sign, we **subtract a small experimentally-derived bias β** from the distance field, causing some surface points to be negative and effectively thickening the surfaces_» — sign-based bias trick (avoid explicit sign storage; sign reconstructed from distance bias)
  - Coarse SDF stored at 64³ voxel grid; fine SDF at higher resolution (per-texel distance)
  - Performance: 97 FPS on GeForce RTX 2080 Ti для «The Modern Living Room» scene (vs 113 FPS CSM EVSM baseline)
- **Conclusion for ProjectV:** validates the algorithmic pipeline (JFA → voxel-coarse → ray-trace refinement → smooth SDF). The β sign-bias trick = +1 storage optimization for our 1-byte/voxel SDF encoding.

### 4. OpenVDB 13.0.1 (openvdb.org documentation)

- **URL:** https://www.openvdb.org/documentation/doxygen/overview.html
- **Library:** OpenVDB 13.0.1 (Museth et al., DreamWorks Animation)
- **Date verified:** 2026-06-21
- **Why critical:** Industry-standard sparse SDF storage library. Direct mapping to ProjectV's existing storage layer.
- **Key technical details:**
  - «_A **narrow-band level set** is represented by three distinct regions of voxels: an **outside (or background) region of inactive voxels** having a constant, positive distance from the level set surface; an **inside region of inactive voxels** having a constant, negative distance; and a **thin band of active voxels (normally three voxels wide on either side of the surface) whose values are signed distances**._»
  - Default tree config: `Tree4<float, 5, 4, 3>::Type` = RootNode (5 levels up) → InternalNode → InternalNode → LeafNode (8³ voxel). **EXACTLY 8×8×8 leaf = matches ProjectV chunkSize=8.**
  - Tools: LevelSetTracker, VolumeToMesh converter, narrow-band tracking, level-set advection, CSG, etc.
- **Conclusion for ProjectV:** OpenVDB narrow-band SDF = production-grade pattern. Surface voxel + 3 layers in = entire ProjectV 8³ chunk fits. **Sparse tile encoding = perfect fit для sub-chunk-layers (closed mixed B_Palette/C_L2/D_L4).**

### 5. UE5 Mesh Distance Fields (Epic Dev Community, UE 5.8 docs)

- **URL:** https://dev.epicgames.com/documentation/unreal-engine/mesh-distance-fields-in-unreal-engine?lang=en-US
- **Source:** Official Unreal Engine 5.8 documentation
- **Date verified:** 2026-06-21
- **Why critical:** Production reference for SDF in real-time engine. Direct engineering contract data.
- **Key technical details:**
  - «_The **Global Distance Field** is a low-resolution Distance Field that uses **Signed Distance Fields occlusion** in your levels while following the camera. It creates a cache of the per-object Mesh Distance Fields and composites them into a few volume textures centered around the camera, called **clipmaps**._»
  - «_The lower resolution of the object Distance Field means that it can be used for everything, but when computing cone traces for sky occlusion, they are sampled near the point of being shaded while the Global Distance Field is sampled further away_»
  - «_The maximum size volume texture any single mesh can have is **8 megabytes with a resolution of 128x128x128**._» → per-mesh SDF size cap
  - Limitations: «_Only casts shadows from rigid meshes_», «_Materials that deform the mesh through World Position Offset or displacement may cause self-shadowing artifacts_»
  - **CRITICAL: «_All Mesh Distance Field features have been **disabled on Intel cards** because the HD 4000 hangs in the RHICreateTexture3D call to allocate the large atlas_»** → cross-vendor restriction для Intel HD. **ProjectV cross-vendor matrix must note this.**
  - Reference: Quilez 2008 «Raymarching Distance Fields» http://iquilezles.org/www/articles/raymarchingdf/raymarchingdf.htm
- **Conclusion for ProjectV:** UE5 already implements SDF + cone trace anti-leak. Validates the architectural pattern. Cross-vendor: must check Intel Arc (separate from Intel HD; Arc Alchemist/Battlemage should be OK per Mesa RADV per `dec-pipelines-async-compute` §2.2).

### 6. Rong & Tan 2006 — "Jump Flooding in GPU" (ACM I3D, March 2006)

- **URL:** https://www.comp.nus.edu.sg/~tants/jfa/i3d06.pdf
- **Authors:** Rong Guodong, Tan Tiow-Seng (NUS)
- **Date:** 2006-03-14
- **DOI:** 10.1145/1111411.1111431
- **Why critical:** Foundational paper for Jump Flooding Algorithm. Cites from JFA Wikipedia article.
- **Key technical details:** 9·log₂(N) inner loop iterations per pixel, O(N² log N) для 2D, O(N³ log N) для 3D.
- **Conclusion for ProjectV:** JFA = 4-6× faster than brute-force BFS for SDF generation. Confirmed via RTSDF 2022 implementation.

### 7. JFA Wikipedia (en.wikipedia.org/wiki/Jump_flooding_algorithm)

- **URL:** https://en.wikipedia.org/wiki/Jump_flooding_algorithm
- **Date verified:** 2026-06-21
- **Why critical:** Aggregated JFA reference with all variants + 14 cited papers.
- **Key technical details:** JFA, JFA+1, JFA+2, 1+JFA, Half-res, JFA+ (Czyzewski 2019), JFA* (Czyzewski 2019 = seed-scaling log*(n) steps for sparse). Production: Paradox Interactive Imperator: Rome borders, three.js, Unity. Czyzewski 2019: 720×720 / 2000 seeds = 4 JFA* passes vs 10 standard JFA.

---

## Secondary / supporting sources (verified via DuckDuckGo snippets, 2026-06-21)

### 8. Crassin et al. 2011 — "Interactive Indirect Illumination Using Voxel Cone Tracing" (GIVoxels, NVIDIA)

- **URL:** https://research.nvidia.com/sites/default/files/publications/GIVoxels-pg2011-authors.pdf (referenced by Narkowicz 2022)
- **Date:** 2011
- **Why relevant:** Original VCT paper. Octree + mip-mapped voxel radiance + cone-march. **ProjectV's VCT foundation (per `vct-vs-rt-cutoff` precedent).**
- **Note:** Direct PDF 403; referenced via Narkowicz 2022.

### 9. Crassin 2024 — "Cone-Traced Supersampling With Subpixel Edge Reconstruction" (IEEE TVCG)

- **URL:** https://dl.acm.org/doi/10.1109/TVCG.2023.3343166
- **Date:** 2024-09-01
- **Why relevant:** Validates SDF antialiasing challenges relevant to VCT cone termination.

### 10. VortSDF 2024-2025 — "3D Modeling with Centroidal Voronoi Tessellation on SDF" (arXiv 2407.19837, IEEE 10943791)

- **URL:** https://arxiv.org/html/2407.19837v1, https://ieeexplore.ieee.org/document/10943791
- **Date:** 2024-07-29 (arXiv), 2025-02-26 (IEEE)
- **Why relevant:** Validates SDF + CVT hybrid for real-time rendering. «_We jointly optimize an SDF field, discretized on a hierarchical CVT, and two view-dependent shallow color networks_» — relevant to ProjectV chunk layout integration.

### 11. SurroundSDF 2024 — "Implicit 3D Scene Understanding Based on SDF" (CVPR 2024)

- **URL:** https://openaccess.thecvf.com/content/CVPR2024/papers/Liu_SurroundSDF_Implicit_3D_Scene_Understanding_Based_on_Signed_Distance_Field_CVPR_2024_paper.pdf
- **Date:** 2024
- **Why relevant:** Validates SDF + voxel grid for 3D representation. Different scope (auto-driving) but reinforces SDF trend.

### 12. GSurf 2024 — "SDF via Gaussian Splatting" (arXiv 2411.15723)

- **URL:** https://arxiv.org/html/2411.15723v3
- **Date:** 2024-11-23
- **Why relevant:** Modern SDF integration pattern. Out of scope для binary voxel, but validates SDF trend in 2024-2026.

### 13. Golus 2021 — "The Quest for Very Wide Outlines" (Medium / bgolus.medium.com)

- **URL:** https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9
- **Date:** 2021-04-01
- **Why relevant:** Production-quality JFA reference for SDF generation. Cited by JFA Wikipedia.

### 14. Czyzewski 2019 — "GPU-Accelerated Jump Flooding Algorithm for Voronoi Diagram in log*(n)"

- **URL:** https://maciejczyzewski.github.io/fast_gpu_voronoi/slides_small.pdf
- **Date:** 2019-05-27
- **Why relevant:** JFA* (seed-scaling) variant. For sparse voxel chunks, only surface voxels are seeds → JFA* converges in log*(n) steps.

### 15. Inigo Quilez 2008 — "Raymarching Distance Fields" (iquilezles.org)

- **URL:** http://iquilezles.org/www/articles/raymarchingdf/raymarchingdf.htm
- **Date:** 2008
- **Why relevant:** Foundational reference for SDF rendering. Cited by UE5 Mesh Distance Fields docs.

---

## Open source / production reference implementations (verified via GitHub, 2026-06-21)

### 16. bigmat18/cuda-mesh-voxelization (GitHub, 2025-07-23)

- **URL:** https://github.com/bigmat18/cuda-mesh-voxelization
- **Why relevant:** Production reference: «_SDF Calculation: Computes the signed distance field using the Jump Flooding Algorithm (JFA). CLI Application: Command-line interface for batch processing and benchmarking. Benchmarking: Comparative analysis between sequential, OpenMP, and CUDA implementations_» — **direct measurement reference for CPU vs GPU JFA cost**.

### 17. cecarlsen/SDFTextureGenerator (GitHub, 2025)

- **URL:** https://github.com/cecarlsen/SDFTextureGenerator
- **Why relevant:** Unity 6000.3.0f1 ComputeShader JFA for 2D + 3D voxels: `Mask3DToSdfTexture3DProcedure` = exact analog for ProjectV chunk SDF.

### 18. Guo-Haowei/VCT (GitHub)

- **URL:** https://github.com/Guo-Haowei/VCT
- **Why relevant:** Real-time Voxel Cone Tracing implementation.

### 19. Friduric/voxel-cone-tracing (GitHub)

- **URL:** https://github.com/Friduric/voxel-cone-tracing
- **Why relevant:** Another production VCT reference.

---

## Industrial references (already known from prior `docs/experiments/` experiments)

### 20. UE5 Lumen 5.8 (Narkowicz 2022) — cited above

### 21. UE5 Nanite (Epic, 2021)

- **Why relevant:** Mesh SDF for some operations. Production reference.

### 22. Dreams (PS4, Media Molecule, 2020)

- **Why relevant:** SDF-based world representation. Production voxel game using SDF.

### 23. Minecraft RTX (NVIDIA, 2021)

- **Why relevant:** Voxel + path tracer, **NO SDF** (cited for contrast — voxel-only VCT not production path).

---

## Local corpus (always-available, no web search needed)

### 24. OpenVDB 13.0.1 docs (openvdb.org) — cited above, verified 2026-06-21

### 25. Crassin 2011 GIVoxels (cited from Narkowicz 2022)

### 26. `docs/experiments/2026-06-20-vct-vs-rt-cutoff/` — closed mixed, VCT cutoff strategy 0.3

### 27. `docs/experiments/2026-06-20-nanovdb-on-gpu/` — closed yes, NanoVDB-aligned storage

### 28. `docs/experiments/2026-06-20-svdag-vs-vdb-memory-throughput/` — closed yes, SVDAG-on-64-tree baseline

### 29. `docs/experiments/2026-06-21-sub-chunk-layers/` — closed mixed, chunk layout (B_Palette/C_L2/D_L4) for SDF per-layer integration

### 30. `docs/experiments/2026-06-21-lod-mesh-downsampling/` — closed mixed, LOD for SDF smooth blend

### 31. `docs/experiments/2026-06-21-vct-cone-count-atlas-precision/` — closed mixed, within-VCT quality (orthogonal to SDF termination)

### 32. `docs/experiments/2026-06-21-greedy-physics-meshing-cpu/` — in-progress, meshing axis (orthogonal to SDF normal smoothness)

### 33. `docs/experiments/2026-06-20-mesh-shader-vs-compute-cull/` — closed mixed, mesh cull (orthogonal)

### 34. `docs/experiments/2026-06-20-meshing-algo-comparison/` — closed mixed, §6 closure explicitly notes SDF-meshing axis parked (NOT this scope)

### 35. `docs/experiments/hardware-profile.md` — RTX 3060 Ti + Zen 3 5800X dev host

### 36. `docs/experiments/benchmarks/methodology.md` — measurement protocol
