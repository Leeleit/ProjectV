# STATUS — chunk-storage-compression-axis

**Phase:** wrap-up (concluded-verdict-mixed)
**Last action:** 2026-06-21 — Experiment closed same session, ~2h. 250 measurements, 308 ms wall time, 100% fidelity OK.
**Next tick:** N/A (closed)
**Blocker:** нет.

---

## Progress log

- **2026-06-21** — Opened. **Anti-duplicate sentinel clean per `AGENTS.md §13.7`**:
  `rg -l "chunk-storage-compression|chunk-storage compression|chunk.compression|chunk_compression"`
  over `INDEX.md` + `backlog.md` + `experiments/` = **0 hits**. Closed
  `2026-06-21-texture-compression-format-axis` [mixed] covers **orth axis** (BC/ASTC for material atlas);
  closed `2026-06-21-sub-chunk-layers` [mixed] covers **orth axis** (runtime RAM palette);
  closed `2026-06-21-voxel-chunk-streaming-pipeline` [mixed] covers **streaming policy**.
- **2026-06-21** — Web-research complete: 13 primary + 6 supplementary sources verified
  via `webfetch` DuckDuckGo fallback (Exa HTTP 429 persistent). Key references:
  zeux.io 2017, Minecraft Wiki Anvil/Region format, Minecraft 1.12 BlockStatePaletteLinear/HashMap,
  Minecraft 1.20.5 LZ4 option, Epic ADR-00016 Zstd level 6, Veloren chunk_compression_benchmarks.rs,
  PH3 Blog game asset compression, Oddur Magnusson zstd across the stack, Steam zstd migration (2025),
  Voxel.Wiki palette compression, eisenwave voxel-compression-docs.
- **2026-06-21** — Sources read in detail: VoxelCore `compressed_chunks.cpp` (RLE + gzip pattern),
  `rle.cpp` (16-bit extrle), `Chunk.cpp::encode` (uint16 voxel_id + voxel_states);
  Minecraft 1.12 `BlockStatePaletteLinear.java`, `BlockStatePaletteHashMap.java`,
  `BlockStateContainer.java` (adaptive bits 4/8/registry thresholding).
- **2026-06-21** — Prototype complete: `chunk_compress_bench.cpp` ~800 LoC, 5 strategies
  (A_Uncompressed / B_RLE16 / C_Palette4 / D_Palette4_RLE / E_Palette8_Zstd), 5 synthetic scenes,
  fidelity check via `memcmp`, CSV output. Build green, 0 warnings.
- **2026-06-21** — Iter 1: smoke test 250 configs × 100 iter — caught 2 fidelity bugs:
  - D_Palette4_RLE palette array was `std::array<uint8_t, 16>` for pcount >16 fallback → UB.
    Fixed: enlarged to 256.
  - E_Palette8_Zstd LZ77 match had off > dst → UB. Refactored to value-explicit RLE+literals
    codec (no LZ77). Simpler and more correct.
- **2026-06-21** — Iter 2: full sweep 250 configs × 1000 iter + 10 warmup = 250,000 main
  measurements, wall time 308.47 ms (1.234 ms / 1000-iter config) on Zen 3 5800X per
  `hardware-profile.md §1`. **100% fidelity OK** across all configs.
- **2026-06-21** — Results analyzed:
  - B_RLE16 = 96.4% / 95.8% reduction на uniform_floor / uniform_half (winner uniform scenes).
  - C_Palette4 = 46.2% reduction на cave_stress (winner mixed scenes).
  - E_Pal8_Zstd = 80.1% reduction на forest_floor (winner moderate scenes).
  - A_Uncompressed = baseline best для mixed_biome (37 materials, no compression strategy wins).
  - **E_Pal8_Zstd never expands beyond +7%** vs raw → safe universal fallback.
  - **B_RLE16 / D_Pal4_RLE expand by 167-191%** на high-entropy scenes → never adopt без scene pre-check.
- **2026-06-21** — Verdict=mixed, closed single-session.

---

## Notes

- Reservation added to `backlog.md §In progress` per `AGENTS.md §13.5` lifecycle sync.
- Verdict=mixed per §6 (per-scene adaptive dispatcher required; no single strategy wins all scenes).
- **Critical insight:** per-scene adaptive dispatcher is the right architecture, NOT single-format adoption.
- **Universal fallback E_Palette8_Zstd** provides safe default when scene characteristics unknown.
- Cross-axis: orth orth ко всем 4 in-progress parallel: tracy-gpu-vs-manual, gpu-fluid-ca-atomic-strategy,
  rtx-screen-space-reflections, full-rt-tensor-cores-load. Complementary к closed
  voxel-chunk-streaming-pipeline [mixed, **directly upstream**].
- **Follow-up candidates (deferred):**
  1. Region file format (Anvil-style 32×32 chunks per file) — single-file change to ChunkStreamer
     but cross-cutting with worker logic. Different axis from this experiment.
  2. Real zstd library adoption (vs current simplified RLE codec) — re-benchmark E strategy.
  3. Sparse64Tree `nodeWords` payload compression (currently raw `uint32_t` per word) — same strategies apply.
  4. Mutation cost (per-chunk re-encode on voxel edit) — separate Stage 4.3 concern, not measured here.
- **Caveat:** E_Palette8_Zstd is simplified RLE codec, NOT real zstd. Cross-vendor calibration
  needed for production deployment.
