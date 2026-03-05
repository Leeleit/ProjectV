# ADR-0001: Vulkan 1.4 Renderer Architecture

**Статус:** Принято
**Дата:** 2026-02-22
**Автор:** Architecture Team

---

## Контекст

ProjectV требует высокопроизводительного рендерера для воксельного мира с поддержкой:

- GPU-driven rendering (минимизация CPU-GPU синхронизации)
- Ray marching через SVO (Sparse Voxel Octree)
- Compute/Mesh Shaders для процедурной генерации геометрии
- Интеграции с Slang для шейдерной компиляции

## Решение

Принята архитектура на базе **Vulkan 1.4** с следующими ключевыми решениями:

### 1. Volk для статической загрузки Vulkan функций

```cpp
// Спецификация интерфейса
export module ProjectV.Render.VulkanLoader;

import std;

export namespace projectv::render {

/// RAII-обёртка над Volk-инициализацией.
/// Загружает Vulkan функции статически, без системного loader.
class VulkanLoader final {
public:
    /// Инициализирует Volk. Вызывает volkInitialize().
    /// @returns std::expected<void, LoaderError> — успех или код ошибки
    [[nodiscard]] static auto initialize() noexcept
        -> std::expected<void, LoaderError>;

    /// Загружает инстанс-специфичные функции.
    /// Вызывает volkLoadInstance(VkInstance).
    /// @param instance Валидный VkInstance
    static auto load_instance(VkInstance instance) noexcept -> void;

    /// Загружает device-специфичные функции.
    /// Вызывает volkLoadDevice(VkDevice).
    /// @param device Валидный VkDevice
    static auto load_device(VkDevice device) noexcept -> void;

    VulkanLoader() = delete;
    VulkanLoader(const VulkanLoader&) = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;
};

/// Коды ошибок загрузчика Vulkan
export enum class LoaderError : uint8_t {
    VolkInitializeFailed,    ///< volkInitialize() вернул ошибку
    VersionNotSupported,     ///< Требуется Vulkan 1.4+
    DriverNotAvailable       ///< Драйвер не найден
};

} // namespace projectv::render
```

### 2. VMA (Vulkan Memory Allocator) для управления памятью GPU

```cpp
// Спецификация интерфейса
export module ProjectV.Render.MemoryAllocator;

import std;
import vulkan; // Vulkan handles

export namespace projectv::render {

/// Параметры создания аллокатора
struct AllocatorCreateInfo {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t vulkan_api_version{VK_API_VERSION_1_4};
    bool prefer_device_local{true};  ///< Prefer VRAM over system RAM
};

/// RAII-обёртка над VmaAllocator.
/// Предоставляет типобезопасное управление GPU памятью.
class GPUAllocator final {
public:
    /// Создаёт аллокатор VMA.
    [[nodiscard]] static auto create(AllocatorCreateInfo const& create_info) noexcept
        -> std::expected<GPUAllocator, AllocatorError>;

    /// Разрушает аллокатор. Все выделенные буферы должны быть освобождены.
    ~GPUAllocator() noexcept;

    // Move-only
    GPUAllocator(GPUAllocator&& other) noexcept;
    GPUAllocator& operator=(GPUAllocator&& other) noexcept;
    GPUAllocator(const GPUAllocator&) = delete;
    GPUAllocator& operator=(const GPUAllocator&) = delete;

    /// Выделяет буфер с указанным использованием.
    /// @param size Размер в байтах
    /// @param usage Флаги использования буфера
    /// @param memory_usage Тип памяти (GPU_ONLY, CPU_TO_GPU, GPU_TO_CPU)
    [[nodiscard]] auto allocate_buffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage
    ) const noexcept -> std::expected<BufferAllocation, AllocationError>;

    /// Освобождает буфер.
    auto deallocate_buffer(BufferAllocation&& allocation) const noexcept -> void;

    /// Возвращает нативный дескриптор VmaAllocator.
    [[nodiscard]] auto native() const noexcept -> VmaAllocator { return allocator_; }

private:
    explicit GPUAllocator(VmaAllocator allocator) noexcept;
    VmaAllocator allocator_{VK_NULL_HANDLE};
};

/// Результат выделения буфера
struct BufferAllocation {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo info{};
    VkDeviceSize size{0};

    /// Мапит память в CPU-адресное пространство.
    [[nodiscard]] auto map() const noexcept -> std::expected<void*, MapError>;

    /// Размапивает память.
    auto unmap() const noexcept -> void;

    /// Копирует данные в буфер (для CPU-visible памяти).
    [[nodiscard]] auto write(std::span<const std::byte> data) const noexcept
        -> std::expected<void, MapError>;
};

/// Коды ошибок аллокации
export enum class AllocatorError : uint8_t {
    VmaCreateFailed,        ///< vmaCreateAllocator() failed
    InvalidInstance,        ///< VkInstance is VK_NULL_HANDLE
    InvalidDevice,          ///< VkDevice is VK_NULL_HANDLE
    InvalidPhysicalDevice   ///< VkPhysicalDevice is VK_NULL_HANDLE
};

export enum class AllocationError : uint8_t {
    OutOfMemory,            ///< GPU memory exhausted
    BufferCreateFailed,     ///< vkCreateBuffer() failed
    AllocationFailed,       ///< vmaAllocateMemoryForBuffer() failed
    BindFailed,             ///< vmaBindBufferMemory() failed
    InvalidSize             ///< size == 0
};

export enum class MapError : uint8_t {
    MemoryNotMappable,      ///< Memory type is not CPU-visible
    MapFailed               ///< vmaMapMemory() failed
};

} // namespace projectv::render
```

