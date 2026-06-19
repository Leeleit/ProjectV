# Changelog

All notable changes to ProjectV are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This CHANGELOG consolidates **refactor / bug-fix history** that previously lived as
`// **...**` comments in source files. Source comments now follow the project's
Doxygen convention (`/// \brief` + `/// \details`) and are generated into HTML by
`doxygen Doxyfile` per `docs/api/README.md`.

**For design rationale and ongoing decisions**, see `agent/decisions.md`.
**For session log / commit narrative**, see `agent/active-sessions.md` and `git log`.

---

## 2026-06-20

### Changed

- `src/voxel/VoxelWorld.cpp:176` — Tier 0.D hardening.** `[[unlikely]]` on the
  `chunk.rebuildQueued` early-out in `QueueChunkRebuildRequest`. The branch
  predictor (and the branch hint) target the common path "chunk fresh,
  process rebuild request"; the early-return is rare and correctly marked.

### Verified (Tier 0.D audit, no code change)

- `src/voxel/VoxelWorld.cpp:366,1162` — `pendingChunkRebuildIndices.reserve(world->chunks.size())`
  on scene-load and `MarkAllVoxelChunksDirty`. Capacity matches `chunks.size()`,
  bounded — no realloc during voxel-edit `QueueChunkRebuildRequest` calls.
- `src/render/SceneResources.cpp:1074,1076,1078,1129,1162` — `latestVoxelPayloadChunkIndices`,
  `completedChunkRebuildIndices`, `pendingChunkRebuildIndices` reserved at scene rebuild
  and snapshot restore boundaries. No per-frame realloc in steady state.
- `src/render/VoxelMeshingPushConstants.hpp:32-37` — `ChunkVisibilityCache` uses
  `std::array<VkDrawIndirectCommand, kChunkVisibilityCacheMaxChunks>` (fixed-cap,
  1024 entries). No `std::vector` here — `std::inplace_vector` (Tier 1.A) not needed.
- `src/debug/DebugOverlays.cpp:224-226` — `debugOverlayBoxes.reserve(requiredBoxCount)`
  before the per-frame build loop. Capacity grows lazily only when `requiredBoxCount`
  exceeds previous capacity.
- `src/app/InputReplay.cpp:176,278` — `InputReplayCapture.frames.reserve(expectedFrameCount)`
  on load; `capture.frames.reserve(512)` on `StartInputReplayRecording` start. Recording
  amortizes O(1) push_back per frame even past 512 frames (geometric growth).
- `src/app/FramePreparation.cpp:30,45` — local `cullCandidates.reserve(modelInstances.size())`
  and `visibleModelInstances.reserve(visible.size())` before push_back loops.
- `src/voxel/VoxelWorld.cpp:1068` — local BFS `queue.reserve(256)` in `FillVoxelMaterial`.

---

## [Unreleased] — pre-2026-06-12 / un-dated history

### Added

- `src/c_kernels/FrustumCulling.cpp:76` — Scalar C kernel.** Per the Tier 3 benchmark, this
- `src/c_kernels/FrustumCulling.hpp:67` — Culls `instances` against `parameters` in a single
- `tests/CFrustumCullingTests.cpp:73` — 300 AABBs in a 32x32 chunk grid centred on
- `tests/CFrustumCullingTests.cpp:101` — 5 visibility runs** with the camera at

### Changed

