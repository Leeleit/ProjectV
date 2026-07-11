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
- `app/AppUpdateHelpers.cpp`
- `app/Camera.cpp`
- `app/InputActions.cpp`
- `app/InputReplay.cpp`
- `app/FramePreparation.cpp`
- `app/ModelGravigun.cpp`
- `app/BenchmarkAutomation.cpp`
- `app/LookDevCaptureAutomation.cpp`

### `src/core/`

- общие типы приложения;
- runtime diagnostics;
- runtime probes;
- shader IO.

Ключевые файлы:

- `core/Types.hpp` / `core/Types.cpp`
- `core/RuntimeDiagnostics.cpp`
- `core/RuntimeProbe.cpp`
- `core/ShaderIO.cpp`
- `core/Math.ixx`
- `core/StringId.ixx`
- `core/Probe.ixx`

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
- `ecs/EcsWorld.ixx`

### `src/physics/`

- минимальный physics glue layer на `JoltPhysics`;
- static voxel collision world;
- physics raycast;
- `CharacterVirtual` controller для `creative` / `walk` и sync с voxel edits.

Ключевые файлы:

- `physics/PhysicsWorld.cpp`
- `physics/PhysicsWorld.hpp`
- `physics/PhysicsWorld_Walk.cpp`
- `physics/PhysicsWorld_Internal.hpp`
- `physics/GreedyPhysicsMerger.cpp`

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
- `render/RendererDrawFrame.cpp`
- `render/RendererRecordCommands.cpp`
- `render/RendererOverlay.cpp`
- `render/RendererScreenshot.cpp`
- `render/SceneResources.cpp`
- `render/SceneResourcesUpdate.cpp`
- `render/SceneResourcesVisibility.cpp`
- `render/SceneResourcesDestroy.cpp`
- `render/SceneResourcesUtilities.cpp`
- `render/RayTracedShadows.cpp`
- `render/RayTracedShadowsBlas.cpp`
- `render/RayTracedShadowsTlas.cpp`
- `render/RayTracedShadowsPass.cpp`
- `render/RayTracedShadowsMask.cpp`
- `render/RtxGiProbes.cpp`
- `render/RtxGiProbesPipeline.cpp`
- `render/RtxGiProbesUpdate.cpp`
- `render/HizCulling.cpp`
- `render/vulkan/VulkanInit.cpp`
- `render/vulkan/VulkanSwapchain.cpp`
- `render/vulkan/VulkanGraphicsPipeline.cpp`
- `render/vulkan/VulkanGraphicsPipelineCreate.cpp`
- `render/vulkan/VulkanGraphicsPipelineBindings.cpp`
- `render/vulkan/VulkanGraphicsPipelineOverlay.cpp`
- `render/vulkan/VulkanAsyncCompute.cpp`
- `render/vulkan/VulkanFluidCaPipeline.cpp`
- `render/vulkan/VulkanMeshShaderPipeline.cpp`
- `render/vulkan/VulkanVoxelMeshingPipeline.cpp`
- `render/vulkan/VulkanVoxelizePipeline.cpp`
- `render/vulkan/VulkanWorldGenPipeline.cpp`

### `src/voxel/`

- `VoxelWorld`;
- voxel materials;
- raycast;
- runtime interaction.

Ключевые файлы:

- `voxel/VoxelWorld.cpp`
- `voxel/VoxelWorldInternal.hpp`
- `voxel/VoxelWorldPreset.cpp`
- `voxel/VoxelWorldSnapshot.cpp`
- `voxel/VoxelWorldFluid.cpp`
- `voxel/VoxelWorldLod.cpp`
- `voxel/VoxelWorldStatic.cpp`
- `voxel/VoxelMaterials.cpp`
- `voxel/VoxelRaycast.cpp`
- `voxel/VoxelInteraction.cpp`
- `voxel/Sparse64Tree.hpp`
- `voxel/NanoVdb.cpp`
- `voxel/ChunkStreamer.cpp`
- `voxel/VoxelLodDownsample.cpp`
- `voxel/CpuMeshGenerator.cpp`
- `voxel/SceneConfig.cpp`

### `src/shaders/`

Шейдеры пока сознательно остаются отдельной плоской зоной внутри `src/`. Это не считается нарушением `8.2`: перенос shader assets в другую структуру не был нужен для текущего mainline.

Ключевые файлы:

- `shaders/voxel.frag`
- `shaders/voxel.vert`
- `shaders/voxel_mesh.comp`
- `shaders/probe_update.comp`
- `shaders/voxel_rtx_shadow.rgen`
- `shaders/fluid_ca.comp`
- `shaders/hzb_cull.comp`

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
- [README](../README.md)
- [Voxel MVP Smoke Checklist](voxel_mvp_smoke_checklist.md)
- [TODO](../TODO.md)
