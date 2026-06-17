# Defense Competency FAQ — T4 (Тесты и проверки)

**Slot:** T4 (2:40–3:15, Участник 4 = Тиммейт 4, slide 8: Тесты)
**Кто говорит:** Тиммейт 3
**Реальная компетенция:** Рендеринг (Vulkan 1.4, TAA, CSM, AOCC, шейдеры, C-ядро frustum cull)
**Out of scope (к кому перенаправлять в Q&A):** архитектура выбора API — к T2 (le1t); стек/сборка — к T1; воксельный мир — к T3; физика — к T6; ассеты/демо — к T5; все баги — к T2.

---

## 1. Verbatim твоей речи (T4)

> «Здравствуйте. Процесс испытаний системы полностью автоматизирован. Четырнадцать наборов тестов ядра покрывают математику, физический walk-контроллер, отсечение по пирамиде видимости и логику жидкостей [T1.md, T4.md]. Все они успешно проходятся в CTest при каждой сборке [T1.md]. Визуальная часть проверяется скриптом поканального сравнения графики: генерируются шесть снимков, сверяющих тени, контактное затенение, локальный свет, фоновое заслонение полостей и временное сглаживание TAA [T1.md, T4.md]. Передаю слово.»

---

## 2. Кто ты

**Легенда:** ты отвечал за рендеринг — Vulkan 1.4 init, graphics pipeline, TAA, CSM, AOCC, шейдеры, C-ядро frustum culling. Это самый большой и технически насыщенный модуль.

**На сцене:** ты говоришь T4 (Тесты и проверки) — обзор тестов с акцентом на визуальные (render) тесты.

**На Q&A:** ты отвечаешь на вопросы про **рендеринг, шейдеры, TAA, CSM, AOCC, C-ядро, освещение**.

---

## 3. Твоя компетенция: Рендеринг

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

### 3.1. Алгоритм 6 — Каскадные тени (CSM, Cascaded Shadow Maps)

**Где:** `src/render/ShadowProjection.{hpp,cpp}` (CPU build) + `src/shaders/voxel_shadow.{vert,frag}` (depth pass) + `voxel.frag` (sample).
**Проблема:** один shadow map 2048×2048 на всю сцену даёт texel size = 64 м (воксель 1 м) — тени «зубчатые» на близких объектах, размытые на дальних.

**Constants (`src/render/ShadowProjection.cpp:17-23`):**
- `kSunShadowCascadeCount = 4u` (per `ShadowTypes.hpp:7`)
- `kDefaultShadowMapResolution = 2048u`
- `kDefaultCascadeSplitLambda = 0.80f`
- `kDefaultCascadeNearPlane = 0.1f`
- `kDefaultCascadeFarPlane = 128.0f`
- `kMinShadowCoverageScale = 0.5f`, `kMaxShadowCoverageScale = 3.0f`

**Алгоритм (на каждый кадр, CPU `BuildSunShadowCascadeSplits`):**

1. **Split planning** (per `decisions.md §15`):
   - `lambda = 0.80` (near-biased)
   - Split depths: practical scheme `split[i] = lerp(near*pow(far/near, i/n), near + (far-near)*i/n, lambda)`
   - Receiver horizon: `min(camera.farPlane, 64)` — не весь far plane, а видимая сцена.

2. **Per-cascade projection build** (для каждого из 4 каскадов):
   - Slice near/far → camera frustum sub-frustum (8 углов).
   - Sub-frustum → light-space → XY sphere extent (rotation-stable, не дёргается при yaw).
   - Extrude slice upstream along sun direction → caster coverage.
   - Snap light camera position to shadow texel grid → стабильна при малом движении камеры.
   - Output: `sunShadowViewProjections[4]` в `VoxelSceneLighting` SSBO.

3. **Shadow pass** (compute indirect):
   - Один subpass на каскад.
   - Indirect draw с per-cascade chunk commands (чанк может быть в каскаде, но не в другом).
   - Empty cascade → skip draw call (`commands.size() == 0`).

