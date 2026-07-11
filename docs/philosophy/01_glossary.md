# Глоссарий

Глобальный справочник терминов, на которые ссылаются документы в
`docs/philosophy/`. Краткое определение и ссылка на основной документ.

Соглашение: термины идут в алфавитном порядке внутри категорий. Каждая
запись — 1-2 строки + ссылка.

---

## Аппаратное обеспечение

### Apple Silicon

Семейство ARM-процессоров Apple (M1-M4). 128-байтные cache lines,
отсутствие L3 в пользу единого L2 до 16 MB на кластер, unified memory.

Для cross-platform совместимости — `alignas(std::hardware_destructive_interference_size)`.
Подробнее: [16_memory.md](16_memory.md).

### AVX / AVX-512 / NEON

SIMD-расширения x86 и ARM. В C++26 — кроссплатформенная абстракция
`std::simd` (P1928).

Подробнее: [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md).

### Branch Misprediction

CPU «угадывает» ветку `if`. Угадал — конвейер летит, ошибся — сброс
(10-20 тактов штрафа). Минимизация: branchless код, сортировка данных.

Подробнее: [10_manifesto.md](10_manifesto.md).

### Cache Line

Атомарная единица обмена CPU-RAM: 64 байта на x86, 128 байт на Apple
Silicon.

Подробнее: [10_manifesto.md](10_manifesto.md), [05_hardware-tour.md](05_hardware-tour.md).

### Cache Coherence (MESI / MESIF / MOESI)

Протоколы согласованности кэшей между ядрами. Intel — MESIF, AMD —
MOESI.

Подробнее: [16_memory.md](16_memory.md).

### DDR5

Стандарт DRAM 2025-2026. Пропускная способность 50-100 GB/s sequential,
латентность 200-300 тактов.

Подробнее: [34_math-and-space.md](34_math-and-space.md).

### False Sharing

Атомики или часто изменяемые данные разных потоков в одной кэш-линии.
Решение: `alignas(std::hardware_destructive_interference_size)`.

Подробнее: [11_anti-patterns.md](11_anti-patterns.md), [16_memory.md](16_memory.md).

### Hardware Prefetchers

Аппаратные блоки CPU, угадывающие паттерн доступа к памяти. Линейный
доступ: ~50 GB/s. Случайный порядок: ~1 GB/s.

Подробнее: [16_memory.md](16_memory.md).

### ULP (Unit in the Last Place)

Минимальное расстояние между двумя соседними представимыми `float`
числами. Растёт экспоненциально: ULP(1.0) ≈ 1.19 × 10⁻⁷, ULP(10⁷) = 1.0.

Подробнее: [34_math-and-space.md](34_math-and-space.md).

### x86 / x86_64 / AMD64

Доминирующая десктопная и серверная архитектура. Primary dev-host на
Linux. Target hardware — NVIDIA RTX 20/30/40/50 (Turing RT cores или
новее).

Подробнее: [10_manifesto.md](10_manifesto.md).

---

## Данные и алгоритмы

### Archetype (ECS)

Группа сущностей с одинаковым набором компонентов. В flecs v4 каждая
archetype хранит компоненты в SoA layout.

Подробнее: [22_ecs.md](22_ecs.md).

### AoS (Array of Structures)

Хранение данных как массива объектов с полями. Плохо для batch-processing
на миллионах элементов.

Подробнее: [21_dod.md](21_dod.md), [18_data-layout.md](18_data-layout.md).

### Bindless Rendering

Паттерн «один descriptor set, много ресурсов, индексация в шейдере».

Подробнее: [31_vulkan.md](31_vulkan.md).

### Cluster (Meshlet / Cluster Culling)

Группа примитивов (meshlet), обрабатываемая в одном workgroup. Cluster
Culling Shaders — Nanite-style virtualized geometry.

Подробнее: [31_vulkan.md](31_vulkan.md), [32_voxel-data.md](32_voxel-data.md).

### DOD (Data-Oriented Design)

Мышление данными и их трансформациями. 12 принципов Майка Актона (2014),
переосмыслены Теодореску (2022) и Ромео (2025).

Подробнее: [21_dod.md](21_dod.md), [10_manifesto.md](10_manifesto.md).

### Double Buffering

Кадр N читает, кадр N+1 пишется. Никаких гонок данных.

Подробнее: [24_data-flow.md](24_data-flow.md).

### ECS (Entity Component System)

Композиция вместо наследования. Сущность — ID, компоненты — данные,
системы — функции трансформации.

Подробнее: [22_ecs.md](22_ecs.md).

### Fixed-Point арифметика

