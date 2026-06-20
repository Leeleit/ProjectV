# TODO — ProjectV Roadmap (`2026-06-20`)

**Build preset baseline:** `linux-clang-debug` (Clang 22.1.6 + libstdc++ 16 + sccache, ctest 16/16 ≈ 0.77s).
**Scope discipline:** `external/`, `legacy/`, `docs/`, build-артефакты — out of scope.

**Project context (so a fresh session can orient):**
- ProjectV = reproducible interactive voxel MVP. C++26 + Vulkan 1.4 (see A1 for 1.3 option) + Flecs ECS + Jolt Physics + GPU-driven greedy meshing.
- Current voxel storage: flat `std::vector<uint8_t>` (1 byte/voxel, no SIMD, no SoA) — **will be replaced in Stage 1**.
- Current draw distance cap: `min(camera.farPlane, 64)` chunks (`src/render/ShadowProjection.cpp::BuildSunShadowCascadeSplits`) — lifted in Stage 4.3.
- Current shadow path: 4-cascade CSM (`agent/decisions.md §15` — stable, do NOT replace with RTX blindly; RTX = additive feature-flag per Stage 5.2).
- Current lighting: direct GGX + ambient + contact shadow + local point light. Forward voxel DDA, no RT, no SSAO/GTAO.
- Current Fluid CA: CPU fall-only rule (`agent/decisions.md §30`, soon-to-be-superseded by GPU per `§30.4`).
- Per `AGENTS.md §6.3`: do `web_search` (Exa) for unfamiliar Vulkan / C++26 / Jolt API before writing code.

**How to read this file:**
- Each `- [ ]` = one logical task (small = 1 commit, large = multi-commit slice).
- **Pre-Stage 0** = quick-win bugfixes. Do these first; they unblock subsequent work.
- **Stage 0** = architectural decisions (low risk, high impact on subsequent stages).
- **Stage 1** = NEW Voxel Database & Compression (Sparse 64-trees + SVDAG + async audio scan).
  - **CRITICAL DEPENDENCY**: All subsequent GPU geometry, cull, sim, GI, LOD code (Stages 2-5) **MUST read from the new SVDAG/64-tree storage**, not the flat `std::vector<uint8_t>`. Building any of those on top of the flat array would require a full rewrite when Stage 1 lands. The migration is large; Stages 2-5 are designed to assume Stage 1 is already in mainline.
- **Stage 2** = GPU-Driven Geometry & Culling (Mesh Shaders, HZB cull, Virtual Texturing). Depends on Stage 1.
- **Stage 3** = Physics & Simulation (GPU Fluid CA, Incremental Jolt, Greedy Physics Meshing). Depends on Stage 1.
- **Stage 4** = Procedural Generation & LOD (GPU noise gen, geometry LOD, lift draw distance). Depends on Stage 1.
- **Stage 5** = GI & Temporal (VCT, RTX shadows, TAA + Motion Vectors). Depends on Stage 1.
- **Stage 6** = Tech-debt & ECS refactor. **Do in parallel with Stages 2-5** (per dependency-aware plan), not as a final cleanup. Converting each new system to a Flecs system as it lands is cheaper than retro-fitting a 989-line god-function later.
- Each item: **What** / **Why** / **Files** / **Approach** / **Verify** / **Acceptance**.

**Verification policy (cross-cutting, applies to all stages):**
1. **A/B test buffers** during data-format migrations (Stage 1). Keep the old code path as an inactive branch, populate both structures in parallel, compare per-chunk bytewise. Roll forward only when byte-equal across all existing test fixtures.
2. **MeshingStress measurement on every optimization** (`src/bench/` or `TracyPlot` per `decisions.md §4`). Each implementation must improve the relevant Tracy metric by **> 5%** on the stress scene. If not, simplify the implementation or revert to the scalar version with `[[likely]]` / `[[unlikely]]` / branchless optimizations. Per `legacy/docs/philosophy/01_foundation/05_decision-making.md`: «если прирост < 5-10% при значительном усложнении — простой».
3. **Per-`decisions.md §15` close-out rule for any rendering work**: inspected runtime captures required (FINAL + relevant debug views like SHDW / CSM / CTSH / AOCC / LOCL / MOTION / VOXLIGHT), not sidecar numbers alone.

---

## Pre-Stage 0 — Quick wins (low-risk bugfixes)

