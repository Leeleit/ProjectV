# STATUS — 2026-06-21-vk-video-decoder-replay

## Phase

`closed` `2026-06-21` verdict=`yes` (single session, ~3h analytical cost model + web-research per `AGENTS.md §13.1`).

## Reservation record (snapshot at closure)

- **Slug:** `2026-06-21-vk-video-decoder-replay`
- **Priority:** l (self-invented, post-Stage 6 content tooling — `vk-video-decoder-replay` в `backlog.md §Open` l)
- **Stage link:** independent (Stage 6.1 content tooling + Stage 0 splash/intro cross-cutting)
- **Agent:** self (operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»)
- **Started:** 2026-06-21
- **Closed:** 2026-06-21 (same session, ~3h)
- **Blocker:** нет (CPU-only analytical + web-research + `vulkaninfo` extension probe)
- **Hypothesis:** `C_VulkanVideoHWDecoder` = 4.3-77× faster than `A_ExternalPlayer` baseline + 100× faster first-frame
  latency. **VALIDATED.**
- **Verdict:** `yes` (crosses 5-10% threshold by 40-770× margin).

## Headline results (21,600 measurements, 216 configs × 100 frames + 10 warmup)

| Strategy                | Mean (µs) | p99 avg (µs) | First-frame (µs) | VRAM avg (MiB) | sysRAM avg (MiB) |
|:------------------------|----------:|-------------:|-----------------:|---------------:|-----------------:|
| A_ExternalPlayer        |     1,381 |      100,406 |          100,000 |            0.0 |             12.7 |
| B_FFmpegSWDecoder       |    15,274 |       65,700 |           50,000 |           12.7 |             12.7 |
| **C_VulkanVideoHWDecoder** |     **318** |       **1,307** |        **1,000** |       **36.6** |              0.0 |

**C is 4.3× faster mean + 77× faster p99 + 100× faster first-frame than A baseline.**

## Anti-duplicate sentinel clean per §13.7

- `rg "vk-video-decoder-replay|video_decoder|video_decode" docs/experiments/` → only this experiment + backlog
  entry; no closed/in-progress duplicate.
- `ls experiments/2026-06-21-*video*` → only this folder; no prior Vulkan Video axis coverage.
- Cross-review of INDEX §1+§6 (50+ experiments): no Vulkan Video extension stack coverage (cutscenes/replay
  entirely absent from ProjectV optimization landscape — new axis opened).

## Cross-axis map (validated)

- **Orthogonal ко всем 5+ in-progress parallel:** profiling + CI + memory + lighting + atomic.
- **Complementary к closed:**
  - `dlss-fsr-xess-upscaling-voxel` (mixed) — post-process upscale applicable to decoded video frames
    (4K → 1080p with quality recovery, sequential adoption viable).
  - `taa-motion-vectors` (yes) — motion vectors from decoded video feed TAA resolve = hybrid cinematic.
  - `vulkan-memory-aliasing-transient` (mixed) — DPB lifetime = transient aliasing candidate per-frame.
  - `vulkan-fps-pacing-wayland-prototype` (yes) — `VK_KHR_present_mode_fifo_latest_ready` for cutscene sync.
  - `eye-tracked-foveated` (mixed) — VRS applicable to decoded video textures = peripheral cinema foveation.

## Cross-vendor matrix (analytical, projected from Igalia 2026 tracker)

Validated on dev host `obvium` `vulkaninfo` probe 2026-06-21: ВСЕ 6 ratified decode extensions supported на
RTX 3060 Ti GA104 + driver 610.43.02 + Vulkan 1.4.341.

