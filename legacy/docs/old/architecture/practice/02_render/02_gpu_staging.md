# GPU Staging Contracts Specification

---

## Обзор

Документ определяет API-контракты для передачи данных Voxel Chunks с CPU на GPU через Vulkan 1.4 Staging-пайплайн.
Ключевые принципы:

1. **Zero-copy where possible** — минимум копирований между буферами
2. **Ring Buffer allocation** — эффективное переиспользование staging-памяти
3. **Timeline Semaphores** — синхронизация без spin-wait
4. **Synchronization2 abstraction** — скрытие сложности барьеров от клиентского кода

---

## Memory Layout

### StagingBuffer

```
StagingBuffer (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── buffer_: VkBuffer (8 bytes)                        │
│      ├── allocation_: VmaAllocation (8 bytes)               │
│      ├── mapped_ptr_: void* (8 bytes)                       │
│      ├── capacity_: VkDeviceSize (8 bytes)                  │
│      ├── alignment_: VkDeviceSize (8 bytes)                 │
│      └── memory_property_flags_: VkMemoryPropertyFlags (4)  │
│  Total: 8 bytes (external) + 44 bytes (internal)            │
└─────────────────────────────────────────────────────────────┘

RingBufferDescriptor
┌─────────────────────────────────────────────────────────────┐
│  head: uint64_t (8 bytes)        ── write offset            │
│  tail: uint64_t (8 bytes)        ── read offset             │
│  capacity: uint64_t (8 bytes)    ── total size              │
│  frame_head: uint64_t[FRAME_COUNT] (16 bytes for 2 frames)  │
│  padding: 8 bytes                                           │
│  Total: 48 bytes                                            │
└─────────────────────────────────────────────────────────────┘

StagingAllocation
┌─────────────────────────────────────────────────────────────┐
│  offset: VkDeviceSize (8 bytes)                             │
│  size: VkDeviceSize (8 bytes)                               │
│  mapped_ptr: void* (8 bytes)                                │
│  frame_index: uint32_t (4 bytes)                            │
│  signal_value: uint64_t (8 bytes)                           │
│  padding: 4 bytes                                           │
│  Total: 40 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

### VoxelChunkTransferData

```
VoxelChunkGpuData (std430 aligned)
┌─────────────────────────────────────────────────────────────┐
│  header:                                                    │
│  ├── chunk_x: int32_t (4 bytes)                             │
│  ├── chunk_y: int32_t (4 bytes)                             │
│  ├── chunk_z: int32_t (4 bytes)                             │
│  ├── voxel_count: uint32_t (4 bytes)                        │
│  ├── material_mask: uint64_t (8 bytes)                      │
│  └── padding: 8 bytes                                       │
│  Total header: 32 bytes                                     │
│                                                             │
│  voxel_data: VoxelData[] (4 bytes each)                     │
│  └── material_id: uint16_t (2 bytes)                        │
│  └── flags: uint8_t (1 byte)                                │
│  └── light: uint8_t (1 byte)                                │
│                                                             │
│  Total: 32 + voxel_count * 4 bytes                          │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machine

### Chunk Transfer State

```
ChunkTransferState
       ┌────────────┐
       │ CPU_DIRTY  │ ←── Данные изменены на CPU
       └─────┬──────┘
             │ allocate_staging()
             ▼
       ┌────────────┐
       │  STAGING   │ ←── Данные в staging buffer
       └─────┬──────┘
             │ record_transfer_commands()
             ▼
┌───────────────────────┐
│ TRANSFER_CMD_RECORDED │ ←── Commands в command buffer
└───────────┬───────────┘
            │ submit() + signal semaphore
            ▼
       ┌────────────┐
       │ GPU_READY  │ ←── Timeline semaphore signaled
       └─────┬──────┘
             │ reset() (after frame complete)
             ▼
       ┌────────────┐
       │  COMPLETE  │ ←── Можно переиспользовать
       └────────────┘

Error States:
┌─────────────────────┐
│ ALLOCATION_FAILED   │ ←── Недостаточно памяти в ring buffer
└─────────────────────┘
```

### Ring Buffer State

