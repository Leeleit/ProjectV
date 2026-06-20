# Changelog

All notable changes to ProjectV are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This CHANGELOG consolidates **refactor / bug-fix history** that previously lived as
`// **...**` comments in source files. Source comments now follow the project's
Doxygen convention (`/// \brief` + `/// \details`) and are generated into HTML by
`doxygen Doxyfile` per `docs/api/README.md`.

**For design rationale and ongoing decisions**, see `agent/knowledge.md Part A`.
**For session log / commit narrative**, see `agent/workspace.md §5 (Active tasks)` and `git log`.

---

## 2026-06-20 (session: 2x scope continuation, part 2)

### Stage 1.1 chunk 7 — homogeneous optimization

- `src/voxel/Sparse64Tree.hpp` — added `kSparse64HomogeneousFlag = 0x40000000u` slot encoding. A homogeneous marker
  encodes "all 64 child cells have the same material" as a single uint32 slot, saving memory for flat-color
  regions (walls, floors). New helpers: `MakeSparse64Homogeneous(material)`, `IsSparse64Homogeneous(slot)`,
  `Sparse64HomogeneousMaterial(slot)`. Slot node-index mask changed from `0x7FFFFFFF` to `0x3FFFFFFF` (30 bits).
- `SetCellRecursive` checks `CanCollapseToHomogeneous(node)` after each mutation — if all 64 child slots
  are now the same material (leaf or homogeneous marker), the node is replaced with a homogeneous marker
  (refCount decremented). Cascade naturally propagates upward.
- Early-out for "same material" SetCell on leaf or homogeneous slots (no node allocation).
- Leaf expansion path now sets `fillMask = full` (sub-volume fully populated), so re-collapse works after
  revert edits.
- `LiveNodeCount()` helper added (counts nodes with `refCount > 0`) — `NodeCount()` keeps old "allocated"
  semantics for snapshot save/load.
- `IsHomogeneousRoot()` public helper.
- `tests/Sparse64TreeTests.cpp` — 5 new sub-tests: `TestHomogeneousCollapseSmall` (4×4×4), `TestHomogeneousExpansionOnSingleEdit`,
  `TestHomogeneousCascadeSixteen` (16×16×16 full cascade), `TestHomogeneousRecollapseAfterEdit`,
  `TestHomogeneousDedupEqual`.

### Stage 1.2 chunks 4-5 — per-chunk static flag + lazy promotion

- `src/voxel/VoxelWorld.hpp` — `VoxelChunk` gained `bool isStatic = false;` + `uint32_t ticksSinceLastEdit = 0;`
  (struct size 32 → 36 bytes, static_asserts updated).
- `src/voxel/VoxelWorld.{hpp,cpp}` — new API: `GetVoxelChunkStaticPromotionThreshold()` (env `PROJECTV_SVDAG_STATIC_PROMOTION_TICKS`,
  default 60), `TickVoxelChunkStaticPromotion(world, threshold)` (per-frame increment + auto-promote + DedupPass trigger
  if dedup enabled), `CountStaticVoxelChunks(world)`.
- `SetVoxelMaterial` resets `isStatic = false; ticksSinceLastEdit = 0;` on every voxel edit (auto-demote).
- `tests/VoxelWorldTests.cpp` — 2 new sub-tests: `TestVoxelChunkStaticPromotion`, `TestVoxelChunkStaticPromotionThresholdFromEnv`.

### Stage 2.1 chunk 2 — task+mesh shader spike (nvidia-only)

- `src/shaders/voxel_mesh.task` — new task shader. Frustum cull per chunk (6-plane AABB test),
  `EmitMeshTasksEXT(visible_count, 1, 1)` to dispatch mesh shader. Reads `ChunkDescriptor` SSBO + push-constant
  frustum planes. Uses `GL_EXT_mesh_shader` extension. Compiles under `vulkan1.3` target env.
- `src/shaders/voxel_mesh.mesh` — new mesh shader. Stub: per visible chunk, emits 1 triangle at chunk origin
  (NDC positions). Real greedy mesh emission deferred (multi-session work).
