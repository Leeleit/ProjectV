# ProjectV Render Architecture

> [!WARNING]
> **Исторический документ.** Данный файл содержит устаревшие разделы (такие как описание каскадных теней CSM, сглаживания TAA и конусной трассировки VCT), которые были полностью удалены или перемещены в архив (`legacy/aa/`) в ходе пост-ресет рефакторинга в июне-июле 2026 года.
> Актуальное описание современной архитектуры рендеринга приведено в документе [RTX Renderer Architecture](RTX_Renderer_Architecture.md).

Дата фиксации: `2026-04-22` (Обновлено: `2026-07-11`)

Этот документ описывает исторический и базовый render path `ProjectV`: как данные переходят из `VoxelWorld` в scene resources, как
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

CPU-side scene preparation живёт в семействе файлов `SceneResources.cpp` (~810 строк) плюс `SceneResourcesUpdate.cpp` (заливка CPU->GPU), `SceneResourcesVisibility.cpp` (CPU куллинг), `SceneResourcesDestroy.cpp` (deferred NanoVDB очистка), `SceneResourcesUtilities.cpp` и `SceneResourcesInternal.hpp`.

Этот слой держит:

- `PackedSceneChunkDescriptor` для каждого чанка;
- packed voxel payload, где материалы упакованы по четыре в `uint32_t`;
- `PackedSceneVoxelFace` buffer для результатов meshing; each face now also carries a cheap local ambient-visibility
  byte in addition to voxel/material identity;
- indirect buffers for opaque, shadow, and transparent draw;
- dirty chunk index buffer;
- отдельный per-frame vertex buffer для HUD;
- optional swapchain-sized screenshot readback buffer for live look-dev capture when the current surface supports
  `TRANSFER_SRC`;
- material visual table;
- scene lighting buffer для sky/horizon/ground/sun/fog look текущего `VoxelScenePreset` плюс
  exposure/environment/tone-map/debug-view, minimal color-grading, and CPU-side scene-key exposure metering contract;
- first CSM sun-shadow contract: per-preset shadow tuning, explicit 4-cascade split state, and
  `sunShadowViewProjections[4]` in `VoxelSceneLighting`. CPU builds per-cascade light projections from camera view
  slices, active scene bounds, and the authored sun direction; the shadow pass renders a 4-layer depth array and the
  final shader samples it with `sampler2DArrayShadow`. Cascade projection centers are snapped to the shadow texel grid
  using the active shadow-map resolution to avoid continuous sub-texel projection drift during small camera movement.
  Split planning now also follows the same visible-scene receiver contract as main-pass chunk visibility instead of the
  raw camera far plane: current mainline uses camera near plus `min(farPlane, 64)` as the receiver depth horizon. The
  current default split lambda is `0.80`, not the older `0.65`, because live MeshingStress repro showed the first
  baseline was still too generous to far receivers.
  The same CPU fit now also emits per-cascade coverage diagnostics: view-depth ranges, ortho extents, and effective
  world-space texel size flow into runtime debug state and screenshot sidecars. The current `XY` receiver fit also uses
  a rotation-stable sphere extent per slice instead of a tight light-space AABB, so camera yaw does not churn cascade
  width/height and texel density every frame. Split edges no longer hard-switch either: `voxel.frag` now blends current
  and next cascades over a small runtime-visible band, and that blend width is part of the same tuning/HUD/capture
  contract rather than a hidden shader constant. Caster-depth coverage is also per-cascade now: the CPU fit extrudes the
  current receiver slice upstream along the sun direction before intersecting with active scene bounds, instead of
  feeding every cascade the full active-scene AABB. That caster coverage now expands the cascade's projected `XY`
  footprint too, not only its light-depth range, so nearer cascades do not clip tall/upstream casters at split
  transitions. The cascade light camera also moves upstream far enough to keep that expanded caster range in front of
  the
  shadow near plane; otherwise mid/far cascades can still lose casters even with correct `XY` coverage. Shadow draw
  submission is now per-cascade too: the indirect shadow buffer stores one chunk-command slice per cascade, CPU chunk
  visibility rebuilds those slices against the current cascade clip volumes, and dirty-chunk meshing patches the same
  per-cascade commands on the GPU. Empty cascades can then skip the shadow draw call entirely when the frame has no
  dirty meshing work and CPU culling already knows the cascade is empty.
  A first contact-shadow baseline now also lives in the same forward path instead of another pass: the graphics shader
  binds chunk descriptors plus the packed chunk voxel payload, addresses the voxel world through
  `GraphicsPushConstants.worldMinAndChunkSize/chunkGridAndFlags`, and traces a short voxel DDA ray toward the sun using
  `sunContactShadowParams={strength,maxDistance}`. `CTSH` is the dedicated debug view for that local layer.
  The first ambient/contact-occlusion baseline is similarly bounded: `ambientOcclusionParams` drives a short
  hemisphere voxel DDA in `voxel.frag`, and `AOCC` visualizes that local visibility term. This is not a full
  screen-space `SSAO/GTAO` pass.
  The first local-light contract is authored in the same scene lighting buffer before adding real local shadow maps:
  `localPointLightPositionAndRadius`, `localPointLightColorAndIntensity`, and
  `localPointLightParams={enabled,sourceRadius,shadowStrength,shadowBias}` describe one per-preset inverse-square point
  light. The forward shader evaluates it through the same GGX direct-light helper as the sun and then applies a short
  opaque-only voxel DDA visibility term to get a bounded local-shadow baseline without a separate cubemap/shadow-map
  resource yet. That visibility trace is anchored to a stabilized point on the owning voxel face rather than the raw
  interpolated fragment boundary position, so fully blocked faces do not turn into per-face moire/fractal patterns
  while large flat receivers also avoid obvious per-voxel visibility bucketing. Partially occluded faces also use a
  tiny emitter-disk average around the authored `sourceRadius` instead of one hard ray to the emitter center, which
  keeps close-up local-light response from degenerating into binary speckle. `LOCL` is the dedicated debug view for this
  contribution; `Glass` and `Fluid` are both ignored as local-light occluders in the current policy.
  `sunDirectionAndWrap.xyz` на этом уровне остаётся authored-вектором к солнцу для shading, а shadow fit инвертирует его
  в реальное направление хода света перед сборкой shadow projections.

