# ProjectV VoxelWorld

> [!WARNING]
> **Исторический документ.** В данном файле содержатся устаревшие детали реализации воксельной базы данных (например,
> плоский массив `std::vector<uint8_t> voxels` вместо разреженного SVO дерева `Sparse64Tree`) и каскадных теней CSM.
> Современное описание структур хранения приведено в [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md), а физической структуры —
> в [Physics & Movement Guide](Physics_And_Movement_Guide.md).

Дата фиксации: `2026-04-22` (Обновлено: `2026-07-12`)

Этот документ описывает базовые принципы `VoxelWorld` в `ProjectV`: его логическую структуру, dirty queue, interaction
path и связь с render/physics слоями.

## Роль `VoxelWorld`

`VoxelWorld` — это current source of truth для мира.

Он отвечает за:

- границы мира;
- хранение материалов (SVO + dense fallback);
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

Render-facing material response живёт в [VoxelMaterials.cpp](../src/voxel/VoxelMaterials.cpp): `base color`, `AO`,
`roughness`, `metallic`, `reflectance`, transmission и fog/emissive/ambient/direct-response hooks.

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

Voxel данные хранятся в разреженном SVO (`Sparse64Tree`) с GPU-side представлением через NanoVDB-aligned SSBO.

Особенности:

- хранится material id;
- структура оптимизирована для sparse миров и быстрого GPU upload.

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

`VoxelWorldPreset.cpp` держит builtin preset layer для reproducible runs.

Текущий default preset:

- `VoxelLab`

Дополнительные baseline presets:

- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

Они выбираются через `PROJECTV_SCENE_PRESET` и нужны в первую очередь для profiling/benchmark work.

Поверх preset-baseline current live look-dev ladder остаётся keyboard-driven:

- `B` циклично переключает lighting debug views;
- `N` циклично переключает tone-map operator;
- `H/K` двигают exposure вниз/вверх;
- `V` сбрасывает lighting debug controls к baseline preset;
- `C` сохраняет текущий кадр в `.bmp` плюс sidecar metadata.

## World snapshots

Поверх builtin presets `VoxelWorld` поддерживает file-backed snapshot path.

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

Причина: edit внутри chunk не должен триггерить лишние rebuild'ы соседей; edit на границе chunk всё ещё обязан безопасно
обновлять видимые faces across-chunk.

## Interaction path

Runtime block interaction сейчас не идёт через physics.

Он устроен так:

1. камера даёт origin и direction;
2. CPU `VoxelRaycast` проходит по плотному миру;
3. `InteractionState` хранит selected voxel, placement voxel, normal, distance;
4. `VoxelInteraction` вызывает `SetVoxelMaterial` для `remove/place`.

Это важное решение: CPU raycast уже решает gameplay MVP задачу; physics добавляет collision/walk, а не подменяет
selection loop.

## Связь с renderer

Renderer использует `VoxelWorld` не напрямую, а через `SceneResources`.

- `VoxelWorld` отдаёт dirty chunk list;
- `SceneResources` repack'ает voxel payload и chunk descriptors;
- compute meshing создаёт packed faces;
- graphics pass рисует результат.

То есть `VoxelWorld` отвечает за truth и dirty bookkeeping, а не за GPU layout.

## Связь с physics

Physics sync сейчас завязан на `editVersion`.

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

## Известные ограничения current slice

- мир по-прежнему фиксирован по границам и размеру procedural lab scene;
- current snapshot path покрывает только `VoxelWorld`, а не полный game/session state;
- richer chunk model ещё не сделан;
- `Fluid` пока лишь visual/world material, а не полноценная simulation or collision system.

## Связанные документы

- [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md) — полный разбор `VoxelWorld` и смежных систем
- [Physics & Movement Guide](Physics_And_Movement_Guide.md) — физика и перемещение
- [ArchitectureGuide](ArchitectureGuide.md)
- [RenderArchitecture (Historical)](RenderArchitecture.md)
- [Debugging](Debugging.md)