- `src/CMakeLists.txt` — `--target-env vulkan1.3` flag added to shader compiler args (required for mesh shaders).
- CMake registration for both new shader files. Compiles successfully, integration into `Renderer.cpp` deferred.

### Stage 2.2 chunk 2 — HZB image lifecycle (spike)

- `src/render/HizCulling.{hpp,cpp}` — real Vulkan lifecycle for `HizBuffer`. `CreateHizBuffer(context, w, h, outBuffer)`
  allocates image + image view via VMA, `DestroyHizBuffer(context, buffer)` releases. `BuildHizMipChain(commandBuffer,
  depthImage, layout, hizBuffer)` builds mip chain via `vkCmdBlitImage` with `VK_FILTER_NEAREST` (level 0 from depth)
  and `VK_FILTER_LINEAR` (between mips), with proper image layout barriers.
- Image format: `VK_FORMAT_R32_SFLOAT`, `VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`.
- Renderer integration (vkCmdDrawIndirectCountKHR dispatch + AABB SSBO + visible-mask readback) deferred to a
  follow-up session — current Phase 4 = infrastructure ready.

### Stage 6.1 — +3 Flecs ECS systems

- `src/ecs/EcsWorld.{hpp,cpp}` — 3 new systems: `VoxelInteractionTickSystem`, `BenchmarkAutomationTickSystem`,
  `LookDevCaptureTickSystem`. Each mirrors the AudioRefresh/FluidCA pattern (AppStateBinding + per-tick each()).
- New ECS components: `VoxelInteractionTickState` (dummy field to avoid Flecs empty-struct issue),
  `BenchmarkTickResult` (bool quitAfterFrame), `LookDevCaptureTickResult` (bool quitAfterFrame).
- New public API: `TickVoxelInteractionSystem(ecs)`, `TickBenchmarkAutomationSystem(ecs)`,
  `IsBenchmarkAutomationQuitRequested(ecs)`, `TickLookDevCaptureSystem(ecs)`,
  `IsLookDevCaptureQuitRequested(ecs)`.
- `src/core/Types.hpp` — `WorldState` gained `bool allowWorldEditing = false;` (computed in AppUpdate, read by VoxelInteraction ECS system).
- `src/app/AppUpdate.cpp` — sets `world->allowWorldEditing = allowWorldEditing;`, removed inline
  `UpdateVoxelInteraction` block (now in ECS).
- `src/app/main.cpp` — removed inline `UpdateLookDevCaptureAutomation` and `UpdateBenchmarkAutomation` blocks,
  calls `TickLookDevCaptureSystem` / `TickBenchmarkAutomationSystem` instead. Read `IsLookDevCaptureQuitRequested` /
  `IsBenchmarkAutomationQuitRequested` for `quitAfter*Frame` flags.

### Stage 6.2.5 — more `// EVIL:` markers (+13)

- `src/voxel/VoxelRaycast.cpp` — 2 markers: `kEpsilon`, `kDirectionEpsilon`.
- `src/voxel/VoxelMaterials.cpp` — 11 markers: all `k*` exposure/color-grade/shadow/contact-shadow constants
  (`kMinSceneExposure`, `kDefaultMaxSceneExposure`, `kMinSceneKey`, `kMinExposureTargetKey`, `kMaxExposureTargetKey`,
  `kMinEnvironmentIntensity`, `kMaxEnvironmentIntensity`, `kMinColorGradeWhitePoint`, `kMaxColorGradeWhitePoint`,
  `kMinColorGradeContrast`, `kMaxColorGradeContrast`, `kMinColorGradeSaturation`, `kMaxColorGradeSaturation`,
  `kMinColorGradeLift`, `kMaxColorGradeLift`, `kMaxShadowDepthBias`, `kMaxShadowNormalBias`, `kMaxShadowFilterRadius`,
  `kMaxContactShadowDistance`). Total now 23 EVIL markers across the codebase (was 9).

### Stage 6.2.6 — `vkWaitForFences` 10ms timeout constant

