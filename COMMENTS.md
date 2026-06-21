# COMMENTS.md

External documentation for ProjectV source code. **Agent-managed** — added,
edited, and queried via the protocol described in `AGENTS.md` §11.

All `//` (C++) and `#` (CMake) comment blocks that previously lived in source
have been extracted here. Source files contain **only `keep` markers** (license
headers, IDE hints, EVIL markers, include-order markers, TODO/FIXME/XXX).

For git-archeology (refactor/bug-fix history of past commits), see `CHANGELOG.md`.
`COMMENTS.md` describes **current** code; `CHANGELOG.md` describes **past** changes.

Categories:
- `refactor-history` — git-archeology (`// **Tier X.Y (2026-MM-DD).** Removed; replaced by ...`)
- `design-rationale` — why this code exists / this choice was made. Cross-refs to
  `agent/knowledge.md Part A` (formerly decisions.md) and `agent/knowledge.md Part B` (formerly memory.md) are preserved verbatim.
- `intent` — what the code does / contract of a function, struct, or field.
- `test-narrative` — test scenario description (`// M5: ...`, `// **X axis.** ...`).

**Anchoring:** each entry has a line range (`L<start>-L<end>`). The line numbers
reflect the **file state at extraction time**. If code moves, this entry becomes
stale — re-anchor it (see `AGENTS.md` §10 for the stale-handling rule; was §9
pre-`2026-06-20` consolidation r0).

<!-- Last validated 2026-06-20 against src/core/Math.ixx (commit 44362d1) and src/core/StringId.ixx (same). L-anchors in entries below are still accurate. -->

**Querying:**

```bash
rg -A 20 '^## .src/core/Types.hpp.\$' COMMENTS.md
rg -B 1 '^### L.*design-rationale' COMMENTS.md
rg 'knowledge.md Part A §30' COMMENTS.md
```

---

## `src/core/Math.ixx`

### L22-L31 (intent)

\brief **Single-arg subscript** for the `camera.c[i]` /

\details
`result.c[i]` codepaths where the caller is

iterating a Vec3's components by index. Mirrors

the `glm::vec3[i]` convention. Without this,

`.c[0]`-style code in callers wouldn't compile.

### L51-L62 (intent)

\brief **Single-arg subscript** for the `mat * vec` and

\details
`mat[row, col]` codepaths where the caller is

iterating a Vec4's components by index. Without

this, `m.c[col][row]` would fail because Vec4

doesn't have a one-arg `operator[]` (Mat4 has

`operator[](col, row)`, not Vec4).

### L79-L88 (intent)

\brief **Column accessor** (the original `Math.hpp` form, kept

\details
for ABI/grep compat). `m.column(col)[row]` or

`m.c[col][row]` both work. Matches the column-major

convention used everywhere in the project

(`Renderer.cpp::InvertColumnMajorMat4`, `Camera.cpp`).

### L97-L102 (intent)

\brief **2-arg (col, row) accessor.** Returns the (row, col)

\details
element. Mirrors `glm::mat4[col][row]` and is what

`operator*(Mat4, Mat4)` uses internally.

### L111-L116 (intent)

\brief **Direct column access via .c[col].xyz.** Avoids the

\details
`glm::mat4[col][row]` ↔ `Mat4.c[col][row]` translation

friction.

### L129-L129 (intent)

\brief **Free functions.**

### L197-L212 (intent)

\brief **Gauss-Jordan elimination** on a 4x4 column-major matrix.

\details
Kept from `Math.hpp` (was the implementation

`Renderer.cpp::InvertColumnMajorMat4` was based on).

The output is the inverse of `m`; `m` itself is

unmodified. Degenerate (singular) input is detected

via the zero-pivot guard at the end of each column

step; the function falls through to `zero()` in that

case (the same fallback the pre-module code used).

### L217-L224 (intent)

\brief Augment with identity by overlaying onto the second

\details
half; for column-major, this means we treat the

4x8 working buffer as 4 rows × 8 cols (transposed view).

Easier: do row-wise Gauss-Jordan in transposed space.

### L237-L237 (intent)

\brief **Find a non-zero pivot row at or below `pivot`.**

### L387-L392 (intent)

\brief 4-element array → Vec3 (first 3 elements, sentinel

\details
w=0 on the Vec3 per the `_pad` field's default-init

contract).


## `src/core/StringId.ixx`

### L18-L27 (intent)

\brief **FNV-1a 64-bit basis.** Per

\details
http://www.isthe.com/chongo/tech/comp/fnv/. The basis

and prime are baked into constexpr helpers so the

hash is computed entirely at compile time for

`constexpr` callers.

### L34-L41 (intent)

\brief **Compile-time ctor for string literals.** Resolves

\details
to a single `mov` of the precomputed hash at the

call site; no init code emitted, no `.rodata`

string lookup.

### L47-L60 (intent)

\brief **Runtime ctor for `std::string_view`.** Used by

\details
env-var parsers (`ParseAssetManifestString`),

file loaders, and any path that doesn't have a

literal at the call site. The hash is identical to

the literal ctor for identical bytes, so

`StringID("rock")` from env equals

`StringID("rock")` literal.

### L65-L70 (intent)

\brief **Hashing helper.** Public so callers (e.g.

\details
`std::hash<StringID>`) don't have to inline the

FNV-1a arithmetic themselves.

### L81-L92 (design-rationale)

\brief **Reverse mapping.** Linear-scans a static table of

\details
literals for a matching `(hash, length)` tuple.

Returns the literal on hit, or `nullptr` (or a

fallback) on miss. Intended for UI / logging

only — the hot path uses `operator==` and never

needs the original string.

### L123-L140 (intent)

\brief **Specialise `std::hash<StringID>`** so the type can be

\details
used directly as `std::unordered_map<StringID, T>::key_type`

without a custom hasher.

NOTE on `export namespace std`: C++20 modules allow

`export namespace std { ... }` to add declarations into

the standard library's namespace from a module. Clang

22 accepts this pattern. The `template<>` is required

to make the specialisation distinguishable from the

primary template.


## `src/render/vulkan/VulkanMeshShaderPipeline.cpp`

### L1-L41 (design-rationale)