4. **Voxel frag sample** (`ComputeSunShadowSample`):
   - `cascadeIndex = selectCascade(viewDepth, splits)`.
   - PCF 5×5 (weighted) внутри каскада.
   - Cascade blend band: на границе каскадов blend между current/next (`BLD` контрол в HUD).
   - N·L-aware bias + receiver world-space bias.

**Complexity:** O(1) на каскад для build. O(1) на fragment для sample (PCF 5×5 = 25 texture reads).
**Empirical:** 4 каскада дают texel size 0.125 м на близких объектах (vs 64 м на одном каскаде) → 512× плотность теней.

**Edge cases:**
- Glass: не кастует тень (`transparent_shadow_policy=GLASS_IGNORED_FLUID_CASTS` в sidecar).
- Каскад с 0 чанков → skip draw (per `decisions.md §15`).
- Split transition: blend band ширина = `BLD` контрол.

**Говорить:**
- «4 каскада 2048×2048, lambda 0.80 near-biased, per-cascade XY sphere fit».
- «Light camera snap к texel grid — стабильна при малом движении».
- «Cascade blend band на границе, не hard switch».
- «Glass не кастует, Fluid кастует (зафиксировано в `decisions.md §15`)».

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

### 3.2. Алгоритм 7 — PCF 5×5 (взвешенный)

**Где:** `src/shaders/voxel.frag` → `ComputeSunShadowSample`.
**Проблема:** hard shadow comparison = резкие зубчатые границы теней. Unreal Engine 2 / Minecraft — выглядит «деревянно».

**Алгоритм:**

1. 5×5 = 25 точек в shadow map space.
2. Для каждой точки: standard shadow comparison (`d <= shadowMap[i]`).
3. Вес = bilinear/gaussian kernel (центр тяжелее):
   ```
   weights[5][5] = {
       {1, 4, 6, 4, 1},
       {4,16,24,16, 4},
       {6,24,36,24, 6},
       {4,16,24,16, 4},
       {1, 4, 6, 4, 1}
   };  // sum = 256
   ```
4. N·L-aware: при grazing angles (N·L → 0) → bias увеличивается, иначе acne.
5. Receiver world-space bias: `d - bias * (1 - N·L)`.
6. Smoothstep между current/next cascade в band.

**Vulkan 1.4 detail:** `sampler2DArrayShadow` с `magFilter=LINEAR` даёт hardware 2×2 PCF — **уже бесплатный baseline**, manual 5×5 поверх для дополнительного сглаживания.

**Говорить:**
- «Vulkan 1.4 LINEAR magFilter → hardware 2×2 PCF бесплатно».
- «Manual 5×5 weighted поверх — 25 reads, веса gaussian».
- «N·L-aware bias для предотвращения shadow acne на grazing angles».

### 3.3. Алгоритм 8 — Контактные тени (voxel DDA)

**Где:** `src/shaders/voxel.frag` → `ComputeContactShadow`.
**Проблема:** CSM texel size конечен → на близких к кастеру поверхностях тень «парит» в воздухе, нет контакта с землёй.

**Алгоритм (Amanatides-Woo 3D DDA):**

1. Старт: `pos = worldPos фрагмента + smallEpsilon * sunDir`.
2. Step direction: `sign(sunDir)`, normalize.
3. `tMax[3] = abs((floor(pos) - pos) / sunDir)` — dist до следующей воксельной границы по каждой оси.
4. `tDelta[3] = abs(1.0 / sunDir)`.
5. Loop: `minAxis = argmin(tMax)`, advance pos по `minAxis`, `tMax[minAxis] += tDelta[minAxis]`.
6. На каждом шаге: lookup `voxelData[pos]`. Если solid (Glass/FloorWhite/FloorGray) → **hit**, attenuate sun shadow.
7. Max iterations = `maxDistance / min(tDelta)` (clamped to N=16).

**Edge cases:**
- Out-of-bounds: terminate, treat as no occluder.
- Glass в DDA: не считать occluder (per `decisions.md §15`).
- Fluid в DDA: считать occluder (per `decisions.md §15`).

**Говорить:**
- «Короткая DDA от фрагмента к солнцу, max ~5 единиц».
- «Glass пропускает, Fluid блокирует — зафиксировано в `decisions.md §15`».
- «Это **локальный** contact shadow, не заменяет CSM, а дополняет».

