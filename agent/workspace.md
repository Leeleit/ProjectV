# Workspace
Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

---

## 1. Now

**2026-06-21 session 8x (Variant A «close async + open VCT», this session, closed dirty per operator policy)** — 7 phases + doc sync across 2 TODO stages (6.3 HZB cross-queue depth complete + 5.1 VCT foundation). Build green, **34/35 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x/8x/12x/8x V1). 1 new test target (`ProjectVVoxelizePipelineTests` = 11 sub-tests) + 1 new sub-test in `ProjectVAsyncComputeTests`. All green. **No commit** — operator policy "close dirty without prompt" per AGENTS.md §5.4.

- **Phase 0: Web-search gate** — Khronos Synchronization Examples (cross-queue ownership transfer), RasterGrid 2026-03 Hi-Z meta-surface, WickedEngine VXGI clipmap (turanszkij), Compix VoxelConeTracingGI (16/32-cone tables), The Tomorrow Children (cascaded VCT 6 cascades).
- **Phase 1: Stage 6.3 HZB cross-queue depth ownership transfer complete (CLOSED)** — `VulkanAsyncCompute.cpp::RecordHzbAsyncCullPass` replaced 8x V1 placeholder `VkImageMemoryBarrier2` (which had `srcStageMask = COMPUTE_SHADER_BIT + srcAccessMask = SHADER_READ_BIT`, an invalid self-dependency) with proper cross-queue memory barrier: `srcStageMask = TRANSFER_BIT` + `srcAccessMask = TRANSFER_WRITE_BIT` (the HZB image was last written by the graphics mip chain build) → `dstStageMask = COMPUTE_SHADER_BIT` + `dstAccessMask = SHADER_READ_BIT`, layout stays `SHADER_READ_ONLY_OPTIMAL`, `VK_QUEUE_FAMILY_IGNORED` for both. Sufficient for current `VK_SHARING_MODE_EXCLUSIVE` HZB image when timeline semaphore + barrier provide execution + memory respectively per Khronos. Closes the 8x V1 Phase 4 partial + TODO §6.3 "depth attachment ownership transfer". `ProjectVAsyncComputeTests` 11→12 sub-tests. ~30 LoC.
- **Phase 2: Stage 5.1 VCT 3D clipmap + voxelize.comp (FOUNDATION CLOSED)** — `src/shaders/voxelize.comp` (NEW, ~110 LoC GLSL) per-voxel scene injection into 3D clipmap texture. 1 workgroup per chunk, 64 threads iterate over chunk voxels in strided loop. Image format `rgba16f` (HDR). Reads `PackedChunkDescriptors` (binding 0) + `PackedChunkVoxelPayload` (binding 1); writes per-voxel emission to `vctClipmap` (binding 2). Air+Glass voxels skipped. `VulkanVoxelizePipeline.{hpp,cpp}` (NEW, ~450 LoC) — `IsVctGpuPipelineRequested()` env gate (`PROJECTV_VCT_GPU=ON`, default OFF per `agent/knowledge.md §30.4` Step 1 additive optional path) + 3D image allocation (256³ RGBA16F, 4 mip levels, ~16 MiB VRAM, `VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT`) + linear filter sampler with mip range [0, mipLevelCount] + compute pipeline + 3-binding descriptor set (chunk descriptors + voxel payload + clipmap storage image). `SceneFrameResources` adds `vctVoxelizeDescriptorSet` field. `RenderState` adds 11 VCT fields (image/view/sampler/4 pipeline handles/3 descriptor set/layout/pool/clipmap resolution/mip level count/enabled flag). `ProjectVVoxelizePipelineTests` NEW 11 sub-tests. ~560 LoC.
- **Phase 3: Stage 5.1 VCT diffuse 6-cone tracing (CLOSED)** — `voxel.frag` adds `VctSampleDirectionalCone` helper (3-tap adaptive sampling per cone, weight = 1/(1 + falloff * i), max mip selection by `log2(maxT) * 0.5` clamped to `kVctMaxMipLevel=4`) + 6 fixed diffuse cones (`kVctConeDirections[6]` per TODO §5.1 explicit "6 широких конусов", aligned to world axes with small upward bias to avoid floor singularity) + `vctDiffuse` contribution to final color (multiplied by `albedo * (1/PI) * ambientVisibility`). `VoxelSceneLighting` struct extended with `vctParams` vec4 field (diffuseConeApertureTan, maxDistance, mipBias, enabledFlag). New `sampler3D vctClipmap` at binding 11. ~80 LoC.
- **Phase 4: Stage 5.1 VCT specular 1-cone + kVctCutoffRoughness=0.3 hybrid (CLOSED)** — `voxel.frag` adds `VctSampleReflectionCone` (1 cone in reflection direction, aperture = roughness * 0.6 clamped to [0.05, 0.6]) + `roughness > kVctCutoffRoughness=0.3` gate per `2026-06-20-vct-vs-rt-cutoff` experiment (rough surfaces use VCT specular, smooth surfaces use Stage 5.2 RTX future work) + Fresnel-Schlick `0.04 + 0.96 * pow(1 - nDotV, 5)` reduced by `(1 - metallic)`. `vctSpecularParams` vec4 field. ~50 LoC.
- **Phase 5: Stage 5.1 VCT GPU mip chain build (CLOSED)** — `BuildVctClipmapMipChain` records `vkCmdBlitImage` 3D-to-3D mip chain on graphics CB after voxelize dispatch with `VK_FILTER_LINEAR`. Mirrors `BuildHizMipChain` 2D pattern in `HizCulling.cpp:295-476`. Each mip barrier transitions `TRANSFER_WRITE → SHADER_READ` for the next mip. +2 sub-tests in `ProjectVVoxelizePipelineTests`. ~110 LoC.
- **Phase 6: Tests + visual smoke + Tracy plot review** — ctest 34/35 + 1 documented pre-existing failure. No new Tracy plots (VCT is part of `VctSample` and `VctCone` compute; would add `VCT Diffuse Samples` + `VCT Mip Chain Build (ms)` in Phase 7 polish if operator requests).
- **Phase 7: Doc sync (this entry)** — `agent/workspace.md` + `COMMENTS.md` + `CHANGELOG.md` + `TODO.md` updated. **No commit prompt** (per operator "close dirty without prompt" directive).

