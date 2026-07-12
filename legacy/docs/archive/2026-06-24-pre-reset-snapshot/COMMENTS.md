<!--
================================================================================
PRE-RESET SNAPSHOT — ARCHIVED 2026-06-24
================================================================================

> [!WARNING]
> **АРХИВНЫЙ АРТЕФАКТ — НЕ SOURCE OF TRUTH.**
>
> Этот файл сохранён как часть `legacy/docs/archive/2026-06-24-pre-reset-snapshot/`
> в ходе операторского reset baseline. Содержимое может содержать:
> - Неточности, stale claims, неверифицированные утверждения
> - Устаревшие engineering contracts, не отражающие текущий код
> - Ссылаться на удалённые файлы, артефакты, конвенции
> - Противоречить решениям, принятым после reset baseline
>
> **Используйте ТОЛЬКО для исторического контекста. Не цитируйте как авторитетный источник.**
>
> Для восстановления оригинальной git-истории (274 коммита, 2026-02-24 → 2026-06-24):
> - Bundle: `/tmp/pre-reset-snapshot-20260624.bundle` (61 MB, 6 refs)
> - Origin object DB: `https://github.com/Leeleit/ProjectV.git` (старые SHA остаются до GC)
>
> После reset, активные engineering contracts и текущее состояние живут в:
> - `agent/knowledge.md` (активные contracts + runtime facts)
> - `agent/workspace.md` (текущий контекст + active tasks)
> - `TODO.md` (roadmap)
> - `AGENTS.md` (протокол работы агента)
    ================================================================================
    -->

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
  `agent/knowledge.md Part A` (formerly decisions.md) and `agent/knowledge.md Part B` (formerly memory.md) are preserved
  verbatim.
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

Stage 3.1 GPU Fluid CA full pipeline integration per `TODO.md §3.1` + `agent/knowledge.md §30.4` 3-step migration
precedent. `FluidCaPushConstants` (48 bytes) + `FluidCaGpuFrameStats` (16 bytes) cross-shader byte-exact contracts.
Public API surface minimal: env-gate (`IsFluidCaGpuPipelineRequested`), pipeline lifecycle (`CreateFluidCaPipelines` /
`DestroyFluidCaPipelines`), per-frame record (`RecordFluidCaDispatch`), cross-queue submit (
`SubmitFluidCaToComputeQueue` via `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` + `renderTimelineSemaphore`), stats
readback (`ReadFluidCaFrameStats` via `vmaInvalidateAllocation` mapped buffer). Atomic strategy: `fluid_ca.comp` uses
`atomicOr` + bit-check (functionally equivalent to CAS for "set bit if unset" claim per
`2026-06-21-gpu-fluid-ca-atomic-strategy` in-progress experiment).

## `src/render/vulkan/VulkanFluidCaPipeline.cpp`

### L1-L88 (design-rationale)

Constants block: 5 descriptor bindings (PackedChunkDescriptors / ActiveChunkIds / SourceFluidCells /
DestinationFluidCells / FluidStats) match `fluid_ca.comp` layout 1:1; 5*MAX_FRAMES_IN_FLIGHT pool size for storage
buffer descriptors; `kFluidCaStatsResetValue=0u` for `vkCmdFillBuffer` reset. Shader module loader mirrors
`VulkanMeshShaderPipeline::CreateMeshShaderModule` pattern (extracted helper to avoid duplication with the mesh shader
code path). `CreateFluidCaPipelines` does graceful fallback (returns false on missing spv or device creation failure;
caller in `VulkanInit.cpp` logs informational and continues with CPU path per `agent/knowledge.md §30.4` Step 1).

### L120-L250 (design-rationale)

`CreateFluidCaPipelines` builds compute pipeline from `fluid_ca.comp.spv` via `vkCreateComputePipelines`. Pipeline
layout uses single 5-binding descriptor set layout + 48-byte push constant range. Shader module named "FluidCaShader",
pipeline layout "FluidCaPipelineLayout", pipeline "FluidCaPipeline", descriptor set layout "FluidCaDescriptorSetLayout"
via `SetVulkanObjectName` for Validation Layer debug. Sets `render->fluidCaPipelineEnabled = true` on success.
`DestroyFluidCaPipelines` tears down in reverse order (pipeline, pipeline layout, descriptor pool, descriptor set
layout, shader module, all 4 per-frame buffers per SceneFrameResources). Safe to call on `pipelineEnabled=false` (
no-op).

### L260-L380 (design-rationale)

`RefreshFluidCaResourceBindings` creates descriptor pool + allocates 1 descriptor set per `SceneFrameResources` + writes
5 `VkWriteDescriptorSet` entries (chunkDescriptorBuffer, fluidCaActiveChunkIdBuffer, fluidCaSourceBuffer,
fluidCaDestinationBuffer, fluidCaStatsBuffer). Skips sets where any binding is null (graceful for frames with partial
init). `RecordFluidCaDispatch` resets stats buffer via `std::memset` (mapped memory) + 3 pre-dispatch
`VkBufferMemoryBarrier2` (stats fill + source + activeChunkId HOST→COMPUTE) + binds pipeline + descriptor set + push
constants + `vkCmdDispatch(activeChunkCount, 1, 1)` + 2 post-dispatch barriers (stats + dest COMPUTE→HOST).

### L420-L530 (design-rationale)

`SubmitFluidCaToComputeQueue` uses `vkQueueSubmit2` with `VkCommandBufferSubmitInfo` + `VkSemaphoreSubmitInfo` wait on
`renderTimelineSemaphore` (value = previous) + `VkSemaphoreSubmitInfo` signal on same semaphore (value = incremented).
Bumps `context->renderTimelineValue += 1u` to advance the timeline. RAW hazard: compute→graphics (writeOutput →
readInput) satisfied by semaphore signal+wait. Cross-queue submission ready but not yet wired in `Renderer.cpp` (current
path uses main graphics command buffer for dispatch, then SubmitFluidCaToComputeQueue can route to dedicated compute
queue once dedicated compute command pool is added — see `agent/workspace.md §2`).

### L540-L620 (design-rationale)

`ReadFluidCaFrameStats` invalidates the mapped stats buffer via `vmaInvalidateAllocation` + copies 16 bytes (4 × uint32:
activeFluidCells / droppedFluidCells / iteration / reserved) for CPU-side debug HUD. Per `agent/knowledge.md §30.4`
contract the stats are debug-only — count conservation invariant enforced by `fluid_ca.comp:101-105` `atomicOr` +
bit-check, not by stats counter.

## `src/voxel/VoxelLodDownsample.{hpp,cpp}`

### L1-L132 (design-rationale)

Stage 4.2 LOD chunk 2 B_SurfacePreserve downsampling kernel + per-chunk `LodDownsampleJob` orchestrator per
`2026-06-21-lod-mesh-downsampling` verdict=mixed. Lives in `projectv::voxel` namespace (separate from
`voxel/VoxelWorld.cpp` to minimize transitive include cost for the test target). `LodDownsampleStepForLod` maps LOD
0/1/2/3 → step 1/2/4/8 (per `SelectLodLevelForDistance` distance thresholds <32m/<64m/<128m/≥128m).
`LodDownsampledExtentForLod` returns `chunkSize/step` (clamped to ≥1 for safety). `SurfacePreserveVote8` reads step³
source voxels in fixed `sz,sy,sx` order, returns first non-Air material found OR Air if all step³ are Air — 0 T-junction
holes across 75 boundary configurations per experiment. `DownsampleChunkForLodSurfacePreserve` allocates
`outDownsampled` of size `outExtent³`, populates from `chunk.min` origin. `RunLodDownsampleJobs` iterates all chunks,
calls downsample, sets `lodDownsampledNonAirCount` byte. `IsLodDownsampleEnabled` env gate (
`PROJECTV_LOD_DOWNSAMPLE=ON`, default OFF).

## `src/physics/GreedyPhysicsMerger.{hpp,cpp}`

### L1-L200 (design-rationale)

Stage 3.3 Greedy Physics Meshing integration per `2026-06-21-greedy-physics-meshing-cpu` verdict=yes (D_3D greedy merge
algorithm, 35× shape reduction, 100% volume preservation). `MergedVoxelBox` struct holds min/max-exclusive extents in
voxel coordinates. `GreedyMergeSolidVoxelsInBounds` algorithm: for each (x,y,z) in fixed `z,y,x` ascending order, find
max X extent (X+), then max Y extent over X-range (Y+), then max Z extent over XY-range (Z+), mark consumed via byte
mask, emit one `MergedVoxelBox` per maximal extents. `IsSolidAt` inline helper checks `IsPhysicsSolidMaterial` (Glass +
FloorWhite + FloorGray; Air + Fluid return false). `IsGreedyPhysicsMeshEnabled` env gate (
`PROJECTV_GREEDY_PHYSICS_MESH=ON` default; `=OFF` falls back to naive per-voxel loop in PhysicsWorld.cpp). Both
`BuildStaticVoxelCollisionBody` and `BuildChunkStaticCollisionBody` (per-chunk incremental Jolt) integrate greedy merge.
Per-chunk rebuild path uses greedy merge for new compound shape. Tests cover empty world, single voxel unit box, full
chunk single box, volume preservation (sum of merged box volumes equals solid voxel count), mixed half-chunk reduction,
fluid+air ignored, oversized bounds clamp to world extents.

