# Knowledge

Единый файл долговечных repo-specific фактов и действующих инженерных договорённостей.
Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`, текущий workspace — `agent/workspace.md`.

**Pre-reset content (2026-06-24, ~2199 строк / 36 contracts):** archived at
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/knowledge.md`. Treat as historical
artifact — see WARNING header in that file. **DO NOT cite as authoritative.**

**This file was rebuilt from code as the post-reset source of truth** (session 25x,
2026-06-25). Every contract below was re-validated against the live source tree.
When a contract is partially-obsolete (e.g. CSM-shadows section in lighting contract),
the obsolete sub-clauses are explicitly crossed out with `~~strikethrough~~` and the
superseding approach is named.

---

## 0. Source-of-truth ranking (см. также `AGENTS.md` §4)

```
1. Код (.cpp/.hpp/.ixx/.glsl + тесты)            ← абсолютный приоритет
2. AGENTS.md                                       ← протокол работы агента
3. agent/knowledge.md  (этот файл)                ← действующие engineering contracts
4. agent/workspace.md                              ← снимок текущего состояния
5. docs/VulkanSDK-Linux-Docs-1.4.350.1/            ← вендорная документация Vulkan 1.4
6. TODO.md                                         ← roadmap, приоритеты, риски
```

Анти-дублирование: перед добавлением новой секции — `rg "^## " agent/knowledge.md`.
Если новая секция дублирует существующую, расширять существующую.

---

# Part A — Engineering contracts

## 1. Document boundaries (sources of truth)

**Решение:** четыре файла хранят непересекающиеся слои знаний:
- `TODO.md` — roadmap + приоритеты + риски + чекбоксы.
- `AGENTS.md` — протокол работы AI-агента (commit format, scope discipline, safety).
- `agent/knowledge.md` (этот файл) — действующие engineering contracts + runtime facts.
- `agent/workspace.md` — короткий снимок текущего состояния + активные задачи.

Per-session narrative (что делал агент, в каком порядке) → `agent/workspace.md`.
Per-commit history → `CHANGELOG.md` + `git log`.
Archived detail (если файл разрастается) → `legacy/docs/archive/`.

**Почему:** цена обязательного чтения растёт быстрее полезного контекста, когда
документы пересказывают друг друга.

## 2. Hardware target: RTX-only, hard-fail на non-RTX

**Решение:** целевой GPU = NVIDIA RTX 20/30/40/50 series (Turing RT cores или новее).
- `IsRayTracedShadowEnabled(context)` = `context.rayTracing.accelerationStructure &&
  context.rayTracing.rayQuery` (auto-detect, **НЕ** env-gated).
- Если GPU не RTX-capable → engine refuses to start с понятным error message.
  Никаких CSM-fallback, никакого non-RTX path, никаких silent degradations.
- `PROJECTV_HW_RAY_TRACING` cmake option **существует** (`CMakeLists.txt:34`) но
  влияет только на feature-flags в коде; runtime auto-detect всё равно включается.

**Почему:** CSM bias tuning (предыдущий подход) упёрся в Peter Panning который
не решается через bias coefficients. RTX даёт ground-truth shadows + AO + GI +
specular + refraction на dedicated RT cores, лучше по perf чем CSM + DDA + VCT.
Pet-project = pet-project, нет нужды в legacy уступках.

**Cross-refs:** `TODO.md` §5.2.C (`Milestone 5.2.C Closed — RTX shadows = default`).

## 3. Build presets & verification contract

**Решение:** mainline repeatable build paths живут в `CMakePresets.json`.

| Preset | Host | Build type | Tests | Benchmarks | Tracy UI | Validation |
|---|---|---|---|---|---|---|
| `linux-clang-debug` | Linux clang 22 | Debug | ON | ON | OFF | ON |
| `linux-clang-debug-ci` | Linux clang 22 | Debug | ON | ON | OFF | ON |
| `linux-clang-debug-tracy-profiler` | Linux clang 22 | Debug | OFF | ON | OFF | ON |
| `linux-clang-release` | Linux clang 22 | Release | ON | OFF | OFF | OFF |
| `windows-clang-debug` | Windows clang-cl | Debug | ON | OFF | OFF | ON |
| `windows-clang-debug-ci` | Windows clang-cl | Debug | ON | OFF | OFF | ON |
| `windows-clang-debug-tracy-profiler` | Windows clang-cl | Debug | OFF | OFF | OFF | ON |
| `windows-clang-release` | Windows clang-cl | Release | ON | OFF | OFF | OFF |

**Build-preset target-list invariant:** все 5 build-presets (кроме smoke-варианта)
**обязаны** перечислять все ctest-registered executables в `targets[]`. Без этого
`cmake --build --preset X-build` + `ctest --preset X-tests` ломается на чистом
clone: 39+ тестов получают «cannot find executable» если build-preset не собрал
соответствующий бинарь. Benchmark (`ProjectVFrustumCullBenchmark`) — только
linux-clang-debug, linux-clang-debug-ci, linux-clang-debug-tracy-profiler
(gated by `PROJECTV_ENABLE_BENCHMARKS=ON`).

**Runtime smoke:** `windows-clang-debug-smoke` запускает
`tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` против собранного `ProjectV.exe`.
Это **targeted** lifecycle check, не mandatory DoD для каждой задачи. Используется
после изменений в Vulkan/bootstrap/swapchain/window lifecycle/present/screenshot
sync или при риске device-lost/hang.

**Jolt include contract:** все TU, использующие Jolt, начинают с
`<Jolt/Jolt.h>` **первым**; auto-refactor не должен поднимать другие Jolt headers
выше него.

**Почему:** без явного target-list чистый clone ломается при первой попытке
`ctest` после свежей конфигурации. Без smoke harness изменения в bootstrap
проходят build+tests но ломают runtime.

## 4. Release preset policy (conservative)

**Решение:** `linux-clang-release` / `windows-clang-release` — Release build path
со следующими **обязательными** compile/link flags (живут в
`CMakeLists.txt:58-71`, не в preset-level override):

**Compile flags:** `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections
-fno-finite-math-only`.

**Link flags:** `-flto=thin -Wl,--gc-sections`.

**Категорически запрещено** в Release:
- `-ffast-math` — ломает детерминизм Fluid CA (CPU path в `VoxelWorld::UpdateFluidCA`,
  `z,y,x`-ascending ordering) и TAA YCoCg clamp (`taa_resolve.frag`).
- `-fno-omit-frame-pointer` — нет пользы без backtrace symbols в production.
- PGO / AutoFDO — отдельный 3-step workflow, не часть release-пресета.

**Опционально** (вкл. вручную через `-DPROJECTV_ENABLE_NATIVE_ARCH=ON`):
- `-march=native` — разрешён, т.к. пользователь работает на одной машине; остальные собирают из исходников.

**Per-define toggles в Release:** `PROJECTV_ENABLE_TRACY=OFF`,
`PROJECTV_ENABLE_RENDERDOC_MARKERS=OFF`, `PROJECTV_ENABLE_BENCHMARKS=OFF`.

**Linker:** `CMAKE_LINKER_TYPE=LLD` (Linux: `/usr/bin/ld.lld` 22.1.6; Windows: clang-cl LLD).

**Почему:** без conservative policy release binary получает undeclared UB в hot path
(детерминизм simulation страдает).

## 5. C++ module conventions

**Решение:** 5 primary C++ module files (`.ixx`):
- `src/core/Math.ixx` — `projectv::math::Vec3/Vec4/Mat4`.
- `src/core/StringId.ixx` — `projectv::core::StringID`.
- `src/core/Probe.ixx` — runtime probes.
- `src/core/Types.ixx` — `projectv::core::Types` aggregated.
- `src/ecs/EcsWorld.ixx` — `projectv::ecs::EcsState`.

**Module gate** (`CMakeLists.txt:40-52`):
- `CMAKE_CXX_STDLIB libc++` + `add_compile_options(-stdlib=libc++)` — только
  на non-MSVC && non-WIN32.
