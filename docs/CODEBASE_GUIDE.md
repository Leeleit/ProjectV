# ProjectV Codebase & Architecture Guide (Exhaustive Edition)

Добро пожаловать в исчерпывающий путеводитель по кодовой базе **ProjectV**! Этот документ создан для того, чтобы помочь вам детально разобраться в архитектуре, принципах работы и каждом исходном файле проекта. После фаз активного прототипирования ("вайбкодинга") здесь собрана полная карта кодовой базы без сокращений.

---

## 1. Общая архитектура движка

ProjectV — это высокопроизводительный интерактивный воксельный MVP-слайс, сочетающий в себе объектно-ориентированную оркестрацию на CPU, ориентированное на данные представление (DOD) воксельного мира и GPU-driven конвейер рендеринга с аппаратной трассировкой лучей (RTX KHR).

### Взаимодействие компонентов движка

```mermaid
graph TD
    subgraph Core
        AppState[AppState / Types.hpp] --> World[WorldState]
        AppState --> Render[RenderState]
        AppState --> Physics[PhysicsState]
        AppState --> ECS[EcsState]
    end

    subgraph Simulation Loop (60 Hz)
        AppUpdate[AppUpdate.cpp] --> PhysicsWorld[PhysicsWorld.cpp]
        AppUpdate --> Character[CharacterVirtual / PhysicsWorld_Walk.cpp]
        AppUpdate --> VoxelInt[VoxelInteraction.cpp]
        AppUpdate --> ModelGG[ModelGravigun.cpp]
    end

    subgraph Render Pipeline (Frame Draw)
        DrawFrame[RendererDrawFrame.cpp] --> Mesher[voxel_mesh.comp / VulkanVoxelMeshingPipeline.cpp]
        DrawFrame --> RTX[RayTracedShadows.cpp / rtxShadowMask]
        DrawFrame --> DDGI[RtxGiProbes.cpp / probe_update.comp]
        DrawFrame --> HZB[HizCulling.cpp / HizBuffer]
        DrawFrame --> Graphics[RendererRecordCommands.cpp / sceneColorTarget]
    end

    World -->|Voxel World State| PhysicsWorld
    World -->|Sparse64Tree| Mesher
    World -->|NanoVDB Buffer| DDGI
    PhysicsWorld -->|Physics Colliders| Character
    RTX -->|Shadow Mask| Graphics
    DDGI -->|Indirect GI| Graphics
    HZB -->|Visible Chunk Bitmask| Graphics
```

---

## 2. Анатомия кадра (Frame Walkthrough)

Выполнение одного кадра симуляции и отрисовки — от опроса клавиатуры до вывода пикселей на монитор:

```mermaid
sequenceDiagram
    autonumber
    participant SDL as SDL2 (main.cpp)
    participant Upd as AppUpdate.cpp
    participant Phys as PhysicsWorld.cpp
    participant Prep as FramePreparation.cpp
    participant Res as SceneResourcesUpdate.cpp
    participant RTX as RayTracedShadows.cpp
    participant Draw as RendererDrawFrame.cpp
    participant Cmd as RendererRecordCommands.cpp

    SDL->>Upd: SDL_AppIterate / UpdateApp
    Note over Upd: Опрос ввода, перемещение камеры,<br/>выбор режима (Creative/Spectator/Walk)
    Upd->>Phys: TickPhysicsWorld (Jolt Simulation)
    Note over Phys: Симуляция твердых тел,<br/>почаночная синхронизация мешей с Jolt
    SDL->>Prep: PrepareFrameRenderData
    Note over Prep: Сборка HUD вершин, selection box,<br/>расчет HZB mip/blend width для чанков
    Prep->>Res: UpdateSceneResources / Upload CPU->GPU
    Note over Res: Загрузка буферов, обновление дескрипторов,<br/>синхронизация NanoVDB
    SDL->>Draw: DrawFrame
    Draw->>RTX: BuildDirtyBlases / UpdateTlas
    Note over RTX: Пересчет AABB для измененных чанков,<br/>сборка BLAS на GPU, обновление TLAS
    Draw->>RTX: RecordVoxelAwareRtxShadowPass
    Note over RTX: Трассировка теней от солнца в текстуру rtxShadowMask
    Draw->>Cmd: RecordGraphicsCommands
    Note over Cmd: Запуск GPU Greedy Mesher (voxel_mesh.comp),<br/>отрисовка вокселей через DrawIndirect,<br/>отрисовка HUD и Debug Box overlay
    Draw->>SDL: vkQueuePresentKHR
```

