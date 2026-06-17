# Defense Competency FAQ — T1 (Вступление и проблема)

**Slot:** T1 (0:00–0:50, Участник 1 = Тиммейт 1, slides 1-2-3: Title+Problem+Goals)
**Кто говорит:** Тиммейт 1
**Реальная компетенция:** Сборка и тестирование (CMake, ctest, runtime smoke, presets)
**Out of scope (к кому перенаправить в Q&A):** архитектура/стек — к T2 (le1t); воксельный мир — к T3; рендеринг — к T4; физика — к T6; ассеты/аудио — к T5; все баги — к T2.

---

## 1. Verbatim твоей речи (T1)

> «Здравствуйте. Наша команда представляет проект ProjectV — открытый высокопроизводительный воксельный движок на базе Vulkan 1.4 и современного стандарта C++26.
>
> **[Переход на Слайд 2 — Проблема и ценность]**
>
> Разработчики воксельных миров сегодня сталкиваются с компромиссом: использовать закрытые коммерческие решения вроде движка Minecraft, готовые high-level инструменты без прямого контроля над ресурсами компьютера, либо устаревший OpenGL без современных оптимизаций памяти [T2.md]. Наша цель — создать открытый низкоуровневый фундамент, предоставляющий исследователям графики и разработчикам песочниц прямой контроль над CPU и видеокартой.
>
> **[Переход на Слайд 3 — Цели и спецификации]**
>
> Мы перевели эту задачу в измеримые требования. Из сорока восьми пунктов технического задания тридцать восемь закрыты в рамках текущего MVP, а пять явно отложены в роадмап [T2.md]. Основные критерии успеха: стабильные пятьсот плюс FPS на сцене VoxelLab в дебаг-сборке [T1.md, T2.md], размер релизного бинарника девятнадцать мегабайт, четырнадцать успешно проходящих тестов ядра и шесть автоматических рантайм-снимков пиксель-в-пиксель [T1.md, T4.md]. Передаю слово.»

---

## 2. Кто ты

**Легенда:** ты отвечал за сборку проекта и инфраструктуру тестирования. CMake пресеты, ctest 14 наборов, runtime smoke 6 captures, бенчмарки, документация. Ты НЕ отвечаешь за архитектуру, воксели, рендеринг, физику или ассеты — это другие тиммейты.

**На сцене:** ты говоришь T1 (Вступление и проблема) — это про проект в целом, не про твою зону.

**На Q&A:** ты отвечаешь на вопросы про **сборку, тесты, метрики, пресеты**.

---

## 3. Твоя компетенция: Сборка и тестирование

