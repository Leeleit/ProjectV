# ProjectV Architecture Guide

Дата фиксации: `2026-04-07`

Этот документ описывает текущую mainline-архитектуру `ProjectV` после закрытия `8.5`: не “идеальный движок на будущее”,
а реально работающий voxel MVP slice с interaction, HUD, ECS bridge и базовой physics-интеграцией.

## Что такое `ProjectV` сейчас

`ProjectV` в текущем состоянии:

- рендерит CPU-backed `VoxelWorld` через Vulkan;
- позволяет выбирать, ставить и удалять блоки в runtime;
- поддерживает `creative`, `spectator` и `walk`;
- держит minimal ECS и minimal physics как practical slices, а не как тотальный rewrite.

Главный принцип архитектуры сейчас такой:

- `VoxelWorld` остаётся источником истины для мира;
- ECS добавляет структуру данных и мосты между подсистемами;
- physics усиливает interaction loop, а не подменяет его;
- app loop остаётся явным и читаемым.

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
- frame preparation.

Здесь нет “толстого gameplay framework”. Это сознательная orchestration layer.

### `voxel/`

`voxel/` хранит главный источник истины для мира:

- dense voxel storage;
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

- `render/` — scene resources и orchestration draw path;
- `render/vulkan/` — bootstrap, swapchain, pipelines и Vulkan-specific plumbing.

Compute meshing, graphics passes, overlay и HUD собираются здесь, но данные приходят из `VoxelWorld` через
`SceneResources`.

### `debug/`

`debug/` отвечает за:

- Tracy glue;
- GPU profiling glue;
- HUD generation.

Это intentional lightweight debug layer, а не editor framework.

## Как идёт кадр

Высокоуровневый кадр сейчас выглядит так:

1. `SDL_AppEvent` обновляет input и window lifecycle flags.
2. `UpdateApp`:
   - применяет hotkeys;
   - крутит control modes;
   - тикает `creative`, `spectator` или `walk`;
   - делает runtime interaction;
   - синхронизирует physics после world edits.
3. `SyncEcsWorldState` обновляет ECS summary.
4. `PrepareFrameRenderData`:
   - забирает dirty chunk rebuild requests из `VoxelWorld`;
   - обновляет CPU-side scene resources;
   - ждёт fence текущего кадра;
   - загружает актуальные scene buffers в per-frame mapped buffers;
   - строит HUD vertices и push constants.
5. `DrawFrame`:
   - acquire image;
   - при необходимости recreates swapchain;
   - dispatch compute meshing;
   - рисует opaque/transparent passes;
   - рисует selection overlay, crosshair и HUD;
   - present.

## Control modes

Семантика режимов сейчас такая:

- `creative` — collision-backed flight mode, подчиняется `pause` вместе с physics, edits разрешены;
- `spectator` — observe-only noclip mode, не зависит от `pause` для camera movement, edits запрещены;
- `walk` — grounded collision-based mode, использует physics controller и разрешает edits.

Это не финальная gameplay-модель. Это честный MVP split между debug camera, observe mode и базовым player-like mode.

## Что не надо путать с текущей архитектурой

Сейчас в проекте ещё нет:

- большого gameplay scheduler;
- полного ownership мира через ECS;
- “настоящего” player/game object stack;
- сложного editor UI;
- save/load pipeline.

Если в коде есть `flecs` и `Jolt`, это не означает, что `ProjectV` уже стал полноценным ECS-first physics engine. Пока
это practical mainline slices.

## Связанные документы

- [BuildAndRun](BuildAndRun.md)
- [RenderArchitecture](RenderArchitecture.md)
- [VoxelWorld](VoxelWorld.md)
- [Debugging](Debugging.md)
- [Source Layout Guide](source_layout.md)
