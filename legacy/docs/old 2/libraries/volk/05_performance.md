# Производительность volk

**🟡 Уровень 2: Средний**

Оптимизация dispatch overhead.

---

## Dispatch Overhead

### Что это

При каждом вызове Vulkan функции через loader:

```
Приложение
    ↓
vulkan-1.dll (loader) — диспетчеризация
    ↓
Validation Layers — проверки (если включены)
    ↓
Драйвер GPU — выполнение
```

Каждый переход добавляет накладные расходы.

### Величина overhead

| Тип нагрузки                                | Overhead через loader  |
|---------------------------------------------|------------------------|
| Device-intensive (vkCmdDraw, vkCmdDispatch) | До 7%                  |
| Instance-intensive (vkCreateDevice)         | Минимальный            |
| Смешанная                                   | Зависит от соотношения |

---

## Как volk устраняет overhead

### Без volk

```cpp
// Компиляция: вызов через PLT/GOT
vkCmdDraw(cmd, ...);

// Runtime:
// 1. PLT lookup → loader dispatch
// 2. Loader dispatch → layer chain
// 3. Layer chain → driver
```

### С volk после volkLoadDevice

```cpp
// Указатель уже заполнен
vkCmdDraw(cmd, ...);

// Runtime:
// 1. Прямой вызов по указателю → driver (если нет layers)
```

---

## Когда использовать volkLoadDevice

### Обязательно

- Частые draw/dispatch вызовы (более 100 на кадр)
- Compute-intensive приложения
- Real-time рендеринг

### Не критично

- Редкие Vulkan вызовы
- UI-приложения
- Tools и утилиты

---

## Измерение производительности

### Benchmark код

```cpp
#include <chrono>

void benchmark_draw_calls(VkCommandBuffer cmd, uint32_t count) {
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < count; ++i) {
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double callsPerSecond = count / (duration.count() / 1e6);
    printf("%.0f calls/sec (%.2f us/call)\n", callsPerSecond, duration.count() / double(count));
}
```

### Типичные результаты

| Конфигурация                | Calls/sec | Overhead          |
|-----------------------------|-----------|-------------------|
| Через loader, без layers    | ~2M       | Baseline          |
| Через volk (volkLoadDevice) | ~2.15M    | ~7% быстрее       |
| Через loader + validation   | ~0.5M     | Зависит от layers |

---

## Validation Layers

volk не устраняет overhead от validation layers.

### С layers

```cpp
// При включённых layers
vkCreateInstance(..., &instance);
volkLoadInstance(instance);

// Указатели указывают на layer functions
vkCmdDraw(...);  // → layer → driver
```

### Без layers (Release build)

```cpp
// При выключенных layers
vkCreateInstance(..., &instance);
volkLoadInstance(instance);
vkCreateDevice(..., &device);
volkLoadDevice(device);

// Указатели указывают напрямую на driver
vkCmdDraw(...);  // → driver
```

---

## Дополнительные оптимизации

### Multi-Draw Indirect

Снижает количество вызовов `vkCmdDraw`:

```cpp
// Вместо N вызовов
for (uint32_t i = 0; i < N; ++i) {
    vkCmdDraw(cmd, ...);
}

// Один вызов
vkCmdDrawIndirect(cmd, indirectBuffer, 0, N, stride);
```

### Batch operations

Группировка операций:

```cpp
// Вместо множества копирований
for (auto& copy : copies) {
    vkCmdCopyBuffer(cmd, copy.src, copy.dst, 1, &copy.region);
}

// Одно копирование с массивом регионов
VkBufferCopy regions[N];
vkCmdCopyBuffer(cmd, src, dst, N, regions);
```

### Pipeline caching

Снижает overhead создания pipelines:

```cpp
VkPipelineCache cache;
vkCreatePipelineCache(device, &cacheInfo, nullptr, &cache);

// При повторном запуске — быстрое создание
vkCreateGraphicsPipelines(device, cache, pipelineCount, &createInfos, nullptr, pipelines);
```

---

## Рекомендации

### Debug сборка

1. Включите validation layers
2. Используйте `volkLoadInstance(instance)` — device функции через loader
3. Overhead от layers перекрывает выигрыш от volk

### Release сборка

1. Отключите validation layers
2. Используйте `volkLoadDevice(device)` — прямые вызовы
3. Комбинируйте с multi-draw indirect

### Множество устройств

```cpp
// Используйте таблицы для каждого устройства
VolkDeviceTable tables[N];
for (uint32_t i = 0; i < N; ++i) {
    volkLoadDeviceTable(&tables[i], devices[i]);
}

// Вызовы через таблицы
tables[deviceIndex].vkCmdDraw(...);
```

---

## Профилирование

### Измерение dispatch overhead

```cpp
// Benchmark с volkLoadInstance (через loader)
volkLoadInstance(instance);
benchmark_draw_calls(cmd, 1000000);

// Benchmark с volkLoadDevice (прямые)
volkLoadDevice(device);
benchmark_draw_calls(cmd, 1000000);

// Сравните результаты
```

### Интеграция с профилировщиками

volk предоставляет указатели `volkGetInstanceProcAddr` и `volkGetDeviceProcAddr` для интеграции:

```cpp
// Для GPU profiling
auto pfn_vkGetInstanceProcAddr = volkGetInstanceProcAddr;
auto pfn_vkGetDeviceProcAddr = volkGetDeviceProcAddr;

// Передайте эти указатели в профилировщик
```

---

## Итог

| Оптимизация         | Эффект | Сложность |
|---------------------|--------|-----------|
| `volkLoadDevice`    | До 7%  | Низкая    |
| Multi-Draw Indirect | До 50% | Средняя   |
| Batch operations    | До 30% | Низкая    |
| Pipeline caching    | До 15% | Низкая    |

volk — простая оптимизация с измеримым эффектом для device-intensive workloads.
