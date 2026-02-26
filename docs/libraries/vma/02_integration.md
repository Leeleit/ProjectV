# VMA в ProjectV: Интеграция

> **Для понимания:** Интеграция VMA — это как подключение центрального отопления в дом. Сначала нужно правильно
> установить котел (аллокатор), развести трубы (пулы), подключить радиаторы (буферы), и только потом можно наслаждаться
> теплом. Ошибка на любом этапе = холод зимой.

**VMA — это не просто библиотека, а архитектурный выбор.** В ProjectV он становится центральным узлом управления
памятью, связывающим Vulkan, ECS и системы рендеринга.

---

## Как собрать

### CMake конфигурация

ProjectV использует Git submodules для внешних зависимостей:

```cmake
# CMakeLists.txt
find_package(Vulkan REQUIRED)

add_subdirectory(external/volk)
add_subdirectory(external/VMA)

add_executable(ProjectV
  src/main.cpp
  src/vma_init.cpp
)

target_link_libraries(ProjectV PRIVATE
  volk
  GPUOpen::VulkanMemoryAllocator
)

target_compile_definitions(ProjectV PRIVATE
  VK_NO_PROTOTYPES
)
```

### VMA_IMPLEMENTATION

VMA распространяется как header-only библиотека. Реализация подключается через макрос `VMA_IMPLEMENTATION` в **одном**
.cpp файле:

```cpp
// src/vma_init.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

**Важно:**

- Макрос должен быть определён только в одном .cpp файле
- Файл должен компилироваться как C++ (не C)
- В остальных файлах только `#include "vma/vk_mem_alloc.h"` без макроса

---

## Как инициализировать в движке

### Создание аллокатора с volk (C++26 версия)

После инициализации Vulkan и создания device:

```cpp
#include <print>
#include <expected>
#include "vma/result.hpp"  // Наш VmaResult из 01_reference.md

VmaResult<VmaAllocator> create_vma_allocator(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    bool hasMemoryBudgetExtension,
    bool hasBufferDeviceAddress)
{
    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physicalDevice;
    allocInfo.device = device;
    allocInfo.instance = instance;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    // Vulkan 1.1+ включает KHR_dedicated_allocation и KHR_bind_memory2
    allocInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                      VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

    // Интеграция с volk
    VmaVulkanFunctions vulkanFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
    allocInfo.pVulkanFunctions = &vulkanFunctions;

    // Бюджет памяти (требует VK_EXT_memory_budget)
    if (hasMemoryBudgetExtension) {
        allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }

    // Buffer device address (требует VK_KHR_buffer_device_address)
    if (hasBufferDeviceAddress) {
        allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocInfo, &allocator) != VK_SUCCESS) {
        std::println(stderr, "Failed to create VMA allocator");
        return std::unexpected(VmaError::AllocatorCreationFailed);
    }

    std::println("VMA allocator created with flags: {:#x}", allocInfo.flags);
    return allocator;
}
```

### Порядок уничтожения

VMA не предоставляет RAII. Уничтожать объекты нужно в правильном порядке:

```cpp
void cleanup() {
    // 1. Освободить все буферы и изображения
    for (auto& chunk : chunks) {
        vmaDestroyBuffer(allocator, chunk.buffer, chunk.allocation);
    }

    // 2. Уничтожить пулы
    vmaDestroyPool(allocator, chunkPool);

    // 3. Уничтожить аллокатор
    vmaDestroyAllocator(allocator);

    // 4. Уничтожить device
    vkDestroyDevice(device, nullptr);
}
```

---

## Наши ECS компоненты

### Компоненты для GPU ресурсов

```cpp
// src/ecs/components/vma_components.hpp
#pragma once

#include <flecs.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace projectv::ecs {

// Компонент для буферов
struct VmaBufferComponent {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
};

// Компонент для изображений
struct VmaImageComponent {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D extent = {};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
};

// Тег для ресурсов, требующих загрузки на GPU
struct NeedsGpuUpload {};

// Тег для ресурсов, готовых к использованию
struct GpuReady {};

} // namespace projectv::ecs
```

### Система очистки ресурсов

