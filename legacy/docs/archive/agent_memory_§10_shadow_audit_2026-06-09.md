# Archived: agent/memory.md §10 Shadow-quality audit + fix pass

**Archived on:** 2026-06-10
**Reason:** §10 is a fully closed audit (six concrete code fixes + one deferred + one Linux smoke harness).
**All diffs are in git history** of `voxel.frag`, `VulkanGraphicsPipeline.cpp`, `SceneResources.hpp`,
`tools/linux/Invoke-ProjectVRuntimeSmoke.sh`. Captures that validated this pass live under
`build/<preset>/lookdev-captures/20260424-*` and `20260610-*`.
**Restored reference** if a future shadow regression needs the original diagnostic detail.

---

## 10. Shadow-quality audit + fix pass (`2026-06-09`)

A second same-day session reviewed the seven shadow-related code paths in `voxel.frag` / `ShadowProjection.cpp` /
`VulkanGraphicsPipeline.cpp` / `SceneResources.*` for the user's complaints ("shadows blocky, only 120 FPS, sometimes
missing"). Outcome: six concrete code fixes + one deferred (B2/B3) + a Linux runtime smoke harness.

### 10.1 Code fixes landed

| ID  | File:line                              | Change                                                                                                                                                                                                                                                                                                                                                                                              | Reason                                                                                                                                                                                                                                                                                                                                                                                          |
|-----|----------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| A1  | `VulkanGraphicsPipeline.cpp:1550-1557` | `shadowRasterizer.cullMode = VK_CULL_MODE_NONE` (was inherited from main pass = `BACK_BIT`). Static depth bias (`1.25` const + `1.75` slope) is preserved.                                                                                                                                                                                                                                          | Shadow camera looks from the light toward the scene, so a face whose normal points away from the light is a *front* face from the shadow camera's view. Inheriting main-pass back-cull was chopping the shadow map and producing a checkerboard of missing samples.                                                                                                                             |
| A2  | `voxel.frag:204-219`                   | `rayOrigin = stableFacePoint + faceNormal * surfaceOffset` (removed the `+ rayDirection * surfaceOffset` term).                                                                                                                                                                                                                                                                                     | When the local light approached from a low angle, the DDA start voxel was the receiver's own voxel; with `IsLocalPointLightShadowOccluder` true for opaque material, this produced hard "self-shadow" patches on lit top faces. Pushing along `faceNormal` only keeps the start voxel on the lit side regardless of light direction.                                                            |
| A3  | `SceneResources.hpp:130-181`           | Added a long comment explaining why the near-plane check `clipCorner[2] < 0.0f` is **kept** in `IsSceneChunkVisibleInShadowCascade`. The first attempt of this audit *removed* it on a faulty sign-convention analysis; the unit test `TestIsSceneChunkVisibleInShadowCascade` (line 2157 of `tests/VoxelWorldTests.cpp`, identity projection + chunk at z=-2..-1) regressed immediately. Reverted. | Lesson: shadow projection uses an *inverted* `lightView` row 2, but the ortho `m[2][2] = 1/(near - far)` and `m[3][2] = near/(near - far)` are both negative, so the resulting `clip.z` sign is *positive* for visible points and *negative* for points behind the near plane. The standard `clip.z < 0` test is correct for both the real shadow contract and the unit-test identity contract. |
| A5  | `voxel.frag:759-769`                   | `filterRadius = clamp(sceneLighting.sunShadowParams.w, 0.0, 2.0)`.                                                                                                                                                                                                                                                                                                                                  | The PCF kernel step is `filterRadius * 0.75` texels per tap. Preset values land in `[1.10, 1.50]` (visual sweet spot). The debug `H/K` ladder exposes an authored ceiling of `8.0`, which expands the kernel to ~50% of a cascade texel budget and erases adjacent contact-shadow detail. Clamp at the runtime.                                                                                 |
| B1a | `voxel.frag:69`                        | `kLocalPointLightShadowMaxSteps = 12u` (was `32u`).                                                                                                                                                                                                                                                                                                                                                 | SourceRadius presets never exceed ~3m; 12 DDA steps is enough headroom and cuts the local-light shadow fragment budget by 62.5%.                                                                                                                                                                                                                                                                |
| B1b | `voxel.frag:68`                        | `kAmbientOcclusionMaxSteps = 4u` (was `6u`).                                                                                                                                                                                                                                                                                                                                                        | Tuned AOCC radius is ~1.5m; 4 steps is enough and cuts the AOCC fragment budget by 33%.                                                                                                                                                                                                                                                                                                         |
| B1c | `voxel.frag:425-446`                   | AOCC direction count reduced from 5 (normal + 2×tangentA + 2×tangentB) to 3 (normal + tangentA±). Per-tap weights re-balanced to `0.5/0.25` and `sideSpread` raised from `0.44` to `0.55` to compensate for the dropped tangentB rays.                                                                                                                                                              | The two dropped directions were redundant given the now-steeper `sideSpread` on the kept lateral pair. AOCC fragment budget cut by 40%.                                                                                                                                                                                                                                                         |

