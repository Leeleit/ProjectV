# Контракты жизненного цикла приложения (Shutdown Sequence) [🟢 Уровень 1]

**Статус:** Technical Specification
**Уровень:** 🟢 Фундаментальный
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ описывает **детерминированный процесс завершения работы** (Graceful Shutdown) для ProjectV. Неправильное
выключение — самая частая причина зависаний Vulkan-приложений, memory leaks и driver crashes. Спецификация определяет
строгий порядок освобождения ресурсов и механизмы отмены асинхронных операций.

---

## 1. Диаграмма Shutdown Sequence

### 1.1 Полный жизненный цикл

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Application Lifecycle                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐       │
│  │  INIT    │ ──▶ │  RUN     │ ──▶ │ SHUTDOWN │ ──▶ │  EXIT    │       │
│  │          │     │          │     │          │     │          │       │
│  └──────────┘     └──────────┘     └──────────┘     └──────────┘       │
│       │                │                 │                 │            │
│       ▼                ▼                 ▼                 ▼            │
│  - SDL Init       - Game Loop     - Cancel Jobs      - std::exit()     │
│  - Vulkan Init    - ECS Update    - Wait Jobs        - Return 0        │
│  - ECS Init       - Render        - Destroy ECS                        │
│  - Job System     - Input         - Destroy Vulkan                     │
│                                    - Destroy SDL                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Детальная Shutdown диаграмма

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Graceful Shutdown Sequence                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Phase 1: SIGNAL & CANCELLATION                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  1.1 Shutdown request (window close / signal / assertion)        │   │
│  │  1.2 Set shutdown_requested_ = true                              │   │
│  │  1.3 Request stop on all stop_sources                           │   │
│  │  1.4 Signal condition variables                                  │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                              │                                           │
│                              ▼                                           │
│  Phase 2: JOB SYSTEM DRAIN                                              │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  2.1 Cancel pending chunk generation tasks                       │   │
│  │  2.2 Wait for in-flight senders (with timeout)                   │   │
│  │  2.3 Process remaining completion callbacks                      │   │
│  │  2.4 Shutdown thread pool                                        │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                              │                                           │
│                              ▼                                           │
│  Phase 3: ECS CLEANUP                                                   │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  3.1 Trigger OnRemove observers for all entities                 │   │
│  │  3.2 Cleanup components with external resources                  │   │
│  │  3.3 Destroy Flecs world                                         │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                              │                                           │
│                              ▼                                           │
│  Phase 4: VULKAN CLEANUP                                                │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  4.1 vkDeviceWaitIdle()                                          │   │
│  │  4.2 Destroy Framebuffers                                        │   │
│  │  4.3 Destroy RenderPass                                          │   │
│  │  4.4 Destroy Pipelines & PipelineLayouts                         │   │
│  │  4.5 Destroy DescriptorSets & DescriptorPools                    │   │
│  │  4.6 Destroy Buffers & Images (via VMA)                          │   │
│  │  4.7 vmaDestroyAllocator()                                       │   │
│  │  4.8 vkDestroyDevice()                                           │   │
│  │  4.9 vkDestroyInstance()                                         │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                              │                                           │
│                              ▼                                           │
│  Phase 5: SDL & FINAL CLEANUP                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  5.1 SDL_DestroyWindow()                                         │   │
│  │  5.2 SDL_Quit()                                                  │   │
│  │  5.3 Flush logs                                                  │   │
│  │  5.4 Return exit code                                            │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Shutdown Manager

### 2.1 Основной интерфейс