```cpp
// src/ecs/systems/vma_cleanup_system.cpp
#include "ecs/components/vma_components.hpp"
#include <vk_mem_alloc.h>

namespace projectv::ecs {

void registerVmaCleanupSystem(flecs::world& world, VmaAllocator allocator) {
    // Observer для буферов
    world.observer<VmaBufferComponent>()
        .event(flecs::OnRemove)
        .iter([allocator](flecs::iter& it, VmaBufferComponent* buffers) {
            for (auto i : it) {
                if (buffers[i].buffer != VK_NULL_HANDLE) {
                    vmaDestroyBuffer(allocator, buffers[i].buffer, buffers[i].allocation);
                }
            }
        });

    // Observer для изображений
    world.observer<VmaImageComponent>()
        .event(flecs::OnRemove)
        .iter([allocator](flecs::iter& it, VmaImageComponent* images) {
            for (auto i : it) {
                if (images[i].image != VK_NULL_HANDLE) {
                    vmaDestroyImage(allocator, images[i].image, images[i].allocation);
                }
            }
        });
}

} // namespace projectv::ecs
```

### Система загрузки чанков

```cpp
// src/ecs/systems/chunk_upload_system.cpp
#include "ecs/components/vma_components.hpp"
#include "ecs/components/chunk_components.hpp"
#include <vk_mem_alloc.h>

namespace projectv::ecs {

class ChunkUploadSystem {
    VmaAllocator m_allocator;
    VkDeviceSize m_chunkSize;

public:
    ChunkUploadSystem(flecs::world& world, VmaAllocator allocator, VkDeviceSize chunkSize)
        : m_allocator(allocator), m_chunkSize(chunkSize)
    {
        world.system<VmaBufferComponent, const NeedsGpuUpload>("ChunkUploadSystem")
            .iter([this](flecs::iter& it, VmaBufferComponent* buffers, const NeedsGpuUpload*) {
                for (auto i : it) {
                    if (buffers[i].buffer == VK_NULL_HANDLE) {
                        buffers[i] = createChunkBuffer();
                        it.entity(i)
                            .remove<NeedsGpuUpload>()
                            .add<GpuReady>();
                    }
                }
            });
    }

private:
    VmaBufferComponent createChunkBuffer() {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_chunkSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

        VmaBufferComponent result;
        result.size = m_chunkSize;
        result.usage = bufferInfo.usage;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, nullptr);

        return result;
    }
};

} // namespace projectv::ecs
```

---

## RAII обёртки

### Обёртка для аллокатора

```cpp
// src/vulkan/vma_allocator_wrapper.hpp
#pragma once

#include <vk_mem_alloc.h>
#include <utility>

namespace projectv::vulkan {

class VmaAllocatorWrapper {
public:
    VmaAllocatorWrapper() = default;

    VmaAllocatorWrapper(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
        VmaAllocatorCreateInfo createInfo = {};
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.instance = instance;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        createInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                          VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

        VmaVulkanFunctions vulkanFunctions = {};
        vmaImportVulkanFunctionsFromVolk(&createInfo, &vulkanFunctions);
        createInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&createInfo, &m_allocator);
    }

    ~VmaAllocatorWrapper() {
        if (m_allocator) {
            vmaDestroyAllocator(m_allocator);
        }
    }

    // Non-copyable
    VmaAllocatorWrapper(const VmaAllocatorWrapper&) = delete;
    VmaAllocatorWrapper& operator=(const VmaAllocatorWrapper&) = delete;

    // Movable
    VmaAllocatorWrapper(VmaAllocatorWrapper&& other) noexcept
        : m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE)) {}

    VmaAllocatorWrapper& operator=(VmaAllocatorWrapper&& other) noexcept {
        if (this != &other) {
            if (m_allocator) vmaDestroyAllocator(m_allocator);
            m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        }
        return *this;
    }

    VmaAllocator get() const { return m_allocator; }
    operator VmaAllocator() const { return m_allocator; }

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};

} // namespace projectv::vulkan
```

### Обёртка для буфера

```cpp
// src/vulkan/vma_buffer.hpp
#pragma once

#include <vk_mem_alloc.h>
#include <cstddef>
#include <utility>

namespace projectv::vulkan {

class VmaBuffer {
public:
    VmaBuffer() = default;

    VmaBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
              VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0)
        : m_allocator(allocator), m_size(size)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = flags;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &m_buffer, &m_allocation, &m_allocationInfo);
    }

    ~VmaBuffer() {
        if (m_buffer) {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    // Non-copyable
    VmaBuffer(const VmaBuffer&) = delete;
    VmaBuffer& operator=(const VmaBuffer&) = delete;

    // Movable
    VmaBuffer(VmaBuffer&& other) noexcept
        : m_allocator(other.m_allocator),
          m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
          m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE)),
          m_allocationInfo(other.m_allocationInfo),
          m_size(other.m_size) {}

    VmaBuffer& operator=(VmaBuffer&& other) noexcept {
        if (this != &other) {
            if (m_buffer) {
                vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
            }
            m_allocator = other.m_allocator;
            m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
            m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
            m_allocationInfo = other.m_allocationInfo;
            m_size = other.m_size;
        }
        return *this;
    }

    VkBuffer get() const { return m_buffer; }
    VkDeviceSize size() const { return m_size; }
    void* mappedData() const { return m_allocationInfo.pMappedData; }

    void map(void** ppData) {
        vmaMapMemory(m_allocator, m_allocation, ppData);
    }

    void unmap() {
        vmaUnmapMemory(m_allocator, m_allocation);
    }

    void flush() {
        vmaFlushAllocation(m_allocator, m_allocation, 0, VK_WHOLE_SIZE);
    }

    void invalidate() {
        vmaInvalidateAllocation(m_allocator, m_allocation, 0, VK_WHOLE_SIZE);
    }

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocationInfo = {};
    VkDeviceSize m_size = 0;
};

} // namespace projectv::vulkan
```

