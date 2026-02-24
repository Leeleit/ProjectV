# VMA в ProjectV: Примеры

**🔴 Уровень 3: Продвинутый**

Полные примеры кода для типичных задач ProjectV.

---

## Пример 1: Инициализация VMA с SDL3 + volk

```cpp
// vma_example_init.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>

int main() {
    // 1. SDL init
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL init failed\n");
        return 1;
    }

    // 2. Volk init
    if (volkInitialize() != VK_SUCCESS) {
        printf("Volk init failed\n");
        return 1;
    }

    // 3. Create instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    // Extensions for SDL
    uint32_t extensionCount;
    char const* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions;

    VkInstance instance;
    vkCreateInstance(&instanceInfo, nullptr, &instance);
    volkLoadInstance(instance);

    // 4. Create window + surface
    SDL_Window* window = SDL_CreateWindow("VMA Example", 800, 600, SDL_WINDOW_VULKAN);
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // 5. Select physical device
    uint32_t gpuCount;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
    VkPhysicalDevice physicalDevice = gpus[0];

    // 6. Create device
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = 0; // Simplified
    queueInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueInfo.pQueuePriorities = &queuePriority;

    // Extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device;
    vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    volkLoadDevice(device);

    // 7. Create VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                         VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

    VmaVulkanFunctions vulkanFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions);
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    VmaAllocator allocator;
    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);

    if (result == VK_SUCCESS) {
        printf("VMA allocator created successfully!\n");

        // Print memory info
        VmaTotalStatistics stats;
        vmaCalculateStatistics(allocator, &stats);
        printf("Total allocations: %zu\n", stats.total.statistics.allocationCount);
    }

    // Cleanup
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

---

## Пример 2: Vertex + Index buffers

```cpp
// vma_example_vertex_buffer.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

struct Vertex {
    float x, y, z;
    float u, v;
};

void createMeshBuffers(VmaAllocator allocator,
                       const std::vector<Vertex>& vertices,
                       const std::vector<uint32_t>& indices,
                       VkBuffer& outVertexBuffer,
                       VmaAllocation& outVertexAllocation,
                       VkBuffer& outIndexBuffer,
                       VmaAllocation& outIndexAllocation)
{
    // Vertex buffer
    VkDeviceSize vertexSize = vertices.size() * sizeof(Vertex);

    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vertexSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vertexAllocInfo = {};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(allocator, &vertexBufferInfo, &vertexAllocInfo,
                   &outVertexBuffer, &outVertexAllocation, nullptr);

    // Index buffer
    VkDeviceSize indexSize = indices.size() * sizeof(uint32_t);

    VkBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indexSize;
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo indexAllocInfo = {};
    indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(allocator, &indexBufferInfo, &indexAllocInfo,
                   &outIndexBuffer, &outIndexAllocation, nullptr);

    // Upload via staging (simplified)
    uploadViaStaging(allocator, outVertexBuffer, vertices.data(), vertexSize);
    uploadViaStaging(allocator, outIndexBuffer, indices.data(), indexSize);
}

void uploadViaStaging(VmaAllocator allocator, VkBuffer dstBuffer,
                      const void* data, VkDeviceSize size)
{
    // Create staging buffer
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingAllocResult;
    vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo,
                   &stagingBuffer, &stagingAllocation, &stagingAllocResult);

    // Copy data
    memcpy(stagingAllocResult.pMappedData, data, size);
    vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);

    // Record copy command (requires command buffer)
    // vkCmdCopyBuffer(cmd, stagingBuffer, dstBuffer, 1, &copyRegion);

    // Submit and wait (simplified)
    // ...

    // Cleanup staging
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}
```

---

## Пример 3: Uniform buffer с persistent mapping

```cpp
// vma_example_uniform_buffer.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

struct UniformData {
    float viewMatrix[16];
    float projMatrix[16];
    float time;
};

class UniformBufferExample {
    VmaAllocator m_allocator;
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
    void* m_mappedData;
    VkDeviceSize m_size;

public:
    void init(VmaAllocator allocator, VkDeviceSize size) {
        m_allocator = allocator;
        m_size = size;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &m_buffer, &m_allocation, &allocationInfo);

        m_mappedData = allocationInfo.pMappedData;
    }

    void update(const UniformData& data) {
        memcpy(m_mappedData, &data, sizeof(data));
        vmaFlushAllocation(m_allocator, m_allocation, 0, VK_WHOLE_SIZE);
    }

    VkBuffer getBuffer() const { return m_buffer; }

    void cleanup() {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
};
```

---

## Пример 4: Texture loading

```cpp
// vma_example_texture.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <vector>

struct Texture {
    VkImage image;
    VmaAllocation allocation;
    VkImageView view;
    VkExtent2D extent;
};

Texture createTexture(VmaAllocator allocator, VkDevice device,
                      const std::vector<uint8_t>& pixels,
                      uint32_t width, uint32_t height,
                      VkCommandBuffer cmdBuffer)
{
    Texture texture;
    texture.extent = {width, height};

    VkDeviceSize imageSize = width * height * 4;  // RGBA8

    // Staging buffer
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = imageSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingResult;
    vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo,
                   &stagingBuffer, &stagingAllocation, &stagingResult);

    memcpy(stagingResult.pMappedData, pixels.data(), imageSize);
    vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);

    // Create image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo imageAllocInfo = {};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateImage(allocator, &imageInfo, &imageAllocInfo,
                  &texture.image, &texture.allocation, nullptr);

    // Transition to transfer dst
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = texture.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, texture.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCreateImageView(device, &viewInfo, nullptr, &texture.view);

    // Cleanup staging
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

    return texture;
}

