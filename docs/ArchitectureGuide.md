# ProjectV Architecture Guide

Дата фиксации: `2026-04-07`

Этот документ описывает текущую mainline-архитектуру `ProjectV` после закрытия `8.5`: не “идеальный движок на будущее”,
а реально работающий voxel MVP slice с interaction, HUD, ECS bridge и базовой physics-интеграцией.

## Что такое `ProjectV` сейчас

`ProjectV` в текущем состоянии:

- рендерит CPU-backed `VoxelWorld` через Vulkan 1.4;
- позволяет выбирать, ставить и удалять блоки в runtime;
- поддерживает `creative`, `spectator` и `walk`;
- держит minimal ECS и minimal physics как practical slices, а не как тотальный rewrite;
- использует полностью RTX-Driven конвейер освещения (аппаратные тени солнца и сетку зондов DDGI глобального освещения);
- производит Greedy Meshing и HZB куллинг чанков на GPU.

Главный принцип архитектуры сейчас такой:

- `VoxelWorld` остаётся источником истины для мира;
- ECS добавляет структуру данных и мосты между подсистемами;
- physics усиливает interaction loop, а не подменяет его;
- app loop остаётся явным и читаемым;
- Vulkan-рендерер логически разделен на компактные доменные C++ файлы (до 600 строк каждый) для упрощения сопровождения.

## Верхний runtime lifecycle

Точка входа находится в [main.cpp](../src/app/main.cpp).

Основной жизненный цикл:

1. `SDL_AppInit`
2. `InitializeAppEcs`
3. `InitVulkan`
4. `SDL_AppEvent`
5. `SDL_AppIterate`
6. `SDL_AppQuit`

Практически это означает:

- ECS создаётся до Vulkan и живёт как часть `AppState`;
- Vulkan init создаёт окно, swapchain, scene resources, pipelines, world и physics state;
- runtime update идёт кадр-за-кадром через `UpdateApp -> SyncEcsWorldState -> PrepareFrameRenderData -> DrawFrame`;
- shutdown остаётся явным через `ShutdownVulkan`, а не спрятанным в деструкторах.

## Ownership и состояние

Центральный контейнер — `AppState` в [Types.hpp](../src/core/Types.hpp).

Он держит:

- `PlatformState` — SDL window и lifecycle-флаги;
- `VulkanContextState` и `SwapchainState` — Vulkan runtime;
- `WorldState` — ownership `VoxelWorld`;
- `RenderState` и `FrameState` — GPU resources, pipelines и per-frame buffers;
- `SimulationState` — fixed-step timing;
- `InputState` и `InteractionState` — input и selection/edit state;
- `EcsStatePtr` — minimal `flecs` world;
- `PhysicsStatePtr` — Jolt-backed `creative` / `walk` collision state.

Важно:

- ownership voxel мира всё ещё находится в `WorldState`, а не переехал в ECS;
- ECS и physics читают/зеркалят этот state, но не отбирают ownership;
- такой split выбран специально, чтобы не делать premature full migration.

## Подсистемы и границы

### `app/`

`app/` оркестрирует верхний runtime loop:

- input actions;
- camera update;
- simulation tick;
- interaction;
- frame preparation;
- benchmark + lookdev capture automation.

Здесь нет “толстого gameplay framework”. Это сознательная orchestration layer.

### `asset/`

`asset/` загружает и управляет 3D-моделями:

- загрузка glTF через fastgltf;
- декомпрессия Draco;
- бэйкинг meshoptimizer (vertex cache + fetch optimization);
- загрузка манифестов моделей из JSON;
- GPU upload + model pass.

### `audio/`

`audio/` — минимальный аудиодвижок на miniaudio:

- музыкальный плейлист с async scan через `std::jthread`;
- play/pause/stop/next/prev/volume;
- информация о текущем треке в HUD.

### `voxel/`

`voxel/` хранит главный источник истины для мира:

- SVO (Sparse64Tree) voxel storage;
- chunk bookkeeping;
- dirty queue;
- CPU interaction raycast;
- remove/place edits.

Именно этот слой определяет, что существует в мире, какие чанки грязные и какие данные должны попасть в render и
physics.

### `ecs/`

`ecs/` сейчас нужен как minimal data-model slice:

- primary camera entity;
- primary player entity;
- `world` singleton;
- `debug` singleton;
- chunk mirror summary.

