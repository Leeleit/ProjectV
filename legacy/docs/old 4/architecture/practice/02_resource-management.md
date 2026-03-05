# Спецификация управления ресурсами ProjectV

**Статус:** Утверждено
**Версия:** 2.0
**Дата:** 2026-02-22

---

## Обзор

ProjectV обрабатывает огромные объёмы ресурсов:

- **Геометрия**: Миллионы вокселей, чанки, меши
- **Текстуры**: Атласы текстур для вокселей, материалы
- **Буферы**: GPU буферы для compute шейдеров
- **Шейдеры**: Compute и graphics шейдеры

Архитектура основана на **RAII**, **Reference Counting** и **Type Erasure** с полной изоляцией через **PIMPL**.

---

## Интерфейсы ResourceManager

```cpp
// ProjectV.Resource.Manager.cppm
export module ProjectV.Resource.Manager;

import std;
import ProjectV.Render.Vulkan;

export namespace projectv::resource {

/// Type-erased handle (32-bit ID + 16-bit generation + type).
export struct ResourceHandle {
    uint32_t id : 20;         ///< Индекс в массиве (до 1M ресурсов)
    uint16_t generation : 12; ///< Generation для use-after-free detection
    uint8_t type;             ///< Тип ресурса

    [[nodiscard]] auto valid() const noexcept -> bool {
        return id != 0 || type != 0;
    }

    [[nodiscard]] auto operator==(ResourceHandle const& other) const noexcept -> bool = default;
};

/// Типы ресурсов.
export enum class ResourceType : uint8_t {
    Unknown = 0,
    VulkanBuffer,
    VulkanImage,
    VulkanPipeline,
    VulkanDescriptorSet,
    GLTFModel,
    ShaderModule,
    Texture2D,
    TextureArray,
    Material,
    Mesh,
    AudioClip,
    ConfigFile
};

/// Состояние загрузки ресурса.
export enum class LoadState : uint8_t {
    NotLoaded,
    Loading,
    Loaded,
    Failed,
    Unloading
};

/// Приоритет загрузки.
export enum class LoadPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/// Коды ошибок ResourceManager.
export enum class ResourceError : uint8_t {
    NotFound,
    AlreadyExists,
    LoadFailed,
    InvalidHandle,
    InvalidType,
    OutOfMemory,
    StillInUse,
    DependencyFailed
};

/// Конфигурация ResourceManager.
export struct ResourceManagerConfig {
    size_t max_resources{65536};
    size_t gpu_budget_mb{2048};
    size_t cpu_budget_mb{512};
    bool enable_hot_reload{false};
    bool enable_defragmentation{true};
};

/// Метаданные ресурса.
export struct ResourceMetadata {
    ResourceType type{ResourceType::Unknown};
    LoadState load_state{LoadState::NotLoaded};
    std::string name;
    size_t memory_usage{0};
    uint32_t ref_count{0};
    uint64_t last_access_frame{0};
};

/// Главный менеджер ресурсов (PIMPL).
export class ResourceManager {
public:
    /// Создаёт ResourceManager.
    [[nodiscard]] static auto create(
        ResourceManagerConfig const& config = {},
        VkDevice device = VK_NULL_HANDLE,
        VmaAllocator allocator = nullptr
    ) noexcept -> std::expected<ResourceManager, ResourceError>;

    ~ResourceManager() noexcept;

    ResourceManager(ResourceManager&&) noexcept;
    ResourceManager& operator=(ResourceManager&&) noexcept;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // ========== Loading API ==========

    /// Загружает ресурс из файла.
    /// @param path Путь к файлу
    /// @param type Тип ресурса
    /// @return Handle ресурса или ошибка
    [[nodiscard]] auto load(
        std::string_view path,
        ResourceType type
    ) noexcept -> std::expected<ResourceHandle, ResourceError>;

    /// Асинхронная загрузка ресурса.
    [[nodiscard]] auto load_async(
        std::string_view path,
        ResourceType type,
        LoadPriority priority = LoadPriority::Normal
    ) noexcept -> std::future<std::expected<ResourceHandle, ResourceError>>;

    /// Получает ресурс по имени.
    [[nodiscard]] auto get(std::string_view name) const noexcept
        -> std::expected<ResourceHandle, ResourceError>;

    /// Проверяет валидность handle.
    [[nodiscard]] auto is_valid(ResourceHandle handle) const noexcept -> bool;

    // ========== Reference Counting ==========

    /// Увеличивает refCount.
    auto add_ref(ResourceHandle handle) noexcept -> void;

    /// Уменьшает refCount, освобождает при 0.
    auto release(ResourceHandle handle) noexcept -> void;

    /// Получает текущий refCount.
    [[nodiscard]] auto ref_count(ResourceHandle handle) const noexcept -> uint32_t;

    // ========== Memory Management ==========

    /// Получает использование памяти.
    [[nodiscard]] auto memory_usage() const noexcept -> size_t;

    /// Получает использование GPU памяти.
    [[nodiscard]] auto gpu_memory_usage() const noexcept -> size_t;

    /// Запускает garbage collection.
    auto garbage_collect() noexcept -> void;

    /// Освобождает неиспользуемые ресурсы по LRU.
    auto trim_cache(size_t target_bytes) noexcept -> size_t;

    // ========== Vulkan Access ==========

    /// Получает VkBuffer по handle.
    [[nodiscard]] auto get_buffer(ResourceHandle handle) const noexcept
        -> std::expected<VkBuffer, ResourceError>;

    /// Получает VkImage по handle.
    [[nodiscard]] auto get_image(ResourceHandle handle) const noexcept
        -> std::expected<VkImage, ResourceError>;

    /// Получает метаданные ресурса.
    [[nodiscard]] auto get_metadata(ResourceHandle handle) const noexcept
        -> std::expected<ResourceMetadata, ResourceError>;

    // ========== Hot Reload ==========

    /// Включает слежение за изменениями файла.
    auto watch_for_changes(ResourceHandle handle) noexcept -> void;

    /// Перезагружает изменённые ресурсы.
    auto reload_changed() noexcept -> std::vector<ResourceHandle>;

    /// Принудительно перезагружает ресурс.
    [[nodiscard]] auto reload(ResourceHandle handle) noexcept
        -> std::expected<void, ResourceError>;

private:
    ResourceManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::resource
```