```cpp
// ProjectV.Core.Shutdown.cppm
export module ProjectV.Core.Shutdown;

import std;
import std.execution;

export namespace projectv::core {

/// Состояние shutdown.
export enum class ShutdownState : uint8_t {
    Running,          // Нормальная работа
    Requested,        // Запрошен shutdown
    DrainingJobs,     // Ожидание завершения задач
    CleaningECS,      // Очистка ECS
    CleaningVulkan,   // Очистка Vulkan
    CleaningSDL,      // Очистка SDL
    Completed         // Готово к exit
};

/// Менеджер завершения работы.
///
/// ## Thread Safety
/// - Все методы thread-safe
/// - shutdown_requested() lock-free
/// - state transitions use atomic
export class ShutdownManager {
public:
    /// Возвращает singleton instance.
    static auto instance() -> ShutdownManager&;

    /// Запрашивает graceful shutdown.
    /// @param reason Причина shutdown (для логирования)
    auto request_shutdown(std::string_view reason = "User request") -> void;

    /// Проверяет, запрошен ли shutdown.
    /// Lock-free, может вызываться из любого потока.
    [[nodiscard]] auto shutdown_requested() const noexcept -> bool;

    /// Получает stop_token для Job System.
    [[nodiscard]] auto stop_token() const noexcept -> std::stop_token;

    /// Получает stop_source для сигнала отмены.
    [[nodiscard]] auto stop_source() noexcept -> std::stop_source&;

    /// Выполняет полный shutdown sequence.
    /// Блокирует до завершения всех фаз.
    auto execute_shutdown() -> void;

    /// Получает текущее состояние.
    [[nodiscard]] auto state() const noexcept -> ShutdownState;

    /// Устанавливает callback для фазы.
    auto set_phase_callback(
        ShutdownState phase,
        std::function<void()> callback
    ) -> void;

private:
    ShutdownManager() = default;

    std::atomic<ShutdownState> state_{ShutdownState::Running};
    std::stop_source stop_source_;
    std::string shutdown_reason_;
    std::mutex mutex_;
    std::unordered_map<ShutdownState, std::function<void()>> phase_callbacks_;
};

/// Глобальная функция для удобства.
export auto request_shutdown(std::string_view reason = "") -> void {
    ShutdownManager::instance().request_shutdown(reason);
}

/// Глобальная функция для проверки.
export auto is_shutdown_requested() noexcept -> bool {
    return ShutdownManager::instance().shutdown_requested();
}

} // namespace projectv::core
```

### 2.2 Реализация ShutdownManager

```cpp
// ProjectV.Core.Shutdown.cpp
module ProjectV.Core.Shutdown;

import std;
import ProjectV.Core.Diagnostics;
import ProjectV.Core.Tracy;

namespace projectv::core {

auto ShutdownManager::instance() -> ShutdownManager& {
    static ShutdownManager instance;
    return instance;
}

auto ShutdownManager::request_shutdown(std::string_view reason) -> void {
    auto expected = ShutdownState::Running;

    if (!state_.compare_exchange_strong(expected, ShutdownState::Requested)) {
        // Shutdown уже был запрошен
        return;
    }

    shutdown_reason_ = std::string(reason);

    // Логируем причину
    PV_LOG_INFO(std::format("Shutdown requested: {}", reason));
    tracy_log(TracyLogLevel::Info, std::format("Shutdown: {}", reason));

    // Сигнализируем всем stop_source
    stop_source_.request_stop();
}

auto ShutdownManager::shutdown_requested() const noexcept -> bool {
    return state_.load(std::memory_order_acquire) != ShutdownState::Running;
}

auto ShutdownManager::stop_token() const noexcept -> std::stop_token {
    return stop_source_.get_token();
}

auto ShutdownManager::stop_source() noexcept -> std::stop_source& {
    return stop_source_;
}

auto ShutdownManager::state() const noexcept -> ShutdownState {
    return state_.load(std::memory_order_acquire);
}

auto ShutdownManager::execute_shutdown() -> void {
    PV_ZONE("ShutdownManager::execute_shutdown");

    // Phase 2: Drain Job System
    state_.store(ShutdownState::DrainingJobs, std::memory_order_release);
    if (auto it = phase_callbacks_.find(ShutdownState::DrainingJobs);
        it != phase_callbacks_.end()) {
        it->second();
    }

    // Phase 3: ECS Cleanup
    state_.store(ShutdownState::CleaningECS, std::memory_order_release);
    if (auto it = phase_callbacks_.find(ShutdownState::CleaningECS);
        it != phase_callbacks_.end()) {
        it->second();
    }

    // Phase 4: Vulkan Cleanup
    state_.store(ShutdownState::CleaningVulkan, std::memory_order_release);
    if (auto it = phase_callbacks_.find(ShutdownState::CleaningVulkan);
        it != phase_callbacks_.end()) {
        it->second();
    }

    // Phase 5: SDL Cleanup
    state_.store(ShutdownState::CleaningSDL, std::memory_order_release);
    if (auto it = phase_callbacks_.find(ShutdownState::CleaningSDL);
        it != phase_callbacks_.end()) {
        it->second();
    }

    state_.store(ShutdownState::Completed, std::memory_order_release);
    PV_LOG_INFO("Shutdown completed");
}

auto ShutdownManager::set_phase_callback(
    ShutdownState phase,
    std::function<void()> callback
) -> void {
    std::lock_guard lock(mutex_);
    phase_callbacks_[phase] = std::move(callback);
}

} // namespace projectv::core
```