### 3.4. Алгоритм 9 — AOCC (фоновое затенение полостей, ambient occlusion cavity check)

**Где:** `src/shaders/voxel.frag` → `ComputeAmbientOcclusionVisibility`.
**Проблема:** углы и полости выглядят «плоско» без локального occlusion term. SSAO/GTAO = screen-space, требует depth/normal prepass.

**Алгоритм (hemisphere DDA):**

1. `params = ambientOcclusionParams = {strength, radius, minVisibility}` (Vec4 в `VoxelSceneLighting`).
2. 3 направления × 4 шага = **12 DDA трассировок** на фрагмент.
3. Направления: tangent-space hemisphere, 3 рандомных seed.
4. На каждом шаге: lookup voxel, если solid → bump visibility вниз.
5. Visibility = `clamp(1.0 - hitCount * strength, minVisibility, 1.0)`.
6. Multiplied в sky/horizon/ground fill term.

**Edge cases:**
- Per-face visibility baked в `PackedSceneVoxelFace` (`voxel.frag` flat) — комбинируется с runtime DDA.
- 12 reads — встроено в forward path, не отдельный pass.

**Говорить:**
- «Локальный forward-path occlusion, 12 DDA, не full SSAO».
- «Baked per-face AO в compute meshing + runtime DDA — два слоя».
- «3 направления × 4 шага = 12 трассировок на фрагмент».

### 3.5. Алгоритм 10 — TAA + YCoCg + CAS

**Где:** `src/render/Taa.{hpp,cpp}` + `src/render/TaaRenderTargets.{hpp,cpp}` + `src/shaders/taa_resolve.{frag,vert}`.
**Проблема:** camera motion → aliasing на мелких деталях. MSAA = дорого, FXAA = blurry. TAA = хорошее качество при разумной цене.

**Алгоритм (на каждый кадр):**

1. **Jitter:**
   - 8-sample Halton(2,3) sequence: `(0.5/N) * (halton(i, 2), halton(i, 3))` где `i ∈ [0,8)`.
   - Применяется в projection matrix: `projection[2][0] += jitterX / width; projection[2][1] += jitterY / height`.

2. **History sampling:**
   - Reproject current pixel UV → previous frame UV (motion vectors).
   - Sample `historyTexture[uv]`.

3. **Color space — YCoCg:**
   - Convert history RGB → YCoCg.
   - Clamp color components отдельно (luma + chroma). Меньше ghosting на ярких участках.
   - Convert back to RGB для blending.

4. **Blending:**
   - `outColor = mix(currentFrame, history, taaBlend)` где `taaBlend = 0.10..0.90`.
   - Neighbourhood radius 1-7 для clamping (исключает outliers).

5. **History invalidation** (7 триггеров, per `decisions.md §19`):
   - Swapchain resize
   - World reset/reload
   - TAA toggle
   - Jitter scale change
   - Blend change
   - Radius change
   - Manual `RequestTaaHistoryInvalidate()`

6. **CAS (Contrast Adaptive Sharpening):**
   - После TAA resolve.
   - High-pass filter: 4-угловое среднее соседей.
   - Вес: `(1 - taaBlend) * max(neighbors)`.
   - Sharpened output.

7. **Color format:**
   - `B10G11R11_UFLOAT_PACK32` — 32-bit на пиксель, 2× экономия vs R16G16B16A16_SFLOAT.
   - Loss of precision: minimal (10-битный лум, 11-битный chroma — достаточно для PBR).

**Complexity:** 1 history sample, 1 current sample, ~10 ALU на пиксель. ~0.3-0.5 ms на 1080p.

**TAA practical state (per `Taa.hpp`):**
- 8-tap Halton(2,3) sub-pixel jitter sequence (in pixel units relative to rasterization center)
- По умолчанию `taaEnabled=false` (jitter=0) — стабильная картинка, нет дрожания
- YCoCg color space clamp в `taa_resolve.frag:170-190` — не вымывает chroma на ярких highlight'ах
- Per-layer history (CTSH/AOCC/LOCL) — `mix(rawCurrent, history, blend=0.4)`, per `agent/decisions.md §18`
- CTSH history **не** blended (deferred — separation refactor)
- Sidecar metadata: `taaEnabled`, `taaJitterX/Y`, `taaBlend`, `taaNeighbourhoodRadius`

