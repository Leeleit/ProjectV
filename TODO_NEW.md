# ТЕХНИЧЕСКИЙ ДОЛГ И АУДИТ PROJECTV (2026-06-21)

## Введение и назначение файла

Файл **`TODO_NEW.md`** — параллельный аудит-driven трекер техдолга. Создан по результатам работы 5 сабагентов-аудиторов
от
`2026-06-21`. **Не заменяет** `TODO.md` (feature roadmap по ЭТАП 1-6) и **не дублирует** его. Согласно
`AGENTS.md §4` (таблица классификации информации), этот файл попадает в категорию «Roadmap, приоритеты, риски, чекбоксы»
и служит для отслеживания устранения **только** аудит-находок.

**Принципы:**

- **1 issue = 1 задача.** Каждой находке аудита соответствует отдельная задача с ID вида `AUDIT-<scope>-<NNN>`.
- **Source of truth** — отчёт сабагента (см. `agent/workspace.md §1`, входы от сабагентов с ID
  `356100e9-…`, `4447696f-…`, `7df98bf4-…`, `dbafe47a-…`, `00dd21fc-…`).
- **Никаких «ритуальных» sync-пассов** по `TODO.md` / `workspace.md` / `knowledge.md` после создания этого файла
  (см. `AGENTS.md §5.2`).
- **Закрытие задачи** — через commit в mainline, помечающий задачу как `✅ Closed` в этом файле. Без commit-промпта
  (см. `AGENTS.md §5.4`, `§5.9`).

**Связанные источники:**

- `AGENTS.md` — протокол работы.
- `agent/knowledge.md Part A` — действующие engineering contracts (build target invariant §4, Jolt include order §4,
  VoxelSceneLighting byte-exact contract §15, etc.).
- `agent/workspace.md` — текущий снимок активных сессий.
- `TODO.md` — фичевый roadmap (этапы 1-6). Перекрёстные ссылки на задачи TODO.md даны в `Cross-refs` где возможно.
- `docs/VulkanSDK-Linux-Docs-1.4.350.1/` — вендорная документация Vulkan 1.4 для рендер-задач.

---

## Сводная таблица (executive summary)

| Severity                     | Core/ECS/App | Physics/Voxel | Vulkan/Render | Tests/CMake | Docs sync | **Итого** |
|:-----------------------------|:------------:|:-------------:|:-------------:|:-----------:|:---------:|:---------:|
| **High**                     |      1       |       0       |       1       |      1      |     0     |   **3**   |
| **Medium**                   |      5       |       4       |       5       |      3      |     6     |  **23**   |
| **Medium-Low**               |      0       |       1       |       0       |      0      |     0     |   **1**   |
| **Low**                      |      6       |       4       |       6       |      4      |     4     |  **24**   |
| **Info / N-A / false alarm** |      3       |       1       |       0       |      2      |     2     |   **8**   |
| **Итого**                    |      15      |      10       |      12       |     10      |    12     |  **59**   |

**Top-3 по приоритету (рекомендуемый порядок устранения):**

1. `AUDIT-VK-001` — `SceneResources.cpp` `GrowNanoVdbBuffer`: GPU use-after-free при grow. **High.** Риск визуальной
   коррапты + Validation Layer ошибок.
2. `AUDIT-CORE-001` — `Camera.cpp` `UpdateCamera`: деление на ноль при `windowHeight == 0` (свёрнутое окно). **High.**
   Падение `aspectRatio` в `inf` → коррапция projection matrix.
3. `AUDIT-TC-001` — `tests/CMakeLists.txt`: `ProjectVTests` + `ProjectVFluidCATests` link errors. **High.** 2
   тест-таргета
   permanently broken, 2 pre-existing failures во всех ctest-прогонах.

**Сквозные паттерны (cross-cutting):** часть находок относится к одним и тем же fix-pattern'ам и должна устраняться
пачками:

- **Документирование ownership raw-указателей в `RenderState`** — `AUDIT-CORE-002`, `AUDIT-CORE-003` (одна
  design-rationale правка в `Types.hpp` + `Types.cpp`).
- **Добавление `[[nodiscard]]`** — `AUDIT-CORE-012` (multiple files, ~10 функций).
- **Добавление EVIL-маркеров** — `AUDIT-CORE-009`, `AUDIT-VK-010`, `AUDIT-TC-008` (magic numbers в benchmarks/push
  consts).
- **CMakePresets.json backfill** — `AUDIT-TC-002`, `AUDIT-TC-004` (после `AUDIT-TC-001` фикса необходимо
  пересчитать target count).
- **TODO.md/workspace.md sync** — все 6 Medium-задач `AUDIT-DOC-*` могут быть закрыты одним doc-sync commit'ом.

---

# Глава 1. Core / ECS / App / Audio / Asset / Platform / Debug / Bench / c_kernels

> Сабагент: `356100e9-d7c7-42b2-9fd3-f586f50d4fff (Core/ECS/App Auditor)`.
> Файлы: `src/core/`, `src/ecs/`, `src/app/`, `src/audio/`, `src/asset/`, `src/platform/`, `src/debug/`, `src/bench/`,
> `src/c_kernels/`.

## High severity

### AUDIT-CORE-001 — `Camera.cpp` `UpdateCamera`: деление на ноль при `windowHeight == 0`

* **Файл / строки:** `src/app/Camera.cpp:55-60` (функция `UpdateCamera`).
* **Описание:** `camera.aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight)`. При
  свёрнутом окне `windowHeight == 0` → деление на ноль → `aspectRatio == +inf` → `inf` пропагируется в projection matrix
  и далее в culling/rendering пайплайн. Может приводить к crash или визуальной коррапции.
* **Severity rationale:** High — легко воспроизводится (Alt+Tab → minimize), пользовательский сценарий.
* **Предлагаемый фикс:** early-out guard: `if (windowHeight == 0) return;` (предпочтительно, т.к. projection на
  свёрнутом
  окне всё равно не используется) **либо** безопасное вычисление: `camera.aspectRatio = windowHeight > 0 ? ... : 1.0f;`.
* **DoD / Верификация:**
    * [ ] Добавить unit-test `TestCameraAspectRatioZeroWindowHeight` в `ProjectVTests` (или существующий camera test
      executable), проверяющий что вызов `UpdateCamera(camera, 1920, 0)` не устанавливает `aspectRatio = inf` и не
      падает.
    * [ ] Ручная проверка: свёрнутое окно → re-restore → кадр рендерится без артефактов.
* **Cross-refs:** `AGENTS.md §7.2` (philosophy §5 — expected errors explicit), `agent/knowledge.md Part B` (runtime
  facts
  про camera/SDL window lifecycle).
* **Effort estimate:** XS (один guard, один unit-test).
* **Статус:** 🔓 Open.

## Medium severity

### AUDIT-CORE-002 — `Types.cpp` `DestroyRenderState`: fragile raw-pointer ownership pattern (`rayTracedShadows`)

* **Файл / строки:** `src/core/Types.cpp:120-145` (внутри `DestroyRenderState`).
* **Описание:** `render.rayTracedShadows` — raw owning pointer, уничтожаемый через `DestroyRayTracedShadowResources`.
  Функция делает null-check, `delete` и `set nullptr`, но паттерн fragile: если `DestroyRenderState` вызывается без
  matching `Create` или init fails partway, можно наткнуться на dangling pointer. Сейчас не bug, но design-level
  fragility.
* **Severity rationale:** Medium — нет текущего bug, но design hazard при будущих рефакторингах.
* **Предлагаемый фикс:** `std::unique_ptr<RayTracedShadows, DestroyRayTracedShadowsDeleter>` с custom deleter
  (предпочтительно — RAII-safe, нет ручного null-check) **либо** явный ownership contract в комментариях
  (per `AGENTS.md §8.1` keep-markers, EVIL-marker не нужен, нужен `// owned by CreateRayTracedShadowResources`).
  Альтернатива — ownership документация в `COMMENTS.md` (см. `AUDIT-CORE-003`).
* **DoD / Верификация:**
    * [ ] Ownership pattern унифицирован для всех ~20 raw pointer fields в `RenderState` (не только `rayTracedShadows`).
    * [ ] Code review: каждый pointer field имеет либо `unique_ptr<…,CustomDeleter>`, либо ownership comment.
    * [ ] Leak check: запустить с `PROJECTV_ENABLE_VALIDATION=ON`, exit, убедиться в отсутствии
      `VK_OBJECT_LEAKS`-сообщений от Validation Layers.
* **Cross-refs:** `AGENTS.md §8.1` (keep markers), `AUDIT-CORE-003` (ownership docs).
* **Effort estimate:** S (один refactor session).
* **Статус:** 🔓 Open.

### AUDIT-CORE-003 — `Types.hpp` `RenderState`: ~20 forward-declared pointer fields без ownership docs

* **Файл / строки:** `src/core/Types.hpp:80-250` (struct `RenderState`).
* **Описание:** `RenderState` содержит ~20+ raw pointer fields (forward-declared типы), ни одно не имеет ownership
  аннотации. Maintenance hazard: новый код может случайно free/reassign без понимания lifecycle.
* **Severity rationale:** Medium — нет текущего bug, высокий maintenance risk.
* **Предлагаемый фикс:** добавить `// owned by CreateXxx/DestroyXxx` комментарий для каждого pointer field (это
  попадает под `AGENTS.md §8.1` keep-markers как design-rationale). **Альтернативно** — вынести ownership документацию
  в `COMMENTS.md` секцию `## src/core/Types.hpp` (per `AGENTS.md §8.3` формат), оставив в hpp короткий marker
  `// see COMMENTS.md for ownership contract`.
* **DoD / Верификация:**
    * [ ] Каждое raw pointer поле в `RenderState` имеет явный ownership marker (либо inline, либо cross-ref в
      `COMMENTS.md`).
    * [ ] `COMMENTS.md` секция `## src/core/Types.hpp` содержит mapping pointer → owning create/destroy function.
    * [ ] Grep-проверка: `rg "::\*\s*\w+\s*;" src/core/Types.hpp` показывает 0 полей без ownership comment.
* **Cross-refs:** `AGENTS.md §8.1` (keep-markers), `AGENTS.md §8.3` (COMMENTS.md формат), `AUDIT-CORE-002`.
* **Effort estimate:** S (механический проход по полям).
* **Статус:** 🔓 Open.

### AUDIT-CORE-004 — `AudioEngine.cpp`: race condition между `jthread` stop и `ma_engine_uninit`

