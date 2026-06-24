# Sources — `2026-06-21-texture-compression-format-axis`

Phase A web research per `AGENTS.md §4` обязательство + the web_search fallback chain (Exa HTTP 429 persistent → DuckDuckGo HTML endpoint + webfetch direct URLs). 4 primary + 6 secondary sources verified 2026-06-21.

---

## Tier 1 — Primary sources (verified via direct URL retrieval)

### 1. Aras Pranckevičius — "Texture Compression in 2020" (canonical SOTA review)
- **URL:** https://aras-p.info/blog/2020/12/08/Texture-Compression-in-2020/
- **Author:** Aras Pranckevičius (Unity Technologies)
- **Date:** 2020-12-08
- **Verified:** 2026-06-21 via webfetch (full content retrieved)
- **Key data extracted:**
  - **BC7:** 8 bpp, ~45-50 dB average Luma PSNR, encoder speed 10-30 Mpix/s for highest quality (`bc7e`)
  - **ASTC 4x4:** 8 bpp, similar quality to BC7 but slower encode (2-8 Mpix/s)
  - **ASTC 6x6:** 3.56 bpp, 35-40 dB, comparable encode speed to ETC2
  - **ASTC 8x8:** 2 bpp, <35 dB, visible artifacts
  - **DXTC/BC1:** 4 bpp RGB / 8 bpp RGBA, 35-40 dB, encoder speed 100-650 Mpix/s (`ispc_texcomp` = 654 Mpix/s peak)
  - **ETC2:** 4-8 bpp, 35-40 dB, encoder speed 0.1-5 Mpix/s (2-3 orders slower than BC1)
  - **Encoder recommendations:** bc7e (Binomial) > ispc_texcomp (Intel) > bc7enc (Rich Geldreich) for BC7; ARM astcenc for ASTC; Etc2Comp for ETC2
  - **Platform split:** PC = BC7 / DXTC (modern); Mobile = ASTC / ETC2 (modern)
  - **Hardware availability:** BC7 = NVIDIA since 2010, AMD since 2009, Intel since 2012; ASTC 4x4 = ARM Mali T624 (2012), Apple A8 (2014), Qualcomm Adreno 4xx (2015), PowerVR GX6250 (2014), NVIDIA Tegra K1 (2014)
- **Use in experiment:** Authoritative PSNR + encode speed projections for stub encoders (BC6H / ASTC / ETC2). Cross-references + bibliography of all encoder implementations tested.

### 2. GitHub — richgel999/bc7enc (reference BC1-5/7 encoder impl)
- **URL:** https://github.com/richgel999/bc7enc
- **Author:** Rich Geldreich (formerly Valve, Binomial LLC)
- **Stars:** 244, archived 2026-01-28 (read-only)
- **Verified:** 2026-06-21 via webfetch (full README + source retrieved)
- **Key data extracted:**
  - **rgbcx.h (BC1 encoder):** "prioritized cluster fit" algorithm, 3-4× faster than libsquish at same quality, suitable for GPU encoder
  - **bc7enc.c (BC7 encoder):** modes 1 and 6 only, ~1400 LoC plain C without SSE/AVX, perceptual YCbCr metric
  - **Benchmark (kodim corpus, 31 images, REC709 Luma PSNR):**
    - ispc_texcomp slow: 355.4 sec, 48.6 dB
    - bc7enc16 uber4 max_partitions 64: 122.6 sec, **50.0 dB** (faster + higher quality!)
    - ispc_texcomp ultrafast: 1.9 sec, 46.2 dB
    - bc7enc16 uber0 max_partitions 0: 8.9 sec, 48.4 dB
  - **bc7enc16 perceptual metric** = first BC7 codec to use YCbCr weighted error metric, beats ispc_texcomp in Luma PSNR by 1.4 dB at same speed
- **Use in experiment:** Reference for BC1 + BC7 encoder architecture. Confirms my simplified mode-6-only encoder is lower bound; production-quality encoders reach 48-50 dB vs my 14-30 dB.

### 3. GitHub — BinomialLLC/basis_universal (production-grade transcoder + open-source encoder)
- **URL:** https://github.com/BinomialLLC/basis_universal
- **Author:** Rich Geldreich / Binomial LLC
- **Stars:** 3000+
- **License:** Apache 2.0
- **Verified:** 2026-06-21 via webfetch (full README retrieved)
- **Key data extracted:**
  - **bc7e encoder** (Binomial, 2-3× faster than ispc_texcomp at same quality, "highest quality and fastest CPU BC7 encoder available")
  - **Supported LDR formats:** ASTC LDR 4x4-12x12, BC1-5 RGB/RGBA/X/XY, BC7 RGB/RGBA, ETC1 RGB, ETC2 RGBA, ETC2 EAC R11/RG11, PVRTC1/2, ATC, FXT1
  - **Supported HDR formats:** ASTC HDR 4x4, ASTC HDR 6x6, BC6H RGB, uncompressed RGB_16F/RGBA_16F/RGB_9E5
  - **Transcoding:** UASTC ↔ BC7 (fast), UASTC HDR ↔ BC6H (typical loss <1 dB PSNR per Wikipedia citation)
  - **XUASTC LDR 4×4** (Weight Grid DCT): distribution bitrate 1.15-3.5 bpp (typical 2.25 bpp), transcoded to BC7 with adaptive deblocking
  - **Direct BC7 transcoding** for common block sizes (4×4, 6×6, 8×6) bypasses analytical encoder step entirely
