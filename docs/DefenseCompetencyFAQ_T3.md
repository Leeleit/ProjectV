# Defense Competency FAQ — Тиммейт 3 (Рендеринг)

**Участник:** [Имя Тимейта 3]
**Реальная компетенция:** Рендеринг (Vulkan 1.4, TAA, CSM, AOCC, шейдеры, C-ядро frustum cull)
**Speech slot на сцене:** T4 Тесты и проверки (2:40-3:20)
**Verbatim текст выступления:** `docs/DefenseScript_Team.md` → раздел «Участник 4 (Тесты и проверки)»

**Out of scope (к кому перенаправлять в Q&A):** архитектура выбора API — к le1t; стек/сборка — к Тиммейту 1; воксельный мир — к Тиммейту 2; физика — к Тиммейту 4; демо — к Тиммейту 5; все баги — к le1t.

**Common (стек, метрики, хоткеи, glossary, chronology):** `docs/DefenseCompetencyFAQ.md`

---

## 3.1. Кто ты

**Легенда:** ты отвечал за рендеринг — Vulkan 1.4 init, graphics pipeline, TAA, CSM, AOCC, шейдеры, C-ядро frustum culling. Это самый большой и технически насыщенный модуль.

**На сцене:** ты говоришь T4 (Тесты и проверки) — обзор тестов с акцентом на визуальные (render) тесты.

**На Q&A:** ты отвечаешь на вопросы про **рендеринг, шейдеры, TAA, CSM, AOCC, C-ядро, освещение**.

## 3.2. Твоя компетенция: Рендеринг

**Файлы (самая большая категория):**
- `src/render/Renderer.{hpp,cpp}` — `DrawFrame` (главный per-frame entry point, ~1600 строк)
- `src/render/SceneResources.{hpp,cpp}` — chunk visibility cache, XOR-fold splitmix64 hash, `IsSceneChunkVisible`, `IsSceneChunkVisibleInShadowCascade`, `IsAabbVisibleAgainstCameraFrustum`
- `src/render/ShadowProjection.{hpp,cpp}` — 4-cascade projection, `BuildSunShadowCascadeSplits(near, far, lambda=0.80)`
- `src/render/Taa.{hpp,cpp}` — `AdvanceTaaPixelJitter`, `BuildTaaHistoryParams`, `BuildTaaLayerHistoryParams`
- `src/render/RayMarchPass.{hpp,cpp}` — STUB, `SetRayMarchEnabled`, `RecordRayMarchCommands` (no-op)
- `src/render/ScreenshotCapture.{hpp,cpp}` — `.bmp` + `.txt` sidecar writer
- `src/render/vulkan/VulkanInit.{hpp,cpp}` — Vulkan 1.4 init, `VulkanInitError` enum (16 вариантов)
- `src/render/vulkan/VulkanGraphicsPipeline.{hpp,cpp}` — graphics pipelines (TAA-aware variants)
- `src/render/vulkan/VulkanVoxelMeshingPipeline.{hpp,cpp}` — compute-шейдер пайплайн
- `src/render/vulkan/TaaResolvePipeline.{hpp,cpp}` — TAA resolve
- `src/render/vulkan/VulkanSwapchain.{hpp,cpp}` — swapchain, present modes
- `src/render/vulkan/VulkanBootstrap.{hpp,cpp}` — Vulkan loader
- `src/render/vulkan/VulkanDebug.{hpp,cpp}` — debug utils
- `src/render/ShadowTypes.hpp` — shadow types
- `src/render/TaaRenderTargets.{hpp,cpp}` — TAA render target management
- `src/c_kernels/frustum_cull.{hpp,c}` — С-ядро
- `src/c_kernels/FrustumCulling.hpp` — C++ обёртка
- `src/shaders/*.comp/.frag/.vert` — все шейдеры (12 файлов)

**Vulkan 1.4 init (`VulkanInit.hpp:19-58`):**
- 16 `VulkanInitError` enum вариантов: `PreconditionFailed`, `BootstrapFailed`, `TracyContextFailed`, `SwapchainFailed`, `WorldCreationFailed`, `EcsSyncFailed`, `PhysicsStateFailed`, `SceneResourcesFailed`, `GraphicsPipelineFailed`, `VoxelMeshingPipelineFailed`, `ModelPipelineFailed`, `ModelManifestFailed`, и т.д.
- Tier 1.B: `std::expected<void, VulkanInitError>` вместо `bool` (cold path, 1× per startup)
- `InitVulkan(AppState*)` — мутирует `AppState` in place, error variant — machine-readable сигнал

