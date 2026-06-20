# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-20 session 2x part 5 (in progress — no commit, dirty tree)** — Build green, 19/20 ctests pass + 1 documented pre-existing failure (`TestSpectatorModeAllowsPausedMovementButBlocksEdits:2629`).
- **Phase 9: close-out 2x part 4** — `ProcessChunkRebuildQueue(physics, world->voxelWorld.get())` now called per-frame in `AppUpdate.cpp` after post-interaction `SyncPhysicsWorld`. Tracy plot "Processed Chunk Rebuilds" tracks per-frame count. Per-chunk static body rebuild path now active end-to-end (2x part 4 Phase 5 closed).
- **Phase 10: Pre-Stage 0 / Stage 0 audit** — B1 (redundant model load loop), B2 (RecreateSwapchain destroy pipeline), B3 (cache PROJECTV_GRAVIGUN_SNAP), A1 (Vulkan 1.4 → 1.3 + env override) verified CLOSED in current mainline. No code changes needed; status can be flipped in TODO.md.

**Previous session summary (2x part 4, committed `818579e`):**
- **Phase 1: HZB Renderer integration** — pipeline + descriptor set + buffers + dispatch all wired. Env `PROJECTV_HZB_CULLING=ON` enables; default OFF → no-op. CSM HZB deferred per operator "main pipeline only".
- **Phase 2: NanoVDB flatten helper** — `src/voxel/NanoVdb.{hpp,cpp}` + `tests/NanoVdbTests.cpp` (1035 assertions, 0 failures). Hybrid strategy per `nanovdb-on-gpu` verdict=yes. ProjectV chunkSize=8 (depth=2). byte-exact re-walk test.
- **Phase 3: async foundation Step 1** — dedicated compute queue family detection + VK_KHR_timeline_semaphore + `VulkanSyncPrimitives.{hpp,cpp}`. VulkanContextState extended with `dedicatedComputeQueue`, `renderTimelineSemaphore`, `renderTimelineValue`. Per `dec-pipelines-async-compute` verdict=yes.
- **Phase 4: VCT/RT cutoff** — `kVctCutoffRoughness=0.3f` + `kRtxCutoffRoughness=0.3f` constants in `core/Types.hpp`. `PROJECTV_HW_RAY_TRACING` CMake flag (default OFF). `ProbeHardwareRayTracingSupport` probes VK_KHR_acceleration_structure + VK_KHR_ray_query extension strings.
- **Phase 5: Incremental Jolt wiring** — `SetVoxelMaterial(VoxelWorld&, Int3, VoxelMaterial, PhysicsState* = nullptr)` signature change. VoxelInteraction.cpp passes physics state through (edit path now queues per-chunk rebuilds). Mechanical sed updated all 17 callers. **Per-chunk rebuild queue: now ACTIVE in Phase 9 (see above).**
- **Phase 6: +12 EVIL markers** — 4 in `VulkanBootstrap.cpp` (queuePriority=1.0f, queueCount=1, computeQueueInfo.queueCount=1, UINT32_MAX sentinel) + 8 in `PhysicsWorld.cpp` (kPhysicsDirectionEpsilon, kPhysicsRaycastVoxelEpsilon, kWalkSpawnClearance, kWalkSneakShapeMaxPenetrationDepth, etc).
- **Phase 7: std::span sweep** — `RefreshChunkAabbBuffer` now takes `std::span<const VoxelChunk>` + `std::span<const PackedSceneChunkDescriptor>` instead of (VoxelWorld&, RenderState&).
- **Phase 8: doc sync** — this entry + new test target `ProjectVNanoVdbTests` registered in ctest.

