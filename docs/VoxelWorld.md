# ProjectV VoxelWorld

Дата фиксации: `2026-04-22`

Этот документ описывает current `VoxelWorld` в `ProjectV`: его структуру данных, dirty queue, interaction path и связь с
render/physics слоями.

## Роль `VoxelWorld`

`VoxelWorld` — это current source of truth для мира.

Он отвечает за:

- границы мира;
- плотное хранение материалов;
- chunk decomposition;
- dirty rebuild bookkeeping;
- статистику мира;
- runtime edits.

ECS, renderer и physics читают этот state, но ownership мира остаётся здесь.

## Материалы

Сейчас мир использует `VoxelMaterial` из [VoxelWorld.hpp](../src/voxel/VoxelWorld.hpp):

- `Air`
- `Glass`
- `Fluid`
- `FloorWhite`
- `FloorGray`

Для current physics slice solid-материалами считаются:

- `Glass`
- `FloorWhite`
- `FloorGray`

`Air` и `Fluid` коллизии не дают.

Render-facing material response теперь живёт в [VoxelMaterials.cpp](../src/voxel/VoxelMaterials.cpp): `base color`,
`AO`, `roughness`, `metallic`, `reflectance`, transmission и fog/emissive/ambient/direct-response hooks, scene-lighting
presets и первый
exposure/tone-mapping/color-grading/scene-key metering contract описываются на CPU, а не как жёстко зашитые константы в
shader.
Local cavity ambient visibility now also rides on the same voxel render path instead of pretending that an upward
normal automatically sees the whole sky: compute meshing bakes a cheap per-face visibility byte into
`PackedSceneVoxelFace`, `voxel.vert` forwards it flat, and `voxel.frag` multiplies sky/horizon/ground fill by it.
Current blocker policy for that term is `Air/Open`, `Glass/Open`, `Fluid/Occluder`, `Opaque/Occluder`; this is a
bounded voxel-neighborhood visibility term, not `SSAO/GTAO`.
Там же теперь живёт и первый CSM sun-shadow baseline: per-preset shadow tuning, 4-cascade split state, а
`SceneResources` дополняет его `sunShadowViewProjections[4]`, собранными из camera view slices, active scene bounds и
направления солнца. Projection centers snap to the shadow texel grid, so sub-texel camera movement does not continuously
slide cascades across world-space receivers. The same path now also records per-cascade view ranges, ortho extents, and
effective world-space texel size for debug HUD / capture metadata. Cascade `XY` fit now uses a stable sphere extent per
slice too, so camera yaw no longer changes cascade width/height and texel density just because the frustum rotated.
`voxel.frag` now also blends current/next cascades across a small runtime-visible split band instead of hard-switching
right at the split edge. Caster-depth coverage is no longer full-scene for every cascade either: each cascade extrudes
its current receiver slice upstream along the sun direction before intersecting with active scene bounds.
Split planning itself now follows the same visible-scene receiver horizon as current mainline chunk visibility
(`min(camera.farPlane, 64)`), so tower-top receivers do not get pushed into lower-density cascades only because the raw
far plane is larger than the part of the scene we actually draw. The current default split lambda is `0.80`, so this
first mainline CSM baseline is deliberately near-biased rather than keeping the original softer `0.65` distribution.
The caster-coverage follow-up now expands cascade `XY` extents too, not just caster light-depth, so a nearer cascade
does not simply lose a tall tower's shadow because the tower projects outside the receiver-only footprint.
The cascade light camera now also moves upstream enough to keep that expanded caster range in front of the shadow near
plane, so mid/far cascades do not silently clip the tower before the map is even sampled. Shadow draw submission is now
per cascade too: the shadow indirect buffer stores one chunk-command slice per cascade, CPU chunk visibility rebuilds
those slices against the current cascade clip
volumes, and dirty-chunk meshing patches the same per-cascade commands for current-frame correctness instead of drawing
every opaque chunk into every cascade. When a frame has no dirty meshing work and CPU culling already knows a cascade is
empty, the renderer also skips the empty shadow draw call for that cascade.
Current `voxel.frag` больше не держит direct sun на ad-hoc `spec power + shininess`: direct-light BRDF теперь базово
следует `GGX + Fresnel-Schlick + Smith`, но остаётся встроенным в тот же forward voxel path без отдельного PBR framework
или IBL stack.

## Границы и координаты

Мир хранит:

- `min`
- `maxExclusive`
- `width / height / depth`

Все voxel координаты сейчас integer-based и описываются `Int3`.

Вне границ:

- `GetVoxelMaterial` возвращает `Air`;
- запись через `SetVoxelMaterial` просто игнорируется.

Это важно для interaction и для ограничений editable area вокруг procedural demo-scene.

## Хранение данных

Voxel данные лежат плотно в `std::vector<uint8_t> voxels`.

Особенности:

- хранится именно material id;
- индекс считается из локальных координат мира;
- структура intentionally dense и простая, без sparse-структур и без SVO.

Это соответствует current mainline: быстрый и понятный mutation path важнее “идеального будущего представления”.