---

## 3. Job System Shutdown (P2300)

### 3.1 Cancellation через Stop Source

```cpp
// ProjectV.JobSystem.Shutdown.cppm
export module ProjectV.JobSystem.Shutdown;

import std;
import std.execution;
import ProjectV.Core.Shutdown;

export namespace projectv::jobsystem {

/// Менеджер отмены задач.
export class JobCancellationManager {
public:
    /// Регистрирует long-running task для отмены.
    auto register_task(std::string_view name, std::stop_source source) -> void;

    /// Отменяет все зарегистрированные задачи.
    auto cancel_all() -> void;

    /// Ожидает завершения всех задач с timeout.
    /// @param timeout_ms Максимальное время ожидания
    /// @return true если все задачи завершились, false если timeout
    auto wait_all(uint32_t timeout_ms = 5000) -> bool;

    /// Выполняет drain всех sender'ов.
    auto drain_senders() -> void;

private:
    struct TaskInfo {
        std::string name;
        std::stop_source stop_source;
        std::atomic<bool> completed{false};
    };

    std::vector<std::shared_ptr<TaskInfo>> tasks_;
    std::mutex mutex_;
};

} // namespace projectv::jobsystem
```

### 3.2 Примеры отмены задач

```cpp
// Пример 1: Chunk Generation Task с cancellation

auto generate_chunk_async(
    ChunkCoordinates coords,
    std::stop_token stoken
) -> std::expected<Chunk, GenerationError> {

    // Периодически проверяем cancellation
    for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
        if (stoken.stop_requested()) {
            PV_LOG_INFO(std::format(
                "Chunk generation cancelled at y={}: ({}, {}, {})",
                y, coords.x, coords.y, coords.z
            ));
            return std::unexpected(GenerationError::Cancelled);
        }

        // Генерируем слой
        generate_layer(coords, y);
    }

    return Chunk{coords};
}

// Пример 2: Sender с stop_token

auto make_chunk_generation_sender(
    ChunkCoordinates coords,
    std::stop_token stoken
) -> stdexec::sender auto {

    return stdexec::just(coords)
        | stdexec::let_value([stoken](ChunkCoordinates c) {
            if (stoken.stop_requested()) {
                return stdexec::just(std::unexpected(GenerationError::Cancelled));
            }
            return generate_chunk_sender(c, stoken);
        });
}

// Пример 3: Интеграция с shutdown

void setup_job_system_shutdown_hooks() {
    auto& shutdown = core::ShutdownManager::instance();

    shutdown.set_phase_callback(
        core::ShutdownState::DrainingJobs,
        []() {
            PV_ZONE("JobSystem::Drain");

            auto& cancellation = JobCancellationManager::instance();

            // 1. Отменяем все задачи
            cancellation.cancel_all();

            // 2. Ждём завершения с timeout
            if (!cancellation.wait_all(5000)) {
                PV_LOG_WARNING("Some jobs did not complete within timeout");
            }

            // 3. Drain remaining senders
            cancellation.drain_senders();
        }
    );
}
```

### 3.3 Timeout и принудительное завершение

