# Vulkan 1.4 Audit — ProjectV

**Date:** 2026-07-13  
**Auditor:** AI agent (read-only review, one new file deliverable)  
**Spec reference:** `docs/VulkanSDK-Linux-Docs-1.4.350.1/` (Vulkan SDK 1.4.350.1, May 2026)  
**Codebase:** `/home/le1t/Projects/ProjectV/src/`  
**Methodology:** Subsystem-by-subsystem review against chunked HTML spec (`chapN.html`), `best_practices.html`,
`release_notes.html`, and project philosophy (`docs/philosophy/`, `agent/knowledge.md`). All findings verified with
`rg`/`read` on the live tree. No code was modified.

---

## 0. Executive Summary

This audit reviewed all Vulkan rendering code in `src/render/` and `src/render/vulkan/` against the official Vulkan
1.4.350.1 specification and the project's own engineering contracts. The implementation is largely functional on the
target NVIDIA RTX hardware, but several spec-correctness gaps and contract divergences exist.

### Top 5 critical findings

1. **API version ceiling is 1.3, not 1.4.** `VulkanBootstrap.cpp:26` sets
   `kDefaultMinVulkanApiVersion = VK_API_VERSION_1_3`. No `VkPhysicalDeviceVulkan14Features` struct is queried anywhere
   in `src/`. This directly contradicts `AGENTS.md §2` ("Vulkan 1.4") and `docs/philosophy/31_vulkan.md` ("Vulkan 1.4
   без legacy"). This is the root blocker for every Vulkan 1.4 feature listed below.
2. **Depth attachment is discarded before HZB and post-FX read it.** `RendererRecordCommands.cpp:179` sets
   `depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE`. Immediately after the pass, `BuildHizMipChain` and
   `RecordPostFxPass` read `depthImage`. Per spec, `DONT_CARE` makes the contents undefined; the code relies on NVIDIA
   IMR behavior.
3. **Cloudscape is nested inside the main dynamic rendering pass.** `RendererRecordCommands.cpp:440` calls
   `RecordCloudscapeRaymarchPass` between `vkCmdBeginRendering` (line 230) and `vkCmdEndRendering` (line 459).
   `Cloudscape.cpp:551` issues its own `vkCmdBeginRendering`, violating `VUID-vkCmdBeginRendering-renderpass` (no nested
   render pass instances). Only triggers when `PROJECTV_CLOUDS=ON` (default OFF).
4. **Async HZB culling is dead code that silently disables culling.** `RecordHzbAsyncCullPass` /
   `SubmitHzbAsyncCullToComputeQueue` are defined but have no production callers. `RendererDrawFrame.cpp:307` skips the
   inline HZB cull when `asyncComputeHzbPathActive` is true, but the async replacement never runs.
   `vkCmdDrawIndirectCountKHR` then consumes a stale/uninitialized `hzbVisibleCountBuffer`.
5. **No cross-queue ownership transfer for EXCLUSIVE resources.** All barriers use `VK_QUEUE_FAMILY_IGNORED` for both
   src/dst queue families, yet async compute runs on a dedicated queue family (`VulkanBootstrap.cpp:265-298`). For
   `VK_SHARING_MODE_EXCLUSIVE` resources (all resources in the project), this is spec-non-compliant for cross-family
   access.

### Highest-impact contract divergences

- **Bindless descriptors not implemented** despite `docs/philosophy/31_vulkan.md` mandate. No
  `VkPhysicalDeviceDescriptorIndexingFeatures` is queried; every descriptor layout binding has `descriptorCount = 1`.
- **No pipeline cache** despite `90_code-review-checklist.md` requirement. All 31 `vkCreate*Pipelines` calls pass
  `VK_NULL_HANDLE`. Every cold start recompiles all PSOs from scratch.
- **Descriptor pools are destroyed and recreated** on refresh instead of updated, contradicting the §90 checklist.
- **19 legacy `vkCmdPipelineBarrier` and 6 legacy `vkQueueSubmit` remain** despite `synchronization2` being enabled and
  philosophy §31 / §90 requiring sync2.

### Overall verdict

The codebase is **functional but not spec-clean**. On the target NVIDIA RTX platform most issues are masked by driver
behavior, but the project is accumulating technical debt relative to its stated "Vulkan 1.4 core, no legacy" target. The
recommended path is a staged cleanup: (1) fix the spec-correctness items above, (2) bump the API version to 1.4 and
query the feature struct, (3) adopt the low-risk 1.4-promoted features, (4) measure before attempting
performance-oriented modernizations.

---

## 1. Methodology

### Scope

All files in `src/render/` and `src/render/vulkan/` that call Vulkan API functions, plus `src/core/Types.hpp/cpp`,
`src/asset/MeshGpuResources.*`, and selected shaders.

### Evidence-gate classification

Recommendations are tagged:

- **[A] Cleanup** — safe to do without runtime measurement; correctness or hygiene fix.
- **[B] Profiler-gated** — per `docs/philosophy/93_performance-methodology.md`, requires Tracy/NSight showing ≥10%
  improvement in a real scene before implementation.
- **[C] Hardware-gated** — depends on specific driver/GPU support (e.g. NVIDIA-only extensions, post-1.4 KHR
  extensions).

### Spec navigation

All spec citations point to `docs/VulkanSDK-Linux-Docs-1.4.350.1/chunked_spec/chapN.html#anchor`. Symbol locations were
resolved with `chunked_spec/search.index.js`. The two large PDFs (`1.4/vkspec.pdf`, `1.4-extensions/vkspec.pdf`) contain
the same normative content but were not used as primary references because the chunked HTML is faster to navigate.

### Limitations

- **Runtime validation layers were not executed.** This is a static audit. Several findings are marked "verify with
  validation layers enabled".
- **Windows paths in `CMakePresets.json` were not verified.** Mainline dev host is Linux per `AGENTS.md §3` and
  `agent/workspace.md`.
- **No shader SPIR-V disassembly was inspected.** Shader-side correctness assumes the GLSL source compiles to valid
  SPIR-V for the declared target environment.

---

## 2. Critical Correctness Findings

These are spec violations or functional bugs that can produce incorrect rendering, validation errors, or crashes on
compliant hardware.

### 2.1 API version ceiling contradicts the Vulkan 1.4 target

- **Files:** `src/render/vulkan/VulkanBootstrap.cpp:26`, `:686`, `:981`, `:543`
- **Code:** `inline constexpr uint32_t kDefaultMinVulkanApiVersion = VK_API_VERSION_1_3;`
- **Contracts:** `AGENTS.md §2` declares the stack as "Vulkan 1.4". `docs/philosophy/31_vulkan.md:8-24` says "Vulkan 1.4
  без legacy" and "Использовать Vulkan 1.4 core, не extensions". `agent/knowledge.md` calls the project "Vulkan
  1.4-capable" and "Vulkan 1.4 support: full".
- **Spec:** `chap4.html#VkApplicationInfo` (`pApiVersion` is the minimum required version); `chap62.html#versions-1.4`.
- **Finding:** The instance, device gate, and VMA allocator are all told the project requires Vulkan 1.3. No code in
  `src/` references `VK_API_VERSION_1_4`, `VkPhysicalDeviceVulkan14Features`, or any 1.4-promoted feature struct. This
  is not a VUID violation (advertising a lower minimum is legal), but it is a contract drift that blocks every 1.4
  modernization item.
- **Evidence:** `rg -n "VK_API_VERSION_1_4|VkPhysicalDeviceVulkan14Features" src/` → 0 matches.
- **Recommendation [A]:** Decide the version contract. Either bump `kDefaultMinVulkanApiVersion` to
  `VK_API_VERSION_1_4`, chain `VkPhysicalDeviceVulkan14Features`, and adopt the relevant 1.4 features; or amend
  `AGENTS.md` / philosophy §31 to document the deliberate 1.3 floor. The current undocumented divergence is the worst of
  both worlds.

### 2.2 Depth attachment discarded before HZB and post-FX read it

- **Files:** `src/render/RendererRecordCommands.cpp:179`, `src/render/HizCulling.cpp:294-361`,
  `src/render/PostFx.cpp:684-770`
- **Code:** `depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;`
- **Spec:** `chap8.html` (`VkAttachmentStoreOp`) — `DONT_CARE` makes the attachment contents undefined for subsequent
  reads.
- **Finding:** The main dynamic-rendering pass writes depth, then ends with `storeOp = DONT_CARE`. The same frame then
  calls `BuildHizMipChain` (which blits from `depthImage`) and `RecordPostFxPass` (which samples depth). On NVIDIA IMR
  the depth buffer is physically present and the reads work, but this is undefined behavior per spec and will break on
  TBDR architectures.
- **Recommendation [A]:** Set `depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE` whenever HZB or post-FX is
  enabled. Apply the same fix to `Cloudscape.cpp:539` once its nested begin/end is resolved.

### 2.3 Cloudscape pass is nested inside the main dynamic rendering pass

- **Files:** `src/render/RendererRecordCommands.cpp:230, 440, 459`; `src/render/Cloudscape.cpp:484-572`
- **Code:** `RecordCloudscapeRaymarchPass` is called between `vkCmdBeginRendering` and `vkCmdEndRendering`;
  `Cloudscape.cpp:551` calls `vkCmdBeginRendering` again.
- **Spec VUID:** `VUID-vkCmdBeginRendering-renderpass` — "This command must only be called outside of a render pass
  instance" (`chap8.html:1025`).
- **Finding:** Dynamic rendering does not support nesting. This only triggers when `PROJECTV_CLOUDS=ON` (default OFF),
  which is why it has not been observed in default runs.
- **Recommendation [A]:** End the main pass before Cloudscape, then begin a new main pass after it (or fold Cloudscape
  into the main pass without its own begin/end).

### 2.4 Async HZB culling is dead code and disables culling when active

- **Files:** `src/render/vulkan/VulkanAsyncCompute.cpp:306-454`, `src/render/RendererDrawFrame.cpp:290-307, 465-473`,
  `src/render/RendererRecordCommands.cpp:319`
- **Code:** `RecordHzbAsyncCullPass` and `SubmitHzbAsyncCullToComputeQueue` are defined but never called from production
  code. `RendererDrawFrame.cpp:307` skips the inline `RecordHzbCullingDispatch` when `asyncComputeHzbPathActive` is
  true. `RendererDrawFrame.cpp:465-473` signals `hzbBuildTimelineSemaphore` but no waiter exists.
- **Finding:** When `asyncComputeHzbPathActive` is true, the culling dispatch never runs, yet
  `vkCmdDrawIndirectCountKHR` consumes `hzbVisibleCountBuffer`. The inline cull is skipped because the code assumes the
  async path will run, but it never does. Additionally, `RecordHzbAsyncCullPass` hardcodes `currentFrame = 0` (line
  353), which would race even if it were wired up.
- **Evidence:** `rg -n "RecordHzbAsyncCullPass|SubmitHzbAsyncCullToComputeQueue" src/render --include="*.cpp"` → only
  `VulkanAsyncCompute.cpp`.
- **Recommendation [A]:** Either fully wire the async HZB path (with correct frame indexing, queue ownership transfer,
  and a wait on `hzbBuildTimelineSemaphore` in the graphics submit), or remove the `asyncComputeHzbPathActive` branch
  and delete the dead functions until the feature is implemented.

### 2.5 No cross-queue ownership transfer for EXCLUSIVE resources

- **Files:** All `VkImageMemoryBarrier` / `VkBufferMemoryBarrier` / `*Barrier2` sites in `src/render/`
- **Code:** Every barrier uses `srcQueueFamilyIndex = dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED`.
- **Spec:** `chap7.html` (Queue Family Ownership Transfer) — `VK_SHARING_MODE_EXCLUSIVE` resources require a release
  barrier (`srcFamily != dstFamily`) when crossing queue families.
- **Finding:** ProjectV creates all resources with `VK_SHARING_MODE_EXCLUSIVE` (no `VK_SHARING_MODE_CONCURRENT`
  anywhere) and runs async compute on a separate queue family. Timeline semaphores provide execution ordering but not
  the memory-availability handoff required by the spec for EXCLUSIVE resources. This is latent on NVIDIA but will fail
  on stricter hardware.
- **Evidence:** `rg -n "VK_SHARING_MODE_CONCURRENT" src/` → 0; `rg -n "queueFamilyIndex" src/render` → only
  `VK_QUEUE_FAMILY_IGNORED` in barrier fields.
- **Recommendation [A]:** For each resource written by compute and read by graphics (`worldGenVoxelBuffer`, fluid CA
  ping-pong buffers, HZB image if async path is wired), either create it with `VK_SHARING_MODE_CONCURRENT` listing both
  families, or add release/acquire barriers with real src/dst queue family indices.

### 2.6 HZB mip chain final barrier uses wrong `oldLayout` for intermediate mips

- **Files:** `src/render/HizCulling.cpp:311-452`
- **Code:** The intermediate loop transitions each mip `mipLevel-1` from `TRANSFER_DST_OPTIMAL` to
  `TRANSFER_SRC_OPTIMAL`. The final barrier then transitions the whole mip range from `TRANSFER_DST_OPTIMAL` to
  `SHADER_READ_ONLY_OPTIMAL`, but intermediate levels are already in `TRANSFER_SRC_OPTIMAL`.
- **Finding:** The final barrier's `oldLayout` does not match the actual layout of the intermediate mip levels. This is
  a layout mismatch that validation layers should flag.
- **Recommendation [A]:** Split the final transition into two subresource ranges: the last-written mip from
  `TRANSFER_DST_OPTIMAL` to `SHADER_READ_ONLY_OPTIMAL`, and the prior mips from `TRANSFER_SRC_OPTIMAL` to
  `SHADER_READ_ONLY_OPTIMAL`.

### 2.7 HZB depth barrier uses invalid source stage mask

- **Files:** `src/render/HizCulling.cpp:334`
- **Code:** `srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`
  with `srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT`.
- **Spec:** `chap7.html` — `VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT` is only valid with `EARLY_FRAGMENT_TESTS_BIT`
  or `LATE_FRAGMENT_TESTS_BIT`; `COLOR_ATTACHMENT_OUTPUT_BIT` does not produce depth writes.
- **Recommendation [A]:** Remove `COLOR_ATTACHMENT_OUTPUT_BIT` from the source stage mask.

### 2.8 VCT clipmap mip chain missing source memory dependency

- **Files:** `src/render/vulkan/VulkanVoxelizePipeline.cpp:540-599`
- **Code:** The pre-barrier before the first `vkCmdBlitImage` uses `srcAccessMask = 0` and
  `srcStageMask = COMPUTE_SHADER`, even though the preceding compute dispatch writes the clipmap mip 0 via `GENERAL`
  -layout storage image.
- **Spec:** `chap7.html` — a transfer-read must wait on the prior `SHADER_WRITE` from `COMPUTE_SHADER`.
- **Recommendation [A]:** Set `srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT` and
  `srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`.

### 2.9 Duplicate screenshot implementation (ODR hazard)

- **Files:** `src/render/RendererScreenshot.cpp`, `src/render/ScreenshotDispatch.cpp`
- **Code:** Both files define identical free functions: `ShouldCaptureScreenshot`, `RecordSwapchainScreenshotCopy`,
  `SaveRequestedScreenshot`. `RendererRecordCommands.cpp` links against the global symbols from
  `RendererScreenshot.cpp`.
- **Finding:** `ScreenshotDispatch.cpp` is dead code and a one-definition-rule hazard. Both files define the same
  non-static functions in the global namespace.
- **Recommendation [A]:** Remove `ScreenshotDispatch.cpp` (or keep it as the single implementation and delete
  `RendererScreenshot.cpp`).

---

## 3. Contract Divergences

These are cases where the implementation does not match the project's own engineering contracts or philosophy.

### 3.1 Bindless descriptors (descriptor indexing) not implemented

- **Contract:** `docs/philosophy/31_vulkan.md:54-88` mandates "Bindless Descriptors (VK_EXT_descriptor_indexing) —
  неограниченные массивы ресурсов в одном descriptor set".
- **Reality:** No `VkPhysicalDeviceDescriptorIndexingFeatures` is queried or enabled. Every descriptor binding has
  `descriptorCount = 1`. The project uses `vkCmdBindDescriptorSets` (15 sites) and `vkUpdateDescriptorSets` (15 sites)
  for 1:1 bindings.
- **Impact:** The project is not using the binding model described in its own philosophy. This is a strategic gap, not a
  bug.
- **Recommendation [B/C]:** Enable `descriptorIndexing`, `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`,
  `descriptorBindingVariableDescriptorCount`, and migrate to unbounded descriptor arrays. This is a large architectural
  change; profiler-gate it.

### 3.2 No pipeline cache

- **Contract:** `90_code-review-checklist.md` — "Pipeline cache используется для тяжёлых pipelines".
- **Reality:** `rg "VkPipelineCache|vkCreatePipelineCache|vkMergePipelineCaches|vkGetPipelineCacheData" src/` → 0
  matches. All 31 `vkCreate*Pipelines` calls pass `VK_NULL_HANDLE`.
- **Impact:** Every cold start recompiles all pipelines from SPIR-V. The RT shadow pipeline and mesh shader pipeline are
  particularly expensive.
- **Recommendation [A]:** Create one `VkPipelineCache` at startup, pass it to every pipeline create call, and persist
  `vkGetPipelineCacheData` to disk. This is strictly additive and low-risk.

### 3.3 Descriptor pools are recreated, not updated

- **Contract:** `90_code-review-checklist.md` — "Descriptor sets обновляются через `vkUpdateDescriptorSets`, не
  пересоздаются".
- **Reality:** Nine `Refresh*ResourceBindings` functions destroy and recreate their descriptor pools on resize /
  resource change (e.g. `VulkanGraphicsPipelineBindings.cpp:32-91`, `VulkanVoxelMeshingPipeline.cpp:146-198`).
  `vkResetDescriptorPool` is never used; `vkFreeDescriptorSets` is never used.
- **Recommendation [A]:** Use `vkResetDescriptorPool` + `vkUpdateDescriptorSets` instead of destroy/recreate, or at
  least gate recreation to device-level events rather than routine refreshes.

### 3.4 Sync1/Sync2 mix

- **Contract:** `docs/philosophy/31_vulkan.md:224-243` and `90_code-review-checklist.md` expect Synchronization2 (
  `vkCmdPipelineBarrier2`/`vkQueueSubmit2`).
- **Reality:** `synchronization2` is enabled (`VulkanBootstrap.cpp:531`), but 19 `vkCmdPipelineBarrier` (sync1) and 6
  `vkQueueSubmit` (sync1) remain, concentrated in ray tracing, HZB, volumetric fog, RTX GI, and voxelize pipelines.
- **Recommendation [A]:** Migrate remaining sync1 calls to sync2. The mapping is mechanical for most sites.

### 3.5 Validation layers are compile-time gated, not runtime toggled

- **Contract:** `docs/philosophy/31_vulkan.md:288` says "В Debug: ON. В Release: OFF".
- **Reality:** `PROJECTV_ENABLE_VALIDATION` is a CMake compile definition (`CMakeLists.txt`) and is gated by `#if` in
  `VulkanBootstrap.cpp`. The intent is met (Debug ON, Release OFF) but via rebuild, not a runtime switch.
- **Verdict:** Minor divergence. The contract does not strictly require a runtime toggle, but the wording implies one.

### 3.6 Dead extension/feature flags

- **`VK_KHR_get_surface_capabilities2`** and **`VK_KHR_surface_maintenance1`** instance extensions are enabled but never
  used (no `vkGetPhysicalDeviceSurfaceCapabilities2KHR` etc.).
- **`VK_KHR_swapchain_maintenance1`** device extension and `swapchainMaintenance1` feature are enabled, but
  `VkSwapchainCreateInfoKHR.pNext` is `nullptr` (no `VkSwapchainPresentModesCreateInfoEXT`).
- **`VK_KHR_deferred_host_operations`** is enabled but only because it is a transitive dependency of
  `VK_KHR_acceleration_structure`; no `VkDeferredOperationKHR` is ever created or passed.
- **`bufferDeviceAddressCaptureReplay`** is enabled (`VulkanBootstrap.cpp:521`) but no capture/replay workflow exists.
- **Recommendations [A]:** Remove unused instance extensions and unused feature flags; consume swapchain maintenance1
  properly if kept, or drop it.

### 3.7 File length exceeds 600 lines

- **Contract:** `90_code-review-checklist.md` and the project's decomposition practice (see `knowledge.md` §11.1 and
  recent Renderer/SceneResources splits).
