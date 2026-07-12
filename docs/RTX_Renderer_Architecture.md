# ProjectV RTX-Driven Renderer Architecture

Этот документ описывает современную графическую архитектуру движка `ProjectV`, ориентированную исключительно на аппаратную трассировку лучей (RTX-only) и Vulkan 1.4. Вся легаси-система каскадных теней (CSM) и TAA были полностью удалены в пользу современных RTX-технологий и прямого HDR-рендеринга с постобработкой.

---

## 1. Философия RTX-only и требования к оборудованию

В проекте принята политика **отказа от легаси-рендеринга**:
*   **Минимум:** Видеокарта NVIDIA GeForce RTX 2060 (Turing, 2019 г.) или новее с аппаратной поддержкой трассировки лучей.
*   **Рекомендуется:** NVIDIA RTX 3060 Ti / 3070 / 4070+ (Ampere / Ada Lovelace).
*   **Не поддерживается:** Любое оборудование без выделенных RT-ядер (серии GTX 10xx/16xx, встроенная графика, старые карты AMD/Intel).
*   При запуске на неподдерживаемом GPU движок принудительно завершает работу с выводом сообщения о необходимости наличия RTX-видеокарты (вместо бесшумной деградации кадра).

---

## 2. Иерархия ускорения трассировки (BLAS & TLAS)

Оркестрация структур ускорения реализована в семействе файлов `RayTracedShadows.cpp` (Blas, Tlas, Pass, Mask)
и [RayTracedShadows.hpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.hpp).

1.  **Bottom-Level Acceleration Structure (BLAS):**
    * Строится для каждого воксельного чанка (8x8x8 блоков) на основе его AABB геометрии (`VK_GEOMETRY_TYPE_AABBS_KHR`).
    * _Device address caching:_ После построения адрес BLAS кэшируется через
      `vkGetAccelerationStructureDeviceAddressKHR` в `m_config.blasDeviceAddresses[i]`, что даёт `O(1)` доступ при
      сборке TLAS.
    * Создание и обновление BLAS происходит на GPU асинхронно в методе `BuildDirtyBlases` через один буфер команд +
      fence. Dirty-чанки ставятся в очередь `DirtyChunkRebuild { chunkIndex, aabb }`.
    * При редактировании мира `pendingBlasRebuildIndices` заполняется одновременно с `pendingChunkRebuildIndices`.
2.  **Top-Level Acceleration Structure (TLAS):**
    * Объединяет все видимые инстансы BLAS чанков с единичными матрицами трансформации.
    * `UpdateTlas(visibleChunks)` подготавливает буфер инстансов (до 4096), проставляя `accelerationStructureReference`
      из кэшированного BLAS device address.
    * `RecordTlasBuild` записывает `vkCmdBuildAccelerationStructuresKHR` с
      `VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR`, `VK_GEOMETRY_TYPE_INSTANCES_KHR`.
    * Барьер синхронизации `AS_BUILD → FRAGMENT_SHADER` гарантирует готовность TLAS для `rayQueryEXT` во фрагментном
      шейдере.

---

## 3. RTX Тени Солнца (Voxel-Aware Shadow Mask)

Вместо каскадных карт теней CSM введена двухпроходная трассировка теней в `voxel_rtx_shadow.rgen` / `.rint` / `.rchit` /
`.rmiss`:

### Проходы трассировки

1. **Primary Ray:** Луч из камеры вдоль направления взгляда (`T_max = far`). `rchit` пишет `gl_HitTEXT` в
   `payload.hitT`.
2. **Shadow Ray (только если primary hit найден):** Из `worldOrigin + viewDir*hitT + viewDir*0.05` (bias) вдоль sun
   direction, `T_max = 256`. Sky pixels (primary miss) сохраняют `shadowFactor = 1.0`.

### Voxel-aware procedural intersection

Шейдер `.rint` получает AABB hit из TLAS и выполняет полный DDA traversal через `PackedChunkVoxelPayload` для
определения точного вокселя пересечения внутри чанка. Это заменяет подход с per-voxel AABB (который был бы слишком дорог
для TLAS).

### Self-shadow fix (session 23x)

