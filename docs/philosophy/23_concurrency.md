# Конкурентность

Документ описывает модель многопоточности для высокопроизводительных
движков.

---

## Проблема с `std::thread`

Создание потока ОС — дорого:

- Системный вызов (syscall).
- Аллокация стека (1 MB+).
- Регистрация в планировщике ОС.
- Микросекунды на создание.

Переключение контекста ещё дороже:

- Сохранение всех регистров.
- Переключение контекста.
- Обновление TLB (кэш адресов).
- Потеря L1/L2 кэша.
- Тысячи тактов.

1000 задач × 1000 потоков = 1000 переключений контекста в секунду.
`std::thread` — инструмент для ОС, не для движка.

---

## Архитектура M:N

Движок использует M:N threading:

- **N worker threads** — по одному на логическое ядро CPU (минус 1-2
  для OS и аудио). Потоки никогда не спят и не блокируются.
- **M fibers (jobs)** — легковесные задачи с маленьким стеком (64 KB).
  Их тысячи.

Worker threads выполняют fibers из общей очереди. Когда fiber упирается
в ожидание (I/O, синхронизация) — worker берёт следующий fiber.
Переключение fiber — микросекунды. Переключение thread — десятки
микросекунд.

---

## Job System

Job — единица работы в Job System. Job знает свои данные и зависимости.

```cpp
auto job = world.job()
    .read(chunk_a)
    .write(chunk_b)
    .build();

world.submit(job);
```

Планировщик определяет, когда job готов к исполнению (зависимости
разрешены), и ставит в очередь worker thread.

### Work stealing

Каждый worker thread имеет свою очередь (deque). Когда очередь пуста,
worker «крадёт» job из соседней очереди.

- Балансировка нагрузки без централизованного арбитра.
- Локальность: пока у worker есть работа, он работает со своими
  данными.

### Зависимости между jobs

DAG (directed acyclic graph) задач. Job объявляет, какие данные он
читает и пишет. Планировщик строит граф и выполняет jobs в порядке
топологической сортировки.

---

## Lock-free структуры

Мьютексы (`std::mutex`) заставляют поток спать через syscall. В Job
System это антипаттерн.

### Work stealing deque

Каждый worker имеет свой deque. Операции:

- `push_bottom`: текущий worker добавляет задачу (lock-free).
- `pop_bottom`: текущий worker забирает задачу (lock-free).
- `steal`: другой worker крадёт задачу из top (lock-free через CAS).

Реализация: Chase-Lev work-stealing deque или TBB.

### MPSC queue

Multiple producers, single consumer. Используется для общения между
системами: продюсеры пишут события, консьюмер обрабатывает.

Lock-free через атомарные операции (`std::atomic`).

---

## Task Graph (DAG)

Кадр — не список последовательных функций. Кадр — DAG зависимостей.

```cpp
auto physics = world.task("Physics")
    .reads(positions, velocities)
    .writes(new_positions);

auto ai = world.task("AI")
    .reads(positions)
    .writes(ai_state);

auto render = world.task("Render")
    .reads(new_positions, ai_state, lights)
    .writes(framebuffer);

world.run({physics, ai, render});
```

Планировщик выполняет независимые tasks параллельно. Physics и AI не
зависят друг от друга — выполняются одновременно на разных ядрах.

### Преимущества

- Автоматический параллелизм: программист описывает *что*, планировщик
  решает *как*.
- Нет deadlock: DAG ацикличен по построению.
- Предсказуемая latency: граф известен до выполнения.

---

## Data Race

Data race — два потока одновременно читают и пишут одну память без
синхронизации. Результат непредсказуем.

Решения (по приоритету):

1. **Double buffering:** кадр N читает, кадр N+1 пишет.
2. **Partitioning:** разные chunks данных обрабатываются разными jobs.
3. **Read-only components:** большинство систем только читают.
4. **Lock-free структуры:** когда shared state неизбежен.

Мьютексы — последнее средство. На горячих путях они убивают
производительность.

---

## Почему не `std::async`

`std::async` — абстракция над потоками ОС. Не даёт:

- Контроля над планированием.
- Fibers.
- Графа зависимостей.
- Work stealing.

Инструмент для простых задач, не для high-performance движка.

---

## C++26 `std::execution` (P2300R10)

`std::execution` (Sender/Receiver) — стандартизированная асинхронность в
C++26.

- **Scheduler:** где выполняется работа.
- **Sender:** ленивое описание работы (lazy).
- **Receiver:** callback при завершении.

```cpp
auto sender = just(42)
    | then([](int x) { return x * 2; })
    | via(thread_pool_scheduler{});

auto [result] = sync_wait(std::move(sender)).value();
```

Преимущества:

- Композиция задач через операторы.
- Автоматическое построение графа зависимостей.
- Переносимый execution context.
- Structured concurrency.

В движке: миграция с custom Job System на `std::execution` — после
стабилизации в Clang 22+.

---

## Практика

### Разделение данных

Каждая система работает со своим набором компонентов. Система только
читает `Position` — может работать параллельно с системой, которая
пишет `Velocity`.

### Чанкование

Мир разбит на chunks. Каждый chunk обрабатывается независимым job.

### Frame-based синхронизация

Вместо fine-grained синхронизации — синхронизация на границах кадров.

### Профилирование

Tracy показывает: какие workers простаивают, какие очереди переполнены,
где contention.

---

## Правила

1. Никаких `std::thread` в hot path. Только Job System.
2. Никаких мьютексов в hot path. Lock-free структуры или
   архитектурное избегание shared state.
3. Граф зависимостей — закон. Задачи описывают зависимости,
   планировщик решает, что выполнять параллельно.
4. Данные разделены по умолчанию. Два потока пишут в одну память —
   архитектура сломана.

---

## Пример: применение в движке

В ProjectV Job System реализован через flecs worker threads с
work-stealing scheduler. ECS staging даёт автоматическую
многопоточность для систем. `std::execution` — плановая замена в
будущих версиях.

---

## Источники и дальнейшее чтение

- **P2300R10 — `std::execution` (Sender/Receiver)** — стандарт C++26.
- **NVIDIA stdexec** — reference implementation.
  <https://github.com/NVIDIA/stdexec>
- **flecs Multithreading and Staging** — DeepWiki, март 2026.
  <https://deepwiki.com/SanderMertens/flecs/5.3-multithreading-and-staging>
- [10_manifesto.md](10_manifesto.md) — данные важнее кода.
- [22_ecs.md](22_ecs.md) — ECS staging.
- [24_data-flow.md](24_data-flow.md) — поток данных между системами.
- [16_memory.md](16_memory.md) — аллокаторы в многопоточном коде.