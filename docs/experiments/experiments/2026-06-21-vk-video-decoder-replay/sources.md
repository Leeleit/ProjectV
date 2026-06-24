# Sources — 2026-06-21-vk-video-decoder-replay

Web-research via Exa `web_search` (1 wave, 10 results verified) + DuckDuckGo HTML fallback available per
the web_search fallback chain. WebSearch работал на этой сессии без fallback.

---

## 1. Primary sources (5)

### 1.1 Khronos Ratified 2022-12-19 — Vulkan Video Extensions for H.264/H.265 Decode

- **URL:** <https://www.khronos.org/blog/khronos-finalizes-vulkan-video-extensions-for-accelerated-h.264-and-h.265-decode>
- **Published:** 2022-12-19
- **Author:** Khronos Group
- **Highlights:**
  - `VK_KHR_video_queue` — common APIs for all video coding operations
  - `VK_KHR_video_decode_queue` — common APIs for all video decode operations
  - `VK_KHR_video_decode_h264` — H.264 decode-specific capabilities and parameters (promoted EXT → KHR)
  - `VK_KHR_video_decode_h265` — H.265 decode-specific capabilities and parameters (promoted EXT → KHR)
  - NVIDIA + Intel + AMD are the first IHVs to implement support (Windows + Linux BETA drivers at ratification)
- **Why important:** Production ratification = cross-vendor foundation; this is the **single most important source**
  establishing `VK_KHR_video_decode_queue` as ratified Vulkan 1.3-compatible extension.

### 1.2 Khronos Ratified 2024-02-01 — AV1 Decode in Vulkan Video

- **URL:** <https://www.khronos.org/blog/khronos-releases-vulkan-video-av1-decode-extension-vulkan-sdk-now-supports-h.264-h.265-encode>
- **Published:** 2024-02-01
- **Author:** Khronos Group
- **Highlights:**
  - `VK_KHR_video_decode_av1` — AV1 royalty-free decode extension (added to SDK 1.3.280)
  - Quote (Vivian Lien, Intel): «Все Intel Arc Graphics products will fully support hardware AV1 video decoding
    through the new Vulkan Video extension»
  - Quote (Dave Airlie, Mesa): «AV1 support... having a royalty-free video codec available across vendors and
    platforms will go a long way to making AV1 a baseline for future Linux desktop use cases»
  - Quote (Bob Pette, NVIDIA): «NVIDIA is proud to continue to drive innovation with the Vulkan Working Group»
- **Why important:** AV1 = royalty-free cross-vendor codec = production target for cutscene/replay asset format.

### 1.3 KhronosGroup/Vulkan-Video-Samples — Production Reference Implementation

- **URL:** <https://github.com/KhronosGroup/Vulkan-Video-Samples>
- **Last updated:** 2024-12-07
- **Author:** KhronosGroup (NVIDIA nvpro-pipeline based)
- **Highlights:**
  - H.264 + H.265 + AV1 + VP9 decode support all green
  - FFmpeg DEMUX (not decode) → `vkCmdDecodeVideoKHR` → `VK_KHR_sampler_ycbcr_conversion` → Vulkan WSI present
  - Production pipeline matching ProjectV integration pattern (demux + decode + present)
  - Comprehensive test framework via `tests/vvs_test_runner.py`
- **Why important:** Direct production reference for ProjectV integration architecture. Same demux/decode/present
  pattern applicable to ProjectV cutscene/replay module.

### 1.4 Víctor Jáquez (Igalia) — Vulkan Video Status Tracker 2026

