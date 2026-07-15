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

HUD — Dear ImGui (status strip + Settings/Stats). Bitmap debug HUD pipeline удалён.

Видно в strip: FPS/frame time, control mode, pause, editor tool, AA/SMAA/scale.

Settings (`` ` ``): overlays, lighting, walk, world/editor actions, music, replay, dev
(screenshot, shader reload, present mode, quit). Stats panel — детальная телеметрия.

- `F1` — скрыть весь UI (strip + окна);
- `` ` `` — Settings on/off (+ освобождает/возвращает relative mouse);
- `Tab` — relative mouse;
- редкие debug toggles — только через Settings (не отдельные hotkey).

## Полезные hotkeys

Основные клавиши рантайма (bindings в `src/app/InputActions.cpp`):

- `W/A/S/D` — перемещение;
- `Space` — вверх / двойное нажатие toggles `creative ↔ walk`;
- `Shift` — вниз / sneak в walk mode;
- `Ctrl` — speed boost;
- `Alt` — speed slow;
- `Tab` — relative mouse mode;
- `F1` — hide UI;
- `F2` — cycle placement material (White → Gray → Glass → Fluid);
- `` ` `` — Settings;
- `Esc` — quit.

Остальные действия (MSAA, overlays, save/load, editor tool, music, lighting, …) —
кнопки/галочки в Settings. Editor LMB/RMB без изменений.

Editor-tool contract:

- `OFF` сохраняет старый `LMB remove / RMB place` path;
- `PAINT` красит hit voxel по `LMB` и ставит adjacent voxel по `RMB`; при активном anchor заполняет весь box между
  anchor и текущим target/placement voxel;
- `ERASE` удаляет hit voxel обеими кнопками; при активном anchor стирает весь box между anchor и текущим target voxel;
- `FILL` flood-fill'ит connected region материала hit voxel в выбранный placement material;
- `INSPECT` оставляет raycast/read-only path и добавляет chunk-oriented target/placement telemetry/overlay.

Anchor contract:

- Settings → Editor → mutation anchor ставит или снимает anchor на логически текущем voxel для активного tool;
- для `PAINT` / `INSPECT` anchor обычно идёт по `placementVoxel`, для `ERASE` — по `targetVoxel`;
- overlay показывает и сам anchor voxel, и preview box до текущего target/placement voxel;
- pick material не меняет мир, а только синхронизирует placement material с текущим hit material.

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
