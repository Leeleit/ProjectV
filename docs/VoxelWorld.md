# ProjectV VoxelWorld

Дата фиксации: `2026-04-07`

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

Render-facing material response теперь живёт в [VoxelMaterials.cpp](../src/voxel/VoxelMaterials.cpp): цвет, lighting
response,
fresnel/transmission/emissive параметры и scene-lighting presets описываются на CPU, а не как жёстко зашитые константы в
shader.

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
параметры для `voxel.frag`, поэтому `F5` циклично меняет и scene layout, и освещение.

`VoxelLab` по-прежнему создаёт текущую основную demo-scene:

- шахматный пол;
- стеклянную сферу;
- жидкость внутри;
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