---

## 3. Детальный разбор модулей и файлов

Ниже представлена полная структура файлов в папке [src](file:///home/le1t/Projects/ProjectV/src), сгруппированных по 8 логическим модулям.

### 📂 1. Core (Ядро и Глобальное Состояние)
Файлы ядра определяют структуры данных, общие для всех систем.

*   [Types.hpp](file:///home/le1t/Projects/ProjectV/src/core/Types.hpp) / [Types.cpp](file:///home/le1t/Projects/ProjectV/src/core/Types.cpp) *(Hard · ~940 строк)*
    *   **Роль:** Главный словарь проекта. Содержит структуры `AppState` (корневой контейнер), `RenderState`, `SimulationState`, `InputState` и `VoxelSceneLighting`.
    *   **Связи:** Связывает все модули. В конце `Types.cpp` расположены жесткие `static_assert` на смещения байтов в структурах, отправляемых на GPU в виде SSBO.
*   [Math.ixx](file:///home/le1t/Projects/ProjectV/src/core/Math.ixx) *(Easy · ~120 строк)*
    *   **Роль:** C++26 модуль, экспортирующий типы векторов `Vec3`, `Vec4` и матриц `Mat4` для устранения зависимости от внешних тяжелых математических библиотек в заголовках.
*   [StringId.ixx](file:///home/le1t/Projects/ProjectV/src/core/StringId.ixx) *(Easy · ~80 строк)*
    *   **Роль:** C++26 модуль для быстрой работы с хэшированными строками (`StringID`). Позволяет делать сравнения строк на равенство за O(1).
*   [Probe.ixx](file:///home/le1t/Projects/ProjectV/src/core/Probe.ixx) *(Easy · ~50 строк)*
    *   **Роль:** Легковесный модуль для расстановки runtime-зондов производительности.

---

### 📂 2. App (Оркестрация и Игровой Цикл)
Управляет инициализацией приложения, обработкой ввода, логикой камеры и автоматизацией.

*   [main.cpp](file:///home/le1t/Projects/ProjectV/src/app/main.cpp) *(Easy · ~521 строка)*
    *   **Роль:** Точка входа. Содержит функции `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate` и `SDL_AppQuit`. Настраивает порядок вызовов систем кадра.
*   [AppUpdate.cpp](file:///home/le1t/Projects/ProjectV/src/app/AppUpdate.cpp) / [AppUpdate.hpp](file:///home/le1t/Projects/ProjectV/src/app/AppUpdate.hpp) *(Medium · ~710 строк)*
    *   **Роль:** Управляет обновлением состояния игры на каждом шаге. Переключает режимы движения, обновляет камеру и запускает симуляцию физики и клеточного автомата жидкости.
*   [AppUpdateHelpers.cpp](file:///home/le1t/Projects/ProjectV/src/app/AppUpdateHelpers.cpp) / [AppUpdateHelpers.hpp](file:///home/le1t/Projects/ProjectV/src/app/AppUpdateHelpers.hpp) *(Easy · ~86 строк)*
    *   **Роль:** Дополнительные хелперы для обновления свободной камеры и сброса состояний при смене режимов.
*   [Camera.cpp](file:///home/le1t/Projects/ProjectV/src/app/Camera.cpp) / [Camera.hpp](file:///home/le1t/Projects/ProjectV/src/app/Camera.hpp) *(Medium · ~240 строк)*
    *   **Роль:** Логика камеры. Вычисляет матрицы вида (`view`) и проекции (`projection`), обрабатывает джиттер проекции для алгоритма сглаживания (TAA).
*   [FramePreparation.cpp](file:///home/le1t/Projects/ProjectV/src/app/FramePreparation.cpp) / [FramePreparation.hpp](file:///home/le1t/Projects/ProjectV/src/app/FramePreparation.hpp) *(Medium · ~280 строк)*
    *   **Роль:** Подготовка данных кадра перед рендерингом. Сборка HUD текста, рамки выделения блока и расчет параметров HZB-куллинга для чанков.
*   [InputActions.cpp](file:///home/le1t/Projects/ProjectV/src/app/InputActions.cpp) / [InputActions.hpp](file:///home/le1t/Projects/ProjectV/src/app/InputActions.hpp) *(Easy · ~250 строк)*
    *   **Роль:** Маппинг клавиш клавиатуры и кнопок мыши на логические события (`InputAction`).
*   [InputReplay.cpp](file:///home/le1t/Projects/ProjectV/src/app/InputReplay.cpp) / [InputReplay.hpp](file:///home/le1t/Projects/ProjectV/src/app/InputReplay.hpp) *(Medium · ~360 строк)*
    *   **Роль:** Запись и воспроизведение ввода игрока. Позволяет воспроизводить баги с точностью до кадра.
*   [ModelGravigun.cpp](file:///home/le1t/Projects/ProjectV/src/app/ModelGravigun.cpp) / [ModelGravigun.hpp](file:///home/le1t/Projects/ProjectV/src/app/ModelGravigun.hpp) *(Medium · ~180 строк)*
    *   **Роль:** Симуляция гравитационной пушки. Позволяет притягивать физические объекты, удерживать их перед камерой и запускать в пространство.
*   [BenchmarkAutomation.cpp](file:///home/le1t/Projects/ProjectV/src/app/BenchmarkAutomation.cpp) / [BenchmarkAutomation.hpp](file:///home/le1t/Projects/ProjectV/src/app/BenchmarkAutomation.hpp) *(Medium · ~160 строк)*
    *   **Роль:** Автоматизированный запуск бенчмарков, сбор статистики по кадрам и экспорт метрик.
*   [LookDevCaptureAutomation.cpp](file:///home/le1t/Projects/ProjectV/src/app/LookDevCaptureAutomation.cpp) / [LookDevCaptureAutomation.hpp](file:///home/le1t/Projects/ProjectV/src/app/LookDevCaptureAutomation.hpp) *(Medium · ~240 строк)*
    *   **Роль:** Инструмент для автоматического снятия скриншотов с дебаг-метрологией для сравнения качества RTX-теней и GI.

---

### 📂 3. Voxel (Воксельное Хранилище и Геометрия)
Управляет базами данных блоков, асинхронной загрузкой и CPU-взаимодействием.

*   [Sparse64Tree.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/Sparse64Tree.hpp) *(Hard · ~390 строк)*
    *   **Роль:** Разреженное воксельное дерево (SVO). Каждая нода содержит 4x4x4 дочерних элементов (64 штуки). Отвечает за быстрое чтение и запись блоков.
*   [NanoVdb.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/NanoVdb.hpp) / [NanoVdb.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/NanoVdb.cpp) *(Hard · ~280 строк)*
    *   **Роль:** Конвертер разреженного дерева SVO в линеаризованный формат NanoVDB, пригодный для чтения на GPU.
*   [VoxelWorld.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelWorld.cpp) / [VoxelWorld.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelWorld.hpp) / [VoxelWorldInternal.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelWorldInternal.hpp) *(Medium · ~350 строк)*
    *   **Роль:** Хранилище чанков (8x8x8 вокселей каждый). Отвечает за координатную математику, bound checks и dirty-очередь измененных чанков.
    *   **Декомпозиция:** Генерация пресетов мира вынесена в `VoxelWorldPreset.cpp` (~590 строк), snapshot сэйвы в `VoxelWorldSnapshot.cpp` (~230 строк), симуляция клеточных автоматов жидкости в `VoxelWorldFluid.cpp` (~280 строк), LOD куллинг в `VoxelWorldLod.cpp` (~75 строк), статик промоушн в `VoxelWorldStatic.cpp` (~40 строк).
*   [VoxelMaterials.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelMaterials.cpp) / [VoxelMaterials.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelMaterials.hpp) *(Easy · ~510 строк)*
    *   **Роль:** Свойства материалов вокселей (альбедо, шероховатость, металл, прозрачность) и профили освещения для пресетов сцен.
*   [VoxelRaycast.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelRaycast.cpp) / [VoxelRaycast.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelRaycast.hpp) *(Medium · ~180 строк)*
    *   **Роль:** Прохождение луча по воксельной сетке на CPU для выбора блока, на который смотрит игрок.
*   [ChunkStreamer.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/ChunkStreamer.cpp) / [ChunkStreamer.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/ChunkStreamer.hpp) *(Medium · ~310 строк)*
    *   **Роль:** Асинхронная подгрузка и выгрузка чанков с диска по мере перемещения камеры.
*   [VoxelInteraction.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelInteraction.cpp) / [VoxelInteraction.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelInteraction.hpp) *(Medium · ~290 строк)*
    *   **Роль:** Логика установки и разрушения блоков, проверка пересечения с персонажем перед установкой блока.
*   [VoxelLodDownsample.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelLodDownsample.cpp) / [VoxelLodDownsample.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/VoxelLodDownsample.hpp) *(Medium · ~110 строк)*
    *   **Роль:** Алгоритм снижения разрешения вокселей чанков для дальних LOD-уровней (сохранение формы B_SurfacePreserve).
*   [CpuMeshGenerator.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/CpuMeshGenerator.cpp) / [CpuMeshGenerator.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/CpuMeshGenerator.hpp) *(Medium · ~90 строк)*
    *   **Роль:** Тестовый генератор полигональной сетки вокселей на CPU.
*   [SceneConfig.cpp](file:///home/le1t/Projects/ProjectV/src/voxel/SceneConfig.cpp) / [SceneConfig.hpp](file:///home/le1t/Projects/ProjectV/src/voxel/SceneConfig.hpp) *(Easy · ~130 строк)*
    *   **Роль:** Загрузка параметров сцены из файла `scene.json`.

---

### 📂 4. Render (Высокоуровневый Рендерер)
Управляет фазами рендеринга кадра, распределением буферов и эффектами.

*   [Renderer.cpp](file:///home/le1t/Projects/ProjectV/src/render/Renderer.cpp) / [Renderer.hpp](file:///home/le1t/Projects/ProjectV/src/render/Renderer.hpp) / [RendererInternal.hpp](file:///home/le1t/Projects/ProjectV/src/render/RendererInternal.hpp) *(Easy · ~90 строк)*
    *   **Роль:** Головной оркестратор графической подсистемы. Содержит структуру Renderer и хранит ссылки на составные части.
*   [RendererDrawFrame.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererDrawFrame.cpp) *(Hard · ~510 строк)*
    *   **Роль:** Реализация функции `DrawFrame`. Управляет барьерами синхронизации, вызовом прохода HZB-куллинга и презентацией в Swapchain.
*   [RendererRecordCommands.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererRecordCommands.cpp) *(Medium · ~505 строк)*
    *   **Роль:** Запись Vulkan-команд для прорисовки воксельных мешей, моделей, скайбокса, HUD и дебаг-оверлеев.
*   [RendererOverlay.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererOverlay.cpp) *(Easy · ~110 строк)*
    *   **Роль:** Запись команд для вывода оверлея дебаг-коробок (например, AABB физических объектов).
*   [RendererScreenshot.cpp](file:///home/le1t/Projects/ProjectV/src/render/RendererScreenshot.cpp) *(Easy · ~120 строк)*
    *   **Роль:** Логика сохранения кадра на диск в BMP формат.
*   [SceneResources.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResources.cpp) / [SceneResources.hpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResources.hpp) / [SceneResourcesInternal.hpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesInternal.hpp) *(Hard · ~810 строк)*
    *   **Роль:** Оркестрация GPU-ресурсов сцены (буферов дескрипторов, текстурных атласов).
*   [SceneResourcesUpdate.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesUpdate.cpp) *(Hard · ~470 строк)*
    *   **Роль:** Синхронизация данных сцены CPU->GPU. Обновляет глобальные UBO и SSBO, перезаливает NanoVDB буферы при редактировании мира.
*   [SceneResourcesVisibility.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesVisibility.cpp) *(Medium · ~290 строк)*
    *   **Роль:** Расчет видимости чанков на CPU с помощью Frustum Culling.
*   [SceneResourcesDestroy.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesDestroy.cpp) *(Medium · ~390 строк)*
    *   **Роль:** Безопасное удаление ресурсов. Содержит deferred-очередь удаления NanoVDB-буферов во избежание гонок на GPU.
*   [SceneResourcesUtilities.cpp](file:///home/le1t/Projects/ProjectV/src/render/SceneResourcesUtilities.cpp) *(Easy · ~70 строк)*
    *   **Роль:** Утилитарные методы создания Vulkan-буферов через аллокатор VMA.
*   [RayTracedShadows.cpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.cpp) / [RayTracedShadows.hpp](file:///home/le1t/Projects/ProjectV/src/render/RayTracedShadows.hpp) *(Hard · ~360 строк)*
    *   **Роль:** Менеджер аппаратных RTX-теней. Инициализация и выделение глобальных буферов.
    *   **Декомпозиция:** Сборка BLAS вынесена в `RayTracedShadowsBlas.cpp` (~320 строк), сборка TLAS в `RayTracedShadowsTlas.cpp` (~150 строк), запись прохода теней в `RayTracedShadowsPass.cpp` (~180 строк), создание теневой маски и fallback-текстур в `RayTracedShadowsMask.cpp` (~420 строк).
*   [RtxGiProbes.cpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.cpp) / [RtxGiProbes.hpp](file:///home/le1t/Projects/ProjectV/src/render/RtxGiProbes.hpp) *(Hard · ~350 строк)*
    *   **Роль:** Dynamic Diffuse Global Illumination (DDGI). Оркестрация сетки зондов и выделение ресурсов.
    *   **Декомпозиция:** Инициализация вычислительного пайплайна вынесена в `RtxGiProbesPipeline.cpp` (~150 строк), а запись вычислительного прохода обновления зондов в `RtxGiProbesUpdate.cpp` (~220 строк).
*   [HizCulling.cpp](file:///home/le1t/Projects/ProjectV/src/render/HizCulling.cpp) / [HizCulling.hpp](file:///home/le1t/Projects/ProjectV/src/render/HizCulling.hpp) *(Hard · ~910 строк)*
    *   **Роль:** Расчет иерархического Z-буфера (HZB) на GPU для быстрого куллинга скрытых чанков.
*   [Cloudscape.cpp](file:///home/le1t/Projects/ProjectV/src/render/Cloudscape.cpp) / [Cloudscape.hpp](file:///home/le1t/Projects/ProjectV/src/render/Cloudscape.hpp) *(Medium · ~580 строк)*
    *   **Роль:** Отрисовка облаков с помощью 3D Raymarching на GPU.
*   [SkyAtmosphere.cpp](file:///home/le1t/Projects/ProjectV/src/render/SkyAtmosphere.cpp) / [SkyAtmosphere.hpp](file:///home/le1t/Projects/ProjectV/src/render/SkyAtmosphere.hpp) *(Medium · ~790 строк)*
    *   **Роль:** Симуляция рассеяния света в атмосфере на основе физической модели Рэлея-Ми.
*   [VolumetricFog.cpp](file:///home/le1t/Projects/ProjectV/src/render/VolumetricFog.cpp) / [VolumetricFog.hpp](file:///home/le1t/Projects/ProjectV/src/render/VolumetricFog.hpp) *(Medium · ~620 строк)*
    *   **Роль:** Расчет объемного тумана.

---

### 📂 5. Vulkan Low-Level (Слой Инициализации Vulkan)
Содержит низкоуровневые обертки для объектов Vulkan API.

*   [VulkanBootstrap.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanBootstrap.cpp) / [VulkanBootstrap.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanBootstrap.hpp) *(Medium · ~1110 строк)*
    *   **Роль:** Выбор физического GPU, проверка поддержки расширений (Ray Query, Acceleration Structure), создание Logical Device.
*   [VulkanSwapchain.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanSwapchain.cpp) / [VulkanSwapchain.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanSwapchain.hpp) *(Medium · ~530 строк)*
    *   **Роль:** Инициализация Swapchain, выбор формата кадра, реализация переключения Vsync (FIFO/Immediate).
*   [VulkanGraphicsPipelineCreate.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineCreate.cpp) / [VulkanGraphicsPipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipeline.hpp) *(Hard · ~590 строк)*
    *   **Роль:** Создание и компиляция графических конвейеров, настройка блендинга и тестов глубины.
*   [VulkanGraphicsPipelineBindings.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineBindings.cpp) *(Medium · ~360 строк)*
    *   **Роль:** Привязка Descriptor Set layouts и Push Constant диапазонов к графическим пайплайнам.
*   [VulkanGraphicsPipelineOverlay.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanGraphicsPipelineOverlay.cpp) *(Medium · ~470 строк)*
    *   **Роль:** Инициализация пайплайнов для вывода отладочных оверлеев и текста HUD.
*   [VulkanAsyncCompute.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanAsyncCompute.cpp) / [VulkanAsyncCompute.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanAsyncCompute.hpp) *(Medium · ~430 строк)*
    *   **Роль:** Оркестрация асинхронных вычислений на GPU с использованием Timeline Semaphores.
*   [VulkanFluidCaPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanFluidCaPipeline.cpp) / [VulkanFluidCaPipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanFluidCaPipeline.hpp) *(Medium · ~610 строк)*
    *   **Роль:** Пайплайн для симуляции физики жидкостей на GPU с использованием клеточного автомата.
*   [VulkanMeshShaderPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanMeshShaderPipeline.cpp) / [VulkanMeshShaderPipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanMeshShaderPipeline.hpp) *(Hard · ~790 строк)*
    *   **Роль:** Пайплайн для рендеринга вокселей через Mesh Shaders (Pattern C) на совместимых GPU.
*   [VulkanVoxelMeshingPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelMeshingPipeline.cpp) / [VulkanVoxelMeshingPipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelMeshingPipeline.hpp) *(Medium · ~480 строк)*
    *   **Роль:** Настройка вычислительного шейдера Greedy Mesher (`voxel_mesh.comp`).
*   [VulkanVoxelizePipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelizePipeline.cpp) / [VulkanVoxelizePipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanVoxelizePipeline.hpp) *(Hard · ~610 строк)*
    *   **Роль:** Пайплайн для GPU-вокселизации полигональных моделей в воксельное дерево.
*   [VulkanWorldGenPipeline.cpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanWorldGenPipeline.cpp) / [VulkanWorldGenPipeline.hpp](file:///home/le1t/Projects/ProjectV/src/render/vulkan/VulkanWorldGenPipeline.hpp) *(Medium · ~290 строк)*
    *   **Роль:** Вычислительный конвейер генерации шума рельефа на GPU.

---

### 📂 6. Physics (Физический Движок Jolt)
Управляет коллизиями твердых тел, склеиванием геометрии и движением персонажа.

*   [PhysicsWorld.cpp](file:///home/le1t/Projects/ProjectV/src/physics/PhysicsWorld.cpp) / [PhysicsWorld.hpp](file:///home/le1t/Projects/ProjectV/src/physics/PhysicsWorld.hpp) *(Medium · ~430 строк)*
    *   **Роль:** Инициализация Jolt Physics. Создает статические физические тела чанков и динамические тела моделей.
*   [PhysicsWorld_Walk.cpp](file:///home/le1t/Projects/ProjectV/src/physics/PhysicsWorld_Walk.cpp) / [PhysicsWorld_Internal.hpp](file:///home/le1t/Projects/ProjectV/src/physics/PhysicsWorld_Internal.hpp) *(Hard · ~780 строк)*
    *   **Роль:** Реализация перемещения игрока `walk` на основе `CharacterVirtual` из Jolt.
    *   **Особенности:** Сложный расчет поддержки под ногами (foot support score), сцепления со склонами, автоматического запрыгивания на блоки (auto-jump) иsneak режима (крадущийся шаг без падения с краев).
*   [GreedyPhysicsMerger.cpp](file:///home/le1t/Projects/ProjectV/src/physics/GreedyPhysicsMerger.cpp) / [GreedyPhysicsMerger.hpp](file:///home/le1t/Projects/ProjectV/src/physics/GreedyPhysicsMerger.hpp) *(Medium · ~120 строк)*
    *   **Роль:** Объединяет кубические воксели чанка в большие физические коробки (AABB). Снижает количество коллидеров в Jolt примерно в 35 раз.

---

### 📂 7. ECS (Flecs ECS Bridge)
Связывает сущности игры с логическими C++ системами.

*   [EcsWorld.cpp](file:///home/le1t/Projects/ProjectV/src/ecs/EcsWorld.cpp) / [EcsWorld.hpp](file:///home/le1t/Projects/ProjectV/src/ecs/EcsWorld.hpp) / [EcsWorld.ixx](file:///home/le1t/Projects/ProjectV/src/ecs/EcsWorld.ixx) *(Easy · ~320 строк)*
    *   **Роль:** Создает Flecs ECS мир, определяет компоненты игрока, камеры и симуляции, настраивает системы синхронизации состояний.

---

### 📂 8. Shaders (Шейдеры GLSL)
Программы, выполняемые на GPU. Определяют геометрию, освещение и симуляции.

*   [voxel.frag](file:///home/le1t/Projects/ProjectV/src/shaders/voxel.frag) *(Hard · ~1500 строк)*
    *   **Роль:** Основной фрагментный шейдер отрисовки вокселей.
    *   **Алгоритмы:** Содержит расчет PBR BRDF, сэмплирование DDGI-зондов глобального освещения, чтение маски RTX-теней, расчет отражений на воде и рефракции через NanoVDB трассировку.
*   [voxel.vert](file:///home/le1t/Projects/ProjectV/src/shaders/voxel.vert) *(Easy · ~150 строк)*
    *   **Роль:** Вершинный шейдер вокселей. Распаковывает компактную 16-байтовую структуру `PackedFace` в вершины полигонов, применяет субпиксельный джиттер TAA.
*   [voxel_mesh.comp](file:///home/le1t/Projects/ProjectV/src/shaders/voxel_mesh.comp) *(Hard · ~584 строки)*
    *   **Роль:** Вычислительный шейдер GPU Greedy Mesher. Сканирует чанк по 6 осям и склеивает грани блоков.
*   [probe_update.comp](file:///home/le1t/Projects/ProjectV/src/shaders/probe_update.comp) *(Hard · ~720 строк)*
    *   **Роль:** Обновляет DDGI зонды. Выпускает 64 луча из центров зондов через `rayQueryEXT` в TLAS, рассчитывает освещенность точек пересечения вокселей и сохраняет результат в 3D-текстуры с учетом гистерезиса.
*   [voxel_rtx_shadow.rgen](file:///home/le1t/Projects/ProjectV/src/shaders/voxel_rtx_shadow.rgen) *(Medium · ~180 строк)*
    *   **Роль:** RTX-шейдер генерации теней. Выпускает первичный луч из камеры для поиска геометрии, затем вторичный луч к солнцу через TLAS для записи в `rtxShadowMask`.
*   [fluid_ca.comp](file:///home/le1t/Projects/ProjectV/src/shaders/fluid_ca.comp) *(Hard · ~480 строк)*
    *   **Роль:** GPU симуляция воды клеточным автоматом (CA).
*   [hzb_cull.comp](file:///home/le1t/Projects/ProjectV/src/shaders/hzb_cull.comp) *(Medium · ~310 строк)*
    *   **Роль:** Проверяет видимость AABB чанков по HZB-пирамиде глубин, отсекая невидимые чанки.

---

## 4. Глубокое погружение в алгоритмы движка

### 4.1 Гибридное хранение вокселей
Данные о вокселях синхронизируются между хостом и видеокартой:
*   **Sparse64Tree (CPU):** Позволяет динамически изменять блоки, проводить трассировку лучей для прицела на CPU и перестраивать физику.
*   **NanoVDB (GPU):** Линеаризованная структура данных, записанная в SSBO. Любой шейдер может вызвать `TraceRay` по NanoVDB на GPU без обращения к CPU-памяти. Это используется для рефракции света в воде/стекле и расчета прямого освещения зондов.

### 4.2 GPU Greedy Meshing
Процесс превращения кубического мира в полигоны:
1.  Чанк (8x8x8) обрабатывается вычислительным шейдером по шести направлениям.
2.  Шейдер использует битовую маску для отслеживания посещенных граней вокселей.
3.  Одинаковые соседние грани объединяются по ширине и высоте в один квад.
4.  Вывод записывается в буфер `opaqueIndirectBuffer`. Отрисовка происходит через indirect-команды без пересылок на CPU.

### 4.3 Аппаратные RTX тени
Для замены медленных каскадных теней (CSM) с их артефактами (Peter Panning) внедрены аппаратные лучи:
*   Для каждого чанка на GPU создается `VkAccelerationStructureKHR` типа BLAS.
*   Все BLAS объединяются в TLAS сцены.
*   Шейдер `voxel_rtx_shadow.rgen` трассирует лучи солнца.
*   Вода и стекло игнорируются при пересечении теней (они не отбрасывают жестких теней).
*   Фрагментный шейдер просто читает полученную текстуру `rtxShadowMask` по экранным координатам.

---

## 5. Как изучать этот код самостоятельно?

Рекомендуется следующий порядок чтения файлов:

1.  **Поймите структуры данных:** Изучите `Types.hpp` и `Sparse64Tree.hpp`.
2.  **Проследите игровой цикл:** Откройте `main.cpp` -> `SDL_AppIterate` и посмотрите, как вызываются `UpdateApp` и `DrawFrame`.
3.  **Изучите физику:** Прочтите `PhysicsWorld_Walk.cpp`, уделив внимание расчету поддержки под ногами.
4.  **Разберитесь с геометрией:** Прочитайте `voxel_mesh.comp` (как работает жадный мешинг на GPU).
5.  **Изучите проходы кадра:** Откройте `RendererDrawFrame.cpp` и `RendererRecordCommands.cpp`.
6.  **Трассировка лучей:** Изучите `RayTracedShadows.cpp` и шейдер `voxel_rtx_shadow.rgen`.
7.  **Освещение:** Прочтите `RtxGiProbes.cpp` и `probe_update.comp`.
