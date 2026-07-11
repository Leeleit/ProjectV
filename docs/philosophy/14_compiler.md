# Диалог с компилятором

Clang — союзник, а не чёрный ящик. Документ описывает, как использовать
компилятор как инструмент.

---

## Что такое Clang

Clang — фронтенд LLVM для C/C++/Obj-C. На июнь 2026 года актуальная
версия — Clang 22. Clang предоставляет:

- Полноценный AST, доступный через libclang, Clang AST Matchers,
  clang-query.
- Статический анализ через clang-tidy, scan-build, Clang Static Analyzer.
- Инструменты для PGO (Profile-Guided Optimization), ThinLTO, BOLT.
- Санитайзеры: ASan, TSan, UBSan, MSan.

При работе с движком Clang — не «компилятор, который выдаёт бинарник».
Clang — платформа, на которой построены все остальные инструменты.

---

## Profile-Guided Optimization (PGO)

PGO — двухпроходная компиляция:

1. **Первый проход:** собирается инструментированный бинарник, который
   при запуске записывает профиль реального выполнения (какие ветки `if`
   сработали, какие функции вызваны).
2. **Второй проход:** профиль используется для оптимизации: предсказание
   ветвлений, инлайнинг, размещение горячего кода, удаление мёртвого
   кода.

Эффект: 5-15% прироста в hot path без изменения исходного кода.

Команды:

```bash
# Первый проход: инструментированная сборка
cmake -B build/pgo-gen -S . -DCMAKE_CXX_FLAGS="-fprofile-instr-generate"
cmake --build build/pgo-gen

# Запуск с реалистичной нагрузкой для сбора профиля
./build/pgo-gen/ProjectV runtime/scene.json
# → default.profraw

# Второй проход: оптимизированная сборка
cmake -B build/pgo -S . -DCMAKE_CXX_FLAGS="-fprofile-instr-use=default.profraw"
cmake --build build/pgo
```

PGO требует реалистичной нагрузки на первом проходе. Запуск с пустой
сценой даст профиль, не соответствующий реальному использованию.

---

## ThinLTO (Link Time Optimization)

LTO — оптимизация на этапе линковки, когда компилятор видит весь граф
вызовов. ThinLTO — облегчённая версия LTO, которая масштабируется на
большие проекты.

Эффект: 5-10% прироста по сравнению с обычным `-O3`. Основной выигрыш —
в инлайнинге через translation unit boundaries и удалении мёртвого
кода.

Включение в Release-сборке:

```
-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections
-fno-finite-math-only
```

---

## Санитайзеры

Санитайзеры — runtime-проверки, встроенные в бинарник. Цена: 2-5×
замедление, увеличение потребления памяти. Используются в Debug и CI,
не в Release.

### AddressSanitizer (ASan)

Обнаруживает: use-after-free, buffer overflow (heap, stack, global),
double free, memory leaks.

Включение: `-fsanitize=address`.

### ThreadSanitizer (TSan)

Обнаруживает data races. Цена: 5-15× замедление, 5-10× память.

Включение: `-fsanitize=thread`.

### UndefinedBehaviorSanitizer (UBSan)

Обнаруживает: signed integer overflow, shift на величину больше ширины
типа, null pointer dereference, выход за границы массива через
`operator[]`.

Включение: `-fsanitize=undefined`.

### MemorySanitizer (MSan)

Обнаруживает use of uninitialized memory. Требует перекомпиляции всех
зависимостей с MSan.

Включение: `-fsanitize=memory`.

**Комбинации:**

- ASan + UBSan: типичная Debug-сборка. Ловит большинство багов.
- TSan отдельно: требует особой конфигурации CMake.
- MSan отдельно: только если разрабатывается криптография или
  сериализация.

---

## Подсказки компилятору

### `[[likely]]` и `[[unlikely]]` (C++20)

Подсказки для branch prediction. Используются редко: компилятор обычно
лучше предсказывает ветки, чем программист.

```cpp
if (error_code != 0) [[unlikely]] {
    log_error(error_code);
    return error_code;
}
```

Не злоупотребляй. Неправильная подсказка хуже отсутствия подсказки.

### `[[nodiscard]]`

Функция возвращает значение, которое нельзя игнорировать. Применяется к:

- Функциям, возвращающим `std::expected<T, E>`.
- Функциям, возвращающим коды ошибок.
- Геттерам ресурсов.

### `noexcept`

Обещание, что функция не бросает исключений. Позволяет компилятору
генерировать более агрессивный код.

Применяется к: деструкторам, функциям swap, move-конструкторам и
move-присваиваниям.

### `constexpr`

Помечает функции, которые могут быть вычислены на этапе компиляции. В
C++20+ расширено: `constexpr` функции могут содержать `if`, циклы,
try/catch (без throw), аллокации (с `std::allocator`).

В hot path, где параметры известны на этапе компиляции, `constexpr`
даёт выигрыш: код вычисляется один раз при компиляции, а не при каждом
вызове.

---

## Предупреждения как ошибки (`-Werror`)

Сборка с `-Werror`. Предупреждение — ошибка. Это требование заставляет
писать код, который компилируется чисто.

Что включается в проекте:

- `-Wall -Wextra -Wpedantic` — базовый набор.
- `-Wshadow` — переменная в локальной области скрывает переменную из
  внешней области.
- `-Wnon-virtual-dtor` — деструктор базового класса не виртуальный.
- `-Wold-style-cast` — C-style cast.
- `-Wcast-align` — alignment issue при cast.
- `-Wunused` — неиспользуемые переменные, параметры, значения.
- `-Woverloaded-virtual` — сокрытие виртуальных функций.
- `-Wconversion` — потенциально опасные конверсии.
- `-Wsign-conversion` — знаковые/беззнаковые конверсии.
- `-Wnull-dereference` — очевидные null deref.
- `-Wdouble-promotion` — float к double в arithmetic.
- `-Wformat=2` — format string безопасность.

Каждое предупреждение имеет причину быть включённым. Не отключай без
обоснования в коде.

---

## Модули C++26

`import std;` — стандартная библиотека как модуль. Ускорение компиляции
в 5-10× за счёт отсутствия `#include` каскадов.

В проекте включается через `CMAKE_CXX_MODULE_STD` в CMake. По умолчанию
OFF, потому что libstdc++ ещё не поставляет std-модуль в C++20-формате.
Для libc++-only probe builds — ON.

См. [15_compile-time.md](15_compile-time.md).

---

## Пример: применение в движке

В ProjectV санитайзеры включены в Debug по умолчанию через
`PROJECTV_ENABLE_VALIDATION=ON`. PGO и ThinLTO используются в Release.
Все warning-флаги из этого документа — в `CMakeLists.txt` и `.clang-tidy`.

---

## Источники и дальнейшее чтение

- **Clang User's Manual** — документация по флагам.
  <https://clang.llvm.org/docs/UsersManual.html>
- **PGO в LLVM** — официальное руководство.
  <https://llvm.org/docs/HowToBuildPGO.html>
- **AddressSanitizer** — документация Google.
  <https://github.com/google/sanitizers/wiki/AddressSanitizer>
- **Compiler Explorer (Godbolt)** — проверка ассемблерного вывода.
  <https://godbolt.org/>
- [06_why-cpp26.md](06_why-cpp26.md) — почему C++26.
- [15_compile-time.md](15_compile-time.md) — модули и constexpr.
- [19_debugging.md](19_debugging.md) — санитайзеры в Debug workflow.
- [91_tooling-landscape.md](91_tooling-landscape.md) — версии
  инструментов.