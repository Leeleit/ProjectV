# Prototype — chunk-storage-compression-axis benchmark

Standalone C++26 CPU prototype для 5 file format compression strategies.

## Files

- `chunk_compress_bench.cpp` (~800 LoC) — main benchmark: 5 compression strategies
  × 5 synthetic voxel scenes × measurement harness + fidelity check + CSV output.
- `CMakeLists.txt` — `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++`

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```

Requires `clang++ 22.1.6+` for full C++26 support per `hardware-profile.md §6`.

## Run

### Full sweep (5 scenes × 5 strategies × 10 seeds × 1000 iter = 250 measurements):

```bash
./build/chunk_compress_bench --all --iters 1000 --seeds 10 --output build/results.csv
```

### Smoke test:

```bash
./build/chunk_compress_bench --all --iters 100 --seeds 2 --output build/results_smoke.csv
```

## Strategies

- **A_Uncompressed** — current mainline baseline. Raw 512-byte `uint8_t` voxel payload + 16-byte file header = 528 bytes total per chunk.
- **B_RLE16** — VoxelCore `extrle::encode16` analog. 16-bit `(counter, value)` tuples; counter max 0x3FFF = 16383. Excellent для uniform scenes.
- **C_Palette4** — Minecraft 1.12 `BlockStatePaletteLinear` analog. 4-bit indices (max 16 materials); auto-fallback to 8-bit indices inline if >16 materials.
- **D_Palette4_RLE** — palette + RLE on the 4-bit index stream. Hybrid pattern.
- **E_Palette8_Zstd** — 8-bit palette (max 256 materials) + simplified RLE codec (literal + 2-byte RLE token). Real zstd would do better; this captures the same idea with simple code.

## Scenes

- **uniform_floor** — 1 material, full chunk. Best case для RLE.
- **uniform_half** — 2 materials (air + floor in 4+4 Y layers). Typical WorldGen.
- **forest_floor** — 5-7 materials stratified (soil/dirt/stone/glass/fluid patches).
- **cave_stress** — 8-15 materials with cave geometry (random walk voids).
- **mixed_biome** — 16-30 materials random (worst case).

## Outputs

- `build/results.csv` — full per-config measurements (250 rows × 11 cols).
- `build/summary_means.csv` — aggregated mean per (scene, strategy) pair.
- `build/run.log` — execution log.

## Fidelity check

Each measurement runs full round-trip (compress → decompress → memcmp). Any mismatch
= `fidelity_ok=FAIL` in CSV. All 250 configs pass with current implementation.

## Caveats

- **E_Palette8_Zstd** is a simplified RLE+literals codec, NOT a real zstd. Real zstd
  uses LZ77 with hash chains + FSE/Huffman entropy coding + dictionary support
  (~5000 LoC). This prototype captures the same algorithmic idea for 512-byte chunks
  where LZ77 sliding window is small. Compress/decompress cost calibrated against
  Epic ADR-00016 published numbers (~136 MiB/s compress, ~1285 MiB/s decompress for
  Zstd level 6 on Silesia corpus).
- **Per-material assumption:** voxel IDs fit in `uint8_t` per `src/voxel/Sparse64Tree.hpp:8`
  `kSparse64MaterialMask = 0xFFu`. Material IDs >255 are out of scope.
- **No metadata payload:** current mainline `ChunkData` includes
  `std::vector<uint32_t> nodeWords` (Sparse64Tree), but this prototype only covers the
  voxel byte array. The same strategies apply independently to `nodeWords` (also
  compressible via RLE/palette).
- **CPU prototype only**, no Vulkan dispatch. Decompress cost from real `ChunkStreamer.cpp`
  worker thread measured separately; this prototype isolates strategy-level overhead.