```
Ring Buffer Allocations Over Time

Frame N:
┌─────────────────────────────────────────────────────────────┐
│ [AAAAAA][BBBB][CCCCCCCC][DDDDD][     FREE SPACE     ]       │
│         ▲                                                   │
│         head                                                │

Frame N+1 (head wraps around):
┌─────────────────────────────────────────────────────────────┐
│ [EEEEEE][FFFFFFFF][   FREE   ][AAAAAA][BBBB][CCCCCCCC]      │
│                          ▲                                  │
│                          head                               │

Frame N+2 (after garbage collection):
┌─────────────────────────────────────────────────────────────┐
│ [GGGGG][HHHH][IIIIIIII][          FREE SPACE          ]     │
│         ▲                                                   │
│         head                                                │
└─────────────────────────────────────────────────────────────┘
```

---

## API Contracts

### StagingManager

```cpp
// ProjectV.Render.Staging.cppm
export module ProjectV.Render.Staging;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.Render.Sync2;

export namespace projectv::render {

/// Коды ошибок Staging.
export enum class StagingError : uint8_t {
    OutOfMemory,
    BufferTooLarge,
    InvalidAlignment,
    TransferFailed,
    NotInitialized,
    AlreadySubmitted
};

/// Конфигурация Staging Manager.
export struct StagingConfig {
    VkDeviceSize ring_buffer_size{256 * 1024 * 1024};  ///< 256 MB default
    VkDeviceSize alignment{256};                        ///< Alignment for allocations
    uint32_t max_frames_in_flight{2};                   ///< Frames in flight
    bool prefer_host_visible{true};                     ///< Prefer HOST_VISIBLE memory
    bool prefer_device_local{true};                     ///< Use DEVICE_LOCAL if available
};

/// Staging allocation result.
export struct StagingAllocation {
    VkDeviceSize offset{0};
    VkDeviceSize size{0};
    void* mapped_ptr{nullptr};
    uint32_t frame_index{0};
    uint64_t signal_value{0};      ///< Timeline semaphore value when done

    /// Проверяет валидность.
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return mapped_ptr != nullptr && size > 0;
    }
};

/// Staging Manager — Ring Buffer для CPU→GPU transfers.
///
/// ## Memory Model
/// - Использует VMA для аллокации
/// - Предпочитает HOST_VISIBLE | HOST_COHERENT | DEVICE_LOCAL
/// - Fallback на HOST_VISIBLE только
///
/// ## Thread Safety
/// - allocate(): thread-safe (spinlock internally)
/// - submit(): must be called from main thread
/// - wait(): thread-safe
///
/// ## Invariants
/// - ring buffer never wraps mid-allocation
/// - signal_value монотонно возрастает
/// - after wait(value), все allocations с signal_value <= value завершены
export class StagingManager {
public:
    /// Создаёт Staging Manager.
    ///
    /// @param device Vulkan device
    /// @param allocator VMA allocator
    /// @param config Конфигурация
    ///
    /// @pre device != VK_NULL_HANDLE
    /// @pre allocator != VK_NULL_HANDLE
    ///
    /// @post ready for allocations
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        StagingConfig const& config = {}
    ) noexcept -> std::expected<StagingManager, StagingError>;

    ~StagingManager() noexcept;

    StagingManager(StagingManager&&) noexcept;
    StagingManager& operator=(StagingManager&&) noexcept;
    StagingManager(const StagingManager&) = delete;
    StagingManager& operator=(const StagingManager&) = delete;

    /// Выделяет память в ring buffer.
    ///
    /// @param size Размер в байтах
    /// @param alignment Выравнивание (default from config)
    ///
    /// @pre size > 0
    /// @pre size < ring_buffer_size
    /// @pre !frame_full()
    ///
    /// @return Allocation или ошибка
    ///
    /// @note Thread-safe. Использует spinlock.
    [[nodiscard]] auto allocate(
        VkDeviceSize size,
        VkDeviceSize alignment = 0
    ) noexcept -> std::expected<StagingAllocation, StagingError>;

    /// Выделяет память и копирует данные.
    ///
    /// @param data Указатель на данные
    /// @param size Размер в байтах
    ///
    /// @pre data != nullptr
    /// @pre size > 0
    ///
    /// @return Allocation с уже скопированными данными
    [[nodiscard]] auto allocate_and_copy(
        void const* data,
        VkDeviceSize size
    ) noexcept -> std::expected<StagingAllocation, StagingError>;

    /// Выделяет память для std::span.
    ///
    /// @tparam T тип элементов
    /// @param data данные
    ///
    /// @return Allocation
    template<typename T>
    [[nodiscard]] auto allocate_and_copy(std::span<T const> data) noexcept
        -> std::expected<StagingAllocation, StagingError>;

    /// Записывает команды копирования.
    ///
    /// @param cmd Command buffer
    /// @param allocation Staging allocation
    /// @param dst_buffer Destination buffer
    /// @param dst_offset Offset in destination
    ///
    /// @pre cmd в recording state
    /// @pre allocation.is_valid()
    /// @pre dst_buffer != VK_NULL_HANDLE
    ///
    /// @post Transfer commands recorded
    /// @post Barrier inserted for dst_buffer
    auto record_transfer(
        VkCommandBuffer cmd,
        StagingAllocation const& allocation,
        VkBuffer dst_buffer,
        VkDeviceSize dst_offset = 0
    ) noexcept -> void;

    /// Записывает команды копирования в image.
    ///
    /// @param cmd Command buffer
    /// @param allocation Staging allocation
    /// @param dst_image Destination image
    /// @param dst_layout Destination image layout
    /// @param region Buffer-image copy region
    ///
    /// @pre cmd в recording state
    /// @pre allocation.is_valid()
    auto record_transfer_to_image(
        VkCommandBuffer cmd,
        StagingAllocation const& allocation,
        VkImage dst_image,
        VkImageLayout dst_layout,
        VkBufferImageCopy const& region
    ) noexcept -> void;

    /// Получает timeline semaphore для синхронизации.
    ///
    /// @return Timeline semaphore handle
    [[nodiscard]] auto timeline_semaphore() const noexcept -> VkSemaphore;

    /// Получает текущее значение semaphore.
    [[nodiscard]] auto current_signal_value() const noexcept -> uint64_t;

    /// Ожидает завершения transfers до указанного значения.
    ///
    /// @param value Signal value
    /// @param timeout_ns Timeout
    ///
    /// @return true если завершено
    [[nodiscard]] auto wait(
        uint64_t value,
        uint64_t timeout_ns = UINT64_MAX
    ) const noexcept -> bool;

    /// Сбрасывает frame allocations.
    ///
    /// @param frame_index Frame index
    ///
    /// @pre Все transfers для frame_index завершены
    /// @post Ring buffer space reclaimed
    auto reset_frame(uint32_t frame_index) noexcept -> void;

    /// Получает размер ring buffer.
    [[nodiscard]] auto capacity() const noexcept -> VkDeviceSize;

    /// Получает использованное пространство.
    [[nodiscard]] auto used() const noexcept -> VkDeviceSize;

    /// Получает свободное пространство.
    [[nodiscard]] auto available() const noexcept -> VkDeviceSize;

    /// Проверяет, полон ли текущий frame.
    [[nodiscard]] auto frame_full() const noexcept -> bool;

    /// Получает staging buffer.
    [[nodiscard]] auto buffer() const noexcept -> VkBuffer;

private:
    StagingManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render
```