---

## Интерфейсы Buffer Management

```cpp
// ProjectV.Resource.Buffer.cppm
export module ProjectV.Resource.Buffer;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.Resource.Manager;

export namespace projectv::resource {

/// Конфигурация буфера.
export struct BufferConfig {
    VkDeviceSize size{0};
    VkBufferUsageFlags usage{0};
    VmaMemoryUsage memory_usage{VMA_MEMORY_USAGE_AUTO};
    VmaAllocationCreateFlags alloc_flags{0};
    bool mapped{false};
};

/// Builder для создания буферов.
export class BufferBuilder {
public:
    explicit BufferBuilder(ResourceManager& manager) noexcept;

    /// Устанавливает размер буфера.
    auto set_size(VkDeviceSize size) noexcept -> BufferBuilder&;

    /// Устанавливает usage flags.
    auto set_usage(VkBufferUsageFlags usage) noexcept -> BufferBuilder&;

    /// Устанавливает memory usage.
    auto set_memory_usage(VmaMemoryUsage usage) noexcept -> BufferBuilder&;

    /// Включает persistent mapping.
    auto set_mapped(bool mapped = true) noexcept -> BufferBuilder&;

    /// Устанавливает имя для отладки.
    auto set_name(std::string_view name) noexcept -> BufferBuilder&;

    /// Строит буфер.
    [[nodiscard]] auto build() noexcept -> std::expected<ResourceHandle, ResourceError>;

private:
    ResourceManager& manager_;
    BufferConfig config_;
    std::string name_;
};

/// Утилиты для работы с буферами.
export class BufferUtils {
public:
    /// Копирует данные в буфер (staging).
    [[nodiscard]] static auto upload(
        ResourceManager& manager,
        ResourceHandle buffer,
        void const* data,
        VkDeviceSize size,
        VkDeviceSize offset = 0
    ) noexcept -> std::expected<void, ResourceError>;

    /// Копирует данные из буфера.
    [[nodiscard]] static auto download(
        ResourceManager& manager,
        ResourceHandle buffer,
        void* data,
        VkDeviceSize size,
        VkDeviceSize offset = 0
    ) noexcept -> std::expected<void, ResourceError>;

    /// Получает mapped pointer.
    [[nodiscard]] static auto map(
        ResourceManager& manager,
        ResourceHandle buffer
    ) noexcept -> std::expected<void*, ResourceError>;

    /// Освобождает mapped pointer.
    static auto unmap(
        ResourceManager& manager,
        ResourceHandle buffer
    ) noexcept -> void;
};

} // namespace projectv::resource
```