Pattern C mesh shader pipeline per `TODO.md §2.1` + `mesh-shader-vs-compute-cull` verdict=mixed
+ `agent/knowledge.md §10.11` per-vertex AO no-op contract. Three sub-pipelines share one
descriptor set layout (4 SSBOs): pre-cull compute + mesh-shader graphics + future pipelines.
Push-constant range 128 bytes (Vulkan min) covers VoxelMeshingPushConstants(64) +
viewProjection(64) exactly. `vkGetPhysicalDeviceMeshShaderFeaturesEXT.meshShader == VK_TRUE`
probed at init; graceful fallback to PackedFace indirect draw when absent or env unset.
Cross-vendor support: NVIDIA (RTX 30/40/50), AMD RDNA2/3/4, Intel Arc Battlemage+.

### L210-L240 (design-rationale)

`BuildMeshCullPushConstants` extracts 6 frustum planes from `ChunkCullingParameters` (camera
position, forward/right/up, FOV tangents, near/max distance). Planes unnormalized (so they
include camera position offset baked into `plane.w`) — shader uses linear radius scale, so
magnitude cancels out. Per `agent/knowledge.md §30.4` async-compute precedent for pre-cull
separation: cull runs as compute, draw runs as graphics, both gated by
`PROJECTV_MESH_SHADER_PIPELINE=ON`.

### L268-L340 (intent)

`RecordMeshShaderPreCull`: per-frame contract — CPU memsets `visibilityCounter` to 0 via
mapped memory, dispatch pre-cull compute with 6 planes + chunk count, barrier from
COMPUTE→MESH stage. Returns true if dispatch happened. Counter overflow safe (capacity =
chunk count, which is upper bound for visible set per camera frustum).

--
## `src/shaders/voxel_mesh.mesh`

### L1-L4 (design-rationale)

`#extension GL_EXT_mesh_shader : enable` per Vulkan 1.3 EXT (core in 1.3, ratified 2022-03-08).
Layout declaration `layout(triangles, max_vertices = 256, max_primitives = 256) out` is the
Vulkan 1.3 spec minimum for `VkPhysicalDeviceMeshShaderPropertiesEXT`. ProjectV chunkSize=8 →
worst case 6×8×8 = 384 isolated quads/chunk. Greedy merge reduces to <64 quads for typical
scenes. Bump `max_vertices`/`max_primitives` to per-device `maxMeshOutputVertices/Primitives`
if a real chunk exceeds the cap (would require dynamic specialization).

### L165-L255 (design-rationale)

`GreedyFacePass` is a 1:1 port of `voxel_mesh.comp::GreedyFacePass` adapted to mesh-shader
output: instead of writing to `packedFaces[]` SSBO, it writes to `gl_MeshVerticesEXT[]` +
`gl_PrimitiveTriangleIndicesEXT[]`. Per-vertex outputs match `voxel.vert:107-138` byte-for-byte
(outNormal, outWorldPosition, outMaterialIndex flat, outAmbientVisibility). 2-pass:
pre-count quads → call `SetMeshOutputsEXT(vCount, pCount)` → re-emit. This pattern is
required because `SetMeshOutputsEXT` must precede any output write (Khronos GLSL_EXT_mesh_shader
spec).

## `src/render/vulkan/VulkanFluidCaPipeline.hpp`

### L1-L48 (design-rationale)

Stage 3.1 GPU Fluid CA full pipeline integration per `TODO.md §3.1` + `agent/knowledge.md §30.4` 3-step migration precedent. `FluidCaPushConstants` (48 bytes) + `FluidCaGpuFrameStats` (16 bytes) cross-shader byte-exact contracts. Public API surface minimal: env-gate (`IsFluidCaGpuPipelineRequested`), pipeline lifecycle (`CreateFluidCaPipelines` / `DestroyFluidCaPipelines`), per-frame record (`RecordFluidCaDispatch`), cross-queue submit (`SubmitFluidCaToComputeQueue` via `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` + `renderTimelineSemaphore`), stats readback (`ReadFluidCaFrameStats` via `vmaInvalidateAllocation` mapped buffer). Atomic strategy: `fluid_ca.comp` uses `atomicOr` + bit-check (functionally equivalent to CAS for "set bit if unset" claim per `2026-06-21-gpu-fluid-ca-atomic-strategy` in-progress experiment).

## `src/render/vulkan/VulkanFluidCaPipeline.cpp`

### L1-L88 (design-rationale)

Constants block: 5 descriptor bindings (PackedChunkDescriptors / ActiveChunkIds / SourceFluidCells / DestinationFluidCells / FluidStats) match `fluid_ca.comp` layout 1:1; 5*MAX_FRAMES_IN_FLIGHT pool size for storage buffer descriptors; `kFluidCaStatsResetValue=0u` for `vkCmdFillBuffer` reset. Shader module loader mirrors `VulkanMeshShaderPipeline::CreateMeshShaderModule` pattern (extracted helper to avoid duplication with the mesh shader code path). `CreateFluidCaPipelines` does graceful fallback (returns false on missing spv or device creation failure; caller in `VulkanInit.cpp` logs informational and continues with CPU path per `agent/knowledge.md §30.4` Step 1).

### L120-L250 (design-rationale)

`CreateFluidCaPipelines` builds compute pipeline from `fluid_ca.comp.spv` via `vkCreateComputePipelines`. Pipeline layout uses single 5-binding descriptor set layout + 48-byte push constant range. Shader module named "FluidCaShader", pipeline layout "FluidCaPipelineLayout", pipeline "FluidCaPipeline", descriptor set layout "FluidCaDescriptorSetLayout" via `SetVulkanObjectName` for Validation Layer debug. Sets `render->fluidCaPipelineEnabled = true` on success. `DestroyFluidCaPipelines` tears down in reverse order (pipeline, pipeline layout, descriptor pool, descriptor set layout, shader module, all 4 per-frame buffers per SceneFrameResources). Safe to call on `pipelineEnabled=false` (no-op).

### L260-L380 (design-rationale)

`RefreshFluidCaResourceBindings` creates descriptor pool + allocates 1 descriptor set per `SceneFrameResources` + writes 5 `VkWriteDescriptorSet` entries (chunkDescriptorBuffer, fluidCaActiveChunkIdBuffer, fluidCaSourceBuffer, fluidCaDestinationBuffer, fluidCaStatsBuffer). Skips sets where any binding is null (graceful for frames with partial init). `RecordFluidCaDispatch` resets stats buffer via `std::memset` (mapped memory) + 3 pre-dispatch `VkBufferMemoryBarrier2` (stats fill + source + activeChunkId HOST→COMPUTE) + binds pipeline + descriptor set + push constants + `vkCmdDispatch(activeChunkCount, 1, 1)` + 2 post-dispatch barriers (stats + dest COMPUTE→HOST).

