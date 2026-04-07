# ProjectV

Актуальный обзор проекта на `2026-04-07`.

`ProjectV` сейчас находится в стадии `pre-MVP alpha / ранний vertical slice`: это уже не triangle-prototype, а ранний
voxel sandbox renderer с интерактивным editing loop, HUD, базовым ECS/physics slice и проверенным runtime stability
path.

## Что уже работает

- Vulkan bootstrap, swapchain recreate и controlled shutdown;
- CPU `VoxelWorld` с чанками, dirty queue и chunk rebuild bookkeeping;
- минимальный `flecs` ECS slice с primary camera/player entities, `world`/`debug` singleton data и chunk mirror summary;
- минимальный `JoltPhysics` slice со static voxel collision world, physics raycast и общим controller для `creative` /
  `walk`;
- compute voxel meshing и opaque/transparent indirect draw;
- CPU raycast, block picking, `remove/place` edits и корректное dirty-neighbor обновление;
- `creative`, `spectator` и `walk` control modes;
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
- `Space / Shift` — вверх / вниз в `creative` и `spectator`.
- `Ctrl / Alt` — ускорение / замедление камеры.
- `Tab` — toggle relative mouse mode.
- двойной `Space` — переключение `creative` / `walk`.
- `F1` — показать или скрыть весь debug UI: HUD, highlight и crosshair.
- `F2` — сменить placement material.
- `F3` — reset camera.
- `F4` — переключение `creative` / `spectator` / `walk`.
- `F5` — cycle builtin scene preset без перезапуска приложения.
- `P` — pause simulation.
- `LMB` — удалить voxel в `creative` и `walk`.
- `RMB` — поставить voxel в `creative` и `walk`.

Поведение control modes:

- `creative` — physics-backed flight mode: камера летает с collision, двигается даже при `pause`, `Space/Shift` двигают
  по
  вертикали, а edits разрешены;
- `spectator` — observe-only noclip mode: edits отключены, а камера подчиняется `pause`;
- `walk` — grounded physics-driven mode: движение подчиняется collision, `Space` работает как jump, а edits по-прежнему
  разрешены; при переходе из `creative`/`spectator` камера не телепортируется в центр и не снапается к полу без
  необходимости.

## Где смотреть дальше

- [Build And Run](docs/BuildAndRun.md)
- [Architecture Guide](docs/ArchitectureGuide.md)
- [Render Architecture](docs/RenderArchitecture.md)
- [VoxelWorld](docs/VoxelWorld.md)
- [Debugging](docs/Debugging.md)
- [Profiling](docs/Profiling.md)
- [Source Layout Guide](docs/source_layout.md)
- [Voxel MVP Smoke Checklist](docs/voxel_mvp_smoke_checklist.md)
- [TODO](TODO.md)
- [AGENTS](AGENTS.md)

## Текущий фокус

Ближайший mainline-фокус после закрытия `9.1`:

1. добить `9.2` build/automation hygiene;
2. вернуться к `10.1` и добавить `save/load` поверх уже существующих builtin scene presets;
3. потом дочищать player/debug tooling уже поверх связки `InputActions + ECS + physics`.
