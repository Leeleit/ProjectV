# Спецификация Vulkan 1.4 Backend ProjectV

**Статус:** Утверждено
**Версия:** 3.0 (Enterprise)
**Дата:** 2026-02-22

---

## Обзор

ProjectV использует **Vulkan 1.4** с архитектурой **Dynamic Rendering** (без legacy VkRenderPass/VkFramebuffer).

### Требования Vulkan 1.4

| Фича                         | Core/Extension           | Применение                       |
|------------------------------|--------------------------|----------------------------------|
| Dynamic Rendering            | Core in 1.4              | Без VkRenderPass                 |
| Dynamic Rendering Local Read | Core in 1.4              | G-Buffer/Deferred без subpass'ов |
| Synchronization2             | Core in 1.4              | VkDependencyInfo barriers        |
| Timeline Semaphores          | Core in 1.4              | Async compute sync               |
| Push Descriptors             | Core in 1.4              | Без descriptor pools             |
| Buffer Device Address        | Core in 1.4              | Bindless rendering               |
| Mesh Shaders                 | VK_EXT_mesh_shader       | GPU geometry generation          |
| Descriptor Buffers           | VK_EXT_descriptor_buffer | Альтернатива push descriptors    |
| Sparse Resources             | VK_KHR_sparse_binding    | Большие воксельные миры          |

---

## Memory Layout

### VulkanContext

```
VulkanContext (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── instance_: VkInstance (8 bytes)                    │
│      ├── physical_device_: VkPhysicalDevice (8 bytes)       │
│      ├── device_: VkDevice (8 bytes)                        │
│      ├── allocator_: VmaAllocator (8 bytes)                 │
│      ├── graphics_queue_: VkQueue (8 bytes)                 │
│      ├── compute_queue_: VkQueue (8 bytes)                  │
│      ├── transfer_queue_: VkQueue (8 bytes)                 │
│      ├── queue_families_: QueueFamilyIndices (16 bytes)     │
│      ├── extensions_: DeviceExtensions (8 bytes)            │
│      └── properties_: VkPhysicalDeviceProperties (~256 B)   │
│  Total: 8 bytes (external) + ~320 bytes (internal)          │
└─────────────────────────────────────────────────────────────┘

QueueFamilyIndices
┌─────────────────────────────────────────────────────────────┐
│  graphics: uint32_t (4 bytes)                               │
│  compute: uint32_t (4 bytes)                                │
│  transfer: uint32_t (4 bytes)                               │
│  present: uint32_t (4 bytes)                                │
│  Total: 16 bytes                                            │
└─────────────────────────────────────────────────────────────┘

DeviceExtensions (bitfield)
┌─────────────────────────────────────────────────────────────┐
│  dynamic_rendering: 1 bit                                   │
│  mesh_shader: 1 bit                                         │
│  descriptor_buffer: 1 bit                                   │
│  timeline_semaphore: 1 bit                                  │
│  synchronization2: 1 bit                                    │
│  sparse_binding: 1 bit                                      │
│  shader_object: 1 bit                                       │
│  buffer_device_address: 1 bit                               │
│  padding: 8 bits                                            │
│  Total: 2 bytes (uint16_t)                                  │
└─────────────────────────────────────────────────────────────┘
```

### RenderingInfo

```
VkRenderingInfo (Vulkan 1.4)
┌─────────────────────────────────────────────────────────────┐
│  sType: VkStructureType (4 bytes)                           │
│  pNext: const void* (8 bytes)                               │
│  flags: VkRenderingFlags (4 bytes)                          │
│  renderArea: VkRect2D (16 bytes)                            │
│  layerCount: uint32_t (4 bytes)                             │
│  viewMask: uint32_t (4 bytes)                               │
│  colorAttachmentCount: uint32_t (4 bytes)                   │
│  pColorAttachments: VkRenderingAttachmentInfo* (8 bytes)    │
│  pDepthAttachment: VkRenderingAttachmentInfo* (8 bytes)     │
│  pStencilAttachment: VkRenderingAttachmentInfo* (8 bytes)   │
│  Total: 72 bytes                                            │
└─────────────────────────────────────────────────────────────┘

VkRenderingAttachmentInfo
┌─────────────────────────────────────────────────────────────┐
│  sType: VkStructureType (4 bytes)                           │
│  pNext: const void* (8 bytes)                               │
│  imageView: VkImageView (8 bytes)                           │
│  imageLayout: VkImageLayout (4 bytes)                       │
│  resolveMode: VkResolveModeFlagBits (4 bytes)               │
│  resolveImageView: VkImageView (8 bytes)                    │
│  resolveImageLayout: VkImageLayout (4 bytes)                │
│  loadOp: VkAttachmentLoadOp (4 bytes)                       │
│  storeOp: VkAttachmentStoreOp (4 bytes)                     │
│  clearValue: VkClearValue (16 bytes)                        │
│  Total: 64 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

### Synchronization2

```
VkDependencyInfo (Synchronization2)
┌─────────────────────────────────────────────────────────────┐
│  sType: VkStructureType (4 bytes)                           │
│  pNext: const void* (8 bytes)                               │
│  dependencyFlags: VkDependencyFlags (4 bytes)               │
│  memoryBarrierCount: uint32_t (4 bytes)                     │
│  pMemoryBarriers: VkMemoryBarrier2* (8 bytes)               │
│  bufferMemoryBarrierCount: uint32_t (4 bytes)               │
│  pBufferMemoryBarriers: VkBufferMemoryBarrier2* (8 bytes)   │
│  imageMemoryBarrierCount: uint32_t (4 bytes)                │
│  pImageMemoryBarriers: VkImageMemoryBarrier2* (8 bytes)     │
│  Total: 56 bytes                                            │
└─────────────────────────────────────────────────────────────┘

VkMemoryBarrier2
┌─────────────────────────────────────────────────────────────┐
│  sType: VkStructureType (4 bytes)                           │
│  pNext: const void* (8 bytes)                               │
│  srcStageMask: VkPipelineStageFlags2 (8 bytes)              │
│  srcAccessMask: VkAccessFlags2 (8 bytes)                    │
│  dstStageMask: VkPipelineStageFlags2 (8 bytes)              │
│  dstAccessMask: VkAccessFlags2 (8 bytes)                    │
│  Total: 48 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machines

