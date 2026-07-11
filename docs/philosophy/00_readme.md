# Философия высокопроизводительного движка

> Код диктуется физикой железа. Либо ты уважаешь эту физику, либо движок
> не взлетит.

Каталог содержит универсальные принципы разработки высокопроизводительных
real-time движков. Гайд объясняет **что** и **почему**, и указывает,
**где** читать подробности. Конкретные реализации — в коде конкретных
проектов.

---

## С чего начать

Не знаешь, как читать? Открой [02_how-to-read.md](02_how-to-read.md) —
три маршрута по времени (30 мин / 1 день / 1 неделя) и таблица «кто ты
→ что читай».

Не знаешь, что такое движок? Начни с
[04_what-is-a-game-engine.md](04_what-is-a-game-engine.md).

Не знаешь, как устроено железо? Начни с [05_hardware-tour.md](05_hardware-tour.md).

Хочешь быстро освежить принципы? Открой [10_manifesto.md](10_manifesto.md).

---

## Структура каталога (34 файла)

### Frontmatter (00-09)

Для тех, кто не знает, с чего начать.

- [00_readme.md](00_readme.md) — этот файл.
- [01_glossary.md](01_glossary.md) — глоссарий терминов.
- [02_how-to-read.md](02_how-to-read.md) — маршруты чтения.
- [03_about-this-guide.md](03_about-this-guide.md) — зачем гайд
  существует, как устроен.
- [04_what-is-a-game-engine.md](04_what-is-a-game-engine.md) — что такое
  игровой движок и какие у него слои.
- [05_hardware-tour.md](05_hardware-tour.md) — аппаратный ликбез.
- [06_why-cpp26.md](06_why-cpp26.md) — почему C++26, статус на июнь
  2026.

### Фундамент (10-19)

Базовые принципы разработки.

- [10_manifesto.md](10_manifesto.md) — производительность как
  обязательство.
- [11_anti-patterns.md](11_anti-patterns.md) — чего не делать в hot
  path.
- [12_decision-making.md](12_decision-making.md) — как принимать
  технические решения.
- [13_evil-hacks.md](13_evil-hacks.md) — когда нарушение правил
  оправдано.
- [14_compiler.md](14_compiler.md) — диалог с компилятором.
- [15_compile-time.md](15_compile-time.md) — модули, constexpr,
  метапрограммирование.
- [16_memory.md](16_memory.md) — аллокаторы, cache coherence.
- [17_error-handling.md](17_error-handling.md) — обработка ошибок.
- [18_data-layout.md](18_data-layout.md) — padding, alignment, SoA.
- [19_debugging.md](19_debugging.md) — телеметрия и санитайзеры.

### Парадигмы (20-29)

Какие подходы использовать и почему.

- [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md) — C++26
  абстракции без рантайм-стоимости.
- [21_dod.md](21_dod.md) — Data-Oriented Design.
- [22_ecs.md](22_ecs.md) — Entity Component System.
- [23_concurrency.md](23_concurrency.md) — Job System и lock-free
  структуры.
- [24_data-flow.md](24_data-flow.md) — границы, double buffering,
  FrameGraph.
- [25_strings.md](25_strings.md) — строки и StringID.

### Домен (30-39)

Специфика движков: оптимизация, рендеринг, воксели, тестирование,
математика, время.

- [30_optimization.md](30_optimization.md) — иерархия оптимизации.
- [31_vulkan.md](31_vulkan.md) — Vulkan 1.4 без legacy.
- [32_voxel-data.md](32_voxel-data.md) — GPU-driven воксели.
- [33_testing.md](33_testing.md) — тестирование инвариантов.
- [34_math-and-space.md](34_math-and-space.md) — численная точность в
  больших мирах.
- [35_time-and-determinism.md](35_time-and-determinism.md) — Fixed Time
  Step и детерминизм.

### Практика (90-99)

Инструменты, источники, методология, CI.

- [90_code-review-checklist.md](90_code-review-checklist.md) — чек-лист
  для code review.
- [91_tooling-landscape.md](91_tooling-landscape.md) — обзор
  инструментов и версий.
- [92_external-sources.md](92_external-sources.md) — канонические
  внешние источники.
- [93_performance-methodology.md](93_performance-methodology.md) —
  методология измерений.
- [94_build-and-ci.md](94_build-and-ci.md) — build, CI, release flow.

---

## Основные принципы (одним блоком)

1. **Производительность — обязательство.** Медленный код — архитектурная
   ошибка. См. [10_manifesto.md](10_manifesto.md),
   [30_optimization.md](30_optimization.md).
2. **Данные важнее кода.** Структуры данных определяют архитектуру. См.
   [21_dod.md](21_dod.md), [18_data-layout.md](18_data-layout.md).
3. **Явный контроль.** Никакой скрытой магии в критических путях. См.
   [13_evil-hacks.md](13_evil-hacks.md),
   [11_anti-patterns.md](11_anti-patterns.md).
4. **Прагматизм.** Инструменты под задачу. См.
   [06_why-cpp26.md](06_why-cpp26.md),
   [91_tooling-landscape.md](91_tooling-landscape.md).
5. **Знай своё железо.** Кэш-линии, MESI, branch prediction, GPU
   warp/wavefront. См. [05_hardware-tour.md](05_hardware-tour.md),
   [16_memory.md](16_memory.md).

---

## Где ещё искать ответы

- **Код конкретного проекта** — единственный источник истины о том,
  **как** что-то сделано.
- **Engineering contracts проекта** — конкретные обязательства.
- **Roadmap проекта** — что в работе, что заблокировано.
- **Vendor-документация** конкретных библиотек.

---

## Пример реализации: ProjectV

ProjectV — песочница в духе War Thunder, Foxhole, HoI4, Warno,
Supreme Commander, Minecraft и Garry's Mod. Используется как иллюстрация
описанных принципов в большинстве файлов каталога.

ProjectV — не субъект этого гайда. Гайд описывает универсальные
принципы; ProjectV — пример их применения.

---

*Сложность — враг. Простота — оружие.*