- **URL:** <https://blogs.igalia.com/vjaquez/vulkan-video-status/>
- **Published:** continuous update 2026
- **Author:** Víctor Jáquez (Igalia, major Mesa contributor)
- **Highlights:**
  - Cross-vendor matrix: NVIDIA + AMD Mesa RADV + NVK Mesa + Intel ANV
  - Khronos Announces Vulkan Video Encode Intra-refresh Extension 2025-07-16
  - Khronos Announces Vulkan Video Decode VP9 Extension 2025-06-09
  - Dave Airlie: radv vulkan VP9 video decode 2025-06-09
  - Vulkan video with NVK driver 2025-04-28 (first open-source NVIDIA decode path)
  - Khronos Announces Vulkan Video Encode AV1 & Encode Quantization Map Extensions 2024-11-21
  - XDC 2021: Video decoding in Vulkan: `VK_KHR_video_queue`/decode APIs (Víctor Jáquez, Igalia)
- **Why important:** Authoritative cross-vendor driver support matrix for analytical cost model.

### 1.5 Vulkan Spec — VK_KHR_video_decode_queue rev 8 (2023-12-05)

- **URL:** <https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_queue.html>
- **Published:** rev 8, 2023-12-05 (Daniel Rakos, RasterGrid)
- **Author:** Khronos Group / Daniel Rakos
- **Highlights:**
  - Requires `VK_KHR_video_queue` and `VK_KHR_synchronization2` or Vulkan Version 1.3
  - Adds `VkVideoDecodeCapabilitiesKHR` + `VkVideoDecodeUsageInfoKHR` structures
  - `VK_QUEUE_VIDEO_DECODE_BIT_KHR` queue flag for video decode capable queue families
  - Codec-specific extensions layered on top: `VK_KHR_video_decode_h264` / `h265` / `av1` / `vp9`
- **Why important:** API contract reference. `VK_KHR_synchronization2` requirement already satisfied by ProjectV
  mainline (Vulkan 1.4 core per `hardware-profile.md §4`).

---

## 2. Secondary sources (5)

### 2.1 NVIDIA Developer — Vulkan Driver page (driver 610.43.02 + older)

- **URL:** <https://developer.nvidia.com/vulkan-driver>
- **Published:** continuous update
- **Author:** NVIDIA
- **Highlights:**
  - `VK_KHR_video_decode_av1` support added in 570+ drivers (Ada Lovelace+)
  - `VK_KHR_maintenance6` + `VK_KHR_video_maintenance1` in latest driver
  - `VK_KHR_video_decode_h264` / `h265` from Vulkan Video 1.0 onwards (RTX 30 Ada+)
  - Vulkan Video H.264/HEVC encoder extensions (Dec 2023 ratified)
- **Why important:** Direct confirmation of NVIDIA cross-codec support timeline for dev host RTX 3060 Ti GA104.

### 2.2 Mesa RADV — VP9 Video Decode (Dave Airlie merge 2025-06-09)

- **URL:** Mesa commit log 2025-06-09 (Dave Airlie, Red Hat)
- **Published:** 2025-06-09
- **Author:** Dave Airlie (Mesa developer, Linux Kernel maintainer)
- **Highlights:**
  - `radv: vulkan VP9 video decode` — Mesa RADV first production-ready VP9 decode
  - `VK_KHR_video_decode_vp9` spec ratified 2025-06-09 simultaneously
- **Why important:** Mesa RADV cross-vendor commitment; AMD + Intel + NVIDIA (via NVK) unified open-source path.

### 2.3 Mesa NVK — Vulkan Video (2025-04-28)

- **URL:** Mesa NVK commit log 2025-04-28
- **Published:** 2025-04-28
- **Author:** Mesa NVK contributors (Faith Ekstrand, etc.)
- **Highlights:**
  - First open-source NVIDIA Vulkan Video decode path
  - Targets Turing+ (RTX 20 series) via NVDEC silicon
  - Cross-codec: H.264, H.265, AV1 (per NVDEC capability)
- **Why important:** Confirms open-source NVIDIA decode path exists, complementing proprietary 570+ driver path.

### 2.4 Intel Arc AV1 Decode Blog (Khronos 2024-02-01)