**Файлы (что ты реально знаешь):**
- `CMakePresets.json` — 12 configure-пресетов (8 debug + 4 release)
- `src/CMakeLists.txt` — root build script, ~30 строк
- `tests/CMakeLists.txt` — 14 test executables + 17 source files
- `cmake/ProjectVThirdParty.cmake` — third-party dependencies
- `src/app/BenchmarkAutomation.{hpp,cpp}` — автобенчмарки (env: `PROJECTV_BENCHMARK_FRAMES`, `*_WARMUP_FRAMES`, `*_QUIT`, `*_LOG_EVERY`)
- `src/app/LookDevCaptureAutomation.{hpp,cpp}` — runtime smoke (env: `PROJECTV_START_CAMERA_POSITION`, `*_CAMERA_LOOK`, `*_CAPTURE_VIEWS`, `*_WARMUP_FRAMES`, `*_INTERVAL_FRAMES`, `*_QUIT`)
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` — обёртка smoke
- `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` — Windows-версия

**14 ctest-тестов (baseline 14/14, 0.78s debug, 0.06s release):**
1. `ProjectVTests` (VoxelWorldTests.cpp) — ~157 sub-тестов: математика, инвалидация, fluid CA, voxel raycast, snapshot, scene config
2. `ProjectVAssetTests` (AssetLoaderTests.cpp) — 9 sub-тестов: LoadGlb, error paths
3. `ProjectVMeshBakerTests` (MeshBakerTests.cpp) — 4 sub-теста: meshopt steps
4. `ProjectVDracoTests` (DracoDecoderTests.cpp) — 3 sub-теста: Draco decode
5. `ProjectVFrustumCullingTests` (FrustumCullingTests.cpp) — 5 sub-тестов: C++ helper
6. `ProjectVCFrustumCullingTests` (CFrustumCullingTests.cpp) — C-kernel tests
7. `ProjectVSunShadowCascadeSplitsTests` (SunShadowCascadeSplitsTests.cpp) — Tier 5 split planning
8. `ProjectVBoxUvFixtureTests` (BoxUvFixtureTests.cpp) — 2 sub-теста: UV projection
9. `ProjectVMathTests` (MathTest.cpp) — Tier 0.A математика
10. `ProjectVStringIdTests` (StringIdTest.cpp) — Tier 1.D StringID
11. `ProjectVModuleSmoke` (ModuleSmokeTest.cpp) — Tier 2 C++26 modules
12. `ProjectVStdModuleProbe` (ProbeTest.cpp + StdModuleProbe.cpp) — Tier 2 std module probe
13. `ProjectVFluidCATests` (FluidCATests.cpp) — fluid CA determinism + spread
14. `ProjectVPresentModeTests` (PresentModeTests.cpp) — present mode cycle

**6/6 runtime smoke captures:** `FINAL` / `SHDW` / `CSM` / `CTSH` / `AOCC` / `LOCL` — пиксель-в-пиксель с эталоном. Запуск: `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh`. Outputs в `build/<preset>/lookdev-captures/<timestamp>/`.

**Sidecar metadata** — `.txt` рядом с `.bmp`, **60+ ключей** метаданных: FPS, frame time, voxel counts (per material), shadow params, TAA state, tone map operator, exposure bias, hot shader version, etc.

**Build команды (запомни):**
```bash
# Configure
cmake --preset linux-clang-debug
cmake --preset linux-clang-release

# Build
cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8
cmake --build build/linux-clang-release --target ProjectV ProjectVTests --parallel 8

# Test
ctest --test-dir build/linux-clang-debug --output-on-failure
# → 14/14 Test #1: ProjectVTests ... Passed
# → 100% tests passed, 0 tests failed out of 14