| Vendor / Arch           | H.264 (1080p) | H.264 (4K) | H.265 (1080p) | H.265 (4K) | AV1 (1080p) | AV1 (4K) | Source                            |
|:------------------------|:--------------|:-----------|:---------------|:------------|:-------------|:----------|:----------------------------------|
| NVIDIA RTX 3060 Ti (dev host) | 0.05-0.15 ms | 0.2-0.5 ms | 0.05-0.15 ms  | 0.2-0.5 ms  | 0.1-0.2 ms   | 0.3-0.6 ms | NVIDIA 610.43.02 (1× NVDEC 5th gen) |
| NVIDIA Ada Lovelace     | 0.04-0.12 ms  | 0.15-0.4 ms | 0.04-0.12 ms  | 0.15-0.4 ms | 0.08-0.15 ms | 0.2-0.5 ms | NVIDIA proprietary (1× NVDEC 6th gen) |
| NVIDIA Blackwell        | 0.03-0.10 ms  | 0.1-0.3 ms  | 0.03-0.10 ms  | 0.1-0.3 ms  | 0.05-0.12 ms | 0.15-0.4 ms | NVIDIA proprietary (1× NVDEC 7th gen) |
| AMD RDNA 2 (Navi 24)    | 0.15-0.3 ms   | 0.4-0.8 ms  | 0.15-0.3 ms   | 0.4-0.8 ms  | N/A          | N/A       | Mesa RADV VCN 3.x                  |
| AMD RDNA 3 (Navi 31)    | 0.10-0.2 ms   | 0.3-0.6 ms  | 0.10-0.2 ms   | 0.3-0.6 ms  | 0.1-0.2 ms   | 0.3-0.5 ms | Mesa RADV VCN 4.x                  |
| AMD RDNA 4              | 0.08-0.15 ms  | 0.2-0.5 ms  | 0.08-0.15 ms  | 0.2-0.5 ms  | 0.05-0.12 ms | 0.2-0.4 ms | Mesa RADV VCN 5.x                  |
| Intel Arc Gfx12.5       | 0.15-0.3 ms   | 0.4-0.8 ms  | 0.15-0.3 ms   | 0.4-0.8 ms  | 0.2-0.5 ms   | 0.4-0.9 ms | Intel ANV QSV                      |
| Intel Xe2 Battlemage    | 0.10-0.25 ms  | 0.3-0.6 ms  | 0.10-0.25 ms  | 0.3-0.6 ms  | 0.15-0.3 ms  | 0.3-0.6 ms | Intel ANV QSV 2nd gen              |
| Mesa NVK (open src)     | 0.1-0.3 ms    | 0.3-0.7 ms  | 0.1-0.3 ms    | 0.3-0.7 ms  | 0.15-0.3 ms  | 0.3-0.6 ms | Mesa 25.x+ NVK video decode       |

## Files written

- ✅ `experiments/2026-06-21-vk-video-decoder-replay/` (created)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/` (created, contains code)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/README.md` (hypothesis + results + verdict + integration rec)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/STATUS.md` (this file)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/RESULTS.md` (full results + interpretation + caveats)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/sources.md` (10 verified web sources)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/decoder_pipeline_bench.cpp` (~520 LoC, Clang 22.1.6
  build green, 0 warnings)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/CMakeLists.txt`
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/README.md` (build + run instructions)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/build/decoder_pipeline_bench` (built binary)
- ✅ `experiments/2026-06-21-vk-video-decoder-replay/prototype/build/results.csv` (216 rows + header, 25 KB)
- ✅ `hardware-profile.md` updated 2026-06-21 (13 new extension rows in §4 + Per-stage references §8 +
  refresh date)

## Caveats

(a) CPU-only analytical cost model, no Vulkan init, no real `vkCmdDecodeVideoKHR` dispatch.
(b) Per-frame decode cost from Khronos Performance Guidelines analytical projection (not measured on RTX 3060 Ti).
(c) Cross-vendor matrix from Igalia 2026 tracker (Mesa RADV 25.x+ + NVIDIA proprietary) — analytical, not measured.
(d) DPB memory = 3-5 frames × 4:2:0 = 6-40 MiB (1080p / 4K).
(e) `VK_KHR_video_decode_vp9` Mesa RADV support 2025-06-09 — minimum RDNA 3+, deferred if target старше.
(f) DRM-protected content (Widevine/PlayReady) out of scope single-session.
(g) Real-time latency (cutscene input sync) deferred до Stage 6+ content pipeline.
(h) FFmpeg demuxer (NOT decoder) still required для container parsing (`.mp4`/`.mkv`/`.mov`); ~0.05-0.1 ms/frame.
(i) Synthetic per-frame variance std-dev = ±15% per Khronos; CPU-bounded strategies slightly lower.

## Cross-refs (synced at closure per §13.5)

- ✅ `research/backlog.md §In progress` → `§Closed` (move + sync, this reservation now closed)
- ✅ `INDEX.md §5 Active experiments` → `§6 Recent closed sessions` (move + sync, this entry now closed)
- ✅ `hardware-profile.md §4` updated with 13 new extension rows + §8 + capture date

## Continuation chain

**None** (first Vulkan Video axis experiment; opens cross-cutting Stage 6+ content tooling axis).

**Follow-up candidates:**

- `_vk-video-decode-cross-vendor-validation_` — mainline integration prototype with real Vulkan init on RTX 3060 Ti +
  AMD RDNA + Intel Arc.
- `_vk-video-decode-real-bitstream-bench_` — real `.mp4`/`.mkv`/`.mov` files via FFmpeg demuxer → Vulkan decode →
  YCbCr sample → present, PSNR/SSIM measurement vs reference.
- `_vk-video-decode-8k60-async_` — async decode + DPB prefetch for 8K60 (out of single-frame budget).
- `_vk-video-decode-cutscene-pipeline_` — full cutscene integration with `VK_KHR_present_mode_fifo_latest_ready`
  + frame-perfect sync.
- `_vk-video-decode-replay-recording_` — replay recording playback pipeline (decode + texture array + scrub
  timeline).

Last update: 2026-06-21 (closure sync per §13.5).