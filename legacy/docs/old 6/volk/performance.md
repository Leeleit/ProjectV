# Производительность volk

**🟡 Уровень 2: Средний** — Оптимизация производительности Vulkan вызовов с использованием volk.

## Dispatch Overhead: Теоретические основы

Vulkan использует dispatch цепочку для вызова функций:

1. **Приложение** → Глобальный указатель функции
2. **Vulkan Loader** (`vulkan-1.dll` / `libvulkan.so`) → Проверка и маршрутизация
3. **Validation Layers** (если включены) → Дополнительные проверки
4. **GPU Driver** → Фактическая реализация функции

**volk устраняет первый уровень dispatch**, предоставляя прямые указатели на функции драйвера после вызова
`volkLoadDevice(device)`.

## Ожидаемое улучшение производительности

### Device-intensive workloads

Для интенсивных рабочих нагрузок с частыми вызовами device функций наблюдается следующее улучшение:

| Тип вызовов                    | Ожидаемое улучшение | Комментарий                                        |
|--------------------------------|---------------------|----------------------------------------------------|
| `vkCmdDraw*` (рендеринг)       | До 7% ускорения     | Наиболее заметно при рендеринге множества объектов |
| `vkCmdDispatch` (compute)      | До 7% ускорения     | Критично для compute-heavy приложений              |
| `vkCmdCopy*` (передача данных) | До 5% ускорения     | При частых передачах CPU↔GPU                       |
| `vkQueueSubmit`                | Минимальный эффект  | Один вызов на кадр                                 |

### Instance-intensive workloads

Для вызовов instance функций улучшение минимально, так как они и так вызываются редко:

- `vkCreateInstance`, `vkEnumeratePhysicalDevices` и др. — нет заметного эффекта

## Методика измерения производительности

### Бенчмарк для оценки dispatch overhead

```cpp
#include <chrono>
#include <vector>

void benchmark_vulkan_calls(VkCommandBuffer cmd, uint32_t iterationCount) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (uint32_t i = 0; i < iterationCount; ++i) {
        // Тестируемые вызовы Vulkan
        vkCmdDraw(cmd, 3, 1, 0, 0);  // Один треугольник
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double callsPerSecond = iterationCount / (duration.count() / 1e6);
    printf("%.2f вызовов/сек\n", callsPerSecond);
}
```

### Сравнительные тесты

Для объективной оценки производительности рекомендуется:

1. **Тестировать с отключенными validation layers** в релизных сборках
2. **Использовать одинаковые настройки драйвера** для всех тестов
3. **Проводить многократные замеры** и брать медианное значение
4. **Тестировать на различных GPU** от разных производителей

## Оптимизации, совместимые с volk

### 1. Multi-Draw Indirect

Группировка множества draw вызовов в один indirect вызов:

```cpp
// Плохо: Множественные вызовы vkCmdDraw
for (uint32_t i = 0; i < chunkCount; ++i) {
    vkCmdDraw(cmd, vertexCount[i], 1, firstVertex[i], 0);
}

// Хорошо: Один вызов vkCmdDrawIndirect
vkCmdDrawIndirect(cmd, indirectBuffer, 0, chunkCount, sizeof(VkDrawIndirectCommand));
```

### 2. Compute Shader Batching

Группировка compute dispatch вызовов:

```cpp
// Вместо множественных вызовов vkCmdDispatch
vkCmdDispatch(cmd, totalWorkGroupsX, totalWorkGroupsY, totalWorkGroupsZ);
```

### 3. Buffer Copy Batching

Объединение копирований буферов:

```cpp
VkBufferCopy copyRegions[] = {
    {srcOffset1, dstOffset1, size1},
    {srcOffset2, dstOffset2, size2},
    // ...
};

vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, regionCount, copyRegions);
```

## Влияние Validation Layers

Validation layers добавляют значительный overhead к вызовам Vulkan:

| Конфигурация                   | Относительная производительность | Рекомендации                    |
|--------------------------------|----------------------------------|---------------------------------|
| **Без validation layers**      | 100% (базовая)                   | Для релизных сборок             |
| **С validation layers**        | 40-60% от базовой                | Только для отладки и разработки |
| **С volk + validation layers** | 45-65% от базовой                | Все ещё значительно медленнее   |