- `tCurrent = tMaxAxis.{axis}` (per-step update) вместо stuck `max(tEntry, rayTmin)` — иначе хиты проецировались на
  грани чанка.
- Ignore `Glass` material в traversal (прозрачная оболочка не отбрасывает тень).
- `grazingFactor = mix(0.25, 1.0, rtxLit)` для избежания pitch-black теней.

### Chunk boundary precision fix (session 26x)

- `1e-4` offset к `tMin` в `TraceVoxelIntersection` при входе луча в чанк — иначе float-округление на границах чанков
  выталкивало стартовую позицию DDA наружу.
- DDA advance для лучей, стартующих внутри non-air вокселя: `tMin += tWall + 1e-4` — иначе DDA коммитился на
  `tCurrent = tMin` с неправильной нормалью.
- Normal из dominant-axis направления луча (не position offset) — иначе FP micro-fluctuation давал random face.

### Результат

Записывается в `rtxShadowMask` (формат `R8_UNORM`, разрешение экрана). Фрагментный шейдер читает текстуру по экранным
координатам. Strength factor `0.75` (25% ambient sun bleed) — `mix(0.25, 1.0, rtxLit)`.

### Ray budget (RTX 3060 Ti, VoxelLab 1080p × 120 FPS)

Общий бюджет — 248 MRays/sec (RTX 3060 Ti, 38 RT cores):

| Луч                  | Количество на пиксель                    | MRays/sec          | % RT utilization |
|----------------------|------------------------------------------|--------------------|------------------|
| Sun shadow           | 1                                        | 248                | 0.65%            |
| Contact shadow       | 1                                        | 248                | 0.65%            |
| AO                   | 3                                        | 744                | 1.95%            |
| DDGI diffuse         | 6 (shared across frames, ~0.1 effective) | ~25                | 0.07%            |
| Specular reflection  | 1 (roughness ≤ 0.3)                      | ≤248               | ≤0.65%           |
| Refraction           | 1 (glass/fluid only)                     | ≤248               | ≤0.65%           |
| **Total worst-case** | **~18**                                  | **~4.5 GRays/sec** | **~5-15%**       |

### Интеграция в кадр

RTX pipeline встраивается в `RendererDrawFrame` в строгом порядке:

1. **Pre-graphics:** `BuildDirtyBlases` (one-shot cmd + fence) → `UpdateTlas` (instance fill) →
   `RecordVoxelAwareRtxShadowPass` (shadow mask)
2. **Graphics:** `RecordTlasBuild` (AS_BUILD → FRAGMENT barrier) → voxel rendering (rayQueryEXT в `voxel.frag` читает
   TLAS binding 13)
3. **DDGI:** `RecordRtxGiProbeUpdatePass` (round-robin 1 probe/frame, rayQueryEXT против TLAS)

---

## 4. Глобальное Освещение DDGI Probes

Динамическое глобальное диффузное освещение (DDGI) заменяет старый воксельный конус трассировки (VCT diffuse) и реализовано в семействе файлов `RtxGiProbes.cpp` (Pipeline, Update) и [RtxGiProbes.hpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.hpp).

