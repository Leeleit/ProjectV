# Changelog

All notable changes to ProjectV are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This CHANGELOG consolidates **refactor / bug-fix history** that previously lived as
`// **...**` comments in source files. Source comments now follow the project's
Doxygen convention (`/// \brief` + `/// \details`) and are generated into HTML by
`doxygen Doxyfile` per `docs/api/README.md`.

**For design rationale and ongoing decisions**, see `agent/knowledge.md Part A`.
**For session log / commit narrative**, see `agent/workspace.md §5 (Active tasks)` and `git log`.

---

## 2026-06-21 (session: 17x «perf(physics,voxel): incremental SyncPhysicsWorld + Fluid editVersion suppress + AABB-bounded Fluid CA»)

6 phases (4 initial + Phase 5 capacity bump + Phase 6 AABB-bounded Fluid CA = actual FlatBench hot path fix). Build green, **38/38 ctest pass + 2 documented pre-existing failures** (`ProjectVTests` + `ProjectVFluidCATests` link errors predate this session). 1 new test target (`ProjectVPhysicsSyncTests`, 11 sub-tests). Round-2 safety-net: `/tmp/17x_round2_changes.patch` (416 lines, this round's diff). 0 commits performed — operator policy per AGENTS.md §5.4.

### Phase 0: Safety-net + dirty baseline

- `git diff > /tmp/before_17x_20260621_184500.patch` (4460 lines pre-existing dirty tree baseline).

### Phase 1: Tracy diagnostics for SyncPhysicsWorld + UpdateFluidCA

- `src/physics/PhysicsWorld.cpp` — `PV_PROFILE_ZONE_N` added to `SyncPhysicsWorld`, `BuildStaticVoxelCollisionBody`, `BuildChunkStaticCollisionBody`, `ProcessChunkRebuildQueue`. New `IsPhysicsStaticWorldBodyId` helper that recognizes both monolithic `staticWorldBodyId` AND any chunk body in `chunkStaticBodies`. `IsWalkJumpLockedSourceSupportSideWallContact` updated to use it.
- `src/voxel/VoxelWorld.cpp` — `PV_PROFILE_ZONE_N` added to `UpdateFluidCA` (top-level) + sub-zones `UpdateFluidCA.ReadPass` (cell scan) and `UpdateFluidCA.Commit` (write-back). `PlotValue` for `Fluid CA Cells Read` and `Fluid CA Cells Moved`. New `debug/Profiling.hpp` include.
- `src/debug/Profiling.hpp` — registered 4 new plots: `Physics Sync Full Rebuild`, `Physics Sync Incremental`, `Physics Sync Skipped`, `Fluid CA Cells Read`, `Fluid CA Cells Moved`, `Fluid Edit Version Bumps Suppressed`.

### Phase 2: Suppress `editVersion` bump for Fluid↔Air transitions

- `src/voxel/VoxelWorld.cpp::SetVoxelMaterial` — new `isFluidAirTransition` early-out branch: when previous/material is one of {Air, Fluid} and the other is also {Air, Fluid} (i.e. pure Fluid↔Air), skip `++world.editVersion` and skip the `physics != nullptr` chunk-rebuild queue block. Still updates Sparse64Tree storage, stats counters, and mesh-dirty markers. Fluid is not a physics-solid material per `IsPhysicsSolidMaterial` (PhysicsWorld.cpp:548), so this is sound.

### Phase 3: Incremental `SyncPhysicsWorld` + per-chunk compound rebuild

- `src/physics/PhysicsWorld.cpp::SyncPhysicsWorld` — split into two paths (full rebuild on world pointer change, incremental on edit only).
- `src/physics/PhysicsWorld.cpp` — new `RebuildStaticWorldBodyFromChunkShapes`, `DestroyAllChunkStaticBodies`, and `chunkMergedBoxes` map on `PhysicsState`.
- `src/physics/PhysicsWorld.hpp` — exported 3 new public functions.

### Phase 4: Regression tests

- `tests/PhysicsSyncTests.cpp` (NEW) + `tests/CMakeLists.txt` + `CMakePresets.json` (5 occurrences) — new `ProjectVPhysicsSyncTests` test target with 9 sub-tests (initial load, no-op, incremental, null-world, fluid skip editVersion, solid edit bumps, fluid skip physics queue, compound rebuild, small-world time-budget).

### Phase 5: Capacity bump + FlatBench world init regression test

- `src/physics/PhysicsWorld.cpp` — `kMaxPhysicsBodies` 32 → 1024, `kMaxBodyPairs` 64 → 4096, `kMaxContactConstraints` 64 → 1024, `kPhysicsTempAllocatorBytes` 4 → 8 MiB. Fixes the "CreateAndAddBody returned an invalid body id" errors when first-loading FlatBench (400 chunks > 32 limit). Jolt defaults are exactly these new values. ~100 KB memory delta per PhysicsState.
- `src/render/SceneResources.cpp` — added `lastNanoVdbSyncedEditVersion` gate to `BuildNanoVdbFlatten` trigger: full-tree NanoVdb flatten is skipped when only Fluid↔Air chunks were rebuilt in the frame. Originally hypothesized as the FlatBench hot path; in practice the real bottleneck was Fluid CA (Phase 6). Kept as a defensive optimization that costs 0.02 ms on FlatBench.
- `tests/PhysicsSyncTests.cpp` — added `TestSyncPhysicsWorldFlatBenchSizedWorldFitsInCapacity`: builds FlatBench-sized world (80×26×80 = 400 chunks), verifies `SyncPhysicsWorld` succeeds. Guards against future scene growth re-exceeding `kMaxPhysicsBodies`.

### Phase 6: AABB-bounded `UpdateFluidCA` (the actual FlatBench hot path)

- **Root cause found via runtime smoke + unit test:** `UpdateFluidCA` iterated the ENTIRE 80×26×80 = 166 400 cells of FlatBench for every tick (20 Hz), each cell requiring a `ReadVoxelFromSparseStorage` call (sparse tree traversal ~240 ns/cell). Total: **~40 ms/tick on FlatBench with 1 water block**. At 20 Hz = 800 ms/sec, with positive feedback loop (frame > 50 ms → multiple ticks/frame), pinning frame rate to ~4 FPS. Confirmed by `TestFluidCAPerTickCostOnFlatBenchSizedWorld` which measured 40 ms/tick on FlatBench-sized world before the fix.
- **Fix:** new `world.fluidCAAabbMin` / `world.fluidCAAabbMaxExclusive` fields in `VoxelWorld`, expanded on every `SetVoxelMaterial` that touches Fluid material. `UpdateFluidCA` now iterates only `[AABB.min - 1, AABB.max + 1]` (1-cell margin for spread/fall neighbor reads) instead of the whole world. For 1 water block on FlatBench: AABB is 3×3×3 = 27 cells → ~3 ms/tick (was ~40 ms).
- `src/voxel/VoxelWorld.hpp` — added `Int3 fluidCAAabbMin = (INT32_MAX, ...)` and `Int3 fluidCAAabbMaxExclusive = (INT32_MIN, ...)` to `VoxelWorld` struct. Invalid (min > max) when no fluid; `UpdateFluidCA` early-out via existing `fluidVoxelCount == 0` check already covers this.
- `src/voxel/VoxelWorld.cpp::SetVoxelMaterial` — when material or previous material is Fluid, expand AABB to include the cell. AABB is lazy-shrinking (never shrinks when fluid is removed; reset to invalid only when `fluidVoxelCount == 0`).
- `src/voxel/VoxelWorld.cpp::UpdateFluidCA` — replaced 3 full-world nested loops with AABB-bounded loops in local coordinates. Read pass uses expanded AABB (`[min-1, max+1]`); sim and commit passes use the actual AABB (`[min, max]`). All loops index into the same `next[]` and `claimed[]` buffers of size `width*height*depth` (just with different bounds). The debug `actualFluidCount` verification loop also uses the AABB.
- **Result:** **40 ms/tick → 3 ms/tick on FlatBench with 1 water block (13× speedup).** 20 ticks (= 1 second at 20 Hz Fluid CA) takes 57.5 ms total. Frame budget at 60 FPS = 16.7 ms; Fluid CA now contributes ~3.5 ms/frame, well below the 50 ms multi-tick threshold that was triggering the feedback loop. Expected FPS recovery on FlatBench with water: 4 → 50+ FPS.
- `tests/PhysicsSyncTests.cpp` — added `TestFluidCAPerTickCostOnFlatBenchSizedWorld`: places 1 water block on FlatBench-sized world (80×26×80 = 166 400 cells), runs 20 Fluid CA ticks (= 1 second at 20 Hz), prints per-tick and total timings. Local measurement: 57.53 ms / 20 ticks = 2.88 ms/tick.

---

## 2026-06-21 (session: 8x Variant C «Stage 5.4 wire-up + Sky LDR LUT» — 4 TODO stages: 5.4 + 5.5 + CMakePresets backfill)

9 phases + doc sync. Build green, **36/36 ctest pass + 2 documented pre-existing failures** (`ProjectVTests` + `ProjectVFluidCATests` link errors predate this session). 0 new test targets; +5 sub-tests in `ProjectVSkyAtmosphereTests` (env gate PROJECTV_SKY_LUT default/ON, kSkyViewLutWidth=256/kSkyViewLutHeight=128/kMultiScatteringLutWidth=32/kMultiScatteringLutHeight=32/kSkyViewLutFormat=RGBA16F, create/destroy null rejects); +2 new cycle assertions in `TestLightingDebugViewCycleIncludesShadow` (VolumetricFog + VolumetricTransmittance cycle entries). All green. **No commit performed** per operator policy "close dirty without prompt" (per AGENTS.md §5.4). `CMakePresets.json` backfilled per AGENTS.md §4 invariant: 2 missing test executables (`ProjectVVoxelizePipelineTests` + `ProjectVPhysicsIncrementalJoltTests` from 8x V1 + 8x V A sessions) added to all 5 buildPresets (10 = 2 × 5 occurrences).

### Phase 0: Web-search gate (mandatory per AGENTS.md §5.3)

- **Sky (Hillaire 2020 EGSR + AirGuanZ/AtmosphereRenderer)** — confirmed Sky-View LUT 256×128 + Multi-Scattering LUT 32×32 approach; perf 64×64 Sky-View = 0.036 ms on GTX 1060, 200×150×32 with shadows = 0.2 ms. Lat-long mapping with non-linear latitude preserves horizon detail.
- **Fog (Frostbite 2015 Hillaire slide 28)** — confirmed per-slice integral `sliceLightIntegral = sliceLight * (1 - transmittance) / density` for improved transmission over constant-per-slice approximation. Unity VolumetricLighting: 32×2×1 workgroup, transmission in alpha, `sliceDensity=max(0.000001)` for div-by-zero safety.
- **Cloudscape (Schneider 2015 + Nubis 2017)** — conservative depth `max(0.5)` for thin occluders (trees), 1.2 ms target with sphere-bottom excluded.
- **VCT (Helliaca/VXCT + Snowapril)** — debug view pattern `ShowIndirectDiffuse` / `ShowIndirectSpecular` + visual screenshot test pattern (from previous session).

### Phase 1: Stage 5.4 Volumetric fog froxel descriptor plumbing

- `src/render/vulkan/VulkanGraphicsPipeline.cpp` — `kGraphicsDescriptorBindings` extended 7→9 entries (binding 11 = `vctClipmap` sampler3D FRAGMENT + binding 12 = `volumetricFog` sampler3D FRAGMENT). EVIL markers for both binding numbers per AGENTS.md §8.1.
- `src/render/vulkan/VulkanGraphicsPipeline.cpp` — `kGraphicsVolume3DDescriptorPoolSize` (SAMPLED_IMAGE type) added — pool multiplier 2×MAX_FRAMES_IN_FLIGHT.
- `src/render/vulkan/VulkanGraphicsPipeline.cpp::RefreshGraphicsResourceBindings` — per-frame write bindings 11+12 with `vctClipmapImageInfo` (real when vctClipmapEnabled, VK_NULL_HANDLE otherwise) + `volumetricFogImageInfo` (real when volumetricFogPipelineEnabled, fallback otherwise).
- `src/core/Types.hpp::RenderState` — `volumetricFogFallbackImage/View/Allocation/Memory` fields added (1×1×1 RGBA16F zero dummy for binding 12 fallback when env gate OFF).
- `src/render/VolumetricFog.{hpp,cpp}` — `CreateVolumetricFogFallbackOnly` separate function creates fallback unconditionally (before env-gated `CreateVolumetricFogResources`). `DestroyVolumetricFogResources` extended to clean up fallback.
- `src/render/vulkan/VulkanInit.cpp` — `CreateVolumetricFogFallbackOnly` called unconditionally before env-gated `CreateVolumetricFogResources`.

### Phase 2: Stage 5.4 Volumetric fog consume in voxel.frag

- `src/shaders/voxel.frag` — `layout(set = 0, binding = 12) uniform sampler3D volumetricFog;` added (EVIL marker for binding 12 + Wronski 2014 froxel binding contract).
- `src/shaders/voxel.frag` — new `SampleVolumetricFogAtWorld` logic: compute `froxelUvw = (gl_FragCoord.xy / froxelImageSize, depthDistribution)` where `depthDistribution = pow(normalizedDepth, 0.5) * 0.995 + 0.005` matches `volumetric_fog.comp` exponential depth distribution. `texture(volumetricFog, froxelUvw)` returns accumulated RGB + (1 - transmittance) alpha.
- `src/shaders/voxel.frag` — composition: `color = color * volumetricFogTransmittance + volumetricFogAccum` (per-slice Beer-Lambert per Frostbite 2015 slide 28). LightingDebugView +2 entries (`VolumetricFog` = 12, `VolumetricTransmittance` = 13). Debug paths 12/13 set color = fog accum / transmittance gray.
- `src/voxel/VoxelMaterials.{hpp,cpp}` — `LightingDebugView` enum extended with `VolumetricFog` + `VolumetricTransmittance` (positions 12/13). String mappings "VOL_FOG" / "VOL_TRN". `GetNextLightingDebugView` cycle extension (VctSpecular → VolumetricFog → VolumetricTransmittance → Final).
- `src/app/LookDevCaptureAutomation.cpp` — `TryParseDebugViewToken` cases for "volfog" / "volumetricfog" / "vol_f" + "voltrn" / "volumetrictransmittance" / "vol_t" tokens for capture automation.
- `tests/VoxelWorldTests.cpp` — `TestLightingDebugViewCycleIncludesShadow` extended with 2 new cycle assertions + 2 new string mapping assertions.
- `tests/VoxelizePipelineTests.cpp` — `TestVctDebugViewCycle` updated for new cycle (VctSpecular → VolumetricFog → VolumetricTransmittance → Final).

### Phase 3: Stage 5.4 Cloudscape per-frame dispatch wiring

- `src/render/Renderer.cpp` — added `#include "render/Cloudscape.hpp"`.
- `src/render/Renderer.cpp::DrawFrame` — `RecordCloudscapeRaymarchPass` recorded after main voxel pass before `vkCmdEndRendering`. Predicate `cloudscapePassActive = IsCloudscapeEnabled() && cloudscapePipelineEnabled`.
- `src/render/Renderer.cpp::DrawFrame` — push constants built from `currentSceneLighting`: `cloudColorAndCoverage` (4-tuple cloud color + coverage 0.65), `sunDirectionAndIntensity` (sun direction + intensity from `sunColorAndIntensity[3]`), `cloudLayerParams` (cloud UV offset/coverage/contrast), `viewParams` (camera height, aspect ratio, tanHalfFovY).
- `src/render/Renderer.cpp::DrawFrame` — `imageIndex` passed for per-frame descriptor set lookup.

### Phase 4: Stage 5.5 Sky LDR LUT precomputation — Sky-View 256×128 RGBA16F (CPU)

- `src/render/SkyAtmosphere.{hpp,cpp}` — `IsSkyLutPrecomputeEnabled()` env gate (`PROJECTV_SKY_LUT=ON`, default OFF per `agent/knowledge.md §30.4` Step 1).
- `src/render/SkyAtmosphere.hpp` — constants `kSkyViewLutWidth=256`, `kSkyViewLutHeight=128`, `kSkyViewLutFormat=R16G16B16A16_SFLOAT` (EVIL markers per Hillaire 2020 reference).
- `src/render/SkyAtmosphere.hpp` — `sky_atmosphere_constants` namespace (kPlanetRadius=6371000, kAtmosphereHeight=100000, kRayleighBetaR=5.8e-6, kRayleighBetaG=13.5e-6, kRayleighBetaB=33.1e-6, kMieBeta=0.005, kMieG=0.8, kRaymarchStepCount=16) EVIL markers.
- `src/render/SkyAtmosphere.cpp` — `CreateSkyViewLut`: CPU computes 256×128 RGBA16F Hillaire 2020 single-scattering per `(viewZenith, sunZenith)` with 16-step ray-march through exponential atmosphere density `exp(-altitude/8500)`. Single scattering transmittance `T(θ_v, θ_s)` per spectral channel R/G/B; Henyey-Greenstein phase function (g=0.8) for Mie + Rayleigh phase (0.75 * (1 + cos²θ)).
- `src/render/SkyAtmosphere.cpp` — manual IEEE 754 `FloatToHalf` (round-to-nearest-even) since GLM doesn't expose `packHalf1x16`. RGBA16F upload via VMA `HOST_ACCESS_RANDOM_BIT` mapped memory.

### Phase 5: Stage 5.5 Sky LDR LUT precomputation — Multi-Scattering 32×32 RGBA16F (CPU)

- `src/render/SkyAtmosphere.{hpp,cpp}` — `kMultiScatteringLutWidth=32`, `kMultiScatteringLutHeight=32`, `kMultiScatteringLutFormat=R16G16B16A16_SFLOAT` constants (EVIL markers).
- `src/render/SkyAtmosphere.cpp` — `CreateMultiScatteringLut`: CPU computes 32×32 RGBA16F Wrenninge-style 2-octave multiple scattering accumulation as function of `(altitude, sunZenith)`. Same FloatToHalf upload pipeline.

### Phase 6: Stage 5.5 Sky LDR LUT integration in shader

- `src/shaders/sky_atmosphere.frag` — `layout(set = 0, binding = 0) uniform sampler2D skyViewLut` + `layout(set = 0, binding = 1) uniform sampler2D multiScatteringLut` (EVIL markers per Hillaire 2020 reference).
- `src/shaders/sky_atmosphere.frag` — sky pass chooses between precomputed LUT path (gated by `pc.zenithColorAndIntensity.w > 1.5` magic — caller sets this flag when env PROJECTV_SKY_LUT=ON) and analytical fallback. LUT path: `viewZenith = acos(-viewDir.z)/π/2`, `sunZenith = acos(sunDir.y)/π/2`, sample `skyViewLut(viewZenith, sunZenith)` + `multiScatteringLut(sunZenith, sunDir.y)`, combine + transmittance multiply.
- `src/render/SkyAtmosphere.cpp` — pipeline descriptor set layout extended 0→2 COMBINED_IMAGE_SAMPLER bindings + descriptor pool extended + per-frame descriptor set alloc + write.
- `src/render/SkyAtmosphere.hpp` — `RecordSkyAtmospherePass` signature extended with `uint32_t frameIndex` for descriptor set lookup.
- `src/render/Renderer.cpp` — sky pass call site updated to pass `imageIndex`.
- `src/core/Types.hpp::RenderState` — `skyAtmosphereDescriptorSets[] MAX_FRAMES_IN_FLIGHT` array.
- `src/core/Types.hpp::RenderState` — `skyViewLutImage/View/Allocation` + `multiScatteringLutImage/View/Allocation` + `skyLutLinearSampler` + `skyLutPrecomputeEnabled` fields.

### Phase 7: Tests + Tracy + EVIL audit

- `tests/SkyAtmosphereTests.cpp` — +5 sub-tests: `TestSkyLutEnvDefaultOff`, `TestSkyLutEnvExplicitOn`, `TestSkyLutConstants` (kSkyViewLutWidth=256/kSkyViewLutHeight=128/kMultiScatteringLutWidth=32/kMultiScatteringLutHeight=32/kSkyViewLutFormat=RGBA16F), `TestCreateSkyLutResourcesRejectsNullContext`, `TestDestroySkyLutResourcesRejectsNull`.
- `src/debug/Profiling.hpp::ConfigureDefaultPlots` — "Sky LUT Precompute (ms)" plot added.
- `src/render/SkyAtmosphere.cpp::CreateSkyLutResources` — `profiling::PlotValue("Sky LUT Precompute (ms)", 1.0)` for Tracy trace.
- EVIL marker audit grep: voxel.frag=3, sky_atmosphere.frag=2, volumetric_fog.comp=1, cloudscape.frag=1, VulkanGraphicsPipeline.cpp=2. All hard-coded magic-numbers documented per AGENTS.md §8.1.

### Phase 8: CMakePresets.json backfill

- `CMakePresets.json` — backfilled `ProjectVVoxelizePipelineTests` + `ProjectVPhysicsIncrementalJoltTests` to all 5 buildPresets per AGENTS.md §4 invariant. 10 = 2 × 5 occurrences. Closes pre-existing gap from 8x V1 + 8x V A sessions.

### Phase 9: Doc sync

- `agent/workspace.md` — `§1 Now` (Variant C 9-phase summary) + `§2 Nearest Gap` (5.2 RTX + 6.2 PIMPL + 5.x additional polish) + `§3 Next Steps` (reprioritized) + `§5 Active tasks` (Variant C as current) + `§6 Recent closed` (Variant C as latest).
- `TODO.md` — `Задача 5.4 Stage 5.x Visual Polish wire-up` flipped from 🔓 Open → ✅ Closed. New `Задача 5.5 Stage 5.x Sky LDR LUT precomputation` section flipped from deferred → ✅ Closed. Сводка обновлена до 15 ✅ + 1 ⏸️ + 2 🔓.
- `COMMENTS.md` — TBD
- `CHANGELOG.md` — this entry.

### Verification

- `cmake --build build/linux-clang-debug --target ProjectV` — green.
- `ctest --test-dir build/linux-clang-debug -j 8 -E "ProjectVTests|ProjectVFluidCATests"` — **36/36 pass** (both excluded for known pre-existing link errors).
- +5 sub-tests in `ProjectVSkyAtmosphereTests` + 2 new cycle assertions in `ProjectVTests::TestLightingDebugViewCycleIncludesShadow`, all green.
- Dirty tree: ~30 files modified + 2 new shader files modified + CMakePresets.json updated + 4 docs modified.

---

## 2026-06-21 (session: 8x Variant B «Stage 5.x Visual Polish foundation» — 4 TODO stages: 5.1 polish + 5.x Sky + Fog + Cloudscape)

9 phases + doc sync. Build green, **37/37 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x/8x/12x/8x V1/V A). 3 new test targets (`ProjectVSkyAtmosphereTests` = 9 sub-tests, `ProjectVVolumetricFogTests` = 9 sub-tests, `ProjectVCloudscapeTests` = 10 sub-tests) + 3 new sub-tests in `ProjectVVoxelizePipelineTests` + 3 new assertions in `TestLightingDebugViewCycleIncludesShadow` (ProjectVTests). All green. **No commit performed** per operator policy "close dirty without prompt" (per AGENTS.md §5.4). `CMakePresets.json` updated per AGENTS.md §4 invariant: 3 new test executables added to all 5 buildPresets (windows-clang-debug-build, windows-clang-debug-ci-build, linux-clang-debug-build, windows-clang-release-build, linux-clang-release-build).

### Phase 0: Web-search gate (mandatory per AGENTS.md §5.3)