### VulkanContext Lifecycle

```
         ┌────────────┐
         │   EMPTY    │ ←── Default constructed
         └─────┬──────┘
               │ create()
               ▼
         ┌────────────┐
         │  CREATED   │ ←── Instance + Device ready
         └─────┬──────┘
               │ window surface attached
               ▼
    ┌────────────────────┐
    │ SURFACE_ATTACHED   │ ←── Swapchain can be created
    └─────────┬──────────┘
              │ swapchain created
              ▼
    ┌────────────────────┐
    │     READY          │ ←── Rendering possible
    └─────────┬──────────┘
              │ wait_idle() + destroy
              ▼
    ┌────────────────────┐
    │    DESTROYED       │ ←── Terminal state
    └────────────────────┘
```

### Command Buffer Lifecycle

```
    ┌─────────────┐
    │  INVALID    │ ←── Initial/After free
    └──────┬──────┘
           │ allocate()
           ▼
    ┌─────────────┐
    │  ALLOCATED  │ ←── Ready for recording
    └──────┬──────┘
           │ begin()
           ▼
    ┌─────────────┐
    │  RECORDING  │ ←── Commands being recorded
    └──────┬──────┘
           │ end()
           ▼
    ┌─────────────┐
    │  EXECUTABLE │ ←── Ready for submit
    └──────┬──────┘
           │ submit()
           ▼
    ┌─────────────┐
    │   PENDING   │ ←── Submitted, awaiting completion
    └──────┬──────┘
           │ signal/fence
           ▼
    ┌─────────────┐
    │   COMPLETE  │ ←── Can be reset/reused
    └─────────────┘
```

### Timeline Semaphore State

```
         Value Timeline
         │
    100 ─┼───────────────────────● (CPU wait returns)
         │                      ╱
     80 ─┼─────────────────────● (GPU signals)
         │                    ╱
     60 ─┼───────────────────● (CPU signal)
         │                  ╱
     40 ─┼─────────────────● (GPU wait)
         │                ╱
     20 ─┼───────────────● (Initial value)
         │
      0 ─┼─────────────────────────────────► Time
         │  T1    T2    T3    T4    T5
```

---

## API Contracts

### VulkanContext

```cpp
// ProjectV.Render.Vulkan.Context.cppm
export module ProjectV.Render.Vulkan.Context;

import std;
import volk;

export namespace projectv::render::vulkan {

/// Конфигурация Vulkan контекста.
export struct VulkanConfig {
    std::string application_name{"ProjectV"};
    uint32_t api_version{VK_API_VERSION_1_4};
    bool enable_validation{true};
    bool enable_render_doc{false};
    bool prefer_discrete_gpu{true};
};

/// Поддерживаемые расширения устройства.
export struct DeviceExtensions {
    bool dynamic_rendering{false};
    bool mesh_shader{false};
    bool descriptor_buffer{false};
    bool timeline_semaphore{false};
    bool synchronization2{false};
    bool sparse_binding{false};
    bool shader_object{false};
    bool buffer_device_address{false};
};

/// Queue family indices.
export struct QueueFamilyIndices {
    uint32_t graphics{UINT32_MAX};
    uint32_t compute{UINT32_MAX};
    uint32_t transfer{UINT32_MAX};
    uint32_t present{UINT32_MAX};

    /// Проверяет полноту.
    ///
    /// @return true если все очереди найдены
    [[nodiscard]] auto is_complete() const noexcept -> bool;

    /// Проверяет наличие выделенной compute queue.
    [[nodiscard]] auto has_dedicated_compute() const noexcept -> bool;

    /// Проверяет наличие выделенной transfer queue.
    [[nodiscard]] auto has_dedicated_transfer() const noexcept -> bool;
};

/// Vulkan 1.4 контекст.
///
/// ## Vulkan Version Requirement
/// - Minimum: Vulkan 1.4
/// - Required features: dynamic_rendering, synchronization2, timeline_semaphore
///
/// ## Thread Safety
/// - Device creation/destruction: external synchronization
/// - Queue submission: external synchronization per queue
///
/// ## Invariants
/// - instance_ валиден после успешного create()
/// - device_ валиден после успешного create()
/// - allocator_ валиден после успешного create()
export class VulkanContext {
public:
    /// Создаёт Vulkan 1.4 контекст.
    ///
    /// @param config Конфигурация
    ///
    /// @pre volkInitialize() уже вызван
    /// @pre Vulkan 1.4 support available
    ///
    /// @post instance() != VK_NULL_HANDLE
    /// @post device() != VK_NULL_HANDLE
    /// @post allocator() != VK_NULL_HANDLE
    ///
    /// @return Контекст или ошибка
    [[nodiscard]] static auto create(VulkanConfig const& config = {}) noexcept
        -> std::expected<VulkanContext, VulkanError>;

    ~VulkanContext() noexcept;

    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /// Получает VkInstance.
    [[nodiscard]] auto instance() const noexcept -> VkInstance;

    /// Получает VkPhysicalDevice.
    [[nodiscard]] auto physical_device() const noexcept -> VkPhysicalDevice;

    /// Получает VkDevice.
    [[nodiscard]] auto device() const noexcept -> VkDevice;

    /// Получает VmaAllocator.
    [[nodiscard]] auto allocator() const noexcept -> VmaAllocator;

    /// Получает graphics queue.
    [[nodiscard]] auto graphics_queue() const noexcept -> VkQueue;

    /// Получает compute queue.
    [[nodiscard]] auto compute_queue() const noexcept -> VkQueue;

    /// Получает queue family indices.
    [[nodiscard]] auto queue_families() const noexcept -> QueueFamilyIndices const&;

    /// Получает поддерживаемые расширения.
    [[nodiscard]] auto extensions() const noexcept -> DeviceExtensions const&;

    /// Получает физические свойства устройства.
    [[nodiscard]] auto physical_properties() const noexcept -> VkPhysicalDeviceProperties const&;

    /// Ожидает завершения всех операций.
    auto wait_idle() const noexcept -> void;

private:
    VulkanContext() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

---

### Dynamic Rendering

```cpp
// ProjectV.Render.Vulkan.DynamicRendering.cppm
export module ProjectV.Render.Vulkan.DynamicRendering;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Attachment info для dynamic rendering.
///
/// ## Usage
/// Используется с vkCmdBeginRendering вместо legacy VkRenderPass.
export struct AttachmentInfo {
    VkImageView view{VK_NULL_HANDLE};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkAttachmentLoadOp load_op{VK_ATTACHMENT_LOAD_OP_CLEAR};
    VkAttachmentStoreOp store_op{VK_ATTACHMENT_STORE_OP_STORE};
    VkClearValue clear_value{};
};

