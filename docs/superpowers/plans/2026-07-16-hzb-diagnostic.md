# HZB Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the VoxelLab HZB A/B explainable by exposing accurate cull counts and GPU profiling ranges without
changing HZB eligibility, render defaults, or visual output.

**Architecture:** The completed per-slot visibility mask is the authoritative source for visible chunk count after its
fence signals; derive the HUD/replay counter from its valid bits rather than the GPU counter reused by cull and apply
passes. Add nested profiling ranges around the existing HZB work in `RecordGraphicsCommands`, then include the resulting
visible/cull state in the 120-tick InputReplay metrics line.

**Tech Stack:** C++26, Vulkan 1.4 dynamic rendering, Tracy Vulkan zones, Vulkan debug labels, existing
`ProjectVHzbCullingTests`.

## Global Constraints

- Target only `windows-clang-debug` evidence; do not use Release numbers.
- Preserve `MAX_FRAMES_IN_FLIGHT=2`, present mode behavior, HZB draw ordering, MSAA STORE/LOAD, mask scale, and AA
  defaults.
- Do not add a VoxelLab-specific HZB gate: `PROJECTV_HZB_CULLING` remains the explicit opt-in.
- Use code comments only as one trailing line when needed.
- Do not create a git commit unless the operator explicitly orders one.

---

### Task 1: Add observable HZB profiling boundaries

**Files:**

- Modify: `tests/HzbCullingTests.cpp`
- Modify: `src/render/RendererRecordCommands.cpp`
- Modify: `src/app/AppUpdate.cpp`

**Interfaces:**

- Consumes: `PV_PROFILE_GPU_ZONE`, `PV_PROFILE_GPU_LABEL`, and the existing InputReplay metrics log.
- Produces: five nested GPU ranges and three replay fields: `hzb_vis`, `hzb_cull`, `hzb_cut`.

- [x] **Step 1: Write the failing source-contract test**

```cpp
void TestHzbDiagnosticRangesAndReplayFields(TestContext &context)
{
    const std::string rendererSource = ReadProjectSource("render/RendererRecordCommands.cpp");
    const std::string appUpdateSource = ReadProjectSource("app/AppUpdate.cpp");
    for (const char *name : {"HZB Pass A Apply", "HZB Build Mip Chain", "HZB Cull Dispatch", "HZB Pass B Apply", "HZB Pass B Raster"}) {
        EXPECT_TRUE(context, rendererSource.find(name) != std::string::npos, name);
    }
    EXPECT_TRUE(context, appUpdateSource.find("hzb_vis=%u") != std::string::npos, "replay emits visible HZB chunks");
    EXPECT_TRUE(context, appUpdateSource.find("hzb_cull=%u") != std::string::npos, "replay emits culled HZB chunks");
    EXPECT_TRUE(context, appUpdateSource.find("hzb_cut=%u") != std::string::npos, "replay emits HZB cut state");
}
```

- [x] **Step 2: Run the focused test and verify RED**

Run: `ctest --test-dir build/windows-clang-debug -R "^ProjectVHzbCullingTests$" --output-on-failure`

Expected: test failure because the new range names and replay fields are absent.

- [x] **Step 3: Add only the diagnostic boundaries**

In `RendererRecordCommands.cpp`, bracket these exact calls/blocks with both
`PV_PROFILE_GPU_LABEL(cmd, "...")` and
`PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "...")`:

```cpp
// "HZB Pass A Apply"
const bool passAApplied = projectv::render::RecordHzbApplyVisibility(
    cmd, &context, render, render.sceneFrameResources[frameRenderData.frameIndex],
    frameRenderData.chunkDescriptorCount, passAMode);

// "HZB Build Mip Chain"
projectv::render::BuildHizMipChain(
    cmd, useDepthResolve ? render.depthResolveImage : render.depthImage, hizDepthLayout,
    render.hizBuffer, &render, &context);

// "HZB Cull Dispatch"
projectv::render::RecordHzbCullingDispatch(
    cmd, &context, render, render.sceneFrameResources[frameRenderData.frameIndex],
    *reinterpret_cast<const float (*)[16]>(viewProjectionFlat.data()),
    frameRenderData.chunkDescriptorCount);

// "HZB Pass B Apply"
const bool passBApplied = projectv::render::RecordHzbApplyVisibility(
    cmd, &context, render, render.sceneFrameResources[frameRenderData.frameIndex],
    frameRenderData.chunkDescriptorCount, projectv::render::HzbApplyMode::PassB);
```