- **Reality:** 9 files exceed 600 lines: `VulkanBootstrap.cpp` (1102), `HizCulling.cpp` (1071), `PostFx.cpp` (945),
  `VulkanMeshShaderPipeline.cpp` (840), `SceneResources.cpp` (803), `SkyAtmosphere.cpp` (790),
  `VulkanVoxelizePipeline.cpp` (611), `VulkanFluidCaPipeline.cpp` (606), `VolumetricFog.cpp` (600).
- **Recommendation [A]:** Decompose the largest files following the same pattern used for
  Renderer/SceneResources/RayTracedShadows.

### 3.8 EVIL marker drift

- **Contract:** `docs/philosophy/13_evil-hacks.md` and `AGENTS.md §5.7` — `EVIL` must mark only localized, measured,
  documented evil hacks; comments must be one line after the code.
- **Reality:** `EVIL` appears 100+ times across `src/`. Many are tuned constants, binding-number comments, or other
  non-hacks. This dilutes the marker's meaning.
- **Recommendation [A]:** Audit `EVIL` usage and remove/mark properly.

---

## 4. Vulkan 1.4 Modernization Gap Analysis

### 4.1 Promoted-to-1.4 features not used

Because the API version is capped at 1.3, none of these are queried. Each is now core in Vulkan 1.4.

