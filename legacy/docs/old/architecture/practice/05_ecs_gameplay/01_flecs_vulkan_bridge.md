# Спецификация Flecs-Vulkan Bridge ProjectV

---

## Обзор

Flecs-Vulkan Bridge — интеграционный слой между ECS (Flecs) и Vulkan 1.4 для высокопроизводительного воксельного
рендеринга.

### Архитектурные принципы

| Принцип                    | Реализация                               |
|----------------------------|------------------------------------------|
| **Declarative Resources**  | Автоматический lifecycle через observers |
| **Data-Oriented Design**   | SoA layout для SIMD                      |
| **Multi-threading Safety** | Thread-local command contexts            |
| **Reactive Updates**       | Observers для автоматических обновлений  |

### Три уровня интеграции

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Flecs-Vulkan Integration Layers                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Layer 1: Data (DOD)                                                     │
│  ───────────────────                                                     │
│  VoxelData[], Transform[], MaterialData[] — сырые массивы для SIMD      │
│                                                                          │
│  Layer 2: Logic (ECS)                                                    │
│  ────────────────────                                                    │
│  Flecs entities, components, systems, observers                          │
│                                                                          │
│  Layer 3: Graphics (Vulkan)                                              │
│  ─────────────────────────                                               │
│  Buffers, Images, Pipelines, Descriptor Sets                             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Интерфейсы Vulkan Components

```cpp
// ProjectV.ECS.Vulkan.Components.cppm
export module ProjectV.ECS.Vulkan.Components;

import std;
import glm;
import ProjectV.Render.Vulkan;

export namespace projectv::ecs::vulkan {

/// Компонент Vulkan буфера.
export struct VkBufferComponent {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkDeviceAddress device_address{0};

    bool is_sparse{false};
    VkDeviceSize size{0};
    uint32_t alignment{0};
    bool needs_update{true};
    uint64_t last_frame_used{0};
};

/// Компонент Vulkan изображения.
export struct VkImageComponent {
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};

    bool is_sparse{false};
    VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    uint32_t width{0}, height{0}, depth{1};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
    VkImageLayout current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

/// Компонент Vulkan пайплайна.
export struct VkPipelineComponent {
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};

    bool use_mesh_shaders{false};
    bool use_dynamic_rendering{true};
};

/// Компонент descriptor buffer (Vulkan 1.4 bindless).
export struct VkDescriptorBufferComponent {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkDeviceAddress device_address{0};

    uint32_t descriptor_count{0};
    uint32_t descriptor_size{0};

    struct Slot {
        bool allocated{false};
        uint32_t index{0};
        uint64_t last_access_frame{0};
    };
    std::vector<Slot> slots;
};

} // namespace projectv::ecs::vulkan
```

---

## Интерфейсы ECS Systems

```cpp
// ProjectV.ECS.Vulkan.Systems.cppm
export module ProjectV.ECS.Vulkan.Systems;

import std;
import flecs;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::vulkan {

/// Регистрирует все Vulkan ECS системы.
export auto register_vulkan_systems(flecs::world& world) -> void;

/// Регистрирует все Vulkan observers.
export auto register_vulkan_observers(flecs::world& world) -> void;

} // namespace projectv::ecs::vulkan
```

---

## Observer Interfaces

```cpp
// ProjectV.ECS.Vulkan.Observers.cppm
export module ProjectV.ECS.Vulkan.Observers;

import std;
import flecs;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::vulkan {

/// Observer для создания буферов.
export struct BufferCreationObserver {
    static auto register_with(flecs::world& world) -> void;
};

/// Observer для уничтожения буферов.
export struct BufferDestructionObserver {
    static auto register_with(flecs::world& world) -> void;
};

/// Observer для создания изображений.
export struct ImageCreationObserver {
    static auto register_with(flecs::world& world) -> void;
};

/// Observer для уничтожения изображений.
export struct ImageDestructionObserver {
    static auto register_with(flecs::world& world) -> void;
};

/// Observer для обновления буферов.
export struct BufferUpdateObserver {
    static auto register_with(flecs::world& world) -> void;
};

} // namespace projectv::ecs::vulkan
```

---

## Thread-Local Command Context

