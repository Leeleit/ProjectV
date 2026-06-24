# Sources — genlayer-functional-biome-pipeline

## Tier 1: Minecraft GenLayer source (canonical)

- **Minecraft 1.12 `GenLayer.java:25-94`** — `initializeAllBiomeGenerators()` builds the canonical 20-layer chain: GenLayerIsland → GenLayerFuzzyZoom → GenLayerAddIsland → GenLayerZoom×4 → GenLayerAddSnow → GenLayerZoom + AddIsland×3 → GenLayerAddMushroomIsland → GenLayerZoom.magnify → GenLayerRiverInit → GenLayerZoom.magnify × b+2 → GenLayerRiver → GenLayerSmooth → GenLayerZoom.magnify → GenLayerBiome → GenLayerZoom × 2 → GenLayerHills → (Zoom + AddIsland + Shore + SwampRiver) × b → GenLayerSmooth → GenLayerRiverMix → GenLayerVoronoiZoom.
  - LCG PRNG constants: `6364136223846793005L`, `1442695040888963407L` (Mojang official).
  - `initWorldGenSeed`, `initChunkSeed`, `nextInt` methods.
  - Decompiled source: <https://www.javatips.net/api/MoKitchen-master/minecraft/net/minecraft/world/gen/layer/GenLayer.java>
- **Minecraft 1.12 `GenLayerBiome.java`** — temperature → biome class mapping via `warmBiomes[]`, `mediumBiomes[]`, `coldBiomes[]`, `iceBiomes[]` arrays. Source: <https://github.com/Awe23123/Optifine-1.12-SRC/blob/master/src/net/minecraft/world/gen/layer/GenLayerBiome.java>
- **MCP 1.12 Forge API docs:** <https://skmedix.github.io/ForgeJavaDocs/javadoc/forge/1.12.2-14.23.5.2859/net/minecraft/world/gen/layer/GenLayer.html>

## Tier 2: Reference implementations

- **Cubiomes (Cubitect)** — clean C library implementing the full MC 1.17 biome pipeline. Defines `LayerStack` with entry points at scales 1:1, 1:4, 1:16, 1:64, 1:256. Clean separation of `mapZoom`, `mapBiome`, `mapHills`, `mapRiver`, `mapSmooth`, `mapShore`, `mapSwampRiver`. ~3000 LoC, open-source MIT. <https://github.com/Cubitect/cubiomes/blob/master/layers.h>
- **AdityaGupta1/mega-minecraft (2026)** — GPU terrain gen with CUDA. Quote: "Since each column's biomes and height are independent of all other columns, this process lends itself very well to GPU parallelization. We combined small kernels into mega-kernels to reduce launch overhead." <https://github.com/AdityaGupta1/mega-minecraft>
- **hlsvortex/HLS_WebGPUPlugins (2026-05-07)** — Three.js r184 WebGPU engine, `biome.compute.wgsl` compute shader for biome classification from height/slope/moisture (single-pass, not layered chain). <https://github.com/hlsvortex/HLS_WebGPUPlugins>
- **AMD GPUOpen Work Graphs Mesh Node Sample (2026)** — `biomes.hlsl` — procedural biome weight function via layered Perlin noise, evaluated on demand (no stored texture). <https://github.com/GPUOpen-LibrariesAndSDKs/WorkGraphsMeshNodeSample/blob/main/meshNodeSample/shaders/biomes.hlsl>
- **B4rtekk1/Minerust (2025-12-27)** — Rust voxel engine, 11 biomes via FBM Perlin noise + multi-threaded async generation (rayon), FastNoise-Lite library. 200+ FPS on mid-range hardware. <https://github.com/B4rtekk1/Minerust>
- **draquel/VoxelWorlds (UE5)** — GPU-first voxel architecture in Unreal Engine 5: voxel generation and meshing entirely on GPU via compute shaders, `FVoxelBiomeRegistry` static biome definitions with temperature/moisture noise. <https://github.com/draquel/VoxelWorlds>
- **Markgatcha/ProceduralTerrainToolkit (Unity 2026)** — dual-noise biomes with synchronized moisture/temperature maps; optional GPU noise via compute shader + AsyncGPUReadback. <https://github.com/Markgatcha/ProceduralTerrainToolkit>
- **paulrobello/voxel-world (Rust 2026-02-02)** — Vulkan compute ray-marching voxel sandbox; 17 biomes via 5D climate noise (temperature, humidity, continentalness, erosion, weirdness); 32³ chunks. <https://github.com/paulrobello/voxel-world>

## Tier 3: ProjectV context

- **closed `2026-06-21-biome-transition-blending` (mixed)** — biome blending strategies; C_DistanceBlend_BiL = 0.640 µs/chunk; orthogonal to pipeline design.
- **closed `2026-06-21-trilinear-noise-interpolation` (mixed)** — coarse grid interpolation for terrain height; C_Trilerp_3 (19× reduction). Orthogonal axis.
- **closed `2026-06-21-wfc-procedural-worlds` (mixed)** — alternative constraint-based world gen approach.
- **closed `2026-06-21-gpu-procedural-noise-compute-kernels`** — OpenSimplex2 3D-S chosen for noise kernel.
- **closed `2026-06-21-sub-chunk-layers`** — per-Y-layer chunk structure.
- **`TODO.md §4.1`** — Stage 4.1 world gen specification.
- **`agent/knowledge.md`** — 3-step migration pattern.
- **`hardware-profile.md`** — Zen 3 5800X CPU + RTX 3060 Ti GPU + Vulkan 1.4.341.

## Cross-axis analysis

This experiment covers a previously-uncovered axis: **biome generation pipeline architecture** for Stage 4.1. Cross-axis:
- **orthogonal** to all in-progress parallel sessions (different axis).
- **complementary** to closed `biome-transition-blending` (blending = post-pipeline smoothing, not pipeline itself).
- **alternative** to closed `wfc-procedural-worlds` (constraint solver vs functional pipeline).