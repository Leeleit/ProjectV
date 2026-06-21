# Sources — chunk-storage-compression-axis

Verified web-research sources per `AGENTS.md §2` prior art + sources.md format.

Web-research via `webfetch` DuckDuckGo HTML endpoint + direct source URL fetch
(Exa MCP HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424).

## Tier 1: Primary (13 sources verified)

### Voxel/Minecraft production references

1. **Aras Pranckevičius (zeux) 2017 "Voxel terrain: storage"** — canonical voxel chunk
   compression reference.
   - URL: `https://zeux.io/2017/03/27/voxel-terrain-storage/`
   - Key finding: RLE-only disk = 73 MB / 0.07 bytes-per-voxel; RLE+LZ4 = 50 MB / 0.05; RLE+zstd = 38 MB / 0.04.
   - "Running RLE before LZ may seem counter-intuitive, but it significantly reduces the size of data making LZ faster, and in some cases means you don't even need to do LZ compression because RLE on its own is enough — voxel data is frequently very regular."
   - 2-byte `(counter, value)` run format → 256 bytes for single-material chunk vs 64 KB raw = **256× compression** at extreme.

2. **Minecraft Wiki — Region file format** — Anvil since 1.2.1 (12w07a).
   - URL: `https://minecraft.fandom.com/wiki/Region_file_format`
   - 32×32 chunks per region file, 4 KiB sector alignment, compression scheme 2 (zlib) standard, scheme 1 (gzip) unused in practice, scheme 3 (uncompressed) optional.
   - 1.20.5 (24w04a) added LZ4 option.

3. **Minecraft Wiki — Anvil file format** — chunk storage details.
   - URL: `https://minecraft.wiki/w/Anvil_file_format`
   - Block ordering XZY → YZX for better compression; sections = 16×16×16 per chunk.

4. **wiki.vg Chunk Format** — protocol-level palette bits-per-block.
   - URL: `https://wiki.vg/Chunk_Format`
   - Palette bits per block: 4 bits for 1-16 unique states (per section), 5-8 bits for indirect palette, 9+ bits = registry-based.

5. **Minecraft 1.12 BlockStatePaletteLinear.java** — linear palette + resize handler.
   - Local: `docs/experiments/sources/minecraft-master (1.12)/src/minecraft/net/minecraft/world/chunk/BlockStatePaletteLinear.java`
   - `states[1 << bitsIn]` array, resize callback on overflow.

6. **Minecraft 1.12 BlockStatePaletteHashMap.java** — hash map palette > 4 bits.
   - Local: `docs/experiments/sources/minecraft-master (1.12)/src/minecraft/net/minecraft/world/chunk/BlockStatePaletteHashMap.java`
   - `IntIdentityHashBiMap<IBlockState>` for O(1) lookup.

7. **Minecraft 1.12 BlockStateContainer.java** — adaptive bits thresholding.
   - Local: `docs/experiments/sources/minecraft-master (1.12)/src/minecraft/net/minecraft/world/chunk/BlockStateContainer.java:50-53`
   - `setBits`: ≤4 → linear palette, 5-8 → hashmap palette, >8 → registry-based.

8. **VoxelCore `compressed_chunks.cpp`** — production RLE + gzip pattern.
   - Local: `docs/experiments/sources/voxelcore-main/src/voxels/compressed_chunks.cpp`
   - `extrle::encode16` (16-bit run tuples) + `gzip::compress` + metadata block.

9. **VoxelCore `rle.cpp`** — RLE codec implementation.
   - Local: `docs/experiments/sources/voxelcore-main/src/coders/rle.cpp:62-80`
   - 16-bit extrle format `(counter, value)` with max_sequence16 = 0x3FFF.

10. **VoxelCore `Chunk.cpp::encode`** — chunk payload encoding.
    - Local: `docs/experiments/sources/voxelcore-main/src/voxels/Chunk.cpp:72-80`
    - Format: `uint16_t voxel_id[CHUNK_VOL] + uint16_t voxel_states[CHUNK_VOL]` = 4 bytes/voxel = 131072 bytes for 32³ chunk.

### Modern compression codec references

11. **Epic Games Lore ADR-00016 "Switch default compression to Zstd level 6"** (2024).
    - URL: `https://epicgames.github.io/lore/developing/decisions/00016-compression-algorithm-selection/`
    - Benchmark Silesia corpus (202 MiB, 3 iter avg):
      - LZ4 default = 47.6% ratio at 719/2495 MiB/s
      - LZ4 HC 9 = 36.7% ratio at 49/2494 MiB/s
      - **Zstd level 6 = 28.9% ratio at 136 MiB/s compress, 1285 MiB/s decompress** (CHOSEN)
      - Oodle Kraken 6 = 23.9% ratio at 2.2/1312 MiB/s (proprietary)
    - "42% faster to compress at a comparable ratio tier (136 vs 96 MiB/s), Open source (BSD license), ~1285 MiB/s decompression is fast enough for any storage back end."

12. **PH3 Blog "Game Asset Storage, Loading, Compression and Caching" (Peter Thoman, "Durante", 2023)**.
    - URL: `https://ph3at.github.io/posts/Asset-Compression/`
    - **LZ4 default 589.3 MB/s decompress @ 22.8 MB compressed; ZSTD 351.5 MB/s @ 16.3 MB; ZSTD with dictionary 610.3 MB/s @ 5.7 MB (best of both!).**