- [ ] **B1. Remove redundant model load loop** — `src/asset/ModelManifestLoader.cpp::LoadAndRegisterModelsFromManifest` (lines ~87-183)

  **What:** Function builds `render->modelInstances` in two consecutive loops; the first loop's results are wiped by a second `clear()` before the second loop starts. The first loop is dead code.
  **Why:** Per-block CPU time wasted on matrix math that is then discarded. Misleading: future readers may think the first loop is authoritative and the second is a refinement.
  **Files:** `src/asset/ModelManifestLoader.cpp` only.
  **Approach:** (1) Read full function. (2) Identify the `clear()` between the two loops. (3) Delete the first loop + the now-redundant `clear()` + its preceding `reserve()` if only the first loop used it. (4) Keep only the second loop as the single source of truth.
  **Verify:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests` green, `ctest 16/16`, 0 new warnings. Spot-check `render->modelInstances` content byte-identical to pre-fix.
  **Acceptance:** One loop, no double `clear()`, identical render output.

- [ ] **B2. Don't destroy graphics pipeline on `RecreateSwapchain`** — `src/render/vulkan/VulkanSwapchain.cpp::RecreateSwapchain`

  **What:** Currently destroys and recompiles the entire graphics pipeline on every window resize. Pipeline does not depend on swapchain extent (viewport + scissor are dynamic state), so this destroy/recreate is unnecessary.
  **Why:** Shader compile = 100-300 ms hitch on resize. Vulkan API allows keeping the pipeline and just rebuilding swapchain + depth image + framebuffers.
  **Files:** `src/render/vulkan/VulkanSwapchain.cpp` (primary), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (verify no leak — pipeline outlives swapchain), possibly `src/core/Types.hpp::RenderState` if pipeline ownership moves up.
  **Approach:** (1) Read current `RecreateSwapchain`. (2) Verify render pass format is stable across resize (same color/depth format). (3) Move pipeline creation out into one-shot init (`CreateGraphicsPipeline` in `VulkanGraphicsPipeline.cpp`, called once). (4) `RecreateSwapchain` only swaps images + framebuffers + depth.
  **Verify:** Resize window 10× in a row, measure frame time before/after via `TracyPlot`. `ctest 16/16` baseline. Smoke run with mid-run resize — captures before/after resize visually identical.
  **Acceptance:** No pipeline destroy on resize. 0 visible hitches during resize. ctest baseline preserved.

- [ ] **B3. Cache `PROJECTV_GRAVIGUN_SNAP` once at startup** — `src/app/ModelGravigun.cpp::GravigunSnapEnabled`

  **What:** Currently calls `std::getenv("PROJECTV_GRAVIGUN_SNAP")` every frame. `std::getenv` is platform-dependent and may take locks inside the standard library.
  **Why:** Per-frame hot-path call to a non-hot-path lookup. The env var never changes during runtime.
  **Files:** `src/app/ModelGravigun.cpp` only.
  **Approach:** (1) Add `static const bool kSnapEnabled = [] { const char *v = std::getenv("PROJECTV_GRAVIGUN_SNAP"); return v != nullptr && v[0] != '\0' && v[0] != '0'; }();` at function-scope. (2) Replace per-frame `getenv` with the cached value.
  **Verify:** Build green, ctest baseline, smoke run with and without env var set — behavior identical to pre-fix.
  **Acceptance:** 0 `getenv` calls per frame in `ModelGravigun.cpp`, behavior unchanged.

- [ ] **B4. Verify closed: VSync FIFO bug fix** — read-only verification, no code change

  **What:** Operator reported the VSync bug `2026-06-14`. Fix landed in commits `af69d06` family per `agent/decisions.md §30.1-§30.3`. This entry exists so a fresh session verifies the fix is still in place before anyone tries to re-fix.
  **Why:** Pre-Stage 0 quick wins should not silently re-open a closed bug.
  **Files:** None (read-only).
  **Approach:** (1) `git log --oneline -- src/render/vulkan/VulkanSwapchain.cpp src/render/vulkan/VulkanSwapchain.hpp | head -10` — confirm VSync-fix lineage. (2) Read `agent/decisions.md §30.1, §30.2, §30.3` — three VSync-related contracts. (3) Read `tests/PresentModeTests.cpp` — 12 sub-tests covering auto-detect cycle, rebuild preservation, walk scenarios. (4) Optionally runtime: press V on a supported host, confirm cycle walks through physically-supported modes without sticking.
  **Verify:** All three decision sections present, test count ≥ 12, no TODO/XXX/FIXME in `VulkanSwapchain.{hpp,cpp}` related to VSync cycle.
  **Acceptance:** Read-only confirmation recorded in commit body. No code change.

---

## Stage 0 — Architectural decisions (do before Stage 1)

- [ ] **A1. Lower Vulkan API requirement 1.4 → 1.3** — `src/render/vulkan/VulkanBootstrap.cpp::TryPickPhysicalDevice`

  **What:** Hard requirement `props.apiVersion >= VK_API_VERSION_1_4` blocks MoltenVK, older dGPUs, integrated graphics, and Vulkan 1.3-only hardware. Used features (Dynamic Rendering, Synchronization 2, push descriptors, buffer device address, dynamic state) are all 1.3. 1.4-specific features (e.g. `VK_KHR_dynamic_rendering_local_read`) are NOT currently used.
  **Why:** Per `legacy/docs/philosophy/01_foundation/05_decision-making.md` — minimum spec should match real usage, not marketing. Lowering 1.4 → 1.3 unlocks significant install base for marginal engineering risk.
  **Files:** `src/render/vulkan/VulkanBootstrap.cpp::TryPickPhysicalDevice` (change hard floor), `src/core/Types.hpp` if there's a `kMinVulkanApiVersion` constant, root `CMakeLists.txt` if there's a version gate. Add new env override `PROJECTV_MIN_VULKAN_API_VERSION` for testing.
  **Approach:** (1) Grep for `VK_API_VERSION_1_4` and `VK_VERSION_1_4` to confirm nothing requires 1.4. (2) Grep `VkPhysicalDeviceVulkan14Properties` and similar — should be 0 results. (3) Change hard floor to `VK_API_VERSION_1_3`. (4) Add `PROJECTV_MIN_VULKAN_API_VERSION` env override (parse "1.X.Y" string, default `1.3.0`). (5) If 1.4-only feature is discovered, decide: feature-flag that subsystem, or revert this task.
  **Verify:** `vulkaninfo` on a 1.3-only host (MoltenVK via DXVK or a CI matrix). ctest 16/16. Runtime smoke run.
  **Acceptance:** Boots on Vulkan 1.3 hardware. All features still work. Env override accepted. ctest baseline preserved.

- [ ] **A2. Fluid CA reversal (planning marker — code lives in Stage 3.1)**

  **What:** No code in this item. The contract is already in `agent/decisions.md §30.4` (GPU Fluid CA: ping-pong + atomicOr + active chunk list). This TODO entry is a planning marker so the actual implementation (Stage 3.1) has its decision contract clearly cross-referenced.
  **Why:** Operator explicitly reversed `§30` (CPU fall-only) → GPU compute on `2026-06-20`. The contract is binding; do not deviate from it when implementing Stage 3.1.
  **Files:** None for this entry. Cross-refs:
  - `agent/decisions.md §30.4` — full contract (must-read before Stage 3.1)
  - `agent/decisions.md §30` — CPU reference (preserved, OUTDATED marker, content kept for test fixtures + reference implementation)
  - `agent/decisions.md §30.1` — tick rate (20 Hz default), `effectivePaused` gate, `timeScale` integration in `UpdateApp` (still applies, only the dispatcher changes)
  **Verify:** N/A (planning marker).
  **Acceptance:** When Stage 3.1 is implemented, the contract in `§30.4` is followed literally.

---

## Stage 1 — NEW Voxel Database & Compression (foundation for Stages 2-5)

**CRITICAL:** This stage establishes the storage format that all subsequent GPU geometry, cull, sim, GI, LOD code (Stages 2-5) reads from. Do not start any Stage 2-5 work until Stage 1 is in mainline.

- [ ] **1.1. Sparse 64-trees** — `src/voxel/VoxelWorld.{hpp,cpp}` (replace flat `std::vector<uint8_t>`)

  **What:** Replace flat `std::vector<uint8_t>` storage with a sparse 64-ary tree. Each internal node covers a `4×4×4 = 64` cell sub-volume; child occupancy stored as `uint64_t fillMask` (one bit per child slot). Walk via `findFirstSet` / `clearBit` intrinsics. Leaves store material IDs (1-2 bytes each, or 1 byte with material table for >256 materials).
  **Why:** (1) Memory: empty space = single `fillMask = 0` per empty 4×4×4 = 8 bytes for 64 cells instead of 64 bytes — 8× reduction for sparse worlds. (2) Cache locality: tree walks are predictable; leaf material data is dense. (3) Enables SVO raytracing later (1.2, 5.1) without a second storage rewrite. (4) SIMD-friendly: 64-bit mask = single `__builtin_ctzll` / `_pdep_u64` operations.
  **Files:** `src/voxel/VoxelWorld.hpp` (replace `voxels: std::vector<uint8_t>`), `src/voxel/VoxelWorld.cpp` (all access sites), new `src/voxel/Sparse64Tree.hpp` (the tree itself), `src/voxel/VoxelRaycast.{hpp,cpp}` (raycast now tree-walking), `src/shaders/voxel_mesh.comp` (or replacement — needs read access to tree leaves), `src/physics/PhysicsWorld.cpp` (read access for collision), `src/voxel/UpdateFluidCA` (read + write access).
  **Approach:** (1) **A/B test buffers** per Verification policy: implement `Sparse64Tree` as standalone header-only template, unit-test with byte-exact parity against flat `std::vector<uint8_t>`. Keep both paths. (2) Add `VoxelWorld::SetVoxelMaterial` / `GetVoxelMaterial` on top of tree. (3) **3-step migration** (per `decisions.md §30.4` precedent): (a) additive `PROJECTV_SPARSE_64_STORAGE=ON` env, both paths run in parallel, output cross-checked per chunk; (b) flip default; (c) delete flat path. (4) **Verify byte-equal output** on VoxelLab, MeshingStress, all `tests/VoxelWorldTests.cpp` fixtures. (5) MeshingStress measurement: TracyPlot for `VoxelAccess (ms)` should drop ≥ 5% on sparse scenes.
  **Verify:** `ctest 16/16` (existing 24 FluidCA + others). New `ProjectVSparse64TreeTests` with byte-exact comparison vs flat snapshot. Snapshot save/load round-trip. Memory profiling: VoxelLab + 10× empty chunks before/after.
  **Acceptance:** Storage swap complete, byte-equal output across all existing test fixtures, 5-10× memory reduction measured on VoxelLab. All Stage 2-5 features are free to assume this storage format.

- [ ] **1.2. SVDAG (Sparse Voxel Directed Acyclic Graph)** — extends 1.1

  **What:** For static geometry (chunks that haven't been edited in N ticks), deduplicate identical subtrees. Two `Sparse64Tree` nodes with the same `fillMask` AND same child material/structure share a single allocation. DAG pointer replaces one of the two node references. Active (mutable) chunks use plain trees; static chunks use DAG nodes. Lazy promotion: chunk becomes "static" after N ticks without edits.
  **Why:** 50-100× memory reduction for worlds with repeating structure (most procedural worlds). This is the difference between "fits in 8 GB VRAM" and "fits in 80 MB VRAM" for the same world.
  **Files:** `src/voxel/Sparse64Tree.hpp` (add `NodeId` indirection + DAG node pool), `src/voxel/VoxelWorld.hpp` (track per-chunk `isStatic` flag), `src/voxel/MarkAllVoxelChunksDirty` (clears static flag), `src/shaders/voxel_mesh.comp` (or replacement — handles DAG indirection on GPU side; possibly via SSBO of node IDs).
  **Approach:** (1) Wait for 1.1 to land — SVDAG = Sparse 64-tree + dedup. (2) Add node pool with `NodeId = uint32_t` indirection. (3) Add `tryMerge(nodeA, nodeB) → NodeId` (structural + material hash). (4) Lazy dedup: when chunk becomes static, walk subtree, dedup identical siblings. (5) Mutation invalidates DAG: if a static chunk is edited, revert to plain tree. (6) GPU access: SSBO of node IDs, shader traverses DAG.
  **Verify:** Memory profiling on VoxelLab + synthetic test scene with deliberate repetition (e.g. 10× 4×4×4 brick patterns). Byte-exact output. Mutation correctness: edit a previously-static chunk, verify visual change appears.
  **Acceptance:** SVDAG on by default for static chunks. 50-100× memory reduction on repetitive test scenes. Mutation safety verified. GPU traversal performance ≥ scalar CPU (MeshingStress measurement).

- [ ] **1.3. Async audio playlist scan** — `src/audio/AudioEngine.cpp::tick`

  **What:** `AudioEngine::tick()` currently calls `scanPlaylist()` every 5 seconds on the main thread. `scanPlaylist` does `std::filesystem::directory_iterator` over the music folder. On slow disks (HDD, network share) this is 50-200 ms freeze.
  **Why:** Regular micro-stutter every 5 seconds. Easy fix. Also makes the data-layer I/O pattern consistent with Stage 1.3 (async I/O theme).
  **Files:** `src/audio/AudioEngine.cpp` (move `scanPlaylist` to background thread or trigger only on demand / file watcher event).
  **Approach:** (1) Read `AudioEngine::tick` and identify the 5-second timer block. (2) Replace with: (a) on startup, scan once async (background thread); (b) on user-initiated playlist refresh (e.g. `R` keybind, or `RefreshPlaylist()` API), scan async; (c) remove the periodic timer entirely. (3) Use a `std::jthread` or `std::async` with `std::filesystem::directory_iterator` in the background.
  **Verify:** `ctest 16/16`. Runtime smoke: idle for 30 seconds with slow disk, no frame stalls > 4 ms. Test playlist refresh on keypress: scan completes within 1 second, playlist updates.
  **Acceptance:** Zero periodic main-thread disk I/O. Manual refresh path works. No regression in playlist contents.

---

## Stage 2 — GPU-Driven Geometry & Culling (depends on Stage 1 SVDAG)

**All Stage 2 shaders MUST read from SVDAG (Stage 1.2), not the flat array.** Building them on top of the flat array would require a full rewrite when Stage 1 lands.

- [ ] **2.1. Mesh + Task Shaders for SVDAG (`VK_EXT_mesh_shader`)** — `src/shaders/voxel_mesh.comp` → new `voxel_mesh.task` + `voxel_mesh.mesh`

  **What:** Replace the compute-shader-driven indirect draw pipeline (`voxel_mesh.comp` writes vertex/index payloads to `packedFaces` SSBO) with mesh-shader-driven pipeline. **Task shader** does cluster-level cull (micro-frustum + Hi-Z + back-face reject) on the GPU workgroup level, **traversing the SVDAG**. **Mesh shader** generates vertices and indices directly into LDS/shared memory and outputs to the rasterizer — no intermediate global-memory geometry buffer. `VK_EXT_mesh_shader` must be available.
  **Why:** Eliminates per-frame `packedFaces` VRAM allocation growth. Mesh shader output goes directly to the rasterizer. Aligns with current 2.2 HZB work: task shader cull = same AABB-vs-HiZ as compute cull, but per-cluster (sub-chunk granularity) instead of per-chunk. **Direct dependency on Stage 1**: shader reads `NodeId` SSBO from SVDAG, not flat voxel array.
  **Files:** `src/shaders/voxel_mesh.comp` (replace or branch), new `src/shaders/voxel_mesh.task` + `src/shaders/voxel_mesh.mesh`, `src/render/Renderer.cpp::RecordGraphicsCommands` (replace `vkCmdDispatch` + `vkCmdDrawIndexedIndirect` with `vkCmdDrawMeshTasksIndirect[Count]KHR`), `src/render/SceneResources.{hpp,cpp}` (drop `packedFaces` buffer or keep as fallback).
  **Approach:** (1) **Hardware check first**: `vkGetPhysicalDeviceMeshShaderPropertiesEXT` — feature-gate entire task (`PROJECTV_MESH_SHADER_PIPELINE=ON` env, default off if extension unsupported). (2) **A/B test**: dual pipeline (compute + mesh), runtime switch, parity test. (3) Port `voxel_mesh.comp` cull logic → task shader; port vertex/index emit → mesh shader. Task shader reads SVDAG NodeId SSBO. (4) Verify pixel-identical output (framebuffer hash compare). (5) Once stable, switch default to mesh path on supported hardware.
  **Verify:** Hardware check on RTX 3060 Ti (current dev host) and any RTX 20+/AMD RDNA2+ hardware available. Bit-identical framebuffer via `lookdev-captures` (FINAL + SHDW + AOCC + LOCL all match). Performance: `TracyPlot("Meshing (ms)")` + `TracyPlot("Render (ms)")` should drop or stay flat. ctest 16/16. MeshingStress: 5%+ improvement.
  **Acceptance:** Mesh shader path produces byte-identical output to compute path on VoxelLab + MeshingStress. Hardware feature-gated. Fallback to compute on unsupported hardware verified. Reads SVDAG, not flat array.

- [ ] **2.2. Two-pass HZB Occlusion Culling** — `src/render/SceneResources.{hpp,cpp}` + new `src/shaders/hzb_cull.comp`

  **What:** Currently CPU frustum cull produces per-frame `ChunkVisibilityCache`; nothing rejects chunks hidden by already-rasterized geometry (overdraw in caves, behind hills, interior of dense structures). Replace with GPU two-pass: (1) render prev-frame-visible chunks, (2) build Hi-Z mip chain from depth buffer (`vkCmdBlitImage` with `MIN` filter for reverse-Z, `MAX` for forward-Z), (3) compute shader tests all chunks' AABBs against HZB and writes a visibility bitmask, (4) render only newly-disoccluded chunks (`vkCmdDrawIndirectCountKHR`/`vkCmdDrawIndexedIndirectCountKHR` — indirect count buffer populated by the compute shader). **Chunk AABBs are derived from SVDAG (Stage 1.2)** — clusters update their AABBs lazily as SVDAG mutates.
  **Why:** 40-70% FPS gain in closed spaces (per upstream plan benchmark). Eliminates overdraw that frustum cull cannot catch. Removes per-frame CPU chunk-visibility-rebuild work for the cull step.
  **Files:** `src/render/SceneResources.{hpp,cpp}` (new `HizBuffer`, `HizMipChain`, `OccludedDrawCommandBuffer`), `src/shaders/hzb_cull.comp` (new — AABB-vs-mip test), `src/render/Renderer.cpp::RecordGraphicsCommands` (insert 4 dispatches: prev-frame draw, HZB build, compute cull, disoccluded draw), `src/render/ShadowProjection.cpp` (CSM also benefits — same HZB test for per-cascade caster visibility).
  **Approach:** (1) **Spike first**: add `HizBuffer` as optional (`PROJECTV_HZB_CULLING=ON` env), keep CPU cull as fallback. (2) Build mip chain via `vkCmdBlitImage` after main pass; chain levels = `log2(min(width, height))`. (3) `hzb_cull.comp` reads `ChunkAabb` SSBO + HZB image, writes `uint32_t visibleMask[chunkCount/32]` + `VkDrawIndirectCommand` for visible chunks only. (4) Wire `vkCmdDrawIndirectCountKHR` with `drawCount = visibleCount`. (5) AABB source: SVDAG chunk AABB cache (computed at chunk-mesh time, invalidated on edit). (6) Compare FPS / draw count before/after on VoxelLab + MeshingStress + a synthetic closed-space test scene.
  **Verify:** `TracyPlot` for `ChunkCulling (ms)` before/after. Per-frame draw count drops 30-60% in closed scenes. ctest 16/16. Runtime smoke with `PROJECTV_HZB_CULLING=ON` and `=OFF` both produce visually identical output. MeshingStress measurement: 5%+ improvement.
  **Acceptance:** HZB-driven cull path on by default in dev, off-by-default in release until stable. Measurable FPS gain in closed scenes. Fallback path verified.

- [ ] **2.3. 3D Virtual Texturing (Megatexture + Feedback Buffer)** — new `src/asset/TextureStreamer.{hpp,cpp}`

  **What:** Today's per-block textures fit in `Texture Arrays`; this breaks when block variety grows (1k+ unique blocks). Replace with virtual texturing: (1) all block textures baked into a single physical atlas; (2) page table maps (block_id, mip) → atlas (page_x, page_y, mip); (3) fragment shader writes "this page is visible" feedback to a low-res Feedback Buffer; (4) CPU async streamer reads Feedback Buffer, evicts cold pages, loads hot pages from disk into atlas slots.
  **Why:** Decouples "number of unique blocks" from "max descriptor count + max texture array size". Enables world-scale material variety on hardware with small descriptor limits.
  **Files:** New `src/asset/TextureStreamer.{hpp,cpp}` (page allocator + async loader), new `src/shaders/virtual_texture_sample.glsl` (or include into `voxel.frag`), new `src/shaders/feedback.frag` (writes visible pages), modified `src/asset/AssetLoader.cpp` (block → page binding), `src/render/SceneResources.{hpp,cpp}` (atlas + page table + feedback buffer), `src/debug/DebugHud.cpp` (page residency HUD line).
  **Approach:** (1) **Design doc first** — page size, atlas size, feedback resolution, mip levels per page. (2) **Spike**: single-page test (atlas = 1 page, no streaming yet). (3) Add feedback pass + page allocator. (4) Add async streamer (background thread + frame-paced uploads). (5) Wire sample path in `voxel.frag`. (6) HUD page-residency indicator.
  **Verify:** VoxelLab material set renders identically. HUD shows page residency count rising as camera moves. No visual artifacts at page boundaries (seamless). ctest 16/16. MeshingStress: 5%+ improvement in atlas upload cost.
  **Acceptance:** Megatexture path on by default. Page residency in HUD. Async streamer doesn't block main thread. Visual parity with current array path on existing test scenes.

---

## Stage 3 — Physics & Simulation (depends on Stage 1 SVDAG)

- [ ] **3.1. GPU Fluid CA (REVERSAL — implements `agent/decisions.md §30.4`)** — new `src/shaders/fluid_ca.comp`

  **What:** Per `decisions.md §30.4` — implement GPU compute fluid CA. (1) Two `VkImage` (or SSBO) ping-pong buffers for voxel state, **reading and writing SVDAG nodes** (not flat array). (2) `atomicOr` for fluid destination claim; `imageAtomicCompareExchange` for the "is target Air?" check. (3) Frontend CPU builds `activeChunks` list (chunks with non-stable fluid or recent edits); dispatch `activeChunks.count` workgroups. (4) Iteration order inside workgroup: `z, y, x` ascending (preserves per-tile determinism). (5) `SimulationState` (tick rate, accumulator, pause/timeScale) unchanged — only the dispatcher changes.
  **Why:** Reversal of `§30` per operator `2026-06-20`. CPU CA scales as O(N³); GPU CA scales as O(active_chunks). Required for 64+ chunk draw distance (Stage 4.3) and procedural worlds. **Direct dependency on Stage 1**: shader operates on SVDAG nodes, so this stage cannot begin meaningfully until SVDAG (1.2) is in mainline.
  **Files:** New `src/shaders/fluid_ca.comp`, `src/render/Renderer.cpp::RecordComputeCommands` (new compute pass dispatch), `src/voxel/VoxelWorld.{hpp,cpp}` (add `activeChunks` SSBO, add GPU-side dispatch helper), `src/core/Types.hpp::SimulationState` (no change to fields, but add `fluidCaGpuEnabled` flag), `src/app/AppUpdate.cpp` (dispatch instead of CPU loop), `src/shaders/voxel.frag` (not affected — reads same voxel buffer).
  **Approach:** Per `§30.4` 3-step migration: (1) **Additive**: `PROJECTV_FLUID_CA_GPU=ON` env, CPU path remains default. Both produce same visual output. A/B validate. (2) **Default flip**: GPU on for dev presets, CPU = emergency fallback. (3) **Deprecate CPU**: CPU kept as reference + test fixture only (`PROJECTV_RUN_CPU_REFERENCE_TESTS=ON` for opt-in). Reuse 24 sub-tests from `tests/FluidCATests.cpp` as CPU reference; write new `tests/FluidCAGpuTests.cpp` with same scenarios + GPU-specific tests (workgroup determinism, multi-tile race semantics, performance).
  **Verify:** Per `§30.4` acceptance: (a) VoxelLab glass-break scenario: fluid falls and spreads identically to CPU version. (b) Pause / timeScale honored. (c) Snapshot save/load round-trip with multi-tile determinism contract. (d) Performance: 1M+ fluid voxels without mainline FPS drop. (e) `ctest 16/16` (CPU ref) + new GPU tests pass. MeshingStress: 5%+ improvement on multi-chunk fluid scenarios.
  **Acceptance:** GPU CA produces visually identical result to CPU CA on VoxelLab. 24+24 tests pass (CPU ref + GPU new). Reversal contract from `§30.4` implemented. Operates on SVDAG, not flat array.

- [ ] **3.2. Incremental Jolt Physics (per-chunk static bodies)** — `src/physics/PhysicsWorld.cpp::SyncPhysicsWorld` + `BuildStaticVoxelCollisionBody`

  **What:** Currently `BuildStaticVoxelCollisionBody` rebuilds the entire static physics world (`CompoundShape` of `BoxShape` per voxel) on any voxel edit. Replace with per-chunk static bodies (e.g. 16×16×16 voxel chunks). On edit, destroy + rebuild only the affected chunk's body. `HeightFieldShape` for terrain (heightmap chunks, local-area update). **Chunk indices align with SVDAG (Stage 1.2) chunks.**
  **Why:** Current path = 100-500 ms spike on single voxel edit. Breaks gameplay on any build/break action. Per-chunk granularity reduces rebuild cost by ~chunk_factor.
  **Files:** `src/physics/PhysicsWorld.cpp::SyncPhysicsWorld` (rebuild only diff chunks), `src/physics/PhysicsWorld.hpp` (new per-chunk body map: `chunkIndex → BodyId`), `src/physics/BuildStaticVoxelCollisionBody` (now per-chunk, not global), `src/physics/PhysicsWorld.cpp::QueueChunkRebuildRequest` (mark chunk body for rebuild), `src/voxel/VoxelWorld.cpp` (call site on voxel edit, was triggering global rebuild).
  **Approach:** (1) **Audit current behavior**: measure spike time on a 1000-voxel world. (2) **Design per-chunk body layout**: BodyId per chunk, Jolt `BodyInterface::RemoveBody` + `CreateAndAddBody` for incremental update. (3) **Per-chunk rebuild**: `BuildChunkStaticBody(chunkIndex) → BodyId`, called from `SyncPhysicsWorld` when `world->chunks[i].rebuildQueued`. (4) **HeightFieldShape path** for terrain: `HeightFieldShapeSettings` with local heightmap. (5) **3.3 Greedy Physics Meshing** provides the per-chunk collision shape (without 3.3, fallback to per-voxel BoxShapes within the chunk).
  **Verify:** Single voxel edit: spike drops from 100-500 ms to < 16 ms. 1000 voxel edits in a single frame: total spike < 100 ms. Character collision still works (existing walk/creative tests). ctest 16/16 (especially `ProjectVPhysicsTests`).
  **Acceptance:** Per-chunk body rebuild. No global rebuild. Existing physics tests pass. Spike time < 1 frame.

- [ ] **3.3. Greedy Physics Meshing** — new `src/shaders/voxel_physics_mesh.comp` + integration in `src/physics/`

  **What:** Current physics uses per-voxel `BoxShape`. Visual meshing uses greedy meshing. Add a compute-shader-driven greedy physics meshing: each chunk produces a `JPH::MeshShape` (triangle mesh) or `HeightFieldShape` (heightmap), not a `CompoundShape` of `BoxShape`. **Same algorithm as 2.1 visual meshing, but output is collision geometry.** Reads SVDAG (Stage 1.2) directly.
  **Why:** Per-voxel BoxShape = N body shapes per chunk (N = non-air voxels). Greedy mesh = O(visible faces) triangles. 5-50× fewer shapes. Aligns with DoD philosophy: data prepared efficiently for both GPU rendering and CPU physics.
  **Files:** New `src/shaders/voxel_physics_mesh.comp` (mirror of `voxel_mesh.comp` but writes triangle/index buffers for Jolt), `src/physics/PhysicsWorld.cpp::BuildChunkStaticBody` (consume the mesh output, build Jolt `MeshShape`), `src/physics/PhysicsWorld.hpp` (per-chunk mesh buffer).
  **Approach:** (1) Port `voxel_mesh.comp` greedy meshing logic to a new compute shader that outputs triangle list instead of vertex/index runs. (2) `BuildChunkStaticBody` reads the triangle list, builds Jolt `MeshShapeSettings`, creates the body. (3) Coordinate with 3.2 (Incremental Jolt) — both per-chunk; share the same per-chunk body lifecycle.
  **Verify:** Per-chunk shape count drops 5-50×. Character collision behavior unchanged. `ctest 16/16`. MeshingStress: 5%+ improvement in physics step time.
  **Acceptance:** Per-chunk mesh shape. Fewer total shapes. Same collision behavior. 5-50× reduction measured.

---

## Stage 4 — Procedural Generation & LOD (depends on Stage 1 SVDAG)

- [ ] **4.1. GPU noise generation** — new `src/shaders/world_gen.comp`

  **What:** Currently `WorldGen.cpp` generates voxel data on CPU using Perlin/Simplex noise. Move to compute shader. GPU writes voxel data directly to the **SVDAG node pool (Stage 1.2)**. CPU submits dispatch (`PROJECTV_GENERATE_CHUNK(chunkCoord)`), GPU returns when done. For big batch generation: one dispatch covers a `N×N` chunk region.
  **Why:** CPU bottleneck during fast flight. GPU is 10-100× faster for noise compute.
  **Files:** New `src/shaders/world_gen.comp` (Perlin/Simplex/fBm), `src/voxel/WorldGen.cpp` (CPU-side dispatcher + queue), `src/asset/AsyncChunkLoader.{hpp,cpp}` (consumer for newly-generated chunks), `src/core/Types.hpp::WorldState` (chunk generation request queue).
  **Approach:** (1) **CPU reference first**: verify current CPU noise produces expected output (snapshot test). (2) Port to compute shader — bit-exact or visually-equivalent noise function. (3) Wire dispatcher. (4) Cross-check GPU output vs CPU reference for random chunk coordinates.
  **Verify:** Generated chunks visually identical to CPU version. CPU `tests/WorldGenTests.cpp` (if exists) + new GPU tests pass. Stress test: generate 100 chunks in one frame, measure time. MeshingStress: 5%+ improvement.
  **Acceptance:** GPU world gen on by default. 10×+ speedup measured on batch generation. Output byte-equal to CPU reference on test fixtures. Writes to SVDAG, not flat array.

- [ ] **4.2. Geometry LOD (MIP-level chunks with geomorphing)** — `src/render/SceneResources.cpp` (LOD assignment) + `src/shaders/voxel_mesh.comp` (or replacement)

  **What:** For chunks far from camera, use a downsampled voxel representation (e.g. 2×, 4×, 8× coarser). Generate coarser representation on chunk creation (or lazily on first LOD need). Render with geomorphing: at LOD transition boundary, blend between two LOD levels to hide the pop. **Coarser levels are SVDAG sub-graphs sampled at lower resolution** (no separate storage).
  **Why:** Poly count grows O(N²) with distance for naive mesh. With 4× LOD, distant chunks have 1/64 the poly count. Required for 128+ chunk draw distance (4.3) at 60 FPS.
  **Files:** `src/voxel/VoxelWorld.{hpp,cpp}` (per-chunk LOD level), `src/render/SceneResources.{hpp,cpp}` (LOD selection per chunk based on distance), `src/shaders/voxel_mesh.comp` (or `voxel_mesh.task` from 2.1 — handle variable-density mesh).
  **Approach:** (1) **Spike on VoxelLab**: generate 2× and 4× LOD versions of a chunk, verify visual quality. (2) Add geomorphing blend (between LOD N and N+1 across a transition band). (3) Wire LOD selection (camera distance → LOD level). (4) Validate that transitions don't show seams (cracks at LOD boundaries).
  **Verify:** VoxelLab at 32+ chunks distance: poly count significantly reduced, no visible LOD seams during slow camera move. ctest 16/16. `LOD` debug view shows per-chunk LOD level. MeshingStress: 5%+ improvement at distance.
  **Acceptance:** LOD on by default for distance > threshold. Visible transition smoothing. Poly count reduction measured.

- [ ] **4.3. Lift draw distance cap** — `src/render/ShadowProjection.cpp::BuildSunShadowCascadeSplits` + `src/core/Types.hpp::kChunkVisibilityCacheMaxChunks`

  **What:** Current cap: `min(camera.farPlane, 64)` chunks. Lift to 128-256 chunks. **Depends on Stages 1-3** (HZB cull, SVDAG, greedy physics) and **Stage 5 (VCT + RTX)** to be in place — otherwise FPS will drop.
  **Why:** Stages 1-3 enable larger worlds. Without lifting the cap, the new infrastructure is unused.
  **Files:** `src/render/ShadowProjection.cpp::BuildSunShadowCascadeSplits` (cap constant), `src/core/Types.hpp::kChunkVisibilityCacheMaxChunks` (1024 → 4096 or similar), `src/render/SceneResources.{hpp,cpp}` (any fixed-cap buffers tied to chunk count), `src/debug/DebugHud.cpp` (show current draw distance).
  **Approach:** (1) Verify all Stages 1-3 are stable. (2) Lift cap to 128. (3) Profile: FPS, GPU memory, CPU memory, ctest. (4) Lift to 256 if 128 is stable. (5) HUD: current draw distance + chunk count visible.
  **Verify:** 128 chunk draw distance runs at acceptable FPS (target 60+ on VoxelLab, 30+ on MeshingStress). ctest 16/16. CSM still covers (no shadow pop at new distance). MeshingStress: FPS at max draw distance ≥ 30.
  **Acceptance:** Draw distance cap lifted to at least 128. Performance acceptable. All previous tests pass.

---

## Stage 5 — GI & Temporal (depends on Stage 1 SVDAG)

- [ ] **5.1. Voxel Cone Tracing (VCT)** — new `src/shaders/voxelize.comp` + new `src/shaders/vct.frag`

  **What:** Build a low-res 3D texture (e.g. 256³ R8G8B8A8) per frame, populated by a compute shader that **voxelizes the SVDAG scene (Stage 1.2)** — avg color + opacity per voxel. Generate mipmap chain via `vkCmdBlitImage` (MIN/MAX for opacity, AVG-style box filter for color). Fragment shader does cone traces into this mip chain: diffuse cone (wide) for indirect bounce, specular cone (narrow) for glossy reflections.
  **Why:** Real-time global illumination without pre-baked lightmaps or full RT. Adds indirect bounce to cavities, specular reflections off indirect light, ambient that respects scene geometry. Cheaper than RT (5.2), no hardware dependency.
  **Files:** New `src/shaders/voxelize.comp` (SVDAG → 3D atlas), new `src/shaders/vct.frag` (or extend `voxel.frag`), `src/render/SceneResources.{hpp,cpp}` (3D atlas + mip chain), `src/render/Renderer.cpp::RecordGraphicsCommands` (voxelize dispatch + mip generation after main pass).
  **Approach:** (1) **Spike on VoxelLab**: voxelize at 64³, see if indirect light is visible. (2) Add mip chain. (3) Add cone trace to fragment shader. (4) Tune: cone angle, mip selection, integration count, max distance. (5) `VOXLIGHT` debug view (visualize voxelized scene).
  **Verify:** Visible indirect bounce in VoxelLab (e.g. glass sphere interior shows tinted color from surrounding opaque). Debug view shows correct voxelization. `ctest 16/16`. Runtime smoke: 2-3 captured frames in `lookdev-captures/` showing indirect light contribution. MeshingStress: 5%+ improvement in cavity lighting.
  **Acceptance:** Voxel Cone Tracing on by default in dev. Measurable indirect bounce in closed spaces. Debug view functional. No regression in direct lighting. Voxelizes from SVDAG, not flat array.

- [ ] **5.2. RTX shadows (feature-flag)** — new `src/render/RayTracedShadows.{hpp,cpp}`

  **What:** Add `VK_KHR_acceleration_structure` + `VK_KHR_ray_query` support. Build a **BLAS per chunk from the SVDAG mesh data (Stage 1.2 + 2.1)**. TLAS updated as chunks become visible/hidden. Fragment shader uses `rayQueryEXT` to trace a hard shadow ray + a few samples for soft shadow PCF. Feature-flagged: `PROJECTV_ENABLE_HW_RAY_TRACING=ON` (default OFF in release, ON in dev if hardware supports).
  **Why:** Per-pixel soft shadows with proper area light integration. CSM (current `decisions.md §15` path) is cheap and good for sun, but doesn't handle small light sources or fine detail. RTX shadows are additive — they don't replace CSM, they complement it.
  **Files:** New `src/render/RayTracedShadows.{hpp,cpp}` (BLAS/TLAS build, ray query shader), `src/shaders/voxel.frag` (ray query call), `src/render/SceneResources.{hpp,cpp}` (BLAS per-chunk, TLAS), `src/render/Renderer.cpp::RecordGraphicsCommands` (TLAS update), root `CMakeLists.txt` (RTX extension gating).
  **Approach:** (1) **Hardware check first**: `vkGetPhysicalDeviceAccelerationStructurePropertiesKHR`. (2) **Spike**: single hard shadow ray, no soft sampling. (3) Add BLAS build per chunk (use existing mesh data from SVDAG + 2.1). (4) Add TLAS update per frame. (5) Add soft shadow sampling (4-8 rays). (6) Feature-gate: hardware absent → skip; hardware present + env off → skip; env on → use. (7) Coexist with CSM: CSM for sun, RTX for local lights and additional contact details.
  **Verify:** Hardware check on RTX 3060 Ti. `PROJECTV_ENABLE_HW_RAY_TRACING=ON` produces visibly better shadows on VoxelLab. `=OFF` produces identical output to baseline (CSM only). ctest 16/16.
  **Acceptance:** RTX path on by default in dev on supported hardware. Fallback to CSM on unsupported. Visual improvement visible in VoxelLab. Coexists with CSM (not replacement). BLAS built from SVDAG-derived mesh.

- [ ] **5.3. TAA + Motion Vectors** — `src/shaders/voxel.frag` (motion vector emit) + `src/render/Taa.cpp` (disocclusion reject)

  **What:** Currently TAA accumulates history per-pixel. Two improvements: (1) **Motion vectors**: fragment shader emits per-pixel motion = previous-clip-position - current-clip-position. Used to reproject history buffer; if reprojected sample is occluded in current frame (depth test fails), reject history sample. (2) **Disocclusion detection**: where current-frame depth is closer to camera than reprojected-history depth, history is "stale" — reset accumulator for that pixel.
  **Why:** Removes TAA ghosting during fast camera yaw / motion.
  **Files:** `src/shaders/voxel.frag` (compute motion vector, output to MRT or separate buffer), `src/shaders/voxel.vert` (pass world position to fragment for motion calc), new `src/shaders/motion_vector.frag` (if separate pass preferred), `src/render/Taa.cpp` (reproject + disocclusion test), `src/render/SceneResources.{hpp,cpp}` (motion vector buffer), `src/render/Renderer.cpp::RecordGraphicsCommands` (motion vector pass before TAA resolve).
  **Approach:** (1) **Spike**: motion vector pass only, no TAA change yet — visualize in `MOTION` debug view. (2) Wire TAA reprojection using motion vectors. (3) Add disocclusion reject. (4) Verify ghosting reduction on VoxelLab fast-yaw test scene.
  **Verify:** `MOTION` debug view shows correct motion. Fast yaw in VoxelLab: no ghosting, history correctly reset on disocclusion. ctest 16/16. Per `decisions.md §15` close-out rule: inspected runtime captures required (FINAL + SHDW + relevant debug views).
  **Acceptance:** TAA ghosting eliminated. Motion vector debug view functional. No regression in TAA quality for static camera.

---

## Stage 6 — Tech-debt & ECS refactor (parallel with Stages 2-5)

**Run in parallel with Stages 2-5.** Per the dependency-aware plan: retrofitting a 989-line god-function (`UpdateApp`) after adding 5 new systems is much more expensive than converting each new system to a Flecs system as it lands. Stage 6.1 should grow incrementally — convert the new physics system when Stage 3.2 lands, convert the new async streamer when Stage 1.3 lands, etc.

- [ ] **6.1. Flecs ECS migration (incremental)** — `src/app/AppUpdate.cpp::UpdateApp` + `src/physics/PhysicsWorld.cpp`

  **What:** Currently `UpdateApp` is a 989-line god-function with 60+ input actions and a 200-line manual mirror block. `PhysicsWorld` is procedural, passing `AppState` by reference. Both bypass ECS systems. Migrate incrementally: each new system (per Stages 1.3, 2.x, 3.x, 4.x, 5.x) is implemented as a Flecs system from the start. The god-function shrinks by attrition. Flecs is already in the project (per `agent/memory.md §4`).
  **Why:** Per `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md`: data-oriented systems, parallel-friendly, cache-friendly. Enables future multi-threading without a per-system rewrite. Doing this incrementally (vs. as a final cleanup) is the only way the migration stays tractable.
  **Files:** `src/app/AppUpdate.cpp` (split into per-system functions as new systems are added), `src/ecs/EcsWorld.{hpp,cpp}` (system registration for each new system), `src/physics/PhysicsWorld.cpp` (extract voxel-solver into a Flecs system when Stage 3.2 lands).
  **Approach:** (1) **Spike first**: convert the async audio scan (Stage 1.3) to a Flecs system. (2) When Stage 2.2 HZB cull lands, wrap its CPU-side bookkeeping in a Flecs system. (3) When Stage 3.2 Incremental Jolt lands, extract the per-chunk rebuild into a Flecs system. (4) Iterate: convert 1-2 systems per Stage 2-5 commit. (5) After all Stages land, the god-function should be small enough to split without a dedicated refactor session. (6) Optional: enable Flecs multi-threading (`ecs_set_target_fps` + `ecs_progress` multi-threaded mode).
  **Verify:** Behavior byte-identical (input replay fixtures). ctest 16/16. Per `decisions.md §10` rule: live walk bugs need fixed-step tests, not blind heuristic patches — same applies here.
  **Acceptance:** Each new system (Stages 1.3, 2.x, 3.x, 4.x, 5.x) lands as a Flecs system. After Stages 1-5 complete, `UpdateApp` is small enough that no dedicated refactor session is needed. Future multi-threading unblocked.

- [ ] **6.2. AppState PIMPL + `std::span` migration + r0 carry-overs** — `src/core/Types.hpp::AppState` + small refactors

  **What:** Several small refactors from old `TODO.md Tier 5` that weren't closed in the r0 pass:
  - **AppState PIMPL refactor** (`src/core/Types.hpp::AppState`): `AppState = std::unique_ptr<AppStateImpl>`, `AppStateImpl` owns `RenderContext + SimulationContext + BootstrapContext`. Reduces include bloat, clarifies ownership.
  - **`std::array` → `std::span`** for non-owning buffer views. After Stage 1.2 (SVDAG) and Stage 2.2 (HZB) settle, the remaining `std::array<T, N>` buffer-view sites can become `std::span<T>`.
  - **DDA shader template macro**: 3 copies of DDA trace in `voxel.frag` (`TraceLocalPointLightShadowRay`, `ComputeSunContactVisibility`, `TraceAmbientOcclusionRay`) — identical 12-step DDA, different occluder predicates. Macro `#define DDA_BODY(IS_OCCLUDER_FN)` substitutes 3 times.
  - **`vkWaitForFences` timeout**: per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` "low latency > throughput", `UINT64_MAX` → `10ms` on remaining call sites. Audit `src/render/Renderer.cpp` for current values.
  - **`// EVIL:` comments on magic numbers** per `legacy/docs/standards/04_evil-hacks-philosophy.md` §3. VoxelLab has many (0.05, 0.14, etc.) without EVIL markers.
  **Why:** Cleanup. None are critical; all improve maintainability. AppState PIMPL + `std::span` specifically unblock Stage 6.1 ECS migration (smaller `AppState` → cleaner ECS component contracts).
  **Files:** Various. Each sub-item is a separate small commit.
  **Approach:** One commit per sub-item. Each is small enough to land in a single session. Order: AppState PIMPL first (largest), then `std::span` migration, then the 3 small items (DDA macro, vkWaitForFences, EVIL markers).
  **Verify:** ctest 16/16 for each. No behavior change.
  **Acceptance:** Each sub-item closed individually. No behavior change.

---

## Cross-refs (for orientation)

- `agent/decisions.md §15` — CSM shadow path baseline (do not break; RTX = additive).
- `agent/decisions.md §30` — CPU Fluid CA reference (OUTDATED for new code, kept for test fixtures).
- `agent/decisions.md §30.1` — CA tick rate (20 Hz), `effectivePaused` gate, `timeScale` integration.
- `agent/decisions.md §30.4` — GPU Fluid CA binding contract (ping-pong + atomicOr + active chunk list).
- `agent/decisions.md §4` — Build / verification contract (build presets, ctest baseline, smoke policy).
- `legacy/docs/philosophy/01_foundation/05_decision-making.md` — design heuristics (data → algo → code; "low latency > throughput"; "if perf gain < 5-10%, choose simple").
- `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` — DoD, Flecs, greedy meshing.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — perf philosophy.
- `legacy/docs/philosophy/03_domain/04_testing-philosophy.md` — test coverage requirements for hot invariants.
- `legacy/docs/standards/04_evil-hacks-philosophy.md` — `// EVIL:` markers for magic numbers.
- `agent/active-sessions.md` — current session state; coordinate with parallel sessions via scope discipline.