## `src/physics/PhysicsWorld.cpp` (17x incremental SyncPhysicsWorld)

### L3205-L3241 (design-rationale)

`SyncPhysicsWorld` split into two paths after Stage 3.2 follow-up. World-pointer change (first load or scene switch)
does full chunk rebuild via per-chunk `BuildChunkStaticCollisionBody` for every chunk — populates `chunkStaticBodies` +
new `chunkMergedBoxes` map. Edit-only path is incremental: `ProcessChunkRebuildQueue` applies dirty chunk rebuilds
first, then `RebuildStaticWorldBodyFromChunkShapes` re-emits the monolithic `staticWorldBodyId` as a single
`JPH::StaticCompoundShapeSettings` covering all per-chunk merged boxes. Net effect: per-edit physics sync cost drops
from O(N_world_cells) full scan (≈250 ms on FlatBench 166 400 cells) to O(N_dirty_chunks + N_total_chunks) compound
rebuild (≈30 ms on 400 chunks). Walk-character contact check (`IsWalkJumpLockedSourceSupportSideWallContact`) uses new
`IsPhysicsStaticWorldBodyId` helper that recognizes both monolithic and per-chunk body IDs, so the contact normal vs
`staticWorldBodyId` API stays compatible.

### L3040-L3110 (design-rationale)

New helpers `DestroyAllChunkStaticBodies(physics)` and `RebuildStaticWorldBodyFromChunkShapes(physics, world)`. The
first clears `chunkStaticBodies` + `chunkMergedBoxes` and removes Jolt bodies. The second iterates `chunkMergedBoxes` (
one entry per dirty or pre-built chunk) and emits a single `JPH::StaticCompoundShapeSettings` for the whole world, then
`compoundSettings.Create()` + `CreateAndAddBody()` + replaces `staticWorldBodyId`. `PhysicsState` now also stores
`std::unordered_map<uint32_t, std::vector<projectv::physics::MergedVoxelBox>> chunkMergedBoxes` — populated by
`BuildChunkStaticCollisionBody` after the GreedyMerge pass so `RebuildStaticWorldBodyFromChunkShapes` doesn't have to
re-merge. Erased in `DestroyChunkStaticBody` and in the empty-chunk early-out of `BuildChunkStaticCollisionBody`.

### L427-L437 (design-rationale)

`IsPhysicsStaticWorldBodyId(physics, bodyId)` — true if `bodyId` matches the monolithic `staticWorldBodyId` OR any body
in `chunkStaticBodies.values()`. Replaces the original `bodyId == physics.staticWorldBodyId` check at line 432 (walk
jump support side-wall contact) so the contact check keeps working when the static world is represented as multiple
per-chunk bodies. Linear scan over `chunkStaticBodies` (~400 entries on FlatBench, sub-microsecond) is acceptable for
walk character tick rate (60 Hz × 1 lookup per contact = 24 K lookups/sec).

## `src/voxel/VoxelWorld.cpp` (17x Fluid editVersion suppress)

### L1063-L1148 (design-rationale)

`SetVoxelMaterial` now has a `isFluidAirTransition` early-out: when both `previousMaterial` and `material` are within
the {Air, Fluid} set (i.e. one is Air and the other is Fluid), skip `++world.editVersion` and skip the
`physics != nullptr` chunk-rebuild queue block. Rationale: Fluid is not a `IsPhysicsSolidMaterial` (PhysicsWorld.cpp:
548), so changing Fluid↔Air cannot change static-collision geometry; bumping `editVersion` would trigger a full
`SyncPhysicsWorld` rebuild on every fluid tick (20 Hz × 1-8 movements per tick = up to 160 redundant rebuilds per second
on FlatBench before the fix). The storage write + `AccumulateMaterialCount` + `MarkChunksTouchedByVoxelEditDirty` (mesh
rebuild) still happen — water moving still needs new meshes, just not new physics bodies.
`chunk.isStatic = false; chunk.ticksSinceLastEdit = 0;` is preserved for first placement semantics (Air→Fluid on a
previously-empty chunk).

### L1085-L1091 (design-rationale)

AABB-bounded Fluid CA support. `VoxelWorld` tracks `fluidCAAabbMin` (initialized to `INT32_MAX` corner — invalid) and
`fluidCAAabbMaxExclusive` (initialized to `INT32_MIN` corner). On every `SetVoxelMaterial` that touches Fluid (either
old or new material is Fluid), the AABB is expanded to include the cell. The AABB is lazy-shrinking: it only grows; it
does not shrink when fluid is removed. When `world.stats.fluidVoxelCount == 0` the `UpdateFluidCA` early-out kicks in
and the AABB doesn't matter. AABB reset to invalid happens implicitly on world reload (new `VoxelWorld` struct is
zero-initialized via default member initializers). `UpdateFluidCA` uses this AABB to scope all 3 nested loops (read
pass, sim pass, commit pass) — read pass uses `[min-1, max+1]` for 1-cell margin so spread/fall neighbor reads land on
real data; sim and commit use `[min, max]`. For 1 water block on FlatBench the AABB is 3×3×3 = 27 cells, vs the old
full-world iteration of 80×26×80 = 166 400 cells. This is the actual root-cause fix for the 4 FPS-on-FlatBench
regression (the `SyncPhysicsWorld` per-edit rebuild was only the secondary symptom).

### L1663-L1686 (design-rationale) — 18x debug-assertion fix

`UpdateFluidCA` debug-only invariant assertion was scoped to `[fluidCAAabbMin, fluidCAAabbMaxExclusive]`. That scope is
the **monotonic-grow AABB** of where fluid has *ever* been touched (see L1085 above), not where fluid currently is.
After enough CA ticks, fluid can move outside the original AABB (e.g. a column placed at Y=5..9 settles at Y=0..4 on the
floor) and the assertion would count fluid only inside the stale AABB while `stats.fluidVoxelCount` is the world-wide
count — they diverge, `PV_ASSERT` fires. Fix: count over `[world.min, world.maxExclusive]` (the full world bounds). Cost
is `O(world.volume)` per CA tick in debug only, which is negligible. The AABB stays as a fast-path scan range for the *
*sim and commit loops** (the production hot path); only the debug invariant expands to world bounds. New regression test
`TestFluidCAStatsCountStaysConsistentWhenFluidMovesOutsideAabb` +
`TestFluidCAStatsCountStaysConsistentOnInputReplaySnapshot` (loads the user's actual
`/tmp/ProjectV/InputReplay/latest.projectv.replay.snapshot.bin` if present and runs 300 ticks) lock the contract.

### L653-L686 (design-rationale) — 18x+ AABB recompute on derived state rebuild

`RebuildVoxelWorldDerivedState` iterates all `[min, maxExclusive)` cells once
and rebuilds `world.stats` from scratch. Added: when a cell is `Fluid`, also
expand `world.fluidCAAabbMin` / `world.fluidCAAabbMaxExclusive` to include it
(resetting to invalid sentinels first). Without this, `LoadVoxelWorldSnapshot`
left `fluidCAAabbMin = (INT32_MAX, INT32_MAX, INT32_MAX)` and
`fluidCAAabbMaxExclusive = (INT32_MIN, INT32_MIN, INT32_MIN)` — the default
sentinel values from the `VoxelWorld` struct. After load, `UpdateFluidCA`
early-out check `if (world.stats.fluidVoxelCount == 0u) return 0u;` did not
fire (count = 436), but the sim range `[fluidCAAabbMin, fluidCAAabbMaxExclusive]`
became `[INT32_MAX-1, INT32_MIN+1]` = empty range, so the CA loop ran over
zero cells and produced `movedCount = 0` every tick. **Water in a loaded
snapshot would never fall even after the player broke the glass**, because
the CA saw no work to do. The fix: piggy-back on the existing O(world_volume)
iteration that already runs once per snapshot load — no extra cost — to keep
the AABB consistent with the actual fluid voxel positions. The same helper
also runs on every `CreateVoxelSceneWorld` path that calls
`RebuildVoxelWorldDerivedState`, but those paths already had the AABB
initialized via `SetVoxelMaterial` during construction, so the recompute is
a no-op redundant safety net there.

## `src/voxel/ChunkStreamer.{hpp,cpp}`

### L1-L80 (design-rationale)

Stage 4.3 Chunk Streaming foundation Step 1 per `2026-06-21-voxel-chunk-streaming-pipeline` (in-progress experiment,
closed mixed verdict expected). Interface contract: `ChunkStreamRequest` (chunkIndex + priority), `ChunkData` (
voxelBytes + nodeWords vectors), `EnqueueChunkStreamRequest` (mutex-guarded enqueue), `DrainChunkStreamQueueSize` (peek
queue depth), `TryDequeueChunkData` (returns `std::expected<ChunkData, ChunkStreamError>` for thread-safe dequeue).
`ChunkStreamError` enum covers `QueueFull` + `InvalidChunk` + `NotInitialized`. `IsChunkStreamingEnabled` env gate (
`PROJECTV_CHUNK_STREAMING=ON` default; `=OFF` returns `NotInitialized` from TryDequeue). Pending and ready deques are
mutex-protected via static-local `std::mutex` instances. Cold-path per `agent/knowledge.md §29.0` (`std::expected<T, E>`
for I/O). Background thread + SSD read integration deferred to dedicated session — interface is in place, ready for
`ChunkStreamer::ProcessPendingRequests()` background worker.

