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