Это ещё не “весь движок на ECS”. Это bridge к более явной структуре данных без потери current mainline.

### `physics/`

`physics/` держит minimal Jolt integration:

- static collision body из solid voxels;
- physics raycast;
- `CharacterVirtual` controller для `creative` / `walk`;
- sync с миром через `VoxelWorld::editVersion`.

Interaction при этом остаётся на CPU `VoxelRaycast`. Physics не подменяет mainline selection/edit loop.

### `render/` и `render/vulkan/`

Render слой делится на две части:

- `render/` — scene resources и orchestration draw path. В рамках повышения читаемости монолитные файлы
  декомпозированы (лимит 600 строк на файл):
    - **Renderer** разделен на: `RendererDrawFrame.cpp` (синхронизация кадра, HZB, презентация),
      `RendererRecordCommands.cpp` (запись команд отрисовки), `RendererOverlay.cpp` (дебаг-оверлеи),
      `RendererScreenshot.cpp` (захват кадра).
    - **SceneResources** разделен на: `SceneResourcesUpdate.cpp` (заливка CPU→GPU), `SceneResourcesVisibility.cpp` (CPU
      куллинг), `SceneResourcesDestroy.cpp` (deferred NanoVDB очистка), `SceneResourcesUtilities.cpp` (VMA хелперы).
    - **RTX Shadows** разделены на: `RayTracedShadowsBlas.cpp` (сборка BLAS), `RayTracedShadowsTlas.cpp` (сборка TLAS),
      `RayTracedShadowsPass.cpp` (запись прохода), `RayTracedShadowsMask.cpp` (маска + fallback).
    - **DDGI** разделена на: `RtxGiProbesPipeline.cpp` (compute pipeline), `RtxGiProbesUpdate.cpp` (запись прохода).
- `render/vulkan/` — bootstrap (`VulkanBootstrap.cpp`), swapchain, pipelines и Vulkan-specific plumbing:
    - **VulkanGraphicsPipeline** разделен на: `VulkanGraphicsPipelineCreate.cpp` (создание конвейеров),
      `VulkanGraphicsPipelineBindings.cpp` (привязка дескрипторов), `VulkanGraphicsPipelineOverlay.cpp` (пайплайны
      оверлеев).
    - Асинхронная вычислительная очередь `VulkanAsyncCompute.cpp` с timeline semaphores.
    - Пайплайны: `VulkanFluidCaPipeline.cpp` (жидкость), `VulkanVoxelMeshingPipeline.cpp` (greedy mesher),
      `VulkanMeshShaderPipeline.cpp` (mesh shaders), `VulkanVoxelizePipeline.cpp` (вокселизация),
      `VulkanWorldGenPipeline.cpp` (генерация мира).

Compute meshing, graphics passes, overlay и HUD собираются здесь, но данные приходят из `VoxelWorld` через
`SceneResources`.

### `c_kernels/`

`c_kernels/` — CPU-оптимизированные ядра (AVX2) для производительных операций:

- `frustum_cull.cpp` — AVX2-оптимизированный frustum culling для CPU-side pre-culling моделей.

### `bench/`

`bench/` — микро-бенчмарки:

- `FrustumCullBenchmark.cpp` — измерение производительности frustum culling kernels.

### `debug/`

`debug/` отвечает за:

- Tracy glue;
- GPU profiling glue;
- HUD generation;
- CPU planning для lightweight debug overlay boxes.

Это intentional lightweight debug layer, а не editor framework: editor-like tooling остаётся keyboard-driven и сидит на
том же CPU `VoxelRaycast`/overlay path без `imgui` и без отдельного UI-стека.

## Как идёт кадр

Высокоуровневый кадр сейчас выглядит так:

1. `SDL_AppEvent` обновляет input и window lifecycle flags.
2. `UpdateApp`:
   - применяет hotkeys;
   - крутит control modes;
   - тикает `creative`, `spectator` или `walk`;
   - делает runtime interaction через `classic` remove/place или активный debug editor tool;
   - синхронизирует physics после world edits.
3. `SyncEcsWorldState` обновляет ECS summary.
4. `PrepareFrameRenderData`:
   - забирает dirty chunk rebuild requests из `VoxelWorld`;
   - обновляет CPU-side scene resources;
   - ждёт fence текущего кадра;
   - загружает актуальные scene buffers в per-frame mapped buffers;
   - строит HUD vertices, selection state и debug overlay boxes.