## Chunk model

Каждый chunk хранит:

- `min`
- `maxExclusive`
- `rebuildQueued`
- `nonAirVoxelCount`

Сам `VoxelWorld` держит:

- `chunkSize`
- `chunkCountX / Y / Z`
- `chunks`
- `pendingChunkRebuildIndices`
- aggregate `stats`
- `editVersion`

Chunk bookkeeping сейчас нужен не ради gameplay, а ради render/meshing path.

## Builtin scene presets

`VoxelWorld.cpp` теперь держит небольшой builtin preset layer для reproducible runs.

Текущий default preset:

- `VoxelLab`

Дополнительные baseline presets:

- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

Они выбираются через `PROJECTV_SCENE_PRESET` и сейчас нужны в первую очередь для profiling/benchmark work, а не как полноценная save/load
система.

Теперь preset задаёт не только геометрию, но и reproducible visual look: `SceneResources` загружает matching
lighting/fog/sun
параметры и baseline post-process для `voxel.frag`, поэтому `F5` циклично меняет и scene layout, и освещение.
Первый sun-shadow path следует тому же принципу: при смене preset сохраняется reproducible baseline для силы и bias
теней, а shadow projection каждый кадр пересчитывается от актуальных bounds активной сцены, а не от камеры; если сцена
пуста, path fallback'ается на полные world bounds. Authored sun vector при этом по-прежнему указывает к солнцу для
shading, а shadow fit инвертирует его во внутреннее light-travel direction, чтобы direct light и shadow placement
читались от одного и того же preset.

Поверх preset-baseline current live look-dev ladder остаётся keyboard-driven и не требует отдельного editor path:

- `B` циклично переключает lighting debug views;
- `N` циклично переключает tone-map operator;
- `H/K` двигают exposure вниз/вверх;
- `V` сбрасывает lighting debug controls к baseline preset;
- `C` сохраняет текущий кадр в `.bmp` плюс sidecar metadata-файл с preset/exposure/metering/grading/shadow state,
  including the active CSM split plan, per-cascade coverage diagnostics, and transparent-shadow policy.

По умолчанию такие look-dev captures пишутся в `ProjectVScreenshots` рядом с executable, а `PROJECTV_SCREENSHOT_DIR`
может переопределить директорию вывода.

После первого sun-shadow quality follow-up `B` теперь включает и dedicated `Shadow` view, а detailed HUD показывает
current
shadow resolution / strength / filter radius / bias, так что базовый shadow look-dev остаётся reproducible внутри
runtime.
Detailed HUD also shows the current 4-cascade split plan (`CSM ...`) plus per-cascade view range / extent / texel-size
diagnostics, while `COV ... BLD ... TUNE ...` shows the current coverage scale and split-blend width. Per-cascade lines
now also include `CD` caster light-depth ranges. `B` cycles through a dedicated `CSM` debug view that visualizes
cascade selection and the transition band near split edges.
Detailed HUD also reports `TSHD GLASS_IGNORED_FLUID_CASTS`: glass is not a sun-shadow caster in the current mainline
renderer, while `Fluid` casts through the current opaque shadow-map path.

`VoxelLab` по-прежнему создаёт текущую основную demo-scene:

- шахматный пол;
- непрозрачный right-side stepped anchor для читаемых sun shadows;
- стеклянную сферу, которая не кастит sun shadow в текущей policy;
- жидкость внутри, которая кастит sun shadow через текущий opaque shadow-map path;
- padding вокруг сцены;
- initial dirty state для всех chunks.

Общая world-конфигурация теперь отделена от `VoxelLab`-специфичной геометрии:

- `VoxelWorldConfig` держит только общие параметры мира: пол, границы по `Y`, padding и chunk size;
- стеклянный купол и жидкость `VoxelLab` собираются из отдельного preset-specific shell config внутри builder path, а не через общий конфиг всех
  сцен.

Это сделано намеренно: `FlatBenchmark`, `TransparencyStress`, `ChunkGrid` и `MeshingStress` больше не таскают по коду фиктивные поля вроде
`sphereRadius = 0` только ради совместимости с одним `VoxelLab`.

## World snapshots

Поверх builtin presets `VoxelWorld` теперь поддерживает file-backed snapshot path.

Snapshot сохраняет:

- `scenePreset`;
- `VoxelWorldConfig`;
- world bounds;
- voxel payload;
- `editVersion`.

Snapshot намеренно **не** сохраняет:

- camera state;
- control mode;
- runtime physics internals;
- GPU-side scene resources.

Практический contract:

- `F6` пишет snapshot по пути из `PROJECTV_SNAPSHOT_PATH` или, если env var не задан, в `ProjectV.snapshot.bin` рядом с
  executable;
- `F7` читает тот же файл;
- после load весь мир считается dirty заново, чтобы render/meshing/ECS/physics синхронизировались от fresh CPU truth.

## Dirty queue

Dirty queue сейчас строится очень просто:

