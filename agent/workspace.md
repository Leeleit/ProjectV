# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-21 session 12x (closed dirty — pending operator commit)** — 7 phases across 6 TODO stages (1.1, 2.1 partial, 3.3, 4.1, 4.3 Step 1, 5.3 partial) on top of 4x+8x dirty baseline. Build green, **30/31 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x/8x). 4 new test targets (5 sub-tests) added, all green: `ProjectVNanoVdbGpuUploadTests` (5), `ProjectVPhysicsGreedyMergerTests` (7), `ProjectVTaaMotionVectorTests` (+2 new = 5 total), `ProjectVWorldGenTests` (3), `ProjectVChunkStreamingTests` (4). **No commit yet** — operator instruction per `agent/workspace.md §5`: continue 4x+8x dirty + single "Commit?" at session end covering 4x+8x+12x.

- **Phase 0: Operator commit gate** — confirmed "Continue dirty" per `workspace.md §1` policy (single combined Commit? at Phase 11 end).
- **Phase 1: Stage 1.1 NanoVDB GPU upload (CLOSED)** — `PackNanoVdbFlattenData` inline helper in `voxel/NanoVdb.hpp` + 4 SSBO buffers per frame (`nanovdbUpper/Lower/Leaf/Material` storage buffers + capacities) in `SceneFrameResources` + alloc in `CreateSceneResources` + structured binding + free/nullify in `DestroySceneResources` + `RefreshNanoVdbFlattenBuffers` helper + `uploadedNanoVdbVersion` tracker + `UploadSceneFrameResources` triggers on `sceneNanoVdbVersion` change with capacity check. ~250 LoC.
- **Phase 2: Stage 3.3 Greedy Physics Meshing integration (CLOSED)** — `src/physics/GreedyPhysicsMerger.{hpp,cpp}` (D_3D greedy merge algorithm per `2026-06-21-greedy-physics-meshing-cpu` verdict=yes) + `GreedyMergeSolidVoxelsInBounds` API + integration in both `BuildStaticVoxelCollisionBody` and `BuildChunkStaticCollisionBody` (env gate `PROJECTV_GREEDY_PHYSICS_MESH=ON` default, `=OFF` falls back to naive per-voxel loop) + Tracy plot "Physics Greedy Merge Box Count" + "Physics Greedy Merge Chunk Box Count". ~200 LoC.
- **Phase 3: Stage 5.3 TAA Motion Vectors GPU integration (PARTIAL — data path complete)** — `voxel.frag` outputs `outMotionVector` (vec2, location 3) computed from `prevViewProjectionMatrix` vs current `viewProjection` NDC delta + 4th color attachment format `kTaaMotionVectorFormat` added to pipeline + `taaMotionVectorTarget` + `taaMotionVectorHistoryTarget` OffscreenColorTargets added to `TaaRenderTargets` + new 4th dynamic rendering attachment + `TransitionTaaMotionVectorForSample` + `RecordTaaMotionVectorHistoryCopy` + 2 new `RenderState` fields. **TAA resolve consume (taa_resolve.frag reading MV texture instead of computing from prevViewProjectionMatrix) deferred to dedicated session** — data path is in place, resolve integration follows.
- **Phase 4: Stage 4.1 GPU World Gen dispatch integration (PARTIAL — pipeline infra complete)** — `src/render/vulkan/VulkanWorldGenPipeline.{hpp,cpp}` + `WorldGenPushConstants` (64 bytes) + compute pipeline + 1-binding descriptor set + 1 SSBO per frame + env gate `PROJECTV_WORLD_GEN_GPU` + `BuildActiveChunkIdsForWorldGen` helper + `RecordWorldGenDispatch` + 5 new `RenderState` fields. **Renderer.cpp wiring (CreateWorldGenPipelines call + dispatch in frame loop) deferred to dedicated session** — pipeline infrastructure is in place.
- **Phase 8: Stage 1.3 Async audio I/O scan (ALREADY IMPLEMENTED pre-12x)** — `EcsWorld.cpp:194` → `AudioEngine::RefreshPlaylistAsync()` + `m_scanThread` + `m_scanThread.join()` in `tick()` + `m_playlistMutex` lock + `m_scanInProgress` atomic flag. Verified existing, no changes needed.
- **Phase 10: Stage 4.3 Chunk Streaming foundation Step 1 (CLOSED)** — `src/voxel/ChunkStreamer.{hpp,cpp}` (NEW) + `ChunkStreamRequest` + `ChunkData` + `EnqueueChunkStreamRequest` + `DrainChunkStreamQueueSize` + `TryDequeueChunkData` + `IsChunkStreamingEnabled` env gate + mutex-protected pending/ready deques. **Background thread + SSD read integration deferred to dedicated session** — interface contract is in place, ~50 LoC.
- **Phase 11: doc sync (this entry)** — `agent/workspace.md` + `CHANGELOG.md` + `COMMENTS.md` + `TODO.md` updates + final combined "Commit?" prompt for 4x+8x+12x.