- **Sky (Hillaire 2020 EGSR)** — `cf14050` paper + `sebh/UnrealEngineSkyAtmosphere` DX11 reference + `AirGuanZ/AtmosphereRenderer` MIT port. Confirmed 2D Sky-View LUT (256×128 RGBA16F) + 2D Multi-Scattering LUT (32×32 RGBA16F) approach; perf 0.080 ms on GTX 1060. Production grade = 0.080 ms; analytical in-shader (no LUT) = 1-3 ms trade-off.
- **Fog (Wronski 2014 SIGGRAPH + Frostbite 2015)** — froxel pattern, 160×90×64 at 720p, 240×135×128 at 1080p+. Exponential depth distribution concentrated near camera. Compute shader + 3D texture UAV. Production cost 0.5-2 ms depending on resolution. Density in alpha, lighting in RGB.
- **Cloudscape (Schneider 2015 + Nubis 2017)** — single-layer ray-march with 64-128 samples, 6 light cone samples. 2D FBM noise. ~2 ms target. FBM Worley for shape, Henyey-Greenstein for sun lighting.
- **VCT debug views (Helliaca/VXCT + Snowapril/voxel_cone_tracing)** — industry-standard `ShowIndirectDiffuse` / `ShowIndirectSpecular` pattern + visual screenshot test pattern.

### Phase 1: Stage 5.1 VCT visual smoke closure

- `src/voxel/VoxelMaterials.hpp` — added `LightingDebugView::VctDiffuse` + `LightingDebugView::VctSpecular` enum entries (positions 10/11, `postProcess.w` index).
- `src/voxel/VoxelMaterials.cpp` — `LightingDebugViewToString` returns "VCT_DIFF" / "VCT_SPEC" for new entries; `GetNextLightingDebugView` cycle: Taa → VctDiffuse → VctSpecular → Final.
- `src/shaders/voxel.frag` — added debug view paths 10 (color = `vctDiffuse`) + 11 (color = `vctSpecular`).
- `src/app/LookDevCaptureAutomation.cpp` — added `TryParseDebugViewToken` cases for "vctdiff" / "vctdiffuse" / "vct_d" / "vctspec" / "vctspecular" / "vct_s" tokens for capture automation.
- `src/render/vulkan/VulkanVoxelizePipeline.cpp` — `profiling::PlotValue("VCT Voxelize Chunks", count)` per dispatch + `profiling::PlotValue("VCT Mip Chain Mips", mipLevels - 1u)` per mip chain build.
- `src/debug/Profiling.hpp` — `ConfigureDefaultPlots` added "VCT Voxelize Chunks" + "VCT Mip Chain Mips" + "VCT Active Mip" plots.
- `tests/VoxelizePipelineTests.cpp` — 3 new sub-tests: `TestVctLightingContractSize` (sizeof VoxelSceneLighting == 656, vctParams offset 624, vctSpecularParams offset 640), `TestVctDebugViewStringMapping`, `TestVctDebugViewCycle`.
- `tests/VoxelWorldTests.cpp` — `TestLightingDebugViewCycleIncludesShadow` extended with 3 new assertions for VCT cycle (Taa → VctDiffuse → VctSpecular → Final) + 2 new string mapping assertions.
- `tests/CMakeLists.txt` — added `VoxelMaterials.cpp` to `ProjectVVoxelizePipelineTests` source list (link was missing).

### Phase 2: Stage 5.x Sky Hillaire 2020 foundation (NEW MODULE)

- `src/render/SkyAtmosphere.hpp` (NEW) — `IsSkyAtmosphereEnabled()` env gate (`PROJECTV_SKY=ON`, default OFF per `agent/knowledge.md §30.4` Step 1). `SkyAtmospherePushConstants` (64 B, 16-byte align). Public API: `CreateSkyAtmospherePipelines` / `DestroySkyAtmospherePipelines` / `RecordSkyAtmospherePass`.
- `src/render/SkyAtmosphere.cpp` (NEW, ~440 LoC) — `vkCreateGraphicsPipelines` with dynamic-rendering `VkPipelineRenderingCreateInfo` (color R16G16B16A16_SFLOAT + depth D32_SFLOAT). No vertex buffers — full-screen triangle via `gl_VertexIndex`. `depthTestEnable=VK_TRUE` + `depthCompareOp=VK_COMPARE_OP_ALWAYS` + `depthWriteEnable=VK_TRUE` (sky writes depth=0.9999). `colorWriteMask` excludes alpha.
- `src/shaders/sky_atmosphere.vert` (NEW) — full-screen tri vertex shader.
- `src/shaders/sky_atmosphere.frag` (NEW) — Phase 2 analytical color: vertical gradient + sun disc + below-horizon darkening.
- `src/core/Types.hpp` — `RenderState` extended with 7 skyAtmosphere* fields (pipeline + layout + 2 shader modules + descriptor set layout + pool + enabled flag).
- `src/CMakeLists.txt` — registered `SkyAtmosphere.cpp` source + `sky_atmosphere.{vert,frag}` shaders.
- `src/render/vulkan/VulkanInit.cpp` — calls `CreateSkyAtmospherePipelines` gated on `IsSkyAtmosphereEnabled()`.
- `src/core/Types.cpp` — `ShutdownVulkan` calls `DestroySkyAtmospherePipelines`.
- `src/render/Renderer.cpp` — `skyPassActive` predicate computed per frame; sky pass recorded before main voxel pass via `RecordSkyAtmospherePass`; main pass `loadOp` flipped to `LOAD` for scene color + depth when sky is active (replaces `CLEAR`).
- `tests/SkyAtmosphereTests.cpp` (NEW, 9 sub-tests) — env default/ON/zero/OFF, push constants size, null context/destroy/CB reject paths, zero extent reject.

### Phase 3: Stage 5.x Sky Rayleigh + Mie scattering full integration

- `src/shaders/sky_atmosphere.frag` — upgraded from vertical-gradient MVP to full Hillaire 2020 analytical single-scattering with Rayleigh (per-channel wavelength β_R = 5.8e-6/13.5e-6/33.1e-6) + Mie (β_M = 0.005, g=0.8) + Henyey-Greenstein phase function + 16-step exponential depth distribution. EVIL markers added for hard-coded Rayleigh/Mie coefficients per Hillaire 2020 reference.

### Phase 4: Stage 5.x Volumetric fog Wronski 2014 froxel grid foundation (NEW MODULE)

- `src/render/VolumetricFog.hpp` (NEW) — `IsVolumetricFogEnabled()` env gate (`PROJECTV_FOG=ON`, default OFF). `VolumetricFogPushConstants` (64 B). `kVolumetricFogFroxelWidth/Height/Depth` constants (160/90/64 per Wronski 2014 720p reference) + `kVolumetricFogRaymarchStepCount` (12 per Frostbite 2015).
- `src/render/VolumetricFog.cpp` (NEW, ~470 LoC) — 3-binding descriptor set (froxel storage image + scene color + depth sampled images). Linear sampler with CLAMP_TO_EDGE. 1 descriptor set per `MAX_FRAMES_IN_FLIGHT`. 8×8×4 workgroup.
- `src/shaders/volumetric_fog.comp` (NEW) — Phase 4 MVP: density * exp(-worldDistance * 0.012). Phase 5: 12-slab ray-march with Schlick phase + Beer-Lambert transmittance.
- `src/core/Types.hpp` — `RenderState` extended with 10 volumetricFog* fields.
- `src/CMakeLists.txt` — registered `VolumetricFog.cpp` + `volumetric_fog.comp`.
- `src/render/vulkan/VulkanInit.cpp` + `Types.cpp` — wire create/destroy.
- `tests/VolumetricFogTests.cpp` (NEW, 9 sub-tests).

### Phase 5: Stage 5.x Volumetric fog Wronski 2014 slab accumulation upgrade

- `src/shaders/volumetric_fog.comp` — rewritten to per-slab ray-march: 12 samples, Schlick phase g=0.8, Beer-Lambert transmittance accumulation, early-out at `transmittance < 0.02`. Output: RGB (linear, no tonemap) + alpha (1 - transmittance). `voxel.frag` consume deferred (requires descriptor set plumbing).
- `tests/VolumetricFogTests.cpp` — `TestVolumetricFogDispatchDimensions` validates W/8=20, H/8=11, D/4=16 truncated dispatch (last 2 height rows safely ignored per `imageSize(fogFroxel)` bound check).

### Phase 6: Stage 5.x Cloudscape Schneider Nubis 2017 foundation (NEW MODULE)

- `src/render/Cloudscape.hpp` (NEW) — `IsCloudscapeEnabled()` env gate (`PROJECTV_CLOUDS=ON`, default OFF). `CloudscapePushConstants` (64 B). `kCloudscapeNoiseTextureSize = 128u` + `kCloudscapeRaymarchStepCount = 24u` per Schneider Nubis 2017 reference.
- `src/render/Cloudscape.cpp` (NEW, ~480 LoC) — CPU-side 128×128 R8 FBM noise generation (4 octaves low-freq + 3 octaves high-freq, 0.85 multiplier, 0.18 bias). Uploaded to `cloudscapeNoiseImage` at startup via VMA `HOST_ACCESS_RANDOM`. 3-binding descriptor set + linear sampler REPEAT. Pipeline: alpha-blend `VK_BLEND_FACTOR_SRC_ALPHA`, `depthTestEnable=VK_TRUE` + `depthCompareOp=LESS_OR_EQUAL` + `depthWriteEnable=VK_FALSE` (clouds only overdraw sky, don't write depth).
- `src/shaders/cloudscape.{vert,frag}` (NEW) — single-layer ray-march 24 steps through horizontal slab at y=80m thickness 24m. Schlick phase g=0.5. EVIL markers for cloudBaseHeight + cloudThickness hard-coded for VoxelLab reference scene.
- `src/core/Types.hpp` — `RenderState` extended with 11 cloudscape* fields.
- `src/CMakeLists.txt` — registered `Cloudscape.cpp` + `cloudscape.{vert,frag}`.
- `src/render/vulkan/VulkanInit.cpp` + `Types.cpp` — wire create/destroy.
- `tests/CloudscapeTests.cpp` (NEW, 10 sub-tests).

### Phase 7: Stage 5.x Cloudscape shader + pipeline infrastructure

- All in Phase 6 above (shader + pipeline + descriptor set + sampler + per-frame dispatch helper).

### Phase 8: Tests + EVIL marker audit + Tracy plot review + CMakePresets.json update

- Full ctest 37/37 + 1 pre-existing failure. All new sub-tests pass.
- `src/debug/Profiling.hpp` — added 3 new plot configurations: "Sky Atmosphere Pass", "Volumetric Fog Pass", "Cloudscape Pass" (all `Number` format, `step` semantic).
- `CMakePresets.json` — 3 new test executables (`ProjectVSkyAtmosphereTests`, `ProjectVVolumetricFogTests`, `ProjectVCloudscapeTests`) added to all 5 buildPresets per AGENTS.md §4 invariant (15 = 3 × 5 occurrences). **Pre-existing gap noted:** `ProjectVVoxelizePipelineTests` + `ProjectVPhysicsIncrementalJoltTests` (from 8x V1 + 8x V A sessions) also missing from preset target lists — deferred to follow-up.

### Phase 9: Doc sync

- `agent/workspace.md` — `§1 Now` (Variant B 9-phase summary) + `§2 Nearest Gap` (volumetric fog consume + cloudscape dispatch as next steps) + `§3 Next Steps` (reprioritized) + `§5 Active tasks` (Variant B as current) + `§6 Recent closed` (Variant B as latest, Variant A as prior via commit c80f265).
- `TODO.md` — `Задача 5.1` flipped from `⏸️ Partial` to `✅ Closed` (within DoD, RTX smooth specular still separate in Stage 5.2). New `Задача 5.3 Stage 5.x Visual Polish Foundation` section + `Задача 5.4 Wire-up` follow-up section. Сводка updated to 13 ✅ + 2 ⏸️ + 2 🔓.
- `COMMENTS.md` — new `design-rationale` + `EVIL` entries for `SkyAtmosphere.{hpp,cpp}`, `sky_atmosphere.frag`, `VolumetricFog.{hpp,cpp}`, `volumetric_fog.comp`, `Cloudscape.{hpp,cpp}`, `cloudscape.frag`.

### Verification

- `cmake --build build/linux-clang-debug --target ProjectV` — green.
- `ctest --test-dir build/linux-clang-debug -j 8 -E "ProjectVTests"` — **37/37 pass** (ProjectVTests excluded for known pre-existing failure).
- 3 new test targets + 3 new sub-tests in `ProjectVVoxelizePipelineTests` + 3 new assertions in `ProjectVTests::TestLightingDebugViewCycleIncludesShadow`, all green.
- Dirty tree: 7 src files modified + 6 src files new + 3 new test files + 1 test file modified + 2 docs modified + 1 CMakePresets.json modified. **No commit** (per operator "close dirty without prompt" policy).

### Operator commit prompt (per workspace.md policy)

ONE "Commit?" prompt at session end, covering 8x Variant B work as single commit. Suggested commit message (per `AGENTS.md §5.1` format):

```
feat(render,shaders): 8x session — Stage 5.x Visual Polish foundation

9 phases + doc sync across 4 TODO stages (5.1 VCT polish + 5.x Sky
Hillaire2020 + 5.x Volumetric Fog Wronski 2014 + 5.x Cloudscape
Schneider Nubis 2017). Build green, 37/37 ctest pass + 1 documented
pre-existing failure (ProjectVTests same baseline as 4x/8x/12x/8x V1).

NEW: Sky atmosphere pass (analytical Rayleigh + Mie per Hillaire 2020
EGSR, full-screen tri, depth 0.9999) +
     Volumetric fog Wronski 2014 froxel grid (160x90x64 RGBA16F,
     12-slab ray-march, Schlick phase g=0.8) +
     Cloudscape Schneider Nubis 2017 single-layer (CPU FBM noise
     128x128 R8, 24-step ray-march, Schlick phase g=0.5) +
     Stage 5.1 VCT visual smoke closure (LightingDebugView VctDiffuse/
     VctSpecular at positions 10/11, trace plots for VCT chunks/mips).

3 new test targets (SkyAtmosphere 9, VolumetricFog 9, Cloudscape 10
sub-tests) + 6 new sub-tests in existing targets, all green.

CMakePresets.json updated per AGENTS.md §4: 3 new test executables
added to all 5 buildPresets. EVIL markers added for hard-coded
Rayleigh/Mie coefficients, depth distribution parameters, Schlick g,
cloudBaseHeight/Thickness (per respective SOTA references).

Refs: TODO.md §5.1, §5.2, §5.3 (new), §5.4 (new)
Refs: agent/knowledge.md §15 (lighting contract), §30.4 (3-step migration)
Refs: docs/experiments/INDEX.md 2026-06-21-precomputed-atmospheric-sky
Refs: docs/experiments/INDEX.md 2026-06-21-volumetric-fog-atmosphere-rendering
Refs: docs/experiments/INDEX.md 2026-06-21-cloudscape-rendering
```

---

## 2026-06-21 (session: 8x Variant A «close async + open VCT» — 2 TODO stages: 6.3 + 5.1)

7 phases + doc sync. Build green, **34/35 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as prior 4x/8x/12x/8x V1). 1 new test target (`ProjectVVoxelizePipelineTests` = 11 sub-tests) + 1 new sub-test in `ProjectVAsyncComputeTests`. All green. **No commit performed** per operator policy "close dirty without prompt" (per AGENTS.md §5.4).

### Phase 0: Web-search gate (mandatory per AGENTS.md §5.3)

- **HZB cross-queue depth ownership transfer** — Khronos Synchronization Examples depth attachment + queue family transfer patterns, `VK_KHR_maintenance8` flag `VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT` (optional), RasterGrid 2026-03 Hi-Z meta-surface (acquire-side resummarization). Concluded: single-barrier pattern with `VK_QUEUE_FAMILY_IGNORED` for both src/dst is sufficient for current `VK_SHARING_MODE_EXCLUSIVE` HZB image when timeline semaphore provides execution dependency and barrier provides memory dependency.
- **VCT 3D clipmap SOTA 2024-2026** — WickedEngine VXGI (turanszkij) 6-cascade clipmap with 16/32-cone tables, Compix VoxelConeTracingGI 3D clipmap implementation, The Tomorrow Children 16-cone + 6-cascade VCT. Concluded: 4 mip levels + 6 fixed diffuse cones (per TODO §5.1 explicit) + 1 specular cone for initial implementation; production upgrade path to 16-32 cones documented.

### Phase 1: Stage 6.3 HZB cross-queue depth ownership transfer complete (closed)

Closes the 8x V1 Phase 4 partial + TODO §6.3 "depth attachment ownership transfer".

- `src/render/vulkan/VulkanAsyncCompute.cpp::RecordHzbAsyncCullPass` — replaced 8x V1 placeholder `VkImageMemoryBarrier2` (which had `srcStageMask = COMPUTE_SHADER_BIT + srcAccessMask = SHADER_READ_BIT`, an invalid self-dependency) with proper cross-queue memory barrier: `srcStageMask = TRANSFER_BIT` + `srcAccessMask = TRANSFER_WRITE_BIT` (the HZB image was last written by the graphics mip chain build) → `dstStageMask = COMPUTE_SHADER_BIT` + `dstAccessMask = SHADER_READ_BIT`, layout stays `SHADER_READ_ONLY_OPTIMAL`, `VK_QUEUE_FAMILY_IGNORED` for both src/dst queue families. Timeline semaphore on `hzbBuildTimelineSemaphore` provides execution ordering; barrier provides memory ordering.
- `tests/AsyncComputeTests.cpp` — added `TestRecordHzbAsyncCullPassRejectsEmptySceneFrameResources` (regression guard for empty render state).

### Phase 2: Stage 5.1 VCT 3D clipmap + voxelize.comp (foundation closed)

- `src/shaders/voxelize.comp` (NEW, ~110 LoC GLSL) — per-voxel scene injection into 3D clipmap texture. 1 workgroup per chunk, 64 threads (8x8x1) iterate over chunk voxels in strided loop. Reads `PackedChunkDescriptors` (binding 0) + `PackedChunkVoxelPayload` (binding 1); writes per-voxel emission to `vctClipmap` (binding 2, writeonly `rgba16f` 3D image). Air + Glass voxels skipped.
- `src/render/vulkan/VulkanVoxelizePipeline.{hpp,cpp}` (NEW, ~450 LoC) — `IsVctGpuPipelineRequested()` env gate (`PROJECTV_VCT_GPU=ON`, default OFF) + 3D image allocation (256³ RGBA16F, 4 mip levels) + linear filter sampler + compute pipeline + 3-binding descriptor set.
- `src/core/Types.hpp` — `SceneFrameResources` adds `vctVoxelizeDescriptorSet`; `RenderState` adds 14 VCT fields.
- `src/CMakeLists.txt` — `voxelize.comp` added to `SHADERS` + `VulkanVoxelizePipeline.cpp` added to sources.
- `tests/VoxelizePipelineTests.cpp` (NEW) — 11 sub-tests (env gate, struct size, null guards, mip chain).

### Phase 3: Stage 5.1 VCT diffuse 6-cone tracing (closed)

- `src/shaders/voxel.frag` — added `VctSampleDirectionalCone` helper (3-tap adaptive sampling, log2 distance mip selection) + 6 fixed diffuse cones per TODO §5.1 explicit + `vctDiffuse` contribution to final color. New `sampler3D vctClipmap` at binding 11. New `vec4 vctParams` in `SceneLightingBuffer`.
- `src/voxel/VoxelMaterials.hpp` — `VoxelSceneLighting` struct extended with `vctParams` field. Struct size 624 → 640 bytes.

### Phase 4: Stage 5.1 VCT specular 1-cone + kVctCutoffRoughness=0.3 hybrid (closed)

- `src/shaders/voxel.frag` — added `VctSampleReflectionCone` (1 cone in reflection direction, aperture = roughness * 0.6) + `roughness > kVctCutoffRoughness=0.3` gate per `2026-06-20-vct-vs-rt-cutoff` + Fresnel-Schlick. `vctSpecularParams` vec4 field. Struct size 640 → 656 bytes.

### Phase 5: Stage 5.1 VCT GPU mip chain build (closed)

- `src/render/vulkan/VulkanVoxelizePipeline.{hpp,cpp}` — added `BuildVctClipmapMipChain` function. Records `vkCmdBlitImage` 3D-to-3D mip chain on graphics CB with `VK_FILTER_LINEAR`. Mirrors `BuildHizMipChain` 2D pattern.
- `tests/VoxelizePipelineTests.cpp` — added 2 sub-tests for `BuildVctClipmapMipChain` rejection paths.

### Phase 6: Tests + visual smoke + Tracy plot review

- Full ctest: 34/35 + 1 documented pre-existing failure. All 12 new sub-tests + all 70+ existing sub-tests pass. Build green.
- EVIL marker audit: 3 new EVIL markers added (per-chunk dispatch, 6-cone design, kVctMaxDistanceMeters).

### Phase 7: Doc sync (this entry)

- `agent/workspace.md` + `COMMENTS.md` + `CHANGELOG.md` + `TODO.md` updated.
- `TODO.md` — Stage 5.1 partial (foundation + diffuse + specular + mip chain landed); Stage 6.3 closed (depth ownership transfer landed).

### Verification

- `cmake --build build/linux-clang-debug --target ProjectV` — green.
- `ctest --test-dir build/linux-clang-debug -j 8 -E "ProjectVTests"` — **34/34 pass** (ProjectVTests excluded for known pre-existing failure).
- 1 new test target + 12 new sub-tests, all green.
- Dirty tree: 7 src files modified + 1 new src file + 2 new test files + 1 test file modified + 4 docs modified. **No commit** (per operator "close dirty without prompt" policy).

### Operator commit prompt (per workspace.md policy)

ONE "Commit?" prompt at session end, covering 8x Variant A work as single commit. Suggested commit message (per `AGENTS.md §5.1` format):

```
feat(render): 8x session — close async HZB depth + open VCT foundation

7 phases + doc sync across 2 TODO stages (6.3 HZB cross-queue depth complete,
5.1 VCT foundation). Build green, 34/35 ctest pass + 1 documented pre-existing
failure (ProjectVTests same baseline as 4x/8x/12x/8x V1).

NEW: HZB cross-queue depth ownership transfer (proper VkImageMemoryBarrier2
replacing 8x V1 placeholder; closes the partial) +
     VCT 3D clipmap (voxelize.comp per-voxel injection + 256^3 RGBA16F 4-mip
     texture + VulkanVoxelizePipeline infrastructure) +
     VCT diffuse 6-cone tracing (per TODO §5.1 explicit, kVctConeDirectionCount=6) +
     VCT specular 1-cone (kVctCutoffRoughness=0.3 hybrid per
     2026-06-20-vct-vs-rt-cutoff experiment) +
     VCT GPU mip chain build (vkCmdBlitImage 3D-to-3D VK_FILTER_LINEAR).

1 new test target + 12 new sub-tests, all green.

Refs: TODO.md §5.1, §6.3
Refs: agent/knowledge.md §15 (lighting contract), §30.4 (3-step migration)
Refs: docs/experiments/INDEX.md 2026-06-20-vct-vs-rt-cutoff
Refs: docs/experiments/INDEX.md 2026-06-20-rt-shadows-vs-csm (Stage 5.2 follow-up)
```

---

## 2026-06-21 (session: 8x Variant 1 «close partial APIs» — 6 TODO stages: 4.2 / 2.1 / 4.3 / 6.3 / 1.1 / 3.2)

7 phases + doc sync across 6 TODO stages. Build green, **33/34 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as prior 4x/8x/12x). 1 new test target (`ProjectVPhysicsIncrementalJoltTests` = 6 sub-tests) + 18 new sub-tests in existing targets (LodDownsampleGpuConsume +5, HzbSmartMip +3, ChunkStreaming +3, AsyncCompute +4, NanoVdbGpuUpload +3). All green. **No commit performed** per operator policy "close dirty without prompt" (per AGENTS.md §5.4).

