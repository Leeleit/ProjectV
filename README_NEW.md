# ProjectV

Reproducible interactive voxel MVP на C++26, Vulkan 1.4, Data-Oriented Design.
Детальная архитектура — `legacy/docs/architecture/`, инженерные принципы — `legacy/docs/philosophy/`,
статус проекта — `agent/workspace.md §1 (Now)`.

## Стек

- C++26 (`CMAKE_CXX_STANDARD 26`). Stdlib: libc++ на Linux/macOS, MSVC STL на Windows. Per `agent/memory.md §6` + `agent/decisions.md §17`.
- Vulkan 1.4 (volk + VMA), SDL3, JoltPhysics, flecs, fmt, Tracy, miniaudio, meshoptimizer, fastgltf, draco
- Сборка: CMake 3.30+ + Ninja, clang-cl 22 на Windows, native clang 22 + lld 22 + libc++ на Linux. Windows-side uses MSVC STL (clang-cl flag `-stdlib=libc++` is a no-op there per `CMakeLists.txt:174`).

## Development build (Debug)

Каждодневная dev-петля — `Debug` build с validation layers, Tracy, RenderDoc-маркерами.

**Linux (Arch, native clang 22):**
```bash
cmake --preset linux-clang-debug
cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8
ctest --test-dir build/linux-clang-debug --output-on-failure
./build/linux-clang-debug/bin/ProjectV
```

**Windows (clang-cl 22, Visual Studio Build Tools 2026 + Vulkan SDK 1.4):**
```powershell
cmake --preset windows-clang-debug
cmake --build build/windows-clang-debug --target ProjectV ProjectVTests --parallel 8
ctest --test-dir build/windows-clang-debug --output-on-failure
.\build\windows-clang-debug\bin\ProjectV.exe
```

Подробности по debug-preset family (`-ci`, `-tracy-profiler`) и runtime smoke harness — `legacy/docs/standards/cmake/`,
`agent/decisions.md §4`, `tools/linux/Invoke-ProjectVRuntimeSmoke.sh`,
`tools/windows/Invoke-ProjectVRuntimeSmoke.ps1`.

## Release build

Оптимизированный binary для запуска готового продукта. Без validation layers, без Tracy,
без RenderDoc-маркеров, без Google Benchmark dev-harness. Conservative policy: `-O3 -flto=thin
-NDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only`; **без** `-ffast-math` (ломает
Fluid CA determinism + TAA YCoCg clamp) и **без** `-march=native` (binary должен быть переносим
между CPU). Полная политика — `agent/decisions.md §4` «Release presets».

**Linux:**
```bash
cmake --preset linux-clang-release
cmake --build build/linux-clang-release --target ProjectV ProjectVTests --parallel 8
ctest --test-dir build/linux-clang-release --output-on-failure
./build/linux-clang-release/bin/ProjectV
```

**Windows (собирать на Windows-хосте):**
```powershell
cmake --preset windows-clang-release
cmake --build build/windows-clang-release --target ProjectV ProjectVTests --parallel 8
ctest --test-dir build/windows-clang-release --output-on-failure
.\build\windows-clang-release\bin\ProjectV.exe
```

Ожидаемый размер ELF/EXE: 25-40 MB (vs 50.5 MB debug). Ожидаемый FPS на `VoxelLab` reference shot:
+1.5-2.5× vs debug.

## Управление

Управление, env-vars, debug-контролы — `docs/KT-3.1_User_Guide.md` (User Guide) и `agent/memory.md §1`.
Рантайм-смок на `VoxelLab` reference shot — `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh` /
`powershell -ExecutionPolicy Bypass -File tools\windows\Invoke-ProjectVRuntimeSmoke.ps1`.