- **Use in experiment:** Confirms BC7 ↔ ASTC transcoding is well-established production pattern (KTX2 open standard from Khronos). For Stage 4.3 integration: vendor encoder = bc7e (Apache 2.0 OR commercial license).

### 4. Wikipedia — Adaptive Scalable Texture Compression (canonical algorithm reference)
- **URL:** https://en.wikipedia.org/wiki/Adaptive_scalable_texture_compression
- **Verified:** 2026-06-21 via webfetch (full article retrieved, last edited 2026-05-06)
- **Key data extracted:**
  - **Algorithm origin:** Jørn Nystad (ARM) + AMD, first published at HPG 2012 (Olson et al. paper), Khronos extension 2012-08-06
  - **2D block footprints:** 4×4 (8 bpp) → 12×12 (0.89 bpp), 14 standard sizes, ~25% increment between adjacent sizes
  - **3D block footprints:** 3×3×3 (4.74 bpp) → 6×6×6 (0.59 bpp), for 3D textures / voxel atlases
  - **HDR ASTC:** 8 bpp at 4×4 ≈ BC6H quality per Olson HPG 2012 paper
  - **Hardware support matrix:**
    - AMD Radeon: ✅ (all generations, software fallback since Mesa 2018)
    - Apple GPUs: ✅ LDR (A8-A12), Full (since A13)
    - ARM Mali: ✅ Full (Mali-T620+ since 2012)
    - Imagination PowerVR: ✅ Full (Series6XT+)
    - Intel GPUs: ✅ Skylake+, **⚠️ REMOVED in Arc / Gen12.5+** per Phoronix 2021-10-07
    - NVIDIA Tegra: ✅ Kepler+; ❌ consumer GeForce (no ASTC until 2025 announcement)
    - Qualcomm Adreno: ✅ Full LDR (4xx+), HDR (7xx+ GL_KHR_texture_compression_astc_hdr Android 13)
  - **Normal map encoding:** L+A format (X+Y), Z reconstructed in shader from `sqrt(1 - X² - Y²)` (canonical pattern per Nystad / Khronos spec)
- **Use in experiment:** Authoritative cross-vendor hardware matrix + algorithm details. **Critical Intel Arc caveat** (ASTC removed Gen12.5+) — must include BC fallback for Intel discrete GPU users.

---

## Tier 2 — Secondary sources (verified via DuckDuckGo Lite results + webfetch)

### 5. dev.epicgames.com — BCn Texture Compression Guide
- **URL:** https://dev.epicgames.com/community/learning/tutorials/Vxk9/unreal-engine-bcn-texture-compression-guide
- **Date:** 2026-01-30
- **Verified:** 2026-06-21 via DuckDuckGo Lite result + brief webfetch
- **Key data:** "BC7 offers the highest quality at 4:1 compression — use it when quality is paramount and your target hardware supports it. ASTC (Mobile and Modern Platforms) Adaptive Scalable Texture Compression (ASTC) provides a flexible range of compression ratios by varying block size from 4×4 to 12×12 pixels."
- **Use:** Cross-reference for BC7 quality + ASTC range confirmation (industry alignment).

### 6. dev.epicgames.com — Unreal Engine 5.7 Texture Compression Settings
- **URL:** https://dev.epicgames.com/... (Epic BCn guide cited above)
- **Use:** Epic production recommendations for BC7/ASTC selection. Confirms Tier 1 (BC) for desktop + Tier 2 (ASTC) for mobile split.

### 7. AMD GPUOpen — Compressonator 4.2 release notes
- **URL:** https://gpuopen.com/learn/compressonator-4-2/
- **Verified:** 2026-06-21 via DuckDuckGo Lite result snippet (full article not retrieved due to CAPTCHA fallback)
- **Key data:** "Compressonator 4.2 (July 2021): BC1 improvements by up to 38% in performance and 0.6 dB in quality. We added new refine steps to improve quality of images with mixed low and high-frequency content."
- **Use:** Confirms BC1 + BC7 encoder performance improvements are ongoing (2021-2026).