| Feature                                    | Promoted from                              | Relevance                                                                                                       | Class |
|--------------------------------------------|--------------------------------------------|-----------------------------------------------------------------------------------------------------------------|-------|
| `pushDescriptor`                           | `VK_KHR_push_descriptor`                   | High — per-frame descriptor updates (RT shadow, DDGI) could avoid pool churn.                                   | [B]   |
| `dynamicRenderingLocalRead`                | `VK_KHR_dynamic_rendering_local_read`      | Medium — deferred voxel passes could read attachments locally.                                                  | [C]   |
| `maintenance5`                             | `VK_KHR_maintenance5`                      | Medium — `vkGetImageSubresourceLayout2KHR`, `vkGetRenderingAreaGranularityKHR`, cleaner `vkCmdBindIndexBuffer`. | [A]   |
| `maintenance6`                             | `VK_KHR_maintenance6`                      | Medium — small API cleanups, `VkImageViewMinLod` without EXT.                                                   | [A]   |
| `load_store_op_none`                       | `VK_KHR_load_store_op_none`                | Medium — cleanly expresses "don't care" loads/stores.                                                           | [A]   |
| `map_memory2`                              | `VK_KHR_map_memory2`                       | Medium — `vkMapMemory2`/`vkUnmapMemory2` with flags; VMA will use automatically once `vulkanApiVersion >= 1.4`. | [A]   |
| `indexTypeUint8`                           | `VK_KHR_index_type_uint8`                  | High — `MeshGpuResources.cpp` forces `uint32_t` indices; uint8 cuts index memory 4× for small meshes.           | [C]   |
| `shaderFloatControls2`                     | `VK_KHR_shader_float_controls2`            | High — directly relevant to determinism contract (`knowledge.md §4` Fluid CA / TAA).                            | [C]   |
| `shaderSubgroupRotate` + `rotateClustered` | `VK_KHR_shader_subgroup_rotate`            | Medium — useful in compute reductions (HZB, voxel meshing, fluid CA).                                           | [C]   |
| `vertexAttributeInstanceRateDivisor`       | `VK_KHR_vertex_attribute_divisor`          | Medium — model instancing could benefit.                                                                        | [C]   |
| `hostImageCopy`                            | `VK_EXT_host_image_copy` (optional in 1.4) | Medium — direct CPU→image copies without staging buffer.                                                        | [C]   |
| `pipelineRobustness`                       | `VK_EXT_pipeline_robustness`               | Low — per-pipeline robustness control.                                                                          | [A]   |
| `line_rasterization`                       | `VK_KHR_line_rasterization`                | Low — debug overlay uses triangles, not lines.                                                                  | [B]   |
| `globalPriority`                           | `VK_KHR_global_priority`                   | Low — queue scheduling hints.                                                                                   | [B]   |
| `shaderExpectAssume`                       | `VK_KHR_shader_expect_assume`              | Low — SPIR-V hint intrinsics; requires compiler support.                                                        | [C]   |

