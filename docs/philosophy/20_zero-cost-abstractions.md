# Абстракции с нулевой стоимость

Документ описывает zero-cost абстракции в C++26: что это, какие доступны,
когда абстракция НЕ zero-cost.

---

## Что такое zero-cost

Zero-cost abstraction — принцип Bjarne Stroustrup:

> То, что ты не используешь, не должно стоить тебе ничего. И то, что ты
> используешь, ты не мог бы написать лучше вручную.

Два следствия:

- Неиспользуемая абстракция — нулевой overhead.
- Используемая абстракция компилируется в то же, что ручной код.

C++26 выполняет этот принцип для: шаблонов, concepts, constexpr, RAII,
`std::expected`, `std::simd`, `std::execution`.

---

## Современные zero-cost абстракции C++20/23/26

### Concepts (C++20)

Concepts заменяют SFINAE для ограничений на template-параметры.

```cpp
template <typename T>
concept Numeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

template <Numeric T>
T add(T a, T b) { return a + b; }
```

Ошибка компиляции при нарушении constraint ясная. Runtime overhead: ноль.

### `std::span` (C++20)

Non-owning view на contiguous range. Без аллокации, без ownership.

```cpp
void process(std::span<const float> data);
```

Заменяет `(const float* ptr, size_t size)` пары без потери информации о
типе. Runtime overhead: ноль — `span` это два указателя.

### `std::expected<T, E>` (C++23)

Типобезопасная замена исключений и кодов ошибок. Без аллокаций (для
тривиальных типов).

### `std::flat_map` / `std::flat_set` (C++23)

Cache-friendly контейнеры поверх `std::vector`. Заменяют `std::map` там,
где порядок не меняется после вставки.

### `std::mdspan` (C++23)

Multidimensional view. Без аллокаций. Без копий данных.

### `std::generator` (C++23)

Coroutine-генератор для ленивых последовательностей. Zero-cost на
выходных значениях.

### `std::simd` (P1928, в C++26)

Кроссплатформенная SIMD-абстракция. Компилируется в SSE/AVX/AVX-512 на
x86, NEON на ARM, SVE на ARMv8.4+. Замена ручных intrinsic'ов.

### `std::linalg` (P1673R7, в C++26)

Стандартизированный BLAS. `std::linalg::matrix`, `std::linalg::vector`,
`std::linalg::transposed`.

---

## Zero-cost инструменты

### Templates

Шаблоны C++ — zero-cost. Инстанцирование происходит на этапе
компиляции. Runtime-код идентичен ручному коду для конкретного набора
параметров.

Ограничения:

- Compile-time cost: инстанцирование дорогое.
- Code bloat: каждая комбинация параметров порождает новый код.

### Concepts

Замена SFINAE. Compile-time checks, zero runtime cost.

### `constexpr` функции

Вычисление на этапе компиляции. Runtime стоимость — ноль.

```cpp
constexpr auto lookup_table = generate_table();
```

### Inline-функции

`inline` — подсказка компилятору. Реальное решение принимает линкер
(LTO инлайнит по эвристикам).

### RAII

Resource Acquisition Is Initialization. Деструктор вызывается при выходе
из scope. Стоимость — ноль при включённой оптимизации.

---

## Абстракции, которые стоят

Не всё в C++ zero-cost. Некоторые абстракции требуют runtime-поддержки.

### `std::shared_ptr`

Атомарный счётчик ссылок. Каждое копирование / удаление — атомарная
операция. Стоимость на многоядерных системах — десятки тактов на
операцию.

Альтернатива: `std::unique_ptr` (zero-cost) или сырой указатель с явным
владением.

### `std::function`

Тип-стирание. Требует аллокации для больших callables. Виртуальный
вызов внутри.

Альтернативы: function template параметр, `std::move_only_function`
(C++23).

### Exceptions (`try`/`throw`/`catch`)

Unwind tables в бинарнике. Throw — тяжёлая операция.

Подробности в [11_anti-patterns.md](11_anti-patterns.md).

### RTTI (`dynamic_cast`, `typeid`)

Строки типа хранятся в бинарнике. `dynamic_cast` обходит дерево
наследования.

### Thread-local storage (`thread_local`)

Каждый поток получает свою копию переменной. Скрытый overhead на
управление TLS-блоками.

### `std::shared_mutex`

Shared/exclusive locking. Overhead на атомарных операциях.

---

## Правила

1. **Шаблоны и concepts** — для типов с разными параметрами. Zero-cost.
2. **`constexpr`** — для вычислений, известных на этапе компиляции.
3. **`std::span`** — для non-owning views на массивы.
4. **`std::expected`** — для типизированных ошибок.
5. **`std::simd`** — для векторизации.
6. **`std::unique_ptr`** — для unique ownership.
7. **`std::shared_ptr`** — только когда shared ownership действительно
   нужен.
8. **Исключения** — запрещены в hot path.
9. **RTTI** — запрещён в hot path.

---

## Пример: применение в движке

В ProjectV активно используются: concepts для ограничений на template
параметры (flecs wrappers), `std::span` для non-owning views на буферы
вершин, `std::expected` для всех fallible операций. `std::simd` —
плановая замена ручных intrinsic'ов в hot path voxel processing.

---

## Источники и дальнейшее чтение

- **Bjarne Stroustrup — The Design and Evolution of C++** — zero-cost
  abstraction как принцип.
- **Herb Sutter — C++26 is done!** — статус std::simd, std::linalg,
  std::expected.
  <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>
- [06_why-cpp26.md](06_why-cpp26.md) — почему C++26.
- [14_compiler.md](14_compiler.md) — диалог с компилятором.
- [15_compile-time.md](15_compile-time.md) — constexpr и модули.
- [11_anti-patterns.md](11_anti-patterns.md) — что не использовать.