Put the `"HZB Pass B Raster"` pair immediately before `vkCmdBeginRendering(cmd, &passBInfo)`
and let the scope end immediately after `vkCmdEndRendering(cmd)`. In `AppUpdate.cpp`, extend the
existing `SDL_Log` format string with `hzb_vis=%u hzb_cull=%u hzb_cut=%u` and append
`debug->stats.hzbVisibleChunkCount`, `debug->stats.hzbCulledChunkCount`, and
`debug->stats.hzbCameraCut ? 1u : 0u` in that order.

- [x] **Step 4: Run the focused test and verify GREEN**

Run: `ctest --test-dir build/windows-clang-debug -R "^ProjectVHzbCullingTests$" --output-on-failure`

Expected: `ProjectVHzbCullingTests` passes.

### Task 2: Derive accurate visible/cull counts from the fenced mask

**Files:**

- Modify: `tests/HzbCullingTests.cpp`
- Modify: `src/render/HizCulling.hpp`
- Modify: `src/render/HizCullingDispatch.cpp`
- Modify: `src/render/HizCullingApply.cpp`
- Modify: `src/app/FramePreparation.cpp`

**Interfaces:**

- Produces: `projectv::render::CountHzbVisibleChunks(std::span<const uint32_t>, uint32_t) -> uint32_t`.
- Consumes: the host-visible `visibilityMaskMappedData` copied after `vkWaitForFences`.

- [x] **Step 1: Write a failing count test**

```cpp
const std::array<uint32_t, 2> words{0b1011u, 0xffffffffu};
const uint32_t visible = projectv::render::CountHzbVisibleChunks(words, 34u);
EXPECT_EQUAL_UINT(context, 5u, visible, __LINE__, "only valid trailing mask bits count");
```

Use a matching test-local forward declaration until the public header exposes the function, so the focused target fails
to link for the missing implementation.

- [x] **Step 2: Run the focused test and verify RED**

Run: `cmake --build build/windows-clang-debug --target ProjectVHzbCullingTests --parallel 8`

Expected: link failure for `CountHzbVisibleChunks`, proving the new behavior is not present.

- [x] **Step 3: Implement the smallest safe helper and wire it after the fence**

```cpp
uint32_t CountHzbVisibleChunks(
    const std::span<const uint32_t> words,
    const uint32_t chunkCount);
```

Count all complete 32-bit words with `std::popcount`; mask the final partial word before counting it. In
`SyncHzbUnifiedVisibilityAfterFence`, update `hzbLastVisibleChunkCount` and `hzbLastCulledChunkCount` from the copied
unified mask and `slot.chunkDescriptorCount`. Remove the stale `hzbVisibleCountMappedData` read in
`FramePreparation.cpp`; that counter is overwritten by Pass B and is not the cull result.

- [x] **Step 4: Run focused unit tests and verify GREEN**

Run: `ctest --test-dir build/windows-clang-debug -R "^ProjectVHzbCullingTests$" --output-on-failure`

Expected: `ProjectVHzbCullingTests` passes.

### Task 3: Build and capture the diagnostic baseline

**Files:**

- Modify: `agent/workspace.md`

**Interfaces:**

- Consumes: the existing Windows InputReplay profiling harness and HZB environment toggle.
- Produces: an updated workspace snapshot and paired HZB OFF/ON capture paths.

- [x] **Step 1: Build the changed Debug targets**

Run: `cmake --build --preset windows-clang-debug-build --target ProjectV ProjectVHzbCullingTests --parallel 8`

Expected: both targets build successfully.

- [x] **Step 2: Run the HZB test target**

Run: `ctest --test-dir build/windows-clang-debug -R "^ProjectVHzbCullingTests$" --output-on-failure`

Expected: `ProjectVHzbCullingTests` passes.

- [x] **Step 3: Capture one HZB-OFF and one HZB-ON full-look replay**

Use the existing InputReplay, MAILBOX, fullscreen, MSAA4 + SMAA, render scale 1.00, mask scale 1.0. Collect
`gpu_opaque`, `hzb_vis`, `hzb_cull`, `hzb_cut`, mean FPS, p1 FPS, and the five GPU ranges. Do not change
`runtime/scene.json`.

- [x] **Step 4: Record only the result in the workspace snapshot**

Update `agent/workspace.md` with the capture paths, the observed HZB stage attribution, and the decision whether a later
HZB redesign is justified. Do not change `TODO.md` unless the result creates a roadmap task.

