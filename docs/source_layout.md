# Source Layout Guide

Дата фиксации: `2026-04-07`

Этот документ описывает текущую физическую раскладку `src/` и правило include boundaries после закрытия `8.2`, включая
добавленные позже practical slices `src/ecs` из `8.3` и `src/physics` из `8.4`.

## Правило include paths

`ProjectV` больше не полагается на широкие transitional include directories вроде `src/app`, `src/render` или
`src/voxel`.

Для production target и test target в include path остаётся только корень `src/`, поэтому project headers нужно
подключать явными путями от корня:

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
  asset/
  audio/
  bench/
  c_kernels/
  core/
  debug/
  ecs/
  physics/
    walk/
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

- `app/main.cpp` — точка входа (SDL_AppInit / Event / Iterate / Quit)
- `app/AppUpdate.cpp` — per-frame update logic
- `app/AppUpdateHelpers.cpp` — хелперы для камеры и сброса состояний
- `app/Camera.cpp` — управление камерой (view/projection/jitter)
- `app/FramePreparation.cpp` — подготовка данных кадра (HUD, selection, HZB params)
- `app/InputActions.cpp` — маппинг клавиш на InputAction
- `app/InputReplay.cpp` — запись/воспроизведение ввода
- `app/ModelGravigun.cpp` — гравитационная пушка
- `app/BenchmarkAutomation.cpp` — автоматизация бенчмарков
- `app/LookDevCaptureAutomation.cpp` — автоматический захват скриншотов

### `src/asset/`

- загрузка glTF через fastgltf;
- декомпрессия Draco;
- бэйкинг meshoptimizer (vertex cache + fetch optimization);
- загрузка манифестов моделей и регистрация на GPU.

Ключевые файлы:

- `asset/AssetLoader.cpp`
- `asset/AssetManifest.cpp`
- `asset/AssetRegistry.cpp`
- `asset/DracoMeshDecoder.cpp`
- `asset/MeshBaker.cpp`
- `asset/MeshGpuResources.cpp`
- `asset/ModelPass.cpp`
- `asset/ModelManifestLoader.cpp`

### `src/audio/`

- аудиодвижок на miniaudio;
- async scan плейлиста через `std::jthread`;
- управление громкостью (0.0..1.0, шаг 0.05);
- горячие клавиши: Q play/pause, E stop, 7/8 volume, 9/0 next/prev.

Ключевые файлы:

- `audio/AudioEngine.cpp`

### `src/bench/`

- FrustumCullBenchmark — AVX2-оптимизированный микро-бенчмарк.

Ключевые файлы:

- `bench/FrustumCullBenchmark.cpp`

### `src/c_kernels/`

- CPU-оптимизированные ядра (AVX2) для производительных операций.

Ключевые файлы:

- `c_kernels/frustum_cull.cpp`
- `c_kernels/FrustumCulling.cpp`

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

- profiling glue (Tracy);
- HUD generation (текстовый HUD с FPS, статистикой чанков, параметрами освещения);
- 3D debug overlay boxes (chunk bounds, dirty chunk highlights).

Ключевые файлы:

- `ui/ImGuiLayer.cpp` / `ui/HudPanels.cpp` — Dear ImGui HUD (status strip, Settings, Stats)
- `debug/DebugOverlays.cpp` — 3D дебаг-оверлеи (AABB)
- `debug/Profiling.hpp` / `debug/ProfilingGpu.hpp` — Tracy макросы

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

- `physics/PhysicsWorld.cpp` / `physics/PhysicsWorld.hpp` — инициализация Jolt, синхронизация мира
- `physics/PhysicsWorld_Walk.cpp` — walk-режим: support score, edge grace, auto-jump, sneak
- `physics/PhysicsWorld_Internal.hpp` — PhysicsState: JPH::PhysicsSystem, CharacterVirtual, chunk body map
- `physics/GreedyPhysicsMerger.cpp` — жадное объединение вокселей в AABB (35× reduction)
- `physics/walk/` — вспомогательные файлы walk-режима (WalkConstants.hpp, WalkInternals.cpp)

### `src/platform/`

- SDL/window event helpers.

Ключевые файлы:

- `platform/PlatformEvents.cpp`

### `src/render/`

- renderer main pass;
- scene resources;
- Vulkan-specific rendering backend в `render/vulkan/`.

Ключевые файлы:

