# ProjectV Render Architecture

> [!WARNING]
> **Исторический документ.** Данный файл содержит устаревшие разделы (такие как описание каскадных теней CSM и
> сглаживания TAA), которые были полностью удалены или перемещены в архив (`legacy/aa/`) в ходе пост-ресет рефакторинга
> в
> июне-июле 2026 года.
> Актуальное описание современной архитектуры рендеринга приведено в
> документе [RTX Renderer Architecture](RTX_Renderer_Architecture.md).

Дата фиксации: `2026-04-07` (Обновлено: `2026-07-12`)

Этот документ описывает базовый render path `ProjectV`: как данные переходят из `VoxelWorld` в scene resources, как
compute meshing готовит packed faces и как поверх этого рисуются overlay/HUD.

## Ключевая идея

Текущий render path строится вокруг трёх шагов:

1. CPU-side `VoxelWorld` и dirty chunk queue.
2. CPU-side `SceneResources`, которые пакуют мир в GPU-friendly buffers.
3. Vulkan frame recording с compute meshing + graphics passes.

То есть mainline не рендерит мир “напрямую” из `VoxelWorld`. Между миром и draw pass есть явный staging слой.

## Scene resources

CPU-side scene preparation живёт в семействе файлов `SceneResources.cpp` плюс `SceneResourcesUpdate.cpp`,
`SceneResourcesVisibility.cpp`, `SceneResourcesDestroy.cpp`, `SceneResourcesUtilities.cpp` и
`SceneResourcesInternal.hpp`.

Этот слой держит:

- `PackedSceneChunkDescriptor` для каждого чанка;
- packed voxel payload, где материалы упакованы по четыре в `uint32_t`;
- `PackedSceneVoxelFace` buffer для результатов meshing; each face also carries a cheap local ambient-visibility byte;
- indirect buffers for opaque, shadow, and transparent draw;
- dirty chunk index buffer;
- отдельный per-frame vertex buffer для HUD;
- optional swapchain-sized screenshot readback buffer;
- material visual table;
- scene lighting buffer для sky/horizon/ground/sun/fog look текущего `VoxelScenePreset` плюс
  exposure/tone-map/debug-view state.

Основной смысл:

- мир остаётся плотным и простым для mutation;
- renderer получает уже подготовленные и компактные буферы;
- dirty world edits не заставляют пересобирать всё дерево абстракций;
- lighting/material response описываются на CPU и уходят в scene resources как явные буферы, а не как shader hardcode.

## Жизненный цикл dirty chunk

Сейчас dirty chunk path выглядит так:

1. `VoxelWorld` ставит chunk или регион в dirty queue.
2. `PrepareFrameRenderData` вызывает `CollectDirtyVoxelChunkRebuildRequests`.
3. `UpdateSceneResources` repack'ает voxel payload только для dirty chunks.
4. `UploadSceneFrameResources` либо patch'ит latest dirty chunks, либо перезаливает всё, если version gap
   неинкрементальный.
5. `UpdateSceneFrameChunkVisibility` каждый кадр пересобирает indirect draw commands под текущие frustum/distance
   условия.
6. `PrepareDirtyChunkMeshingList` формирует compute-dispatch список dirty чанков.
7. `RecordVoxelMeshingCommands` запускает compute meshing только для этих dirty chunks и поверх свежих face counts
   перезаписывает их indirect-команды.
8. После CPU-side scene update `CommitDirtyVoxelChunkRebuildRequests` снимает rebuildQueued с реально завершённых
   chunks.

Это и есть current “meshing path”.

## Подготовка кадра

Подготовка кадра идёт в [FramePreparation.cpp](../src/app/FramePreparation.cpp).

Она делает:

- сбор dirty rebuild requests из `VoxelWorld`;
- `UpdateSceneResources`;
- `vkWaitForFences` для текущего кадра;
- `UploadSceneFrameResources`;
- `UpdateSceneFrameChunkVisibility`;
- сбор HUD vertices;
- заполнение `FrameRenderData` (descriptor sets, buffers, push constants, interaction selection, dirty chunk count, draw
  counts).