---

## Интерфейсы Image Management

```cpp
// ProjectV.Resource.Image.cppm
export module ProjectV.Resource.Image;

import std;
import glm;
import ProjectV.Render.Vulkan;
import ProjectV.Resource.Manager;

export namespace projectv::resource {

/// Формат текстуры.
export struct TextureFormat {
    VkFormat format{VK_FORMAT_R8G8B8A8_SRGB};
    VkImageType image_type{VK_IMAGE_TYPE_2D};
    VkImageViewType view_type{VK_IMAGE_VIEW_TYPE_2D};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
};

/// Конфигурация изображения.
export struct ImageConfig {
    uint32_t width{1};
    uint32_t height{1};
    uint32_t depth{1};
    TextureFormat format;
    VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT};
    VmaMemoryUsage memory_usage{VMA_MEMORY_USAGE_GPU_ONLY};
    VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

/// Builder для создания изображений.
export class ImageBuilder {
public:
    explicit ImageBuilder(ResourceManager& manager) noexcept;

    /// Устанавливает размеры.
    auto set_extent(uint32_t width, uint32_t height, uint32_t depth = 1) noexcept -> ImageBuilder&;

    /// Устанавливает формат.
    auto set_format(VkFormat format) noexcept -> ImageBuilder&;

    /// Устанавливает mip levels.
    auto set_mip_levels(uint32_t levels) noexcept -> ImageBuilder&;

    /// Устанавливает usage flags.
    auto set_usage(VkImageUsageFlags usage) noexcept -> ImageBuilder&;

    /// Устанавливает имя.
    auto set_name(std::string_view name) noexcept -> ImageBuilder&;

    /// Строит изображение.
    [[nodiscard]] auto build() noexcept -> std::expected<ResourceHandle, ResourceError>;

private:
    ResourceManager& manager_;
    ImageConfig config_;
    std::string name_;
};

/// Утилиты для работы с изображениями.
export class ImageUtils {
public:
    /// Загружает текстуру из файла.
    [[nodiscard]] static auto load_from_file(
        ResourceManager& manager,
        std::string_view path
    ) noexcept -> std::expected<ResourceHandle, ResourceError>;

    /// Загружает данные в изображение.
    [[nodiscard]] static auto upload(
        ResourceManager& manager,
        ResourceHandle image,
        void const* data,
        VkDeviceSize size
    ) noexcept -> std::expected<void, ResourceError>;

    /// Генерирует mip maps.
    static auto generate_mips(
        ResourceManager& manager,
        ResourceHandle image,
        VkCommandBuffer cmd
    ) noexcept -> void;

    /// Переходит в layout.
    static auto transition_layout(
        ResourceManager& manager,
        ResourceHandle image,
        VkCommandBuffer cmd,
        VkImageLayout new_layout,
        VkPipelineStageFlags2 src_stage,
        VkPipelineStageFlags2 dst_stage
    ) noexcept -> void;
};

} // namespace projectv::resource
```

---

## Интерфейсы Texture Atlas