void destroyTexture(VmaAllocator allocator, VkDevice device, Texture& texture) {
    vkDestroyImageView(device, texture.view, nullptr);
    vmaDestroyImage(allocator, texture.image, texture.allocation);
    texture = {};
}
```

---

## Пример 5: VMA statistics monitoring

```cpp
// vma_example_stats.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <cstdio>

void printMemoryStats(VmaAllocator allocator) {
    VmaTotalStatistics stats;
    vmaCalculateStatistics(allocator, &stats);

    printf("=== VMA Statistics ===\n");
    printf("Allocations: %zu\n", stats.total.statistics.allocationCount);
    printf("Allocated: %.2f MB\n",
           double(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0));
    printf("Unused: %.2f MB\n",
           double(stats.total.statistics.unusedBytes) / (1024.0 * 1024.0));
    printf("Blocks: %zu\n", stats.total.statistics.blockCount);

    // Per-heap budget
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator, budgets);

    printf("\n=== Memory Budget ===\n");
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budgets[i].budget > 0) {
            printf("Heap %u:\n", i);
            printf("  Usage: %.2f MB / %.2f MB\n",
                   double(budgets[i].usage) / (1024.0 * 1024.0),
                   double(budgets[i].budget) / (1024.0 * 1024.0));
            printf("  Blocks: %zu, Allocations: %zu\n",
                   budgets[i].statistics.blockCount,
                   budgets[i].statistics.allocationCount);
        }
    }
}

void checkMemoryHealth(VmaAllocator allocator) {
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator, budgets);

    bool warning = false;
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budgets[i].budget > 0) {
            float usagePercent = float(budgets[i].usage) / float(budgets[i].budget) * 100.0f;
            if (usagePercent > 90.0f) {
                printf("WARNING: Heap %u at %.1f%% capacity!\n", i, usagePercent);
                warning = true;
            }
        }
    }

    if (!warning) {
        printf("Memory usage is healthy.\n");
    }
}
```

---

## Пример 6: Custom pool для чанков

```cpp
// vma_example_chunk_pool.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <vector>
#include <unordered_map>

class VoxelChunkPool {
    VmaAllocator m_allocator;
    VmaPool m_pool;
    VkDeviceSize m_chunkSize;
    std::unordered_map<uint64_t, std::pair<VkBuffer, VmaAllocation>> m_chunks;

public:
    VoxelChunkPool(VmaAllocator allocator, VkDeviceSize chunkSize, size_t maxMemoryMB)
        : m_allocator(allocator), m_chunkSize(chunkSize)
    {
        // Find memory type for storage buffer
        VkBufferCreateInfo sampleInfo = {};
        sampleInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sampleInfo.size = chunkSize;
        sampleInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo sampleAlloc = {};
        sampleAlloc.usage = VMA_MEMORY_USAGE_AUTO;

        uint32_t memoryTypeIndex;
        vmaFindMemoryTypeIndexForBufferInfo(allocator, &sampleInfo,
                                           &sampleAlloc, &memoryTypeIndex);

        // Create pool
        VmaPoolCreateInfo poolInfo = {};
        poolInfo.memoryTypeIndex = memoryTypeIndex;
        poolInfo.blockSize = 64 * 1024 * 1024;  // 64 MB blocks
        poolInfo.minBlockCount = 1;
        poolInfo.maxBlockCount = maxMemoryMB / 64;

        vmaCreatePool(allocator, &poolInfo, &m_pool);
    }

    ~VoxelChunkPool() {
        // Free all chunks
        for (auto& [id, pair] : m_chunks) {
            vmaDestroyBuffer(m_allocator, pair.first, pair.second);
        }
        vmaDestroyPool(m_allocator, m_pool);
    }

    VkBuffer getChunk(uint64_t chunkId) {
        auto it = m_chunks.find(chunkId);
        if (it != m_chunks.end()) {
            return it->second.first;
        }
        return VK_NULL_HANDLE;
    }

    VkBuffer allocateChunk(uint64_t chunkId) {
        if (m_chunks.count(chunkId)) {
            return m_chunks[chunkId].first;
        }

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_chunkSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.pool = m_pool;
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        m_chunks[chunkId] = {buffer, allocation};
        return buffer;
    }

    void freeChunk(uint64_t chunkId) {
        auto it = m_chunks.find(chunkId);
        if (it != m_chunks.end()) {
            vmaDestroyBuffer(m_allocator, it->second.first, it->second.second);
            m_chunks.erase(it);
        }
    }

    size_t getChunkCount() const { return m_chunks.size(); }

    VmaStatistics getStatistics() {
        VmaStatistics stats;
        vmaGetPoolStatistics(m_allocator, m_pool, &stats);
        return stats;
    }
};
```

---

## Ссылки

| Файл                                                | Описание             |
|-----------------------------------------------------|----------------------|
| [01_quickstart.md](01_quickstart.md)                | Быстрый старт        |
| [03_concepts.md](03_concepts.md)                    | Основные концепции   |
| [04_api-reference.md](04_api-reference.md)          | Справочник API       |
| [09_projectv-patterns.md](09_projectv-patterns.md)  | Паттерны             |
