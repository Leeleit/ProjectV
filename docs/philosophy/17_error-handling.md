# Обработка ошибок

Документ описывает правила обработки ошибок в высокопроизводительных
движках.

---

## Два типа ошибок

Ошибки делятся на две категории. Для каждой — свой механизм.

### Инварианты движка

Утверждения, которые по построению не должны нарушаться. Если нарушены —
программа в неопределённом состоянии.

Примеры:

- Передан `nullptr` туда, где ожидался валидный указатель.
- Индекс массива вышел за границы.
- Состояние GPU стало невалидным.
- Время кадра отрицательное или нулевое.

### Ожидаемые ошибки

Часть логики приложения. Не нарушение инвариантов, а ситуации, которые
могут произойти в нормальной работе.

Примеры:

- Игрок пытается загрузить битый сейв.
- По сети пришёл невалидный пакет.
- Файл не найден по пути, введённому пользователем.
- GPU не смог выделить буфер (out of memory).

---

## Crash Early, Crash Loud

Инвариант нарушен — `assert` и crash. Без попытки «аккуратно
обработать».

Причины:

- Инвариант — гарантия корректности. Нарушение = программа в
  неопределённом состоянии.
- Попытка продолжить = повреждение данных, undefined behavior, баги с
  симптомами далеко от причины.

`assert` в Debug, `assert` в Release (для инвариантов, не для
пользовательского ввода). Стек-трейс обязателен. Core dump для
постмортем-анализа.

---

## `std::expected<T, E>` для ожидаемых ошибок

Ошибка — часть логики. Возвращаем `std::expected<Data, Error>`.

Преимущества:

- Типобезопасность: невозможно забыть проверить ошибку.
- Явное указание возможных ошибок в типе.
- Композиция через `.and_then()`, `.transform()`, `.or_else()`.
- Не требует исключений (zero-cost).

### Уровни критичности

| Уровень | Действие | Пример |
|:--------|:---------|:-------|
| Fatal | `assert` + crash | `nullptr` в рендерере, double-free |
| Error | Возврат `std::expected` | Не удалось загрузить шейдер, GPU OOM |
| Warning | Продолжить + лог | Медленная загрузка ассета, deprecated API |
| Info | Только лог | Уровень загружен, соединение установлено |

### Паттерны

#### Композиция через `.and_then()`

```cpp
auto result = load_config("config.json")
    .and_then([](auto& cfg) { return validate(cfg); })
    .and_then([](auto& cfg) { return init_subsystems(cfg); });
```

#### Преобразование через `.transform()`

```cpp
auto user_id = parse_request(request)
    .transform([](Request& req) { return req.user_id; });
```

#### Fallback через `.or_else()`

```cpp
auto texture = load_texture("high_res.png")
    .or_else([](Error) { return load_texture("low_res.png"); });
```

#### `Result<T>` alias для читаемости

```cpp
template <typename T>
using Result = std::expected<T, Error>;

Result<std::string> read_file(std::string_view path);
```

---

## Системные библиотеки

STL, Vulkan, SDL — разные источники ошибок.

- STL: `std::filesystem` бросает исключения, `std::ofstream` возвращает
  failbit.
- Vulkan: возвращает `VkResult` (код ошибки).
- SDL: возвращает код ошибки.

Стратегия: оборачивать в единый интерфейс `Result<T, Error>` на границе
библиотеки.

Внутри движка — только `Result<T, E>`. Коды ошибок и исключения
остаются на границе с внешними библиотеками.

---

## Граница с C кодом

SDL, zstd, stb-библиотеки возвращают коды ошибок или указатели.

Стратегия: принять код ошибки, сконвертировать в `Result<T, Error>` на
границе, внутри — только `Result<T, E>`.

---

## C++26 Contracts (P2900R14) для публичного API

C++26 Contracts — замена `assert` на уровне языка.

```cpp
void render_mesh(const Mesh* mesh)
    [[pre: mesh != nullptr]]
    [[post: !rendering_failed]];
```

Уровни enforcement:

- `audit`: проверка только в Debug.
- `default`: проверка в Debug и Release.
- `axiom`: всегда проверяется (для инвариантов).

`contract_violation_handler` — единая точка для обработки нарушений.

Contracts доступны в Clang 22 (стабилизация в процессе). До стабилизации
используется `assert`.

---

## Правила

1. Инвариант нарушен → `assert` + crash.
2. Ожидаемая ошибка → `std::expected<T, E>`.
3. Уровни критичности определены: Fatal, Error, Warning, Info.
4. Логировать всё. Даже при crash — stack trace.
5. `[[nodiscard]]` на функциях, возвращающих `Result`.
6. На границе с C-кодом — конвертация в `Result<T, E>`.
7. На границе с STL — оборачивание исключений.

---

## Пример: применение в движке

В ProjectV все hot path функции возвращают `Result<T, Error>` или его
alias `Result<T>`. Vulkan вызовы оборачиваются в `Result<VkBuffer>` и
подобные. STL-исключения ловятся на границе библиотеки и конвертируются
в `Result<T, E>`. Подробности в `src/` (классы `Result`, `Error`,
`VulkanError`).

---

## Источники и дальнейшее чтение

- **P2900R14 — Contracts for C++** — основной proposal.
  <https://github.com/cplusplus/papers/issues/1648>
- **Herb Sutter — C++26 is done!** — статус Contracts.
  <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>
- **std::expected (C++23)** — мотивация и API.
  <https://en.cppreference.com/w/cpp/utility/expected>
- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [11_anti-patterns.md](11_anti-patterns.md) — запрет исключений.
- [14_compiler.md](14_compiler.md) — `-Werror`, `-Wunused`.