Важно: именно здесь происходит handshake между world/update layers и renderer; `DrawFrame` не должен сам читать
`VoxelWorld`.

## Compute meshing

Compute meshing pipeline создаётся и биндуется
в [VulkanVoxelMeshingPipeline.cpp](../src/render/vulkan/VulkanVoxelMeshingPipeline.cpp).

Descriptor set сейчас связывает восемь storage buffers:

1. chunk descriptors
2. chunk voxel payload
3. packed face buffer
4. dirty chunk index buffer
5. opaque indirect buffer
6. transparent indirect buffer
7. shadow indirect buffer
8. chunk culling parameters buffer

Compute dispatch идёт по количеству dirty chunks, не по всему миру.

После dispatch renderer ставит buffer barriers так, чтобы vertex shader видел packed faces, а indirect draw path видел
обновлённые draw commands.

## Graphics passes

Graphics path записывается в семействе файлов `Renderer.cpp`: `RendererDrawFrame.cpp`, `RendererRecordCommands.cpp`,
`RendererOverlay.cpp` и `RendererScreenshot.cpp`.

В текущем mainline кадр включает:

1. compute meshing для dirty chunks;
2. RTX shadow pass (через `RayTracedShadows` family);
3. transition swapchain image / scene color target;
4. `vkCmdBeginRendering`;
5. opaque indirect pass;
6. model pass;
7. transparent indirect pass;
8. debug overlay boxes;
9. crosshair overlay;
10. debug HUD;
11. `vkCmdEndRendering`;
12. blit из `sceneColorTarget` в swapchain image;
13. transition в `PRESENT_SRC_KHR`.

Dynamic rendering используется вместо старого render pass / framebuffer graph.

Современное освещение и трассировка лучей описаны в [RTX Renderer Architecture](RTX_Renderer_Architecture.md).

## Opaque и transparent path

Оба pass используют:

- один и тот же descriptor-backed scene data model;
- один и тот же push-constant view/projection/camera block;
- отдельные indirect buffers;
- общий packed face storage.

Scene triangle count считается из generated face counts после upload/meshing round-trip.

## Overlay и HUD

Selection highlight, inspect chunk bounds, optional global chunk bounds и dirty-chunk overlay рисуются через один и тот
же debug overlay pipeline.

Особенности:

- overlay не вмешивается в voxel material/render path;
- line-box геометрия генерируется из `gl_VertexIndex`, а CPU only подготавливает список world-space box bounds/colors;
- crosshair тоже рисуется отдельным overlay draw call.

HUD живёт отдельным pipeline и отдельным CPU-built vertex buffer:

- текст собирается на CPU;
- vertex buffer host-mapped;
- panel bounds считаются от реально измеренных HUD-строк;
- draw идёт после основных voxel passes.

## Swapchain recreate и zero-extent lifecycle

`DrawFrame` и swapchain path сейчас рассчитаны на normal window lifecycle:

- resize;
- minimize/restore;
- maximize;
- graceful shutdown.

Transient `0x0` surface extent не должен уничтожать pipeline/depth resources раньше времени.

## Profiling

Render path размечен Tracy-зонами:

- CPU зоны через `PV_PROFILE_ZONE_N(...)`;
- GPU зоны через `PV_PROFILE_GPU_ZONE(...)`.

Готовые графики конфигурируются в [Profiling.hpp](../src/debug/Profiling.hpp).

## Что renderer пока не делает

Пока в current mainline ещё нет:

- render graph;
- advanced transparent sorting;
- scene streaming.

## Связанные документы

- [RTX Renderer Architecture](RTX_Renderer_Architecture.md) — актуальная RTX-only архитектура
- [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md) — полный разбор файлов и алгоритмов
- [ArchitectureGuide](ArchitectureGuide.md) — общая архитектура движка
- [VoxelWorld (Historical)](VoxelWorld.md)
- [Debugging](Debugging.md)
