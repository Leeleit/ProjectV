# Поток данных между системами

Документ описывает правила передачи данных между системами в ECS,
CPU↔GPU синхронизацию, FrameGraph.

---

## Границы — главный принцип

Данные не «текут свободно» между системами. Каждая система имеет
чёткие границы: что она читает, что пишет, когда.

Границы определяются:

1. **Данными:** система читает одни компоненты, пишет другие.
2. **Временем:** система работает в определённой фазе кадра.
3. **Синхронизацией:** переходы между фазами требуют явных barrier.

Система без границ — god-system. Архитектура без границ — спагетти.

---

## ECS как система границ

flecs автоматически даёт границы через query API.

```cpp
world.system<Position, const Velocity>("MovementSystem")
    .kind(flecs::OnUpdate)
    .each([](Position& p, const Velocity& v, float dt) {
        p.x += v.vx * dt;
        p.y += v.vy * dt;
    });
```

Система объявляет:

- Какие компоненты читает: `const Velocity`.
- Какие компоненты пишет: `Position`.
- Когда работает: фаза `OnUpdate`.

flecs гарантирует, что системы с пересекающимися write-зависимостями
не выполняются параллельно. Двойной buffer между write и read
автоматический.

### Фазы кадра

Определяются в ECS pipeline:

- `OnLoad`: загрузка ассетов.
- `OnUpdate`: игровая логика (movement, AI, физика).
- `OnValidate`: проверки инвариантов.
- `OnStore`: сохранение состояния.
- `OnRender`: подготовка данных для GPU.

Между фазами — implicit barrier.

---

## Double Buffering

Кадр N читает из State N, кадр N+1 пишет в State N+1. Никаких гонок
данных.

Реализация в ECS:

```cpp
struct PositionState {
    Position current;
    Position previous;
};

// Кадр N: системы читают previous, пишут current
// Кадр N+1: swap(current, previous)
```

Системы рендеринга используют `previous` для интерполяции между
состояниями.

### Применение

- Физика: предыдущее и текущее состояние для интерполяции.
- AI: текущее решение и предыдущее для отладки.
- Сеть: отправка дельты состояния.

---

## Передача данных в Vulkan: от ECS к GPU

### Прямой SSBO upload

Компоненты ECS хранятся в массивах. Эти массивы напрямую копируются в
SSBO.

```cpp
positions.each_chunk([&](flecs::iter& it) {
    auto slice = it.get<Position>();
    vkCmdBindBuffer(cmd, ..., positions_ssbo);
    vkCmdPushConstants(cmd, ..., sizeof(u32), &it.count);
});
```

CPU↔GPU transfer через PCIe: ~32 GB/s (PCIe Gen4 x16). В 50× медленнее
VRAM. Минимизируется через batching.

### Vulkan timeline semaphores

Timeline semaphores — кросс-очередная синхронизация.

```cpp
vkQueueSubmit(graphics_queue, ..., timeline_value++);
vkQueueSubmit(graphics_queue, ..., wait_timeline_value++);
```

Семантика: кадр N+1 ждёт завершения кадра N перед использованием
общих ресурсов.

### Persistent descriptor sets

Descriptor sets создаются один раз. Обновляются через
`vkUpdateDescriptorSets` без пересоздания pipeline.

---

## Границы синхронизации

### CPU↔CPU: явные events

Когда две CPU-задачи зависят друг от друга, зависимость объявляется в
DAG явно. Планировщик вставляет barrier.

### CPU↔GPU: Vulkan sync objects

- **Fence:** CPU ↔ GPU синхронизация. CPU ждёт завершения GPU submit.
- **Semaphore:** GPU ↔ GPU синхронизация между очередями.
- **Event:** тонкая синхронизация внутри command buffer.
- **Barrier:** синхронизация доступа к памяти внутри GPU.

### GPU↔CPU: только явный readback

CPU не читает GPU-память напрямую. Операции:

1. GPU пишет в buffer.
2. `vkQueueSubmit` с fence.
3. CPU ждёт fence (`vkWaitForFences`).
4. CPU читает `vkMapMemory`.

Чтение GPU данных на CPU — антипаттерн в hot path. Только для отладки.

---

## FrameGraph: декларативный pipeline

FrameGraph (Kerem Tuncer, University of Vienna, Vulkanised 2026) —
декларативное описание render-pipeline через data dependencies.

```cpp
auto graph = framegraph::Builder()
    .add_pass("GBuffer", &gbuffer_data)
    .add_pass("Lighting", &light_data)
        .reads(gbuffer_data)
        .writes(light_data)
    .add_pass("Tonemap", &framebuffer)
        .reads(light_data)
        .writes(framebuffer)
    .build();
```

FrameGraph автоматически:

- Вычисляет execution order.
- Вставляет barriers.
- Делает resource aliasing.
- Оптимизирует pipeline.

Преимущества:

- Декларативное описание, не императивное.
- Автоматическая оптимизация.

---

## Минимизация CPU↔GPU round-trip

Каждый CPU↔GPU round-trip — десятки микросекунд. В real-time это
существенно.

Правила:

- Все per-frame updates — в один SSBO upload.
- Никаких per-draw uploads.
- Pipeline switching через pipeline cache.
- Descriptor sets — persistent, обновляются через
  `vkUpdateDescriptorSets`.

---

## Принципы

1. Граница системы — это контракт. Что читает, что пишет, когда.
2. ECS queries автоматически дают контракты через query API.
3. Double buffering — основной механизм безопасной передачи данных.
4. CPU↔GPU синхронизация — explicit, не implicit.
5. FrameGraph — путь к декларативному pipeline.

---

## Пример: применение в движке

В ProjectV компоненты ECS хранятся в SSBO через persistent descriptor
sets. Timeline semaphores координируют CPU compute-queue и GPU
graphics-queue. FrameGraph — плановая замена ручного управления
барьерами.

---

## Источники и дальнейшее чтение

- **flecs documentation** — pipeline, queries, OnUpdate phases.
  <https://www.flecs.dev/flecs/>
- **Kerem Tuncer — FrameGraph** (Vulkanised 2026, University of
  Vienna).
- **Vulkan Timeline Semaphores** — Vulkan 1.2 core.
- [22_ecs.md](22_ecs.md) — ECS.
- [23_concurrency.md](23_concurrency.md) — Job System, DAG.
- [31_vulkan.md](31_vulkan.md) — Vulkan API.