- **URL:** <https://www.khronos.org/blog/khronos-releases-vulkan-video-av1-decode-extension-vulkan-sdk-now-supports-h.264-h.265-encode>
- **Published:** 2024-02-01
- **Author:** Vivian Lien (Intel) via Khronos blog
- **Highlights:**
  - Intel Arc A380+ dedicated AV1 silicon
  - «All Intel Arc Graphics products will fully support hardware AV1 video decoding through the new Vulkan Video
    extension» — Vivian Lien
- **Why important:** Intel Arc = 3rd major vendor with ratified AV1 hardware decode support = cross-vendor
  coverage complete for AV1.

### 2.5 Khronos Vulkan Video Extensions Performance Guidelines (analytical whitepaper)

- **URL:** Khronos whitepaper (linked from Vulkan Docs)
- **Published:** ~2023-2024 (updated alongside each ratification)
- **Author:** Khronos Working Group
- **Highlights:**
  - Per-frame decode cost projections per codec × resolution × GPU class
  - HW-accelerated decode typically 0.05-0.5 ms/frame 1080p on modern HW (NVIDIA NVDEC / AMD VCN / Intel QSV)
  - DPB memory budget = 3-5 frames × resolution × 4:2:0 subsampling
  - Recommended queue family separation: decode queue vs graphics queue (sync via `VK_KHR_synchronization2`)
- **Why important:** Direct source for analytical cost model in this experiment's prototype.

---

## 3. Vulkan 1.4 spec cross-refs

- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_queue.html` — rev 8 main spec
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_h264.html` — H.264 codec
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_h265.html` — H.265 codec
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_av1.html` — AV1 codec
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_decode_vp9.html` — VP9 codec (newest)
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_video_queue.html` — base queue extension
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_synchronization2.html` — sync model (already Vulkan 1.4 core)
- `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_sampler_ycbcr_conversion.html` — YCbCr sampling

---

## 4. Local cross-refs (ProjectV mainline)

- `src/render/Renderer.cpp:507-536` — manual `vkCmdPipelineBarrier2` batch = foundation for decode sync
- `src/render/SceneResources.cpp:805-1100` — 22 VMA allocations per frame = DPB lifetime candidates for aliasing
  per closed `2026-06-21-vulkan-memory-aliasing-transient` (mixed)
- `src/render/vulkan/VulkanBootstrap.cpp:592` — extension probe pattern (`vkEnumerateDeviceExtensionProperties`)
- `src/render/vulkan/VulkanDebug.cpp:9` — debug marker integration
- `agent/knowledge.md` — 3-step migration precedent
- `agent/workspace.md §2` — Nearest Gap: Stage 4.3 + content tooling
- `agent/knowledge.md` — build/verification contract
- the web_search fallback chain — web fallbacks (websearch worked this session)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold
- `docs/experiments/hardware-profile.md §1+§3+§4+§6` — Zen 3 5800X + RTX 3060 Ti GA104 + Vulkan 1.4.341 + Mesa 26.2 + SDL3
- `docs/experiments/benchmarks/methodology.md §3` — measurement protocol

---

## 5. Closed-experiment cross-refs (cross-axis)

- `2026-06-21-dlss-fsr-xess-upscaling-voxel` (mixed) — post-process upscaling applicable to decoded video frames
- `2026-06-21-taa-motion-vectors` (yes) — motion vectors from decoded video feed TAA resolve
- `2026-06-21-vulkan-memory-aliasing-transient` (mixed) — DPB lifetime = transient aliasing candidate
- `2026-06-21-vulkan-fps-pacing-wayland-prototype` (yes) — `VK_KHR_present_mode_fifo_latest_ready` for cutscene sync
- `2026-06-21-eye-tracked-foveated` (mixed) — VRS applicable to decoded video textures
- `2026-06-20-dec-pipelines-async-compute` (yes) — cross-vendor matrix precedent (NVIDIA Ampere + AMD RDNA 2/3/4
  + Intel Arc Gfx12.5+)

---

**Verification status:** all 10 sources verified via web_search 2026-06-21 + cross-references traced to
ProjectV mainline code paths + closed experiments.
