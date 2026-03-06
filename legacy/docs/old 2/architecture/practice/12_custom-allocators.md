# Custom Memory Allocators [🔴 Уровень 3]

**🔴 Уровень 3: Продвинутый** — Кастомные аллокаторы для hot paths.

> ⚠️ **Предупреждение:** Стандартный `malloc` слишком медленный для воксельного мешинга. Используйте кастомные
> аллокаторы только после профилирования.

## Проблема

```cpp
// Стандартный malloc/new — слишком медленный для hot paths
void generateChunkMesh() {
    std::vector<Vertex> vertices;  // malloc под капотом
    // Для 16³ чанка: 4096 вокселей × 24 вершины = ~100 allocations
    // Каждая аллокация: ~100-500 нс
    // Итого: 10-50 мкс на чанк — это 10-50 мс на 1000 чанков!
}
```

## Решение: Кастомные аллокаторы

---

## 1. Linear Allocator (Bump Allocator)

Самый быстрый аллокатор — O(1) для allocation, O(1) для reset.

```cpp
// src/memory/linear_allocator.hpp
#pragma once

#include <cstdint>
#include <cstddef>

namespace ProjectV::Memory {

class LinearAllocator {
public:
    explicit LinearAllocator(size_t capacity) {
        buffer_ = static_cast<uint8_t*>(std::malloc(capacity));
        capacity_ = capacity;
        offset_ = 0;
    }

    ~LinearAllocator() {
        std::free(buffer_);
    }

    // O(1) allocation — просто сдвиг указателя
    void* allocate(size_t size, size_t alignment = 16) {
        // Выравнивание
        size_t alignedOffset = (offset_ + alignment - 1) & ~(alignment - 1);

        if (alignedOffset + size > capacity_) {
            return nullptr;  // Out of memory
        }

        void* ptr = buffer_ + alignedOffset;
        offset_ = alignedOffset + size;

        return ptr;
    }

    // O(1) reset — невозможно освободить отдельные блоки!
    void reset() {
        offset_ = 0;
    }

    // Статистика
    size_t used() const { return offset_; }
    size_t capacity() const { return capacity_; }
    size_t available() const { return capacity_ - offset_; }

private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t offset_;

    // Non-copyable
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
};

} // namespace ProjectV::Memory
```

### Использование для мешинга

```cpp
// Мешинг чанка с linear allocator
class ChunkMeshGenerator {
    LinearAllocator allocator_;  // 1 MB на кадр

public:
    ChunkMeshGenerator() : allocator_(1024 * 1024) {}

    Mesh generateMesh(const VoxelChunk& chunk) {
        // Сброс аллокатора в начале кадра
        allocator_.reset();

        // Все данные в одной непрерывной памяти
        Vertex* vertices = allocator_.allocate<Vertex>(MAX_VERTICES);
        uint32_t* indices = allocator_.allocate<uint32_t>(MAX_INDICES);

        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;

        // Генерация меша
        for (uint32_t i = 0; i < chunk.voxelCount; i++) {
            if (chunk.voxels[i] != 0) {
                // Добавление вершин
                addVoxelFaces(vertices, indices, vertexCount, indexCount, i);
            }
        }

        // Копирование в GPU buffer
        Mesh mesh;
        mesh.vertexCount = vertexCount;
        mesh.indexCount = indexCount;
        mesh.vertices = copyToGPU(vertices, vertexCount);
        mesh.indices = copyToGPU(indices, indexCount);

        // Память освободится при reset() в следующем кадре
        return mesh;
    }
};
```

---

## 2. Pool Allocator

Для объектов одинакового размера — O(1) allocation и deallocation.

```cpp
// src/memory/pool_allocator.hpp
#pragma once

#include <cstdint>
#include <cstddef>

namespace ProjectV::Memory {

class PoolAllocator {
public:
    PoolAllocator(size_t objectSize, size_t capacity)
        : objectSize_(objectSize), capacity_(capacity) {

        // Выравнивание размера объекта под указатель
        objectSize_ = (objectSize + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

        buffer_ = static_cast<uint8_t*>(std::malloc(objectSize_ * capacity));

        // Инициализация free list
        freeList_ = reinterpret_cast<void**>(buffer_);
        for (size_t i = 0; i < capacity - 1; i++) {
            void** current = reinterpret_cast<void**>(buffer_ + i * objectSize_);
            void** next = reinterpret_cast<void**>(buffer_ + (i + 1) * objectSize_);
            *current = next;
        }

        // Последний элемент
        void** last = reinterpret_cast<void**>(buffer_ + (capacity - 1) * objectSize_);
        *last = nullptr;

        allocatedCount_ = 0;
    }

    ~PoolAllocator() {
        std::free(buffer_);
    }

    // O(1) allocation
    void* allocate() {
        if (!freeList_) return nullptr;  // Pool exhausted

        void* ptr = freeList_;
        freeList_ = *freeList_;
        allocatedCount_++;

        return ptr;
    }

    // O(1) deallocation
    void deallocate(void* ptr) {
        if (!ptr) return;

        void** node = reinterpret_cast<void**>(ptr);
        *node = freeList_;
        freeList_ = node;
        allocatedCount_--;
    }

    // Статистика
    size_t allocated() const { return allocatedCount_; }
    size_t capacity() const { return capacity_; }
    size_t available() const { return capacity_ - allocatedCount_; }

private:
    uint8_t* buffer_;
    size_t objectSize_;
    size_t capacity_;
    size_t allocatedCount_;
    void** freeList_;
};

} // namespace ProjectV::Memory
```

### Использование для чанков