**Говорить:**
- «8-sample Halton jitter в projection matrix».
- «YCoCg clamp — избегает ghosting на ярких участках».
- «CAS sharpening поверх TAA — high-pass через 4-угловое среднее».
- «B10G11R11_UFLOAT — 2× bandwidth saving vs R16G16B16A16».

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

### 3.6. Алгоритм 11 — Трассировка лучей через compute-шейдер (ray-marching compute pass)

**Где:** `src/shaders/ray_march.comp` + `src/render/RayMarchPass.{hpp,cpp}`.
**Проблема:** mesh-based геометрия даёт видимые «грани» вокселей при cinematic-камерах. ТЗ требовало «GPU ray-marching через compute-шейдеры» (п. 4.1.2).

**Алгоритм (Amanatides-Woo 3D DDA через packed voxel payload) — `src/shaders/ray_march.comp`:**

1. **Input:** uniform buffer (`RayMarchParams`) + storage buffer `PackedVoxelPayload` + storage image `rayMarchOutput`.
2. **Per-pixel compute (1 thread per pixel, `local_size_x=8, local_size_y=8`):**
   - Ray origin = camera position, ray dir = perspective ray из forward + right * u * tanHalfFovX + up * v * tanHalfFovY.
   - Convert ray to voxel-space, `deltaDist = abs(1.0 / max(abs(rayDir), 1e-6))`.
   - `sideDist` initial, `cell = floor(originVoxelSpace)`.
   - DDA loop (max `maxSteps` из params):
     - На каждом шаге: `FetchVoxel(cell)`. Если != 0 (≠ Air) → hit, break.
     - Step to next cell along dominant axis (compare `sideDist.x/y/z`).
     - Update `hitNormal` по направлению шага.
   - Output: simple N·L diffuse shading (sun direction packed в up.w), `palette[min(hitMaterial, 4u)]` base color.
3. **Output:** `imageStore(rayMarchOutput, pixel, outColor)`. RGBA8 image.

**Текущее состояние: STUB.**

Per `src/render/RayMarchPass.cpp:59-79`, `RecordRayMarchCommands`:
```cpp
void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData) {
    const auto &state = MutableRayMarchState();
    if (!state.enabled) return;
    if (context.device == VK_NULL_HANDLE) return;
    // Phase 7 follow-up: full Vulkan integration binds the shader,
    // allocates offscreen RGBA8 storage image, dispatches 8x8x1.
    // Current entry point emits a diagnostic record so the toggle
    // is observable in the runtime output stream and the call site
    // is not silently swallowed.
    std::fprintf(stderr,
        "[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up: shader is compiled, pipeline / offscreen target / composite are the next slice)\n");
}
```

То есть **compute shader скомпилирован** (даёт `.spv` через glslc), API state (`SetRayMarchEnabled` / `IsRayMarchEnabled` / `RequestRayMarchPipelineRecreate`) работает, **но graphics command stream НЕ вызывает** compute pass. Полная интеграция (offscreen target + composite) — Phase 7 follow-up.

**Toggle:** `SDLK_F12` в `main.cpp` → `projectv::render::SetRayMarchEnabled(bool)` (relocated 2026-06-15 с F6 — F6 теперь чисто для `SaveWorldSnapshot` InputAction). По умолчанию OFF.

**Говорить:**
- «Compute shader скомпилирован, API работает, но в graphics stream не вкомпонован — STUB, Phase 7 follow-up».
- «Toggle F12 в рантайме (relocated с F6), OFF по умолчанию».
- «Compute DDA через packed voxel payload, RGBA8 output image (planned)».
- «Альтернативный путь рендеринга для cinematic camera — мягкие грани вокселей (planned)».

### 3.7. Локальный точечный свет (LOCL, к вопросу «зачем нужен, если есть солнце?»)

Сцена Voxel Laboratory имеет один на пресет обратно-квадратичный точечный свет в дополнение к направленному солнцу. Это даёт объёмный эффект (не плоский), подсвечивает тёмные стороны сферы. `voxel.frag` вычисляет GGX BRDF для обоих источников. Локальная тень — через член видимости DVA только для непрозрачных (`localPointLightParams.shadowStrength`). Отладочный вид `LOCL` показывает вклад.