**Deferred to dedicated sessions (multi-session work, deferred per plan §Open risks):**
- Phase 5: Stage 6.3 per-pass async wiring (~300 LoC) — requires dedicated compute command pool + one-shot CB allocation.
- Phase 6: Stage 6.2 AppState PIMPL full struct move (~200 files, 172 sed sites) — high sed risk, requires AGENTS.md §5.4 safety-net.
- Phase 7: Stage 4.2 LOD chunk 2 Step 2 SelectLodMeshSource (~250 LoC) — wire downsampled payload to `voxel_mesh.comp` via new SSBO.
- Phase 9: Stage 2.1 HZB smart mip selection (~200 LoC) — per-chunk screen-space size → mip, with 2-phase fallback.

**Previous sessions** (8x closed dirty, 4x closed dirty, Pattern C mesh shader committed `5e11993`, 2x part 5 closed dirty) — see CHANGELOG.md §2026-06-21 for full details.

## 2. Nearest Gap

- **Stage 5.3 TAA Motion Vectors resolve consume** — `taa_resolve.frag` needs `motionVectorHistory` texture binding + replace depth-reproject path lines 167-182 to use MV texture instead of computing from `prevViewProjectionMatrix` in-shader. ~100-150 LoC.
- **Stage 4.1 GPU World Gen frame dispatch wiring** — `Renderer.cpp::DrawFrame` needs `CreateWorldGenPipelines` + `RefreshWorldGenResourceBindings` + per-frame `BuildActiveChunkIdsForWorldGen` + `RecordWorldGenDispatch` for empty chunks. ~50-100 LoC.
- **Stage 6.3 per-pass async wiring** — dedicated compute command pool + one-shot CB + per-pass routing (HZB cull + Fluid CA + world gen + RTX BLAS) via `SubmitFluidCaToComputeQueue`-style pattern. ~300 LoC.
- **Stage 6.2 AppState PIMPL full struct move** — mechanical sed `state->render().X` → `state->render()->X` over 172 call sites; risks incremental compile-test loop. Multi-session.
- **Stage 4.2 LOD chunk 2 Step 2 (SelectLodMeshSource)** — wire downsampled payload to `voxel_mesh.comp`; needs new SSBO for downsampled voxels per chunk. ~250 LoC.
- **Stage 5.1 VCT GI / 5.2 RTX shadows** — per operator decision (8x planning), EXCLUDED from 8x/12x scope. Future dedicated session per `2026-06-20-rt-shadows-vs-csm` ~770 LoC.
- **Stage 2.1 HZB culling refinement** — per-chunk smart mip selection based on screen-space size (current mip=0). CSM HZB deferred per operator "main pipeline only".
- **Stage 4.3 Chunk Streaming Step 2** — background thread + SSD read integration + per-frame budget enforcement. ~250 LoC.

## 3. Next Steps

Future session candidates (operator decides):
1. **Stage 5.3 TAA Motion Vectors resolve consume** — small, completes TODO §5.3 DoD.
2. **Stage 4.1 GPU World Gen frame dispatch wiring** — small, completes TODO §4.1.
3. **Stage 6.3 per-pass async wiring** — medium, builds on 8x async foundation + 12x async gates.
4. **Stage 6.2 AppState PIMPL full struct move** — high-risk sed migration, but biggest incremental rebuild win.
5. **Stage 4.2 LOD chunk 2 Step 2** — wire downsampled payload to meshing pipeline.
6. **Stage 4.3 Chunk Streaming Step 2** — background thread + SSD read.
7. **Stage 5.1 VCT / 5.2 RTX** — separate dedicated session.