13. **Veloren `world/examples/chunk_compression_benchmarks.rs`** — production Rust benchmark.
    - URL: `https://github.com/veloren/veloren/blob/02f01903/world/examples/chunk_compression_benchmarks.rs`
    - Compares `lz4_chonk / rle_chonk / deflate0_chonk / deflate1_chonk / palette_k_ree` for actual voxel chunks. Direct validation of strategy-axis shape.

## Tier 2: Supplementary (6 sources verified)

14. **Steam zstd migration (r/pcgaming 2025-02-17)**.
    - URL: `https://www.reddit.com/r/pcgaming/comments/1irq2wd/`
    - Valve migrating from LZMA to zstd for game chunks (1 MB chunk size for delta updates).

15. **Oddur Magnusson "Zstandard Across the Stack" (2026-04-02)**.
    - URL: `https://oddur.me/posts/zstandard-across-the-stack/`
    - Custom zstd dictionaries для small messages: 70-90% bandwidth reduction. Zstd level 19 = 13% better than zip.

16. **Voxel.Wiki "Palette Compression"**.
    - URL: `https://voxel.wiki/wiki/palette-compression/`
    - Bit-buffers (1-bit per voxel in best case), tagged pointers, O(1) amortized read-write via power-of-two resize.

17. **eisenwave voxel-compression-docs**.
    - URL: `https://eisenwave.github.io/voxel-compression-docs/rle/rle.html`
    - RLE in-band signaling patterns + adaptive RLE + Qubicle Binary Tree (QBT) zlib reference.

18. **Reddit r/VoxelGameDev "Palette-based compression for chunked discrete voxel data" (2018-11-20)**.
    - URL: `https://www.reddit.com/r/VoxelGameDev/comments/9yu8qy/`
    - `BlockStorage` data structure: palette + variable-bit-length index buffer + reference counter.
    - "single block ideally takes up one single bit of memory. The common case is three to four bits."

19. **Minecraft 1.13+ `PalettedContainer` (Fabric yarn 23w06a+build.14)**.
    - URL: `https://maven.fabricmc.net/docs/yarn-23w06a+build.14/net/minecraft/world/chunk/PalettedContainer.html`
    - Modern Fabric reference: `PaletteProvider` selects palette type based on bits-per-entry; `PaletteResizeListener::onResize` callback для automatic grow.

## Source mapping by strategy

| Strategy | Source references |
|:---------|:------------------|
| **A_Uncompressed** | Minecraft Wiki Region file format scheme 3 (uncompressed optional) |
| **B_RLE16** | VoxelCore `compressed_chunks.cpp:17-22` (`extrle::encode16` + gzip), zeux.io 2017 RLE canonical |
| **C_Palette4** | Minecraft 1.12 `BlockStatePaletteLinear.java` + `BlockStateContainer.java:50-53` (adaptive ≤4 bits), Voxel.Wiki palette compression |
| **D_Palette4_RLE** | Hybrid (palette + RLE on index stream), Veloren `rle_chonk` analog |
| **E_Palette8_Zstd** | Epic Games ADR-00016 Zstd level 6 (chosen), PH3 Blog zstd+dict best of both, Minecraft 1.20.5 LZ4 option, Steam zstd migration |

## Local source files (ProjectV references)

- `src/voxel/ChunkStreamer.cpp:57-59` — current mainline 16-byte file header (`kChunkFileHeaderMagic = 0x504B5631u` + version 1 + uint64 byte count).
- `src/voxel/ChunkStreamer.cpp:76-120` — `ReadChunkBinaryFile` worker function.
- `src/voxel/VoxelWorld.hpp:78` — `chunkSize = 8` (used in this prototype).
- `src/voxel/VoxelWorld.hpp:86` — `int chunkSize = 8;` config default.
- `src/voxel/Sparse64Tree.hpp:8` — `kSparse64MaterialMask = 0xFFu` (8-bit material IDs).
- `src/voxel/Sparse64Tree.hpp:18-20` — `MakeSparse64Leaf` (uint32 slot encoding).
- `agent/workspace.md §1 Phase 3` — Stage 4.3 Chunk Streaming Step 2 closed `2026-06-21`.
- `agent/workspace.md §2 line 44-45` — Nearest Gap "Stage 4.3 Chunk Streaming Step 3 — prebake all chunks at world init + on-demand paging".

## Cross-refs to other experiments

- `2026-06-21-texture-compression-format-axis` [mixed] — **orth axis** (BC/ASTC for material atlas textures, not voxel chunks).
- `2026-06-21-sub-chunk-layers` [mixed] — **orth axis** (runtime RAM palette/layer design).
- `2026-06-21-voxel-chunk-streaming-pipeline` [mixed] — **directly upstream** (streaming policy, this is file format for same Step 3).
- `2026-06-20-svdag-vs-vdb-memory-throughput` [yes] — voxel storage topology in CPU RAM.
- `2026-06-20-nanovdb-on-gpu` [yes] — GPU upload path.
