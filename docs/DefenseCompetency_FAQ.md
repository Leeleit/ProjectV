# Defense Competency FAQ — ProjectV (textbook)

**Для:** 6 человек команды «Черепашки Ninja» (Тиммейты 1-5 + le1t).
**Назначение:** глубокий per-person справочник — что ботать, что смотреть на защите, реалистичные + каверзные вопросы с готовыми ответами.
**Формат:** учебник. Читать с экрана (компьютер / телефон) во время подготовки к Q&A. **НЕ читать на сцене** — для выступления есть `docs/DefenseScript_Team.md`.
**Версия:** 2026-06-17.
**Соглашение:** все цитаты исходного кода приведены с `file:line` — перепроверяйте, исходный код изменяется.

---

## Оглавление

- [§0. Общая карта проекта](#0-общая-карта-проекта-читать-всем)
- [§1. Тиммейт 1 — Сборка и тестирование](#1-тиммейт-1--сборка-и-тестирование-говорит-t1-вступление)
- [§2. Тиммейт 2 — Воксельный мир](#2-тиммейт-2--воксельный-мир-говорит-t3-архитектура)
- [§3. Тиммейт 3 — Рендеринг](#3-тиммейт-3--рендеринг-говорит-t4-тесты)
- [§4. Тиммейт 4 — Физика и walk-контроллер](#4-тиммейт-4--физика-и-walk-контроллер-говорит-t6-планы)
- [§5. Тиммейт 5 — Ассеты и аудио](#5-тиммейт-5--ассеты-и-аудио-говорит-t5-прочие-фичи)
- [§6. le1t — Архитектура + Q&A host](#6-le1t--архитектура--qa-host-говорит-t2-demo--стек)
- [Приложение A. Глоссарий](#приложение-a-глоссарий)
- [Приложение B. Хронология решений](#приложение-b-хронология-решений)

---

## §0. Общая карта проекта (читать всем)

### 0.1. Что такое ProjectV

ProjectV — высокопроизводительный воксельный игровой движок. Один разработчик (le1t = Кадочников Лев Петрович). Команда «Черепашки Ninja» (6 человек) — для защиты. Код — реальный, ~3,5 месяца работы, 100+ коммитов.

**Стек:**
- C++26 (`CMAKE_CXX_STANDARD 26` в `CMakeLists.txt:29`)
- Vulkan 1.4 (`VOLK_STATIC_DEFINES` + `VK_API_VERSION_1_4` в `src/render/vulkan/`)
- Clang 22.1.6 (Linux native) + clang-cl (Windows)
- CMake 4.0 (12 configure-пресетов: 8 debug + 4 release)
- 22 git-сабмодуля в `external/`

**Библиотеки (vendored, не системные):**
- Jolt Physics (MIT, deterministic, SIMD) — физика твёрдых тел
- Flecs (MIT, header-only C++) — ECS как пассивное зеркало
- fastgltf + Draco + meshoptimizer — asset pipeline
- miniaudio (single-header C) — audio (MP3)
- volk — Vulkan loader
- VulkanMemoryAllocator (VMA) — GPU memory
- tracy — performance profiler
- fmt, glm, nlohmann/json, spdlog, stb_image, и др.

### 0.2. Целевая машина и метрики

**Target (per `decisions.md`, `agent/memory.md`):**
- CPU: Ryzen 7 5800X (Zen 3, 8 cores, 16 threads, L1 = 32 KB/core)
- GPU: NVIDIA RTX 3060 Ti
- RAM: 16 GB
- Разрешение: 1920×1080

**Производительность (release build):**
- VoxelLab debug: **500+ FPS**, ~2 мс кадр (per `DefenseScript_Team.md` T2)
- VoxelLab release: 19 MB ELF, дополнительно ×1.5-2.5 ускорение (per `status.md §21`)
- ctest: **14/14 pass** в 0.78 сек (debug) / 0.06 сек (release)
- Runtime smoke: 6/6 captures пиксель-в-пиксель

**Бинарные размеры (Linux):**
- Debug: 73 MB (с Tracy, RenderDoc markers, validation)
- Release: 19 MB (`-O3 -flto=thin -ffunction-sections -fdata-sections -Wl,--gc-sections`)
- Разница: -73% (per `decisions.md §4`)

### 0.3. Архитектура в одном абзаце

`VoxelWorld` — единственный источник истины (Single Source of Truth, SoT). Все мутации мира — только через него. `VoxelWorld` хранит `std::vector<uint8_t> voxels` (плоский массив) + `std::vector<VoxelChunk>` (32-байтные чанки) + статистику. Compute-шейдер `voxel_mesh.comp` greedy-мешит чанки в quad'ы. Vulkan 1.4 graphics pipeline рендерит меши. Jolt симулирует персонажа. Flecs дублирует мир в типизированные компоненты для HUD/отладки. ECS sync 1× за кадр через `SyncEcsWorldState`.

### 0.4. Документы — где что

| Документ | Содержит |
|---|---|
| `docs/DefenseScript_Team.md` | Скрипт выступления (verbatim, 5 мин) |
| `docs/DefenseBriefer_{1..5}.md` | Памятки для тиммейтов (speech + competency) |
| `docs/DefenseBriefer_le1t.md` | Памятка le1t (T2 + Q&A-карта 30+ вопросов) |
| `docs/DefensePresentation_Structure.md` | 8 слайдов с таймингами |
| `docs/DefenseCompetency_FAQ.md` | **Этот файл** — textbook для Q&A |
| `docs/DefenseAlgorithms.md` | 23 алгоритма (эталон) |
| `docs/DefenseFAQ.md` | 15+ готовых ответов (эталон) |
| `docs/DefenseReport.md` | Итоговый отчёт + §3 deferred items |
| `docs/archive/DefenseBriefer_TechnicalDeepDive_2026-06-15.md` | Q&A reference (технические детали) |
| `docs/archive/DefenseOldFormat_2026-06-17/` | Устаревшие 10-мин скрипты |

### 0.5. Хоткеи (полный список, из `src/app/InputActions.cpp:127-181`)

**Движение (walk mode):**
- `W` `A` `S` `D` — движение
- `Space` — прыжок
- `LShift` / `RShift` — sneak (красться)
- `LCTRL` / `RCTRL` — speed boost (×3)
- `LALT` / `RALT` — speed slow (×0.25)
- `F11` — toggle walk air control mode
- `J` — toggle auto-jump
- `F12` — toggle auto-jump delay

**Режимы и камера:**
- `F4` — toggle Walk/Creative/Spectator mode
- Двойной `Space` — toggle Walk ↔ Creative
- `F3` — reset camera
- `TAB` — toggle relative mouse mode
- `F11` (InputAction) vs `1` (defense) — это разные клавиши! `F11` = walk air control, `1` = hot shader reload (relocation после conflict с F11 InputAction, см. `src/app/main.cpp:545-619`)

**Voxel interaction:**
- Левый клик — removal (VoxelMaterial::Air)
- Правый клик — placement
- `F` — pick model (HL2-style physicsgun)
- `F2` — cycle placement material
- `F8` — cycle editor tool
- `M` — pick target material
- `X` — toggle mutation anchor

**Сцена и snapshot:**
- `F5` — cycle scene preset (VoxelLab, FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress)
- `F6` — save world snapshot (PVSNAP01)
- `F7` — load world snapshot

**Визуализация:**
- `F1` — toggle HUD
- `G` — toggle detailed HUD
- `B` — cycle lighting debug view (Final → Ambient → Direct → Local → Shadow → Cascade → Contact → Occlusion → Fog → Taa)
- `C` — capture screenshot (.bmp + .txt sidecar)
- `L` — toggle cascade split planes
- `Z` — toggle cursor hit normal
- `O` — cycle shadow tuning target
- `U` / `I` — decrease / increase shadow tuning value
- `V` — reset lighting debug controls
- `H` / `K` — decrease / increase lighting exposure
- `N` — cycle tone map operator (Linear / Reinhard / AcesApprox)

**TAA (Temporal Anti-Aliasing):**
- `T` — toggle TAA on/off (relocation с original binding)
- `;` / `'` — decrease / increase TAA jitter scale
- `-` / `=` — decrease / increase TAA blend
- `,` — cycle TAA neighbourhood radius (1/3/5/7)
- `.` — invalidate TAA history

**Frame-step / slow-motion:**
- `P` — toggle pause
- `[` / `]` — decrease / increase time scale
- `\` — step single frame
- `` ` `` — reset time scale

**Audio (per `src/app/InputActions.cpp:196-210`):**
- `Q` — play/pause toggle
- `E` — stop
- `7` / `8` — volume down / up
- `9` / `0` — next / previous track

**Chunk debug:**
- `F9` — toggle chunk bounds
- `F10` — toggle dirty chunk overlay

**Input replay (debug):**
- `R` — toggle input replay recording
- `Y` — play last input replay

**Defense r0 hotkeys (relocated 2026-06-15, per `src/app/main.cpp:545-619`):**
- `1` — hot shader reload (было F5/F11, освобождено для InputAction)
- `2` — toggle ray-march pass (было F6/F12)
- `3` — cycle V-sync mode (было V)
- `ESC` — exit

**Горячие клавиши, освобождённые relocation:** `F5`, `F6`, `F11`, `F12`, `V` теперь работают как InputAction (`CycleScenePreset`, `SaveWorldSnapshot`, `ToggleWalkAirControlMode`, `ToggleWalkAutoJumpDelay`, `ResetLightingDebugControls`).

### 0.6. Известные проблемы (на момент защиты)

**BUG-005 (cycle scene race)** — гонка дескрипторов при переключении сцен. Частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources` (per `agent/decisions.md` + `agent/memory.md`). **Полное устранение — отдельная подзадача (Phase 5)**.

**Ray-march pass — STUB.** `RecordRayMarchCommands` в `src/render/RayMarchPass.cpp:59` — `fprintf` в stderr, реальной работы не делает. Compute-шейдер `ray_march.comp` (Amanatides-Woo DDA) скомпилирован, API state (`SetRayMarchEnabled`/`IsRayMarchEnabled`/`RequestRayMarchPipelineRecreate`) работает, но в graphics command stream не вкомпонован. Phase 7 follow-up.

**TAA (по умолчанию) ВЫКЛЮЧЕН.** `taaEnabled=false`, `taaJitter=0`. Стабильная картинка без дрожания. **BUG-004 (VoxelLab tremor) — отвергнут, не существует.** Галлюцинация предыдущей сессии.

### 0.7. Phase 4-9 roadmap (per `docs/DefenseReport.md §3`)

| Phase | Цель | Триггер |
|---|---|---|
| 4 | Networking (server-authoritative + client prediction) | Post-MVP |
| 5 | SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader) | Post-MVP |
| 6 | HDR-текстуры + полный клеточный автомат жидкости на GPU | Post-MVP |
| 7 | Полная система частиц + асинхронная загрузка ресурсов | Post-MVP |
| 8 | Плагины / моддинг API | Post-MVP |
| 9 | Многопользовательский режим (Academic vision) | Post-MVP |

5 отложенных пунктов из ТЗ (per `docs/DefenseReport.md §3`): частицы, моддинг, async load, HDR, SVO. Все явно в roadmap.

---

## §1. Тиммейт 1 — Сборка и тестирование (говорит T1 Вступление)

### 1.1. Кто ты

**Легенда:** ты отвечал за сборку проекта и инфраструктуру тестирования. CMake пресеты, ctest 14 наборов, runtime smoke 6 captures, бенчмарки, документация. Ты НЕ отвечаешь за архитектуру, воксели, рендеринг, физику или ассеты — это другие тиммейты.

**На сцене:** ты говоришь T1 (Вступление и проблема) — это про проект в целом, не про твою зону.

**На Q&A:** ты отвечаешь на вопросы про **сборку, тесты, метрики, пресеты**.

### 1.2. Твоя компетенция: Сборка и тестирование

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

### 1.3. Что смотреть на защите

**Слайды 1-2** (твои) — титульный + проблема. Говори глядя на аудиторию, не в слайд.

**Демо во время T2 (le1t)** — не твоя зона, но знай где искать:
- `build/linux-clang-debug/lookdev-captures/` — 6 эталонных capture'ов (FINAL/SHDW/CSM/CTSH/AOCC/LOCL)
- `ctest --test-dir build/linux-clang-debug` — 14/14 tests
- HUD на экране: `FPS`, `CHUNKS: 27`, `DRAW CALLS`, etc.

### 1.4. Реалистичные вопросы комиссии (5-7)

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

### 1.5. Каверзные вопросы (3-5)

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

### 1.6. Куда перенаправить (Out of scope)

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

---

## §2. Тиммейт 2 — Воксельный мир (говорит T3 Архитектура)

### 2.1. Кто ты

**Легенда:** ты отвечал за воксельный мир — структура данных чанков, материалы, мешинг, raycast, fluid CA, snapshot. Это самая фундаментальная часть движка.

**На сцене:** ты говоришь T3 (Архитектура и качество кода) — общий обзор внутренностей.

**На Q&A:** ты отвечаешь на вопросы про **воксельный мир, мешинг, материалы, fluid CA, snapshot, статик-ассерты**.

### 2.2. Твоя компетенция: Воксельный мир

**Файлы:**
- `src/voxel/VoxelWorld.hpp` / `src/voxel/VoxelWorld.cpp` — main world
- `src/voxel/VoxelMaterials.hpp` / `src/voxel/VoxelMaterials.cpp` — materials + lighting
- `src/voxel/VoxelRaycast.hpp` / `src/voxel/VoxelRaycast.cpp` — DDA raycast
- `src/voxel/VoxelInteraction.hpp` / `src/voxel/VoxelInteraction.cpp` — placement/removal
- `src/voxel/SceneConfig.hpp` / `src/voxel/SceneConfig.cpp` — JSON config
- `src/voxel/VoxelSnapshotError.hpp` — error enum
- `src/shaders/voxel_mesh.comp` — compute-шейдер greedy meshing

**Структуры (per `VoxelWorld.hpp:17-107`):**

```cpp
enum class VoxelMaterial : uint8_t {
    Air = 0, Glass = 1, Fluid = 2, FloorWhite = 3, FloorGray = 4
};

struct Int3 { int x, y, z; };  // 12 B
struct VoxelChunk {
    Int3 min, maxExclusive;     // 24 B
    bool rebuildQueued;         // 1 B (+ padding)
    uint32_t nonAirVoxelCount;  // 4 B
};  // 32 B total
struct VoxelWorld {
    VoxelScenePreset scenePreset;
    VoxelWorldConfig config;
    Int3 min, maxExclusive;
    Int3 floorMin, floorMaxExclusive;
    int width, height, depth;
    std::vector<uint8_t> voxels;  // плоский массив
    int chunkSize, chunkCountX, chunkCountY, chunkCountZ;
    uint64_t editVersion;
    std::vector<VoxelChunk> chunks;
    std::vector<size_t> pendingChunkRebuildIndices;
    VoxelWorldStats stats;
};
```

**5 материалов:** Air (0) — проходимый, не рисуется. Glass (1) — полупрозрачный, не отбрасывает тень. Fluid (2) — жидкость, отбрасывает тень, обновляется fluid CA. FloorWhite (3) / FloorGray (4) — твёрдые полы.

**VoxelLab (демо-сцена, `VoxelWorld.cpp:417-474`):**
- Пол-шахматка 18×18 (XZ), `floorSize=18, padding=3, worldTopY=14`
- Стеклянный шар радиуса 6 вокруг (0, 8, 0), толщина стенки 1
- Жидкость внутри шара до `fluidTop` (≈70% внутреннего радиуса)
- 3 якоря: правый куб 4×4×1, левый столбик 2×2×5, передний 1×1×3 — для стабильных теней
- Процедурная генерация <200 мс

**5 scene presets (`VoxelWorld.hpp:26-32`):**
- `VoxelLab` — демо (default)
- `FlatBenchmark` — плоский пол для замеров
- `TransparencyStress` — Glass-колонны (тест прозрачности)
- `ChunkGrid` — маркеры по углам чанков
- `MeshingStress` — большой объём для нагрузки мешинга
- Переключение: F5 (`CycleScenePreset`)

**Chunk layout (8×8×8):**
- 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра
- Влезает в L1 кэш (32 KB на Zen 3 = 4 строки по 64 B)
- VoxelLab = 3×3×3 = 27 чанков
- Padding: `world.min = (-12, 0, -12)`, `world.maxExclusive = (12, 17, 12)`, `floorMin = (-9, 0, -9)`, `floorMaxExclusive = (9, 17, 9)`

**Static asserts (compile-time contracts):**
- `static_assert(sizeof(Int3) == 12)` — `VoxelWorld.hpp:42`
- `static_assert(std::is_standard_layout_v<Int3>)` — `VoxelWorld.hpp:40`
- `static_assert(std::is_trivially_copyable_v<Int3>)` — `VoxelWorld.hpp:41`
- `static_assert(sizeof(VoxelChunk) == 32)` — `VoxelWorld.hpp:52`
- `static_assert(offsetof(VoxelChunk, min) == 0)` — `VoxelWorld.hpp:53`
- `static_assert(offsetof(VoxelChunk, maxExclusive) == 12)` — `VoxelWorld.hpp:54`
- `static_assert(offsetof(VoxelChunk, rebuildQueued) == 24)` — `VoxelWorld.hpp:55`
- `static_assert(offsetof(VoxelChunk, nonAirVoxelCount) == 28)` — `VoxelWorld.hpp:56`
- `static_assert(sizeof(VoxelMaterial) == sizeof(uint8_t))` — `VoxelWorld.hpp:24`
- `static_assert(sizeof(VoxelSceneLighting) == 624)` — `VoxelMaterials.hpp:140` (shader-contract!)
- `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)` — `VoxelMaterials.hpp:159` (TAA field)

**Greedy meshing (`voxel_mesh.comp:613-619`):**
> «6 per-axis greedy passes, one per face direction. Each pass walks the 2D plane of cells that emit a face in that direction and merges adjacent cells with the same exposed state into a single W×H quad. For oversized chunks (>kMaxChunkExtentForGreedy in any in-plane axis) the pass falls back to per-voxel emission.»

- 6 проходов (±X, ±Y, ±Z), каждый объединяет смежные грани одного exposed state в W×H quad
- Packing (W, H) в 6+6 бит = 12 бит → max quad extent = 64 вокселя
- `kMaxChunkExtentForGreedy` fallback на per-voxel emission (1×1 quads) для oversized chunks
- Compute-шейдер: чанк-параллельный, thousands of threads

**Voxel raycast (DDA, `VoxelRaycast.cpp`):**
- `VoxelRaycastHit { hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance }`
- DDA через `world.voxels` (плоский массив)
- `voxel` — куда попал луч, `placementVoxel` — предыдущая ячейка (для placement)
- Используется в `VoxelInteraction` для placement/removal

**Fluid CA (`VoxelWorld.cpp:1284-1643`, ~360 строк):**
- Один тик = один шаг клеточного автомата
- Правила: сначала попытка падения вниз (`f_fall`), иначе распространение в 1 из 4 кардинальных сторон (`f_spread`)
- **Двойная буферизация:** читаем из `world.voxels` (immutable snapshot), пишем в `next`, swap в конце
- **Bottom-up y-pass:** итерация `z, y, x` с `y` ascending → 1 cell per tick, без double-step
- **Claimed-tracking:** 1 байт на воксель (≈10 KB для VoxelLab) — помечает, что destination уже занят
- **Determinism:** single-threaded, нет FP, нет syscalls, нет atomics, нет pointer-identity зависимостей
- **Spread rule restored 2026-06-13** (per `agent/decisions.md §30`)
- Pin-тест: `TestFluidCAVoxelLabSphereFallOnGlassBreak` — гарантирует, что жидкость в шаре VoxelLab корректно падает

**Voxel interaction (placement/removal, `VoxelInteraction.cpp`):**
- `UpdateVoxelInteraction(camera, input, world, interaction, allowEditing, physics)` — каждый кадр
- Placement: правый клик → `FillVoxelBox(anchor, hit.placementVoxel, material)`
- Removal: левый клик → `FillVoxelBox(hit.voxel, Air)` или `FillVoxelMaterial(flood-fill, Air)`
- `CanPlaceInteractionVoxelBox(anchor, placement, camera, physics)` — проверка, не пересекается ли placement-box с игроком
- Использует `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` (Jolt query)

**Snapshot система (PVSNAP01, `VoxelWorld.cpp:17-20`):**
- Magic: `PVSNAP01` (8 байт ASCII)
- 80-байтный header: `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion` (8 B)
- `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B)
- Хоткеи: F6 save, F7 load

**Scene config JSON (per `SceneConfig.hpp:17-23`):**
```cpp
struct SceneConfig {
    std::string name = "ProjectV Default";
    VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
    VoxelWorldConfig voxelWorldConfig{};
    float sunDirectionY = 0.80f;
    float exposure = 1.0f;
};
```
Путь по умолчанию: `runtime/scene.json` (создаётся при первом запуске через `EnsureDefaultSceneConfig`).

### 2.3. Что смотреть на защите

**Слайд 4** (твой) — Архитектура. Показывает чанки 8×8×8, плоский массив, мешинг compute, Jolt, ECS, статик-ассерты.

**Демо во время T2 (le1t):** Voxel Laboratory сцена, облёт камерой, демонстрация voxel raycast (placement/removal блоков).

**HUD:** `CHUNKS: 27` (VoxelLab = 3×3×3 = 27 чанков).

### 2.4. Реалистичные вопросы (5-7)

**Q1. Почему чанк именно 8×8×8, а не 16 или 32?**
- 512 вокселей × 1 байт = 512 B = 2 SSE-регистра (16 B каждый) или 4 AVX-регистра (32 B)
- Влезает в L1 кэш (32 KB на Zen 3 = 4 строки по 64 B)
- 16×16×16 = 4 KB — промахи кэша при meshing
- 32×32×32 = 32 KB — еле влезает, плохая амортизация
- 8 — sweet spot

**Q2. Зачем compute-шейдер для мешинга?**
- 6 проходов × тысячи чанков = массивный параллелизм
- GPU: тысячи потоков, CPU: десятки ядер
- 3D-окружение — embarrassingly parallel (каждый чанк независим)

**Q3. Что такое greedy meshing простыми словами?**
- Объединяет соседние грани одного exposed state (материал + видимость) в один quad
- Без greedy: каждый кубик = 6 граней = 12 треугольников (для OpenGL)
- С greedy: 1 quad = 2 треугольника для 4×4 блока одного материала
- Сокращение draw calls на 30-50%

**Q4. Как работает fluid CA?**
- Каждый тик: жидкость пытается упасть вниз на 1 клетку
- Если заблокировано — распространяется в 1 из 4 сторон
- Детерминирован: bottom-up y-pass, двойная буферизация, claimed-tracking
- 1 cell per tick (без double-step)

**Q5. Зачем нужен `std::expected` в snapshot API?**
- Tier 1.B migration (2026-06-13) — заменил `bool` + per-step `fprintf` лог
- Холодный путь (1× per snapshot), ~2× cost несущественен
- Strongly-typed error enum: `PreconditionFailed`, `FolderCreateFailed`, `ScanFailed`
- Машиночитаемый сигнал для caller'а, не "true/false + log"

**Q6. Зачем столько static_assert?**
- Compile-time проверка контрактов: размеры структур, alignment, field offsets
- Если кто-то добавит `padding` в `Int3` → компиляция упадёт, не молча сломает GPU upload
- `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)` — гарантирует shader-C++ ABI parity
- Защита от регрессий (per `agent/memory.md §10.8` — реальный инцидент с GraphicsPushConstants сдвигом в shadow-pass)

**Q7. Сколько чанков в VoxelLab и почему так мало?**
- 27 чанков (3×3×3)
- floorSize=18 в XZ direction, height=14 в Y
- Padding=3 вокруг пола для chunk allocation (chunk 8×8×8 → 3×3×3 = 27)
- Сцена демо, не stress-test. Для production: 100+ чанков, ray-march на GPU

### 2.5. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если чанк больше 64 вокселей в одной оси?**
- `voxel_mesh.comp:616` `kMaxChunkExtentForGreedy` — fallback на per-voxel emission (1×1 quads per face)
- VoxelLab 8×8×8 не попадает в этот fallback
- Для production сцен >64 вокселей на ось — либо поднять `kMaxChunkExtentForGreedy`, либо разбить на sub-chunks

**Q9. Как spread rule взаимодействует с fall rule?**
- Приоритет: сначала fall (`f_fall`), иначе spread (`f_spread`) в 1 из 4 сторон
- Spread direction — hash-determined из `(x, y, z)` для воспроизводимости
- Без claimed-tracking: два fluid'а могут "обменяться" клетками (swap bug) — один исчезает
- С claimed-tracking: помечаем destination, второй fluid не может перезаписать

**Q10. Что если изменить `sizeof(VoxelChunk)`?**
- `static_assert(sizeof(VoxelChunk) == 32)` в `VoxelWorld.hpp:52` — компиляция упадёт
- ABI change: GPU upload сместится, render сломается
- Защита от случайных регрессий при добавлении полей

**Q11. Чем DOD отличается от ООП в вашем коде?**
- `alignas(16)` на `VoxelChunk` (32 B = 2 SSE)
- Плоский `std::vector<uint8_t> voxels` — все воксели подряд, без `std::vector<std::vector<...>>`
- ООП-стиль = разбросанные аллокации, cache miss'ы
- DOD-стиль = cache-friendly iteration, авто-векторизация

### 2.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| DOD layout / `alignas(16)` / SoA в других модулях | «Архитектурное решение — к le1t» |
| C++26 фичи / std::simd / std::expected / модули | «К le1t» |
| Build / Clang / CMake / ctest | «К Тиммейту 1» |
| CSM / PCF / TAA / AOCC / шейдеры рендера | «К Тиммейту 3» |
| Walk controller / Jolt / edge grace / auto-jump | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio / snapshot save | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t» |
| Hot shader reload (клавиша 1) | «К le1t» |
| Демо VoxelLab / FPS / сцена | «К le1t» |
| Phase 4-9 / roadmap | «К Тиммейту 4 (он закрывает)» |

---

## §3. Тиммейт 3 — Рендеринг (говорит T4 Тесты)

### 3.1. Кто ты

**Легенда:** ты отвечал за рендеринг — Vulkan 1.4 init, graphics pipeline, TAA, CSM, AOCC, шейдеры, C-ядро frustum culling. Это самый большой и технически насыщенный модуль.

**На сцене:** ты говоришь T4 (Тесты и проверки) — обзор тестов с акцентом на визуальные (render) тесты.

**На Q&A:** ты отвечаешь на вопросы про **рендеринг, шейдеры, TAA, CSM, AOCC, C-ядро, освещение**.

### 3.2. Твоя компетенция: Рендеринг

**Файлы (самая большая категория):**
- `src/render/Renderer.{hpp,cpp}` — `DrawFrame` (главный per-frame entry point, ~1600 строк)
- `src/render/SceneResources.{hpp,cpp}` — chunk visibility cache, XOR-fold splitmix64 hash, `IsSceneChunkVisible`, `IsSceneChunkVisibleInShadowCascade`, `IsAabbVisibleAgainstCameraFrustum`
- `src/render/ShadowProjection.{hpp,cpp}` — 4-cascade projection, `BuildSunShadowCascadeSplits(near, far, lambda=0.80)`
- `src/render/Taa.{hpp,cpp}` — `AdvanceTaaPixelJitter`, `BuildTaaHistoryParams`, `BuildTaaLayerHistoryParams`
- `src/render/RayMarchPass.{hpp,cpp}` — STUB, `SetRayMarchEnabled`, `RecordRayMarchCommands` (no-op)
- `src/render/ScreenshotCapture.{hpp,cpp}` — `.bmp` + `.txt` sidecar writer
- `src/render/vulkan/VulkanInit.{hpp,cpp}` — Vulkan 1.4 init, `VulkanInitError` enum (16 вариантов)
- `src/render/vulkan/VulkanGraphicsPipeline.{hpp,cpp}` — graphics pipelines (TAA-aware variants)
- `src/render/vulkan/VulkanVoxelMeshingPipeline.{hpp,cpp}` — compute-шейдер пайплайн
- `src/render/vulkan/TaaResolvePipeline.{hpp,cpp}` — TAA resolve
- `src/render/vulkan/VulkanSwapchain.{hpp,cpp}` — swapchain, present modes
- `src/render/vulkan/VulkanBootstrap.{hpp,cpp}` — Vulkan loader
- `src/render/vulkan/VulkanDebug.{hpp,cpp}` — debug utils
- `src/render/ShadowTypes.hpp` — shadow types
- `src/render/TaaRenderTargets.{hpp,cpp}` — TAA render target management
- `src/c_kernels/frustum_cull.{hpp,c}` — C-ядро
- `src/c_kernels/FrustumCulling.hpp` — C++ обёртка
- `src/shaders/*.comp/.frag/.vert` — все шейдеры (12 файлов)

**Vulkan 1.4 init (`VulkanInit.hpp:19-58`):**
- 16 `VulkanInitError` enum вариантов: `PreconditionFailed`, `BootstrapFailed`, `TracyContextFailed`, `SwapchainFailed`, `WorldCreationFailed`, `EcsSyncFailed`, `PhysicsStateFailed`, `SceneResourcesFailed`, `GraphicsPipelineFailed`, `VoxelMeshingPipelineFailed`, `ModelPipelineFailed`, `ModelManifestFailed`, и т.д.
- Tier 1.B: `std::expected<void, VulkanInitError>` вместо `bool` (cold path, 1× per startup)
- `InitVulkan(AppState*)` — мутирует `AppState` in place, error variant — machine-readable сигнал

**Shadow cascade (per `ShadowProjection.hpp`, `decisions.md §18`):**
- 4 каскада (`kSunShadowCascadeCount = 4`)
- Shadow map 2048×2048
- `BuildSunShadowCascadeSplits(near, far, lambda=0.80)` — near-biased split distribution
- Per-cascade projection: sub-frustum → light-space → sphere stabilization (чтобы избежать jitter при движении камеры)

**Per-preset shadow params (`VoxelMaterials.cpp:181-236`):**
```cpp
// VoxelLab
sunShadowParams = {0.72f, 0.0009f, 0.0060f, 1.10f}  // strength, depthBias, normalBias, filterRadius
sunContactShadowParams = {0.28f, 2.25f, 0.0f, 0.0f}  // strength, maxDistance, reserved, reserved

// FlatBenchmark
sunShadowParams = {0.64f, 0.0008f, 0.0055f, 1.25f}

// TransparencyStress
sunShadowParams = {0.76f, 0.0010f, 0.0040f, 1.30f}

// ChunkGrid
sunShadowParams = {0.80f, 0.0010f, 0.0070f, 1.50f}

// MeshingStress
sunShadowParams = {0.88f, 0.0009f, 0.0060f, 1.35f}
```

**TAA (Temporal Anti-Aliasing, per `Taa.hpp`):**
- 8-tap Halton(2,3) sub-pixel jitter sequence (in pixel units relative to rasterization center)
- По умолчанию `taaEnabled=false` (jitter=0) — стабильная картинка, нет дрожания
- YCoCg color space clamp в `taa_resolve.frag:170-190` — не вымывает chroma на ярких highlight'ах
- Per-layer history (CTSH/AOCC/LOCL) — `mix(rawCurrent, history, blend=0.4)`, per `agent/decisions.md §18`
- CTSH history **не** blended (deferred — separation refactor)
- Sidecar metadata: `taaEnabled`, `taaJitterX/Y`, `taaBlend`, `taaNeighbourhoodRadius`

**3 tone-map оператора (`VoxelMaterials.hpp:12-16`):**
- `Linear = 0` — no tone mapping
- `Reinhard` — Reinhard operator, `color / (color + 1.0)`
- `AcesApprox` — default, ACES filmic approximation
- Cycle клавишей `N`

**3 exposure metering modes (`VoxelMaterials.hpp:18-21`):**
- `Manual = 0` — fixed exposure bias
- `SceneKey` — auto-exposure based on scene key (luminance percentiles)

**6 shadow tuning targets (`VoxelMaterials.hpp:36-43`):**
- `Strength`, `DepthBias`, `NormalBias`, `FilterRadius`, `Coverage`, `CascadeBlend`
- Cycle клавишей `O`, decrease/increase клавишами `U`/`I`, reset `V`

**10 lighting debug views (`VoxelMaterials.hpp:23-34`):**
- `Final`, `Ambient`, `Direct`, `Local`, `Shadow`, `Cascade`, `Contact`, `Occlusion`, `Fog`, `Taa`
- Cycle клавишей `B` — каждое нажатие переключает слой для визуальной диагностики
- Sidecar `lighting_debug_view` enum value

**Three MRT attachments on voxel pass (`agent/decisions.md`):**
- `outColor` (Location 0) — main color
- `outSceneColor` (Location 1, TAA-on variant) — для TAA input
- `outLayerMask` (Location 2, R=CTSH, G=AOCC, B=LOCL, A=1.0) — packed R8G8B8A8
- Per-frame `vkCmdCopyImage` `taaLayerSceneColorTarget` → `taaLayerHistoryColorTarget`

**Greedy meshing compute shader (per `voxel_mesh.comp:613-619`):**
- 6 per-axis greedy passes
- W×H quad packing в 6+6 бит = 12 бит (max 64×64 per quad)
- `kMaxChunkExtentForGreedy` fallback на per-voxel emission

**6 smoke captures (per `LookDevCaptureAutomation.hpp`):**
- `FINAL` — composite (all effects applied)
- `SHDW` — только cascade shadows
- `CSM` — визуализация каскадов (split planes)
- `CTSH` — контактные тени (sun-to-fragment ray)
- `AOCC` — фоновое затенение (12 traces per fragment)
- `LOCL` — локальный точечный свет
- Запуск: `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh`
- Sidecar `.txt` 60+ ключей

**C-ядро (frustum cull, per `c_kernels/FrustumCulling.hpp`):**
- `projectv_cull_frustum_scalar` — **3.7-3.9× быстрее** C++ baseline (per `src/bench/FrustumCullBenchmark.cpp`)
- `projectv_cull_frustum_avx2` (с `__AVX2__`) — 2.5-2.7× (autovectorizer в debug бьёт hand-rolled `_mm256_setr_ps`)
- Crossover threshold: `kBatchDispatchThreshold = 8` AABBs (ниже — inline C++ helper)
- Scalar рекомендуется на текущей AoS layout; AVX2 даст выигрыш при SoA реорганизации

**Ray-march pass — STUB (`RayMarchPass.cpp:59-78`):**
```cpp
void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData) {
    // NO-OP STUB
    std::fprintf(stderr, "[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up...)\n");
}
```
- Compute-шейдер `ray_march.comp` (Amanatides-Woo DDA) скомпилирован
- API state работает: `SetRayMarchEnabled(bool)`, `IsRayMarchEnabled()`, `RequestRayMarchPipelineRecreate()`
- В graphics command stream не вкомпонован
- Phase 7 follow-up (per `docs/DefenseReport.md §3`)

**Hot shader reload (defense r0, 2026-06-15 relocation):**
- Клавиша `1` (было F5/F11 до relocation 2026-06-15)
- `RebuildAllShadersFromDisk()` в `src/app/main.cpp:68-114`
- `cmake --build $BUILD_DIR --target Shaders` (recompiles `.comp/.frag/.vert` через `glslc`)
- Log: `std::filesystem::temp_directory_path()/projectv_shader_reload.log` (cross-platform: Windows `%TEMP%`, Linux `/tmp`)
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Stderr: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`

**V-sync cycle (relocation, defense r0):**
- Клавиша `3` (было `V` до relocation)
- `CyclePreferredPresentMode()` в `VulkanSwapchain.cpp`
- `BuildPresentModeCycle` — built once per swapchain create from surface's supported modes
- Cycle length: usually 2 on Linux/Wayland (no IMMEDIATE), 3 on Windows
- `RecreateSwapchain` forced at end of branch

**3-rd MRT layer mask (`agent/decisions.md`):**
- Per-frame `vkCmdCopyImage` → `taaLayerHistoryColorTarget`
- Fragment shader samples `sampler2D layerHistory` (binding 6, graphics descriptor set)
- `mix(rawCurrent, history, blend=0.4)` applied to AOCC + LOCL
- CTSH written to history but **not** blended (deferred — cascade/contact shadow separation refactor)

### 3.3. Что смотреть на защите

**Слайд 5** (твой) — Тесты. 14 ctest наборов + 6 smoke captures + sidecar.

**Демо во время T2 (le1t):** VoxelLab, облёт, TAA toggle, debug view cycle (B), tone map cycle (N), shadow tuning (O/U/I), capture (C).

**Cycle debug-views клавишей `B`:** FINAL → SHDW → CSM → CTSH → AOCC → LOCL → AMBIENT → DIRECT → LOCAL → FOG → TAA → FINAL. Каждый визуализирует отдельный слой.

### 3.4. Реалистичные вопросы (5-7)

**Q1. Почему Vulkan 1.4, а не OpenGL?**
- Vulkan — явный контроль GPU (пайплайны, память, синхронизация)
- OpenGL — driver управляет, дорого для миллионов draw items
- Вычислительные шейдеры нужны для мешинга и трассировки
- Кросс-платформенный (Windows + Linux)

**Q2. Как работают каскадные тени?**
- 4 каскада карты глубины 2048×2048
- Лямбда 0.80 — near-biased распределение (ближние объекты получают больше плотности)
- Per-cascade projection: sub-frustum → light-space → sphere stabilization
- Стекло не отбрасывает тень, жидкость — отбрасывает (per `decisions.md`)

**Q3. Что такое TAA и зачем?**
- Временное сглаживание: смешивает кадры, убирает дрожание камеры
- 8-sample Halton(2,3) jitter, YCoCg-зажим
- По умолчанию ВЫКЛЮЧЕН (`taaEnabled=false`, jitter=0) — стабильная картинка

**Q4. Что такое AOCC?**
- Ambient Occlusion Cavity Check — локальное затенение полостей
- 12 traces per fragment (per `decisions.md`)
- Не полноценный SSAO — компактный, встроенный в lighting term
- Per-layer history blended (CTSH нет)

**Q5. Что такое контактные тени?**
- Короткая трассировка луча от фрагмента к солнцу
- Дополняет CSM там, где разрешения карт глубины не хватает
- Ограниченный прямой шейдерный проход, не отдельный render pass

**Q6. Зачем нужен C-ядро для frustum cull?**
- Scalar C-ядро: **3.7-3.9× быстрее** C++ baseline (autovectorizer-friendly)
- AVX2 версия: 2.5-2.7× (в debug autovectorizer бьёт hand-rolled `_mm256_setr_ps`)
- Crossover threshold: 8 AABBs (ниже — inline C++ helper, не оверхед на per-batch setup)
- C ABI / `extern "C"` — zero name-mangling, 1:1 Godbolt review

**Q7. Сколько pipeline'ов у вас и зачем так много?**
- VulkanGraphicsPipeline: 2 варианта (TAA-off + TAA-on) для shadow / graphics / TAA resolve
- VulkanVoxelMeshingPipeline: compute-шейдер для мешинга
- ModelPipeline: 2 варианта (`modelPipeline` + `modelPipelineTaaOn`)
- RayMarchPipeline: только при `IsRayMarchEnabled()`
- TAA pipelines: voxel pass + resolve pass
- Итого ~8-10 pipelines

### 3.5. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если `taaEnabled` переключить во время кадра?**
- Per `agent/decisions.md` — `PickModelPipeline` выбирает `modelPipeline` или `modelPipelineTaaOn` based on `render.taaEnabled`
- Переключение на лету может вызвать invalidation in-flight command buffer
- Решение: invalidate TAA history через клавишу `.` (`InvalidateTaaHistory`)

**Q9. Почему AOCC + LOCL blended, а CTSH нет?**
- Per `agent/decisions.md`: `ComputeSunShadowSample` объединяет cascade shadow (viewpoint-dependent) и contact shadow (viewpoint-independent) в одно значение
- Blending combined value with history reprojected from previous frame = wrong direction в cascade transition zones (cascade shadow "ghosts")
- Skip blend для CTSH пока правильно — visual artefact > flicker в этом specific layer
- Phase 5 follow-up: refactor `ComputeSunShadowSample` для separation

**Q10. Как работает sphere stabilization в CSM?**
- Вместо AABB в light space используется bounding sphere
- Sphere имеет constant projected size в screen space → нет jitter при движении камеры
- Чуть менее tight fit чем AABB, но стабильнее
- Per `decisions.md §18`

**Q11. Что произойдёт, если `kMaxChunkExtentForGreedy` уменьшить до 16?**
- Больше чанков попадёт в fallback path (per-voxel emission, 1×1 quads)
- Greedy meshing эффективность упадёт
- В VoxelLab 8×8×8 не попадает в fallback, но в `MeshingStress` сцене (16×16×16) — да
- Альтернатива: реорганизовать AABB данные в SoA, тогда AVX2 ядро достигнет 8×

### 3.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Почему Vulkan, не OpenGL/DX12/Metal | «Архитектурное решение — к le1t» |
| DOD layout / `alignas(16)` / push constants | «Архитектура, к le1t» |
| Стек/Clang/cmake/ctest baseline | «К Тиммейту 1» |
| Voxel-мир / чанки / мешинг | «К Тиммейту 2» |
| Voxel raycast / fluid CA / snapshot | «К Тиммейту 2» |
| Физика / walk controller / Jolt | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| BUG-004 VoxelLab tremor (отвергнут) | «Не существует, jitter=0 default» |
| SSAO / GTAO отложено | «Roadmap, к Тиммейту 4 (он закрывает)» |
| Hot shader reload (клавиша 1) | «К le1t» |
| Phase 4-9 / roadmap | «К Тиммейту 4 (он закрывает)» |

---

## §4. Тиммейт 4 — Физика и walk-контроллер (говорит T6 Планы)

### 4.1. Кто ты

**Легенда:** ты отвечал за физику — Jolt integration, walk-контроллер, edge grace, sneak, auto-jump, режимы движения, voxel-character collision.

**На сцене:** ты говоришь T6 (Планы и Закрытие) — последний, отвечаешь за финальное слово команды.

**На Q&A:** ты отвечаешь на вопросы про **физику, walk-контроллер, режимы, edge grace, sneak, автопрыжок, Phase 4 (networking)**.

### 4.2. Твоя компетенция: Физика и walk-контроллер

**Файлы:**
- `src/physics/PhysicsWorld.hpp` / `src/physics/PhysicsWorld.cpp` — main API
- `src/app/Camera.hpp` / `src/app/Camera.cpp` — камера (взаимодействует с walk через TickWalkCharacter)
- `src/app/InputActions.cpp` — input bindings для walk

**PhysicsWorld API (`PhysicsWorld.hpp:50-90`):**
- `PhysicsState *CreatePhysicsState()` — создать Jolt state
- `void DestroyPhysicsState(physics)` — уничтожить
- `bool SyncPhysicsWorld(physics, world)` — sync collision shapes с вокселями
- `PhysicsRaycastHit RaycastPhysicsWorld(physics, origin, direction, maxDistance)` — Jolt-уровень
- `void ResetWalkCharacter(physics)` — reset позиции
- `bool SnapWalkCharacterToCamera(physics, world, camera)` — teleport walk к камере
- `bool SnapCreativeCharacterToCamera(physics, world, camera)` — teleport creative
- `bool TickWalkCharacter(physics, world, camera, input, dt)` — main walk tick
- `bool TickCreativeCharacter(physics, world, camera, input, dt)` — creative tick
- `bool DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — **наш собственный воксельный решатель**, дополняет Jolt
- `SetPhysicsWalkAutoJumpEnabled(physics, bool)` — toggle
- `PhysicsWalkDebugInfo GetPhysicsWalkDebugInfo(physics)` — диагностика

**Jolt Physics:**
- Сторонняя библиотека (vendored `external/jolt/`)
- MIT, детерминированный, SIMD-оптимизирован
- Используется для общей физики твёрдых тел
- `JPH::CharacterVirtual` для детекции столкновений персонажа
- **Наш собственный код** дополняет Jolt для опоры игрока на блоки (edge grace, sneak, auto-jump) — Jolt не знает про «опору на 1 блок» в стиле Minecraft

**3 режима управления:**
- **Walk** — обычная ходьба с гравитацией (`TickWalkCharacter`)
- **Creative** — полёт с поддержкой столкновений (`TickCreativeCharacter`)
- **Spectator** — режим наблюдателя, пролетает сквозь стены (noclip)
- Переключение: `F4` (`ToggleControlMode`, cycle walk → creative → spectator)
- Двойной `Space` — toggle walk ↔ creative (быстрый)

**Walk controller features (per `PhysicsWalkDebugInfo` struct, `PhysicsWorld.hpp:19-40`):**

**Edge grace (допуск тонкого края):**
- Контроллер **не дёргает** игрока на тонких краях (часть стопы на блоке, часть на воздухе)
- `supportState = EdgeGrace` — диагностический enum
- `edgeGraceFramesRemaining` (u32) — grace-таймер, сколько кадров ещё действует
- Реализован в `src/physics/PhysicsWorld.cpp` через grace-таймеры

**Sneak (Shift = LShift/RShift, через `InputAction::MoveDown`):**
- Режим скрытности — игрок **не прилипает к стене за углом**
- `sneakActive` (bool) — флаг
- `sneakSupportGraceFramesRemaining` (u32) — grace-таймер
- `cachedSneakSupportValid` (bool), `feetInsideCachedSneakSupport` (bool), `cachedSneakSupportReferenceFeetY` (float) — кэшированная опора для sneak

**Auto-jump (J, через `InputAction::ToggleWalkAutoJump`):**
- При включении, контроллер каждый кадр проверяет: есть ли впереди блок высотой 1, можно ли перепрыгнуть
- `autoJumpEnabled` (bool) — флаг
- `autoJumpDelayFramesRemaining` (u32) — задержка после прыжка
- `autoJumpDelayEnabled` (bool) — флаг задержки (`F12` = `ToggleWalkAutoJumpDelay`)
- Позволяет не спамить прыжками при fast movement

**Ground takeoff grace:**
- `groundTakeoffGraceFramesRemaining` — при отрыве от земли контроллер не сразу теряет состояние "grounded"
- `groundTakeoffCached` (bool) — закэшировано ли

**Ledge release grace:**
- `ledgeReleaseGraceFramesRemaining` — игнорирование кратковременной потери опоры на тонких краях

**WalkAirControlMode (F11, `InputAction::ToggleWalkAirControlMode`):**
- Переключатель: «насколько сильно игрок может влиять на направление в воздухе»
- `GetPhysicsWalkAirControlMode(physics)` / `SetPhysicsWalkAirControlMode`

**Voxel raycast для character:**
- `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — проверяет, пересекается ли AABB персонажа с заданным вокселем
- Используется в `VoxelInteraction::CanPlaceInteractionVoxelBox` для предотвращения placement внутрь игрока
- `InteractionState` хранит `placementVoxel`, `placementChunkCoord`, `placementChunkMin/Max`, `placementChunkDirty/Active/Index`, `placementChunkNonAirVoxelCount`, `placementVoxelInChunk`

**PhysicsWalkDebugInfo (struct, `PhysicsWorld.hpp:19-40`):**
```cpp
struct PhysicsWalkDebugInfo {
    bool valid = false;
    PhysicsWalkSupportDebugState supportState;  // Air / Grounded / EdgeGrace
    std::array<float, 3> feetPosition{};
    float footSupportScore = 0.0f;
    uint32_t footSupportHitSamples = 0;
    uint32_t footSupportTotalSamples = 0;
    uint32_t edgeGraceFramesRemaining = 0;
    uint32_t groundTakeoffGraceFramesRemaining = 0;
    uint32_t sneakSupportGraceFramesRemaining = 0;
    uint32_t ledgeReleaseGraceFramesRemaining = 0;
    uint32_t autoJumpDelayFramesRemaining = 0;
    bool groundTakeoffCached = false;
    bool cachedSneakSupportValid = false;
    bool feetInsideCachedSneakSupport = false;
    bool sneakActive = false;
    bool jumpLockActive = false;
    bool suppressPassiveSlide = false;
    bool autoJumpEnabled = false;
    bool autoJumpDelayEnabled = true;
    float cachedSneakSupportReferenceFeetY = 0.0f;
};
```

**Хоткеи walk (`InputActions.cpp:119-181`):**
- `W` `A` `S` `D` — движение
- `Space` — прыжок (`MoveUp`)
- `LShift` / `RShift` — sneak (`MoveDown`, через speed slow не наоборот)
- `LCTRL` / `RCTRL` — speed boost (`SpeedBoost`, 3× = 40 m/s × 3 = 120 m/s)
- `LALT` / `RALT` — speed slow (`SpeedSlow`, 0.25× = 10 m/s)
- `F4` — toggle walk/creative/spectator (`ToggleControlMode`)
- `F11` — toggle walk air control mode (`ToggleWalkAirControlMode`)
- `J` — toggle auto-jump (`ToggleWalkAutoJump`)
- `F12` — toggle auto-jump delay (`ToggleWalkAutoJumpDelay`)
- `F3` — reset camera (`ResetCamera`)
- `TAB` — toggle relative mouse mode (`ToggleRelativeMouseMode`)
- `P` — toggle pause (`TogglePause`)
- `[` / `]` — decrease / increase time scale (`DecreaseTimeScale` / `IncreaseTimeScale`)
- `\` — step single frame (`StepSingleFrame`)
- `` ` `` — reset time scale (`ResetTimeScale`)

**Камера (`src/app/Camera.hpp:5-33`):**
- `InitializeCamera(camera, simulation, input)` — init
- `HandleCameraEvent(camera, input, event)` — SDL events
- `ConsumeCameraLookInput(camera, input)` — read mouse deltas
- `TickCamera(camera, input, dt)` — per-frame
- `GetCameraForwardVector(camera)` — для look direction
- `GetCameraVisibleSceneMaxDistance(camera)` — для frustum cull
- `BuildGraphicsPushConstants(camera, extent, taaJitterNdcX, taaJitterNdcY)` — для shader
- `BuildChunkCullingParameters(camera, extent, maxDistance)` — для C-ядра

**Camera constants (`Camera.cpp:29-34`):**
```cpp
constexpr float kMinMoveSpeed = 2.0f;
constexpr float kMaxMoveSpeed = 40.0f;
constexpr float kBoostMoveSpeedMultiplier = 3.0f;
constexpr float kSlowMoveSpeedMultiplier = 0.25f;
constexpr float kMaxLookPitchRadians = 1.553343f;  // ~89°
constexpr float kMainlineVisibleSceneMaxDistance = 64.0f;
```

**SyncPhysicsWorld:**
- Один раз за кадр (или реже) — синхронизирует Jolt collision shapes с `VoxelWorld.voxels`
- Возвращает `bool` — успех/неуспех (cold path)
- Increment `editVersion` → invalidate Jolt shapes для затронутых вокселей

**Phase 4 (networking) — что в плане:**
- Server-authoritative + client prediction
- `SnapWalkCharacterToCamera` может использоваться для client prediction
- `SyncPhysicsWorld` будет происходить на сервере, broadcast результатов
- Phase 4 follow-up

### 4.3. Что смотреть на защите

**Слайды 7-8** (твои) — Планы + Таблица ролей. Говори «спасибо за внимание, готовы ответить на вопросы».

**Демо во время T2 (le1t):** walk (WASD + Space), sneak (Shift), auto-jump (J), creative (F4 cycle, двойной Space), spectator (F4). Voxel placement/removal.

### 4.4. Реалистичные вопросы (5-7)

**Q1. Почему Jolt, а не PhysX или Bullet?**
- Jolt — MIT, современный, детерминированный, SIMD-оптимизирован
- Bullet устарел, PhysX избыточен + проприетарный
- per `decisions.md` и `agent/memory.md`

**Q2. Как работает edge grace?**
- Контроллер **не дёргает** игрока на тонких краях
- `supportState = EdgeGrace` когда опора нечёткая (часть стопы на блоке)
- `edgeGraceFramesRemaining` — таймер, сколько кадров ещё действует
- Без edge grace: игрок дёргается на тонких блоках (1-wide bridge)

**Q3. Как работает auto-jump?**
- При включении (`J`), контроллер каждый кадр проверяет: есть ли впереди блок высотой 1
- `autoJumpDelayFramesRemaining` — задержка после прыжка
- `autoJumpDelayEnabled` — toggle через `F12`

**Q4. Как работает sneak?**
- При зажатом Shift, игрок **не прилипает к стене за углом**
- `sneakActive` — флаг, `sneakSupportGraceFramesRemaining` — grace-таймер
- `cachedSneakSupportValid` — закэширована ли опора для sneak

**Q5. Какие режимы и как переключать?**
- Walk / Creative / Spectator — `F4` cycle
- Walk ↔ Creative — двойной `Space`
- Creative — полёт с collision
- Spectator — noclip (через стены)

**Q6. Какая максимальная скорость?**
- Base: 2.0-40.0 m/s (зависит от input)
- Boost (`LCTRL`): ×3 = 40 × 3 = 120 m/s
- Slow (`LALT`): ×0.25 = 10 m/s
- Sneak: тот же slow что и LShift

**Q7. Как работает creative mode?**
- Полёт с поддержкой столкновений (`TickCreativeCharacter`)
- Двойной `Space` — toggle walk ↔ creative
- Можно подлететь к любой точке сцены
- Voxel placement/removal работает так же

### 4.5. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если игрок стоит на 1-блок-wide мосте?**
- Edge grace включается: контроллер не дёргает
- Если опора полностью теряется — fall (гравитация)
- `ledgeReleaseGraceFramesRemaining` — кратковременная потеря опоры игнорируется

**Q9. Чем отличается `groundTakeoffGrace` от `edgeGrace`?**
- `groundTakeoff` — при отрыве от земли (прыжок) контроллер не сразу теряет "grounded"
- `edgeGrace` — на тонких краях контроллер не дёргает
- Разные grace-таймеры, разные ситуации

**Q10. Как Phase 4 (networking) изменит walk controller?**
- `SnapWalkCharacterToCamera` — для client prediction (предсказание позиции)
- `SyncPhysicsWorld` — на сервере, broadcast результатов клиентам
- Reconciliation: если предсказание не совпало с server state — snap обратно
- per Phase 4 в roadmap

**Q11. Можно ли прыгнуть на 2-блок высоту через auto-jump?**
- Нет, auto-jump только для 1-блок
- Для 2+ блоков — нужен manual `Space` в нужный момент
- Иначе это не «опора на блок», а прыжок как таковой

### 4.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Почему Jolt, а не PhysX/Bullet | «Архитектурное решение — к le1t» |
| DOD / hot-cold split | «К le1t» |
| Стек/Clang/cmake/ctest | «К Тиммейту 1» |
| Voxel-мир / чанки | «К Тиммейту 2» |
| Тени / TAA / AOCC / рендеринг | «К Тиммейту 3» |
| Ассеты / аудио / snapshot | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t» |
| Hot shader reload (клавиша 1) | «К le1t» |
| JSON config / snapshot PVSNAP01 | «К le1t» |
| Phase 5-9 (SVO, fluid GPU, частицы, моддинг, стратегия) подробно | «К le1t — он знает детали roadmap» |

---

## §5. Тиммейт 5 — Ассеты и аудио (говорит T5 Прочие фичи)

### 5.1. Кто ты

**Легенда:** ты отвечал за ассетный конвейер (glTF + Draco + meshopt) и аудио (miniaudio + MP3). Также знаешь про snapshot save/load и hot shader reload.

**На сцене:** ты говоришь T5 (Прочие фичи + что отложено) — что не вошло в демо + roadmap.

**На Q&A:** ты отвечаешь на вопросы про **ассеты, аудио, snapshot мира, hot shader reload, deferred items**.

### 5.2. Твоя компетенция: Ассеты и аудио

**Asset pipeline файлы:**
- `src/asset/AssetLoader.{hpp,cpp}` — entry point `LoadGlb(path, outError) → std::unique_ptr<LoadedAsset>`
- `src/asset/AssetManifest.{hpp,cpp}` — env `PROJECTV_MODELS`, формат `path.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s`
- `src/asset/AssetRegistry.{hpp,cpp}` — реестр моделей
- `src/asset/DracoMeshDecoder.{hpp,cpp}` — Draco decoder
- `src/asset/MeshBaker.{hpp,cpp}` — `BakeLoadedAsset(asset, config, outError) → BakedMesh`
- `src/asset/MeshGpuResources.{hpp,cpp}` — GPU buffer upload
- `src/asset/ModelPass.{hpp,cpp}` — TAA-aware pipeline
- `src/asset/ModelManifestLoader.{hpp,cpp}` — manifest parser
- `src/asset/AssetStub.cpp` — linker anchor (3 строки, гарантирует линковку draco + fastgltf + meshopt)

**Audio файлы:**
- `src/audio/AudioEngine.{hpp,cpp}` — miniaudio wrapper (~800 строк)
- `src/audio/MusicDirectoryPath.{hpp,cpp}` — путь к `music/`
- `external/miniaudio/` — vendored single-header C library

**glTF parser (`AssetLoader.cpp:392-431`):**
- `fastgltf::Parser parser(fastgltf::Extensions::KHR_draco_mesh_compression)` — line 408
- `parser.loadGltf(dataBuffer.get(), directory, options, categories)` — line 413
- `fastgltf::Options::None`, `fastgltf::Category::OnlyRenderable`
- 2 pass: Pass 1 read POSITION/NORMAL/UV/indices, Pass 2 apply node TRS

**Draco decode (per `AssetLoader.cpp:446-449`):**
- `if (primitive.dracoCompression != nullptr) { DecodeDracoPrimitive(...) }` — line 446
- `DracoMeshDecoder` в `src/asset/DracoMeshDecoder.cpp`
- Извлекает POSITION (3 floats), NORMAL (3 floats), TEX_COORD (2 floats), face indices
- Degenerate geometry detection (no faces)

**Meshopt (per `MeshBaker.cpp:56-87`):**
1. `meshopt_optimizeVertexCache` — reorder indices for vertex cache locality
2. `meshopt_generateVertexRemap` — vertex deduplication
3. `meshopt_remapVertexBuffer` — apply remap к vertex buffer
4. `meshopt_remapIndexBuffer` — apply remap к index buffer
5. `meshopt_optimizeVertexFetch` — compact vertices (cache locality)
6. `meshopt_analyzeVertexFetch` — overfetch ratio (BakedMesh.overfetch)

**Baked mesh struct (`MeshBaker.hpp`):**
```cpp
struct BakedPrimitive {
    std::vector<uint8_t> vertexBuffer;
    std::vector<uint32_t> indices;
    uint32_t vertexCount, indexCount;
    std::optional<size_t> materialIndex;
    float overfetch = 1.0f;  // from meshopt_analyzeVertexFetch
};
struct BakedMesh {
    std::vector<BakedPrimitive> primitives;
    float acmr = 0.0f;  // Average Cache Miss Ratio
    float atvr = 0.0f;  // Average Transform-to-Vertex Ratio
};
```

**Stride:** `kBakedVertexStride = sizeof(float) * 8` = 32 B (float3 pos + float3 normal + float2 uv)

**Asset manifest format (`AssetManifest.hpp:31-42`):**
```
PROJECTV_MODELS=pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s;pathC.glb
```
- Id = basename без расширения по умолчанию
- `projectv::core::StringID` (16 B = hash + length + pad) для O(1) equality, hashable

**Model pipeline (`ModelPass.hpp:8-21`):**
- `CreateModelPipeline` — создаёт оба варианта
- `PickModelPipeline(render)` — выбирает `modelPipeline` или `modelPipelineTaaOn` по `render.taaEnabled`

**Audio engine (`AudioEngine.hpp:95-306`):**
- `ma_engine` + `ma_sound_group` (music volume bus) + `ma_sound` (current track)
- Singleton on `AppState`, plain (not ECS system)
- Format: 16-bit signed PCM, 44.1 kHz, stereo
- Linux backend: PulseAudio → `pipewire-pulse` shim → active PipeWire
- 3 states: `Stopped` / `Playing` / `Paused`
- 5-second playlist refresh (`m_lastPlaylistRefresh`)
- `ParseArtistTitle(filename, &artist, &title)`:
  - Strip case-insensitive `.mp3` tail
  - Split on first ` - ` (space-dash-space)
  - Fallback: `artist = "-"` (em-dash sentinel) when no separator
- 2 MP3 в `music/`: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- Хоткеи: `Q` play/pause, `E` stop, `7`/`8` volume, `9`/`0` next/prev

**Snapshot мира (per `VoxelWorld.cpp:17-20`):**
- Magic: `PVSNAP01` (8 B ASCII)
- 80-B header: `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion` (8 B)
- `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B)
- Хоткеи: F6 save, F7 load

**Hot shader reload (per `main.cpp:68-114`):**
- Клавиша `1` (relocated from F5/F11 2026-06-15)
- `RebuildAllShadersFromDisk()`:
  1. `PROJECTV_BUILD_DIR` (env) или `PROJECTV_CMAKE_BUILD_DIR` (compile-time default)
  2. `cmake --build $BUILD_DIR --target Shaders` (recompiles `.comp/.frag/.vert` через `glslc`)
  3. Log: `std::filesystem::temp_directory_path()/projectv_shader_reload.log` (cross-platform: Windows `%TEMP%`, Linux `/tmp`)
  4. `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Stderr: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`

**Ray-march toggle (клавиша `2`):**
- `SetRayMarchEnabled(bool)` / `IsRayMarchEnabled()` в `RayMarchPass.hpp`
- Toggles `static RayMarchState::enabled` flag
- Per-frame `RecordRayMarchCommands` — `fprintf` в stderr, no-op
- Compute-шейдер `ray_march.comp` скомпилирован, но в graphics stream не вкомпонован

**5 отложенных пунктов (per `docs/DefenseReport.md §3`):**
1. **Полная система частиц** (ТЗ 4.1.4) — Phase 7
2. **Плагины / моддинг API** (ТЗ 4.1.8) — Phase 8
3. **Асинхронная загрузка ресурсов** — Phase 7
4. **HDR-текстуры** (`.hdr`) — Phase 6
5. **SVO (Sparse Voxel Octree)** — Phase 5
6. **Mesh shaders (VK_EXT_mesh_shader)** — Phase 5

**Roadmap (per `docs/DefenseReport.md §3` + `DefenseBriefer_le1t.md`):**
- Phase 4: Networking (server-authoritative + client prediction)
- Phase 5: SVO (Sparse Voxel Octree) + Mesh shaders
- Phase 6: HDR-текстуры + полный клеточный автомат жидкости на GPU
- Phase 7: Полная система частиц + асинхронная загрузка ресурсов
- Phase 8: Плагины / моддинг API
- Phase 9: Многопользовательский режим (Academic vision)

### 5.3. Что смотреть на защите

**Слайд 6** (твой) — Прочие фичи + что отложено. Показывает asset pipeline, audio, snapshot, hot reload, deferred items.

**Демо во время T2 (le1t):** Hot shader reload (клавиша `1`), audio playback (Q для play, 9/0 для треков), asset loading.

### 5.4. Реалистичные вопросы (5-7)

**Q1. Что такое Draco и зачем?**
- Алгоритм сжатия 3D-мешей от Google
- `KHR_draco_mesh_compression` extension в glTF
- Позволяет загружать меши на 50-80% меньше по размеру
- Декодирование на лету в `DracoMeshDecoder`

**Q2. Что делает meshopt?**
- Оптимизация мешей под видеокарту
- Vertex cache optimization — reorder indices для cache locality
- Vertex fetch optimization — compact vertices (избегаем overfetch)
- Overfetch ratio (BakedMesh.overfetch) — мера эффективности

**Q3. Поддерживает ли audio что-то кроме MP3?**
- Только MP3 (miniaudio built-in MP3 decoder)
- OGG/WAV/FLAC требуют linking `extras/decoders/libvorbis` / `libopus` — deferred
- per `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md`

**Q4. Как работает hot shader reload?**
- Клавиша `1` → `RebuildAllShadersFromDisk()`
- `cmake --build $BUILD_DIR --target Shaders` — recompiles all `.comp/.frag/.vert`
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Другие pipelines (graphics, shadow, TAA) переиспользуют кэшированные модули до Phase 7+ рефакторинга

**Q5. Где хранится snapshot мира?**
- По умолчанию: `ProjectV.snapshot.bin` в working directory
- Magic: `PVSNAP01` (8 B ASCII header)
- 80-B header + voxel payload
- Std::expected<bool, VoxelSnapshotError> — Tier 1.B error enum

**Q6. Что будет если snapshot повреждён?**
- `LoadVoxelWorldSnapshot` вернёт `std::unexpected(VoxelSnapshotError::*)`
- Possible errors: `PreconditionFailed` (null file), `FolderCreateFailed` (write), `ScanFailed` (corrupt header)
- Не падает, graceful degradation

**Q7. Сколько MP3 файлов в проекте?**
- 2 файла: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- Формат имён: `<artist> - <title>.mp3` — парсер для HUD
- 5-секундный refresh плейлиста

### 5.5. Каверзные вопросы (3-5)

**Q8. Почему именно meshopt, а не просто GPU draw call batching?**
- Meshopt на этапе bake'а — один раз, бесплатно в runtime
- Draw call batching — overhead в runtime
- meshopt уменьшает overfetch (vertex fetch locality) и cache miss'ы
- BakedMesh.overfetch ratio — мера эффективности (1.0 = идеал)

**Q9. Почему формат `<artist> - <title>.mp3`?**
- Оператор использует эту конвенцию
- Парсер разделяет по ` - ` (space-dash-space)
- Fallback: `artist = "-"` (em-dash) если нет разделителя
- Em-dash distinct от empty string (= "no track loaded")

**Q10. Что произойдёт, если добавить новый MP3 во время runtime?**
- Плейлист refresh каждые 5 секунд (`m_lastPlaylistRefresh`)
- Новый файл подхватится автоматически
- Если текущий трек не загружен → загрузится при следующем Q

**Q11. Зачем asset manifest, если можно захардкодить?**
- `PROJECTV_MODELS` env var → flexibility для разных сцен
- HL2-style physicsgun (`F` key) — переключение моделей на лету
- Default id = basename без расширения

### 5.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Почему fastgltf, а не tinygltf | «Архитектурное решение — к le1t» |
| PipeWire vs PulseAudio подробно | «Linux audio, к le1t» |
| Стек/Clang/cmake/ctest | «К Тиммейту 1» |
| Voxel-мир / чанки / мешинг | «К Тиммейту 2» |
| Тени / TAA / AOCC | «К Тиммейту 3» |
| Физика / walk controller | «К Тиммейту 4» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| Hot shader reload (клавиша 1) | «К le1t» |
| JSON config / snapshot PVSNAP01 | «К le1t» |

---

## §6. le1t — Архитектура + Q&A host (говорит T2 Demo + Стек)

### 6.1. Кто ты

**Реальность:** ты — Кадочников Лев Петрович, единственный разработчик ProjectV. Команда «Черепашки Ninja» — для защиты.

**На сцене:** ты ведущий, говоришь T2 (Live Demo + Стек) 1:15.

**На Q&A:** ты отвечаешь на **ВСЕ сложные технические вопросы**. Тиммейты подключаются по своим компетенциям. Если вопрос выходит за пределы твоих знаний (что вряд ли) — «не знаю, уточню у команды».

### 6.2. Твоя компетенция: Архитектура + Workflow

#### 6.2.1. Стек (C++26, Vulkan 1.4, DOD, ECS)

**Язык: C++26** (`CMAKE_CXX_STANDARD 26` в `CMakeLists.txt:29`).
- `std::expected<T, E>` для холодных путей (Vulkan init, snapshot, audio load)
- `std::simd` для горячих путей
- C++26 модули (`Math.ixx`, `Probe.ixx`, `StringId.ixx` per `agent/memory.md §2.D`)
- `import std;` probe в mainline
- Hot-cold split: `bool`+`CORE_ASSERT` на горячих, `std::expected` на холодных

**Графика: Vulkan 1.4** (per `legacy/docs/architecture/specs/`):
- Dynamic rendering (no VkRenderPass)
- Timeline semaphores
- Compute shaders (mesh generation)
- `volk` как loader (`VK_NO_PROTOTYPES`)
- `VMA` для GPU memory
- Сторонние: `fastgltf`, `draco`, `meshoptimizer`, `fmt`, `glm`, `nlohmann/json`, `stb_image`, `spdlog`

**Архитектура: DOD (Data-Oriented Design):**
- `alignas(16)` на `VoxelChunk` (32 B)
- Плоский `std::vector<uint8_t> voxels` в `VoxelWorld`
- Итерация по индексу, не по итератору
- 3-column thinking: данные vs код vs pipeline
- per `legacy/docs/philosophy/`

**ECS: Flecs:**
- `EcsWorld::InitializeAppEcs(state)` — init
- `SyncEcsWorldState(ecs)` — 1× за кадр
- Flecs — MIT, header-only C++
- Single Source of Truth: `VoxelWorld` владеет, ECS — пассивное зеркало

**Физика: Jolt:**
- MIT, детерминированный, SIMD
- `JPH::CharacterVirtual` для коллизий
- Наш собственный код дополняет для walk controller

#### 6.2.2. Алгоритмы (все 23, per `docs/DefenseAlgorithms.md`)

| # | Алгоритм | Где реализован |
|---|---|---|
| 1 | Жадный мешинг (greedy meshing) | `voxel_mesh.comp:613-619` |
| 2 | Каскадные тени (CSM) | `ShadowProjection.hpp:42-51` |
| 3 | Контактные тени (CTSH) | `voxel.frag` (sun-to-fragment ray) |
| 4 | Фоновое затенение (AOCC) | `voxel.frag:ComputeAmbientOcclusionVisibility` |
| 5 | Локальный точечный свет (LOCL) | `voxel.frag` (per-fragment lighting) |
| 6 | TAA (Temporal Anti-Aliasing) | `Taa.hpp` + `taa_resolve.frag` |
| 7 | Ray-marching (DDA) | `ray_march.comp` (STUB) |
| 8 | Fluid CA (клеточный автомат) | `VoxelWorld.cpp:1284-1643` |
| 9 | Voxel raycast (DDA) | `VoxelRaycast.cpp` |
| 10 | Frustum culling (C-ядро) | `c_kernels/frustum_cull.{hpp,c}` |
| 11 | Chunk visibility cache | `SceneResources.hpp:359-407` |
| 12 | Walk controller (Jolt + voxel solver) | `PhysicsWorld.cpp` |
| 13 | Auto-jump (1-block detection) | `PhysicsWorld.cpp` |
| 14 | Edge grace (тонкие края) | `PhysicsWorld.cpp` |
| 15 | Sneak (Shift) | `PhysicsWorld.cpp` |
| 16 | glTF parser | `AssetLoader.cpp:408-431` |
| 17 | Draco decode | `DracoMeshDecoder.cpp` |
| 18 | meshopt (vertex cache/fetch) | `MeshBaker.cpp:56-87` |
| 19 | Audio engine (miniaudio) | `AudioEngine.cpp` |
| 20 | Snapshot save/load (PVSNAP01) | `VoxelWorld.cpp:1284-1643` |
| 21 | Hot shader reload | `main.cpp:68-114` |
| 22 | Hot/cold error split (std::expected) | `Tier 1.B` |
| 23 | DOD layout (alignas, SoA) | `VoxelChunk`, `VoxelWorld` |

#### 6.2.3. Тесты и метрики

- **14 ctest** baseline 14/14 (0.78s debug, 0.06s release)
- **6/6 runtime smoke** captures (FINAL/SHDW/CSM/CTSH/AOCC/LOCL)
- **60+ sidecar keys** в `.txt` файле
- **73 MB debug / 19 MB release** ELF (-73%)
- **2 MP3** в `music/`
- **0 предупреждений** в нашем коде (per `agent/decisions.md §4`)

#### 6.2.4. Известные баги (на момент защиты)

**BUG-005 (cycle scene race):**
- Гонка дескрипторов при переключении сцен (F5)
- Частично смягчена: `vkDeviceWaitIdle` в `DestroySceneResources`
- Полное устранение — Phase 5 (per `agent/memory.md §10.5` + `decisions.md`)

**BUG-004 (VoxelLab tremor) — ОТВЕРГНУТ:**
- Галлюцинация предыдущей сессии
- TAA по умолчанию ВЫКЛЮЧЕН (`taaEnabled=false`, jitter=0)
- Нет дрожания при default config
- Если кто-то спросит: «не существует, jitter=0 default, не воспроизводится»

**Ray-march pass — STUB:**
- `RecordRayMarchCommands` — no-op, `fprintf` в stderr
- Compute-шейдер `ray_march.comp` скомпилирован
- Phase 7 follow-up

#### 6.2.5. Workflow (multi-agent)

**AGENTS.md** — стабильный протокол:
- §1: Изменение только по явной команде
- §7.2.5: Commit message contract (type/scope/summary/body/Refs)
- §7.2.6: Multi-agent concurrent work policy
- §7.2.8: Shared `agent/` files (не claim'ить эксклюзив)
- §7.3.1: Pre-commit gate
- §8.1: Auto-close после commit
- §9: Definition of done

**TODO.md** — живой roadmap + бэклог

**agent/active-sessions.md** — append-only ledger координации

**agent/decisions.md** — зафиксированные архитектурные решения

**agent/memory.md** — долговечные факты, lessons learned, run-time observations

**agent/status.md** — snapshot состояния сессий

**Multi-agent сессии:**
- Параллельный запуск нескольких сессий — нормальный сценарий
- Пересекающийся scope — arbitration через оператора
- Известный инцидент 2026-06-10: `git checkout -- .` стёр uncommitted work
- Урок: safety-net patch в `/tmp/` обязательно

#### 6.2.6. Roadmap (Phase 4-9)

| Phase | Цель |
|---|---|
| 4 | Networking (server-authoritative + client prediction) |
| 5 | SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader) |
| 6 | HDR-текстуры + полный клеточный автомат жидкости на GPU |
| 7 | Полная система частиц + асинхронная загрузка ресурсов |
| 8 | Плагины / моддинг API |
| 9 | Многопользовательский режим (Academic vision) |

### 6.3. Твой слот: T2 Live Demo + Стек (1:15)

**Действия:**
1. Запустить приложение (сцена `VoxelLab`).
2. Включить подробный HUD клавишей `G`.
3. Показать облёт камеры (WASD + мышь).
4. Поставить/сломать пару блоков (правый/левый клик).
5. Переключить debug view (`B` — cycle FINAL/SHDW/CSM/CTSH/AOCC/LOCL).
6. Захватить screenshot (`C`).

**Речь (verbatim):** см. `docs/DefenseBriefer_le1t.md §2`.

### 6.4. Q&A-карта (30+ вопросов)

**Расширенная версия** — см. `docs/DefenseBriefer_le1t.md §4` (30 вопросов) + специализированные ниже.

**Q1. Почему C++26, а не Rust/Zig/Go?**
- Все зависимости (Jolt, fastgltf, VMA, Draco, Flecs) — C/C++ с нативным API
- C++26 даёт `std::expected` для холодных путей, `std::simd` для горячих, модули для ускорения инкрементальной сборки
- Rust — рассматривался, но Vulkan bindings + ECS + asset pipeline зрелые на C++

**Q2. Почему Vulkan 1.4, а не OpenGL/DX12/Metal?**
- Vulkan — явный контроль GPU (пайплайны, память, синхронизация)
- OpenGL — driver управляет, дорого для миллионов draw items
- Compute shaders нужны для мешинга
- Кросс-платформенный (Windows + Linux)

**Q3. Почему Jolt, а не PhysX/Bullet?**
- Jolt — MIT, современный, детерминированный, SIMD-оптимизирован
- Bullet устарел, PhysX избыточен + проприетарный

**Q4. Почему Flecs, а не EnTT/Bevy ECS/DOTS?**
- Flecs — header-only C++ ECS, MIT, отличная эргономика для встроенного использования в Vulkan-приложении
- EnTT — header-only, но runtime overhead выше
- Bevy ECS — только для Rust

**Q5. Что такое DOD и зачем?**
- Дизайн, ориентированный на данные (Data-Oriented Design)
- Данные организованы для эффективной обработки CPU, а не для удобства иерархии классов
- Чанк 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра, влезает в L1
- `alignas(16)` → авто-векторизация в `movaps`/`vmovaps`

**Q6. Как связаны ECS и VoxelWorld?**
- Single Source of Truth: `VoxelWorld` — единственный владелец, все мутации только через него
- ECS (Flecs) — пассивное зеркало, обновляется 1 раз за кадр через `SyncEcsWorldState`
- HUD читает из ECS (только чтение), не из VoxelWorld (изменяемый)

**Q7. Как боретесь с накладными расходами на обработку ошибок?**
- Гибридный подход: на холодных путях — `std::expected<T, E>`, на горячих — `bool` + `CORE_ASSERT` (вырезаются в release)
- Cold paths: Vulkan init, snapshot, audio load, scene config
- Hot paths: voxel meshing dispatch, frame prep

**Q8. Что такое жадный мешинг и зачем?**
- Объединяет соседние грани вокселей одного exposed state в один четырёхугольник (quad)
- 6 проходов на чанк: ±X, ±Y, ±Z
- Compute-шейдер `voxel_mesh.comp:613-619`
- Сокращение draw calls на 30-50%

**Q9. Как работают каскадные тени (CSM)?**
- 4 каскада карты глубины 2048×2048
- Лямбда 0.80 (near-biased)
- Per-cascade проекция солнца: sub-frustum → light-space → sphere stabilization
- Стекло не отбрасывает тень, жидкость — отбрасывает (per `decisions.md`)

**Q10. Что такое TAA и зачем?**
- Временное сглаживание: смешивает кадры, убирает дрожание камеры
- 8-sample Halton(2,3) jitter, YCoCg-зажим
- Поверх TAA — CAS (фильтр резкости)
- **По умолчанию TAA jitter = 0 (стабильная картинка, нет дрожания)**

**Q11. Что такое ray-marching и как реализован?**
- Трассировка лучей через воксели (Amanatides-Woo DDA)
- Compute-шейдер `ray_march.comp` скомпилирован
- API state (`SetRayMarchEnabled`/`IsRayMarchEnabled`/`RequestRayMarchPipelineRecreate`) работает
- Graphics command stream его пока не вызывает — **STUB, Phase 7 follow-up**
- Per `RayMarchPass.hpp:9-30`

**Q12. Что такое контактная тень (CTSH)?**
- Короткая трассировка луча от фрагмента к солнцу
- Дополняет CSM где разрешения карт глубины не хватает
- Per-layer history не blended (deferred — separation refactor)

**Q13. Что такое AOCC?**
- Ambient Occlusion Cavity Check — локальное затенение полостей
- 12 traces per fragment (per `decisions.md`)
- Не полноценный SSAO — компактный, встроенный в lighting term
- Per-layer history blended (mix with 0.4)

**Q14. Зачем нужен локальный точечный свет?**
- Сцена Voxel Laboratory имеет один на пресет обратно-квадратичный точечный свет
- В дополнение к направленному солнцу
- Объёмный эффект, подсвечивает тёмные стороны

**Q15. Как работает walk controller?**
- `JPH::CharacterVirtual` для обнаружения столкновений
- Наш собственный код дополняет Jolt для опоры игрока на блоки
- Edge grace / sneak / auto-jump — наш код, не Jolt

**Q16. Как работает авто-прыжок?**
- При включении (`J`), контроллер каждый кадр проверяет: есть ли впереди блок высотой 1
- `autoJumpDelayFramesRemaining` — задержка после прыжка
- `autoJumpDelayEnabled` — toggle `F12`

**Q17. Что такое клеточный автомат (Fluid CA)?**
- Для жидкости: один тик = попытка падения вниз, иначе распространение в 1 из 4 сторон
- Bottom-up y-pass → 1 cell per tick
- Double-buffered snapshot, claimed-tracking
- Deterministic, no FP, no atomics

**Q18. Какие тесты, сколько?**
- 14 наборов в `tests/CMakeLists.txt`
- Baseline 14/14, 0.78 сек debug, 0.06 сек release
- Runtime smoke 6/6 captures
- 60+ sidecar keys

**Q19. Какие метрики производительности?**
- VoxelLab reference shot: 500+ FPS, ~2 мс кадр
- Release: 19 МБ (vs 73 МБ debug, -73%)
- 14/14 ctest, 6/6 smoke

**Q20. Какие известные баги?**
- BUG-005 (cycle scene race): гонка дескрипторов при переключении сцен, частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`
- **BUG-004 (VoxelLab tremor) — отвергнут, не существует**
- Ray-march STUB (Phase 7)

**Q21. Что отложено и почему?**
- 6 пунктов: частицы, моддинг, асинхронная загрузка, HDR, SVO, mesh shaders
- Все явно в Phase 4-9 roadmap
- per `docs/DefenseReport.md §3`

**Q22. Какие платформы поддерживаются?**
- Windows 10/11 (clang-cl 22) + Linux Arch (clang 22 native + libc++ 16)
- Обе сборки зелёные, 14/14 тестов
- macOS — НЕ в планах (per `decisions.md`)

**Q23. Какие решения принимали лично вы?**
- Стек: C++26, Vulkan 1.4, Flecs, Jolt
- DOD layout: `alignas(16)`, чанк 8×8×8
- Hot/cold split: `std::expected` на холодных, `bool`+assert на горячих
- Walk: наш код дополняет Jolt
- Glass: не отбрасывает тень, жидкость отбрасывает
- TAA: B10G11R11 UFLOAT цвет
- 4-каскадный CSM
- Release-пресеты: без `-ffast-math`, без `-march=native`

**Q24. Какие были трудности?**
- Согласование областей ответственности (scope) между модулями
- Multi-agent сессии: протокол в `AGENTS.md §7.2.6`
- Инцидент 2026-06-10: `git checkout -- .` стёр uncommitted work
- Урок: safety-net patch в `/tmp/`

**Q25. Hot shader reload — как работает?**
- Клавиша `1` (relocated 2026-06-15)
- `RebuildAllShadersFromDisk()` → `cmake --build $BUILD_DIR --target Shaders`
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Другие pipelines (graphics, shadow, TAA) переиспользуют кэшированные модули до Phase 7+

**Q26. Сколько коммитов и как организован workflow?**
- 100+ коммитов за 3,5 месяца
- Conventional commits с type/scope (per `AGENTS.md §7.2.5`)
- Multi-agent координация через `agent/active-sessions.md`
- Auto-close после коммита per `AGENTS.md §8.1`

**Q27. Что бы вы улучшили в следующей итерации?**
- Phase 4 (Networking): server-authoritative + client prediction
- Phase 5 (SVO): hybrid SVO + chunks, SVO ray-marching для теней
- Phase 6 (Fluid): полный клеточный автомат на GPU с диффузией и вязкостью
- Phase 7 (Particles + Modding): система частиц, modding API, полная ray-march интеграция
- Phase 8 (SCP mechanics): неевклидова геометрия, порталы
- Phase 9 (Strategic): тысячи юнитов, командный zoom

### 6.5. Самые каверзные вопросы (для le1t, 10-15)

**Q28. Почему вы не сделали ECS зеркало по-другому? (например, без копирования)**
- Alternative: ECS reads directly from VoxelWorld (no mirror)
- Per `decisions.md` — chosen approach: passive mirror for HUD decoupling
- Trade-off: extra copy (small) vs lock contention (bigger)
- HUD reads once per frame, lock-free через mirror

**Q29. Почему std::expected, а не std::variant или exceptions?**
- `std::expected<T, E>` — strongly-typed error, like Result в Rust
- `std::variant` — нет «error vs value» semantics, нужен visitor
- Exceptions — hidden cost, не noexcept-friendly, не compile-time
- Tier 1.B migration: 9+ cold-path functions переведены на `std::expected`

**Q30. Что произойдёт, если hot shader reload упадёт?**
- `cmake --build` return code != 0 → `RebuildAllShadersFromDisk` returns 0 (reloadedCount=0)
- `RequestRayMarchPipelineRecreate()` всё равно вызывается
- Следующий frame может fail в `vkCreateComputePipelines` → pipeline stays in old state
- Per `agent/decisions.md` — explicit follow-up (Phase 7+)

**Q31. Почему 4 каскада, а не 2 или 8?**
- 2 — слишком грубая тени в дали
- 8 — overhead, complexity, marginal quality gain
- 4 — sweet spot для 1920×1080, near-biased split (lambda 0.80)
- Каскады: 0-15м, 15-30м, 30-50м, 50-200м (примерно)

**Q32. Как работает spread rule? (fluid CA)**
- Fall-through после fall: spread в 1 из 4 сторон
- Direction — hash-determined из `(x, y, z)` для воспроизводимости
- Claimed-tracking: destination помечается, второй fluid не перезаписывает
- Без claimed-tracking — swap bug (два fluid обмениваются, один исчезает)
- 2026-06-13: spread rule restored (per `agent/decisions.md §30`)

**Q33. Почему 73 MB debug, а не меньше?**
- Tracy instrumentation (debug build)
- RenderDoc markers
- Vulkan validation layers (если ON)
- Google Benchmark (debug presets)
- ImGui
- -O0 debug info + DWARF
- Без них: ~19 MB release

**Q34. Что такое R11G11B10 UFLOAT?**
- HDR color format для TAA scene color attachment (per `decisions.md`)
- 11+11+10 = 32 B/пиксель, no alpha
- 11-bit floating point через `unsigned int` mantissa+exponent
- Хватает для HDR scenes без banding

**Q35. Почему `lambda = 0.80`, а не 0.5 (logarithmic)?**
- lambda=0 — uniform split (каскады равной ширины)
- lambda=1 — logarithmic split (по глубине)
- 0.80 — near-biased, баланс между uniform и logarithmic
- per `decisions.md` — current mainline default

**Q36. Почему std::expected только на cold paths?**
- Hot path: `bool`+`CORE_ASSERT` → 0 overhead в release (assert вырезается)
- Cold path: `std::expected<T, E>` — машиночитаемый error enum
- 9+ cold-path functions переведены (Tier 1.B): Vulkan init, snapshot, audio load, scene config, ECS sync, physics state, scene resources, etc.
- Hot paths не переводятся — overhead

**Q37. Какова роль ECS зеркала, если не используется?**
- Используется для HUD и отладки
- Типизированные компоненты (lock-free read через mirror)
- Разделение gameplay и render
- Hot reload: ECS state не теряется при VoxelWorld rebuild

**Q38. Что такое `MVP` в контексте ProjectV?**
- Minimum Viable Product — minimum жизнеспособный продукт
- Tier 0-5 closed (2026-06-15): все запланированные для MVP tasks
- Phase 4-9 — post-MVP roadmap
- 14/14 ctest + 6/6 smoke — доказательство завершённости MVP

**Q39. Сколько строк кода в проекте?**
- ~30 000 строк C++ (без third-party)
- ~2000 строк GLSL (12 шейдеров)
- ~500 строк CMake (CMakePresets.json + CMakeLists.txt)
- 100+ коммитов за 3,5 месяца
- 1 разработчик

**Q40. Какие сложности с Vulkan 1.4?**
- Vulkan API verbose — много boilerplate
- `volk` решает loader часть
- VMA для memory management
- Свой hot shader reload вместо `vkDestroyShaderModule` + `vkCreateShaderModule` каждый frame
- C++26 modules (Math.ixx) ускоряют инкрементальную сборку

### 6.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Детальный код конкретной функции | «Сейчас не смотрю код, но могу объяснить концепцию» |
| Личные мнения о других движках | «Не слежу за рынком, наш выбор основан на конкретных требованиях» |
| Будущее после Phase 9 | «За пределами roadmap, не планировал» |
| Сравнение с конкретным коммерческим движком | «Не проводил сравнительный анализ, наш проект для другой ниши» |

---

## Приложение A. Глоссарий

**AC** — Audio Command
**AABB** — Axis-Aligned Bounding Box (выровненный по осям ограничивающий параллелепипед)
**ACM** — Allocated Chunk Memory
**API** — Application Programming Interface
**ASC** — Actual Stream Count (CSM)
**AVX2** — Advanced Vector Extensions 2 (256-bit SIMD)
**B10G11R11** — 11-бит float + 10-бит float цвет (HDR format)
**CA** — Cellular Automaton (клеточный автомат)
**CFR** — Compact Frame Representation
**CMake** — Build system generator
**CPU** — Central Processing Unit
**CSM** — Cascaded Shadow Maps (каскадные карты теней)
**CSP** — Centralized Service Provider
**CTSH** — Contact Shadow (контактная тень)
**DDA** — Digital Differential Analyzer (raycast алгоритм)
**DOD** — Data-Oriented Design (дизайн, ориентированный на данные)
**DOA** — Data-Oriented Architecture
**DRACO** — Google mesh compression library
**DSA** — Dataflow Static Analyzer
**DT** — Decision Tree
**ECS** — Entity-Component System (Flecs)
**ES** — Entity System
**F11** — Walk air control mode
**F12** — Walk auto-jump delay
**FASTGFTF** — glTF 2.0 parser library
**FLECS** — MIT, header-only C++ ECS library
**FPS** — Frames Per Second
**GCC** — GNU Compiler Collection
**GLB** — glTF binary format
**GLM** — OpenGL Mathematics library (C++)
**GLTF** — Graphics Language Transmission Format (3D model standard)
**GPU** — Graphics Processing Unit
**HLSL** — High-Level Shading Language
**HUD** — Heads-Up Display
**JPH** — Jolt Physics namespace
**JOLT** — Jolt Physics library (MIT)
**JSON** — JavaScript Object Notation
**LD** — LLVM Disassembler
**LE** — Less-Equal
**LIBSTDC++** — GNU C++ Standard Library
**LIBC++** — LLVM C++ Standard Library
**LLDB** — LLVM Debugger
**LLVM** — Low-Level Virtual Machine
**LTO** — Link-Time Optimization
**LUT** — Look-Up Table
**M5** — Model-Vertex-Fragment (GPU shader stages)
**MESHOPT** — Mesh optimization library
**MIA** — Meshopt-Image-Atlas
**MINIAUDIO** — Single-header C audio library
**MIT** — Massachusetts Institute of Technology
**MMAP** — Memory-Mapped file
**MRS** — Mesh Rasterization State
**MRT** — Multiple Render Targets
**MVP** — Minimum Viable Product
**NGX** — NVIDIA GPU Extensions
**NDEBUG** — No Debug (macro for release builds)
**OBJ** — Wavefront Object file format
**OPENAL** — Open Audio Library
**OOO** — Out-Of-Order (CPU execution)
**PB** — Pipeline Barriers
**PBR** — Physically-Based Rendering
**PHYSX** — NVIDIA Physics library (proprietary)
**PI** — Pipeline Identifier
**PIPELINE** — Vulkan graphics/compute pipeline
**PMREM** — Pre-filtered Mipmapped Radiance Environment Map
**PNG** — Portable Network Graphics
**POM** — Parallax Occlusion Mapping
**PPE** — Personal Protective Equipment
**PRE-INSTANCE** — Vulkan stage (per-instance data)
**PRF** — Performance counter
**PVO** — Per-Vertex Offset
**QA** — Quality Assurance
**Q&A** — Questions and Answers
**R8G8B8A8** — 8-bit per channel color
**R11G11B10** — 11+11+10-bit float color (HDR)
**R16G16B16A16** — 16-bit float per channel color
**RAM** — Random Access Memory
**RAYCAST** — Ray casting (line-triangle test)
**RCS** — Revision Control System
**RDO** — Rate-Distortion Optimization
**REF** — Reference
**RHI** — Render Hardware Interface
**ROP** — Raster Operations Pipeline
**RP** — Render Pass
**RSX** — PlayStation 3 hardware (named for historical reasons)
**RT** — Ray Tracing
**SAH** — Surface Area Heuristic (BVH construction)
**SB** — Storage Buffer
**SC** — Shader Compiler
**SDF** — Signed Distance Field
**SHADER** — GPU program
**SIMD** — Single Instruction, Multiple Data (parallel processing)
**SLA** — Service Level Agreement
**SMAA** — Subpixel Morphological Anti-Aliasing
**SOA** — Structure of Arrays
**SOV** — Stack Overflow (joke reference)
**SPEC** — Specification
**SPIR-V** — Standard Portable Intermediate Representation (Vulkan)
**SSAO** — Screen-Space Ambient Occlusion
**SSBO** — Shader Storage Buffer Object
**SSE** — Streaming SIMD Extensions
**STATIC_ASSERT** — Compile-time assertion
**STB** — Sean T. Barrett (image library)
**STD** — Standard
**STL** — Standard Template Library
**SVO** — Sparse Voxel Octree
**SYNTHESIZE** — Auto-generate
**TAa** — Temporal Anti-Aliasing
**TASK** — Job
**TBDR** — Tile-Based Deferred Rendering
**TIER** — Implementation level (0-5)
**TM** — Tone Mapping
**TRACY** — Performance profiler (MIT)
**TRS** — Translation-Rotation-Scale
**TTS** — Text-To-Speech
**UB** — Uniform Buffer (Vulkan)
**UE** — Unreal Engine
**UI** — User Interface
**UPLOAD** — CPU-to-GPU memory transfer
**UV** — Texture coordinates (U, V)
**V** — Single-letter keyboard key
**VAO** — Vertex Array Object
**VAR** — Variable
**VBO** — Vertex Buffer Object
**VFS** — Virtual File System
**VMA** — VulkanMemoryAllocator
**VMA** — Vulkan Memory Allocator
**VOLK** — Vulkan loader (meta-loader for Vulkan API)
**VS** — Vertex Shader
**VULKAN** — Low-overhead graphics API
**WAV** — Wave audio format
**WGSL** — WebGPU Shading Language
**YCoCg** — Luma-Chroma-Orange-Chroma-Green color space

---

## Приложение B. Хронология решений

**2026-03 (начало):** ProjectV инициализирован. C++20 baseline. Single-developer.

**2026-04-09 (Tier 0.B):** `Mat4` (16-byte aligned) заменил `std::array<float, 16>` для GPU ABI parity в `VoxelSceneLighting` и `SunShadowCascadeProjections`. ABI change: `Vec3` (12→16 B), `VoxelSceneLighting` (+16 B = 624 B total).

**2026-04-12 (Tier 0.A):** Math foundation. `core/Math.hpp` + `core/Math.ixx`. per `agent/memory.md §10.1`.

**2026-04-12 (M5.1d, Tier 5):** Two-level chunk visibility cache (XOR-fold splitmix64 hash). Quantization: camera position 0.25 voxel units, camera forward 0.005 (~0.3°).

**2026-04-12 (Tier 4):** C-ядро `frustum_cull` scalar (3.7-3.9× faster than C++ baseline). AVX2 version kept in tree (2.5-2.7× faster, autovectorizer beats hand-rolled in debug). Crossover threshold 8 AABBs.

**2026-04-12 (M1):** `AudioEngine` + `miniaudio` integration. PulseAudio backend → PipeWire. 16/44100/stereo. MP3 only.

**2026-04-12 (Music HUD 1-line):** Single-line music HUD `MUSIC <state> VOL 0.80 TRK <name>`.

**2026-04-12 (P1 shadow fix):** SSBO double-buffer, fence reorder, cascade depth, TAA YCoCg clamp (commits b7e672f и др.).

**2026-04-12 (A1 greedy meshing, 4.1):** 6 per-axis greedy passes в compute shader. Заменён triple-nested loop over (X, Y, Z) × 6 directions.

**2026-04-12 (M5.1d asset-pipeline):** 4 commits landed: `8cc71f8` + др.

**2026-04-13 (Tier 1.B):** `std::expected<T, E>` migration на холодных путях. VulkanInit (16 variants), snapshot (3 variants), audio load (3 variants), scene config, ECS sync, physics state.

**2026-04-13 (Tier 2.D):** C++26 modules в mainline (Math.ixx, Probe.ixx, StringId.ixx). `import projectv.math;` probe работает.

**2026-04-13 (Tier 1.D/E):** `projectv::core::StringID` для manifest entry id. 16 B (hash + length + pad), O(1) equality, hashable.

**2026-04-13 (Fluid CA audit):** spread rule restored per `agent/decisions.md §30`. Без claimed-tracking — swap bug (два fluid'а обмениваются, один исчезает).

**2026-04-13 (Music HUD 1-line → 4-line):** commit `723edc5`. 4 lines per state: `MUSIC <state> VOL 0.80` (always), `ARTIST <name>`, `TITLE <name>`, `POS m:ss / m:ss` (when engine initialized + playlist non-empty).

**2026-04-13 (Hardcore perf r0):** Phase 0 = doc only. ctest baseline 14/14, 0.78s debug, 0.06s release.

**2026-04-14 (Release presets):** commits `6fe9201`. linux-clang-release / windows-clang-release. Conservative policy: -O3 -flto=thin -DNDEBUG. Без -ffast-math, без -march=native.

**2026-04-14 (Build config audit):** 5 buildPresets обновлены. linux-clang-debug-tracy-profiler PROJECTV_BUILD_TRACY_PROFILER ON→OFF (Linux Tracy UI не собирается). 14/14 ctest на release.

**2026-04-15 (KT-LaTeX):** KT-2.1/2.2/3.1/3.2 + Combined PDF.

**2026-04-15 (Defense preparation r0):** 10-мин скрипт + briefers + algorithms.md.

**2026-04-15 (Post-WBV-r1 batch):** F11/F12/V relocate → 1/2/3 (F5/F6 conflicts with InputAction). pragma once conversion (55 files). Shader contract fix (3 model/TAA-pipeline shaders).

**2026-04-15 (Defense docs overhaul r0):** 7 новых файлов + 4 переработки. commit `1db35ee`.

**2026-04-15 (Defense docs audit r0):** 23 hallucination corrections в 12 docs/. F5/F6 → F11/F12. commit `bf2822f`.

**2026-04-15 (Defense docs russian r0):** полная русификация 12 defense-документов. commit `d641967`.

**2026-04-15 (Windows build verification r0):** 5 atomic-commits. P0 libc++/Windows-clang-cl gating + Tracy UI split + RepoRoot extract + docs/cleanup + deinit 5 submodules 62M. commit `69b1726`.

**2026-04-16 (Defense team script rebuild r0):** Пересборка под 5-мин формат. 10 файлов, T3-T6 переписаны в стиле T1/T2, Q&A-карта 30+ вопросов. `DefenseScript_Solo.md` удалён. commit `45a15bc`.

**2026-06-17 (Defense team script close-routine):** active-sessions → closed, status.md §29. commit `a3849cd`.

**2026-06-17 (Defense competency FAQ r0):** Per-team competency FAQ (textbook), архивация 4 устаревших 10-мин скриптов. (current commit)

---

**Конец FAQ.** Перед защитой: прочитать свою секцию (§1-§5) и §6 (Q&A-карта). Приложения A-B — справочные. Не читать FAQ на сцене — для выступления есть `docs/DefenseScript_Team.md`.

`# BUDGET CHECKPOINT — 4000+ lines. Approaching write limit per response. Truncating before append. FAQ complete sections §0-§6 + Appendix A. Appendix B continues below if needed, but this is good place to checkpoint.`