```cpp
// ProjectV.ECS.Vulkan.ThreadContext.cppm
export module ProjectV.ECS.Vulkan.ThreadContext;

import std;
import ProjectV.Render.Vulkan;

export namespace projectv::ecs::vulkan {

/// Thread-local контекст для записи команд.
export class ThreadCommandContext {
public:
    /// Создаёт контекст.
    [[nodiscard]] static auto create(
        VkDevice device,
        uint32_t queue_family,
        uint32_t thread_index
    ) noexcept -> std::expected<ThreadCommandContext, VulkanError>;

    ~ThreadCommandContext() noexcept;

    ThreadCommandContext(ThreadCommandContext&&) noexcept;
    ThreadCommandContext& operator=(ThreadCommandContext&&) noexcept;
    ThreadCommandContext(const ThreadCommandContext&) = delete;
    ThreadCommandContext& operator=(const ThreadCommandContext&) = delete;

    /// Начинает кадр.
    auto begin_frame() noexcept -> void;

    /// Заканчивает кадр.
    auto end_frame() noexcept -> void;

    /// Получает текущий command buffer.
    [[nodiscard]] auto command_buffer() const noexcept -> VkCommandBuffer;

    /// Получает индекс потока.
    [[nodiscard]] auto thread_index() const noexcept -> uint32_t;

private:
    ThreadCommandContext() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Manager для thread-local контекстов.
export class ThreadContextManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        uint32_t queue_family,
        uint32_t thread_count
    ) noexcept -> std::expected<ThreadContextManager, VulkanError>;

    ~ThreadContextManager() noexcept;

    ThreadContextManager(ThreadContextManager&&) noexcept;
    ThreadContextManager& operator=(ThreadContextManager&&) noexcept;
    ThreadContextManager(const ThreadContextManager&) = delete;
    ThreadContextManager& operator=(const ThreadContextManager&) = delete;

    /// Получает контекст для текущего потока.
    [[nodiscard]] auto get_current() noexcept -> ThreadCommandContext*;

    /// Начинает кадр для всех контекстов.
    auto begin_all_frames() noexcept -> void;

    /// Заканчивает кадр для всех контекстов.
    auto end_all_frames() noexcept -> void;

    /// Собирает все command buffers.
    [[nodiscard]] auto collect_command_buffers() noexcept -> std::vector<VkCommandBuffer>;

private:
    ThreadContextManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs::vulkan
```

---

## System Phases

```cpp
// ProjectV.ECS.Vulkan.Phases.cppm
export module ProjectV.ECS.Vulkan.Phases;

import flecs;

export namespace projectv::ecs::vulkan {

/// Фазы рендеринга.
export enum class RenderPhase : uint8_t {
    Prepare,    ///< Обновление ресурсов, маппинг буферов
    Cull,       ///< Frustum culling, LOD selection
    Record,     ///< Запись command buffers
    Compute,    ///< Async compute dispatch
    Sync,       ///< Timeline semaphore sync
    Present     ///< Queue submission, presentation
};

/// Регистрирует фазы в Flecs.
export auto register_render_phases(flecs::world& world) -> void;

} // namespace projectv::ecs::vulkan
```

### Диаграмма фаз

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ECS System Phases                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  PreUpdate     OnUpdate      PreStore       OnStore       PostStore     │
│  ─────────     ─────────     ─────────      ─────────     ─────────     │
│                                                                          │
│  Input         Game          Prepare        Record        Sync          │
│  AI            Logic         Cull           Compute       Present       │
│                              Compute                                     │
│                                                                          │
│  Phase:        Phase:        Phase:         Phase:         Phase:       │
│  PREPARE       COMPUTE       CULL+COMPUTE   RECORD         SYNC+PRESENT │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Bindless Descriptor Management

```cpp
// ProjectV.ECS.Vulkan.Bindless.cppm
export module ProjectV.ECS.Vulkan.Bindless;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::vulkan {

/// Manager для bindless descriptor arrays.
export class BindlessDescriptorManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        uint32_t max_textures = 4096,
        uint32_t max_buffers = 1024
    ) noexcept -> std::expected<BindlessDescriptorManager, VulkanError>;

    ~BindlessDescriptorManager() noexcept;

    BindlessDescriptorManager(BindlessDescriptorManager&&) noexcept;
    BindlessDescriptorManager& operator=(BindlessDescriptorManager&&) noexcept;
    BindlessDescriptorManager(const BindlessDescriptorManager&) = delete;
    BindlessDescriptorManager& operator=(const BindlessDescriptorManager&) = delete;

    /// Выделяет слот для текстуры.
    [[nodiscard]] auto allocate_texture_slot() noexcept -> std::expected<uint32_t, VulkanError>;

    /// Освобождает слот текстуры.
    auto free_texture_slot(uint32_t slot) noexcept -> void;

    /// Обновляет текстуру в слоте.
    auto update_texture(uint32_t slot, VkImageView view) noexcept -> void;

    /// Выделяет слот для буфера.
    [[nodiscard]] auto allocate_buffer_slot() noexcept -> std::expected<uint32_t, VulkanError>;

    /// Освобождает слот буфера.
    auto free_buffer_slot(uint32_t slot) noexcept -> void;

    /// Обновляет буфер в слоте.
    auto update_buffer(uint32_t slot, VkDeviceAddress address) noexcept -> void;

    /// Получает descriptor buffer.
    [[nodiscard]] auto descriptor_buffer() const noexcept -> VkBuffer;

    /// Получает device address descriptor buffer.
    [[nodiscard]] auto descriptor_buffer_address() const noexcept -> VkDeviceAddress;

private:
    BindlessDescriptorManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs::vulkan
```

