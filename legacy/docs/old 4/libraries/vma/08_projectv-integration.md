# VMA в ProjectV: Интеграция

**🟡 Уровень 2: Средний**

CMake конфигурация и интеграция VMA с компонентами ProjectV.

---

## CMake конфигурация

### Подключение VMA

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

```cpp
// src/vma_init.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

---

## Создание аллокатора

### Инициализация с volk

```cpp
// После vkCreateDevice и volkLoadDevice
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
VkResult result = vmaCreateAllocator(&allocInfo, &allocator);
```

---

## Порядок уничтожения

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

## RAII обёртка

```cpp
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
```

---

## Обёртка для буфера

```cpp
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

    // Non-copyable, movable
    VmaBuffer(const VmaBuffer&) = delete;
    VmaBuffer& operator=(const VmaBuffer&) = delete;
    VmaBuffer(VmaBuffer&& other) noexcept;
    VmaBuffer& operator=(VmaBuffer&& other) noexcept;

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
```

---

## Интеграция с системами ProjectV

### Система рендеринга

```cpp
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
};
```

### Uniform buffer с triple buffering

```cpp
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

    void advanceFrame() {
        m_currentFrame = (m_currentFrame + 1) % FRAME_COUNT;
    }

    void flushCurrent() {
        m_frames[m_currentFrame].buffer.flush();
    }
};
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
#ifndef _DEBUG
// Минимизировать overhead
allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
#endif