### ChunkTransferManager

```cpp
// ProjectV.Voxel.ChunkTransfer.cppm
export module ProjectV.Voxel.ChunkTransfer;

import std;
import glm;
import ProjectV.Render.Staging;
import ProjectV.Render.Vulkan;
import ProjectV.Voxel.SVO;

export namespace projectv::voxel {

/// Состояние трансфера чанка.
export enum class ChunkTransferState : uint8_t {
    CpuDirty,               ///< Данные изменены на CPU
    Staging,                ///< В staging buffer
    TransferCmdRecorded,    ///< Commands записаны
    GpuReady,               ///< Transfer завершён
    Complete,               ///< Готов к переиспользованию
    Error                   ///< Ошибка трансфера
};

/// Результат трансфера.
export struct ChunkTransferResult {
    ChunkTransferState state{ChunkTransferState::CpuDirty};
    uint64_t chunk_id{0};
    uint64_t signal_value{0};
    StagingAllocation allocation{};
    std::string error_message;
};

/// Данные чанка для трансфера.
export struct ChunkTransferData {
    uint64_t chunk_id{0};
    glm::ivec3 chunk_coord{0};
    std::span<std::byte const> voxel_data;
    std::span<std::byte const> material_data;  ///< Optional
};

/// GPU buffer для voxel chunks.
export struct VoxelGpuBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkDeviceSize capacity{0};
    VkDeviceSize used{0};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return buffer != VK_NULL_HANDLE;
    }
};

/// Chunk Transfer Manager — управление трансферами воксельных чанков.
///
/// ## Architecture
/// - Отслеживает dirty chunks
/// - Пакетирует мелкие чанки в batch transfers
/// - Использует Timeline Semaphores для синхронизации
///
/// ## Thread Safety
/// - mark_dirty(): thread-safe
/// - process_transfers(): main thread only
/// - get_chunk_state(): thread-safe for reads
///
/// ## Invariants
/// - Chunk data не изменяется во время transfer
/// - GPU buffer имеет достаточный размер для всех active transfers
export class ChunkTransferManager {
public:
    /// Создаёт Chunk Transfer Manager.
    ///
    /// @param staging Staging Manager
    /// @param gpu_buffer GPU buffer для voxel data
    /// @param max_chunks_per_frame Максимум чанков за кадр
    [[nodiscard]] static auto create(
        render::StagingManager& staging,
        VoxelGpuBuffer const& gpu_buffer,
        uint32_t max_chunks_per_frame = 64
    ) noexcept -> std::expected<ChunkTransferManager, TransferError>;

    ~ChunkTransferManager() noexcept;

    ChunkTransferManager(ChunkTransferManager&&) noexcept;
    ChunkTransferManager& operator=(ChunkTransferManager&&) noexcept;
    ChunkTransferManager(const ChunkTransferManager&) = delete;
    ChunkTransferManager& operator=(const ChunkTransferManager&) = delete;

    /// Помечает чанк как dirty.
    ///
    /// @param chunk_id ID чанка
    /// @param data Данные чанка
    ///
    /// @post Chunk добавлен в dirty queue
    auto mark_dirty(uint64_t chunk_id, ChunkTransferData const& data) noexcept -> void;

    /// Помечает чанк для удаления.
    auto mark_for_removal(uint64_t chunk_id) noexcept -> void;

    /// Обрабатывает pending transfers.
    ///
    /// @param cmd Command buffer
    /// @param max_chunks Максимум чанков для обработки
    ///
    /// @return Количество обработанных чанков
    auto process_transfers(
        VkCommandBuffer cmd,
        uint32_t max_chunks = UINT32_MAX
    ) noexcept -> uint32_t;

    /// Получает состояние чанка.
    [[nodiscard]] auto get_chunk_state(uint64_t chunk_id) const noexcept
        -> ChunkTransferState;

    /// Получает GPU offset чанка.
    [[nodiscard]] auto get_chunk_gpu_offset(uint64_t chunk_id) const noexcept
        -> std::expected<VkDeviceSize, TransferError>;

    /// Ожидает завершения всех transfers.
    auto wait_all(uint64_t timeout_ns = UINT64_MAX) noexcept -> bool;

    /// Ожидает завершения transfer для чанка.
    auto wait_chunk(uint64_t chunk_id, uint64_t timeout_ns = UINT64_MAX) noexcept
        -> bool;

    /// Сбрасывает завершённые transfers.
    auto collect_completed() noexcept -> void;

    /// Получает количество dirty chunks.
    [[nodiscard]] auto dirty_count() const noexcept -> size_t;

    /// Получает количество pending transfers.
    [[nodiscard]] auto pending_count() const noexcept -> size_t;

    /// Получает сигнал value для последнего transfer.
    [[nodiscard]] auto last_signal_value() const noexcept -> uint64_t;

private:
    ChunkTransferManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Synchronization2 Barrier Abstraction

### BarrierBuilder для Staging

```cpp
// ProjectV.Render.Staging.Barriers.cppm
export module ProjectV.Render.Staging.Barriers;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.Render.Sync2;