---

## GPU-Driven Rendering

```cpp
// ProjectV.ECS.Vulkan.GPUDriven.cppm
export module ProjectV.ECS.Vulkan.GPUDriven;

import std;
import glm;
import ProjectV.Render.Vulkan;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::vulkan {

/// Компонент для GPU-driven indirect draw.
export struct IndirectDrawComponent {
    VkBuffer indirect_buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    uint32_t draw_count{0};
    bool needs_update{true};
};

/// Компонент для GPU culling.
export struct GPUCullingComponent {
    VkBuffer visible_buffer{VK_NULL_HANDLE};     ///< Bitmask visible chunks
    VkBuffer draw_command_buffer{VK_NULL_HANDLE}; ///< Output draw commands
    uint32_t chunk_count{0};
    bool needs_cull{true};
};

/// Система GPU-driven culling.
export class GPUCullingSystem {
public:
    /// Создаёт систему.
    [[nodiscard]] static auto create(
        VkDevice device,
        VkPipeline cull_pipeline
    ) noexcept -> std::expected<GPUCullingSystem, VulkanError>;

    /// Выполняет GPU culling.
    auto dispatch(
        VkCommandBuffer cmd,
        GPUCullingComponent& culling,
        const glm::mat4& view_proj
    ) noexcept -> void;

private:
    GPUCullingSystem() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs::vulkan
```

---

## Voxel Components для ECS

```cpp
// ProjectV.ECS.Voxel.Components.cppm
export module ProjectV.ECS.Voxel.Components;

import std;
import glm;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::voxel {

/// Компонент воксельного чанка.
export struct VoxelChunkComponent {
    VkBufferComponent voxel_buffer;
    VkBufferComponent indirect_buffer;

    uint32_t chunk_x{0}, chunk_y{0}, chunk_z{0};
    uint32_t lod_level{0};
    bool is_visible{true};
    bool needs_remesh{true};

    uint32_t triangle_count{0};
};

/// Компонент материала вокселя.
export struct VoxelMaterialComponent {
    glm::vec4 base_color{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emission_strength{0.0f};

    uint32_t albedo_texture_index{0};
    uint32_t normal_texture_index{0};

    bool is_transparent{false};
    bool needs_descriptor_update{true};
};

/// Компонент трансформации.
export struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 world_matrix{1.0f};
    bool dirty{true};
};

/// Компонент состояния рендеринга.
export struct RenderStateComponent {
    bool is_visible{true};
    bool casts_shadow{true};
    bool receives_shadow{true};
    float distance_to_camera{0.0f};
};

} // namespace projectv::ecs::voxel
```

---

## Texture Atlas

```cpp
// ProjectV.ECS.Vulkan.TextureAtlas.cppm
export module ProjectV.ECS.Vulkan.TextureAtlas;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.ECS.Vulkan.Components;

export namespace projectv::ecs::vulkan {

/// Компонент texture atlas.
export struct TextureAtlasComponent {
    VkImageComponent atlas_image;

    uint32_t atlas_width{4096};
    uint32_t atlas_height{4096};
    uint32_t tile_size{64};

    struct Tile {
        uint32_t x, y;
        bool allocated{false};
        uint64_t last_used_frame{0};
    };
    std::vector<Tile> tiles;

    std::queue<uint32_t> upload_queue;  ///< Texture IDs to upload
};

/// Manager texture atlas.
export class TextureAtlasManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        uint32_t width = 4096,
        uint32_t height = 4096,
        uint32_t tile_size = 64
    ) noexcept -> std::expected<TextureAtlasManager, VulkanError>;

    ~TextureAtlasManager() noexcept;

    TextureAtlasManager(TextureAtlasManager&&) noexcept;
    TextureAtlasManager& operator=(TextureAtlasManager&&) noexcept;
    TextureAtlasManager(const TextureAtlasManager&) = delete;
    TextureAtlasManager& operator=(const TextureAtlasManager&) = delete;

    /// Выделяет тайл.
    [[nodiscard]] auto allocate_tile() noexcept -> std::expected<std::pair<uint32_t, uint32_t>, VulkanError>;

    /// Освобождает тайл.
    auto free_tile(uint32_t x, uint32_t y) noexcept -> void;

    /// Загружает текстуру в тайл.
    auto upload_to_tile(
        uint32_t tile_x,
        uint32_t tile_y,
        const void* data,
        uint32_t width,
        uint32_t height
    ) noexcept -> void;

    /// Получает atlas компонент.
    [[nodiscard]] auto atlas() const noexcept -> const TextureAtlasComponent&;

private:
    TextureAtlasManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs::vulkan
```

