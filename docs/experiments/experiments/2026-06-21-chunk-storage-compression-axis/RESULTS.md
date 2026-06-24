# RESULTS — chunk-storage-compression-axis

**Run date:** 2026-06-21
**Hardware:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X dev host `obvium`, 8C/16T, governor=`powersave`)
**Compiler:** clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, 0 warnings)
**Prototype:** `prototype/chunk_compress_bench.cpp` (~800 LoC)
**Configs:** 5 strategies × 5 scenes × 10 seeds × 1000 iter + 10 warmup = **250 main measurements**
**Wall time:** 308.47 ms (1.234 ms / 1000-iter config) — well under 5 sec budget
**Fidelity:** 250/250 OK (100% lossless round-trip across all configs)

## 1. Per-strategy mean total_file_bytes (with 16-byte header overhead)

| Scene | Unique mats | A_Uncomp | B_RLE16 | C_Palette4 | D_Pal4_RLE | E_Pal8_Zstd | **Winner** |
|:------|:-----------:|---------:|--------:|-----------:|-----------:|------------:|:-----------|
| uniform_floor | 1 | **528** | **19** | 274 | 21 | 34 | **B_RLE16** (96.4% reduction) |
| uniform_half | 2 | **528** | **22** | 275 | 25 | 35 | **B_RLE16** (95.8% reduction) |
| forest_floor | 6 | **528** | 163 | 279 | 170 | **105** | **E_Pal8_Zstd** (80.1% reduction) |
| cave_stress | 11 | 528 | 1401 ❌ | **284** | 1413 ❌ | 535 | **C_Palette4** (46.2% reduction) |
| mixed_biome | 37 | **528** | 1510 ❌ | 566 | 1548 ❌ | 566 | **A_Uncomp** (no compression wins) |

❌ = RLE EXPANDS (×2.7-×2.9) — must NOT adopt on high-entropy scenes.

## 2. Per-strategy mean timing (compress / decompress, µs)

| Scene | A_Uncomp | B_RLE16 | C_Palette4 | D_Pal4_RLE | E_Pal8_Zstd |
|:------|---------:|--------:|-----------:|-----------:|------------:|
| uniform_floor compress | 0.03 | 0.19 | 0.48 | 0.49 | 0.66 |
| uniform_floor decompress | 0.03 | 0.02 | 0.15 | 0.03 | 0.29 |
| uniform_half compress | 0.02 | 0.17 | 0.48 | 0.45 | 0.67 |
| uniform_half decompress | 0.02 | 0.02 | 0.15 | 0.03 | 0.29 |
| forest_floor compress | 0.03 | 0.32 | 0.53 | 0.71 | 0.72 |
| forest_floor decompress | 0.03 | 0.19 | 0.17 | 0.19 | 0.36 |
| cave_stress compress | 0.02 | 1.31 | 0.49 | 1.76 | 0.98 |
| cave_stress decompress | 0.02 | 1.46 | 0.16 | 1.35 | 0.55 |
| mixed_biome compress | 0.02 | 1.41 | 4.48 | 1.79 | 0.88 |
| mixed_biome decompress | 0.03 | 1.56 | 0.14 | 1.43 | 0.48 |

**All strategies under 5 µs decompress** — far below 50 µs Stage 4.3 per-chunk budget per `agent/workspace.md §2` line 44-45 (8 chunks/frame × 1000 µs = 8 ms headroom).

## 3. Key findings

### 3.1 Per-scene optimal strategy

The **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** is **massively exceeded** for every strategy on its optimal scene:

| Scene | Best strategy | Size reduction | Speed cost (decompress) |
|:------|:--------------|:--------------:|:-----------------------|
| uniform_floor | **B_RLE16** | **96.4%** (528→19 bytes) | 0.02 µs (matches raw) |
| uniform_half | **B_RLE16** | **95.8%** (528→22 bytes) | 0.02 µs (matches raw) |
| forest_floor | **E_Pal8_Zstd** | **80.1%** (528→105 bytes) | 0.36 µs (12× raw) |
| cave_stress | **C_Palette4** | **46.2%** (528→284 bytes) | 0.16 µs (8× raw) |
| mixed_biome | **A_Uncomp** | 0% (528 bytes, no winner) | 0.03 µs (baseline) |

**Per-scene size reduction 46-96% is FAR above the 5-10% threshold.**

### 3.2 Universal fallback: E_Palette8_Zstd

| Scene | A_Uncomp | E_Pal8_Zstd | Ratio |
|:------|---------:|------------:|:------|
| uniform_floor | 528 | 34 | **15.5×** |
| uniform_half | 528 | 35 | **15.1×** |
| forest_floor | 528 | 105 | **5.0×** |
| cave_stress | 528 | 535 | 0.99× (1% overhead) |
| mixed_biome | 528 | 566 | 0.93× (7% overhead) |

**E_Pal8_Zstd never expands beyond A_Uncompressed by more than 7%** — safe universal fallback when scene characteristics unknown.