- **Phase 1: BLOCKED on `hzb-binding-models` verdict** — per operator "ждём". `HizCulling.{hpp,cpp}` image lifecycle + `BuildHizMipChain` + `hzb_cull.comp` shader ready for integration when verdict closes. Path D (storage + texelFetch + manual min) is universal safe fallback per `hzb-binding-models` §2.2.
- **Phase 2: Stage 2.1 Pattern C mesh shader spike (FEATURE-FLAGGED)** — `voxel_mesh_pre.comp` frustum cull + `voxel_mesh.mesh` refactored to read `visibleChunkIds[]`. `IsMeshShaderPipelineEnabled()` env toggle. Per `mesh-shader-vs-compute-cull` verdict=mixed: compute cull remains DEFAULT, mesh shader optional.
- **Phase 3: Stage 3.1 GPU Fluid CA skeleton** — `fluid_ca.comp` per `§30.4` contract (8×8×4 workgroup, atomicOr, z,y,x iteration). `IsFluidCaGpuEnabled()` env + `BuildActiveChunkIdsForFluidCa` helper. Full pipeline integration deferred until async-compute foundation (Phase 0 per `dec-pipelines-async-compute`).
- **Phase 4: Stage 3.2 Incremental Jolt per-chunk BodyId map** — `PhysicsState::chunkStaticBodies` (unordered_map<uint32_t, BodyID>) + `pendingChunkRebuilds`. `QueueChunkRebuildRequest` + `ProcessChunkRebuildQueue` (sorted + dedup'd). Per-chunk rebuild only on edit. Old single-body world body retained.
- **Phase 5: Stage 4.2 LOD chunk 1 per-chunk LOD level + distance selection** — `VoxelChunk.lodLevel` (uint8_t, struct size 36→40). `SelectLodLevelForDistance` (0/1/2/3 for <32m/<64m/<128m/≥128m). `AssignLodLevels(world, camX, camY, camZ)`. Uniform downsampling deferred.
- **Phase 6: 6.1 UpdateApp god-function refactor** — 3 helpers extracted to `AppUpdateHelpers.{hpp,cpp}`: `UpdateFrameStatistics`, `UpdateEffectivePausedAndEditing`, `RunSimulationTickLoop`. Camera mode functions promoted to public API. Push descriptor Phase A (per `bindless-descriptor-overhead` §7.1) deferred.
- **Phase 7: 6.2.2 std::span sweep (3 sites)** — `ShadowProjection.cpp` helpers migrated from `const std::array<Float3, 8>&` to `std::span<const Float3>`. `BuildBoundsCorners` / `BuildFrustumSliceCorners` return types preserved as `std::array<Float3, 8>`.
- **Phase 8: 6.2.5 +39 EVIL markers (62 total now)** — 10 in `ShadowProjection.cpp` (kShadow* constants) + 22 in `AppUpdate.cpp` (kLighting*/kShadow* constants).
- **Phase 9: doc sync** — этот update + CHANGELOG entry.

Build green, 4/4 relevant tests pass (Hzb/Sparse/Cpu/Fluid). ProjectVTests has 1 pre-existing failure (TestSpectatorModeAllowsPausedMovementButBlocksEdits at line 2629) unrelated to my changes (confirmed via git stash).

## 2. Nearest Gap

- **Stage 3.1 GPU Fluid CA pipeline integration** — ping-pong buffers + actual dispatch + cross-frame sync. Requires async-compute Step 2 = per-pass async adoption remaining (Step 1 done in 2x part 4 Phase 3).
- **Stage 2.1 mesh shader full integration** — `voxel_mesh_pre.comp` + `voxel_mesh.mesh` in dirty tree from prior 2x part 3 sessions; needs SVDAG mainline + port greedy meshing from `voxel_mesh.comp`.
- **Stage 4.2 LOD chunk 2** — uniform downsampling implementation. Distance LOD selection works (2x part 3 Phase 5) but actual mesh-level downsampling not yet built.
- **Stage 5.x GI/temporal** — Stage 5.1 VCT, 5.2 RTX shadows (probe ready), 5.3 TAA motion vectors — not started.
- **HZB Stage 2.2 deferred** — CSM HZB culling deferred per operator "main pipeline only".
- **Dirty tree (20 files, ~2586+/679-)** — all from prior 2x part 1/2/3 sessions that closed dirty. NOT my session's scope. Operator instruction: no commits; tree stays as-is until next session with explicit scope.

## 3. Next Steps

2x part 6 candidate: Stage 3.1 GPU Fluid CA pipeline integration (multi-session work — uses Phase 3 async foundation + Phase 2 NanoVDB flatten). Alternative: Stage 2.1 mesh shader full integration (port `voxel_mesh.comp` greedy meshing to mesh shader output, using `voxel_mesh_pre.comp` + `voxel_mesh.mesh` from dirty tree).

## 4. Risks

- **Phase 1 BLOCKED on `hzb-binding-models`** — currently in wrap-up phase per STATUS.md but no verdict. If verdict recommends Path B (compute atomicMin), Phase 1 needs buffer layout changes. Safe fallback = Path D (storage + texelFetch + manual min), universal across bindless.
- **Pattern C mesh shader** — stub-only emission (1 triangle per chunk at chunk origin). Real greedy meshing port is multi-week work. Feature-flagged, doesn't promote to default per `mesh-shader-vs-compute-cull` verdict=mixed.
- **GPU Fluid CA** — compute shader compiles but no pipeline integration (ping-pong buffers, dispatch). Per `dec-pipelines-async-compute` Step 1 should come first (vkQueueSubmit2 + timeline semaphores).
- **Incremental Jolt** — `ProcessChunkRebuildQueue` exists but isn't called from voxel edit path yet. Real wiring in next session.
- **LOD uniform downsampling** — chunk-level LOD assignment works but actual mesh downsampling not implemented. World will render full-detail chunks at all LODs until chunk 2.

---

## 5. Active tasks (current open sessions)

**2026-06-20 session 2x part 5** — Phase 9 (ProcessChunkRebuildQueue main-loop wire) + Phase 10 (Pre-Stage 0/Stage 0 audit). Build green, ctest 19/20 + 1 documented pre-existing failure. NO commit pending (operator instruction).

## 6. Recent closed sessions

- **2026-06-20 session 8x (closed via commit `c2000e2`)** — Pre-Stage 0+Stage 0 quick wins (B1-B4, A1) + Stage 1.1 chunk 1 (Sparse64Tree header + 15 parity sub-tests).
- **2026-06-20 session x8 (closed dirty)** — Stage 1.1 chunks 2-3-4 (VoxelWorld migration to Sparse64Tree, snapshot v2) + Stage 1.3 (async audio scan) + 6.2.4 verification + 6.2.5 EVIL markers.
- **2026-06-20 session 16x (closed dirty)** — Phase 1: Stage 1.2 SVDAG dedup infrastructure. Phase 2: AppState PIMPL. Phase 3: std::span deferred. Phase 4: more EVIL markers. Phase 5: Flecs ECS AudioRefresh. Phase 6: doc sync.
- **2026-06-20 session 2x (closed dirty)** — Phases 1-8 (first 2x session): `world.voxels` removal + SVDAG COW + CpuMeshGenerator fallback + DDA shader macro + std::span sweep + FluidCA ECS + HZB spike + doc sync.
- **2026-06-20 session 2x part 2 (closed dirty)** — Phases 1-8 (commit `32f710b`): homogeneous optimization + per-chunk static promotion + task+mesh shader spike + HZB image lifecycle + 3 more ECS systems + EVIL markers + vkWaitForFences.
- **2026-06-20 session 2x part 3 (closed dirty)** — Phases 2-9 (current session). Phase 1 BLOCKED on `hzb-binding-models` verdict. Full CHANGELOG entry в `CHANGELOG.md §2026-06-20 (session: 2x scope continuation, part 3)`.

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