*   **Сетка зондов:** Сцена покрыта регулярной трехмерной сеткой зондов (8x8x8 = 512 зондов для тестовой сцены VoxelLab).
*   **Хранение:** 3D текстуры `irradianceImage` (формат `R11G11B10F`) и `distanceImage` (формат `RG16F`).
*   **Обновление (Compute Pass):** Шейдер [probe_update.comp](file:///home/le1t/Projects/ProjectV/src/shaders/probe_update.comp) выпускает 64 луча в сферических направлениях из центра каждого активного зонда с использованием `rayQueryEXT` в TLAS. Для накопления используется гистерезис (95% истории, 5% нового кадра) для подавления шума.
*   **Гладкий спад видимости:** Для устранения артефактов дискретности сетки зондов (probe-grid aliasing) при интерполяции на воде/стекле Chebyshev-тест заменен на **Gaussian visibility falloff** в [voxel.frag](file:///home/le1t/Projects/ProjectV/src/shaders/voxel.frag):
    $$\text{visibility} = \exp\left(-\frac{\text{distExcess}^2}{2 \cdot \max(\text{variance}, 0.25)}\right)$$
    Это обеспечивает плавное затухание в пределах 0.5 метра на стыках геометрии.

---

## 5. RTX Рефракция и Многоотскоковое GI

*   **Аппаратная Рефракция:** Шейдер [voxel.frag](file:///home/le1t/Projects/ProjectV/src/shaders/voxel.frag) при обработке вокселей воды/стекла вызывает трассировку луча преломления через функцию `TraceRtxRefractionRay`. Луч отклоняется на основе показателя преломления (IOR: стекло = 1.5, вода = 1.33) и читает цвет геометрии позади объекта.
*   **Dominant Axis Normal Fix:** Для точного расчета направления преломления и вторичных лучей нормали в DDA-лучах вычисляются строго на основе доминантной оси направления луча:
    $$\text{normal} = -\text{sign}(\text{dir.dominantAxis}) \cdot e_{\text{dominantAxis}}$$
    Это предотвращает мерцание и неверное рассеяние лучей на стыках блоков.
*   **Specular GI (Multi-Bounce):** Зеркальные поверхности рендерятся через `TraceRtxMultiBounceSpecular` (до 2-3 отскоков луча) для ground-truth отражений.

---

## 6. Декомпозиция и Модули Рендерера

В целях повышения читаемости и соответствия лимитам строк (до 600 строк) монолитные файлы рендерера были разделены на специализированные модули:

### Оркестратор Рендеринга
*   [Renderer.cpp](file:///home/le1t/Projects/ProjectV/src/render/Renderer.cpp) / [Renderer.hpp](file:///home/le1t/Projects/ProjectV/src/render/Renderer.hpp) — Точка владения и оркестрации.
*   [RendererDrawFrame.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererDrawFrame.cpp) — Управляет Pace-синхронизацией (fences/semaphores), вызовом HZB-куллинга и презентацией swapchain.
*   [RendererRecordCommands.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererRecordCommands.cpp) — Записывает графический пайплайн отрисовки вокселей, моделей, атмосферы и HUD.
*   [RendererOverlay.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererOverlay.cpp) — Отрисовка отладочных боксов AABB.
*   [RendererScreenshot.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererScreenshot.cpp) — Запись кадров в BMP с экспортом метаданных.

### Ресурсы Сцены
*   [SceneResources.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResources.cpp) / [SceneResources.hpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResources.hpp) — Главный класс управления GPU памятью.
*   [SceneResourcesUpdate.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesUpdate.cpp) — Синхронизация Uniforms, SSBO и данных NanoVDB.
*   [SceneResourcesVisibility.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesVisibility.cpp) — CPU Frustum culling чанков.
*   [SceneResourcesDestroy.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesDestroy.cpp) — Безопасное отложенное (deferred) удаление NanoVDB буферов для избежания гонок.
*   [SceneResourcesUtilities.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesUtilities.cpp) — Хелперы выделения памяти через VMA.

### RTX Shadows
*   [RayTracedShadows.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.cpp) / [RayTracedShadows.hpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.hpp) — Точка владения и выделения глобальных буферов.
* [RayTracedShadowsBlas.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsBlas.cpp) — Сборка,
  dirty-очередь и build BLAS чанков.
*   [RayTracedShadowsTlas.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsTlas.cpp) — Сборка и запись команд для TLAS.
* [RayTracedShadowsPass.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsPass.cpp) — Запись проходов
  теней и дебаг-репорт.
* [RayTracedShadowsMask.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsMask.cpp) — Создание маски
  теней, очистка, fallback-текстуры и voxel-aware RTX setup.
* [RtxShadowPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxShadowPipeline.cpp) — Создание RTX ray
  tracing pipeline (rgen/rint/rchit/rmiss).
* [RtxShadowSBT.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxShadowSBT.cpp) — Shader Binding Table для RTX
  pipeline.

### RTX GI Probes
*   [RtxGiProbes.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.cpp) / [RtxGiProbes.hpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.hpp) — Точка владения и выделения текстур/буферов сетки зондов.
*   [RtxGiProbesPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbesPipeline.cpp) — Инициализация вычислительного конвейера и дескрипторов.
*   [RtxGiProbesUpdate.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbesUpdate.cpp) — Запись вычислительного прохода обновления зондов.

### Конвейеры Vulkan Low-Level ([src/render/vulkan/](file:///home/le1t/Projects/ProjectV/src/render/vulkan/))

* [VulkanBootstrap.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanBootstrap.cpp) — Выбор GPU, проверка
  расширений, создание Logical Device. PNext chain без gaps.
* [VulkanSwapchain.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanSwapchain.cpp) — Инициализация
  swapchain, V-sync cycle (FIFO/MAILBOX/IMMEDIATE).
*   [VulkanGraphicsPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipeline.cpp) — Управление пайплайнами.
* [VulkanGraphicsPipelineCreate.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineCreate.cpp) —
  Создание конвейеров с форматом B10G11R11_UFLOAT_PACK32.
* [VulkanGraphicsPipelineBindings.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineBindings.cpp) —
  Привязка Descriptor Sets и Push Constant диапазонов.
* [VulkanGraphicsPipelineOverlay.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineOverlay.cpp) —
  Пайплайны для дебаг-оверлеев и HUD.
* [VulkanAsyncCompute.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanAsyncCompute.cpp) — Асинхронные
  вычисления (Fluid CA, World Gen, HZB) через Timeline Semaphores. Signal/wait pairing: `renderTimelineSemaphore` +
  `hzbBuildTimelineSemaphore`.
* [VulkanFluidCaPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanFluidCaPipeline.cpp) —
  Пайплайн GPU cellular automaton жидкости.
* [VulkanVoxelMeshingPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelMeshingPipeline.cpp) —
  Пайплайн GPU greedy mesher.
* [VulkanMeshShaderPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanMeshShaderPipeline.cpp) —
  Пайплайн mesh shaders (Pattern C, feature-flagged `PROJECTV_MESH_SHADER_PIPELINE`).
* [VulkanVoxelizePipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelizePipeline.cpp) —
  Пайплайн GPU-вокселизации моделей.
* [VulkanWorldGenPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanWorldGenPipeline.cpp) —
  Пайплайн процедурной генерации мира.

---

## 7. Антиалиасинг: TAA → DLSS/DLAA (в разработке)

### TAA (историческая справка)

Karis 2014 TAA был полностью удалён из mainline (июнь-июль 2026) и перенесён в `legacy/aa/`.
Причины: фундаментальные лимиты single-sample TAA (color-space mismatch линейный HDR current frame + LDR history,
остаточная тряска при jitterScale > 0, ghosting на движущихся объектах).

TAA-инфраструктура включала: Halton (2,3) jitter, YCoCg color space clamp, neighbourhood radius 1/3/5/7,
CAS sharpen, motion vectors (`R16G16`), 4 SPIR-V variants.

### Текущий рендеринг (без TAA)

* Рендеринг идёт напрямую в offscreen-буфер `sceneColorTarget` (формат `VK_FORMAT_B10G11R11_UFLOAT_PACK32` HDR).
* Tonemap + color grading применяются в `voxel.frag` / `model.frag` до output.
* После Graphics Pass — `vkCmdBlitImage` из `sceneColorTarget` в swapchain image.
* **Blit Pass:** решает проблемы несовпадения форматов презентации.

### План: DLSS/DLAA через NVIDIA Streamline (Phase 4)

Следующая фаза антиалиасинга — интеграция NVIDIA Streamline Super Resolution (DLSS).

* DLSS SR доступен на Linux с драйвером 525.72+ (текущий: 610.43.02).
* DLSS-G/Frame Generation — Windows-only, не в scope.
* Замена blit pass на DLSS/DLAA upsample + AA.

---

## Связанные документы

- [Documentation Index](README.md) — карта всех руководств
- [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md) — полный разбор файлов и алгоритмов
- [ArchitectureGuide](ArchitectureGuide.md) — общая архитектура движка
- [Linux Build & Run Guide](Linux_Build_And_Run.md)
- [Physics & Movement Guide](Physics_And_Movement_Guide.md)
- [RenderArchitecture (Historical)](RenderArchitecture.md)