Целое число с фиксированной точкой. Детерминизм на любом железе.

Подробнее: [34_math-and-space.md](34_math-and-space.md), [35_time-and-determinism.md](35_time-and-determinism.md).

### FrameGraph

Декларативное описание render pipeline через data dependencies.

Подробнее: [24_data-flow.md](24_data-flow.md).

### Hot/Cold Splitting

Разделение часто и редко используемых данных на разные структуры.

Подробнее: [18_data-layout.md](18_data-layout.md).

### Indirect Draw

`vkCmdDrawIndexedIndirect` — параметры draw call в GPU buffer.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Mesh Shaders (Task + Mesh)

Vulkan extension: замена vertex+geometry pipeline.

Подробнее: [31_vulkan.md](31_vulkan.md), [32_voxel-data.md](32_voxel-data.md).

### NanoVDB

Sparse VDB структура для GPU.

Подробнее: [32_voxel-data.md](32_voxel-data.md).

### Pointer Chasing

`entity→component→data→field` — каждая стрелочка потенциальный cache
miss.

Подробнее: [21_dod.md](21_dod.md), [18_data-layout.md](18_data-layout.md).

### SoA (Structure of Arrays)

Хранение каждого поля в отдельном массиве. Cache-friendly, SIMD-friendly.

Подробнее: [21_dod.md](21_dod.md), [18_data-layout.md](18_data-layout.md).

### Sparse64Tree

Flat иерархия вокселей с 64-битным ключом. Альтернатива SVO.

Подробнее: [32_voxel-data.md](32_voxel-data.md).

### Sparse Voxel Octree (SVO)

Иерархическое деление пространства на octant-ы. Laine & Karras 2010.

Подробнее: [32_voxel-data.md](32_voxel-data.md).

### StringID

64-битный хеш строки + lookup table. Замена `std::string` в hot path.

Подробнее: [25_strings.md](25_strings.md).

---

## Архитектурные паттерны

### BDA (Buffer Device Address)

64-битные GPU указатели на буферы (Vulkan 1.2 core).

Подробнее: [31_vulkan.md](31_vulkan.md).

### DDGI (Dynamic Diffuse Global Illumination)

Probe-based global illumination.

Подробнее: [32_voxel-data.md](32_voxel-data.md).

### flecs v4

Архетипный ECS. C99 API, lockless scheduler, hierarchies, staging.

Подробнее: [22_ecs.md](22_ecs.md), [91_tooling-landscape.md](91_tooling-landscape.md).

### Frame Allocator

Linear allocator для данных с временем жизни ровно один кадр.

Подробнее: [16_memory.md](16_memory.md).

### Linear Allocator

Самый быстрый аллокатор — сдвиг указателя.

Подробнее: [16_memory.md](16_memory.md).

### Stack Allocator

LIFO allocator для рекурсивных алгоритмов.

Подробнее: [16_memory.md](16_memory.md).

### Pool Allocator

Для объектов одинакового размера.

Подробнее: [16_memory.md](16_memory.md).

### PMR (Polymorphic Memory Resources, C++17)

Стандартизированный интерфейс аллокаторов.

Подробнее: [16_memory.md](16_memory.md).

### Property-Based Testing

Генерация случайных входных данных и проверка инвариантов.

Подробнее: [33_testing.md](33_testing.md).

### Senders / Receivers (P2300 / `std::execution`)

Стандартизированная асинхронность в C++26.

Подробнее: [23_concurrency.md](23_concurrency.md), [06_why-cpp26.md](06_why-cpp26.md).

### Timeline Semaphores

Vulkan semaphores с monotonic counter value.

Подробнее: [24_data-flow.md](24_data-flow.md).

---

## GPU и Vulkan

### Cluster Culling Shaders (`VK_EXT_cluster_culling_shader`)

Расширение поверх mesh shaders, Nanite-style geometry.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Dynamic Rendering (Vulkan 1.3 core)

Без `VkRenderPass` и `VkFramebuffer`.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Dynamic Rendering Local Read (`VK_KHR_dynamic_rendering_local_read`)

Доступ к attachments из compute/transfer без subpass dependencies.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Opacity Micromaps (OMM)

RT-фича для alpha-tested геометрии. Blackwell+ NVIDIA.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Shader Execution Reordering (SER)

NVIDIA RTX-фича для снижения divergence в ray-tracing шейдерах.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Validation Layers

VK_LAYER_KHRONOS_validation. Только Debug.

Подробнее: [31_vulkan.md](31_vulkan.md).

### Vulkan 1.4

Каноническая спека. Все ключевые extensions promoted в core.

Подробнее: [31_vulkan.md](31_vulkan.md).