/// Rendering info builder для vkCmdBeginRendering.
///
/// ## Vulkan 1.4
/// Dynamic Rendering — core feature, не требует расширения.
///
/// ## Invariants
/// - render_area не превышает attachment dimensions
/// - color_attachments не более maxColorAttachments
export class RenderingInfo {
public:
    RenderingInfo() noexcept;

    /// Устанавливает render area.
    ///
    /// @param area Область рендеринга
    /// @pre area.extent.width > 0 && area.extent.height > 0
    auto set_render_area(VkRect2D area) noexcept -> RenderingInfo&;

    /// Устанавливает render area из extent.
    auto set_render_area(VkExtent2D extent) noexcept -> RenderingInfo&;

    /// Добавляет color attachment.
    ///
    /// @param attachment Информация об attachment
    /// @pre attachment.view != VK_NULL_HANDLE
    /// @pre color_count < maxColorAttachments
    auto add_color_attachment(AttachmentInfo const& attachment) noexcept -> RenderingInfo&;

    /// Устанавливает depth attachment.
    auto set_depth_attachment(AttachmentInfo const& attachment) noexcept -> RenderingInfo&;

    /// Устанавливает stencil attachment.
    auto set_stencil_attachment(AttachmentInfo const& attachment) noexcept -> RenderingInfo&;

    /// Устанавливает layer count.
    auto set_layer_count(uint32_t layers) noexcept -> RenderingInfo&;

    /// Устанавливает view mask (multiview).
    auto set_view_mask(uint32_t mask) noexcept -> RenderingInfo&;

    /// Получает VkRenderingInfo для vkCmdBeginRendering.
    ///
    /// @pre Хотя бы один attachment добавлен
    [[nodiscard]] auto get() const noexcept -> VkRenderingInfo const*;

    /// Сбрасывает для переиспользования.
    auto reset() noexcept -> void;

private:
    VkRenderingInfo info_{};
    std::vector<VkRenderingAttachmentInfo> color_attachments_;
    VkRenderingAttachmentInfo depth_attachment_{};
    VkRenderingAttachmentInfo stencil_attachment_{};
};

/// RAII wrapper для begin/end rendering.
///
/// ## Usage
/// ```cpp
/// {
///     ScopedRendering scope(cmd, rendering_info);
///     // rendering commands here
/// } // vkCmdEndRendering called automatically
/// ```
export class ScopedRendering {
public:
    /// Начинает rendering pass.
    ///
    /// @param cmd Command buffer
    /// @param info Rendering info
    /// @pre cmd в recording state
    /// @post vkCmdBeginRendering вызван
    ScopedRendering(VkCommandBuffer cmd, RenderingInfo const& info) noexcept;

    /// Заканчивает rendering pass.
    ///
    /// @post vkCmdEndRendering вызван
    ~ScopedRendering() noexcept;

    ScopedRendering(const ScopedRendering&) = delete;
    ScopedRendering& operator=(const ScopedRendering&) = delete;

private:
    VkCommandBuffer cmd_;
};

} // namespace projectv::render::vulkan
```

---

### Synchronization2

```cpp
// ProjectV.Render.Vulkan.Synchronization.cppm
export module ProjectV.Render.Vulkan.Synchronization;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Timeline semaphore (Vulkan 1.4 core).
///
/// ## Invariants
/// - value монотонно возрастает
/// - signal(value) требует value > current_value
export class TimelineSemaphore {
public:
    /// Создаёт timeline semaphore.
    ///
    /// @param device Vulkan device
    /// @param initial_value Начальное значение
    ///
    /// @pre device != VK_NULL_HANDLE
    /// @post value() == initial_value
    [[nodiscard]] static auto create(
        VkDevice device,
        uint64_t initial_value = 0
    ) noexcept -> std::expected<TimelineSemaphore, VulkanError>;

    ~TimelineSemaphore() noexcept;

    TimelineSemaphore(TimelineSemaphore&&) noexcept;
    TimelineSemaphore& operator=(TimelineSemaphore&&) noexcept;
    TimelineSemaphore(const TimelineSemaphore&) = delete;
    TimelineSemaphore& operator=(const TimelineSemaphore&) = delete;

    /// Получает текущее значение.
    [[nodiscard]] auto value() const noexcept -> uint64_t;

    /// Получает native handle.
    [[nodiscard]] auto native() const noexcept -> VkSemaphore;

    /// Signal на CPU.
    ///
    /// @param value Новое значение
    /// @pre value > current value
    /// @post value() == value
    auto signal(uint64_t value) noexcept -> void;

    /// Wait на CPU.
    ///
    /// @param value Значение для ожидания
    /// @param timeout_ns Таймаут
    /// @return true если значение достигнуто
    [[nodiscard]] auto wait(
        uint64_t value,
        uint64_t timeout_ns = UINT64_MAX
    ) const noexcept -> bool;

private:
    TimelineSemaphore() noexcept = default;
    VkDevice device_{VK_NULL_HANDLE};
    VkSemaphore semaphore_{VK_NULL_HANDLE};
};

/// Submit info builder для vkQueueSubmit2.
///
/// ## Vulkan 1.4
/// vkQueueSubmit2 — core, не требует VK_KHR_synchronization2.
export class SubmitInfoBuilder {
public:
    SubmitInfoBuilder() noexcept;

    /// Добавляет command buffer.
    auto add_command_buffer(VkCommandBuffer cmd) noexcept -> SubmitInfoBuilder&;

    /// Добавляет wait semaphore.
    auto add_wait_semaphore(
        VkSemaphore semaphore,
        uint64_t value,
        VkPipelineStageFlags2 stage
    ) noexcept -> SubmitInfoBuilder&;

