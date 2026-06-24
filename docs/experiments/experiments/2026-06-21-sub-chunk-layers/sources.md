# Sources — sub-chunk-layers (верифицированный список)

Web-research complete `2026-06-21`. ~14 ключевых источников верифицированы по году/автору/контексту.
Подробные highlights — в README §2 (prior art).

## Minecraft Java Edition sub-chunks (3D biomes since 1.18)

- [Minecraft Wiki Chunk format](https://minecraft.wiki/w/Chunk_format) — `[NBT List / JSON Array] sections`
  = 16×16×16 blocks each, `PalettedContainer<BlockState>` + `PalettedContainer<Biome>` (64 entries 4×4×4).
  Updated 2026-06-11. Biomes paletted and live in `Level.Sections[].biomes.palette/data`.
- [FabricMC/yarn DeepWiki — Chunk Data Structures](https://deepwiki.com/FabricMC/yarn/4.1-chunk-data-structures)
  — Published 2025-10-11. `ChunkSection` 16×16×16 blocks + biomes 4×4×4 = 64 entries per section.
  PalettedContainer adaptive bits-per-value selection.
- [Chunk Format (wiki.vg protocol docs)](https://wiki.vg/Chunk_Format) — `Paletted Container` structure
  description, 4096 blocks + 64 biomes max per chunk section.
- [ChunkSection (yarn 1.18-rc4+build.1 API)](https://maven.fabricmc.net/docs/yarn-1.18-rc4%2Bbuild.1/net/minecraft/world/chunk/ChunkSection.html) —
  `PalettedContainer<BlockState>` + `PalettedContainer<Biome>` official Fabric API.
- [1.18.1 LevelChunkSection API](https://mappings.dev/1.18.1/net.minecraft.world.level.chunk.LevelChunkSection.html) —
  `BIOME_CONTAINER_BITS` field, `biomes` container reference.

## Minecraft Bedrock Edition (4D sub-chunks: x, y, z, storage layer)

- [Bedrock Edition level format (wiki.vg)](https://wiki.vg/Bedrock_Edition_level_format) — `SubChunk` 16×16×16,
  block data stored across 4 dimensions (x, y, z, **storage layer**), Layer 0 = terrain, Layer 1 = liquid/snow.
  3D biome format with bits-per-value encoding.
- [uNmINeD — DEV: Bedrock 1.18 3D biome format](https://unmined.net/2021/12/10/dev-bedrock-1-18-3d-biome-format/)
  — Reverse-engineering 2021-12-10. Paletted biome data per subchunk with 1-byte header.

## Layered voxel chunks (alternative formats)

- [SHARD Format (scrayos)](https://scrayos.net/justchunks-shard-format/) — Published 2024-11-04. Layered/merge
  semantics with `BlockMask` + `BiomeMask`, arbitrary positive/negative offsets, async merge optimization.
  3D biomes support with per-block storage trade-off.
- [scrayosnet/shard-format GitHub](https://github.com/scrayosnet/shard-format) — Java implementation,
  2024-10-13 initial commit.
- [Tunact124/atlas (ATLAS + AARF)](https://github.com/Tunact124/atlas) — Published 2026-03-05. **AARF =
  columnar storage** that pivots section-based Minecraft data into vertical columns. Plus HOCS off-heap
  storage + content-adaptive compression (LZ4/ZSTD/RLE). 3 stars, Java 98.6%.

## Staged / cave-specific voxel generation

- [Hytale NStagedChunkGenerator (vulpeslab/hytale-docs)](https://github.com/vulpeslab/hytale-docs/blob/5c38d02e/src/content/docs/modding/content/world-generation.md)
  — commit `5c38d02e`. Staged pipeline: NBiomeStage → NBiomeDistanceStage → NTerrainStage → NPropStage →
  NTintStage → NEnvironmentStage. Material cache + buffer capacity factors. Zone System + Biome System +
  Cave System + Prefab System.
- [Cubyz CaveMap (PixelGuys/Cubyz DeepWiki)](https://deepwiki.com/PixelGuys/Cubyz/5.3-cave-and-underground-generation)
  — Published 2026-03-19. `CaveMap` fragment = 64×64×64 blocks with 1-bit per block (32 KiB/fragment).
  `CaveBiomeMap` 2048³ resolution with biome pointers per 128×128×128 region. Vertical axis packed into
  64-bit integers for fast bitwise operations.

## Multi-layer chunk rendering (Ascendant pattern)

- [Vulkan Guide — Ascendant Geometry](https://www.vkguide.dev/docs/ascendant/ascendant_geometry/) —
  8×8×8 chunk size with **multiple "layers" per chunk** (main draw + transparent + clutter/vegetation)
  rendered at different passes. Mesh chunks use center+extent instead of fixed size, 3 layers. Vulkan Guide,
  2024-2025 era.

## Generation pipeline overview

- [Minecraft World Generation Overview (Telepathic Grunt / XI64 Gist)](https://gist.github.com/XI64/0480256ed2836e4d301210899551c659)
  — Published 2021-02-22. Biome Provider → Chunk Generator → SurfaceBuilder → Carvers (caves) →
  Features/Structures. Generation Stages pipeline: RAW_GENERATION → LAKES → LOCAL_MODIFICATIONS →
  UNDERGROUND_STRUCTURES → SURFACE_STRUCTURES → STRONGHOLDS → UNDERGROUND_ORES → UNDERGROUND_DECORATION
  → VEGETAL_DECORATION → TOP_LAYER_MODIFICATION.

## Production-grade chunk pipeline reference

- [maguirekrist/voxel_enginevk — chunk_refactor.md](https://github.com/maguirekrist/voxel_enginevk/blob/main/chunk_refactor.md)
  — Chunk pipeline 5 layers: Residency → Data → Meshing → Upload/Render → Scheduler. Canonical neighborhood
  sampling, async meshing with stale-result discard. Production-grade Vulkan voxel engine reference design.

## ProjectV cross-references (not web sources but mainline code refs)

- `src/voxel/VoxelWorld.hpp:85` — `chunkSize = 8` (8³ = 512 voxels/chunk, 40-byte chunk header).
- `src/voxel/VoxelWorld.hpp:20-26` — `VoxelMaterial` enum (5 values).
- `src/voxel/Sparse64Tree` — SVO storage per `2026-06-20-sparse-64-tree-alternatives` (yes) +
  `2026-06-20-nanovdb-on-gpu` (yes, hybrid SVDAG + NanoVDB).
- `src/voxel/VoxelWorld.hpp:105` — `pendingChunkRebuildIndices` (per-chunk dirty bit, wired mainline
  Phase 9 2x part 5 per `agent/workspace.md`).
- `TODO.md §4.1` (GPU Noise & World Gen) + `§4.2` (LOD) + `§5.1` (VCT).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% integration threshold.
- `agent/knowledge.md` — 3-step migration precedent.