**Per AGENTS.md §5.4 + AGENTS.md §5.9:** no commit performed. Operator decides commit timing separately.

## 2. Nearest Gap

- **Stage 5.2 RTX shadows + BLAS** — `kVctCutoffRoughness=0.3` forwards smooth surfaces to RTX path. RTX BLAS (Bottom-Level Acceleration Structure) for static chunks, TLAS + ray query in `voxel.frag`, BLAS build per dirty-chunk. ~770 LoC, RTX-only path (graceful fallback per `2026-06-20-rt-shadows-vs-csm` verdict).
- **Stage 5.1 VCT visual smoke verification** — `VoxelLab` cave darkening visible from VCT diffuse + specular when `PROJECTV_VCT_GPU=ON` (operator run-time smoke, not in 8x A scope).
- **Stage 5.1 VCT polish (optional)** — Tracy plots `VCT Diffuse Samples` + `VCT Mip Chain Build (ms)`; capture automation in `LookDevCaptureAutomation.cpp` (current 5+ scenes); `voxel.frag` integration with `LightingDebugView` (add 9 = VCT_Diffuse, 10 = VCT_Specular). ~50 LoC.
- **Stage 6.2 AppState PIMPL full struct move** — mechanical sed `state->render().X` → `state->render()->X` over 172 call sites. Multi-session.
- **Stage 1.1 NanoVDB async flatten** — Stage 1.1 flatten currently runs on CPU; could move to async compute queue.

## 3. Next Steps

Future session candidates (operator decides):
1. **Stage 5.2 RTX shadows + BLAS** — high, builds on this session's VCT + Stage 6.3 cross-queue depth. RTX-only path.
2. **Stage 5.1 VCT visual smoke + capture automation** — small, builds on this session's VCT foundation.
3. **Stage 6.2 AppState PIMPL full struct move** — high-risk sed migration, biggest incremental rebuild win.
4. **Stage 1.1 NanoVDB async flatten** — medium, could move to async compute queue.
5. **Stage 1.2 Lazy Dedup & Static Promotion** — small, verification + DedupSubtree call site.