export namespace projectv::render::staging {

/// Предустановленные barrier patterns для staging.
///
/// ## Purpose
/// Скрыть сложность VkMemoryBarrier2 от клиентского кода,
/// предоставляя семантически понятные операции.
export class StagingBarriers {
public:
    /// Barrier после записи в staging buffer (host write → transfer read).
    ///
    /// @param cmd Command buffer
    static auto host_write_to_transfer_read(VkCommandBuffer cmd) noexcept -> void {
        BarrierBuilder builder(cmd);

        builder.add_memory_barrier(
            VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_HOST_WRITE_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT
        );

        builder.execute();
    }

    /// Barrier после copy (transfer write → vertex shader read).
    ///
    /// @param cmd Command buffer
    /// @param dst_buffer Destination buffer
    static auto transfer_write_to_vertex_read(
        VkCommandBuffer cmd,
        VkBuffer dst_buffer
    ) noexcept -> void {
        BarrierBuilder builder(cmd);

        builder.add_buffer_barrier(
            dst_buffer,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );

        builder.execute();
    }

    /// Barrier после copy (transfer write → compute shader read).
    ///
    /// @param cmd Command buffer
    /// @param dst_buffer Destination buffer
    static auto transfer_write_to_compute_read(
        VkCommandBuffer cmd,
        VkBuffer dst_buffer
    ) noexcept -> void {
        BarrierBuilder builder(cmd);

        builder.add_buffer_barrier(
            dst_buffer,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );

        builder.execute();
    }