- `render/Renderer.cpp` / `render/Renderer.hpp`
- `render/RendererDrawFrame.cpp` — синхронизация кадра, HZB, презентация
- `render/RendererRecordCommands.cpp` — запись графических команд
- `render/RendererOverlay.cpp` — дебаг-оверлеи AABB
- `render/RendererScreenshot.cpp` — захват кадра в BMP
- `render/SceneResources.cpp` / `render/SceneResources.hpp` — оркестрация GPU-ресурсов
- `render/SceneResourcesUpdate.cpp` — заливка CPU→GPU
- `render/SceneResourcesVisibility.cpp` — CPU frustum culling
- `render/SceneResourcesDestroy.cpp` — deferred NanoVDB очистка
- `render/SceneResourcesUtilities.cpp` — VMA хелперы
- `render/RayTracedShadows.cpp` / `render/RayTracedShadows.hpp` — менеджер RTX-теней
- `render/RayTracedShadowsBlas.cpp` — сборка BLAS чанков
- `render/RayTracedShadowsTlas.cpp` — сборка TLAS сцены
- `render/RayTracedShadowsPass.cpp` — запись прохода теней
- `render/RayTracedShadowsMask.cpp` — маска теней и fallback-текстуры
- `render/RtxShadowPipeline.cpp` / `render/RtxShadowSBT.cpp` — RTX pipeline + SBT
- `render/RtxGiProbes.cpp` / `render/RtxGiProbes.hpp` — DDGI зонды
- `render/RtxGiProbesPipeline.cpp` — compute pipeline DDGI
- `render/RtxGiProbesUpdate.cpp` — запись прохода обновления зондов
- `render/HizCulling.cpp` / `render/HizCulling.hpp` — HZB occlusion culling
- `render/LodDownsampleGpuConsume.cpp` — LOD downsample на GPU
- `render/SkyAtmosphere.cpp` — атмосфера (Preetham / Hillaire)
- `render/Cloudscape.cpp` — облака (Nubis ray-march)
- `render/VolumetricFog.cpp` — объёмный туман (Wronski froxel)
- `render/vulkan/VulkanBootstrap.cpp` / `render/vulkan/VulkanBootstrap.hpp` — инициализация Vulkan
- `render/vulkan/VulkanSwapchain.cpp` — управление swapchain
- `render/vulkan/VulkanGraphicsPipeline.cpp` — управление пайплайнами
- `render/vulkan/VulkanGraphicsPipelineCreate.cpp` — создание конвейеров
- `render/vulkan/VulkanGraphicsPipelineBindings.cpp` — привязка дескрипторов
- `render/vulkan/VulkanGraphicsPipelineOverlay.cpp` — пайплайны оверлеев + HUD
- `render/vulkan/VulkanAsyncCompute.cpp` — асинхронные вычисления (timeline semaphores)
- `render/vulkan/VulkanFluidCaPipeline.cpp` — pipeline для Fluid CA
- `render/vulkan/VulkanMeshShaderPipeline.cpp` — pipeline для mesh shaders (Pattern C)
- `render/vulkan/VulkanVoxelMeshingPipeline.cpp` — pipeline для greedy mesher
- `render/vulkan/VulkanVoxelizePipeline.cpp` — pipeline для вокселизации моделей
- `render/vulkan/VulkanWorldGenPipeline.cpp` — pipeline для генерации мира
- `render/vulkan/HardwareRayTracingProbe.cpp` — проверка поддержки аппаратной трассировки

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

Шейдеры пока сознательно остаются отдельной плоской зоной внутри `src/`. Это не считается нарушением `8.2`: перенос
shader assets в другую структуру не был нужен для текущего mainline.

Ключевые файлы:

- `shaders/voxel.frag` — основной фрагментный шейдер (PBR + DDA + RTX + DDGI)
- `shaders/voxel.vert` — вершинный шейдер вокселей
- `shaders/voxel_mesh.comp` — GPU greedy meshing (compute)
- `shaders/voxel_mesh_pre.comp` — frustum culling pre-pass для mesh shaders
- `shaders/voxel_mesh.mesh` — mesh shader (Pattern C)
- `shaders/probe_update.comp` — DDGI probe update (compute)
- `shaders/voxel_rtx_shadow.rgen` — RTX ray generation (тени солнца)
- `shaders/voxel_rtx_shadow.rint` — RTX procedural intersection
- `shaders/voxel_rtx_shadow.rchit` — RTX closest hit
- `shaders/voxel_rtx_shadow.rmiss` — RTX miss
- `shaders/fluid_ca.comp` — GPU клеточный автомат жидкости
- `shaders/hzb_cull.comp` — HZB occlusion culling
- `shaders/world_gen.comp` — процедурная генерация мира
- `shaders/voxelize.comp` — вокселизация моделей
- `shaders/lighting.glsl` — PBR BRDF функции (GGX, Fresnel-Schlick, Smith, tone mapping)
- `shaders/model.vert` / `shaders/model.frag` — шейдеры моделей
- `shaders/sky_atmosphere.vert` / `shaders/sky_atmosphere.frag` — атмосфера
- `shaders/cloudscape.vert` / `shaders/cloudscape.frag` — облака
- `shaders/volumetric_fog.comp` — объёмный туман (compute)
- `shaders/common/common_constants.glsl` — общие константы

## Что это меняет для будущих правок

- новые `.cpp/.hpp` должны жить в своей подсистеме сразу;
- новые include-строки должны использовать qualified path от `src/`;
- если подсистема начинает разрастаться, сначала уточняется её каталог и include boundary, а не только имя файла;
- test target должен следовать тем же include rules, что и production target.

## Связанные документы

- [Documentation Index](README.md) — карта всех руководств
- [Build And Run](BuildAndRun.md)
- [Architecture Guide](ArchitectureGuide.md)
- [Render Architecture](RenderArchitecture.md)
- [VoxelWorld](VoxelWorld.md)
- [Debugging](Debugging.md)
- [Profiling](Profiling.md)
- [Documentation Index](README.md)
- [Voxel MVP Smoke Checklist](voxel_mvp_smoke_checklist.md)
- [TODO](../TODO.md)