### 4.2 1.4 updated limits of interest

| Limit                            | 1.3 min      | 1.4 min      | ProjectV impact                                                                                                              |
|----------------------------------|--------------|--------------|------------------------------------------------------------------------------------------------------------------------------|
| `maxPushConstantsSize`           | 128          | **256**      | `GraphicsPushConstants` is 128 B; `ModelPushConstants` is 192 B. Bumping to 1.4 gives headroom to merge more per-frame data. |
| `maxBoundDescriptorSets`         | 4            | **7**        | All shaders currently use `set = 0`; no immediate change, but future bindless layouts can use more sets.                     |
| `maxPushDescriptors`             | 16           | **32**       | Larger push-descriptor budget if adopted.                                                                                    |
| `maxDescriptorSetStorageBuffers` | 24           | **96**       | Critical for bindless SSBO arrays.                                                                                           |
| `maxDescriptorSetStorageImages`  | 24           | **144**      | Critical for bindless image arrays (clipmaps, fog volumes).                                                                  |
| `bufferImageGranularity`         | 131072       | **4096**     | Better VMA sub-allocation packing, lower memory waste.                                                                       |
| `maxComputeWorkGroupSize`        | (128,128,64) | (256,256,64) | Larger compute dispatches possible.                                                                                          |
| `maxImageDimension2D`            | 4096         | **8192**     | 8K textures / viewports.                                                                                                     |
| `maxColorAttachments`            | 7            | **8**        | One more MRT possible.                                                                                                       |