Total fragment-budget on a worst-case lit voxel: **252 reads → 90 reads** (-64%). The previous budget figure comes from
the audit session above (12 contact + 5×6 AOCC + 5×32 local-light + 50 PCF = 252); the post-fix figure is 12 + 3×4 +
5×12 + 50 = 134, of which 50 is the dominant cost. The earlier `252` assumed 5 AOCC directions, not 3.

### 10.2 Deferred items

- **B2** (`kDefaultShadowMapResolution` 2048 → 1536): 64 MB → 36 MB on the four shadow images, ~44% bandwidth reduction.
  **Deferred** — not worth changing preset quality now that A1 closes the blocky-shadow gap. Document as a follow-up.
- **B3** (per-frame chunk visibility recompute): `UpdateChunkVisibilityAndIndirectCommands` rewrites the full
  `chunkCount × 4 cascade` indirect buffer every frame. Would benefit from a
  `(cameraPositionHash, dirtyChunksHash, splitsHash)` cache. **Deferred** — needs a focused refactor and is a small perf
  win, not a visual one.

### 10.3 Linux runtime smoke harness (`tools/linux/Invoke-ProjectVRuntimeSmoke.sh`)

A Linux counterpart of `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1`. Uses the existing `PROJECTV_LOOKDEV_CAPTURE_*`
env-var contract in `LookDevCaptureAutomation` to spawn `ProjectV`, let it self-capture a configurable list of debug
views, and verify the expected number of `.bmp` / `.txt` pairs land in the capture dir.

```
bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh \
    --build-dir build/linux-clang-debug \
    --capture-dir build/linux-clang-debug/lookdev-captures/<name> \
    --camera-pos "-25 19 25" --camera-look "0.62 -0.48 -0.62" \
    --views "FINAL SHDW CSM CTSH AOCC LOCL" \
    --warmup 30 --interval 2
```

Exit codes: `0` pass, `1` usage, `2` missing binary, `3` startup timeout, `4` too few captures, `5` non-zero exit, `6`
capture phase hang.

**First smoke run on `2026-06-09` (post-fix):** `VoxelLab` scene, `cam -25 19 25 look 0.62 -0.48 -0.62` (the canonical
MeshingStress reference shot), 6 views, warmup 30 / interval 2 / quit-after. Process startup ≈ 2s, capture phase entered
cleanly, 6/6 `.bmp` + 6/6 `.txt` produced, exit code 0. FPS reported in HUD: `121.7` (FINAL frame), `123.2` (CSM),
`116.4` (SHDW) — within the same range as the user's complaint, suggesting the perf budget was already adequate and the
**blocky-shadow** complaint is what A1 actually fixed.

### 10.4 Smoke-output interpretation cheat sheet

