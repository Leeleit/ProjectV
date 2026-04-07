# ProjectV

Актуальный обзор проекта на `2026-04-07`.

`ProjectV` сейчас находится в стадии `pre-MVP alpha / ранний vertical slice`: это уже не triangle-prototype, а ранний voxel sandbox renderer с интерактивным editing loop, HUD и проверенным runtime stability path.

## Что уже работает

- Vulkan bootstrap, swapchain recreate и controlled shutdown;
- CPU `VoxelWorld` с чанками, dirty queue и chunk rebuild bookkeeping;
- compute voxel meshing и opaque/transparent indirect draw;
- CPU raycast, block picking, `remove/place` edits и корректное dirty-neighbor обновление;
- `free-fly` и `spectator` control modes;
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
- `Space / Shift` — вверх / вниз.
- `Ctrl / Alt` — ускорение / замедление камеры.
- `Tab` — toggle relative mouse mode.
- `F1` — показать или скрыть HUD.
- `F2` — сменить placement material.
- `F3` — reset camera.
- `F4` — переключение `free-fly` / `spectator`.
- `P` — pause simulation.
- `LMB` — удалить voxel в `free-fly`.
- `RMB` — поставить voxel в `free-fly`.

Поведение control modes:

- `free-fly` — debug/tool mode: камера двигается даже при `pause`, а edits разрешены;
- `spectator` — observe-only mode: edits отключены, а камера подчиняется `pause`.

## Где смотреть дальше

- [Source Layout Guide](docs/source_layout.md)
- [Voxel MVP Smoke Checklist](docs/voxel_mvp_smoke_checklist.md)
- [TODO](TODO.md)
- [AGENTS](AGENTS.md)

## Текущий фокус

Ближайший mainline-фокус после закрытия `8.2`:

1. дочистить remaining authored docs и корневую документацию;
2. решить, что из runtime diagnostics нужно расширять дальше;
3. после этого двигаться в `walk / noclip`, player controller, ECS/physics/save-load.
