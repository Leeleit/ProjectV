# Prototype — 2026-06-21-vk-video-decoder-replay

Standalone C++26 CPU analytical cost model для in-engine Vulkan Video decode pipeline. Per
[`docs/experiments/AGENTS.md §13`](../AGENTS.md) — **analytical model only, NO Vulkan init, NO real
`vkCmdDecodeVideoKHR` dispatch**.

## Files

- `decoder_pipeline_bench.cpp` (~520 LoC) — main harness.
- `CMakeLists.txt` — CMake 4.x build config.
- `build/decoder_pipeline_bench` — built binary (Clang 22.1.6).
- `build/results.csv` — 216 rows × 13 cols = 25 KB.

## Build

```bash
cd prototype/
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_LINKER_TYPE=LLD
cmake --build build -j
```

Build green, **0 warnings** with `-Wall -Wextra -Wpedantic`.

## Run

```bash
./build/decoder_pipeline_bench
```

Output: `build/results.csv` (216 rows + header) + per-strategy summary on stdout.

## Measurement protocol

Per [`benchmarks/methodology.md §3`](../../benchmarks/methodology.md):

- **Strategies:** 3 (A_ExternalPlayer + B_FFmpegSWDecoder + C_VulkanVideoHWDecoder)
- **Scenarios:** 4 (intro_720p30 + cinematic_1080p60 + replay_1080p60 + trailer_4k30)
- **Codecs:** 3 (H.264 + H.265 + AV1)
- **Bitrates:** 2 (2 Mbps + 8 Mbps)
- **Seeds:** 3 (1 + 7 + 42)
- **Warmup:** 10 frames per config (discarded)
- **Measure:** 100 frames per config
- **Total:** 3 × 4 × 3 × 2 × 3 = 216 configs × 100 frames = **21,600 main measurements**

Wall time: < 1 sec on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.

## Cost model

Synthetic per-frame sample generation:

- A_ExternalPlayer: IPC + SDL upload + SDL_RenderPresent (CPU-bound)
- B_FFmpegSWDecoder: libavcodec CPU decode (H.264=3.5ms, H.265=5.0ms, AV1=8.0ms @1080p per FFmpeg benchmarks)
- C_VulkanVideoHWDecoder: vkCmdDecodeVideoKHR GPU dispatch (NVDEC/VCN/QSV silicon)

Per-frame variance: ±15% (Khronos Performance Guidelines steady-state HW bound).

First-frame latency: 100 ms (A subprocess fork+exec) / 50 ms (B decoder init) / 1 ms (C video session create).

## Cross-references

- Mainline integration: see [experiment README §7](../README.md#7-integration-recommendation).
- Results + interpretation: [RESULTS.md](../RESULTS.md).
- Sources: [sources.md](../sources.md).

## Reproducibility

Build environment:

- Clang 22.1.6 (per `hardware-profile.md §6`)
- CMake 4.x
- LLD linker
- C++26 standard
- `-O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic`

Reproducible: same source code + same compiler flags + same dev host = deterministic output
(within ±15% per-frame variance from synthetic sampling, controlled by seed).

## Caveats

See [experiment README §9](../README.md#9-mapping-to-projectv-hot-path) for full caveat list.
Key points:

- CPU-only analytical model (no Vulkan init).
- Per-frame decode cost from Khronos Performance Guidelines (not measured on dev host).
- NVDEC reference numbers from NVIDIA Application Note (RTX 3090 same arch as RTX 3060 Ti).
- Cross-vendor matrix from Igalia 2026 driver tracker (analytical projection).