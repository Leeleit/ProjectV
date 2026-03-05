# Vulkan Integration for ProjectV

**🔴 Уровень 3: Продвинутый** — Специфичные для ProjectV подходы к использованию Vulkan.

## Оглавление

- [Воксельный рендеринг в Vulkan](#воксельный-рендеринг-в-vulkan)
- [Архитектурные паттерны Vulkan для воксельного рендеринга](#архитектурные-паттерны-vulkan-для-воксельного-рендеринга)
- [Sparse Voxel Octree (SVO) теория](#sparse-voxel-octree-svo-теория)
- [GPU-generated LOD algorithms](#gpu-generated-lod-algorithms)
- [Современные фичи Vulkan 1.4 для воксельного рендеринга](#современные-фичи-vulkan-14-для-воксельного-рендеринга)
- [Теория синхронизации для асинхронных воксельных конвейеров](#теория-синхронизации-для-асинхронных-воксельных-конвейеров)
- [Интеграция с экосистемой ProjectV](#интеграция-с-экосистемой-projectv)
- [Оптимизация производительности](#оптимизация-производительности)
- [Типичные проблемы и решения](#типичные-проблемы-и-решения)

---

## Воксельный рендеринг в Vulkan

ProjectV как воксельный движок требует специальных подходов к рендерингу. Рассмотрим ключевые концепции с практическими
примерами:

### Compute Shaders для генерации геометрии вокселей

Вместо хранения вершин вокселей в памяти CPU, ProjectV использует compute shaders для генерации геометрии на лету. Это
позволяет:

- **Динамический LOD (Level of Detail)** в зависимости от расстояния
- **Эффективную обработку разрушаемых миров** без перестроения мешей на CPU
- **Параллельную генерацию тысяч чанков** через GPU work groups

**Пример: Простой compute shader для генерации куба (вокселя)**

```glsl
// Простой compute shader для генерации кубического вокселя
#version 460
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) writeonly buffer VertexBuffer {
    vec4 vertices[];
};

layout(std430, binding = 1) writeonly buffer IndexBuffer {
    uint indices[];
};

layout(std430, binding = 2) buffer CounterBuffer {
    atomic_uint vertexCount;
    atomic_uint indexCount;
};

// Позиции 8 вершин куба
const vec3 cubeVertices[8] = vec3[8](
    vec3(-0.5, -0.5, -0.5), vec3( 0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5), vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5),
    vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5)
);

// Индексы для 12 треугольников куба
const uint cubeIndices[36] = uint[36](
    0, 1, 2, 2, 3, 0,  // front
    1, 5, 6, 6, 2, 1,  // right
    5, 4, 7, 7, 6, 5,  // back
    4, 0, 3, 3, 7, 4,  // left
    3, 2, 6, 6, 7, 3,  // top
    4, 5, 1, 1, 0, 4   // bottom
);

void main() {
    uint baseVertex = atomicAdd(vertexCount, 8);
    uint baseIndex = atomicAdd(indexCount, 36);

    // Записываем вершины
    for (int i = 0; i < 8; i++) {
        vertices[baseVertex + i] = vec4(cubeVertices[i], 1.0);
    }

    // Записываем индексы
    for (int i = 0; i < 36; i++) {
        indices[baseIndex + i] = baseVertex + cubeIndices[i];
    }
}
```

**Пайплайн от простого к сложному:**

1. **Треугольник** → Базовый Vulkan pipeline
2. **Куб** → Compute shader генерация + инстансинг
3. **Чанк 16×16×16** → Параллельная генерация + greedy meshing
4. **Мир чанков** → GPU-driven rendering + LOD

### GPU Driven Rendering для воксельных миров

Для масштабирования до больших миров ProjectV использует GPU driven pipeline:

1. **Compute culling** — определение видимых чанков на GPU через frustum/occlusion culling
2. **Indirect drawing** — GPU генерирует draw commands через `vkCmdDrawIndexedIndirect`
3. **Bindless rendering** — доступ ко всем ресурсам через descriptor indexing (тысячи текстур)

**Архитектура GPU-Driven пайплайна для чанков:**

```cpp
struct VoxelDrawCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
    uint32_t chunkID;      // Дополнительные данные для вокселей
    uint32_t materialID;
    uint32_t LODLevel;
};

// На CPU: один вызов для всех видимых чанков
vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer,
                        0, visibleChunkCount,
                        sizeof(VoxelDrawCommand));
```

### Sparse Memory Management для воксельных миров

Воксельные миры могут быть огромными (гигабайты данных). Для эффективного использования памяти:

- **Sparse residency** — выделение памяти только для используемых регионов через `VkSparseImageMemoryBind`
- **Buffer device address** — прямой доступ к памяти из шейдеров через `VK_KHR_buffer_device_address`
- **Virtual texturing** — streaming текстур по мере необходимости с mipmap fading

### Async Compute Pipeline для параллельной обработки

Параллельное выполнение compute и graphics операций:

- **Timeline semaphores** — точная синхронизация между очередями через `VK_SEMAPHORE_TYPE_TIMELINE`
- **Separate compute queue** — изоляция compute операций в отдельную очередь family
- **Resource ownership transfer** — передача данных между очередями через `vkCmdPipelineBarrier`

---

## Архитектурные паттерны Vulkan для воксельного рендеринга

### Compute-first vs Raster-first подходы

**Compute-first архитектура** (рекомендуемая для ProjectV):

1. **Data preparation** — compute shaders обрабатывают сырые воксельные данные
2. **Geometry generation** — генерация мешей на GPU через compute pipelines
3. **Culling & LOD** — отсечение и детализация на уровне compute
4. **Indirect dispatch** — GPU-driven рендеринг с минимальным CPU overhead

**Raster-first архитектура** (традиционная):

1. **CPU processing** — подготовка геометрии на CPU
2. **Vertex submission** — передача вершин через буферы
3. **Fixed-function pipeline** — использование традиционного графического конвейера

**Сравнение для воксельных миров:**

| Параметр         | Compute-first | Raster-first |
|------------------|---------------|--------------|
| CPU нагрузка     | Низкая        | Высокая      |
| Гибкость LOD     | Динамическая  | Статическая  |
| Разрушаемость    | Реалтайм      | Сложная      |
| Масштабируемость | Линейная      | Ограниченная |

### Sparse Voxel Octree (SVO) теория

**SVO** — иерархическая структура данных для представления разреженных воксельных миров:

- **Octree nodes** — 8 детей на узел, рекурсивное подразделение
- **Sparse representation** — хранятся только ненулевые узлы
- **GPU-friendly traversal** — параллельный обход в compute shaders

**Реализация в Vulkan:**

- **Buffer device address** — прямой доступ к дереву из шейдеров
- **Descriptor indexing** — bindless доступ к узлам
- **Atomic operations** — параллельное построение дерева

**Оптимизации для ProjectV:**

- **Compressed nodes** — 64-битная упаковка (8 бит на ребёнок)
- **Mipmapped SVO** — предварительно вычисленные уровни детализации
- **GPU construction** — инкрементальное обновление при модификациях

---

## GPU-generated LOD algorithms

**Chunk-based LOD:**

- **Distance-based selection** — выбор уровня детализации по расстоянию
- **Morphing transitions** — плавные переходы между уровнями
- **Error metrics** — метрики качества для автоматического LOD

**Нейросетевые подходы:**

- **Neural compression** — сжатие воксельных данных через автоэнкодеры
- **Real-time upscaling** — DLSS/FSR для воксельного рендеринга
- **Procedural generation** — генерация деталей через нейросети

---

## Современные фичи Vulkan 1.4 для воксельного рендеринга

### Mesh Shaders (`VK_EXT_mesh_shader`)

- **Task shaders** — coarse-grained work distribution
- **Mesh shaders** — fine-grained geometry generation
- **Advantages for voxels**:
  - Elimination of CPU-side mesh generation
  - Dynamic topology changes
  - Better utilization of GPU compute

### Dynamic Rendering (`VK_KHR_dynamic_rendering`)

- **No RenderPass/Framebuffer objects** — simplified API
- **Attachmentless rendering** — для compute-only pipelines
- **Benefits for voxel engines**:
  - Reduced driver overhead
  - Easier integration with compute pipelines
  - Better performance for complex rendering graphs

### Shader Object Layer (`VK_EXT_shader_object`)

- **Pipeline-less rendering** — динамическая компиляция шейдеров
- **Runtime specialization** — оптимизация под конкретные воксельные паттерны
- **Use cases**:
  - Procedural material shaders
  - Dynamic terrain generation
  - Real-time voxel editing

### Descriptor Buffer (`VK_EXT_descriptor_buffer`)

- **CPU-direct descriptor updates** — минуя descriptor sets
- **Bindless rendering optimization** — тысячи текстур без overhead
- **Voxel texture streaming** — эффективный streaming текстур

---

## Теория синхронизации для асинхронных воксельных конвейеров

**Multi-queue synchronization patterns:**

- **Producer-consumer** — compute → graphics через timeline semaphores
- **Parallel pipelines** — независимые compute pipelines для разных чанков
- **Resource ownership transfer** — atomic transfer между очередями

**Memory model для воксельных данных:**

- **Coherent vs non-coherent** — выбор модели памяти
- **Write-after-read hazards** — предотвращение конфликтов
- **Memory barriers optimization** — минимальные барьеры для воксельных операций

**Производительность через Tracy integration:**

- **GPU profiling** — трассировка compute/graphics операций
- **Memory allocation tracking** — мониторинг использования VMA
- **Pipeline bottlenecks identification** — поиск узких мест в конвейере

---

## Интеграция с экосистемой ProjectV

### Flecs ECS + Vulkan

**Компонентно-ориентированный дизайн:**

- **Vulkan resource components** — `VulkanBuffer`, `VulkanImage`, `VulkanPipeline`
- **Observer-based lifecycle** — автоматическое создание/уничтожение ресурсов
- **System ordering** — гарантия правильного порядка рендеринга

**Patterns:**

- **Entity-per-chunk** — каждый чанк как сущность с компонентами ресурсов
- **Material system** — компоненты материалов с descriptor sets
- **Render graph integration** — системы как узлы графа рендеринга

### Tracy Profiling + Vulkan

**GPU profiling integration:**

- **Vulkan debug markers** — аннотация командных буферов
- **Queue profiling** — измерение времени выполнения очередей
- **Memory profiling** — отслеживание выделений VMA

**Performance optimization workflow:**

1. **Identify bottlenecks** через Tracy GPU traces
2. **Optimize barriers** — минимизация stalls
3. **Balance workloads** — распределение между compute/graphics

### FastGLTF + Vulkan

**Efficient glTF loading for voxel assets:**

- **Direct GPU upload** — загрузка напрямую в Vulkan буферы
- **Texture atlas generation** — объединение текстур вокселей
- **LOD generation** — автоматическое создание уровней детализации

**Integration patterns:**

- **Async loading** — фоновый parsing glTF
- **Progressive streaming** — потоковая загрузка больших миров
- **Memory-mapped loading** — zero-copy загрузка через VMA

### JoltPhysics + Vulkan

**Physics-voxel integration:**

- **Heightfield shapes** — для статических воксельных ландшафтов
- **Dynamic mesh shapes** — для разрушаемых объектов
- **GPU-physics synchronization** — передача данных между Vulkan и Jolt

**Optimization techniques:**

- **Broad-phase optimization** — spatial hashing для вокселей
- **Sleeping bodies** — деактивация статических объектов
- **Contact caching** — повторное использование контактов

### Miniaudio + Vulkan

**Spatial audio for voxel worlds:**

- **3D audio positioning** — позиционирование звуков относительно вокселей
- **Occlusion/obstruction** — расчёт затухания через воксельную геометрию
- **Real-time DSP** — обработка звука параллельно с рендерингом

**Integration architecture:**

- **Separate audio thread** — изоляция audio processing
- **Async audio updates** — обновление позиций в фоновом режиме
- **GPU audio processing** — экспериментальные compute shaders для audio

---

## Оптимизация производительности

### GPU Driven Rendering для вокселей

Для рендеринга тысяч чанков используется **Multi-Draw Indirect**.

**Архитектура:**

1. **Culling Compute Shader**: Проверяет видимость чанков (Frustum Culling) и записывает команды отрисовки в
   `IndirectBuffer`.
2. **Render Pass**: Выполняет один `vkCmdDrawIndexedIndirect` с буфером команд.

```cpp
// Culling pass
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline);
vkCmdDispatch(cmd, chunkCount / 64, 1, 1);

// Barrier
// ...

// Render pass
vkCmdDrawIndexedIndirect(cmd, indirectBuffer, 0, maxChunkCount, sizeof(VkDrawIndexedIndirectCommand));
```

Это снижает CPU overhead до минимума.

### Async Compute для генерации геометрии

Генерация геометрии выполняется в отдельной очереди (`COMPUTE`), параллельно с рендерингом теней/GBuffer в `GRAPHICS`
очереди.

**Синхронизация:** Используются Timeline Semaphores.

```cpp
// Compute queue
vkQueueSubmit(computeQueue, ...); // Signal timeline value N

// Graphics queue
VkTimelineSemaphoreSubmitInfo timelineInfo = {};
timelineInfo.pWaitSemaphoreValues = &N;
vkQueueSubmit(graphicsQueue, ...); // Wait for value N
```

### Compute Shaders для генерации геометрии

Вместо передачи вершин с CPU, ProjectV использует Compute Shaders для генерации мешей чанков на лету.

**Пайплайн генерации:**

1. **Input**: Буфер с данными вокселей (ID, материалы).
2. **Compute Shader**:
  - Анализирует соседей (culling невидимых граней).
  - Генерирует вершины и индексы в выходные буферы.
3. **Output**: Vertex/Index буферы для рендеринга.

```cpp
// Диспатч генерации (группы по 8x8x8 вокселей)
uint32_t groups = chunk_size / 8;
vkCmdDispatch(cmd, groups, groups, groups);

// Барьер перед рендерингом
VkBufferMemoryBarrier barrier = {};
barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, ...);
```

---

## Типичные проблемы и решения

### Проблемы производительности при рендеринге миллионов вокселей

**Проблема:** Низкий FPS при отрисовке 1M+ вокселей, высокое потребление памяти, долгая загрузка чанков.

**Решение:**

1. **GPU-driven rendering:** Используйте `vkCmdDrawIndirect` для минимизации draw calls. Генерируйте indirect команды в
   compute shaders.
2. **Frustum culling на GPU:** Выполняйте отсечение невидимых чанков в compute shader перед отправкой на рендеринг.
3. **LOD (Level of Detail):** Автоматически генерируйте упрощённые меши для дальних вокселей.
4. **Batch rendering:** Объединяйте воксели с одинаковыми материалами в один draw call.
5. **Memory optimization:** Используйте sparse textures для миров >4GB.

### Ошибки compute shaders для генерации геометрии

**Проблема:** Compute shader не генерирует геометрию или создаёт артефакты.

**Решение:**

1. **Проверка work group size:** Убедитесь, что `gl_WorkGroupSize` соответствует диспатчу (`vkCmdDispatch`).
2. **Memory barriers:** Добавьте `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` барьеры между compute и graphics queue.
3. **Buffer bounds:** Проверьте, что indirect buffer имеет достаточный размер для максимального количества команд.
4. **Atomic operations:** При параллельной записи в буфер используйте `atomicAdd` для индексов.

### Bindless rendering ошибки

**Проблема:** Текстуры не отображаются или появляются артефакты при использовании descriptor indexing.

**Решение:**

1. **Включение расширений:** Убедитесь, что `VK_EXT_descriptor_indexing` и `VK_KHR_maintenance3` включены.
2. **Descriptor set limits:** Проверьте `maxDescriptorSetSamplers` и `maxPerStageDescriptorSamplers` через
   `vkGetPhysicalDeviceProperties2`.
3. **Texture array bounds:** В шейдере проверяйте индекс текстуры перед доступом: `if (textureIndex < textureCount)`.
4. **Sampler compatibility:** Используйте immutable samplers для consistency.

### Sparse memory allocation failures

**Проблема:** `vkQueueBindSparse` возвращает `VK_ERROR_OUT_OF_DEVICE_MEMORY` или `VK_ERROR_FEATURE_NOT_PRESENT`.

**Решение:**

1. **Проверка поддержки:** Вызовите `vkGetPhysicalDeviceSparseImageProperties` для проверки возможностей.
2. **Page size:** Используйте правильный размер страницы (обычно 64KB или 256KB).
3. **Memory type:** Убедитесь, что memory type поддерживает `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`.
4. **Gradual allocation:** Аллоцируйте sparse memory постепенно по мере необходимости.

### Async compute synchronization issues

**Проблема:** Compute и graphics очереди конфликтуют, приводя к артефактам или падению производительности.

**Решение:**

1. **Timeline semaphores:** Используйте `VK_SEMAPHORE_TYPE_TIMELINE` для точной синхронизации.
2. **Queue family индекс:** Убедитесь, что compute и graphics очереди принадлежат разным family (если поддерживается).
3. **Resource ownership transfer:** При передаче ресурсов между очередями используйте `VK_ACCESS_MEMORY_READ_BIT`/
   `WRITE_BIT` барьеры.
4. **Pipeline barriers:** Устанавливайте барьеры между compute dispatch и graphics draw.

### Оптимизация для воксельного рендеринга

| Проблема        | Решение                                 | Производительность |
|-----------------|-----------------------------------------|--------------------|
| High draw calls | GPU-driven rendering с indirect drawing | 1000x reduction    |
| Memory overhead | Sparse textures + compression           | 70% reduction      |
| Texture binding | Bindless descriptor arrays              | 10x faster         |
| LOD popping     | Геометрические мипмапы + dithering      | Smooth transitions |
| Loading stutter | Async streaming + prefetching           | No frame drops     |

---

## 🧭 Навигация

### Рекомендуемый порядок чтения

1. **[Основные понятия Vulkan](concepts.md)** — фундаментальные концепции Vulkan
2. **[Быстрый старт Vulkan](quickstart.md)** — практическое создание треугольника
3. **[Интеграция Vulkan](integration.md)** — настройка с SDL3, volk, VMA
4. **Этот документ (ProjectV Integration)** — специфичные для ProjectV подходы

### Связанные разделы

🔗 **[Производительность Vulkan](performance.md)** — общие оптимизации Vulkan
🔗 **[Решение проблем Vulkan](troubleshooting.md)** — отладка и типичные ошибки
🔗 **[Справочник API Vulkan](api-reference.md)** — полный список функций и структур
🔗 **[Глоссарий Vulkan](glossary.md)** — определения терминов

### Экосистема ProjectV

🔗 **[Flecs ECS](../flecs/README.md)** — компонентно-ориентированная архитектура
🔗 **[VMA](../vma/README.md)** — управление памятью GPU
🔗 **[Tracy](../tracy/README.md)** — профилирование производительности
🔗 **[JoltPhysics](../joltphysics/README.md)** — физический движок для вокселей
🔗 **[Miniaudio](../miniaudio/README.md)** — пространственный звук для воксельных миров

### Архитектурная документация ProjectV

🔗 **[Core Loop](../../architecture/core-loop.md)** — фундамент игрового цикла
🔗 **[Voxel Pipeline](../../architecture/voxel-pipeline.md)** — GPU-driven рендеринг вокселей
🔗 **[Modern Vulkan Guide](../../architecture/modern-vulkan-guide.md)** — Vulkan 1.4 для ProjectV
🔗 **[Flecs-Vulkan Bridge](../../architecture/flecs-vulkan-bridge.md)** — интеграция ECS с Vulkan
🔗 **[Jolt-Vulkan Bridge](../../architecture/jolt-vulkan-bridge.md)** — интеграция физики и рендеринга

---

← **[Назад к основной документации Vulkan](README.md)**
