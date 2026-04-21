# ProjectV Debugging

Дата фиксации: `2026-04-07`

Этот документ описывает текущий debugging/tooling path в `ProjectV`: HUD, runtime diagnostics, Tracy, smoke scripts и
failure probes.

## Быстрый набор инструментов

В current mainline уже есть:

- in-app HUD;
- runtime diagnostics с единым форматом логов;
- Tracy CPU/GPU instrumentation;
- automated runtime smoke;
- automated failure probes;
- unit tests на critical world/interaction/physics glue.

Это и есть текущий “первый честный” debugging contour.

## HUD

HUD рисуется внутри приложения отдельным debug pipeline.

Сейчас через него видны:

- FPS и frame time;
- simulation steps;
- triangle count;
- dirty/active chunks;
- non-air voxels и memory usage;
- текущий editor tool;
- состояние `chunk bounds` / `dirty chunk overlay`;
- camera telemetry;
- selection telemetry;
- выбранный chunk и его dirty/active summary;
- текущий control mode.

Полезные hotkeys:

- двойной `Space` — quick toggle `creative / walk`
- `F1` — весь debug UI on/off: HUD, selection/inspect highlight, chunk overlays и crosshair
- `F2` — cycle placement material
- `F3` — reset camera
- `F4` — cycle `creative / spectator / walk`
- `F5` — cycle builtin scene preset в рантайме
- `F6` — сохранить current world snapshot
- `F7` — загрузить world snapshot
- `F8` — cycle editor tool: `OFF -> PAINT -> ERASE -> FILL -> INSPECT`
- `F9` — toggle `chunk bounds`
- `F10` — toggle `dirty chunk overlay`
- `P` — pause
- `Tab` — relative mouse mode

Editor-tool contract:

- `OFF` сохраняет старый `LMB remove / RMB place` path;
- `PAINT` красит hit voxel по `LMB` и ставит adjacent voxel по `RMB`;
- `ERASE` удаляет hit voxel обеими кнопками;
- `FILL` flood-fill'ит connected region материала hit voxel в выбранный placement material;
- `INSPECT` оставляет raycast/read-only path и добавляет chunk-oriented telemetry/overlay.

## Runtime diagnostics

Критические runtime ошибки проходят через `RuntimeDiagnostics` из [RuntimeDiagnostics.hpp](../src/core/RuntimeDiagnostics.hpp).

Формат логов:

- `[ProjectV][Subsystem][Step] ...`

Текущие helpers:

- `LogRuntimeFailure`
- `LogVkFailure`
- `LogVmaFailure`
- `LogSdlFailure`
- `PV_CHECK_OR_RETURN`
- `PV_ASSERT`

Практический смысл:

- expected failures логируются явно и возвращают `false`;
- invariant failures в debug build должны падать быстро и явно.

## Smoke scripts

### Runtime smoke

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVRuntimeSmoke.ps1
```

Проверяет:

- запуск окна;
- resize;
- minimize/restore;
- maximize/restore;
- graceful shutdown.

### Failure probes

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVFailureProbes.ps1
```

Проверяет:

- controlled failure на missing shader blob;
- controlled failure на intentional incomplete init.

## Failure injection knobs

Сейчас поддерживаются две главные env-based probe ручки:

- `PROJECTV_SHADER_BASE_DIR`
- `PROJECTV_FAIL_INIT_STAGE`

Поддерживаемые `PROJECTV_FAIL_INIT_STAGE` значения:

- `after_bootstrap`
- `after_world`
- `after_scene_resources`
- `before_graphics_pipeline`
- `before_voxel_meshing_pipeline`

Это current reproducible failure path без порчи кода и без ручного удаления `.spv`.

## Tracy

### Что уже инструментировано

CPU зоны размечаются через:

- `PV_PROFILE_ZONE()`
- `PV_PROFILE_ZONE_N(...)`
- `PV_PROFILE_FRAME_MARK()`

GPU зоны размечаются через:

- `PV_PROFILE_GPU_ZONE(...)`

Основные plot'ы конфигурируются в [Profiling.hpp](../src/debug/Profiling.hpp) и сейчас считаются фиксированным benchmark metrics pack:

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
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
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

Отдельная practical methodology и baseline scene presets описаны в [Profiling](Profiling.md).

### Как смотреть Tracy

1. Собери и запусти `ProjectV`.
2. Если нужен bundled profiler UI, используй preset `windows-clang-debug-tracy-profiler` и отдельно собери target `tracy-profiler.exe`.
3. Запусти profiler и подключись к приложению.
4. Смотри:
   - CPU zones вокруг `UpdateApp`, `TickWalkCharacter`, `UpdateWalkGroundSupport`, `PrepareFrameRenderData`, `DrawFrame`;
   - GPU zones для `Voxel Meshing`, `Opaque Pass`, `Transparent Pass`, `Debug Overlay`, `Debug HUD`;
   - plot'ы по dirty chunks, face counts, upload bytes и `walk` state drift.

### Важная оговорка

Обычный `windows-clang-debug` уже инструментирован Tracy по умолчанию. Специальный Tracy preset нужен в основном для
bundled profiler UI, а не для самого факта instrumentation.

## Что использовать для каких проблем

Если проблема в input/control modes:

- смотри HUD;
- проверяй `UpdateApp` и `InputActions`;
- проверяй manual smoke.

Если проблема в world edits / raycast / chunk rebuild:

- смотри HUD и `VoxelWorldTests`;
- смотри dirty chunk counters и face counts;
- проверяй `VoxelWorld`, `VoxelInteraction`, `SceneResources`.

Если проблема в resize/minimize/restore/shutdown:

- сначала запускай runtime smoke script;
- потом смотри swapchain recreate path и stderr runtime logs.

Если проблема в init/bootstrap:

- сначала failure probes;
- потом `RuntimeDiagnostics` logs;
- потом `VulkanBootstrap` / `VulkanInit`.

Если проблема в производительности:

- сначала Tracy plots и zones;
- потом HUD counters;
- только потом hypotheses/optimisation.

## Полезные файлы

- [voxel_mvp_smoke_checklist.md](voxel_mvp_smoke_checklist.md)
- [BuildAndRun](BuildAndRun.md)
- [Profiling](Profiling.md)
- [RenderArchitecture](RenderArchitecture.md)
- [VoxelWorld](VoxelWorld.md)
