# ProjectV Render Architecture

Дата фиксации: `2026-04-22`

Этот документ описывает текущий render path `ProjectV`: как данные переходят из `VoxelWorld` в scene resources, как
compute meshing готовит packed faces и как поверх этого рисуются overlay/HUD.

## Ключевая идея

Текущий render path строится вокруг трёх шагов:

1. CPU-side `VoxelWorld` и dirty chunk queue.
2. CPU-side `SceneResources`, которые пакуют мир в GPU-friendly buffers.
3. Vulkan frame recording с compute meshing + graphics passes.

То есть mainline сейчас не рендерит мир “напрямую” из `VoxelWorld`. Между миром и draw pass есть явный staging слой.

## Scene resources

Shadow-path update `2026-04-22`:

- Packed voxel faces are not a dense global prefix. Each chunk owns reserved opaque/transparent ranges, and any draw
  path that consumes packed faces must respect `firstInstance`/indirect addressing.

CPU-side scene preparation живёт в [SceneResources.cpp](../src/render/SceneResources.cpp).

Этот слой держит:

- `PackedSceneChunkDescriptor` для каждого чанка;
- packed voxel payload, где материалы упакованы по четыре в `uint32_t`;
- `PackedSceneVoxelFace` buffer для результатов meshing;
- indirect buffers for opaque, shadow, and transparent draw;
- dirty chunk index buffer;
- отдельный per-frame vertex buffer для HUD;
- optional swapchain-sized screenshot readback buffer for live look-dev capture when the current surface supports
  `TRANSFER_SRC`;
- material visual table;
- scene lighting buffer для sky/horizon/ground/sun/fog look текущего `VoxelScenePreset` плюс
  exposure/tone-map/debug-view post-process contract;
- первый sun-shadow contract: per-preset shadow tuning и scene-wide `sunShadowViewProjection`, который CPU собирает из
  bounds активных chunk-ов и только fallback'ает на полные границы `VoxelWorld`, если сцена пуста.
  `sunDirectionAndWrap.xyz` на этом уровне остаётся authored-вектором к солнцу для shading, а shadow fit инвертирует его
  в реальное направление хода света перед сборкой shadow camera.

Основной смысл:

- мир остаётся плотным и простым для mutation;
- renderer получает уже подготовленные и компактные буферы;
- dirty world edits не заставляют пересобирать всё дерево абстракций.
- lighting/material response и первый post-lighting contract теперь тоже описываются на CPU и уходят в scene resources
  как
  явные буферы, а не как shader hardcode.
- first sun shadow follows the same rule: shadow projection и baseline tuning описываются на CPU, а не собираются как
  скрытое shader-only состояние.
- CPU descriptor versioning не должен стирать GPU-сгенерированные face counts: динамические `drawRanges` считаются
  runtime mesh state, а не authored scene layout.

## Жизненный цикл dirty chunk

Сейчас dirty chunk path выглядит так:

1. `VoxelWorld` ставит chunk или регион в dirty queue.
2. `PrepareFrameRenderData` вызывает `CollectDirtyVoxelChunkRebuildRequests`.
3. `UpdateSceneResources` repack'ает voxel payload только для dirty chunks.
4. `UploadSceneFrameResources` либо patch'ит latest dirty chunks, либо перезаливает всё, если version gap
   неинкрементальный. Upload path сохраняет уже сгенерированные GPU-side `drawRanges` counts и меняет на CPU только
   descriptor layout/static fields плюс `nonAir` summary.
5. `UpdateSceneFrameChunkVisibility` каждый кадр пересобирает indirect draw commands под текущие frustum/distance
   условия и кладёт chunk-culling параметры в отдельный buffer. Frustum visibility при этом использует консервативную
   проверку chunk AABB против near/left/right/top/bottom planes, а не только центр+bounding-sphere приближение.
6. `PrepareDirtyChunkMeshingList` формирует compute-dispatch список dirty чанков.
7. `RecordVoxelMeshingCommands` запускает compute meshing только для этих dirty chunks и поверх свежих face counts
   перезаписывает их indirect-команды: camera-culled opaque/transparent visibility plus the dedicated all-occluder
   shadow commands.
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
- current scene clear color тоже берётся из того же scene-lighting contract, а не из отдельной hardcoded константы.

