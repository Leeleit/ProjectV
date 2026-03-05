# Структура движка ProjectV [🟢 Уровень 1]

**🟢 Уровень 1: Базовый** — Организация кодовой базы ProjectV.

## Обзор

ProjectV следует **Data-Oriented Design (DOD)** и **Entity-Component-System (ECS)** архитектуре. Код организован по
слоям: от низкоуровневого ядра до высокоуровневой игровой логики.

---

## Структура директорий

```
src/
├── core/                    # Ядро движка (низкий уровень)
│   ├── memory/              # Аллокаторы, memory arenas
│   ├── containers/          # DOD контейнеры (PodVector, FlatMap)
│   ├── jobs/                # Job System (M:N threading)
│   ├── io/                  # Файловая система, сериализация
│   └── platform/            # Платформозависимый код
│
├── render/                  # Рендеринг (Vulkan 1.4)
│   ├── device/              # Vulkan device, queues, command buffers
│   ├── pipeline/            # Pipeline layouts, shader modules
│   ├── passes/              # Render passes (gbuffer, deferred, etc.)
│   ├── resources/           # Buffers, images, descriptors
│   └── svo/                 # SVO renderer (ray marching, mesh gen)
│
├── voxel/                   # Воксельная система
│   ├── svo/                 # Sparse Voxel Octree
│   ├── chunk/               # Управление чанками
│   ├── mesh/                # Генерация mesh'ей
│   └── editing/             # Редактирование вокселей
│
├── physics/                 # Физика (JoltPhysics)
│   ├── world/               # Физический мир
│   ├── shapes/              # Коллайдеры
│   └── queries/             # Raycasts, overlaps
│
├── gameplay/                # Игровая логика (ECS)
│   ├── components/          # ECS компоненты (data only)
│   ├── systems/             # ECS системы (logic)
│   ├── blocks/              # Типы блоков
│   └── player/              # Игрок
│
├── audio/                   # Аудио (miniaudio)
│   ├── engine/              # Audio engine
│   ├── sources/             # Audio sources
│   └── listener/            # Audio listener
│
├── ui/                      # Пользовательский интерфейс
│   ├── imgui/               # Debug UI
│   └── game/                # Game UI
│
└── main.cpp                 # Точка входа
```

---

## Слои архитектуры

