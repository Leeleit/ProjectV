# Tracy в ProjectV: Примеры

**🟡 Уровень 2: Средний** — Готовые примеры кода для интеграции Tracy в ProjectV.

---

## Пример 1: Полный игровой цикл

```cpp
// src/main.cpp
#include "tracy/Tracy.hpp"
#include "Game.hpp"

int main() {
    TracyMessageL("ProjectV starting...");

    projectv::Game game;
    game.init();

    TracyMessageL("Game initialized");

    while (game.isRunning()) {
        FrameMark;

        {
            ZoneScopedNC("Events", projectv::ProfilingColors::SDLEvents);
            game.processEvents();
        }

        {
            ZoneScopedNC("Update", projectv::ProfilingColors::ECS);
            game.update();
        }

        {
            ZoneScopedNC("Render", projectv::ProfilingColors::Render);
            game.render();
        }

        TracyPlot("FPS", (int64_t)game.getFPS());
        TracyPlot("FrameTime", game.getFrameTime());
    }

    TracyMessageL("ProjectV shutting down...");
    game.cleanup();

    return 0;
}
```

---

## Пример 2: Vulkan рендерер с Tracy

```cpp
// src/renderer/VulkanRenderer.hpp
#pragma once

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#include <vulkan/vulkan.h>

namespace projectv {

class VulkanRenderer {
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkCommandPool m_commandPool;

    tracy::VkCtx* m_tracyCtx = nullptr;

public:
    void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
        m_physicalDevice = physicalDevice;
        m_device = device;

        // Получить graphics queue
        vkGetDeviceQueue(device, 0, 0, &m_graphicsQueue);

        // Создать command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = 0;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool);

        // Инициализация Tracy для Vulkan
        VkCommandBuffer initCmd = allocateCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(initCmd, &beginInfo);

        m_tracyCtx = tracy::CreateVkContext(physicalDevice, device, m_graphicsQueue, initCmd);

        vkEndCommandBuffer(initCmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &initCmd;
        vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(device, m_commandPool, 1, &initCmd);

        TracyMessageL("Vulkan Tracy context initialized");
    }

    void render() {
        ZoneScopedNC("VulkanRender", ProfilingColors::Render);

        VkCommandBuffer cmd = allocateCommandBuffer();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        TracyVkZone(m_tracyCtx, cmd, "Frame");

        // Shadow pass
        {
            TracyVkZone(m_tracyCtx, cmd, "ShadowPass");
            renderShadows(cmd);
        }

        // Geometry pass
        {
            TracyVkZone(m_tracyCtx, cmd, "GeometryPass");
            renderGeometry(cmd);
        }

        // Voxel compute
        {
            TracyVkZone(m_tracyCtx, cmd, "VoxelCompute");
            computeVoxels(cmd);
        }

        // Post-process
        {
            TracyVkZone(m_tracyCtx, cmd, "PostProcess");
            renderPostProcess(cmd);
        }

        vkEndCommandBuffer(cmd);

        // Submit
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);

        // Собрать данные Tracy
        TracyVkCollect(m_tracyCtx, m_graphicsQueue);

        vkQueueWaitIdle(m_graphicsQueue);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
    }

    void cleanup() {
        if (m_tracyCtx) {
            tracy::DestroyVkContext(m_tracyCtx);
            m_tracyCtx = nullptr;
        }
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }

private:
    VkCommandBuffer allocateCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);
        return cmd;
    }

    void renderShadows(VkCommandBuffer cmd) { /* ... */ }
    void renderGeometry(VkCommandBuffer cmd) { /* ... */ }
    void computeVoxels(VkCommandBuffer cmd) { /* ... */ }
    void renderPostProcess(VkCommandBuffer cmd) { /* ... */ }
};

} // namespace projectv
```

---

## Пример 3: ECS система с профилированием

```cpp
// src/ecs/systems/MovementSystem.cpp
#include "tracy/Tracy.hpp"
#include "MovementSystem.hpp"

namespace projectv::systems {

void MovementSystem::update(flecs::world& world, float dt) {
    ZoneScopedNC("MovementSystem", ProfilingColors::ECS);

    size_t processedEntities = 0;

    world.each([&](flecs::entity e, Position& pos, const Velocity& vel) {
        // Пропускаем неподвижные сущности
        if (vel.x == 0.0f && vel.y == 0.0f && vel.z == 0.0f) {
            return;
        }

        // Обновление позиции
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;

        processedEntities++;
    });

    TracyPlot("Movement_Entities", (int64_t)processedEntities);
}

} // namespace projectv::systems
```

---

## Пример 4: Менеджер памяти с VMA и Tracy