# Smoke
bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug
# → exit 0, 6 captures, ~1 sec wall clock
```

### 3.1. Алгоритм 22 — Система сборки (CMake presets, ctest, RuntimeSmoke)

**Где:** корневой `CMakeLists.txt` + `CMakePresets.json`.
**Структура:**

**Configure presets (12 total = 8 debug + 4 release):**
- `windows-clang-debug` (основной dev tree)
- `windows-clang-debug-ci` (CI, suppress developer warnings)
- `windows-clang-debug-tracy-profiler` (только Tracy config changes)
- `linux-clang-debug` (baseline 2026-06-09)
- `linux-clang-debug-build` (только build)
- `linux-clang-debug-tests` (только ctest)
- `linux-clang-release`, `linux-clang-release-build`, `linux-clang-release-tests` (2026-06-14)
- `windows-clang-release`, `windows-clang-release-build`, `windows-clang-release-tests`

**Build presets (6):**
- Каждый покрывает 14-17 ctest executables.

**Test presets (5):**
- Per configure preset.

**Release policy (per `decisions.md §4`):**
- `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only`
- Без `-ffast-math` (ломает Fluid CA determinism + TAA YCoCg clamp)
- Без `-march=native` (portability между CPU)
- Link: `-flto=thin -Wl,--gc-sections`
- **Результат:** ELF **19 MB release vs 73 MB debug** (-73%), +1.5-2.5× FPS.

**Build verification (2026-06-15 baseline):**
- `linux-clang-debug`: 137/137 targets, ctest 14/14, smoke 6/6, **ELF 73 MB**.
- `linux-clang-release`: 137/137 targets, ctest 14/14 (0.06s), smoke 6/6, **ELF 19 MB**.
- VoxelLab reference shot: **500+ FPS, ~2 мс кадр** (debug baseline).

**Compile-time настройки (per `CMakePresets.json` + `decisions.md §4`):**
- **Debug:** `PROJECTV_ENABLE_VALIDATION=ON` (если установлен Vulkan SDK), `PROJECTV_ENABLE_TRACY=ON`, `PROJECTV_ENABLE_RENDERDOC_MARKERS=ON`, `PROJECTV_ENABLE_BENCHMARKS=ON`, `PROJECTV_ENABLE_IMGUI=ON`
- **Release:** все выше = OFF. Оптимизации: `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only -Wl,--gc-sections`
- **Без** `-ffast-math` (ломает Fluid CA determinism + TAA YCoCg clamp)
- **Без** `-march=native` (release binary должен быть переносим между CPU)

**Говорить:**
- «8 debug + 4 release configure presets, 6 build, 5 test».
- «Release: -O3 -flto=thin без -ffast-math без -march=native».
- «ELF 19 MB release vs 73 MB debug (verified 2026-06-15), +1.5-2.5× FPS».

### 3.2. Переменные окружения PROJECTV_*

Полный список env vars проекта:

- `PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK` — воспроизводимая настройка камеры
- `PROJECTV_LOOKDEV_CAPTURE_VIEWS`, `PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES`,
  `PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES`, `PROJECTV_LOOKDEV_CAPTURE_QUIT` — сценарные захваты
- `PROJECTV_SCREENSHOT_DIR` — переопределение директории вывода
- `PROJECTV_MODELS=path.glb@x,y,z;...` — манифест для размещения полигональных моделей
- `PROJECTV_SNAPSHOT_PATH` — загрузка снапшота воксельного мира
- `PROJECTV_ENABLE_VALIDATION` — 1/0, по умолчанию ON в Debug
- `PROJECTV_ENABLE_RENDERDOC_MARKERS` — 1/0, по умолчанию ON в Debug, OFF в `linux-clang-debug`
- `PROJECTV_ENABLE_TRACY` — 1/0, по умолчанию ON
- `PROJECTV_BENCHMARK_FRAMES`, `PROJECTV_BENCHMARK_WARMUP_FRAMES`,
  `PROJECTV_BENCHMARK_LOG_EVERY`, `PROJECTV_BENCHMARK_QUIT` — автоматизация бенчмарков
- `PROJECTV_MUSIC_DIR` — переопределение папки музыки
- `PROJECTV_BUILD_DIR` / `PROJECTV_CMAKE_BUILD_DIR` — для hot shader reload

### 3.3. Политика комментариев и покрытие тестами

**Почему мало комментариев в коде:** `AGENTS.md §10.5`: «DO NOT ADD ANY COMMENTS unless asked». Философия проекта — чистый код через хорошие имена, а не комментарии. Юмор-маркеры `// EVIL:` для магических чисел — отдельное исключение (per `legacy/docs/philosophy/01_foundation/04_*_evil-hacks*.md`). Блоки документации в заголовках — есть (по соглашению Doxygen для публичного API).

**Какой процент покрытия тестами:** ~40-50% по моей оценке. Фокус на критичных путях: ECS-состояние, редактирование материалов вокселей, walk controller, frustum culling, декодирование ассетов. **14 наборов ctest** (per §14 baseline 14/14, 0.78 сек debug, 0.06 сек release). GPU-стороны покрывается визуальными smoke-проверками (RuntimeSmoke 6/6 captures). **80% покрытия** — явный follow-up, не критично для демонстрации архитектуры.

---

## 4. Hotkeys в твоей зоне

- `B` — cycle lighting debug view (для sidecar-метаданных в smoke-captures)
- `C` — capture screenshot (.bmp + .txt sidecar, 60+ ключей)
- `F5` — cycle scene preset (VoxelLab, FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress)
- `F6` — save world snapshot (PVSNAP01, формат для тестов save/load)
- `F7` — load world snapshot
- `1` — hot shader reload (defense r0, 2026-06-15 relocation)