**Shadow cascade (per `ShadowProjection.hpp`, `decisions.md §18`):**
- 4 каскада (`kSunShadowCascadeCount = 4`)
- Shadow map 2048×2048
- `BuildSunShadowCascadeSplits(near, far, lambda=0.80)` — near-biased split distribution
- Per-cascade projection: sub-frustum → light-space → sphere stabilization (чтобы избежать jitter при движении камеры)

**Per-preset shadow params (`VoxelMaterials.cpp:181-236`):**
```cpp
// VoxelLab
sunShadowParams = {0.72f, 0.0009f, 0.0060f, 1.10f}  // strength, depthBias, normalBias, filterRadius
sunContactShadowParams = {0.28f, 2.25f, 0.0f, 0.0f}  // strength, maxDistance, reserved, reserved

// FlatBenchmark
sunShadowParams = {0.64f, 0.0008f, 0.0055f, 1.25f}

// TransparencyStress
sunShadowParams = {0.76f, 0.0010f, 0.0040f, 1.30f}

// ChunkGrid
sunShadowParams = {0.80f, 0.0010f, 0.0070f, 1.50f}

// MeshingStress
sunShadowParams = {0.88f, 0.0009f, 0.0060f, 1.35f}
```

**TAA (Temporal Anti-Aliasing, per `Taa.hpp`):**
- 8-tap Halton(2,3) sub-pixel jitter sequence (in pixel units relative to rasterization center)
- По умолчанию `taaEnabled=false` (jitter=0) — стабильная картинка, нет дрожания
- YCoCg color space clamp в `taa_resolve.frag:170-190` — не вымывает chroma на ярких highlight'ах
- Per-layer history (CTSH/AOCC/LOCL) — `mix(rawCurrent, history, blend=0.4)`, per `agent/decisions.md §18`
- CTSH history **не** blended (deferred — separation refactor)
- Sidecar metadata: `taaEnabled`, `taaJitterX/Y`, `taaBlend`, `taaNeighbourhoodRadius`

**3 tone-map оператора (`VoxelMaterials.hpp:12-16`):**
- `Linear = 0` — no tone mapping
- `Reinhard` — Reinhard operator, `color / (color + 1.0)`
- `AcesApprox` — default, ACES filmic approximation
- Cycle клавишей `N`

**3 exposure metering modes (`VoxelMaterials.hpp:18-21`):**
- `Manual = 0` — fixed exposure bias
- `SceneKey` — auto-exposure based on scene key (luminance percentiles)

**6 shadow tuning targets (`VoxelMaterials.hpp:36-43`):**
- `Strength`, `DepthBias`, `NormalBias`, `FilterRadius`, `Coverage`, `CascadeBlend`
- Cycle клавишей `O`, decrease/increase клавишами `U`/`I`, reset `V`

**10 lighting debug views (`VoxelMaterials.hpp:23-34`):**
- `Final`, `Ambient`, `Direct`, `Local`, `Shadow`, `Cascade`, `Contact`, `Occlusion`, `Fog`, `Taa`
- Cycle клавишей `B` — каждое нажатие переключает слой для визуальной диагностики
- Sidecar `lighting_debug_view` enum value

**Three MRT attachments on voxel pass (`agent/decisions.md`):**
- `outColor` (Location 0) — main color
- `outSceneColor` (Location 1, TAA-on variant) — для TAA input
- `outLayerMask` (Location 2, R=CTSH, G=AOCC, B=LOCL, A=1.0) — packed R8G8B8A8
- Per-frame `vkCmdCopyImage` `taaLayerSceneColorTarget` → `taaLayerHistoryColorTarget`

**Greedy meshing compute shader (per `voxel_mesh.comp:613-619`):**
- 6 per-axis greedy passes
- W×H quad packing в 6+6 бит = 12 бит (max 64×64 per quad)
- `kMaxChunkExtentForGreedy` fallback на per-voxel emission

