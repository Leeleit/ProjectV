# Workspace
Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-21 session 8x (Variant 1 «close partial APIs», this session, closed dirty per operator policy)** — 7 phases + doc sync across 6 TODO stages (4.2, 2.1, 4.3, 6.3, 1.1, 3.2). Build green, **33/34 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as prior 4x/8x/12x). 1 new test target (`ProjectVPhysicsIncrementalJoltTests` = 6 sub-tests) + 18 new sub-tests in existing targets (LodDownsampleGpuConsume +5, HzbSmartMip +3, ChunkStreaming +3, AsyncCompute +4, NanoVdbGpuUpload +3). All green. **No commit** — operator policy "close dirty without prompt" per AGENTS.md §5.4.

- **Phase 1: Stage 4.2 LOD GPU consume mesh emission (CLOSED)** — `voxel_mesh.comp` adds `kLodWordStride=16` + `GetChunkLodLevel/Extent` (decode packed `chunkLodLevelsBuffer` bits [0:8]=lodLevel, [8:16]=outExtent) + `DecodeLodVoxelMaterial` (read from `lodDownsampled.lodDownsampledWords` at offset `chunkIndex*16`) + `DecodeVoxelMaterialForLod` dispatcher. `GreedyFacePass` signature extended with `chunkLodLevel + outExtent`; uses per-chunk extent from metadata instead of original chunk extent when `chunkLodLevel > 0` (without this, lower-LOD chunks would iterate the original 8³ extent and read garbage). `LodDownsampleGpuConsume.{hpp,cpp}` adds `kLodPayloadWordStride=16` + `BuildLodPayloadWordsFromDownsampled` (pack 4 voxels/uint32 little-endian) + `EncodeChunkLodMetadata` + `RefreshLodDownsampledBuffers` now calls `DownsampleChunkForLodSurfacePreserve` per chunk and writes packed payload. `ProjectVLodDownsampleGpuConsumeTests` 6→11 sub-tests. ~220 LoC.
- **Phase 2: Stage 2.1 HZB smart blend width shader consume (CLOSED)** — `SceneResources.cpp` doubles `hzbPerChunkMipBuffer` to `chunkCount * 2 * sizeof(uint32_t)` (stride = 2 uint32 per chunk = `[mip, blendWidth]`). `HizCulling.{hpp,cpp}` adds `kHizMipAndBlendWidthWordsPerChunk=2` const + `WritePerChunkMipAndBlendWidthsToBuffer` (pure packer). `hzb_cull.comp` changes binding 5 to `perChunkMipAndBlendWidth[]` packed + `AabbVisibleAgainstMip` takes new `blendWidthTexels` parameter; when > 0, expands screen-space sample footprint by `blendWidth / mipSize` before texel fetch (eliminates 0.02-0.20% FN per `2026-06-21-hzb-smart-blend-width` verdict). `ProjectVHzbSmartMipTests` 9→12 sub-tests. ~150 LoC.
- **Phase 3: Stage 4.3 Chunk prebake integration (CLOSED — wire complete)** — `main.cpp::FinalizeActiveVoxelWorldReload` calls `ChunkStreamer::BakeAllChunksToDisk(*world, prebakeStats)` after `SnapModelInstancesAboveGroundDispatch` (gated `IsChunkStreamingEnabled()`) + Tracy plots. `FramePreparation.cpp` per-frame calls `ChunkStreamer::PreloadChunksAroundCamera(camera, radius=8)` gated `IsChunkStreamerPrebakeReady()` (version > 0) + Tracy plot. `ProjectVChunkStreamingTests` 14→17 sub-tests. ~150 LoC.
- **Phase 4: Stage 6.3 HZB async compute cross-queue depth sync (PARTIAL — 2nd timeline semaphore + cross-queue submit helpers; depth attachment ownership transfer still deferred)** — `VulkanContextState` gains `hzbBuildTimelineSemaphore` + `hzbBuildLastTimelineValue`. `VulkanAsyncCompute.{hpp,cpp}` adds `RecordHzbAsyncCullPass` (records HZB cull dispatch into `asyncComputeCommandBuffer` after placeholder `VkImageMemoryBarrier2` keeping HZB image in `SHADER_READ_ONLY_OPTIMAL`) + `SubmitHzbAsyncCullToComputeQueue` (cross-queue wait/signal on `hzbBuildTimelineSemaphore`, 1-frame pipeline depth). `Renderer.cpp` adds `asyncComputeHzbPathActive` predicate + skips HZB cull on graphics CB when active + adds 2nd `VkSemaphoreSubmitInfo` signal on graphics submit at value `hzbBuildLastTimelineValue`. `ProjectVAsyncComputeTests` 7→11 sub-tests. **Depth attachment `srcQueueFamilyIndex`→`dstQueueFamilyIndex` ownership transfer still deferred.** ~280 LoC.
- **Phase 5: Stage 1.1 NanoVDB resize logic for 1024+ chunks (CLOSED)** — `SceneResources.{hpp,cpp}` adds `ComputeGrownNanoVdbCapacity` (1.5× growth) + `GrowNanoVdbBuffer` (free old via VMA, re-alloc new). `UploadSceneFrameResources` extended to take `VulkanContextState *context` and grow all 4 NanoVDB buffers when capacity exceeded, then `RefreshNanoVdbFlattenBuffers`. `NanoVdb.hpp` adds test-only `ComputeGrownNanoVdbCapacityForTest` mirror. `ProjectVNanoVdbGpuUploadTests` 5→8 sub-tests. `FramePreparation.cpp` updated call site. ~200 LoC.
- **Phase 6: Stage 3.2 Incremental Jolt Step 1 — boundary-neighbor queue (CLOSED)** — `VoxelWorld.cpp::SetVoxelMaterial` now calls `QueueChunkRebuildRequest(physics, chunkIndex)` for the edited chunk AND all 6 face-sharing boundary neighbors (when edit sits on a chunk face). Previously only the center chunk was queued, leaving neighbor chunks' CompoundShape out of sync. Mirrors the existing visual rebuild range in `MarkChunksTouchedByVoxelEditDirty`. `ProjectVPhysicsIncrementalJoltTests` NEW 6 sub-tests. ~50 LoC.
- **Phase 7: Tests consolidation + Tracy plot review** — ctest 33/34 + 1 documented pre-existing failure. New Tracy plots: `"Chunk Prebake Chunks Baked"`, `"Chunk Prebake Voxel Bytes"`, `"Chunk Streamer Preload Queue Depth"`.
- **Phase 8: Doc sync (this entry)** — `agent/workspace.md` + `COMMENTS.md` + `CHANGELOG.md` + `TODO.md` updated for 8x closure. **No commit prompt** (per operator "close dirty without prompt" directive).