### 4.3 Post-1.4 KHR extensions to watch

These are in SDK 1.4.350.1 headers but not promoted to core 1.4.

| Extension                                                                 | Relevance                                                                | Class |
|---------------------------------------------------------------------------|--------------------------------------------------------------------------|-------|
| `VK_KHR_maintenance7` / `maintenance8` / `maintenance9` / `maintenance11` | API cleanups; `maintenance7` likely promoted soon.                       | [A]   |
| `VK_KHR_pipeline_binary`                                                  | Pipeline binary cache / precompiled shaders — big cold-start win.        | [C]   |
| `VK_KHR_device_fault`                                                     | GPU fault diagnostics.                                                   | [A]   |
| `VK_KHR_present_id` / `VK_KHR_present_wait`                               | Frame pacing / latency control.                                          | [C]   |
| `VK_KHR_fragment_shader_barycentric`                                      | Barycentric coords for wireframe / voxel highlighting.                   | [C]   |
| `VK_KHR_compute_shader_derivatives`                                       | Derivatives in compute for cone tracing.                                 | [C]   |
| `VK_KHR_ray_tracing_position_fetch`                                       | Natural follow-up to existing RTX path.                                  | [C]   |
| `VK_KHR_calibrated_timestamps` (KHR)                                      | Same as current `VK_EXT_calibrated_timestamps`; mostly naming migration. | [B]   |
| `VK_KHR_shader_maximal_reconvergence` / `VK_KHR_shader_quad_control`      | Niche shader control-flow.                                               | [C]   |

