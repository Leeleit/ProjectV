# Почему C++26

Документ объясняет, почему для высокопроизводительного движка
выбирается C++26, а не более ранние стандарты или другие языки.

---

## TL;DR

C++26 — **первая редакция C++ после C++11, которая реально сдвигает
платформу**. Три главные фичи закрывают три десятилетних TODO:

1. **Contracts (P2900R14)** — замена `assert` на уровне языка.
2. **Reflection (P2996R13)** — статическое метапрограммирование без SFINAE.
3. **`std::execution` (P2300R10)** — стандартизированная асинхронность.

Плюс второстепенные, но критичные для движков:

- **SIMD через `std::simd` (P1928)** — кроссплатформенная векторизация.
- **Linear Algebra (P1673)** — стандартизированный BLAS.
- **Pack expansion в большем числе контекстов.**
- **Consteval blocks (P3289R1)** — compile-time блоки кода.

---

## Статус C++26 на июнь 2026

C++26 завершил техническую работу **28 марта 2026** на встрече ISO C++
в London Croydon. Herb Sutter опубликовал trip report в тот же день.

- **DIS (Draft International Standard)** отправлен на международное
  утверждение.
- **Clang** и **GCC** уже имеют ~2/3 фич в trunk, релиз ожидается во
  второй половине 2026.
- **MSVC** отстаёт, но работает над модульной поддержкой.

> Ключевая цитата (Herb Sutter, March 2026): «Reflection is by far the
> biggest upgrade for C++ development that we've shipped since the
> invention of templates.»

Источник: <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>

---

## Топ-3 фичи C++26

### 1. Contracts (P2900R14) — замена `assert`

**Было (C++11-23):**

- `assert()` — стандартный макрос, но игнорируется в Release.
- `if (!cond) throw ...` — ручная проверка.
- `[[nodiscard]]` — нет precondition check.

**Стало (C++26):**

```cpp
void push(Entity e) [[pre: e != null]] [[post: !empty()]];
contract_assert(x > 0);
```

Уровни enforcement: `audit`, `default`, `axiom` — настраивается для
Debug/Release. `contract_violation_handler` — единая точка для логирования.

**Что даёт движку:** строгая проверка инвариантов без шума в коде;
единый механизм обработки нарушений.

### 2. Reflection (P2996R13) — статическое метапрограммирование

**Было:** template SFINAE, `if constexpr`-цепочки, `std::enable_if`,
`std::is_detected`.

**Стало:** `^ClassName` (splice operator), `std::meta::info`,
`std::meta::reflect_value(...)`, `template_for`.

**Что даёт движку:**

- ECS registration: автоматическая регистрация компонентов.
- Vulkan binding generation: descriptor bindings из структур.
- Configuration deserialization: JSON→struct без макросов.
- Logging: `std::meta::name_of<T>()` для имени типа.

### 3. `std::execution` (P2300R10) — стандартизированная асинхронность

Scheduler + Sender + Receiver. Lazy work graph, structured concurrency,
race-free by construction.

```cpp
auto sender = just(42)
    | then([](int x) { return x * 2; })
    | via(thread_pool_scheduler{});
```

**Что даёт движку:** замена самописного Job System на стандартизированный
API. NVIDIA `stdexec` уже есть как reference implementation.

---

## Второстепенные фичи (критичные для движков)

### `std::simd` (P1928)

Кроссплатформенная SIMD-абстракция. Компилируется в SSE/AVX/AVX-512 на
x86, NEON на ARM, SVE на ARMv8.4+.

В движке применяется в:

- Meshlet processing (16-wide float).
- Frustum culling (8-wide vec3).
- GPU readback decode (4-wide uint32 → 4-wide float).

### Linear Algebra (P1673R7)

Стандартизированный BLAS: `std::linalg::matrix`, `std::linalg::vector`,
`std::linalg::transposed`, `std::linalg::scaled`.

Миграция с GLM на `std::linalg` планируется.

### Consteval blocks (P3289R1)

Compile-time блоки кода. В отличие от `consteval` функций, могут содержать
несколько операторов и локальные переменные.

### `import std;` (C++23+)

Стандартная библиотека как модуль. Ускорение компиляции в 5-10× за счёт
отсутствия `#include` каскадов.

---

## Что C++26 НЕ даёт

- **Нет memory safety.** C++26 — эволюция C++, не переизобретение.
- **Нет встроенной async cancellation** в полном объёме.
- **Нет полноценной поддержки в MSVC** на момент выхода C++26.

---

## Почему не Rust

- **Compile time.** Rust компилирует на ~3× дольше C++.
- **Borrow checker в graphics code.** У нас большой объём ownership-shared
  данных (Vulkan buffers, ECS). Borrow checker требует `Rc<RefCell<T>>`
  или unsafe.
- **Экосистема.** Vulkan bindings в Rust всё ещё не на уровне C++.
- **SIMD.** `std::simd`-подобных API в Rust нет.

Rust отличный язык для system software, web backends, embedded. Для
real-time graphics engine в 2026 — C++26 + libc++ + Clang 22.

---

## Почему не C++17 или C++20

- **C++17:** нет modules, нет `std::execution`, нет `std::simd`, нет
  `std::linalg`.
- **C++20:** есть concepts и `std::span`, но нет трёх главных фич C++26.
- **C++23:** есть `expected`, deducing `this`, multidimensional `mdspan`,
  но нет Contracts, Reflection, execution.

C++26 требуется потому что без Contracts мы возвращаемся к `assert`-макросам,
без Reflection — к SFINAE-шаблонам, без execution — к самописному Job
System.

---

## Что реально используется в движке

Из `CMakeLists.txt`:

- `set(CMAKE_CXX_STANDARD 26)` — обязательно.
- `set(CMAKE_CXX_STDLIB libc++)` — Clang + libc++.
- `import std;` — opt-in per-target.

Из кода:

- `std::expected<T, E>` (C++23) — везде.
- `std::string_view` — параметры read-only.
- `std::pmr::polymorphic_allocator<T>` — кастомные аллокаторы.
- Concepts (`template <typename T> concept ...`) — ограничения на template.

**Планируется на C++26:**

- Contracts для публичного API (после Clang 22 стабилизирует).
- Reflection для ECS registration.
- `std::execution` для Job System v2.

---

## Пример: реализация в ProjectV

ProjectV требует C++26 в `CMakeLists.txt:53`. Concepts используются для
ограничений на template параметры. `static_assert` — для проверки
инвариантов данных. `import std;` через `CMAKE_CXX_MODULE_STD` —
opt-in per-target.

---

## Источники и дальнейшее чтение

- **Herb Sutter — C++26 is done! Trip report March 2026**.
  <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>
- **cppreference.com — C++26**.
  <https://cppreference.com/cpp/26>
- **P2996R13 — Reflection for C++26**.
  <https://www.open-std.org/jtc1/SC22/wg21/docs/papers/2025/p2996r13.html>
- **P2900R14 — Contracts for C++**.
  <https://github.com/cplusplus/papers/issues/1648>
- **P2300R10 — `std::execution`**.
- **NVIDIA stdexec**: <https://github.com/NVIDIA/stdexec>
- [15_compile-time.md](15_compile-time.md) — модули, constexpr.
- [17_error-handling.md](17_error-handling.md) — обработка ошибок.
- [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md) — zero-cost.