# Sources — Voxel Chunk Impostor Rendering for Far LOD

## Tier 1 — Primary (voxel LOD / impostor / billboard techniques)

1. **Minecraft Distant Horizons mod** — James Seibel et al. 2023-2026.
   CurseForge: https://www.curseforge.com/minecraft/mc-mods/distant-horizons
   Modrinth: https://modrinth.com/project/uCdwusMi
   GitLab: https://gitlab.com/distant-horizons-team/distant-horizons
   Iris integration docs: https://shaders.properties/current/reference/mod-support/distant_horizons/
   DH API docs: https://distant-horizons-f74d0f.gitlab.io/
   Canonical production voxel LOD mod (21.6M+ downloads). Separate rendering pipeline for LOD chunks with own depth buffers and projection matrices. LOD mesh aggregation (8 chunks → 1 LOD chunk). Supports up to 4096 block render distance. DH API exposes `dh_terrain`/`dh_water` programs + shadow pass. Fog/mipmap bias for LOD transition.

2. **Aokana: A GPU-Driven Voxel Rendering Framework for Open World Games** — arXiv 2505.02017, May 2025.
   https://arxiv.org/html/2505.02017v1
   Octree-like LOD: 8 LOD0 chunks → 1 LOD1 chunk with density threshold (≥2 non-empty voxels → aggregate). Implicit octree CPU-side for chunk culling. Chebyshev distance LOD selection. GPU-driven rendering pipeline with chunk selection / tile selection / ray marching / Hi-Z passes.

3. **Project Ascendant — Vulkan Guide** (2024-2026).
   https://www.vkguide.dev/docs/ascendant/ascendant_geometry/
   Two strategies for far voxel rendering: sprite renderer (billboard-per-voxel) vs 3-quad per block. Block ID uploaded to GPU for texturing. Per-block culling patterned after Nanite. Pure-block rendering mode for extreme distances.

4. **Voxceleron2 Engine** (Ayanali, 2025-2026).
   https://voxceleron2.ayanali.net/
   Hybrid Sparse LOD Octree. Elastic chunks with varying internal resolution per LOD index. Chebyshev distance LOD selection creates concentric detail shells. Deferred rendering with SSAO + Bloom.

5. **SimLOD: Simultaneous LOD Generation and Rendering for Point Clouds** — Schütz, Herzberger, Wimmer 2024. ACM CGIT.
   https://github.com/m-schuetz/SimLOD
   https://arxiv.org/html/2310.03567v1
   Octree LOD for point clouds. Voxels in inner nodes (128³ grid), points in leaves. CUDA compute rendering with atomic ops. Incremental LOD generation. Linked-list chunk storage for dynamic growth.

6. **Screen Space Billboard Voxel Buffer** — ZCU / Naos, 2015 (updated).
   https://naos-be.zcu.cz/server/api/core/bitstreams/81edf7fb-9041-43e4-9bef-59a85729faf5/content
   Geometry shader generates billboard quads from voxel point cloud. Sparse 3D texture for voxel ID lookup. Mipmap-based LOD control. Fixed-size buffer limits vertex count.

7. **Jedjoud10/VoxelTerrain** — Unity voxel terrain engine, 2023-2026.
   https://github.com/jedjoud10/VoxelTerrain
   Impostors (advanced billboards) for distant props. Automatic texture captures at frame start with custom camera matrices. Texture arrays for variant handling. Compute-based culler before indirect rendering.

## Tier 2 — Foundational (octrees, SVO, GPU-driven rendering)

8. **Laine & Karras 2010 "Efficient Sparse Voxel Octrees"** — IEEE TVCG / NVIDIA Research.
   https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010i3d_paper.pdf
   Foundational SVO. Pointer-based sparse octree with bitmask encoding. Parent-pointer-free traversal. Direct inspiration for all octree-impostor strategies.

9. **Crassin et al. 2009 "GigaVoxels: Ray-Guided Streaming for Very Large 3D Models"** — NVIDIA / INRIA.
   Ray-guided LOD selection for voxel octrees. Hierarchical page table for out-of-core voxel data. Production reference for octree-based impostor streaming.

10. **Haar & Aaltonen 2015 (Ubisoft) "GPU-Driven Rendering Pipelines"** — SIGGRAPH 2015.
    GPU-driven culling and indirect draw. Reference for GPU compute-based impostor update dispatch pattern.

11. **Majercik et al. 2018 "An Efficient Ray-Box Intersection Algorithm for Real-Time Rendering of Large Dynamic Voxel Scenes"** — Eurographics 2018.
    Splat voxel billboards for rough visibility + fragment shader ray-box for precise visibility. No precomputation — suitable for fully dynamic scenes.

## Tier 3 — Cross-references (ProjectV experiments)

12. **`2026-06-21-lod-mesh-downsampling`** — closed `mixed`. Explicitly defers impostor/billboard chunks as Stage 4.2 chunk 3. This experiment builds on that gap. Downsampler kernel + stitch strategy for LOD meshing. Impostor layer is complementary to LOD meshing (impostors for far distance, meshes for near).

13. **`2026-06-21-lod-transition-strategy`** — closed `mixed`. LOD transition via geomorph/crossfade/pop. Impostor layer requires its own transition strategy between full mesh and impostor (Distant Horizons fog-based approach).

14. **`2026-06-21-precomputed-atmospheric-sky`** — closed `yes`. Sky rendering LUT approach parallels impostor texture pre-bake methodology.

15. **`2026-06-21-mesh-shader-mega-instancing`** — closed `mixed`. Mesh shader for instanced rendering — potential rendering path for impostor quads (Pattern C amplification shader + indirect draw).

16. **`2026-06-22-surface-micro-detail`** — closed `mixed`. Uses "impostor" as conceptual lowest-LOD detail (Hoppe 1997). Orthogonal axis (per-fragment normal vs per-chunk billboard).