```cpp
// ProjectV.JobSystem.Timeout.cppm
export module ProjectV.JobSystem.Timeout;

import std;
import std.execution;
import ProjectV.Core.Diagnostics;

export namespace projectv::jobsystem {

/// Выполняет sender с timeout.
/// @returns std::expected<T, TimeoutError>
export template<stdexec::sender S>
auto with_timeout(
    S&& sender,
    std::chrono::milliseconds timeout
) -> stdexec::sender auto {

    return std::forward<S>(sender)
        | stdexec::let_value([](auto&& result) {
            return stdexec::just(std::forward<decltype(result)>(result));
        })
        | stdexec::let_error([timeout](auto&& error) {
            // Handle timeout
            PV_LOG_ERROR(errors::JOB_TIMEOUT,
                std::format("Job timed out after {}ms", timeout.count()));
            return stdexec::just(std::unexpected(std::forward<decltype(error)>(error)));
        });
}

/// Принудительно завершает task после timeout.
export class ForcefulShutdown {
public:
    /// Устанавливает deadline для shutdown.
    auto set_deadline(std::chrono::milliseconds timeout) -> void {
        deadline_ = std::chrono::steady_clock::now() + timeout;
    }

    /// Проверяет, истёк ли deadline.
    [[nodiscard]] auto is_expired() const -> bool {
        return std::chrono::steady_clock::now() > deadline_;
    }

    /// Выполняет graceful shutdown, затем forceful если timeout.
    auto shutdown_with_timeout(
        std::function<void()> graceful,
        std::function<void()> forceful,
        std::chrono::milliseconds timeout
    ) -> void {

        set_deadline(timeout);

        // Пробуем graceful
        std::thread graceful_thread([this, &graceful]() {
            graceful();
        });

        // Ждём с timeout
        while (!is_expired() && graceful_thread.joinable()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (graceful_thread.joinable()) {
            // Timeout — forceful shutdown
            PV_LOG_WARNING("Graceful shutdown timed out, forcing...");
            forceful();

            // Detach thread (не идеально, но лучше чем hang)
            graceful_thread.detach();
        } else {
            graceful_thread.join();
        }
    }

private:
    std::chrono::steady_clock::time_point deadline_;
};

} // namespace projectv::jobsystem
```

---

## 4. Порядок уничтожения ресурсов

### 4.1 Vulkan Cleanup Order

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Vulkan Destruction Order                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Vulkan требует уничтожения в строго обратном порядке создания:         │
│                                                                          │
│  1. Sync Objects                                                        │
│     └─► vkDestroyFence()                                                │
│     └─► vkDestroySemaphore()                                            │
│     └─► vkDestroyEvent() (rarely used)                                  │
│                                                                          │
│  2. Command Buffers                                                     │
│     └─► vkFreeCommandBuffers()                                          │
│     └─► vkDestroyCommandPool()                                          │
│                                                                          │
│  3. Framebuffer & RenderPass                                            │
│     └─► vkDestroyFramebuffer()                                          │
│     └─► vkDestroyRenderPass()                                           │
│                                                                          │
│  4. Pipeline Objects                                                    │
│     └─► vkDestroyPipeline()                                             │
│     └─► vkDestroyPipelineLayout()                                       │
│     └─► vkDestroyShaderModule()                                         │
│                                                                          │
│  5. Descriptor Objects                                                  │
│     └─► vkDestroyDescriptorPool()  (destroys all descriptor sets)       │
│     └─► vkDestroyDescriptorSetLayout()                                  │
│                                                                          │
│  6. Sampler                                                             │
│     └─► vkDestroySampler()                                              │
│                                                                          │
│  7. Images & Buffers (via VMA)                                          │
│     └─► vmaDestroyImage()                                               │
│     └─► vmaDestroyBuffer()                                              │
│     └─► vmaDestroyAllocation()                                          │
│     └─► vmaDestroyAllocator()                                           │
│                                                                          │
│  8. Swapchain                                                           │
│     └─► vkDestroySwapchainKHR()                                         │
│                                                                          │
│  9. Surface                                                             │
│     └─► vkDestroySurfaceKHR()                                           │
│                                                                          │
│  10. Device & Instance                                                  │
│      └─► vkDestroyDevice()                                              │
│      └─► vkDestroyInstance()                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Vulkan Cleanup Implementation