```cpp
// Пул для воксельных чанков
struct VoxelChunk {
    uint16_t voxels[4096];  // 16³ = 4096 вокселей × 2 байта = 8 KB
    // ... другие поля
};

class ChunkPool {
    PoolAllocator pool_;

public:
    ChunkPool(size_t maxChunks) : pool_(sizeof(VoxelChunk), maxChunks) {}

    VoxelChunk* createChunk() {
        void* memory = pool_.allocate();
        if (!memory) return nullptr;

        VoxelChunk* chunk = new (memory) VoxelChunk();  // Placement new
        return chunk;
    }

    void destroyChunk(VoxelChunk* chunk) {
        chunk->~VoxelChunk();  // Явный деструктор
        pool_.deallocate(chunk);
    }
};
```

---

## 3. Stack Allocator

Для вложенных областей видимости с поддержкой push/pop.

```cpp
// src/memory/stack_allocator.hpp
#pragma once

#include <cstdint>
#include <cstddef>

namespace ProjectV::Memory {

class StackAllocator {
public:
    struct Marker {
        size_t offset;
    };

    explicit StackAllocator(size_t capacity) {
        buffer_ = static_cast<uint8_t*>(std::malloc(capacity));
        capacity_ = capacity;
        offset_ = 0;
    }

    ~StackAllocator() {
        std::free(buffer_);
    }

    // Сохранение текущей позиции
    Marker getMarker() const {
        return Marker{offset_};
    }

    // O(1) allocation
    void* allocate(size_t size, size_t alignment = 16) {
        size_t alignedOffset = (offset_ + alignment - 1) & ~(alignment - 1);

        if (alignedOffset + size > capacity_) {
            return nullptr;
        }

        void* ptr = buffer_ + alignedOffset;
        offset_ = alignedOffset + size;

        return ptr;
    }

    // O(1) deallocation до маркера
    void deallocateToMarker(Marker marker) {
        offset_ = marker.offset;
    }

    void reset() {
        offset_ = 0;
    }

private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t offset_;
};

} // namespace ProjectV::Memory
```

### Использование для вложенных операций

```cpp
// Вложенные операции со стековым аллокатором
class FrameAllocator {
    StackAllocator allocator_;

public:
    FrameAllocator(size_t capacity) : allocator_(capacity) {}

    void processFrame() {
        auto frameMarker = allocator_.getMarker();

        // Уровень 1: Обработка чанков
        for (auto& chunk : visibleChunks) {
            auto chunkMarker = allocator_.getMarker();

            // Уровень 2: Мешинг чанка
            Vertex* vertices = allocator_.allocate<Vertex>(maxVertices);
            uint32_t* indices = allocator_.allocate<uint32_t>(maxIndices);

            generateMesh(chunk, vertices, indices);
            uploadToGPU(vertices, indices);

            // Освобождение памяти уровня 2
            allocator_.deallocateToMarker(chunkMarker);
        }

        // Освобождение всей памяти кадра
        allocator_.deallocateToMarker(frameMarker);
    }
};
```

---

## 4. Double-Buffered Allocator

Для параллельной работы CPU и GPU.

```cpp
// src/memory/double_buffer_allocator.hpp
#pragma once

#include "linear_allocator.hpp"

namespace ProjectV::Memory {

class DoubleBufferAllocator {
public:
    DoubleBufferAllocator(size_t capacityPerBuffer)
        : buffers_{LinearAllocator(capacityPerBuffer),
                   LinearAllocator(capacityPerBuffer)}
        , currentIndex_(0) {}

    void* allocate(size_t size, size_t alignment = 16) {
        return buffers_[currentIndex_].allocate(size, alignment);
    }

    // Переключение буферов (в конце кадра)
    void swap() {
        buffers_[1 - currentIndex_].reset();  // Сброс старого буфера
        currentIndex_ = 1 - currentIndex_;
    }

    LinearAllocator& current() { return buffers_[currentIndex_]; }
    LinearAllocator& previous() { return buffers_[1 - currentIndex_]; }

private:
    LinearAllocator buffers_[2];
    size_t currentIndex_;
};

} // namespace ProjectV::Memory
```

### Использование для GPU данных

```cpp
// CPU пишет в current buffer, GPU читает из previous
class GPUUploadManager {
    DoubleBufferAllocator allocator_;
    VkBuffer stagingBuffers_[2];

public:
    GPUUploadManager(size_t capacity) : allocator_(capacity) {}

    void* stageData(size_t size) {
        return allocator_.allocate(size);
    }

    void endFrame(VkCommandBuffer cmd) {
        // GPU копирует из previous buffer
        VkBuffer prevBuffer = stagingBuffers_[1 - currentIndex_];
        vkCmdCopyBuffer(cmd, prevBuffer, gpuBuffer, ...);

        // Переключение
        allocator_.swap();
    }
};
```

---

## Сравнение производительности

| Аллокатор         | Allocation  | Deallocation | Use Case                 |
|-------------------|-------------|--------------|--------------------------|
| **malloc**        | ~100-500 ns | ~100-500 ns  | General purpose          |
| **Linear**        | ~1-5 ns     | O(1) reset   | Мешинг, временные данные |
| **Pool**          | ~1-5 ns     | ~1-5 ns      | Объекты одного размера   |
| **Stack**         | ~1-5 ns     | O(1) marker  | Вложенные операции       |
| **Double-Buffer** | ~1-5 ns     | O(1) swap    | GPU staging              |

---

## Best Practices

1. **Linear Allocator** для:

- Генерация мешей чанков
- Временные массивы данных
- Стринг formatting

1. **Pool Allocator** для:

- VoxelChunk объекты
- Entity компоненты в ECS
- Event объекты

1. **Stack Allocator** для:

- Вложенные операции
- Рекурсивные алгоритмы
- Scoped временные данные

1. **Double-Buffer** для:

- GPU staging buffers
- Асинхронные данные