```cpp
// src/memory/GPUMemoryManager.hpp
#pragma once

#include "tracy/Tracy.hpp"
#include "vk_mem_alloc.h"
#include <unordered_map>

namespace projectv {

class GPUMemoryManager {
    VmaAllocator m_allocator;
    std::unordered_map<VmaAllocation, const char*> m_allocationNames;

public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance) {
        VmaAllocatorCreateInfo createInfo{};
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.instance = instance;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;

        vmaCreateAllocator(&createInfo, &m_allocator);

        TracyMessageL("VMA allocator created");
    }

    struct BufferResult {
        VkBuffer buffer;
        VmaAllocation allocation;
        void* mappedData;
    };

    BufferResult createBuffer(size_t size, VkBufferUsageFlags usage,
                              VmaMemoryUsage memoryUsage, const char* name) {
        ZoneScopedNC("CreateBuffer", ProfilingColors::Memory);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
            memoryUsage == VMA_MEMORY_USAGE_GPU_TO_CPU) {
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        BufferResult result;
        VmaAllocationInfo allocationInfo;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, &allocationInfo);

        result.mappedData = allocationInfo.pMappedData;

        // Отслеживание в Tracy
        TracyAllocN(result.allocation, size, name);
        m_allocationNames[result.allocation] = name;

        TracyPlot(name, (int64_t)size);

        return result;
    }

    void destroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
        auto it = m_allocationNames.find(allocation);
        if (it != m_allocationNames.end()) {
            TracyFreeN(allocation, it->second);
            m_allocationNames.erase(it);
        }

        vmaDestroyBuffer(m_allocator, buffer, allocation);
    }

    void cleanup() {
        // Освободить все оставшиеся аллокации в Tracy
        for (const auto& [allocation, name] : m_allocationNames) {
            TracyFreeN(allocation, name);
        }
        m_allocationNames.clear();

        vmaDestroyAllocator(m_allocator);
    }

    size_t getTotalAllocated() const {
        VmaStats stats;
        vmaCalculateStats(m_allocator, &stats);
        return stats.total.bytesAllocated;
    }
};

} // namespace projectv
```

---

## Пример 5: Генератор чанков

```cpp
// src/world/ChunkGenerator.cpp
#include "tracy/Tracy.hpp"
#include "ChunkGenerator.hpp"

namespace projectv {

Chunk ChunkGenerator::generateChunk(const ChunkCoord& coord) {
    ZoneScopedNC("GenerateChunk", ProfilingColors::ChunkGen);

    Chunk chunk;
    chunk.coord = coord;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Этап 1: Генерация шума
    {
        ZoneScopedNC("Noise", ProfilingColors::ChunkGen);
        generateNoise(chunk, coord);
    }

    // Этап 2: Карвинг (пещеры)
    {
        ZoneScopedNC("Carving", ProfilingColors::ChunkGen);
        carveCaves(chunk);
    }

    // Этап 3: Размещение объектов
    {
        ZoneScopedNC("Features", ProfilingColors::ChunkGen);
        placeFeatures(chunk);
    }

    // Этап 4: Построение меша
    {
        ZoneScopedNC("MeshBuild", ProfilingColors::ChunkGen);
        buildMesh(chunk);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    // Метрики
    TracyPlot("ChunkGen_TimeMS", ms);
    TracyPlot("ChunkGen_Voxels", (int64_t)chunk.voxels.size());
    TracyPlot("ChunkGen_Vertices", (int64_t)chunk.vertices.size());

    if (ms > 10.0f) {
        TracyMessageLC(fmt::format("Slow chunk gen at ({},{},{}): {:.1f}ms",
                                   coord.x, coord.y, coord.z, ms).c_str(), 0xFFFF00);
    }

    return chunk;
}

void ChunkGenerator::generateNoise(Chunk& chunk, const ChunkCoord& coord) {
    ZoneScopedNC("GenerateNoise", ProfilingColors::ChunkGen);

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                float noise = m_noiseGenerator.noise(
                    (coord.x * CHUNK_SIZE + x) * 0.01f,
                    (coord.y * CHUNK_SIZE + y) * 0.01f,
                    (coord.z * CHUNK_SIZE + z) * 0.01f
                );

                chunk.voxels[x][y][z] = noise > 0.5f ? 1 : 0;
            }
        }
    }
}

} // namespace projectv
```

---

## Пример 6: Streaming система

```cpp
// src/world/ChunkStreamer.cpp
#include "tracy/Tracy.hpp"
#include "ChunkStreamer.hpp"

namespace projectv {

ChunkStreamer::ChunkStreamer(size_t workerCount) {
    ZoneScopedNC("StreamerInit", ProfilingColors::Streaming);

    m_running = true;

    for (size_t i = 0; i < workerCount; i++) {
        m_workers.emplace_back(&ChunkStreamer::workerThread, this);
    }

    TracyMessageL(fmt::format("ChunkStreamer started with {} workers", workerCount).c_str());
}

ChunkStreamer::~ChunkStreamer() {
    m_running = false;
    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ChunkStreamer::requestLoad(const ChunkCoord& coord, LoadPriority priority) {
    ZoneScopedNC("RequestLoad", ProfilingColors::Streaming);

    {
        std::lock_guard lock(m_queueMutex);
        m_loadQueue.push({coord, priority});
    }

    m_condition.notify_one();

    TracyPlot("Streaming_PendingLoads", (int64_t)m_loadQueue.size());
}

void ChunkStreamer::workerThread() {
    TracyMessageL("Streaming worker started");

    while (m_running) {
        ChunkLoadRequest request;

        {
            std::unique_lock lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return !m_loadQueue.empty() || !m_running;
            });

            if (!m_running) break;

            if (m_loadQueue.empty()) continue;

            request = m_loadQueue.top();
            m_loadQueue.pop();
        }

        // Обработка запроса
        ZoneScopedNC("LoadChunk", ProfilingColors::Streaming);

        Chunk chunk = m_generator.generateChunk(request.coord);

        {
            ZoneScopedNC("UploadToGPU", ProfilingColors::Streaming);
            uploadChunkToGPU(chunk);
        }

        {
            std::lock_guard lock(m_loadedMutex);
            m_loadedChunks.push(chunk);
        }

        TracyPlot("Streaming_LoadedChunks", (int64_t)m_loadedChunks.size());
    }

    TracyMessageL("Streaming worker stopped");
}

} // namespace projectv
```