- `src/core/Types.hpp` — added `kVulkanFenceWaitTimeoutNs = 10'000'000` and `kVulkanFenceWaitTimeoutUnboundedNs = UINT64_MAX`
  inline constexpr constants. Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` "low latency > throughput".
- `src/render/Renderer.cpp` + `src/app/FramePreparation.cpp` — replaced literal `10'000'000` / `UINT64_MAX` with named constants.
  All `vkWaitForFences` call sites follow the bounded-fallback pattern (10ms primary, unbounded only on `VK_TIMEOUT`).

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug` — 19/19 pass (~1.0 sec)
- `ProjectVSparse64TreeTests` 24 sub-tests pass (+5 homogeneous)
- `ProjectVVoxelWorldTests` (+2 static promotion)
- `ProjectVHzbCullingTests` 4 sub-tests pass
- All previous tests preserved baseline

---

## 2026-06-20 (session: 2x scope continuation)

### Stage 1.1 chunks 5-7 — `world.voxels` fully removed

- `src/voxel/VoxelWorld.hpp` — `world.voxels` field removed from `VoxelWorld` struct.
  Storage is now exclusively the `sparseStorage` (Sparse64Tree). Test fixtures that
  relied on the flat array now use `BuildFlatVoxelSnapshot(world)` helper in
  `src/voxel/VoxelWorld.cpp` (loops `GetVoxelMaterial`).
- `SetVoxelMaterial` now reads previous material via `GetVoxelMaterial` (sparse path)
  for the no-op early-exit, eliminating the flat read.
- `CreateEmptyVoxelWorld` no longer resizes flat; `LoadVoxelWorldSnapshot` no longer
  populates flat.
- `tests/FluidCATests.cpp` + `tests/VoxelWorldTests.cpp` refactored to use
  `BuildFlatVoxelSnapshot` + `SetVoxelMaterial` loops; `state.world.X` →
  `state->world().X` (per AppState PIMPL from 16x session).
- `tests/CMakeLists.txt` + 5 `CMakePresets.json` buildPresets updated for
  `ProjectVShadowProjectionBenchmark` deps (VoxelWorld.cpp, RuntimeDiagnostics.cpp,
  VulkanResult.cpp, fmt, SDL3).

### Stage 1.2 chunks 2-3 — Lazy DedupPass + copy-on-write (COW)

- `src/voxel/Sparse64Tree.hpp` — `Node` struct now has `uint32_t refCount`.
  `AllocateNode` sets `refCount=1`; `FindEquivalentNode` increments on dedup;
  `MarkNodeUnique(nodeIndex)` implements COW: if `refCount > 1`, decrement old,
  allocate copy with `refCount=1`, return new index. `SetCellRecursive` calls
  `MarkNodeUnique` before mutation. `GetRefCount`, `DedupPass()`,
  `DedupSubtree(slot, level)`, `SetDeduplicationEnabled` (resets refCount 0/1)
  public API. Dedup OFF by default.
- `tests/Sparse64TreeTests.cpp` — added `TestCopyOnWriteOnMutation` sub-test.

### Stage 2.1 chunk 1 — CPU mesh fallback (minimal)

- `src/voxel/CpuMeshGenerator.{hpp,cpp}` — minimal CPU-side single-axis (X+) face
  emission for one chunk (4 sub-tests). Used as fallback / reference for future
  mesh shader work; not integrated into Renderer yet.
- `tests/CpuMeshGeneratorTests.cpp` — 4 sub-tests (empty chunk, single voxel,
  boundary voxel, filled chunk).
- `tests/CMakeLists.txt` + 5 buildPresets updated to register `ProjectVCpuMeshGeneratorTests`.

### Stage 2.2 chunk 1 — HZB culling minimal spike

- `src/shaders/hzb_cull.comp` — new compute shader: AABB-vs-HZB test, atomicOr
  visibility bitmask writeback. Compiles to SPIR-V (added to `Shaders` target).
- `src/render/HizCulling.{hpp,cpp}` — `HizBuffer` struct (image/imageView/memory
  placeholder), `IsHzbCullingEnabled()` (env `PROJECTV_HZB_CULLING=ON` toggle,
  default OFF), `ComputeHzbMipLevelCount` helper.
- `tests/HzbCullingTests.cpp` — 4 sub-tests (env disabled default, 1024² → 11 mips,
  800×600 → 10 mips, 1×1 → 1 mip).