### 4.4 Ray tracing / mesh shader / NVIDIA-specific gaps

| Feature                                                        | Status   | Relevance                                                    | Class |
|----------------------------------------------------------------|----------|--------------------------------------------------------------|-------|
| `vkCmdTraceRaysIndirectKHR`                                    | Not used | GPU-driven RT dispatch (pixel mask from HZB).                | [B]   |
| `vkCmdDrawMeshTasksIndirectEXT` / `IndirectCount`              | Not used | GPU-culled mesh draws.                                       | [B]   |
| `VK_KHR_pipeline_library` + deferred host ops                  | Not used | Faster RT pipeline compile via linked libraries.             | [B]   |
| `VK_NV_shader_invocation_reorder` (SER)                        | Not used | RT divergence reduction; NVIDIA-only.                        | [C]   |
| `VK_EXT_opacity_micromap` (OMM)                                | Not used | Alpha-tested geometry; currently no triangle/alpha assets.   | [C]   |
| `VK_EXT_cluster_culling_shader`                                | Not used | Nanite-style cluster culling; below chunk granularity.       | [C]   |
| TLAS refit (`VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR`) | Not used | `ALLOW_UPDATE_BIT_KHR` not set on BLAS, so refit impossible. | [B]   |

---

## 5. Subsystem-by-Subsystem Verdict

| Subsystem                                 | Verdict             | Key findings                                                                                                                                                             |
|-------------------------------------------|---------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Bootstrap**                             | Mostly correct      | API version cap = 1.3; dead extension/feature flags; feature pNext chain is spec-legal.                                                                                  |
| **Swapchain**                             | Correct             | Format selection, present mode cycle, image usage, oldSwapchain handling are correct. Swapchain maintenance1 is enabled but not consumed.                                |
| **Memory / VMA**                          | Correct             | VMA 3.4.0 setup with volk is correct. No custom pools, defrag, or statistics — gap for long-running sessions.                                                            |
| **Synchronization**                       | Partial             | Sync2 used in many places; sync1 remains in RT/HZB/fog/GI/voxelize. Timeline semaphores for async compute correct. HZB timeline dead. No cross-queue ownership transfer. |
| **Command recording / Dynamic rendering** | Two critical bugs   | Nested Cloudscape pass; depth `storeOp = DONT_CARE` before reads. Otherwise correct (no legacy render passes).                                                           |
| **Descriptors**                           | Contract divergence | No bindless, no push descriptors, pools recreated instead of reset. Functionally correct.                                                                                |
| **Pipelines**                             | Contract divergence | No pipeline cache. Shader modules destroyed correctly after pipeline creation. RT pipeline/SBT correct.                                                                  |
| **Ray tracing**                           | Mostly correct      | BLAS/TLAS/SBT build correct; `ALLOW_COMPACTION_BIT_KHR` unused; scratch/SBT alignment assumed from VMA. `maxPipelineRayRecursionDepth = 1` matches shader.               |
| **Mesh shading**                          | Mostly correct      | Task+mesh shader setup correct (no task shader used). `vkCmdDrawMeshTasksEXT` chunk count not validated against device limits.                                           |
| **Async compute / frame loop**            | Partial             | Fluid CA / WorldGen async pairing correct. HZB async path dead. BLAS build stalls CPU with per-frame fence wait.                                                         |
| **Copy / Blit / Screenshot**              | Mostly correct      | Screenshot readback correct. HZB/VCT mip chains have barrier bugs. Duplicate screenshot implementation.                                                                  |

---

## 6. Philosophy §90 Compliance Checklist