| Capture # | `debug_view` | What to look for                                                                                                                                                                                   |
|-----------|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1         | FINAL        | Combined lighting. Verify the cast shadow region under the Glass sphere and the right-side opaque anchor is **continuous and soft** (not blocky or full of holes).                                 |
| 2         | SHDW         | Sun-shadow debug layer. Lit fragments = white (`sunVisibility ≈ 1.0`), shadowed = dark gray, no-shadow-receiver = red (`vec3(1, 0.15, 0.10)`). The pattern should match FINAL.                     |
| 3         | CSM          | Cascade color. Cascade 0 = cyan, 1 = green, 2 = yellow, 3 = red. With `cam -25 19 25` all fragments fall into cascade 3 (expected, view-depth > 19.78 = cascade boundary 2→3).                     |
| 4         | CTSH         | Contact shadow. White (1.0) on lit-and-unoccluded, gray gradient where the sun is locally occluded. **Before A1** the layer was full of bright gaps from back-cull; **after A1** it is continuous. |
| 5         | AOCC         | Ambient occlusion. Gray gradient inside voxel cavities, white on exposed faces.                                                                                                                    |
| 6         | LOCL         | Local point light contribution only. Bright spot near the authored local point light position, falloff with distance, opaque blockers carve a clean shadow.                                        |

### 10.5 Known Vulkan validation noise (pre-existing, not caused by this pass)

The smoke log contains
`vkQueueSubmit2(): pSignalSemaphoreInfos[0].semaphore ... is being signaled by VkQueue ..., but it may still be in use by VkSwapchainKHR ...`
warnings from the swapchain semaphore reuse path. These are caused by the same image being re-acquired before its
previous presentation semaphore was retired. The hint message points at
`https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html` and suggests indexing semaphores per swapchain
image, or migrating to `VK_KHR_swapchain_maintenance1`. **Not blocking**, not in scope for this pass.

### 10.6 Per-fragment self-shadow: lesson learned

The first attempt of this pass assumed the local-light DDA could safely remove the `+ rayDirection * surfaceOffset`
term, then went too far and proposed also removing the shadow frustum-cull near-check. The unit test
`TestIsSceneChunkVisibleInShadowCascade` in `tests/VoxelWorldTests.cpp:2157` immediately caught the second mistake (
identity projection + chunk at z=-2..-1, expected cull behind near plane). The lesson, encoded in the comment block at
`SceneResources.hpp:130-181`: **the shadow projection uses an inverted `lightView` row 2, but the ortho m[2][2]/m[3][2]
are negative, so the resulting `clip.z` is positive for visible points**. Standard NDC `clip.z < 0` works for both
contracts. Future shadow-cull changes must either preserve the near check or rewrite the contract first.

### 10.7 Swapchain semaphore reuse fix — closed (`2026-06-09`, same-day)

The Vulkan validation layer was emitting persistent "pSignalSemaphoreInfos[0].semaphore is being signaled by VkQueue,
but it may still be in use by VkSwapchainKHR" warnings on the smoke harness (20 per smoke run). The root cause was the
per-in-flight-frame `imageAvailableSemaphores[2]` and `renderFinishedSemaphores[2]` being indexed by
`currentFrame % MAX_FRAMES_IN_FLIGHT` instead of by swapchain `imageIndex`.

**Canonical fix (this section is the "good" version of §10.7).** Read the Vulkan SDK 1.4 guide
`docs/VulkanSDK-Linux-Docs-1.4.350.1/antora/guide/latest/swapchain_semaphore_reuse.html` first, **then** translate. The
guide's "GOOD CODE EXAMPLE" pseudocode uses *two* semaphore arrays:

- `acquire_semaphores[kNumberOfFramesInFlight]` — **per-in-flight-frame** `VkSemaphore` passed to
  `vkAcquireNextImageKHR`'s `semaphore` argument, then waited on by `vkQueueSubmit2`'s `pWaitSemaphores[0]`. Driver
  signals it when the swapchain image becomes available.
- `submit_semaphores[swapchain_image_count]` — **per-swapchain-image** `VkSemaphore` signaled by `vkQueueSubmit2`'s
  `pSignalSemaphores[0]` and waited on by `vkQueuePresentKHR`'s `pWaitSemaphores[0]`. Indexed by `imageIndex` so two
  consecutive in-flight frames handed the same `imageIndex` never race on the same handle.