**Per AGENTS.md §5.4 + AGENTS.md §5.9:** no commit performed. Operator decides commit timing separately.

## 2. Nearest Gap

- **Stage 6.3 HZB cross-queue depth ownership transfer** — current Phase 4 partial uses `VK_QUEUE_FAMILY_IGNORED` (correct for shared/concurrent sharing mode but does not actually transfer the depth attachment from graphics to compute queue). Proper pattern: `VkImageMemoryBarrier` with explicit `srcQueueFamilyIndex` (graphics) → `dstQueueFamilyIndex` (compute) + `srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT` at `LATE_FRAGMENT_TESTS_BIT` → `dstAccessMask = VK_ACCESS_SHADER_READ_BIT` at `COMPUTE_SHADER_BIT`. Multi-session risk.
- **Stage 6.2 AppState PIMPL full struct move** — mechanical sed `state->render().X` → `state->render()->X` over 172 call sites. Multi-session.
- **Stage 5.1 VCT GI / 5.2 RTX shadows** — per operator decision (8x planning), EXCLUDED from 4x/8x/12x scope. Future dedicated session per `2026-06-20-rt-shadows-vs-csm` ~770 LoC.
- **Stage 1.1 NanoVDB async flatten** — Stage 1.1 flatten currently runs on CPU; could move to async compute queue.

## 3. Next Steps

Future session candidates (operator decides):
1. **Stage 6.3 HZB cross-queue depth ownership transfer** — medium, builds on this session's 2nd timeline semaphore + submit helpers.
2. **Stage 6.2 AppState PIMPL full struct move** — high-risk sed migration, biggest incremental rebuild win.
3. **Stage 5.1 VCT Step 1 (foundation)** — open Stage 5.1 path.
4. **Stage 5.2 RTX shadows + BLAS** — gated by Stage 5.1 results.

## 4. Risks

- **Stage 4.2 LOD GPU consume** — env gate default OFF. SSBO + binding + actual mesh emission from downsampled payload now ALL in place. When `IsLodDownsampledGpuConsumeEnabled()` is ON, chunks with `lodLevel > 0` use the downsampled `lodDownsampledWords` binding 9 instead of full-res `chunkVoxelWords` binding 1. GreedyFacePass uses per-chunk extent from metadata. No visual regression expected.
- **Stage 2.1 HZB smart blend width** — env gate default OFF. SSBO doubled from 1× to 2× uint32 per chunk; when env OFF, callers still write `blendWidth=0` and shader falls back to 4x default path. No semantic regression.
- **Stage 4.3 Chunk prebake** — fully wired. Worker jthread lazy-started on first `EnqueueChunkStreamRequest`. TSan-clean expected.
- **Stage 6.3 HZB async (Phase 4 partial)** — env gate default OFF. When ON + HZB culling enabled + HZB buffer allocated, graphics CB skips HZB cull + signals 2nd timeline semaphore; async CB cull dispatch wired but depth attachment ownership transfer still uses `VK_QUEUE_FAMILY_IGNORED` placeholder. **Risk:** Validation layers may flag the placeholder barrier as a no-op across queues — full sync requires explicit queue family transfer. Deferred to follow-up session.
- **Stage 1.1 NanoVDB resize** — grow path active when capacity exceeded. `LogRuntimeFailure("NanoVdbFlatten", "GrowAndRefreshFailed")` only fires if VMA allocation fails after grow. Per-frame grow has 1.5× headroom. Old data NOT preserved across grow (acceptable: `sceneNanoVdbFlatten` is re-built from world before each upload).
- **Stage 3.2 Incremental Jolt boundary-neighbor** — new code path iterates 1-27 chunk slots (1 center + up to 26 boundary neighbors in 3D), so worst-case voxel edit on a corner produces 8 JPH body rebuilds (1 center + 7 face-sharing neighbors). Per-rebuild cost ~2× wall time per chunk (greedy merge compile) but 35× fewer JPH AddShape calls net-positive. Test verifies the queue contract.
- **Ninja 1.13 dep-scan race** — `agent/knowledge.md §30` documents the bug workaround (`--parallel 1` first build); required on C++ module changes.