- `src/app/AppUpdate.cpp:156` — P0.3 follow-up: any (re-)enable of relative mouse mode risks a large
- `src/app/Camera.cpp:109` — P0.3 follow-up: drop the first MOUSE_MOTION event after relative
- `src/app/InputReplay.cpp:96` — Tier 5.** `0ull` (was `0u`) — the mask type
- `src/app/main.cpp:358` — Tier 5.** `0ull` (was `0u`) — the mask type
- `src/core/InplaceVectorShim.hpp:57` — No copy/move ctors / assignment.** Matches
- `src/render/SceneResources.hpp:87` — Tier 5.** `[[unlikely]]` on the near-plane cull.
- `src/render/SceneResources.hpp:132` — Tier 5.** `[[unlikely]]` on the 4 side-plane
- `src/render/ShadowProjection.cpp:87` — Tier 0.B.** `matrix` is now `projectv::math::Mat4` (16-byte
- `src/render/ShadowProjection.hpp:18` — Tier 0.B.** `Vec3` (16-byte aligned) replaces
- `src/render/ShadowProjection.hpp:32` — Tier 0.B.** `lightViewProjections` stays `std::array<float, N>`
- `src/shaders/voxel_mesh.comp:290` — P0.3 follow-up: the parameters are intentionally unused here. Touch
- `src/shaders/voxel_mesh.comp:290` — P0.3 follow-up: the parameters are intentionally unused here. Touch
- `src/voxel/VoxelWorld.hpp:109` — (Tier 1.E: replaced by `ParseVoxelScenePreset(std::string_view)` below
- `tests/VoxelWorldTests.cpp:366` — Tier 1.E.** `VoxelScenePresetToString` now returns
- `tests/VoxelWorldTests.cpp:1697` — Tier 5.** `1ull <<` (not `1u <<`) — the
- `tests/VoxelWorldTests.cpp:2494` — Tier 5.** `1ull <<` (was `1u <<`) — the mask

### Removed

- `src/render/vulkan/VulkanGraphicsPipeline.cpp:77` — P0.3 follow-up (final): per-vertex AO was removed (see voxel.vert
- `src/shaders/voxel.frag:81` — P0.3: inAmbientVisibility is no longer `flat` so the rasterizer bilinearly
- `src/shaders/voxel.frag:81` — P0.3: inAmbientVisibility is no longer `flat` so the rasterizer bilinearly
- `src/shaders/voxel_mesh.comp:275` — P0.3 follow-up: ComputeFaceCornerPackedAO is now a no-op. The vertex
- `src/shaders/voxel_mesh.comp:275` — P0.3 follow-up: ComputeFaceCornerPackedAO is now a no-op. The vertex

---

## 2026-06-18

### Fixed

- `src/asset/AssetLoader.cpp:400` — Windows STL portability (`2026-06-18`,
- `src/asset/ModelPass.cpp:94` — Windows clang-cl portability (`2026-06-18`,
- `src/core/StringId_fallback.hpp:3` — StringID fallback header for Windows clang-cl
- `tests/FrustumCullingTests.cpp:47` — Windows portability (`2026-06-18`,
- `tests/FrustumCullingTests.cpp:148` — Windows portability (`2026-06-18`,

### Removed

- `src/core/Math_fallback.hpp:3` — Math fallback header for Windows clang-cl

---

## 2026-06-15

### Changed

- `src/app/main.cpp:90` — Cross-platform log path (`2026-06-15`).** The previous
- `src/voxel/SceneConfig.cpp:37` — Repo-root walk-up (`2026-06-15`).** Per

### Fixed

- `src/app/main.cpp:36` — Hot shader reload fallback (`2026-06-15`).** If the
- `src/app/main.cpp:81` — Cross-platform fallback (`2026-06-15`).** Per
- `src/core/RepoRoot.hpp:1` — Repo root discovery (`2026-06-15`).

---

## 2026-06-14

### Added

- `src/app/Camera.cpp:120` — Window-event mouse freeze (`2026-06-14`).** After
- `src/core/Types.hpp:1403` — Window-event mouse freeze (`2026-06-14`).** When the WM
- `src/render/vulkan/VulkanSwapchain.hpp:182` — Build the runtime present-mode cycle
- `tests/PresentModeTests.cpp:1` — Present-mode cycle tests (auto-detect cycle,
- `tests/PresentModeTests.cpp:211` — 2026-06-14 fix:** explicitly reset `g_active` to
- `tests/PresentModeTests.cpp:288` — Cycle rebuild preserves `g_active` (2026-06-14 fix).

### Changed

- `src/app/Camera.cpp:132` — Fullscreen / resize defence-in-depth (`2026-06-14`).
- `src/app/main.cpp:634` — Fullscreen / window-resize mouse guard (`2026-06-14`).
- `src/core/Types.hpp:1375` — Fluid CA accumulator (2026-06-14).** Wall-clock-free,
- `src/debug/DebugHud.cpp:553` — 2026-06-14: VSync line.** Shows the current
- `src/render/vulkan/VulkanSwapchain.cpp:259` — 2026-06-14: build the runtime present-mode cycle
- `src/render/vulkan/VulkanSwapchain.hpp:88` — Build the runtime present-mode cycle (`2026-06-14`).
- `src/render/vulkan/VulkanSwapchain.hpp:111` — Read-only accessors for the HUD (`2026-06-14`).** The
- `src/render/vulkan/VulkanSwapchain.hpp:159` — V-sync toggle (auto-detect cycle on `2026-06-14`).
- `tests/FluidCATests.cpp:498` — `stats.fluidVoxelCount` stays in sync with `world.voxels`
- `tests/FluidCATests.cpp:541` — Run enough ticks for the column to spread out. With the
- `tests/FluidCATests.cpp:592` — Run enough ticks for the column to settle. With the

### Fixed

- `src/app/AppUpdate.cpp:694` — Fluid CA tick (2026-06-14).** Mirrors the physics-
- `src/app/main.cpp:697` — Fluid CA tick — moved to `UpdateApp` on 2026-06-14.
- `src/core/Types.hpp:1357` — Fluid CA tick rate (2026-06-14).** Live, in-Hz, applied
- `src/render/vulkan/VulkanSwapchain.cpp:160` — 2026-06-14 fix.** Honour the operator's `V`-hotkey
- `src/voxel/VoxelWorld.cpp:1504` — Strict count conservation (2026-06-14).** The
- `tests/FluidCATests.cpp:414` — Strict count conservation (2026-06-14 fix).** Earlier
- `tests/FluidCATests.cpp:569` — Fluid column over Air + platform: platform stays intact,
- `tests/FluidCATests.cpp:765` — ---------------------------------------------------------------------------
- `tests/FluidCATests.cpp:819` — Fluid does NOT move when paused (2026-06-14 invariant).
- `tests/PresentModeTests.cpp:303` — 2026-06-14 fix:** explicit reset to FIFO before
- `tests/PresentModeTests.cpp:336` — 2026-06-14 fix:** explicit reset to FIFO before
- `tests/PresentModeTests.cpp:376` — 2026-06-14 fix:** explicit reset to FIFO before

---

## 2026-06-13

### Added

- `src/app/Camera.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/app/FramePreparation.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/app/FramePreparation.cpp:63` — Tier 4 (`2026-06-13`).** C / AVX2 frustum-cull
- `src/app/main.cpp:545` — Defense r0 hotkeys (2026-06-13, relocated twice 2026-06-15).** Originally
- `src/app/main.cpp:583` — V-sync toggle (`2026-06-13`, auto-detect
- `src/audio/AudioEngine.cpp:38` — Artist / title parser, 2026-06-13.** See
- `src/audio/AudioEngine.cpp:159` — Tier 1.B (`2026-06-13`).** `std::expected<size_t,
- `src/audio/AudioEngine.cpp:350` — Cursor semantics 2026-06-13 (two
- `src/bench/FrustumCullBenchmark.cpp:1` — Tier 3 (`2026-06-13`).** `FrustumCullBenchmark` —
- `src/bench/ShadowProjectionBenchmark.cpp:1` — Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `src/c_kernels/FrustumCulling.hpp:1` — Tier 4 (`2026-06-13`).** C++ wrapper for the
- `src/c_kernels/frustum_cull.c:1` — Tier 3 (`2026-06-13`).** `frustum_cull.c` — scalar
- `src/core/InplaceVectorShim.hpp:3` — libc++ migration shim (`2026-06-13`).** libc++ 22 (the
- `src/core/Math.hpp:51` — Original Tier 0.A doc preserved below for git-blame
- `src/core/Types.hpp:61` — libc++ migration shim (`2026-06-13`).** `std::inplace_vector`
- `src/debug/DebugHud.cpp:689` — Music HUD moved out, 2026-06-13.** The
- `src/render/Renderer.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/render/SceneResources.cpp:449` — Tier 1.A (`2026-06-13`).** `ChunkVisibilityCache` now uses
- `src/render/ShadowProjection.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/render/vulkan/VulkanInit.hpp:63` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.hpp:49` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.hpp:70` — V-sync toggle hotkey (`2026-06-13`, auto-detect cycle
- `src/voxel/VoxelWorld.cpp:705` — Tier 1.E (`2026-06-13`).** `constexpr std::string_view` return
- `src/voxel/VoxelWorld.cpp:829` — Tier 1.B (`2026-06-13`).** Returns `std::expected<bool,
- `src/voxel/VoxelWorld.cpp:1576` — 3. Commit the new state through the public `SetVoxelMaterial`
- `src/voxel/VoxelWorld.hpp:153` — Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13;
- `tests/CFrustumCullingTests.cpp:1` — Tier 4 (`2026-06-13`).** Unit test for the
- `tests/ModuleSmokeTest.cpp:1` — Tier 2.A probe (`2026-06-13`).** Tiny TU that
- `tests/PresentModeTests.cpp:152` — Modes appear in the surface support in a different
- `tests/ProbeTest.cpp:1` — Tier 2.A probe (`2026-06-13`).** Tiny TU that imports
- `tests/StdModuleProbe.cpp:1` — Tier 2.C probe (`2026-06-13`).** Tiny TU that
- `tests/VoxelWorldTests.cpp:88` — Tier 0 (`2026-06-13`) signature change:

### Changed

- `src/app/AppUpdate.cpp:885` — Music HUD mirrors, 2026-06-13.
- `src/app/InputActions.cpp:261` — Tier 5 (`2026-06-13`).** `uint64_t` return type
- `src/app/InputActions.hpp:15` — Tier 5 (`2026-06-13`).** `uint64_t` mask type.
- `src/app/InputReplay.cpp:151` — Tier 5 (`2026-06-13`).** Accept v1, v2, and v3
- `src/app/InputReplay.cpp:316` — Tier 1.B (`2026-06-13`).** `std::expected` carries the
- `src/app/main.cpp:61` — Hot shader reload (defense r0, 2026-06-13).** F5 re-runs `cmake --build`
- `src/app/main.cpp:233` — Tier 1.B (`2026-06-13`).** `std::expected` returns the
- `src/app/main.cpp:262` — Tier 1.B (`2026-06-13`).** `std::expected` returns the
- `src/app/main.cpp:316` — Tier 1.B (`2026-06-13`).** `std::expected` carries the
- `src/app/main.cpp:411` — Tier 1.B (`2026-06-13`).** `InitVulkan` now returns
- `src/app/main.cpp:477` — Scene config (defense r0, 2026-06-13).** Read the JSON
- `src/asset/AssetManifest.cpp:122` — Tier 1.D/E (`2026-06-13`).** `ManifestEntry::id` is now a
- `src/asset/AssetManifest.hpp:14` — Tier 1.D/E (`2026-06-13`).** Replaced `std::string` with
- `src/asset/AssetRegistry.cpp:10` — Tier 1.B (`2026-06-13`).** `std::expected<const LoadedAsset *,
- `src/asset/AssetRegistry.hpp:16` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/asset/ModelManifestLoader.cpp:35` — Tier 0.B (`2026-06-13`).** Takes the destination as a
- `src/asset/ModelManifestLoader.cpp:187` — Tier 0.D (`2026-06-13`).** Reserve once before the loop so
- `src/audio/AudioEngine.cpp:525` — Cursor reset, 2026-06-13.** E is the
- `src/audio/AudioEngine.hpp:29` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/audio/AudioEngine.hpp:53` — Artist / title parser, 2026-06-13.** Splits a
- `src/audio/AudioEngine.hpp:113` — Set the music folder and trigger the first
- `src/audio/AudioEngine.hpp:180` — Artist / title for the HUD, 2026-06-13.
- `src/audio/AudioEngine.hpp:191` — Playback position / duration, 2026-06-13.
- `src/audio/AudioEngine.hpp:241` — Artist / title cache updater, 2026-06-13.
- `src/audio/AudioEngine.hpp:267` — Dead code, 2026-06-13.** Saved cursor
- `src/audio/AudioEngine.hpp:295` — Artist / title cache, 2026-06-13.** Parsed
- `src/c_kernels/FrustumCulling.cpp:1` — Tier 4 (`2026-06-13`).** C++ wrapper implementation
- `src/c_kernels/frustum_cull.hpp:1` — Tier 3 (`2026-06-13`).** C / intrinsics
- `src/core/Math.ixx:156` — libc++ migration / `import` regression (`2026-06-13`).
- `src/core/Math.ixx:365` — Cast helpers** from `glm::mat4` / `float[N]` to
- `src/core/StringId.ixx:1` — Tier 2.A — `StringId.ixx` (`2026-06-13`).** C++20
- `src/core/Types.hpp:343` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned
- `src/core/Types.hpp:371` — Matches the `ResolvePushConstants` struct declared in
- `src/core/Types.hpp:404` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/core/Types.hpp:483` — Tier 5 (`2026-06-13`).** Widened from `uint32_t`
- `src/core/Types.hpp:610` — Tier 0.B (`2026-06-13`).** Four `Vec4`s (16-byte aligned
- `src/core/Types.hpp:720` — Music HUD mirrors, 2026-06-13.** Parsed
- `src/core/Types.hpp:912` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/core/Types.hpp:939` — Tier 1.D/E (`2026-06-13`).** Replaced `std::string` with
- `src/core/Types.hpp:1121` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned)
- `src/debug/DebugHud.cpp:28` — Music panel, 2026-06-13.** Minimum width
- `src/debug/DebugHud.cpp:39` — Music panel cap, 2026-06-13.** Four lines
- `src/debug/DebugHud.cpp:60` — mm:ss formatter, 2026-06-13.** Used by the
- `src/debug/DebugHud.cpp:95` — HUD uppercase helper, 2026-06-13.** Copies
- `src/debug/DebugHud.cpp:1030` — Music HUD block builder, 2026-06-13.** Emits
- `src/debug/DebugHud.cpp:1252` — Music panel color, 2026-06-13.** Slightly
- `src/debug/DebugHud.cpp:1270` — Music lines, 2026-06-13.** Built and
- `src/debug/DebugHud.cpp:1289` — Music panel anchored at top-right,
- `src/render/RayMarchPass.cpp:11` — State for the ray-march pass (defense r0, 2026-06-13).** A single
- `src/render/RayMarchPass.cpp:69` — Phase 7 follow-up (defense r0, 2026-06-13).** The full Vulkan
- `src/render/RayMarchPass.hpp:9` — Ray-march compute pass (defense r0, 2026-06-13).** A second, optional
- `src/render/SceneResources.cpp:39` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/render/SceneResources.cpp:433` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) per
- `src/render/SceneResources.hpp:21` — Tier 5 (`2026-06-13`).** `[[unlikely]]` on the
- `src/render/ShadowProjection.hpp:9` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/render/TaaRenderTargets.cpp:76` — Tier 1.B (`2026-06-13`).** Returns `std::expected<void, TaaError>`.
- `src/render/TaaRenderTargets.hpp:27` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1696` — Fluid (2026-06-13 follow-up):** disable back-face culling for
- `src/render/vulkan/VulkanInit.cpp:128` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanInit.hpp:11` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/render/vulkan/VulkanSwapchain.cpp:219` — Tier 1.B (`2026-06-13`).** Moved `CreateOrRecreateSwapchain` to
- `src/render/vulkan/VulkanSwapchain.cpp:229` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.cpp:516` — Tier 1.B (`2026-06-13`).** `CreateOrRecreateSwapchain`
- `src/render/vulkan/VulkanSwapchain.cpp:564` — Tier 1.B (`2026-06-13`).** `std::expected<void, TaaError>`
- `src/render/vulkan/VulkanSwapchain.hpp:13` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/shaders/ray_march.comp:3` — Ray-march compute shader (defense r0, 2026-06-13).** Implements the
- `src/shaders/ray_march.comp:3` — Ray-march compute shader (defense r0, 2026-06-13).** Implements the
- `src/shaders/voxel_mesh.comp:190` — Fluid (2026-06-13 follow-up): emit against ALL non-Air
- `src/shaders/voxel_mesh.comp:190` — Fluid (2026-06-13 follow-up): emit against ALL non-Air
- `src/voxel/SceneConfig.hpp:11` — Scene configuration (defense r0, 2026-06-13).** Runtime-readable
- `src/voxel/VoxelMaterials.hpp:116` — previous frame's viewProjection (column-major, same layout as
- `src/voxel/VoxelWorld.cpp:673` — Tier 1.E (`2026-06-13`).** Replaced `bool TryParse(..., &out)`
- `src/voxel/VoxelWorld.cpp:755` — Tier 1.E (`2026-06-13`).** `ParseVoxelScenePreset` returns
- `src/voxel/VoxelWorld.hpp:117` — Tier 1.B (`2026-06-13`).** `std::expected<T, VoxelSnapshotError>`
- `src/voxel/VoxelWorld.hpp:125` — Tier 1.E (`2026-06-13`).** `VoxelScenePresetToString` returns
- `tests/FluidCATests.cpp:1` — Fluid CA unit tests (audit `2026-06-13`).
- `tests/FluidCATests.cpp:213` — Run many ticks. With the 2026-06-13 2-direction spread
- `tests/FluidCATests.cpp:228` — Fluid stops falling when it hits a solid floor.** A fluid at
- `tests/FluidCATests.cpp:283` — Before break:** glass is solid, fluid cannot fall through.
- `tests/FluidCATests.cpp:356` — First tick: fluid at (2, 1, 2) cannot fall through the
- `tests/FluidCATests.cpp:377` — Fluid on a flat surface (where the fall rule can't fire —
- `tests/FluidCATests.cpp:754` — Subsequent ticks:** the fluid continues to fall and spread.
- `tests/MathTest.cpp:1` — Tier 0.A — r0 hardcore perf (`2026-06-13`).** Smoke test for
- `tests/StringIdTest.cpp:1` — Tier 1.D (`2026-06-13`).** Tests for `projectv::core::StringID`.
- `tests/SunShadowCascadeSplitsTests.cpp:1` — Tier 5 (`2026-06-13`).** Unit test for
- `tests/VoxelWorldTests.cpp:351` — Tier 1.E (`2026-06-13`).** `TryParseVoxelScenePreset(text, &out)`
- `tests/VoxelWorldTests.cpp:1003` — Tier 0.B (`2026-06-13`).** `SunShadowProjection::lightViewProjection`
- `tests/VoxelWorldTests.cpp:1131` — Tier 0.B (`2026-06-13`).** `lightViewProjections` (the
- `tests/VoxelWorldTests.cpp:2197` — Tier 0 (`2026-06-13`) signature change:

### Fixed

- `src/app/InputReplay.cpp:15` — Tier 5 (`2026-06-13`).** Bumped from 2 to 3.
- `src/audio/AudioEngine.cpp:493` — Cursor preservation, 2026-06-13.
- `src/core/Types.hpp:273` — Tier 5 (`2026-06-13`).** Bit-mask overflow fix.
- `src/core/Types.hpp:565` — Tier 1.A (`2026-06-13`).** `std::inplace_vector` (P0843, C++26)
- `src/core/Types.hpp:781` — Tier 5 follow-up (`2026-06-13`).** Default 0.0
- `src/core/Types.hpp:1128` — Per-pass TAA tuning ladder (live `;`/`'`/`-`/`=`/`,`/`.` ladder, see
- `src/render/Renderer.cpp:239` — Tier 5 (`2026-06-13`).** Tight fence-wait
- `src/render/SceneResources.cpp:723` — Pre-existing race fix (`2026-06-13`).** Wait for
- `src/render/TaaRenderTargets.hpp:115` — Build / recreate the TAA offscreen color images (colour +
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1253` — TAA resolve descriptor update (`2026-06-13`).** The
- `src/render/vulkan/VulkanSwapchain.cpp:98` — V-sync toggle (`2026-06-13`).** Live-cycled by the
- `src/shaders/taa_resolve.frag:261` — Reconstruct the world position of the *current* pixel from its
- `src/shaders/taa_resolve.frag:261` — Reconstruct the world position of the *current* pixel from its
- `src/shaders/voxel.frag:722` — Tremor fix (`2026-06-13`).** Vulkan's
- `src/shaders/voxel.frag:722` — Tremor fix (`2026-06-13`).** Vulkan's
- `src/shaders/voxel.frag:944` — TAA layer-history reprojection (`2026-06-13`).** The
- `src/shaders/voxel.frag:944` — TAA layer-history reprojection (`2026-06-13`).** The
- `src/voxel/VoxelSnapshotError.hpp:3` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/voxel/VoxelWorld.cpp:895` — Tier 1.B (`2026-06-13`).** Returns `std::expected<unique_ptr<VoxelWorld>,
- `src/voxel/VoxelWorld.cpp:1278` — Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13).
- `src/voxel/VoxelWorld.cpp:1337` — Claimed tracking (2026-06-13 spread restore).** When a fluid
- `src/voxel/VoxelWorld.cpp:1380` — f_fall rule:** if the cell below is `Air`,
- `src/voxel/VoxelWorld.cpp:1442` — f_spread rule (2-direction perpendicular):** if
- `tests/FluidCATests.cpp:335` — Fluid on a `FloorWhite` cell does NOT fall through (the
- `tests/FluidCATests.cpp:729` — Before break:** fluid on glass → no fall. The fluid may

### Removed

- `src/audio/AudioEngine.cpp:430` — Bug fix 2026-06-13.** Previously this
- `src/core/Math.hpp:3` — Tier 2.D (`2026-06-13`).** This header is now a **forwarding
- `src/core/Math.ixx:1` — Tier 2.A — `Math.ixx` (`2026-06-13`).** C++20 module
- `src/core/StringId.hpp:3` — Tier 2.D (`2026-06-13`).** This header is a **forwarding
- `src/core/Types.hpp:3` — libc++ migration debug (`2026-06-13`).** Re-enabled
- `src/render/Renderer.cpp:30` — `BuildGraphicsPushConstants` is non-singular for any sensible near/far
- `src/render/Renderer.cpp:96` — Tier 0.B (`2026-06-13`).** The local `InvertColumnMajorMat4` is
- `tests/FluidCATests.cpp:158` — A column of N fluid voxels over Air "percolates" downward in

---

## 2026-06-12

### Added

- `src/app/ModelGravigun.cpp:261` — Drag.** Cast a ray from the camera through the
- `src/asset/ModelManifestLoader.cpp:134` — Source AABB in world space (transformed, not yet frustum-culled;
- `src/asset/ModelManifestLoader.hpp:28` — Voxel-grid "ground snap" for the loaded `modelInstances`. The
- `src/asset/ModelManifestLoader.hpp:61` — M5.1d, 2026-06-12: centred snap (opt-in).** Same
- `src/audio/AudioEngine.hpp:3` — Audio engine, 2026-06-12.** Thin wrapper over
- `src/core/Types.hpp:1285` — 5.3 Benchmark automation, 2026-06-12.** Tracks per-frame timings
- `src/debug/DebugHud.cpp:48` — Helper-panel cap, 2026-06-12 bump.** Was 16
- `src/render/ScreenshotCapture.cpp:421` — Audio engine keys, 2026-06-12.** Three

### Changed

- `src/app/AppUpdate.cpp:486` — Frame-step / slow-motion debug (2026-06-12). The
- `src/app/AppUpdate.cpp:573` — Audio engine handlers, 2026-06-12.** All four
- `src/app/AppUpdate.cpp:842` — Per-pass CPU timing mirrors (2026-06-12). Each
- `src/app/InputActions.cpp:171` — Frame-step / slow-motion debug (2026-06-12). `[` / `]`
- `src/app/InputActions.cpp:183` — Audio engine, 2026-06-12.** Music
- `src/app/InputActions.cpp:200` — Track switching, 2026-06-12.** `9` and `0`
- `src/app/ModelGravigun.cpp:94` — M5.1d, 2026-06-12:** the gravigun's default behaviour
- `src/app/ModelGravigun.cpp:221` — Drop** the picked model. The default behaviour
- `src/app/ModelGravigun.hpp:69` — Horizontal plane the cursor projects onto during drag.
- `src/app/ModelGravigun.hpp:80` — Pick anchor, 2026-06-12:** the AABB min of the
- `src/app/main.cpp:424` — Audio engine, 2026-06-12.** Init is non-fatal
- `src/asset/ModelManifestLoader.cpp:483` — Centred snap, 2026-06-12.** Same clamp-to-world + snap-
- `src/asset/ModelManifestLoader.cpp:582` — Dispatcher (M5.1d, 2026-06-12). The default snap is
- `src/asset/ModelManifestLoader.hpp:98` — M5.1d, 2026-06-12: snap dispatch wrapper.** Reads the
- `src/audio/AudioEngine.hpp:69` — Playback state, 2026-06-12.** Three-valued enum so
- `src/audio/MusicDirectoryPath.hpp:7` — Music folder resolution, 2026-06-12.** Mirrors the
- `src/core/Types.hpp:191` — M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
- `src/core/Types.hpp:204` — 5.2 Debug gizmos, 2026-06-12:** `L` cycles a world-aligned
- `src/core/Types.hpp:215` — 5.2 Debug gizmos, 2026-06-12:** `Z` toggles a 1-voxel-wide
- `src/core/Types.hpp:237` — Audio engine, 2026-06-12.** Music player
- `src/core/Types.hpp:251` — Track switching, 2026-06-12.** Next /
- `src/core/Types.hpp:530` — Two-level chunk visibility cache (2026-06-12).** When the
- `src/core/Types.hpp:675` — Frame-step / slow-motion mirrors, 2026-06-12.** See
- `src/core/Types.hpp:687` — Per-pass CPU timing mirrors (2026-06-12). Source fields
- `src/core/Types.hpp:703` — Audio engine mirrors, 2026-06-12.** Source of
- `src/core/Types.hpp:954` — Per-pass CPU timing, 2026-06-12.** Wall-clock
- `src/core/Types.hpp:1261` — The model pipeline reuses the main `graphicsPipelineLayout`
- `src/core/Types.hpp:1514` — Audio engine, 2026-06-12.** Single
- `src/debug/DebugHud.cpp:707` — Per-pass CPU timing lines (2026-06-12). Two-line
- `src/debug/DebugHud.cpp:1146` — Movement / speed, 2026-06-12.** WASD
- `src/debug/DebugHud.cpp:1170` — Debug gizmos, 2026-06-12.** `L`
- `src/debug/DebugHud.cpp:1204` — Audio engine, 2026-06-12.** Music
- `src/debug/DebugHud.cpp:1214` — Track switching, 2026-06-12.** `9` =
- `src/render/SceneResources.cpp:404` — Two-level cache rebuild path (2026-06-12).** Per-chunk
- `src/render/SceneResources.cpp:511` — Two-level cache hit path (2026-06-12).** Copies the cached
- `src/render/SceneResources.cpp:595` — Two-level cache check, 2026-06-12.** The hash folds
- `src/render/SceneResources.cpp:627` — Cache miss path (2026-06-12).** Run the canonical
- `src/render/SceneResources.hpp:346` — Two-level chunk visibility cache, 2026-06-12.** The hash
- `src/render/ScreenshotCapture.cpp:397` — Per-pass CPU timing keys (2026-06-12). Split into
- `src/shaders/taa_resolve.frag:76` — Color-distance rejection threshold in YCoCg space. When the current
- `src/shaders/taa_resolve.frag:76` — Color-distance rejection threshold in YCoCg space. When the current
- `src/voxel/VoxelWorld.cpp:328` — Floor bounds (M5.1d, 2026-06-12):** the XZ extent of
- `src/voxel/VoxelWorld.hpp:81` — Floor bounds (M5.1d, 2026-06-12):** the XZ extent of

### Fixed

- `src/app/AppUpdate.cpp:640` — Frame-step / slow-motion accumulator override
- `src/app/AppUpdate.cpp:863` — Audio engine mirrors, 2026-06-12.** Source
- `src/app/ModelGravigun.hpp:8` — M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
- `src/asset/AssetLoader.cpp:257` — Walk the default scene's node hierarchy in DFS order, applying
- `src/asset/ModelManifestLoader.cpp:274` — M5.1d, 2026-06-12: per-axis smart snap (replaces M5.1c
- `src/asset/ModelManifestLoader.cpp:371` — X axis.** Two valid placements per axis:
- `src/core/Types.hpp:221` — Frame-step / slow-motion debug, 2026-06-12:** live time-scale
- `src/core/Types.hpp:1332` — Frame-step / slow-motion debug, 2026-06-12.** Live
- `src/core/Types.hpp:1345` — Frame-step request, 2026-06-12.** One-shot flag. Set by

---

## 2026-06-11

### Changed

- `src/asset/AssetManifest.hpp:31` — Parses a manifest string of the form

### Fixed

- `src/app/FramePreparation.cpp:261` — TAA jitter: advance the 8-tap Halton(2,3) sub-pixel sequence and stash

---

## 2026-06-09

### Changed

- `src/shaders/voxel.frag:117` — AOCC and local-light DDA caps were tuned down from 6 and 32 to 4 and 12 after a
- `src/shaders/voxel.frag:117` — AOCC and local-light DDA caps were tuned down from 6 and 32 to 4 and 12 after a

---


## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:153` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:137` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:137` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:162` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:172` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:187` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:228` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:244` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:289` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:162` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:172` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:187` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:228` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:244` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:289` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:65` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:167` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:177` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:192` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:233` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:249` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:294` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:25` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:223` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:232` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:349` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:382` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:430` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:513` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:562` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:584` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:643` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:713` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