* **Файл / строки:** `src/audio/AudioEngine.cpp:45-60` (деструктор `AudioEngine`).
* **Описание:** `std::jthread m_scanThread` корректно используется для RAII join. Однако деструктор вызывает
  `ma_engine_uninit(&m_engine)`, что может инвалидировать ресурсы, которые `m_scanThread` всё ещё использует, если
  scan loop в середине `ma_sound_init_from_file`. `jthread` деструктор запрашивает stop и join'ит, но ordering не
  гарантирован.
* **Severity rationale:** Medium — race condition на shutdown, не воспроизводится в happy path, но на медленных
  дисках / большом playlist возможен.
* **Предлагаемый фикс:** явно запросить stop и join `m_scanThread` **до** `ma_engine_uninit`:
  ```cpp
  if (m_scanThread.joinable()) {
      m_scanThread.request_stop();
      m_scanThread.join();
  }
  ma_engine_uninit(&m_engine);
  ```
  Дополнительно — добавить `std::stop_token` check в scan loop (для cooperative cancellation).
* **DoD / Верификация:**
    * [ ] TSan-clean прогон `ProjectVAudioEngineTests` + `ProjectVAppStateLifecycleTests` под
      `linux-clang-debug` с `-fsanitize=thread`.
    * [ ] Stress test: `kill -USR1 <pid>` (или аналог rapid shutdown) × 100 → 0 aborts, 0 leak reports от
      `ma_engine` (включить `MA_DEBUG_OUTPUT` если возможно).
    * [ ] Manual: 10000 треков в playlist → exit → проверка отсутствия segfault.
* **Cross-refs:** `AGENTS.md §7.2` (TSan requirement для AudioEngine, см. `TODO.md §1.3 DoD`),
  `agent/knowledge.md Part B §29.0` (cold path concurrency).
* **Effort estimate:** XS (5-10 LoC + TSan-верификация).
* **Статус:** 🔓 Open.

### AUDIT-CORE-005 — `Replay.cpp`: replay file loader без magic number / version check

* **Файл / строки:** `src/app/Replay.cpp:30-50` (file loader entry point).
* **Описание:** binary reader читает данные без проверки magic number или версии файла. Повреждённый или
  wrong-format файл → undefined behavior во время parsing.
* **Resolution:** Файл `src/app/Replay.cpp` **не существует** — actual file is `src/app/InputReplay.cpp`,
  и audit claim **уже закрыт** существующим кодом:
    * `InputReplay.cpp:14` — `constexpr std::string_view kInputReplayMagic = "PROJECTV_INPUT_REPLAY";`
    * `InputReplay.cpp:15` — `constexpr int kInputReplayVersion = 3;`
    * `InputReplay.cpp:130-134` — `ReadReplayCapture` rejects on magic mismatch или
      version != {1, 2, 3} с explicit `runtime::LogRuntimeFailure`.
  Magic + version check полностью присутствует, без багов.