- 5 buildPresets updated to register `ProjectVHzbCullingTests`.

### 6.2.3 — DDA shader template macro

- `src/shaders/voxel.frag` — `DDA_BODY(MAX_STEPS, TRAVELED_OP, PRED, RETURN_EXPR, DEFAULT_RETURN)`
  macro unifies the 3 copies of 12-step 3D-DDA loop (was ~50 lines each, now ~10 lines
  per caller + macro body). Replaced for-loops in `TraceLocalPointLightShadowRay`,
  `ComputeSunContactVisibility`, `TraceAmbientOcclusionRay`. `traveled >=` for
  `LocalPointLightShadowRay` preserved via `TRAVELED_OP` arg (others use `>`).
  Net −72 lines.

### 6.2.2 — std::span sweep (one target migrated)

- `src/render/SceneResources.cpp` — `BuildMaterialVisualTable()` return type changed
  from `std::array<VoxelMaterialVisual, kVoxelMaterialCount>` to
  `std::span<const VoxelMaterialVisual>` (static const backing array, span return).
  Caller uses `.size_bytes()` instead of `sizeof(materialVisuals)`.

### 6.1 — Flecs ECS migration (FluidCATickSystem added)

- `src/core/Types.hpp` — `SimulationState` gained `bool effectivePaused = false;`
  so ECS systems can read the derived pause state computed in `UpdateApp`.
- `src/ecs/EcsWorld.{hpp,cpp}` — new `FluidCATickState` component (accumulatorSeconds),
  `FluidCATickSystem` (OnUpdate) registered. Reads AppStateBinding, checks
  effectivePaused/fluidTickRateHz/voxelWorld, drives accumulator + UpdateFluidCA.
  Mirrors AudioRefreshPlaylistSystem pattern.
- `src/app/AppUpdate.cpp` — sets `simulation->effectivePaused` after computing it;
  removed inline FluidCA block (was lines 791-801).
- `src/app/main.cpp` — added `TickFluidCASystem(state->ecs().get())` call after
  UpdateApp (alongside TickAudioRefreshPlaylistSystem).
- Initial InputReplay ECS migration was attempted but reverted: replay playback must
  run BEFORE UpdateApp (input state is read by UpdateApp), ECS systems run AFTER,
  so semantic change would break replay. Reverted InputReplayPlaybackTickSystem.

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug` — 19/19 pass (~1.0 sec)
- `ProjectVHzbCullingTests` 4 sub-tests pass (new)
- `ProjectVCpuMeshGeneratorTests` 4 sub-tests pass
- `ProjectVSparse64TreeTests` 18 sub-tests pass (+1 TestCopyOnWriteOnMutation)
- All other tests preserved baseline

---

## 2026-06-20

### Changed (agent-file-consolidation r0 — commits `3c148e3`, `f1eeb6a`, `1bf096f`, `4f5f379`, `<this-commit>`)

- **`agent/knowledge.md`** (~1860 lines, new) — single source of truth for
  engineering contracts (Part A, formerly `agent/decisions.md` §1-§31)
  and runtime facts (Part B, formerly `agent/memory.md` §1-§11). Part C
  inlines the archive index (formerly `agent/ARCHIVE-INDEX.md`).
  Per-section contracts unchanged — all §N refs in live files resolve
  via `tools/verify_section_anchors.sh`.
- **`agent/workspace.md`** (~250 lines, new) — consolidated live snapshot
  + active tasks ledger. §1 Now / §2 Gap / §3 Next Steps / §4 Risks /
  §5 Active tasks (formerly `agent/active-sessions.md`) / §6 Recent
  closed (archive pointer) / §7 Archive references.
- **`agent/session-checklist.md`** deleted — inlined as `AGENTS.md §9`
  ("Session checklist"), with `§9.1`-`§9.6` matching the original 6
  sections. Old `§9` (Comment management protocol) renumbered to `§10`.
- **`agent/active-sessions.md`** compressed (1951 → 114 lines) — all
  20 pre-`2026-06-20` closed sessions moved to
  `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (1214 lines,
  full per-session detail preserved: commit hashes, file-touched-intent,
  notes). Per operator directive «удалить все сессии, кроме текущей».