## `src/render/vulkan/VulkanWorldGenPipeline.{hpp,cpp}`

### L1-L300 (design-rationale)

Stage 4.1 GPU World Gen dispatch infrastructure per `2026-06-21-gpu-procedural-noise-compute-kernels` verdict=mixed (CC0
OpenSimplex2 3D-S recommended). `WorldGenPushConstants` (64 bytes, static_assert'd) packs chunkOriginAndChunkSize (
ivec4) + chunkCountAndFlags (uvec4) + noiseParams (vec4) + seed (uint) + reserved (3× uint). Compute pipeline from
`world_gen.comp.spv` via `ReadShaderFile` + `vkCreateComputePipelines`. 1-binding descriptor set: storage buffer at
binding 0 (writeonly voxel buffer). `BuildActiveChunkIdsForWorldGen(world, outChunkIds)` helper filters out non-empty
chunks (only generates voxels for chunks with `nonAirVoxelCount == 0`).
`RecordWorldGenDispatch(commandBuffer, render, frameResources, pushConstants, activeChunkCount)` does HOST→COMPUTE
buffer barrier + bind pipeline + bind descriptor set + push constants + `vkCmdDispatch(activeChunkCount, 1, 1)`.
`RefreshWorldGenResourceBindings` allocates 1 descriptor set per frame + 1 storage buffer write. Per-frame SSBO
capacity = `sizeof(uint32_t) * 8³ * max(chunks.size(), 1)`. `IsWorldGenGpuPipelineRequested` env gate (
`PROJECTV_WORLD_GEN_GPU=ON` default; `=OFF` short-circuits before shader load). `IsWorldGenGpuPipelineRequested` is
`inline` in header for testability without linking the .cpp.

## `src/render/TaaRenderTargets.{hpp,cpp}` (12x updates)

### L44-L65 (design-rationale)