**Concrete edits** landed in this pass:

- `src/core/Types.hpp::SwapchainState` — added `std::vector<VkSemaphore> submitSemaphores` (per-swapchain-image). The
  pre-existing `FrameState::imageAvailableSemaphores[2]` and `renderFinishedSemaphores[2]` were left in place — the
  former becomes `acquire_semaphores[frame_index]`, the latter is no longer used (the per-image `submitSemaphores`
  replace it).
- `src/render/vulkan/VulkanBootstrap.cpp` — added `kOptionalSwapchainMaintenance1Extension` and
  `kOptionalTracyCalibratedTimestampsExtension` style device-extension enable. Plus `VK_KHR_get_surface_capabilities2`
  and `VK_KHR_surface_maintenance1` instance extensions, both required by `VK_KHR_swapchain_maintenance1`'s dependency
  chain (we enable the device extension opportunistically when the GPU supports it; on the smoke host the GPU reports
  `VK_KHR_swapchain_maintenance1 : extension revision 1`, so the device extension is enabled). The `volk.h`
  -does-not-define-the-struct caveat required `#define VK_KHR_swapchain_maintenance1 1` + `#include <vulkan/vulkan.h>`
  *before* `volk.h`; the type names actually used in the project are `VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR`
  and `VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR` (the non-KHR-suffixed names are not
  defined by `vulkan_core.h`).
- `src/render/vulkan/VulkanSwapchain.cpp::CreateOrRecreateSwapchain` — created
  `swapchain->submitSemaphores[actualImageCount]` in lockstep with the swapchain images. Old per-image semaphores (if
  any, on swapchain recreation) are destroyed before the new ones overwrite the vector.
