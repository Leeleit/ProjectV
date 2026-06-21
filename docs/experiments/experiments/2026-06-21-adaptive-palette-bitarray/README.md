# 2026-06-21-adaptive-palette-bitarray — Adaptive palette BitArray for voxel chunk sections

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 4.x (chunk storage), independent
**Estimated effort:** M
**Author:** self (derived from Minecraft 1.12 source analysis)

---

## 1. Hypothesis

Minecraft 1.12's `BlockStateContainer` uses an adaptive BitArray palette that auto-resizes from 4 bits (≤16 unique blocks) up to 13+ bits (global registry) as chunk sections become more diverse. This is different from our closed `2026-06-21-chunk-storage-compression-axis` (which tested fixed palette sizes + RLE + Zstd for **file format** compression). The adaptive palette operates at **runtime RAM** level within a chunk section, providing memory savings on top of file compression.

**Hypothesis:** An adaptive bit-width palette per 16³ sub-chunk section (4→5→6→...→global bits) reduces RAM per section by 50-90% for homogeneous sections (≤16 types) while keeping lookup O(1) via direct bit extraction. This complements the file-format compression from `chunk-storage-compression-axis` (which targets disk I/O) — adaptive palette targets runtime RAM.

**Alternatives:** Fixed-width 16-bit per voxel (current mainline `uint16_t voxel_id`), fixed palette per chunk (not per section), NanoVDB sparse storage.

---

## 2. Prior art

Key sources from Minecraft 1.12 source analysis:

- **Minecraft 1.12 `BlockStateContainer.java`** — adaptive palette: linear scan (≤4 bits), hash map (5-8 bits), global registry (>8 bits). Auto-resize via `onResize()` callback.
- **Minecraft 1.13+ `PalettedContainer`** — evolution of same concept, now with `ID paletted container` for biome data (4×4×4 sections).
- **VoxelCore** — uses direct `uint16_t` block ID, no palette (simpler but less memory-efficient).
- **Minecraft Wiki Anvil format** — section-level palette with variable bits per entry.
- **closed `2026-06-21-chunk-storage-compression-axis`** — file-format compression (Palette4/Palette8 + RLE/Zstd); this experiment is orthogonal (runtime RAM, not disk).

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 representative voxel scenes (uniform_air, uniform_floor, forest_floor, cave_stress, mixed_biome per `sub-chunk-layers` precedent)
- **Metrics:** bytes per 16³ section, lookup time (ns/voxel), resize cost (ns), mutation cost (ns/voxel)
- **Baseline:** current mainline `uint16_t` (32 bytes per 16³ = 32 KiB per section)
- **Strategies:**
  - A_Fixed16: current mainline (uint16_t per voxel)
  - B_AdaptivePalette: Minecraft 1.12 style adaptive BitArray (4→13 bits)
  - C_Palette4Fixed: fixed 4-bit palette (best for ≤16 types, 16-byte palette + 8 KiB data)
  - D_PaletteMap: hash-map based palette (O(1) lookup, higher per-entry overhead)

---

## 4. Prototype

Standalone C++26 CPU harness measuring memory footprint and access patterns across voxel scenes.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/adaptive_palette_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 4.x (chunk storage optimization)
- **Конкретные изменения:** `src/voxel/VoxelWorld.hpp` — new `SectionStorage` variant with adaptive palette alongside existing flat array.
- **Подход:** per-section palette selection based on unique block count; auto-resize from 4 bits upward; fallback to flat 16-bit for sections >256 types.
- **Риски:** cache miss increase from indirection; resize cost during world gen; mutation cost for gameplay edits.
- **Критерии приёмки:** >50% RAM reduction on forest_floor/cave_stress scenes (≥32 KiB → ≤16 KiB per section) with <10% lookup overhead.
- **Зависимости:** none (orthogonal to file compression axis).
- **Estimated effort:** M (~400 LoC, 2-3 sessions)

---

## 8. Sources

- Minecraft 1.12 `BlockStateContainer.java` (local source: `sources/minecraft-master (1.12)/src/minecraft/net/minecraft/world/chunk/BlockStateContainer.java`)
- Minecraft 1.12 `ExtendedBlockStorage.java` (local source: same dir)
- closed `2026-06-21-chunk-storage-compression-axis` (file-format orthogonal)
- closed `2026-06-21-sub-chunk-layers` (layout orthogonal)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/voxel/VoxelWorld.hpp:78-107` — VoxelChunk storage, currently flat `uint16_t` arrays.
- **Assumptions:** chunkSize=8 means 512 voxels per chunk (not 4096 like MC sections); palette benefit scales with homogeneity.
- **Unmeasured:** driver memory allocation overhead, Vulkan buffer binding cost, GPU texture buffer indirection.