    /// Добавляет signal semaphore.
    auto add_signal_semaphore(
        VkSemaphore semaphore,
        uint64_t value,
        VkPipelineStageFlags2 stage
    ) noexcept -> SubmitInfoBuilder&;

    /// Получает VkSubmitInfo2.
    [[nodiscard]] auto build() noexcept -> VkSubmitInfo2;

    /// Сбрасывает builder.
    auto reset() noexcept -> void;

private:
    VkSubmitInfo2 info_{};
    std::vector<VkCommandBufferSubmitInfo> command_buffers_;
    std::vector<VkSemaphoreSubmitInfo> wait_semaphores_;
    std::vector<VkSemaphoreSubmitInfo> signal_semaphores_;
};

/// Memory barrier builder для vkCmdPipelineBarrier2.
///
/// ## Synchronization2
/// Использует VkDependencyInfo вместо legacy barrier structs.
export class BarrierBuilder {
public:
    BarrierBuilder() noexcept;

    /// Добавляет memory barrier.
    ///
    /// @param src_stage Source pipeline stage
    /// @param src_access Source access flags
    /// @param dst_stage Destination pipeline stage
    /// @param dst_access Destination access flags
    auto add_memory_barrier(
        VkPipelineStageFlags2 src_stage,
        VkAccessFlags2 src_access,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 dst_access
    ) noexcept -> BarrierBuilder&;

    /// Добавляет buffer barrier.
    auto add_buffer_barrier(
        VkBuffer buffer,
        VkPipelineStageFlags2 src_stage,
        VkAccessFlags2 src_access,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 dst_access,
        uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED
    ) noexcept -> BarrierBuilder&;

    /// Добавляет image barrier.
    ///
    /// @param image Image
    /// @param src_stage Source pipeline stage
    /// @param src_access Source access flags
    /// @param dst_stage Destination pipeline stage
    /// @param dst_access Destination access flags
    /// @param old_layout Current layout
    /// @param new_layout New layout
    /// @param range Subresource range
    auto add_image_barrier(
        VkImage image,
        VkPipelineStageFlags2 src_stage,
        VkAccessFlags2 src_access,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 dst_access,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkImageSubresourceRange range
    ) noexcept -> BarrierBuilder&;

    /// Выполняет barrier на command buffer.
    ///
    /// @param cmd Command buffer
    /// @pre cmd в recording state
    /// @post vkCmdPipelineBarrier2 вызван
    auto execute(VkCommandBuffer cmd) noexcept -> void;

    /// Сбрасывает builder.
    auto reset() noexcept -> void;

private:
    VkDependencyInfo info_{};
    std::vector<VkMemoryBarrier2> memory_barriers_;
    std::vector<VkBufferMemoryBarrier2> buffer_barriers_;
    std::vector<VkImageMemoryBarrier2> image_barriers_;
};

} // namespace projectv::render::vulkan
```

---

### Descriptor Buffers

```cpp
// ProjectV.Render.Vulkan.DescriptorBuffer.cppm
export module ProjectV.Render.Vulkan.DescriptorBuffer;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Descriptor buffer для bindless rendering (VK_EXT_descriptor_buffer).
///
/// ## Advantages
/// - No descriptor pool management
/// - Direct memory access via device address
/// - Can be updated on CPU without command buffer
///
/// ## Requirements
/// - VK_EXT_descriptor_buffer
/// - bufferDeviceAddress feature
export class DescriptorBuffer {
public:
    /// Создаёт descriptor buffer.
    ///
    /// @param device Vulkan device
    /// @param allocator VMA allocator
    /// @param size Размер в байтах
    /// @param usage VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT или
    ///              VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        size_t size,
        VkBufferUsageFlags usage
    ) noexcept -> std::expected<DescriptorBuffer, VulkanError>;

    ~DescriptorBuffer() noexcept;

    DescriptorBuffer(DescriptorBuffer&&) noexcept;
    DescriptorBuffer& operator=(DescriptorBuffer&&) noexcept;
    DescriptorBuffer(const DescriptorBuffer&) = delete;
    DescriptorBuffer& operator=(const DescriptorBuffer&) = delete;

    /// Записывает combined image sampler.
    ///
    /// @param index Индекс в buffer
    /// @param view Image view
    /// @param sampler Sampler
    /// @param layout Image layout
    auto write_sampler(
        size_t index,
        VkImageView view,
        VkSampler sampler,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) noexcept -> void;

    /// Записывает storage image.
    auto write_storage_image(
        size_t index,
        VkImageView view,
        VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL
    ) noexcept -> void;

    /// Записывает uniform buffer.
    auto write_uniform_buffer(
        size_t index,
        VkBuffer buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = VK_WHOLE_SIZE
    ) noexcept -> void;

    /// Записывает storage buffer.
    auto write_storage_buffer(
        size_t index,
        VkBuffer buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = VK_WHOLE_SIZE
    ) noexcept -> void;

    /// Получает device address.
    [[nodiscard]] auto address() const noexcept -> VkDeviceAddress;

    /// Получает buffer.
    [[nodiscard]] auto buffer() const noexcept -> VkBuffer;

    /// Получает размер.
    [[nodiscard]] auto size() const noexcept -> size_t;

private:
    DescriptorBuffer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Bindless descriptor manager.
export class BindlessDescriptorManager {
public:
    /// Создаёт manager.
    ///
    /// @param context Vulkan context
    /// @param max_textures Максимум texture slots
    /// @param max_buffers Максимум buffer slots
    [[nodiscard]] static auto create(
        VulkanContext const& context,
        uint32_t max_textures = 65536,
        uint32_t max_buffers = 4096
    ) noexcept -> std::expected<BindlessDescriptorManager, VulkanError>;

    ~BindlessDescriptorManager() noexcept;

    BindlessDescriptorManager(BindlessDescriptorManager&&) noexcept;
    BindlessDescriptorManager& operator=(BindlessDescriptorManager&&) noexcept;
    BindlessDescriptorManager(const BindlessDescriptorManager&) = delete;
    BindlessDescriptorManager& operator=(const BindlessDescriptorManager&) = delete;

    /// Регистрирует texture.
    ///
    /// @return Index или ошибка
    [[nodiscard]] auto register_texture(
        VkImageView view,
        VkSampler sampler
    ) noexcept -> std::expected<uint32_t, VulkanError>;