| Item                                                 | Status        | Evidence / Recommendation                                                                                                                                                                                                               |
|------------------------------------------------------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **G1. No Vulkan handle leaks**                       | Compliant     | Static create/destroy counts show gaps, but all singleton handles are stored in `RenderState`/`VulkanContextState` and destroyed in `ShutdownVulkan` (`src/core/Types.cpp:26-151`). Run validation object-lifetime tracking to confirm. |
| **G2. Command buffers not used after pool reset**    | Compliant     | No `vkResetCommandPool` in `src/`; per-frame buffers are reset with `vkResetCommandBuffer` before `vkBeginCommandBuffer`.                                                                                                               |
| **G3. Descriptor sets updated, not recreated**       | **Violation** | Nine refresh functions destroy and recreate descriptor pools. Use `vkResetDescriptorPool` + `vkUpdateDescriptorSets`.                                                                                                                   |
| **G4. Pipeline cache used**                          | **Violation** | No pipeline cache anywhere. Add persistent cache.                                                                                                                                                                                       |
| **G5. Validation layers clean in Debug**             | Cannot verify | Static review found several patterns that commonly trigger validation (nested dynamic rendering, sync1 mix, depth storeOp, layout mismatches). Run validation and capture first 100 frames.                                             |
| **G6. No synchronous `vkQueueWaitIdle` in hot path** | Compliant     | `vkQueueWaitIdle`/`vkDeviceWaitIdle` only in cold paths (shutdown, scene destroy, swapchain recreate, scene reload). BLAS build uses fence but still stalls CPU in the frame loop.                                                      |
| **G7. Resource barriers via Synchronization2**       | **Partial**   | Sync2 used in new paths; 19 sync1 barriers and 6 sync1 submits remain. Migrate.                                                                                                                                                         |
| **G8. Timeline semaphores for cross-queue sync**     | **Partial**   | `renderTimelineSemaphore` used correctly. `hzbBuildTimelineSemaphore` created but not connected to a running compute dispatch. No queue-family ownership transfer.                                                                      |

### Other philosophy checks

- **P1 Error handling (§17):** Compliant — all `VkResult` returns checked and logged.
- **P2 EVIL markers (§13):** Drift — 100+ markers, many not actual hacks.
- **P3 Optimization hierarchy (§30):** Not verified — requires profiling.
- **P4 Hot-path allocations (§16, §18):** Compliant — no `vmaCreateBuffer/Image` in `DrawFrame`/
  `RecordGraphicsCommands`.
- **P5 Apple Silicon safety (§18, §90):** Compliant — no `alignas(64)`, no x86 intrinsics in render code.
- **P6 Anti-patterns in render hot path (§11):** Compliant — no `shared_ptr`, `map`, `unordered_map`, `string`, `try`/
  `catch`, `dynamic_cast`, `typeid`, or `virtual` in `src/render/*.cpp`.
- **P7 File length (§90):** Violation — 9 files > 600 lines.
- **P8 Validation layer gating (§31):** Compliant in intent (Debug ON, Release OFF) but compile-time, not runtime.

---

## 7. Prioritized Action Plan

### Phase 1 — Correctness (do first, [A])

1. **Bump API version to 1.4** or amend the contract documents. Add `VkPhysicalDeviceVulkan14Features` to the device
   chain if bumping.
2. **Fix depth `storeOp` to `STORE`** when HZB/post-FX is enabled (`RendererRecordCommands.cpp:179`,
   `Cloudscape.cpp:539`).
3. **Fix Cloudscape nested dynamic rendering** (`RendererRecordCommands.cpp:440` / `Cloudscape.cpp:551`).
4. **Fix or remove async HZB path** (`RendererDrawFrame.cpp:290-307, 465-473`, `VulkanAsyncCompute.cpp:306-454`).
5. **Fix HZB mip chain final barrier** (`HizCulling.cpp:432-452`) and invalid depth source stage mask (
   `HizCulling.cpp:334`).
6. **Fix VCT clipmap source barrier** (`VulkanVoxelizePipeline.cpp:540-548`).
7. **Add cross-queue ownership transfer** for compute→graphics resources (or use `VK_SHARING_MODE_CONCURRENT`).
8. **Remove duplicate screenshot implementation** (`ScreenshotDispatch.cpp` or `RendererScreenshot.cpp`).
9. **Remove dead extension/feature flags** (`VK_KHR_get_surface_capabilities2`, `VK_KHR_surface_maintenance1`,
   `VK_KHR_swapchain_maintenance1` if not consumed, `bufferDeviceAddressCaptureReplay`).
10. **Add pipeline cache** (`VkPipelineCache` + disk persistence).

### Phase 2 — Cleanup / modernization (low risk, [A])

11. **Replace descriptor pool destroy/recreate with `vkResetDescriptorPool`** in the 9 refresh functions.
12. **Migrate remaining sync1 barriers/submits to sync2**.
13. **Adopt `VK_ATTACHMENT_LOAD_OP_NONE` / `STORE_OP_NONE`** for dynamic rendering.
14. **Adopt `maintenance5` / `maintenance6`** feature structs (they become core in 1.4).
15. **Remove unused `ALLOW_COMPACTION_BIT_KHR`** from BLAS build or implement compaction.
16. **Audit and clean up `EVIL` markers**.
17. **Decompose files > 600 lines** (`VulkanBootstrap.cpp`, `HizCulling.cpp`, `PostFx.cpp`,
    `VulkanMeshShaderPipeline.cpp`, `SceneResources.cpp`, `SkyAtmosphere.cpp`).
18. **Remove unused `taskShader = VK_TRUE`** feature enable (no task shader exists).

### Phase 3 — Performance modernizations (measure first, [B])