### 3. Device Management (Physical Device Selection + Logical Device)

```cpp
// Спецификация интерфейса
export module ProjectV.Render.Device;

import std;
import ProjectV.Render.VulkanLoader;
import ProjectV.Render.MemoryAllocator;

export namespace projectv::render {

/// Требования к физическому устройству
struct DeviceRequirements {
    bool require_discrete_gpu{true};         ///< Только дискретные GPU
    bool require_ray_tracing{false};         ///< NV_ray_tracing / KHR_ray_tracing_pipeline
    bool require_mesh_shaders{true};         ///< NV_mesh_shader / EXT_mesh_shader
    uint32_t min_vram_mb{4096};              ///< Минимум VRAM
    uint32_t min_api_version{VK_API_VERSION_1_4};
    std::vector<const char*> required_extensions;  ///< VK_KHR_* extensions
};

/// Информация о физическом устройстве
struct PhysicalDeviceInfo {
    VkPhysicalDevice device{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures2 features2{};
    VkPhysicalDeviceMemoryProperties memory{};
    std::vector<VkQueueFamilyProperties> queue_families;
    std::vector<VkExtensionProperties> extensions;
    uint64_t vram_size{0};
    bool supports_mesh_shaders{false};
    bool supports_ray_tracing{false};
    bool supports_descriptor_indexing{false};
};

/// Очереди устройства
struct DeviceQueues {
    uint32_t graphics_family{UINT32_MAX};
    uint32_t compute_family{UINT32_MAX};
    uint32_t transfer_family{UINT32_MAX};
    uint32_t present_family{UINT32_MAX};
    VkQueue graphics_queue{VK_NULL_HANDLE};
    VkQueue compute_queue{VK_NULL_HANDLE};
    VkQueue transfer_queue{VK_NULL_HANDLE};
    VkQueue present_queue{VK_NULL_HANDLE};
};

/// RAII-обёртка над VkDevice + VkPhysicalDevice
class RenderDevice final {
public:
    /// Создаёт рендер-устройство с указанными требованиями.
    /// Автоматически выбирает лучшее физическое устройство.
    [[nodiscard]] static auto create(
        VkInstance instance,
        VkSurfaceKHR surface,
        DeviceRequirements const& requirements
    ) noexcept -> std::expected<RenderDevice, DeviceError>;

    ~RenderDevice() noexcept;

    // Move-only
    RenderDevice(RenderDevice&& other) noexcept;
    RenderDevice& operator=(RenderDevice&& other) noexcept;
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    /// Возвращает нативный VkDevice.
    [[nodiscard]] auto device() const noexcept -> VkDevice { return device_; }

    /// Возвращает нативный VkPhysicalDevice.
    [[nodiscard]] auto physical_device() const noexcept -> VkPhysicalDevice { return physical_device_; }

    /// Возвращает очереди устройства.
    [[nodiscard]] auto queues() const noexcept -> DeviceQueues const& { return queues_; }

    /// Возвращает GPU аллокатор.
    [[nodiscard]] auto allocator() const noexcept -> GPUAllocator const& { return allocator_; }

    /// Возвращает информацию о физическом устройстве.
    [[nodiscard]] auto info() const noexcept -> PhysicalDeviceInfo const& { return device_info_; }

    /// Ожидает завершения работы GPU (vkDeviceWaitIdle).
    auto wait_idle() const noexcept -> void;

private:
    explicit RenderDevice(
        VkDevice device,
        VkPhysicalDevice physical_device,
        DeviceQueues queues,
        GPUAllocator allocator,
        PhysicalDeviceInfo device_info
    ) noexcept;

    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    DeviceQueues queues_;
    GPUAllocator allocator_;
    PhysicalDeviceInfo device_info_;
};

/// Коды ошибок устройства
export enum class DeviceError : uint8_t {
    NoSuitableDevice,          ///< Ни одно устройство не соответствует требованиям
    InstanceNotValid,          ///< VkInstance is VK_NULL_HANDLE
    SurfaceNotValid,           ///< VkSurfaceKHR is VK_NULL_HANDLE
    LogicalDeviceCreateFailed, ///< vkCreateDevice() failed
    QueueFamilyNotFound,       ///< Required queue family not available
    ExtensionNotSupported,     ///< Required extension not available
    AllocatorCreateFailed      ///< GPUAllocator::create() failed
};

} // namespace projectv::render
```