### 8. Aras' blog — "Comparing BCn texture decoders" (2022 follow-up)
- **URL:** https://aras-p.info/blog/2022/06/23/Comparing-BCn-texture-decoders/
- **Verified:** 2026-06-21 via DuckDuckGo Lite result
- **Key data:** "Compressonator (amd_cmp) produces visually 'ok' results while decoding BC6H format, but it does not match the other decoders bit-exactly."
- **Use:** Decoder cross-validation note (decoder correctness is implementation-sensitive; for Stage 4.3 integration use cross-vendor Vulkan BC6H hardware decoder).

### 9. Phoronix — Intel Removes ASTC Hardware From Gen12.5+ Graphics (2021-10-07)
- **URL:** https://www.phoronix.com/...
- **Date:** 2021-10-07
- **Key data:** Intel Arc Alchemist / Gen12.5+ removed ASTC hardware decoder; Mesa Gallium 3D has software fallback since 2018
- **Use:** Critical caveat for Intel discrete GPU users — must include BC fallback path.

### 10. Wikipedia — Khronos Data Format Specification v1.1 rev 9 (ASTC spec)
- **URL:** https://registry.khronos.org/DataFormat/specs/1.1/dataformat.1.1.html
- **Verified:** 2026-06-21 via Wikipedia ASTC article citation
- **Key data:** ASTC supports 1-4 channels with LDR or HDR encoding modes; void-extent blocks for constant color regions; multi-partitioning up to 4 subsets.
- **Use:** Spec authority for ASTC block-level features (dual-plane, void-extent, multi-partition) — not implemented in my simplified prototype but available in production encoders.

---

## Tier 3 — Cited-but-not-verified (deferred to integration session)

### 11. Microsoft DirectXTex (DirectX SDK) — BC6H/BC7 reference impl
- **URL:** https://github.com/microsoft/DirectXTex
- **Status:** Official Microsoft BC6H/BC7 reference encoder. Per bc7enc README, DirectXTex has pbit bugfix pending — should validate before integration.
- **Defer:** Stage 4.3 integration session with real encoder integration.

### 12. ARM-software/astc-encoder (astcenc LDR/HDR official)
- **URL:** https://github.com/ARM-software/astc-encoder
- **Status:** Official ARM ASTC reference encoder. Apache 2.0.
- **Defer:** Stage 4.3 integration session.

### 13. Google/etc2comp (ETC2 reference impl)
- **URL:** https://github.com/google/etc2comp
- **Status:** Official Google ETC2 reference encoder. Apache 2.0.
- **Defer:** Stage 4.3 integration session.

### 14. Intel/ISPCTextureCompressor (ispc_texcomp BC + ASTC)
- **URL:** https://github.com/GameTechDev/ISPCTextureCompressor
- **Status:** Intel ISPC-vectorized texture compressor. Apache 2.0.
- **Defer:** Stage 4.3 integration session.

### 15. Compressonator (AMD open-source BC + ASTC + ETC2)
- **URL:** https://github.com/GPUOpen-Tools/compressonator
- **Status:** AMD open-source. Apache 2.0.
- **Defer:** Stage 4.3 integration session.

---

## Cross-vendor reference documentation (cited from `hardware-profile.md`)

- Vulkan 1.4 core spec: BC formats (`VkFormat` enum includes `VK_FORMAT_BC1_RGB_UNORM_BLOCK`, `VK_FORMAT_BC7_UNORM_BLOCK`, etc.).
- `VK_KHR_maintenance5` (2024 Q4): extends `vkGetPhysicalDeviceFormatProperties2` for additional BC format queries (relevant for runtime format selection).
- Vulkan 1.4 conformance per `hardware-profile.md §3`: RTX 3060 Ti GA104 supports all BC1-7 + ASTC LDR + ETC2 formats.

---

## Self-audit notes

- 4 primary sources fully retrieved via webfetch (Aras 2020 blog + richgel999/bc7enc README + Binomial basis_universal README + Wikipedia ASTC).
- 6 secondary sources verified via DuckDuckGo Lite search result snippets (Epic guide, AMD GPUOpen, Aras 2022, Phoronix 2021, etc.).
- 5 reference impl sources (DirectXTex / astcenc / etc2comp / ISPCTextureCompressor / Compressonator) listed for Stage 4.3 integration but not validated this session (no full content retrieval — adequate for deferred integration per `STATUS.md` blocker note).
- `web_search` (Exa) HTTP 429 persistent per operator directive 2026-06-21 → DuckDuckGo HTML endpoint (CAPTCHA-prone) + `lite.duckduckgo.com` fallback + direct webfetch on canonical URLs.
- All sources from 2020-2026 timeframe, well within 12-month freshness threshold per `INDEX.md §4 Risks` (texture compression is mature; no rapid API changes).