### L420-L530 (design-rationale)

`SubmitFluidCaToComputeQueue` uses `vkQueueSubmit2` with `VkCommandBufferSubmitInfo` + `VkSemaphoreSubmitInfo` wait on `renderTimelineSemaphore` (value = previous) + `VkSemaphoreSubmitInfo` signal on same semaphore (value = incremented). Bumps `context->renderTimelineValue += 1u` to advance the timeline. RAW hazard: compute→graphics (writeOutput → readInput) satisfied by semaphore signal+wait. Cross-queue submission ready but not yet wired in `Renderer.cpp` (current path uses main graphics command buffer for dispatch, then SubmitFluidCaToComputeQueue can route to dedicated compute queue once dedicated compute command pool is added — see `agent/workspace.md §2`).

### L540-L620 (design-rationale)

`ReadFluidCaFrameStats` invalidates the mapped stats buffer via `vmaInvalidateAllocation` + copies 16 bytes (4 × uint32: activeFluidCells / droppedFluidCells / iteration / reserved) for CPU-side debug HUD. Per `agent/knowledge.md §30.4` contract the stats are debug-only — count conservation invariant enforced by `fluid_ca.comp:101-105` `atomicOr` + bit-check, not by stats counter.

## `src/voxel/VoxelLodDownsample.{hpp,cpp}`

### L1-L132 (design-rationale)

Stage 4.2 LOD chunk 2 B_SurfacePreserve downsampling kernel + per-chunk `LodDownsampleJob` orchestrator per `2026-06-21-lod-mesh-downsampling` verdict=mixed. Lives in `projectv::voxel` namespace (separate from `voxel/VoxelWorld.cpp` to minimize transitive include cost for the test target). `LodDownsampleStepForLod` maps LOD 0/1/2/3 → step 1/2/4/8 (per `SelectLodLevelForDistance` distance thresholds <32m/<64m/<128m/≥128m). `LodDownsampledExtentForLod` returns `chunkSize/step` (clamped to ≥1 for safety). `SurfacePreserveVote8` reads step³ source voxels in fixed `sz,sy,sx` order, returns first non-Air material found OR Air if all step³ are Air — 0 T-junction holes across 75 boundary configurations per experiment. `DownsampleChunkForLodSurfacePreserve` allocates `outDownsampled` of size `outExtent³`, populates from `chunk.min` origin. `RunLodDownsampleJobs` iterates all chunks, calls downsample, sets `lodDownsampledNonAirCount` byte. `IsLodDownsampleEnabled` env gate (`PROJECTV_LOD_DOWNSAMPLE=ON`, default OFF).

## `src/physics/GreedyPhysicsMerger.{hpp,cpp}`

### L1-L200 (design-rationale)

Stage 3.3 Greedy Physics Meshing integration per `2026-06-21-greedy-physics-meshing-cpu` verdict=yes (D_3D greedy merge algorithm, 35× shape reduction, 100% volume preservation). `MergedVoxelBox` struct holds min/max-exclusive extents in voxel coordinates. `GreedyMergeSolidVoxelsInBounds` algorithm: for each (x,y,z) in fixed `z,y,x` ascending order, find max X extent (X+), then max Y extent over X-range (Y+), then max Z extent over XY-range (Z+), mark consumed via byte mask, emit one `MergedVoxelBox` per maximal extents. `IsSolidAt` inline helper checks `IsPhysicsSolidMaterial` (Glass + FloorWhite + FloorGray; Air + Fluid return false). `IsGreedyPhysicsMeshEnabled` env gate (`PROJECTV_GREEDY_PHYSICS_MESH=ON` default; `=OFF` falls back to naive per-voxel loop in PhysicsWorld.cpp). Both `BuildStaticVoxelCollisionBody` and `BuildChunkStaticCollisionBody` (per-chunk incremental Jolt) integrate greedy merge. Per-chunk rebuild path uses greedy merge for new compound shape. Tests cover empty world, single voxel unit box, full chunk single box, volume preservation (sum of merged box volumes equals solid voxel count), mixed half-chunk reduction, fluid+air ignored, oversized bounds clamp to world extents.

## `src/voxel/ChunkStreamer.{hpp,cpp}`

### L1-L80 (design-rationale)

Stage 4.3 Chunk Streaming foundation Step 1 per `2026-06-21-voxel-chunk-streaming-pipeline` (in-progress experiment, closed mixed verdict expected). Interface contract: `ChunkStreamRequest` (chunkIndex + priority), `ChunkData` (voxelBytes + nodeWords vectors), `EnqueueChunkStreamRequest` (mutex-guarded enqueue), `DrainChunkStreamQueueSize` (peek queue depth), `TryDequeueChunkData` (returns `std::expected<ChunkData, ChunkStreamError>` for thread-safe dequeue). `ChunkStreamError` enum covers `QueueFull` + `InvalidChunk` + `NotInitialized`. `IsChunkStreamingEnabled` env gate (`PROJECTV_CHUNK_STREAMING=ON` default; `=OFF` returns `NotInitialized` from TryDequeue). Pending and ready deques are mutex-protected via static-local `std::mutex` instances. Cold-path per `agent/knowledge.md §29.0` (`std::expected<T, E>` for I/O). Background thread + SSD read integration deferred to dedicated session — interface is in place, ready for `ChunkStreamer::ProcessPendingRequests()` background worker.

## `src/render/vulkan/VulkanWorldGenPipeline.{hpp,cpp}`

### L1-L300 (design-rationale)

