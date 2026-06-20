# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-21 session 8x (closed dirty — pending operator commit)** — 8 phases across 6 TODO stages (3.1, 4.1, 4.2, 5.3, 6.2, 6.3) on top of 4x dirty baseline. Build green, **26/27 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x per earlier workspace). 6 new test targets + 36 new sub-tests, all green. **No commit yet** — operator instruction: continue 4x dirty + single "Commit?" at session end covering both 4x + 8x.

- **Phase 0: web-search gate** — `atomicOr` + bit-check is correct for Fluid CA "set bit if unset" claim; no shader change needed.
- **Phase 1: Stage 3.1 GPU Fluid CA full pipeline (CLOSED)** — `VulkanFluidCaPipeline.{hpp,cpp}` ~700 LoC + ping-pong SSBO + 5-binding descriptor + compute pipeline + `vkQueueSubmit2` cross-queue helper + ECS routing (`simulation.fluidGpuTicksPending` counter drained per frame in `Renderer.cpp`).
- **Phase 2: Stage 4.2 LOD chunk 2 B_SurfacePreserve (CLOSED)** — `VoxelLodDownsample.{hpp,cpp}` + `VoxelChunk.lodDownsampledNonAirCount` + per-frame orchestrator gated by `PROJECTV_LOD_DOWNSAMPLE`. 0 T-junction holes per `2026-06-21-lod-mesh-downsampling` experiment.
- **Phase 3: Stage 5.3 TAA Motion Vectors foundation (CLOSED)** — `kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT` in `TaaRenderTargets.hpp` per Karis 2014 + `TODO.md §5.3`. Full GPU integration (MRT + dynamic rendering + TaaResolve consume) deferred to dedicated session.
- **Phase 4: Stage 6.3 Async compute env gate (CLOSED)** — `IsAsyncComputeEnabled()` env toggle (`PROJECTV_ASYNC_COMPUTE=ON`); `SubmitFluidCaToComputeQueue` helper ready for per-pass wiring (deferred to dedicated session needing separate compute command pool).
- **Phase 5: Stage 6.2 AppState PIMPL contract (CLOSED, partial)** — `static_assert` test verifies existing 12 accessor return types; full struct move to `Types.cpp` requires ~200 file edits via mechanical sed (172 call sites) — multi-session work, deferred.
- **Phase 6: Stage 4.1 GPU Noise Step 1 (CLOSED)** — `src/shaders/world_gen.comp` CC0 OpenSimplex2 3D-S port + FBM wrapper, glslc validation green.
- **Phase 7: Stage 1.1 NanoVDB GPU integration (PARTIAL)** — CPU-side `BuildNanoVdbFlatten` wired in `UpdateSceneResources`; GPU upload (SSBO alloc + descriptor + payload version tracking) deferred.
- **Phase 8: doc sync (CLOSED)** — this entry + CHANGELOG entry.

**Previous sessions** (4x closed dirty, Pattern C mesh shader committed `5e11993`, 2x part 5 closed dirty) — see CHANGELOG.md §2026-06-21 for full details.

## 2. Nearest Gap

- **Stage 7.1 GPU upload for NanoVDB** — CPU flatten result now in `RenderState.sceneNanoVdbFlatten`; need SSBO allocation in `SceneFrameResources` + `sceneVoxelPayloadVersion` tracking + descriptor set + payload version-based upload trigger. ~150-200 LoC.
- **Stage 5.3 TAA Motion Vectors GPU integration** — `voxel.vert` needs `prevViewProjectionMatrix` access (currently only in sceneLighting SSBO, not in vertex push constants); add MRT attachment in dynamic rendering; modify `voxel.frag` passthrough; consume in `taa_resolve.frag`. ~200-300 LoC.
- **Stage 6.3 per-pass async wiring** — one-shot command buffer + dedicated compute command pool; route HZB cull + Fluid CA + future world gen + RTX BLAS through `SubmitFluidCaToComputeQueue`-style pattern. ~300 LoC.
- **Stage 6.2 AppState PIMPL full struct move** — mechanical sed `state->render().X` → `state->render()->X` over 172 call sites; risks incremental compile-test loop. Multi-session.
- **Stage 4.2 LOD chunk 2 Step 2 (SelectLodMeshSource)** — wire downsampled payload to `voxel_mesh.comp`; needs new SSBO for downsampled voxels per chunk. ~250 LoC.
- **Stage 5.1 VCT GI / 5.2 RTX shadows** — per operator decision (8x planning), EXCLUDED from 8x scope. Future dedicated session per `2026-06-20-rt-shadows-vs-csm` ~770 LoC.
- **Stage 2.1 HZB culling refinement** — per-chunk smart mip selection based on screen-space size (current mip=0). CSM HZB deferred per operator "main pipeline only".

## 3. Next Steps

Future session candidates (operator decides):
1. **Stage 7.1 NanoVDB GPU upload** — small, mechanical, unblocks Stage 5.1 VCT.
2. **Stage 6.2 AppState PIMPL full struct move** — high-risk sed migration, but biggest incremental rebuild win.
3. **Stage 5.3 TAA Motion Vectors GPU integration** — closes ghosting per `TODO.md §5.3` DoD.
4. **Stage 4.2 LOD chunk 2 Step 2** — wire downsampled payload to meshing pipeline.
5. **Stage 5.1 VCT / 5.2 RTX** — separate dedicated session.

## 4. Risks

- **Dirty tree (25 files modified + 5 untracked source + 25 untracked docs/)** — 4x + 8x combined. Operator: no commit until explicit instruction. Single "Commit?" prompt expected.
- **Stage 7 NanoVDB GPU upload deferred** — CPU-side flatten result now computed every frame but not consumed by GPU. Need to track & invalidate when `sceneNanoVdbVersion` changes to avoid leaking stale data.
- **Stage 4.2 LOD downsample CPU cost** — `RunLodDownsampleJobs` runs per-frame for ALL chunks regardless of camera motion. Trivial cost (< 1.5 µs/chunk per experiment) but full `Stage 4.2` chunk 2 still needs mesh shader integration.
- **Stage 6.2 PIMPL sed migration** — 172 call sites; one missed `.` → `->` and the build breaks. Per AGENTS.md §5.4 safety-net workflow required (patch saved to `/tmp/before_pimpl_*.patch`).
- **Ninja 1.13 dep-scan race** — `agent/knowledge.md §30` documents the bug workaround (`--parallel 1` first build); required on this dirty tree due to C++ module changes.

---

## 5. Active tasks (current open sessions)

**2026-06-21 session 8x (closed dirty, pending operator commit)** — 8 phases across 6 TODO stages, build green, 26/27 ctest pass + 1 documented pre-existing failure. NO commit pending. Single "Commit?" prompt expected at session end covering both 4x + 8x work.

## 6. Recent closed sessions

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

