# Sources — 2026-06-21-renderdoc-ci-capture

> **Capture date:** 2026-06-21 (single session).
> **Method:** webfetch + DuckDuckGo HTML fallback (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`).
> **Dev host:** `obvium` Arch Linux Zen 3 5800X / RTX 3060 Ti / Vulkan 1.4.341 per `hardware-profile.md §1+§3`.

---

## Primary SOTA references

### RenderDoc official docs (2026-06-21 fetched, v1.44)

1. **RenderDoc main docs** — [renderdoc.org/docs/index.html](https://renderdoc.org/docs/index.html).
   Baldur Karlsson (RenderDoc author), MIT license, supports Vulkan / D3D11 / D3D12 / OpenGL / OpenGL ES,
   Windows + Linux + Android + Nintendo Switch.

2. **Vulkan Support** — [renderdoc.org/docs/behind_scenes/vulkan_support.html](https://renderdoc.org/docs/behind_scenes/vulkan_support.html).
   Key facts:
   - "Vulkan is intended as a high-performance low CPU overhead API, and RenderDoc strives to maintain that
     performance contract at a reasonable level. While some overhead is inevitable RenderDoc aims to have no
     locks on the 'hot path' of command buffer recording, minimal or no allocation, and in general to have low
     performance overhead while not capturing."
   - Vulkan 1.4 support (with caveats); raytracing support limited on nvidia drivers.
   - "Try to avoid making very large memory allocations in the range of 1GB and above. By its nature RenderDoc
     must save one or more copies of memory allocations to enable proper capture" — **CRITICAL для ProjectV 8 GiB
     VRAM budget per `hardware-profile.md §3`**: large VBOs/SSBOs (>1 GB) trigger significant capture overhead.
   - Layer registration: Linux implicit layer.d (`/usr/share/vulkan/implicit_layer.d`,
     `/etc/vulkan/implicit_layer.d`, `$HOME/.local/share/vulkan/implicit_layer.d`).

3. **Quick Start** — [renderdoc.org/docs/getting_started/quick_start.html](https://renderdoc.org/docs/getting_started/quick_start.html).
   In-app overlay + capture hotkey (F12 or Print Screen) workflow.

4. **In-application API** — [renderdoc.org/docs/in_application_api.html](https://renderdoc.org/docs/in_application_api.html).
   API version 1.6.0. Key functions:
   - `RENDERDOC_GetAPI(RENDERDOC_Version version, void **outAPIPointers)` — runtime dynamic load via
     `dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)` on Linux.
   - `StartFrameCapture(device, window)` / `EndFrameCapture(device, window)` / `DiscardFrameCapture` — runtime
     trigger.
   - `TriggerCapture()` — trigger as if user pressed capture hotkey.
   - `TriggerMultiFrameCapture(numFrames)` — capture N sequential frames (API 1.1.0+).
   - `SetCaptureFilePathTemplate("my_captures/example")` — control capture file naming.
   - Capture options enum (`RENDERDOC_CaptureOption`): `eRENDERDOC_Option_CaptureCallstacks`,
     `eRENDERDOC_Option_RefAllResources`, `eRENDERDOC_Option_HookIntoChildren`,
     `eRENDERDOC_Option_CaptureAllCmdLists`, `eRENDERDOC_Option_APIValidation`.
   - For Vulkan: `RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(VkInstance)` helper macro.
   - Annotation system (API 1.7.0): `SetCommandAnnotation` / `SetObjectAnnotation` для `VK_EXT_debug_utils`
     object tagging (matches `PROJECTV_ENABLE_RENDERDOC_MARKERS`).

### RenderDoc CLI + automation

5. **Command-line Interface DeepWiki** — [deepwiki.com/baldurk/renderdoc/1.2-command-line-interface](https://deepwiki.com/baldurk/renderdoc/1.2-command-line-interface).
   `renderdoccmd` tool: capturing applications, managing/replaying captures, generating thumbnails,
   setting up remote connections.

6. **`rdc-cli` (BANANASJIM, 2026-02-23)** — [github.com/BANANASJIM/rdc-cli](https://github.com/BANANASJIM/rdc-cli)
   + [PyPI rdc-cli](https://pypi.org/project/rdc-cli/). **DIRECT SOTA reference для CI integration**: "Scriptable
   CLI for RenderDoc captures — built for terminal workflows, CI pipelines, and AI agents". `--json` / `--jsonl`
   output, TSV format. Released 2026-06-04 per PyPI.

7. **`renderdog-automation` (Rust crate, 2026-05-03)** — [lib.rs/crates/renderdog-automation](https://lib.rs/crates/renderdog-automation).
   Production Rust crate wrapping `renderdoccmd capture` + `qrenderdoc --python`.

8. **RenderDoc capture frame docs** — [renderdoc.org/docs/how/how_capture_frame.html](https://renderdoc.org/docs/how/how_capture_frame.html).
   `.cap` file format: executable path + working dir + cmd line + capture options.

9. **RenderDoc Python API: Capturing** — [renderdoc.org/docs/python_api/renderdoc/capturing.html](https://renderdoc.org/docs/python_api/renderdoc/capturing.html).
   Python-side control of `captureCallstacks`, `captureAllCmdLists`, etc.

10. **Phoronix "RenderDoc 1.7 Released With Vulkan Improvements"** — [phoronix.com/news/RenderDoc-1.7-Released](https://www.phoronix.com/news/RenderDoc-1.7-Released).
    "RenderDoc 1.7 comes with Python API changes, improved capture performance for Direct3D 12 programs,
    better handling of queue ownership transfer barriers in Vulkan, support for Vulkan's KHR_shader_non_semantic_info
    extension." — Vulkan capture overhead optimizations across versions.

11. **RenderDoc Wiki Vulkan API Support** — [github.com/baldurk/renderdoc/wiki/Vulkan/Vulkan-API-Support](https://github.com/baldurk/renderdoc/wiki/Vulkan/Vulkan-API-Support).

12. **vkguide (Vulkan Guide) Extra Chapter Graphics Performance Analysis** — [deepwiki.com/vblanco20-1/vulkan-guide/14-extra-chapter:-graphics-performance-analysis](https://deepwiki.com/vblanco20-1/vulkan-guide/14-extra-chapter:-graphics-performance-analysis).
    Production vkguide workflow: `VulkanEngine::draw()` → capture with RenderDoc → analyze.

13. **Blender GPU Debug RenderDoc integration** — [developer.blender.org/docs/features/gpu/tools/renderdoc/](https://developer.blender.org/docs/features/gpu/tools/renderdoc/).
    Production example: `GPU_debug_capture_begin` / `GPU_debug_capture_end` macros + `--debug-gpu-renderdoc`
    CLI flag. **DIRECT PRECEDENT для ProjectV `PROJECTV_CAPTURE_TRIGGER` env proposal**.

14. **`rudybear/renderdoc-skill` (Claude Code skill, 2026-02-28)** — [github.com/rudybear/renderdoc-skill](https://github.com/rudybear/renderdoc-skill).
    "A Claude Code skill that gives Claude the ability to capture, inspect, and debug GPU frames using
    RenderDoc. Works on Vulkan, D3D11, D3D12, and OpenGL." — **AI agent integration pattern для future
    enhancement**.

### CI/CD + golden image testing

15. **`Manas103/vision-regression-kit`** — [github.com/Manas103/vision-regression-kit](https://github.com/Manas103/vision-regression-kit).
    **DIRECT SOTA пример** для моего `ProjectVRegressionCaptureTests` CTest target: "small perceptual-diff
    regression harness for image pipelines. You give it a suite YAML (inputs + reference outputs + thresholds),
    it runs your pipeline, computes PSNR / SSIM / CLIP-similarity against the goldens, and decides pass / warn
    / fail for the whole run. There is also a Streamlit viewer for eyeballing borderline cases."

16. **Glint3D/Immersalab CI issue #6 "Visual Regression CI"** — [github.com/Immersalab/Glint3D/issues/6](https://github.com/Immersalab/Glint3D/issues/6).
    **PRODUCTION CI threshold**: "Add CI step to render example ops and compare against goldens using SSIM;
    upload diffs on failure. Acceptance Criteria Desktop: SSIM ≥ 0.995 or per-channel Δ ≤ 2 LSB."

17. **`vblanco20-1/vulkan-guide` performance analysis** — per source #12 above.

### Pixel-diff baselines + image quality metrics

18. **PSNR & SSIM Complete Guide 2026** — [123ofai.com/qnalab/system-design/blocks/psnr-ssim](https://123ofai.com/qnalab/system-design/blocks/psnr-ssim).
    Definitive reference: "PSNR (Peak Signal-to-Noise Ratio) and SSIM (Structural Similarity Index Measure)
    are the two most widely used full-reference image quality metrics in computer vision and signal processing."

19. **OpenCV PSNR/SSIM implementation reference** — [amroamroamro.github.io/mexopencv/opencv/image_similarity_demo.html](https://amroamroamro.github.io/mexopencv/opencv/image_similarity_demo.html).
    C++ reference implementation с PSNR + SSIM formulas.

20. **OpenCV GPU PSNR/SSIM** — [vovkos.github.io/doxyrest-showcase/opencv/sphinx_rtd_theme/page_tutorial_gpu_basics_similarity.html](https://vovkos.github.io/doxyrest-showcase/opencv/sphinx_rtd_theme/page_tutorial_gpu_basics_similarity.html).
    GPU-accelerated implementation reference (relevant для ProjectV GPU integration).

21. **Structural similarity (SSIM) — Wikipedia** — [en.wikipedia.org/wiki/Structural_similarity_index_measure](https://en.wikipedia.org/wiki/Structural_similarity_index_measure).
    Canonical SSIM formula reference.

22. **"Ways of cheating on popular objective metrics" — Video Processing** — [videoprocessing.ai/metrics/ways-of-cheating-on-popular-objective-metrics.html](https://videoprocessing.ai/metrics/ways-of-cheating-on-popular-objective-metrics.html).
    **IMPORTANT caveat**: PSNR/SSIM можно обмануть через blur, noise, super-resolution. Гласит что PSNR/SSIM
    не идеальны для visual quality, но sufficient для regression detection (not visual quality assessment).

23. **Comparative Analysis of Image Quality Assessment Metrics (MSE, PSNR, SSIM, FSIM)** — [researchgate.net/publication/378769473](https://www.researchgate.net/publication/378769473_Comparative_Analysis_of_Image_Quality_Assessment_Metrics_MSE_PSNR_SSIM_and_FSIM).
    Academic comparison 2024.

### StudyRaid / VkDebugUtils

24. **"Understand GPU timeline capture with tools like RenderDoc"** — [app.studyraid.com/en/read/14981/517341/gpu-timeline-capture-with-tools-like-renderdoc](https://app.studyraid.com/en/read/14981/517341/gpu-timeline-capture-with-tools-like-renderdoc).
    **Confirms `VK_EXT_debug_utils` extension integration path** с ProjectV `PROJECTV_ENABLE_RENDERDOC_MARKERS`.

25. **Scthe's blog "Debugging Vulkan using RenderDoc"** — [sctheblog.com/blog/debugging-vulkan-using-renderdoc/](https://www.sctheblog.com/blog/debugging-vulkan-using-renderdoc/).
    Practical Vulkan + RenderDoc workflow guide.

### Phoronix cross-references

26. **RADV VK_KHR_performance_query** — [phoronix.com/news/RADV-VK_KHR_performance_query](https://www.phoronix.com/news/RADV-VK_KHR_performance_query).
    Mesa 22.2 RADV supports `VK_KHR_performance_query`, verified against RenderDoc. Cross-vendor validation path.

---

## ProjectV mainline cross-references (verified `2026-06-21`)

- `agent/knowledge.md §547` — `PROJECTV_ENABLE_RENDERDOC_MARKERS` contract documentation (Debug default ON,
  `linux-clang-debug` preset OFF, gated compile-time, `PV_PROFILE_GPU_LABEL` / `PV_PROFILE_GPU_LABEL_COLOR`
  macros, volk function pointers для `VK_EXT_debug_utils` always enabled).
- `src/debug/ProfilingGpu.hpp:14,161,203` — existing RenderDoc marker integration (conditional on
  `PROJECTV_ENABLE_RENDERDOC_MARKERS`).
- `src/render/vulkan/VulkanBootstrap.cpp:592` — `VK_EXT_debug_utils` extension load (gated by
  `PROJECTV_ENABLE_VALIDATION || PROJECTV_ENABLE_RENDERDOC_MARKERS`).
- `src/render/vulkan/VulkanDebug.cpp:9` — debug utils integration (conditional on
  `!PROJECTV_ENABLE_RENDERDOC_MARKERS` — i.e. debug utils used when RenderDoc markers NOT enabled).
- `agent/knowledge.md §810` — `RecordGraphicsCommands` 5 sub-passes (shadow / meshing / taaResolve /
  debugOverlay / debugHud), `TaaRenderTargets.hpp`, `kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT`.
- `agent/knowledge.md §4` — build/verification contract.
- `agent/knowledge.md §30.4` — 3-step migration precedent.
- `TODO.md §Stage 0` — cross-cutting DoD «reproducibility».

## ProjectV pipeline enumeration (12 Vulkan passes per Stage 0-6 + Stage 5.x planned)

Per `TODO.md §Stage 0-6` + `agent/knowledge.md §810` + RenderDoc pass enumeration pattern:

1. **Depth prepass** (`TODO.md §2.2`) — depth-only forward pass, 1080p ≈ 8.3 MiB depth attachment.
2. **HZB cull compute** (`TODO.md §2.1`) — `RecordHzbCullingDispatch` + `hizVisibleCountBuffer` SSBO,
   4 B/chunk atomicAdd per visible chunk.
3. **HIZ mip chain build** (`TODO.md §2.1`) — `BuildHizMipChain` compute, ~10 mip levels for 1080p.
4. **voxel_mesh dispatch** (`TODO.md §2.2` / Pattern C) — compute or mesh shader per `agent/knowledge.md §32`,
   per-chunk greedy emit.
5. **CSM shadow cascade** (`TODO.md §Stage 0`) — 4 cascades, depth-only.
6. **Opaque forward pass** — `RenderGraphicsCommands` main pass (5 sub-passes).
7. **VCT cone-march** (`TODO.md §5.1`, planned Stage 5.1) — 6 diffuse + 1 specular cone per `TODO.md §5.1`.
8. **RTX ray query shadow** (`TODO.md §5.2`, planned Stage 5.2) — `VK_KHR_ray_query` per Boksansky 2019.
9. **Fluid CA ping-pong** (`TODO.md §3.1`, partial per `agent/workspace.md §1 Phase 1`) — compute pass,
   `SourceFluidCells` ↔ `DestinationFluidCells`.
10. **TAA resolve** (`TODO.md §5.3`, planned Stage 5.3) — `taa_resolve.frag` consume R16G16_SFLOAT
    motion vectors.
11. **Transparent forward pass** (`TODO.md §Stage 0`) — sorted back-to-front transparent.
12. **UI / debug overlay** (`agent/knowledge.md §810` debugOverlay + debugHud).

---

## Notes

- **`renderdoccmd` не установлен на dev host `obvium`** (verified `which renderdoccmd` → not found
  2026-06-21). Production validation = mainline scope, не this experiment. CPU-only analytical overhead
  model + CMakeLists/CTest integration design (а не реальный `renderdoccmd --capture` execution).
- **Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`**; used `webfetch` + DuckDuckGo HTML
  fallback throughout Phase B.
- **Sources freshness:** all primary RenderDoc docs fetched `2026-06-21` (RenderDoc v1.44 latest as of
  fetch). Industry CI tools (`rdc-cli`, `vision-regression-kit`) latest 2026 Q1-Q2.
- **Cross-vendor scope:** RenderDoc cross-vendor (Vulkan/D3D11/D3D12/GL/GLES); vkguide Vulkan example;
  Blender Vulkan production. Cross-vendor validation matrix deferred to mainline integration.