---

## Пример 7: Профилирование с ImGui интеграцией

```cpp
// src/ui/ProfilerWindow.cpp
#include "tracy/Tracy.hpp"
#include "imgui.h"
#include "ProfilerWindow.hpp"

namespace projectv::ui {

void ProfilerWindow::render() {
    ZoneScopedNC("ProfilerWindow", 0xFF00FF);

    if (!m_visible) return;

    ImGui::Begin("Profiler", &m_visible);

    // FPS график
    ImGui::Text("Performance");
    ImGui::Separator();

    ImGui::Text("FPS: %.1f", m_fps);
    ImGui::Text("Frame: %.2f ms", m_frameTime);
    ImGui::Text("GPU: %.2f ms", m_gpuTime);

    ImGui::Spacing();

    // Memory usage
    ImGui::Text("Memory");
    ImGui::Separator();

    ImGui::Text("Voxel Data: %.1f MB", m_voxelMemory / (1024.0 * 1024.0));
    ImGui::Text("GPU Memory: %.1f MB", m_gpuMemory / (1024.0 * 1024.0));
    ImGui::Text("ECS Memory: %.1f MB", m_ecsMemory / (1024.0 * 1024.0));

    ImGui::Spacing();

    // Chunk stats
    ImGui::Text("Chunks");
    ImGui::Separator();

    ImGui::Text("Total: %zu", m_totalChunks);
    ImGui::Text("Visible: %zu", m_visibleChunks);
    ImGui::Text("Loading: %zu", m_loadingChunks);

    ImGui::Spacing();

    // Tracy status
    ImGui::Text("Tracy");
    ImGui::Separator();

    bool connected = TracyIsConnected;
    ImGui::Text("Status: %s", connected ? "Connected" : "Disconnected");

    if (ImGui::Button(connected ? "Disconnect" : "Connect")) {
        // Tracy управляет подключением автоматически
    }

    if (ImGui::Button("Save Trace")) {
        TracyMessageL("Save trace requested from UI");
    }

    ImGui::End();

    // Обновление Plots
    TracyPlot("UI_WindowCount", (int64_t)m_windowCount);
}

} // namespace projectv::ui
```

---

## Пример 8: Конфигурация CMakeLists.txt

```cmake
# CMakeLists.txt (фрагмент для Tracy)

# Опции Tracy
option(TRACY_ENABLE "Enable Tracy profiling" ON)
option(TRACY_ON_DEMAND "Enable on-demand profiling" OFF)

if(TRACY_ENABLE)
    # Добавить Tracy как subdirectory
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy)

    # Определения для Tracy
    target_compile_definitions(ProjectV PRIVATE
        TRACY_ENABLE
    )

    # Debug конфигурация
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=16
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    # Release конфигурация с минимальным overhead
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=4
        )

        if(TRACY_ON_DEMAND)
            target_compile_definitions(ProjectV PRIVATE
                TRACY_ON_DEMAND
            )
        endif()

        # Отключить системное профилирование для release
        target_compile_definitions(ProjectV PRIVATE
            TRACY_NO_SAMPLING
            TRACY_NO_SYSTEM_TRACING
            TRACY_NO_CONTEXT_SWITCH
        )
    endif()

    # Линковка
    target_link_libraries(ProjectV PRIVATE
        Tracy::TracyClient
    )

    # Include директории
    target_include_directories(ProjectV PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tracy/public
    )

    message(STATUS "Tracy profiling: ENABLED")
else()
    message(STATUS "Tracy profiling: DISABLED")
endif()
```

---

## Пример использования

```cpp
// Полный пример использования
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

int main() {
    // Информация о приложении
    TracyAppInfo("ProjectV v0.0.1", 14);

    // Инициализация Vulkan и Tracy
    // ...

    while (running) {
        FrameMark;

        {
            ZoneScopedNC("Update", 0x00FF00);

            // Обновление мира
            world.update(dt);

            TracyPlot("EntityCount", (int64_t)world.entityCount());
        }

        {
            ZoneScopedNC("Render", 0x0000FF);

            // Рендеринг
            TracyVkZone(tracyCtx, cmd, "GPU_Frame");
            renderer.render(cmd);

            TracyVkCollect(tracyCtx, queue);
        }
    }

    return 0;
}