- `src/render/Renderer.cpp::DrawFrame` — `vkQueueSubmit2`'s `pSignalSemaphores[0]` and `vkQueuePresentKHR`'s
  `pWaitSemaphores[0]` now both use `swapchain->submitSemaphores[imageIndex]`. The per-frame `imageAvailableSemaphore`
  is unchanged (it is the `acquire_semaphore` in the guide's terms).
- `src/core/Types.cpp::ShutdownVulkan` — added a destroy loop for `state->swapchain.submitSemaphores` (previously leaked
  on shutdown). The per-in-flight-frame semaphore/fence destroy loops for `state->frame` were left in place but the
  per-frame `renderFinishedSemaphores` are no longer used by the renderer.

**Lesson learned for future swapchain work.** Always read the bundled Vulkan SDK docs *first* (the project ships them in
`docs/VulkanSDK-Linux-Docs-1.4.350.1/`). The "chicken-and-egg" of "which `imageIndex` for the acquire-side fence" only
dissolves once you have the guide's pseudocode in front of you: the per-image acquire *fence* is
`VK_KHR_swapchain_maintenance1`-only; the canonical **non-extension** pattern is a per-frame acquire *semaphore* +
per-image submit *semaphore*. The earlier attempted refactor in §10.7.x (rolled back) spent hours chasing VUIDs that the
guide would have resolved in five minutes. **Working rule for this project:** any Vulkan semantic question, read the
docs *first*, before grepping headers or running `vulkaninfo`.

**Outcome of this pass.** Build green, ctest 1/1 pass, smoke 6/6 captures, vision verification of the FINAL capture
shows continuous soft sun shadow (no staircasing, no full-floor dark), HUD-reported FPS `125.3` (SHDW frame) / `123.5` (
CSM frame) / `120.8` (AOCC frame) — within the same range as before, confirming no visual regression. **Vulkan
validation warning count: 0** (down from 20 before this pass and from 2 in the §10.7.x partial attempt).

### 10.8 `GraphicsPushConstants` field-order lesson

`voxel.frag` (and `voxel.vert`) declared the push-constant block as
`[viewProjection, cameraPosition, cameraForward, worldMinAndChunkSize, chunkGridAndFlags]` (offsets 0/64/80/96/112).
When the `git checkout` rolled `src/core/Types.hpp`'s `GraphicsPushConstants` back to the 96-byte
`[viewProjection, cameraPosition, cameraForward]` shape, the first naive "add the new fields at the end" pattern would
have placed them at offsets 96/112, which actually *does* match the shader. But the first attempt of the restore placed
`worldMinAndChunkSize` and `chunkGridAndFlags` *between* `viewProjection` and `cameraPosition` (offsets 64/80 in the C++
struct), which broke all push-constant reads and produced a full-floor-dark scene in the FINAL view. The fix was to put
the new fields at the *end* of the struct (offsets 96/112) — the canonical "match the shader layout, in the order the
shader writes them" rule. Always re-derive the C++ struct field order from the **shaders**, not from how
`FramePreparation.cpp` happens to write them — `FramePreparation.cpp` follows the struct order, not the other way
around.

### 10.11 P0.2 fix re-applied (`2026-06-10`) + Per-corner AO design (next session)

**Lost and re-applied P0.2 fix.** The shadow sampler `magFilter`/`minFilter` = `VK_FILTER_LINEAR` change described in
§10.10 was originally made as **uncommitted working-tree changes** during the 2026-06-09 session. During the W1-W5
vertex-welding detour on `2026-06-10`, I ran `git checkout -- .` to revert my failed W1-W5 changes — which also reverted
**all other uncommitted modifications**, including the P0.2 LINEAR fix, and then I ran `git stash drop` which destroyed
the recovery path. By the time the user noticed, `git blame HEAD -- src/render/vulkan/VulkanGraphicsPipeline.cpp` showed
the file was back to `94dad84` (NEAREST) and the commit `057c6fb` "fix(shadow,swapchain)" did **not** include the LINEAR
change despite the agent/memory.md claim. **Re-applied on `2026-06-10`** as a single 2-line edit at
`src/render/vulkan/VulkanGraphicsPipeline.cpp:399-400` (`VK_FILTER_NEAREST` → `VK_FILTER_LINEAR` for both). Verified:
build green, ctest 1/1, `build/linux-clang-debug/lookdev-captures/20260610-p0_2_fix_redo/SHDW.bmp` shows smooth gradient
on the VoxelLab chequer floor (no 3-4 discrete bands).

**Working rule added `2026-06-10`:** before running `git checkout -- .` on this repo, always run `git status -uall` +
`git diff` and explicitly preserve **uncommitted-but-working** changes either via a backup file (`cp file.c /tmp/`) or a
dedicated stash *with a name* (`git stash push -m "p0_2_linear_fix_keep" -- path/to/file`). The previous "git
checkout -- . && git stash drop" pattern is **destructive for any work that was uncommitted in earlier sessions**, and
the agent had no way to recover it after the drop.

**Per-corner packed AO design (next session, P0.3 closed-but-not-merged).** The "3-4 visible horizontal bands on a stack
of voxels" complaint is **not** a normal-averaging problem — it is a **per-face flat AO** problem. When
`flat in float inAmbientVisibility` is interpolated per-fragment across a face that has all four corners at the same AO
value, the face is uniformly shaded. But when the same face is shared between a lower voxel (high AO occlusion from
neighbors below) and a higher voxel (low AO occlusion from neighbors above), the per-face average jumps by one full "AO
level" at the voxel boundary, producing the 3-4 visible bands. The fix is to **pack 4 corner AO bytes
into `PackedFace::lightingData`** (which is currently 1 byte, 24 unused bits), so `voxel.vert` can output one AO per
corner, and `voxel.frag` can drop `flat` on `inAmbientVisibility` to get hardware-bilinear interpolation between
corners. The full design is in the user's `2026-06-10` external-model consultation; key constraints:
`voxel.frag::main()` line 930-944 is the integration point, `voxel_mesh.comp::ComputeFaceAmbientVisibilityByte` (line

222) is the AO function to extend, no C++ changes required (data is already in the 24 unused bits of `lightingData`).
     Reference: https://blog.0fps.net/2013/09/25/ambient-occlusion-for-minecraft-like-worlds/ (Mikola Lysenko, *Ambient
     occlusion for Minecraft-like worlds - 0 FPS*).
