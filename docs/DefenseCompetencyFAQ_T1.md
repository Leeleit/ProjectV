# Defense Competency FAQ — Тиммейт 1 (Сборка и тестирование)

**Участник:** [Имя Тимейта 1]
**Реальная компетенция:** Сборка и тестирование (CMake, ctest, runtime smoke, presets)
**Speech slot на сцене:** T1 Вступление и проблема (0:00-0:45)
**Verbatim текст выступления:** `docs/DefenseScript_Team.md` → раздел «Участник 1 (Вступление и Проблема)»

**Out of scope (к кому перенаправлять в Q&A):** архитектура/стек — к le1t; воксельный мир — к Тиммейту 2; рендеринг — к Тиммейту 3; физика — к Тиммейту 4; ассеты/аудио — к Тиммейту 5; все баги — к le1t.

**Common (стек, метрики, хоткеи, glossary, chronology):** `docs/DefenseCompetencyFAQ.md`

---

## 1.1. Кто ты

**Легенда:** ты отвечал за сборку проекта и инфраструктуру тестирования. CMake пресеты, ctest 14 наборов, runtime smoke 6 captures, бенчмарки, документация. Ты НЕ отвечаешь за архитектуру, воксели, рендеринг, физику или ассеты — это другие тиммейты.

**На сцене:** ты говоришь T1 (Вступление и проблема) — это про проект в целом, не про твою зону.

**На Q&A:** ты отвечаешь на вопросы про **сборку, тесты, метрики, пресеты**.

## 1.2. Твоя компетенция: Сборка и тестирование

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

**Compile-time настройки (per `CMakePresets.json` + `decisions.md §4`):**
- **Debug:** `PROJECTV_ENABLE_VALIDATION=ON` (если установлен Vulkan SDK), `PROJECTV_ENABLE_TRACY=ON`, `PROJECTV_ENABLE_RENDERDOC_MARKERS=ON`, `PROJECTV_ENABLE_BENCHMARKS=ON`, `PROJECTV_ENABLE_IMGUI=ON`
- **Release:** все выше = OFF. Оптимизации: `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only -Wl,--gc-sections`
- **Без** `-ffast-math` (ломает Fluid CA determinism + TAA YCoCg clamp)
- **Без** `-march=native` (release binary должен быть переносим между CPU)

## 1.3. Что смотреть на защите

**Слайды 1-2** (твои) — титульный + проблема. Говори глядя на аудиторию, не в слайд.

**Демо во время T2 (le1t)** — не твоя зона, но знай где искать:
- `build/linux-clang-debug/lookdev-captures/` — 6 эталонных capture'ов (FINAL/SHDW/CSM/CTSH/AOCC/LOCL)
- `ctest --test-dir build/linux-clang-debug` — 14/14 tests
- HUD на экране: `FPS`, `CHUNKS: 27`, `DRAW CALLS`, etc.

## 1.4. Реалистичные вопросы комиссии (5-7)

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

## 1.5. Каверзные вопросы (3-5)

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

## 1.6. Куда перенаправить (Out of scope)

| Вопрос про… | Говори |
|---|---|
| C++26 / Vulkan 1.4 / DOD / SIMD / C-ядра / std::expected | «Архитектурное решение — к le1t» |
| Демо / FPS / HUD / сцена VoxelLab / стек | «К le1t» |
| Воксельный мир / чанки / meshing / статик-ассерты / fluid CA / snapshot | «К Тиммейту 2» |
| Рендеринг / Vulkan / TAA / CSM / AOCC / шейдеры / C-ядро | «К Тиммейту 3» |
| Физика / walk-контроллер / Jolt / edge grace | «К Тиммейту 4» |
| Ассеты / аудио / glTF / Draco / meshopt / miniaudio / hot reload | «К Тиммейту 5» |
| BUG-005 cycle scene race / баги / known issues | «К le1t» |
| Hot shader reload (клавиша 1) / ray-march (клавиша 2) | «К le1t» |
| Phase 4-9 / roadmap / планы | «К Тиммейту 4 (он закрывает)» |
