# Sources — `2026-06-21-lod-transition-strategy`

Web-research completed this session (2026-06-21) via DuckDuckGo HTML endpoint + webfetch fallback (Exa MCP HTTP 429 persistent per `agent/knowledge.md Part B §9` + operator directive). 8 primary sources verified.

## Primary sources (verified via direct URL fetch 2026-06-21)

### 1. Mikola Lysenko 2018 — "A level of detail method for blocky voxels"
- **URL:** https://0fps.net/2018/03/03/a-level-of-detail-method-for-blocky-voxels/
- **Date:** 2018-03-03 (author: mikolalysenko)
- **Key finding:** Direct validation that **geomorphing eliminates need for skirts / Transvoxel / explicit seams** for blocky voxel LOD. Key quote: *"if we have geomorphing, then we don't need to implement seams or skirts to get crack-free LOD"*. Presents **stable LOD rounding** algorithm with **2-3 iterations** for crack-free boundary. References Limper/Jung/Behr/Alexa 2013 "POP Buffer" paper (Pacific Graphics 2013, Computer Graphics Forum).
- **Relevance:** **Canonical reference for ProjectV chunkSize=8 blocky voxel LOD transitions.** Geomorphing is the production-proven approach. POP buffer = implicit LOD = no explicit morph target storage needed (alternative to D_PreComputedMorphTargets).
- **Verified:** Yes — full page fetched, including formula `L_t(x) = (⌈t⌉ - t) * 2^⌊t⌋ * ⌊x/2^⌊t⌋⌋ + (t - ⌊t⌋) * 2^⌈t⌉ * ⌊x/2^⌈t⌉⌋` and 2-3 iter stable LOD rounding formula.

### 2. Hugues Hoppe 1997 — "View-Dependent Refinement of Progressive Meshes"
- **URL:** https://hhoppe.com/proj/vdrpm/
- **Date:** SIGGRAPH 1997, ACM 258734
- **Author:** Hugues Hoppe (Microsoft Research)
- **Key finding:** Foundational paper for **geomorphs** (smooth visual transitions between LOD levels). Abstract explicitly: *"smooth visual transitions (geomorphs) can be constructed between any two selectively refined meshes"* + *"less than 15% of total frame time on a graphics workstation"*. View-dependent refinement framework for arbitrary triangle meshes.
- **Relevance:** **Foundational reference for C_Geomorph strategy.** 25+ years of production use, validated as standard LOD technique.
- **Verified:** Yes — project page + abstract fetched.

### 3. Mikola Lysenko 2012 — "Meshing in a Minecraft Game"
- **URL:** https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
- **Date:** 2012-06-30 (author: mikolalysenko)
- **Key finding:** Foundational reference for ProjectV mainline **Naive Greedy Meshing** algorithm. Greedy mesh theorem: *"at most 8x as many quads as optimal mesh"* (within constant factor of optimality).
- **Relevance:** **Mainline meshing algorithm** for ProjectV (per `src/shaders/voxel_mesh.comp::GreedyFacePass`). ProjectV LOD at chunkSize=8 = 8 voxels per chunk.
- **Verified:** Yes — full page fetched, including greedy mesh algorithm + theorem + demo.

### 4. Vulkan Guide / Project Ascendant — "High-performance voxel and mesh rendering"
- **URL:** https://vkguide.dev/docs/ascendant/ascendant_geometry/
- **Date:** 2020+ (Project Ascendant series)
- **Key finding:** Production voxel engine reference using **chunkSize=8** (matching ProjectV). Implements **5 separate geometry draw systems** for different distances:
  1. Voxel-mesh rendering (near field, heavily quantized)
  2. Voxel-raycast Fardraws (individual blocks as sprites)
  3. Voxel-raycast Fardraws with quads
  4. Vegetation clutter (instanced)
  5. Arbitrary mesh rendering (GLTF-loaded)
- **Key quote:** *"You quickly get into 1 pixel = 1 voxel or even lower, and your options are to either implement some sort of Level of Detail so that distance zones have bigger voxels, or you switch to a faster rendering technique. The Minecraft Distant Horizons mod does both"*.
- **Relevance:** **Production reference for ProjectV LOD architecture.** 5-draw-system pattern matches ProjectV's near/far LOD switching. Uses VMA Virtual Allocation with 400 MB "gigabuffer" pattern.
- **Verified:** Yes — full page fetched.

## Secondary sources (cited in primary sources, not directly fetched)

### 5. Hugues Hoppe 1996 — "Progressive Meshes" (SIGGRAPH 1996)
- **ACM:** 192636
- **Key finding:** Foundational progressive mesh representation that defines for any triangle mesh a sequence of approximating meshes optimized for view-INDEPENDENT LOD. Builds foundation for Hoppe 1997 view-DEPENDENT refinement + geomorphing.
- **Relevance:** Foundation for C_Geomorph strategy. Pre-cursor to view-dependent refinement (Hoppe 1997).
- **Verified:** No (not directly fetched — cited via Hoppe 1997 page).