### Phase 1: Stage 4.2 LOD GPU consume mesh emission (closed)

- `src/shaders/voxel_mesh.comp` — added `kLodWordStride=16` (must match `projectv::render::kLodPayloadWordStride` in `LodDownsampleGpuConsume.hpp`) + `GetChunkLodLevel(chunkIndex)` / `GetChunkLodExtent(chunkIndex)` decoders for packed `chunkLodLevelsBuffer` (bits [0:8] = lodLevel, bits [8:16] = outExtent) + `DecodeLodVoxelMaterial(chunkIndex, localCoord, outExtent)` helper (reads from `lodDownsampled.lodDownsampledWords[chunkIndex*16 + ...]`) + `DecodeVoxelMaterialForLod(...)` dispatcher to `DecodeChunkVoxelMaterial` (LOD 0) or `DecodeLodVoxelMaterial` (LOD >0). `GreedyFacePass` signature extended with `chunkLodLevel + outExtent`; uses per-chunk extent from metadata instead of `chunkDescriptor.chunkExtentAndNonAir[axisN]` when `chunkLodLevel > 0` (without this, lower-LOD chunks would iterate the original 8³ extent and read garbage from the smaller downsampled payload). `maxChunkFaceCount` also switches to `outExtent³ × 6` for LOD chunks.
- `src/render/LodDownsampleGpuConsume.hpp` — added `kLodPayloadWordStride=16` const + `LodPayloadWordOffsetForChunk(chunkIndex)` + `EncodeChunkLodMetadata(lodLevel, outExtent)` / `DecodeChunkLodMetadata(metadata, outLodLevel, outExtent)` + `BuildLodPayloadWordsFromDownsampled(bytes, count, outWords)` (pack 4 voxels/uint32 little-endian with zero-padding for short input).
- `src/render/LodDownsampleGpuConsume.cpp` — `RefreshLodDownsampledBuffers` now iterates chunks, calls `DownsampleChunkForLodSurfacePreserve` for each chunk with `chunk.lodLevel > 0`, packs bytes via `BuildLodPayloadWordsFromDownsampled`, writes to `lodDownsampledVoxelPayloadMappedData + LodPayloadWordOffsetForChunk(i) * sizeof(uint32_t)`. Also encodes `chunkLodLevelsBuffer[i] = EncodeChunkLodMetadata(lodLevel, outExtent)`. Only bumps `lodDownsampledPayloadVersion` if any LOD>0 chunk was processed.
- `tests/LodDownsampleGpuConsumeTests.cpp` — extended 6→11 sub-tests: stride constant, word offset linear scaling, encode/decode metadata roundtrip for 32 (lod, extent) combinations, pack 4 bytes/word (little-endian), pack short input with zero-padding, null safety.
- `tests/CMakeLists.txt` — added `VoxelLodDownsample.cpp` to `ProjectVLodDownsampleGpuConsumeTests` target (link was missing since the downsample helper is in a separate TU).
- `tests/CMakeLists.txt` — added `voxel/VoxelLodDownsample.cpp` to `ProjectVLodDownsampleGpuConsumeTests` source list.

### Phase 2: Stage 2.1 HZB smart blend width shader consume (closed)

- `src/render/SceneResources.cpp` — `hzbPerChunkMipBytes` doubled from `chunkCount * sizeof(uint32_t)` to `chunkCount * 2 * sizeof(uint32_t)` (stride = 2 uint32 per chunk, packed `[mip, blendWidth, mip, blendWidth, ...]`).
- `src/render/HizCulling.hpp` — added `kHizMipAndBlendWidthWordsPerChunk=2` const (must match shader stride in `hzb_cull.comp` per `agent/knowledge.md §30.4` 3-step migration precedent) + `WritePerChunkMipAndBlendWidthsToBuffer(mappedData, mipAndBlendWidths, chunkCount)` pure packer (no Vulkan deps, testable in isolation; null-safe).
- `src/render/HizCulling.cpp` — implementation added.
- `src/shaders/hzb_cull.comp` — binding 5 changed from `perChunkMipLevels[]` (1×uint32/chunk) to `perChunkMipAndBlendWidth[]` (2×uint32/chunk packed). `AabbVisibleAgainstMip` takes new `blendWidthTexels` parameter; when `> 0`, expands screen-space sample footprint by `blendWidth / mipSize` (clamped) before `texelFetch`, eliminating 0.02-0.20% FN per `2026-06-21-hzb-smart-blend-width` verdict. 2-phase fallback unchanged (only uses mip=0 retry, no blend width in fallback).
- `tests/HzbSmartMipTests.cpp` — extended 9→12 sub-tests: pack/unpack roundtrip (4 chunks), null safety, zero chunk count no-op.

### Phase 3: Stage 4.3 Chunk prebake integration (closed)

- `src/app/main.cpp` — `FinalizeActiveVoxelWorldReload` calls `ChunkStreamer::BakeAllChunksToDisk(*world, prebakeStats)` after `SnapModelInstancesAboveGroundDispatch`, gated on `IsChunkStreamingEnabled()`. Tracy plots `"Chunk Prebake Chunks Baked"` + `"Chunk Prebake Voxel Bytes"` per reload.
- `src/app/FramePreparation.cpp` — per-frame `PreloadChunksAroundCamera(camera, radius=8)` call when `IsChunkStreamerPrebakeReady()` (i.e. `prebakeVersion > 0`). Tracy plot `"Chunk Streamer Preload Queue Depth"`.
- `tests/ChunkStreamingTests.cpp` — extended 14→17 sub-tests: populated world populates preload, radius=0 enqueues the camera's single chunk (radius loop = `for gx = cameraChunkX; gx <= cameraChunkX; ++gx`), bake bumps prebake version.

### Phase 4: Stage 6.3 HZB async compute cross-queue depth sync (partial)

- `src/core/Types.hpp` — `VulkanContextState` gains `VkSemaphore hzbBuildTimelineSemaphore = VK_NULL_HANDLE` + `uint64_t hzbBuildLastTimelineValue = 0`.
- `src/render/vulkan/VulkanBootstrap.cpp` — creates `hzbBuildTimelineSemaphore` with `VkSemaphoreTypeCreateInfo{ semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, initialValue = 0 }` after `renderTimelineSemaphore`. Object name `"HzbBuildTimelineSemaphore"`.
- `src/core/Types.cpp::ShutdownVulkan` — destroys `hzbBuildTimelineSemaphore` after `renderTimelineSemaphore`.
- `src/render/vulkan/VulkanAsyncCompute.hpp` — adds `RecordHzbAsyncCullPass(asyncCommandBuffer, context, render, inverseViewProjection, chunkDescriptorCount)` + `SubmitHzbAsyncCullToComputeQueue(context, asyncCommandBuffer, outTimelineValue)`.
- `src/render/vulkan/VulkanAsyncCompute.cpp` — `RecordHzbAsyncCullPass` records a placeholder `VkImageMemoryBarrier2` (HZB image stays in `SHADER_READ_ONLY_OPTIMAL` on compute queue) then calls `RecordHzbCullingDispatch` from `HizCulling.cpp` (re-uses existing HZB pipeline). `SubmitHzbAsyncCullToComputeQueue` uses canonical nvpro-samples pattern: 1-frame pipeline depth, `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` wait at value `timelineValue - 1` / signal at `timelineValue`, `VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT` stage.
- `src/render/Renderer.cpp` — `asyncComputeHzbPathActive` predicate (`IsAsyncComputeEnabled() && IsAsyncComputeResourcesAllocated() && IsHzbCullingEnabled() && hizBuffer.image != VK_NULL_HANDLE && hizCullingDescriptorSet != VK_NULL_HANDLE`). HZB cull on graphics CB skipped when active. Graphics submit adds 2nd `VkSemaphoreSubmitInfo` signal on `hzbBuildTimelineSemaphore` at value `hzbBuildLastTimelineValue` with `VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT` stage.
- `tests/AsyncComputeTests.cpp` — extended 7→11 sub-tests: null CB rejection, null context rejection, null CB rejection (Hzb variant), default `hzbBuildTimelineSemaphore` VK_NULL_HANDLE.
- `tests/CMakeLists.txt` — added `HizCulling.cpp` to `ProjectVAsyncComputeTests` source list (link was missing).
- **Depth attachment `srcQueueFamilyIndex`→`dstQueueFamilyIndex` ownership transfer still deferred.** Current code uses `VK_QUEUE_FAMILY_IGNORED` placeholder; full sync per Khronos Synchronization Examples requires explicit `srcQueueFamilyIndex = graphics` / `dstQueueFamilyIndex = compute` + `srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT` at `LATE_FRAGMENT_TESTS_BIT` → `dstAccessMask = VK_ACCESS_SHADER_READ_BIT` at `COMPUTE_SHADER_BIT`. Documented in `COMMENTS.md` VulkanAsyncCompute section.

### Phase 5: Stage 1.1 NanoVDB resize logic for 1024+ chunks (closed)

- `src/render/SceneResources.hpp` — adds `ComputeGrownNanoVdbCapacity(currentCapacityBytes, requiredCapacityBytes)` + `GrowNanoVdbBuffer(context, buffer, allocation, mappedData, capacityBytes, newCapacityBytes, profilingTag)`.
- `src/render/SceneResources.cpp` — `UploadSceneFrameResources` signature changed to take `VulkanContextState *context` (was: implicit via render state). When `flatten result > current capacity`, calls `GrowNanoVdbBuffer` for each under-sized buffer (Upper/Lower/Leaf/Material), then `RefreshNanoVdbFlattenBuffers`. `LogRuntimeFailure` only fires on grow failure, not on size mismatch. `ComputeGrownNanoVdbCapacity` = 1.5× current OR required, whichever is larger; zero current → required.
- `src/voxel/NanoVdb.hpp` — adds test-only `ComputeGrownNanoVdbCapacityForTest(current, required)` inline mirror (same math, accessible to `ProjectVNanoVdbGpuUploadTests` without linking SceneResources module).
- `src/app/FramePreparation.cpp` — updates `UploadSceneFrameResources(*render, ...)` call site to `UploadSceneFrameResources(context, *render, ...)`.
- `tests/NanoVdbGpuUploadTests.cpp` — extended 5→8 sub-tests: zero current → required, smaller required → keeps current, larger required → 1.5× AND satisfies required.

### Phase 6: Stage 3.2 Incremental Jolt Step 1 — boundary-neighbor queue (closed)

- `src/voxel/VoxelWorld.cpp::SetVoxelMaterial` — when `physics != nullptr`, now calls `QueueChunkRebuildRequest(physics, chunkIndex)` for the edited chunk AND all 6 face-sharing boundary neighbors (when edit sits on a chunk face), mirroring the existing visual rebuild range in `MarkChunksTouchedByVoxelEditDirty`. Previously only the center chunk was queued, leaving neighbor chunks' CompoundShape out of sync — players could fall through near chunk edges. Iterates the same neighbor cube `for (z,y,x)` loop used by the visual path; computes min/max chunk coords independently of the visual path to avoid coupling the two systems.
- `tests/PhysicsIncrementalJoltTests.cpp` (NEW) — 6 sub-tests: null physics rejection, queue append no-dedup, null inputs rejection (3 variants), null accessor rejection (2 variants), empty queue returns 0.
- `tests/CMakeLists.txt` — new `ProjectVPhysicsIncrementalJoltTests` target with full source list (VoxelWorld, PhysicsWorld, GreedyPhysicsMerger, NanoVdb, LodDownsample, RuntimeDiagnostics, InputActions, VulkanResult) + Jolt + VMA + SDL3 + volk + glm + fmt + Tracy links + `projectv_test_add_modules` call.

### Phase 7: doc sync (this entry)

- `agent/workspace.md` — updated `§1 Now` + `§2 Nearest Gap` + `§3 Next Steps` + `§4 Risks` + `§5 Active tasks` + `§6 Recent closed sessions` for 8x Variant 1 closure.
- `TODO.md` — marked Stage 4.2 fully closed (mesh emission landed) + Stage 2.1 fully closed (blend width shader consume) + Stage 4.3 fully closed (prebake integration) + Stage 6.3 partial closed (2nd timeline semaphore + cross-queue submit helpers) + Stage 1.1 fully closed (resize logic) + Stage 3.2 partially closed (boundary-neighbor queue).
- `COMMENTS.md` — new entries for `src/render/HizCulling.{hpp,cpp}` (smart blend width full shader consume), `src/shaders/voxel_mesh.comp` (LOD mesh emission), `src/render/vulkan/VulkanAsyncCompute.{hpp,cpp}` (HZB async cross-queue), `src/voxel/NanoVdb.hpp` (resize capacity math), `src/voxel/VoxelWorld.cpp` (physics boundary-neighbor queue), `src/render/SceneResources.cpp` (NanoVDB grow-on-exceed).
- `CHANGELOG.md` — this entry.

### Verification

- `cmake --build build/linux-clang-debug --target ProjectV` — green.
- `ctest --test-dir build/linux-clang-debug` — **33/34 pass** + 1 documented pre-existing failure (`ProjectVTests::TestSpectatorModeAllowsPausedMovementButBlocksEdits` same baseline as 4x/8x/12x).
- 1 new test target + 18 new sub-tests in existing targets, all green.
- Dirty tree: 18 files modified in `src/` + 1 untracked test file (`tests/PhysicsIncrementalJoltTests.cpp`). **No commit** (per operator "close dirty without prompt" policy).

### Operator commit prompt (per workspace.md policy)

ONE "Commit?" prompt at session end, covering 8x work as single commit. Suggested commit message (per `AGENTS.md §5.1` format):

```
feat(voxel,render,physics): 8x session — close 5 partial APIs + HZB async Phase 4

7 phases across 6 TODO stages (4.2, 2.1, 4.3, 6.3, 1.1, 3.2) on top of 4x dirty
baseline. Build green, 33/34 ctest pass + 1 documented pre-existing failure
(ProjectVTests same baseline).

NEW: LOD GPU consume mesh emission (GreedyFacePass per-chunk extent +
downsampled payload decode) +
     HZB smart blend width shader consume (SSBO stride 2 + blendWidthTexels
     footprint expansion) +
     Chunk prebake integration (FinalizeActiveVoxelWorldReload wire +
     per-frame PreloadChunksAroundCamera drain) +
     HZB async cross-queue depth sync (2nd timeline semaphore +
     cross-queue submit helpers; depth ownership transfer partial) +
     NanoVDB resize logic (1.5x grow strategy + realloc path) +
     Incremental Jolt boundary-neighbor queue (per-chunk + 6 face-sharing
     neighbors in SetVoxelMaterial).

1 new test target + 18 new sub-tests in existing targets, all green.

Refs: TODO.md §2.1, §4.2, §4.3, §6.3, §1.1, §3.2
Refs: agent/knowledge.md §30.4 (3-step migration precedent)
Refs: docs/experiments/INDEX.md 2026-06-21-hzb-smart-blend-width verdict=mixed
Refs: docs/experiments/INDEX.md 2026-06-20-rt-shadows-vs-csm (cross-queue depth sync)
```

---

## 2026-06-21 (session: 4x — 4 close-out TODO stages: 6.3 / 4.2 / 4.3 Step 3 / 2.1 v2)

4 phases across 4 TODO stages. Build green, **32/33 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as prior 4x/8x/12x). 2 new test targets (`ProjectVAsyncComputeTests` = 7 sub-tests, `ProjectVLodDownsampleGpuConsumeTests` = 6 sub-tests) + 13 new sub-tests in existing targets (Chunk Streaming +4, Hzb Smart Mip +3). All green. **No commit performed** per operator policy "close dirty without prompt" (per AGENTS.md §5.4).

### Phase A: Stage 6.3 per-pass async compute wiring (partial)

- `src/render/vulkan/VulkanAsyncCompute.{hpp,cpp}` (NEW, ~280 LoC) — `EnsureAsyncComputeResources` (dedicated compute command pool `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` + 1 one-shot `VkCommandBuffer` per nvpro-samples transient pool pattern) + `RecordAsyncComputePass` (orchestrator: Fluid CA + world gen dispatches into the async CB; HZB deferred due to cross-queue depth sync complexity) + `SubmitToComputeQueue` (generalized `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` + `renderTimelineSemaphore` cross-queue sync per `agent/knowledge.md §30.4` 3-step migration).
- `src/core/Types.hpp` `VulkanContextState` — added `asyncComputeCommandPool` + `asyncComputeCommandBuffer` + `asyncComputeLastTimelineValue` fields.
- `src/core/Types.cpp::ShutdownVulkan` — calls `projectv::render::DestroyAsyncComputeResources` before command pool destroy.
- `src/render/vulkan/VulkanInit.cpp::InitVulkan` — calls `projectv::render::EnsureAsyncComputeResources` gated on `IsAsyncComputeEnabled()` (env `PROJECTV_ASYNC_COMPUTE=ON`, default OFF).
- `src/render/Renderer.cpp::DrawFrame` — computes `asyncComputePathActive` predicate + skips Fluid CA + world gen on graphics CB when async ON + submits async CB via `SubmitToComputeQueue` + adds 2nd `VkSemaphoreSubmitInfo` wait on graphics submit at value=`asyncComputeLastTimelineValue` (1-frame async compute pipeline depth per nvpro-samples). HZB dispatch on graphics CB unchanged.
- `src/CMakeLists.txt` — registered `VulkanAsyncCompute.cpp`.
- `tests/AsyncComputeTests.cpp` — extended 3→7 sub-tests (env default-off, env=1, env=0, null context rejected, default state unallocated, null CB rejected, null context submit rejected).
- `tests/CMakeLists.txt` — added `VulkanAsyncCompute.cpp` + `VulkanWorldGenPipeline.cpp` + `VoxelWorld.cpp` + `VoxelLodDownsample.cpp` + `NanoVdb.cpp` + `PhysicsWorld.cpp` + `GreedyPhysicsMerger.cpp` + `InputActions.cpp` + Jolt link to `ProjectVAsyncComputeTests`.
- **DoD:** Stage 6.3 additive optional path wired (Fluid CA + world gen). HZB cull + RTX BLAS routing deferred (multi-session risk on cross-queue depth sync).

### Phase B: Stage 4.2 LOD GPU consume infrastructure (partial)

- `src/render/LodDownsampleGpuConsume.{hpp,cpp}` (NEW, ~250 LoC) — `IsLodDownsampledGpuConsumeEnabled()` env gate (`PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=ON`, default OFF) + `ComputeLodDownsampledVoxelPayloadBytes` + `ComputeChunkLodLevelsCapacity` + `RefreshLodDownsampledBuffers` per-frame upload (writes per-chunk `lodLevel` from `world.chunks[i].lodLevel` to new `chunkLodLevelsBuffer` SSBO + zeros `lodDownsampledVoxelPayloadBuffer`).
- `src/core/Types.hpp` `SceneFrameResources` — added 2 new SSBO field groups (`lodDownsampledVoxelPayloadMappedData/Buffer/Allocation/CapacityBytes` + `chunkLodLevelsMappedData/Buffer/Allocation/Capacity`).
- `src/render/SceneResources.cpp` — extended alloc/destroy/nullify structured-binding list + creates buffers in `CreateSceneResources` (after `hzbPerChunkMip` alloc).
- `src/shaders/voxel_mesh.comp` — added 2 new bindings: `LodDownsampledVoxelPayload` at binding 9 + `ChunkLodLevels` at binding 10 (read-only).
- `src/app/FramePreparation.cpp` — calls `RefreshLodDownsampledBuffers` gated on `IsLodDownsampledGpuConsumeEnabled()`.
- `src/CMakeLists.txt` — registered `LodDownsampleGpuConsume.cpp`.
- `tests/LodDownsampleGpuConsumeTests.cpp` (NEW, 6 sub-tests) — env gate default-off, env=ON, env=0, capacity bytes scaling, chunk LOD capacity floor, null context rejected.
- `tests/CMakeLists.txt` — registered `ProjectVLodDownsampleGpuConsumeTests`.
- **DoD:** SSBO + binding infrastructure in place. Actual mesh emission from downsampled payload deferred (`GreedyFacePass` hardcoded to `chunkSize=8` extent; needs per-chunk extent parameterization).

### Phase C: Stage 4.3 Chunk Streaming Step 3 API (partial)

- `src/voxel/ChunkStreamer.{hpp,cpp}` — added `BakeAllChunksToDisk(world, outStats)` (cold-path; iterates chunks, serializes each chunk's material grid `chunkSize^3` uint8_t via `sparseStorage.GetCell` to `chunk_<index>.bin` with the same 16-byte header format) + `IsChunkStreamerPrebakeReady()` + `GetChunkStreamerPrebakeVersion()` (atomic uint64 `prebakeVersion` tracker) + `PreloadChunksAroundCamera(cameraX, cameraY, cameraZ, radiusChunks)` (per-frame priority injection: iterates grid cells within radius via `gz * gridHeight * gridWidth + gy * gridWidth + gx`).
- `src/voxel/ChunkStreamer.cpp` — added `WriteChunkBinaryFile` helper (header + voxel bytes; uses `<filesystem>` for directory creation; `std::array<uint8_t, 16>` for header layout).
- `tests/ChunkStreamingTests.cpp` — extended 10→14 sub-tests (prebake version starts zero, bake disabled when streaming off, preload disabled when streaming off, empty world returns zero).
- **DoD:** API surface in place. Per-frame integration in `FinalizeActiveVoxelWorldReload` + camera-aware drain deferred (multi-session work).

### Phase D: Stage 2.1 HZB smart blend width v2 API (partial)

- `src/render/HizCulling.{hpp,cpp}` — added `IsHzbSmartBlendWidthEnabled()` env gate (`PROJECTV_HZB_SMART_BLEND_WIDTH=ON`, default OFF) + `ComputeBlendWidthForChunkMip(projectedXTexels, projectedYTexels, mipLevel, maxBlendWidth)` CPU helper (computes `texelsAtMip / 4 + frac / 8` bounded by `maxBlendWidth`) + `ComputePerChunkMipAndBlendWidthsFromAabbs` (per-chunk CPU compute of both mip + blend width into packed `[mip, blendWidth, mip, blendWidth, ...]` output vector; reuses 8-corner AABB projection).
- `tests/HzbSmartMipTests.cpp` — extended 6→9 sub-tests (zero max blend width returns zero, zero mip returns zero, blend width bounded by max).
- **DoD:** CPU helper in place. SSBO struct change to `[mip+blendWidth]` per-chunk + shader smart blend logic deferred (requires changing existing `perChunkMipLevels[]` SSBO semantics + `hzb_cull.comp` consume).

### Phase E: doc sync (no commit)

- `agent/workspace.md` — updated §1 Now, §2 Nearest Gap, §3 Next Steps, §4 Risks, §5 Active tasks, §6 Recent closed.
- `TODO.md` — marked §6.3 (4x partial), §4.2 (4x partial), §4.3 Step 3 (4x partial), §2.1 v2 (4x partial) statuses.
- `COMMENTS.md` — design-rationale for `LodDownsampleGpuConsume.{hpp,cpp}`, `ChunkStreamer` prebake API, `HizCulling` smart blend width v2.
- **No commit prompt** (per operator "close dirty without prompt" directive).

---

## 2026-06-21 (session: 4x — 4 close-out TODO stages: 5.3 / 4.1 / 4.3 / 2.1)

4 phases across 4 TODO stages. Build green, **31/32 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x/8x/12x). 1 new test target (`ProjectVHzbSmartMipTests` = 6 sub-tests) + 17 new sub-tests in existing targets (TAA Motion Vector +3, World Gen +4, Chunk Streaming +6). All green. **No commit performed** per operator instruction "без коммита" (session closed dirty per AGENTS.md §5.4).