**Per-preset localPointLightParams (per `VoxelMaterials.cpp:181-236`):**
```cpp
// VoxelLab
sunShadowParams = {0.72f, 0.0009f, 0.0060f, 1.10f}  // strength, depthBias, normalBias, filterRadius
sunContactShadowParams = {0.28f, 2.25f, 0.0f, 0.0f}  // strength, maxDistance, reserved, reserved
```
(Аналогично для FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress.)

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

---

## 4. Hotkeys в твоей зоне

- `B` — cycle lighting debug view (10 views: Final / Ambient / Direct / Local / Shadow / Cascade / Contact / Occlusion / Fog / Taa)
- `C` — capture screenshot (.bmp + .txt sidecar с 60+ ключами)
- `T` — toggle TAA on/off
- `N` — cycle tone map operator (Linear / Reinhard / AcesApprox)
- `O` — cycle shadow tuning target (Strength / DepthBias / NormalBias / FilterRadius / Coverage / CascadeBlend)
- `U` / `I` — decrease / increase shadow tuning value
- `V` — reset lighting debug controls
- `H` / `K` — decrease / increase lighting exposure
- `L` — toggle cascade split planes (визуализация split planes)
- `Z` — toggle cursor hit normal
- `;` / `'` — decrease / increase TAA jitter scale
- `-` / `=` — decrease / increase TAA blend
- `,` — cycle TAA neighbourhood radius (1/3/5/7)
- `.` — invalidate TAA history
- `1` — hot shader reload (defense r0, 2026-06-15 relocation)
- `2` — toggle ray-march pass
- `3` — cycle V-sync mode

---

## 5. Глоссарий (твоя зона)

**VULKAN 1.4** — low-overhead graphics API. `VK_API_VERSION_1_4`. `volk` loader.

**VOLK** — Vulkan meta-loader. Vendored. `VK_NO_PROTOTYPES`.

**VMA (VulkanMemoryAllocator)** — GPU memory аллокатор. Vendored.

**CSM (Cascaded Shadow Maps)** — 4 каскада карт глубины 2048×2048, near-biased (lambda=0.80). Sphere stabilization.

**LAMBDA=0.80** — параметр split distribution. 0 = uniform, 1 = logarithmic. 0.80 = near-biased.

**SPHERE_STABILIZATION** — bounding sphere вместо AABB в light space. Constant projected size, нет jitter.

**PCF (Percentage-Closer Filtering)** — взвешенное сглаживание теней через множественные depth comparisons.

**TAA (Temporal Anti-Aliasing)** — смешивание кадров для убирания дрожания. 8-tap Halton(2,3). YCoCg-зажим.

**HALTON(2,3)** — low-discrepancy sequence для TAA jitter. Quasi-random, non-grid.

**YCoCg** — Luma-Chroma-Orange-Chroma-Green color space. Clamp в TAA не вымывает chroma.

**HALFTONE** — нет, не нужно

**AOCC (Ambient Occlusion Cavity Check)** — локальное затенение полостей, 12 traces per fragment. Per-layer history blended.

**CTSH (Contact Shadow)** — короткая трассировка луча от фрагмента к солнцу. Дополняет CSM.

**LOCL (Local Point Light)** — обратно-квадратичный точечный свет. Per preset.

**MRT (Multiple Render Targets)** — несколько выходов fragment shader. У нас 3: outColor, outSceneColor (TAA), outLayerMask.

**VULKAN_PIPELINE** — `VkPipeline` (graphics / compute). 8-10 pipelines: graphics (TAA off/on), voxel meshing compute, model (TAA off/on), ray-march compute, TAA resolve.

**RENDER_PASS (Vulkan 1.4 dynamic rendering)** — `vkCmdBeginRendering` / `vkCmdEndRendering`. Нет `VkRenderPass`/`VkFramebuffer`.

**SSBO (Shader Storage Buffer Object)** — large read-write buffer. Используется для history, scene data.