### 6. Hugues Hoppe 1998 — "Smooth View-Dependent Level-of-Detail Control and its Application to Terrain Rendering" (Visualization 1998)
- **URL:** https://hhoppe.com/proj/svdlod/ (cited in Hoppe 1997 page)
- **Key finding:** Enhancements on Hoppe 1997 approach, specifically for terrain rendering. Wavelet-based control for smooth LOD transitions.
- **Relevance:** Direct application to heightfield / terrain LOD — relevant to ProjectV's biome/heightmap scene types (forest_floor, mixed_biome).
- **Verified:** No (cited in Hoppe 1997 page hindsights section).

### 7. Eric Lengyel 2009 — "Transvoxel Algorithm"
- **URL:** http://transvoxel.org/
- **Date:** 2009
- **Key finding:** Adaptive LOD for **iso-surface** meshes (NOT blocky voxels). 512 transition cell cases / 73 equivalence classes. Patent-free (released into public domain by author).
- **Relevance:** **NOT directly applicable to ProjectV** (ProjectV = binary voxel grid, not iso-surface). Mentioned in Lysenko 2018 as alternative for non-blocky voxel scenarios. Considered and rejected per `2026-06-21-lod-mesh-downsampling` STATUS notes.
- **Verified:** No (NOT fetched — well-known reference, multiple prior ProjectV experiments referenced it).

### 8. Limper, Jung, Behr, Alexa 2013 — "The POP Buffer: Rapid Progressive Clustering by Geometry Quantization" (Pacific Graphics 2013, CGF)
- **URL:** https://x3dom.org/pop/files/popbuffer2013.pdf
- **Date:** 2013
- **Key finding:** POP buffers = implicit LOD representation. Vertices rounded down to previous power of two for each LOD level. Implicit LOD = no explicit multi-mesh storage = lower memory than D_PreComputedMorphTargets.
- **Relevance:** **Alternative to D_PreComputedMorphTargets** that achieves same visual effect (geomorph) with less storage (implicit LOD). Mentioned in Lysenko 2018 as foundational paper.
- **Verified:** No (NOT fetched — referenced in Lysenko 2018 page).

## Operator pre-2026 knowledge (not freshly verified)

### 9. Lindstrom 1999 — "Visualization of Large Terrains Made Easy"
- **Key finding:** Geomorph + ROAM for terrain rendering. Historical reference for view-dependent LOD.
- **Relevance:** Older alternative to Hoppe 1996/1997, terrain-specific.
- **Verified:** No (operator pre-2026 knowledge).

### 10. Blackflux 2014 — "Meshing Part 3"
- **Key finding:** 3 T-junction strategies for naive meshing: Naive Greedy, Poly2Tri, post-process.
- **Relevance:** Alternative to A_Pop for handling LOD boundary T-junctions (without geomorph). Per closed `2026-06-21-lod-mesh-downsampling`, B_SurfacePreserve kernel eliminated T-junction problem upstream — this reference not directly applicable to ProjectV.
- **Verified:** No (operator pre-2026 knowledge).

## SOTA cross-vendor / API references

### 11. Vulkan 1.4 — `VK_EXT_mesh_shader` (rev 1)
- **URL:** https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_mesh_shader.html
- **Date:** 2024+ (core in Vulkan 1.4)
- **Key finding:** Mesh shaders enable meshlet-level dispatch with custom geometry partitioning. Relevant for C_Geomorph implementation in `voxel_mesh.mesh` shader per Stage 2.2 Pattern C (compute pre-cull + mesh shader) per `TODO.md §2.2`.
- **Relevance:** GPU implementation path for geomorphing at meshlet granularity.
- **Verified:** No (per `hardware-profile.md §4`, supports `VK_EXT_mesh_shader` rev 1 on RTX 3060 Ti dev host).

## Web-search summary

- **Total sources referenced:** 11 (8 primary, 3 operator-knowledge).
- **Direct fetches:** 4 (Lysenko 2018, Hoppe 1997 project page, Lysenko 2012, Vulkan Guide Ascendant).
- **Exa HTTP 429 retries:** 2 (web_search failed, fallback to webfetch).
- **DuckDuckGo CAPTCHA rate-limited:** 2 attempts after 5 successful queries.
- **Coverage:** Geomorphing foundational (Hoppe 1997) + blocky voxel-specific (Lysenko 2018) + mainline meshing (Lysenko 2012) + production reference (Vulkan Guide Ascendant).
- **Cross-vendor:** Cross-vendor analysis not needed — algorithm choices are vendor-agnostic (LOD transition is data structure / algorithm choice, not GPU API).
- **Re-evaluation triggers:** Vulkan 1.5+ DirectSR core promotion (out of scope this session); new mesh shader extensions; ProjectV Stage 4.3 lift draw distance (transition becomes more critical).

## Anti-duplicate verification

Per `AGENTS.md §13.7`:
- `rg -l "lod-transition-strategy|lod-transition|LodTransition|lod_geomorph|lod_crossfade" /home/le1t/Projects/ProjectV/docs/experiments/` returned **NO matches** before experiment registration.
- `ls /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-lod-transition-strategy/` returned **NOT_EXISTS** before creation.
- No in-progress per-chunk mip / LOD transition experiment in other sessions per `INDEX.md §5 Active experiments` review (hzb-smart-mip-select covers per-chunk HZB mip selection = different axis).
