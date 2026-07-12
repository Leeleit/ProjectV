# ProjectV Debugging

Дата фиксации: `2026-04-21` (Обновлено: `2026-07-12`)

Этот документ описывает текущий debugging/tooling path в `ProjectV`: HUD, runtime diagnostics, Tracy, smoke scripts и
failure probes.

## Быстрый набор инструментов

В current mainline уже есть:

- in-app HUD;
- runtime diagnostics с единым форматом логов;
- Tracy CPU/GPU instrumentation;
- automated runtime smoke (Linux и Windows);
- automated failure probes;
- unit tests на critical world/interaction/physics glue.

## HUD

HUD рисуется внутри приложения отдельным debug pipeline.

Сейчас через него видны:

- FPS и frame time;
- simulation steps;
- triangle count;
- dirty/active chunks;
- non-air voxels, memory usage и `world edit version`;
- текущий editor tool;
- состояние `chunk bounds` / `dirty chunk overlay`;
- camera telemetry;
- selection telemetry;
- локальные voxel coords / hit normal;
- target/placement chunk summary;
- mutation anchor / preview box;
- текущий control mode;
- параметры освещения и exposure (в detailed mode).

HUD verbosity contract:

- `G` toggles `normal / detailed` HUD;
- `F1` toggles весь debug UI on/off: HUD, selection/inspect highlight, chunk overlays и crosshair.

## Полезные hotkeys

Основные клавиши рантайма (актуальные binding'и живут в `src/app/InputActions.cpp`):

- `W/A/S/D` — перемещение;
- `Space` — вверх / двойное нажатие toggles `creative ↔ walk`;
- `Shift` — вниз / sneak в walk mode;
- `Ctrl` — speed boost;
- `Alt` — speed slow;
- `Tab` — relative mouse mode;
- `P` — pause;
- `F1` — весь debug UI on/off;
- `F2` — cycle placement material;
- `F3` — reset camera;
- `F4` — cycle `creative / spectator / walk`;
- `F5` — cycle builtin scene preset (`VoxelLab`, `FlatBenchmark`, `TransparencyStress`, `ChunkGrid`, `MeshingStress`);
- `F6` — сохранить current world snapshot;
- `F7` — загрузить world snapshot;
- `F8` — cycle editor tool: `OFF -> PAINT -> ERASE -> FILL -> INSPECT`;
- `F9` — toggle `chunk bounds`;
- `F10` — toggle `dirty chunk overlay`;
- `F11` — toggle `walk` air-control mode (`MinecraftLike / Realistic`);
- `F12` — toggle one-block auto-jump micro-delay when auto-jump is enabled;
- `J` — toggle one-block auto-jump on/off;
- `R` — записать latest input replay вместе со snapshot;
- `Y` — проиграть latest input replay;
- `X` — toggle mutation anchor для box paint/erase helper;
- `M` — pick material из текущего hit voxel в placement material;
- `F` — pick model (model gravigun);
- `Z` — toggle cursor hit normal display;
- `C` — capture screenshot (.bmp + sidecar);
- `B` — cycle lighting debug view;
- `N` — cycle tone-map operator;
- `H/K` — decrease/increase lighting exposure;
- `V` — reset lighting debug controls to baseline preset;
- `Q/E` — music play/pause / stop;
- `7/8` — music volume down/up;
- `9/0` — next/previous music track;
- `[ / ]` — decrease/increase time scale;
- `\` — step single frame when paused;
- `` ` `` — reset time scale.

Editor-tool contract:

- `OFF` сохраняет старый `LMB remove / RMB place` path;
- `PAINT` красит hit voxel по `LMB` и ставит adjacent voxel по `RMB`; при активном anchor заполняет весь box между
  anchor и текущим target/placement voxel;
- `ERASE` удаляет hit voxel обеими кнопками; при активном anchor стирает весь box между anchor и текущим target voxel;
- `FILL` flood-fill'ит connected region материала hit voxel в выбранный placement material;
- `INSPECT` оставляет raycast/read-only path и добавляет chunk-oriented target/placement telemetry/overlay.

Anchor contract:

- `X` ставит или снимает anchor на логически текущем voxel для активного tool;
- для `PAINT` / `INSPECT` anchor обычно идёт по `placementVoxel`, для `ERASE` — по `targetVoxel`;
- overlay показывает и сам anchor voxel, и preview box до текущего target/placement voxel;
- `M` не меняет мир, а только синхронизирует placement material с текущим hit material.

## Runtime diagnostics

Критические runtime ошибки проходят через `RuntimeDiagnostics`
из [RuntimeDiagnostics.hpp](../src/core/RuntimeDiagnostics.hpp).

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

### Runtime smoke (Linux)

```bash
./tools/linux/Invoke-ProjectVRuntimeSmoke.sh
```

### Runtime smoke (Windows)

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVRuntimeSmoke.ps1
```

Это developer-only GUI smoke: targeted lifecycle check, не универсальный DoD. Используйте его после изменений в
Vulkan/bootstrap/swapchain/window lifecycle/present/screenshot sync или при риске device-lost/hang.

Проверяет:

- запуск окна;
- resize;
- minimize/restore;
- maximize/restore;
- graceful shutdown.

Для lighting/material/shader tuning основной сигнал — build/tests, scripted captures, sidecar metadata и визуальное
сравнение.

## Failure probes

### Linux

```bash
./tools/linux/Invoke-ProjectVFailureProbes.sh
```

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVFailureProbes.ps1
```

Проверяет:

- controlled failure на missing shader blob (`PROJECTV_SHADER_BASE_DIR`);
- controlled failure на intentional incomplete init (`PROJECTV_FAIL_INIT_STAGE`).

### Failure injection knobs

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

Основные plot'ы конфигурируются в [Profiling.hpp](../src/debug/Profiling.hpp) и считаются фиксированным benchmark
metrics pack.

### Как смотреть Tracy

1. Собери и запусти `ProjectV`.
2. Если нужен bundled profiler UI, используй preset `windows-clang-debug-tracy-profiler` /
   `linux-clang-debug-tracy-profiler` (если доступен) и отдельно собери target `tracy-profiler`.
3. Запусти profiler и подключись к приложению.
4. Смотри:
    - CPU zones вокруг `UpdateApp`, `TickWalkCharacter`, `UpdateWalkGroundSupport`, `PrepareFrameRenderData`,
      `DrawFrame`;
    - GPU zones для `Voxel Meshing`, `Opaque Pass`, `Transparent Pass`, `Debug Overlay`, `Debug HUD`;
    - plot'ы по dirty chunks, face counts, upload bytes и `walk` state drift.

## Что использовать для каких проблем

Если проблема в input/control modes:

- смотри HUD;
- проверяй `UpdateApp` и `InputActions`;
- проверяй replay/unit tests или manual interaction по конкретному сценарию.

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
- [Linux Build & Run Guide](Linux_Build_And_Run.md)
- [RTX Renderer Architecture](RTX_Renderer_Architecture.md)
- [Physics & Movement Guide](Physics_And_Movement_Guide.md)
- [BuildAndRun (Windows)](BuildAndRun.md)
- [Profiling](Profiling.md)
- [RenderArchitecture (Historical)](RenderArchitecture.md)
- [VoxelWorld (Historical)](VoxelWorld.md)
