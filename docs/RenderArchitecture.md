# ProjectV Render Architecture

Дата фиксации: `2026-04-07`

Этот документ описывает текущий render path `ProjectV`: как данные переходят из `VoxelWorld` в scene resources, как
compute meshing готовит packed faces и как поверх этого рисуются overlay/HUD.

## Ключевая идея

Текущий render path строится вокруг трёх шагов:

1. CPU-side `VoxelWorld` и dirty chunk queue.
2. CPU-side `SceneResources`, которые пакуют мир в GPU-friendly buffers.
3. Vulkan frame recording с compute meshing + graphics passes.

То есть mainline сейчас не рендерит мир “напрямую” из `VoxelWorld`. Между миром и draw pass есть явный staging слой.

## Scene resources

CPU-side scene preparation живёт в [SceneResources.cpp](../src/render/SceneResources.cpp).

Этот слой держит:

- `PackedSceneChunkDescriptor` для каждого чанка;
- packed voxel payload, где материалы упакованы по четыре в `uint32_t`;
- `PackedSceneVoxelFace` buffer для результатов meshing;
- indirect buffers для opaque и transparent draw;
- dirty chunk index buffer;
- отдельный per-frame vertex buffer для HUD;
- material visual table.

Основной смысл:

- мир остаётся плотным и простым для mutation;
- renderer получает уже подготовленные и компактные буферы;
- dirty world edits не заставляют пересобирать всё дерево абстракций.

## Жизненный цикл dirty chunk

Сейчас dirty chunk path выглядит так:

1. `VoxelWorld` ставит chunk или регион в dirty queue.
2. `PrepareFrameRenderData` вызывает `CollectDirtyVoxelChunkRebuildRequests`.
3. `UpdateSceneResources` repack'ает voxel payload только для dirty chunks.
4. `UploadSceneFrameResources` либо patch'ит latest dirty chunks, либо перезаливает всё, если version gap неинкрементальный.
5. `PrepareDirtyChunkMeshingList` формирует compute-dispatch список чанков.
6. `RecordVoxelMeshingCommands` запускает compute meshing только для этих dirty chunks.
7. После CPU-side scene update `CommitDirtyVoxelChunkRebuildRequests` снимает rebuildQueued с реально завершённых chunks.

Это и есть current “meshing path”.

## Подготовка кадра

Подготовка кадра идёт в [FramePreparation.cpp](../src/app/FramePreparation.cpp).

Она делает:

- сбор dirty rebuild requests из `VoxelWorld`;
- `UpdateSceneResources`;
- `vkWaitForFences` для текущего кадра;
- `UploadSceneFrameResources`;
- сбор HUD vertices;
- заполнение `FrameRenderData`:
  - descriptor sets;
  - buffers;
  - push constants;
  - interaction selection;
  - dirty chunk count;
  - draw counts.

Важно:

- именно здесь происходит handshake между world/update layers и renderer;
- `DrawFrame` не должен сам читать `VoxelWorld`.

## Compute meshing

Compute meshing pipeline создаётся и биндуется в [VulkanVoxelMeshingPipeline.cpp](../src/render/vulkan/VulkanVoxelMeshingPipeline.cpp).

Descriptor set сейчас связывает шесть storage buffers:

1. chunk descriptors
2. chunk voxel payload
3. packed face buffer
4. dirty chunk index buffer
5. opaque indirect buffer
6. transparent indirect buffer

Compute dispatch идёт по количеству dirty chunks, не по всему миру.

После dispatch renderer ставит buffer barriers так, чтобы:

- vertex shader видел packed faces;
- indirect draw path видел обновлённые draw commands.

## Graphics passes

Graphics path записывается в [Renderer.cpp](../src/render/Renderer.cpp).

В текущем mainline кадр включает:

1. transition swapchain image в `COLOR_ATTACHMENT_OPTIMAL`
2. transition depth image
3. `vkCmdBeginRendering`
4. opaque indirect pass
5. transparent indirect pass
6. selection overlay
7. crosshair overlay
8. debug HUD
9. `vkCmdEndRendering`
10. transition в `PRESENT_SRC_KHR`

Dynamic rendering используется вместо старого render pass / framebuffer graph.

## Opaque и transparent path

Оба pass используют:

- один и тот же descriptor-backed scene data model;
- один и тот же push-constant view/projection/camera block;
- отдельные indirect buffers;
- общий packed face storage.

Scene triangle count сейчас считается из generated face counts после upload/meshing round-trip, а не оценивается “на
глаз”.

## Overlay и HUD

Selection highlight и crosshair рисуются отдельным debug overlay pipeline.

Особенности:

- overlay не вмешивается в voxel material/render path;
- selection box генерируется из `gl_VertexIndex`;
- crosshair тоже рисуется отдельным overlay draw call.

HUD живёт отдельным pipeline и отдельным CPU-built vertex buffer:

- текст собирается на CPU;
- vertex buffer host-mapped;
- draw идёт после основных voxel passes.

Это сознательно проще и дешевле, чем тащить `imgui` в current MVP.

## Swapchain recreate и zero-extent lifecycle

`DrawFrame` и swapchain path сейчас рассчитаны на normal window lifecycle:

- resize;
- minimize;
- restore;
- maximize;
- graceful shutdown.

Transient `0x0` surface extent не должен уничтожать pipeline/depth resources раньше времени. Recreate откладывается до
валидного extent, поэтому restore path не ломает render state.

## Profiling

Render path уже размечен Tracy-зонами:

- CPU зоны через `PV_PROFILE_ZONE_N(...)`;
- GPU зоны через `PV_PROFILE_GPU_ZONE(...)`.

Готовые графики конфигурируются в [Profiling.hpp](../src/debug/Profiling.hpp):

- `Scene Triangles`
- `Generated Opaque Faces`
- `Generated Transparent Faces`
- `Meshing Dirty Chunks`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`

Это текущий baseline telemetry path до отдельного benchmark/profiling sprint.

## Что renderer пока не делает

Пока в current mainline ещё нет:

- frustum culling;
- distance culling;
- greedy meshing;
- advanced transparent sorting;
- scene streaming;
- render graph.

Это осознанное ограничение: сначала честный renderable voxel MVP, потом более тяжёлые optimisation/polish slices.

## Связанные документы

- [ArchitectureGuide](ArchitectureGuide.md)
- [VoxelWorld](VoxelWorld.md)
- [Debugging](Debugging.md)