---

## 5. Глоссарий (твоя зона)

**BUILD-TEST** — набор из 14 ctest-тестов, запуск через `ctest --test-dir build/<preset>`.

**CTest** — стандартный тестовый runner из CMake, не требует внешних зависимостей (Google Test не нужен). Тесты регистрируются через `add_test()`.

**CTEST_BASELINE** — все 14 тестов должны быть зелёными (14/14). Если красный — release не считается готовым (`decisions.md §4`).

**SMOKE (smoke-проверки)** — runtime capture-ы эталона, пиксель-в-пиксель. 6 captures: FINAL/SHDW/CSM/CTSH/AOCC/LOCL. Запуск `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh`. Exit 0 = прошло.

**SIDECAR** — текстовый `.txt` рядом с `.bmp`, содержит 60+ ключей метаданных (FPS, frame time, voxel counts, shadow params, TAA state).

**PRESETS (configure-пресеты)** — 12 штук в `CMakePresets.json`: 8 debug (linux-clang-debug, linux-clang-debug-tracy-profiler, windows-clang-debug, windows-clang-debug-ci, windows-clang-debug-tracy-profiler) + 4 release (linux-clang-release-base, linux-clang-release, windows-clang-release-base, windows-clang-release).

**BUILD_PRESETS** — отдельные target-lists: debug-build × 3, release-build × 2, debug-tests/release-tests.

**RELEASE_BIN_SIZE** — 19 MB ELF (vs 73 MB debug, -73%). За счёт ThinLTO + gc-sections + dead code removal.

**THIN_LTO** — `-flto=thin` — параллельный LTO, low memory overhead, совместим с разными CPU.

**GC-SECTIONS** — `-ffunction-sections -fdata-sections -Wl,--gc-sections` — удаление unused секций.

**CMakePresets.json** — корневой файл с 12 configure-пресетами + build-presets + test-presets.

**CLANG 22.1.6** — `clang-22` (Linux Arch), `clang-cl.exe` (Windows). Используется с `-std=c++26` и C++26 modules (`FILE_SET CXX_MODULES`).

**CMAKE 4.0** — минимум для C++26 modules. CMake 4.x на mainline, 3.30+ поддерживается.

**LIBC++ 16** — LLVM C++ Standard Library (на Linux). libstdc++ 16.1 (тоже поддерживается).

**TRACY** — performance profiler (MIT). `PROJECTV_ENABLE_TRACY=ON` (только debug).

**RENDERDOC_MARKERS** — debug-utility метки для RenderDoc. `PROJECTV_ENABLE_RENDERDOC_MARKERS=ON` (только debug).

**VULKAN_VALIDATION_LAYERS** — `PROJECTV_ENABLE_VALIDATION=ON` (только debug, требует установленный Vulkan SDK).

**GOOGLE_BENCHMARK** — `PROJECTV_ENABLE_BENCHMARKS=ON` (только debug). Используется в `src/bench/FrustumCullBenchmark.cpp` и `ShadowProjectionBenchmark.cpp`.

**IMGUI** — `PROJECTV_ENABLE_IMGUI=ON` (только debug). Debug UI.

**LTO (Link-Time Optimization)** — `-flto=thin`. Conservative policy в release (per `decisions.md §4`).

**DEBUG_INFO** — DWARF debug info в debug build, удалён в release.

**NDEBUG** — `NDEBUG` макро для release builds, отключает asserts.

---

## 6. Реалистичные вопросы (5-7)

**Q1. Сколько у вас тестов и сколько они идут?**
- 14 наборов (`ctest -N`), baseline 14/14, debug 0.78 сек, release 0.06 сек.
- Покрывают: математика, инвалидация грязных чанков, walk-контроллер, жадный мешинг, frustum culling, fluid CA, snapshot, scene config, glTF asset loader, Draco decode, meshopt steps, C-kernel, sun shadow splits, math, StringId, C++26 modules, present mode.
- Запускаются при каждой сборке: `cmake --build ... && ctest ...`.

