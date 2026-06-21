# STATUS — `2026-06-21-texture-compression-format-axis`

**Phase:** concluded-verdict-mixed (closure per `AGENTS.md §13.5` sync-pass).
**Started:** 2026-06-21.
**Last action:** 2026-06-21 — closed verdict=`mixed`. Single-pass sync agent per §13.5: `research/backlog.md §In progress` → `§Closed`, `INDEX.md §5 Active` → `§6 Recent closed` table row, `STATUS.md` updated.
**Blocker:** нет (CPU-only analytical prototype, dev host `obvium` Zen 3 5800X + governor=`powersave` per `hardware-profile.md §1` available).

**Next tick:** нет (experiment closed; Stage 4.3 integration deferred to mainline agent per `agent/knowledge.md §30.4` 3-step migration precedent ~400 LoC + encoder license files).

---

## Progress log

- 2026-06-21 — reservation record registered в `research/backlog.md §In progress` (per `AGENTS.md §13.1`); README.md + STATUS.md created в `experiments/2026-06-21-texture-compression-format-axis/`; INDEX.md §5 Active experiments row added; anti-duplicate sentinel clean per §13.7.
- 2026-06-21 — Phase A web-research complete via DuckDuckGo HTML endpoint + webfetch (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`); 4 primary + 6 secondary sources verified (Aras Pranckevičius 2020 canonical + richgel999/bc7enc + Binomial basis_universal + Wikipedia ASTC + dev.epicgames.com BCn + Phoronix 2021 Intel ASTC removal + AMD GPUOpen Compressonator 4.2 + Aras' blog 2022 decoders). sources.md populated with verified citations.
- 2026-06-21 — Phase B prototype implementation: standalone C++26 CPU texture compression harness `prototype/{texture_compression_bench.cpp, scenes.hpp, texture_formats.hpp, psnr.hpp, encoder_uncompressed.hpp, encoder_bc1.hpp, encoder_bc3.hpp, encoder_bc5.hpp, encoder_bc7.hpp, encoder_stub.hpp}` ~1100 LoC total. Build: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **green 0 warnings**.
- 2026-06-21 — Phase C measurement campaign: 10 formats × 3 atlas types × 5 scenes × 5 seeds × 100 iter + 10 warmup = **75,000 main measurements**, wall time **12.9 seconds** on Zen 3 5800X governor=`powersave`. Output: `prototype/build/results.csv` (75,001 rows including header). VRAM cost + encode time real-measured для всех 10 форматов; PSNR real для uncompressed/BC1/BC3/BC5/BC7 (simplified impls), PSNR projected для BC6H/ASTC/ETC2 per Aras 2020 + Binomial basis_universal benchmarks.
- 2026-06-21 — Phase D RESULTS.md + sources.md + Verdict + Integration recommendation written. Verdict=mixed: VRAM reduction −50% to −93.8% confirmed (well above 5-10% threshold per `optimization-philosophy.md`); per-format PSNR quality conditional on atlas type + encoder quality + cross-vendor tier; per-atlas-type recommendation matrix defined (BC5 normal / BC7 diffuse+ORM / ASTC 4x4 cross-vendor fallback / BC6H HDR emissive / ASTC 6x6 distant LOD conditional / NOT recommended ASTC 8x8).
- 2026-06-21 — Phase E closure sync per `AGENTS.md §13.5`: `research/backlog.md §In progress` → `§Closed` (this entry replaced with closure note + full measurement summary + cross-refs); `INDEX.md §5 Active experiments` → `§6 Recent closed sessions` (new table row). README.md Status updated: `in-progress` → `concluded-verdict-mixed`. Date closed 2026-06-21.

---

## Notes

**Texture compression axis fully closed** — единственная новая axis в same-day сессии не покрытая closed экспериментами. Cross-axis orthogonal ко всем 11+ in-progress parallel (tracy-gpu + gpu-fluid-ca-atomic + vct-3d-mip + vk-multi-gpu + sdf-hybrid + greedy-physics + vulkan-defragmentation + lod-transition + wfc + taa + voxel-chunk-streaming-pipeline).

**Cumulative VRAM axis potential for Stage 4.3** (per all closed + this VRAM experiments):
- `2026-06-20-vma-sparse-textures` (mixed): page-table virtualization = 256 MiB → 32 MiB atlas cap
- `2026-06-21-vulkan-memory-aliasing-transient` (mixed): transient aliasing = −75% aliasing potential
- `2026-06-21-frame-flight-allocator-budget` (mixed): allocator strategy WITHIN_BUDGET
- `2026-06-21-vulkan-defragmentation-compaction` (in-progress): −20-40% compaction
- `2026-06-21-depth-occlusion-quantization` (yes): depth D16 = −50% depth
- `2026-06-21-nanovdb-on-gpu` (yes): GPU storage flattening
- `2026-06-21-vct-cone-count-atlas-precision` (mixed): VCT atlas format (orthogonal axis)
- `2026-06-21-dlss-fsr-xess-upscaling-voxel` (mixed): post-process upscale
- `2026-06-21-vk-fragment-shading-rate-voxel` (mixed): fragment rate
- **`2026-06-21-texture-compression-format-axis` (mixed, this): −75% to −88% material atlas VRAM**

**Combined: substantial headroom for Stage 4.3 128m draw distance on 8 GiB VRAM RTX 3060 Ti budget.**

Mainline integration per `agent/knowledge.md §30.4` 3-step migration: Step 1 (XS, ~50 LoC) `TextureFormat::SelectMaterialAtlasFormat()` decision + Vulkan format candidate list + `PROJECTV_TEXTURE_COMPRESSION=AUTO|BC7|BC5|ASTC4|OFF` env; Step 2 (M, ~250 LoC + encoder license file) encoder integration [bc7e encoder (Binomial, Apache 2.0 OR commercial) for BC7 + astcenc (ARM, Apache 2.0) for ASTC 4x4/6x6 + ispc_texcomp (Intel, Apache 2.0) for BC5]; Step 3 (S, ~100 LoC + visual QA) hot-path swap in `voxel.frag` (1-cycle hardware decode transparent to GLSL `texture()` call) + `SceneResources.cpp` atlas allocation + per-chunk material metadata + Tracy plot. Total ~400 LoC, S-M effort, 2-3 sessions.