## Compute meshing

Compute meshing pipeline создаётся и биндуется в [VulkanVoxelMeshingPipeline.cpp](../src/render/vulkan/VulkanVoxelMeshingPipeline.cpp).

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

После dispatch renderer ставит buffer barriers так, чтобы:

- vertex shader видел packed faces;
- indirect draw path видел обновлённые draw commands.

## Graphics passes

Shadow-path update `2026-04-22`:

- The shadow pass now uses a dedicated descriptor/pipeline layout instead of binding the main graphics descriptor set
  while the shadow image is being written as depth.
- The current stable shadow baseline uses a dedicated all-occluder `shadowIndirectBuffer`, so shadow draws follow the
  renderer's sparse per-chunk face layout without inheriting camera-frustum culling from the main opaque pass.
- The remaining limitation is now explicit: the current shadow path is opaque-only, so transparent-heavy scenes can
  still look largely shadowless.

Graphics path записывается в [Renderer.cpp](../src/render/Renderer.cpp).

В текущем mainline кадр включает:

1. compute meshing для dirty chunks;
2. отдельный shadow depth pass в scene-wide orthographic sun map;
3. transition swapchain image в `COLOR_ATTACHMENT_OPTIMAL`;
4. transition main depth image;
5. `vkCmdBeginRendering`;
6. opaque indirect pass;
7. transparent indirect pass;
8. debug overlay boxes;
9. crosshair overlay;
10. debug HUD;
11. `vkCmdEndRendering`;
12. optional swapchain copy-to-buffer for `C` screenshot capture;
13. transition в `PRESENT_SRC_KHR`.

Dynamic rendering используется вместо старого render pass / framebuffer graph.

Важно:

- shadow depth pass не использует camera-culling indirect buffers как source of truth;
- он читает dedicated all-occluder `shadowIndirectBuffer`, чтобы тени от opaque geometry не зависели от текущего view
  frustum;
- main voxel shader семплирует эту карту только для direct sun, не для local lights или более сложного GI;
- current quality baseline для этого path — `2048x2048` depth map плюс лёгкий shader-side `3x3` PCF, а не single hard
  compare sample, и angle-aware receiver biasing from the authored depth/normal bias controls instead of one flat
  brute-force offset for every sun angle.
- render-facing material response in the same pass is now PBR-friendlier than the old ad-hoc
  ambient/diffuse/spec/shininess knobs: the CPU material table packs `base color`, `AO`, `roughness`, `metallic`,
  `reflectance`, transmission tint and fog/emissive/ambient/direct-response hooks, while `voxel.frag` evaluates direct
  sun with a `GGX + Fresnel-Schlick + Smith` baseline on top of the existing ambient gradient and shadow visibility term
  without dropping authored shadow contrast.
- when the surface supports swapchain `TRANSFER_SRC`, the same frame loop can also copy the final color image into a
  host-visible readback buffer and save a `.bmp` plus sidecar metadata for reproducible look-dev capture.

## Opaque и transparent path

Оба pass используют:

- один и тот же descriptor-backed scene data model;
- один и тот же push-constant view/projection/camera block;
- отдельные indirect buffers;
- общий packed face storage.

Scene triangle count сейчас считается из generated face counts после upload/meshing round-trip, а не оценивается “на
глаз”.

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
- panel bounds считаются от реально измеренных HUD-строк, а не от fixed width констант;
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
- `Visible Chunks`
- `Culled Chunks`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`

Это текущий baseline telemetry path до отдельного benchmark/profiling sprint.

## Что renderer пока не делает

Пока в current mainline ещё нет:

- greedy meshing;
- advanced transparent sorting;
- scene streaming;
- render graph.

Это осознанное ограничение: сначала честный renderable voxel MVP, потом более тяжёлые optimisation/polish slices.

## Связанные документы

- [ArchitectureGuide](ArchitectureGuide.md)
- [VoxelWorld](VoxelWorld.md)
- [Debugging](Debugging.md)
