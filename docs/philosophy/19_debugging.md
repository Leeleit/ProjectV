# Отладка

Документ описывает инструменты и методы отладки real-time движка.
Брейкпоинты в hot path не работают. Отладка чёрного ящика заменяется
телеметрией.

---

## Почему брейкпоинты не работают в real-time

Брейкпоинт останавливает поток. В однопоточном приложении это приемлемо.
В real-time движке:

- Игровой цикл работает на 60+ FPS. Остановка = потеря кадра = видимый
  артефакт.
- Многопоточное исполнение: остановка одного потока не останавливает
  другие.
- GPU-команды уже отправлены. Остановка CPU не отменяет GPU-работу.

Брейкпоинт — инструмент для cold path: startup, shutdown, обработка
ошибок. Для hot path нужна телеметрия.

---

## Три столпа отладки

### 1. Статический анализ

До запуска. Находит ошибки без исполнения кода.

- **clang-tidy** — статический анализатор. Проверки вроде `bugprone-*`,
  `performance-*`, `readability-*`.
- **clang Static Analyzer** — более глубокий анализ через Clang AST.
- **Compiler warnings** — `-Wall -Wextra -Wpedantic -Werror`. См.
  [14_compiler.md](14_compiler.md).

Что ловит: race conditions (частично), use-after-free (через
`-fsanitize=address` в Debug), undefined behavior.

### 2. Динамический анализ

Во время исполнения. Требует запуска кода.

- **AddressSanitizer (ASan):** use-after-free, buffer overflow, double free.
  Цена: 2-3× slowdown, 3-5× memory overhead.
- **ThreadSanitizer (TSan):** data races. Цена: 5-15× slowdown.
- **UndefinedBehaviorSanitizer (UBSan):** signed overflow, null deref,
  out-of-bounds array access. Цена: ~2× slowdown.
- **MemorySanitizer (MSan):** use of uninitialized memory. Требует
  перекомпиляции всех зависимостей.

### 3. Профилирование

Непрерывная телеметрия во время исполнения.

- **Tracy Profiler 0.13.1** — frame profiler с поддержкой CPU и GPU
  (Vulkan contexts через `VK_EXT_host_query_reset`), memory, locks,
  screenshots. Низкий overhead: < 1% в Release-сборке.
- **AMD uProf 5.3** — аппаратные счётчики на AMD Zen (май 2026).
- **Intel VTune Profiler 2026.1** — аппаратные счётчики на Intel,
  microarchitecture analysis, XPU offload analysis.
- **Nsight Compute** — для NVIDIA GPU: register usage, memory
  throughput, divergence analysis.
- **RenderDoc** — frame capture, draw call inspection, shader
  hot-reload.

---

## Паттерны отладки в real-time

### Instrumentation markers

Ключевые точки в коде помечаются Tracy-зонами.

```cpp
FrameMark;
ZoneScoped;
ZoneScopedN("PhysicsSystem::update");
ZoneScopedC(Color::Red);
```

Зоны вложенные. Tracy показывает flame graph.

### GPU ↔ CPU synchronization markers

Vulkan context в Tracy требует явных точек синхронизации:

```cpp
auto* ctx = TracyVkContext(device, physical_device, queue);
TracyVkZone(ctx, render_pass);
// ... рисование ...
TracyVkZoneEnd(ctx);
TracyVkCollect(ctx);
```

Зоны GPU и CPU появляются на одном таймлайне.

### Lock contention analysis

Tracy показывает contention на каждом мьютексе. Если contention > 5%,
архитектура требует рефакторинга.

### Memory allocation tracking

Tracy записывает каждую аллокацию в hot path. Если аллокаций > 0 в
runtime — нарушение правила №1 в [16_memory.md](16_memory.md).

---

## Структурированное логгирование

Старый подход: `printf` и grep по stdout. Не работает для real-time.

Новый подход: структурированное логгирование с уровнями и контекстом.

### Уровни

- `FATAL`: crash imminent.
- `ERROR`: операция не удалась, движок продолжает.
- `WARN`: подозрительная ситуация, не критично.
- `INFO`: значимые события.
- `DEBUG`: детали для отладки.
- `TRACE`: максимальная детализация.

Release-сборка: FATAL, ERROR, WARN, INFO. Debug-сборка: все уровни.

### Формат

Структурированный JSON для парсинга инструментами:

```json
{"ts": "2026-06-27T12:34:56.789Z", "level": "ERROR", "module": "Renderer", "msg": "vkCreateImage failed", "code": -9}
```

### Sink'и

- Stdout: для запуска из консоли.
- Файл: для долгосрочного анализа.
- Tracy: для привязки логов к таймлайну.

---

## Crash dumps и постмортем анализ

Когда crash неизбежен — собрать максимум информации для последующего
анализа.

### Linux

- Включить core dumps: `ulimit -c unlimited`.
- Сохранять в фиксированную директорию через `/proc/sys/kernel/core_pattern`.
- Backtrace через `addr2line` или `gdb` + бинарник с отладочными символами.

### Windows

- Mini-dump через `MiniDumpWriteDump` (DbgHelp API).
- WinDbg для анализа.

### Стратегия

1. Включить core dump в Debug-сборке по умолчанию.
2. При crash — дамп + автоматический bug report.
3. Анализ: gdb backtrace + bisect к ревизии, где баг впервые появился.

---

## Интеграция с CI/CD

### CI pipeline

1. **Build Debug:** ASan + UBSan включены. Любой санитайзер-отчёт =
   провал CI.
2. **Tests:** 39/39 ctest должно проходить.
3. **Lint:** clang-tidy, clang-format.
4. **Release build:** PGO + ThinLTO.

### Pre-commit gate

Перед коммитом:

1. Код компилируется чисто.
2. Все тесты проходят.
3. Формат соответствует `.clang-format`.

Подробнее: см. [94_build-and-ci.md](94_build-and-ci.md).

---

## Tracy Profiler: конфигурация

Tracy включается через `PROJECTV_ENABLE_TRACY=ON` (по умолчанию в
большинстве preset'ов). Бандлинг Tracy UI собирается через
`PROJECTV_BUILD_TRACY_PROFILER=OFF` по умолчанию.

Включение в Release:

```
-DPROJECTV_ENABLE_TRACY=ON
```

Tracy работает с минимальным overhead (< 1%) и доступна для
профилирования production build.

---

## Пример: применение в движке

В ProjectV Tracy интегрирован через `PROJECTV_ENABLE_TRACY=ON`.
Санитайзеры включены в Debug. Custom log levels через fmt 12.2.
Frame markers на каждом кадре через `FrameMark`. Smoke-тест проверяет
отсутствие Vulkan validation errors за 1000 кадров.

---

## Источники и дальнейшее чтение

- **Tracy Profiler manual** — поставляется с Tracy 0.13.1.
  <https://github.com/wolfpld/tracy/releases/tag/v0.13.1>
- **AddressSanitizer** — документация Google.
  <https://github.com/google/sanitizers/wiki/AddressSanitizer>
- **Intel VTune Profiler 2026.1** — release notes.
  <https://www.intel.com/content/www/us/en/developer/articles/release-notes/vtune-profiler/2026.html>
- **AMD uProf 5.3** — release notes, май 2026.
  <https://www.amd.com/en/developer/uprof.html>
- [14_compiler.md](14_compiler.md) — санитайзеры, PGO.
- [30_optimization.md](30_optimization.md) — иерархия оптимизации.
- [93_performance-methodology.md](93_performance-methodology.md) —
  методология.