# ProjectV Build And Run

> [!NOTE]
> **Платформозависимое руководство (Windows).** Данный файл описывает сборку и запуск проекта на операционной системе Windows.
> Для сборки и тестирования в среде Linux (основной дев-контур проекта) используйте специализированное руководство [Linux Build & Run Guide](Linux_Build_And_Run.md).

Дата фиксации: `2026-04-07` (Обновлено: `2026-07-11`)

Этот документ фиксирует актуальный practical build/run path для `ProjectV` на Windows `clang-cl` mainline после
`9.2` cleanup: какие presets считаются основными, как запускать automation loop и какие build options сейчас реально
поддерживаются.

## Что нужно в окружении

Минимально ожидается:

- Windows;
- CMake `3.30+`;
- Ninja;
- `clang-cl`;
- Visual Studio Build Tools / MSVC headers and linker environment;
- **Visual C++ Redistributable** (`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`) — нужны `ProjectV.exe` на runtime, потому что `CMakeLists.txt` явно не override'ит `CMAKE_MSVC_RUNTIME_LIBRARY` и по дефолту это `/MD` (dynamic MSVC runtime). Либо поставь VC++ Redist с https://aka.ms/vs/17/release/vc_redist.x64.exe, либо пересобери со static runtime через `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` (тогда runtime влинкован в `ProjectV.exe`, но это +20% к размеру ELF);
- Vulkan SDK с `glslc` или `glslangValidator` в `PATH`;
- Git с поддержкой submodules.

## Сабмодули

Перед первой сборкой нужно инициализировать vendored dependencies:

```powershell
git submodule update --init --recursive
```

Это обязательно, потому что проект собирает `SDL`, `volk`, `VMA`, `fmt`, `flecs`, `JoltPhysics`, `Tracy` и другие
bundled зависимости.

## Mainline presets

Mainline теперь опирается на два явных configure preset:

- `windows-clang-debug` — локальная dev-сборка;
- `windows-clang-debug-ci` — тот же debug toolchain, но зафиксированный как automation/CI entrypoint.

Оба preset'а явно включают:

- `BUILD_TESTING=ON`
- `PROJECTV_ENABLE_VALIDATION=ON`
- `PROJECTV_ENABLE_TRACY=ON`
- `PROJECTV_ENABLE_IMGUI=OFF`
- `PROJECTV_ENABLE_RENDERDOC_MARKERS=ON`

Configure:

```powershell
cmake --preset windows-clang-debug
cmake --preset windows-clang-debug-ci
```

Отдельный `windows-clang-debug-tracy-profiler` остаётся opt-in tooling preset для Tracy instrumentation в
`ProjectV.exe` (без Tracy UI — UI теперь собирается standalone через `tools/tracy-standalone/build-tracy-windows.ps1`,
см. ниже). Не считается частью основного automation contour.

## Build presets

Для repeatable сборки теперь лучше использовать не ad-hoc target names, а preset'ы:

```powershell
cmake --build --preset windows-clang-debug-build
cmake --build --preset windows-clang-debug-ci-build
```

Они явно собирают:

- `ProjectV`
- `ProjectVTests`

Важно:

- shader compilation встроена в target `ProjectV` через `glslc` с fallback на `glslangValidator`;
- `.spv` blobs копируются post-build рядом с `ProjectV.exe`;
- проверки одного и того же build tree лучше гонять последовательно, а не несколькими параллельными `cmake --build`.

## Test presets

Запуск unit/regression tests:

```powershell
ctest --preset windows-clang-debug-tests
ctest --preset windows-clang-debug-ci-tests
```

Сейчас это проверяет не только `VoxelWorld`, но и interaction/physics glue path; тестовый target собирается с теми же
feature defines, что и runtime build, включая Tracy instrumentation when enabled.

## Automation script

Минимальный automation contour теперь зафиксирован в `tools/windows/Invoke-ProjectVBuildChecks.ps1`.

Локальный прогон dev loop:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVBuildChecks.ps1 `
  -ConfigurePreset windows-clang-debug `
  -BuildPreset windows-clang-debug-build `
  -TestPreset windows-clang-debug-tests
```

CI loop:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVBuildChecks.ps1
```

По умолчанию скрипт использует `windows-clang-debug-ci`, `windows-clang-debug-ci-build` и
`windows-clang-debug-ci-tests`.

## Запуск приложения

Прямой запуск:

```powershell
build/windows-clang-debug/bin/ProjectV.exe
```

## Runtime smoke

Smoke по-прежнему живёт как PowerShell script и build target, но это targeted lifecycle check, а не обязательная
проверка после каждого изменения. Используй его, когда менялись Vulkan/bootstrap/swapchain/window lifecycle,
present/screenshot sync или есть риск device-lost/hang.

```powershell
cmake --build --preset windows-clang-debug-smoke
```

или:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVBuildChecks.ps1 `
  -ConfigurePreset windows-clang-debug `
  -BuildPreset windows-clang-debug-build `
  -TestPreset windows-clang-debug-tests `
  -RunSmoke
```

Smoke проверяет:

- открытие окна;
- resize;
- minimize/restore;
- maximize/restore;
- graceful shutdown.