12x Phase 3 added motion vector + history render targets per `2026-06-21-taa-motion-vectors` verdict=yes Pipeline A.
`kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT` (Karis 2014 "16:16 RG velocity buffer"). VRAM cost 8 MiB/frame
double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3`. `CreateOrRecreateTaaRenderTargets`
signature extended with 2 new `OffscreenColorTarget&` params (`motionVectorColor` + `motionVectorHistoryColor`).
`TransitionTaaMotionVectorForSample` transitions MOTION_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL with
COLOR_ATTACHMENT_WRITE → SHADER_SAMPLED_READ access. `RecordTaaMotionVectorHistoryCopy` does scene→history transfer with
full barrier chain (matches `RecordTaaHistoryCopy` pattern for scene color). `DestroyTaaRenderTargets` extended with 2
new destroy targets. Note: this data path is complete; `taa_resolve.frag` integration (consume MV texture instead of
computing from prevViewProjectionMatrix in-shader) is deferred to dedicated session — `agent/workspace.md §2` Nearest
Gap.

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

(as recorded in earlier sessions — see entries below for the 18x async wait-semaphore fix)

## `src/render/vulkan/VulkanBootstrap.cpp` (18x+ device creation chain fix)

### L819-L867 (design-rationale)

`VkDeviceCreateInfo::pNext` chain must ALWAYS include all feature structs
regardless of `selected.supports*`. The previous code used ternary
`supportsSwapchainMaintenance1 ? &struct : nullptr` which broke the chain
mid-way when an intermediate optional feature was unsupported on the device.
On hardware without `swapchainMaintenance1` (the user's RTX 3060 Ti etc.),
`enabledFeatures12.pNext = nullptr` and **no subsequent feature struct
(`meshShader`, `accelerationStructure`, `rayQuery`) ever reached
`vkCreateDevice`**. The probe at startup correctly reported `rayQuery=1`
(because it queries via `vkGetPhysicalDeviceFeatures2`), but the device was
actually created **without** `rayQuery` feature enabled. Validation layer
flagged `VUID-VkShaderModuleCreateInfo-pCode-08740` on every
`vkCreateShaderModule` for shader code that declared `RayQueryKHR`
capability in SPIR-V (the `voxel.frag.rtx` / `voxel.frag.rtx_taa_on` SPIR-V
variants). Per Vulkan spec, an unsupported-or-disabled `sType` in a chain
node would itself error out, so the fix is to always set `sType` and always
link. Each struct stays in the chain with `VK_FALSE` feature fields when not
supported — that's valid per spec. Refactored to always-set-sType aggregate
initialization + always-link chain; only the feature field assignments stay
conditional.

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

### L114-L134 (design-rationale) — 18x async wait-semaphore fix

`RecordAsyncComputePass` must wait on `context->renderTimelineSemaphore` at
`context->renderTimelineValue` before re-recording the persistent
`asyncComputeCommandBuffer`. The buffer is submitted by `SubmitToComputeQueue`
via `vkQueueSubmit2` with `renderTimelineSemaphore` as the signal semaphore
(L277, L283); it therefore completes when the GPU reaches `renderTimelineValue`.
Per Vulkan spec `VUID-vkBeginCommandBuffer-commandBuffer-00049` the cmd buffer
must not be in Pending state when `vkBeginCommandBuffer` is called. The previous
fix in `cee5db6` waited on `hzbBuildTimelineSemaphore` at
`hzbBuildLastTimelineValue` — but that semaphore is signalled by
`SubmitHzbAsyncCullToComputeQueue` (L441), **not** by `SubmitToComputeQueue`.
When `RecordHzbAsyncCullPass` was not active (or hadn't yet signalled),
`hzbBuildLastTimelineValue` stayed 0, the wait was skipped, the cmd buffer
stayed Pending from the previous frame's `SubmitToComputeQueue`, and
`vkBeginCommandBuffer` (L141) tripped the validation layer with
`vkBeginCommandBuffer-commandBuffer-00049` + the symmetric
`vkQueueSubmit2-commandBuffer-03875` on the next submit. The fix is
mechanical: wait on the same semaphore that signalled the previous submission.
Note: `RecordHzbAsyncCullPass` (L308) does correctly wait on
`hzbBuildTimelineSemaphore` because `SubmitHzbAsyncCullToComputeQueue` does
signal that one (L441). The two record functions use the SAME persistent
`asyncComputeCommandBuffer` but DIFFERENT signal semaphores — the wait must
match the submit per-function.

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

## `src/render/SkyAtmosphere.hpp` (8x V B)

### L1-L46 (design-rationale)

Stage 5.x Sky Hillaire 2020 EGSR Sky atmosphere pass. Per closed
`2026-06-21-precomputed-atmospheric-sky` experiment (C_Hillaire2020
universal default). Env-gate `IsSkyAtmosphereEnabled()` (`PROJECTV_SKY=ON`,
default OFF per `agent/knowledge.md §30.4` Step 1 additive optional path
precedent). `SkyAtmospherePushConstants` (64 bytes, 16-byte align for
push-constant range) packs: zenithColorAndIntensity (vec4) +
horizonColorAndSunIntensity (vec4) + sunDirectionAndAngularSize (vec4) +
viewParams (vec4). `kSkyAtmosphereResolution = 256u` reserved for future
LUT precomputation. Public API: `CreateSkyAtmospherePipelines`,
`DestroySkyAtmospherePipelines`, `RecordSkyAtmospherePass`. No external
resource bindings in MVP (analytical in-fragment-shader color); future
Sky-View LUT (binding 0) + Multi-Scattering LUT (binding 1) follow per
Hillaire 2020 production reference.

## `src/render/SkyAtmosphere.cpp` (8x V B)

### L1-L180 (design-rationale)

Pipeline creation: `vkCreateGraphicsPipelines` with dynamic-rendering
`VkPipelineRenderingCreateInfo` (color format R16G16B16A16_SFLOAT + depth
D32_SFLOAT). No vertex buffers — full-screen triangle via `gl_VertexIndex`
derivation. `depthTestEnable=VK_TRUE` + `depthCompareOp=VK_COMPARE_OP_ALWAYS`

+ `depthWriteEnable=VK_TRUE` so sky writes depth=0.9999 (and main pass
  `loadOp=LOAD` will see it). `colorWriteMask` excludes alpha (sky is opaque
  over scene). `DestroySkyAtmospherePipelines` mirrors creation in reverse
  order, safe to call when `skyAtmospherePipelineEnabled=false` (no-op).

### L182-L260 (design-rationale)

`RecordSkyAtmospherePass` builds a one-off `VkRenderingInfo` with single
color attachment + depth attachment, `loadOp=DONT_CARE` (sky overwrites
both), calls `vkCmdDraw(commandBuffer, 3, 1, 0, 0)` (3 vertices = 1
triangle). `profiling::PlotValue("Sky Atmosphere Pass", 1.0)` for Tracy
trace. Caller (`Renderer.cpp::DrawFrame`) is responsible for `loadOp=LOAD`
on the main rendering pass when sky pass is active so the sky color
isn't clobbered by `VK_ATTACHMENT_LOAD_OP_CLEAR` of the main pass.

## `src/shaders/sky_atmosphere.frag` (8x V B Phases 2-3)

### L1-L9 (design-rationale)

Per Hillaire 2020 EGSR (publication `cf14050` + `sebh/UnrealEngineSkyAtmosphere`
reference impl). MVP analytical color: zenith → horizon gradient + sun
disc approximation. Phase 3 upgrade: full single-scattering Rayleigh
(per-channel wavelength β_R) + Mie (β_M) + Henyey-Greenstein phase function

+ 16-step exponential depth distribution. No LUT precomputation in
  fragment shader (Phase 3 deferred to follow-up session — add Sky-View
  LUT 256×128 RGBA16F + Multi-Scattering LUT 32×32 RGBA16F per Hillaire
  2020 production reference for ~10× cost reduction at 4K).

### L20-L40 (EVIL)

`kRayleighBeta = vec3(5.8e-6, 13.5e-6, 33.1e-6)`, `kMieBeta = 0.005`,
`kMieG = 0.8` hard-coded per Hillaire 2020 reference (lambda^-4 scaling
R=680nm, G=550nm, B=440nm wavelengths; Mie g=0.8 broad forward scattering
per Wronski 2014 production tuning). Production tunable via push constants
or per-scene VoxelScenePreset. Cross-vendor same formula; RTX 3060 Ti +
AMD RDNA + Intel Arc all match the analytical reference.

## `src/render/VolumetricFog.{hpp,cpp}` (8x V B Phases 4-5)

### L1-L35 (design-rationale)

Wronski 2014 SIGGRAPH froxel pattern + Frostbite 2015 unified volumetric
reference. Env-gate `IsVolumetricFogEnabled()` (`PROJECTV_FOG=ON`,
default OFF). Wronski 2014 720p reference: 160×90×64 froxel grid. Public
API: `CreateVolumetricFogResources`, `DestroyVolumetricFogResources`,
`RecordVolumetricFogAccumulationPass`. `kVolumetricFogFroxelWidth/Height/Depth`
constants (160/90/64) match Wronski 2014 reference for 720p; for 1080p+
scale to 240×135×128 per Frostbite 2015 production tuning. 12-slab ray-march
matches Frostbite 2015 slab count (8-16 slabs production range).

### L40-L100 (design-rationale)

Compute pipeline: 3-binding descriptor set (froxel storage image 0,
scene color sampled image 1, depth sampled image 2). Linear sampler with
CLAMP_TO_EDGE address mode on all 3 axes. Descriptor pool: 1 set per
`MAX_FRAMES_IN_FLIGHT`. 8×8×4 workgroup matches Wronski 2014 reference;
dispatch = (W/8, H/8, D/4) = (20, 11, 16) workgroups. Bound check via
`imageSize(fogFroxel)` ignores last 2 height rows (90 % 8 = 2 unused).

## `src/shaders/volumetric_fog.comp` (8x V B Phases 4-5)

### L1-L20 (design-rationale)

Wronski 2014 per-slab ray-march accumulator. Phase 4 MVP: density
multiplied by `exp(-worldDistance * 0.012)` exponential falloff. Phase 5
upgrade: 12-slab ray-march with Schlick phase function (g=0.8 broad
forward scattering) and Beer-Lambert transmittance accumulation
(`transmittance *= stepTransmittance` per slab). Early-out at
`transmittance < 0.02` to skip the dark tail of the ray (per
Frostbite 2015 production pattern). Output: accumulated RGB (linear,
no tonemap) + alpha (1 - transmittance) for consume in voxel.frag.

### L25-L30 (EVIL)

`kDepthDistributionGamma=0.5`, `kDepthDistributionBias=0.005` per Wronski
2014 reference (concentrates froxel resolution near camera where aliasing
artifacts are most visible). `kSchlickG=0.8` broad forward scattering per
Wronski 2014 production tuning. Tunable via push constants if needed.

## `src/render/Cloudscape.{hpp,cpp}` (8x V B Phases 6-7)

### L1-L35 (design-rationale)

Schneider "Nubis" 2017 single-layer ray-march (B_SingleLayerRayMarch
universal default per closed `2026-06-21-cloudscape-rendering`).
Env-gate `IsCloudscapeEnabled()` (`PROJECTV_CLOUDS=ON`, default OFF).
128×128 R8 noise texture (CPU-generated FBM at startup) + 24-step
ray-march per pixel. Public API: `CreateCloudscapeResources`,
`DestroyCloudscapeResources`, `RecordCloudscapeRaymarchPass`.
`kCloudscapeNoiseTextureSize = 128u` per Schneider 2017 reference.
`kCloudscapeRaymarchStepCount = 24u` (Schneider 2017 used 64-128 for
production; 24 is sufficient for MVP visual fidelity at 1080p).

### L40-L100 (design-rationale)

CPU-side noise generation: `ValueNoise2D` + 4-octave FBM low-freq + 3-octave
high-freq * 0.35 multiplier + 0.18 bias, clamped [0,1] → 0..255 → uint8_t
texture. 3-binding descriptor set (cloud noise + scene color + depth).
Linear sampler with REPEAT address mode (cloud noise is tileable).
Pipeline: alpha-blend `VK_BLEND_FACTOR_SRC_ALPHA` over scene color,
`depthTestEnable=VK_TRUE` + `depthCompareOp=LESS_OR_EQUAL` + `depthWriteEnable=VK_FALSE`
(clouds only overdraw sky, don't write depth).

## `src/shaders/cloudscape.frag` (8x V B Phases 6-7)

### L1-L20 (design-rationale)

Schneider "Nubis" 2017 single-layer ray-march. Reconstructs world ray
from NDC + push-constant camera position. T-intersection with horizontal
slab at y=kCloudBaseHeight, ray-march 24 steps through slab, sample
2D FBM noise per step, accumulate Beer-Lambert transmittance, add sun
contribution (Schlick g=0.5 phase). Output: linear RGB (no tonemap) +
alpha (1 - transmittance) for alpha-blend over scene color.

### L25-L35 (EVIL)

`kCloudBaseHeight=80.0`, `kCloudThickness=24.0` hard-coded for VoxelLab
reference scene. `kSchlickG=0.5` per Schneider 2017 cloud-tuning (broader
than fog because clouds have larger droplets). Production tunable via
push constants or per-scene VoxelScenePreset.

---

## `src/core/Types.hpp` (AUDIT-CORE-003 ownership mapping, 2026-06-21)

### RenderState field → Create/Destroy pair (design-rationale)

`RenderState` aggregates ~50 raw Vulkan handles / VMA allocations across
~15 sub-resources. Each is owned by a specific Create/Destroy pair called
from `src/app/main.cpp:InitializeVulkanBase` (init) and `src/core/Types.cpp:42-123`
(DestroyRenderState shutdown). **All pointer/handle fields are non-owning
by default — the owning function is the only place allowed to vmaDestroy*
or vkDestroy*.** Do NOT free these fields directly outside of their
owner.

| Field group                                                                                                  | Owner (Create)                                                                             | Owner (Destroy)                                                   |
|--------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------|-------------------------------------------------------------------|
| `graphicsDescriptorSetLayout/Pool`, `shadowDescriptorSetLayout/Pool`, `voxelMeshingDescriptorSetLayout/Pool` | `CreateGraphicsPipeline`                                                                   | `DestroyGraphicsPipeline`                                         |
| `sceneFrameResources[]` (32+ SSBO triads: `*Buffer`+`*Allocation`+`*MappedData`+`*CapacityBytes`)            | `CreateSceneResources` (`RefreshSceneFrameResources`)                                      | `DestroySceneResources` (`DrainAllDeferredNanoVdbDestroys` first) |
| `deferredNanoVdbDestroys[]` (per-frame-in-flight queue)                                                      | `GrowNanoVdbBuffer` (enqueue)                                                              | `DestroySceneResources` (drain)                                   |
| `rayTracedShadows *`                                                                                         | `CreateRayTracedShadowResources`                                                           | `DestroyRayTracedShadowResources`                                 |
| `depthImage/View/Allocation`                                                                                 | `CreateDepthResources`                                                                     | `DestroyDepthResources`                                           |
| `vctClipmapImage/View/Allocation/Memory/Sampler`                                                             | `CreateVctClipmapFallbackSamplerOnly` (fallback) / `CreateVctClipmapResources` (env-gated) | `DestroyVctClipmapResources`                                      |
| `vctVoxelize*` (pipeline/layout/shader/descriptor set)                                                       | `CreateVoxelizePipelines`                                                                  | `DestroyVoxelizePipelines`                                        |
| `shadowImage/View/CascadeViews/Allocation/Sampler`                                                           | `CreateShadowResources`                                                                    | `DestroyShadowResources`                                          |
| `hizBuffer`, `hizCulling*`                                                                                   | `CreateHizCullingPipeline` (lazily created on first dispatch)                              | `DestroyHizCullingPipeline`                                       |
| `screenshotReadbackBuffer/Allocation/MappedData`                                                             | `CreateScreenshotReadbackResources`                                                        | `DestroyScreenshotReadbackResources`                              |
| `materialVisualBuffer/Allocation/MappedData`                                                                 | `CreateMaterialVisualResources` (env-gated)                                                | `DestroyMaterialVisualResources`                                  |
| `skyAtmospherePipeline/Layout/Modules/DescriptorSetLayout/Pool/Sets[]/Lut images`                            | `CreateSkyAtmosphereResources` / `CreateSkyLutResources`                                   | `DestroySkyAtmosphereResources` / `DestroySkyLutResources`        |
| `volumetricFogFroxelImage/View/Sampler/Pipeline/Layout/Descriptor*`, `volumetricFogFallbackImage/View`       | `CreateVolumetricFogFallbackOnly` (fallback) / `CreateVolumetricFogResources` (env-gated)  | `DestroyVolumetricFogResources`                                   |
| `cloudscapePipeline/Layout/Descriptor*`                                                                      | `CreateCloudscapeResources` (env-gated)                                                    | `DestroyCloudscapeResources`                                      |

**Why raw pointers and not `std::unique_ptr` / RAII wrappers?** Most of
these types are opaque Vulkan handles (VkBuffer, VkImage, etc.) whose
destruction requires VMA allocator + logical device context, which
`std::unique_ptr<>` cannot express without a heavy custom deleter +
singleton-pattern lifetime violation. The pattern chosen (Create/Destroy
pair + bool-initialized fields + exhaustive central shutdown) is
preferred over per-field RAII for two reasons:

1. **Initialization order matters.** Vulkan requires device → allocator
   → swapchain → pipelines → descriptor sets. RAII destroys in reverse
   declaration order, but our field order in `RenderState` is data-flow
   oriented (descriptors, then buffers, then images), not lifecycle.
2. **Error recovery.** `Create*` functions do partial cleanup on failure
   (calls `Destroy*` mid-init). RAII makes this hard to express.

**Refactor hazard:** if a new pointer field is added to `RenderState`:

1. Add to the matching `CreateXxxResources` function (and `DestroyXxxResources`).
2. Add to the table above.
3. Verify DestroyRenderState calls the right destroyer.

---

## `src/c_kernels/` (AUDIT-CORE-010 alignment contract, 2026-06-21)

### Common alignment policy (design-rationale)

All C kernel entry points operate on raw voxel/parameter buffers that
**must be 16-byte aligned** for AVX2 SIMD correctness. The platform's
allocator (VMA for GPU buffers, `std::aligned_alloc` for CPU staging)
guarantees this for top-level allocations. Nested structs
(`ProjectvCFrustumCullParameters`, `ProjectvCAabb`) are declared with
`alignas(16)` at their definitions in
`src/c_kernels/frustum_cull.hpp:8-26` so even in-place construction
preserves alignment.

### Per-kernel contract

| Function                                      | Caller                                     | Alignment requirement                       | Asserted at runtime                                                                                                            |
|-----------------------------------------------|--------------------------------------------|---------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------|
| `projectv_cull_frustum_scalar` (C, CPU)       | `tests/FrustumCullBenchmark`, `RunCScalar` | `aabbs` and `masks` arrays: 16-byte aligned | No runtime assert (release benchmark, contract per declaration)                                                                |
| `projectv_cull_frustum_avx2` (C, AVX2 CPU)    | `tests/FrustumCullBenchmark`, `RunCAvx2`   | Same                                        | Yes — `static_assert(alignof(ProjectvCFrustumCullParameters) >= 16)` at call site, vector base addr checked at vector prologue |
| `FrustumCulling::TestXxx` (C++ thin wrappers) | CPU code                                   | Inherits via `alignas` on POD structs       | Compile-time `static_assert` on consumer side                                                                                  |

**EVIL markers** would normally be added per declaration, but
`alignas(16)` is the C++ standard mechanism — adding `// EVIL: aligned for AVX2`
would be redundant. The compile-time guarantee from `alignas` + `static_assert`
is the design-rationale anchor.