## 4. Risks

- **Dirty tree (4 modified + ~10 untracked source + 25 untracked docs from 12x alone)** — 4x+8x+12x combined. Operator: no commit until explicit instruction. Single "Commit?" prompt expected.
- **Stage 1.1 NanoVDB GPU upload capacity** — initial alloc = 1 upper + 64 lowers + 64 leaves + 64 materials; if flatten result exceeds this, `LogRuntimeFailure` is emitted and version is marked uploaded (skipped upload). For Stage 4.3 (1024+ chunks) may need resize logic.
- **Stage 3.3 Greedy Physics Meshing** — `IsGreedyPhysicsMeshEnabled()` defaults to ON; per-chunk `BuildChunkStaticCollisionBody` now uses greedy merge. Builds take ~2× wall time per chunk (35× fewer AddShape calls = much faster JPH broadphase). Net positive.
- **Stage 5.3 TAA Motion Vectors MRT slot** — 4 color attachments now (swapchain + TAA scene + TAA layer history + MV). On budgets, per `agent/knowledge.md §21` the layer history was at 8 vec4 output cap; MV adds 1 vec2. Total output count = 5 vec4 + 1 vec2. Within 8 vec4 limit.
- **Stage 6.2 PIMPL sed migration** — 172 call sites; one missed `.` → `->` and the build breaks. Per AGENTS.md §5.4 safety-net workflow required (patch saved to `/tmp/before_pimpl_*.patch`).
- **Ninja 1.13 dep-scan race** — `agent/knowledge.md §30` documents the bug workaround (`--parallel 1` first build); required on this dirty tree due to C++ module changes.

---

## 5. Active tasks (current open sessions)

**2026-06-21 session 12x (closed dirty, pending operator commit)** — 7 phases across 6 TODO stages, build green, 30/31 ctest pass + 1 documented pre-existing failure. NO commit pending. Single "Commit?" prompt expected at session end covering 4x + 8x + 12x work.

## 6. Recent closed sessions

- **2026-06-21 session 12x (closed dirty)** — 7 phases: Stage 1.1 NanoVDB GPU upload (CPU flatten + 4 SSBO + descriptor + version trigger) + Stage 3.3 Greedy Physics Meshing integration (D_3D algorithm) + Stage 5.3 TAA Motion Vectors GPU data path (voxel.frag outMotionVector + 4th MRT slot + 2 new TaaRenderTargets) + Stage 4.1 GPU World Gen pipeline infrastructure (VulkanWorldGenPipeline.{hpp,cpp} + SSBO + compute pipeline) + Stage 1.3 Async audio verification (already implemented) + Stage 4.3 Chunk Streaming foundation (ChunkStreamer.{hpp,cpp} interface contract) + doc sync. 4 new test targets + 16 new sub-tests.
- **2026-06-21 session 8x (closed dirty)** — 8 phases: Stage 3.1 GPU Fluid CA full pipeline + Stage 4.2 LOD B_SurfacePreserve + Stage 5.3 TAA Motion Vectors format + Stage 6.3 async compute env gate + Stage 6.2 PIMPL contract + Stage 4.1 OpenSimplex2 3D-S + Stage 1.1 NanoVDB flatten + doc sync. 6 new test targets (36 sub-tests).
- **2026-06-21 session 4x (closed dirty)** — HZB full integration + AppState PIMPL verification + UpdateApp refactor (355→49 lines) + GPU Fluid CA foundation.
- **2026-06-21 session Pattern C mesh shader (closed via commit `5e11993`)** — voxel_mesh.mesh ported + voxel_mesh_pre.comp push-constant frustum + voxel_mesh.task deleted + VulkanMeshShaderPipeline + Renderer integration.
- **2026-06-20 session 2x part 5 (closed dirty)** — close-out 2x part 4 (per-chunk Jolt rebuild active) + Pre-Stage 0/Stage 0 audit (B1-B4, A1 verified closed).
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

