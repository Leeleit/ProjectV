# ProjectV Profiling

Дата фиксации: `2026-04-07`

Этот документ фиксирует текущий practical profiling path в `ProjectV`: какие baseline-сцены использовать, какие Tracy plot'ы считаются
основным metrics pack и как воспроизводить perf-замеры без «магии в голове».

## Что считать текущим profiling baseline

`ProjectV` сейчас не имеет полноценного benchmark harness или automated perf lab. Вместо этого mainline использует:

- builtin benchmark scene presets в `VoxelWorld`;
- Tracy CPU/GPU instrumentation;
- HUD counters для quick sanity-check;
- ручную, но воспроизводимую methodology.

Этого достаточно для текущего этапа: сначала измеримость и повторяемость, потом уже optimisation work.

## Baseline scene presets

Scene preset выбирается через runtime env var `PROJECTV_SCENE_PRESET`.

Поддерживаемые значения:

- `VoxelLab`
- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

Если переменная не задана, используется `VoxelLab`.

### Что мерить на какой сцене

- `VoxelLab` — повседневный default baseline: mixed opaque + transparent + fluid scene, полезен для общего regression-check.
- `FlatBenchmark` — почти плоская сцена без стекла и жидкости; хороший baseline для camera/update/render cost без stress-case geometry.
- `TransparencyStress` — dense набор стеклянных колонн; нужен для transparent pass и face-count pressure на прозрачной геометрии.
- `ChunkGrid` — world-wide chunk markers; полезен для active chunk coverage, upload path и chunk-oriented debug.
- `MeshingStress` — 3D checker volume; нужен для worst-case-ish meshing, face generation и upload churn.

## Как запускать нужную сцену

Пример для PowerShell:

```powershell
$env:PROJECTV_SCENE_PRESET = "MeshingStress"
build/windows-clang-debug/bin/ProjectV.exe
Remove-Item Env:PROJECTV_SCENE_PRESET
```

Если нужна не просто instrumentation, а bundled Tracy profiler UI:

```powershell
cmake --preset windows-clang-debug-tracy-profiler
cmake --build build/windows-clang-debug-tracy-profiler --target ProjectV tracy-profiler.exe
$env:PROJECTV_SCENE_PRESET = "TransparencyStress"
build/windows-clang-debug-tracy-profiler/bin/ProjectV.exe
build/windows-clang-debug-tracy-profiler/bin/tracy-profiler.exe
Remove-Item Env:PROJECTV_SCENE_PRESET
```

Это opt-in tooling path, а не основной automation preset.

## Зафиксированный Tracy metrics pack

Текущий plot pack считается частью profiling contract и конфигурируется в `src/debug/Profiling.hpp`.

Ключевые plot'ы:

- `Frame Delta (ms)`
- `Simulation Accumulator (ms)`
- `Simulation Steps`
- `Dirty Chunks`
- `Active Chunks`
- `Rebuilt Chunks`
- `Repacked Chunk Voxels`
- `Scene Triangles`
- `Generated Opaque Faces`
- `Generated Transparent Faces`
- `Meshing Dirty Chunks`
- `Visible Chunks`
- `Culled Chunks`
- `Chunk Voxel Words`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Uploaded Chunk Voxel Words`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`
- `Walk Support State`
- `Walk Support Score`
- `Walk Feet Y`
- `Walk Velocity Y`
- `Walk Sneak Active`
- `Walk Jump Lock`
- `Walk Cached Sneak Support`
- `Walk Feet Inside Sneak Cache`
- `Walk Edge Grace`
- `Walk Ground Takeoff Grace`
- `Walk Sneak Support Grace`
- `Walk Ledge Release Grace`
- `Walk Ground Return Anchor`

Это покрывает ровно тот minimum pack, который сейчас нужен по `TODO`: frame time, chunk rebuild count, repacked voxel count,
generated opaque/transparent faces, visibility pressure, upload sizes и live `walk` state drift.

## Recommended measurement methodology

Если цель — честно сравнить два состояния проекта, держи одинаковыми:

- один и тот же build preset;
- одну и ту же scene preset;
- один и тот же window size;
- один и тот же control mode;
- одинаковую camera position / view direction;
- одинаковое состояние HUD.

Практический порядок:

1. Собери нужный build preset.
2. Выбери baseline scene через `PROJECTV_SCENE_PRESET`.
3. Запусти приложение и подключи Tracy.
4. Если измеряешь renderer baseline, скрой HUD через `F1`, чтобы не включать его CPU overlay cost в цифры.
5. Дай сцене прогреться несколько секунд после запуска или после first-frame rebuild.
6. Не редактируй мир во время capture, если меряешь steady-state render/update.
7. Для interaction/meshing path отдельно делай controlled edit sequence и смотри `Dirty Chunks`, `Rebuilt Chunks`,
   `Repacked Chunk Voxels`, `Generated * Faces`, `Visible/Culled Chunks` и upload bytes.
8. Снимай не один кадр, а небольшой непрерывный промежуток времени.

## Как интерпретировать текущие сцены

### Steady-state render baseline

Используй:

- `FlatBenchmark`
- `VoxelLab`

Смотри в первую очередь:

- `Frame Delta (ms)`
- CPU zones вокруг `UpdateApp`, `PrepareFrameRenderData`, `DrawFrame`
- GPU zones для `Opaque Pass`, `Transparent Pass`, `Debug HUD`

### Transparent pass pressure

Используй:

- `TransparencyStress`

Смотри:

- `Generated Transparent Faces`
- `Scene Triangles`
- GPU zone `Transparent Pass`

### Chunk/update pressure

Используй:

- `ChunkGrid`
- `MeshingStress`

Смотри:

- `Dirty Chunks`
- `Rebuilt Chunks`
- `Repacked Chunk Voxels`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`

## Важные оговорки текущего этапа

- Это ещё не automated benchmark suite.
- Scene presets пока builtin и code-driven, а не save/load/data-driven assets.
- HUD сам по себе стоит CPU/GPU времени, поэтому для честного renderer baseline его лучше выключать.
- `windows-clang-debug` и `windows-clang-debug-ci` уже instrumented Tracy по умолчанию; специальный Tracy preset нужен в
  основном для bundled profiler UI.

## Связанные документы

- [BuildAndRun](BuildAndRun.md)
- [Debugging](Debugging.md)
- [VoxelWorld](VoxelWorld.md)
- [README_NEW](../README_NEW.md)
- [TODO](../TODO.md)