**Важно:** volk не устраняет overhead от validation layers. Для максимальной производительности отключайте layers в
релизных сборках.

## Рекомендации по оптимизации

### Для новых проектов

1. Всегда используйте `volkLoadDevice(device)` после создания логического устройства
2. Отключайте validation layers в релизных сборках
3. Проектируйте архитектуру рендеринга с учетом возможностей группировки вызовов

### Для существующих проектов

1. Добавьте volk через `volkInitialize()` и `volkLoadInstance(instance)`
2. Замените `volkLoadInstance` на `volkLoadDevice` для device функций
3. Проведите профилирование для определения наиболее частых Vulkan вызовов
4. Оптимизируйте эти вызовы через группировку (multi-draw, batched copies)

## Профилирование с Tracy

Tracy может помочь определить dispatch overhead в вашем приложении:

```cpp
#include "tracy/TracyVulkan.hpp"

// Инициализация Tracy с volk
TracyVkContext tracyContext = TracyVkContext(
    device,
    queue,
    vkQueueSubmit,      // Указатель из volk
    vkCmdBeginQuery,    // Указатель из volk
    vkCmdEndQuery       // Указатель из volk
);

// Профилирование вызовов Vulkan
void render_with_profiling(TracyVkContext ctx, VkCommandBuffer cmd) {
    TracyVkZone(ctx, cmd, "Vulkan Dispatch");
    
    // Ваши Vulkan вызовы
    vkCmdDraw(cmd, ...);
    
    TracyVkCollect(ctx, cmd);
}
```

## Сравнение с другими оптимизациями

| Оптимизация              | Эффективность                | Сложность внедрения | Совместимость с volk |
|--------------------------|------------------------------|---------------------|----------------------|
| **volk (прямые вызовы)** | До 7%                        | Низкая              | Полная               |
| **Multi-Draw Indirect**  | До 50% (для многих объектов) | Средняя             | Полная               |
| **Bindless rendering**   | До 30%                       | Высокая             | Полная               |
| **Pipeline caching**     | До 15% (первый запуск)       | Низкая              | Полная               |
| **Async compute**        | До 20% (зависит от нагрузки) | Высокая             | Полная               |

## Практические примеры

### Пример 1: Рендеринг воксельного чанка

```cpp
void render_voxel_chunk(VkCommandBuffer cmd, const Chunk& chunk) {
    // Без volk: Каждый vkCmdDraw проходит через loader dispatch
    for (const auto& mesh : chunk.meshes) {
        vkCmdDraw(cmd, mesh.vertexCount, 1, mesh.firstVertex, 0);
    }
    
    // С volk: Прямые вызовы к драйверу
    // + multi-draw для дальнейшей оптимизации
    if (chunk.indirectBuffer) {
        vkCmdDrawIndirect(cmd, chunk.indirectBuffer, 0, chunk.meshCount, sizeof(VkDrawIndirectCommand));
    }
}
```

### Пример 2: Compute шейдер для генерации вокселей

```cpp
void dispatch_voxel_generation(VkCommandBuffer cmd, uint32_t chunkCount) {
    // Группировка dispatch вызовов
    uint32_t totalGroups = (chunkCount + 63) / 64; // 64 work items на chunk
    
    // Прямой вызов к драйверу через volk
    vkCmdDispatch(cmd, totalGroups, 1, 1);
}
```

## Заключение

volk предоставляет простое и эффективное средство для уменьшения dispatch overhead в Vulkan приложениях. Ключевые
преимущества:

1. **Простота интеграции**: Минимальные изменения в коде
2. **Измеримый эффект**: До 7% ускорения для device-intensive workloads
3. **Совместимость**: Работает со всеми Vulkan расширениями и validation layers
4. **Гибкость**: Поддержка как глобальных указателей, так и таблиц для multi-device

Для максимальной производительности комбинируйте volk с другими оптимизациями: multi-draw indirect, compute batching и
эффективным управлением памятью.

← [Назад: Сценарии использования](use-cases.md) | [Далее: Decision Trees](decision-trees.md) →