```
┌─────────────────────────────────────────────────────────────────┐
│                      Gameplay Layer                              │
│  (Components, Systems, Blocks, Player)                         │
├─────────────────────────────────────────────────────────────────┤
│                       Voxel Layer                                │
│  (SVO, Chunks, Mesh Generation, Editing)                       │
├─────────────────────────────────────────────────────────────────┤
│                      Physics Layer                               │
│  (JoltPhysics PIMPL, Colliders, Queries)                       │
├─────────────────────────────────────────────────────────────────┤
│                      Render Layer                                │
│  (Vulkan 1.4, Dynamic Rendering, Synchronization2)            │
├─────────────────────────────────────────────────────────────────┤
│                       Core Layer                                 │
│  (Memory, Containers, std::execution Jobs, IO, Platform)       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. Core Layer

### 1.1 Memory Module

#### Memory Layout

```
LinearArena (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── buffer_: void* (8 bytes)                           │
│      ├── capacity_: size_t (8 bytes)                        │
│      ├── offset_: size_t (8 bytes)                          │
│      └── Padding: 8 bytes (for alignment)                   │
│  Total: 8 bytes (external) + 32 bytes (internal)            │
└─────────────────────────────────────────────────────────────┘

PoolAllocator<T> (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── buffer_: void* (8 bytes)                           │
│      ├── free_list_: void** (8 bytes)                       │
│      ├── capacity_: size_t (8 bytes)                        │
│      ├── available_: size_t (8 bytes)                       │
│      └── object_size_: size_t (8 bytes)                     │
│  Total: 8 bytes (external) + 40 bytes (internal)            │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Core.Memory.cppm
export module ProjectV.Core.Memory;

import std;

export namespace projectv::core {

/// Коды ошибок аллокации.
export enum class AllocationError : uint8_t {
    OutOfMemory,
    InvalidAlignment,
    InvalidSize,
    AllocationFailed
};

/// Linear allocator для временных данных.
///
/// ## Invariants
/// - `offset_ <= capacity_` всегда
/// - `buffer_ != nullptr` после успешной инициализации
/// - Все аллокации выровнены по указанному alignment
///
/// ## Thread Safety
/// - НЕ thread-safe. Каждый поток ДОЛЖЕН иметь свой LinearArena.
///
/// ## Lifetime
/// - ДОЛЖЕН пережить все аллокации из него.
/// - reset() НЕ вызывает деструкторы объектов.
export class LinearArena {
public:
    /// Создаёт arena заданного размера.
    ///
    /// @param capacity Размер буфера в байтах
    ///
    /// @pre capacity > 0
    /// @post capacity() == capacity
    /// @post used() == 0
    /// @post remaining() == capacity
    ///
    /// @throws std::bad_alloc если не удалось выделить память
    explicit LinearArena(size_t capacity) noexcept;

    ~LinearArena() noexcept;

    // Move-only
    LinearArena(LinearArena&& other) noexcept;
    LinearArena& operator=(LinearArena&& other) noexcept;
    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;

    /// Выделяет память из arena.
    ///
    /// @param size Размер в байтах
    /// @param alignment Выравнивание (по умолчанию 8 байт)
    ///
    /// @pre size > 0
    /// @pre alignment is power of 2
    /// @pre alignment <= alignof(std::max_align_t)
    ///
    /// @post returned pointer is aligned to `alignment`
    /// @post used() >= old used() + size (rounded to alignment)
    ///
    /// @return Указатель на память или ошибку
    [[nodiscard]] auto allocate(
        size_t size,
        size_t alignment = 8
    ) noexcept -> std::expected<void*, AllocationError>;

    /// Сбрасывает arena, освобождая всю память.
    ///
    /// @post used() == 0
    /// @post remaining() == capacity()
    ///
    /// @warning НЕ вызывает деструкторы объектов!
    auto reset() noexcept -> void;

    /// @return Общий размер arena в байтах
    [[nodiscard]] auto capacity() const noexcept -> size_t;

    /// @return Использовано байт
    [[nodiscard]] auto used() const noexcept -> size_t;

    /// @return Оставшееся место в байтах
    [[nodiscard]] auto remaining() const noexcept -> size_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Pool allocator для объектов одинакового размера.
///
/// ## Requirements
/// - T ДОЛЖЕН быть trivially destructible
///
/// ## Invariants
/// - `available_ <= capacity_` всегда
/// - Все свободные слоты связаны через free list
/// - Каждый слот имеет размер `max(sizeof(T), sizeof(void*))`
///
/// ## Thread Safety
/// - НЕ thread-safe.
export template<typename T>
requires std::is_trivially_destructible_v<T>
class PoolAllocator {
public:
    /// Создаёт pool на заданное количество объектов.
    ///
    /// @param capacity Количество объектов
    ///
    /// @pre capacity > 0
    /// @post available() == capacity
    /// @post capacity() == capacity
    explicit PoolAllocator(size_t capacity) noexcept;

    ~PoolAllocator() noexcept;

    PoolAllocator(PoolAllocator&&) noexcept;
    PoolAllocator& operator=(PoolAllocator&&) noexcept;
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    /// Выделяет один объект.
    ///
    /// @pre available() > 0 (иначе nullptr)
    /// @post available() == old available() - 1
    ///
    /// @return Указатель на объект или nullptr если pool полон
    [[nodiscard]] auto allocate() noexcept -> T*;

    /// Освобождает объект.
    ///
    /// @param ptr Указатель на объект из этого pool
    ///
    /// @pre ptr != nullptr
    /// @pre ptr был получен из этого pool
    /// @pre ptr не был освобождён ранее
    /// @post available() == old available() + 1
    auto deallocate(T* ptr) noexcept -> void;

    /// @return Количество свободных слотов
    [[nodiscard]] auto available() const noexcept -> size_t;

    /// @return Общее количество слотов
    [[nodiscard]] auto capacity() const noexcept -> size_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::core
```

---

### 1.2 Job System Module (std::execution)

#### Memory Layout

```
JobSystem (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── scheduler_: std::execution::run_loop*              │
│      ├── thread_pool_: std::execution::static_thread_pool*  │
│      ├── active_jobs_: std::atomic<uint32_t> (4 bytes)      │
│      ├── shutdown_: std::atomic<bool> (1 byte + padding)    │
│      └── worker_count_: uint32_t (4 bytes)                  │
│  Total: 8 bytes (external) + ~128 bytes (internal)          │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Core.Jobs.cppm
export module ProjectV.Core.Jobs;

import std;
import std.execution;

export namespace projectv::core {

/// Система задач на базе std::execution (P2300).
///
/// ## Архитектура
/// Использует Senders/Receivers модель из std::execution:
/// - `std::execution::schedule()` — создаёт sender для планирования
/// - `std::execution::then()` — цепочка continuation
/// - `std::execution::when_all()` — параллельное выполнение
///
/// ## Invariants
/// - `active_jobs_` корректно отражает количество активных задач
/// - После shutdown() новые задачи не принимаются
///
/// ## Thread Safety
/// - Все методы thread-safe.
export class JobSystem {
public:
    /// Создаёт Job System.
    ///
    /// @param thread_count Количество worker потоков (0 = hardware_concurrency)
    ///
    /// @pre thread_count > 0 или thread_count == 0 (auto)
    /// @post thread_count() возвращает актуальное количество
    explicit JobSystem(uint32_t thread_count = 0) noexcept;

    ~JobSystem() noexcept;

    JobSystem(JobSystem&&) noexcept;
    JobSystem& operator=(JobSystem&&) noexcept;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    /// Планирует задачу для асинхронного выполнения.
    ///
    /// @tparam Fun callable, возвращающий void или sender
    /// @param fun Функция для выполнения
    ///
    /// @pre !shutdown_requested()
    /// @post active_jobs() >= old active_jobs()
    ///
    /// @return sender, который можно синхронизировать
    template<std::execution::sender Fun>
    [[nodiscard]] auto schedule(Fun&& fun) noexcept
        -> std::execution::sender auto;

    /// Параллельное выполнение диапазона.
    ///
    /// @param count Количество итераций
    /// @param func Функция, принимающая индекс итерации
    ///
    /// @pre count > 0
    /// @post Все итерации выполнены при возврате
    ///
    /// @return sender для синхронизации
    template<typename Fun>
    [[nodiscard]] auto parallel_for(
        size_t count,
        Fun&& func
    ) noexcept -> std::execution::sender auto;

    /// Блокирует до завершения всех активных задач.
    ///
    /// @post active_jobs() == 0
    auto wait() noexcept -> void;

    /// Запрашивает graceful shutdown.
    auto shutdown() noexcept -> void;

    /// @return Количество worker потоков
    [[nodiscard]] auto thread_count() const noexcept -> uint32_t;

    /// @return Количество активных задач
    [[nodiscard]] auto active_jobs() const noexcept -> uint32_t;

    /// @return true если shutdown запрошен
    [[nodiscard]] auto shutdown_requested() const noexcept -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// RAII scope guard для задач.
export class JobScope {
public:
    explicit JobScope(JobSystem& system) noexcept;
    ~JobScope() noexcept;  // Вызывает wait()

    JobScope(JobScope const&) = delete;
    JobScope& operator=(JobScope const&) = delete;

private:
    JobSystem& system_;
};

} // namespace projectv::core
```

---

## 2. Render Layer (Vulkan 1.4)

### 2.1 Vulkan Device Module

#### Memory Layout

```
VulkanRenderer (PIMPL)
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
│      ├── swapchain_: VkSwapchainKHR (8 bytes)               │
│      ├── current_frame_: uint32_t (4 bytes)                 │
│      ├── frames_in_flight_: uint32_t (4 bytes)              │
│      └── frame_data_: std::array<FrameData, MAX_FRAMES>     │
│  Total: 8 bytes (external) + ~1KB (internal)                │
└─────────────────────────────────────────────────────────────┘

FrameData (per frame in flight)
┌─────────────────────────────────────────────────────────────┐
│  command_pool_: VkCommandPool (8 bytes)                     │
│  command_buffer_: VkCommandBuffer (8 bytes)                 │
│  image_available_: VkSemaphore (8 bytes)                    │
│  render_finished_: VkSemaphore (8 bytes)                    │
│  in_flight_fence_: VkFence (8 bytes)                        │
│  descriptor_pool_: VkDescriptorPool (8 bytes)               │
│  staging_buffer_: BufferAllocation (~24 bytes)              │
│  Total: ~80 bytes per frame                                 │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Render.Vulkan.cppm
module;

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

export module ProjectV.Render.Vulkan;

import std;
import glm;

export namespace projectv::render::vulkan {

/// Коды ошибок Vulkan.
export enum class VulkanError : uint8_t {
    InstanceCreationFailed,
    DeviceCreationFailed,
    SwapchainCreationFailed,
    OutOfDeviceMemory,
    SurfaceNotSupported,
    ExtensionNotPresent,
    LayerNotPresent,
    PipelineCreationFailed,
    ShaderCompilationFailed
};

/// Конфигурация рендерера.
export struct RendererConfig {
    uint32_t width{1920};
    uint32_t height{1080};
    bool vsync{true};
    bool enable_validation{false};
    uint32_t max_frames_in_flight{2};
};

/// Главный класс Vulkan 1.4 рендерера.
///
/// ## Vulkan Version
/// Требует Vulkan 1.4 с расширениями:
/// - VK_KHR_dynamic_rendering (core in 1.4)
/// - VK_KHR_synchronization2 (core in 1.4)
/// - VK_EXT_descriptor_indexing
///
/// ## Invariants
/// - device_ всегда валиден после успешного create()
/// - current_frame_ < max_frames_in_flight
/// - Все command buffers сброшены перед begin_frame()
///
/// ## Thread Safety
/// - begin_frame()/end_frame() вызываются из одного потока
/// - Resource creation thread-safe через external synchronization
export class VulkanRenderer {
public:
    /// Инициализирует рендерер.
    ///
    /// @param config Конфигурация
    /// @param native_window_handle HWND или equivalent
    ///
    /// @pre native_window_handle валиден
    /// @post device() возвращает валидный Device
    /// @post current_command_buffer() возвращает валидный command buffer после begin_frame()
    [[nodiscard]] static auto create(
        RendererConfig const& config,
        void* native_window_handle
    ) noexcept -> std::expected<VulkanRenderer, VulkanError>;

    ~VulkanRenderer() noexcept;

    VulkanRenderer(VulkanRenderer&&) noexcept;
    VulkanRenderer& operator=(VulkanRenderer&&) noexcept;
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    /// Начинает кадр.
    ///
    /// @pre !in_frame()
    /// @post in_frame() == true
    /// @post current_command_buffer() возвращает валидный command buffer
    ///
    /// @return void или ошибку
    [[nodiscard]] auto begin_frame() noexcept
        -> std::expected<void, VulkanError>;

    /// Заканчивает кадр и представляет.
    ///
    /// @pre in_frame()
    /// @post in_frame() == false
    /// @post current_frame_ = (current_frame_ + 1) % max_frames_in_flight
    auto end_frame() noexcept -> void;

    /// Обрабатывает resize окна.
    ///
    /// @param new_width Новый размер
    /// @param new_height Новый размер
    ///
    /// @pre !in_frame()
    auto on_resize(uint32_t new_width, uint32_t new_height) noexcept -> void;

    /// @return Device для создания ресурсов
    [[nodiscard]] auto device() const noexcept -> VkDevice;

    /// @return VMA allocator
    [[nodiscard]] auto allocator() const noexcept -> VmaAllocator;

    /// @return Текущий command buffer (valid between begin_frame/end_frame)
    [[nodiscard]] auto current_command_buffer() const noexcept -> VkCommandBuffer;

    /// @return Текущий image index
    [[nodiscard]] auto current_image_index() const noexcept -> uint32_t;

    /// @return true если внутри begin_frame/end_frame
    [[nodiscard]] auto in_frame() const noexcept -> bool;

private:
    VulkanRenderer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

---

### 2.2 Dynamic Rendering (Vulkan 1.4)

#### Memory Layout

```
RenderingAttachmentInfo
┌─────────────────────────────────────────────────────────────┐
│  imageView: VkImageView (8 bytes)                           │
│  imageLayout: VkImageLayout (4 bytes)                       │
│  loadOp: VkAttachmentLoadOp (4 bytes)                       │
│  storeOp: VkAttachmentStoreOp (4 bytes)                     │
│  clearValue: VkClearValue (16 bytes)                        │
│  Total: 36 bytes (aligned to 16)                            │
└─────────────────────────────────────────────────────────────┘

RenderingInfo
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
```

#### API Contracts

```cpp
// ProjectV.Render.DynamicRendering.cppm
export module ProjectV.Render.DynamicRendering;

import std;
import ProjectV.Render.Vulkan;

export namespace projectv::render {

/// Attachment info для dynamic rendering.
export struct RenderingAttachmentInfo {
    VkImageView image_view;
    VkImageLayout image_layout;
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;

    /// Создаёт color attachment.
    static auto color(
        VkImageView view,
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkClearValue clear = {}
    ) noexcept -> RenderingAttachmentInfo;

    /// Создаёт depth attachment.
    static auto depth(
        VkImageView view,
        VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        float depth_clear = 1.0f
    ) noexcept -> RenderingAttachmentInfo;
};

/// Dynamic rendering pass builder.
///
/// ## Vulkan 1.4 Requirement
/// Использует vkCmdBeginRendering/vkCmdEndRendering без VkRenderPass.
export class DynamicRenderPass {
public:
    explicit DynamicRenderPass(VkCommandBuffer cmd) noexcept;

    /// Добавляет color attachment.
    auto add_color(RenderingAttachmentInfo const& attachment) noexcept
        -> DynamicRenderPass&;

    /// Устанавливает depth attachment.
    auto set_depth(RenderingAttachmentInfo const& attachment) noexcept
        -> DynamicRenderPass&;

    /// Устанавливает render area.
    auto set_render_area(VkRect2D area) noexcept -> DynamicRenderPass&;

    /// Начинает rendering pass.
    ///
    /// @pre cmd в recording state
    /// @post rendering pass active
    auto begin() noexcept -> void;

    /// Заканчивает rendering pass.
    ///
    /// @post rendering pass inactive
    auto end() noexcept -> void;

private:
    VkCommandBuffer cmd_;
    VkRect2D render_area_;
    std::array<RenderingAttachmentInfo, 8> color_attachments_;
    size_t color_count_{0};
    std::optional<RenderingAttachmentInfo> depth_attachment_;
};

} // namespace projectv::render
```

---

### 2.3 Synchronization2

#### API Contracts

```cpp
// ProjectV.Render.Sync2.cppm
export module ProjectV.Render.Sync2;

import std;
import ProjectV.Render.Vulkan;

export namespace projectv::render {

/// Barrier builder для Synchronization2.
///
/// ## Vulkan 1.4
/// Использует VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2
/// с vkCmdPipelineBarrier2.
export class BarrierBuilder {
public:
    explicit BarrierBuilder(VkCommandBuffer cmd) noexcept;

    /// Добавляет memory barrier.
    auto add_memory_barrier(
        VkPipelineStageFlags2 src_stage,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 src_access,
        VkAccessFlags2 dst_access
    ) noexcept -> BarrierBuilder&;

    /// Добавляет buffer barrier.
    auto add_buffer_barrier(
        VkBuffer buffer,
        VkPipelineStageFlags2 src_stage,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 src_access,
        VkAccessFlags2 dst_access,
        uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED
    ) noexcept -> BarrierBuilder&;

    /// Добавляет image barrier.
    auto add_image_barrier(
        VkImage image,
        VkPipelineStageFlags2 src_stage,
        VkPipelineStageFlags2 dst_stage,
        VkAccessFlags2 src_access,
        VkAccessFlags2 dst_access,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
    ) noexcept -> BarrierBuilder&;

    /// Выполняет все барьеры.
    ///
    /// @pre cmd в recording state
    /// @post все барьеры применены
    auto execute() noexcept -> void;

private:
    VkCommandBuffer cmd_;
    std::vector<VkMemoryBarrier2> memory_barriers_;
    std::vector<VkBufferMemoryBarrier2> buffer_barriers_;
    std::vector<VkImageMemoryBarrier2> image_barriers_;
};

/// Конвертирует old-style flags в Synchronization2.
export namespace sync2_flags {

constexpr VkPipelineStageFlags2 to_stage2(VkPipelineStageFlags flags) noexcept;
constexpr VkAccessFlags2 to_access2(VkAccessFlags flags) noexcept;

} // namespace sync2_flags

} // namespace projectv::render
```

---

## 3. Voxel Layer

### 3.1 SVO Module

#### Memory Layout

```
SVONode (16 bytes, std430 aligned)
┌─────────────────────────────────────────────────────────────┐
│  children: uint32_t[8] (32 bytes) → indices to children     │
│  parent: uint32_t (4 bytes)                                 │
│  voxel_index: uint32_t (4 bytes) → index in voxel buffer    │
│  flags: uint16_t (2 bytes)                                  │
│  padding: uint16_t (2 bytes)                                │
│  Total: 44 bytes → padded to 48 bytes for alignment         │
└─────────────────────────────────────────────────────────────┘

VoxelData (4 bytes)
┌─────────────────────────────────────────────────────────────┐
│  material_id: uint16_t (2 bytes)                            │
│  flags: uint8_t (1 byte)                                    │
│  light: uint8_t (1 byte)                                    │
│  Total: 4 bytes                                             │
└─────────────────────────────────────────────────────────────┘

SVOTree GPU Buffers
┌─────────────────────────────────────────────────────────────┐
│  node_buffer: VkBuffer (node_count * sizeof(SVONode))       │
│  voxel_buffer: VkBuffer (voxel_count * sizeof(VoxelData))   │
│  indirect_buffer: VkBuffer (for indirect draw calls)        │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Voxel.SVO.cppm
export module ProjectV.Voxel.SVO;

import std;
import glm;

export namespace projectv::voxel {

/// Данные одного вокселя (4 байта).
export struct alignas(4) VoxelData {
    uint16_t material_id;    ///< ID материала (0 = air)
    uint8_t flags;           ///< Флаги
    uint8_t light;           ///< Уровень освещённости
};

static_assert(sizeof(VoxelData) == 4);
static_assert(alignof(VoxelData) == 4);

/// Коды ошибок SVO.
export enum class SVOError : uint8_t {
    InvalidCoordinates,
    NodeAllocationFailed,
    InvalidDepth,
    UploadFailed,
    CompressionFailed,
    GPUBufferCreationFailed
};

/// Конфигурация SVO.
export struct SVOConfig {
    uint32_t max_depth{16};           ///< Максимальная глубина дерева (max_depth=16 → 2^16 = 65536³ voxels)
    uint32_t initial_capacity{1024};  ///< Начальная ёмкость узлов
    bool enable_dag_compression{true}; ///< Включить DAG сжатие
};

/// Sparse Voxel Octree.
///
/// ## Invariants
/// - depth <= max_depth
/// - Все children indices либо валидны, либо равны INVALID_INDEX
/// - voxel_index валиден только для leaf nodes
///
/// ## Complexity
/// - get(): O(depth) = O(log n)
/// - set(): O(depth) + amortized O(1) for allocation
/// - compress_dag(): O(n log n) where n = node_count
export class SVOTree {
public:
    /// Создаёт пустое SVO.
    ///
    /// @pre config.max_depth > 0 && config.max_depth <= 24
    /// @post node_count() == 1 (root only)
    explicit SVOTree(SVOConfig const& config = {}) noexcept;

    ~SVOTree() noexcept;

    SVOTree(SVOTree&&) noexcept;
    SVOTree& operator=(SVOTree&&) noexcept;
    SVOTree(const SVOTree&) = delete;
    SVOTree& operator=(const SVOTree&) = delete;

    /// Получает воксель по координатам.
    ///
    /// @param coord Координаты в мировом пространстве
    ///
    /// @pre coord в пределах [0, 2^max_depth)
    ///
    /// @return Данные вокселя или ошибка
    [[nodiscard]] auto get(glm::ivec3 coord) const noexcept
        -> std::expected<VoxelData, SVOError>;

    /// Устанавливает воксель по координатам.
    ///
    /// @param coord Координаты
    /// @param data Данные вокселя
    ///
    /// @pre coord в пределах [0, 2^max_depth)
    /// @post get(coord) == data
    [[nodiscard]] auto set(glm::ivec3 coord, VoxelData data) noexcept
        -> std::expected<void, SVOError>;

    /// Выполняет DAG сжатие.
    ///
    /// @post node_count() <= old node_count()
    /// @return Количество сэкономленных узлов
    [[nodiscard]] auto compress_dag() noexcept -> size_t;

    /// Загружает SVO на GPU.
    ///
    /// @pre gpu_node_buffer() == VK_NULL_HANDLE
    /// @post gpu_node_buffer() != VK_NULL_HANDLE
    [[nodiscard]] auto upload_to_gpu(
        VkDevice device,
        VmaAllocator allocator
    ) noexcept -> std::expected<void, SVOError>;

    /// @return Количество узлов в дереве
    [[nodiscard]] auto node_count() const noexcept -> size_t;

    /// @return Количество ненулевых вокселей
    [[nodiscard]] auto voxel_count() const noexcept -> size_t;

    /// @return Использование памяти в байтах
    [[nodiscard]] auto memory_usage() const noexcept -> size_t;

    /// @return GPU buffer с узлами
    [[nodiscard]] auto gpu_node_buffer() const noexcept -> VkBuffer;

    /// @return GPU buffer с вокселями
    [[nodiscard]] auto gpu_voxel_buffer() const noexcept -> VkBuffer;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## 4. ECS Layer (C++26 Reflection)

### 4.1 Components Module

#### Memory Layout

```
TransformComponent (48 bytes)
┌─────────────────────────────────────────────────────────────┐
│  position: glm::vec3 (12 bytes)                             │
│  padding0: 4 bytes                                          │
│  rotation: glm::quat (16 bytes)                             │
│  scale: glm::vec3 (12 bytes)                                │
│  padding1: 4 bytes                                          │
│  Total: 48 bytes (aligned to 16)                            │
└─────────────────────────────────────────────────────────────┘

VoxelChunkComponent (16 bytes)
┌─────────────────────────────────────────────────────────────┐
│  chunk_x: int32_t (4 bytes)                                 │
│  chunk_y: int32_t (4 bytes)                                 │
│  chunk_z: int32_t (4 bytes)                                 │
│  flags: uint32_t (4 bytes)                                  │
│  Total: 16 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Gameplay.Components.cppm
export module ProjectV.Gameplay.Components;

import std;
import glm;

export namespace projectv::gameplay {

/// Transform компонент.
///
/// ## Memory Layout
/// - 48 bytes total
/// - 16-byte aligned for SIMD
/// - Cache-line friendly (fits 1 component per cache line with padding)
export struct alignas(16) TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    /// Вычисляет world matrix.
    [[nodiscard]] auto to_matrix() const noexcept -> glm::mat4;

    /// C++26 Static Reflection (P2996)
    /// Автоматическая сериализация без макросов.
    static constexpr auto reflection_members = std::meta::members_of(^TransformComponent);
};

static_assert(sizeof(TransformComponent) == 48);
static_assert(alignof(TransformComponent) == 16);

/// Velocity компонент.
export struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};

/// Physics body компонент (opaque ID).
export struct PhysicsBodyComponent {
    uint64_t body_id{0};
    bool is_dynamic{true};
};

/// Mesh компонент.
export struct MeshComponent {
    uint64_t mesh_handle{0};
    uint64_t material_handle{0};
};

/// Voxel chunk компонент.
export struct VoxelChunkComponent {
    int32_t chunk_x{0};
    int32_t chunk_y{0};
    int32_t chunk_z{0};
    uint32_t flags{0};

    static constexpr uint32_t FLAG_NEEDS_REBUILD = 1 << 0;
    static constexpr uint32_t FLAG_IS_DIRTY = 1 << 1;
};

/// Player компонент.
export struct PlayerComponent {
    float move_speed{5.0f};
    float jump_force{8.0f};
    float mouse_sensitivity{0.1f};
    bool is_grounded{false};
};

} // namespace projectv::gameplay
```

---

### 4.2 ECS Module

#### State Machine

```
Entity Lifecycle
    ┌─────────┐
    │ CREATED │ ←── entity() called
    └────┬────┘
         │ set<T>() called
         ▼
┌─────────────────┐
│ INITIALIZED     │ ←── Components added
└────────┬────────┘
         │ progress() processes entity
         ▼
┌─────────────────┐
│ ACTIVE          │ ←── Normal operation
└────────┬────────┘
         │ destroy() called
         ▼
┌─────────────────┐
│ DESTROYED       │ ←── Entity removed
└─────────────────┘
```

#### API Contracts

```cpp
// ProjectV.ECS.Flecs.cppm
export module ProjectV.ECS.Flecs;

import std;
import glm;

export namespace projectv::ecs {

/// ECS World — PIMPL wrapper для flecs.
///
/// ## Invariants
/// - Entity IDs уникальны
/// - Component данные contiguous в памяти (DOD)
///
/// ## Thread Safety
/// - progress() — single-threaded
/// - get/set — thread-safe для разных entities
export class World {
public:
    World() noexcept;
    explicit World(int32_t thread_count) noexcept;
    ~World() noexcept;

    World(World&&) noexcept;
    World& operator=(World&&) noexcept;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// Выполняет шаг симуляции ECS.
    ///
    /// @param delta_time Временной шаг
    /// @return true если симуляция продолжается
    [[nodiscard]] auto progress(float delta_time) noexcept -> bool;

    /// Создаёт новую сущность.
    ///
    /// @post exists(id) == true
    /// @return ID сущности
    [[nodiscard]] auto entity() noexcept -> uint64_t;

    /// Создаёт именованную сущность.
    [[nodiscard]] auto entity(std::string_view name) noexcept -> uint64_t;

    /// Уничтожает сущность.
    ///
    /// @pre exists(entity_id)
    /// @post exists(entity_id) == false
    auto destroy(uint64_t entity_id) noexcept -> void;

    /// Проверяет существование сущности.
    [[nodiscard]] auto exists(uint64_t entity_id) const noexcept -> bool;

    /// Устанавливает компонент.
    ///
    /// @pre exists(entity_id)
    /// @post get<T>(entity_id) != nullptr
    template<typename T>
    auto set(uint64_t entity_id, T const& component) noexcept -> void;

    /// Получает компонент.
    ///
    /// @pre exists(entity_id)
    /// @return Указатель или nullptr если компонент отсутствует
    template<typename T>
    [[nodiscard]] auto get(uint64_t entity_id) const noexcept -> T const*;

    /// Получает mutable компонент.
    template<typename T>
    [[nodiscard]] auto get_mut(uint64_t entity_id) noexcept -> T*;

    /// Удаляет компонент.
    template<typename T>
    auto remove(uint64_t entity_id) noexcept -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs
```

---

## 5. SIMD Cellular Automata (C++26)

### 5.1 CA Simulation with std::simd

#### Memory Layout

```
CellState (32 bytes, GPU-aligned)
┌─────────────────────────────────────────────────────────────┐
│  density: float (4 bytes)                                   │
│  velocity_x: float (4 bytes)                                │
│  velocity_y: float (4 bytes)                                │
│  velocity_z: float (4 bytes)                                │
│  material_type: uint32_t (4 bytes)                          │
│  flags: uint32_t (4 bytes)                                  │
│  temperature: float (4 bytes)                               │
│  pressure: float (4 bytes)                                  │
│  Total: 32 bytes                                            │
└─────────────────────────────────────────────────────────────┘

CellChunk (for SIMD processing)
┌─────────────────────────────────────────────────────────────┐
│  cells: std::array<CellState, CHUNK_SIZE³>                  │
│  CHUNK_SIZE = 8 → 512 cells = 16 KB per chunk               │
│  Fits in L1 cache (32 KB typical)                           │
└─────────────────────────────────────────────────────────────┘
```

#### API Contracts

```cpp
// ProjectV.Simulation.CellularAutomata.cppm
export module ProjectV.Simulation.CellularAutomata;

import std;
import std.simd;
import glm;

export namespace projectv::simulation {

/// SIMD width для текущей платформы.
export constexpr size_t SIMD_WIDTH = std::simd_abi::max_fixed_size<float>;

/// Cell state (GPU-aligned, 32 bytes).
export struct alignas(16) CellState {
    float density{0.0f};
    float velocity_x{0.0f};
    float velocity_y{0.0f};
    float velocity_z{0.0f};
    uint32_t material_type{0};
    uint32_t flags{0};
    float temperature{0.0f};
    float pressure{0.0f};
};

static_assert(sizeof(CellState) == 32);

/// SIMD-optimized CA simulator.
///
/// ## Performance
/// - Uses std::simd for vectorized updates
/// - Processes cells in cache-friendly chunks
/// - Lock-free with std::hazard_pointer for concurrent reads
///
/// ## Complexity
/// - step(): O(n) where n = cell_count
/// - Memory bandwidth: ~32 bytes * cell_count per step
export class CASimulator {
public:
    /// Создаёт симулятор заданного размера.
    explicit CASimulator(uint32_t size_x, uint32_t size_y, uint32_t size_z) noexcept;

    /// Выполняет шаг симуляции.
    ///
    /// @param delta_time Временной шаг
    /// @post Все cells обновлены
    auto step(float delta_time) noexcept -> void;

    /// Устанавливает cell.
    auto set_cell(uint32_t x, uint32_t y, uint32_t z, CellState const& state) noexcept -> void;

    /// Получает cell.
    [[nodiscard]] auto get_cell(uint32_t x, uint32_t y, uint32_t z) const noexcept -> CellState;

    /// Lock-free доступ для чтения (hazard pointer).
    [[nodiscard]] auto acquire_cell(uint32_t x, uint32_t y, uint32_t z) const noexcept
        -> std::hazard_pointer<CellState>;

private:
    std::vector<CellState> cells_;
    uint32_t size_x_, size_y_, size_z_;

    /// SIMD-векторизованное обновление.
    auto update_velocity_simd(float delta_time) noexcept -> void;
    auto update_density_simd(float delta_time) noexcept -> void;
};

} // namespace projectv::simulation
```

---

## 6. MVP Build Mode

### 6.1 Обзор

**MVP Mode** — минимальная конфигурация сборки для быстрого получения первого рабочего прототипа.

```bash
# Сборка в MVP режиме
cmake -B build -DPROJECTV_MVP_MODE=ON
cmake --build build --config Release
```

### 6.2 Feature Matrix

| Feature              | MVP Mode | Full Build |
|----------------------|----------|------------|
| Vulkan Rendering     | ✅        | ✅          |
| Voxel Rendering      | ✅        | ✅          |
| Job System (stdexec) | ✅        | ✅          |
| ECS (Flecs)          | ✅        | ✅          |
| Physics (Jolt)       | ❌        | ✅          |
| Cellular Automata    | ❌        | ✅          |
| GUI (RmlUI/ImGui)    | ❌        | ✅          |
| Asset Loading        | ❌        | ✅          |
| Audio                | ❌        | ✅          |
| Networking           | ❌        | ✅          |
| Tracy Profiling      | ❌        | ✅          |
| Draco Compression    | ❌        | ✅          |

### 6.3 MVP Module List

```
MVP_MODULES = [
    # Layer 0: Foundation
    Core.Types,
    Core.Memory,
    Core.Jobs,
    Core.ECS,

    # Layer 1: Rendering
    Render.Vulkan,
    Render.Window,
    Render.Pipeline,
    Render.Descriptors,

    # Layer 2: Voxel
    Voxel.Data,
    Voxel.Mesh,
    Voxel.Render,
    Voxel.World,

    # Layer 3: Game
    Game.Main,
    Game.Input,
    Game.Settings,
    Game.Serialization
]
```

### 6.4 CMake Configuration

```cmake
# cmake/MVPMode.cmake

option(PROJECTV_MVP_MODE "Build only MVP features" OFF)

if (PROJECTV_MVP_MODE)
  message(STATUS "Building in MVP Mode - minimal feature set")

  # Disable non-essential features
  set(PROJECTV_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_CA OFF CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_GUI OFF CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_ASSETS OFF CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_AUDIO OFF CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_NETWORK OFF CACHE BOOL "" FORCE)

  # Enable core features
  set(PROJECTV_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_VOXEL ON CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_JOBS ON CACHE BOOL "" FORCE)
  set(PROJECTV_ENABLE_ECS ON CACHE BOOL "" FORCE)

  # Minimal dependencies
  set(PROJECTV_USE_TRACY OFF CACHE BOOL "" FORCE)
  set(PROJECTV_USE_JOLT OFF CACHE BOOL "" FORCE)
  set(PROJECTV_USE_RMLUI OFF CACHE BOOL "" FORCE)
  set(PROJECTV_USE_DRACO OFF CACHE BOOL "" FORCE)
endif ()
```

### 6.5 MVP Success Criteria

Для завершения MVP требуется:

- [ ] Приложение запускается без падений
- [ ] Vulkan инициализируется без ошибок validation layers
- [ ] Один chunk (32³ blocks) рендерится на экране
- [ ] Camera movement (WASD + mouse) работает
- [ ] Chunk generation при движении camera
- [ ] Graceful shutdown без утечек памяти (проверить через Valgrind/ASan)

---

## Ссылки

- [ADR-0004: Build System & C++26 Modules](../adr/0004-build-and-modules-spec.md)
- [DOD Philosophy](../../philosophy/03_dod-philosophy.md)
- [ECS Philosophy](../../philosophy/04_ecs-philosophy.md)
- [SVO Architecture](./00_svo-architecture.md)
- [Vulkan 1.4 Dynamic Rendering](./28_render_graph_spec.md)
