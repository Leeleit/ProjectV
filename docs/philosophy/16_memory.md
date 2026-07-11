# Аллокации

Документ описывает правила выделения и освобождения памяти в
высокопроизводительных движках.

---

## Правило №1: нет кучи в runtime

Аллокации общего назначения (`malloc`, `new`, `std::make_unique`)
разрешены только на этапе инициализации или загрузки уровня. В игровом
цикле — запрет.

Причины:

- **Фрагментация.** Каждая аллокация оставляет дыры в куче.
- **Системный вызов.** `malloc` — поиск свободного блока, обновление
  таблиц. Тысячи тактов процессора.
- **Непредсказуемость.** Время аллокации зависит от состояния кучи.

---

## Кастомные аллокаторы

### Linear allocator

Самый быстрый аллокатор — сдвиг указателя. Аллокация O(1), освобождение
O(1) для всего пула через `reset()`.

Ограничения:

- Нельзя освободить отдельный объект.
- Подходит для временных данных с одинаковым временем жизни.

Применение: scratch buffers, command buffers, временные массивы в
системах ECS.

### Frame allocator

Специализированный linear allocator для данных с временем жизни ровно
один кадр. `reset()` вызывается в конце кадра.

Что хранится:

- Временные массивы трансформаций для GPU.
- Строки для отладки (логгирование).
- Промежуточные результаты вычислений.
- Команды рендеринга до отправки в GPU.

### Stack allocator

Аллокация с дисциплиной LIFO. Используется для рекурсивных алгоритмов.

### Pool allocator

Для объектов одинакового размера (частицы, события ECS, узлы графа).

- O(1) аллокация и освобождение.
- Нет фрагментации.
- Идеально для пулов объектов с фиксированным временем жизни.

---

## Мост к STL: Polymorphic Memory Resources (PMR, C++17)

PMR — стандартизированный интерфейс аллокаторов. Позволяет подменить
аллокатор для контейнеров STL без переписывания контейнеров.

PMR не решает проблему фрагментации в hot path сам по себе — решает
конкретный аллокатор, переданный в PMR.

---

## Cache Coherence (MESI / MESIF / MOESI)

Протоколы coherence управляют согласованностью кэшей между ядрами.

- **Intel:** MESIF (Modified, Exclusive, Shared, Invalid, Forward).
- **AMD:** MOESI (Modified, Owned, Exclusive, Shared, Invalid).

Принцип: когда ядро пишет в кэш-линию, остальные ядра должны узнать об
этом (линия invalidate-ится в их кэшах). Десятки тактов.

### False sharing — последствие непонимания MESI

Если два потока пишут в разные поля одной кэш-линии, ядра считают, что
линия «грязная», и invalidate-ят её друг у друга. Каждый write —
неявный synchronization overhead.

Решение: `alignas(std::hardware_destructive_interference_size)` —
разнести данные на разные кэш-линии.

---

## Hardware prefetchers

CPU имеет аппаратные prefetch-блоки:

- **Next-line prefetch:** при чтении строки автоматически загружает
  следующую.
- **Stride prefetcher:** распознаёт паттерн `arr[i], arr[i+stride],
  arr[i+2*stride]` и загружает заранее.
- **Stream prefetcher:** распознаёт последовательный доступ.

Линейный доступ к массиву: ~50 GB/s. Тот же объём в случайном порядке:
~1 GB/s.

`__builtin_prefetch` нужен, когда паттерн сложный (BVH traversal) и
железо не угадывает. В большинстве случаев hardware prefetcher
справляется сам.

---

## Apple Silicon: другая модель

ARM-процессоры Apple (M1-M4) отличаются от x86:

- **Cache line: 128 байт** (vs 64 на x86).
- **Нет L3.** Единый большой L2 до 16 MB на кластер.
- **Unified memory.** CPU и GPU разделяют одну RAM.
- **Memory bandwidth:** до 800 GB/s на M3 Ultra.

Практические следствия:

- `alignas(64)` заменить на `alignas(std::hardware_destructive_interference_size)`.
- Apple Silicon не нуждается в async upload через PCIe — данные можно
  шарить напрямую.

---

## Ландшафт аллокаторов общего назначения (2025-2026)

Кастомные аллокаторы — для hot path. Вне hot path (editor, tools,
third-party код) используются стандартные аллокаторы.

### glibc malloc (ptmalloc2)

Дефолт в Linux. Универсальный, простой, не оптимален для
многопоточности.

### jemalloc (BSD, Facebook/Meta)

Сильный в долгоживущих процессах с фрагментацией. Per-thread arena.

### tcmalloc (Apache 2.0, Google)

Сильный в масштабируемости на многопроцессорных серверах. Per-thread
cache.

### mimalloc (MIT, Microsoft)

Scalable дизайн: per-thread heap (theap), page-stealing. Минимальные
накладные расходы, bounded worst-case время аллокации. Интегрирован в
Unreal Engine 5, используется в Death Stranding.

### rpmalloc (MIT, Mattias Jansson)

Lock-free дизайн, оптимизирован для игр и real-time систем.

### Эвристика выбора

- Общий Linux-сервис: glibc, пока профиль не покажет иное.
- Долгоживущая БД / кэш: jemalloc.
- Большой многопоточный сервер: tcmalloc или mimalloc.
- Игровой движок / real-time: rpmalloc или mimalloc (по tail latency).

---

## Пример: применение в движке

В ProjectV для hot path используются кастомные аллокаторы (linear/frame/pool).
Для editor и tools — системный аллокатор (glibc на Linux). VMA 2.1
субаллоцирует GPU память через один большой VkDeviceMemory для уменьшения
количества вызовов vkAllocateMemory.

---

## Источники и дальнейшее чтение

- **mimalloc (Microsoft Research)** — обзор алгоритма.
  <https://www.microsoft.com/en-us/research/blog/mimalloc-a-high-performance-scalable-memory-allocator-for-the-modern-era/>
- **Deep dive: memory allocators 2026** — сравнение mimalloc, jemalloc,
  tcmalloc, rpmalloc.
  <https://braindetox.kr/en/posts/mimalloc_performance_allocator_2026.html>
- **Unreal Engine — Memory Allocators** — Binned malloc, mimalloc
  интеграция.
  <https://forums.unrealengine.com/t/difference-in-memory-allocators/2704565>
- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [18_data-layout.md](18_data-layout.md) — padding, alignment, SoA.
- [05_hardware-tour.md](05_hardware-tour.md) — аппаратные детали кэша.