# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-20 session 2x part 2 (closed dirty)** — все 8 phases done + safety-net patches per phase. Full CHANGELOG entry в `CHANGELOG.md §2026-06-20 (session: 2x scope continuation, part 2)`.

- **Phase 1: Stage 1.1 chunk 7 homogeneous optimization** — `kSparse64HomogeneousFlag = 0x40000000u` slot encoding. `MakeSparse64Homogeneous` / `IsSparse64Homogeneous` / `Sparse64HomogeneousMaterial` helpers. `SetCellRecursive` cascades homogeneous markers upward when all 64 children collapse. `LiveNodeCount()` helper. 5 new sub-tests pass.
- **Phase 2: Stage 1.2 chunks 4-5 per-chunk static flag + lazy promotion** — `VoxelChunk` + `isStatic` + `ticksSinceLastEdit`. `TickVoxelChunkStaticPromotion` API + env `PROJECTV_SVDAG_STATIC_PROMOTION_TICKS` (default 60). `SetVoxelMaterial` auto-demotes. 2 new sub-tests pass.
- **Phase 3: Stage 2.1 task+mesh shader (nvidia-only)** — `voxel_mesh.task` (frustum cull + EmitMeshTasksEXT) + `voxel_mesh.mesh` (stub triangle emission). `--target-env vulkan1.3` for shader compile. CMake registers both. Integration deferred.
- **Phase 4: Stage 2.2 HZB image lifecycle** — `CreateHizBuffer` (VMA image + image view), `DestroyHizBuffer`, `BuildHizMipChain` (vkCmdBlitImage NEAREST from depth + LINEAR between mips + barriers). Renderer integration deferred (Phase 4 = infra ready, not wired).
- **Phase 5: 6.1 +3 Flecs ECS systems** — `VoxelInteractionTickSystem`, `BenchmarkAutomationTickSystem`, `LookDevCaptureTickSystem`. New ECS result components. `allowWorldEditing` added to `WorldState`. Inline blocks replaced.
- **Phase 6: 6.2.5 +13 EVIL markers** — `kEpsilon` + `kDirectionEpsilon` in VoxelRaycast.cpp, 11 markers in VoxelMaterials.cpp exposure/color-grade/shadow/contact-shadow constants.
- **Phase 7: 6.2.6 vkWaitForFences 10ms audit** — `kVulkanFenceWaitTimeoutNs` + `kVulkanFenceWaitTimeoutUnboundedNs` constants in Types.hpp. Renderer.cpp + FramePreparation.cpp updated.
- **Phase 8: doc sync** — этот update + CHANGELOG entry.

Build green, 19/19 ctest pass (~1.0 sec).

## 2. Nearest Gap

- **Stage 2.1 task+mesh shader full integration** — current task+mesh shaders are spike-only (task culls, mesh emits stub triangle). Real greedy meshing in mesh shader is multi-session work.
- **Stage 2.2 HZB full integration** — image lifecycle ready (Phase 4 part 2). Need AABB SSBO + hzb_cull.comp dispatch + vkCmdDrawIndirectCountKHR wiring + visible-mask readback. Chunk AABB source from SVDAG cache still TBD.
- **Stage 3.1 GPU Fluid CA** — per `agent/knowledge.md §30.4` contract, depends on Stage 1.2 done (✓ chunk 1-3 done + homogeneous + static promotion), but full chunk AABB SSBO + GPU dispatch + test fixtures not yet built.
- **Stage 3.2 Incremental Jolt** — per-chunk static bodies, rebuild only diff chunks.
- **Stage 4.2 Geometry LOD** — per-chunk LOD level + geomorphing.
- **6.2.2 std::span sweep (next 2x)** — HZB integration in Phase 4 introduced buffer-view sites (HizBuffer image + mip chain SSBO). Next session can do bigger span migration.
- **6.2.5 EVIL markers** — now 23 total (was 9). Still ~5-10 more in other files (ShadowProjection, voxel.frag, etc.) — note: shaders can't have comments per AGENTS.md §8.1.

## 3. Next Steps

Продолжать Stage 2 (Mesh shaders full + HZB full integration) → Stage 3 (GPU Fluid + Incremental Jolt) → Stage 4 (LOD + draw distance) → Stage 5 (VCT + RTX).

## 4. Risks

- **Stage 1.2 dedup OFF by default**: `SetDeduplicationEnabled(true)` now safe via COW + homogeneous collapse, but real-world memory benefit не измерен на VoxelLab scenes.
- **Stage 2.2 HZB image ready but not used**: `CreateHizBuffer` + `BuildHizMipChain` work, but Renderer.cpp не вызывает их. Чтобы увидеть эффект, нужен `PROJECTV_HZB_CULLING=ON` + Renderer integration.
- **Stage 2.1 task+mesh spike**: shaders compile but emit stub geometry (1 triangle per visible chunk at chunk origin). No real greedy meshing. NDC positions, not world-space. Needs full port for production.
- **6.2.2 std::span sweep**: 1 target migrated (`BuildMaterialVisualTable`). HZB integration creates new buffer-view sites for next 2x.
- **VoxelInteractionTickSystem moved to ECS**: timing changed from "during UpdateApp" to "after UpdateApp". Should be semantically identical (voxel interaction reads world state, not modified between UpdateApp and ECS tick).
- **Per-chunk static threshold (60 ticks = 1 sec @ 60 Hz)**: may need tuning on real workloads. Configurable via env.

---

## 5. Active tasks (current open sessions)

None (2x part 2 complete, dirty tree waiting for commit).

## 6. Recent closed sessions

- **2026-06-20 session 8x (closed via commit `c2000e2`)** — Pre-Stage 0+Stage 0 quick wins (B1-B4, A1) + Stage 1.1 chunk 1 (Sparse64Tree header + 15 parity sub-tests).
- **2026-06-20 session x8 (closed dirty)** — Stage 1.1 chunks 2-3-4 (VoxelWorld migration to Sparse64Tree, snapshot v2) + Stage 1.3 (async audio scan) + 6.2.4 verification + 6.2.5 EVIL markers.
- **2026-06-20 session 16x (closed dirty)** — Phase 1: Stage 1.2 SVDAG dedup infrastructure. Phase 2: AppState PIMPL. Phase 3: std::span deferred. Phase 4: more EVIL markers. Phase 5: Flecs ECS AudioRefresh. Phase 6: doc sync.
- **2026-06-20 session 2x (closed dirty)** — Phases 1-8 (previous 2x session): `world.voxels` removal + SVDAG COW + CpuMeshGenerator fallback + DDA shader macro + std::span sweep + FluidCA ECS + HZB spike + doc sync.
- **2026-06-20 session 2x part 2 (closed dirty)** — Phases 1-8 (current session): homogeneous optimization + per-chunk static promotion + task+mesh shaders + HZB image lifecycle + 3 more ECS systems + 13 EVIL markers + vkWaitForFences constant + doc sync.

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

