# ProjectV

Актуальный обзор проекта на `2026-04-07`.

`ProjectV` сейчас находится в стадии `pre-MVP alpha / ранний vertical slice`: это уже не triangle-prototype, а ранний
voxel sandbox renderer с интерактивным editing loop, HUD, базовым ECS/physics slice и проверенным runtime stability
path.

## Что уже работает

- Vulkan bootstrap, swapchain recreate и controlled shutdown;
- CPU `VoxelWorld` с чанками, dirty queue и chunk rebuild bookkeeping;
- минимальный `flecs` ECS slice с primary camera/player entities, `world`/`debug` singleton data и chunk mirror summary;
- минимальный `JoltPhysics` slice со static voxel collision world, physics raycast и walk controller;
- compute voxel meshing и opaque/transparent indirect draw;
- CPU raycast, block picking, `remove/place` edits и корректное dirty-neighbor обновление;
- `free-fly`, `spectator` и `walk` control modes;
- block highlight, crosshair и in-app HUD;
- runtime smoke и failure probes через скрипты в `tools/windows/`.

## Быстрый старт

1. Инициализировать сабмодули:

```powershell
git submodule update --init --recursive
```

2. Сконфигурировать основной debug preset:

```powershell
cmake --preset windows-clang-debug
```

3. Собрать приложение и тесты:

```powershell
cmake --build build/windows-clang-debug --target ProjectV
cmake --build build/windows-clang-debug --target ProjectVTests
```

4. Прогнать тесты:

```powershell
ctest --test-dir build/windows-clang-debug --output-on-failure
```

5. Запустить приложение:

```powershell
build/windows-clang-debug/bin/ProjectV.exe
```

## Управление

- `WASD` — движение в плоскости.
- `Space / Shift` — вверх / вниз в `free-fly`.
- `Ctrl / Alt` — ускорение / замедление камеры.
- `Tab` — toggle relative mouse mode.
- `F1` — показать или скрыть HUD.
- `F2` — сменить placement material.
- `F3` — reset camera.
- `F4` — переключение `free-fly` / `spectator` / `walk`.
- `P` — pause simulation.
- `LMB` — удалить voxel в `free-fly` и `walk`.
- `RMB` — поставить voxel в `free-fly` и `walk`.

Поведение control modes:

- `free-fly` — debug/tool mode: noclip-камера двигается даже при `pause`, `Space/Shift` двигают по вертикали, а edits
  разрешены;
- `spectator` — observe-only mode: edits отключены, а камера подчиняется `pause`;
- `walk` — physics-driven mode: камера снапается к полу, движение подчиняется collision, `Space` работает как jump, а
  edits по-прежнему разрешены.

## Где смотреть дальше

- [Source Layout Guide](docs/source_layout.md)
- [Voxel MVP Smoke Checklist](docs/voxel_mvp_smoke_checklist.md)
- [TODO](TODO.md)
- [AGENTS](AGENTS.md)

## Текущий фокус

Ближайший mainline-фокус после закрытия `8.4`:

1. закрыть authored docs в `docs/` под текущий interaction/runtime/ECS/physics loop;
2. после этого двигаться в `save/load` и benchmark scene presets;
3. параллельно дочищать player/debug tooling уже поверх связки `InputActions + ECS + physics`.
