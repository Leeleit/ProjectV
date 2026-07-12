# ProjectV Voxel MVP Smoke Checklist

Дата фиксации: `2026-04-07`

## Targeted Windows smoke

`Invoke-ProjectVRuntimeSmoke.ps1` — это targeted GUI lifecycle check, а не универсальный DoD. Его имеет смысл запускать
после изменений в Vulkan/bootstrap/swapchain/window lifecycle/present/screenshot sync или при риске device-lost/hang.
Для lighting/material/shader tuning основной сигнал — build/tests, scripted captures, sidecar metadata и визуальное
сравнение.

1. Собрать `ProjectV`:

```powershell
cmake --build build/windows-clang-debug --target ProjectV
```

2. Запустить runtime smoke script:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVRuntimeSmoke.ps1
```

Ожидаемый результат:

- окно успешно открывается;
- проходят resize -> minimize -> restore -> maximize -> restore;
- приложение закрывается через normal window shutdown;
- процесс завершается с `exit code 0`.

## Автоматические failure probes

1. Собрать `ProjectV`:

```powershell
cmake --build build/windows-clang-debug --target ProjectV
```

2. Запустить failure-probe script:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVFailureProbes.ps1
```

Ожидаемый результат:

- probe с пустым `PROJECTV_SHADER_BASE_DIR` завершает приложение с non-zero exit code и логом про missing shader blob;
- probe с `PROJECTV_FAIL_INIT_STAGE=before_voxel_meshing_pipeline` завершает приложение с non-zero exit code и явным intentional init failure log;
- оба сценария завершаются быстро, без зависания процесса.

## Базовый manual smoke

1. Запустить `build/windows-clang-debug/bin/ProjectV.exe`.
2. Проверить, что сцена рендерится, HUD виден, crosshair и highlight работают.
3. Нажать `F4` и убедиться, что HUD циклически переключает `MODE CREATIVE` / `MODE SPECTATOR` / `MODE WALK`.
4. В `creative` режиме ПКМ поставить блок рядом с существующим voxel.
5. В `creative` режиме ЛКМ удалить выбранный voxel.
6. В `creative` режиме упереться в voxel-геометрию и убедиться, что полёт больше не проходит сквозь блоки.
7. Дважды нажать `Space` и убедиться, что быстрый toggle переводит только между `creative` и `walk`.
8. Переключиться в `spectator` и убедиться, что selection остаётся, но `remove/place` больше не меняют мир.
9. Нажать `P` в `spectator` и убедиться, что симуляция стоит, но noclip-камера всё ещё двигается и крутится.
10. Не снимая `pause`, переключиться в `creative` и убедиться, что physics-backed movement больше не идёт, пока пауза
    активна.
11. Переключиться в `walk` и убедиться, что камера не телепортируется в центр/на пол без необходимости, не проваливается
   сквозь платформу и не проходит сквозь voxel-геометрию.
12. В `walk` режиме нажать `Space` и проверить, что jump работает, а затем ЛКМ/ПКМ проверить, что edits по-прежнему
    меняют мир и collision остаётся консистентной после world edits.
13. Нажать `Tab`, проверить toggle relative mouse mode, затем вернуть захват мыши обратно.
14. Во время интеракций выполнить resize окна в несколько размеров.
15. Свернуть окно и восстановить его.
16. Закрыть окно обычным способом.

Ожидаемый результат:

- нет crash;
- нет зависания на restore/shutdown;
- swapchain recreate не ломает voxel pass, overlay и HUD;
- интерактивные edits продолжают работать после restore/resize;
- `creative`, `spectator` и `walk` ведут себя по-разному и HUD это честно показывает;
- walk collision остаётся рабочей после прыжка, voxel edits и window lifecycle-событий.

## Остаток runtime-stability backlog

Эти проверки всё ещё остаются отдельным следующим слоем:

- единый стиль runtime error logging во всех init/runtime путях за пределами уже покрытого bootstrap/render/pipeline path;
- минимальные debug asserts/check macros в остальных runtime-модулях;
- интеграция smoke/failure probes в автоматический контур.

---

## Связанные документы

- [Documentation Index](README.md) — карта всех руководств
- [Debugging](Debugging.md)
- [BuildAndRun (Windows)](BuildAndRun.md)

