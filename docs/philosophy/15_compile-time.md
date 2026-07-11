# Compile-time вычисления

Документ описывает compile-time в C++26: модули, constexpr, consteval,
метапрограммирование.

---

## Почему `#include` убивает время сборки

`#include` — текстовая вставка. Каждый файл, включающий `<vector>`,
обрабатывает тысячи строк шаблонного кода STL заново. В проекте с 500
файлами, где 200 из них включают `<vector>`, компилятор обрабатывает
`<vector>` 200 раз.

Результат:

- Холодная сборка: 5-15 минут.
- Инкрементальная: 30-90 секунд.
- Память на компиляцию одного файла: 500 MB - 2 GB.

`#include` — основная причина медленной сборки C++ проектов.

---

## Модули C++26

Модули — современная замена `#include`. Вместо текстовой вставки —
компилированный интерфейс модуля (BMI — Binary Module Interface).

```cpp
// MyModule.ixx
export module MyModule;

export int add(int a, int b) {
    return a + b;
}
```

```cpp
// main.cpp
import MyModule;

int main() {
    return add(1, 2);
}
```

`import MyModule` подгружает BMI один раз. Не текстовая вставка, не
повторная компиляция заголовка. Время инкрементальной сборки
сокращается в 5-10×.

### Поддержка в 2026

- Clang 22: полная поддержка модулей, включая `import std;` через libc++.
- GCC 14: поддержка модулей, std-модуль в разработке.
- MSVC 19.4x (Visual Studio 2022 17.10+): поддержка модулей, std-модуль
  в стадии preview.

libstdc++ не поставляет std-модуль в C++20-формате на июнь 2026. libc++
поставляет.

### CMake поддержка

CMake 3.28+ поддерживает сканирование исходников на модули. CMake 4.4+
(в разработке, 2026) добавляет полную поддержку с автоматической
генерацией synthetic targets для совместимости BMI.

---

## Разделение интерфейса и реализации

Модули дают чистое разделение:

```cpp
// module.ixx — интерфейс
export module MyModule;
export void public_function();
class Impl; // не export — внутренний класс
```

Реализация скрыта. Пользователь видит только экспортированные сущности.

---

## Быстрый билд = быстрая разработка

Время компиляции — метрика качества проекта. Долгая сборка = медленная
разработка = баги не исправляются сразу = накопление технического долга.

Целевые метрики (для C++26 проекта на Clang 22 с модулями):

- Холодная сборка: < 2 минуты.
- Инкрементальная: < 10 секунд.
- Перекомпиляция одного файла: < 3 секунды.

---

## `constexpr` и вычисления при компиляции

`constexpr` — пометка функции, которая может быть вычислена на этапе
компиляции. В C++20+ возможности `constexpr` расширены: условные
операторы, циклы, `try`/`catch` (без `throw`), аллокации, `constexpr`
виртуальные функции (C++26).

```cpp
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }
    return result;
}

constexpr int fact_5 = factorial(5); // 120
```

Hot path, где параметры известны на этапе компиляции, получает
выигрыш: код вычисляется один раз компилятором, runtime работает с
результатом.

---

## Consteval (C++20)

`consteval` — обязательное compile-time вычисление. Функция, помеченная
`consteval`, не может быть вызвана с runtime-параметрами.

Применяется к функциям, которые по смыслу должны быть compile-time:
хеш-функции для switch, lookup tables, type traits.

---

## Consteval blocks (C++26, P3289R1)

Consteval blocks — блоки кода, которые выполняются на этапе компиляции.
В отличие от `consteval` функций, consteval blocks могут содержать
несколько операторов и локальные переменные.

Используется для генерации таблиц инициализации, lookup arrays, FNV-1a
hash precomputation.

---

## Reflection (C++26, P2996R13)

Reflection в C++26 — статическое метапрограммирование без template
metaprogramming.

```cpp
template <typename T>
auto get_name() -> std::string {
    return std::meta::name_of<T>();
}
```

`std::meta::info`, `std::meta::reflect_value(...)`, `^ClassName`
(splice operator), `template_for<Types>` — инструменты для рефлексии.

**Применения:**

- ECS registration: автоматическая регистрация компонентов через
  `template_for` вместо ручного макроса.
- Vulkan binding generation: генерация descriptor bindings из POD-структур.
- Configuration deserialization: JSON в struct без макросов.
- Logging: `std::meta::name_of<T>()` для имени типа в логах.

Reflection в Clang 22 — в trunk, частично. К моменту широкого
использования (2027-2028) API стабилизируется.

---

## Метапрограммирование в стиле C++17-23

До полного принятия C++26 в проекте используются:

- `std::enable_if`, `std::is_same`, `std::is_integral` — type traits.
- `if constexpr` — compile-time branching.
- Concepts (C++20) — ограничения на template-параметры.
- `std::void_t`, `std::is_detected` — SFINAE-утилиты.
- Fold expressions (C++17) — pack expansion в выражениях.

Concepts предпочтительнее SFINAE: ошибки компиляции понятнее.

```cpp
template <typename T>
concept Numeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

template <Numeric T>
T add(T a, T b) { return a + b; }
```

---

## Compile-time как метрика

Время компиляции — одна из метрик качества проекта.

Что замедляет сборку:

- `#include` каскады.
- Шаблоны с длинными instantiation chains.
- SFINAE с множеством перегрузок.
- Макросы, разворачивающиеся в большие блоки кода.

Что ускоряет:

- Модули.
- PGO.
- Forward declarations вместо `#include` где возможно.
- Concepts вместо SFINAE.

---

## Пример: применение в движке

В ProjectV модули C++26 — плановая цель: переход на `import std;` после
стабилизации в libstdc++. Concepts используются для ограничений на
template-параметры (см. flecs type-erased wrappers). `static_assert` —
для проверки инвариантов данных (sizeof, alignment).

---

## Источники и дальнейшее чтение

- **Herb Sutter — C++26 is done! Trip report March 2026** — статус
  Reflection, Contracts, std::execution в C++26.
  <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>
- **P2996R13 — Reflection for C++26** — основной proposal.
  <https://www.open-std.org/jtc1/SC22/wg21/docs/papers/2025/p2996r13.html>
- **CMake C++ modules documentation** — cmake-cxxmodules(7).
  <https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html>
- [06_why-cpp26.md](06_why-cpp26.md) — почему C++26.
- [14_compiler.md](14_compiler.md) — диалог с компилятором.
- [91_tooling-landscape.md](91_tooling-landscape.md) — CMake 4.4 для
  модулей.