# Как читать этот гайд

Документ задаёт маршруты чтения. Не содержание, навигация.

---

## Три уровня погружения

### Маршрут «30 минут»

Для тех, кто хочет понять, о чём движок, без деталей.

1. [00_readme.md](00_readme.md) — карта всего гайда (5 минут).
2. [03_about-this-guide.md](03_about-this-guide.md) — зачем этот гайд
   существует (3 минуты).
3. [04_what-is-a-game-engine.md](04_what-is-a-game-engine.md) — что такое
   движок и какие у него слои (10 минут).
4. [10_manifesto.md](10_manifesto.md) — фундаментальные принципы
   (10 минут).

После этого контекст есть. Дальше — по необходимости.

### Маршрут «1 день»

Для тех, кто уже пишет на C++, но хочет понять специфику движков.

**Утро (теория):**

1. [05_hardware-tour.md](05_hardware-tour.md) — что важно знать про
   железо (30 минут).
2. [06_why-cpp26.md](06_why-cpp26.md) — почему C++26 и что он даёт
   (20 минут).
3. [10_manifesto.md](10_manifesto.md) — фундаментальные принципы
   (10 минут).
4. [11_anti-patterns.md](11_anti-patterns.md) — чего не делать
   (30 минут).

**День (парадигмы):**

5. [21_dod.md](21_dod.md) — Data-Oriented Design (40 минут).
6. [22_ecs.md](22_ecs.md) — Entity Component System (40 минут).
7. [31_vulkan.md](31_vulkan.md) — Vulkan 1.4 как рендер-API
   (40 минут).

**Вечер (практика):**

8. [91_tooling-landscape.md](91_tooling-landscape.md) — стек инструментов
   (20 минут).
9. [92_external-sources.md](92_external-sources.md) — что ещё читать
   (10 минут).

### Маршрут «1 неделя»

Для тех, кто готов стать мейнтейнером движка.

**День 1: фундамент.** [10_manifesto](10_manifesto.md),
[11_anti-patterns](11_anti-patterns.md),
[12_decision-making](12_decision-making.md),
[13_evil-hacks](13_evil-hacks.md).

**День 2: язык и компилятор.** [14_compiler](14_compiler.md),
[15_compile-time](15_compile-time.md),
[06_why-cpp26](06_why-cpp26.md).

**День 3: память и данные.** [16_memory](16_memory.md),
[18_data-layout](18_data-layout.md),
[17_error-handling](17_error-handling.md).

**День 4: отладка и стиль.** [19_debugging](19_debugging.md),
[33_testing](33_testing.md),
[90_code-review-checklist](90_code-review-checklist.md).

**День 5: парадигмы.** [20_zero-cost-abstractions](20_zero-cost-abstractions.md),
[21_dod](21_dod.md), [22_ecs](22_ecs.md).

**День 6: конкурентность и потоки.** [23_concurrency](23_concurrency.md),
[24_data-flow](24_data-flow.md),
[25_strings](25_strings.md).

**День 7: домен.** [30_optimization](30_optimization.md),
[31_vulkan](31_vulkan.md),
[32_voxel-data](32_voxel-data.md),
[34_math-and-space](34_math-and-space.md),
[35_time-and-determinism](35_time-and-determinism.md).

**День 8 (бонус):** [93_performance-methodology](93_performance-methodology.md),
[94_build-and-ci](94_build-and-ci.md),
[91_tooling-landscape](91_tooling-landscape.md).

---

## Кто ты → что читать

| Хочешь... | Минимальный набор | Дополнительно |
|:----------|:------------------|:--------------|
| Понять, что за движок | 00, 03, 04, 10 | 05, 06 |
| Оптимизировать горячий цикл | 05, 16, 18, 21, 30 | 14, 20, 91, 93 |
| Написать новую ECS-систему | 22, 23, 24 | 21, 30 |
| Добавить рендер-фичу | 31, 32, 24 | 05, 19, 91 |
| Дебажить падение FPS | 19, 30, 91, 93 | 14, 17, 23 |
| Работать с памятью/аллокациями | 16, 18 | 21, 30 |
| Подготовить PR | 90, 11, 12 | 19, 33 |
| Настроить CI | 94, 19 | 33, 90 |

---

## Где искать ответы (вне этого гайда)

- **Код конкретного проекта** — единственный источник истины о том,
  **как** что-то сделано.
- **Engineering contracts проекта** — конкретные обязательства.
- **Roadmap проекта** — что в работе, что заблокировано.
- **Vendor-документация** конкретных библиотек.

---

## Чего этот гайд не делает

- Не учит C++ — есть [cppreference.com](https://cppreference.com/).
- Не учит Vulkan — есть [docs.vulkan.org](https://docs.vulkan.org/).
- Не учит DOD — есть [Mike Acton, CppCon 2014](https://www.youtube.com/watch?v=rX0ItVEVjHc) и
  [Vittorio Romeo, CppCon 2025](https://www.youtube.com/watch?v=SzjJfKHygaQ).
- Не учит писать движки — есть [Game Engine Architecture, Jason Gregory,
  4ed (2026)](https://www.gameenginebook.com/).
- Не содержит длинных примеров кода.

Гайд объясняет **что** мы делаем и **почему**, и указывает, **где**
искать остальное.