    /// Регистрирует storage image.
    [[nodiscard]] auto register_storage_image(VkImageView view) noexcept
        -> std::expected<uint32_t, VulkanError>;

    /// Регистрирует buffer.
    [[nodiscard]] auto register_buffer(
        VkBuffer buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = VK_WHOLE_SIZE
    ) noexcept -> std::expected<uint32_t, VulkanError>;

    /// Удаляет texture.
    auto unregister_texture(uint32_t index) noexcept -> void;

    /// Удаляет buffer.
    auto unregister_buffer(uint32_t index) noexcept -> void;

    /// Bind descriptor buffers на command buffer.
    auto bind(VkCommandBuffer cmd) noexcept -> void;

    /// Получает descriptor buffer для textures.
    [[nodiscard]] auto texture_buffer() const noexcept -> DescriptorBuffer const&;

    /// Получает descriptor buffer для buffers.
    [[nodiscard]] auto buffer_buffer() const noexcept -> DescriptorBuffer const&;

private:
    BindlessDescriptorManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

---

### Mesh Shaders

```cpp
// ProjectV.Render.Vulkan.MeshShader.cppm
export module ProjectV.Render.Vulkan.MeshShader;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Mesh shader pipeline.
///
/// ## Requirements
/// - VK_EXT_mesh_shader
/// - Task shader optional (taskCount = 1 if omitted)
///
/// ## Advantages over Vertex Shaders
/// - GPU-driven culling
/// - Variable output size
/// - No vertex buffer binding required
export class MeshShaderPipeline {
public:
    /// Создаёт pipeline.
    ///
    /// @param device Vulkan device
    /// @param task_shader Optional task shader
    /// @param mesh_shader Mesh shader (required)
    /// @param fragment_shader Fragment shader
    /// @param layout Pipeline layout
    /// @param color_format Color attachment format
    /// @param depth_format Depth attachment format
    [[nodiscard]] static auto create(
        VkDevice device,
        VkShaderModule task_shader, // Can be VK_NULL_HANDLE
        VkShaderModule mesh_shader,
        VkShaderModule fragment_shader,
        VkPipelineLayout layout,
        VkFormat color_format,
        VkFormat depth_format = VK_FORMAT_UNDEFINED
    ) noexcept -> std::expected<MeshShaderPipeline, VulkanError>;

    ~MeshShaderPipeline() noexcept;

    MeshShaderPipeline(MeshShaderPipeline&&) noexcept;
    MeshShaderPipeline& operator=(MeshShaderPipeline&&) noexcept;
    MeshShaderPipeline(const MeshShaderPipeline&) = delete;
    MeshShaderPipeline& operator=(const MeshShaderPipeline&) = delete;

    /// Bind pipeline на command buffer.
    auto bind(VkCommandBuffer cmd) noexcept -> void;

    /// Draw mesh tasks.
    ///
    /// @param group_count_x Y Z Workgroup dimensions
    auto draw(
        VkCommandBuffer cmd,
        uint32_t group_count_x,
        uint32_t group_count_y = 1,
        uint32_t group_count_z = 1
    ) noexcept -> void;

    /// Draw mesh tasks indirect.
    auto draw_indirect(
        VkCommandBuffer cmd,
        VkBuffer indirect_buffer,
        VkDeviceSize offset,
        uint32_t draw_count,
        uint32_t stride = sizeof(VkDrawMeshTasksIndirectCommandEXT)
    ) noexcept -> void;

    /// Получает pipeline.
    [[nodiscard]] auto pipeline() const noexcept -> VkPipeline;

    /// Получает layout.
    [[nodiscard]] auto layout() const noexcept -> VkPipelineLayout;

private:
    MeshShaderPipeline() noexcept = default;
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

/// Проверка поддержки mesh shaders.
[[nodiscard]] auto is_mesh_shader_supported(VkPhysicalDevice physical_device) noexcept -> bool;

} // namespace projectv::render::vulkan
```

---

## Dynamic Rendering Local Read (Vulkan 1.4 Core)

### Обзор

`VK_KHR_dynamic_rendering_local_read` стал частью Vulkan 1.4 Core. Эта фича позволяет:

- **Input attachment reads** внутри dynamic rendering без subpass'ов
- **G-Buffer reads** в deferred shading pipleine
- **Упрощение render graph** — нет необходимости в VkRenderPass

### Формальное определение

Пусть $\mathcal{R}$ — rendering pass, $\mathcal{A} = \{a_1, \ldots, a_n\}$ — множество attachments.

Для input attachment $a_i$, доступного в shader:

$$\text{read}(a_i, \vec{uv}) = \text{texelFetch}(a_i, \vec{uv}, \text{currentLayer})$$

при условии:

$$\text{pipelineStage} \geq \text{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT}$$

$$\text{access} \supseteq \text{VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT}$$

### Memory Layout

```
VkRenderingInputAttachmentIndexInfo (Vulkan 1.4)
┌─────────────────────────────────────────────────────────────┐
│  sType: VkStructureType (4 bytes)                           │
│  pNext: const void* (8 bytes)                               │
│  colorAttachmentCount: uint32_t (4 bytes)                   │
│  pColorAttachmentInputIndices: uint32_t const* (8 bytes)    │
│  pDepthInputAttachmentIndex: uint32_t const* (8 bytes)      │
│  pStencilInputAttachmentIndex: uint32_t const* (8 bytes)    │
│  Total: 48 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

### API Contract

```cpp
// ProjectV.Render.Vulkan.LocalRead.cppm
export module ProjectV.Render.Vulkan.LocalRead;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Input attachment info для local read.
export struct InputAttachmentInfo {
    uint32_t color_index{VK_ATTACHMENT_UNUSED};  ///< Индекс color attachment
    uint32_t depth_index{VK_ATTACHMENT_UNUSED};  ///< Индекс depth attachment
    uint32_t stencil_index{VK_ATTACHMENT_UNUSED}; ///< Индекс stencil attachment
};

/// Local Read Builder для G-Buffer deferred shading.
///
/// ## Vulkan 1.4 Core
/// Dynamic Rendering Local Read позволяет читать attachments
/// из fragment shader без VkRenderPass subpass dependency.
///
/// ## Pipeline Requirements
/// - pipelineStage >= FRAGMENT_SHADER
/// - access |= INPUT_ATTACHMENT_READ_BIT
/// - Image layout: VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
export class LocalReadBuilder {
public:
    LocalReadBuilder() noexcept = default;