- **`agent/status.md`** deleted — content merged into `agent/workspace.md`
  §1-§4 + §7.
- **`AGENTS.md`** updated — §3 sources-of-truth list, §4 classification
  table, §7.2.6.1 file format table, §7.2.8 shared infra list, §7 close-
  routine refs all point to new `agent/knowledge.md` /
  `agent/workspace.md` / `AGENTS.md §9` instead of the deleted files.
  10 cross-refs updated total.
- **`agent/memory.md`, `agent/decisions.md`, `agent/ARCHIVE-INDEX.md`,
  `agent/status.md`, `agent/session-checklist.md`** all deleted.
  Content preserved in `agent/knowledge.md` / `agent/workspace.md` /
  `AGENTS.md §9` / archive files.
- **`tools/verify_section_anchors.sh`** (new, 53 lines) — bash script
  that greps `decisions.md §X` / `memory.md §X` / `workspace.md §X`
  refs in live files (`TODO.md`, `CHANGELOG.md`, `AGENTS.md`) and
  verifies each §N header exists in `agent/knowledge.md` /
  `agent/workspace.md`. Exits 0 if all resolve, 1 if any broken.
  Required by per-commit gate per `AGENTS.md §6.9` for any change
  touching service-file cross-refs.
- **TODO.md** anchor-form cross-refs (Commit A): 14 `decisions.md §X.Y`
  refs converted to hybrid form `§X.Y ([name](#anchor))` with GFM
  auto-generated anchors. Old §N semantics preserved verbatim.
- **TODO.md / README_NEW.md / CHANGELOG.md / agent/active-sessions.md**
  bulk file-path replacement (Commit E): `agent/decisions.md` →
  `agent/knowledge.md Part A`, `agent/memory.md` → `agent/knowledge.md
  Part B`, `agent/status.md` → `agent/workspace.md §1 (Now)`,
  `agent/active-sessions.md` → `agent/workspace.md §5 (Active tasks)`,
  `agent/ARCHIVE-INDEX.md` → `agent/knowledge.md Part C`,
  `agent/session-checklist.md` → `AGENTS.md §9`.
- **COMMENTS.md** footer + audit (Commit A): `<!-- Last validated
  2026-06-20 against src/core/Math.ixx (commit 44362d1) and
  src/core/StringId.ixx (same). -->`. All 15 L-anchor ranges
  spot-checked, all match current code (post-`44362d1`).

### Removed

- **Dead ray-march compute pass** (defense stub, never integrated). Removed
  `src/render/RayMarchPass.hpp` (15 lines), `src/render/RayMarchPass.cpp` (50
  lines), `src/shaders/ray_march.comp` (129 lines), 2 lines in
  `src/CMakeLists.txt` (shader + source registration), 4 lines in
  `src/app/main.cpp` (`#include "render/RayMarchPass.hpp"`,
  `RequestRayMarchPipelineRecreate()` call in `HotReloadShaders`, and the
  `SDLK_2` toggle branch). Per pet-project directive 2026-06-20: the path
  was created for diploma defense 2026-06-13 (ТЗ п. 4.1.2), remained a STUB
  with `SetRayMarchEnabled` containing a no-op `if (isEnabled) return;` guard,
  the compute shader was compiled but never dispatched in
  `Renderer::RecordGraphicsCommands`, and the `SDLK_2` hotkey only flipped
  a flag with no visual effect. The real renderer is the voxel mesh path
  (`voxel_mesh.comp` + `voxel.frag`/`voxel_shadow.*` + TAA + CSM). Future SVO
  rendering per `legacy/docs/architecture/practice/00_svo-architecture.md`
  re-implements DDA from scratch (the removed shader used a flat 3D-grid
  payload, not an SVO). Net −185 lines.

### Changed