---

## Интеграция с системами рендеринга

### Система рендеринга

```cpp
// src/rendering/render_system.cpp
#include "vulkan/vma_buffer.hpp"
#include "vulkan/vma_allocator_wrapper.hpp"

namespace projectv::rendering {

class RenderSystem {
    VmaAllocator m_allocator;
    VmaBuffer m_vertexBuffer;
    VmaBuffer m_indexBuffer;

public:
    void init(VmaAllocator allocator) {
        m_allocator = allocator;

        // Создание vertex buffer
        m_vertexBuffer = VmaBuffer(allocator, vertexDataSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO);

        // Создание index buffer
        m_indexBuffer = VmaBuffer(allocator, indexDataSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO);
    }

    void render(VkCommandBuffer cmd) {
        VkBuffer vertexBuffers[] = {m_vertexBuffer.get()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }
};

} // namespace projectv::rendering
```

### Uniform buffer с triple buffering

```cpp
// src/rendering/uniform_buffer_system.hpp
#pragma once

#include "vulkan/vma_buffer.hpp"
#include <array>

namespace projectv::rendering {

class UniformBufferSystem {
    static constexpr size_t FRAME_COUNT = 3;

    struct FrameData {
        VmaBuffer buffer;
        size_t frameIndex;
    };

    std::array<FrameData, FRAME_COUNT> m_frames;
    size_t m_currentFrame = 0;

public:
    void init(VmaAllocator allocator, VkDeviceSize size) {
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            m_frames[i].buffer = VmaBuffer(allocator, size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
            m_frames[i].frameIndex = i;
        }
    }

    void* getCurrentMappedData() {
        return m_frames[m_currentFrame].buffer.mappedData();
    }

    VkBuffer getCurrentBuffer() const {
        return m_frames[m_currentFrame].buffer.get();
    }

    void updateCurrent(const void* data, size_t size) {
        void* mapped = getCurrentMappedData();
        memcpy(mapped, data, size);
        m_frames[m_currentFrame].buffer.flush();
    }

    void advanceFrame() {
        m_currentFrame = (m_currentFrame + 1) % FRAME_COUNT;
    }
};

} // namespace projectv::rendering
```

---

## Конфигурация для разных сборок

### Debug

```cpp
#ifdef _DEBUG
// Включить дополнительные проверки
allocInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

// Debug annotations для аллокаций
VmaAllocationCreateInfo allocInfo = {};
allocInfo.pUserData = (void*)"VertexBufferData";
#endif
```

### Release

```cpp
// Release: минимум проверок, максимум производительности
allocInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                  VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
// Без VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT если не нужен бюджет
```

## Злые хаки интеграции

### Хак #1: Принудительное выравнивание для SoA данных

```cpp
// Вместо AoS (Array of Structures):
struct VertexAoS {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
}; // 32 байта, плохая локализация

// Используем SoA (Structure of Arrays) с alignas:
struct alignas(64) VertexSoA {
    // Позиции (выровнены для SIMD)
    alignas(16) float positions[16384][3];

    // Нормали (отдельный массив)
    alignas(16) float normals[16384][3];

    // UV координаты
    alignas(8) float uvs[16384][2];

    // Размер: 16384 вершин * (12 + 12 + 8) = 524KB
    // Выровнено по 64 байта = идеально для кэша
};

// Создаём буфер с VMA:
VmaResult<VmaBuffer> create_vertex_buffer(VmaAllocator allocator, size_t vertexCount) {
    VkDeviceSize size = sizeof(VertexSoA);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    VkBuffer buffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &buffer, &allocation, nullptr) != VK_SUCCESS) {
        return std::unexpected(VmaError::BufferCreationFailed);
    }

    std::println("Created SoA vertex buffer: {} vertices, {} KB aligned to 64 bytes",
                 vertexCount, size / 1024);

    return VmaBuffer(allocator, buffer, allocation, size);
}
```