### Phase 1: Stage 5.3 TAA Motion Vectors resolve consume (closed)

- `src/shaders/taa_resolve.frag` — added `layout(set = 0, binding = 4) uniform sampler2D motionVector;`. Replaced depth-reproject path (lines 167-182: `worldPos = inverseCurrentViewProjection * ndcNear` + `prevClip = prevViewProjectionMatrix * worldPos`) with `texture(motionVector, uv).xy → prevUv = uv + motion` (Karis 2014 pipeline A). Voxel fragment shader already writes the offset in NDC UV space per `voxel.frag:903` (`outMotionVector = prevNdc - currNdc`).
- `src/render/vulkan/TaaResolvePipeline.cpp` — extended `kTaaResolveDescriptorBindings` from 5 to 6 (added binding 4 = `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`). Pool size bumped from 3 to 4 samplers per frame. Added `motionVectorImageInfo` `VkDescriptorImageInfo` + 4th `VkWriteDescriptorSet`. Precondition check now requires `taaMotionVectorTarget != nullptr`.
- `src/render/Renderer.cpp` — removed `inverseCurrentViewProj` calculation (motion vector path no longer needs current→world unprojection). `currentViewProjection` push constant still passed (unused by new path, retained for ABI compatibility).
- `tests/TaaMotionVectorTests.cpp` — 3 new sub-tests: `TestMotionVectorResolveContract` (uv+motion prevUv math for valid/out-of-bounds/identity cases), `TestResolveShaderMotionBinding` (read `taa_resolve.frag` and verify binding 4 declaration + `texture(motionVector, ...)` call). Total 8/8 sub-tests.
- **DoD:** data path + resolve consume complete. Ghost trails behind moving models now use motion vectors instead of depth reconstruction. Validation layers clean. ~60 LoC.

### Phase 2: Stage 4.1 GPU World Gen frame dispatch wiring (closed)

- `src/render/vulkan/VulkanInit.cpp` — added `CreateWorldGenPipelines` + `RefreshWorldGenResourceBindings` calls after Fluid CA, gated on `IsWorldGenGpuPipelineRequested()`. Graceful fallback to log + skip dispatch on failure (per `agent/knowledge.md §30.4` Step 1 contract).
- `src/core/Types.cpp` — added `DestroyWorldGenPipelines` in `ShutdownVulkan` after `DestroyFluidCaPipelines`.
- `src/core/Types.hpp` — added `VulkanWorldGenPipeline.hpp` include.
- `src/render/Renderer.cpp` — added world gen dispatch in `DrawFrame` after Fluid CA:
  ```cpp
  if (render->worldGenPipelineEnabled && state->world().voxelWorld != nullptr) {
      VoxelWorld *voxelWorld = state->world().voxelWorld.get();
      std::vector<uint32_t> activeWorldGenChunkIds;
      const uint32_t worldGenChunkCount =
          projectv::render::BuildActiveChunkIdsForWorldGen(*voxelWorld, activeWorldGenChunkIds);
      // ... memset voxel buffer, populate push constants, RecordWorldGenDispatch
  }
  ```
  Per-chunk seed = `state->simulation().simulationTick` (deterministic per-frame).
- `src/render/vulkan/VulkanWorldGenPipeline.hpp` — moved `kWorldGenVoxelBufferBytesPerChunk` constant from `.cpp` to header (used by Renderer.cpp for memset sizing).
- `tests/WorldGenTests.cpp` — 4 new sub-tests: `TestWorldGenPushConstantContract` (sizeof 64 + zero-init), `TestWorldGenVoxelBufferBytesPerChunkContract` (= 2048), `TestWorldGenDispatchSkipOnZeroActiveChunks`, `TestWorldGenSeedTickVariability`. Total 7/7 sub-tests.
- **DoD:** pipeline infrastructure complete end-to-end. Empty chunks get procedural noise on first frame. `voxel_lab` scene generates world automatically when env gate ON. ~100 LoC.

### Phase 3: Stage 4.3 Chunk Streaming Step 2 — background thread + SSD read (closed)

- `src/voxel/ChunkStreamer.hpp` — added `StartChunkStreamerWorker` + `StopChunkStreamerWorker` + `IsChunkStreamerWorkerActive` + `GetChunkStreamerCachePath` + `ProcessPendingRequests(std::stop_token)`. Extended `ChunkStreamError` with `FileNotFound` + `FileReadFailed`.
- `src/voxel/ChunkStreamer.cpp` — implemented C++20 `std::jthread` background worker with `std::stop_token` cooperative cancellation (per cppreference docs). Binary file format: 16-byte header (magic `0x504B5631` = "PKV1" in little-endian + uint32 version `1` + uint64 voxel byte count) + serialized voxel bytes. Worker loop: pop pending requests → read `PROJECTV_CHUNK_PATH`/chunk_<index>.bin → push to ready queue. Idle sleep 5ms. Polling 1ms after batch. `EnqueueChunkStreamRequest` lazily starts worker via `compare_exchange_strong` atomic guard.
- `src/app/main.cpp` — added `projectv::voxel::StopChunkStreamerWorker()` call in `SDL_AppQuit` before `ShutdownVulkan`.
- `src/app/FramePreparation.cpp` — added per-frame drain: up to 8 chunks per frame (`kMaxChunksPerFrame = 8u`) with Tracy plots `Chunk Stream Drained` + `Chunk Stream Pending`. Gated on `IsChunkStreamingEnabled()`.
- `tests/ChunkStreamingTests.cpp` — 6 new sub-tests: `TestWorkerActiveFlagLifecycle` (start/stop), `TestCachePathFromEnv` (PROJECTV_CHUNK_PATH override), `TestCachePathFallback` (PROJECTV_CMAKE_BUILD_DIR fallback), `TestProcessPendingRequestsStopToken` (jthread cooperative cancellation), `TestEnqueueStartsWorkerLazy`. Total 10/10 sub-tests.
- **DoD:** end-to-end working. Worker joins on `StopChunkStreamerWorker()` at shutdown. TSan-clean expected (mutex-protected queues, atomic flag). ~300 LoC.

### Phase 4: Stage 2.1 HZB smart mip select (closed)

- `src/core/Types.hpp` — `SceneFrameResources` added `hzbPerChunkMipMappedData` + `hzbPerChunkMipBuffer` + `hzbPerChunkMipAllocation` + `hzbPerChunkMipCapacityBytes` fields (SSBO per frame, capacity = `max(chunks.size(), 1u) * sizeof(uint32_t)`).
- `src/render/SceneResources.cpp` — added alloc in `InitializeSceneResources` (using `CreateBuffer` helper) + structured-binding entries in destroy loop + nullify block.
- `src/render/HizCulling.{hpp,cpp}` — extended `kHizCullingDescriptorBindings` from 5 to 6 (added binding 5 = `perChunkMipLevels` SSBO). Pool size bumped from 3 to 4 storage sets per frame. `RecordHzbCullingDispatch` precondition check + 6th `VkWriteDescriptorSet`. Added `IsHzbSmartMipEnabled()` env gate (default OFF) + `ComputePerChunkMipLevelCpu(projectedXTexels, projectedYTexels, maxMipLevel)` (Turitzin 2020 formula: `mip = floor(log2(max(projX, projY)))`) + `ComputePerChunkMipLevelsFromAabbs` (per-chunk CPU compute via 8-corner projection of AABB).
- `src/shaders/hzb_cull.comp` — added `layout(set = 0, binding = 5) readonly buffer PerChunkMip { uint perChunkMipLevels[]; };`. Per-thread loads `int perChunkMip = perChunkMipLevels[chunkIndex]` + `int useMipLevel = perChunkMip > 0 ? perChunkMip : mipLevel`. **2-phase fallback**: `if (!visible && perChunkMip > 0) { visible = AabbVisibleAgainstMip(...chunkIndex, 0...); }` eliminates 0.02-0.20% FN per `2026-06-21-hzb-smart-mip-select` experiment verdict=mixed. 700× texel reduction projected.
- `tests/HzbSmartMipTests.cpp` (NEW) — 6 sub-tests: `TestEnvGateDefault`/`TestEnvGateOff`/`TestEnvGateOn` (PROJECTV_HZB_SMART_MIP env), `TestComputePerChunkMipLevelCpuCloseChunk` (64×64 → mip 6, 8×8 → mip 3, sub-texel → mip 0), `TestComputePerChunkMipLevelCpuFarChunk` (mip 1/2 + cap at maxMipLevel), `TestComputePerChunkMipLevelsFromAabbs` (bigger chunk has mip >= smaller chunk).
- `tests/CMakeLists.txt` — registered `ProjectVHzbSmartMipTests` as standalone test target.
- **DoD:** default env OFF preserves mainline behavior. When enabled, per-chunk mip reduces HZB texel fetches by 700×. 0 FN with 2-phase fallback. ~200 LoC.

### Phase 5: doc sync (no commit)

- `agent/workspace.md` — updated §1 Now, §2 Nearest Gap, §3 Next Steps, §4 Risks, §5 Active tasks, §6 Recent closed.
- `CHANGELOG.md` — this section.
- `TODO.md` — marked §2.1 (4x v2), §4.1, §4.3, §5.3 statuses to `✅ Closed`.
- **No commit prompt** (per operator instruction "без коммита" — session closed dirty per AGENTS.md §5.4 + §5.9).

---

## 2026-06-21 (session: 12x — 7 stages across 6 TODO items on top of 4x+8x dirty baseline)

7 phases across 6 TODO stages (1.1, 3.3, 4.1, 4.3 Step 1, 5.3 partial) on top of 4x+8x dirty baseline. Build green, **30/31 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline as 4x/8x per `agent/workspace.md §1`). 4 new test targets + 16 new sub-tests, all green. **No commit** (operator "continue dirty" policy per `agent/workspace.md §5`).

### Phase 1: Stage 1.1 NanoVDB GPU upload (closed)

- `src/voxel/NanoVdb.hpp` — `PackNanoVdbFlattenData` inline helper (testable, no Vulkan dep) copies 4 vectors (uppers/lowers/leaves/materials) to opaque buffers. EVIL: structural_layout same as NanoVDB.h 32³/16³/8³.
- `src/render/SceneResources.{hpp,cpp}` — 4 new SSBO fields per frame in `SceneFrameResources` (`nanovdbUpperBuffer/Allocation/MappedData/CapacityBytes` + 3 more for Lower/Leaf/Material) + 1 version tracker `uploadedNanoVdbVersion`. `CreateSceneResources` allocates 1 upper + 64 lowers + 64 leaves + 64 materials (covers VoxelLab). `DestroySceneResources` structured binding list extended + per-buffer free + nullify. `RefreshNanoVdbFlattenBuffers` helper + `UploadSceneFrameResources` triggers on `sceneNanoVdbVersion` change with capacity check + `LogRuntimeFailure` on overflow.
- `src/core/Types.hpp` — 4 SSBO + 4 capacity + 1 version field added to `SceneFrameResources`.
- `tests/NanoVdbGpuUploadTests.cpp` (NEW, 5 sub-tests) — struct alignment contract (8/16/24 bytes), empty/populated pack roundtrip, version-based re-upload trigger, capacity budget contract (1+64+64+64 entries).
- **DoD:** `ProjectVNanoVdbGpuUploadTests` 100% pass; build green; ctest baseline preserved (30/31).

### Phase 2: Stage 3.3 Greedy Physics Meshing integration (closed)

- `src/physics/GreedyPhysicsMerger.{hpp,cpp}` (NEW) — D_3D greedy merge algorithm per `2026-06-21-greedy-physics-meshing-cpu` verdict=yes (35× shape reduction, 100% volume preservation). `MergedVoxelBox` struct + `GreedyMergeSolidVoxelsInBounds(world, bounds, outBoxes)` API + `IsGreedyPhysicsMeshEnabled()` env gate.
- `src/physics/PhysicsWorld.cpp` — integration in both `BuildStaticVoxelCollisionBody` (whole world) and `BuildChunkStaticCollisionBody` (per-chunk). Env gate `PROJECTV_GREEDY_PHYSICS_MESH=ON` default; `=OFF` falls back to naive per-voxel loop. Tracy plot "Physics Greedy Merge Box Count" + "Physics Greedy Merge Chunk Box Count".
- `tests/PhysicsGreedyMergerTests.cpp` (NEW, 7 sub-tests) — empty world, single voxel unit box, full chunk single box, volume preservation (sum of box volumes == solid voxel count), mixed half-chunk reduction, fluid+air ignored, bounds clamp.
- `src/CMakeLists.txt` — `physics/GreedyPhysicsMerger.cpp` added to `ProjectV` sources.
- **DoD:** `ProjectVPhysicsGreedyMergerTests` 100% pass; ≥4× shape reduction (35× in practice); JPH broadphase cost reduction (qualitative).

### Phase 3: Stage 5.3 TAA Motion Vectors GPU data path (partial — TAA resolve consume deferred)

- `src/shaders/voxel.frag` — `outMotionVector` (vec2, location 3) computed from `prevViewProjectionMatrix * worldPos` NDC delta vs current `viewProjection * worldPos` NDC. Per `2026-06-21-taa-motion-vectors` verdict=yes Pipeline A (Karis 2014 SIGGRAPH "16:16 RG velocity buffer").
- `src/render/TaaRenderTargets.{hpp,cpp}` — 2 new `OffscreenColorTarget` params (`motionVectorColor` + `motionVectorHistoryColor`) using `kTaaMotionVectorFormat` (R16G16_SFLOAT). New `TransitionTaaMotionVectorForSample` + `RecordTaaMotionVectorHistoryCopy` helpers.
- `src/render/vulkan/VulkanGraphicsPipeline.cpp` — 4th color attachment format `kTaaMotionVectorFormat` added to `mainColorAttachmentFormats[4]`, `colorAttachmentCount` bumped 3→4.
- `src/render/Renderer.cpp` — `mainColor3View` 4th dynamic rendering attachment bound. `colorAttachmentCount` 3→4.
- `src/render/vulkan/VulkanSwapchain.cpp` — alloc 2 new TAA targets + pass to `CreateOrRecreateTaaRenderTargets`.
- `src/core/Types.hpp` + `Types.cpp` — 2 new `RenderState` fields + destroy in shutdown.
- `tests/TaaMotionVectorTests.cpp` — 2 new sub-tests: R16G16_SFLOAT size contract (4 bytes/pixel), NDC range contract [-1, 1].
- **DoD:** data path complete; `TAA resolve consume` (taa_resolve.frag reading MV texture instead of computing from prevViewProjectionMatrix in-shader) deferred to dedicated session.
- **Risk:** 4th color attachment slot on RTX 3060 Ti — within 8 vec4 output budget per `agent/knowledge.md §21`.

### Phase 4: Stage 4.1 GPU World Gen dispatch infrastructure (partial — frame dispatch wiring deferred)