- `CMAKE_CXX_MODULE_STD ON` — включается только когда libc++ доступен
  (libstdc++ не ship'ит std.cppm).
- 3-branch `projectv_build_options`: `if (MSVC)` / `elseif (WIN32)` / `else ()`.
  Linux-clang попадает в `else()` (libc++ + libstdc++ hybrid). Windows-clang-cl
  попадает в `elseif (WIN32)` (MSVC STL, no libc++).
- Module `FILE_SET` в `src/CMakeLists.txt:3-13` и `tests/CMakeLists.txt:1-11`
  пропускается на WIN32 + Clang (clang-cl не поддерживает C++20 modules).

**`import std;` — probe-only.** `tests/StdModuleProbe.cpp` тестирует precompiled
`std.pcm` от libc++ 22; в mainline **не** используется (libc++ 22 std.cppm
конфликтует с fmt headers). Это сознательный trade-off: иметь compile-time
gate для будущего включения, но не платить за текущие conflicts.

**Почему:** C++26 modules — это долгосрочная цель. Текущая стратегия: ship'ить
то что работает (5 .ixx файлов), держать `import std;` как testable gate.

## 6. Voxel storage: hybrid Sparse64Tree (CPU) + NanoVDB (GPU)

**Решение:** два уровня хранения.

**Level 1 (CPU, canonical):** `projectv::voxel::Sparse64Tree`
(`src/voxel/Sparse64Tree.hpp`) — 4²-per-axis octree (bits-per-axis=2, 64 children per node).
- Slot encoding: `Leaf|Homogeneous|NodeIndex|material8` (32-bit).
- Deduplication via splitmix64 hash + `unordered_multimap` index, gated by
  `SetDeduplicationEnabled(bool)`.
- Tree depth = `log_k(chunkSize)` where `k=4`; depth=2 for chunkSize=8 (default).
- Methods: `Reset/GetCell/SetCell/Contains/NodeCount/LiveNodeCount/RestoreFrom/NonAirCount`.

**Level 2 (GPU mirror):** `projectv::voxel::nanovdb::NanoVdbFlattenResult`
(`src/voxel/NanoVdb.hpp`).
- 3-level VDB: `NanoVdbUpper` (8 B), `NanoVdbLower` (16 B), `NanoVdbLeaf` (24 B),
  materials (uint8).
- `BuildNanoVdbFlatten(tree, materialLookup, outResult)` walks Sparse64Tree.
- `ReadNanoVdbVoxelMaterial(result, rootUpperIndex, lx, ly, lz)` for shader consume.
- **Grow-on-exceed** buffers in `SceneResources::GrowNanoVdbBuffer` +
  **deferred-destroy queue** `RenderState::deferredNanoVdbDestroys[MAX_FRAMES_IN_FLIGHT]`
  чтобы избежать in-flight GPU read races.

**Relationship:** Sparse64Tree = canonical CPU storage. NanoVDB = GPU mirror,
rebuilt every frame on voxel edit (`sceneUploadVersion` / `sceneVoxelPayloadVersion` /
`sceneNanoVdbVersion` в `RenderState`).

**Почему:** CPU representation нужна для raycast, edits, fluid CA, greedy merger.
GPU representation нужна для fast shader-side consume без per-voxel buffer walks.
Два уровня синхронизируются через `sceneVoxelPayloadVersion` counter.

## 7. Meshing: GPU greedy (primary) + CPU fallback

**Решение:** GPU compute path — primary (`voxel_mesh.comp`, ~584 LoC).
- Per-dirty-chunk dispatch with `GreedyFacePass(faceIndex, axisN, axisU, axisV, signN, ...)`.
- 6-axis pass: `X+/X-/Y+/Y-/Z+/Z-`.
- Per-axis greedy merge up to `kMaxChunkExtentForGreedy=64u` (4096-bit per-axis visited
  bitmask = 3KB local memory); fallback to per-voxel 1×1 quads для oversize.
- **Floor material group merging:** `IsSameMeshingGroup(a, b)` allows FloorWhite(3) ↔
  FloorGray(4) to merge into one quad (identical surface properties, differ only in
  baseColor). Merged quad stores first-encountered material; `voxel.frag` computes the
  checkerboard albedo procedurally from world position: `ivec3(floor(worldPos - normal*0.5))`
  then `(voxelCoord.x + voxelCoord.z) & 1`. Mirrors `GreedyPhysicsMerger` which already
  ignores material type for collision shapes.
- Writes `PackedFace` (16 B, см. §16) в `packedFaces[]`, packs `(width, height)` в
  `packedExtents`. Записывает 2 indirect-draw SSBO (opaque + transparent).
- Inline camera-frustum cull (`IsChunkVisible`).
- `IsChunkInsideShadowCascade` — stub, всегда возвращает `true`
  (CSM удалён per TODO.md §5.2.D; солнечные тени RTX path).

**CPU fallback:** `CpuMeshGenerator::GenerateCpuChunkMeshXPositive` — scalar path,
emits `PackedSceneVoxelFace` per exposed voxel on +X face. kMaxExtent=64.
Используется только в tests (`ProjectVCpuMeshGeneratorTests`).

**Pattern C (mesh shader) — feature-flagged:** `voxel_mesh_pre.comp` (compute pre-cull
with `atomicAdd(visibleCount)`) + `voxel_mesh.mesh` (mesh shader). Gated by
`PROJECTV_MESH_SHADER_PIPELINE=ON` (default OFF per `VulkanMeshShaderPipeline.hpp:28`).

**Почему:** GPU greedy быстрее (нет CPU↔GPU transfer), 6-axis за один dispatch
(shared bitmask amortization). CPU fallback — для регрессий и pre-RTX smoke.

## 8. Voxel fluid CA: GPU primary, CPU reference

**Решение:** GPU compute ping-pong — mainline path.
- `fluid_ca.comp` читает `sourceFluidCells` (binding 2), пишет `destinationFluidCells`
  (binding 3). Per-tile determinism via `imageAtomicCompareExchange`.
- `ActiveChunkIds` (binding 1) фильтрует skip-tile chunks.
- Default rate 5 Hz (`simulation.fluidTickRateHz`, `Types.hpp:943`).
- Multi-tick allowed per frame (no cap on N в `Renderer.cpp:1512-1548`).
- Pipeline: `VulkanFluidCaPipeline` + `SubmitFluidCaToComputeQueue` (async compute).
- `IsFluidCaGpuEnabled()` / `ToggleFluidCaGpuEnabledForTesting(bool)` — env-driven gate.

**CPU reference implementation** (`VoxelWorld::UpdateFluidCA`,
`VoxelWorld.cpp:1510+`) — 3-phase: read snapshot → sim (fall+spread with z,y,x
ascending order) → commit. Determinism per `decisions §30`: никаких FP в simulation,
зера/y/x ascending. **Сохранён** для regression tests.

**Почему:** GPU ping-pong на порядок быстрее для сцен с тысячами вокселей. CPU
остаётся как authoritative reference + для tests где нужна детерминированная
симуляция без GPU.

## 9. Voxel world pipeline (full)

**Решение:** `VoxelWorld` (`src/voxel/VoxelWorld.hpp`) — canonical state.

**Fields:**
- `Sparse64Tree sparseStorage` (level 1)
- `std::vector<VoxelChunk> chunks` (40 B/struct, `static_assert` в `VoxelWorld.hpp:59-69`)
- `pendingChunkRebuildIndices` — chunks для re-mesh
- `pendingBlasRebuildIndices` — chunks для re-build BLAS (RTX)
- `editVersion` — monotonically incrementing на каждый `SetVoxelMaterial`
- `stats { dirtyChunkCount, activeChunkCount, nonAirVoxelCount, glassVoxelCount, fluidVoxelCount, ... }`
- `fluidCAAabbMin/MaxExclusive` — invalid sentinels `{INT32_MAX, …}` /
  `{INT32_MIN, …}`, recomputed during snapshot load + каждый раз когда fluid voxel
  установлен.

**VoxelChunk struct (40 B, 8-byte aligned):** `min` (Int3, 12 B) +
`maxExclusive` (Int3, 12 B) + `rebuildQueued` (bool) + `isStatic` (bool) +
`nonAirVoxelCount` (u32) + `ticksSinceLastEdit` (u32) + `lodLevel` (u8) +
`lodDownsampledNonAirCount` (u8) + 2 reserved u8.

**VoxelWorldConfig:** `floorSize=18, floorY=0, worldTopY=14, padding=3, chunkSize=8`.

**VoxelMaterial enum (uint8_t):** `Air=0, Glass=1, Fluid=2, FloorWhite=3, FloorGray=4`.
Material defaults в `VoxelMaterials.cpp:131-163` (см. §15).

**Почему:** single canonical world struct делает `SetVoxelMaterial(world, pos, mat)`
edits reproducible +128-бит `editVersion` counter даёт cheap "has changed?" test
для downstream consumers (physics re-sync, chunk re-mesh, BLAS re-build).

## 10. Lighting BRDF + material model (single source of truth)

**Решение:** `VoxelSceneLighting` struct (352 B, `VoxelMaterials.hpp:61-83`) —
SSBO at set=0, binding=3. **Byte-exact** с GLSL counterpart в 5+ шейдерах
(см. §16). 14 `static_assert` enforces offsets.

**VoxelMaterialVisual (64 B, `VoxelMaterials.hpp:47-59`):** `baseColor | surface |
medium | shading` — 4×vec4 layout. SSBO at set=0, binding=2. 4 `static_assert`.

**BRDF functions** (`src/shaders/lighting.glsl` + дублированы в `voxel.frag`):
- `ProjectV_DistributionGGX` — Trowbridge-Reitz GGX NDF.
- `ProjectV_GeometrySchlickGGX` + `ProjectV_GeometrySmith` — Smith Schlick-GGX.
- `ProjectV_FresnelSchlick(cosTheta, f0)` — Schlick approximation;
  `f0 = mix(0.16*reflectance², albedo, metallic)`.
- `ProjectV_EvaluateDirectLighting` — полный direct-sun:
  `diffuse * radiance * wrappedDiffuse * directDiffuseStrength + specular * radiance * nDotL`.
- `ProjectV_SampleEnvironmentDiffuse` — smoothstep-blended sky/horizon/ground.
- `ProjectV_ApplyToneMap(linear, op)` — `Linear | Reinhard | AcesApprox`
  (Narkowicz ACES approximation).
- `ProjectV_ApplyColorGrading(color, whitePoint, contrast, saturation, lift)` —
  luma-saturation mix → contrast S-curve → lift.

**ToneMapOperator enum:** `Linear=0, Reinhard=1, AcesApprox=2` (default).
**ExposureMeteringMode enum:** `Manual=0, SceneKey=1` (default).
**LightingDebugView enum (14 values):** `Final → Ambient → Direct → Local → Shadow →
Contact → Occlusion → Fog → DiffuseGI → SpecularGI → RtxSpecular → VolumetricFog →
VolumetricTransmittance → GreedyMeshing → Final`. Default `Final`. Stored in
`sceneLighting.postProcess.w` (SSBO binding 3). Shader branch IDs match enum values 1:1.
`GreedyMeshing(13)` draws red borders + crosses per merged greedy quad via normalized
[0,1] UV (`inQuadUV`, location 4 varying from `voxel.vert`).

**~Strikethrough~ (per §5.2.D removal):** ~~first sun shadow path = 4-layer CSM~~
полностью удалён. Текущий path = RTX shadows (§14). Voxel.frag `ComputeSunShadowSample`
делает `texture(rtxShadowMask, gl_FragCoord.xy / vec2(textureSize(rtxShadowMask, 0))).r`
для non-RTX fallback возвращает 1.0 + applies `ComputeSunContactVisibility` для lit fragments.

**Почему:** material visuals + scene lighting как SSBO даёт hot-swap без pipeline
recompile. BRDF centralized в `lighting.glsl` + per-shader copy избегает #include
проблем в glslang/glslc 1.3 target.

## 11. Frame pipeline ordering (DrawFrame)

**Решение:** `Renderer::DrawFrame` (`src/render/Renderer.cpp:1203-1703`) выполняет
строго упорядоченную sequence кадр-за-кадром. Per-frame control flow:

1. **Drain:** `DrainDeferredNanoVdbDestroysForFrame` (VMA cleanup).
2. **Acquire:** `vkAcquireNextImageKHR(UINT64_MAX)` → `imageIndex`.
3. **Wait+reset:** `vkWaitForFences(UINT64_MAX)` + `vkResetFences` +
   `vkResetCommandBuffer` + `vkBeginCommandBuffer`.
4. **Pre-graphics:**
   a. Mesh shader pre-cull (compute dispatch) — если `meshShaderEnabled`.
   b. RTX: collect dirty + initial BLAS chunks → `SetBlasDirtyQueue` →
      `BuildDirtyBlases` (one-shot cmd + fence) → `UpdateTlas` (instance write) →
      `RecordVoxelAwareRtxShadowPass` (writes `rtxShadowMask`).
   c. DDGI: `RecordRtxGiProbeUpdatePass` (если `rtxGiProbes->IsEnabled() &&
      tlas != null`).
5. **Graphics:** `RecordGraphicsCommands`:
   - Voxel meshing compute dispatch (если `dirtyChunkCount > 0`).
   - ~~Shadow~~ — `RecordShadowCommands` empty (CSM removed).
   - RTX: `RecordTlasBuild` + AS_BUILD→FRAGMENT barrier.
   - Image transitions (TAA scene, layer, history, motion, depth) → COLOR_ATTACHMENT.
   - Dynamic rendering: 4 color attachments (main+layer+layer history+motion) + 1 depth.
   - Sky atmosphere pre-pass (если `IsSkyAtmosphereEnabled`).
   - Opaque voxel pass: `vkCmdDrawIndirect` (or `vkCmdDrawIndirectCountKHR` если
     HZB culling) на `opaqueIndirectBuffer`; для mesh shader — `vkCmdDrawMeshTasksEXT`.
   - Model pass: `vkCmdDrawIndexed` per visible model instance.
   - Transparent pass: `vkCmdDrawIndirect` на `transparentIndirectBuffer`.
   - Debug overlay + HUD (если `!taaOn`).
   - Cloudscape raymarch (если `IsCloudscapeEnabled`).
   - `RecordRayTracedShadowPass` (sync barrier).
6. **Post-graphics (если TAA on):** transitions → TAA resolve second dynamic
   rendering → `vkCmdDraw(3,1,0,0)` на swapchain → history copy (vkCmdCopyImage
   в `taaHistoryColorTarget`).
7. **HZB chain:** `BuildHizMipChain` (depth → mip chain) + sync/async HZB cull dispatch.
8. **Inline compute (если async compute path inactive):** Fluid CA dispatch × N +
   WorldGen dispatch.
9. **Submit:** `vkEndCommandBuffer` + (если async path) `RecordAsyncComputePass` +
   `SubmitToComputeQueue` (signals `renderTimelineSemaphore`).
10. **vkQueueSubmit2:** wait = imageAvailableSemaphore + (compute) renderTimeline;
    signal = submitSemaphore + (HZB) hzbBuildTimeline.
11. **Present:** `vkQueuePresentKHR` + `SaveRequestedScreenshot` (fenced BMP + sidecar).
12. **Lifecycle:** `RecreateSwapchain` если OUT_OF_DATE/SUBOPTIMAL/resized.

**Почему:** строгий ordering = reproducible race-free frame. Все GPU resources
(буферы, images) проходят явные layout transitions. Persistent async compute
cmd buffer (см. §11) signal/wait парный contract.

## 12. Async compute + timeline semaphore pairing

**Решение:** persistent `asyncComputeCommandBuffer` (`asyncComputeCommandPool`),
per `VulkanAsyncCompute.cpp`.

**Signal/wait pairing** — per-pass dedicated timeline semaphore:
- **`renderTimelineSemaphore`**: `RecordAsyncComputePass` ждёт его же (skip first
  frame if value=0). `SubmitToComputeQueue` signalит `asyncComputeLastTimelineValue`.
  Graphics `vkQueueSubmit2` ждёт `asyncComputeLastTimelineValue` чтобы graphics
  не стартовал раньше compute.
- **`hzbBuildTimelineSemaphore`**: `RecordHzbAsyncCullPass` / `SubmitHzbAsyncCullToComputeQueue`
  — separate pair. `Renderer.cpp:1647-1654` signalит `hzbBuildLastTimelineValue+1`
  после graphics submit (если async HZB path active).

**Persistent cmd buffer requires** `vkResetCommandBuffer` once at allocation
+ skip wait on first frame (`renderTimelineValue == 0`).

**Почему:** signal/wait pairing enforced by code structure (per-function dedicated
semaphore) предотвращает race conditions которые были бы возможны с shared
semaphore. Симметричный pattern для HZB даёт independent async chains.

## 13. HZB culling

**Решение:** `projectv::render::HizBuffer` (`src/render/HizCulling.hpp`) — mip
chain of depth image, reduces full-res depth → ½ → ¼ → … → 1×1.

**Pipeline:** `BuildHizMipChain` (compute shader downsamples) →
`RecordHzbCullingDispatch` (per-chunk AABB vs HZB, writes 64-bit visibility bitmask
via `atomicOr`).

**Bindings:** `set=0 binding=0` `ChunkAabbBuffer` + `binding=1` `VisibilityMask`
(WO) + `binding=2` `hizTexture` (sampler2D) + `binding=3` `hizSampler` (sampler) +
`binding=4` `VisibleCount` + `binding=5` `PerChunkMip` (`kHizMipAndBlendWidthWordsPerChunk=2u`).

**Smart mip + blend width** per-chunk selection (`ComputePerChunkMipLevelCpu`,
`ComputePerChunkMipAndBlendWidthsFromAabbs`).

**Async compute path:** `RecordHzbAsyncCullPass` separate from main async compute
(Fluid CA + WorldGen). Default = sync (inline) path; async gated by
`IsAsyncComputeEnabled() && IsAsyncComputeResourcesAllocated()`.

**Почему:** HZB даёт conservative occlusion culling → сокращает vertex work на
~50% для типичных сцен. Async path разгружает graphics queue.

## 14. RTX shadows (BLAS + TLAS + voxel-aware intersection)

**Решение:** `projectv::render::RayTracedShadows` (`src/render/RayTracedShadows.hpp`).
**Canonical** sun shadow path после §5.2.D CSM removal.

**Architecture:**
- **BLAS** per chunk: `VkGeometryTypeAabbKHR` через `BuildDirtyBlases` one-shot
  cmd + fence. AABB в `DirtyChunkRebuild { chunkIndex, aabb }` queue.
  `vkGetAccelerationStructureDeviceAddressKHR` кэширует per-chunk address в
  `m_config.blasDeviceAddresses[i]`.
- **TLAS** per frame: `UpdateTlas(visibleChunks, identityTransforms)` writes
  instances; `RecordTlasBuild` dispatches `vkCmdBuildAccelerationStructuresKHR`
  + barrier AS_BUILD → fragment shader.
- **Voxel-aware shadow pass** (`RecordVoxelAwareRtxShadowPass`):
  2-pass trace в `voxel_rtx_shadow.rgen`:
  - Pass 1: primary ray (camera near plane → view direction, `T_max = far`).
    `rchit` пишет `gl_HitTEXT` в `payload.hitT`.
  - Pass 2 (only if primary hit): shadow ray от
    `worldOrigin + viewDir*hitT + viewDir*0.05` (small bias) along sun direction
    with `T_max = 256`. Sky pixels (primary miss) keep `shadowFactor = 1.0`.
- **Output:** `rtxShadowMask` image (R8_UNORM, swapchain extent) at set=0 binding=18.
- **Shader consume:** `voxel.frag` (`ComputeSunShadowSample`): для non-RTX
  fallback возвращает 1.0 (lit). Voxel-aware path: `texture(rtxShadowMask,
  gl_FragCoord.xy / vec2(textureSize(rtxShadowMask, 0))).r`. Strength factor
  `0.75` (25% ambient sun bleed) — `mix(0.25, 1.0, rtxLit)`.

**Constants:**
- `kRtxSunShadowTMinMeters = 0.5` (escape launch voxel)
- `kRtxSunShadowMaxDistanceMeters = 256`
- `kRtxCutoffRoughness = 0.3` (roughness > 0.3 → use VCT specular not ray trace)

**Fallback:** `CreateRtxShadowMaskFallbackOnly` allocates 1×1 R8 fallback image
so binding 18 is always valid even без RTX-capable GPU (defensive — но см. §2:
non-RTX GPU hard-fails).

**Self-shadow fix (session 23x):** procedural `rint` использует
`tCurrent = tMaxAxis.{axis}` (per-step update) вместо stuck `max(tEntry, rayTmin)`,
else hits проектируются на chunk boundary faces. Ignore `Glass` material в
traversal (transparent shell не cast shadow). `grazingFactor = mix(0.25, 1.0,
rtxLit)` для избежания pitch-black shadows.

**Chunk boundary precision fix (session 26x):** В `voxel.frag` и `probe_update.comp` добавлено смещение `1e-4` к `tMin` в `TraceVoxelIntersection` при входе луча в чанк (`tMin = max(tEntry, rayTmin) + 1e-4`). Без этого смещения погрешности `float` на границах чанков приводили к выходу начальной позиции DDA наружу чанка, прерывая цикл и заставляя лучи рефракции/GI просачиваться сквозь воду в небо (белые точки).

**Ray starts inside non-air voxel — DDA advance (session 26x follow-up #2):**
В `TraceVoxelIntersection` (`voxel.frag` + `probe_update.comp`, в т.ч. inline shadow ray в `EvaluateVoxelLighting`) добавлен блок: если воксель в стартовой позиции луча non-air, вычислить `tWall = min(tExitAxis.{x,y,z})` (время до границы текущего вокселя в направлении луча), затем `tMin += tWall + 1e-4` и пересчитать `currentVoxel = floor(localStartPos)`. Без этого фикса DDA коммитился на `tCurrent = tMin` (≈0 для луча внутри чанка), и нормаль в наружном hit-блоке вычислялась из position offset (5 мм) вместо реального направления стенки. Неправильная нормаль приводила к тому, что shadow ray в `EvaluateVoxelLighting` для проб, лежащих внутри воды/стекла, уходил в небо (`shadowFactor = 0`, но яркое значение из-за неверной нормали) → пробы сохраняли яркие "sky" значения для всех направлений → GI sampling давал probe-grid артефакты. Фикс: луч теперь стартует за стенкой вокселя, DDA начинается со следующего (воздушного) вокселя, нормаль и hit position вычисляются корректно.

**Hit normal derived from ray dominant axis (session 26x follow-up #3):**
В `TraceVoxelIntersection` (наружный hit-блок после ray query) в `voxel.frag` + `probe_update.comp` нормаль ВЫЧИСЛЯЕТСЯ из направления луча (dominant axis), а не из position offset. Раньше `diff = insidePos - voxelCenter`, `insidePos = worldHitPos + dir * 0.005`, и из 6 граней выбиралась ближайшая по `abs(diff)` — это давало рандомную грань, основанную на FP-микро-плавании commit t, часто НЕ совпадающую с реальной стенкой. Неправильная нормаль попадала в shadow ray `EvaluateVoxelLighting`: для refraction ray после back face воды shadow ray уходил в воздушный gap над водой (`shadowFactor = 1`) → яркое значение в refraction result → мелкие яркие точки в Final view, НО НЕ в debug views (refraction не показывается отдельно). Корректная нормаль для DDA-луча = `sign(dir.dominantAxis) * e_dominantAxis`.


**Почему:** RTX даёт ground-truth shadows без CSM артефактов (peter panning,
acne, PCF tuning, cascade bleeding). RT cores имеют spare capacity
(1080p × 120 FPS × 1 ray/pixel = 0.65% от peak).

**Opacity micromaps (Phase 4.7):** `VK_EXT_opacity_micromap` is probed, but its
use is blocked on real alpha-tested project assets. The current asset path has no
`AlphaMode::Mask` material handling or alpha-mask texture upload; do not enable or
build OMM data until such assets are present.

## 15. RTX GI: DDGI probes (Diffuse Global Illumination)

**Решение:** `projectv::render::RtxGiProbes` (`src/render/RtxGiProbes.hpp`).
**Заменяет** VCT 3D clipmap для diffuse GI (VCT specular path сохраняется).

**Architecture:**
- 8×8×8 grid = 512 probes (VoxelLab scene bounds).
- Per-probe: 16×16 octahedral (256 directions).
- Storage: 3D textures — `irradianceImage` (R11G11B10F, 16-bit per channel),
  `distanceImage` (RG16F), `probeDataImage` (fallback 2D).
- 64-ray spherical sampling per probe update.
- Update rate: 1 probe/frame round-robin → full update за 8 frames (or configurable).
- Probe sample: trilinear interpolation among 8 nearest probes at receiver,
  normal-based cosine weighting, Chebyshev depth visibility test.
- `RtxGiVolumeDesc` SSBO (binding 17) описывает volume: origin/halfExtent/
  invProbeCount+spacing/maxRayDistance/probeCounts+rays.

**Shader consume:** `voxel.frag` (`SampleRtxGiProbeIrradiance`):
- trilinear fetch among 8 nearest probes
- normal-based cosine weight
- **Gaussian visibility falloff (session 26x):** original Chebyshev test
  (`p = variance / (variance + g*g)`) создавал резкий переход на
  `distToProbe == mean`, особенно для проб внутри opaque geometry
  (water/glass с `variance`~0.1–0.5, `mean`~1м) — это проявлялось как
  проб-сеточный алиасинг на задней грани воды (мелкие статичные точки
  на регулярной 8м сетке, прыгающие при движении камеры). Заменено на
  `exp(-distExcess^2 / (2 * max(variance, 0.25)))` — плавный спад с
  минимальной шириной 0.5м, сохраняющий окклюзионное поведение.
- per-frame `voxel.frag` `vctDiffuseIrradiance` → `ddgiDiffuseIrradiance` substitution

**Shader:** `probe_update.comp` evaluates voxel lighting per ray via
`EvaluateVoxelLighting`; uses `rayQueryEXT` against `rtxTlas` (binding 13).
`#extension GL_EXT_ray_query : require`. Cos-weighted accumulate, hysteresis
blend (first-frame = 1.0, then 0.95).

**Почему:** DDGI is RTX-standard для dynamic diffuse GI. VCT clipmap имеет
bleeding artifacts на разных плотностях геометрии. DDGI updates per-probe in
real-time без clipmap rebuild lag.

## 16. SSBO struct byte-exact invariant

**Решение:** 5+ шейдеров биндят `SceneLightingBuffer` (set=0 binding=3) +
`MaterialVisualBuffer` (set=0 binding=2) + `PackedChunkDescriptors` (set=0
binding=1, depends on shader).

**Source-of-truth:** C++ structs в `VoxelMaterials.hpp:47-105` + `Types.hpp:57-83`.
- `VoxelSceneLighting` — 352 B, 14 `static_assert` enforces offsets 0/16/32/48/64/80/96/112/128/144/160/176/192/208/224/288/304/320/336.
- `VoxelMaterialVisual` — 64 B, 4 `static_assert` (0/16/32/48).
- `PackedSceneVoxelFace` — 16 B, 5 `static_assert` (offsets 0/4/8/12, sizes).
- `PackedSceneChunkDescriptor` — 64 B, 4 `static_assert` (chunkOrigin/extent/voxelDataInfo/drawRanges).
- `PackedSceneChunkAabb` — 32 B, 2 `static_assert`.
- `SceneChunkVoxelPayloadRange` — 16 B, 3 `static_assert`.
- `GraphicsPushConstants` — 128 B, 4 `static_assert` (offsets 0/64/80/96/112).
- `ResolvePushConstants` — 144 B, 4 `static_assert`.
- `DebugOverlayPushConstants` — 112 B, 4 `static_assert`.
- `DebugHudVertex` — 32 B, 2 `static_assert`.
- `ChunkCullingParameters` — 64 B, 4 `static_assert`.

**Mirrored in shaders** (per TODO.md §24x consolidation):
- `voxel.frag:32-52` — `VoxelSceneLighting` (352 B, identical layout).
- `taa_resolve.frag:10-28` — same.
- `voxel_mesh.comp:57-75` — same.
- `model.frag` + `model.vert` — same.
- `probe_update.comp:32-52` — same (binding 3 = scene lighting).

**Invariant:** любое изменение в `VoxelSceneLighting` требует mirror update во
всех 5+ шейдерах. Compile-time catches это (`static_assert` + GLSL compile error).

**Почему:** byte-exact = single source of truth + compile-time validation. Иначе
silent GPU read garbage.

## 17. Scene presets + SceneConfig JSON

**Решение:** `VoxelScenePreset` enum (5 values, `uint8_t`):
`VoxelLab=0, FlatBenchmark=1, TransparencyStress=2, ChunkGrid=3, MeshingStress=4`.

**Cycle** (`GetNextVoxelScenePreset`): `VoxelLab → FlatBenchmark → TransparencyStress
→ ChunkGrid → MeshingStress → VoxelLab`. Hotkey `F5` → `InputAction::CycleScenePreset`.

**Lighting profiles:** `GetVoxelSceneLighting(preset)` (`VoxelMaterials.cpp:166-253`)
возвращает `VoxelSceneLighting` для каждого preset (sky/horizon/ground/sun
color+intensity, sun direction+wrap, post-process, contact, AO, grading, exposure,
local point light). Default scene: VoxelLab.

**SceneConfig JSON** (`runtime/scene.json`):
```json
{
  "name": "ProjectV Default",
  "scenePreset": "VoxelLab",
  "voxelWorld": { "floorSize": 18, "floorY": 0, "worldTopY": 14, "padding": 3, "chunkSize": 8 },
  "lighting": { "sunDirectionY": 0.80, "exposure": 1.0 }
}
```

**Env override:** `PROJECTV_SCENE_PRESET` env var (consumed by
`GetRequestedVoxelScenePreset`).

**`EnsureDefaultSceneConfig(path)`** writes default JSON if file отсутствует.
**`LoadSceneConfig(path, config)`** parses JSON via nlohmann_json (FetchContent v3.12.0).

**Reload flow:** `F5` → `InputAction::CycleScenePreset` → `world.requestedScenePreset
= next; world.scenePresetReloadRequested = true` → next `SDL_AppIterate` →
`ReloadActiveVoxelScene` → `CreateVoxelSceneWorld(state, preset)` →
`FinalizeActiveVoxelWorldReload` (camera reset, ECS sync, scene resources rebuild,
physics re-sync, model instance snap to ground, chunk prebake if streaming enabled).

**Почему:** declarative scene config + per-preset lighting profile = reproducible
visual regression targets для RTX-driven milestones.

## 18. TAA: SPIR-V variants + history params

**Решение:** 4 SPIR-V variants of `voxel.frag`:
- `voxel.frag.spv` (default, Location 0 output `outColor`).
- `voxel.frag.taa_on.spv` (`-DTAA_ENABLED`, Location 1 output `outSceneColor`).
- `voxel.frag.rtx.spv` (`-DVOXEL_RTX_ENABLED`, ray query consume).
- `voxel.frag.rtx_taa_on.spv` (`-DTAA_ENABLED -DVOXEL_RTX_ENABLED`).

**TAA shader (`taa_resolve.frag`):**
- YCoCg color space clamp (outlier rejection).
- `taaHistoryParams` (vec4): `.xy` = texelSize, `.z` = historyValid (0/1), `.w` = neighbourhood radius.
- Neighbourhood radius cycle: `1 → 3 → 5 → 7 → 1` (`,` hotkey).
- Inline CAS (Contrast Adaptive Sharpening) at end of resolve, linear-light pre-tonemap.
- `kTaaCasSharpnessMax = 0.5f` (`Types.hpp:858`).
- `sharpenAmount = max(0, (1.0 - taaBlend) * taaCasSharpnessMax)`.

**TAA scene color format** = `VK_FORMAT_B10G11R11_UFLOAT_PACK32` (HDR-friendly).

**TAA per-layer history** (`taaLayerHistory` = `VK_FORMAT_R8G8B8A8_UNORM`):
- R = `CTSH` (contact shadow), G = `AOCC` (AO), B = `LOCL` (local point light), A = 1.0.
- 3rd MRT attachment (Location 2) в `voxel.frag`.

**Jitter:** `AdvanceTtaPixelJitter` (Halton 2,3 sequence).

**Camera-cut detection:** Chebyshev L-infinity delta vs `prevViewProjectionMatrix`,
threshold `0.10` (`FramePreparation.cpp:256-282`). On cut: `taaHistoryValid = false`.

**History invalidation triggers (7):**
1. `T` ToggleTaa (enable flip).
2. Swapchain resize.
3. World reload (preset/snapshot).
4. `;`/`'` jitter scale.
5. `-`/`=` blend.
6. `,` neighbourhood radius.
7. `.` InvalidateTaaHistory (explicit).

**Per-frame state** (`RenderState`, `Types.hpp:843-880`):
- `taaEnabled=true` (default).
- `taaBlend=0.10f` (default), range 0..1.
- `taaFrameCounter`, `taaHistoryValid`, `taaJitterScale=0.0` (computed from
  `taaJitterX/Y`), `taaNeighbourhoodRadius=1`, `taaCasSharpnessMax=0.5f`.
- `taaPrevViewProjectionMatrix`, `taaPrevViewProjectionMatrixInitialized`.

**Почему:** SPIR-V variants = no runtime branches в hot path, optimal code-gen.
YCoCg + history clamp + neighbourhood = стабильность на low-frequency сценах.

## 19. Multi-frame-per-frame simulation tick

**Решение:** fixed-timestep simulation loop с accumulator.
- `fixedSimulationDeltaSeconds = 1.0f / 60.0f` (60 Hz).
- `kMaxSimulationStepsPerFrame` cap (см. `AppUpdateHelpers.hpp`).
- `effectivePaused = simulation.paused && !simulation.frameStepRequestedNow`
  (read-then-clear pattern).
- `frameDeltaSeconds *= timeScale` (slow-mo).
- Frame-step (`\`): force `simulationAccumulator = fixedSimulationDelta`
  → 1 tick consumed next frame.

**Walk mode:** `TickWalkCharacter(physics, world, camera, input, dt)` — voxel-solver
authority для grounded ownership (§20). `CharacterVirtual` = proxy/stance carrier.

**Creative mode:** `TickCreativeCharacter` — `CharacterVirtual::ExtendedUpdate` с
capped substepping для boosted flight (~0.05 m, max 32 substeps). Required для
high-speed transparency boost.

**Spectator mode:** `TickCamera` (free flight, not bound by pause per §21).

**Fluid CA throttling:** `simulation.fluidTickRateHz=5.0` (default) +
`fluidAccumulatorSeconds` + `fluidGpuTicksPending`. ECS `TickFluidCASystem` drains
`fluidGpuTicksPending` → up to N GPU dispatches per frame (no cap on N в
`Renderer.cpp:1512-1548`).

**Почему:** fixed timestep = deterministic physics. Accumulator pattern = stable
simulation при variable framerate. Frame-step = debug-friendly without TAA history
invalidation.

## 20. Walk authority + air-control + auto-jump (Jolt + voxel solver)

**Решение:** Static-world `walk` авторится voxel solver'ом в `PhysicsWorld.cpp`,
НЕ `CharacterVirtual::ExtendedUpdate`. `CharacterVirtual` остаётся proxy/stance
carrier для collision/contact infrastructure.

**WalkAirControlMode enum:** `MinecraftLike=0` (default), `Realistic=1`. Hotkey
`F11` cycle.

**Auto-jump contract:**
- `J` toggle `IsPhysicsWalkAutoJumpEnabled` (default OFF).
- `F12` toggle `IsPhysicsWalkAutoJumpDelayEnabled`.
- Manual held jump (`Space`) обнуляет pending auto-jump delay countdown.
- One-block auto-jump = optional traversal path, не always-on baseline.

**Jump / edge support:**
- Rising jump не использует voxel top-promotion.
- Cached ground-takeoff grace авторизует coyote/takeoff handoff только до первого
  jump commit; после ballistic jump active, не даёт second airborne jump.
- Edge support (`footSupportScore ≈ 0.5` + stable `feetY` + `velY` not rising) →
  `EdgeGrace`, не `Air`.
- Ultra-thin edge support (`footSupportScore < 0.2`) → handoff только под active
  jump request.
- Sneak-support region: `XZ` overlap + proximity к sampled support plane.
  Crouch wall-cling (lateral wall voxel) **не** считается опорой сам по себе.

**Почему:** этот path покрыт regression suite (`PhysicsIncrementalJoltTests`,
`PhysicsSyncTests`) + `creative_transparency_boost_*` fixtures. Real fixed-step
simulation = reproducible walk bugs, не synthetic-case false signals.

## 21. Control mode: Creative / Spectator / Walk

**Решение:** `CameraState::ControlMode` enum:
- `Creative=0` — collision-backed flight/edit mode. Подчиняется `pause` (кроме
  camera look). `DoubleTap Space` ↔ Walk toggle.
- `Spectator=1` — observe-only noclip без world edits. **НЕ** подчиняется `pause`
  для movement/look. Paused в simulation, но camera can still rotate (separate
  `TickCamera` call в `AppUpdateHelpers.cpp:104-106`).
- `Walk=2` — grounded physics mode (см. §20). `DoubleTap Space` ↔ Creative.

**Cycle:** `F4` hotkey: `Creative → Spectator → Walk → Creative`.

**Per-mode reset:** `ApplyControlModeTransition` calls `ResetWalkCharacter` чтобы
clear cached support ownership при смене mode.

**Почему:** режимы должны быть явными и предсказуемыми. Physics-backed path не
должен обходить paused simulation (creative obeys pause). Spectator нужен для
debug + look-dev без influence на simulation state.

## 22. Window / present mode cycle

**Решение:** `VulkanSwapchain` auto-detect cycle `[FIFO, MAILBOX, IMMEDIATE]`
per `BuildPresentModeCycle` / `CyclePreferredPresentMode` / `GetActivePresentMode`
(`VulkanSwapchain.hpp:69-148`).

**`g_active` preserved across rebuilds** — `inline` variable в header, не теряется
при `RecreateSwapchain`.

**Hotkey `3`:** `CyclePreferredPresentMode` + immediate `RecreateSwapchain` +
log `CycleVsync: <mode> [cycle idx/size]`.

**Почему:** developer needs uncapped FPS для профайлинга + tear-free для visual
regression. Auto-detect из `vkGetPhysicalDeviceSurfacePresentModes` гарантирует
поддержку на любом GPU.

## 23. Snapshot round-trip restores derived state

**Решение:** `SaveVoxelWorldSnapshot(world, path)` + `LoadVoxelWorldSnapshot(path)`
(`VoxelWorld.cpp`) — `std::expected<…, VoxelSnapshotError>` cold-path.

**After load:** `RebuildVoxelWorldDerivedState` сбрасывает `fluidCAAabbMin` /
`fluidCAAabbMaxExclusive` на sentinels (`INT32_MAX, …` / `INT32_MIN, …`) и
re-expand на каждый Fluid voxel found. Stats rebuild выполняется в той же
O(world_volume) итерации.

**Finalize flow** (`main.cpp:82-166` `FinalizeActiveVoxelWorldReload`):
1. `ResetCameraState` + `ApplyStartupCameraOverrideFromEnvironment`.
2. Zero mouse deltas + remove/place pressed.
3. `state->interaction().selection = {}`.
4. `state->frame().renderData.interactionSelection = {}`.
5. Reset `simulationAccumulatorSeconds`, `simulationStepsLastFrame`.
6. `SyncEcsWorldState`.
7. `CreateSceneResources` (rebuild scene SSBOs).
8. `SyncPhysicsWorld` (rebuild static body).
9. `SnapModelInstancesAboveGroundDispatch` (model gravity snap).
10. `BakeAllChunksToDisk` (if streaming enabled).
11. `SnapWalkCharacterToCamera` / `SnapCreativeCharacterToCamera` (per control mode).
12. Reset `state->render().taaHistoryValid = false` (TAA history invalid).

**Почему:** snapshot save/load — primary persistence + replay fixture format.
Stale derived state (fluid AABB, cached walk support) ломает downstream consumers.

## 24. Audio engine: miniaudio + async scan

**Решение:** `projectv::audio::AudioEngine` (`src/audio/AudioEngine.hpp`).

**Backend:** miniaudio 1.x single-header + PulseAudio → pipewire-pulse → PipeWire
(Linux default). 16-bit s16 stereo at device-native rate.

**State:** `MusicState {Stopped, Playing, Paused}`. `ma_engine` + `ma_sound_group`
(music bus) + `ma_sound`. `MA_TRUE` loop mode.

**Async scan:** `std::jthread` 5-second playlist refresh via
`std::filesystem::directory_iterator` на `PROJECTV_MUSIC_FOLDER` env var.
Sticky `currentIndex` (не reset'ится при удалении трека).

**Volume:** 0.0..1.0, step 0.05, default 0.8.

**Hotkeys** (`InputActions.cpp:172-177`):
- `Q` play/pause, `E` stop.
- `7/8` volume ±0.05.
- `9/0` next/prev track.

**Init:** `state->audio()->init()` in `SDL_AppInit`. Если init fails → running
without music (no hard-fail). `loadMusicFolder` consumes env-var path, returns
track count.

**4-line HUD block** (`DebugStats:434-443`):
- `MUSIC <state> VOL 0.80` (always).
- `ARTIST` / `TITLE` (если available).
- `POS m:ss / m:ss`.

**Почему:** miniaudio = single-header cross-platform audio. PipeWire = current
Linux default audio server. Async scan не блокирует main thread.

## 25. Asset pipeline (glTF + Draco + meshoptimizer)

**Решение:** 5-stage pipeline (`src/asset/`).

1. **fastgltf load:** `AssetLoader::LoadGlb(path, outError)` → `LoadedAsset
   {primitives[], aabb, vertexCount, triangleCount}`.
2. **Draco decode** (if compressed): `DracoMeshDecoder::DecodeDracoPrimitive(asset,
   primitive, out, outError)`.
3. **Mesh bake** (`meshoptimizer`): `MeshBaker::BakeLoadedAsset(asset, config,
   outError)` → `BakedMesh {BakedPrimitive{vertexBuffer=8 floats/vertex
   (pos+norm+uv), indices}, acmr, atvr}` с vertex cache + fetch optimization.
4. **GPU upload:** `MeshGpuResources::UploadBakedPrimitiveToGpu(...)` → `VkBuffer
   + VmaAllocation`.
5. **Registry:** `AssetRegistry` mutex-guarded `unordered_map<string,
   unique_ptr<LoadedAsset>>` + insertion-order vector.

**Manifest:** `AssetManifest::ParseAssetManifestString` → `vector<ManifestEntry
{id: StringID, path, position, rotation, scale}>`.
`ModelManifestLoader::LoadAndRegisterModelsFromManifest` uploads to GPU +
registers в `RenderState::modelRegistry` + spawns `ModelInstanceData` в
`render.modelInstances`.

**Model pass:** separate from main voxel pass. `PickModelPipeline` selects
`modelPipeline` или `modelPipelineTaaOn`. Per-frame CPU cull via
`c_kernels::FilterVisibleInstances` (AVX2 path).

**Почему:** glTF = standard interchange format. Draco = glTF compression
extension. meshoptimizer = GPU-friendly vertex order. fastgltf = header-only glTF
parser без external dependencies.

## 26. Physics: Jolt + GreedyMerger

**Решение:** `projectv::physics::PhysicsWorld` (`src/physics/PhysicsWorld.cpp`,
~4110 LoC).

**Jolt features:** `CharacterVirtual` (proxy/stance carrier), BroadPhase,
NarrowPhase. **DX12/VK/MTL/CPU compute disabled** — pure CPU path
(`JPH_USE_*_COMPUTE=OFF` per `CMakeLists.txt:130-133`).

**Incremental `SyncPhysicsWorld`:** только dirty chunks re-build static body
(17x fix). `RebuildStaticWorldBodyFromChunkShapes` consumes
`physics.chunkMergedBoxes` (от `GreedyPhysicsMerger`), destroys old static body,
builds new compound shape.

**Per-chunk:** `BuildChunkStaticCollisionBody(physics, world, chunkIndex)` —
per-chunk rebuild от greedy merger output.

**Queue drain:** `ProcessChunkRebuildQueue` drains `pendingChunkRebuildIndices`
post-SyncPhysicsWorld в `RunFrameSimulation`.

**Greedy merger:** `GreedyMergeSolidVoxelsInBounds(world, boundsMin, boundsMaxExclusive,
outBoxes)` → `vector<MergedVoxelBox {minX,minY,minZ, maxX,maxY,maxZ}>`.
35× reduction vs per-voxel box.

**After world edit:** `SyncPhysicsWorld` re-syncs to clear walk cached support
ownership (per §20).

**Почему:** Jolt = battle-tested physics engine. Incremental sync = no full
world rebuild on every edit. Greedy merger = 35× fewer collision shapes.

## 27. Chunk streaming + LOD

**Решение:**

**Chunk streamer** (`src/voxel/ChunkStreamer.cpp`): `std::jthread` worker drains
`EnqueueChunkStreamRequest` → reads `chunk_N.bin` files from
`PROJECTV_CHUNK_PATH` (default `${PROJECTV_CMAKE_BUILD_DIR}/cache/chunks`) into
`ChunkData {vector<uint8_t> voxelBytes, vector<uint32_t> nodeWords}`.

**Prebake:** `BakeAllChunksToDisk(world, prebakeStats)` writes chunk binary
(16-byte header `0x504B5631` PKV1 magic + voxelBytes) per chunk; consumes
`world.sparseStorage.GetCell(...)`. `ChunkPrebakeStats {chunksBaked, totalVoxelBytes}`
returned.

**Preload:** `PreloadChunksAroundCamera(world, x, y, z, radiusChunks)` enqueues
all chunks in radius. Called from `FramePreparation.cpp:147-156` if prebake ready.

**LOD downsample** (`src/voxel/VoxelLodDownsample.{hpp,cpp}`): CPU
`SurfacePreserveVote8` kernel (per TODO §30.4 verdict, current mainline). Step
sizes `1/2/4/8` for LOD 0/1/2/3+. Output: per-chunk `outExtent*outExtent*outExtent`
uint8 material array.

**GPU consume:** `lodDownsampledWords` SSBO (binding 8 в `voxel_mesh.comp`) +
`chunkLodLevelsBuffer` (binding 9). `LodDownsampleGpuConsume` handles refresh +
grow-on-exceed.

**Frame integration:** `FramePreparation.cpp:113-128` orchestrates `AssignLodLevels`
→ `RunLodDownsampleJobs` → `RefreshLodDownsampledBuffers`.

**Почему:** streamer = draw-distance не блокирует main thread. LOD downsample =
GPU render budget независим от world size. SurfacePreserve kernel = better
quality чем simple majority vote (preserves surface voxels).

## 28. Debug overlays + HUD

**Решение:**

**Debug HUD** (`src/debug/DebugHud.cpp`): `BuildDebugHudVertices(stats, camera,
interaction, hudVisible, extent, vertices, maxCount)`. `DEBUG_HUD_MAX_VERTEX_COUNT
= 262144` (`Types.hpp:276`). `kMaxStatsLineCount = 38`.

**Font:** supports `A-Z, 0-9, ., -, :`. Bracket/backslash/backtick spelled out
in helper text. Custom rendering via `DebugHudVertex` NDC quads.

**Per-frame `BuildDebugHudVertices` ordering:**
1. Title bar (FPS + frame time + scene preset).
2. Camera state (position, yaw/pitch, FOV, control mode).
3. Walk debug info (support state, feet position, grace timers, jump lock).
4. Render pass timings (mesh ms, graphics ms, TAA resolve ms, etc.).
5. Lighting + tone-map + debug view.
6. TAA state (jitter, blend, history valid).
7. Audio state (music, volume, position).
8. Input replay state.
9. World stats (chunk count, voxel count by material).

**Debug overlays** (`src/debug/DebugOverlays.cpp`): `BuildDebugOverlayBoxes(world,
interaction, debug, outBoxes)` produces world-axis-aligned
`Int3 min/maxExclusive` boxes.

**Overlay types:**
- Chunk bounds (если `debug->showChunkBounds`).
- Dirty chunk overlay (если `debug->showDirtyChunkOverlay`).
- Placement preview box.
- Cursor hit normal shaft (если `debug->showCursorHitNormal`).

**Toggles:**
- `F1` ToggleHud.
- `G` ToggleDetailedHud (normal vs detailed HUD).
- `F9` ToggleChunkBounds.
- `F10` ToggleDirtyChunkOverlay.
- `Z` ToggleCursorHitNormal.

**Per-pass CPU timing** (`RenderPassTimings`, `Types.hpp:652-662`): 6 measured +
1 derived (`otherMs`) + 1 count. `ScopedPassTimer` в `Renderer.cpp:27-46` для
graphics; manual `SDL_GetPerformanceCounter` для TAA resolve.

**Почему:** operator + agent нужны оба HUD tiers (normal для readability, detailed
для diagnostics). Overlays нужны для chunk management, dirty visualization,
placement preview, cursor hit.

## 29. LookDev capture + Benchmark automation

**Решение:**

**LookDev** (`src/app/LookDevCaptureAutomation.{hpp,cpp}`):
- `LookDevCaptureAutomationState {active, quitWhenDone, completed, warmupFramesRemaining,
  intervalFrames, intervalFramesRemaining, views[8], viewCount, nextViewIndex}`.
- `MAX_LOOK_DEV_CAPTURE_VIEW_COUNT = 8`.
- Env vars: `PROJECTV_LOOKDEV_CAPTURE_VIEWS` (comma-separated `LightingDebugView`
  names), `PROJECTV_LOOKDEV_WARMUP_FRAMES`, `PROJECTV_LOOKDEV_INTERVAL_FRAMES`,
  `PROJECTV_LOOKDEV_QUIT`.
- Per-view: warmup → interval → capture BMP + sidecar metadata → next view.
- After all views: `completed=true`; if `quitWhenDone` → `SDL_APP_SUCCESS` from
  `SDL_AppIterate`.

**Benchmark** (`src/app/BenchmarkAutomation.{hpp,cpp}`):
- `BenchmarkAutomationState {active, quitWhenDone, completed, warmupFramesRemaining,
  targetFrameCount, framesRendered, logEveryFrames, startCounter, firstFrameCounter,
  lastFrameCounter, totalFrameSeconds, minFrameSeconds, maxFrameSeconds}`.
- Env vars: `PROJECTV_BENCHMARK_FRAMES`, `PROJECTV_BENCHMARK_WARMUP_FRAMES`,
  `PROJECTV_BENCHMARK_LOG_EVERY`, `PROJECTV_BENCHMARK_QUIT`.
- 30-frame default warmup, 60-frame log interval.

**Screenshot capture** (`src/render/ScreenshotCapture.{hpp,cpp}`):
- BMP writer (4 B/pixel BGRA), path = `${repo}/runtime/captures/<preset>_<seq>.bmp`.
- Sidecar metadata: `key=value` lines (scene preset, lighting params, render
  pass timings, camera state). Uses 2 `fmt::format` blocks из-за 99-arg compile-time
  limit.
- Fenced via in-flight fence in `SaveRequestedScreenshot` (`Renderer.cpp:183-251`).

**Input replay** (`src/app/InputReplay.{hpp,cpp}`):
- Capture: `R` toggle recording → write `capture.frames[]` (mouse deltas +
  `actionDownMask` + `actionPressedMask` + remove/place flags).
- Replay: `Y` → `playbackRequested=true` → `StartLastInputReplayPlayback` loads
  snapshot + restores camera/interaction + sets `replay.playbackActive=true`.
- Per-frame: `PrepareNextInputReplayPlaybackFrame` applies frame to input state.
- File path: `PROJECTV_INPUT_REPLAY_DIR` env var.
- 64 actions max (`InputAction::Count ≤ 64` enforced by `static_assert` в
  `Types.hpp:173-177`).

**Почему:** deterministic capture = visual regression targets. Benchmark = perf
regression measurement. Replay = bug reproduction sharing. All env-driven для
CI integration.

## 30. Vulkan bootstrap invariants

**Решение:** `projectv::render::VulkanBootstrap` (`src/render/vulkan/VulkanBootstrap.cpp`).

**`VkDeviceCreateInfo::pNext` chain must NEVER have nullptr gaps** — `sType`
initialized at declaration, `pNext` linked unconditionally, feature *fields*
gated on `selected.supports*`. RTX (`VK_KHR_acceleration_structure`,
`VK_KHR_ray_query`) + `VK_KHR_swapchain_maintenance1` + `VK_KHR_deferred_host_operations`
+ `VK_KHR_buffer_device_address` + mesh shader always linked.

**Per-platform `VOLK_STATIC_DEFINES`:** `VK_USE_PLATFORM_WIN32_KHR` /
`VK_USE_PLATFORM_MACOS_MVK` / `VK_USE_PLATFORM_ANDROID_KHR` /
`VK_USE_PLATFORM_XCB_KHR` (else branch).

**Volk = static loader** (no dynamic dispatch), `external/volk` submodule.

**VMA** (Vulkan Memory Allocator) — linked via `GPUOpen::VulkanMemoryAllocator`
target, `SYSTEM TRUE` для suppress warnings. `target_compile_options(
VulkanMemoryAllocator INTERFACE -Wno-nullability-completeness)`.

**Swapchain:** present mode cycle auto-detected от
`vkGetPhysicalDeviceSurfacePresentModes`. FIFO always available (guaranteed by
Vulkan spec); MAILBOX/IMMEDIATE optional. `g_active` preserved across rebuilds.

**Почему:** per-platform loader dispatch + unconditional pNext linking = safe
RTX + mesh shader device creation on любом Vulkan 1.4-capable GPU. VMA = standard
allocator (suballocations, defragmentation, budget tracking).

## 31. Vulkan synchronous fences + frame pacing

**Решение:** `MAX_FRAMES_IN_FLIGHT = 2` (`Types.hpp:40`).

**Per-frame state** (`FrameState`, `Types.hpp:920-927`):
- `currentFrame` (0..1 cycle).
- `commandBuffers[MAX_FRAMES_IN_FLIGHT]`, `imageAvailableSemaphores[]`,
  `renderFinishedSemaphores[]`, `inFlightFences[]`.

**Frame pacing:**
- `vkAcquireNextImageKHR(UINT64_MAX)`.
- `vkWaitForFences(UINT64_MAX, inFlightFence)`.
- `vkResetFences`, `vkResetCommandBuffer(0)`, `vkBeginCommandBuffer`.

**Fence timeout for prep work:** `kVulkanFenceWaitTimeoutNs = 10'000'000` (10ms)
+ fallback `kVulkanFenceWaitTimeoutUnboundedNs = UINT64_MAX` для cases where
GPU is slower than 10ms (heavy frame).

**Tracy GPU context:** `CreateTracyGpuContext` initializes
`tracyGraphicsContext` with calibrated timestamps если
`vkGetPhysicalDeviceCalibrateableTimeDomainsEXT` +
`vkGetCalibratedTimestampsEXT` available.

**Почему:** 2-frame-in-flight = balance между latency и throughput. Persistent
fence + per-frame cmd buffer = optimal batching. Tracy = per-pass timing
attribution для bottleneck analysis.

## 32. Static-analysis cleanup contract

**Решение:**
- Checked-in `Problems/*.xml` inspection exports = hints, не source of truth.
- During warning cleanup, patch only issues that still reproduce on current source
  or are trivially visible в current code.
- After meaningful cleanup pass, regenerate `Problems/` before starting the next
  pass.
- **Fix, don't silence**: подавление warnings запрещено (per AGENTS.md §5.7).
  False-positive = local `#pragma` / `[[maybe_unused]]` с documented reason.

**Почему:** cleanup of stale Problems/ exports wastes cycles on already-fixed
issues. False-positive suppression globally masks future real warnings.

## 33. Per-pass CPU timing

**Решение:** `RenderPassTimings` struct (`Types.hpp:652-662`):
```cpp
struct RenderPassTimings {
    float shadowMs = 0.0f;        // unused post-5.2.D
    float meshingMs = 0.0f;       // voxel meshing compute dispatch
    float graphicsMs = 0.0f;       // full graphics pass
    float taaResolveMs = 0.0f;     // TAA resolve
    float debugOverlayMs = 0.0f;   // debug overlay pass
    float debugHudMs = 0.0f;      // debug HUD pass
    float otherMs = 0.0f;          // derived (total - sum)
    uint32_t dirtyChunkRebuiltCount = 0u;
};
```

**Mechanism:** `ScopedPassTimer` (`Renderer.cpp:27-46`) — RAII timer using
`SDL_GetPerformanceCounter` + `SDL_GetPerformanceFrequency`. TAA resolve has
manual start/end pair (`Renderer.cpp:999,1035-1038`) because it's a separate
dynamic rendering pass.

**`MirrorRenderPassTimingsToDebugStats`** in `AppUpdate.cpp:243-257` copies
timings to `DebugStats` for HUD display.

**Почему:** per-pass attribution = bottleneck identification. RAII timer
не может быть забыт (auto-stop at scope exit). Separate TAA resolve timing
because it runs after main graphics pass.

## 34. Frame-step / slow-motion debug

**Решение:** `simulation.timeScale` (default 1.0) применяется в
`UpdateEffectivePausedAndEditing`:
- `frameDeltaSeconds *= timeScale` (slow-mo).
- `timeScale = 0` ≠ `paused` (independent axes; can be both `paused && timeScale=0`).
- `effectivePaused = paused && !frameStepRequestedNow` (frame-step = temporary unpause).

**Hotkeys** (`InputActions.cpp:168-171`):
- `[` DecreaseTimeScale (× 0.5); snaps to 0 if < 0.01.
- `]` IncreaseTimeScale (× 2.0); snaps from 0 to 0.5.
- `\` StepSingleFrame (`simulation.frameStepRequested = true`).
- `` ` `` ResetTimeScale (1.0).
- `P` TogglePause.

**Frame-step does NOT invalidate TAA history** (vs swapchain resize which does).

**Почему:** debug-friendly slow-mo без полной паузы (camera look остаётся).
Frame-step = examine single-tick state without TAA history garbage.

## 35. Shader hot-reload (dev-time)

**Решение:** `1` hotkey → `RebuildAllShadersFromDisk` (`main.cpp:57-80`):
- Reads `PROJECTV_BUILD_DIR` env var (default: `PROJECTV_CMAKE_BUILD_DIR` =
  `build/linux-clang-debug` injected via `target_compile_definitions`).
- Runs `cmake --build <buildDir> --target Shaders` via `std::system`.
- Log path: `temp_directory_path() / projectv_shader_reload.log`.

**EVIL: `std::system` call** — consciously used для dev-time hot-reload
(operator `// EVIL:` comment in source). Production paths не trigger this.

**Shader variants** built via `add_custom_command` per shader:
- 24 base shaders (`SHADERS` list в `src/CMakeLists.txt:28-57`).
- 4 SPIR-V variants of `voxel.frag` (lines 73-104).
- 1 SPIR-V variant of `model.frag` (lines 106-115).
- = 29 `.spv` total.

**`CopyShaders` custom target** copies `.spv` → `$<TARGET_FILE_DIR:ProjectV>`
(per-session fix: всегда runs, depends on `Shaders`).

**Почему:** shader iteration cycle без full rebuild = 5× faster iteration.
Production builds never hit this path (только в interactive dev).

## 36. VolumetricFog + SkyAtmosphere + Cloudscape (env-gated, optional)

**Решение:** 3 дополнительных visual subsystems, gated by env vars.

**VolumetricFog** (`src/render/VolumetricFog.{hpp,cpp}`):
- Wronski 2014 froxel-grid ray-march.
- 3D froxel image (`volumetricFogFroxelImage`, RGBA16F) + fallback image (binding 12
  в `voxel.frag`).
- `PROJECTV_FOG=ON` gate (default OFF).
- `volumetric_fog.comp` writes fog density per froxel.
- `voxel.frag` consumes via `sampler3D volumetricFog` (binding 12).

**SkyAtmosphere** (`src/render/SkyAtmosphere.{hpp,cpp}`):
- Preetham-style analytical sky (Hillaire 2020 EGSR).
- Sky-View LUT (256×128 RGBA16F) + Multi-Scattering LUT (32×32 RGBA16F) optional
  via `PROJECTV_SKY_LUT=ON`.
- `PROJECTV_SKY=ON` gate.
- `kPlanetRadius`, `kAtmosphereHeight`, Rayleigh βR/G/B, Mie β, g constants.
- `sky_atmosphere.{vert,frag}` — full-screen triangle pre-pass.

**Cloudscape** (`src/render/Cloudscape.{hpp,cpp}`):
- Single-layer ray-march cloudscape (Nubis 2017 reference).
- `PROJECTV_CLOUDS=ON` gate.
- `cloudscape.{vert,frag}` — full-screen composite.
- Composite после opaque + transparent, перед swapchain present.

**Default constants** (`src/shaders/common/common_constants.glsl`):
`kFogDepthDistributionExp`, `kFogDepthDistributionScale`, `kFogDepthDistributionBias`
shared between `volumetric_fog.comp` and `voxel.frag`.

**Почему:** 3 subsystems — optional visual upgrades. Default OFF для predictable
benchmark/regression numbers. ON = full atmospheric look.

## 38. Phase 4 hardware gates (env + capability)

| Gate | Env | Notes |
|:-----|:----|:------|
| HZB min-mips | `PROJECTV_HZB_MIN_MIP` (default ON) | compute min-reduction; OFF = LINEAR blit |
| Bindless PostFX composite | `PROJECTV_BINDLESS=ON` | variable-count sampled array in `post_composite_bindless.comp` |
| Host image copy | `PROJECTV_HOST_IMAGE_COPY=ON` | cloud noise upload path |
| Uint8 indices | (capability) | small meshes when `indexTypeUint8` |
| Local-read | `PROJECTV_DYNAMIC_RENDERING_LOCAL_READ=ON` | feature enabled; **no consumer yet** — PostFX is compute-only |
| FloatControls2 | (capability) | declared in `world_gen.comp` (`GL_EXT_shader_float_controls2`) |
| Present wait | `PROJECTV_PRESENT_WAIT=N` | needs present_id + present_wait |
| SER | `PROJECTV_RTX_SER=ON` | `VK_NV_ray_tracing_invocation_reorder` |
| OMM | — | blocked on alpha-tested assets (see §14) |

## 37. RT shadow pipeline path selection

**Decision:** `src/render/RtxShadowPipeline.cpp` supports two paths selected at runtime by
`PROJECTV_RTX_PIPELINE_LIBRARY`:

- `ON` / `1` / unset on capable device: pipeline-library path with batched deferred host
  operations. Cold-start join ~15 ms, slightly higher per-frame cost (~1.8 ms vs ~1.6 ms in
  VoxelLab 10-frame smoke).
- `OFF` / `0`: monolithic `vkCreateRayTracingPipelinesKHR` path. Cold-start ~8 ms, lower
  per-frame cost.

**Why:** The pipeline-library path is architecturally cleaner for incremental updates and
shader hot-reload, but on the current NVIDIA driver it compiles ~2× slower for cold start.
The env var lets us choose correctness/experimentation (library) vs. launch latency/FPS
(monolithic) without rebuilding.

**Async note:** Offloading the library-path compile to a worker thread via
`VkDeferredOperationKHR` deadlocks on this driver when the main thread is submitting frames.
The attempt is archived at `legacy/docs/archive/2026-07-14-task34-attempt/`.

**Deferred init (Phase 4.0):** `RayTracedShadows::Initialize` no longer blocks on
`CreateVoxelAwareRtxResources`. It sets `m_voxelAwareRtxPending` and the draw loop polls
`TryFinishVoxelAwareRtxResources` until SBT/mask are ready (AABB ray-query fallback until then).
Still no worker-thread deferred ops while submitting.

**HZB min-mips (Phase 4.0):** `hiz_minify.comp` + `PROJECTV_HZB_MIN_MIP` (default ON).
Mip0 = NEAREST blit from depth; mips 1..N = compute 2×2 min-reduction (push-descriptor when available);
`PROJECTV_HZB_MIN_MIP=OFF` keeps LINEAR blit fallback.

**Stress bench for 3.2/3.3 re-entry:** use `PROJECTV_SCENE_PRESET=MeshingStress` or `ChunkGrid`
(not VoxelLab). Require ≥10% Tracy/NSight before re-landing; archives remain under
`legacy/docs/archive/2026-07-14-task32-attempt/` and `...-task33-attempt/`.

---

# Part B — Runtime facts

## R1. Linux dev host baseline (post-reset, 2026-06-25)

- **OS:** Linux 7.x kernel (конкретный uname re-validate на dev-хосте).
- **Compiler:** clang 22.1.6 (`/usr/bin/clang++`).
- **Linker:** LLD 22.1.6 (`/usr/bin/ld.lld`).
- **stdlib:** libc++ 22 + libstdc++ 16.1.1 (hybrid link).
- **Build system:** CMake 4.3.3, Ninja 1.13.2.
- **Shader compiler:** `glslc` (preferred) или `glslangValidator` (mandatory fallback)
  from Vulkan SDK 1.4.350.
- **Vulkan SDK:** 1.4.350 (target = vulkan1.3 в shader compile).
- **SDL3:** 3.4.10.
- **GPU:** NVIDIA RTX 3060 Ti (Ampere, 2nd gen RT cores, 38 SMs).
- **VMA:** Vulkan Memory Allocator (vendor: GPUOpen, AMD).
- **Submodules:** 14 (SDL, volk, VMA, fmt, imgui, flecs, JoltPhysics, tracy, glm,
  meshoptimizer, fastgltf, draco, miniaudio, benchmark).
- **Build state (post-reset):** green (`build/linux-clang-debug/bin/ProjectV` exists).
- **Test state:** 39 ctest-registered executables. ctest `linux-clang-debug-tests`
  passes (per session 24x record 37/37 + 4 RTX sub-tests in 25x).

## R2. GPU-specific facts (NVIDIA RTX 3060 Ti)

- **RT cores:** 38 (Ampere, 2nd gen). Peak ~10 GRays/sec.
- **VoxelLab reference (1080p × 120 FPS):**
  - 1 shadow ray/pixel = 248 MRays/sec shadow rays = ~0.65% RT core utilization.
  - 6-cone VCT diffuse = ~1.5 GRays/sec = ~15% utilization.
  - Full RTX-driven (shadows + AO + GI + specular) = 18 rays/pixel = ~5-15% utilization.
- **Vulkan 1.4 support:** full (incl. `VK_KHR_ray_tracing_pipeline`,
  `VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, `VK_KHR_swapchain_maintenance1`).
- **Validation layers:** enable в Debug, disable в Release.
- **VMA:** heap size 4 GiB default, prefer `DEVICE_LOCAL` for GPU resources,
  `HOST_VISIBLE | HOST_COHERENT` for CPU-mapped buffers (scene descriptors,
  voxel payload, debug HUD, screenshot readback).

## R3. Module / build quirks

- **`import std;` probe-only:** `tests/StdModuleProbe.cpp` + `tests/CMakeLists.txt:334-375`
  тестируют precompiled `std.pcm` от libc++ 22. В mainline НЕ используется
  (libc++ 22 std.cppm конфликтует с fmt headers).
- **C++23 module gate:** `CMAKE_CXX_MODULE_STD` option (default ON на Linux non-MSVC).
- **flecs deprecation warnings:** `/wd4996` (MSVC) / `-Wno-deprecated-declarations`
  (Linux Clang) suppressions в `projectv_build_options` (per AGENTS.md §5.7).
- **Tracy UI standalone:** built via `tools/tracy-standalone/build-tracy-{windows,linux}.sh`,
  НЕ via `add_subdirectory` (CMP0002 collision on nlohmann_json target).
- **Linux Tracy UI:** disabled (`PROJECTV_BUILD_TRACY_PROFILER=OFF`) из-за
  upstream `tidy-html5` bug; instrumentation (`PROJECTV_ENABLE_TRACY=ON`) остаётся.

## R4. Save/load paths

- **Snapshot:** `latest.projectv.snapshot.bin` (per `GetVoxelWorldSnapshotPath`),
  binary format, includes `Sparse64Tree` root + nodes.
- **Input replay:** `latest.projectv.replay` (binary) + paired snapshot
  в `PROJECTV_INPUT_REPLAY_DIR`.
- **Screenshot:** `runtime/captures/<preset>_<seq>.bmp` + `.meta.json` sidecar.
- **Scene config:** `runtime/scene.json` (declared `EnsureDefaultSceneConfig`).
- **Chunk cache:** `${PROJECTV_CMAKE_BUILD_DIR}/cache/chunks/chunk_N.bin`
  (PKV1 magic + voxelBytes).

## R5. Hot reload + dev iteration

- **Shader hot-reload:** `1` key in dev mode.
- **Present mode cycle:** `3` key.
- **VMA allocations:** per-frame `deferredNanoVdbDestroys` queue (avoid
  in-flight GPU read races).
- **sccache:** `CMAKE_CXX_COMPILER_LAUNCHER sccache` в dev presets
  (linux-clang-debug, linux-clang-release). CI presets также с sccache.

## R6. Vulkan 1.4 Phase 3 performance notes (2026-07-14)

- **Tracy CLI capture tools now build:** `tools/tracy-capture-cli` is a minimal CMake
  project that builds `tracy-capture` and `tracy-csvexport` without the heavy CPM vendor
  downloads in `external/tracy/cmake/vendor.cmake`. It uses system `libzstd` and downloads
  only `capstone` 6.0.0-Alpha9 and `ppqsort` 1.0.6 via `FetchContent`. The two executables
  need separate `TracyServer` static libraries because capture requires
  `TRACY_NO_STATISTICS` while csvexport requires statistics support.
- **NSight Graphics CLI baseline established:** `/opt/nsight-graphics/latest/host/linux-desktop-nomad-x64/ngfx-capture` and `ngfx-replay --perf-report-dir` work. Example baseline for current `main` (frame 45, VoxelLab, validation ON): replayAdjustedFps ≈ 572. Use this as the Phase 3 performance gate for tasks 3.2-3.4.
- **Tracy CLI capture pipeline automated:** `tools/linux/Invoke-ProjectVTracyCapture.sh` runs ProjectV with `PROJECTV_BENCHMARK_FRAMES` + `PROJECTV_BENCHMARK_QUIT=1` auto-quit, connects `tracy-capture` to the app's Tracy server, and saves the `.tracy` file when the app exits. Capture-to-CSV analysis is still manual via `tracy-csvexport`.
- **Benchmark auto-quit fix:** `UpdateBenchmarkAutomation` must keep returning `quitWhenDone` after `completed` is set, because multiple `flecs::world::progress()` calls per frame would otherwise overwrite the quit request before `SDL_AppIterate` reads it.
- **Push descriptors (Task 3.1):** implemented with runtime fallback. When
  `context.features14.pushDescriptor == VK_TRUE`, RT shadow and DDGI compute passes use
  Vulkan 1.4 core `vkCmdPushDescriptorSet` (not the `KHR` alias) and skip descriptor-pool
  allocation. The old `vkUpdateDescriptorSets` + `vkCmdBindDescriptorSets` path remains for
  hardware without the feature.
- **TLAS refit (Task 3.5):** implemented. BLAS and TLAS builds use
  `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR`. When the visible instance count
  matches the previous frame and the update scratch size fits in the pre-allocated scratch
  buffer, `RecordTlasBuild` uses `VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR` with
  `srcAccelerationStructure = m_config.tlas`; otherwise it falls back to `BUILD` mode.
- **RT pipeline libraries + deferred host ops (Task 3.4):** attempted twice and rolled back.
  Baseline cold-start monolithic `vkCreateRayTracingPipelinesKHR` in `RtxShadowPipeline.Initialize`
  is ~8.1 ms (Tracy, 120-frame VoxelLab capture). A second attempt split the pipeline into
  ray-gen, miss, and procedural-hit-group libraries with `VK_PIPELINE_CREATE_LIBRARY_BIT_KHR`,
  used deferred host operations, and ran joins with the concurrency requested by
  `vkGetDeferredOperationMaxConcurrencyKHR` (rayGen=1, miss=1, hitGroup=2). Tracy per-stage
  breakdown: library kickoffs ~0.2–0.5 ms each, deferred joins ~10–16 ms in parallel, final link
  ~0.5 ms, main-thread `CreatePipeline` total ~17.6 ms. Even with the driver-recommended number
  of join threads the library path is ~2× slower because the RT shadow pipeline is too small
  (3 shader groups) for library+link overhead to amortize. The attempt is archived at
  `legacy/docs/archive/2026-07-14-task34-attempt/`. `VK_KHR_pipeline_library` probing and
  conditional enablement remain; the monolithic path stays with a Tracy zone.
- **Known pre-existing validation noise:** DDGI irradiance/distance images trigger
  `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` vs `VK_IMAGE_LAYOUT_GENERAL` layout warnings at
  `vkQueueSubmit2`. This exists before Phase 3 changes and is unrelated to push descriptors
  or TLAS refit.
- **HZB-driven indirect RT shadow dispatch (Task 3.2):** implemented and rolled back.
  VoxelLab 120-frame bench: `PROJECTV_RTX_HZB_INDIRECT=OFF` mean_ms=2.008 / 498 fps;
  `ON` mean_ms=2.057 / 486 fps (~+2.4% slower). Gate requires ≥10% improvement. Compaction
  + mask clear + barriers outweigh cull savings on a small scene; HZB mips are LINEAR-filtered
  (not min-reduction), so coarse mip culling is approximate. Attempt archived at
  `legacy/docs/archive/2026-07-14-task32-attempt/`. Validation smoke with ON was clean.
  Env var `PROJECTV_RTX_HZB_INDIRECT` is not wired in tree after rollback.
- **GPU-driven indirect mesh dispatch (Task 3.3):** implemented and rolled back.
  VoxelLab 120-frame bench (`PROJECTV_MESH_SHADER_PIPELINE=ON`):
  `PROJECTV_MESH_SHADER_INDIRECT=OFF` mean_ms=2.069 / 483 fps;
  `ON` mean_ms=2.101 / 476 fps (~+1.5% slower). Gate requires ≥10% improvement.
  Attempt archived at `legacy/docs/archive/2026-07-14-task33-attempt/`. Env var not wired after rollback.

---

# Cross-references

- `AGENTS.md §4` — sources of truth ranking.
- `AGENTS.md §5.3` — Web search обязателен для сложных тем.
- `AGENTS.md §5.4` — Git safety (no auto-commit, safety-net на грязном дереве).
- `AGENTS.md §5.7` — comments inline one-line, `// EVIL:` для magic numbers.
- `TODO.md §5.2` — RTX shadows milestones (A/B/C/D closed; E = voxel-aware,
  22x closed; refactor 23x, refraction/DDGI/multi-bounce 23x).
- `TODO.md §5.4` — RTX AO (replace DDA, closed 20x).
- `TODO.md §5.5` — DDGI probes (closed 23x).
- `TODO.md §5.6` — RTX refraction (closed 23x).
- `TODO.md §5.7` — RTX multi-bounce GI (closed 23x).
- `TODO.md §7.x` — post-RTX-shadow polish (VCT cones, TAA, tonemap, post-FX).
- `legacy/docs/archive/2026-06-24-pre-reset-snapshot/knowledge.md` — pre-reset
  contracts (historical, not authoritative).