    /// Barrier после copy to image (transfer write → fragment shader read).
    ///
    /// @param cmd Command buffer
    /// @param dst_image Destination image
    /// @param aspect_mask Image aspect
    static auto transfer_write_to_shader_read(
        VkCommandBuffer cmd,
        VkImage dst_image,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
    ) noexcept -> void {
        BarrierBuilder builder(cmd);

        builder.add_image_barrier(
            dst_image,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            aspect_mask
        );

        builder.execute();
    }

    /// Full transition: host write → transfer → shader read.
    ///
    /// @param cmd Command buffer
    /// @param dst_buffer Destination buffer
    /// @param dst_stage Final pipeline stage
    static auto full_transfer_pipeline(
        VkCommandBuffer cmd,
        VkBuffer dst_buffer,
        VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
    ) noexcept -> void {
        // Host writes are visible via HOST_COHERENT memory
        // No explicit barrier needed before transfer

        // Transfer → Shader barrier
        BarrierBuilder builder(cmd);

        builder.add_buffer_barrier(
            dst_buffer,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            dst_stage,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
        );

        builder.execute();
    }
};

} // namespace projectv::render::staging
```

---

## Implementation Details

### StagingManager Implementation

```cpp
// ProjectV.Render.Staging.cpp
module ProjectV.Render.Staging;

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

import std;
import ProjectV.Render.Vulkan;
import ProjectV.Render.Sync2;

namespace projectv::render {

struct StagingManager::Impl {
    VkDevice device{VK_NULL_HANDLE};
    VmaAllocator allocator{VK_NULL_HANDLE};

    // Ring buffer
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    void* mapped_ptr{nullptr};
    VkDeviceSize capacity{0};
    VkDeviceSize alignment{256};

    // Ring buffer state
    std::atomic<uint64_t> head{0};      // Write offset
    std::atomic<uint64_t> tail{0};      // Read offset (for reclamation)
    uint64_t frame_heads[MAX_FRAMES_IN_FLIGHT]{};
    uint32_t current_frame{0};

    // Synchronization
    TimelineSemaphore timeline_semaphore;
    std::atomic<uint64_t> next_signal_value{1};

    // Spinlock for allocations
    std::atomic_flag lock = ATOMIC_FLAG_INIT;