    /// Устанавливает color attachment mapping.
    /// @param color_attachment_index Индекс в VkRenderingInfo::pColorAttachments
    /// @param input_index Индекс для subpassInput в shader
    auto set_color_input(
        uint32_t color_attachment_index,
        uint32_t input_index
    ) noexcept -> LocalReadBuilder&;

    /// Устанавливает depth input.
    auto set_depth_input(uint32_t input_index) noexcept -> LocalReadBuilder&;

    /// Устанавливает stencil input.
    auto set_stencil_input(uint32_t input_index) noexcept -> LocalReadBuilder&;

    /// Получает VkRenderingInputAttachmentIndexInfo.
    [[nodiscard]] auto get() const noexcept -> VkRenderingInputAttachmentIndexInfo;

    /// Сбрасывает builder.
    auto reset() noexcept -> void;

private:
    std::vector<uint32_t> color_indices_;
    uint32_t depth_index_{VK_ATTACHMENT_UNUSED};
    uint32_t stencil_index_{VK_ATTACHMENT_UNUSED};
    VkRenderingInputAttachmentIndexInfo info_{};
};

/// G-Buffer Layout для deferred shading.
export struct GBufferLayout {
    VkImageView albedo{VK_NULL_HANDLE};      ///< RGB: albedo, A: unused
    VkImageView normal{VK_NULL_HANDLE};      ///< RGB: normal, A: unused
    VkImageView material{VK_NULL_HANDLE};    ///< R: metallic, G: roughness, B: ao
    VkImageView depth{VK_NULL_HANDLE};       ///< Depth buffer
    VkImageView emission{VK_NULL_HANDLE};    ///< RGB: emission
};

/// Deferred Shading Pipeline.
///
/// ## Pass 1: G-Buffer Fill
/// - Write albedo, normal, material, depth, emission
/// - Layout: COLOR_ATTACHMENT_OPTIMAL / DEPTH_ATTACHMENT_OPTIMAL
///
/// ## Pass 2: Lighting
/// - Read G-Buffer as input attachments
/// - Layout: RENDERING_LOCAL_READ
/// - Barrier: COLOR_ATTACHMENT -> INPUT_ATTACHMENT_READ
export class DeferredPipeline {
public:
    /// Создаёт deferred pipeline.
    [[nodiscard]] static auto create(
        VulkanContext const& ctx,
        VkExtent2D resolution,
        VkFormat albedo_format,
        VkFormat normal_format,
        VkFormat material_format,
        VkFormat depth_format,
        VkFormat emission_format = VK_FORMAT_UNDEFINED
    ) noexcept -> std::expected<DeferredPipeline, VulkanError>;

    ~DeferredPipeline() noexcept;

    // Move-only
    DeferredPipeline(DeferredPipeline&&) noexcept;
    DeferredPipeline& operator=(DeferredPipeline&&) noexcept;
    DeferredPipeline(const DeferredPipeline&) = delete;
    DeferredPipeline& operator=(const DeferredPipeline&) = delete;

    /// Начинает G-Buffer fill pass.
    auto begin_gbuffer_pass(VkCommandBuffer cmd) noexcept -> void;

    /// Заканчивает G-Buffer fill pass.
    auto end_gbuffer_pass(VkCommandBuffer cmd) noexcept -> void;

    /// Начинает lighting pass.
    auto begin_lighting_pass(VkCommandBuffer cmd) noexcept -> void;

    /// Заканчивает lighting pass.
    auto end_lighting_pass(VkCommandBuffer cmd) noexcept -> void;

    /// Получает G-Buffer layout.
    [[nodiscard]] auto gbuffer() const noexcept -> GBufferLayout const&;