```cpp
// ProjectV.Resource.Atlas.cppm
export module ProjectV.Resource.Atlas;

import std;
import glm;
import ProjectV.Resource.Manager;
import ProjectV.Resource.Image;

export namespace projectv::resource {

/// Регион в атласе.
export struct AtlasRegion {
    glm::vec2 uv_min{0.0f};
    glm::vec2 uv_max{1.0f};
    uint32_t texture_index{0};
    uint16_t width{0};
    uint16_t height{0};
};

/// Менеджер texture atlas.
export class TextureAtlasManager {
public:
    /// Создаёт atlas manager.
    [[nodiscard]] static auto create(
        ResourceManager& manager,
        uint32_t atlas_size = 4096,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
    ) noexcept -> std::expected<TextureAtlasManager, ResourceError>;

    ~TextureAtlasManager() noexcept;

    TextureAtlasManager(TextureAtlasManager&&) noexcept;
    TextureAtlasManager& operator=(TextureAtlasManager&&) noexcept;
    TextureAtlasManager(const TextureAtlasManager&) = delete;
    TextureAtlasManager& operator=(const TextureAtlasManager&) = delete;

    /// Добавляет текстуру в атлас.
    /// @param name Имя текстуры
    /// @param data Данные текстуры (RGBA8)
    /// @param width Ширина
    /// @param height Высота
    /// @return Регион в атласе
    [[nodiscard]] auto add_texture(
        std::string_view name,
        std::span<uint8_t const> data,
        uint16_t width,
        uint16_t height
    ) noexcept -> std::expected<AtlasRegion, ResourceError>;

    /// Получает регион по имени.
    [[nodiscard]] auto get_region(std::string_view name) const noexcept
        -> std::expected<AtlasRegion, ResourceError>;

    /// Получает handle атласа.
    [[nodiscard]] auto atlas_handle() const noexcept -> ResourceHandle;

    /// Получает количество текстур.
    [[nodiscard]] auto texture_count() const noexcept -> size_t;

    /// Выводит текстуры из атласа (returns memory).
    auto remove_texture(std::string_view name) noexcept -> void;

private:
    TextureAtlasManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::resource
```

---

## Интерфейсы GLTF Loading

```cpp
// ProjectV.Resource.GLTF.cppm
export module ProjectV.Resource.GLTF;

import std;
import glm;
import ProjectV.Resource.Manager;
import ProjectV.Resource.Buffer;

export namespace projectv::resource {

/// Вершина GLTF модели.
export struct GLTFVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 tex_coord{0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color{1.0f};
};

/// Submesh данные.
export struct SubmeshData {
    ResourceHandle vertex_buffer;
    ResourceHandle index_buffer;
    uint32_t vertex_count{0};
    uint32_t index_count{0};
    uint32_t material_index{0};
    glm::vec3 bounds_min{};
    glm::vec3 bounds_max{};
};

/// Материал GLTF.
export struct GLTFMaterial {
    ResourceHandle albedo_texture;
    ResourceHandle normal_texture;
    ResourceHandle metallic_roughness_texture;
    ResourceHandle occlusion_texture;
    ResourceHandle emissive_texture;
    glm::vec4 base_color_factor{1.0f};
    glm::vec3 emissive_factor{0.0f};
    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    float alpha_cutoff{0.5f};
    uint32_t alpha_mode{0}; // 0 = Opaque, 1 = Mask, 2 = Blend
};

/// Данные GLTF модели.
export class GLTFModelData {
public:
    [[nodiscard]] auto submeshes() const noexcept -> std::span<SubmeshData const>;
    [[nodiscard]] auto materials() const noexcept -> std::span<GLTFMaterial const>;
    [[nodiscard]] auto node_count() const noexcept -> size_t;
    [[nodiscard]] auto total_vertex_count() const noexcept -> size_t;
    [[nodiscard]] auto total_index_count() const noexcept -> size_t;
    [[nodiscard]] auto bounds_min() const noexcept -> glm::vec3;
    [[nodiscard]] auto bounds_max() const noexcept -> glm::vec3;

private:
    friend class GLTFLoader;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Загрузчик GLTF моделей.
export class GLTFLoader {
public:
    /// Загружает GLTF модель.
    [[nodiscard]] static auto load(
        ResourceManager& manager,
        std::string_view path
    ) noexcept -> std::expected<ResourceHandle, ResourceError>;

    /// Загружает GLTF с custom vertex processing.
    template<typename VertexProcessor>
    [[nodiscard]] static auto load_with_processor(
        ResourceManager& manager,
        std::string_view path,
        VertexProcessor&& processor
    ) noexcept -> std::expected<ResourceHandle, ResourceError>;

    /// Получает данные модели.
    [[nodiscard]] static auto get_data(
        ResourceManager& manager,
        ResourceHandle handle
    ) noexcept -> GLTFModelData const*;
};

} // namespace projectv::resource
```