```cpp
// ProjectV.Renderer.VulkanCleanup.cppm
export module ProjectV.Renderer.VulkanCleanup;

import std;
import vulkan;
import ProjectV.Core.Shutdown;
import ProjectV.Core.Diagnostics;
import ProjectV.Memory.VMA;

export namespace projectv::renderer {

/// Менеджер очистки Vulkan ресурсов.
export class VulkanCleanupManager {
public:
    /// Выполняет полный Vulkan cleanup.
    static auto cleanup(
        VkInstance instance,
        VkDevice device,
        VmaAllocator allocator,
        VkSwapchainKHR swapchain,
        VkSurfaceKHR surface
    ) -> void {

        PV_ZONE("VulkanCleanup");

        if (device == VK_NULL_HANDLE) {
            return;  // Already cleaned up
        }

        // 1. Wait for device idle
        PV_LOG_INFO("Waiting for device idle...");
        vkDeviceWaitIdle(device);

        // 2. Destroy sync objects
        destroy_sync_objects(device);

        // 3. Free command buffers
        destroy_command_pools(device);

        // 4. Destroy framebuffers & renderpass
        destroy_framebuffers(device);
        destroy_renderpass(device);

        // 5. Destroy pipelines
        destroy_pipelines(device);

        // 6. Destroy descriptors
        destroy_descriptors(device);

        // 7. Destroy images & buffers (VMA)
        destroy_vma_resources(allocator);

        // 8. Destroy swapchain
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }

        // 9. Destroy surface
        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        // 10. Destroy device
        vkDestroyDevice(device, nullptr);

        // 11. Destroy instance
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }

        PV_LOG_INFO("Vulkan cleanup completed");
    }

private:
    static auto destroy_sync_objects(VkDevice device) -> void {
        // Destroy fences
        for (auto fence : fences_) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(device, fence, nullptr);
            }
        }

        // Destroy semaphores
        for (auto sem : semaphores_) {
            if (sem != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, sem, nullptr);
            }
        }
    }

    static auto destroy_command_pools(VkDevice device) -> void {
        for (auto pool : command_pools_) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, pool, nullptr);
            }
        }
    }

    static auto destroy_framebuffers(VkDevice device) -> void {
        for (auto fb : framebuffers_) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, fb, nullptr);
            }
        }
    }

    static auto destroy_renderpass(VkDevice device) -> void {
        if (render_pass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass_, nullptr);
        }
    }

    static auto destroy_pipelines(VkDevice device) -> void {
        for (auto pipeline : pipelines_) {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline, nullptr);
            }
        }
        for (auto layout : pipeline_layouts_) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, layout, nullptr);
            }
        }
        for (auto module : shader_modules_) {
            if (module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, module, nullptr);
            }
        }
    }

    static auto destroy_descriptors(VkDevice device) -> void {
        for (auto pool : descriptor_pools_) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, pool, nullptr);
            }
        }
        for (auto layout : descriptor_set_layouts_) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, layout, nullptr);
            }
        }
    }

    static auto destroy_vma_resources(VmaAllocator allocator) -> void {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
        }
    }

    // Resource storage (populated during runtime)
    inline static std::vector<VkFence> fences_;
    inline static std::vector<VkSemaphore> semaphores_;
    inline static std::vector<VkCommandPool> command_pools_;
    inline static std::vector<VkFramebuffer> framebuffers_;
    inline static VkRenderPass render_pass_{VK_NULL_HANDLE};
    inline static std::vector<VkPipeline> pipelines_;
    inline static std::vector<VkPipelineLayout> pipeline_layouts_;
    inline static std::vector<VkShaderModule> shader_modules_;
    inline static std::vector<VkDescriptorPool> descriptor_pools_;
    inline static std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
};

} // namespace projectv::renderer
```

### 4.3 ECS Cleanup с OnRemove Observers

```cpp
// ProjectV.ECS.Shutdown.cppm
export module ProjectV.ECS.Shutdown;

import std;
import flecs;
import ProjectV.Core.Shutdown;
import ProjectV.Core.Diagnostics;

export namespace projectv::ecs {

/// Менеджер очистки ECS.
export class ECSCleanupManager {
public:
    /// Регистрирует observer для очистки ресурсов.
    template<typename Component>
    static auto register_cleanup_observer(
        flecs::world& world,
        std::function<void(Component const&)> cleanup_fn
    ) -> void {

        world.observer<Component>()
            .event(flecs::OnRemove)
            .each([cleanup_fn](flecs::entity e, Component const& component) {
                PV_LOG_INFO(std::format(
                    "Cleaning up component {} on entity {}",
                    typeid(Component).name(),
                    e.id()
                ));
                cleanup_fn(component);
            });
    }

    /// Выполняет очистку ECS world.
    static auto cleanup(flecs::world& world) -> void {
        PV_ZONE("ECSCleanup");

        // 1. Удаляем все entities (триггерит OnRemove observers)
        PV_LOG_INFO("Removing all entities...");
        world.delete_with(flecs::Wildcard);

        // 2. Обрабатываем deferred actions
        world.progress(0.0f);

        // 3. Уничтожаем world
        PV_LOG_INFO("Destroying ECS world...");
        world.fini();

        PV_LOG_INFO("ECS cleanup completed");
    }
};

// Пример регистрации cleanup observers

void setup_ecs_cleanup_observers(flecs::world& world) {
    // Cleanup для GPU буферов
    ECSCleanupManager::register_cleanup_observer<VoxelChunkGPUComponent>(
        world,
        [](VoxelChunkGPUComponent const& chunk) {
            if (chunk.vertex_buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(vk_device, chunk.vertex_buffer, nullptr);
            }
            if (chunk.index_buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(vk_device, chunk.index_buffer, nullptr);
            }
        }
    );

    // Cleanup для physics bodies
    ECSCleanupManager::register_cleanup_observer<PhysicsBodyComponent>(
        world,
        [](PhysicsBodyComponent const& body) {
            if (body.jolt_body_id.IsValid()) {
                physics_system->RemoveBody(body.jolt_body_id);
            }
        }
    );

    // Cleanup для audio sources
    ECSCleanupManager::register_cleanup_observer<AudioSourceComponent>(
        world,
        [](AudioSourceComponent const& source) {
            if (source.handle != AUDIO_INVALID_HANDLE) {
                audio_system->unload_source(source.handle);
            }
        }
    );
}

} // namespace projectv::ecs
```