- **`TODO.md` rewritten to GPU-driven roadmap v1 (dependency-aware, 6
  Stages, ~340 lines with detailed per-item approach).** Supersedes
  Tier 0..5 r0 (`2026-06-13` plan). Old TODO content archived verbatim at
  `legacy/docs/archive/agent-todos/2026-06-tier-0-5-r0.md` (207 lines,
  `SUPERSEDED 2026-06-20` header). **Re-ordered to fix a critical
  dependency mismatch**: Stage 1 (new voxel storage: Sparse 64-trees +
  SVDAG + async audio) MUST land before any Stage 2-5 work, because all
  Stage 2-5 GPU geometry, cull, sim, GI, LOD code reads from the new
  SVDAG storage. Building those on top of the flat `std::vector<uint8_t>`
  would require a full rewrite when Stage 1 lands. Stage 6 (Flecs ECS
  migration) moved from tech-debt backlog to mainline-and-parallel —
  converting each new system to a Flecs system as it lands is cheaper
  than retrofitting a 989-line god-function later. New TODO covers:
  Pre-Stage 0 quick wins (B1 redundant model load loop, B2 pipeline
  destroy on resize, B3 `getenv` per frame, B4 verify VSync closed),
  Stage 0 architectural decisions (A1 Vulkan 1.4→1.3, A2 Fluid CA
  reversal planning marker), **Stage 1 Voxel Database & Compression**
  (1.1 Sparse 64-trees, 1.2 SVDAG, 1.3 async audio scanPlaylist),
  **Stage 2 GPU-Driven Geometry & Culling** (2.1 Mesh+Task shaders for
  SVDAG, 2.2 HZB cull, 2.3 Virtual Texturing), **Stage 3 Physics &
  Simulation** (3.1 GPU Fluid CA per §30.4, 3.2 incremental Jolt, 3.3
  greedy physics meshing), **Stage 4 Procedural Generation & LOD** (4.1
  GPU noise, 4.2 geometry LOD, 4.3 lift draw distance cap), **Stage 5
  GI & Temporal** (5.1 VCT, 5.2 RTX shadows feature-flag, 5.3 TAA +
  motion vectors), **Stage 6 Tech-debt & ECS refactor** (6.1 Flecs ECS
  migration parallel with Stages 2-5, 6.2 AppState PIMPL + std::span +
  r0 carry-overs). **Verification policy** (cross-cutting): A/B test
  buffers during data-format migrations (Stage 1), MeshingStress
  measurement on every optimization (5% threshold for adoption per
  `decisions.md §5_decision-making.md`), inspected runtime captures
  required for rendering close-out per `decisions.md §15`. All Tier
  0..5 r0 sub-tasks remain closed (commits `cf4b535`, `af69d06`,
  `bafecf9`, `08de29d`, `20b2d9e`, `44362d1`, `72eca66`).
- **`agent/status.md §1 Active sub-plan` updated** to «Roadmap v1 per
  `TODO.md`» (was «Tier 0..5 (Hardcore perf r0)»).
- **`agent/decisions.md §29` header marked OUTDATED** (Tier 0..5 plan
  superseded; old content preserved as historical record; cold/hot
  `std::expected` rule remains valid).
- **`agent/decisions.md §29` R&D-promotion cross-refs updated** to match
  new 6-Stage numbering: Mesh Shaders → Stage 2.1, Sparse 64-trees →
  Stage 1.1, SVDAG → Stage 1.2, RT shadows → Stage 5.2.
- **`agent/decisions.md §30` header marked OUTDATED** (CPU Fluid CA
  fall-only superseded by GPU compute per §30.4).
- **`agent/decisions.md §30.4` added** — «Fluid CA reversal: GPU compute
  (ping-pong + atomicOr + active chunk list)». New binding contract for
  Stage 3.1 in new TODO. Defines: ping-pong voxel buffers, `imageAtomicOr` /
  CAS for destination claim, active chunk list for skipping sleepy
  voxels, multi-tile determinism contract (single-tile deterministic,
  multi-tile statistically stable), 3-step migration path (additive → default
  → deprecate CPU), `SimulationState` unchanged, render path unaware of
  CPU-vs-GPU source. CPU `UpdateFluidCA` retained as reference implementation
  and test fixture per memory policy. Cross-ref to draw-distance cap
  updated from old `Stage 5.3` to new `Stage 4.3`.

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