---

## Интеграция с ECS

```cpp
// ProjectV.Resource.ECS.cppm
export module ProjectV.Resource.ECS;

import std;
import ProjectV.Resource.Manager;
import ProjectV.ECS.Flecs;

export namespace projectv::resource {

/// Компонент mesh ресурса.
export struct MeshResourceComponent {
    ResourceHandle mesh_handle;
    uint32_t submesh_index{0};
    glm::mat4 local_transform{1.0f};

    // Cached for quick access
    mutable VkBuffer vertex_buffer{VK_NULL_HANDLE};
    mutable VkBuffer index_buffer{VK_NULL_HANDLE};
    mutable uint32_t index_count{0};
};

/// Компонент материала.
export struct MaterialComponent {
    ResourceHandle material_handle;
    glm::vec4 tint{1.0f};
    float roughness_override{-1.0f}; // -1 = use material default
    float metallic_override{-1.0f};
};

/// Компонент текстуры.
export struct TextureComponent {
    ResourceHandle texture_handle;
    glm::vec4 tintColor{1.0f};
    uint32_t bindless_index{UINT32_MAX};
};

/// Система управления ресурсами в ECS.
export class ResourceECSSystem {
public:
    /// Регистрирует системы и observers.
    static auto register_systems(ecs::World& world, ResourceManager& manager) noexcept -> void;

private:
    /// Observer для add_ref при добавлении компонента.
    static auto on_mesh_added(ecs::World& world, ResourceManager& manager) noexcept -> void;

    /// Observer для release при удалении компонента.
    static auto on_mesh_removed(ecs::World& world, ResourceManager& manager) noexcept -> void;

    /// Система обновления кэша.
    static auto update_cache(ecs::World& world, ResourceManager& manager) noexcept -> void;
};

} // namespace projectv::resource
```

---

## Memory Pooling

```cpp
// ProjectV.Resource.Pool.cppm
export module ProjectV.Resource.Pool;

import std;
import ProjectV.Resource.Manager;
import ProjectV.Resource.Buffer;

export namespace projectv::resource {

/// Пул буферов одного размера.
export class BufferPool {
public:
    /// Создаёт пул буферов.
    [[nodiscard]] static auto create(
        ResourceManager& manager,
        VkDeviceSize buffer_size,
        VkBufferUsageFlags usage,
        uint32_t initial_count = 4
    ) noexcept -> std::expected<BufferPool, ResourceError>;

    ~BufferPool() noexcept;

    BufferPool(BufferPool&&) noexcept;
    BufferPool& operator=(BufferPool&&) noexcept;
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    /// Получает буфер из пула.
    [[nodiscard]] auto acquire() noexcept -> std::expected<ResourceHandle, ResourceError>;

    /// Возвращает буфер в пул.
    auto release(ResourceHandle handle) noexcept -> void;

    /// Получает статистику пула.
    [[nodiscard]] auto available_count() const noexcept -> uint32_t;
    [[nodiscard]] auto total_count() const noexcept -> uint32_t;

private:
    BufferPool() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Менеджер пулов памяти.
export class MemoryPoolManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(ResourceManager& resource_manager) noexcept
        -> std::expected<MemoryPoolManager, ResourceError>;

    ~MemoryPoolManager() noexcept;

    MemoryPoolManager(MemoryPoolManager&&) noexcept;
    MemoryPoolManager& operator=(MemoryPoolManager&&) noexcept;
    MemoryPoolManager(const MemoryPoolManager&) = delete;
    MemoryPoolManager& operator=(const MemoryPoolManager&) = delete;

    /// Получает или создаёт пул для размера.
    [[nodiscard]] auto get_pool(
        VkDeviceSize size,
        VkBufferUsageFlags usage
    ) noexcept -> BufferPool*;

    /// Освобождает неиспользуемые пулы.
    auto trim() noexcept -> void;

    /// Получает общую статистику памяти.
    struct Stats {
        size_t total_allocated{0};
        size_t total_used{0};
        uint32_t pool_count{0};
        uint32_t buffer_count{0};
    };
    [[nodiscard]] auto stats() const noexcept -> Stats;

private:
    MemoryPoolManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::resource
```

