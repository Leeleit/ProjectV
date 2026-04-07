# ProjectV Build And Run

Дата фиксации: `2026-04-07`

Этот документ фиксирует актуальный практический путь сборки и запуска `ProjectV` в текущем Windows `clang-cl` mainline.

## Что нужно в окружении

Минимально ожидается:

- Windows;
- CMake `3.30+`;
- Ninja;
- `clang-cl`;
- Visual Studio Build Tools / MSVC headers and linker environment;
- Vulkan SDK с `glslc` в `PATH`;
- Git с поддержкой submodules.

Текущий основной preset также ожидает `ccache` как `CMAKE_CXX_COMPILER_LAUNCHER`. Если его нет в `PATH`, configure
может не пройти без правки preset/cache variables.

## Сабмодули

Перед первой сборкой нужно инициализировать vendored dependencies:

```powershell
git submodule update --init --recursive
```

Это обязательно, потому что проект собирает:

- `SDL`
- `volk`
- `VMA`
- `fmt`
- `flecs`
- `JoltPhysics`
- `Tracy`

и другие bundled зависимости.

## Основной debug preset

Текущий основной preset — `windows-clang-debug`.

Configure:

```powershell
cmake --preset windows-clang-debug
```

Основные свойства этого preset:

- generator: `Ninja`
- compiler: `clang-cl`
- linker: `LLD`
- build type: `Debug`
- compile commands: `ON`

## Сборка

Собрать приложение:

```powershell
cmake --build build/windows-clang-debug --target ProjectV
```

Собрать тесты:

```powershell
cmake --build build/windows-clang-debug --target ProjectVTests
```

Важно:

- shader compilation встроена в build target через `glslc`;
- `.spv` blobs копируются post-build рядом с `ProjectV.exe`;
- проверки одного и того же build tree лучше гонять последовательно, а не несколькими параллельными `cmake --build`.

## Тесты

Запуск тестов:

```powershell
ctest --test-dir build/windows-clang-debug --output-on-failure
```

На текущем mainline это покрывает не только `VoxelWorld`, но и interaction/physics glue regression path.

## Запуск приложения

Прямой запуск:

```powershell
build/windows-clang-debug/bin/ProjectV.exe
```

## Scene presets для profiling runs

`ProjectV` теперь поддерживает builtin scene presets через runtime env var `PROJECTV_SCENE_PRESET`.

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

## Runtime smoke

Автоматический runtime smoke:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVRuntimeSmoke.ps1
```

Он проверяет:

- открытие окна;
- resize;
- minimize/restore;
- maximize/restore;
- graceful shutdown.

Manual checklist живёт отдельно в [voxel_mvp_smoke_checklist.md](voxel_mvp_smoke_checklist.md).

## Failure probes

Автоматический probe path:

```powershell
powershell -ExecutionPolicy Bypass -File tools/windows/Invoke-ProjectVFailureProbes.ps1
```

Сейчас он проверяет два сценария:

- missing shader blobs через пустой `PROJECTV_SHADER_BASE_DIR`;
- intentional incomplete init через `PROJECTV_FAIL_INIT_STAGE=before_voxel_meshing_pipeline`.

## Tracy

По умолчанию `ProjectV` уже собирается с Tracy instrumentation, потому что `PROJECTV_ENABLE_TRACY` в root CMake сейчас
включён по умолчанию.

Отдельный preset `windows-clang-debug-tracy-profiler` нужен, когда хочется собрать ещё и bundled Tracy profiler UI:

```powershell
cmake --preset windows-clang-debug-tracy-profiler
cmake --build build/windows-clang-debug-tracy-profiler --target ProjectV
```

Если цель — максимально лёгкий debug build без Tracy-инструментации, этого не даёт сам по себе `windows-clang-debug`:
нужно явно выключать `PROJECTV_ENABLE_TRACY`.

## Частые проблемы

### `glslc` not found

Проект компилирует shaders через `find_program(GLSLC glslc REQUIRED)`.

Если configure падает на этом шаге:

- проверь, что установлен Vulkan SDK;
- проверь, что `glslc` доступен в `PATH`.

### Missing `.spv` рядом с exe

Если приложение не находит shader blob:

- сначала пересобери target `ProjectV`;
- затем проверь, что post-build copy отработал и `.spv` лежат рядом с бинарником;
- не запускай exe из дерева, где не было актуального build step.

### `ccache` launcher issue

Если configure жалуется на launcher:

- установи `ccache`;
- или переопредели `CMAKE_CXX_COMPILER_LAUNCHER` для локального окружения.

### Dirty submodules

Если build внезапно ломается после переключения веток:

- сначала проверь `git status --short`;
- затем при необходимости снова синхронизируй submodules через `git submodule update --init --recursive`.

## Связанные документы

- [ArchitectureGuide](ArchitectureGuide.md)
- [Debugging](Debugging.md)
- [Profiling](Profiling.md)
- [Source Layout Guide](source_layout.md)
- [Voxel MVP Smoke Checklist](voxel_mvp_smoke_checklist.md)
