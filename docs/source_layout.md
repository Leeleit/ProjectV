# Source Layout Guide

Дата фиксации: `2026-04-07`

Этот документ описывает текущую физическую раскладку `src/` и правило include boundaries после закрытия `8.2`, включая
добавленные позже practical slices `src/ecs` из `8.3` и `src/physics` из `8.4`.

## Правило include paths

`ProjectV` больше не полагается на широкие transitional include directories вроде `src/app`, `src/render` или `src/voxel`.

Для production target и test target в include path остаётся только корень `src/`, поэтому project headers нужно подключать явными путями от корня:

```cpp
#include "app/Camera.hpp"
#include "core/Types.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "voxel/VoxelWorld.hpp"
```

Практический смысл:

- include path сразу показывает подсистему;
- физическая раскладка `src/` становится частью архитектурной границы;
- новые файлы не должны возвращаться к старому стилю `#include "Camera.hpp"` или `#include "Types.hpp"`.

## Текущая структура `src/`

```text
src/
  app/
  core/
  debug/
  ecs/
  physics/
  platform/
  render/
    vulkan/
  shaders/
  voxel/
```

### `src/app/`

- верхний runtime loop;
- camera update;
- input actions;
- frame preparation;
- `main.cpp`.

Ключевые файлы:

- `app/main.cpp`
- `app/AppUpdate.cpp`
- `app/Camera.cpp`
- `app/InputActions.cpp`
- `app/FramePreparation.cpp`

### `src/core/`

- общие типы приложения;
- runtime diagnostics;
- runtime probes;
- shader IO.

Ключевые файлы:

- `core/Types.hpp`
- `core/RuntimeDiagnostics.cpp`
- `core/RuntimeProbe.cpp`
- `core/ShaderIO.cpp`

### `src/debug/`

- profiling glue;
- HUD generation.

Ключевые файлы:

- `debug/DebugHud.cpp`
- `debug/Profiling.hpp`
- `debug/ProfilingGpu.hpp`

### `src/ecs/`

- минимальный `flecs` glue layer;
- primary camera/player entities;
- `world`/`debug` singleton data;
- on-demand chunk mirror entities и world summary для текущего voxel slice.

Ключевые файлы:

- `ecs/EcsWorld.cpp`
- `ecs/EcsWorld.hpp`

### `src/physics/`

- минимальный physics glue layer на `JoltPhysics`;
- static voxel collision world;
- physics raycast;
- `CharacterVirtual` controller для `creative` / `walk` и sync с voxel edits.

Ключевые файлы:

- `physics/PhysicsWorld.cpp`
- `physics/PhysicsWorld.hpp`

### `src/platform/`

- SDL/window event helpers.

Ключевые файлы:

- `platform/PlatformEvents.cpp`

### `src/render/`

- renderer main pass;
- scene resources;
- Vulkan-specific rendering backend в `render/vulkan/`.

Ключевые файлы:

- `render/Renderer.cpp`
- `render/SceneResources.cpp`
- `render/vulkan/VulkanInit.cpp`
- `render/vulkan/VulkanSwapchain.cpp`
- `render/vulkan/VulkanGraphicsPipeline.cpp`
- `render/vulkan/VulkanVoxelMeshingPipeline.cpp`

### `src/voxel/`

- `VoxelWorld`;
- voxel materials;
- raycast;
- runtime interaction.

Ключевые файлы:

- `voxel/VoxelWorld.cpp`
- `voxel/VoxelMaterials.cpp`
- `voxel/VoxelRaycast.cpp`
- `voxel/VoxelInteraction.cpp`

### `src/shaders/`

Шейдеры пока сознательно остаются отдельной плоской зоной внутри `src/`. Это не считается нарушением `8.2`: перенос shader assets в другую структуру не был нужен для текущего mainline.

## Что это меняет для будущих правок

- новые `.cpp/.hpp` должны жить в своей подсистеме сразу;
- новые include-строки должны использовать qualified path от `src/`;
- если подсистема начинает разрастаться, сначала уточняется её каталог и include boundary, а не только имя файла;
- test target должен следовать тем же include rules, что и production target.

## Связанные документы

- [Build And Run](BuildAndRun.md)
- [Architecture Guide](ArchitectureGuide.md)
- [Render Architecture](RenderArchitecture.md)
- [VoxelWorld](VoxelWorld.md)
- [Debugging](Debugging.md)
- [Profiling](Profiling.md)
- [README_NEW](../README_NEW.md)
- [Voxel MVP Smoke Checklist](voxel_mvp_smoke_checklist.md)
- [TODO](../TODO.md)