    /// Resize G-Buffer.
    auto resize(VkExtent2D new_resolution) noexcept
        -> std::expected<void, VulkanError>;

private:
    DeferredPipeline() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

### Slang Shader Interface

```slang
// ProjectV/Render/Deferred/GBuffer.slang
module ProjectV.Render.Deferred.GBuffer;

/// G-Buffer input attachments для lighting pass.
/// Layout: binding 0-4 = subpassInput
[[vk::binding(0, 0)]] subpassInput albedoInput;
[[vk::binding(1, 0)]] subpassInput normalInput;
[[vk::binding(2, 0)]] subpassInput materialInput;
[[vk::binding(3, 0)]] subpassInput depthInput;
[[vk::binding(4, 0)]] subpassInput emissionInput;

/// G-Buffer data structure.
struct GBufferData {
    float3 albedo;
    float3 normal;
    float metallic;
    float roughness;
    float ao;
    float depth;
    float3 emission;
};

/// Sample G-Buffer at current fragment.
GBufferData sampleGBuffer() {
    GBufferData data;

    // Read from input attachments
    float4 albedo = subpassLoad(albedoInput);
    float4 normal = subpassLoad(normalInput);
    float4 material = subpassLoad(materialInput);
    float depth = subpassLoad(depthInput).r;
    float4 emission = subpassLoad(emissionInput);

    data.albedo = albedo.rgb;
    data.normal = normalize(normal.rgb * 2.0 - 1.0);
    data.metallic = material.r;
    data.roughness = material.g;
    data.ao = material.b;
    data.depth = depth;
    data.emission = emission.rgb;

    return data;
}

/// Lighting fragment shader.
float4 fsLighting(float2 uv: SV_Position) : SV_Target {
    GBufferData gbuffer = sampleGBuffer();

    // Reconstruct world position from depth
    float3 world_pos = reconstructWorldPosition(gbuffer.depth, uv);

    // PBR lighting calculation
    float3 Lo = calculatePBRLighting(
        gbuffer.albedo,
        gbuffer.normal,
        gbuffer.metallic,
        gbuffer.roughness,
        world_pos,
        cameraPosition
    );

    // Add emission
    Lo += gbuffer.emission;

    return float4(Lo, 1.0);
}
```

---

## Push Descriptors (Vulkan 1.4 Core)

### Обзор

Push Descriptors устраняют необходимость в descriptor pools:

- **Zero allocation** — descriptors "пушатся" прямо в command buffer
- **Без descriptor sets** — нет overhead на allocation/free
- **Идеально для per-frame data** — UBO, textures

### Формальное определение

Push descriptor — это механизм записи дескрипторов напрямую в command buffer:

$$\text{pushDescriptor} : (\text{PipelineLayout}, \text{set}, \text{writes}) \to \text{Cmd}$$

где:

$$\text{writes} = \{(binding, type, data)\}_{i=1}^{n}$$

### API Contract

```cpp
// ProjectV.Render.Vulkan.PushDescriptor.cppm
export module ProjectV.Render.Vulkan.PushDescriptor;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Push Descriptor Builder.
///
/// ## Vulkan 1.4 Core
/// Push Descriptors — core feature, не требует VK_KHR_push_descriptor.
///
/// ## Advantages
/// - No descriptor pool management
/// - No descriptor set allocation
/// - Immediate update in command buffer
///
/// ## Limitations
/// - Max 256 bytes per push descriptor block (implementation defined)
/// - Not suitable for large arrays
export class PushDescriptorBuilder {
public:
    PushDescriptorBuilder() noexcept;

    /// Добавляет uniform buffer.
    /// @param binding Binding index
    /// @param buffer Buffer handle
    /// @param offset Offset in buffer
    /// @param range Size of data
    auto add_uniform_buffer(
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = VK_WHOLE_SIZE
    ) noexcept -> PushDescriptorBuilder&;

    /// Добавляет storage buffer.
    auto add_storage_buffer(
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = VK_WHOLE_SIZE
    ) noexcept -> PushDescriptorBuilder&;

    /// Добавляет combined image sampler.
    auto add_combined_image_sampler(
        uint32_t binding,
        VkImageView view,
        VkSampler sampler,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) noexcept -> PushDescriptorBuilder&;

    /// Добавляет storage image.
    auto add_storage_image(
        uint32_t binding,
        VkImageView view,
        VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL
    ) noexcept -> PushDescriptorBuilder&;

    /// Пушит descriptors в command buffer.
    /// @param cmd Command buffer
    /// @param layout Pipeline layout
    /// @param set Set index (обычно 0)
    auto push(
        VkCommandBuffer cmd,
        VkPipelineLayout layout,
        uint32_t set = 0
    ) noexcept -> void;

    /// Сбрасывает builder.
    auto reset() noexcept -> void;

private:
    std::vector<VkWriteDescriptorSet> writes_;
    std::vector<VkDescriptorBufferInfo> buffer_infos_;
    std::vector<VkDescriptorImageInfo> image_infos_;
};

/// Per-frame data для push descriptors.
export struct alignas(16) FrameData {
    glm::mat4 view_projection;
    glm::mat4 inverse_view;
    glm::mat4 inverse_projection;
    glm::vec4 camera_position;
    glm::vec4 ambient_color;
    glm::vec2 resolution;
    glm::vec2 inverse_resolution;
    float time;
    float delta_time;
    uint32_t frame_index;
    uint32_t padding;
};

static_assert(sizeof(FrameData) == 256, "FrameData must be 256 bytes");

} // namespace projectv::render::vulkan
```

### Пример использования

```cpp
// В render loop:
void render_frame(VkCommandBuffer cmd, VkPipelineLayout layout) {
    // Push per-frame data
    FrameData frame_data = {
        .view_projection = camera.view_projection(),
        .inverse_view = camera.inverse_view(),
        .inverse_projection = camera.inverse_projection(),
        .camera_position = glm::vec4(camera.position(), 1.0f),
        .resolution = glm::vec2(width, height),
        .time = total_time,
        .delta_time = delta_time
    };

    PushDescriptorBuilder builder;
    builder.add_uniform_buffer(0, frame_uniform_buffer, 0, sizeof(FrameData))
           .add_combined_image_sampler(1, shadow_map_view, shadow_sampler)
           .add_combined_image_sampler(2, env_map_view, env_sampler);

    builder.push(cmd, layout, 0);

    // Draw commands...
}
```

---

## Buffer Device Address (Vulkan 1.4 Core)

### Обзор

Buffer Device Address (BDA) — ключевой механизм для bindless rendering:

- **GPU pointers** — прямой адрес буфера в памяти GPU
- **Универсальный доступ** — любой буфер доступен по адресу
- **Bindless** — без descriptor sets для buffers

### Формальное определение

Пусть $B$ — buffer object с размером $|B|$. Device address:

$$\text{address}(B) : \text{VkBuffer} \to \text{VkDeviceAddress}$$

Для shader доступа по адресу $a$ и типу $T$:

$$\text{load}(a, T) : \text{VkDeviceAddress} \to T$$
$$\text{store}(a, T, v) : \text{VkDeviceAddress} \times T \to \text{void}$$

### API Contract

```cpp
// ProjectV.Render.Vulkan.DeviceAddress.cppm
export module ProjectV.Render.Vulkan.DeviceAddress;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::vulkan {

/// Buffer с device address.
///
/// ## Vulkan 1.4 Core
/// Buffer Device Address — core feature.
///
/// ## Requirements
/// - bufferDeviceAddress feature enabled
/// - VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
export class DeviceAddressBuffer {
public:
    /// Создаёт buffer с device address.
    [[nodiscard]] static auto create(
        VulkanContext const& ctx,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
    ) noexcept -> std::expected<DeviceAddressBuffer, VulkanError>;

    ~DeviceAddressBuffer() noexcept;

    DeviceAddressBuffer(DeviceAddressBuffer&&) noexcept;
    DeviceAddressBuffer& operator=(DeviceAddressBuffer&&) noexcept;
    DeviceAddressBuffer(const DeviceAddressBuffer&) = delete;
    DeviceAddressBuffer& operator=(const DeviceAddressBuffer&) = delete;

    /// Получает device address.
    [[nodiscard]] auto address() const noexcept -> VkDeviceAddress;

    /// Получает buffer.
    [[nodiscard]] auto buffer() const noexcept -> VkBuffer;

    /// Получает размер.
    [[nodiscard]] auto size() const noexcept -> VkDeviceSize;

    /// Mapping для CPU access (если supported).
    [[nodiscard]] auto map() noexcept -> std::expected<void*, VulkanError>;

    /// Unmapping.
    auto unmap() noexcept -> void;

private:
    DeviceAddressBuffer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Bindless Buffer Array Manager.
///
/// ## Architecture
/// - Single large buffer для всех mesh data
/// - Offset-based access в shaders
/// - No descriptor set changes between draws
export class BindlessBufferManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VulkanContext const& ctx,
        VkDeviceSize initial_capacity = 256 * 1024 * 1024  // 256 MB
    ) noexcept -> std::expected<BindlessBufferManager, VulkanError>;

    ~BindlessBufferManager() noexcept;

    // Move-only
    BindlessBufferManager(BindlessBufferManager&&) noexcept;
    BindlessBufferManager& operator=(BindlessBufferManager&&) noexcept;
    BindlessBufferManager(const BindlessBufferManager&) = delete;
    BindlessBufferManager& operator=(const BindlessBufferManager&) = delete;

    /// Выделяет память под данные.
    /// @return Offset в buffer или ошибка
    [[nodiscard]] auto allocate(
        VkDeviceSize size,
        VkDeviceSize alignment = 16
    ) noexcept -> std::expected<VkDeviceSize, VulkanError>;

    /// Освобождает память.
    auto free(VkDeviceSize offset, VkDeviceSize size) noexcept -> void;

    /// Загружает данные в buffer.
    auto upload(
        VkDeviceSize offset,
        void const* data,
        VkDeviceSize size
    ) noexcept -> std::expected<void, VulkanError>;

    /// Получает device address.
    [[nodiscard]] auto address() const noexcept -> VkDeviceAddress;

    /// Получает buffer.
    [[nodiscard]] auto buffer() const noexcept -> VkBuffer;

private:
    BindlessBufferManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

### Slang Shader Interface

```slang
// ProjectV/Render/Core/Bindless.slang
module ProjectV.Render.Core.Bindless;

/// Bindless buffer access через device address.
///
/// ## Vulkan 1.4
/// Использует VK_KHR_buffer_device_address (Core 1.4)
/// для прямого доступа к memory по GPU pointer.

/// Mesh vertex data.
struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float4 tangent;
};

/// Mesh draw command (GPU-driven).
struct MeshDrawCommand {
    uint vertex_count;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
    uint64_t vertex_buffer_address;  // Device address
    uint64_t index_buffer_address;   // Device address
    uint64_t material_index;
    uint padding;
};

/// Vertex fetch по device address.
Vertex fetch_vertex(
    uint64_t vertex_buffer_addr,
    uint vertex_index
) {
    // Direct memory access via pointer
    DevicePointer<Vertex> ptr = devicePointerFromAddress<Vertex>(vertex_buffer_addr);
    return ptr[vertex_index];
}

/// Index fetch.
uint fetch_index(
    uint64_t index_buffer_addr,
    uint index_index
) {
    DevicePointer<uint> ptr = devicePointerFromAddress<uint>(index_buffer_addr);
    return ptr[index_index];
}

/// Mesh shader для bindless rendering.
[numthreads(64, 1, 1)]
[outputtopology("triangle")]
void msBindless(
    uint3 tid: SV_DispatchThreadID,
    // Mesh output
    out indices uint3 triangles[64],
    out vertices Vertex verts[192]
) {
    // Load mesh command from indirect buffer
    MeshDrawCommand cmd = mesh_commands[tid.x];

    // Set mesh size
    SetMeshOutputCounts(cmd.vertex_count, cmd.vertex_count / 3);

    // Fetch vertices directly by address
    for (uint i = 0; i < cmd.vertex_count; ++i) {
        verts[i] = fetch_vertex(cmd.vertex_buffer_address, i);
    }

    // Generate triangle indices
    for (uint i = 0; i < cmd.vertex_count / 3; ++i) {
        triangles[i] = uint3(i * 3, i * 3 + 1, i * 3 + 2);
    }
}
```

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Vulkan 1.4 Backend                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ VulkanContext   │  │ TimelineSemaph  │  │ CommandPool     │          │
│  │ (Instance,      │  │ (Async Sync)    │  │ (Cmd Buffers)   │          │
│  │  Device, VMA)   │  │                 │  │                 │          │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘          │
│           │                    │                    │                    │
│           ▼                    ▼                    ▼                    │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Dynamic Rendering                              │    │
│  │  (vkCmdBeginRendering / vkCmdEndRendering)                       │    │
│  │  NO VkRenderPass / NO VkFramebuffer                              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│           ┌────────────────────────┼────────────────────────┐           │
│           ▼                        ▼                        ▼           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ MeshShader      │  │ DescriptorBuffer│  │ BarrierBuilder  │          │
│  │ Pipeline        │  │ (Bindless)      │  │ (Synchronization2)         │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Error Codes

```cpp
// ProjectV.Render.Vulkan.Error.cppm
export module ProjectV.Render.Vulkan.Error;

import std;

export namespace projectv::render::vulkan {

/// Vulkan error codes.
export enum class VulkanError : uint8_t {
    InstanceCreationFailed,
    DeviceCreationFailed,
    SwapchainCreationFailed,
    BufferCreationFailed,
    ImageCreationFailed,
    PipelineCreationFailed,
    ShaderCompilationFailed,
    OutOfMemory,
    OutOfDeviceMemory,
    SurfaceLost,
    SuboptimalSwapchain,
    UnsupportedFormat,
    UnsupportedFeature,
    InvalidArgument,
    Timeout,
    DescriptorPoolOutOfMemory
};

/// Конвертирует VkResult в VulkanError.
[[nodiscard]] auto to_vulkan_error(VkResult result) noexcept -> VulkanError;

/// Получает строковое описание ошибки.
[[nodiscard]] auto to_string(VulkanError error) noexcept -> std::string_view;

} // namespace projectv::render::vulkan
```

---

## Ссылки

- [ADR-0004: Build System & C++26 Modules](../adr/0004-build-and-modules-spec.md)
- [Engine Structure](../01_core/01_engine_structure.md)
- [Voxel Pipeline](../03_voxel/02_voxel_pipeline.md)
- [Render Graph Specification](../02_render/04_render_graph.md)