### Хак #2: Пул для воксельных чанков с предварительным выделением

```cpp
class VoxelChunkPool {
    VmaAllocator m_allocator;
    VmaPool m_pool;
    std::vector<VmaBuffer> m_preallocated;

public:
    VmaResult<void> initialize(VmaAllocator allocator, size_t chunkCount, VkDeviceSize chunkSize) {
        m_allocator = allocator;

        // Находим тип памяти для storage buffers
        VkBufferCreateInfo sampleInfo = {};
        sampleInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sampleInfo.size = chunkSize;
        sampleInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VmaAllocationCreateInfo sampleAlloc = {};
        sampleAlloc.usage = VMA_MEMORY_USAGE_AUTO;

        uint32_t memoryTypeIndex;
        vmaFindMemoryTypeIndexForBufferInfo(allocator, &sampleInfo, &sampleAlloc, &memoryTypeIndex);

        // Создаём пул
        VmaPoolCreateInfo poolInfo = {};
        poolInfo.memoryTypeIndex = memoryTypeIndex;
        poolInfo.blockSize = 64 * 1024 * 1024;  // 64MB блоки
        poolInfo.minBlockCount = 1;
        poolInfo.maxBlockCount = 16;

        if (vmaCreatePool(allocator, &poolInfo, &m_pool) != VK_SUCCESS) {
            return std::unexpected(VmaError::AllocatorCreationFailed);
        }

        // Предварительно аллоцируем чанки
        m_preallocated.reserve(chunkCount);
        for (size_t i = 0; i < chunkCount; ++i) {
            if (auto buffer = create_chunk(chunkSize); buffer) {
                m_preallocated.push_back(std::move(*buffer));
            } else {
                std::println(stderr, "Failed to preallocate chunk {}", i);
                return std::unexpected(buffer.error());
            }
        }

        std::println("Preallocated {} voxel chunks ({} MB each) in pool",
                     chunkCount, chunkSize / (1024.0 * 1024.0));
        return {};
    }

private:
    VmaResult<VmaBuffer> create_chunk(VkDeviceSize size) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.pool = m_pool;
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

        VkBuffer buffer;
        VmaAllocation allocation;
        if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                           &buffer, &allocation, nullptr) != VK_SUCCESS) {
            return std::unexpected(VmaError::BufferCreationFailed);
        }

        return VmaBuffer(m_allocator, buffer, allocation, size);
    }
};
```

### Хак #3: Tracy + VMA = профилирование в реальном времени

```cpp
#ifdef TRACY_ENABLE
#include "Tracy.hpp"

class TrackedVmaBuffer : public VmaBuffer {
    const char* m_name;

public:
    TrackedVmaBuffer(VmaAllocator allocator, VkDeviceSize size,
                     VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                     const char* name, VmaAllocationCreateFlags flags = 0)
        : VmaBuffer(allocator, size, usage, memoryUsage, flags)
        , m_name(name)
    {
        TracyAllocN((void*)this, size, m_name);
    }

    ~TrackedVmaBuffer() {
        TracyFreeN((void*)this, m_name);
    }
};

// Использование:
auto buffer = TrackedVmaBuffer(allocator, 1024 * 1024,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VMA_MEMORY_USAGE_AUTO,
    "VertexBuffer_Chunk42");
// Tracy покажет аллокацию с именем "VertexBuffer_Chunk42"
#endif
```

## Итог: философия интеграции VMA в ProjectV

1. **RAII везде** — никаких голых указателей, только умные обёртки
2. **std::expected вместо исключений** — предсказуемая обработка ошибок
3. **SoA вместо AoS** — данные выровнены для кэша и SIMD
4. **Пулы для одинаковых объектов** — уменьшение фрагментации
5. **Интеграция с ECS** — ресурсы как компоненты, очистка автоматическая
6. **Профилирование с Tracy** — видимость использования памяти

> **Почему именно так?** Потому что в воксельном движке управление памятью — это 80% производительности. Неправильная
> аллокация = просадка FPS при загрузке чанков. VMA + наши обёртки = предсказуемость и контроль.

---

## Следующий шаг

После интеграции VMA переходим к [продвинутым паттернам](03_advanced.md):

- Double buffering для compute shaders
- Кольцевые staging буферы
- Загрузка текстур через пулы
- Интеграция с Tracy для мониторинга