---

## C++ и стандарт

### Concepts (C++20)

Ограничения на template-параметры. Замена SFINAE.

Подробнее: [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md).

### Constexpr / Consteval

`constexpr` — функция может быть вычислена на этапе компиляции.
`consteval` (C++20) — обязательно compile-time.

Подробнее: [15_compile-time.md](15_compile-time.md).

### Contracts (P2900R14, C++26)

`[[pre:...]]`, `[[post:...]]`, `contract_assert` — замена `assert`.

Подробнее: [17_error-handling.md](17_error-handling.md), [06_why-cpp26.md](06_why-cpp26.md).

### `import std;` (C++23+)

Стандартная библиотека как модуль.

Подробнее: [15_compile-time.md](15_compile-time.md).

### Modules (C++20)

`module Foo;` / `import Foo;` — компилированные интерфейсы вместо
текстовых `#include`.

Подробнее: [15_compile-time.md](15_compile-time.md).

### Reflection (P2996R13, C++26)

`std::meta::info`, `^ClassName` splice, `template_for`.

Подробнее: [15_compile-time.md](15_compile-time.md), [06_why-cpp26.md](06_why-cpp26.md).

### `std::expected<T, E>` (C++23)

Типобезопасная замена исключений и кодов ошибок.

Подробнее: [17_error-handling.md](17_error-handling.md).

### `std::execution` (P2300R10, C++26)

Senders / Receivers.

Подробнее: [23_concurrency.md](23_concurrency.md), [06_why-cpp26.md](06_why-cpp26.md).

### `std::linalg` (P1673R7, C++26)

Стандартизированный BLAS.

Подробнее: [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md).

### `std::mdspan` (C++23)

Multidimensional view без аллокаций.

### `std::simd` (P1928, C++26)

Кроссплатформенная SIMD-абстракция.

Подробнее: [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md).

### `std::span` (C++20)

Non-owning view на contiguous range.

Подробнее: [20_zero-cost-abstractions.md](20_zero-cost-abstractions.md).

---

## Тестирование и отладка

### AddressSanitizer (ASan)

Обнаруживает use-after-free, buffer overflow, double free.

Подробнее: [19_debugging.md](19_debugging.md), [14_compiler.md](14_compiler.md).

### PGO (Profile-Guided Optimization)

Двухпроходная компиляция. 5-15% прирост.

Подробнее: [14_compiler.md](14_compiler.md).

### ThreadSanitizer (TSan)

Обнаруживает data races.

Подробнее: [19_debugging.md](19_debugging.md).

### Tracy Profiler

Frame profiler с CPU + GPU + memory + locks. Vulkan contexts.

Подробнее: [19_debugging.md](19_debugging.md), [91_tooling-landscape.md](91_tooling-landscape.md).

### UndefinedBehaviorSanitizer (UBSan)

Обнаруживает signed overflow, null deref, out-of-bounds.

Подробнее: [19_debugging.md](19_debugging.md).

---

## Математика и время

### Accumulator (Fixed Timestep)

Накопитель реального времени. Fixed step выполняется, пока
accumulator >= FIXED_DT.

Подробнее: [35_time-and-determinism.md](35_time-and-determinism.md).

### Determinism

Одна и та же последовательность операций даёт один и тот же результат
на любом железе.

Подробнее: [35_time-and-determinism.md](35_time-and-determinism.md).

### Floating Point (float, double)

IEEE 754. `float` — 32-bit. `double` — 64-bit.

Подробнее: [34_math-and-space.md](34_math-and-space.md).

### Origin Shifting

Глобальные координаты разбиты на секторы. Entity position = sector +
local.

Подробнее: [34_math-and-space.md](34_math-and-space.md).

### Quaternions

Гиперкомплексные числа для представления вращения.

### Spiral of Death

Кадр 100 ms → accumulator накапливает 100 ms → цикл. Решение: clamp
accumulator.

Подробнее: [35_time-and-determinism.md](35_time-and-determinism.md).

---

## Безопасность типов и контракты

### `[[nodiscard]]`

Функция возвращает значение, которое нельзя игнорировать.

### `noexcept`

Функция не бросает исключений.

### `static_assert`

Compile-time проверка условия.

---

## Примеры реализации

Каталог ссылается на ProjectV как пример реализации описанных принципов.

ProjectV — песочница в духе War Thunder, Foxhole, HoI4, Warno, Supreme
Commander, Minecraft и Garry's Mod. Используется как иллюстрация в
большинстве файлов.

---

## Канонические внешние источники

См. [92_external-sources.md](92_external-sources.md) для полного списка
первоисточников.