Stage 4.1 GPU World Gen dispatch infrastructure per `2026-06-21-gpu-procedural-noise-compute-kernels` verdict=mixed (CC0 OpenSimplex2 3D-S recommended). `WorldGenPushConstants` (64 bytes, static_assert'd) packs chunkOriginAndChunkSize (ivec4) + chunkCountAndFlags (uvec4) + noiseParams (vec4) + seed (uint) + reserved (3× uint). Compute pipeline from `world_gen.comp.spv` via `ReadShaderFile` + `vkCreateComputePipelines`. 1-binding descriptor set: storage buffer at binding 0 (writeonly voxel buffer). `BuildActiveChunkIdsForWorldGen(world, outChunkIds)` helper filters out non-empty chunks (only generates voxels for chunks with `nonAirVoxelCount == 0`). `RecordWorldGenDispatch(commandBuffer, render, frameResources, pushConstants, activeChunkCount)` does HOST→COMPUTE buffer barrier + bind pipeline + bind descriptor set + push constants + `vkCmdDispatch(activeChunkCount, 1, 1)`. `RefreshWorldGenResourceBindings` allocates 1 descriptor set per frame + 1 storage buffer write. Per-frame SSBO capacity = `sizeof(uint32_t) * 8³ * max(chunks.size(), 1)`. `IsWorldGenGpuPipelineRequested` env gate (`PROJECTV_WORLD_GEN_GPU=ON` default; `=OFF` short-circuits before shader load). `IsWorldGenGpuPipelineRequested` is `inline` in header for testability without linking the .cpp.

## `src/render/TaaRenderTargets.{hpp,cpp}` (12x updates)

### L44-L65 (design-rationale)

12x Phase 3 added motion vector + history render targets per `2026-06-21-taa-motion-vectors` verdict=yes Pipeline A. `kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT` (Karis 2014 "16:16 RG velocity buffer"). VRAM cost 8 MiB/frame double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3`. `CreateOrRecreateTaaRenderTargets` signature extended with 2 new `OffscreenColorTarget&` params (`motionVectorColor` + `motionVectorHistoryColor`). `TransitionTaaMotionVectorForSample` transitions MOTION_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL with COLOR_ATTACHMENT_WRITE → SHADER_SAMPLED_READ access. `RecordTaaMotionVectorHistoryCopy` does scene→history transfer with full barrier chain (matches `RecordTaaHistoryCopy` pattern for scene color). `DestroyTaaRenderTargets` extended with 2 new destroy targets. Note: this data path is complete; `taa_resolve.frag` integration (consume MV texture instead of computing from prevViewProjectionMatrix in-shader) is deferred to dedicated session — `agent/workspace.md §2` Nearest Gap.

## `src/render/vulkan/VulkanBootstrap.cpp`

### L447-L454 (intent)

`PhysicalDeviceCandidate` gained `meshShaderFeatures` (VkPhysicalDeviceMeshShaderFeaturesEXT)
+ `supportsMeshShader` (bool). Probed in `CheckRequiredFeatures` via pNext chain; only
queried if `HasDeviceExtension(physicalDevice, "VK_EXT_mesh_shader")` returns true (avoids
spurious pNext struct ignored on devices without extension).

### L743-L748 (design-rationale)

`PROJECTV_MESH_SHADER_PIPELINE=ON` env var gates `deviceExtensions.push_back(kMeshShaderExtension)`
+ `enabledMeshShaderFeatures{meshShader=VK_TRUE, taskShader=VK_TRUE}` chaining in
`VkDeviceCreateInfo::pNext`. Per `agent/knowledge.md §32` Pattern C contract, feature is
opt-in. When env unset, device is created without the extension — same mainline as before.
Both `meshShader` and `taskShader` enabled together because Pattern C uses task shader only
indirectly via compute pre-cull, but the feature must be linked for the pipeline to compile.

### L459 (intent)

`BuildEnabledFeatures12` now enables `timelineSemaphore` feature (was previously missing →
validation error on `vkCreateSemaphore` with `VK_SEMAPHORE_TYPE_TIMELINE`). This caused
`renderTimelineSemaphore` to leak on shutdown (created but never destroyed because the
device rejected the create call silently, OR the destroy was simply missing). Fixed in
`ShutdownVulkan` (Types.cpp L88-91) by adding explicit `vkDestroySemaphore` for
`renderTimelineSemaphore`.

### L470 (intent)

`BuildEnabledFeatures13` now enables `shaderDemoteToHelperInvocation` feature. Per
`agent/knowledge.md §15` lighting contract, `voxel.frag` uses `demote_to_helper` extension
for branchless shadow path. Without this feature enabled, validation layer reports
`SPIR-V Capability DemoteToHelperInvocation was declared` and the shader may behave
unexpectedly on drivers that optimize differently.

## `src/shaders/taa_resolve.frag`

### L1-L8 (design-rationale)

Binding 4 = `sampler2D motionVector` added in 4x session. Replaces depth-reproject path
(Karis 2014 Pipeline A). Per `2026-06-21-taa-motion-vectors` experiment verdict=yes, the
motion vector texture is written by `voxel.frag:903` as `prevNdc - currNdc` in [0,1] UV space.
The TAA resolve consumes it as `prevUv = uv + motion`, no world-space reconstruction needed.
This eliminates the 2 mat4 multiplies + world-position reconstruction in the original
depth-reproject path (15-25 cycles per pixel on RTX 3060 Ti). Binding 2 = `sampler2D depth`
retained for ABI compatibility but unused in main flow.

## `src/render/vulkan/TaaResolvePipeline.cpp`

### L14-L60 (design-rationale)

4x session extended `kTaaResolveDescriptorBindings` from 5 to 6 elements. Binding 4 added for
motion vector sampler (`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`). Pool size bumped 3→4
samplers per frame. `motionVectorImageInfo` written alongside `sceneColorImageInfo` +
`historyColorImageInfo` + `depthImageInfo`. Precondition check requires
`taaMotionVectorTarget != nullptr` (added in 4x Phase 1). The 4th `VkWriteDescriptorSet`
matches `taa_resolve.frag` binding 4 declaration.

## `src/voxel/ChunkStreamer.cpp`

### L98-L168 (design-rationale)

4x session added `std::jthread` background worker (C++20) with `std::stop_token` cooperative
cancellation per cppreference docs. The worker pops pending `ChunkStreamRequest`s from
the mutex-protected queue, reads binary file `chunk_<index>.bin` from
`PROJECTV_CHUNK_PATH` (default: `<build>/cache/chunks/`), pushes `ChunkData` to the
ready queue. File format: 16-byte header (magic `0x504B5631` = "PKV1" little-endian +
uint32 version `1` + uint64 voxel byte count) + serialized voxel bytes. `EnqueueChunkStreamRequest`
calls `StartChunkStreamerWorker` lazily via `compare_exchange_strong` atomic guard.
`StopChunkStreamerWorker` calls `thread.request_stop()` + `thread.join()`. TSan-clean
expected since all shared state is mutex-protected or atomic.

## `src/voxel/ChunkStreamer.hpp`

### L11-L37 (design-rationale)

`ChunkStreamError` enum extended with `FileNotFound` (3) + `FileReadFailed` (4) for
the 4x session background worker. Public API: `StartChunkStreamerWorker` +
`StopChunkStreamerWorker` + `IsChunkStreamerWorkerActive` + `GetChunkStreamerCachePath` +
`ProcessPendingRequests(std::stop_token)` (the worker function passed to `std::jthread`).
Cold path: uses `std::expected<ChunkData, ChunkStreamError>` per `agent/knowledge.md §29.0`.

## `src/render/HizCulling.{hpp,cpp}`

### L17-L82 / L17-L86 (design-rationale)

4x session extended HZB culling to per-chunk mip level selection. `kHizCullingDescriptorBindings`
5→6 elements (added binding 5 = `perChunkMipLevels` SSBO). Pool size bumped 3→4 storage
sets per frame. Per `2026-06-21-hzb-smart-mip-select` experiment verdict=mixed:
- `IsHzbSmartMipEnabled()` env gate (`PROJECTV_HZB_SMART_MIP=ON`, default OFF) preserves
  mainline behavior. When OFF, push constant mipLevel=0 is used (per-chunk SSBO ignored
  because the shader checks `perChunkMip > 0`).
- `ComputePerChunkMipLevelCpu(projectedXTexels, projectedYTexels, maxMipLevel)` uses
  Turitzin 2020 formula `mip = floor(log2(max(projX, projY)))`. Standard mip-of-N texels
  per occlusion-test heuristic from `Hierarchical Depth Buffers` Miketuritzin.com blog.
- `ComputePerChunkMipLevelsFromAabbs` projects 8 AABB corners via the viewProjection
  matrix (column-major `std::array<float, 16>`), computes per-chunk projected screen-space
  extent, applies the formula. Returns count processed. Designed to be called once per
  frame in `FramePreparation.cpp` (wired separately).
- `hzb_cull.comp` **2-phase fallback**: `if (!visible && perChunkMip > 0) { visible = AabbVisibleAgainstMip(...0...); }`
  verifies culled chunks at mip=0. Eliminates 0.02-0.20% FN per the experiment (C_PerChunkStaticMip
  smart mip alone had worst-case 30dB PSNR; 2-phase fallback recovers ∞ dB with 350× texel
  reduction retained).

## `src/render/Renderer.cpp` (4x changes)

### L1369-L1448 (design-rationale)

4x Phase 2: World gen dispatch wired in `DrawFrame` after Fluid CA. Uses
`BuildActiveChunkIdsForWorldGen` to filter empty chunks (`nonAirVoxelCount == 0`),
zero-fills the per-frame SSBO via `std::memset` (mapped memory), populates
`WorldGenPushConstants` (chunkOriginAndChunkSize, chunkCountAndFlags, noiseParams with
`{0.5, 0.5, 4u, 2.0}` = 4-octave FBM with persistence 2.0, seed = `simulationTick` for
deterministic per-frame variation), calls `RecordWorldGenDispatch`. Skip if
`worldGenChunkCount == 0` (zero active chunks = no GPU work).
4x Phase 1: removed `inverseCurrentViewProj` calculation since motion vector path
doesn't need current→world unprojection. `currentViewProjection` push constant retained
for ABI compatibility.

## `src/render/SceneResources.cpp` (4x changes)

### L671-L675 (design-rationale)

4x Phase 4: added `hzbPerChunkMipBuffer` alloc + destroy + structured-binding entry.
Buffer size = `sizeof(uint32_t) * max(chunks.size(), 1u)` (1 uint32 per chunk for mip
level). Capacity check emits `LogRuntimeFailure` if chunks exceed capacity (matches
the NanoVDB pattern). Nullify block sets `hzbPerChunkMipMappedData = nullptr` + buffer
to `VK_NULL_HANDLE` + allocation to `nullptr` + capacity to `0u`.

## `src/app/FramePreparation.cpp` (4x changes)

### L122-L137 (design-rationale)

4x Phase 3: per-frame chunk stream drain with budget `kMaxChunksPerFrame = 8u`. Drains
up to 8 ready chunks per frame, populates chunks into the voxel world. Tracy plots
`Chunk Stream Drained` (count drained) + `Chunk Stream Pending` (queue depth). Gated
on `IsChunkStreamingEnabled()`. Throttles per-frame SSD read pressure (avoids 60Hz frame
budget spikes when many chunks become ready simultaneously).


## `src/render/vulkan/VulkanAsyncCompute.hpp`

### L1-L28 (design-rationale)

Stage 6.3 per-pass async compute wiring per `TODO.md §6.3` + `agent/knowledge.md §30.4`
3-step migration precedent. New file (4x session, this section). Public API:
- `IsAsyncComputeResourcesAllocated(context)` — predicate for early-out in `DrawFrame`
  routing.
- `EnsureAsyncComputeResources(context)` — creates dedicated compute command pool
  (`VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`,
  `queueFamilyIndex = context->dedicatedComputeQueueFamilyIndex`) and allocates 1
  one-shot `VkCommandBuffer` per the nvpro-samples transient pool pattern. Returns
  false if dedicated compute queue is unavailable (graceful fallback to graphics
  queue).
- `DestroyAsyncComputeResources(context)` — symmetric destroy.
- `RecordAsyncComputePass(asyncCB, context, render, state, frame)` — orchestrator that
  records Fluid CA + world gen dispatches into the async CB. Returns false if nothing
  was dispatched. Skips HZB (deferred — cross-queue depth sync needs separate timeline).
- `SubmitToComputeQueue(context, commandBuffer, outTimelineValue)` — generalized
  `vkQueueSubmit2` helper that bumps `context->renderTimelineValue` +1 and waits on
  previous value / signals new value via `renderTimelineSemaphore`. Reuses the
  pattern from 8x Phase 4 `SubmitFluidCaToComputeQueue` so existing timeline
  semantics are preserved.

Env gate: `PROJECTV_ASYNC_COMPUTE=ON` (default OFF per `agent/knowledge.md §30.4`
Step 1 additive optional path precedent). When OFF, `Renderer.cpp::DrawFrame` falls
back to per-pass main graphics command buffer recording (current mainline behavior).

## `src/render/vulkan/VulkanAsyncCompute.cpp`

### L1-L50 (design-rationale)

`EnsureAsyncComputeResources` mirrors `VulkanMeshShaderPipeline::CreateMeshShaderModule`
extraction pattern (helper for transient resource setup). Pipeline barrier pattern
follows Vulkan 1.4 `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` per
`docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit2.html` + nvpro-samples
async compute pattern. `record -> submit -> consume` data flow matches the
canonical 3-stage timeline per `agent/knowledge.md §30.4`.

### L60-L120 (design-rationale)

`RecordAsyncComputePass` body mirrors the per-pass blocks in `Renderer.cpp::DrawFrame`
(Fluid CA + world gen only — HZB deferred). Identical push-constant population as
the inline graphics path; this is by design so the recording is byte-equivalent
regardless of which queue runs it. `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` per
the spec §6.4 command buffer usage flags; implicit `vkResetCommandBuffer` on next
`vkBeginCommandBuffer`.

### L130-L200 (design-rationale)

`SubmitToComputeQueue` reuses `context->renderTimelineValue` (single shared
timeline). Per `agent/knowledge.md §30.4` 3-step migration: this is Step 1 (additive
optional path, default OFF). Bumps timeline by 1; new value goes to caller via
`outTimelineValue` for downstream graphics submit `VkSemaphoreSubmitInfo` wait
(VUID-VkSubmitInfo2-semaphore-03881: signal value > wait value). Failure path
restores the bumped value so subsequent submits continue from a clean state.

## `src/render/Renderer.cpp` (this session)

### L1380-L1383 (design-rationale)

This session: `asyncComputePathActive` predicate computed once per frame.
Gated on `IsAsyncComputeEnabled() && IsAsyncComputeResourcesAllocated(*context) &&
(render->fluidCaPipelineEnabled || render->worldGenPipelineEnabled)`. Last
conjunct avoids unnecessary async work when no compute pass is enabled.

### L1385-L1414 (design-rationale)

This session: Fluid CA + world gen dispatches on graphics CB wrapped in
`if (!asyncComputePathActive && ...)`. When async is ON, these dispatches are
skipped on graphics CB and recorded into the dedicated async compute CB instead.
HZB dispatch on graphics CB is unchanged (deferred cross-queue depth sync).

### L1500-L1530 (design-rationale)

This session: After `vkEndCommandBuffer(graphicsCmd)`, async compute submit path
runs first. On success, `context->asyncComputeLastTimelineValue` is updated with
the new timeline value. Then graphics submit adds a 2nd `VkSemaphoreSubmitInfo`
wait on `renderTimelineSemaphore` at value = `asyncComputeLastTimelineValue` so
graphics consumes the previous frame's async compute result (1-frame pipeline
depth, the canonical nvpro-samples pattern).


## `src/render/LodDownsampleGpuConsume.hpp`

### L1-L25 (design-rationale)

Stage 4.2 LOD GPU consume infrastructure per `TODO.md §4.2` + `agent/knowledge.md §30.4`
3-step migration. New file (this session, this section). Public API:
- `IsLodDownsampledGpuConsumeEnabled()` — env gate predicate (`PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=ON`,
  default OFF per additive optional path precedent).
- `ComputeLodDownsampledVoxelPayloadBytes(chunkCount, chunkSize)` — capacity helper for worst-case
  downsampled extent (`chunkSize/2` clamped to ≥1) cubed, capped at 64 MiB safety.
- `ComputeChunkLodLevelsCapacity(chunkCount)` — capacity helper, `max(chunkCount, 1)` floor.
- `RefreshLodDownsampledBuffers(context, render, world)` — per-frame upload helper.

## `src/render/LodDownsampleGpuConsume.cpp`

### L1-L70 (design-rationale)

`RefreshLodDownsampledBuffers` writes per-chunk `lodLevel` (uint8 → uint32 packed) to
`chunkLodLevelsBuffer` SSBO from `world.chunks[i].lodLevel`. Zeros the
`lodDownsampledVoxelPayloadBuffer` SSBO. Capacity check emits `LogRuntimeFailure`
on overflow (matches the NanoVDB pattern). Bumps `render->lodDownsampledPayloadVersion`
on success. This is the GPU consume infrastructure only — actual mesh emission from
the downsampled payload is deferred (GreedyFacePass needs per-chunk extent parameterization).

## `src/voxel/ChunkStreamer.{hpp,cpp}` (this session changes)

### Prebake API (design-rationale)

This session Step 3 partial: `BakeAllChunksToDisk(world, outStats)` cold-path API
that iterates chunks, serializes each chunk's `chunkSize^3` material grid via
`world.sparseStorage.GetCell(x, y, z)`, writes to `chunk_<index>.bin` with the same
16-byte header format as the existing reader. `ChunkPrebakeStats` struct reports
`chunksBaked` + `chunksSkipped` + `totalVoxelBytes`. `IsChunkStreamerPrebakeReady()`
+ `GetChunkStreamerPrebakeVersion()` expose a monotonic atomic `prebakeVersion`
counter. `PreloadChunksAroundCamera(cameraX, cameraY, cameraZ, radiusChunks)`
iterates grid cells within radius, computes `linearIndex` via
`gz * gridHeight * gridWidth + gy * gridWidth + gx` (matches
`VoxelWorld::chunks` storage order), enqueues high-priority `ChunkStreamRequest`
for each. Grid bounds check via `world.width/height/depth` clamps negative or
out-of-range grid coords. All three functions gated on `IsChunkStreamingEnabled()`.

## `src/render/HizCulling.{hpp,cpp}` (this session changes)

### Smart blend width v2 (design-rationale)

This session partial: `IsHzbSmartBlendWidthEnabled()` env gate
(`PROJECTV_HZB_SMART_BLEND_WIDTH=ON`, default OFF) +
`ComputeBlendWidthForChunkMip(projectedXTexels, projectedYTexels, mipLevel, maxBlendWidth)`
CPU helper computes `texelsAtMip / 4 + frac / 8` bounded by `maxBlendWidth`.
`ComputePerChunkMipAndBlendWidthsFromAabbs` produces a packed output vector
(2 uint32 per chunk: `[mip, blendWidth, mip, blendWidth, ...]`) used for SSBO
struct change deferred to follow-up. Reuses the existing 8-corner AABB projection
from `ComputePerChunkMipLevelsFromAabbs` for visual consistency.

### Smart blend width v2 — full shader consume (8x)

SSBO `hzbPerChunkMipBuffer` enlarged from 1×uint32/chunk to 2×uint32/chunk
(`kHizMipAndBlendWidthWordsPerChunk = 2`). `hzb_cull.comp::AabbVisibleAgainstMip`
takes new `blendWidthTexels` parameter; when > 0, expands the screen-space
sample footprint by `blendWidth / mipSize` before texel fetch, eliminating
0.02-0.20% false-negatives at lower mips per `2026-06-21-hzb-smart-blend-width`
verdict. `WritePerChunkMipAndBlendWidthsToBuffer` helper is a pure packer —
no Vulkan deps, testable in isolation. When `IsHzbSmartBlendWidthEnabled()` is
OFF, callers still write `blendWidth=0` and the shader falls back to the
4x default path (no smart blend expansion).

## `src/shaders/voxel_mesh.comp` (8x: LOD mesh emission)

`kLodWordStride = 16` (chunkSize=8, LOD 1 worst case: outExtent=4, 64 bytes =
16 uint32 words). Must match `projectv::render::kLodPayloadWordStride` in
`LodDownsampleGpuConsume.hpp`. `GetChunkLodLevel(chunkIndex)` /
`GetChunkLodExtent(chunkIndex)` decode the packed `chunkLodLevelsBuffer`
(bits [0:8] = lodLevel, bits [8:16] = outExtent). `DecodeVoxelMaterialForLod`
dispatches to `DecodeChunkVoxelMaterial` (LOD 0, full-res) or
`DecodeLodVoxelMaterial` (LOD >0, downsampled payload at `lodDownsampled`
binding 9). `GreedyFacePass` uses per-chunk extent from metadata instead of
the original `chunkDescriptor.chunkExtentAndNonAir[axisN]` when
`chunkLodLevel > 0` — without this, lower-LOD chunks would iterate the
original 8³ extent and read garbage from the smaller downsampled payload.

## `src/render/vulkan/VulkanAsyncCompute.{hpp,cpp}` (8x: HZB async cross-queue)

8x Variant 1 Phase 4 introduced the 2nd timeline semaphore
`VulkanContextState::hzbBuildTimelineSemaphore` (`VkSemaphoreTypeCreateInfo`
TIMELINE, initialValue=0) + `hzbBuildLastTimelineValue` counter, created in
`VulkanBootstrap::InitializeVulkanBase` after `renderTimelineSemaphore`,
destroyed in `Types.cpp::ShutdownVulkan`. `RecordHzbAsyncCullPass` records
HZB cull into `asyncComputeCommandBuffer` (re-uses existing
`RecordHzbCullingDispatch` from `HizCulling.cpp`) after a memory barrier
that crosses the graphics→compute timeline. `SubmitHzbAsyncCullToComputeQueue`
submits with cross-queue wait/signal on `hzbBuildTimelineSemaphore`
(1-frame pipeline depth, matches the existing Fluid CA pattern). `Renderer.cpp`
adds 2nd `VkSemaphoreSubmitInfo` signal on graphics submit at value
`hzbBuildLastTimelineValue` when `asyncComputeHzbPathActive` is true (env
`PROJECTV_ASYNC_COMPUTE=ON` + HZB culling enabled + HZB buffer allocated),
and skips the HZB cull on graphics CB in that case.

8x Variant A Phase 1 (this session) replaced the placeholder barrier with a
proper cross-queue memory barrier: `srcStageMask = TRANSFER_BIT`,
`srcAccessMask = TRANSFER_WRITE_BIT` (the HZB image was last written by the
graphics mip chain build), `dstStageMask = COMPUTE_SHADER_BIT`,
`dstAccessMask = SHADER_READ_BIT`, layout stays `SHADER_READ_ONLY_OPTIMAL`.
Uses `VK_QUEUE_FAMILY_IGNORED` for both src/dst which is correct for the
current `VK_SHARING_MODE_EXCLUSIVE` HZB image when the memory dependency
is provided by the cross-queue timeline semaphore + barrier (execution +
memory respectively). Per Khronos Synchronization Examples, the deeper
ownership-transfer barrier pattern with explicit `srcQueueFamilyIndex` →
`dstQueueFamilyIndex` is only required for `VK_SHARING_MODE_EXCLUSIVE`
images where the timeline semaphore alone is insufficient — current
single-barrier pattern closes the previously-deferred ownership sync
sufficient for the current 1-frame async pipeline depth.
`COMPUTE_SHADER_BIT`. Deferred to a follow-up session to keep this phase's
risk contained.

## `src/voxel/NanoVdb.hpp` (8x: resize capacity math)

`ComputeGrownNanoVdbCapacityForTest(current, required)` is a test-only
inline mirror of the production `projectv::render::ComputeGrownNanoVdbCapacity`
in `SceneResources.cpp`. Lives in the public header so
`ProjectVNanoVdbGpuUploadTests` can exercise the grow strategy (1.5× current
or required, whichever is larger; zero current → required) without
needing to link the full SceneResources module (which would pull in modules
and Vulkan deps). Test verifies: zero current → returns required; smaller
required → keeps current; larger required → grows by 1.5× AND satisfies
required. Production implementation in SceneResources.cpp allocates a new
VMA buffer, frees the old one, and re-uploads the flatten data via the
same `RefreshNanoVdbFlattenBuffers` path.

## `src/voxel/VoxelWorld.cpp` (8x: physics boundary-neighbor queue)

`SetVoxelMaterial` now calls `QueueChunkRebuildRequest(physics, chunkIndex)`
for the edited chunk AND all 6 face-sharing boundary neighbors (when the
edit sits on a chunk face). Previously only the center chunk was queued,
which meant a voxel on a chunk boundary would leave the neighbor chunk's
CompoundShape out of sync with the actual voxel data — players could
fall through the world near chunk edges. Mirrors the existing visual
rebuild range in `MarkChunksTouchedByVoxelEditDirty` (iterates the same
neighbor cube `for (z,y,x)` loop). Both rebuild paths use the same
boundary-detection logic (compare `position[axis]` against
`chunk.min[axis]` / `chunk.maxExclusive[axis] - 1`).

## `src/render/SceneResources.cpp` (8x: NanoVDB grow-on-exceed)

`ComputeGrownNanoVdbCapacity` + `GrowNanoVdbBuffer` close the
`UploadSceneFrameResources` NanoVDB `CapacityExceeded` log path. When
`sceneNanoVdbFlatten` exceeds current Upper/Lower/Leaf/Material buffer
capacity, each under-sized buffer is freed via `vmaDestroyBuffer` and
re-allocated with the grown capacity. Capacity grows by 1.5× current or
required (whichever is larger), zero-current falls back to required. Old
data is NOT preserved across grow (cold-path; per-frame `sceneNanoVdbFlatten`
is re-built from the world before each upload, so no copy is needed).
Tracy plot `"SceneNanoVdbUpperBufferAllocation"` and equivalents
track the new alloc/free cycle. `UploadSceneFrameResources` now takes a
`VulkanContextState *context` parameter (caller updated in
`FramePreparation.cpp`); without the context, the VMA destroy/recreate
cannot run.

## `src/shaders/voxelize.comp` (8x Variant A: VCT 3D clipmap injection)

Per-voxel scene injection into a 3D clipmap texture for Voxel Cone Tracing (VCT)
indirect lighting. `kVoxelizeWorkgroupSize = 64` (8x8x1 workgroup). One workgroup
per chunk: 64 threads iterate over the chunk's voxels in a strided loop
(`for voxelIdx = gl_LocalInvocationIndex; voxelIdx < totalVoxels; voxelIdx += 64`).
Per `WickedEngine` VXGI (turanszkij) per-chunk dispatch + `Compix
VoxelConeTracingGI` clipmap layout. No thread-write race because `voxelIdx`
is unique per thread. Image format `rgba16f` (signed-half 4 channels,
HDR-capable). Reads `PackedChunkDescriptors` (binding 0) +
`PackedChunkVoxelPayload` (binding 1); writes per-voxel emission to
`vctClipmap` (binding 2, writeonly). Air + Glass voxels skipped
(material == 0 || material == 1 → continue) per TODO.md §5.1 implicit
caveat (transparent voxels don't contribute to VCT specular in this
implementation; deferred to Stage 5.2 RTX path for rough<0.3).

Push constants (48 bytes, `VoxelizePushConstants`): `clipmapOriginAndResolution`
(origin XYZ + resolution W), `chunkCountAndFlags` (chunkCount, mipLevel, 0, 0),
`chunkGrid` (gridX, gridY, gridZ, 0). Same `WorldPositionToClipmapCoord`
math as WickedEngine clipmap addressing (origin = clipmap world center,
halfRes = resolution/2 offset, clamp to [0, resolution-1]).

## `src/render/vulkan/VulkanVoxelizePipeline.{hpp,cpp}` (8x Variant A)

VCT compute pipeline infrastructure. `IsVctGpuPipelineRequested()` env gate
(`PROJECTV_VCT_GPU=ON`, default OFF per `agent/knowledge.md §30.4` Step 1
additive optional path). `CreateVoxelizePipelines` lazy-allocates:
- 3D image `vctClipmapImage` (256³ RGBA16F, 4 mip levels, 16 MiB VRAM)
  with `VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT` (sampled
  in fragment shader, written by voxelize compute, blit for mip chain).
- Linear filter sampler with mip range [0, mipLevelCount] for trilinear
  cone tracing.
- Compute pipeline + 3-binding descriptor set (chunk descriptors + voxel
  payload + clipmap storage image). Pool size 2×MAX_FRAMES_IN_FLIGHT
  storage buffers + 1×MAX_FRAMES_IN_FLIGHT storage images.

`RecordVoxelizeDispatch` calls `vkCmdDispatch(activeChunkCount, 1, 1)` — one
workgroup per chunk (matches voxelize.comp dispatch pattern). Skips if
`activeChunkCount == 0` (avoids GPU validation warning on zero dispatch).
`BuildVctClipmapMipChain` uses `vkCmdBlitImage` with `VK_FILTER_LINEAR`
for 3D-to-3D mip reduction (Mip N → Mip N+1). Mirrors `BuildHizMipChain` 2D
pattern in `HizCulling.cpp:295-476`. Each mip barrier transitions
`TRANSFER_WRITE → SHADER_READ` for the next mip.

`ProjectVVoxelizePipelineTests` NEW (11 sub-tests): env gate default/off/on,
`VoxelizePushConstants` size = 48 (16-byte align), null context rejection,
null CB rejection, empty active chunks, empty render state guard, mip
chain null CB, mip chain empty clipmap. Graceful fallback on shader load
failure or device creation failure (returns false, caller in `VulkanInit.cpp`
logs informational and continues with VCT disabled per §30.4).

## `src/shaders/voxel.frag` (8x Variant A: VCT diffuse + specular cone tracing)

VCT cone integration into the main lighting path. Env gate: `vctParams.w > 0.5`
(zero by default = VCT disabled, no-op fallback). When enabled, 6 fixed
diffuse cones (`kVctConeDirections[6]`) trace the world clipmap with
3-tap adaptive sampling per cone (weight = 1/(1 + falloff * i)), max mip
selection by `log2(maxT) * 0.5` clamped to `kVctMaxMipLevel=4`. Specular cone:
`VctSampleReflectionCone` reflects view direction around normal with
aperture `roughness * 0.6` clamped to [0.05, 0.6]; gated by
`roughness > kVctCutoffRoughness=0.3` per `2026-06-20-vct-vs-rt-cutoff`
experiment (rough surfaces use VCT specular, smooth surfaces use Stage 5.2
RTX future work). Specular Fresnel: `0.04 + 0.96 * pow(1 - nDotV, 5)` (Schlick
approximation), reduced by `(1 - metallic)` for non-metals.

`VoxelSceneLighting` struct extended with 2 new `vec4` fields:
- `vctParams = (diffuseConeApertureTan, maxDistance, mipBias, enabledFlag)` (16 B)
- `vctSpecularParams = (coneApertureMax, distanceScale, mipBias, _)` (16 B)
Total struct size 624 → 656 bytes. Byte-exact contract with shader
`SceneLightingBuffer` binding 3 per `agent/knowledge.md §15` lighting
contract. New `sampler3D vctClipmap` at binding 11. Diffuse contribution
multiplied by `albedo * (1/PI) * ambientVisibility`; specular multiplied
by Fresnel * (1 - metallic). All cone math in tangent world space (no
view rotation), per WickedEngine VXGI per `turanszkij` cone table.

`kVctCutoffRoughness=0.3f` and `kVctMaxDistanceMeters=64.0f` constants in
`voxel.frag:91-94` (kVct constants block). For `chunkSize=8` VoxelLab
reference scene the diff + spec contribution shows cavity darkening
without HZB-style depth reads (cone tracing does not need depth buffer
or shadow maps — pure 3D-texture sample).