Опционально можно запустить полный LookDev capture flow (только Windows PowerShell, эквивалент Linux-варианта
в `tools/linux/Invoke-ProjectVRuntimeSmoke.sh:120-126`):

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVRuntimeSmoke.ps1 `
  -ExePath build\windows-clang-debug\bin\ProjectV.exe `
  -CaptureDir build\windows-clang-debug\lookdev-captures\$(Get-Date -Format yyyy-MM-dd) `
  -Views 'FINAL SHDW CTSH AOCC LOCL' `
  -CameraPosition '-25 19 25' `
  -CameraLook '0.62 -0.48 -0.62' `
  -WarmupFrames 30 -IntervalFrames 2 -QuitAfterCapture
```

Скрипт выставит `PROJECTV_SCREENSHOT_DIR`, `PROJECTV_START_CAMERA_*`, `PROJECTV_LOOKDEV_CAPTURE_*` env vars,
дождётся graceful shutdown и проверит, что ожидаемое число `.bmp` + `.txt` пар (default 6 видов × 2 = 12 файлов)
лежит в `$CaptureDir`. Без `-CaptureDir` поведение остаётся прежним — только lifecycle window dance.

Smoke почти не доказывает корректность gameplay, lighting/material look, shadow quality или shader tuning. Для таких
изменений основной сигнал — unit tests, replay/scripted captures, sidecar metadata и осмысленная визуальная проверка.

Manual checklist живёт отдельно в [voxel_mvp_smoke_checklist.md](voxel_mvp_smoke_checklist.md).

## Failure probes

Автоматический probe path:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVFailureProbes.ps1
```

Сейчас он проверяет два сценария:

- missing shader blobs через пустой `PROJECTV_SHADER_BASE_DIR`;
- intentional incomplete init через `PROJECTV_FAIL_INIT_STAGE=before_voxel_meshing_pipeline`.

## Build options

Текущие user-facing CMake options:

- `PROJECTV_ENABLE_VALIDATION` — включает Vulkan validation layers.
- `PROJECTV_ENABLE_TRACY` — включает Tracy instrumentation и для runtime, и для тестового target.
- `PROJECTV_ENABLE_IMGUI` — собирает bundled Dear ImGui support library для будущего debug UI path; mainline HUD всё ещё
  остаётся CPU-built overlay без `imgui`.
- `PROJECTV_ENABLE_RENDERDOC_MARKERS` — включает Vulkan debug names/markers path для RenderDoc-compatible captures.
- `PROJECTV_BUILD_TRACY_PROFILER` — собирает bundled Tracy profiler UI; использовать только когда это реально нужно.

Пример ручного override:

```powershell
cmake --preset windows-clang-debug -DPROJECTV_ENABLE_TRACY=OFF -DPROJECTV_ENABLE_VALIDATION=OFF
```

## Scene presets для profiling runs

`ProjectV` поддерживает builtin scene presets через runtime env var `PROJECTV_SCENE_PRESET`.

Пример:

```powershell
$env:PROJECTV_SCENE_PRESET = "FlatBenchmark"
build/windows-clang-debug/bin/ProjectV.exe
Remove-Item Env:PROJECTV_SCENE_PRESET
```

Поддерживаемые preset names:

- `VoxelLab`
- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

Это current practical путь для reproducible perf runs до появления полноценного save/load и benchmark automation.

## Tracy profiler preset

`windows-clang-debug-tracy-profiler` нужен только тогда, когда хочется собрать ещё и bundled Tracy profiler UI:

```powershell
cmake --preset windows-clang-debug-tracy-profiler
cmake --build build/windows-clang-debug-tracy-profiler --target ProjectV tracy-profiler.exe
```

Бинарники после этого лежат здесь:

- `build/windows-clang-debug-tracy-profiler/bin/ProjectV.exe`
- `build/windows-clang-debug-tracy-profiler/bin/tracy-profiler.exe`

Важно:

- этот preset тянет отдельный FetchContent/CPM dependency graph для profiler UI;
- он не входит в mainline CI;
- для repeatable day-to-day build/test path использовать нужно `windows-clang-debug` или `windows-clang-debug-ci`.

## Частые проблемы

### `glslc` / `glslangValidator` not found

Проект компилирует shaders через `glslc` и при его отсутствии fallback'ается на `glslangValidator`.

Если configure падает на этом шаге:

- проверь, что установлен Vulkan SDK;
- проверь, что хотя бы один из этих инструментов доступен в `PATH`.

### Missing `.spv` рядом с exe

Если приложение не находит shader blob:

- сначала пересобери `ProjectV` или `windows-clang-debug-build`;
- затем проверь, что post-build copy отработал и `.spv` лежат рядом с бинарником;
- не запускай exe из дерева, где не было актуального build step.

### Dirty submodules

Если build внезапно ломается после переключения веток:

- сначала проверь `git status --short`;
- затем при необходимости снова синхронизируй submodules через `git submodule update --init --recursive`.

## Связанные документы

- [Documentation Index](README.md) — карта всех руководств
- [Linux Build & Run Guide](Linux_Build_And_Run.md) (Основное руководство по Linux)
- [ArchitectureGuide](ArchitectureGuide.md)
- [Debugging](Debugging.md)
- [Profiling](Profiling.md)
- [Source Layout Guide](source_layout.md)
- [Voxel MVP Smoke Checklist](voxel_mvp_smoke_checklist.md)