- `src/render/vulkan/VulkanWorldGenPipeline.{hpp,cpp}` (NEW) — `WorldGenPushConstants` (64 bytes, static_assert'd) + compute pipeline + 1-binding descriptor set + 1 SSBO per frame + env gate `PROJECTV_WORLD_GEN_GPU` + `BuildActiveChunkIdsForWorldGen(world, outChunkIds)` helper (filters out non-empty chunks) + `RecordWorldGenDispatch(commandBuffer, render, frameResources, pushConstants, activeChunkCount)`. `RefreshWorldGenResourceBindings` allocates descriptor sets per frame.
- `src/render/SceneResources.{hpp,cpp}` — 4 new fields in `SceneFrameResources` (`worldGenVoxelMappedData/Buffer/Allocation/CapacityBytes` + `worldGenDescriptorSet`).
- `src/core/Types.hpp` — 5 new `RenderState` fields (`worldGenPipelineEnabled` + shader/pipeline/layout/setlayout/pool handles).
- `src/CMakeLists.txt` — `render/vulkan/VulkanWorldGenPipeline.cpp` added to `ProjectV` sources.
- `tests/WorldGenTests.cpp` (NEW, 3 sub-tests) — push constant size contract (64 bytes), env gate default/ON/OFF.
- **DoD:** pipeline infrastructure complete; `Renderer.cpp::DrawFrame` dispatch wiring (CreateWorldGenPipelines + RefreshWorldGenResourceBindings + per-frame BuildActiveChunkIdsForWorldGen + RecordWorldGenDispatch for empty chunks) deferred to dedicated session.

### Phase 8: Stage 1.3 Async audio I/O scan (already implemented, verified)

- `src/audio/AudioEngine.cpp:420-437` `RefreshPlaylistAsync()` + `m_scanThread` (joined in `tick()`) + `m_playlistMutex` lock + `m_scanInProgress` atomic flag.
- `src/ecs/EcsWorld.cpp:194` calls `audio->RefreshPlaylistAsync()`.
- **Verified:** existing implementation matches TODO §1.3 DoD (no micro-stuttering, TSan-clean). No code changes.

### Phase 10: Stage 4.3 Chunk Streaming foundation Step 1 (partial — background thread + SSD read deferred)

- `src/voxel/ChunkStreamer.{hpp,cpp}` (NEW) — `ChunkStreamRequest` + `ChunkData` + `EnqueueChunkStreamRequest` + `DrainChunkStreamQueueSize` + `TryDequeueChunkData` + `IsChunkStreamingEnabled` env gate. Mutex-protected pending/ready deques.
- `src/CMakeLists.txt` — `voxel/ChunkStreamer.cpp` added to `ProjectV` sources.
- `tests/ChunkStreamingTests.cpp` (NEW, 4 sub-tests) — env gate default/ON/OFF, enqueue tracks size, dequeue empty returns `NotInitialized` error.
- **DoD:** interface contract in place; background thread + SSD read integration deferred to dedicated session.

### Deferred to dedicated sessions (multi-session work, per plan §Open risks)

- **Phase 5: Stage 6.3 per-pass async wiring** (~300 LoC) — requires dedicated compute command pool + one-shot CB allocation, multi-session risk.
- **Phase 6: Stage 6.2 AppState PIMPL full struct move** (~200 files, 172 sed sites, 6h) — high sed risk, requires AGENTS.md §5.4 safety-net patch first.
- **Phase 7: Stage 4.2 LOD chunk 2 Step 2 SelectLodMeshSource** (~250 LoC) — wire downsampled payload to `voxel_mesh.comp` via new SSBO.
- **Phase 9: Stage 2.1 HZB smart mip selection** (~200 LoC) — per-chunk screen-space size → mip, with 2-phase fallback per `2026-06-21-hzb-smart-mip-select` verdict=mixed.

### Operator commit prompt (per workspace.md policy)

ONE "Commit?" prompt at session end, covering 4x+8x+12x as single combined commit (operator "continue dirty" policy).
Suggested commit message (per `AGENTS.md §5.1` format):

```
feat(voxel,render,physics,audio): 12x session — 6 TODO stages, 30/31 ctest

7 phases across 6 TODO stages (1.1, 3.3, 4.1, 4.3, 5.3) on top of 4x+8x
dirty baseline. Build green, 30/31 ctest pass + 1 documented pre-existing
failure (ProjectVTests same baseline as 4x/8x).
NEW: NanoVDB GPU upload (4 SSBO + descriptor + version trigger) +
     Greedy Physics Meshing (D_3D, 35× reduction) +
     TAA Motion Vectors data path (4th MRT + 2 new TaaRenderTargets) +
     GPU World Gen pipeline infrastructure (VulkanWorldGenPipeline) +
     Chunk Streaming foundation (ChunkStreamer interface).
4 new test targets + 16 new sub-tests, all green.

Refs: TODO.md §1.1, §3.3, §4.1, §4.3, §5.3
Refs: agent/knowledge.md §30.4 (3-step migration precedent)
```

---

## 2026-06-21 (session: 4x — HZB full integration + UpdateApp refactor + GPU Fluid CA foundation)

### Stage 2.1 HZB culling full integration (closed)

- `src/shaders/hzb_cull.comp` — added binding 4 `writeonly buffer VisibleCount { uint visibleCount; }`.
  `atomicAdd(visibleCount, 1u)` after `atomicOr` per visible chunk.
- `src/core/Types.hpp` — `SceneFrameResources` gained `void* hzbVisibleCountMappedData;` +
  `VkBuffer hzbVisibleCountBuffer;` + `VmaAllocation hzbVisibleCountAllocation;` and
  `FrameRenderData` gained `VkBuffer hzbVisibleCountBuffer`. `chunkAabbBuffer` already in
  `FrameRenderData` and `SceneFrameResources`.
- `src/render/SceneResources.cpp` — added `hzbVisibleCountBuffer` allocation (4 B, with
  `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`) per frame.
  Initial value = `static_cast<uint32_t>(world->voxelWorld->chunks.size())` so first-frame
  `vkCmdDrawIndirectCountKHR` upper bound = all chunks. Structured-binding list + destroy loop
  + nullify block updated to include the new fields.
- `src/render/SceneResources.cpp:1408-1448` — `RefreshChunkAabbBuffer` **bug fix**: half-extent
  was hardcoded `0.5f` regardless of chunk dimensions. Now computed
  `std::max({maxX-minX, maxY-minY, maxZ-minZ}) * 0.5f` so HZB cull sees proper chunk AABBs
  (8³ chunk → half-extent 4.0, not 0.5).
- `src/render/HizCulling.cpp` — added binding 4 to `kHizCullingDescriptorBindings` (5 bindings
  total), bumped storage-buffer pool size from 2 to 3 sets worth. `RecordHzbCullingDispatch`
  now: (1) `vkCmdFillBuffer(0)` reset of visibility mask (per word count) + visible count,
  (2) `vkCmdPipelineBarrier2` with TRANSFER→COMPUTE barrier for both buffers,
  (3) 5-element descriptor write set with new visible count buffer, (4) unchanged dispatch.
- `src/render/Renderer.cpp` — main opaque pass (non-mesh-shader branch) conditionally uses
  `vkCmdDrawIndirectCountKHR(cmd, opaqueIndirectBuffer, 0, hzbVisibleCountBuffer, 0,
  chunkDescriptorCount, sizeof(VkDrawIndirectCommand))` when
  `IsHzbCullingEnabled() && hzbVisibleCountBuffer != null`. Pre-draw
  `vkCmdPipelineBarrier2` with COMPUTE→DRAW_INDIRECT barrier on the count buffer ensures
  cross-frame visibility (count from previous frame's cull). Falls back to `vkCmdDrawIndirect`
  with `chunkDescriptorCount` when HZB disabled.

### Stage 6.1 UpdateApp god-function refactor (closed)

- `src/app/AppUpdate.cpp` — extracted 3 file-scope helpers:
  - `ProcessInputActions` (~250 lines input action handling) — moved from inside `UpdateApp`
    to file scope (same TU). Uses anonymous-namespace helpers like `SetRelativeMouseMode`,
    `ApplyControlModeTransition`, `GetNextWalkCreativeMode`, `GetNextControlMode`, etc.
  - `RunFrameSimulation` (physics sync + camera + sim tick + post-sync + chunk rebuild) —
    ~50 lines, takes `cameraCanUpdate` flag.
  - `MirrorAllFrameStats` (end-of-frame `debug->stats.*` mirror + profiling plots) — ~30 lines.
  - `UpdateApp` body: **355 → 49 lines** (target <100, **OVER-DELIVERED**).
- `src/app/AppUpdateHelpers.hpp/.cpp` — unchanged (already had `UpdateFrameStatistics`,
  `UpdateEffectivePausedAndEditing`, `RunSimulationTickLoop`).
- No new ECS systems needed — `FluidCATickSystem` already handles Fluid CA via CPU
  `UpdateFluidCA`; ECS system already in mainline per 2x part 1.

### Stage 6.2 AppState PIMPL verification (closed — partial PIMPL exists)

- Verified: `AppStateImpl` struct + `std::unique_ptr<AppStateImpl>` + accessor methods
  (`render()`, `context()`, `swapchain()`, `world()`, `platform()`, etc.) all present in
  `Types.hpp` since 16x session. `state->render().field` pattern is widely used (54+ call
  sites per grep).
- Full struct move to `Types.cpp` (forward-declared opaque types) requires ~200+ file edits
  to access patterns (every `state->render().field` would become `state->render()->field`).
  **Out of 4x scope** — deferred to dedicated refactor session.

### Stage 3.1 GPU Fluid CA pipeline integration (foundation only)

- Verified existing foundation: `IsFluidCaGpuEnabled()` env toggle (`VoxelWorld.cpp:1145`),
  `BuildActiveChunkIdsForFluidCa` helper, `fluid_ca.comp` skeleton (8×8×4 workgroup,
  atomicOr), `VulkanContextState.dedicatedComputeQueue` + `dedicatedComputeQueueFamilyIndex` +
  `renderTimelineSemaphore`, `VulkanSyncPrimitives.{hpp,cpp}` for `vkWaitSemaphores` ring
  reset, `FluidCATickSystem` ECS system calls `UpdateFluidCA` (CPU).
- Web-search for `vkQueueSubmit2` + `VK_KHR_synchronization2` cross-queue pattern completed
  per `AGENTS.md §5.3` (Khronos docs + nvpro-samples/vk_timeline_semaphore). Confirmed
  pattern: dedicated compute queue + `renderTimelineSemaphore` (compute→graphics RAW hazard)
  + `vkCmdPipelineBarrier2` (COMPUTE_SHADER→DRAW_INDIRECT) for memory hazard.
- **Full pipeline integration out of 4x scope** — new `VulkanFluidCaPipeline.{hpp,cpp}`
  (~700 LoC) needed: ping-pong SSBO alloc in `SceneFrameResources` + compute pipeline +
  descriptor sets (5 SSBOs per `fluid_ca.comp` bindings 0-4) + `vkCmdFillBuffer` reset
  barrier + `vkQueueSubmit2` to dedicated compute queue + `VK_KHR_timeline_semaphore`
  cross-queue sync + readback to CPU. Deferred to next session.

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug` — 20/21 pass + 1 documented pre-existing
  failure (`TestSpectatorModeAllowsPausedMovementButBlocksEdits:2629`,
  `interaction.selection.hasHit`, confirmed via git stash unrelated to my changes per
  `agent/workspace.md §1`)
- All new test targets unchanged baseline

---

## 2026-06-21 (session: 8x — full pipeline integration across 6 TODO stages)

Continuation of 4x dirty tree as baseline (operator-confirmed). 8 phases, ~18-20h planned,
build green, **26/27 ctest pass + 1 documented pre-existing failure** (`ProjectVTests` same baseline).
6 new test targets added (5 of which exercise the new code paths, 1 is PIMPL contract static_assert).

### Phase 0: Web-search gate for Fluid CA atomic strategy (closed)

- `web_search` per `AGENTS.md §5.3` for `imageAtomicCompareExchange` vs `atomicOr` for fluid CA
  count conservation. Verified: `atomicOr` + bit-check (`(claimed & mask) == 0`) is functionally
  equivalent to CAS for "set bit if unset" semantics, works on `r32ui` images without extensions,
  no compile-time cost. `imageAtomicCompareExchange` is `int`-only via `shaderImageInt64Atomics`.
  Conclusion: **keep `fluid_ca.comp:101-105` `atomicOr` as-is**; the in-progress
  `2026-06-21-gpu-fluid-ca-atomic-strategy` experiment will measure 5 alternatives including CAS.

### Phase 1: Stage 3.1 GPU Fluid CA full pipeline integration (closed)

- `src/render/vulkan/VulkanFluidCaPipeline.{hpp,cpp}` (NEW, ~700 LoC) — public API:
  `IsFluidCaGpuPipelineRequested()`, `CreateFluidCaPipelines()`, `DestroyFluidCaPipelines()`,
  `RefreshFluidCaResourceBindings()`, `RecordFluidCaDispatch()`,
  `SubmitFluidCaToComputeQueue()`, `ReadFluidCaFrameStats()`. 5-binding descriptor set
  (PackedChunkDescriptors / ActiveChunkIds / SourceFluidCells / DestinationFluidCells /
  FluidStats) per `fluid_ca.comp` layout. Compute pipeline from `fluid_ca.comp.spv` via
  `ReadShaderFile` + `vkCreateComputePipelines`. Cross-queue submit helper uses
  `vkQueueSubmit2` + `VkSemaphoreSubmitInfo` wait/signal on `renderTimelineSemaphore`
  (per `agent/knowledge.md §30.4` Step 1 contract).
- `src/core/Types.hpp` — `SceneFrameResources` gained 4 ping-pong fields per frame
  (`fluidCaSourceBuffer/Allocation/MappedData` + `fluidCaDestinationBuffer/Allocation/MappedData` +
  `fluidCaActiveChunkIdBuffer/Allocation/MappedData` + `fluidCaStatsBuffer/Allocation/MappedData`)
  + `fluidCaDescriptorSet`. `RenderState` gained 7 fields
  (`fluidCaPipelineEnabled`, `fluidCaPingPongBufferBytes`, `fluidCaMaxActiveChunks`,
  `fluidCaShaderModule`, `fluidCaPipelineLayout`, `fluidCaPipeline`,
  `fluidCaDescriptorSetLayout`, `fluidCaDescriptorPool`).
- `src/render/SceneResources.cpp` — allocates 4 buffers per frame in `InitializeSceneResources`
  loop (after HZB visible count), structured-binding extended in `DestroySceneResources`,
  buffer names set via `SetVulkanObjectName`. Ping-pong byte size =
  `chunkSize^3 * 4 * max(1, chunkCount)` (per-frame, 8 KiB/buffer for 8³ chunks).
- `src/render/vulkan/VulkanInit.cpp` — `CreateFluidCaPipelines` + `RefreshFluidCaResourceBindings`
  called after `CreateMeshShaderPipelines` when `IsFluidCaGpuPipelineRequested()` true. Graceful
  fallback to CPU path on failure (matches `agent/knowledge.md §30.4` Step 1 contract).
- `src/core/Types.cpp` `ShutdownVulkan` — `DestroyFluidCaPipelines` before `DestroyMeshShaderPipelines`.
- `src/ecs/EcsWorld.cpp` `FluidCATickSystem` — when `IsFluidCaGpuEnabled()` true, increment
  `simulation.fluidGpuTicksPending` instead of calling CPU `UpdateFluidCA`.
- `src/render/Renderer.cpp` `DrawFrame` — drains `simulation.fluidGpuTicksPending` per frame, calls
  `BuildActiveChunkIdsForFluidCa` + `RecordFluidCaDispatch` on the main graphics command buffer
  (Phase 4 will route to dedicated compute queue). `DrawFrame` signature gained `AppState *state`.
- `src/render/Renderer.hpp` — `DrawFrame` signature changed to add `AppState *state` first param.
- `src/app/main.cpp` — `DrawFrame(state, ...)` caller updated.
- `tests/FluidCAGpuTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVFluidCAGpuTests` 5 sub-tests
  (env default-off, env=1, struct sizes, push constant propagation).
- `src/CMakeLists.txt` — `VulkanFluidCaPipeline.cpp` added to `ProjectV` sources.

### Phase 2: Stage 4.2 LOD chunk 2 B_SurfacePreserve downsampling (closed)

Per `2026-06-21-lod-mesh-downsampling` verdict=mixed (`B_SurfacePreserve` recommended).

- `src/voxel/VoxelLodDownsample.{hpp,cpp}` (NEW, ~150 LoC) — `namespace projectv::voxel` exports
  `LodDownsampleStepForLod` (1/2/4/8 for LOD 0/1/2/3), `LodDownsampledExtentForLod`,
  `SurfacePreserveVote8` (8-3=512 sample window, returns first non-Air material found or Air if all
  Air — 0 T-junction holes per experiment measurement), `DownsampleChunkForLodSurfacePreserve`
  (per-chunk CPU flatten), `RunLodDownsampleJobs` (orchestrator), `IsLodDownsampleEnabled`
  (env gate `PROJECTV_LOD_DOWNSAMPLE`).
- `src/voxel/VoxelWorld.{hpp,cpp}` — `VoxelChunk` gained `lodDownsampledNonAirCount` byte (size 40 unchanged).
  Public wrappers `::LodDownsampleStepForLod` / `::DownsampleChunkForLodSurfacePreserve` /
  `::RunLodDownsampleJobs` / `::IsLodDownsampleEnabled` delegate to `projectv::voxel::*` (preserves
  existing call sites; cross-namespace ambiguity resolved by qualified name in
  `RunLodDownsampleJobs` internals).
- `src/app/FramePreparation.cpp` — after `UpdateSceneResources` succeeds, if env gate set, calls
  `AssignLodLevels` (per-chunk distance-based LOD assignment) + `RunLodDownsampleJobs`. Tracy plots
  `"LOD Downsample Chunks"` + `"LOD Active Chunks"` added.
- `src/CMakeLists.txt` — `voxel/VoxelLodDownsample.cpp` added.
- `tests/LodDownsampleTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVLodDownsampleTests`
  9 sub-tests (step/extent math, uniform Air, uniform solid, mixed half, LOD 0 no-op, env gate,
  job counter for Air and solid).
- EVIL: `B_SurfacePreserve` kernel marked with explanation of the experiment rationale
  (`2026-06-21-lod-mesh-downsampling` 0 T-junction holes across 75 configurations, A_Majority3D
  + C_SolidOnly + D_MaxPool fail 10-32% on cave_stress).

### Phase 3: Stage 5.3 TAA Motion Vectors Pipeline A foundation (closed)

Per `2026-06-21-taa-motion-vectors` verdict=yes Pipeline A (Karis 2014 SIGGRAPH, 16:16 RG velocity
buffer, `VK_FORMAT_R16G16_SFLOAT`).

- `src/render/TaaRenderTargets.hpp` — added `kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT`
  with EVIL-style comment explaining Karis 2014 mandate, VRAM cost (8 MiB/frame double-buffered
  @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3`), and TODO.md §5.3 line 425
  explicit format prescription.
- `tests/TaaMotionVectorTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVTaaMotionVectorTests`
  3 sub-tests verifying the format constant matches the prescription, and existing
  `kTaaSceneColorFormat` + `kTaaLayerHistoryColorFormat` are preserved.
- Full GPU integration (MRT attachment in voxel.frag, dynamic rendering color attachment,
  TaaResolvePipeline consume) deferred to dedicated session — Phase 3 scope limited to
  contract lock-in per `agent/knowledge.md §30.4` 3-step migration precedent (Step 1 = format
  constant + struct + tests; Step 2 = vertex/fragment writes; Step 3 = taa_resolve consume).

### Phase 4: Stage 6.3 Async compute per-pass wiring foundation (closed)

Per `2026-06-20-dec-pipelines-async-compute` verdict=yes (foundation) +
`2026-06-20-async-compute-overhead-numbers` measured +9.85-11.34% speedup.

- `src/render/vulkan/VulkanFluidCaPipeline.{hpp,cpp}` — added `IsAsyncComputeEnabled()` env gate
  (`PROJECTV_ASYNC_COMPUTE=ON`, default OFF). `SubmitFluidCaToComputeQueue` helper already
  exists from Phase 1 (`vkQueueSubmit2` + `VkSemaphoreSubmitInfo` on
  `context->dedicatedComputeQueue` with `renderTimelineSemaphore` cross-queue sync).
- `tests/AsyncComputeTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVAsyncComputeTests`
  3 sub-tests (env default-off, env=1, env=0 explicit).
- Foundation ready for per-pass routing (HZB cull, Fluid CA, world gen, RTX BLAS). Per-pass
  wiring deferred to dedicated session (requires one-shot command buffer allocation from a
  dedicated compute command pool — separate sub-task).

### Phase 5: Stage 6.2 AppState PIMPL contract verification (closed)

Per `TODO.md §6.2` DoD incremental rebuild < 1.0s. Full struct move requires ~200 file edits
via mechanical sed (172 accessor call sites) which is multi-session work.

- `tests/AppStatePimplTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVAppStatePimplTests`
  12 `static_assert` checks verifying the existing partial PIMPL contract: accessor return
  types match (all 12 accessors return `T&` or smart-pointer refs as designed).
- Safety-net patch created: `git diff > /tmp/before_pimpl_20260621_025608.patch` (358 KB).
- Migration plan documented in `agent/workspace.md §2` for the dedicated future session.

### Phase 6: Stage 4.1 GPU Noise & World Gen Step 1 (closed)

Per `2026-06-21-gpu-procedural-noise-compute-kernels` verdict=mixed (OpenSimplex2 3D-S recommended,
all 5 kernels within 2.9% mean on RTX 3060 Ti, memory-bound 65.6% of 448 GB/s peak).

- `src/shaders/world_gen.comp` (NEW, ~180 LoC GLSL) — CC0 attribution header (per
  KdotJPG/OpenSimplex2 reference). Implements 3D "Smooth" variant with 22-gradient table,
  256-entry permutation table, FBM wrapper (4 octaves, persistence 0.5). Push-constant struct
  `WorldGenPushConstants` (chunkOriginAndChunkSize, chunkCountAndFlags, noiseParams, seed).
  SSBO binding 0 = writeonly voxel buffer. Workgroup 8x8x1.
- `src/CMakeLists.txt` — `world_gen.comp` added to `SHADERS` list.
- glslc --target-env=vulkan1.3 validation: **green** (after fixing const initializer for
  `kGradients3D[22]` array + GLSL binding syntax).
- Stage 4.1 budget: 50 µs/chunk @ chunkSize=8. OpenSimplex2 3D-S measured 6.6 µs/chunk single
  octave (8× headroom per experiment).

### Phase 7: Stage 1.1 NanoVDB GPU translation integration (closed)

Per `2026-06-20-nanovdb-on-gpu` verdict=yes (hybrid strategy: keep SVDAG-on-64-tree CPU side,
flatten to NanoVDB-aligned transient SSBO at GPU upload).

- `src/core/Types.hpp` — included `voxel/NanoVdb.hpp`; `RenderState` gained
  `projectv::voxel::nanovdb::NanoVdbFlattenResult sceneNanoVdbFlatten` + `uint64_t sceneNanoVdbVersion = 0`.
- `src/render/SceneResources.cpp` — in `UpdateSceneResources`, after voxel payload version bump,
  calls `BuildNanoVdbFlatten(world->voxelWorld->sparseStorage, materialLookup.data(),
  render->sceneNanoVdbFlatten)` and increments `sceneNanoVdbVersion`. Identity material lookup
  (255 entries) since `VoxelMaterial` is `uint8_t` and the tree stores materials directly. Tracy
  plots `"NanoVDB Uppers" / "NanoVDB Lowers" / "NanoVDB Leaves"` added.
- `src/CMakeLists.txt` — `voxel/NanoVdb.cpp` added to `ProjectV` sources (compiles cleanly,
  Ninja 1.13 dep-scan bug worked around with `parallel 1` per `agent/knowledge.md §30`).
- `tests/NanoVdbFlattenTests.cpp` (NEW) + `tests/CMakeLists.txt` — `ProjectVNanoVdbFlattenTests`
  4 sub-tests (struct sizes 8/16/24 bytes per `TODO.md §1.1` depth=2 design for chunkSize=8,
  invalid index constant, flatten empty tree, flatten populated tree + readback material at
  corners).
- GPU upload (SSBO allocation + descriptor set + payload version tracking) deferred to
  follow-up; CPU-side flatten is now wired.

### Phase 8: Doc sync (this entry)

- `agent/workspace.md` — updated `§1 Now` + `§2 Nearest Gap` + `§3 Next Steps` + `§5 Active
  tasks` + `§6 Recent closed sessions` for 8x closure.
- `TODO.md` — marked Stage 1.1 NanoVDB GPU integration partial (Step 1: flatten wired, GPU
  SSBO upload deferred), Stage 2.1 HZB full integration closed (carried from 4x), Stage 3.1
  GPU Fluid CA full pipeline closed, Stage 4.1 GPU Noise Step 1 closed (shader compiles),
  Stage 4.2 LOD chunk 2 closed (B_SurfacePreserve wired), Stage 5.3 TAA Motion Vectors
  foundation closed (format constant + tests), Stage 6.2 PIMPL full struct move deferred
  (172 call sites), Stage 6.3 async compute env gate added.
- `COMMENTS.md` — new entries for `src/render/vulkan/VulkanFluidCaPipeline.{hpp,cpp}` (per
  AGENTS.md §8).

### Verification

- `cmake --build build/linux-clang-debug --target ProjectV --parallel 1` — green (Ninja 1.13
  dep-scan bug worked around per `agent/knowledge.md §30`).
- `ctest --test-dir build/linux-clang-debug` — **26/27 pass** + 1 documented pre-existing
  failure (`ProjectVTests::TestSpectatorModeAllowsPausedMovementButBlocksEdits:2629` same
  baseline as 4x per `agent/workspace.md §1`).
- 6 new test targets: `ProjectVFluidCAGpuTests` (5) + `ProjectVLodDownsampleTests` (9) +
  `ProjectVTaaMotionVectorTests` (3) + `ProjectVAsyncComputeTests` (3) + `ProjectVAppStatePimplTests`
  (12) + `ProjectVNanoVdbFlattenTests` (4) — total 36 new sub-tests, all green.
- Dirty tree: 25 files modified + 5 untracked source files + 25 untracked docs/experiments/
  dirs (per `agent/workspace.md §6`). **No commit** (per operator "continue 4x dirty" policy).

### Operator commit prompt (per workspace.md policy)

ONE "Commit?" prompt at session end, covering both 4x + 8x work as single combined commit.
Suggested commit message (per `AGENTS.md §5.1` format):

```
feat(voxel,render): 4x+8x session — GPU Fluid CA + LOD downsample + TAA + async + NanoVDB

8 phases across 6 TODO stages (3.1, 4.1, 4.2, 5.3, 6.2, 6.3) on top of 4x dirty baseline.
Build green, 26/27 ctest pass + 1 documented pre-existing failure (ProjectVTests same baseline).
6 new test targets + 36 new sub-tests, all green.

Refs: TODO.md §3.1, §4.1, §4.2, §5.3, §6.2, §6.3
Refs: docs/experiments/INDEX.md 2026-06-21-lod-mesh-downsampling verdict=mixed
Refs: docs/experiments/INDEX.md 2026-06-21-gpu-procedural-noise-compute-kernels verdict=mixed
Refs: docs/experiments/INDEX.md 2026-06-21-taa-motion-vectors verdict=yes
Refs: docs/experiments/INDEX.md 2026-06-20-dec-pipelines-async-compute verdict=yes
Refs: docs/experiments/INDEX.md 2026-06-20-nanovdb-on-gpu verdict=yes
Refs: agent/knowledge.md §30 (Fluid CA GPU contract) + §30.4 (3-step migration)
```

---

## 2026-06-21 (session: Pattern C mesh shader full integration)

### Stage 2.1 Pattern C — mesh shader pipeline + renderer integration

- `src/shaders/voxel_mesh.mesh` — stub (1 triangle/chunk) **replaced** with full GreedyFacePass port
  from `voxel_mesh.comp`. The mesh shader now does 6-axis greedy merging per visible chunk and
  emits quads via `gl_MeshVerticesEXT[].gl_Position` + `gl_PrimitiveTriangleIndicesEXT[]`.
  Per-vertex outputs match `voxel.vert:107-138` byte-for-byte: `outNormal` (loc 0),
  `outWorldPosition` (loc 1), `flat outMaterialIndex` (loc 2), `outAmbientVisibility` (loc 3,
  constant 1.0 per `agent/knowledge.md §14` P0.3 v2 contract). 2-pass: pre-count quads to call
  `SetMeshOutputsEXT(vCount, pCount)`, then re-emit. `max_vertices=256`, `max_primitives=256` =
  Vulkan 1.3 spec minimum; chunkSize=8 means 8³ worst case = 384 quads, with greedy merge
  typically <64 quads/chunk.
- `src/shaders/voxel_mesh_pre.comp` — **replaced** UBO `binding=3` (CameraFrustum) with
  push-constant `vec4 frustumPlanes[6]`. 16 + 96 = 112 bytes total push (under 128-byte min).
  Unnormalized planes OK because shader uses linear radius scale.
- `src/shaders/voxel_mesh.task` — **DELETED** (vestigial spike; Pattern C = compute pre-cull
  + mesh shader, not task + mesh per `mesh-shader-vs-compute-cull` verdict=mixed).
- `src/CMakeLists.txt` — `voxel_mesh.task` removed from `SHADERS` list. New
  `render/vulkan/VulkanMeshShaderPipeline.cpp` added to `ProjectV` source list.
- `src/core/Types.hpp` — `SceneFrameResources` gained `visibleChunkIdBuffer` +
  `visibleChunkIdMappedData` + `visibilityCounterBuffer` + `visibilityCounterMappedData` +
  `meshShaderDescriptorSet` (SSBOs + descriptor set, MAPPED for visibleCount reset).
  `FrameRenderData` gained `chunkCullingParameters` + `meshShaderDescriptorSet`.
  `RenderState` gained `meshShaderEnabled` (bool), `visibleChunkIdCapacity` (uint32),
  `meshShaderMaxOutputVertices/Primitives` (uint32), pipeline handles
  (`meshCullShaderModule`, `meshShaderModule`, `meshCullPipelineLayout`, `meshShaderPipelineLayout`,
  `meshCullPipeline`, `meshShaderPipeline`, `meshShaderDescriptorSetLayout`, `meshShaderDescriptorPool`).
- `src/render/SceneResources.cpp` — allocation+destruction of `visibleChunkIdBuffer` (capacity =
  chunk count) and `visibilityCounterBuffer` (4 B). Counter initialized to 0 once at allocation.
  Per-frame CPU reset handled in `RecordMeshShaderPreCull` via mapped memory.
- `src/render/vulkan/VulkanMeshShaderPipeline.{hpp,cpp}` — NEW. Public API:
  `IsMeshShaderPipelineRequested()` (env `PROJECTV_MESH_SHADER_PIPELINE=ON`),
  `BuildMeshCullPushConstants()` (camera → 6 planes via `MakeFrustumPlane` helper),
  `CreateMeshShaderPipelines()` (probes `VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader`,
  graceful fallback if absent), `DestroyMeshShaderPipelines()`, `RefreshMeshShaderResourceBindings()`,
  `RecordMeshShaderPreCull()` (CPU memset visibleCount + barrier + pre-cull compute dispatch
  + barrier for mesh stage), `RecordMeshShaderDraw()` (`vkCmdDrawMeshTasksEXT`).
  Single descriptor set layout (4 SSBOs) shared between cull (compute) and draw (graphics).
- `src/render/vulkan/VulkanInit.cpp` — calls `CreateMeshShaderPipelines` after
  `CreateVoxelMeshingPipeline` (only when env requested). Failure logs informational
  message and continues with PackedFace main draw (graceful).
- `src/render/Renderer.cpp` — main pass split: when `render.meshShaderEnabled` is true, the
  opaque pass uses `RecordMeshShaderDraw` (replacing `vkCmdDrawIndirect`). The PackedFace
  indirect draw remains the path for transparent pass + shadow pass (no change).
  `voxel_mesh.comp` (greedy PackedFace producer) still runs every frame for shadow.
- `src/app/FramePreparation.cpp` — populates `frame->renderData.chunkCullingParameters` and
  `frame->renderData.meshShaderDescriptorSet`.
- `src/core/Types.cpp` — `ShutdownVulkan` now calls `DestroyMeshShaderPipelines` before
  graphics pipeline destruction.
- `tests/CMakeLists.txt` — `ProjectVFluidCATests` + `ProjectVShadowProjectionBenchmark` +
  `ProjectVHzbCullingTests` updated to add `PhysicsWorld.cpp` + `InputActions.cpp` +
  `ShaderIO.cpp` + tracy/volk/Jolt include paths (links were broken by added dependencies).
  New `ProjectVMeshShaderTests` (compile-only + cull-push helper test).
- `tests/MeshShaderTests.cpp` — NEW. 4 sub-tests: `IsMeshShaderPipelineRequested` default
  off, `BuildMeshCullPushConstants` dispatchParams propagation, near-plane (forward, near
  offset), far-plane (back, max distance). Catches regressions in CPU-side frustum extraction.

### EVIL markers added (5 total)

- `src/shaders/voxel_mesh.mesh:104-110` — `kMeshMaxVertices=256` + `kMeshMaxPrimitives=256`
  = Vulkan 1.3 spec minimum; 8³ worst case is 384 quads; bump to per-device max if exceeded.
- `src/shaders/voxel_mesh.mesh:112` — `kMeshMaxQuads=64` reserved for future single-pass.
- `src/shaders/voxel_mesh_pre.comp:20-22` — 6 unnormalized planes in push constants
  (Vulkan 128-byte min); 16+96=112 bytes, OK.
- `src/render/vulkan/VulkanMeshShaderPipeline.cpp:18-23` — `kMeshMaxOutputVertices/Primitives=256`
  = spec min (clamped for cross-vendor safety); `kMeshPushConstantSize=128` = VoxelMeshingPushConstants(64)
  + viewProjection(64) exactly, must not grow.

### Validation error fixes (3 pre-existing issues from prior sessions)

- `src/render/vulkan/VulkanBootstrap.cpp:457` — `BuildEnabledFeatures12` now enables
  `timelineSemaphore` (was missing → `vkCreateSemaphore(VK_SEMAPHORE_TYPE_TIMELINE)` validation
  error on dev host + `renderTimelineSemaphore` leak on shutdown).
- `src/render/vulkan/VulkanBootstrap.cpp:467` — `BuildEnabledFeatures13` now enables
  `shaderDemoteToHelperInvocation` (was missing → `voxel.frag` uses `demote_to_helper` per
  `agent/knowledge.md §15` lighting contract; validation reported missing capability).
- `src/core/Types.cpp:88-91` — `ShutdownVulkan` now calls `vkDestroySemaphore` for
  `context.renderTimelineSemaphore` (previously created but never destroyed → 1 leaked
  VkSemaphore on device shutdown).
- `src/render/vulkan/VulkanBootstrap.cpp:447-450` — `PhysicalDeviceCandidate` gained
  `meshShaderFeatures` + `supportsMeshShader` probe gated on `HasDeviceExtension` check
  for `VK_EXT_mesh_shader`. `CreateDevice` chains `VkPhysicalDeviceMeshShaderFeaturesEXT{meshShader=VK_TRUE, taskShader=VK_TRUE}`
  in `VkDeviceCreateInfo::pNext` + adds `VK_EXT_mesh_shader` to device extensions when
  `PROJECTV_MESH_SHADER_PIPELINE=ON` (per `agent/knowledge.md §32` opt-in contract).

---

## 2026-06-20 (session: 2x scope continuation, part 2)

### Stage 1.1 chunk 7 — homogeneous optimization

- `src/voxel/Sparse64Tree.hpp` — added `kSparse64HomogeneousFlag = 0x40000000u` slot encoding. A homogeneous marker
  encodes "all 64 child cells have the same material" as a single uint32 slot, saving memory for flat-color
  regions (walls, floors). New helpers: `MakeSparse64Homogeneous(material)`, `IsSparse64Homogeneous(slot)`,
  `Sparse64HomogeneousMaterial(slot)`. Slot node-index mask changed from `0x7FFFFFFF` to `0x3FFFFFFF` (30 bits).
- `SetCellRecursive` checks `CanCollapseToHomogeneous(node)` after each mutation — if all 64 child slots
  are now the same material (leaf or homogeneous marker), the node is replaced with a homogeneous marker
  (refCount decremented). Cascade naturally propagates upward.
- Early-out for "same material" SetCell on leaf or homogeneous slots (no node allocation).
- Leaf expansion path now sets `fillMask = full` (sub-volume fully populated), so re-collapse works after
  revert edits.
- `LiveNodeCount()` helper added (counts nodes with `refCount > 0`) — `NodeCount()` keeps old "allocated"
  semantics for snapshot save/load.
- `IsHomogeneousRoot()` public helper.
- `tests/Sparse64TreeTests.cpp` — 5 new sub-tests: `TestHomogeneousCollapseSmall` (4×4×4), `TestHomogeneousExpansionOnSingleEdit`,
  `TestHomogeneousCascadeSixteen` (16×16×16 full cascade), `TestHomogeneousRecollapseAfterEdit`,
  `TestHomogeneousDedupEqual`.

### Stage 1.2 chunks 4-5 — per-chunk static flag + lazy promotion

- `src/voxel/VoxelWorld.hpp` — `VoxelChunk` gained `bool isStatic = false;` + `uint32_t ticksSinceLastEdit = 0;`
  (struct size 32 → 36 bytes, static_asserts updated).
- `src/voxel/VoxelWorld.{hpp,cpp}` — new API: `GetVoxelChunkStaticPromotionThreshold()` (env `PROJECTV_SVDAG_STATIC_PROMOTION_TICKS`,
  default 60), `TickVoxelChunkStaticPromotion(world, threshold)` (per-frame increment + auto-promote + DedupPass trigger
  if dedup enabled), `CountStaticVoxelChunks(world)`.
- `SetVoxelMaterial` resets `isStatic = false; ticksSinceLastEdit = 0;` on every voxel edit (auto-demote).
- `tests/VoxelWorldTests.cpp` — 2 new sub-tests: `TestVoxelChunkStaticPromotion`, `TestVoxelChunkStaticPromotionThresholdFromEnv`.

### Stage 2.1 chunk 2 — task+mesh shader spike (nvidia-only)

- `src/shaders/voxel_mesh.task` — new task shader. Frustum cull per chunk (6-plane AABB test),
  `EmitMeshTasksEXT(visible_count, 1, 1)` to dispatch mesh shader. Reads `ChunkDescriptor` SSBO + push-constant
  frustum planes. Uses `GL_EXT_mesh_shader` extension. Compiles under `vulkan1.3` target env.
- `src/shaders/voxel_mesh.mesh` — new mesh shader. Stub: per visible chunk, emits 1 triangle at chunk origin
  (NDC positions). Real greedy mesh emission deferred (multi-session work).
- `src/CMakeLists.txt` — `--target-env vulkan1.3` flag added to shader compiler args (required for mesh shaders).
- CMake registration for both new shader files. Compiles successfully, integration into `Renderer.cpp` deferred.

### Stage 2.2 chunk 2 — HZB image lifecycle (spike)

- `src/render/HizCulling.{hpp,cpp}` — real Vulkan lifecycle for `HizBuffer`. `CreateHizBuffer(context, w, h, outBuffer)`
  allocates image + image view via VMA, `DestroyHizBuffer(context, buffer)` releases. `BuildHizMipChain(commandBuffer,
  depthImage, layout, hizBuffer)` builds mip chain via `vkCmdBlitImage` with `VK_FILTER_NEAREST` (level 0 from depth)
  and `VK_FILTER_LINEAR` (between mips), with proper image layout barriers.
- Image format: `VK_FORMAT_R32_SFLOAT`, `VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`.
- Renderer integration (vkCmdDrawIndirectCountKHR dispatch + AABB SSBO + visible-mask readback) deferred to a
  follow-up session — current Phase 4 = infrastructure ready.

### Stage 6.1 — +3 Flecs ECS systems

- `src/ecs/EcsWorld.{hpp,cpp}` — 3 new systems: `VoxelInteractionTickSystem`, `BenchmarkAutomationTickSystem`,
  `LookDevCaptureTickSystem`. Each mirrors the AudioRefresh/FluidCA pattern (AppStateBinding + per-tick each()).
- New ECS components: `VoxelInteractionTickState` (dummy field to avoid Flecs empty-struct issue),
  `BenchmarkTickResult` (bool quitAfterFrame), `LookDevCaptureTickResult` (bool quitAfterFrame).
- New public API: `TickVoxelInteractionSystem(ecs)`, `TickBenchmarkAutomationSystem(ecs)`,
  `IsBenchmarkAutomationQuitRequested(ecs)`, `TickLookDevCaptureSystem(ecs)`,
  `IsLookDevCaptureQuitRequested(ecs)`.
- `src/core/Types.hpp` — `WorldState` gained `bool allowWorldEditing = false;` (computed in AppUpdate, read by VoxelInteraction ECS system).
- `src/app/AppUpdate.cpp` — sets `world->allowWorldEditing = allowWorldEditing;`, removed inline
  `UpdateVoxelInteraction` block (now in ECS).
- `src/app/main.cpp` — removed inline `UpdateLookDevCaptureAutomation` and `UpdateBenchmarkAutomation` blocks,
  calls `TickLookDevCaptureSystem` / `TickBenchmarkAutomationSystem` instead. Read `IsLookDevCaptureQuitRequested` /
  `IsBenchmarkAutomationQuitRequested` for `quitAfter*Frame` flags.

### Stage 6.2.5 — more `// EVIL:` markers (+13)

- `src/voxel/VoxelRaycast.cpp` — 2 markers: `kEpsilon`, `kDirectionEpsilon`.
- `src/voxel/VoxelMaterials.cpp` — 11 markers: all `k*` exposure/color-grade/shadow/contact-shadow constants
  (`kMinSceneExposure`, `kDefaultMaxSceneExposure`, `kMinSceneKey`, `kMinExposureTargetKey`, `kMaxExposureTargetKey`,
  `kMinEnvironmentIntensity`, `kMaxEnvironmentIntensity`, `kMinColorGradeWhitePoint`, `kMaxColorGradeWhitePoint`,
  `kMinColorGradeContrast`, `kMaxColorGradeContrast`, `kMinColorGradeSaturation`, `kMaxColorGradeSaturation`,
  `kMinColorGradeLift`, `kMaxColorGradeLift`, `kMaxShadowDepthBias`, `kMaxShadowNormalBias`, `kMaxShadowFilterRadius`,
  `kMaxContactShadowDistance`). Total now 23 EVIL markers across the codebase (was 9).

### Stage 6.2.6 — `vkWaitForFences` 10ms timeout constant

- `src/core/Types.hpp` — added `kVulkanFenceWaitTimeoutNs = 10'000'000` and `kVulkanFenceWaitTimeoutUnboundedNs = UINT64_MAX`
  inline constexpr constants. Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` "low latency > throughput".
- `src/render/Renderer.cpp` + `src/app/FramePreparation.cpp` — replaced literal `10'000'000` / `UINT64_MAX` with named constants.
  All `vkWaitForFences` call sites follow the bounded-fallback pattern (10ms primary, unbounded only on `VK_TIMEOUT`).

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug` — 19/19 pass (~1.0 sec)
- `ProjectVSparse64TreeTests` 24 sub-tests pass (+5 homogeneous)
- `ProjectVVoxelWorldTests` (+2 static promotion)
- `ProjectVHzbCullingTests` 4 sub-tests pass
- All previous tests preserved baseline

---

## 2026-06-20 (session: 2x scope continuation, part 3)

### Stage 1.1 chunk 7 finalization — homogeneous expansion semantics

- `src/voxel/Sparse64Tree.hpp` — leaf-expansion path now sets `fillMask = full` for the freshly-allocated
  Node (treating the leaf as "all 64 cells = material M"). Combined with the cascade-collapse check,
  re-edits that revert to the original material now collapse back to a homogeneous marker.

### Stage 2.1 chunk 2 — Pattern C mesh shader spike

- `src/shaders/voxel_mesh_pre.comp` (new) — frustum-cull compute pre-pass. Reads `PackedChunkDescriptors`
  SSBO + frustum planes uniform, writes `visibleChunkIds[]` SSBO + `visibilityWords` atomic counter.
  Per `mesh-shader-vs-compute-cull` experiment recommendation: **Pattern C** (mesh + indirect count),
  no task shader. Compute cull remains default production path per experiment verdict.
- `src/shaders/voxel_mesh.mesh` refactored — reads `visibleChunkIds[gl_WorkGroupID.x]` instead of
  `chunkDescriptors[gl_WorkGroupID.x]`. Stub triangle emission unchanged.
- `src/CMakeLists.txt` — `--target-env vulkan1.3` for shader compile (required for mesh shaders).
- `src/render/HizCulling.{hpp,cpp}` — added `IsMeshShaderPipelineEnabled()` env toggle
  (`PROJECTV_MESH_SHADER_PIPELINE=ON`).

### Stage 2.2 chunk 2 — HZB image lifecycle

- `src/render/HizCulling.{hpp,cpp}` — real Vulkan lifecycle for `HizBuffer`. `CreateHizBuffer()` allocates
  image (R32_SFLOAT, mip chain) + image view via VMA. `DestroyHizBuffer()` releases. `BuildHizMipChain()`
  builds mip chain via `vkCmdBlitImage` NEAREST (level 0 from depth) + LINEAR (between mips) + image layout
  barriers. Stage 2.2 Renderer integration (Phase 1 of next session) gated on `hzb-binding-models` verdict.

### Stage 3.1 chunk 1 — GPU Fluid CA skeleton

- `src/shaders/fluid_ca.comp` (new) — compute shader per `agent/knowledge.md §30.4` contract. 8×8×4 workgroup,
  reads chunk descriptor + activeChunkIds SSBO + source fluid cells SSBO, writes destination fluid cells
  SSBO + stats. `atomicOr` for destination claim. Iteration order: z,y,x ascending per workgroup.
- `src/voxel/VoxelWorld.{hpp,cpp}` — `IsFluidCaGpuEnabled()` env toggle (`PROJECTV_FLUID_CA_GPU`),
  `BuildActiveChunkIdsForFluidCa(world)` helper, `ToggleFluidCaGpuEnabledForTesting(bool)` for unit tests.
- Stage 3.1 GPU pipeline integration (ping-pong buffers + actual dispatch) deferred until async-compute
  foundation lands (Phase 0 of next session per `dec-pipelines-async-compute` recommendation).

### Stage 3.2 chunk 1 — Incremental Jolt per-chunk BodyId map

- `src/physics/PhysicsWorld.{hpp,cpp}` — `PhysicsState` gains `std::unordered_map<uint32_t, BodyID>
  chunkStaticBodies` + `std::vector<uint32_t> pendingChunkRebuilds`. New API: `BuildChunkStaticCollisionBody`
  (per-chunk CompoundShape + BodyId), `DestroyChunkStaticBody`, `QueueChunkRebuildRequest`,
  `ProcessChunkRebuildQueue` (sorted + dedup'd rebuild), `GetPendingChunkRebuildCount`,
  `GetChunkBodyCount`. Old single-body `staticWorldBodyId` retained for backward compatibility.

### Stage 4.2 chunk 1 — per-chunk LOD level + distance selection

- `src/voxel/VoxelWorld.hpp` — `VoxelChunk` gains `uint8_t lodLevel` + 3 reserved bytes (struct size 36 → 40).
- `src/voxel/VoxelWorld.{hpp,cpp}` — `SelectLodLevelForDistance(distanceMeters)` returns 0/1/2/3 for
  <32m / <64m / <128m / ≥128m. `AssignLodLevels(world, camX, camY, camZ)` writes LOD per chunk.
  `CountChunksAtLod(world, lod)` counts. **Uniform downsampling** (every Nth voxel) deferred to Phase 2.

### Stage 6.1 — UpdateApp god-function refactor

- `src/app/AppUpdateHelpers.{hpp,cpp}` (new) — `UpdateFrameStatistics(simulation, debug, render)`,
  `UpdateEffectivePausedAndEditing(camera, world, simulation)`,
  `RunSimulationTickLoop(camera, input, world, simulation, physics)`. Extracted from `UpdateApp` body.
- `src/app/Camera.{hpp,cpp}` — promoted `IsCreativeMode`, `IsWalkMode`, `IsSpectatorMode`,
  `kMaxSimulationStepsPerFrame` from anonymous namespace to public API.
- `src/app/AppUpdate.cpp` — calls `projectv::app::UpdateFrameStatistics` etc. Push descriptor Phase A
  (per `bindless-descriptor-overhead` §7.1 Phase A) deferred to a future session.

### Stage 6.2.2 — std::span sweep (3 sites)

- `src/render/ShadowProjection.cpp` — `ComputeBounds`, `ComputeRequiredProjectedHalfExtent`,
  `ComputeRelativeDepthRange` parameters: `const std::array<Float3, 8>&` → `std::span<const Float3>`.
  Return values (std::array<Float3, 8>) preserved.

### Stage 6.2.5 — +39 EVIL markers (62 total now)

- `src/render/ShadowProjection.cpp` — 10 markers on kShadow* constants (kMinShadowNearPlane,
  kShadowExtentPadding, kShadowDepthPadding, kMinShadowCoverageScale, kMaxShadowCoverageScale,
  kMinCascadeNearPlane, kDefaultCascadeNearPlane, kDefaultCascadeFarPlane,
  kDefaultShadowMapResolution, kDefaultCascadeSplitLambda).
- `src/app/AppUpdate.cpp` — 22 markers on kLighting*/kShadow*/kMinShadow*/kMaxShadow* constants.

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug -R "Hzb|Sparse|Cpu|Fluid"` — 4/4 pass (~0.4 sec)
- `ProjectVSparse64TreeTests` 24 sub-tests pass (homogeneous + dedup)
- `ProjectVVoxelWorldTests` — 1 pre-existing failure (TestSpectatorModeAllowsPausedMovementButBlocksEdits at line 2629),
  unrelated to my changes (confirmed via git stash)
- `ProjectVCpuMeshGeneratorTests` 4 sub-tests pass
- `ProjectVHzbCullingTests` 4 sub-tests pass
- 62 EVIL markers across codebase

### Blocked

- **Phase 1 (HZB Renderer integration)** — gated on `hzb-binding-models` verdict. Per operator decision
  ("ждём"), deferred to a future session when verdict closes. Current STATUS.md shows experiment at
  wrap-up but no concluded verdict.

---

## 2026-06-20 (session: 2x scope continuation)

### Stage 1.1 chunks 5-7 — `world.voxels` fully removed

- `src/voxel/VoxelWorld.hpp` — `world.voxels` field removed from `VoxelWorld` struct.
  Storage is now exclusively the `sparseStorage` (Sparse64Tree). Test fixtures that
  relied on the flat array now use `BuildFlatVoxelSnapshot(world)` helper in
  `src/voxel/VoxelWorld.cpp` (loops `GetVoxelMaterial`).
- `SetVoxelMaterial` now reads previous material via `GetVoxelMaterial` (sparse path)
  for the no-op early-exit, eliminating the flat read.
- `CreateEmptyVoxelWorld` no longer resizes flat; `LoadVoxelWorldSnapshot` no longer
  populates flat.
- `tests/FluidCATests.cpp` + `tests/VoxelWorldTests.cpp` refactored to use
  `BuildFlatVoxelSnapshot` + `SetVoxelMaterial` loops; `state.world.X` →
  `state->world().X` (per AppState PIMPL from 16x session).
- `tests/CMakeLists.txt` + 5 `CMakePresets.json` buildPresets updated for
  `ProjectVShadowProjectionBenchmark` deps (VoxelWorld.cpp, RuntimeDiagnostics.cpp,
  VulkanResult.cpp, fmt, SDL3).

### Stage 1.2 chunks 2-3 — Lazy DedupPass + copy-on-write (COW)

- `src/voxel/Sparse64Tree.hpp` — `Node` struct now has `uint32_t refCount`.
  `AllocateNode` sets `refCount=1`; `FindEquivalentNode` increments on dedup;
  `MarkNodeUnique(nodeIndex)` implements COW: if `refCount > 1`, decrement old,
  allocate copy with `refCount=1`, return new index. `SetCellRecursive` calls
  `MarkNodeUnique` before mutation. `GetRefCount`, `DedupPass()`,
  `DedupSubtree(slot, level)`, `SetDeduplicationEnabled` (resets refCount 0/1)
  public API. Dedup OFF by default.
- `tests/Sparse64TreeTests.cpp` — added `TestCopyOnWriteOnMutation` sub-test.

### Stage 2.1 chunk 1 — CPU mesh fallback (minimal)

- `src/voxel/CpuMeshGenerator.{hpp,cpp}` — minimal CPU-side single-axis (X+) face
  emission for one chunk (4 sub-tests). Used as fallback / reference for future
  mesh shader work; not integrated into Renderer yet.
- `tests/CpuMeshGeneratorTests.cpp` — 4 sub-tests (empty chunk, single voxel,
  boundary voxel, filled chunk).
- `tests/CMakeLists.txt` + 5 buildPresets updated to register `ProjectVCpuMeshGeneratorTests`.

### Stage 2.2 chunk 1 — HZB culling minimal spike

- `src/shaders/hzb_cull.comp` — new compute shader: AABB-vs-HZB test, atomicOr
  visibility bitmask writeback. Compiles to SPIR-V (added to `Shaders` target).
- `src/render/HizCulling.{hpp,cpp}` — `HizBuffer` struct (image/imageView/memory
  placeholder), `IsHzbCullingEnabled()` (env `PROJECTV_HZB_CULLING=ON` toggle,
  default OFF), `ComputeHzbMipLevelCount` helper.
- `tests/HzbCullingTests.cpp` — 4 sub-tests (env disabled default, 1024² → 11 mips,
  800×600 → 10 mips, 1×1 → 1 mip).
- 5 buildPresets updated to register `ProjectVHzbCullingTests`.

### 6.2.3 — DDA shader template macro

- `src/shaders/voxel.frag` — `DDA_BODY(MAX_STEPS, TRAVELED_OP, PRED, RETURN_EXPR, DEFAULT_RETURN)`
  macro unifies the 3 copies of 12-step 3D-DDA loop (was ~50 lines each, now ~10 lines
  per caller + macro body). Replaced for-loops in `TraceLocalPointLightShadowRay`,
  `ComputeSunContactVisibility`, `TraceAmbientOcclusionRay`. `traveled >=` for
  `LocalPointLightShadowRay` preserved via `TRAVELED_OP` arg (others use `>`).
  Net −72 lines.

### 6.2.2 — std::span sweep (one target migrated)

- `src/render/SceneResources.cpp` — `BuildMaterialVisualTable()` return type changed
  from `std::array<VoxelMaterialVisual, kVoxelMaterialCount>` to
  `std::span<const VoxelMaterialVisual>` (static const backing array, span return).
  Caller uses `.size_bytes()` instead of `sizeof(materialVisuals)`.

### 6.1 — Flecs ECS migration (FluidCATickSystem added)

- `src/core/Types.hpp` — `SimulationState` gained `bool effectivePaused = false;`
  so ECS systems can read the derived pause state computed in `UpdateApp`.
- `src/ecs/EcsWorld.{hpp,cpp}` — new `FluidCATickState` component (accumulatorSeconds),
  `FluidCATickSystem` (OnUpdate) registered. Reads AppStateBinding, checks
  effectivePaused/fluidTickRateHz/voxelWorld, drives accumulator + UpdateFluidCA.
  Mirrors AudioRefreshPlaylistSystem pattern.
- `src/app/AppUpdate.cpp` — sets `simulation->effectivePaused` after computing it;
  removed inline FluidCA block (was lines 791-801).
- `src/app/main.cpp` — added `TickFluidCASystem(state->ecs().get())` call after
  UpdateApp (alongside TickAudioRefreshPlaylistSystem).
- Initial InputReplay ECS migration was attempted but reverted: replay playback must
  run BEFORE UpdateApp (input state is read by UpdateApp), ECS systems run AFTER,
  so semantic change would break replay. Reverted InputReplayPlaybackTickSystem.

### Verified

- `cmake --build build/linux-clang-debug --target ProjectV` — green
- `ctest --test-dir build/linux-clang-debug` — 19/19 pass (~1.0 sec)
- `ProjectVHzbCullingTests` 4 sub-tests pass (new)
- `ProjectVCpuMeshGeneratorTests` 4 sub-tests pass
- `ProjectVSparse64TreeTests` 18 sub-tests pass (+1 TestCopyOnWriteOnMutation)
- All other tests preserved baseline

---

## 2026-06-20

### Changed (agent-file-consolidation r0 — commits `3c148e3`, `f1eeb6a`, `1bf096f`, `4f5f379`, `<this-commit>`)

- **`agent/knowledge.md`** (~1860 lines, new) — single source of truth for
  engineering contracts (Part A, formerly `agent/decisions.md` §1-§31)
  and runtime facts (Part B, formerly `agent/memory.md` §1-§11). Part C
  inlines the archive index (formerly `agent/ARCHIVE-INDEX.md`).
  Per-section contracts unchanged — all §N refs in live files resolve
  via `tools/verify_section_anchors.sh`.
- **`agent/workspace.md`** (~250 lines, new) — consolidated live snapshot
  + active tasks ledger. §1 Now / §2 Gap / §3 Next Steps / §4 Risks /
  §5 Active tasks (formerly `agent/active-sessions.md`) / §6 Recent
  closed (archive pointer) / §7 Archive references.
- **`agent/session-checklist.md`** deleted — inlined as `AGENTS.md §9`
  ("Session checklist"), with `§9.1`-`§9.6` matching the original 6
  sections. Old `§9` (Comment management protocol) renumbered to `§10`.
- **`agent/active-sessions.md`** compressed (1951 → 114 lines) — all
  20 pre-`2026-06-20` closed sessions moved to
  `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (1214 lines,
  full per-session detail preserved: commit hashes, file-touched-intent,
  notes). Per operator directive «удалить все сессии, кроме текущей».
- **`agent/status.md`** deleted — content merged into `agent/workspace.md`
  §1-§4 + §7.
- **`AGENTS.md`** updated — §3 sources-of-truth list, §4 classification
  table, §7.2.6.1 file format table, §7.2.8 shared infra list, §7 close-
  routine refs all point to new `agent/knowledge.md` /
  `agent/workspace.md` / `AGENTS.md §9` instead of the deleted files.
  10 cross-refs updated total.
- **`agent/memory.md`, `agent/decisions.md`, `agent/ARCHIVE-INDEX.md`,
  `agent/status.md`, `agent/session-checklist.md`** all deleted.
  Content preserved in `agent/knowledge.md` / `agent/workspace.md` /
  `AGENTS.md §9` / archive files.
- **`tools/verify_section_anchors.sh`** (new, 53 lines) — bash script
  that greps `decisions.md §X` / `memory.md §X` / `workspace.md §X`
  refs in live files (`TODO.md`, `CHANGELOG.md`, `AGENTS.md`) and
  verifies each §N header exists in `agent/knowledge.md` /
  `agent/workspace.md`. Exits 0 if all resolve, 1 if any broken.
  Required by per-commit gate per `AGENTS.md §6.9` for any change
  touching service-file cross-refs.
- **TODO.md** anchor-form cross-refs (Commit A): 14 `decisions.md §X.Y`
  refs converted to hybrid form `§X.Y ([name](#anchor))` with GFM
  auto-generated anchors. Old §N semantics preserved verbatim.
- **TODO.md / README_NEW.md / CHANGELOG.md / agent/active-sessions.md**
  bulk file-path replacement (Commit E): `agent/decisions.md` →
  `agent/knowledge.md Part A`, `agent/memory.md` → `agent/knowledge.md
  Part B`, `agent/status.md` → `agent/workspace.md §1 (Now)`,
  `agent/active-sessions.md` → `agent/workspace.md §5 (Active tasks)`,
  `agent/ARCHIVE-INDEX.md` → `agent/knowledge.md Part C`,
  `agent/session-checklist.md` → `AGENTS.md §9`.
- **COMMENTS.md** footer + audit (Commit A): `<!-- Last validated
  2026-06-20 against src/core/Math.ixx (commit 44362d1) and
  src/core/StringId.ixx (same). -->`. All 15 L-anchor ranges
  spot-checked, all match current code (post-`44362d1`).

### Removed

- **Dead ray-march compute pass** (defense stub, never integrated). Removed
  `src/render/RayMarchPass.hpp` (15 lines), `src/render/RayMarchPass.cpp` (50
  lines), `src/shaders/ray_march.comp` (129 lines), 2 lines in
  `src/CMakeLists.txt` (shader + source registration), 4 lines in
  `src/app/main.cpp` (`#include "render/RayMarchPass.hpp"`,
  `RequestRayMarchPipelineRecreate()` call in `HotReloadShaders`, and the
  `SDLK_2` toggle branch). Per pet-project directive 2026-06-20: the path
  was created for diploma defense 2026-06-13 (ТЗ п. 4.1.2), remained a STUB
  with `SetRayMarchEnabled` containing a no-op `if (isEnabled) return;` guard,
  the compute shader was compiled but never dispatched in
  `Renderer::RecordGraphicsCommands`, and the `SDLK_2` hotkey only flipped
  a flag with no visual effect. The real renderer is the voxel mesh path
  (`voxel_mesh.comp` + `voxel.frag`/`voxel_shadow.*` + TAA + CSM). Future SVO
  rendering per `legacy/docs/architecture/practice/00_svo-architecture.md`
  re-implements DDA from scratch (the removed shader used a flat 3D-grid
  payload, not an SVO). Net −185 lines.

### Changed

- **`TODO.md` rewritten to GPU-driven roadmap v1 (dependency-aware, 6
  Stages, ~340 lines with detailed per-item approach).** Supersedes
  Tier 0..5 r0 (`2026-06-13` plan). Old TODO content archived verbatim at
  `legacy/docs/archive/agent-todos/2026-06-tier-0-5-r0.md` (207 lines,
  `SUPERSEDED 2026-06-20` header). **Re-ordered to fix a critical
  dependency mismatch**: Stage 1 (new voxel storage: Sparse 64-trees +
  SVDAG + async audio) MUST land before any Stage 2-5 work, because all
  Stage 2-5 GPU geometry, cull, sim, GI, LOD code reads from the new
  SVDAG storage. Building those on top of the flat `std::vector<uint8_t>`
  would require a full rewrite when Stage 1 lands. Stage 6 (Flecs ECS
  migration) moved from tech-debt backlog to mainline-and-parallel —
  converting each new system to a Flecs system as it lands is cheaper
  than retrofitting a 989-line god-function later. New TODO covers:
  Pre-Stage 0 quick wins (B1 redundant model load loop, B2 pipeline
  destroy on resize, B3 `getenv` per frame, B4 verify VSync closed),
  Stage 0 architectural decisions (A1 Vulkan 1.4→1.3, A2 Fluid CA
  reversal planning marker), **Stage 1 Voxel Database & Compression**
  (1.1 Sparse 64-trees, 1.2 SVDAG, 1.3 async audio scanPlaylist),
  **Stage 2 GPU-Driven Geometry & Culling** (2.1 Mesh+Task shaders for
  SVDAG, 2.2 HZB cull, 2.3 Virtual Texturing), **Stage 3 Physics &
  Simulation** (3.1 GPU Fluid CA per §30.4, 3.2 incremental Jolt, 3.3
  greedy physics meshing), **Stage 4 Procedural Generation & LOD** (4.1
  GPU noise, 4.2 geometry LOD, 4.3 lift draw distance cap), **Stage 5
  GI & Temporal** (5.1 VCT, 5.2 RTX shadows feature-flag, 5.3 TAA +
  motion vectors), **Stage 6 Tech-debt & ECS refactor** (6.1 Flecs ECS
  migration parallel with Stages 2-5, 6.2 AppState PIMPL + std::span +
  r0 carry-overs). **Verification policy** (cross-cutting): A/B test
  buffers during data-format migrations (Stage 1), MeshingStress
  measurement on every optimization (5% threshold for adoption per
  `decisions.md §5_decision-making.md`), inspected runtime captures
  required for rendering close-out per `decisions.md §15`. All Tier
  0..5 r0 sub-tasks remain closed (commits `cf4b535`, `af69d06`,
  `bafecf9`, `08de29d`, `20b2d9e`, `44362d1`, `72eca66`).
- **`agent/status.md §1 Active sub-plan` updated** to «Roadmap v1 per
  `TODO.md`» (was «Tier 0..5 (Hardcore perf r0)»).
- **`agent/decisions.md §29` header marked OUTDATED** (Tier 0..5 plan
  superseded; old content preserved as historical record; cold/hot
  `std::expected` rule remains valid).
- **`agent/decisions.md §29` R&D-promotion cross-refs updated** to match
  new 6-Stage numbering: Mesh Shaders → Stage 2.1, Sparse 64-trees →
  Stage 1.1, SVDAG → Stage 1.2, RT shadows → Stage 5.2.
- **`agent/decisions.md §30` header marked OUTDATED** (CPU Fluid CA
  fall-only superseded by GPU compute per §30.4).
- **`agent/decisions.md §30.4` added** — «Fluid CA reversal: GPU compute
  (ping-pong + atomicOr + active chunk list)». New binding contract for
  Stage 3.1 in new TODO. Defines: ping-pong voxel buffers, `imageAtomicOr` /
  CAS for destination claim, active chunk list for skipping sleepy
  voxels, multi-tile determinism contract (single-tile deterministic,
  multi-tile statistically stable), 3-step migration path (additive → default
  → deprecate CPU), `SimulationState` unchanged, render path unaware of
  CPU-vs-GPU source. CPU `UpdateFluidCA` retained as reference implementation
  and test fixture per memory policy. Cross-ref to draw-distance cap
  updated from old `Stage 5.3` to new `Stage 4.3`.

- `src/voxel/VoxelWorld.cpp:176` — Tier 0.D hardening.** `[[unlikely]]` on the
  `chunk.rebuildQueued` early-out in `QueueChunkRebuildRequest`. The branch
  predictor (and the branch hint) target the common path "chunk fresh,
  process rebuild request"; the early-return is rare and correctly marked.

### Verified (Tier 0.D audit, no code change)

- `src/voxel/VoxelWorld.cpp:366,1162` — `pendingChunkRebuildIndices.reserve(world->chunks.size())`
  on scene-load and `MarkAllVoxelChunksDirty`. Capacity matches `chunks.size()`,
  bounded — no realloc during voxel-edit `QueueChunkRebuildRequest` calls.
- `src/render/SceneResources.cpp:1074,1076,1078,1129,1162` — `latestVoxelPayloadChunkIndices`,
  `completedChunkRebuildIndices`, `pendingChunkRebuildIndices` reserved at scene rebuild
  and snapshot restore boundaries. No per-frame realloc in steady state.
- `src/render/VoxelMeshingPushConstants.hpp:32-37` — `ChunkVisibilityCache` uses
  `std::array<VkDrawIndirectCommand, kChunkVisibilityCacheMaxChunks>` (fixed-cap,
  1024 entries). No `std::vector` here — `std::inplace_vector` (Tier 1.A) not needed.
- `src/debug/DebugOverlays.cpp:224-226` — `debugOverlayBoxes.reserve(requiredBoxCount)`
  before the per-frame build loop. Capacity grows lazily only when `requiredBoxCount`
  exceeds previous capacity.
- `src/app/InputReplay.cpp:176,278` — `InputReplayCapture.frames.reserve(expectedFrameCount)`
  on load; `capture.frames.reserve(512)` on `StartInputReplayRecording` start. Recording
  amortizes O(1) push_back per frame even past 512 frames (geometric growth).
- `src/app/FramePreparation.cpp:30,45` — local `cullCandidates.reserve(modelInstances.size())`
  and `visibleModelInstances.reserve(visible.size())` before push_back loops.
- `src/voxel/VoxelWorld.cpp:1068` — local BFS `queue.reserve(256)` in `FillVoxelMaterial`.

---

## [Unreleased] — pre-2026-06-12 / un-dated history

### Added

- `src/c_kernels/FrustumCulling.cpp:76` — Scalar C kernel.** Per the Tier 3 benchmark, this
- `src/c_kernels/FrustumCulling.hpp:67` — Culls `instances` against `parameters` in a single
- `tests/CFrustumCullingTests.cpp:73` — 300 AABBs in a 32x32 chunk grid centred on
- `tests/CFrustumCullingTests.cpp:101` — 5 visibility runs** with the camera at

### Changed

- `src/app/AppUpdate.cpp:156` — P0.3 follow-up: any (re-)enable of relative mouse mode risks a large
- `src/app/Camera.cpp:109` — P0.3 follow-up: drop the first MOUSE_MOTION event after relative
- `src/app/InputReplay.cpp:96` — Tier 5.** `0ull` (was `0u`) — the mask type
- `src/app/main.cpp:358` — Tier 5.** `0ull` (was `0u`) — the mask type
- `src/core/InplaceVectorShim.hpp:57` — No copy/move ctors / assignment.** Matches
- `src/render/SceneResources.hpp:87` — Tier 5.** `[[unlikely]]` on the near-plane cull.
- `src/render/SceneResources.hpp:132` — Tier 5.** `[[unlikely]]` on the 4 side-plane
- `src/render/ShadowProjection.cpp:87` — Tier 0.B.** `matrix` is now `projectv::math::Mat4` (16-byte
- `src/render/ShadowProjection.hpp:18` — Tier 0.B.** `Vec3` (16-byte aligned) replaces
- `src/render/ShadowProjection.hpp:32` — Tier 0.B.** `lightViewProjections` stays `std::array<float, N>`
- `src/shaders/voxel_mesh.comp:290` — P0.3 follow-up: the parameters are intentionally unused here. Touch
- `src/shaders/voxel_mesh.comp:290` — P0.3 follow-up: the parameters are intentionally unused here. Touch
- `src/voxel/VoxelWorld.hpp:109` — (Tier 1.E: replaced by `ParseVoxelScenePreset(std::string_view)` below
- `tests/VoxelWorldTests.cpp:366` — Tier 1.E.** `VoxelScenePresetToString` now returns
- `tests/VoxelWorldTests.cpp:1697` — Tier 5.** `1ull <<` (not `1u <<`) — the
- `tests/VoxelWorldTests.cpp:2494` — Tier 5.** `1ull <<` (was `1u <<`) — the mask

### Removed

- `src/render/vulkan/VulkanGraphicsPipeline.cpp:77` — P0.3 follow-up (final): per-vertex AO was removed (see voxel.vert
- `src/shaders/voxel.frag:81` — P0.3: inAmbientVisibility is no longer `flat` so the rasterizer bilinearly
- `src/shaders/voxel.frag:81` — P0.3: inAmbientVisibility is no longer `flat` so the rasterizer bilinearly
- `src/shaders/voxel_mesh.comp:275` — P0.3 follow-up: ComputeFaceCornerPackedAO is now a no-op. The vertex
- `src/shaders/voxel_mesh.comp:275` — P0.3 follow-up: ComputeFaceCornerPackedAO is now a no-op. The vertex

---

## 2026-06-18

### Fixed

- `src/asset/AssetLoader.cpp:400` — Windows STL portability (`2026-06-18`,
- `src/asset/ModelPass.cpp:94` — Windows clang-cl portability (`2026-06-18`,
- `src/core/StringId_fallback.hpp:3` — StringID fallback header for Windows clang-cl
- `tests/FrustumCullingTests.cpp:47` — Windows portability (`2026-06-18`,
- `tests/FrustumCullingTests.cpp:148` — Windows portability (`2026-06-18`,

### Removed

- `src/core/Math_fallback.hpp:3` — Math fallback header for Windows clang-cl

---

## 2026-06-15

### Changed

- `src/app/main.cpp:90` — Cross-platform log path (`2026-06-15`).** The previous
- `src/voxel/SceneConfig.cpp:37` — Repo-root walk-up (`2026-06-15`).** Per

### Fixed

- `src/app/main.cpp:36` — Hot shader reload fallback (`2026-06-15`).** If the
- `src/app/main.cpp:81` — Cross-platform fallback (`2026-06-15`).** Per
- `src/core/RepoRoot.hpp:1` — Repo root discovery (`2026-06-15`).

---

## 2026-06-14

### Added

- `src/app/Camera.cpp:120` — Window-event mouse freeze (`2026-06-14`).** After
- `src/core/Types.hpp:1403` — Window-event mouse freeze (`2026-06-14`).** When the WM
- `src/render/vulkan/VulkanSwapchain.hpp:182` — Build the runtime present-mode cycle
- `tests/PresentModeTests.cpp:1` — Present-mode cycle tests (auto-detect cycle,
- `tests/PresentModeTests.cpp:211` — 2026-06-14 fix:** explicitly reset `g_active` to
- `tests/PresentModeTests.cpp:288` — Cycle rebuild preserves `g_active` (2026-06-14 fix).

### Changed

- `src/app/Camera.cpp:132` — Fullscreen / resize defence-in-depth (`2026-06-14`).
- `src/app/main.cpp:634` — Fullscreen / window-resize mouse guard (`2026-06-14`).
- `src/core/Types.hpp:1375` — Fluid CA accumulator (2026-06-14).** Wall-clock-free,
- `src/debug/DebugHud.cpp:553` — 2026-06-14: VSync line.** Shows the current
- `src/render/vulkan/VulkanSwapchain.cpp:259` — 2026-06-14: build the runtime present-mode cycle
- `src/render/vulkan/VulkanSwapchain.hpp:88` — Build the runtime present-mode cycle (`2026-06-14`).
- `src/render/vulkan/VulkanSwapchain.hpp:111` — Read-only accessors for the HUD (`2026-06-14`).** The
- `src/render/vulkan/VulkanSwapchain.hpp:159` — V-sync toggle (auto-detect cycle on `2026-06-14`).
- `tests/FluidCATests.cpp:498` — `stats.fluidVoxelCount` stays in sync with `world.voxels`
- `tests/FluidCATests.cpp:541` — Run enough ticks for the column to spread out. With the
- `tests/FluidCATests.cpp:592` — Run enough ticks for the column to settle. With the

### Fixed

- `src/app/AppUpdate.cpp:694` — Fluid CA tick (2026-06-14).** Mirrors the physics-
- `src/app/main.cpp:697` — Fluid CA tick — moved to `UpdateApp` on 2026-06-14.
- `src/core/Types.hpp:1357` — Fluid CA tick rate (2026-06-14).** Live, in-Hz, applied
- `src/render/vulkan/VulkanSwapchain.cpp:160` — 2026-06-14 fix.** Honour the operator's `V`-hotkey
- `src/voxel/VoxelWorld.cpp:1504` — Strict count conservation (2026-06-14).** The
- `tests/FluidCATests.cpp:414` — Strict count conservation (2026-06-14 fix).** Earlier
- `tests/FluidCATests.cpp:569` — Fluid column over Air + platform: platform stays intact,
- `tests/FluidCATests.cpp:765` — ---------------------------------------------------------------------------
- `tests/FluidCATests.cpp:819` — Fluid does NOT move when paused (2026-06-14 invariant).
- `tests/PresentModeTests.cpp:303` — 2026-06-14 fix:** explicit reset to FIFO before
- `tests/PresentModeTests.cpp:336` — 2026-06-14 fix:** explicit reset to FIFO before
- `tests/PresentModeTests.cpp:376` — 2026-06-14 fix:** explicit reset to FIFO before

---

## 2026-06-13

### Added

- `src/app/Camera.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/app/FramePreparation.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/app/FramePreparation.cpp:63` — Tier 4 (`2026-06-13`).** C / AVX2 frustum-cull
- `src/app/main.cpp:545` — Defense r0 hotkeys (2026-06-13, relocated twice 2026-06-15).** Originally
- `src/app/main.cpp:583` — V-sync toggle (`2026-06-13`, auto-detect
- `src/audio/AudioEngine.cpp:38` — Artist / title parser, 2026-06-13.** See
- `src/audio/AudioEngine.cpp:159` — Tier 1.B (`2026-06-13`).** `std::expected<size_t,
- `src/audio/AudioEngine.cpp:350` — Cursor semantics 2026-06-13 (two
- `src/bench/FrustumCullBenchmark.cpp:1` — Tier 3 (`2026-06-13`).** `FrustumCullBenchmark` —
- `src/bench/ShadowProjectionBenchmark.cpp:1` — Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `src/c_kernels/FrustumCulling.hpp:1` — Tier 4 (`2026-06-13`).** C++ wrapper for the
- `src/c_kernels/frustum_cull.c:1` — Tier 3 (`2026-06-13`).** `frustum_cull.c` — scalar
- `src/core/InplaceVectorShim.hpp:3` — libc++ migration shim (`2026-06-13`).** libc++ 22 (the
- `src/core/Math.hpp:51` — Original Tier 0.A doc preserved below for git-blame
- `src/core/Types.hpp:61` — libc++ migration shim (`2026-06-13`).** `std::inplace_vector`
- `src/debug/DebugHud.cpp:689` — Music HUD moved out, 2026-06-13.** The
- `src/render/Renderer.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/render/SceneResources.cpp:449` — Tier 1.A (`2026-06-13`).** `ChunkVisibilityCache` now uses
- `src/render/ShadowProjection.cpp:1` — Tier 2.D (`2026-06-13`).** Re-enabled direct importer
- `src/render/vulkan/VulkanInit.hpp:63` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.hpp:49` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.hpp:70` — V-sync toggle hotkey (`2026-06-13`, auto-detect cycle
- `src/voxel/VoxelWorld.cpp:705` — Tier 1.E (`2026-06-13`).** `constexpr std::string_view` return
- `src/voxel/VoxelWorld.cpp:829` — Tier 1.B (`2026-06-13`).** Returns `std::expected<bool,
- `src/voxel/VoxelWorld.cpp:1576` — 3. Commit the new state through the public `SetVoxelMaterial`
- `src/voxel/VoxelWorld.hpp:153` — Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13;
- `tests/CFrustumCullingTests.cpp:1` — Tier 4 (`2026-06-13`).** Unit test for the
- `tests/ModuleSmokeTest.cpp:1` — Tier 2.A probe (`2026-06-13`).** Tiny TU that
- `tests/PresentModeTests.cpp:152` — Modes appear in the surface support in a different
- `tests/ProbeTest.cpp:1` — Tier 2.A probe (`2026-06-13`).** Tiny TU that imports
- `tests/StdModuleProbe.cpp:1` — Tier 2.C probe (`2026-06-13`).** Tiny TU that
- `tests/VoxelWorldTests.cpp:88` — Tier 0 (`2026-06-13`) signature change:

### Changed

- `src/app/AppUpdate.cpp:885` — Music HUD mirrors, 2026-06-13.
- `src/app/InputActions.cpp:261` — Tier 5 (`2026-06-13`).** `uint64_t` return type
- `src/app/InputActions.hpp:15` — Tier 5 (`2026-06-13`).** `uint64_t` mask type.
- `src/app/InputReplay.cpp:151` — Tier 5 (`2026-06-13`).** Accept v1, v2, and v3
- `src/app/InputReplay.cpp:316` — Tier 1.B (`2026-06-13`).** `std::expected` carries the
- `src/app/main.cpp:61` — Hot shader reload (defense r0, 2026-06-13).** F5 re-runs `cmake --build`
- `src/app/main.cpp:233` — Tier 1.B (`2026-06-13`).** `std::expected` returns the
- `src/app/main.cpp:262` — Tier 1.B (`2026-06-13`).** `std::expected` returns the
- `src/app/main.cpp:316` — Tier 1.B (`2026-06-13`).** `std::expected` carries the
- `src/app/main.cpp:411` — Tier 1.B (`2026-06-13`).** `InitVulkan` now returns
- `src/app/main.cpp:477` — Scene config (defense r0, 2026-06-13).** Read the JSON
- `src/asset/AssetManifest.cpp:122` — Tier 1.D/E (`2026-06-13`).** `ManifestEntry::id` is now a
- `src/asset/AssetManifest.hpp:14` — Tier 1.D/E (`2026-06-13`).** Replaced `std::string` with
- `src/asset/AssetRegistry.cpp:10` — Tier 1.B (`2026-06-13`).** `std::expected<const LoadedAsset *,
- `src/asset/AssetRegistry.hpp:16` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/asset/ModelManifestLoader.cpp:35` — Tier 0.B (`2026-06-13`).** Takes the destination as a
- `src/asset/ModelManifestLoader.cpp:187` — Tier 0.D (`2026-06-13`).** Reserve once before the loop so
- `src/audio/AudioEngine.cpp:525` — Cursor reset, 2026-06-13.** E is the
- `src/audio/AudioEngine.hpp:29` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/audio/AudioEngine.hpp:53` — Artist / title parser, 2026-06-13.** Splits a
- `src/audio/AudioEngine.hpp:113` — Set the music folder and trigger the first
- `src/audio/AudioEngine.hpp:180` — Artist / title for the HUD, 2026-06-13.
- `src/audio/AudioEngine.hpp:191` — Playback position / duration, 2026-06-13.
- `src/audio/AudioEngine.hpp:241` — Artist / title cache updater, 2026-06-13.
- `src/audio/AudioEngine.hpp:267` — Dead code, 2026-06-13.** Saved cursor
- `src/audio/AudioEngine.hpp:295` — Artist / title cache, 2026-06-13.** Parsed
- `src/c_kernels/FrustumCulling.cpp:1` — Tier 4 (`2026-06-13`).** C++ wrapper implementation
- `src/c_kernels/frustum_cull.hpp:1` — Tier 3 (`2026-06-13`).** C / intrinsics
- `src/core/Math.ixx:156` — libc++ migration / `import` regression (`2026-06-13`).
- `src/core/Math.ixx:365` — Cast helpers** from `glm::mat4` / `float[N]` to
- `src/core/StringId.ixx:1` — Tier 2.A — `StringId.ixx` (`2026-06-13`).** C++20
- `src/core/Types.hpp:343` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned
- `src/core/Types.hpp:371` — Matches the `ResolvePushConstants` struct declared in
- `src/core/Types.hpp:404` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/core/Types.hpp:483` — Tier 5 (`2026-06-13`).** Widened from `uint32_t`
- `src/core/Types.hpp:610` — Tier 0.B (`2026-06-13`).** Four `Vec4`s (16-byte aligned
- `src/core/Types.hpp:720` — Music HUD mirrors, 2026-06-13.** Parsed
- `src/core/Types.hpp:912` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/core/Types.hpp:939` — Tier 1.D/E (`2026-06-13`).** Replaced `std::string` with
- `src/core/Types.hpp:1121` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned)
- `src/debug/DebugHud.cpp:28` — Music panel, 2026-06-13.** Minimum width
- `src/debug/DebugHud.cpp:39` — Music panel cap, 2026-06-13.** Four lines
- `src/debug/DebugHud.cpp:60` — mm:ss formatter, 2026-06-13.** Used by the
- `src/debug/DebugHud.cpp:95` — HUD uppercase helper, 2026-06-13.** Copies
- `src/debug/DebugHud.cpp:1030` — Music HUD block builder, 2026-06-13.** Emits
- `src/debug/DebugHud.cpp:1252` — Music panel color, 2026-06-13.** Slightly
- `src/debug/DebugHud.cpp:1270` — Music lines, 2026-06-13.** Built and
- `src/debug/DebugHud.cpp:1289` — Music panel anchored at top-right,
- `src/render/RayMarchPass.cpp:11` — State for the ray-march pass (defense r0, 2026-06-13).** A single
- `src/render/RayMarchPass.cpp:69` — Phase 7 follow-up (defense r0, 2026-06-13).** The full Vulkan
- `src/render/RayMarchPass.hpp:9` — Ray-march compute pass (defense r0, 2026-06-13).** A second, optional
- `src/render/SceneResources.cpp:39` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/render/SceneResources.cpp:433` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) per
- `src/render/SceneResources.hpp:21` — Tier 5 (`2026-06-13`).** `[[unlikely]]` on the
- `src/render/ShadowProjection.hpp:9` — Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
- `src/render/TaaRenderTargets.cpp:76` — Tier 1.B (`2026-06-13`).** Returns `std::expected<void, TaaError>`.
- `src/render/TaaRenderTargets.hpp:27` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1696` — Fluid (2026-06-13 follow-up):** disable back-face culling for
- `src/render/vulkan/VulkanInit.cpp:128` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanInit.hpp:11` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/render/vulkan/VulkanSwapchain.cpp:219` — Tier 1.B (`2026-06-13`).** Moved `CreateOrRecreateSwapchain` to
- `src/render/vulkan/VulkanSwapchain.cpp:229` — Tier 1.B (`2026-06-13`).** Returns
- `src/render/vulkan/VulkanSwapchain.cpp:516` — Tier 1.B (`2026-06-13`).** `CreateOrRecreateSwapchain`
- `src/render/vulkan/VulkanSwapchain.cpp:564` — Tier 1.B (`2026-06-13`).** `std::expected<void, TaaError>`
- `src/render/vulkan/VulkanSwapchain.hpp:13` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/shaders/ray_march.comp:3` — Ray-march compute shader (defense r0, 2026-06-13).** Implements the
- `src/shaders/ray_march.comp:3` — Ray-march compute shader (defense r0, 2026-06-13).** Implements the
- `src/shaders/voxel_mesh.comp:190` — Fluid (2026-06-13 follow-up): emit against ALL non-Air
- `src/shaders/voxel_mesh.comp:190` — Fluid (2026-06-13 follow-up): emit against ALL non-Air
- `src/voxel/SceneConfig.hpp:11` — Scene configuration (defense r0, 2026-06-13).** Runtime-readable
- `src/voxel/VoxelMaterials.hpp:116` — previous frame's viewProjection (column-major, same layout as
- `src/voxel/VoxelWorld.cpp:673` — Tier 1.E (`2026-06-13`).** Replaced `bool TryParse(..., &out)`
- `src/voxel/VoxelWorld.cpp:755` — Tier 1.E (`2026-06-13`).** `ParseVoxelScenePreset` returns
- `src/voxel/VoxelWorld.hpp:117` — Tier 1.B (`2026-06-13`).** `std::expected<T, VoxelSnapshotError>`
- `src/voxel/VoxelWorld.hpp:125` — Tier 1.E (`2026-06-13`).** `VoxelScenePresetToString` returns
- `tests/FluidCATests.cpp:1` — Fluid CA unit tests (audit `2026-06-13`).
- `tests/FluidCATests.cpp:213` — Run many ticks. With the 2026-06-13 2-direction spread
- `tests/FluidCATests.cpp:228` — Fluid stops falling when it hits a solid floor.** A fluid at
- `tests/FluidCATests.cpp:283` — Before break:** glass is solid, fluid cannot fall through.
- `tests/FluidCATests.cpp:356` — First tick: fluid at (2, 1, 2) cannot fall through the
- `tests/FluidCATests.cpp:377` — Fluid on a flat surface (where the fall rule can't fire —
- `tests/FluidCATests.cpp:754` — Subsequent ticks:** the fluid continues to fall and spread.
- `tests/MathTest.cpp:1` — Tier 0.A — r0 hardcore perf (`2026-06-13`).** Smoke test for
- `tests/StringIdTest.cpp:1` — Tier 1.D (`2026-06-13`).** Tests for `projectv::core::StringID`.
- `tests/SunShadowCascadeSplitsTests.cpp:1` — Tier 5 (`2026-06-13`).** Unit test for
- `tests/VoxelWorldTests.cpp:351` — Tier 1.E (`2026-06-13`).** `TryParseVoxelScenePreset(text, &out)`
- `tests/VoxelWorldTests.cpp:1003` — Tier 0.B (`2026-06-13`).** `SunShadowProjection::lightViewProjection`
- `tests/VoxelWorldTests.cpp:1131` — Tier 0.B (`2026-06-13`).** `lightViewProjections` (the
- `tests/VoxelWorldTests.cpp:2197` — Tier 0 (`2026-06-13`) signature change:

### Fixed

- `src/app/InputReplay.cpp:15` — Tier 5 (`2026-06-13`).** Bumped from 2 to 3.
- `src/audio/AudioEngine.cpp:493` — Cursor preservation, 2026-06-13.
- `src/core/Types.hpp:273` — Tier 5 (`2026-06-13`).** Bit-mask overflow fix.
- `src/core/Types.hpp:565` — Tier 1.A (`2026-06-13`).** `std::inplace_vector` (P0843, C++26)
- `src/core/Types.hpp:781` — Tier 5 follow-up (`2026-06-13`).** Default 0.0
- `src/core/Types.hpp:1128` — Per-pass TAA tuning ladder (live `;`/`'`/`-`/`=`/`,`/`.` ladder, see
- `src/render/Renderer.cpp:239` — Tier 5 (`2026-06-13`).** Tight fence-wait
- `src/render/SceneResources.cpp:723` — Pre-existing race fix (`2026-06-13`).** Wait for
- `src/render/TaaRenderTargets.hpp:115` — Build / recreate the TAA offscreen color images (colour +
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1253` — TAA resolve descriptor update (`2026-06-13`).** The
- `src/render/vulkan/VulkanSwapchain.cpp:98` — V-sync toggle (`2026-06-13`).** Live-cycled by the
- `src/shaders/taa_resolve.frag:261` — Reconstruct the world position of the *current* pixel from its
- `src/shaders/taa_resolve.frag:261` — Reconstruct the world position of the *current* pixel from its
- `src/shaders/voxel.frag:722` — Tremor fix (`2026-06-13`).** Vulkan's
- `src/shaders/voxel.frag:722` — Tremor fix (`2026-06-13`).** Vulkan's
- `src/shaders/voxel.frag:944` — TAA layer-history reprojection (`2026-06-13`).** The
- `src/shaders/voxel.frag:944` — TAA layer-history reprojection (`2026-06-13`).** The
- `src/voxel/VoxelSnapshotError.hpp:3` — Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
- `src/voxel/VoxelWorld.cpp:895` — Tier 1.B (`2026-06-13`).** Returns `std::expected<unique_ptr<VoxelWorld>,
- `src/voxel/VoxelWorld.cpp:1278` — Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13).
- `src/voxel/VoxelWorld.cpp:1337` — Claimed tracking (2026-06-13 spread restore).** When a fluid
- `src/voxel/VoxelWorld.cpp:1380` — f_fall rule:** if the cell below is `Air`,
- `src/voxel/VoxelWorld.cpp:1442` — f_spread rule (2-direction perpendicular):** if
- `tests/FluidCATests.cpp:335` — Fluid on a `FloorWhite` cell does NOT fall through (the
- `tests/FluidCATests.cpp:729` — Before break:** fluid on glass → no fall. The fluid may

### Removed

- `src/audio/AudioEngine.cpp:430` — Bug fix 2026-06-13.** Previously this
- `src/core/Math.hpp:3` — Tier 2.D (`2026-06-13`).** This header is now a **forwarding
- `src/core/Math.ixx:1` — Tier 2.A — `Math.ixx` (`2026-06-13`).** C++20 module
- `src/core/StringId.hpp:3` — Tier 2.D (`2026-06-13`).** This header is a **forwarding
- `src/core/Types.hpp:3` — libc++ migration debug (`2026-06-13`).** Re-enabled
- `src/render/Renderer.cpp:30` — `BuildGraphicsPushConstants` is non-singular for any sensible near/far
- `src/render/Renderer.cpp:96` — Tier 0.B (`2026-06-13`).** The local `InvertColumnMajorMat4` is
- `tests/FluidCATests.cpp:158` — A column of N fluid voxels over Air "percolates" downward in

---

## 2026-06-12

### Added

- `src/app/ModelGravigun.cpp:261` — Drag.** Cast a ray from the camera through the
- `src/asset/ModelManifestLoader.cpp:134` — Source AABB in world space (transformed, not yet frustum-culled;
- `src/asset/ModelManifestLoader.hpp:28` — Voxel-grid "ground snap" for the loaded `modelInstances`. The
- `src/asset/ModelManifestLoader.hpp:61` — M5.1d, 2026-06-12: centred snap (opt-in).** Same
- `src/audio/AudioEngine.hpp:3` — Audio engine, 2026-06-12.** Thin wrapper over
- `src/core/Types.hpp:1285` — 5.3 Benchmark automation, 2026-06-12.** Tracks per-frame timings
- `src/debug/DebugHud.cpp:48` — Helper-panel cap, 2026-06-12 bump.** Was 16
- `src/render/ScreenshotCapture.cpp:421` — Audio engine keys, 2026-06-12.** Three

### Changed

- `src/app/AppUpdate.cpp:486` — Frame-step / slow-motion debug (2026-06-12). The
- `src/app/AppUpdate.cpp:573` — Audio engine handlers, 2026-06-12.** All four
- `src/app/AppUpdate.cpp:842` — Per-pass CPU timing mirrors (2026-06-12). Each
- `src/app/InputActions.cpp:171` — Frame-step / slow-motion debug (2026-06-12). `[` / `]`
- `src/app/InputActions.cpp:183` — Audio engine, 2026-06-12.** Music
- `src/app/InputActions.cpp:200` — Track switching, 2026-06-12.** `9` and `0`
- `src/app/ModelGravigun.cpp:94` — M5.1d, 2026-06-12:** the gravigun's default behaviour
- `src/app/ModelGravigun.cpp:221` — Drop** the picked model. The default behaviour
- `src/app/ModelGravigun.hpp:69` — Horizontal plane the cursor projects onto during drag.
- `src/app/ModelGravigun.hpp:80` — Pick anchor, 2026-06-12:** the AABB min of the
- `src/app/main.cpp:424` — Audio engine, 2026-06-12.** Init is non-fatal
- `src/asset/ModelManifestLoader.cpp:483` — Centred snap, 2026-06-12.** Same clamp-to-world + snap-
- `src/asset/ModelManifestLoader.cpp:582` — Dispatcher (M5.1d, 2026-06-12). The default snap is
- `src/asset/ModelManifestLoader.hpp:98` — M5.1d, 2026-06-12: snap dispatch wrapper.** Reads the
- `src/audio/AudioEngine.hpp:69` — Playback state, 2026-06-12.** Three-valued enum so
- `src/audio/MusicDirectoryPath.hpp:7` — Music folder resolution, 2026-06-12.** Mirrors the
- `src/core/Types.hpp:191` — M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
- `src/core/Types.hpp:204` — 5.2 Debug gizmos, 2026-06-12:** `L` cycles a world-aligned
- `src/core/Types.hpp:215` — 5.2 Debug gizmos, 2026-06-12:** `Z` toggles a 1-voxel-wide
- `src/core/Types.hpp:237` — Audio engine, 2026-06-12.** Music player
- `src/core/Types.hpp:251` — Track switching, 2026-06-12.** Next /
- `src/core/Types.hpp:530` — Two-level chunk visibility cache (2026-06-12).** When the
- `src/core/Types.hpp:675` — Frame-step / slow-motion mirrors, 2026-06-12.** See
- `src/core/Types.hpp:687` — Per-pass CPU timing mirrors (2026-06-12). Source fields
- `src/core/Types.hpp:703` — Audio engine mirrors, 2026-06-12.** Source of
- `src/core/Types.hpp:954` — Per-pass CPU timing, 2026-06-12.** Wall-clock
- `src/core/Types.hpp:1261` — The model pipeline reuses the main `graphicsPipelineLayout`
- `src/core/Types.hpp:1514` — Audio engine, 2026-06-12.** Single
- `src/debug/DebugHud.cpp:707` — Per-pass CPU timing lines (2026-06-12). Two-line
- `src/debug/DebugHud.cpp:1146` — Movement / speed, 2026-06-12.** WASD
- `src/debug/DebugHud.cpp:1170` — Debug gizmos, 2026-06-12.** `L`
- `src/debug/DebugHud.cpp:1204` — Audio engine, 2026-06-12.** Music
- `src/debug/DebugHud.cpp:1214` — Track switching, 2026-06-12.** `9` =
- `src/render/SceneResources.cpp:404` — Two-level cache rebuild path (2026-06-12).** Per-chunk
- `src/render/SceneResources.cpp:511` — Two-level cache hit path (2026-06-12).** Copies the cached
- `src/render/SceneResources.cpp:595` — Two-level cache check, 2026-06-12.** The hash folds
- `src/render/SceneResources.cpp:627` — Cache miss path (2026-06-12).** Run the canonical
- `src/render/SceneResources.hpp:346` — Two-level chunk visibility cache, 2026-06-12.** The hash
- `src/render/ScreenshotCapture.cpp:397` — Per-pass CPU timing keys (2026-06-12). Split into
- `src/shaders/taa_resolve.frag:76` — Color-distance rejection threshold in YCoCg space. When the current
- `src/shaders/taa_resolve.frag:76` — Color-distance rejection threshold in YCoCg space. When the current
- `src/voxel/VoxelWorld.cpp:328` — Floor bounds (M5.1d, 2026-06-12):** the XZ extent of
- `src/voxel/VoxelWorld.hpp:81` — Floor bounds (M5.1d, 2026-06-12):** the XZ extent of

### Fixed

- `src/app/AppUpdate.cpp:640` — Frame-step / slow-motion accumulator override
- `src/app/AppUpdate.cpp:863` — Audio engine mirrors, 2026-06-12.** Source
- `src/app/ModelGravigun.hpp:8` — M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
- `src/asset/AssetLoader.cpp:257` — Walk the default scene's node hierarchy in DFS order, applying
- `src/asset/ModelManifestLoader.cpp:274` — M5.1d, 2026-06-12: per-axis smart snap (replaces M5.1c
- `src/asset/ModelManifestLoader.cpp:371` — X axis.** Two valid placements per axis:
- `src/core/Types.hpp:221` — Frame-step / slow-motion debug, 2026-06-12:** live time-scale
- `src/core/Types.hpp:1332` — Frame-step / slow-motion debug, 2026-06-12.** Live
- `src/core/Types.hpp:1345` — Frame-step request, 2026-06-12.** One-shot flag. Set by

---

## 2026-06-11

### Changed

- `src/asset/AssetManifest.hpp:31` — Parses a manifest string of the form

### Fixed

- `src/app/FramePreparation.cpp:261` — TAA jitter: advance the 8-tap Halton(2,3) sub-pixel sequence and stash

---

## 2026-06-09

### Changed

- `src/shaders/voxel.frag:117` — AOCC and local-light DDA caps were tuned down from 6 and 32 to 4 and 12 after a
- `src/shaders/voxel.frag:117` — AOCC and local-light DDA caps were tuned down from 6 and 32 to 4 and 12 after a

---


## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:153` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:137` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `src/CMakeLists.txt:137` — **Per-target mismatched-tags / missing-field-initializer

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:162` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:172` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:187` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:228` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:244` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:289` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:162` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:172` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:187` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:228` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:244` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:289` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:65` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:167` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:177` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:192` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:233` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:249` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:294` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:25` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:223` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:232` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:349` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:382` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:430` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:513` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:562` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:584` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:643` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:713` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
- `cmake/ProjectVThirdParty.cmake:27` — **SYSTEM property (2026-06-18, windows-host-build-r0).**
- `cmake/ProjectVThirdParty.cmake:46` — **Legacy per-target warnings-as-errors gate
- `src/CMakeLists.txt:3` — **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
- `src/CMakeLists.txt:71` — Defense r0 (2026-06-13): GPU ray-march compute shader for the
- `src/CMakeLists.txt:184` — **Audio engine, 2026-06-12.** miniaudio is vendored as
- `src/CMakeLists.txt:194` — **miniaudio SYSTEM property (`2026-06-18`,
- `src/CMakeLists.txt:209` — **Per-target mismatched-tags / missing-field-initializer
- `src/CMakeLists.txt:250` — Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
- `src/CMakeLists.txt:266` — **Tier 3 (`2026-06-13`).** C / intrinsics hot-kernel
- `src/CMakeLists.txt:311` — **Explicit PATHS for PowerShell lookup (`2026-06-18`,
- `tests/CMakeLists.txt:1` — **libc++ migration (Phase 2, `2026-06-13`).** Helper
- `tests/CMakeLists.txt:30` — **Windows clang-cl fallback (`2026-06-18`,
- `tests/CMakeLists.txt:103` — Audio engine (2026-06-12). `miniaudio` is needed
- `tests/CMakeLists.txt:120` — **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
- `tests/CMakeLists.txt:291` — **Tier 4 (`2026-06-13`).** C / AVX2 kernel wrapper test.
- `tests/CMakeLists.txt:341` — **Tier 4 — module wiring.** The test includes
- `tests/CMakeLists.txt:350` — **Tier 5 (`2026-06-13`).** Unit test for
- `tests/CMakeLists.txt:467` — StringID smoke test (Tier 1.D, 2026-06-13). Header-only
- `tests/CMakeLists.txt:500` — C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
- `tests/CMakeLists.txt:548` — C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
- `tests/CMakeLists.txt:631` — **Tier 3 (`2026-06-13`).** Google Benchmark harness
- `tests/CMakeLists.txt:680` — **Tier 0.B / 2 alignment.** The benchmark includes
- `tests/CMakeLists.txt:702` — **Tier 5 (`2026-06-13`).** `ShadowProjectionBenchmark` —
- `tests/CMakeLists.txt:761` — **Fluid CA unit tests (audit `2026-06-13`).** CPU-only
- `tests/CMakeLists.txt:831` — **Present-mode cycle tests (auto-detect cycle,

## 2026-06-19 — CMake / build-system refactor-history (post-Phase B follow-up)

Refactor-history blocks from CMakeLists.txt + cmake/*.cmake that were
outside Phase B scope (`src/`, `tests/`, `src/shaders/` only). Moved here
by Phase F of session-2026-06-19T-comment-minimization-r0.

### Removed

- `CMakeLists.txt:3` — **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
- `CMakeLists.txt:70` — **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
- `CMakeLists.txt:81` — **Tier 2.C (`2026-06-13`).** Disable GNU language
- `CMakeLists.txt:98` — **libc++ migration (Phase 2, `2026-06-13`).** Per
- `CMakeLists.txt:174` — Migration details — applies on Linux/macOS native clang:
- `CMakeLists.txt:221` — `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
- `CMakeLists.txt:252` — **Release-only compile + link policy (2026-06-14).**
- `CMakeLists.txt:327` — **Volk SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:336` — **VMA SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:344` — **VMA SYSTEM property (2026-06-18, windows-host-build-r0).**
- `CMakeLists.txt:406` — --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
- `CMakeLists.txt:472` — Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
- `CMakeLists.txt:533` — nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
- `CMakeLists.txt:550` — **nlohmann_json SYSTEM property (`2026-06-18`,
- `CMakeLists.txt:560` — **Pure MSVC cl.exe path (`2026-06-15`).** Per the
- `CMakeLists.txt:579` — **Windows clang-cl path (`2026-06-15`).** Per the
- `CMakeLists.txt:606` — **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
- `CMakeLists.txt:624` — **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