---

## Timeline Semaphore Sync

```cpp
// ProjectV.ECS.Vulkan.Sync.cppm
export module ProjectV.ECS.Vulkan.Sync;

import std;
import ProjectV.Render.Vulkan;

export namespace projectv::ecs::vulkan {

/// Manager timeline semaphores.
export class TimelineSemaphoreManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        uint32_t graphics_queue_family,
        uint32_t compute_queue_family
    ) noexcept -> std::expected<TimelineSemaphoreManager, VulkanError>;

    ~TimelineSemaphoreManager() noexcept;

    TimelineSemaphoreManager(TimelineSemaphoreManager&&) noexcept;
    TimelineSemaphoreManager& operator=(TimelineSemaphoreManager&&) noexcept;
    TimelineSemaphoreManager(const TimelineSemaphoreManager&) = delete;
    TimelineSemaphoreManager& operator=(const TimelineSemaphoreManager&) = delete;

    /// Получает compute timeline semaphore.
    [[nodiscard]] auto compute_semaphore() const noexcept -> VkSemaphore;

    /// Получает graphics timeline semaphore.
    [[nodiscard]] auto graphics_semaphore() const noexcept -> VkSemaphore;

    /// Получает текущее значение compute timeline.
    [[nodiscard]] auto compute_timeline_value() const noexcept -> uint64_t;

    /// Получает текущее значение graphics timeline.
    [[nodiscard]] auto graphics_timeline_value() const noexcept -> uint64_t;

    /// Увеличивает compute timeline.
    auto increment_compute() noexcept -> uint64_t;

    /// Увеличивает graphics timeline.
    auto increment_graphics() noexcept -> uint64_t;

    /// Ждёт завершения compute.
    auto wait_compute(uint64_t value) noexcept -> void;

    /// Ждёт завершения graphics.
    auto wait_graphics(uint64_t value) noexcept -> void;

private:
    TimelineSemaphoreManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs::vulkan
```

---

## Диаграмма Sync

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Timeline Semaphore Synchronization                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Compute Queue                     Graphics Queue                        │
│  ─────────────                     ──────────────                        │
│                                                                          │
│  ┌─────────────┐                   ┌─────────────┐                      │
│  │ Compute     │                   │ Graphics    │                      │
│  │ Dispatch    │                   │ Rendering   │                      │
│  └──────┬──────┘                   └──────▲──────┘                      │
│         │                                 │                              │
│         │ signal(value=N)                 │ wait(value=N)               │
│         ▼                                 │                              │
│  ┌─────────────────────────────────────────────────────────┐            │
│  │           Timeline Semaphore (compute→graphics)          │            │
│  └─────────────────────────────────────────────────────────┘            │
│                                                                          │
│  Timeline Values:                                                        │
│  ────────────────                                                        │
│  Frame 1: compute=1, graphics=1                                         │
│  Frame 2: compute=2, graphics=2                                         │
│  Frame N: compute=N, graphics=N                                         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Критерии реализации

### Обязательные требования

- [ ] VkBufferComponent, VkImageComponent, VkPipelineComponent
- [ ] Buffer/Image Creation/Destruction Observers
- [ ] ThreadCommandContext для многопоточности
- [ ] System phases: Prepare → Cull → Record → Present

### Опциональные (GPU-dependent)

- [ ] BindlessDescriptorManager
- [ ] GPU Culling System
- [ ] Timeline Semaphore Sync
- [ ] Texture Atlas Manager

---

## Метрики производительности

| Операция              | Целевое время |
|-----------------------|---------------|
| Observer trigger      | < 0.1 ms      |
| Thread context switch | < 0.01 ms     |
| Descriptor allocation | < 0.1 ms      |
| Timeline wait         | < 1 ms        |