**Q2. Что такое smoke-проверки?**
- Runtime smoke (`tools/linux/Invoke-ProjectVRuntimeSmoke.sh`) — запускает приложение, рендерит 6 эталонных кадров (FINAL / SHDW / CSM / CTSH / AOCC / LOCL) в 1-2 сек wall clock, сравнивает пиксель-в-пиксель с эталоном. Exit code 0 = прошло.
- Управляется env vars: `PROJECTV_START_CAMERA_POSITION`, `*_CAMERA_LOOK`, `*_CAPTURE_VIEWS`, `*_WARMUP_FRAMES`, `*_INTERVAL_FRAMES`, `*_QUIT`.
- Не заменяет unit-тесты, а дополняет их визуальной проверкой (точность попадания теней, корректность TAA, правильность tone mapping).

**Q3. Почему ctest, а не Google Test?**
- ctest — стандартный для CMake, не требует дополнительной интеграции. Тесты пишутся на plain C++ + assertions (или `PV_ASSERT` для soft-проверок), `add_test()` регистрирует в ctest.
- Google Test добавил бы зависимость и не дал бы реального преимущества для нашего use case (тесты быстрые, не нужен mock'и, fixtures — простые struct'ы).

**Q4. Какой release preset и почему?**
- `linux-clang-release` / `windows-clang-release` — оба используют `-O3 -flto=thin` без `-ffast-math` и без `-march=native` (per `decisions.md §4`).
- Conservative: гарантируем детерминизм Fluid CA + TAA YCoCg clamp + переносимость между CPU.
- 19 MB ELF vs 73 MB debug (-73%) за счёт ThinLTO + gc-sections + dead code removal.

**Q5. Сколько пресетов в CMakePresets.json?**
- 12 configure-пресетов: 4 linux-clang-debug* + 4 windows-clang-debug* + 2 linux-clang-release* + 2 windows-clang-release*.
- Плюс build-presets и test-presets (`-build`, `-tests` суффиксы).
- `cmake --list-presets=configure` показывает полный список.

**Q6. Какие версии Clang и CMake?**
- Clang 22.1.6 (`clang-22` пакет на Arch Linux, `clang-cl.exe` на Windows).
- CMake 4.0+ (CMake 4.x на mainline, 3.30+ поддерживается). Нужен для C++26 modules (`FILE_SET CXX_MODULES`).
- libc++ 16 (на Linux). libstdc++ 16.1 (тоже поддерживается).

**Q7. Что не покрыто тестами?**
- ~80% покрытия (явный follow-up per `decisions.md §4`).
- Покрыто: математика, инвалидация грязных чанков, walk-контроллер, meshopt, fluid CA determinism, snapshot, scene config, C-kernel, splits, math, StringId, modules, present mode, asset loader, Draco.
- Не покрыто unit-тестами: рендеринг (заменён runtime smoke), input actions, audio engine (manual), hot shader reload path. Визуальная корректность — через RuntimeSmoke.

---

## 7. Каверзные вопросы (3-5)