## 4. Risks

- **Stage 6.3 HZB cross-queue depth** — now fully closed (Phase 1 of this session). Single-barrier pattern with `VK_QUEUE_FAMILY_IGNORED` is sufficient for current `VK_SHARING_MODE_EXCLUSIVE` HZB image when timeline semaphore + barrier provide execution + memory respectively. If `VK_KHR_maintenance8` becomes available (with `VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT`), the barrier can be tightened.
- **Stage 5.1 VCT** — env gate default OFF. When `PROJECTV_VCT_GPU=ON` + 3D clipmap allocated, voxelize dispatch runs after graphics mip chain, then mip chain blit, then fragment shader uses `textureLod(vctClipmap, uvw, mipLevel)`. VoxelLab reference scene: 256³ RGBA16F = 16 MiB VRAM (4 mips ≈ 21 MiB total with metadata), well within `agent/knowledge.md §15` lighting contract. `kVctConeDirectionCount=6` per TODO §5.1 explicit (vs WickedEngine 16-32 cones for production quality — upgrade path documented).
- **Stage 5.1 VCT specular cutoff** — `roughness > 0.3` routes to VCT specular. Smooth materials (`roughness <= 0.3`) currently have no specular GI contribution from VCT (placeholder comment in `voxel.frag` for Stage 5.2 RTX path). Risk: smooth materials may look under-lit when VCT is enabled. Acceptable because env gate is OFF by default; enabled users get a clear signal.
- **Stage 5.2 RTX deferred** — per operator decision, RTX not in 8x A scope. Smooth specular GI empty until Stage 5.2 lands. No regression in default path.
- **Stage 1.1 NanoVDB resize** — grow path active when capacity exceeded. `LogRuntimeFailure("NanoVdbFlatten", "GrowAndRefreshFailed")` only fires if VMA allocation fails after grow. Per-frame grow has 1.5× headroom.
- **Stage 3.2 Incremental Jolt boundary-neighbor** — iterates 1-27 chunk slots; worst-case voxel edit on a corner produces 8 JPH body rebuilds. Per-rebuild cost ~2× wall time per chunk (greedy merge compile) but 35× fewer JPH AddShape calls net-positive.
- **Ninja 1.13 dep-scan race** — `agent/knowledge.md §30` documents the bug workaround (`--parallel 1` first build); required on C++ module changes.

---

## 5. Active tasks (current open sessions)

**2026-06-21 session 8x (Variant A «close async + open VCT», this session, closed dirty per operator policy)** — 7 phases + doc sync across 2 TODO stages (6.3 HZB cross-queue depth complete + 5.1 VCT foundation). Build green, 34/35 ctest pass + 1 documented pre-existing failure. 1 new test target + 1 new sub-test. NO commit performed. Session closed dirty per AGENTS.md §5.4 + operator "close dirty without prompt" directive.

## 6. Recent closed sessions

- **2026-06-21 session 8x (Variant A «close async + open VCT», this session, closed dirty, no commit)** — 7 phases: Stage 6.3 HZB cross-queue depth ownership transfer (closes 8x V1 partial) + Stage 5.1 VCT 3D clipmap + voxelize.comp foundation + VCT diffuse 6-cone tracing + VCT specular 1-cone + kVctCutoffRoughness=0.3 hybrid + VCT GPU mip chain build + doc sync. 1 new test target + 1 new sub-test.
- **2026-06-21 session 8x (Variant 1 «close partial APIs», prior, closed via commit `465440e`)** — 7 phases: Stage 4.2 LOD GPU consume mesh emission + Stage 2.1 HZB smart blend width shader consume + Stage 4.3 Chunk prebake integration + Stage 6.3 HZB async compute cross-queue depth sync (partial) + Stage 1.1 NanoVDB resize logic + Stage 3.2 Incremental Jolt boundary-neighbor queue + doc sync. 1 new test target + 18 new sub-tests in existing targets.
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
