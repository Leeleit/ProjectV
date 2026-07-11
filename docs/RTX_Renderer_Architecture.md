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

Оркестрация структур ускорения реализована в семействе файлов `RayTracedShadows.cpp` (Blas, Tlas) и [RayTracedShadows.hpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.hpp).

1.  **Bottom-Level Acceleration Structure (BLAS):**
    *   Строится для каждого воксельного чанка (8x8x8 блоков) на основе его AABB геометрии.
    *   Создание и обновление BLAS происходит на GPU асинхронно в методе `BuildDirtyBlases` через один буфер команд с использованием забора адреса `vkGetAccelerationStructureDeviceAddressKHR`.
    *   При редактировании мира dirty-чанки ставятся в очередь `DirtyChunkRebuild` для перестроения BLAS.
2.  **Top-Level Acceleration Structure (TLAS):**
    *   Объединяет все активные инстансы BLAS чанков с единичными матрицами трансформации.
    *   Функция `UpdateTlas` подготавливает буфер инстансов, а `RecordTlasBuild` записывает команду `vkCmdBuildAccelerationStructuresKHR` с типом `VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR`.
    *   Барьер синхронизации переводит TLAS из состояния `AS_BUILD` в `FRAGMENT_SHADER` для чтения в шейдерах.

---

## 3. RTX Тени Солнца (Voxel-Aware Shadow Mask)

Вместо каскадных карт теней CSM введена двухпроходная трассировка теней в [voxel_rtx_shadow.rgen](file:///home/le1t/Projects/ProjectV/src/shaders/voxel_rtx_shadow.rgen):

1.  **Первый проход (Primary Ray):** Луч выпускается из камеры в направлении взгляда (`T_max = far`). При пересечении с вокселем ближайший хит возвращает координату пересечения `payload.hitT`.
2.  **Второй проход (Shadow Ray):** Если найден первичный хит, из точки пересечения `worldOrigin + viewDir * hitT` со смещением вдоль нормали выпускается вторичный луч к солнцу (`T_max = 256.0`).
3.  **Обработка прозрачности:** Шейдер пересечения процедурных вокселей игнорирует материалы с типом `Glass` (стекло не отбрасывает плотных теней).
4.  **Результат:** Записывается в маску `rtxShadowMask` (текстура формата `R8_UNORM` разрешения экрана). В фрагментном шейдере [voxel.frag](file:///home/le1t/Projects/ProjectV/src/shaders/voxel.frag) маска сэмплируется по экранным координатам.

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
*   [RayTracedShadowsBlas.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsBlas.cpp) — Сборка и управление структурами BLAS чанков.
*   [RayTracedShadowsTlas.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsTlas.cpp) — Сборка и запись команд для TLAS.
*   [RayTracedShadowsPass.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsPass.cpp) — Запись проходов теней солнца.
*   [RayTracedShadowsMask.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadowsMask.cpp) — Создание маски теней, очистка и fallback-текстуры.

### RTX GI Probes
*   [RtxGiProbes.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.cpp) / [RtxGiProbes.hpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.hpp) — Точка владения и выделения текстур/буферов сетки зондов.
*   [RtxGiProbesPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbesPipeline.cpp) — Инициализация вычислительного конвейера и дескрипторов.
*   [RtxGiProbesUpdate.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbesUpdate.cpp) — Запись вычислительного прохода обновления зондов.

### Конвейеры Vulkan Low-Level ([src/render/vulkan/](file:///home/le1t/Projects/ProjectV/src/render/vulkan/))
*   [VulkanGraphicsPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipeline.cpp) — Управление пайплайнами.
*   [VulkanGraphicsPipelineCreate.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineCreate.cpp) — Создание конвейера с форматом B10G11R11_UFLOAT_PACK32.
*   [VulkanGraphicsPipelineBindings.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineBindings.cpp) — Привязка Descriptor Sets.
*   [VulkanAsyncCompute.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanAsyncCompute.cpp) — Асинхронные вычисления (Fluid CA, World Gen) через Timeline Semaphores.

---

## 7. Удаление TAA и Прямой Рендеринг

Конвейер сглаживания TAA полностью удален из mainline-ветки разработки из-за ограничений стабильности на низких частотах кадров и перенесен в архив `legacy/aa/`.
*   **Текущий путь кадра:** Рендеринг вокселей идет напрямую в оффскрин-буфер `sceneColorTarget` (формат `VK_FORMAT_B10G11R11_UFLOAT_PACK32` для HDR).
*   **Blit Pass:** После завершения отрисовки сцены и наложения HUD буфер `sceneColorTarget` переносится на экран через `vkCmdBlitImage` в swapchain image. Это решает проблемы с несовпадением форматов презентации.
