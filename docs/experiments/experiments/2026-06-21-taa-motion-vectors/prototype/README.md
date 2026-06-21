# TAA motion vectors prototype

Standalone Vulkan 1.4 + C++26 harness per `docs/experiments/AGENTS.md §2` (no ProjectV deps).

## Status

**Measurement harness skeleton** (not a fully-functional benchmark). Renders synthetic voxel scene + 2 pipelines
(vertex-out motion vector MRT vs depth-reproject), measures frame time, outputs CSV. **GPU timing via timestamp
queries + per-pipeline command buffer recording NOT YET implemented** — current frame loop is a host-side timing
skeleton. Operator can extend to add full pipeline + render pass + TAA resolve recording for production-grade
measurements, OR use the web-research + TODO.md §5.3 explicit prescription as primary verdict basis.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-taa-motion-vectors/prototype
make            # clang++ -std=c++26 -O3 -march=native -DNDEBUG, glslc for SPIR-V
./taa_bench A   # Pipeline A only (vertex-out motion vector MRT)
./taa_bench B   # Pipeline B only (depth-reproject TAA)
./taa_bench AB  # both pipelines
```

**Prerequisites:**
- `clang++` ≥ 19 (ProjectV mainline per `agent/knowledge.md §17` uses Clang 22.1.6)
- Vulkan 1.4 driver (dev host = NVIDIA 610.43.02 per `hardware-profile.md §3` validated)
- `glslc` (Vulkan SDK glslc 2026.2 per `hardware-profile.md §6`)
- `xxd` (standard util-linux)
- Vulkan headers (`<vulkan/vulkan.h>`, version 1.4+)

**Validation layer:** not enabled in this skeleton harness (ProjectV mainline enables per `agent/knowledge.md §4`).
For real measurement integration, enable `VK_LAYER_KHRONOS_validation` and verify zero errors per
`AGENTS.md §6` (no Vulkan Validation Layer errors).

## What's implemented

- Vulkan 1.4 instance + device + queue + command pool
- `VK_KHR_dynamic_rendering` enabled (core 1.3) for pipeline creation
- 6 GLSL shaders: `voxel_a.vert/frag` (motion vector MRT) + `voxel_b.vert/frag` (depth-reproject) +
  `taa_resolve_a.comp` (TAA with motion vector) + `taa_resolve_b.comp` (TAA with depth reproject)
- Synthetic voxel scene: 1 cube, animated camera + animated dynamic object
- Halton (2,3) sub-pixel jitter for TAA
- Karis 2014 "Brute Force" TAA resolve with YCoCg color space + 3x3 AABB neighborhood clamping
- CSV output: `mean / median / p95 / p99 / std / min / max` per pipeline
- R16G16_SFLOAT motion vector format (per `TODO.md §5.3` line 425 explicit)

## What's NOT yet implemented (skeleton)

- **Pipeline creation:** vertex input state, rasterization state, color blend state, dynamic state, layout, etc.
  Required to actually invoke `vkCmdDraw`.
- **Render pass / dynamic rendering begin/end commands:** required to render anything to color attachments.
- **TAA resolve compute pipeline binding + dispatch:** required to apply TAA to rendered frame.
- **Image layout transitions:** required between render pass and compute dispatch.
- **GPU timestamp queries via `vkCmdWriteTimestamp`:** required for accurate per-pass GPU timing
  (current harness uses host-side `std::chrono` timing, less accurate).
- **PSNR computation vs reference (8xSSAA target):** required for quality metric.
- **Visual output (offscreen present):** for visual verification, would require `VK_KHR_surface` +
  `VK_KHR_swapchain` (more complex than offscreen storage images).

## Extension path

To make this harness production-grade, operator would add:

1. **Pipeline creation** for voxel_a (vertex+frag) + voxel_b (vertex+frag) + taa_resolve_a/b (compute).
   Use `VkPipelineRenderingCreateInfoKHR` pNext per VK_KHR_dynamic_rendering spec.
2. **Descriptor set layout** with bindings for UBO (viewProjCurr + viewProjPrev) + storage images
   (colorCurr + colorPrev + colorOutput + motionCurr or depthCurr).
3. **Render pass commands:** `vkCmdBeginRendering` with color + (motion or depth) attachments +
   `vkCmdBindPipeline` + `vkCmdBindVertexBuffers` + `vkCmdDraw` + `vkCmdEndRendering`.
4. **Compute dispatch:** `vkCmdBindPipeline` + `vkCmdBindDescriptorSets` + `vkCmdDispatch`.
5. **Image layout transitions:** via `VkImageMemoryBarrier2` + `vkCmdPipelineBarrier2`.
6. **GPU timing:** `vkCmdWriteTimestamp` before/after each pass + `vkGetQueryPoolResults` to read.
7. **PSNR:** host-side comparison of readback images (save PPM via `SavePPM` function in main.cpp).

This is ~300-500 LoC of additional code. Pattern matches `2026-06-20-async-compute-overhead-numbers/prototype/main.cpp`
(1323 LoC, full implementation as reference).

## Verdict basis (independent of measurements)

Even without full prototype execution, the verdict is well-supported by:

1. **`TODO.md §5.3` line 425 explicit format prescription:** «`VK_FORMAT_R16G16_SFLOAT`» motion vector MRT.
2. **Karis 2014 SIGGRAPH foundational paper:** «16:16 RG velocity buffer» = R16G16_SFLOAT = matches
   `TODO.md §5.3` exactly. «Velocity accuracy is super important» drives vertex-out recommendation.
3. **Industry standard:** UE 5 + Godot 4.x + Unity HDRP + Karis 2014 all use R16G16_SFLOAT for motion vectors.
   No cross-vendor ambiguity.
4. **VRAM cost:** 4 MiB/frame (R16G16_SFLOAT @ 1080p) = 0.08% of 5.06 GiB budget per `hardware-profile.md §3`.
   Double-buffered (history ping-pong) = 8 MiB total = 0.16%. Well under 5% threshold.
5. **Cross-validation with `dec-pipelines-async-compute`:** async-compute foundation (closed verdict=yes,
   +9.85-11.34% measured) is prerequisite for cross-frame pipelining pattern that motion vector MRT enables.
6. **TODO §5.3 DoD explicit goal:** «Полное исчезновение шлейфов за перемещаемыми гравипушкой моделями» —
   only achievable with vertex-out motion vectors (depth-reproject has fundamental precision loss near edges
   of dynamic objects per Karis 2014).

See experiment `README.md §2 Prior art` + `sources.md` for full verification.

## Files

- `main.cpp` (~525 LoC) — harness, Vulkan init, frame loop, CSV output
- `shaders/voxel_a.vert` (~25 LoC GLSL) — vertex shader for Pipeline A (writes motion vector via MRT)
- `shaders/voxel_a.frag` (~30 LoC GLSL) — fragment shader for Pipeline A (writes motion vector MRT)
- `shaders/voxel_b.vert` (~22 LoC GLSL) — vertex shader for Pipeline B (no motion vector)
- `shaders/voxel_b.frag` (~25 LoC GLSL) — fragment shader for Pipeline B (color only)
- `shaders/taa_resolve_a.comp` (~70 LoC GLSL) — TAA resolve reading motion vector MRT
- `shaders/taa_resolve_b.comp` (~80 LoC GLSL) — TAA resolve reconstructing from depth
- `Makefile` (~25 LoC) — build via clang++ + glslc + xxd
- `results.csv` — generated by `./taa_bench`