---

## 5. Обработка Window Close Event

### 5.1 SDL3 Event Handling

```cpp
// ProjectV.Platform.SDLEvents.cppm
export module ProjectV.Platform.SDLEvents;

import std;
import <SDL3/SDL.h>;
import ProjectV.Core.Shutdown;
import ProjectV.Core.Diagnostics;

export namespace projectv::platform {

/// Обрабатывает SDL events.
export class SDLEventHandler {
public:
    /// Обрабатывает одно событие.
    /// @return true если приложение должно продолжать работу
    static auto process_event(SDL_Event const& event) -> bool {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                handle_quit();
                return false;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                handle_window_close(event.window);
                return false;

            case SDL_EVENT_WINDOW_MINIMIZED:
                handle_minimized(event.window);
                return true;

            case SDL_EVENT_WINDOW_RESTORED:
                handle_restored(event.window);
                return true;

            default:
                return true;
        }
    }

    /// Poll events и обрабатывает их.
    /// @return true если приложение должно продолжать работу
    static auto poll_events() -> bool {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!process_event(event)) {
                return false;
            }
        }
        return true;
    }

private:
    static auto handle_quit() -> void {
        PV_LOG_INFO("SDL_EVENT_QUIT received");
        core::request_shutdown("SDL quit event");
    }

    static auto handle_window_close(SDL_WindowEvent const& window_event) -> void {
        PV_LOG_INFO(std::format(
            "Window close requested: window_id={}",
            window_event.windowID
        ));
        core::request_shutdown("Window close button");
    }

    static auto handle_minimized(SDL_WindowEvent const& window_event) -> void {
        PV_LOG_INFO("Window minimized");
        // Можно приостановить rendering
    }

    static auto handle_restored(SDL_WindowEvent const& window_event) -> void {
        PV_LOG_INFO("Window restored");
        // Возможно нужно recreate swapchain
    }
};

} // namespace projectv::platform
```

### 5.2 Main Loop с Shutdown

```cpp
// main.cpp

import std;
import ProjectV.Core.Shutdown;
import ProjectV.Platform.SDLEvents;
import ProjectV.Renderer.VulkanCleanup;
import ProjectV.ECS.Shutdown;
import ProjectV.JobSystem.Shutdown;
import <SDL3/SDL.h>;

auto main(int argc, char* argv[]) -> int {

    // === Initialization ===
    if (!initialize_engine()) {
        return 1;
    }

    // === Setup Shutdown Hooks ===
    auto& shutdown_manager = projectv::core::ShutdownManager::instance();

    // Job System cleanup
    shutdown_manager.set_phase_callback(
        projectv::core::ShutdownState::DrainingJobs,
        []() { projectv::jobsystem::drain_all_jobs(); }
    );

    // ECS cleanup
    shutdown_manager.set_phase_callback(
        projectv::core::ShutdownState::CleaningECS,
        [&]() { projectv::ecs::ECSCleanupManager::cleanup(g_world); }
    );

    // Vulkan cleanup
    shutdown_manager.set_phase_callback(
        projectv::core::ShutdownState::CleaningVulkan,
        []() {
            projectv::renderer::VulkanCleanupManager::cleanup(
                g_instance, g_device, g_allocator, g_swapchain, g_surface
            );
        }
    );

    // SDL cleanup
    shutdown_manager.set_phase_callback(
        projectv::core::ShutdownState::CleaningSDL,
        []() {
            if (g_window) {
                SDL_DestroyWindow(g_window);
            }
            SDL_Quit();
        }
    );

    // === Main Loop ===
    while (!shutdown_manager.shutdown_requested()) {
        // Process events
        if (!projectv::platform::SDLEventHandler::poll_events()) {
            break;
        }

        // Update
        update_frame();

        // Render
        render_frame();
    }

    // === Shutdown ===
    shutdown_manager.execute_shutdown();

    return 0;
}
```