**Future hazard:** if a new C kernel is added without `alignas(16)` on its
parameters, AVX2 builds will crash on strict-alignment architectures
(ARM, certain mobile GPUs in CPU emulation). Mitigation: CI build on
ARM/aarch64 should be added to catch this.

---

## `src/physics/PhysicsWorld.cpp` (AUDIT-PV-001 walkSupport, AUDIT-PV-002 body rebuild, 2026-06-21)

### Walk support cache invalidation (design-rationale)

`UpdateWalkGroundSupport` (`PhysicsWorld.cpp:2389`) is called per walk tick
from `TickWalkSystem` and from `SyncPhysicsWorld` fallbacks. The function
recomputes contact state from scratch each call:

- Reads `character->GetLinearVelocity()` and `GetPosition()` (line 2413)
- Queries Jolt contact manifold via `CharacterVirtual::ExtendedUpdate` (upstream)
- Stores results in `physics.walkXxx` state fields for the next-tick
  walk-jump-lock contract (e.g., `walkPreviousSupportFeetPosition`)

There is **no cross-frame cache of world geometry** to invalidate. The
stateful fields are walk-dynamics state (positions, grace frame counts,
edge-grace timers), not chunk/AABB caches. The audit's "walkSupport
cache invalidation" concern is therefore N-A — the per-tick recompute is
the contract, not a fragility.

### Body ID stability vs SetShape() (design-rationale)

`RebuildStaticWorldBodyFromChunkShapes` (`PhysicsWorld.cpp:3054`) takes the
destroy+create path:

1. `DestroyStaticWorldBody` (line 3057) → `bodyInterface.RemoveBody` + `DestroyBody`
2. Build new compound shape from `physics.chunkMergedBoxes`
3. `bodyInterface.CreateAndAddBody` → new `staticWorldBodyId`

Jolt does expose `BodyInterface::SetShape(bodyID, shape, activationMode)`
that would preserve the body ID across rebuilds (only swapping the
shape). The audit suggests this as a Jolt best practice.

**Why we chose destroy+create:**

- Shape construction cost is the same (compound shape build + sub-shape
  validation must happen either way).
- SetShape would need to handle the case where the new shape is invalid
  (rollback to old shape?). Destroy+create is fail-fast.
- `IsPhysicsStaticWorldBodyId` helper (`PhysicsWorld.hpp:58`) already
  abstracts the "is this the static world body" check for callers that
  cached a body ID. Helper is cheap (one indirection through `physics`).
- Body ID is used only for the walk-jump-lock support contract
  (`PhysicsWorld.cpp:446`); it's not used for contact filtering or
  broad-phase optimization that would benefit from stable IDs.