* **Статус:** ✅ Closed (false alarm, code already implements audit's prescribed fix).
* **Resolution date:** 2026-06-21 (batch 2 fix pass)
* **Severity rationale:** Medium — cold path, но crash на bad input недопустим.
* **Предлагаемый фикс:** добавить magic number `PVRP` (4 байта) + `uint32_t version` в начало файла. Константы
  вынести в `ReplayFormat.hpp`:
  ```cpp
  static constexpr char kReplayMagic[4] = {'P', 'V', 'R', 'P'};
  static constexpr uint32_t kReplayVersion = 1;
  ```
  На open: прочитать header → reject если magic mismatch или version > kReplayVersion (forward-compatible reject).
* **DoD / Верификация:**
    * [ ] Новый test `TestReplayRejectsCorruptedFile` (truncated, random bytes, wrong magic, future version) в
      `ProjectVReplayTests` (или аналогичном).
    * [ ] Test `TestReplayAcceptsValidFile` (round-trip: serialize → load → compare).
    * [ ] Manual: попытка загрузить `.txt` файл как `.pvpr` → корректное error message, не crash.
* **Cross-refs:** `AGENTS.md §7.2` (philosophy §5 — expected errors explicit), `AGENTS.md §4` (use
  `std::expected<T,E>` для cold path).
* **Effort estimate:** S.
* **Статус:** 🔓 Open.

### AUDIT-CORE-006 — `LookDevCapture.cpp`: silent `std::ofstream` failure (sidecar metadata write)

* **Файл / строки:** `src/app/LookDevCapture.cpp:60-80` (sidecar write блок).
* **Описание:** file writes для sidecar metadata используют `std::ofstream` без проверки `is_open()` или `good()`
  после write. Disk full / invalid path → ошибка silently swallowed.
* **Resolution:** Audit ссылается на несуществующий файл. Actual sidecar write — в
  `src/render/ScreenshotCapture.cpp::SaveScreenshotCaptureMetadata`, и он уже **учитывает** failures:
    * `ScreenshotCapture.cpp:223` — `if (!stream) ... return false` после open
    * `ScreenshotCapture.cpp:418-421` — `if (!stream) ... return false` после всех writes (≈70 fmt::format calls)
  Disk full / invalid path errors корректно propagates через bool return + `runtime::LogRuntimeFailure`.
  Bug не существует.
* **Статус:** ✅ Closed (false alarm, code already implements audit's prescribed fix).
* **Resolution date:** 2026-06-21 (batch 2 fix pass)
* **Severity rationale:** Medium — cold path, нарушает `philosophy §5` (explicit expected errors).
* **Предлагаемый фикс:** обернуть sidecar write в helper, возвращающий `std::expected<void, std::string>`:
  ```cpp
  std::expected<void, std::string> WriteSidecarMetadata(const std::filesystem::path &path, const Metadata &m);
  ```
  При ошибке — `PV_LOG_ERROR` + propagate как `std::unexpected`. В capture-функции — fail screenshot если sidecar
  write failed (или warn-only mode через env-gate).
* **DoD / Верификация:**
    * [ ] Test `TestLookDevCaptureFailsOnReadOnlyPath` (записать sidecar в read-only directory → ожидать
      `std::unexpected` + log line).
    * [ ] Test `TestLookDevCaptureSucceedsOnValidPath` (round-trip).
    * [ ] Manual: disk full condition → screenshot fail с explicit error.
* **Cross-refs:** `AGENTS.md §4` (`std::expected<T,E>` для cold path), `AUDIT-CORE-005` (тот же паттерн).
* **Effort estimate:** XS-S.
* **Статус:** 🔓 Open.

## Low severity

### AUDIT-CORE-007 — `Input.cpp`: SDL key state transitions без double-keydown guard

* **Файл / строки:** `src/app/Input.cpp:120-200` (key state transition logic).
* **Resolution:** Файл `src/app/Input.cpp` **не существует**. Actual key event handling — в
  `src/app/InputActions.cpp` и использует **`down`/`pressed` bools per action** (line 56, 222-224),
  не explicit "Pressed/Released" state machine.   SDL event loop в `UpdateApp` корректно обрабатывает
  SDL_KEYDOWN / SDL_KEYUP пары через SDL's built-in keyboard repeat (line 199: `event.key.repeat` check).
  Bug не существует.
* **Статус:** ✅ Closed (false alarm, code uses action-pressed pattern + SDL repeat filter).
* **Resolution date:** 2026-06-21 (batch 5 fix pass)
* **Описание:** переходы key-up → key-down не валидируют, что key-up действительно следовал за key-down. Двойные
  key-down события (SDL может генерировать в edge cases) могут оставить input system в inconsistent state.
* **Severity rationale:** Low — unlikely в practice, SDL обрабатывает это корректно.
* **Предлагаемый фикс:** добавить guard: переход в `released` только из `pressed` state:
  ```cpp
  if (newState == KeyState::Released && currentState != KeyState::Pressed) {
      // Skip spurious event
      return;
  }
  ```
* **DoD / Верификация:**
    * [ ] Добавить unit-test симулирующий SDL double-keydown event sequence → проверить state machine.
* **Cross-refs:** —.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-CORE-008 — `EcsWorld.cpp`: implicit system ordering dependency

* **Файл / строки:** `src/ecs/EcsWorld.cpp:80-150` (system registration).
* **Описание:** ECS-системы зарегистрированы в определённом порядке, execution зависит от него. Добавление новой
  системы между существующими может сломать implicit ordering assumptions. В текущем коде порядок задокументирован в
  структуре файла, но flecs phases не используются явно.
* **Severity rationale:** Low — design pattern, не bug.
* **Предлагаемый фикс:** добавить explicit phase/ordering аннотации per flecs best practices:
  `ecs_ordered_set(flecs::OnLoad)`, `flecs::PreUpdate`, `OnUpdate`, `OnPostUpdate`, `OnStore`. Для каждой системы —
  явный `world.system<...>().kind(flecs::OnUpdate).depends(...)` chain.
* **DoD / Верификация:**
    * [ ] Code review: каждая `world.system<...>()` вызов имеет явный `.kind(...)` (не default).
    * [ ] Все implicit ordering задокументированы через `.depends(otherSystem)`.
    * [ ] `ProjectVEcsWorldTests` (если существует) → green.
* **Cross-refs:** `TODO.md §6.1` (ECS migration contract), `agent/knowledge.md Part A` (flecs contracts).
* **Effort estimate:** S-M (refactor, но механический).
* **Статус:** 🔓 Open.

### AUDIT-CORE-009 — `BenchmarkAutomation.cpp`: hard-coded frame count thresholds без EVIL markers

* **Файл / строки:** `src/app/BenchmarkAutomation.cpp:25-30` (warmup/measurement constants).
* **Описание:** `100` frames warmup, `500` measurement — hard-coded magic numbers без named constants и EVIL markers.
* **Severity rationale:** Low — bench tooling, не production code, но нарушает `AGENTS.md §8.1`.
* **Предлагаемый фикс:** вынести в named constants с EVIL markers:
  ```cpp
  // EVIL: warmup frame count for benchmark steady-state. Profilers/jit cache need ~100 frames.
  static constexpr uint32_t kBenchWarmupFrameCount = 100;
  // EVIL: measurement sample count. Higher = better statistics, longer CI. 500 = 8.3s at 60fps.
  static constexpr uint32_t kBenchMeasurementFrameCount = 500;
  ```
* **DoD / Верификация:**
    * [ ] Grep `rg "100|500" src/app/BenchmarkAutomation.cpp` → 0 magic numbers в benchmark context.
    * [ ] `ProjectVBenchmarks` (или аналогичный test) → green.
* **Cross-refs:** `AGENTS.md §8.1` (EVIL markers), `legacy/docs/standards/04_evil-hacks-philosophy.md`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-CORE-010 — `c_kernels/`: undocumented alignment requirements

* **Файл / строки:** `src/c_kernels/` directory (все C kernel files).
* **Описание:** C kernel files работают с raw voxel data. Если data pointers не выровнены по ожидаемому alignment
  (например, `alignas(16)` для SIMD), kernels могут crash или давать wrong results на strict-alignment архитектурах.
  Сейчас x86 lenient, но код должен быть correct.
* **Severity rationale:** Low — нет текущего bug, latent risk для портирования на ARM/mobile.
* **Предлагаемый фикс:** задокументировать alignment requirements в function signatures (Doxygen-комментарии или
  contract comment в `COMMENTS.md` per `AGENTS.md §8.3`):
  ```c
  /// @param voxels Pointer to 16-byte aligned voxel array.
  /// @pre voxels must be aligned to 16 bytes (alignof(VoxelT) constraint).
  void Kernel_ProcessVoxels(VoxelT *voxels, size_t count);
  ```
  Дополнительно — assert на alignment в debug build: `assert((reinterpret_cast<uintptr_t>(voxels) % 16) == 0)`.
* **DoD / Верификация:**
    * [ ] Каждая kernel функция имеет documented alignment contract.
    * [ ] `COMMENTS.md` секция `## src/c_kernels/` описывает общий alignment policy.
    * [ ] Debug-only alignment assert в каждом kernel entry point.
* **Cross-refs:** `AGENTS.md §8.3` (COMMENTS.md формат).
* **Effort estimate:** S.
* **Статус:** 🔓 Open.

### AUDIT-CORE-011 — `Platform.cpp`: SDL window creation error handling не покрывает все error codes

* **Файл / строки:** `src/platform/Platform.cpp` (SDL_CreateWindow path).
* **Описание:** error handling для `SDL_CreateWindow` — простой null check. SDL-specific error codes не
  инспектируются для detailed diagnostics.
* **Severity rationale:** Low — null check sufficient для crash prevention, но диагностика бедная.
* **Предлагаемый фикс:** после `SDL_CreateWindow` failure, вызвать `SDL_GetError()` + записать в `PV_LOG_ERROR` с
  категорией и `std::error_code`:
* **Resolution:** Actual call site — `src/render/vulkan/VulkanBootstrap.cpp:625-629` (VulkanBase init,
  не `Platform.cpp`). Уже использует `runtime::LogSdlFailure` после null check, который внутри
  вызывает `SDL_GetError()` и propagate message через `LogRuntimeFailure`:
  `src/core/RuntimeDiagnostics.cpp:46-53`:
  ```cpp
  void LogSdlFailure(const std::string_view step) {
      const char *error = SDL_GetError();
      LogRuntimeFailure("SDL", step,
          error && *error ? error : "SDL_GetError returned an empty message");
  }
  ```
  SDL_GetError() уже вызывается и message логируется. Bug не существует.
* **Статус:** ✅ Closed (false alarm, code already implements audit's prescribed fix).
* **Resolution date:** 2026-06-21 (batch 2 fix pass)
  ```cpp
  if (!window) {
      PV_LOG_ERROR("Platform", "SDL_CreateWindow failed: {}", SDL_GetError());
      return std::unexpected(std::make_error_code(std::errc::io_error));
  }
  ```
* **DoD / Верификация:**
    * [ ] Test `TestPlatformWindowCreationFailsGracefully` (mock SDL failure) → expect `std::unexpected` + log.
    * [ ] Manual: попытаться создать window с невалидными параметрами → expect readable error message.
* **Cross-refs:** `AUDIT-CORE-005`, `AUDIT-CORE-006` (cold path error handling паттерн).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-CORE-012 — Multiple files: `[[nodiscard]]` отсутствует на error-indicating returns

* **Файлы:** `src/app/`, `src/audio/`, `src/core/` (различные).
* **Описание:** несколько функций возвращают `bool` или pointer types, индицирующие success/failure, но не помечены
  `[[nodiscard]]`. Это нарушает `philosophy §5` (expected errors explicit) и приводит к silent ignore.
* **Severity rationale:** Low — нет текущего bug от этого, но design hazard.
* **Предлагаемый фикс:** добавить `[[nodiscard]]` ко всем error-indicating returns:
    * `AudioEngine::Initialize() -> bool`
    * `Camera::UpdateCamera() -> bool`
    * `AssetLoader::Load*() -> bool/Result`
    * `Platform::*() -> std::expected<...>` / `bool`
    * любые другие `bool`-returns в cold path.
* **DoD / Верификация:**
    * [ ] `rg "bool\s+\w+\s*\(" src/audio/ src/app/ src/core/ | rg -v "// "` → review каждой сигнатуры, добавить
      `[[nodiscard]]` где return value индицирует success/failure.
    * [ ] Compile with `-Wunused-result` (или `-Werror=unused-result`) после фикса — clean.
* **Cross-refs:** `AGENTS.md §7.2` (philosophy §5), `AGENTS.md §4` (cold path contract).
* **Effort estimate:** XS (механический проход).
* **Статус:** 🔓 Open.

## Info / N-A / false alarm

### AUDIT-CORE-013 — `AssetLoader`: Draco decoder hooks — stubs (known future work)

* **Файл / строки:** `src/asset/AssetLoader.cpp` (Draco integration points).
* **Описание:** Draco decoder integration — placeholder/stub код. Функции возвращают early без actual decoding.
* **Severity rationale:** Info — known future work, не bug.
* **Предлагаемый фикс:** оставить как есть, добавить `// TODO: integrate Draco decoder` marker (это
  `AGENTS.md §8.1` allowed marker). Опционально — отдельная задача на Draco integration в
  `TODO.md R&D Backlog`.
* **Cross-refs:** —.
* **Effort estimate:** — (deferred).
* **Статус:** 🔓 Open (tracked elsewhere as future work).

### AUDIT-CORE-014 — `FramePreparation.cpp`: missing include guard check (false alarm)

* **Файл / строки:** `src/app/FramePreparation.cpp`.
* **Описание:** аудит-сабагент отметил «отсутствующие include guards», но файл `.cpp` — include guards не нужны.
  Includes большого числа headers не являются проблемой.
* **Severity rationale:** N-A (false alarm при детальной проверке).
* **Предлагаемый фикс:** нет действия.
* **Статус:** ✅ Closed (false alarm, no action required).

### AUDIT-CORE-015 — `Profiling.hpp`: Tracy macros (false alarm)

* **Файл / строки:** `src/debug/Profiling.hpp:1-30`.
* **Описание:** файл корректно gate'ит Tracy macros за `PROJECTV_ENABLE_TRACY`. При выключенном Tracy макросы
  expand в no-ops. Проблемы нет.
* **Severity rationale:** N-A (no issue).
* **Предлагаемый фикс:** нет действия.
* **Статус:** ✅ Closed (false alarm, no action required).

---

# Глава 2. Physics & Voxel

> Сабагент: `7df98bf4-0310-475b-b4be-7ce40828e270 (Physics/Voxel Auditor)`.
> Файлы: `src/physics/`, `src/voxel/`, `src/c_kernels/` (GreedyPhysicsMerger).

## Medium severity

### AUDIT-PV-001 — `PhysicsWorld.cpp`: `SyncPhysicsWorld` incremental path не инвалидирует cached walk support явно

* **Файл / строки:** `src/physics/PhysicsWorld.cpp:480-530` (`SyncPhysicsWorld` incremental path).
* **Описание:** per `agent/knowledge.md §5` (Interaction contract): «после успешного world-edit rebuild через
  `SyncPhysicsWorld` cached walk support ownership надо инвалидировать до следующего walk tick». Full-rebuild path
  устанавливает `physics->lastSyncedEditVersion = world.editVersion` и проходит через полную body replacement, что
  implicitly форсит новый contact check. Incremental path через `ProcessChunkRebuildQueue` +
  `RebuildStaticWorldBodyFromChunkShapes` заменяет static world body, что также форсит Jolt recompute contacts. **Но**
  нет явной инвалидации `walkSupport` cache state.
* **Severity rationale:** Medium — implicit invalidation может работать (Jolt body replacement -> contact recalc), но
  contract ambiguity.
* **Предлагаемый фикс:** verify что walk tick's `UpdateWalkGroundSupport` не cache'ит результаты между кадрами (likely
  recompute'ит каждый tick, что делает этот issue non-issue). **Если cache'ит** — добавить explicit invalidation:
  `physics->walkSupportCache.Invalidate();` в `RebuildStaticWorldBodyFromChunkShapes`. **Альтернативно** —
  задокументировать в `COMMENTS.md` (секция `## src/physics/PhysicsWorld.cpp`, range ~480-530) почему explicit
  invalidation не нужна.
* **DoD / Верификация:**
    * [ ] Code review `UpdateWalkGroundSupport` — verify per-tick recompute, не cross-frame cache.
    * [ ] `COMMENTS.md` обновлён с design-rationale.
    * [ ] `ProjectVPhysicsSyncTests` + `ProjectVPhysicsIncrementalJoltTests` → green.
* **Cross-refs:** `agent/knowledge.md Part A §5` (Interaction contract), `agent/knowledge.md Part A §6` (walk
  authority),
  `TODO.md §3.2` (Incremental Jolt).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-PV-002 — `PhysicsWorld.cpp`: `RebuildStaticWorldBodyFromChunkShapes` создаёт новое body на каждый edit

* **Файл / строки:** `src/physics/PhysicsWorld.cpp:420-470` (`RebuildStaticWorldBodyFromChunkShapes`).
* **Описание:** каждый вызов уничтожает старый `staticWorldBodyId` и создаёт новый. Это значит, что `staticWorldBodyId`
  меняется на каждый voxel edit. Код, который cache'ит body ID (e.g., contact filtering), должен lookup fresh
  каждый кадр. `IsPhysicsStaticWorldBodyId` helper существует, но pattern fragile. Дополнительно — новое body на
  каждый edit потенциально дорого для Jolt broad phase.
* **Severity rationale:** Medium — design fragility + potential performance hit.
* **Предлагаемый фикс:** использовать `BodyInterface::SetShape` для изменения shape существующего body (preserves
  body ID). Это Jolt best practice для mutable static bodies. **Если невозможно** (API constraint) — задокументировать
  в `COMMENTS.md` (секция `## src/physics/PhysicsWorld.cpp`).
* **DoD / Верификация:**
    * [ ] Если `SetShape` viable: профилирование через Tracy — broad phase cost должен снизиться.
    * [ ] `IsPhysicsStaticWorldBodyId` helper остаётся в силе (defense in depth).
    * [ ] `ProjectVPhysicsSyncTests` (incremental tests) → green.
* **Cross-refs:** `TODO.md §3.2` (Incremental Jolt Phase 6), `agent/knowledge.md Part A` (Jolt contracts).
* **Effort estimate:** M (требует тестирования Jolt API и broad phase behavior).
* **Статус:** 🔓 Open.

### AUDIT-PV-003 — `VoxelWorld.cpp`: `UpdateFluidCA` AABB bounds не валидируются против world bounds

* **Файл / строки:** `src/voxel/VoxelWorld.cpp:700-800` (`UpdateFluidCA`).
* **Описание:** AABB-bounded fluid CA update итерирует voxels в пределах `fluidAABB`. Нет валидации, что AABB
  coordinates не превышают actual world dimensions. Если чанк на границе мира имеет fluid, neighbor lookup может
  read out-of-bounds.
* **Severity rationale:** Medium — depends on `GetVoxelMaterial` bounds checking. Если возвращает `Air/empty` для
  out-of-bounds — safe. Нужно verify.
* **Предлагаемый фикс:**
    1. Verify `GetVoxelMaterial` корректно handles out-of-bounds coordinates (возвращает `Air`).
    2. **Если нет** — добавить clamp `fluidAABB` к world bounds перед iteration.
    3. **Defense in depth** — assert в debug: `assert(fluidAABB.max <= worldDimensions)`.
* **DoD / Верификация:**
    * [ ] Code review `GetVoxelMaterial` + `UpdateFluidCA` AABB computation.
    * [ ] Test `TestFluidCAOutOfBoundsAABB` (manually crafted world с fluid на границе) → не падает, корректный
      результат.
    * [ ] `ProjectVFluidCATests` → green.
* **Cross-refs:** `TODO.md §3.1` (Fluid CA), `agent/knowledge.md Part A §30.4` (Fluid CA migration steps).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-PV-004 — `PhysicsWorld.hpp`: `chunkMergedBoxes` map может расти unbounded

* **Файл / строки:** `src/physics/PhysicsWorld.hpp:50-60` (поле `chunkMergedBoxes`).
* **Описание:** `std::unordered_map<uint32_t, std::vector<MergedVoxelBox>> chunkMergedBoxes` растёт по мере
  загрузки чанков, но entries не удаляются при chunk unload. Slow memory leak в streaming world.
* **Severity rationale:** Medium — для streaming world это indefinite growth.
* **Предлагаемый фикс:** clear `chunkMergedBoxes` entries при chunk unload (в chunk streaming destroy path):
  ```cpp
  void OnChunkUnloaded(uint32_t chunkIndex) {
      chunkMergedBoxes.erase(chunkIndex);
      // ... existing destroy logic
  }
  ```
  Альтернативно — `chunkMergedBoxes.clear()` в `DestroyAllChunkStaticBodies` (если full destroy вызывается на
  unload).
* **DoD / Верификация:**
    * [ ] Code review chunk streaming destroy path.
    * [ ] Test `TestPhysicsChunkUnloadClearsMergedBoxes` (load → unload → verify map size = 0).
    * [ ] Long-running stress: 10000 chunk loads + unloads → memory stable.
* **Cross-refs:** `TODO.md §4.3` (Chunk Streaming), `AUDIT-PV-002` (related body lifecycle).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

## Medium-Low severity

### AUDIT-PV-005 — `VoxelWorld.cpp`: `editVersion` suppression для Fluid↔Air может вызвать stale NanoVDB

* **Файл / строки:** `src/voxel/VoxelWorld.cpp:350-380` (`SetVoxelMaterial`).
* **Описание:** 17x session'ом введено подавление `++world.editVersion` для fluid transitions. Это значит, что
  NanoVDB flatten (gated on `lastNanoVdbSyncedEditVersion != world.editVersion`) не сработает для fluid-only
  changes. Но fluid voxels влияют на NanoVDB flatten output. Если rendering path читает NanoVDB data, которая
  включает fluid state, displayed state может быть stale.
* **Severity rationale:** Medium-Low — NanoVDB env-gated (VCT + ray queries). При выключенных gates — non-issue.
  При включённых — fluid state в NanoVDB может быть stale до следующего non-fluid edit.
* **Предлагаемый фикс:** добавить separate `meshVersion` или `fluidVersion` counter, который всё ещё инкрементится
  для fluid changes (mesh rebuilds, но не physics sync). Альтернативно — ensure NanoVDB flatten triggered by
  mesh-dirty flags, не только `editVersion`. **Самый чистый** — dual-version scheme:
  ```cpp
  struct VoxelWorld {
      uint64_t editVersion;          // bumps for ALL changes (mesh rebuilds, etc.)
      uint64_t physicsSyncVersion;   // bumps only for physics-solid changes
  };
  ```
* **DoD / Верификация:**
    * [ ] Code review NanoVDB sync gate logic.
    * [ ] Test `TestFluidTransitionUpdatesNanoVdb` (fluid change → NanoVDB should reflect).
    * [ ] `ProjectVNanoVdbTests` + `ProjectVNanoVdbGpuUploadTests` → green.
* **Cross-refs:** `TODO.md §3.2` (17x Fluid editVersion suppress), `TODO.md §1.1` (NanoVDB),
  `agent/knowledge.md Part A §15` (VoxelSceneLighting contract — может быть affected).
* **Effort estimate:** S.
* **Статус:** 🔓 Open.

## Low severity

### AUDIT-PV-006 — `VoxelWorld.cpp`: `MarkChunksTouchedByVoxelEditDirty` hardcoded offsets без early-out для self-chunk

* **Файл / строки:** `src/voxel/VoxelWorld.cpp:300-340` (`MarkChunksTouchedByVoxelEditDirty`).
* **Описание:** функция итерирует 3×3×3 neighborhood (27 chunks) hardcoded offsets. Offsets correct (-1, 0, +1 per
  axis), но нет early-out для center chunk (offset 0,0,0) — он всегда dirty.
* **Severity rationale:** Low — center chunk корректно всегда marked dirty, просто нет minor optimization.
* **Предлагаемый фикс:** micro-optimization: `if (offset == {0, 0, 0}) continue;` — skip self-check.
* **DoD / Верификация:**
    * [ ] `ProjectVPhysicsIncrementalJoltTests` → green (regression check).
* **Cross-refs:** `TODO.md §3.2` (Incremental Jolt).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-PV-007 — `Sparse64Tree.hpp`: `DedupSubtree` recursive без explicit depth guard

* **Файл / строки:** `src/voxel/Sparse64Tree.hpp:200-250` (`DedupSubtree`).
* **Описание:** recursive walk для поиска duplicate subtrees. Tree depth bounded `chunkSize` (8 = depth 2), recursion
  depth max 2-3 levels. Safe для stack overflow. **Но** нет explicit depth guard.
* **Severity rationale:** Low — architecturally bounded.
* **Предлагаемый фикс:** добавить `// Tree depth bounded by chunkSize (8 = depth 2). Max recursion: 2-3 levels.`
  comment в `DedupSubtree` (per `AGENTS.md §8.1` keep-markers — design-rationale).
* **DoD / Верификация:**
    * [ ] Comment добавлен.
    * [ ] `ProjectVSparse64TreeTests` (если существует) → green.
* **Cross-refs:** `AGENTS.md §8.1`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-PV-008 — `NanoVdb.cpp`: `BuildNanoVdbFlatten` не handles partial chunk data

* **Файл / строки:** `src/voxel/NanoVdb.cpp:100-150` (`BuildNanoVdbFlatten`).
* **Описание:** flatten function assumes all chunks имеют fully populated voxel data. Если chunk в loading/streaming
  state с partial data, flatten может read uninitialized memory.
* **Severity rationale:** Low — `ChunkStreamer` should ensure chunks fully loaded до active. Но нет defensive check.
* **Предлагаемый фикс:** assert/guard что chunk's loading state complete до flattening:
  ```cpp
  assert(chunk.loadingState == ChunkLoadingState::Complete);
  ```
  Или skip chunks в non-Complete state.
* **DoD / Верификация:**
    * [ ] Debug-only assert добавлен.
    * [ ] `ProjectVNanoVdbTests` → green.
    * [ ] Manual: streaming world с mid-load chunk edit → graceful handling.
* **Cross-refs:** `TODO.md §1.1` (NanoVDB), `TODO.md §4.3` (Chunk Streaming).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-PV-009 — `GreedyPhysicsMerger.cpp`: volume preservation не проверяется at runtime

* **Файл / строки:** `src/physics/GreedyPhysicsMerger.cpp:50-100` (greedy merge core).
* **Описание:** greedy merge algorithm claims 100% volume preservation в tests, но нет runtime assertion что
  sum of merged box volumes == input voxel count. Если algorithm has bug в untested edge case, collision geometry
  может иметь gaps.
* **Severity rationale:** Low — tests покрывают это, но нет runtime safety net.
* **Предлагаемый фикс:** добавить debug-only assertion:
  ```cpp
  #ifndef NDEBUG
  PV_ASSERT(sumOfBoxVolumes == solidVoxelCount);
  #endif
  ```
* **DoD / Верификация:**
    * [ ] Debug-only assert добавлен.
    * [ ] `ProjectVPhysicsGreedyMergerTests` → green.
* **Cross-refs:** `TODO.md §3.3` (Greedy Physics Meshing), `agent/knowledge.md Part B` (volume preservation contract).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

## False alarm

### AUDIT-PV-000 — `GreedyPhysicsMerger.cpp`: Jolt include order violation (false alarm)

* **Файл / строки:** `src/physics/GreedyPhysicsMerger.cpp:1` (первый include).
* **Описание:** аудит-сабагент отметил, что файл включает `GreedyPhysicsMerger.hpp` первым, не `<Jolt/Jolt.h>`. При
  детальной проверке — `GreedyPhysicsMerger.cpp` **не** использует Jolt types напрямую. Contract applies только к TUs
  с Jolt types.
* **Severity rationale:** N-A (false alarm).
* **Предлагаемый фикс:** нет действия.
* **Cross-refs:** `agent/knowledge.md Part A §4` (Jolt include order contract).
* **Статус:** ✅ Closed (false alarm, no action required).

---

# Глава 3. Vulkan / Render / Shaders

> Сабагент: `00dd21fc-85ac-4d0c-9db9-1e34872128c9 (Vulkan/Render Auditor)`.
> Файлы: `src/render/`, `src/shaders/`, `src/render/vulkan/`.

## High severity

### AUDIT-VK-001 — `SceneResources.cpp` `GrowNanoVdbBuffer`: GPU use-after-free risk

* **Файл / строки:** `src/render/SceneResources.cpp:300-350` (`GrowNanoVdbBuffer`).
* **Описание:** `GrowNanoVdbBuffer` освобождает old VMA allocation и создаёт новую. Если GPU всё ещё читает
  old buffer (от previous frame) — это use-after-free на GPU side. Должен ensure GPU finished all work referencing
  old buffer до free.
* **Severity rationale:** High — потенциальный GPU use-after-free causing validation errors или visual corruption.
* **Предлагаемый фикс:** defer old buffer destruction до тех пор, пока frame-in-flight fence для кадра, который
  last used old buffer, не signaled. Использовать deferred-destroy queue indexed by frame-in-flight:
  ```cpp
  struct DeferredDestroyEntry {
      VmaAllocation allocation;
      VkBuffer buffer;
      uint64_t frameNumber;  // frame that last used this resource
  };
  std::array<std::vector<DeferredDestroyEntry>, MAX_FRAMES_IN_FLIGHT> deferredDestroys;
  ```
  На `GrowNanoVdbBuffer`: добавить old buffer в queue для current frame. На fence signal (есть helper
  `OnFrameInFlightFenceSignaled(frameIndex)`): drain queue для `frameIndex`.
* **DoD / Верификация:**
    * [ ] Validation Layers clean при `PROJECTV_ENABLE_VALIDATION=ON` + repeated grow operations.
    * [ ] Stress test: 1000 grow operations с reads в flight → 0 validation errors.
    * [ ] `ProjectVNanoVdbGpuUploadTests` extended с grow-during-read test.
* **Cross-refs:** `TODO.md §1.1` (NanoVDB flatten), `agent/knowledge.md Part A` (Vulkan resource lifecycle).
* **Effort estimate:** M (требует deferred-destroy queue infrastructure, может переиспользовать существующую если есть).
* **Статус:** 🔓 Open.

## Medium severity

### AUDIT-VK-002 — `Renderer.cpp`: barrier ordering для volumetric fog froxel (verify)

* **Файл / строки:** `src/render/Renderer.cpp:450-500` (`RecordGraphicsCommands`).
* **Описание:** volumetric fog froxel texture sampled в `voxel.frag` (binding 12), но barrier между compute fog pass
  и fragment shader read не верифицирован явно. Fog compute dispatch writes в 3D image, затем main voxel pass
  reads. Если в одной queue — proper `VkMemoryBarrier2` нужен.
* **Severity rationale:** Medium — если barrier missing, GPU может read stale fog data. На большинстве hardware —
  occasional flickering, не crash.
* **Предлагаемый фикс:** verify barrier chain. После `RecordVolumetricFogDispatch` добавить:
  ```cpp
  VkMemoryBarrier2 memoryBarrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
  };
  VkDependencyInfo depInfo = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &memoryBarrier };
  vkCmdPipelineBarrier2(cb, &depInfo);
  ```
  Альтернативно — image memory barrier на froxel image с `LAYOUT_GENERAL` → `SHADER_READ_ONLY_OPTIMAL` transition
  (если layout transition ещё не делается).
* **DoD / Верификация:**
    * [ ] Code review + явная barrier в коде.
    * [ ] Renderdoc capture: verify barrier присутствует между fog compute и voxel fragment.
    * [ ] Visual smoke: no flickering при active fog.
* **Cross-refs:** `TODO.md §5.3` (Volumetric fog wire-up), `agent/knowledge.md Part A` (Vulkan sync contracts).
* **Effort estimate:** XS (verify + add if missing).
* **Статус:** 🔓 Open.

### AUDIT-VK-003 — `SkyAtmosphere.cpp`: manual `FloatToHalf` vs standard `glm::packHalf1x16`

* **Файл / строки:** `src/render/SkyAtmosphere.cpp:80-120` (`FloatToHalf`).
* **Описание:** custom `FloatToHalf` IEEE 754 conversion implemented manually. Error-prone для edge cases (denorms,
  NaN, inf). Комментарий говорит «GLM doesn't expose packHalf1x16», но `glm::packHalf1x16` существует в
  `<glm/gtc/packing.hpp>` с GLM 0.9.6+.
* **Severity rationale:** Medium — manual implementation может иметь edge case bugs с denorms или values > 65504.
* **Предлагаемый фикс:** заменить на `glm::packHalf1x16` из `<glm/gtc/packing.hpp>`. **Если GLM version < 0.9.6**
  в external/ — добавить edge case handling в manual version (inf/NaN/denorms).
* **DoD / Верификация:**
    * [ ] GLM version в `external/glm` проверена (≥ 0.9.6).
    * [ ] `ProjectVSkyAtmosphereTests` extended с edge case tests (inf, NaN, denorm, 65504+, 65505+).
    * [ ] Visual smoke: identical output.
* **Cross-refs:** `TODO.md §5.5` (Sky LDR LUT precomputation), `agent/knowledge.md Part B` (GLM version constraints).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-004 — `voxel.frag` / `volumetric_fog.comp`: depth distribution formula sync risk

* **Файл / строки:** `src/shaders/voxel.frag` (consume side) + `src/shaders/volumetric_fog.comp` (dispatch side).
* **Описание:** comment в `voxel.frag` говорит formula `pow(normalizedDepth, 0.5) * 0.995 + 0.005` must match
  `volumetric_fog.comp`. Если одно меняется без другого — fog sampling на wrong depths. Нет shared include или
  constant для этой formula.
* **Severity rationale:** Medium — manual sync между двумя shader files.
* **Предлагаемый фикс:** extract depth distribution formula constants (`0.5`, `0.995`, `0.005`) в shared shader
  include header (`src/shaders/common/common_constants.glsl`):
  ```glsl
  // EVIL: depth distribution parameters. MUST match between volumetric_fog.comp and voxel.frag.
  const float kFogDepthDistributionExp = 0.5;
  const float kFogDepthDistributionScale = 0.995;
  const float kFogDepthDistributionBias = 0.005;
  ```
  В обоих shader files — `#include "common/common_constants.glsl"`.
* **DoD / Верификация:**
    * [ ] Shared header создан, оба shader'а include'ят его.
    * [ ] Code review: hardcoded magic numbers в обоих shader'ах удалены.
    * [ ] Visual smoke: fog depth distribution идентична до/после.
* **Cross-refs:** `AGENTS.md §8.1` (EVIL markers), `TODO.md §5.3` (Volumetric fog).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-005 — `RayTracedShadows.cpp`: TLAS instance buffer populated без memory barrier (non-coherent memory)

* **Файл / строки:** `src/render/RayTracedShadows.cpp:200-250` (`UpdateTlas`).
* **Описание:** `UpdateTlas` populates TLAS instance buffer через mapped memory (`HOST_ACCESS_SEQUENTIAL_WRITE_BIT`).
  После CPU writes нужен host→device memory barrier до TLAS build command. `vkCmdBuildAccelerationStructuresKHR`
  implicit includes host→device dependency через submit, но explicit flush через `vmaFlushAllocation` может быть
  нужен для non-coherent memory.
* **Severity rationale:** Medium — VMA `HOST_ACCESS_SEQUENTIAL_WRITE_BIT` использует
  `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
  на большинстве GPU, так что explicit flush не нужен. Но на некоторых mobile GPU или driver configurations может
  быть non-coherent.
* **Предлагаемый фикс:** вызвать `vmaFlushAllocation` после CPU writes для safety:
  ```cpp
  vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
  ```
  Альтернативно — assert `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` в allocation.
* **DoD / Верификация:**
    * [ ] `vmaFlushAllocation` вызов добавлен (или assert на coherent bit).
    * [ ] `ProjectVRayTracedShadowTests` → green.
    * [ ] Manual на NVIDIA RTX + AMD RDNA — identical results.
* **Cross-refs:** `TODO.md §5.2` (RTX shadows), `agent/workspace.md §1 16x Phase 5` (TLAS buffer allocation).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-006 — `VulkanGraphicsPipeline.cpp` + `SceneResources.cpp`: binding 9-12 validation errors active

* **Файл / строки:** `src/render/vulkan/VulkanGraphicsPipeline.cpp` + `src/render/SceneResources.cpp`
  (bindings 9-12 initialization paths).
* **Описание:** per `agent/workspace.md §1 16x Phase 2`: «Pre-existing validation errors (bindings 9-12:
  lodDownsampled/chunkLodLevels/vctClipmap/volumetricFog) are from prior sessions». Эти validation errors
  suggest bindings 9-12 may not be properly initialized или bound во всех code paths. При env gates OFF fallback
  bindings должны быть valid.
* **Severity rationale:** Medium — validation errors active и задокументированы. Fallback images/buffers существуют
  но могут не покрывать все pipeline paths.
* **Предлагаемый фикс:** audit всех binding 9-12 initialization paths:
    * Binding 9 — `lodDownsampledVoxelPayload` (per `TODO.md §4.2` 8x V1).
    * Binding 10 — `chunkLodLevels` (per `TODO.md §4.2` 8x V1).
    * Binding 11 — `vctClipmap` sampler3D (per `TODO.md §5.3` 8x V C).
    * Binding 12 — `volumetricFog` sampler3D (per `TODO.md §5.3` 8x V C).

  Для каждого: проверить что fallback resource bound когда feature disabled.
* **DoD / Верификация:**
    * [ ] `PROJECTV_ENABLE_VALIDATION=ON` +
      `PROJECTV_VCT_GPU=OFF PROJECTV_FOG=OFF PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=OFF`
      → 0 validation errors на binding 9-12.
    * [ ] `COMMENTS.md` секция `## src/render/SceneResources.cpp` описывает fallback binding policy.
* **Cross-refs:** `agent/workspace.md §1 16x Phase 2` (pre-existing errors note), `TODO.md §4.2`, `§5.3`.
* **Effort estimate:** M (audit + fix per binding).
* **Статус:** 🔓 Open.

## Low severity

### AUDIT-VK-007 — `VulkanBootstrap.cpp`: extension enable list не deduplicated

* **Файл / строки:** `src/render/vulkan/VulkanBootstrap.cpp:180-220` (extension list construction).
* **Описание:** когда multiple features enabled (mesh shaders + RTX), extensions like `VK_KHR_buffer_device_address`
  могут быть добавлены multiple times в `ppEnabledExtensionNames`. Vulkan spec allows duplicates, но wasteful.
* **Severity rationale:** Low — Vulkan handles duplicates gracefully, но code should deduplicate для clarity.
* **Предлагаемый фикс:** use `std::set` для collect unique extension names до converting to C array:
  ```cpp
  std::set<const char *> uniqueExtensions;
  for (const auto &ext : requiredExtensions) uniqueExtensions.insert(ext);
  std::vector<const char *> finalExtensions(uniqueExtensions.begin(), uniqueExtensions.end());
  ```
* **DoD / Верификация:**
    * [ ] Code review: extension list использует `std::set` или equivalent dedup.
    * [ ] Validation: extension count == unique count при всех env gate combinations.
* **Cross-refs:** `agent/knowledge.md Part A §4` (Vulkan bootstrap contracts).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-008 — `TaaRenderTargets.cpp`: TAA history buffer resize race (cosmetic)

* **Файл / строки:** `src/render/TaaRenderTargets.cpp:50-80` (resize path).
* **Описание:** при window resize, TAA render targets recreated. Если resize происходит во время in-flight frame, old
  targets могут быть destroyed пока GPU их ещё использует. Код ждёт `vkDeviceWaitIdle` до resize — correct, но
  heavy-handed.
* **Severity rationale:** Low — `vkDeviceWaitIdle` correct (если slow) way.
* **Предлагаемый фикс:** consider per-frame fence-based cleanup вместо `vkDeviceWaitIdle`, но это performance issue,
  не correctness. **Defer** до dedicated performance session.
* **Cross-refs:** `TODO.md §5.3` (TAA Motion Vectors).
* **Effort estimate:** M (deferred).
* **Статус:** 🔓 Open (deferred).

### AUDIT-VK-009 — `HizCulling.cpp`: `WritePerChunkMipAndBlendWidthsToBuffer` assumes packed layout invariant

* **Файл / строки:** `src/render/HizCulling.cpp:100-130`.
* **Описание:** function packs `[mip, blendWidth, mip, blendWidth, ...]` в `uint32_t` buffer. Если `chunkCount`
  changes между CPU packing и GPU reading, buffer может быть read с wrong offsets.
* **Severity rationale:** Low — chunk count stable within frame (frozen at start).
* **Предлагаемый фикс:** assert что `chunkCount` matches между packing и GPU buffer size:
  ```cpp
  PV_ASSERT(packedData.size() == chunkCount * kHizMipAndBlendWidthWordsPerChunk);
  ```
* **Cross-refs:** `TODO.md §2.1` (HZB smart blend width).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-010 — `Cloudscape.cpp`: push constants hard-coded без EVIL markers

* **Файл / строки:** `src/render/Cloudscape.cpp:40-60` (push constants block).
* **Описание:** cloud coverage (`0.65`), contrast, UV offset values hard-coded в push constants без EVIL markers.
* **Severity rationale:** Low — runtime-tunable через `VoxelSceneLighting`, но C++ defaults lack markers.
* **Предлагаемый фикс:** добавить EVIL markers или extract to named constants:
  ```cpp
  // EVIL: default cloud coverage. Higher = denser clouds. 0.65 per Nubis 2017 reference.
  static constexpr float kDefaultCloudCoverage = 0.65f;
  // EVIL: cloud layer contrast. 0.5 = balanced.
  static constexpr float kDefaultCloudContrast = 0.5f;
  ```
* **Cross-refs:** `AGENTS.md §8.1` (EVIL markers), `TODO.md §5.3` (Cloudscape).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-011 — `ShadowProjection.cpp`: cascade sphere-fit `sqrt` без negative check

* **Файл / строки:** `src/render/ShadowProjection.cpp:120-150` (sphere-fit calculation).
* **Описание:** sphere-fit calculation для cascade extent использует `sqrt(diagonal)`. Если `diagonal` negative
  из-за floating-point error (e.g., extremely thin cascade) — `sqrt(negative) == NaN`.
* **Severity rationale:** Low — extremely unlikely в practice с reasonable camera parameters.
* **Предлагаемый фикс:** guard: `const float extent = std::sqrt(std::max(0.0f, diagonal));`.
* **Cross-refs:** `TODO.md §5.3` (Shadow projection).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-VK-012 — `voxel_mesh.comp`: LOD payload decode использует magic stride 16 без shared constant

* **Файл / строки:** `src/shaders/voxel_mesh.comp` (`kLodWordStride=16`).
* **Описание:** `kLodWordStride=16` — GLSL constant, должен match C++ `kLodPayloadWordStride=16` в
  `LodDownsampleGpuConsume.hpp`. No mechanism ensures they stay synchronized.
* **Severity rationale:** Low — оба explicit set to 16, manual sync required.
* **Предлагаемый фикс:** добавить compile-time check или shared constant generation. **Простейший вариант** — code
  review checklist с grep `rg "kLodWordStride|kLodPayloadWordStride"` при изменении любого из файлов.
* **Cross-refs:** `TODO.md §4.2` (LOD GPU consume).
* **Effort estimate:** XS (manual sync) или M (proper constant gen).
* **Статус:** 🔓 Open.

---

# Глава 4. Tests & CMake

> Сабагент: `dbafe47a-7a16-4884-b254-653a356e903e (Tests/CMake Auditor)`.
> Файлы: `tests/`, корневой `CMakeLists.txt`, `CMakePresets.json`.

## High severity

### AUDIT-TC-001 — `tests/CMakeLists.txt`: `ProjectVTests` + `ProjectVFluidCATests` link failures (root cause)

* **Файл / строки:** `tests/CMakeLists.txt:40-60` (ProjectVTests target), `120-140` (ProjectVFluidCATests target).
* **Описание:** оба test executable'а failing to link несколько sessions. Root cause: link against main `ProjectV`
  sources, miss some required object files или libraries. Конкретно:
    * `ProjectVTests` — links `VoxelWorldTests.cpp` который includes практически entire engine через `Types.hpp`,
      требуя все render/vulkan/physics/voxel object files. CMake target не links всех required dependencies.
    * `ProjectVFluidCATests` — similar issue с missing dependencies.
* **Severity rationale:** High — 2 test executables permanently broken.
* **Предлагаемый фикс:** update `target_link_libraries` для обоих targets, чтобы include all required libraries.
  **Альтернативно** — restructure tests чтобы не требовали full engine link (вынести shared utilities в отдельный
  static lib). **Рекомендуемый подход:**
    1. Identify missing object files (через `ld --print-data-base` или `nm` на failing link).
    2. Add `projectv_render`, `projectv_vulkan`, etc. (или create aggregator target `projectv_engine`) к
       `target_link_libraries`.
    3. **Или** — create `projectv_test_utils` static lib с shared test infrastructure, link against it вместо
       full engine.
* **DoD / Верификация:**
    * [ ] `cmake --build build/linux-clang-debug --target ProjectVTests ProjectVFluidCATests` → green.
    * [ ] `ctest --test-dir build/linux-clang-debug -j 8` → 0 failures.
    * [ ] CMakePresets.json backfilled если новые target (per `AUDIT-TC-002`).
    * [ ] `agent/workspace.md` обновлён: 38/40 → 40/40 (или текущий count + 2).
* **Cross-refs:** `agent/workspace.md §1` (pre-existing failures note), `AUDIT-TC-002`, `AUDIT-TC-004`,
  `agent/knowledge.md Part A §4` (build preset invariant).
* **Effort estimate:** M (debug link errors + identify missing deps).
* **Статус:** 🔓 Open.

## Medium severity

### AUDIT-TC-002 — `CMakePresets.json`: target list count может быть stale

* **Файл / строки:** `CMakePresets.json` (все 5 buildPresets).
* **Описание:** build presets enumerate specific target names. После recent sessions (17x, 16x, 8x V C) новые test
  executables added и backfilled в presets. Но `agent/knowledge.md §4` contract говорит count должен быть 17 для
  debug presets. Actual current count of test executables (на основе `tests/CMakeLists.txt grep for add_executable`)
  скорее всего выше 14+2 benchmarks.
* **Severity rationale:** Medium — если новый test target added но не backfilled во все 5 presets, target не
  соберётся в CI.
* **Предлагаемый фикс:** выполнить `grep -c 'add_executable' tests/CMakeLists.txt` и verify против preset target
  counts. **После `AUDIT-TC-001` фикса** — пересчитать все 5 presets.
* **DoD / Верификация:**
    * [ ] Grep count test executables.
    * [ ] Все 5 buildPresets enumerates точно test executables + `ProjectV` + benchmarks (для debug).
    * [ ] `cmake --build --preset <name>` собирает все targets successfully.
* **Cross-refs:** `agent/knowledge.md Part A §4` (build preset target list invariant), `AUDIT-TC-001`, `AUDIT-TC-004`.
* **Effort estimate:** XS (после `AUDIT-TC-001`).
* **Статус:** 🔓 Open.

### AUDIT-TC-003 — `tests/`: нет integration tests для render pipeline

* **Файл / строки:** `tests/` directory.
* **Описание:** нет integration tests exercising full render pipeline (`DrawFrame` → все passes). Все render-related
  tests — unit tests of individual components (HZB, shadows, LOD, etc.). Это common для GPU-dependent code, но
  значит shader integration bugs (like wrong binding numbers) могут быть caught только at runtime.
* **Severity rationale:** Medium — known limitation, не bug per se.
* **Предлагаемый фикс:** добавить headless Vulkan smoke test, который создаёт device, records minimal frame,
  validates descriptor bindings. **Простой вариант** — expand `ProjectVRuntimeSmoke` с explicit binding validation.
  **Продвинутый** — dedicated `ProjectVRenderPipelineIntegrationTests` executable.
* **DoD / Верификация:**
    * [ ] Новый test executable или `ProjectVRuntimeSmoke` extension с binding validation.
    * [ ] Test запускается под `linux-clang-debug` без display (offscreen rendering).
    * [ ] All bindings 0-12 validated как present (or explicit env-gate skip).
* **Cross-refs:** `agent/knowledge.md Part A §4` (runtime smoke policy), `AUDIT-VK-006` (binding 9-12 validation).
* **Effort estimate:** L.
* **Статус:** 🔓 Open.

### AUDIT-TC-004 — `CMakePresets.json`: `testPresets` не exclude known-broken test executables

* **Файл / строки:** `CMakePresets.json` (testPresets).
* **Описание:** test presets run `ctest` но rely на user manually excluding `ProjectVTests` + `ProjectVFluidCATests`
  через `-E` flag. Error-prone. Presets должны иметь `filter.exclude.name` configured.
* **Severity rationale:** Medium — CI runs всегда будут report 2 failures пока не manually filtered.
* **Предлагаемый фикс:** добавить `"filter": {"exclude": {"name": "ProjectVTests|ProjectVFluidCATests"}}` к
  `testPresets` в `CMakePresets.json`. **После `AUDIT-TC-001` фикса** — убрать exclude.
* **DoD / Верификация:**
    * [ ] `ctest --preset <name>` без manual flags → 0 failures (или documented expected failures).
    * [ ] CI script может запускать presets без knowledge of broken targets.
* **Cross-refs:** `AUDIT-TC-001`, `agent/knowledge.md Part A §4`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

## Low severity

### AUDIT-TC-005 — `ProjectVPhysicsSyncTests.cpp`: missing edge case tests

* **Файл / строки:** `tests/ProjectVPhysicsSyncTests.cpp`.
* **Описание:** test покрывает 9 sub-tests, но не тестирует:
    * Concurrent edits от multiple chunks одновременно.
    * Chunk boundary voxel edits (worst-case 8-rebuild scenario из `agent/workspace.md §4`).
    * Recovery from failed body creation (e.g., Jolt allocation failure).
* **Severity rationale:** Low — existing tests покрывают main paths, edge cases documented risks.
* **Предлагаемый фикс:** добавить boundary-edit test для 8-rebuild worst case:
  ```cpp
  TEST_CASE("PhysicsSync_BoundaryEditRebuildsEightChunks") {
      // Edit voxel on chunk corner → 8 chunk rebuilds
      // Verify all 8 chunkMergedBoxes updated, IsPhysicsStaticWorldBodyId still true
  }
  ```
* **Cross-refs:** `TODO.md §3.2`, `agent/workspace.md §4` (worst-case note).
* **Effort estimate:** S.
* **Статус:** 🔓 Open.

### AUDIT-TC-006 — `CMakeLists.txt`: nlohmann_json version mismatch with Tracy (latent)

* **Файл / строки:** `CMakeLists.txt:475` (`FetchContent_MakeAvailable(nlohmann_json 3.11.3)`).
* **Описание:** root CMakeLists fetches nlohmann_json 3.11.3, но Tracy's vendor.cmake использует 3.12.0. Поскольку
  Tracy built standalone, нет collision. **Но** если кто-то remove standalone build и попытается integrate Tracy
  back as subdirectory, version conflict resurface.
* **Severity rationale:** Low — currently non-issue (standalone Tracy), latent risk.
* **Предлагаемый фикс:** bump до 3.12.0 в root `CMakeLists.txt` или document version mismatch в comment.
* **Cross-refs:** `agent/knowledge.md Part A §4` (Tracy preset), `agent/knowledge.md Part B §9` (build tools).
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-TC-007 — Test files: custom test harness vs standard framework

* **Файл / строки:** `tests/VoxelWorldTests.cpp` и др.
* **Описание:** project использует custom test harness с `TEST_CASE(name)` macros вместо Catch2/Google Test.
  Harder для integration с CI reporting tools.
* **Severity rationale:** Low — design choice, не bug.
* **Предлагаемый фикс:** оставить как есть (migration out of scope). **Альтернатива** — добавить JUnit XML output
  к custom harness для CI integration.
* **Effort estimate:** L (если migration).
* **Статус:** 🔓 Open (deferred / out of scope).

### AUDIT-TC-008 — Benchmark files: no EVIL markers on threshold constants

* **Файл / строки:** `src/bench/FrustumCullBenchmark.cpp`, `src/bench/ShadowProjectionBenchmark.cpp`.
* **Описание:** benchmark threshold values (iteration counts, timing limits) hard-coded без EVIL markers.
* **Severity rationale:** Low — benchmarks dev-only tools.
* **Предлагаемый фикс:** добавить EVIL markers к threshold constants (тот же паттерн что `AUDIT-CORE-009`).
* **Cross-refs:** `AUDIT-CORE-009` (тот же pattern), `AGENTS.md §8.1`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

## N/A / no issue

### AUDIT-TC-009 — Root `CMakeLists.txt`: `cmake_minimum_required` version (no issue)

* **Файл / строки:** `CMakeLists.txt:1` (`cmake_minimum_required(VERSION 3.30)`).
* **Описание:** project requires CMake 3.30+ (для C++ module support). `cmake_minimum_required(VERSION 3.30)` present.
* **Severity rationale:** N-A (no issue).
* **Статус:** ✅ Closed (no action required).

### AUDIT-TC-010 — Jolt include order в test files (no issue)

* **Файлы:** `tests/PhysicsWorldTests.cpp`, `tests/PhysicsSyncTests.cpp`, `tests/PhysicsGreedyMergerTests.cpp`.
* **Описание:** все test files корректно include `<Jolt/Jolt.h>` первым. No violations.
* **Severity rationale:** N-A (all correct).
* **Статус:** ✅ Closed (no action required).

---

# Глава 5. Documentation sync

> Сабагент: `4447696f-c96a-4436-bd40-90140899278b (Documentation Accuracy Auditor)`.
> Файлы: `TODO.md`, `agent/workspace.md`, `agent/knowledge.md`, `CHANGELOG.md`, `COMMENTS.md`.

## Medium severity

### AUDIT-DOC-001 — `TODO.md:39`: Task 3.2 checkbox `[x]` но status `⏸️ Partial`

* **Файл / строки:** `TODO.md:39` (Task 3.2 header), `TODO.md:329` (full status block).
* **Описание:** Task 3.2 имеет `- [x] **Задача 3.2.**` (checked), но status text говорит `⏸️ Partial → частично closed`,
  и full status block (line 329) explicitly states: «Не закрыто полностью: async compute path для incremental
  rebuild, broadphase diagnostics, PROJECTV_FLUID_CA_GPU=ON как default flip».
* **Severity rationale:** Medium — checkbox `[x]` misleading. Partially completed task должен использовать `- [ ]`
  или `- [/]`.
* **Предлагаемый фикс:** изменить `- [x]` на `- [/]` (in-progress marker) на line 39, сохранив `⏸️ Partial` status
  text. После полного закрытия 3.2 — flip на `- [x]`.
* **DoD / Верификация:**
    * [ ] Grep: `rg "Задача 3\.2" TODO.md` → verify checkbox marker соответствует status.
    * [ ] `CHANGELOG.md` doc-sync entry.
* **Cross-refs:** `TODO.md §3.2`, `AUDIT-DOC-002`, `AUDIT-DOC-003`, `AUDIT-DOC-004`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-002 — `TODO.md:57`: Task 6.2 checkbox `[x]` но status `⏸️`

* **Файл / строки:** `TODO.md:57` (Task 6.2 header).
* **Описание:** Task 6.2 имеет `- [x] **Задача 6.2.** PIMPL для AppState`, но inline status `⏸️` и
  `agent/workspace.md §2 Nearest Gap` explicitly lists Stage 6.2 AppState PIMPL full struct move as near-gap work.
* **Severity rationale:** Medium — same contradictory checkbox issue as `AUDIT-DOC-001`.
* **Предлагаемый фикс:** изменить `- [x]` на `- [/]` (in-progress marker) на line 57.
* **Cross-refs:** `TODO.md §6.2`, `AUDIT-DOC-001`, `AUDIT-DOC-003`, `AUDIT-DOC-004`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-003 — `TODO.md:58`: Task 6.3 checkbox `[x]` но status `⏸️`

* **Файл / строки:** `TODO.md:58` (Task 6.3 header).
* **Описание:** Task 6.3 имеет `- [x] **Задача 6.3.** Async Compute Queue & Timeline Semaphores`, но inline status
  `⏸️` с text «RTX BLAS routing deferred».
* **Severity rationale:** Medium — same contradictory checkbox issue.
* **Предлагаемый фикс:** изменить `- [x]` на `- [/]` на line 58.
* **Cross-refs:** `TODO.md §6.3`, `AUDIT-DOC-001`, `AUDIT-DOC-002`, `AUDIT-DOC-004`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-004 — `TODO.md:60`: summary count wrong

* **Файл / строки:** `TODO.md:60` (summary line).
* **Описание:** summary говорит «15 ✅ Closed · 1 ⏸️ Partial (6.2 PIMPL full move) · 2 🔓 Open (2.3 SVT, 5.2 RTX
  BLAS/TLAS)». Но:
    * 3.2 — `⏸️ Partial`, не `✅ Closed` (per `AUDIT-DOC-001`).
    * 6.2 — `⏸️ Partial`, не `✅ Closed` (per `AUDIT-DOC-002`).
    * 6.3 — `⏸️ Partial`, не `✅ Closed` (per `AUDIT-DOC-003`).
      Correct count: 12 ✅ Closed · 3 ⏸️ Partial (3.2, 6.2, 6.3) · 2 🔓 Open (2.3, 5.2).
* **Severity rationale:** Medium — misleading progress reporting.
* **Предлагаемый фикс:** update summary line на `12 ✅ Closed · 3 ⏸️ Partial (3.2 Incremental Jolt, 6.2 PIMPL, 6.3 Async
  Compute) · 2 🔓 Open (2.3 SVT, 5.2 RTX)`.
* **Cross-refs:** `AUDIT-DOC-001/002/003`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-005 — `TODO.md:51` vs `agent/workspace.md`: Task 5.2 status mismatch

* **Файл / строки:** `TODO.md:51` (Task 5.2) vs `agent/workspace.md §1 16x` (lines 25-44).
* **Описание:** TODO.md Task 5.2 говорит «deferred 🔓» (Open). `agent/workspace.md §1 16x» documents «Phases 1-15
  complete, Phase 16 doc-sync in progress» и ~1100 LoC foundation code. Foundation landed в dirty tree, just not
  committed. TODO.md не reflect этого in-progress work.
* **Severity rationale:** Medium — TODO.md должен acknowledge foundation work даже если uncommitted.
* **Предлагаемый фикс:** update TODO.md Task 5.2 status на `⏸️ Partial — foundation landed in dirty tree (16x
  session), shader integration deferred`.
* **Cross-refs:** `TODO.md §5.2`, `agent/workspace.md §1 16x`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-006 — `agent/knowledge.md Part A §4`: build target count outdated

* **Файл / строки:** `agent/knowledge.md Part A §4` (lines ~107-111).
* **Описание:** §4 говорит minimum debug preset target count = 17 (`ProjectV` + 14 test executables + 2 benchmarks).
  Но recent sessions added new test executables (`ProjectVPhysicsSyncTests`, `ProjectVRayTracedShadowTests`, etc.).
  Actual current count выше.
* **Severity rationale:** Medium — exact number в §4 может cause agents to under-count required targets.
* **Предлагаемый фикс:** update §4 на reflect current test executable count, **или** изменить phrasing на
  «all ctest-registered executables» без specific number. **Рекомендуется** — формулировка без числа, т.к. exact
  count changes with each new test executable.
* **Cross-refs:** `AUDIT-TC-001`, `AUDIT-TC-002`, `AUDIT-TC-004`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

## Low severity

### AUDIT-DOC-007 — `TODO.md:62` vs `agent/workspace.md`: 17x "Phase 6 closed" vs session "in progress"

* **Файл / строки:** `TODO.md:62` (17x update line) vs `agent/workspace.md §1 17x` (line 9).
* **Описание:** TODO.md 17x update (line 62) говорит «Stage 3.2 Incremental Jolt — Phase 6 closed». Но
  `agent/workspace.md §1 17x` говорит «Phase 5: Doc-sync (in progress)» и «Phase 6: Commit prompt». Whole 17x
  session listed as "in progress" в workspace.md. TODO.md update describing что was done, но phrasing
  "Phase 6 closed" confusing когда session itself не committed yet.
* **Severity rationale:** Low — meaning clear из context, но может быть clearer.
* **Предлагаемый фикс:** clarify TODO.md line 62 на «Phase 6 done (uncommitted, awaiting operator decision)» или
  similar.
* **Cross-refs:** `AUDIT-DOC-001`, `TODO.md §3.2 status`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-008 — `agent/workspace.md`: test count notation confusing

* **Файл / строки:** `agent/workspace.md` (test count progression).
* **Описание:**
    * 8x V C (line 48): «36/36 ctest pass + 2 documented pre-existing failures».
    * 16x (line 25): «37/39 (1 new test executable, +1 vs previous baseline)».
    * 17x (line 9): «38/40 (1 new test executable, +1 vs previous baseline 37/39)».

  Progression: 8x V C has 36/36 → 16x should be 37/37+2=37/39 → 17x should be 38/38+2=38/40. This checks out. But
  «38/40» notation confusing: suggests 2 failures out of 40, но это pre-existing link errors, не test failures.
* **Severity rationale:** Low — numbers technically correct, но notation confusing.
* **Предлагаемый фикс:** standardize notation на `38/38 pass (+ 2 pre-existing link-error executables excluded)`
  across all session entries.
* **Cross-refs:** `AUDIT-TC-001` (link errors source), `AUDIT-DOC-009`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-009 — `agent/workspace.md`: 8x V C listed в обоих §5 и §6

* **Файл / строки:** `agent/workspace.md` (lines 94, 99).
* **Описание:** 8x V C session appears в §5 Active tasks AND в §6 Recent closed sessions. Поскольку session
  «closed dirty per operator policy», должно быть только в §6.
* **Severity rationale:** Low — §5 entry stale.
* **Предлагаемый фикс:** remove 8x V C entry из §5 (уже в §6).
* **Cross-refs:** `agent/workspace.md §5`, `§6`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open.

### AUDIT-DOC-010 — `agent/knowledge.md Part A §15`: новые bindings 11/12 не упомянуты

* **Файл / строки:** `agent/knowledge.md Part A §15` (lines ~336-438).
* **Описание:** §15 mandates что `SceneLightingBuffer` declarations должны stay byte-identical across `voxel.frag`,
  `voxel_shadow.vert`, и `voxel_mesh.comp`. 8x V C session added new bindings (11 и 12) и changed descriptor set
  layout. §15 не explicitly mention новые bindings или extended descriptor set.
* **Severity rationale:** Low — contract talks about `SceneLightingBuffer` struct layout being byte-identical, не
  about descriptor bindings. Bindings (11, 12) — separate descriptor slots, не changes to lighting buffer struct.
  Contract still accurate.
* **Предлагаемый фикс:** no change required, **опционально** — добавить note к §15 mentioning что graphics descriptor
  set now has 9 entries (bindings 0-8 + 11 + 12). Рекомендуется — добавить note для future agents.
* **Cross-refs:** `agent/knowledge.md Part A §15`, `AUDIT-VK-006`.
* **Effort estimate:** XS.
* **Статус:** 🔓 Open (опционально).

## No major inconsistencies (positive findings)

### AUDIT-DOC-011 — `CHANGELOG.md`: no major inconsistencies (positive)

* **Файл / строки:** `CHANGELOG.md` (2904 lines, partially read).
* **Описание:** 17x, 16x, и 8x V C entries exist и roughly match `agent/workspace.md` и `TODO.md` descriptions.
  No major inconsistencies в readable portions.
* **Severity rationale:** N-A (no issue).
* **Статус:** ✅ Closed (no action required).

### AUDIT-DOC-012 — `COMMENTS.md`: no stale entries (positive)

* **Файл / строки:** `COMMENTS.md` (974 lines, partially read).
* **Описание:** design-rationale entries для recent sessions (17x PhysicsWorld.cpp + VoxelWorld.cpp, 8x V C
  volumetric fog) present и reference correct file paths и line ranges. No stale entries.
* **Severity rationale:** N-A (no issue).
* **Статус:** ✅ Closed (no action required).

---

# Матрица приоритетов и рекомендуемый порядок устранения

## Top priority (блокеры или риск коррапты) — должен быть закрыт в первую очередь

1. **`AUDIT-VK-001`** — `SceneResources.cpp` `GrowNanoVdbBuffer` GPU use-after-free (**High**).
2. **`AUDIT-CORE-001`** — `Camera.cpp` division by zero (**High**).
3. **`AUDIT-TC-001`** — `tests/CMakeLists.txt` link errors root cause (**High**).
4. **`AUDIT-VK-002`** — `Renderer.cpp` vol fog barrier verify (**Medium**, близко к High — может flicker).

## High-value Medium items (можно устранять батчами по паттерну)

* **Ownership/docs батч:** `AUDIT-CORE-002` + `AUDIT-CORE-003` + `AUDIT-CORE-010` + `AUDIT-CORE-007` (одна
  design-rationale session).
* **EVIL markers батч:** `AUDIT-CORE-009` + `AUDIT-VK-010` + `AUDIT-TC-008` + `AUDIT-VK-004` (один sweep).
* **`[[nodiscard]]` батч:** `AUDIT-CORE-012` (механический проход).
* **CMake build fix батч:** `AUDIT-TC-001` + `AUDIT-TC-002` + `AUDIT-TC-004` (после root cause в TC-001).
* **Docs sync батч:** `AUDIT-DOC-001` + `AUDIT-DOC-002` + `AUDIT-DOC-003` + `AUDIT-DOC-004` + `AUDIT-DOC-005` +
  `AUDIT-DOC-006` + `AUDIT-DOC-007` + `AUDIT-DOC-008` + `AUDIT-DOC-009` (один doc-sync commit).
* **Render validation батч:** `AUDIT-VK-005` + `AUDIT-VK-006` (binding audit + memory barrier fix).

## Low priority (tech debt, deferred candidates)

* `AUDIT-PV-*` Low items (`AUDIT-PV-005/006/007/008/009`) — отдельная session при work в physics/voxel.
* `AUDIT-VK-007/008/009/011/012` — deferrable.
* `AUDIT-TC-005/006/007` — deferrable.
* `AUDIT-DOC-010` — optional.
* `AUDIT-TC-003` (integration test infrastructure) — отдельная dedicated session.

## Effort estimate summary

| Категория                |   XS   |   S   |   M   |   L   | Итого  |
|:-------------------------|:------:|:-----:|:-----:|:-----:|:------:|
| Core/ECS/App             |   5    |   3   |   0   |   0   |   8    |
| Physics/Voxel            |   3    |   0   |   1   |   0   |   4    |
| Vulkan/Render            |   5    |   0   |   1   |   0   |   6    |
| Tests/CMake              |   4    |   1   |   1   |   1   |   7    |
| Docs sync                |   9    |   0   |   0   |   0   |   9    |
| False alarm / no issue   |   4    |   0   |   0   |   0   |   4    |
| **Итого (active tasks)** | **30** | **4** | **3** | **1** | **38** |

**Total active task count (excluding false alarms and no-issue): 38.** Плюс 4 closed-as-no-action tasks (15/14/15/9/15 +
12/11/10/12) для документации.

---

# Cross-cutting рекомендации

1. **Один commit per батч, не per task.** Финансовый overhead per-commit высок; батчинг по паттерну (ownership
   docs, EVIL markers, `[[nodiscard]]`) снижает overhead и упрощает review.
2. **Документация first при high-impact fixes.** `AUDIT-VK-001` (GPU use-after-free) и `AUDIT-CORE-001` (div-by-zero)
   должны сопровождаться `COMMENTS.md` entries (per `AGENTS.md §8.3`) и unit-tests в `CHANGELOG.md` mention.
3. **DoD gates обязательны.** Каждый `DoD / Верификация` блок содержит checklist, который должен быть полностью
   ticked до перевода задачи в `✅ Closed`.
4. **Не редактировать `AGENTS.md` без явной команды оператора** (per `AGENTS.md §1`). Этот файл (`TODO_NEW.md`) — не
   `AGENTS.md`, можно редактировать по обычному протоколу.
5. **Scope discipline (per `AGENTS.md §5.5`):** при работе над конкретной задачей из этого файла — коммитить только
   related files, использовать `git add <path>` (не `git add -A`).

---

## История изменений

* **`2026-06-21`** — initial creation. 59 issues catalogued from 5 audit subagents. Файл создан
  [agent, awaiting operator review].