**6 smoke captures (per `LookDevCaptureAutomation.hpp`):**
- `FINAL` — composite (all effects applied)
- `SHDW` — только cascade shadows
- `CSM` — визуализация каскадов (split planes)
- `CTSH` — контактные тени (sun-to-fragment ray)
- `AOCC` — фоновое затенение (12 traces per fragment)
- `LOCL` — локальный точечный свет
- Запуск: `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh`
- Sidecar `.txt` 60+ ключей

**С-ядро (frustum cull, per `c_kernels/FrustumCulling.hpp`):**
- `projectv_cull_frustum_scalar` — **3.7-3.9× быстрее** C++ baseline (per `src/bench/FrustumCullBenchmark.cpp`)
- `projectv_cull_frustum_avx2` (с `__AVX2__`) — 2.5-2.7× (autovectorizer в debug бьёт hand-rolled `_mm256_setr_ps`)
- Crossover threshold: `kBatchDispatchThreshold = 8` AABBs (ниже — inline C++ helper)
- Scalar рекомендуется на текущей AoS layout; AVX2 даст выигрыш при SoA реорганизации

**Ray-march pass — STUB (`RayMarchPass.cpp:59-78`):**
```cpp
void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData) {
    // NO-OP STUB
    std::fprintf(stderr, "[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up...)\n");
}
```
- Compute-шейдер `ray_march.comp` (Amanatides-Woo DDA) скомпилирован
- API state работает: `SetRayMarchEnabled(bool)`, `IsRayMarchEnabled()`, `RequestRayMarchPipelineRecreate()`
- В graphics command stream не вкомпонован
- Phase 7 follow-up (per `docs/DefenseReport.md §3`)