19. **Push descriptors** for per-frame RT/DDGI descriptor updates.
20. **Indirect RT dispatch** (`vkCmdTraceRaysIndirectKHR`) with HZB pixel mask.
21. **Indirect mesh dispatch** (`vkCmdDrawMeshTasksIndirectEXT`) to remove CPU readback of visible count.
22. **Pipeline libraries + deferred host operations** for RT pipeline compile.
23. **TLAS refit** if `ALLOW_UPDATE_BIT_KHR` added and profiler shows rebuild cost.

### Phase 4 — Hardware-gated future options ([C])

24. **Bindless descriptors** (full descriptor-indexing migration).
25. **Host image copy** (`VK_EXT_host_image_copy`) for CPU uploads.
26. **`indexTypeUint8`** for small index buffers.
27. **`dynamicRenderingLocalRead`** for deferred passes.
28. **`shaderFloatControls2`** for determinism guarantees.
29. **`VK_KHR_present_id` / `VK_KHR_present_wait`** for frame pacing.
30. **NVIDIA SER / OMM** when hardware and assets justify it.

---

## 8. Spec and Contract Reference Index

### Spec chapters cited

- `chap4.html` — Initialization / instances (`VkApplicationInfo.pApiVersion`).
- `chap5.html` — Devices and queues (`VkDeviceCreateInfo`, feature pNext chains, queue priorities).
- `chap6.html` — Command buffers (reset/begin ordering).
- `chap7.html` — Synchronization and cache control (barriers, timeline semaphores, queue ownership transfer).
- `chap8.html` — Render pass / dynamic rendering (`VkRenderingInfo`, `VkRenderingAttachmentInfo`, `VkAttachmentStoreOp`,
  nested render pass VUID).
- `chap9.html` — Shaders (shader module lifetime).
- `chap10.html` — Pipelines (graphics, compute, ray tracing pipelines).
- `chap11.html` — Memory allocation.
- `chap12.html` — Buffers / resource creation.
- `chap13.html` — Images (layouts, subresources).
- `chap15.html` — Resource descriptors.
- `chap17.html` — Descriptor sets.
- `chap18.html` — Descriptor buffers.
- `chap25.html` — Copy commands (`vkCmdBlitImage`, `vkCmdCopyImageToBuffer`).
- `chap26.html` — Drawing commands.
- `chap30.html` — Mesh shading.
- `chap31.html` — Cluster culling shading.
- `chap39.html` — Window System Integration / swapchain.
- `chap42.html` — Acceleration structures.
- `chap44.html` — Opacity micromaps.
- `chap45.html` — Ray traversal.
- `chap46.html` — Ray tracing (`vkCmdTraceRaysKHR`, SBT, ray queries).
- `chap54.html` — Features (`VkPhysicalDeviceVulkan14Features`, `VkPhysicalDeviceVulkan13Features`, etc.).
- `chap55.html` — Limits.
- `chap62.html` — Appendix D: Core Revisions (1.4 promotions, new features, updated limits; 1.3 promotions).
- `chap63.html` — Appendix E: Extensions (extension dependencies).
- `release_notes.html` — SDK 1.4.350.1 release notes.
- `synchronization_usage.html` — Synchronization validation layer usage.

### Project contracts cited

- `AGENTS.md §2` — Stack: Vulkan 1.4.
- `AGENTS.md §4` — Sources of truth (code > AGENTS.md > knowledge.md > vendor docs > TODO.md).
- `AGENTS.md §5.8` — Subagent delegation rules.
- `agent/knowledge.md §2` — RTX-only hardware target.
- `agent/knowledge.md §4` — Release preset policy (no `-ffast-math`).
- `agent/knowledge.md §11` — Frame pipeline ordering.
- `agent/knowledge.md §12` — Async compute + timeline semaphore pairing.
- `agent/knowledge.md §13` — HZB culling.
- `agent/knowledge.md §14` — RTX shadows.
- `agent/knowledge.md §15` — RTX GI / DDGI.
- `agent/knowledge.md §16` — SSBO struct byte-exact invariant.
- `docs/philosophy/11_anti-patterns.md` — Hot-path anti-patterns.
- `docs/philosophy/12_decision-making.md` — Measurement-driven decisions.
- `docs/philosophy/13_evil-hacks.md` — EVIL marker policy.
- `docs/philosophy/17_error-handling.md` — VkResult / `Result<T>` handling.
- `docs/philosophy/18_data-layout.md` — Struct alignment and static_assert.
- `docs/philosophy/30_optimization.md` — Optimization hierarchy.
- `docs/philosophy/31_vulkan.md` — Vulkan 1.4 without legacy, bindless, push descriptors, validation layers.
- `docs/philosophy/90_code-review-checklist.md` — Vulkan/GPU checklist.
- `docs/philosophy/93_performance-methodology.md` — Measurement-first mandate.

---

## 9. Closing Notes

This audit was performed with read-only tools against the live source tree. No files were modified except for this
report. The parallel agent working on `TODO.md`/`agent/*` was not disturbed. All critical findings were cross-verified
by the primary agent before inclusion.

The most important next step is a project decision: **adopt Vulkan 1.4 as the runtime minimum** (matching the documented
stack) or **downgrade the documentation to Vulkan 1.3**. Once that contract is resolved, the Phase 1 correctness fixes
and Phase 2 low-risk modernizations can proceed in parallel.