5. `DrawFrame`:
    - **Drain** deferred NanoVDB destroys (VMA cleanup).
    - **Acquire** `vkAcquireNextImageKHR(UINT64_MAX)`.
    - **Wait+reset** fences + cmd buffer.
    - **Pre-graphics:**
        - Mesh shader pre-cull (compute dispatch).
        - RTX: collect dirty BLAS chunks → `BuildDirtyBlases` (one-shot cmd + fence) → `UpdateTlas` (instance write) →
          `RecordVoxelAwareRtxShadowPass` (пишет `rtxShadowMask`).
        - DDGI: `RecordRtxGiProbeUpdatePass` (обновление зондов, round-robin по 1 зонду/кадр).
    - **Graphics:**
        - Voxel meshing compute dispatch (`voxel_mesh.comp`), если есть dirty чанки.
        - TLAS build + barrier `AS_BUILD→FRAGMENT`.
        - Image transitions → `COLOR_ATTACHMENT`.
        - Sky atmosphere pre-pass (если включено).
        - Opaque voxel pass: `vkCmdDrawIndirect` или `vkCmdDrawIndirectCountKHR` (HZB) или `vkCmdDrawMeshTasksEXT` (mesh
          shader).
        - Model pass: `vkCmdDrawIndexed` per visible instance.
        - Transparent voxel pass: `vkCmdDrawIndirect`.
        - Debug overlay + HUD.
        - Cloudscape ray-march (если включено).
    - **Blit** из `sceneColorTarget` в swapchain image.
    - **HZB chain:** `BuildHizMipChain` (depth → mip chain) + HZB cull dispatch.
    - **Inline compute** (если async path inactive): Fluid CA dispatch × N + WorldGen.
    - **Submit:** `vkQueueSubmit2` (wait = imageAvailableSemaphore + compute timeline).
    - **Present:** `vkQueuePresentKHR` + screenshot capture.

## Control modes

Семантика режимов сейчас такая:

- `creative` — collision-backed flight mode, подчиняется `pause` вместе с physics, edits разрешены;
- `spectator` — observe-only noclip mode, не зависит от `pause` для camera movement, edits запрещены;
- `walk` — grounded collision-based mode, использует physics controller с continuous voxel foot-support score под стопой, коротким `stick-to-floor` step-down и `OnContactSolve` anti-slide listener'ом, который при partial support без input режет downhill-компоненту только на одном best floor-like static contact за кадр; после полной потери опоры режим переходит в обычный gravity fall без lower-floor snap, jump кратко лочит sample-based regrounding и остаётся физическим без `Y`-snap helper'ов, при этом top-edge snag допускает только узкий non-rising ledge catch без upward snap, но уже с lateral probes по ширине капсулы и только из pre-step grounded support, а `Shift` включает отдельный lower-stance sneak path с непрерывной face-based support geometry: pre-move safe-walk проектирует `desired feet XZ` в объединение walkable top-face support area, cached support-region grace и post-solve correction/stick-to-floor используют ту же область; если `feet XZ` остаётся внутри cached support-region, sneak может вернуть небольшой solver-driven drop обратно к cached top-face height, а отпускание `Shift` на уже безопасной кромке удерживается коротким `ledge release` hold вместо мгновенного off-edge drop.

Это не финальная gameplay-модель. Это честный MVP split между debug camera, observe mode и базовым player-like mode.

## Что не надо путать с текущей архитектурой

Сейчас в проекте ещё нет:

- большого gameplay scheduler;
- полного ownership мира через ECS;
- “настоящего” player/game object stack;
- сложного editor UI;
- полноценного persistence stack beyond world-only snapshots.

Если в коде есть `flecs` и `Jolt`, это не означает, что `ProjectV` уже стал полноценным ECS-first physics engine. Пока
это practical mainline slices.

## Связанные документы

- [Linux Build & Run Guide](Linux_Build_And_Run.md) (Основное руководство по Linux)
- [RTX Renderer Architecture](RTX_Renderer_Architecture.md) (Архитектура RTX-рендеринга)
- [Physics & Movement Guide](Physics_And_Movement_Guide.md) (Физика и перемещение Jolt)
- [BuildAndRun (Windows)](BuildAndRun.md)
- [RenderArchitecture (Historical)](RenderArchitecture.md)
- [VoxelWorld (Historical)](VoxelWorld.md)
- [Debugging](Debugging.md)
- [Source Layout Guide](source_layout.md)