**Hot shader reload (defense r0, 2026-06-15 relocation):**
- Клавиша `1` (было F5/F11 до relocation 2026-06-15)
- `RebuildAllShadersFromDisk()` в `src/app/main.cpp:68-114`
- `cmake --build $BUILD_DIR --target Shaders` (recompiles `.comp/.frag/.vert` через `glslc`)
- Log: `std::filesystem::temp_directory_path()/projectv_shader_reload.log` (cross-platform: Windows `%TEMP%`, Linux `/tmp`)
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Stderr: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`

**V-sync cycle (relocation, defense r0):**
- Клавиша `3` (было `V` до relocation)
- `CyclePreferredPresentMode()` в `VulkanSwapchain.cpp`
- `BuildPresentModeCycle` — built once per swapchain create from surface's supported modes
- Cycle length: usually 2 on Linux/Wayland (no IMMEDIATE), 3 on Windows
- `RecreateSwapchain` forced at end of branch

**3-rd MRT layer mask (`agent/decisions.md`):**
- Per-frame `vkCmdCopyImage` → `taaLayerHistoryColorTarget`
- Fragment shader samples `sampler2D layerHistory` (binding 6, graphics descriptor set)
- `mix(rawCurrent, history, blend=0.4)` applied to AOCC + LOCL
- CTSH written to history but **not** blended (deferred — cascade/contact shadow separation refactor)

## 3.3. Что смотреть на защите

**Слайд 5** (твой) — Тесты. 14 ctest наборов + 6 smoke captures + sidecar.

**Демо во время T2 (le1t):** VoxelLab, облёт, TAA toggle, debug view cycle (B), tone map cycle (N), shadow tuning (O/U/I), capture (C).

**Cycle debug-views клавишей `B`:** FINAL → SHDW → CSM → CTSH → AOCC → LOCL → AMBIENT → DIRECT → LOCAL → FOG → TAA → FINAL. Каждый визуализирует отдельный слой.

## 3.4. Реалистичные вопросы (5-7)

**Q1. Почему Vulkan 1.4, а не OpenGL?**
- Vulkan — явный контроль GPU (пайплайны, память, синхронизация)
- OpenGL — driver управляет, дорого для миллионов draw items
- Вычислительные шейдеры нужны для мешинга и трассировки
- Кросс-платформенный (Windows + Linux)

**Q2. Как работают каскадные тени?**
- 4 каскада карты глубины 2048×2048
- Лямбда 0.80 — near-biased распределение (ближние объекты получают больше плотности)
- Per-cascade projection: sub-frustum → light-space → sphere stabilization
- Стекло не отбрасывает тень, жидкость — отбрасывает (per `decisions.md`)

**Q3. Что такое TAA и зачем?**
- Временное сглаживание: смешивает кадры, убирает дрожание камеры
- 8-sample Halton(2,3) jitter, YCoCg-зажим
- По умолчанию ВЫКЛЮЧЕН (`taaEnabled=false`, jitter=0) — стабильная картинка

**Q4. Что такое AOCC?**
- Ambient Occlusion Cavity Check — локальное затенение полостей
- 12 traces per fragment (per `decisions.md`)
- Не полноценный SSAO — компактный, встроенный в lighting term
- Per-layer history blended (CTSH нет)

**Q5. Что такое контактные тени?**
- Короткая трассировка луча от фрагмента к солнцу
- Дополняет CSM там, где разрешения карт глубины не хватает
- Ограниченный прямой шейдерный проход, не отдельный render pass

**Q6. Зачем нужен С-ядро для frustum cull?**
- Scalar С-ядро: **3.7-3.9× быстрее** C++ baseline (autovectorizer-friendly)
- AVX2 версия: 2.5-2.7× (в debug autovectorizer бьёт hand-rolled `_mm256_setr_ps`)
- Crossover threshold: 8 AABBs (ниже — inline C++ helper, не оверхед на per-batch setup)
- C ABI / `extern "C"` — zero name-mangling, 1:1 Godbolt review

**Q7. Сколько pipeline'ов у вас и зачем так много?**
- VulkanGraphicsPipeline: 2 варианта (TAA-off + TAA-on) для shadow / graphics / TAA resolve
- VulkanVoxelMeshingPipeline: compute-шейдер для мешинга
- ModelPipeline: 2 варианта (`modelPipeline` + `modelPipelineTaaOn`)
- RayMarchPipeline: только при `IsRayMarchEnabled()`
- TAA pipelines: voxel pass + resolve pass
- Итого ~8-10 pipelines

## 3.5. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если `taaEnabled` переключить во время кадра?**
- Per `agent/decisions.md` — `PickModelPipeline` выбирает `modelPipeline` или `modelPipelineTaaOn` based on `render.taaEnabled`
- Переключение на лету может вызвать invalidation in-flight command buffer
- Решение: invalidate TAA history через клавишу `.` (`InvalidateTaaHistory`)

**Q9. Почему AOCC + LOCL blended, а CTSH нет?**
- Per `agent/decisions.md`: `ComputeSunShadowSample` объединяет cascade shadow (viewpoint-dependent) и contact shadow (viewpoint-independent) в одно значение
- Blending combined value with history reprojected from previous frame = wrong direction в cascade transition zones (cascade shadow "ghosts")
- Skip blend для CTSH пока правильно — visual artefact > flicker в этом specific layer
- Phase 5 follow-up: refactor `ComputeSunShadowSample` для separation

**Q10. Как работает sphere stabilization в CSM?**
- Вместо AABB в light space используется bounding sphere
- Sphere имеет constant projected size в screen space → нет jitter при движении камеры
- Чуть менее tight fit чем AABB, но стабильнее
- Per `decisions.md §18`

**Q11. Что произойдёт, если `kMaxChunkExtentForGreedy` уменьшить до 16?**
- Больше чанков попадёт в fallback path (per-voxel emission, 1×1 quads)
- Greedy meshing эффективность упадёт
- В VoxelLab 8×8×8 не попадает в fallback, но в `MeshingStress` сцене (16×16×16) — да
- Альтернатива: реорганизовать AABB данные в SoA, тогда AVX2 ядро достигнет 8×

## 3.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Почему Vulkan, не OpenGL/DX12/Metal | «Архитектурное решение — к le1t» |
| DOD layout / `alignas(16)` / push constants | «Архитектура, к le1t» |
| Стек/Clang/cmake/ctest baseline | «К Тиммейту 1» |
| Voxel-мир / чанки / мешинг | «К Тиммейту 2» |
| Voxel raycast / fluid CA / snapshot | «К Тиммейту 2» |
| Физика / walk controller / Jolt | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| BUG-004 VoxelLab tremor (отвергнут) | «Не существует, jitter=0 default» |
| SSAO / GTAO отложено | «Roadmap, к Тиммейту 4 (он закрывает)» |
| Hot shader reload (клавиша 1) | «К le1t» |
| Phase 4-9 / roadmap | «К Тиммейту 4 (он закрывает)» |