---

## 5. Active tasks (current open sessions)

**2026-06-21 session 8x (Variant 1 «close partial APIs», this session, closed dirty per operator policy)** — 7 phases + doc sync across 6 TODO stages (4.2, 2.1, 4.3, 6.3, 1.1, 3.2). Build green, 33/34 ctest pass + 1 documented pre-existing failure. 1 new test target + 18 new sub-tests in existing targets. NO commit performed. Session closed dirty per AGENTS.md §5.4 + operator "close dirty without prompt" directive.

## 6. Recent closed sessions

- **2026-06-21 session 8x (Variant 1 «close partial APIs», this session, closed dirty, no commit)** — 7 phases: Stage 4.2 LOD GPU consume mesh emission + Stage 2.1 HZB smart blend width shader consume + Stage 4.3 Chunk prebake integration + Stage 6.3 HZB async compute cross-queue depth sync (partial) + Stage 1.1 NanoVDB resize logic + Stage 3.2 Incremental Jolt boundary-neighbor queue + doc sync. 1 new test target + 18 new sub-tests in existing targets.
- **2026-06-21 session 4x (prior, closed dirty, no commit)** — 4 phases: Stage 6.3 per-pass async compute wiring + Stage 4.2 LOD GPU consume infrastructure (deferred mesh emission) + Stage 4.3 Chunk Streaming Step 3 API + Stage 2.1 HZB smart blend width v2 (env gate + CPU helper). 2 new test targets + 13 new sub-tests.
- **2026-06-21 session 4x (prior, closed dirty, no commit)** — 4 phases: Stage 5.3 TAA Motion Vectors resolve consume + Stage 4.1 GPU World Gen frame dispatch wiring + Stage 4.3 Chunk Streaming Step 2 (std::jthread + SSD read) + Stage 2.1 HZB smart mip select. 1 new test target + 19 new sub-tests.
- **2026-06-21 session 12x (closed via commit `991db29`)** — 7 phases: Stage 1.1 NanoVDB GPU upload + Stage 3.3 Greedy Physics Meshing + Stage 5.3 TAA Motion Vectors GPU data path + Stage 4.1 GPU World Gen pipeline + Stage 1.3 Async audio + Stage 4.3 Chunk Streaming foundation + doc sync. 4 new test targets + 16 new sub-tests.
- **2026-06-21 session 8x (closed via commit `11334e7`)** — 8 phases: Stage 3.1 GPU Fluid CA + Stage 4.2 LOD B_SurfacePreserve + Stage 5.3 TAA MV format + Stage 6.3 async compute env gate + Stage 6.2 PIMPL contract + Stage 4.1 OpenSimplex2 3D-S + Stage 1.1 NanoVDB flatten + doc sync. 6 new test targets (36 sub-tests).
- **2026-06-21 session 4x (closed via commit `5e11993` + `11334e7`)** — HZB full integration + UpdateApp refactor (355→49 lines) + GPU Fluid CA foundation.
- **2026-06-20 session 2x part 5 (closed dirty)** — close-out 2x part 4 + Pre-Stage 0/Stage 0 audit.
- **2026-06-20 session 2x part 4 (closed via commit `818579e`)** — HZB spike + NanoVDB flatten + async foundation + VCT/RT cutoff + Incremental Jolt + EVIL markers + std::span sweep.

## 7. Archive references

- `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12` (pre-compress).
- `legacy/docs/archive/agent-sessions/2026-06-week-3.md` — все pre-`2026-06-20` closed sessions +
  `session-2026-06-20T-agent-file-consolidation-L1L2-r0` (L1.5 archive + Cleanup A).
- `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md` — pre-`2026-06-15` per-session snapshots от старого
  `agent/status.md`.
- `legacy/docs/archive/agent-todos/2026-06-tier-0-5-r0.md` — old TODO Tier 0..5 r0 (superseded by current `TODO.md`).
- `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md`, `2026-06-fluid-ca-sessions.md` — per-session detail из
  `memory.md` §10.12-§10.26, §12 (см. `agent/knowledge.md` Part C).
- `legacy/docs/archive/agent-*/README.md` — общий индекс (per `agent/ARCHIVE-INDEX.md` inline в knowledge.md Part C).