**Q8. Что если сломается тест в release-сборке?**
- `ctest --test-dir build/linux-clang-release --output-on-failure` — если какой-то тест красный, **release не считается готовым** per `decisions.md §4`. Все 14 должны быть зелёными.
- Release ускорение: 0.78 сек → 0.06 сек (×13). За счёт `-O3 -flto=thin -DNDEBUG`.
- Если тест flaky в release (зависит от timing'а) — баг в тесте, не в release.

**Q9. Можно ли собрать release без -O3?**
- Технически — да, через `CMAKE_BUILD_TYPE=RelWithDebInfo` или override `CMAKE_CXX_FLAGS_RELEASE`. **Не рекомендуется**: потеряем 95% ctest wall clock (0.78 → ~20 сек debug-like), release perf приближается к debug, но без Tracy/RenderDoc markers. Нет смысла.
- Если нужен профайлер — `linux-clang-debug-tracy-profiler` (projectv WITH Tracy instrumentation, без Tracy UI бинаря).

**Q10. Что такое Google Benchmark и зачем он?**
- `PROJECTV_ENABLE_BENCHMARKS=ON` (только debug presets) — подключает Google Benchmark library.
- `src/bench/FrustumCullBenchmark.cpp` — замеряет C-kernel scalar vs AVX2 vs C++ baseline (Tier 3 audit).
- `src/bench/ShadowProjectionBenchmark.cpp` — замеряет sun shadow projection.
- В release `PROJECTV_ENABLE_BENCHMARKS=OFF` — не нужен production binary, не тащим лишний код.

**Q11. Какие 6 captures в smoke и зачем?**
- `FINAL` — composite кадр (все эффекты применены). Главный эталон.
- `SHDW` — только тени (cascade shadows). Проверяет корректность CSM.
- `CSM` — визуализация каскадов (per-cascade split planes).
- `CTSH` — контактные тени (sun-to-fragment ray).
- `AOCC` — фоновое затенение полостей (12 traces per fragment).
- `LOCL` — локальный точечный свет (per-preset).
- Если хоть один capture не совпадает — значит визуальный регресс.

**Q12. Как добавить новый test executable?**
- В `tests/CMakeLists.txt` добавить `add_executable(ProjectVNewTest NewTest.cpp)`, `target_link_libraries(ProjectVNewTest PRIVATE ProjectVLib)` (или аналог), `add_test(NAME ProjectVNewTest COMMAND ProjectVNewTest)`.
- Затем **обновить все 5 buildPresets** (`*-debug-build` × 3 + `*-release-build` × 2) — добавить `ProjectVNewTest` в targets list.
- Альтернатива (per `decisions.md §4` follow-up) — INTERFACE target в `tests/CMakeLists.txt` который зависит от всех test executables, тогда buildPresets ссылаются только на него. Не сделано (out of scope).

---

## 8. Хронология (релевантные события)

**2026-04-13 (Tier 1.B):** `std::expected<T, E>` migration на холодных путях. VulkanInit (16 variants), snapshot (3 variants), audio load (3 variants), scene config, ECS sync, physics state. Позволяет error enum вместо bool+per-step лога.

**2026-04-14 (Release presets r0):** commits `6fe9201`. linux-clang-release / windows-clang-release. Conservative policy: `-O3 -flto=thin -DNDEBUG`. Без `-ffast-math`, без `-march=native`. 19 MB ELF vs 73 MB debug (-73%).

**2026-04-14 (Build config audit r0):** 5 buildPresets обновлены. linux-clang-debug-tracy-profiler `PROJECTV_BUILD_TRACY_PROFILER ON→OFF` (Linux Tracy UI не собирается). 14/14 ctest на release.

**2026-04-15 (Windows build verification r0):** 5 atomic-commits. P0 libc++/Windows-clang-cl gating + Tracy UI split + RepoRoot extract + docs/cleanup + deinit 5 submodules 62M. commit `69b1726`.

---

## 9. Out of scope (Q&A redirect)

| Вопрос про… | Говори |
|---|---|
| C++26 / Vulkan 1.4 / DOD / SIMD / C-ядра / std::expected | «К T2 (le1t)» |
| Демо / FPS / HUD / сцена VoxelLab / стек | «К T2 (le1t)» |
| Воксельный мир / чанки / meshing / статик-ассерты / fluid CA / snapshot | «К T3» |
| Рендеринг / Vulkan / TAA / CSM / AOCC / шейдеры / C-ядро | «К T4» |
| Физика / walk-контроллер / Jolt / edge grace | «К T6» |
| Ассеты / аудио / glTF / Draco / meshopt / miniaudio / hot reload | «К T5» |
| BUG-005 cycle scene race / баги / known issues | «К T2 (le1t)» |
| Hot shader reload (клавиша 1) / ray-march (клавиша 2) | «К T2 (le1t)» |
| Phase 4-9 / roadmap / планы | «К T6» |