- chunk помечается через `rebuildQueued`;
- его индекс попадает в `pendingChunkRebuildIndices`;
- `stats.dirtyChunkCount` отражает текущее состояние queue.

Основные операции:

- `MarkVoxelChunkDirty`
- `MarkVoxelRegionDirty`
- `MarkAllVoxelChunksDirty`
- `CollectDirtyVoxelChunkRebuildRequests`
- `CommitDirtyVoxelChunkRebuildRequests`

Renderer не должен сам вычислять dirty state “по памяти”. Он получает это явно из `VoxelWorld`.

## Runtime edits

Главная mutation-функция — `SetVoxelMaterial`.

Она делает сразу несколько вещей:

1. проверяет границы;
2. проверяет, изменился ли материал реально;
3. пишет новый material id;
4. увеличивает `editVersion`;
5. обновляет aggregate material stats;
6. обновляет `nonAirVoxelCount` конкретного chunk;
7. обновляет `activeChunkCount`;
8. помечает dirty region.

Текущая стратегия dirty region после edit:

- всегда помечается chunk самого voxel;
- дополнительно помечаются только те соседние chunks, чью общую грань/ребро/угол voxel реально затронул на boundary.

Причина проста:

- edit внутри chunk больше не должен триггерить лишние rebuild'ы соседей;
- edit на границе chunk всё ещё обязан безопасно обновлять видимые faces across-chunk без пропущенных border cases.

## Interaction path

Runtime block interaction сейчас не идёт через physics.

Он устроен так:

1. камера даёт origin и direction;
2. CPU `VoxelRaycast` проходит по плотному миру;
3. `InteractionState` хранит:
   - selected voxel;
   - placement voxel;
   - normal;
   - distance;
4. `VoxelInteraction` вызывает `SetVoxelMaterial` для `remove/place`.

Это важное решение:

- CPU raycast уже честно решает gameplay MVP задачу;
- physics на этом этапе добавляет collision/walk, а не подменяет selection loop.

## Связь с renderer

Renderer использует `VoxelWorld` не напрямую, а через `SceneResources`.

Связь выглядит так:

- `VoxelWorld` отдаёт dirty chunk list;
- `SceneResources` repack'ает voxel payload и chunk descriptors;
- compute meshing создаёт packed faces;
- graphics pass рисует результат.

То есть `VoxelWorld` отвечает за truth и dirty bookkeeping, а не за GPU layout.

## Связь с physics

Physics sync сейчас завязан на `editVersion`.

Если edit не меняет material, `editVersion` не растёт.

Если material реально изменился:

- `editVersion` увеличивается;
- physics world при следующем sync понимает, что voxel collision body устарел;
- static collision body перестраивается из актуальных solid voxels.

Это простой и явный sync contract для current MVP.

## Инварианты, которые уже проверяются тестами

Текущий test suite уже покрывает:

- material bookkeeping;
- dirty-neighbor handling;
- CPU raycast;
- interaction remove/place;
- physics world sync;
- walk/collision glue.

Это важнее, чем “кажется, оно и так работает”, потому что chunk границы, raycast и physics sync легко ломаются
тихими регрессиями.

## Известные ограничения current slice

- мир по-прежнему фиксирован по границам и размеру procedural lab scene;
- current snapshot path покрывает только `VoxelWorld`, а не полный game/session state;
- richer chunk model ещё не сделан;
- `walk` controller уже использует continuous foot-support score на block edges и не магнитит персонажа к нижнему floor после
  полной потери опоры; passive edge-slide без input дополнительно режется через `CharacterVirtual` contact listener как
  selective downhill removal только на одном best floor-like contact за кадр, edge-jump кратко лочит sample-based
  regrounding, а jump-on-ledge остаётся физическим без отдельного `Y`-snap helper'а и использует только узкий
  non-rising ledge catch против top-edge snag. `Shift` в `walk` включает отдельный sneak/crouch path с lower
  stance и непрерывной face-based support geometry для центра стопы: pre-move safe-walk проектирует `desired feet XZ`
  в объединение walkable top-face support area, cached support-region grace и post-solve correction используют ту же
  область, так что вдоль края можно идти и
  заходить почти в corner, не сваливаясь вниз при зажатом `Shift`; если `feet XZ` всё ещё остаётся внутри cached
  support-region, sneak может вернуть небольшой solver-driven drop обратно к cached top-face height, чтобы не копить
  wall-cling по `Y`. Отпускание `Shift` на уже безопасной кромке теперь не должно мгновенно ронять персонажа, пока он
  не делает новый unsafe шаг наружу. Сам ledge catch для jump теперь смотрит не только центральный probe, но и lateral
  offsets, и разрешён только из pre-step grounded support, чтобы dead-pixel на single-block edge/corner не был привязан
  к одному точному попаданию.
- `Fluid` пока лишь visual/world material, а не полноценная simulation or collision system.

## Связанные документы

- [ArchitectureGuide](ArchitectureGuide.md)
- [RenderArchitecture](RenderArchitecture.md)
- [Debugging](Debugging.md)
- [Profiling](Profiling.md)