---

## Статистика и профилирование

```cpp
// ProjectV.Resource.Stats.cppm
export module ProjectV.Resource.Stats;

import std;
import ProjectV.Resource.Manager;

export namespace projectv::resource {

/// Статистика памяти.
export struct MemoryStats {
    size_t gpu_total_bytes{0};
    size_t gpu_used_bytes{0};
    size_t cpu_total_bytes{0};
    size_t cpu_used_bytes{0};
    uint32_t buffer_count{0};
    uint32_t image_count{0};
    uint32_t total_resource_count{0};
};

/// Статистика загрузки.
export struct LoadingStats {
    uint32_t resources_loaded{0};
    uint32_t resources_failed{0};
    uint32_t cache_hits{0};
    uint32_t cache_misses{0};
    double total_load_time_ms{0.0};
    size_t bytes_uploaded{0};
};

/// Сборщик статистики ресурсов.
export class ResourceStats {
public:
    /// Собирает статистику памяти.
    [[nodiscard]] static auto collect_memory_stats(
        ResourceManager const& manager
    ) noexcept -> MemoryStats;

    /// Собирает статистику загрузки.
    [[nodiscard]] static auto collect_loading_stats(
        ResourceManager const& manager
    ) noexcept -> LoadingStats;

    /// Выводит статистику в лог.
    static auto log_stats(
        ResourceManager const& manager
    ) noexcept -> void;
};

} // namespace projectv::resource
```

---

## Диаграммы

### Жизненный цикл ресурса

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Resource Lifecycle                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐       │
│   │  Load    │────▶│  Loaded  │────▶│  In Use  │────▶│  Release │       │
│   │ (Async)  │     │          │     │ (ref>0)  │     │ (ref=0)  │       │
│   └──────────┘     └──────────┘     └──────────┘     └────┬─────┘       │
│        │                ▲                                 │              │
│        │                │                                 ▼              │
│        │                │           ┌──────────┐    ┌──────────┐        │
│        │                └───────────│  Cached  │◀───│  Pending │        │
│        │                            │ (LRU)    │    │  Delete  │        │
│        ▼                            └──────────┘    └──────────┘        │
│   ┌──────────┐                           │                              │
│   │  Failed  │                           ▼                              │
│   └──────────┘                     ┌──────────┐                         │
│                                    │ Destroyed │                        │
│                                    └──────────┘                         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Архитектура ResourceManager

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ResourceManager                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │ Load Queue  │  │ Resource    │  │   Cache     │  │   Memory    │    │
│  │ (Async)     │  │ Registry    │  │   (LRU)     │  │   Pools     │    │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘    │
│         │                │                │                │            │
│         ▼                ▼                ▼                ▼            │
│  ┌─────────────────────────────────────────────────────────────┐       │
│  │                    VMA Allocator (PIMPL)                     │       │
│  └─────────────────────────────────────────────────────────────┘       │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
             ┌──────────┐    ┌──────────┐    ┌──────────┐
             │ Buffers  │    │ Images   │    │ Models   │
             │(VkBuffer)│    │(VkImage) │    │(GLTF)    │
             └──────────┘    └──────────┘    └──────────┘
```

---

## Критерии успешной реализации

### Обязательные требования

- [ ] Единая точка управления всеми ресурсами
- [ ] Автоматический reference counting через ECS observers
- [ ] PIMPL изоляция VMA и Vulkan handles
- [ ] Thread-safe API для асинхронной загрузки
- [ ] Memory pooling для уменьшения fragmentation

### Метрики производительности

| Операция                   | Целевое время |
|----------------------------|---------------|
| Cache hit                  | < 0.1ms       |
| Buffer allocation (pooled) | < 0.5ms       |
| Image upload (1MB)         | < 5ms         |
| GLTF load (small)          | < 50ms        |

---

## Ссылки

- [ADR-0004: Build System & C++26 Modules](../adr/0004-build-and-modules-spec.md)
- [Engine Structure](./00_engine-structure.md)
- [Vulkan Specification](./04_vulkan_spec.md)