---

## 6. Таблица зависимостей Shutdown

| Фаза | Компонент         | Зависит от       | Timeout   |
|------|-------------------|------------------|-----------|
| 1    | Shutdown Request  | —                | Immediate |
| 2    | Job Cancellation  | Phase 1          | 100ms     |
| 2    | Job Drain         | Job Cancellation | 5000ms    |
| 3    | ECS OnRemove      | Phase 2          | 1000ms    |
| 3    | ECS World Destroy | OnRemove         | Immediate |
| 4    | vkDeviceWaitIdle  | Phase 3          | 2000ms    |
| 4    | Vulkan Cleanup    | vkDeviceWaitIdle | Immediate |
| 5    | SDL_DestroyWindow | Phase 4          | Immediate |
| 5    | SDL_Quit          | DestroyWindow    | Immediate |

---

## 7. Checklist для Shutdown

### 7.1 Pre-Shutdown Verification

```cpp
/// Проверяет readiness для shutdown.
auto verify_shutdown_readiness() -> std::expected<void, std::string> {

    // 1. Все GPU commands completed
    if (in_flight_frames > 0) {
        return std::unexpected("GPU commands still in flight");
    }

    // 2. No pending allocations
    if (VMA::pending_allocations() > 0) {
        return std::unexpected("VMA allocations pending");
    }

    // 3. Job system idle
    if (!job_system->is_idle()) {
        return std::unexpected("Job system not idle");
    }

    // 4. No ECS deferred actions
    if (world.has_deferred_actions()) {
        return std::unexpected("ECS has deferred actions");
    }

    return {};
}
```

### 7.2 Shutdown Statistics

```cpp
/// Статистика shutdown для логирования.
struct ShutdownStats {
    uint32_t jobs_cancelled{0};
    uint32_t jobs_completed{0};
    uint32_t entities_destroyed{0};
    uint32_t vulkan_objects_destroyed{0};
    uint64_t total_time_ms{0};
};

auto log_shutdown_stats(ShutdownStats const& stats) -> void {
    PV_LOG_INFO("=== Shutdown Statistics ===");
    PV_LOG_INFO(std::format("Jobs cancelled:   {}", stats.jobs_cancelled));
    PV_LOG_INFO(std::format("Jobs completed:   {}", stats.jobs_completed));
    PV_LOG_INFO(std::format("Entities freed:   {}", stats.entities_destroyed));
    PV_LOG_INFO(std::format("Vulkan objects:   {}", stats.vulkan_objects_destroyed));
    PV_LOG_INFO(std::format("Total time:       {} ms", stats.total_time_ms));
}
```

---

## Статус

| Компонент                | Статус         | Приоритет |
|--------------------------|----------------|-----------|
| ShutdownManager          | Специфицирован | P0        |
| Job Cancellation (P2300) | Специфицирован | P0        |
| Vulkan Cleanup Order     | Специфицирован | P0        |
| ECS OnRemove Observers   | Специфицирован | P0        |
| SDL Event Handling       | Специфицирован | P1        |
| Shutdown Statistics      | Специфицирован | P2        |

---

## Ссылки

- [35_error_handling_spec.md](../01_core/07_error_handling.md)
- [31_job_system_p2300_spec.md](../01_core/05_job_system.md)
- [00_engine-structure.md](../01_core/01_engine_structure.md)
- [P2300 std::execution](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2300r7.html)
- [Vulkan Object Destruction Order](https://docs.vulkan.org/guide/latest/destruction_order.html)