The performance gain from SetShape is dominated by the shape-build cost
on either path. Body ID stability is a future optimization if profiling
shows the destroy+create cost in `BodyInterface::RemoveBody` becomes
material (currently ~0.05 ms per rebuild, masked by 35× reduction in
rebuild count after 17x session's incremental work).

---

## `src/voxel/VoxelWorld.cpp` (AUDIT-PV-003 FluidCA AABB, 2026-06-21)

### UpdateFluidCA AABB bounds (design-rationale)

`UpdateFluidCA` (`VoxelWorld.cpp:1500`) reads voxels in a tight AABB
around the last fluid activity. Lines 1529-1540 explicitly clamp the
read AABB to world bounds:

```cpp
int readMinX = world.fluidCAAabbMin.x - 1;
// ... similar for Y/Z
if (readMinX < world.min.x) readMinX = world.min.x;
// ... similar for all 6 bounds
```

This is defense-in-depth on top of `ReadVoxelFromSparseStorage` (which
returns `Air` for out-of-bounds coordinates). The AABB expansion by ±1
on each axis (lines 1529-1534) is for neighbor-lookup during fluid
spreading — clamped to world bounds to prevent OOB reads even on the
boundary. Audit concern already addressed in mainline.

---

## `src/physics/PhysicsWorld.hpp` (AUDIT-PV-004 chunkMergedBoxes, 2026-06-21)

### chunkMergedBoxes map growth (design-rationale)

`PhysicsState::chunkMergedBoxes` is a `std::unordered_map<uint32_t, std::vector<MergedVoxelBox>>`
(`PhysicsWorld.hpp` field). Current lifecycle:

- **Created** at `BuildChunkStaticCollisionBody` (`PhysicsWorld.cpp:2943`) —
  `chunkMergedBoxes[chunkIndex] = mergedBoxes`
- **Erased** at chunk rebuild (`PhysicsWorld.cpp:2928`, `2992`, `3037`)
- **Cleared** at full world rebuild via `DestroyAllChunkStaticBodies` (`PhysicsWorld.cpp:3051`)

**Current code never grows the map unboundedly** because:

1. Chunk edits always rewrite the existing entry (line 2943 overwrites)
2. Full rebuild clears the map (line 3051)
3. **There is no chunk unload path in mainline yet** — `ChunkStreamer.hpp`
   only exposes load-side APIs (`EnqueueChunkStreamRequest`,
   `PreloadChunksAroundCamera`, `BakeAllChunksToDisk`). The streaming
   architecture loads chunks but keeps them in memory; no `OnChunkUnloaded`
   hook exists yet.

**Future hazard:** when chunk streaming matures to support dynamic
unload (player moves far away → chunks evicted from memory), the new
unload path MUST call `physics.chunkMergedBoxes.erase(chunkIndex)`
before destroying the chunk's data, or the map will leak entries
referencing deallocated voxel data. Recommended API:

```cpp
// In new ChunkStreamer unload hook:
void OnChunkUnloaded(PhysicsState &physics, uint32_t chunkIndex) {
    auto it = physics.chunkStaticBodies.find(chunkIndex);
    if (it != physics.chunkStaticBodies.end()) {
        physics.physicsSystem.GetBodyInterface().RemoveBody(it->second);
        physics.physicsSystem.GetBodyInterface().DestroyBody(it->second);
        physics.chunkStaticBodies.erase(it);
    }
    physics.chunkMergedBoxes.erase(chunkIndex);
    // ... existing destroy logic
}
```

---

## `src/voxel/VoxelWorld.cpp` (AUDIT-PV-005 editVersion/NanoVdb, 2026-06-21)

### Fluid↔Air editVersion suppression (design-rationale)

`SetVoxelMaterial` (`VoxelWorld.cpp:1074`) suppresses `++world.editVersion`
for `isFluidAirTransition` (Fluid→Air or Air→Fluid). This is intentional
per 17x session's "Stage 3.2 Incremental Jolt" work: fluid is not a
physics-solid material (per `IsPhysicsSolidMaterial` in
`PhysicsWorld.cpp:548`), so Fluid↔Air transitions don't need a physics
rebuild.

**Side effect:** `BuildNanoVdbFlatten` in `SceneResources.cpp:1716-1717` is
gated by:

```cpp
const bool fluidOnlyChunkRebuilds = world->voxelWorld->editVersion == render->lastNanoVdbSyncedEditVersion;
if ((!render->completedChunkRebuildIndices.empty() && !fluidOnlyChunkRebuilds) ||
    render->sceneNanoVdbVersion == 0u) {
    BuildNanoVdbFlatten(...);
}
```

When a fluid-only edit happens, `editVersion` doesn't change but
`completedChunkRebuildIndices` IS populated (line 1112: every edit
calls `MarkChunksTouchedByVoxelEditDirty`). The gate `!fluidOnlyChunkRebuilds`
becomes `false`, so the flatten is **skipped** for the fluid-only case.

**Why this is acceptable:**

- NanoVdb flatten is consumed by VCT diffuse/specular cone tracing
  (`voxel.frag` binding 9/10) and RTX ray queries (binding 11) — both
  for SOLID geometry. Fluid is rendered via `volumetric_fog.comp`
  (binding 12), which has its own update path independent of NanoVdb.
- Fluid state in sparse storage is read directly by `GetVoxelMaterial`
  in fragment shaders for transparency classification (line 608 of
  voxel.frag), not via NanoVdb.
- A separate `meshVersion` for fluid would add a per-edit bump that
  re-triggers a full NanoVdb flatten (defeating the 17x session's
  optimization).

**If fluid lighting/sampling is added that requires NanoVdb data,
the gate would need a third condition `|| voxelWorld.fluidVoxelCount > previousFluidCount`.**
Documented as future-work refactor.

---

## `src/voxel/VoxelWorld.cpp` (AUDIT-PV-006 MarkChunksTouched, 2026-06-21)

### 3×3×3 neighborhood marking (design-rationale)

`MarkChunksTouchedByVoxelEditDirty` (`VoxelWorld.cpp:222`) iterates
`[minChunkX..maxChunkX] × [minChunkY..maxChunkY] × [minChunkZ..maxChunkZ]`,
which includes the center chunk (offset 0,0,0). The audit suggested
adding `if (offset == {0,0,0}) continue;` to skip the self-chunk.

**The center chunk MUST be marked dirty** when its voxel is edited —
that's literally what "MarkChunksTouchedByVoxelEditDirty" does. The
boundary expansion (lines 234-251) only ADDS neighbor chunks; it
never REMOVES the center. The audit's suggested optimization would be
a bug. False alarm.

---

## `src/voxel/NanoVdb.cpp` (AUDIT-PV-008 ChunkLoadingState, 2026-06-21)

### BuildNanoVdbFlatten partial-chunk assert (design-rationale)

The audit suggested adding `assert(chunk.loadingState == ChunkLoadingState::Complete)`
in `BuildNanoVdbFlatten`. There is no `ChunkLoadingState` enum in mainline
— the `VoxelChunk` struct (`VoxelWorld.hpp`) does not track loading state.
Chunks in mainline are either present (`nonAirVoxelCount` set, sparse
storage populated) or absent (not in `world.chunks`).

`BuildNanoVdbFlatten` (`NanoVdb.cpp`) iterates `world.sparseStorage`
directly; the flatten is bounded by `sparseStorage.GetWidth/Height/Depth()`
which represent fully-loaded chunk extents. No partial-chunk hazard in
current architecture. If streaming chunk loading introduces a
`ChunkLoadingState` (per `AUDIT-PV-004` future-work), the assert would
become meaningful and should be added at the iteration entry point.
False alarm for current mainline.

## `src/render/RayTracedShadows.{hpp,cpp}` (session 19x, Stage 5.2.A→B→C RTX shadows full pipeline)

### L100-L140 (design-rationale)

`RayTracedShadowConfig` расширен 9 новыми полями для per-chunk BLAS storage и TLAS backing buffer.
До этого сетапа TLAS был stub'ом — `RecordTlasBuild` инкрементировал счётчик и всё,
`UpdateTlas` хардкодил `accelerationStructureReference = 0u` (баг
VUID-vkCmdBuildAccelerationStructuresKHR-pInfos-12281).
Теперь каждый chunk имеет свой VkAccelerationStructureKHR + backing buffer + кэшированный device address
(через `EnsureBlasHandle` — lazy allocate при первом `BuildChunkBlas` для chunkIndex).
TLAS handle создаётся один раз в `AllocateBuffers` с capacity для `maxBlasCount = 4096` instances
через `vkGetAccelerationStructureBuildSizesKHR(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)` →
dedicated backing buffer → `vkCreateAccelerationStructureKHR(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)`.
Per-chunk BLAS handles живут в `blasHandles[chunkIndex]` — O(1) lookup при `UpdateTlas`.
`RayTracedShadowTestAccess` friend struct — white-box test access без публичного API
leak в production path.

### L240-L420 (design-rationale)

`AllocateBuffers` делает реальный TLAS allocation: query sizes → allocate backing
buffer → `vkCreateAccelerationStructureKHR` → cache handle. Все previous stubs удалены.
Per-chunk BLAS handles lazy-created в `EnsureBlasHandle` (вызывается из `BuildChunkBlas`)
— экономит VRAM для chunks, которые не были touched (chunks без voxel edits не получают
BLAS handle, экономя ~256-1024 bytes per chunk).

`BuildChunkBlas` (L327-L413) делает настоящий `vkCmdBuildAccelerationStructuresKHR`
для bottom-level + AS_BUILD → AS_READ barrier на BLAS backing buffer
(VUID-vkCmdBuildAccelerationStructuresKHR-pInfos-12258 alignment). dstAccelerationStructure
binding обязателен — без него driver не знает, куда писать результат. Per-BLAS storage
buffer помечен `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR` (VUID-03709 compliance).

`UpdateTlas` (L644+) пишет `instances[i].accelerationStructureReference = blasDeviceAddresses[chunkIndex]`
вместо hardcoded 0. Out-of-range chunkIndex безопасно записывает 0 (no crash).
VmaFlushAllocation вызывается на host-mapped instance buffer для CPU→device visibility.

`RecordTlasBuild` (L697+) делает реальный `vkCmdBuildAccelerationStructuresKHR` для TLAS:
HOST → AS_BUILD barrier на instance buffer (host writes), `VK_GEOMETRY_TYPE_INSTANCES_KHR`
geometry с `arrayOfPointers = VK_FALSE`, dstAccelerationStructure = pre-created TLAS,
scratch from shared scratch buffer, `primitiveCount = tlasInstanceCount`. Emits
AS_BUILD → FRAGMENT|RAY_TRACING barrier (memory + execution dependency) — fragment
shader `rayQueryEXT` reads TLAS сразу после этой команды.

### L24-L31 (design-rationale)

`IsRayTracedShadowEnabled(const VulkanContextState &context)` заменил env-gate версию.
Возвращает `context.rayTracing.accelerationStructure && context.rayTracing.rayQuery`
(auto-detect). `PROJECTV_HW_RAY_TRACING=ON/OFF` env var полностью удалён из всех
callers (`VulkanBootstrap.cpp` 3 sites, `VulkanGraphicsPipeline.cpp` 3 callers updated
to pass `*context`). Hardware target policy — см. `agent/knowledge.md §15`
"Hardware target policy for RTX-driven path (5.2.C)" — minimum NVIDIA RTX 2060 (Turing).

### L519-L537 (design-rationale)

`CreateRayTracedShadowResources` теперь hard-fail на non-RTX GPU: SDL_LogCritical

+ return false. Caller (`VulkanInit::InitVulkan`) propagates as
  `std::unexpected(VulkanInitError::ShadowResourcesFailed)`. Engine refuses to start
  on non-RTX hardware — no partial functionality, no CSM fallback. Это pet-project,
  никаких уступок legacy hardware.

## `src/shaders/voxel.frag` (session 19x, Stage 5.2.B RTX shadow consume)

### L88-L99 (design-rationale)

`TraceRtxSunShadowRay(worldOrigin, sunDir) → float` — single-hit ray query против
`rtxTlas` (binding 13, уже используется для specular GI). `gl_RayFlagsTerminateOnFirstHitEXT`
(early-out на первом hit), `tMin = 0.001` (offset чтобы не hit own surface),
`tMax = 256 m` (EVIL constant `kRtxSunShadowMaxDistanceMeters`, generous vs VoxelLab
64 m receiver max per `agent/knowledge.md §15`). Returns 1.0 if no hit (lit),
0.0 if hit (in shadow). Cost: ~0.5-1ms per frame на RTX 3060 Ti per TODO.md §5.2.B estimate.

### L777-L794 (design-rationale)

`ComputeSunShadowSample` early return для RTX path: `if (nDotLRtx > 0.02) return
vec4(rtxLit*contact, anyRtxShadow, 0.0, contact)`. CSM cascade blend + `receiverDepthBias` /
`receiverNormalBias` / `receiverLightBias` ladder полностью skipped для RTX path.
Никаких `receiverDepthBias` (анти-Peter-Panning hack), никаких cascade transitions,
никаких `0.5x normalBias + 4x depthBias` hacks. RTX даёт ground-truth visibility.
Peter-panning fix chain (P0.4) в `agent/knowledge.md §15` lines 1318-1320 теперь MOOT
— RTX делает их irrelevant. Закрытие CSM в 5.2.D = полное удаление legacy shadow code.

## `src/shaders/voxel.frag` (session 20x, Stage 5.4 RTX ambient occlusion)

### L128-L142 (design-rationale)

`TraceRtxAmbientOcclusionRay(worldOrigin, direction, radius)` GLSL helper. Заменяет
DDA voxel traversal в `TraceAmbientOcclusionRay` (5.4 milestone) когда
`VOXEL_RTX_ENABLED` определён. Использует `gl_RayFlagsTerminateOnFirstHitEXT |
gl_RayFlagsOpaqueEXT` для binary visibility test, T_min=`kRtxAoMinRayLengthMeters`
(0.001m) для anti-self-hit offset, T_max=`radius` для AO radius cap. Binary
visibility (1.0=visible, 0.0=occluded) — отличается от DDA который возвращал
weighted `(1 - traveled/maxDistance)²`. Caller (cone-weighted sum) handles
strength modulation через `ambientOcclusionParams.x`. T_min offset критичен
иначе ray hits own starting triangle (Khronos VK_KHR_ray_query tutorial предупреждает).
Флаг `Opaque` даёт fast path на opaque BLAS (ray skips anyhit shader; наш chunks
BLAS — opaque AABBs per `RayTracedShadows.cpp:496` `VK_GEOMETRY_OPAQUE_BIT_KHR`).

## `src/render/RtxGiProbes.{hpp,cpp}` (session 20x, Stage 5.5 DDGI infrastructure)

### L17-L25 (design-rationale, struct layout)

`RtxGiProbeConfig` — DDGI probe field state. Хранит handles для 3D probe
volume textures (irradiance B10G11R11_UFLOAT_PACK32 8³×16² octahedral, distance
R16G16_SFLOAT 8³×16², probe data R16G16B16A16_SFLOAT 1×1 fallback) + volume
descriptor SSBO (64 bytes, std430 layout matching `VolumeDescGpu` in
`RtxGiProbes.cpp:25-44`). Probe counts stored as `uint32_t` (per-axis) +
`raysPerProbe` для compute pass. `enabled` flag gated on
`IsRtxGiProbeFieldEnabled` (accelerationStructure + rayQuery both required).
Descriptor writes в `VulkanGraphicsPipeline.cpp` (bindings 14-17) skipped when
disabled (avoids VUID-VkWriteDescriptorSet-descriptorType-02997 null imageView).

### L116-L156 (design-rationale, default values)

`Initialize` defaults: 8 probes per axis (512 total), 16×16 octahedral,
64 rays/probe, 32m half-extent, 16m max ray distance. Per
`docs/experiments/experiments/2026-06-22-ddgi-probe-field-voxel-gi/RESULTS.md`
8³=512 probes match VCT (32.4 dB PSNR baseline) at 0.5ms total (1/64 frame
round-robin). Probe update compute pass (Stage 5.5+ follow-up) is the next
step; current scope lays infrastructure only. `VolumeDescGpu` 64-byte
size asserted via `static_assert` (matches GLSL std430 layout).

## `src/render/vulkan/VulkanGraphicsPipeline.cpp` (session 20x, Stage 5.2.D cleanup + 5.5 DDGI bindings)

### L19-L33 (design-rationale, descriptor pool sizes)

`kGraphicsStorageDescriptorPoolSize` 6→7 (added binding 17 = volume desc SSBO).
`kGraphicsCombinedImageSamplerDescriptorPoolSize` 4→7 (added bindings 14/15/16
for DDGI probe textures, 6/11/12/14/15/16 = 6 total samplers, +1 headroom).
Pool size 6 was binding 0/1/2/3/4/13 → 5 used; +1 for binding 17 = 6. Pool
size 4 was binding 6/11/12 → 3 used; +3 for binding 14/15/16 = 6. Vulkan
spec requires `descriptorCount >= max(descriptorCount across all sets)`.
`kGraphicsShadowSamplerDescriptorPoolSize` renamed to
`kGraphicsCombinedImageSamplerDescriptorPoolSize` (имя отражало shadow usage
которого больше нет; sampler всё ещё нужен для binding 6 layer history).

## `src/shaders/{taa_resolve,model}.{frag,vert}` + `voxel_mesh.comp` (session 20x, TAA SSBO layout fix)

### L10-L26 (design-rationale, gray screen root cause)

4 шейдера имели stale `SceneLightingBuffer` SSBO struct с CSM-полями
(`sunShadowParams`, `sunShadowViewProjections[4]`, `shadowCascadeDepthSplits`,
`shadowCascadeBlendParams`) которые были удалены в 5.2.D. C++ struct
`VoxelSceneLighting` (`src/voxel/VoxelMaterials.hpp:61-105`) = 352 bytes. Stale
SSBO struct с extra fields = `colorGrading` at offset 400 (out of bounds).
Reading 0 → `clamp(0, 0.25, 4.0) = 0.25` (whitePoint) +
`clamp(0, 0, 2.0) = 0` (contrast) + `clamp(0, 0, 2.0) = 0` (saturation) →
`mix(vec3(luma), normalizedColor, 0) = vec3(0.5)` → **серый экран при TAA**.
Без TAA: `voxel.frag` использует correct SSBO struct (L27-47) и пишет final
sRGB-ready color в swapchain. Fix = удалить 4 stale fields из всех 4 шейдеров.
Pre-existing в session 19x baseline (не регрессия 5.2.D, но обнаружена и
исправлена в 20x при поиске root cause серого экрана).

## src/render/RayTracedShadows.cpp

### L203-L281 (design-rationale)

TLAS handle + backing buffer must be created **regardless** of
`accelerationStructureHostCommands` support. `hostCommands` only affects the
build path (`vkBuildAccelerationStructuresKHR` vs `vkCmdBuildAccelerationStructuresKHR`);
the handle itself is needed for ray query dispatch via `rtxTlas` binding. The
previous gate (`if (hostCommands) { vkCreateAccelerationStructureKHR(tlas); }`)
skipped TLAS creation when `hostCommands=0` (most non-Quadro NVIDIA drivers),
leaving `m_config.tlas` as `VK_NULL_HANDLE` and disabling the entire RTX shadow
path (`rtxPathActive` in `Renderer.cpp:721-731` checks `tlas != null` before
selecting `graphicsPipelineRtx`). Fix landed 2026-06-22 in session 21x. See
`CHANGELOG.md` for the bug narrative.

### L215-L227 (design-rationale)

TLAS sizing uses `VK_GEOMETRY_TYPE_INSTANCES_KHR` with
`m_config.tlasInstanceDeviceAddress` (NOT `VK_GEOMETRY_TYPE_AABBS_KHR` which would
violate `VUID-vkGetAccelerationStructureBuildSizesKHR-pBuildInfo` for
`VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR` — the spec requires instance
descriptions for top-level AS). The actual per-frame TLAS build in
`RecordTlasBuild` also uses `INSTANCES` geometry; the sizing path was a spec
violation prior to 21x even though it produced a workable build size (Vulkan
validation layer caught it).

### L797-L815 (design-rationale)

`RecordRayTracedShadowPass` is a no-op marker. The actual ray query dispatch
happens inside `voxel.frag.rtx.spv` via the `graphicsPipelineRtx` pipeline
selected in `Renderer.cpp:725-727`. The pass exists as a hook for future
fullscreen RTX post-passes (refraction, multi-bounce GI) and for backward-compat
callers; returning `false` signals "no separate work recorded this frame" — the
real per-frame RTX work is the TLAS build (`RecordTlasBuild`), which
independently increments `shadowRayDispatchCount`.

## src/render/Renderer.cpp

### L1322-L1411 (intent)

DrawFrame BLAS+TLAS block runs in three phases per frame:

1. **Collect + build BLASes.** `CollectDirtyVoxelChunkBlasRebuildRequests` drains
   `world.pendingBlasRebuildIndices` (chunks touched by voxel edits). Then
   `CollectNonBuiltBlasChunksForRayTracing` extends with scene-load chunks whose
   `nonAirVoxelCount > 0` and which lack a `blasDeviceAddress` (i.e. the initial
   BLAS build path — pending chunks queue is empty until the user actually edits
   voxels). Combined list → `SetBlasDirtyQueue` → `BuildDirtyBlases` (synchronous:
   allocates one-shot cmd buffer, dispatches, waits on fence).
2. **Populate TLAS.** `UpdateTlas` writes the instance array to the mapped
   `tlasInstanceBuffer` with `transform = identity`, `mask = 0xFF`,
   `accelerationStructureReference = blasDeviceAddresses[chunkIndex]`. Skips chunks
   without a built BLAS.
3. **Dispatch TLAS build** in `RecordGraphicsCommands` (in `L1413`) via
   `RecordTlasBuild(cmd, context)` — pre-build barrier (HOST write → AS read on
   instance buffer), `vkCmdBuildAccelerationStructuresKHR` for TLAS, post-build
   barrier (AS write → FRAGMENT read on TLAS handle).

This block is the primary wire-up of session 21x. The previous code (5.2.A)
defined `UpdateTlas` and `RecordTlasBuild` but never invoked them from the
frame loop — TLAS was permanently empty, ray queries always missed, no shadows
visible.

## src/render/vulkan/VulkanInit.cpp

### L334-L340 (design-rationale)

Second `RefreshGraphicsResourceBindings` call after
`CreateRayTracedShadowResources` to write the `rtxTlas` descriptor (binding 13)
into the graphics descriptor set. The first call (line 308) runs before
`state->render().rayTracedShadows` is set (the assignment happens at line 327),
so the write is skipped (`rtxActive = false` because `rayTracedShadows == nullptr`).
Without the second pass the rtxTlas descriptor set is never updated, and ray
query dispatch in `voxel.frag.rtx.spv` reads an undefined handle. Re-running after
`CreateRayTracedShadowResources` closes the window. Per
`https://docs.vulkan.org/spec/latest/chapters/descriptors.html`, descriptor sets
must be updated before they are bound to a command buffer; the layer's
`VUID-vkCmdDrawIndirect-None-08114` catches the violation but the visual symptom
is a "valid but uninitialized" descriptor that may or may not affect rendering
depending on driver behavior.

## src/app/LookDevCaptureAutomation.cpp

### L328-L380 (design-rationale)

Latched quit pattern. Once `automation->completed = true`, every subsequent
`UpdateLookDevCaptureAutomation` invocation returns `automation->quitWhenDone`
without touching the early-exit branches. This handles the dual-tick
characteristic of the per-frame loop: `main.cpp:556` calls
`TickLookDevCaptureSystem` (which calls `UpdateLookDevCaptureAutomation` via
flecs OnUpdate) **AND** `main.cpp:577` calls `SyncEcsWorldState` (which
re-progresses the flecs world, re-invoking the same system). Without the latch,
the second tick on the capture frame would observe `completed=true` and return
`false`, resetting `result.quitAfterFrame = false` and preventing the
`IsLookDevCaptureQuitRequested` check at `main.cpp:601` from triggering
`SDL_APP_SUCCESS`. Result: the app never exits the lookdev capture mode and
operator must kill it.

## `src/shaders/voxel_rtx_shadow.rint` (session 22x, Stage 5.2.E DDA intersection fix)

### L79-L97 (design-rationale)

Correct DDA intersection distance calculation. Initializes `tMaxAxis` relative to `rayOriginWorld` (using the same
coordinate space as `reportIntersectionEXT` and `gl_RayTminEXT`/`gl_RayTmaxEXT`) instead of `pos` (which was relative to
`pos = rayOriginWorld + rayDirWorld * tStart`). Tracks `tCurrent` as the exact crossing distance at cell entries,
reporting `tCurrent` on hits. Prevents mixing coordinate spaces (`tMinCell` relative to `pos` vs `tEnd` relative to
`rayOriginWorld`), avoiding invalid hit distances that were reconstructed close to the camera near plane and caused
floor self-shadowing (black platform) as the camera moved.

## `src/shaders/voxel_rtx_shadow.rgen` (session 23x, Stage 5.2.E raygen biasing and bias reduction fixes)

### L90-L127 (design-rationale)

Correct ray flags and biasing logic. Uses `gl_RayFlagsOpaqueEXT` for Step 1 (primary ray) to find the closest hit
point (visible surface) instead of `gl_RayFlagsTerminateOnFirstHitEXT` (which terminated on arbitrary BVH intersections,
resulting in non-closest hits). Biases Step 2 (shadow ray) along `sunDir * 0.003` (towards the light source) and uses
`T_min = 0.001` (total of `0.004` meters / `4` millimeters) to prevent self-intersection of voxel faces while keeping
the shadow starting gap (Peter Panning) completely invisible.

## `src/render/vulkan/VulkanGraphicsPipeline.cpp` (session 23x, Stage 5.2.D CSM shader loading cleanup)

### L1379-L1380 (design-rationale)

Removed loading of `voxel_shadow.vert.spv` and `voxel_shadow.frag.spv` and compilation/management of unused shadow
shader modules/stages. Following the removal of CSM in Milestone 5.2.D, the shader files were deleted, but the pipeline
initialization code still unconditionally tried to load them, causing application startup to crash with
`voxel shader blob is empty` (GraphicsPipelineFailed). Removing the unused loading checks and module creation closes the
loop.

## `src/shaders/voxel_rtx_shadow.rint` (session 23x, Stage 5.2.E DDA and Glass shadow ignore fixes)

### L95-L129 (design-rationale)

DDA voxel-level traversal fixes. Added `tCurrent` updates to `tMaxAxis` when stepping along axes, restoring correct
intersection distance propagation. Previously, `tCurrent` was never updated in the DDA loop and remained equal to
`max(tEntry, rayTmin)` (the entry point of the ray into the chunk's AABB). This caused the raygen primary trace to
register hits at the chunk's front bounds rather than the actual voxel face, producing chunk-aligned black boxes.
Additionally, added a check to ignore `Glass` (material ID `1u`) during shadow ray traversal to prevent transparent
glass spheres/shells from casting solid shadows.

## `src/render/Renderer.cpp` (session 23x, Stage 5.2.E exact chunk AABB restore)

### L1360-L1380 (design-rationale)

Restored exact, unshrunk chunk AABBs for BLAS geometry. The previous workaround shrunk AABBs by `1.5f` meters to avoid
self-shadowing, but this caused rays traversing the outer layers of chunks to miss the AABB entirely, bypassing the
intersection shader and producing empty shadow segments near chunk boundaries. Since the intersection shader correctly
handles self-shadowing through ray biasing (`sunDir * 0.02`) and DDA step offsets, the shrink offset was safely removed
to restore precise shadow coverage for all voxels.

## `src/shaders/voxel.frag` (session 23x, Stage 5.2.E shadow intensity scale and contact shadow removal)

### L798-L803 (design-rationale)

Blended ray-traced shadow visibility and contact shadow removal. Removed screen-space/voxel-DDA contact shadow
multiplication (`rtxContactVisibility`) from the RTX shadow path to prevent overlapping double-shadow edges and
DDA-aliasing artifacts on voxel corners. Additionally, introduced a shadow strength scaling constant (`0.75`) via GLSL
`mix(0.25, 1.0, rtxLit)` in `ComputeSunShadowSample` to prevent ray-traced shadows from being pitch-black, leaving a
`25%` baseline sun light factor in shadowed areas to simulate ambient bounce before real-time indirect GI probes are
fully wired.

## `src/CMakeLists.txt` (session 23x, fix for automatic shader update tracking)

### L228-L232 (design-rationale)

Changed POST_BUILD copy command of generated shader `.spv` files to a target-level dependency pipeline. By declaring a
`CopyShaders` custom target that runs unconditionally and depends on the `Shaders` target, we ensure that shader `.spv`
copies are executed on every build invocation of `ProjectV`. This resolves the dependency tracking issue in IDEs like
CLion, where editing a GLSL shader without modifying C++ sources would rebuild the `.spv` files in `build/` but skip the
copy step to `bin/` because the `ProjectV` executable itself was considered up-to-date.

## `src/shaders/voxel.frag` (session 23x, Stage 5.5 DDGI probe sampling)

### L260-L348 (design-rationale)

Dynamic Diffuse Global Illumination (DDGI) probe volume sampling. Implements full trilinear interpolation among the 8
surrounding probe nodes in the 3D grid based on the receiver's world position. To prevent light leaking and bleeding
across solid voxel walls, weights are attenuated by two factors: a normal-based cosine distribution (back-facing probes
receive zero weight) and a Chebyshev probability test based on mean and mean-squared ray travel distance stored in the
3D depth texture. Probe slice indexing uses discrete normalized depth coordinates to prevent linear filtering from
bleeding between different probe entries in the Z dimension.

## `src/shaders/voxel.frag` (session 24x, DDA traversal consolidation and refraction self-intersection fix)

### L645-L754 (design-rationale)

Chunk DDA traversal consolidation and refraction fixes. Consolidated duplicate chunk-level DDA voxel traversal logic
inside `TraceRtxAmbientOcclusionRay`, `EvaluateVoxelLighting`, and `TraceVoxelIntersection` into a single unified
`TraceVoxelIntersection` helper. Introduced `ignoreGlass` and `ignoreFluid` parameter switches to handle selective
transparent voxel culling (refraction skips all transparents to resolve self-intersection, while shadow and AO checks
can choose to ignore only glass). Added a `rayFlags` parameter to propagate customized ray query flags (
`gl_RayFlagsOpaqueEXT` and `gl_RayFlagsTerminateOnFirstHitEXT`) for driver-level hardware ray tracing optimizations,
fulfilling unit test constraints.