### 4. Command Buffer Management

```cpp
// Спецификация интерфейса
export module ProjectV.Render.CommandBuffer;

import std;
import ProjectV.Render.Device;

export namespace projectv::render {

/// Пул командных буферов
class CommandPool final {
public:
    /// Создает пул командных буферов.
    /// @param device Рендер-устройство
    /// @param queue_family_index Индекс семейства очередей
    /// @param transient true для кратковременных буферов (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)
    [[nodiscard]] static auto create(
        RenderDevice const& device,
        uint32_t queue_family_index,
        bool transient = false
    ) noexcept -> std::expected<CommandPool, CommandPoolError>;

    ~CommandPool() noexcept;

    // Move-only
    CommandPool(CommandPool&& other) noexcept;
    CommandPool& operator=(CommandPool&& other) noexcept;
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    /// Выделяет командный буфер.
    [[nodiscard]] auto allocate() const noexcept
        -> std::expected<CommandBuffer, CommandBufferError>;

    /// Выделяет несколько командных буферов.
    [[nodiscard]] auto allocate_n(uint32_t count) const noexcept
        -> std::expected<std::vector<CommandBuffer>, CommandBufferError>;

    /// Сбрасывает пул (все буферы возвращаются в пул).
    auto reset() const noexcept -> void;

    /// Возвращает нативный дескриптор.
    [[nodiscard]] auto native() const noexcept -> VkCommandPool { return pool_; }

private:
    explicit CommandPool(VkCommandPool pool, RenderDevice const* device) noexcept;
    VkCommandPool pool_{VK_NULL_HANDLE};
    RenderDevice const* device_{nullptr};
};

/// Командный буфер (RAII)
class CommandBuffer final {
public:
    ~CommandBuffer() noexcept;

    // Move-only
    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    /// Начинает запись команд.
    /// @param one_time_submit true если буфер будет сабмичен один раз
    auto begin(bool one_time_submit = false) const noexcept -> void;

    /// Заканчивает запись команд.
    auto end() const noexcept -> void;

    /// Сбрасывает буфер для повторного использования.
    auto reset() const noexcept -> void;

    /// Возвращает нативный дескриптор.
    [[nodiscard]] auto native() const noexcept -> VkCommandBuffer { return cmd_buffer_; }

private:
    friend class CommandPool;
    explicit CommandBuffer(VkCommandBuffer cmd, VkCommandPool pool, RenderDevice const* device) noexcept;
    VkCommandBuffer cmd_buffer_{VK_NULL_HANDLE};
    VkCommandPool pool_{VK_NULL_HANDLE};
    RenderDevice const* device_{nullptr};
};

/// Коды ошибок
export enum class CommandPoolError : uint8_t {
    CreateFailed,        ///< vkCreateCommandPool() failed
    InvalidDevice,       ///< device.native() == VK_NULL_HANDLE
    InvalidQueueFamily   ///< queue_family_index invalid
};

export enum class CommandBufferError : uint8_t {
    AllocateFailed,      ///< vkAllocateCommandBuffers() failed
    PoolNotValid         ///< pool.native() == VK_NULL_HANDLE
};

} // namespace projectv::render
```