    // Pending allocations for current frame
    struct PendingAllocation {
        VkDeviceSize offset;
        VkDeviceSize size;
        uint64_t signal_value;
    };
    std::vector<PendingAllocation> pending_allocations;
};

auto StagingManager::create(
    VkDevice device,
    VmaAllocator allocator,
    StagingConfig const& config
) noexcept -> std::expected<StagingManager, StagingError> {

    StagingManager manager;
    manager.impl_ = std::make_unique<Impl>();

    manager.impl_->device = device;
    manager.impl_->allocator = allocator;
    manager.impl_->capacity = config.ring_buffer_size;
    manager.impl_->alignment = config.alignment;

    // Create buffer
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = config.ring_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    // Memory requirements
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    // Try HOST_VISIBLE | DEVICE_LOCAL first
    if (config.prefer_device_local) {
        alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo vma_info{};
    VkResult result = vmaCreateBuffer(
        allocator,
        &buffer_info,
        &alloc_info,
        &manager.impl_->buffer,
        &manager.impl_->allocation,
        &vma_info
    );

    if (result != VK_SUCCESS) {
        return std::unexpected(StagingError::OutOfMemory);
    }

    manager.impl_->mapped_ptr = vma_info.pMappedData;

    // Create timeline semaphore
    auto sem_result = TimelineSemaphore::create(device, 0);
    if (!sem_result) {
        vmaDestroyBuffer(allocator, manager.impl_->buffer, manager.impl_->allocation);
        return std::unexpected(StagingError::OutOfMemory);
    }
    manager.impl_->timeline_semaphore = std::move(*sem_result);

    return manager;
}

StagingManager::~StagingManager() noexcept {
    if (impl_) {
        if (impl_->buffer) {
            vmaDestroyBuffer(impl_->allocator, impl_->buffer, impl_->allocation);
        }
    }
}

auto StagingManager::allocate(
    VkDeviceSize size,
    VkDeviceSize alignment
) noexcept -> std::expected<StagingAllocation, StagingError> {

    if (!impl_ || !impl_->mapped_ptr) {
        return std::unexpected(StagingError::NotInitialized);
    }

    const VkDeviceSize actual_alignment = alignment > 0 ? alignment : impl_->alignment;
    const VkDeviceSize aligned_size = (size + actual_alignment - 1) & ~(actual_alignment - 1);

    // Spinlock for thread-safety
    while (impl_->lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Scope guard for lock release
    auto lock_guard = std::unique_lock<decltype(impl_->lock)>(impl_->lock, std::adopt_lock);

    // Calculate aligned offset
    const uint64_t current_head = impl_->head.load(std::memory_order_relaxed);
    const uint64_t aligned_offset = (current_head + actual_alignment - 1) & ~(actual_alignment - 1);

    // Check for wrap-around
    if (aligned_offset + aligned_size > impl_->capacity) {
        // Wrap around to beginning
        const uint64_t new_offset = 0;

        // Check if we have space at the beginning
        const uint64_t current_tail = impl_->tail.load(std::memory_order_acquire);
        if (new_offset + aligned_size >= current_tail) {
            // Not enough space
            return std::unexpected(StagingError::OutOfMemory);
        }

        impl_->head.store(aligned_size, std::memory_order_release);
    } else {
        // Check if we have space
        const uint64_t current_tail = impl_->tail.load(std::memory_order_acquire);
        const uint64_t available = (current_tail > current_head)
            ? (impl_->capacity - current_head)
            : (current_tail - current_head);

        if (aligned_size > available) {
            return std::unexpected(StagingError::OutOfMemory);
        }

        impl_->head.store(aligned_offset + aligned_size, std::memory_order_release);
    }

    const uint64_t signal_value = impl_->next_signal_value.fetch_add(1, std::memory_order_relaxed);

    StagingAllocation allocation{
        .offset = aligned_offset,
        .size = size,
        .mapped_ptr = static_cast<std::byte*>(impl_->mapped_ptr) + aligned_offset,
        .frame_index = impl_->current_frame,
        .signal_value = signal_value
    };

    impl_->pending_allocations.push_back({
        allocation.offset,
        aligned_size,
        signal_value
    });

    return allocation;
}

auto StagingManager::allocate_and_copy(
    void const* data,
    VkDeviceSize size
) noexcept -> std::expected<StagingAllocation, StagingError> {

    auto allocation_result = allocate(size);
    if (!allocation_result) {
        return allocation_result;
    }

    // Copy data
    std::memcpy(allocation_result->mapped_ptr, data, size);

    return allocation_result;
}

template<typename T>
auto StagingManager::allocate_and_copy(std::span<T const> data) noexcept
    -> std::expected<StagingAllocation, StagingError> {

    return allocate_and_copy(data.data(), data.size_bytes());
}

auto StagingManager::record_transfer(
    VkCommandBuffer cmd,
    StagingAllocation const& allocation,
    VkBuffer dst_buffer,
    VkDeviceSize dst_offset
) noexcept -> void {

    // Copy command
    VkBufferCopy copy_region{
        .srcOffset = allocation.offset,
        .dstOffset = dst_offset,
        .size = allocation.size
    };

    vkCmdCopyBuffer(cmd, impl_->buffer, dst_buffer, 1, &copy_region);

    // Barrier: transfer write → shader read
    staging::StagingBarriers::transfer_write_to_vertex_read(cmd, dst_buffer);
}

auto StagingManager::record_transfer_to_image(
    VkCommandBuffer cmd,
    StagingAllocation const& allocation,
    VkImage dst_image,
    VkImageLayout dst_layout,
    VkBufferImageCopy const& region
) noexcept -> void {

    // Transition image to transfer dst
    {
        BarrierBuilder builder(cmd);
        builder.add_image_barrier(
            dst_image,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            0,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            region.imageSubresource.aspectMask
        );
        builder.execute();
    }

    // Copy command
    VkBufferImageCopy copy_region = region;
    copy_region.bufferOffset = allocation.offset;

    vkCmdCopyBufferToImage(cmd, impl_->buffer, dst_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    // Transition to shader read
    staging::StagingBarriers::transfer_write_to_shader_read(
        cmd, dst_image, region.imageSubresource.aspectMask);
}

auto StagingManager::timeline_semaphore() const noexcept -> VkSemaphore {
    return impl_->timeline_semaphore.native();
}

auto StagingManager::current_signal_value() const noexcept -> uint64_t {
    return impl_->next_signal_value.load(std::memory_order_relaxed);
}

auto StagingManager::wait(uint64_t value, uint64_t timeout_ns) const noexcept -> bool {
    return impl_->timeline_semaphore.wait(value, timeout_ns);
}

auto StagingManager::reset_frame(uint32_t frame_index) noexcept -> void {
    // Wait for transfers to complete
    const uint64_t frame_signal = impl_->frame_heads[frame_index];
    if (frame_signal > 0) {
        wait(frame_signal);
    }

    // Update tail
    impl_->tail.store(impl_->frame_heads[frame_index], std::memory_order_release);

    // Clear pending allocations for this frame
    impl_->pending_allocations.clear();
}

auto StagingManager::capacity() const noexcept -> VkDeviceSize {
    return impl_->capacity;
}

auto StagingManager::used() const noexcept -> VkDeviceSize {
    const uint64_t head = impl_->head.load(std::memory_order_relaxed);
    const uint64_t tail = impl_->tail.load(std::memory_order_relaxed);
    return head >= tail ? (head - tail) : (impl_->capacity - tail + head);
}

auto StagingManager::available() const noexcept -> VkDeviceSize {
    return impl_->capacity - used();
}

auto StagingManager::buffer() const noexcept -> VkBuffer {
    return impl_->buffer;
}

// Explicit template instantiation
template auto StagingManager::allocate_and_copy(std::span<float const>) noexcept
    -> std::expected<StagingAllocation, StagingError>;
template auto StagingManager::allocate_and_copy(std::span<uint32_t const>) noexcept
    -> std::expected<StagingAllocation, StagingError>;

} // namespace projectv::render
```

---

## Usage Example

### Voxel Chunk Upload

```cpp
// Пример использования StagingManager для загрузки чанка

auto upload_voxel_chunk(
    render::StagingManager& staging,
    VkCommandBuffer cmd,
    VoxelChunk const& chunk,
    VkBuffer gpu_voxel_buffer
) -> std::expected<void, render::StagingError> {

    // 1. Сериализация данных чанка
    std::vector<std::byte> chunk_data = serialize_chunk(chunk);

    // 2. Аллокация в staging buffer
    auto allocation = staging.allocate_and_copy(
        chunk_data.data(),
        chunk_data.size()
    );

    if (!allocation) {
        return std::unexpected(allocation.error());
    }

    // 3. Запись команды копирования
    staging.record_transfer(
        cmd,
        *allocation,
        gpu_voxel_buffer,
        chunk.gpu_offset()
    );

    // 4. Signal timeline semaphore после завершения
    // (выполняется через submit info)

    return {};
}

// Submit с timeline semaphore
auto submit_transfer_commands(
    VkQueue queue,
    VkCommandBuffer cmd,
    render::StagingManager& staging
) -> void {

    // Build submit info
    render::SubmitInfoBuilder builder;

    builder.add_command_buffer(cmd);

    // Signal timeline semaphore
    builder.add_signal_semaphore(
        staging.timeline_semaphore(),
        staging.current_signal_value(),
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    VkSubmitInfo2 submit_info = builder.build();

    vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE);
}

// Wait for completion
auto wait_for_chunk_upload(
    render::StagingManager& staging,
    uint64_t signal_value
) -> bool {
    return staging.wait(signal_value);
}
```

---

## Performance Metrics

| Метрика                 | Цель      | Измерение         |
|-------------------------|-----------|-------------------|
| Allocation Time         | < 100ns   | Tracy CPU         |
| Transfer Bandwidth      | > 10 GB/s | Vulkan timestamps |
| Ring Buffer Utilization | < 80%     | VMA stats         |
| Staging Memory          | < 512 MB  | VMA stats         |