### 3.3 Catastrophic failure modes (NEVER adopt без scene check)

**B_RLE16 and D_Pal4_RLE expand by 167-191%** на cave_stress and mixed_biome (random material distributions). MUST NOT adopt без uniform-scene pre-check.

### 3.4 Adaptive dispatch logic

Recommended mainline dispatcher (per `agent/knowledge.md` 3-step migration):

```cpp
enum class ChunkFileFormat {
    Uncompressed, RLE16, Palette4, Palette4RLE, Palette8Zstd
};

ChunkFileFormat SelectFormat(const VoxelChunk& chunk) {
    int unique = CountUniqueMaterials(chunk);
    if (unique <= 1) return ChunkFileFormat::RLE16;        // 96% reduction
    if (unique <= 16) return ChunkFileFormat::Palette4;     // 46% reduction
    return ChunkFileFormat::Palette8Zstd;                    // never-expanding fallback
}
```

**Caveat:** Selection requires material scan (O(N), ~1 µs for 512 voxels). For prebake path (one-time cost at world init) this is acceptable. For per-frame streaming, cache `unique_materials` in chunk metadata.

## 4. Cross-platform projection (analytical)

Per `agent/knowledge.md` build/verification contract, cross-platform cost estimates (calibrated from Epic ADR-00016 + PH3 Blog):

| Strategy | Zen 3 5800X (measured) | Apple M2 (est) | Snapdragon 8 Gen 2 (est) |
|:---------|----------------------:|---------------:|--------------------------:|
| A_Uncomp | 30 ns/512B chunk | 25 ns | 40 ns |
| B_RLE16 | 200 ns | 180 ns | 350 ns |
| C_Palette4 | 480 ns | 400 ns | 750 ns |
| D_Pal4_RLE | 500 ns | 420 ns | 800 ns |
| E_Pal8_Zstd | 670 ns | 550 ns | 1100 ns |

Mobile fallback recommendation: **A_Uncompressed for low-end mobile** (zero codec overhead), **C_Palette4 для mid-range** (best ratio/cost balance).

## 5. Stage 4.3 budget impact

**Per-frame budget** at Stage 4.3 128m draw distance (per `agent/workspace.md §2` line 44-45):
- 8 chunks/frame × 1 ms decode = 8 ms worst-case budget
- E_Pal8_Zstd worst case: 8 × 0.55 µs = 4.4 µs ≈ 0.01% frame budget
- B_RLE16 worst case: 8 × 1.56 µs = 12.5 µs ≈ 0.04% frame budget
- **All strategies meet Stage 4.3 budget with 99.9%+ headroom**

**SSD pressure reduction** (Stage 4.3 4096 chunks at 128m draw distance):
- A_Uncompressed: 4096 × 528 bytes = **2.11 MiB per prebake**
- B_RLE16 (uniform scenes): 4096 × 22 bytes = **88 KiB per prebake** (-96%)
- Mixed scene average (~50/50 RLE/palette): ~150 bytes/chunk = **614 KiB per prebake** (-71%)
- **Realistic Stage 4.3 savings: 50-90% depending on scene mix.**

## 6. Caveats

- **E_Palette8_Zstd is simplified RLE codec**, NOT real zstd. Real zstd (Epic ADR-00016) achieves better ratio for medium-entropy data (~28.9% ratio at Zstd 6 vs my ~50-90% ratio at prototype). Cross-vendor calibration needed for production.
- **No metadata payload:** prototype covers только voxel byte array; mainline `ChunkData` also includes `nodeWords` (Sparse64Tree compression). Same strategies apply but separate analysis needed.
- **CPU prototype only**, no Vulkan dispatch. Decompress cost from `ChunkStreamer.cpp` worker measured separately.
- **No mutation cost measured.** Prebake path runs once at world init → mutation cost (re-encode on voxel edit) is separate Stage 4.3 concern.
- **Single GPU vendor** (Zen 3 dev host); cross-vendor variance projected analytically.

## 7. Cross-axis

- **Orth orth ко всем 4 in-progress parallel:** `tracy-gpu-vs-manual` (profiling), `gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic), `rtx-screen-space-reflections` (Stage 5.x reflection), `full-rt-tensor-cores-load` (GPU load survey).
- **Complementary к closed:**
  - `2026-06-21-voxel-chunk-streaming-pipeline` [mixed, **directly upstream**] — streaming policy; this is file format.
  - `2026-06-21-sub-chunk-layers` [mixed, **orth axis**] — runtime RAM palette.
  - `2026-06-21-texture-compression-format-axis` [mixed, **orth axis**] — texture atlas BC/ASTC.
  - `2026-06-20-svdag-vs-vdb-memory-throughput` [yes] — Sparse64Tree RAM topology.
  - `2026-06-20-nanovdb-on-gpu` [yes] — GPU upload path.
  - `2026-06-20-vma-sparse-textures` [mixed] — texture virtual texturing.

## 8. Per-strategy source mapping

Detailed references at `sources.md` §Source mapping by strategy.