### 5. Pipeline Cache и Shader Modules

```cpp
// Спецификация интерфейса
export module ProjectV.Render.Pipeline;

import std;
import ProjectV.Render.Device;

export namespace projectv::render {

/// Шейдерный модуль (SPIR-V)
class ShaderModule final {
public:
    /// Загружает шейдер из SPIR-V байткода.
    [[nodiscard]] static auto from_spirv(
        RenderDevice const& device,
        std::span<const uint32_t> spirv
    ) noexcept -> std::expected<ShaderModule, ShaderError>;

    /// Загружает шейдер из Slang-файла с компиляцией в SPIR-V.
    /// Требует интеграции с slangc.
    [[nodiscard]] static auto from_slang(
        RenderDevice const& device,
        std::filesystem::path const& slang_path,
        std::string_view entry_point = "main"
    ) noexcept -> std::expected<ShaderModule, ShaderError>;

    ~ShaderModule() noexcept;

    // Move-only
    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    /// Возвращает нативный дескриптор.
    [[nodiscard]] auto native() const noexcept -> VkShaderModule { return module_; }

    /// Возвращает стадию шейдера (VS, FS, CS, MS, AS, etc.)
    [[nodiscard]] auto stage() const noexcept -> VkShaderStageFlagBits { return stage_; }

private:
    explicit ShaderModule(VkShaderModule module, VkShaderStageFlagBits stage) noexcept;
    VkShaderModule module_{VK_NULL_HANDLE};
    VkShaderStageFlagBits stage_{};
};

/// Описание шейдерной стадии
struct PipelineShaderStage {
    ShaderModule const* shader{nullptr};
    std::string_view entry_point{"main"};
    VkSpecializationInfo const* specialization{nullptr};
};

/// Описание вертексного ввода (для graphics pipelines)
struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

/// Builder для graphics pipeline
class GraphicsPipelineBuilder final {
public:
    explicit GraphicsPipelineBuilder() = default;

    auto add_shader_stage(PipelineShaderStage stage) -> GraphicsPipelineBuilder&;
    auto set_vertex_input(VertexInputDescription desc) -> GraphicsPipelineBuilder&;
    auto set_input_assembly(VkPrimitiveTopology topology, bool primitive_restart = false) -> GraphicsPipelineBuilder&;
    auto set_rasterization(VkPolygonMode mode, VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT) -> GraphicsPipelineBuilder&;
    auto set_multisampling(VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) -> GraphicsPipelineBuilder&;
    auto set_depth_stencil(bool depth_test, bool depth_write, VkCompareOp depth_compare = VK_COMPARE_OP_LESS) -> GraphicsPipelineBuilder&;
    auto add_color_attachment(VkFormat format, bool blend = false) -> GraphicsPipelineBuilder&;
    auto set_layout(VkPipelineLayout layout) -> GraphicsPipelineBuilder&;
    auto set_render_pass(VkRenderPass render_pass, uint32_t subpass = 0) -> GraphicsPipelineBuilder&;

    /// Строит pipeline. В случае ошибки возвращает PipelineError.
    [[nodiscard]] auto build(RenderDevice const& device) const noexcept
        -> std::expected<VkPipeline, PipelineError>;

private:
    std::vector<PipelineShaderStage> shader_stages_;
    VertexInputDescription vertex_input_;
    VkPrimitiveTopology topology_{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    bool primitive_restart_{false};
    VkPolygonMode polygon_mode_{VK_POLYGON_MODE_FILL};
    VkCullModeFlags cull_mode_{VK_CULL_MODE_BACK_BIT};
    VkSampleCountFlagBits samples_{VK_SAMPLE_COUNT_1_BIT};
    bool depth_test_{true};
    bool depth_write_{true};
    VkCompareOp depth_compare_{VK_COMPARE_OP_LESS};
    std::vector<VkFormat> color_attachment_formats_;
    std::vector<bool> color_attachment_blend_;
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    uint32_t subpass_{0};
};

/// Builder для compute pipeline
class ComputePipelineBuilder final {
public:
    explicit ComputePipelineBuilder() = default;

    auto set_shader(ShaderModule const* shader, std::string_view entry = "main") -> ComputePipelineBuilder&;
    auto set_layout(VkPipelineLayout layout) -> ComputePipelineBuilder&;
    auto set_specialization(VkSpecializationInfo const* spec) -> ComputePipelineBuilder&;

    [[nodiscard]] auto build(RenderDevice const& device) const noexcept
        -> std::expected<VkPipeline, PipelineError>;

private:
    ShaderModule const* shader_{nullptr};
    std::string_view entry_point_{"main"};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkSpecializationInfo const* specialization_{nullptr};
};

/// Коды ошибок
export enum class ShaderError : uint8_t {
    SpirvInvalid,         ///< Invalid SPIR-V bytecode
    SpirvLoadFailed,      ///< Failed to load SPIR-V file
    SlangCompileFailed,   ///< slangc compilation failed
    ModuleCreateFailed,   ///< vkCreateShaderModule() failed
    InvalidStage          ///< Cannot determine shader stage
};

export enum class PipelineError : uint8_t {
    NoShaderStages,       ///< No shader stages provided
    InvalidLayout,        ///< layout == VK_NULL_HANDLE
    InvalidRenderPass,    ///< render_pass == VK_NULL_HANDLE (graphics only)
    CreateFailed,         ///< vkCreateGraphicsPipelines() / vkCreateComputePipelines() failed
    NoShaderModule        ///< shader == nullptr (compute only)
};

} // namespace projectv::render
```

---

## Статус

| Компонент         | Статус         | Приоритет |
|-------------------|----------------|-----------|
| VulkanLoader      | Специфицирован | P0        |
| GPUAllocator      | Специфицирован | P0        |
| RenderDevice      | Специфицирован | P0        |
| CommandBuffer     | Специфицирован | P1        |
| ShaderModule      | Специфицирован | P1        |
| Pipeline Builders | Специфицирован | P1        |

---

## Последствия

### Положительные:

- Строгая типизация через `std::expected` вместо исключений
- RAII для всех Vulkan ресурсов — автоматическая очистка
- Чёткое разделение ответственности между модулями
- Явные контракты через `[[nodiscard]]` и `noexcept`

### Риски:

- Требуется компилятор с поддержкой C++26 (`import std;`)
- Интеграция с Slang требует отдельного ADR для шейдерной системы

---

## Ссылки

- [ADR-0002: SVO Storage Architecture](./0002-svo-storage.md)
- [ADR-0004: Build System & C++26 Modules](./0004-build-and-modules-spec.md)
- [Slang Integration Spec](../../libraries/slang/slang_integration_spec.md)