Основной смысл:

- мир остаётся плотным и простым для mutation;
- renderer получает уже подготовленные и компактные буферы;
- dirty world edits не заставляют пересобирать всё дерево абстракций.
- lighting/material response и первый post-lighting contract теперь тоже описываются на CPU и уходят в scene resources
  как
  явные буферы, а не как shader hardcode.
- first sun shadow follows the same rule: shadow projection и baseline tuning описываются на CPU, а не собираются как
  скрытое shader-only состояние.
- CSM follows the same rule too: cascade count, lambda, split depths, image-array storage, and shader debug view are
  explicit runtime state instead of hidden shader constants. Stabilization starts in the CPU projection contract via
  texel-grid snapping, coverage diagnostics are part of that contract too rather than private CPU math, and split
  blending is exposed the same way through the runtime `BLD` control and capture metadata. Caster coverage diagnostics
  (`shadow_cascade_caster_light_ranges`) are part of that same explicit contract, and split planning must stay aligned
  with the visible-scene receiver horizon rather than silently drifting back to raw far-plane math.
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

После dispatch renderer ставит buffer barriers так, чтобы:

- vertex shader видел packed faces;
- indirect draw path видел обновлённые draw commands.

## Graphics passes

Shadow-path update `2026-04-22`:

- The shadow pass now uses a dedicated descriptor/pipeline layout instead of binding the main graphics descriptor set
  while the shadow image is being written as depth.
- The current stable shadow baseline uses a dedicated all-occluder `shadowIndirectBuffer`, so shadow draws follow the
  renderer's sparse per-chunk opaque face layout without inheriting camera-frustum culling from the main opaque pass.
- Transparent-shadow policy is `GLASS_IGNORED_FLUID_CASTS`: glass does not cast sun shadows until a separate
  tinted/transmission or RT-oriented path exists, while `Fluid` casts through the current opaque shadow-map path.
- `Fluid` may still live in the main opaque draw range for forward rendering, so the shadow fragment shader must only
  reject `Glass`; rejecting `Fluid` makes water incorrectly shadowless.

Graphics path записывается в семействе файлов `Renderer.cpp` (~90 строк): `RendererDrawFrame.cpp` (синхронизация кадра, HZB, презентация), `RendererRecordCommands.cpp` (запись команд отрисовки), `RendererOverlay.cpp` и `RendererScreenshot.cpp`.

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
- glass geometry is not part of the current sun-shadow caster set; `Fluid` remains a caster in the current shadow path;
- main voxel shader семплирует эту карту только для direct sun, не для local lights или более сложного GI;
- current quality baseline для этого path — `2048x2048` depth map плюс weighted shader-side `5x5` PCF, а не single hard
  compare sample, и angle-aware receiver biasing from the authored depth/normal bias controls плюс a small
  sun-direction receiver offset instead of one flat brute-force offset for every sun angle
- shadow-map writes use static Vulkan polygon depth bias in the shadow graphics pipeline. This is the caster-side fix
  for one-sided voxel-face self-shadow acne. PCF only filters the result and should not be treated as the root solution.
- render-facing material response in the same pass is now PBR-friendlier than the old ad-hoc
  ambient/diffuse/spec/shininess knobs: the CPU material table packs `base color`, `AO`, `roughness`, `metallic`,
  `reflectance`, transmission tint, and fog/emissive/ambient/direct-response hooks.
  `voxel.frag` evaluates direct sun with a `GGX + Fresnel-Schlick + Smith` baseline on top of the existing ambient
  gradient and shadow visibility term
  without dropping authored shadow contrast.
- local point lights currently reuse that same direct-light BRDF, use inverse-square attenuation with authored
  radius/source-radius clamps, and apply a bounded opaque-only voxel DDA visibility term in the forward shader. They
  still do not use dedicated local shadow-map resources; adding spot shadow maps or point-light cubemaps is the next
  separate local-light quality step.
- environment fill is no longer only a normal-based sky gradient either: compute meshing writes a cheap per-face local
  ambient-visibility term into `PackedSceneVoxelFace`, `voxel.vert` forwards it flat, and `voxel.frag` multiplies
  sky/horizon/ground fill by it so sealed voxel cavities stop reading as if they still saw full sky. This is a bounded
  voxel-neighborhood visibility term, not `SSAO/GTAO`.
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