**UBO (Uniform Buffer Object)** — small read-only buffer. Используется для push constants-like data.

**COMPUTE_SHADER** — `VkComputePipeline`. Используется для meshing (`voxel_mesh.comp`) и ray-march (`ray_march.comp`).

**FRAGMENT_SHADER** — `voxel.frag`, `voxel_shadow.frag`, `taa_resolve.frag`, `model.frag`, `debug_overlay.frag`, `debug_hud.frag`.

**VERTEX_SHADER** — `voxel.vert`, `voxel_shadow.vert`, `taa_resolve.vert`, `model.vert`, `debug_overlay.vert`, `debug_hud.vert`.

**GLSL** — OpenGL Shading Language. Vulkan использует SPIR-V (скомпилированный из GLSL через `glslc`).

**SPIR-V** — Standard Portable Intermediate Representation (Vulkan). Output `glslc`.

**PIPELINE_CACHE (Vulkan)** — `VkPipelineCache`. Кэш для ускорения pipeline creation.

**DESCRIPTOR_SET (Vulkan)** — bindings для shader resources (UBOs, SSBOs, samplers, images).

**VK_IMAGE_LAYOUT** — `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`, `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, etc.

**VK_FORMAT** — pixel format: `VK_FORMAT_R8G8B8A8_UNORM`, `VK_FORMAT_R16G16B16A16_SFLOAT`, `VK_FORMAT_B10G11R11_UFLOAT_PACK32`, `VK_FORMAT_D32_SFLOAT`.

**FRUSTUM_CULLING** — отсечение объектов за пределами пирамиды видимости. С-ядро для batched.

**DEPTH_TEST** — `VkPipelineDepthStencilStateCreateInfo`. Сравнение depth-значений.

**EARLY_Z** — early depth test optimization в Vulkan.

---

## 6. Реалистичные вопросы (5-7)

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

---

## 7. Каверзные вопросы (3-5)

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

---

## 8. Хронология (релевантные события)

**2026-04-12 (P1 shadow fix):** SSBO double-buffer, fence reorder, cascade depth, TAA YCoCg clamp (commits b7e672f и др.).

**2026-04-12 (Tier 4):** С-ядро `frustum_cull` scalar (3.7-3.9× faster than C++ baseline). AVX2 version kept in tree (2.5-2.7× faster, autovectorizer beats hand-rolled в debug). Crossover threshold 8 AABBs.

**2026-04-12 (A1 greedy meshing, 4.1):** 6 per-axis greedy passes в compute shader. Заменён triple-nested loop over (X, Y, Z) × 6 directions.

**2026-06-12 (TAA 1.5 anti-flicker):** per-layer temporal history parameters для убирания per-frame flicker TAA colour-only blend не покрывал. Сейчас AOCC + LOCL blended (mix with 0.4), CTSH written но не blended (deferred).

**2026-04-15 (Post-WBV-r1):** F11/F12/V relocate → 1/2/3 (F5/F6 conflicts with InputAction). pragma once conversion (55 files). Shader contract fix (3 model/TAA-pipeline shaders).

**2026-06-17 (BUG-005 cycle scene race):** гонка дескрипторов при переключении сцен. Частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`. Полное устранение — Phase 5.

---

## 9. Out of scope (Q&A redirect)

| Вопрос про… | Говори |
|---|---|
| Почему Vulkan, не OpenGL/DX12/Metal | «К T2 (le1t)» |
| DOD layout / `alignas(16)` / push constants | «К T2 (le1t)» |
| Стек/Clang/cmake/ctest baseline | «К T1» |
| Voxel-мир / чанки / мешинг | «К T3» |
| Voxel raycast / fluid CA / snapshot | «К T3» |
| Физика / walk controller / Jolt | «К T6» |
| glTF / Draco / meshopt / miniaudio | «К T5» |
| BUG-005 cycle scene race | «К T2 (le1t, InputAction F5)» |
| BUG-004 VoxelLab tremor (отвергнут) | «Не существует, jitter=0 default» |
| SSAO / GTAO отложено | «Roadmap, к T6» |
| Hot shader reload (клавиша 1) | «К T2 (le1t)» |
| Phase 4-9 / roadmap | «К T6» |
