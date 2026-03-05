# CPU-GPU Physics Synchronization Specification

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-22
**Версия:** 1.0

---

## Обзор

Документ описывает архитектуру синхронизации между **GPU Cellular Automata (CA)** и **CPU JoltPhysics** для:

1. **GPU-to-CPU Readback** — асинхронная передача данных CA (жидкости, песок) в JoltPhysics
2. **Buoyancy & Viscosity** — расчёт плавучести и вязкости для тел в жидкости
3. **Zero-Stall Pipeline** — отсутствие блокировок графического пайплайна

---

## 1. Architecture Overview

### 1.1 Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           GPU (Compute Shaders)                              │
│  ┌───────────────┐    ┌───────────────┐    ┌───────────────┐                │
│  │   CA Grid     │───▶│  Staging      │───▶│   Readback    │                │
│  │  (Current)    │    │   Buffer      │    │   Ring        │                │
│  │  [RW]         │    │  [Transfer]   │    │  [CPU Read]   │                │
│  └───────────────┘    └───────────────┘    └───────────────┘                │
│         │                    ▲                    │                          │
│         │                    │                    │ VkFence                  │
│         ▼                    │                    ▼                          │
│  ┌───────────────┐           │           ┌───────────────┐                  │
│  │   CA Grid     │           │           │   Timeline    │                  │
│  │   (Next)      │           │           │   Semaphore   │                  │
│  │  [RW]         │           │           └───────────────┘                  │
│  └───────────────┘           │                    │                          │
└──────────────────────────────│────────────────────│──────────────────────────┘
                               │                    │
                               │    VkCommandBuffer │
                               │                    │
└──────────────────────────────│────────────────────│──────────────────────────┐
│                           CPU (JoltPhysics)     │                          │
│  ┌───────────────┐           │           ┌───────────────┐                  │
│  │  Physics      │◀──────────┘           │   Readback    │                  │
│  │   System      │    memcpy             │   Manager     │                  │
│  │  [Jolt]       │                       │   [Ring]      │                  │
│  └───────────────┘                       └───────────────┘                  │
│         │                                        │                          │
│         │ BuoyancyForce                          │ waitForFence()           │
│         ▼                                        ▼                          │
│  ┌───────────────┐                       ┌───────────────┐                  │
│  │   Rigid       │                       │   Density     │                  │
│  │   Bodies      │                       │   Field       │                  │
│  └───────────────┘                       └───────────────┘                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Key Components

| Компонент           | Назначение                            | Vulkan Features                              |
|---------------------|---------------------------------------|----------------------------------------------|
| `CAReadbackRing`    | Ring buffer для асинхронного readback | `VkBuffer`, `VkFence`                        |
| `StagingBufferPool` | Пул staging buffers для transfer      | `VMA`, `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` |
| `TimelineSemaphore` | Синхронизация GPU→CPU                 | `VkSemaphoreType::TIMELINE`                  |
| `PhysicsBridge`     | Мост между CA данными и Jolt          | C++26, JoltPhysics API                       |

---

## 2. GPU-to-CPU Readback Pipeline

### 2.1 Ring Buffer Architecture

```cpp
// ProjectV.Simulation.CAReadback.cppm
export module ProjectV.Simulation.CAReadback;

import std;
import glm;

export namespace projectv::simulation {

/// Ring buffer configuration
export struct CAReadbackConfig {
    uint32_t frame_count{3};            ///< Frames in-flight (triple buffering)
    uint32_t grid_size_x{256};
    uint32_t grid_size_y{128};
    uint32_t grid_size_z{256};
    size_t cell_size_bytes{32};         ///< sizeof(CellState)
};

/// Readback frame state
export struct ReadbackFrame {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkFence fence{VK_NULL_HANDLE};      ///< Signaled when transfer complete
    uint64_t timeline_value{0};         ///< Timeline semaphore value
    bool is_available{true};            ///< Ready for new transfer
    bool has_pending_data{false};       ///< Data ready for CPU read
};

/// Ring buffer for asynchronous GPU→CPU readback
export class CAReadbackRing {
public:
    /// Creates ring buffer with configuration.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        CAReadbackConfig const& config
    ) noexcept -> std::expected<CAReadbackRing, ReadbackError>;

    ~CAReadbackRing() noexcept;

    // Move-only
    CAReadbackRing(CAReadbackRing&&) noexcept;
    CAReadbackRing& operator=(CAReadbackRing&&) noexcept;
    CAReadbackRing(const CAReadbackRing&) = delete;
    CAReadbackRing& operator=(const CAReadbackRing&) = delete;

    /// Gets buffer for GPU write (current frame).
    [[nodiscard]] auto get_current_buffer() const noexcept -> VkBuffer;

    /// Advances to next frame (called after GPU dispatch).
    auto advance_frame(VkCommandBuffer cmd, VkQueue queue) noexcept -> void;

    /// Checks if data is available for CPU read.
    [[nodiscard]] auto has_available_data() const noexcept -> bool;

    /// Waits for and maps the oldest completed transfer.
    /// Returns pointer to readback data.
    [[nodiscard]] auto map_oldest_frame(uint64_t timeout_ns = 16'000'000) noexcept
        -> std::expected<std::span<std::byte const>, ReadbackError>;

    /// Unmaps and releases the frame for reuse.
    auto unmap_and_release() noexcept -> void;

    /// Gets timeline semaphore for synchronization.
    [[nodiscard]] auto timeline_semaphore() const noexcept -> VkSemaphore;

    [[nodiscard]] auto frame_count() const noexcept -> uint32_t {
        return static_cast<uint32_t>(frames_.size());
    }

    [[nodiscard]] auto cell_count() const noexcept -> size_t {
        return config_.grid_size_x * config_.grid_size_y * config_.grid_size_z;
    }

    [[nodiscard]] auto frame_size_bytes() const noexcept -> size_t {
        return cell_count() * config_.cell_size_bytes;
    }

private:
    explicit CAReadbackRing(
        VkDevice device,
        VmaAllocator allocator,
        CAReadbackConfig config,
        std::vector<ReadbackFrame> frames,
        VkSemaphore timeline_semaphore
    ) noexcept;

    VkDevice device_{VK_NULL_HANDLE};
    VmaAllocator allocator_{VK_NULL_HANDLE};
    CAReadbackConfig config_;
    std::vector<ReadbackFrame> frames_;
    VkSemaphore timeline_semaphore_{VK_NULL_HANDLE};
    uint64_t current_timeline_value_{0};
    uint32_t current_frame_{0};
    uint32_t oldest_readable_frame_{0};
    void* mapped_ptr_{nullptr};
};

} // namespace projectv::simulation
```

### 2.2 Implementation

```cpp
// ProjectV.Simulation.CAReadback.cpp
module ProjectV.Simulation.CAReadback;

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

import std;
import glm;

namespace projectv::simulation {

auto CAReadbackRing::create(
    VkDevice device,
    VmaAllocator allocator,
    CAReadbackConfig const& config
) noexcept -> std::expected<CAReadbackRing, ReadbackError> {

    VkPhysicalDeviceMemoryProperties mem_props;
    vmaGetMemoryProperties(allocator, &mem_props);

    // Find host-visible, coherent memory for readback
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = config.grid_size_x * config.grid_size_y * config.grid_size_z
                * config.cell_size_bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    std::vector<ReadbackFrame> frames;
    frames.reserve(config.frame_count);

    for (uint32_t i = 0; i < config.frame_count; ++i) {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo alloc_result;

        VkResult result = vmaCreateBuffer(
            allocator,
            &buffer_info,
            &alloc_info,
            &buffer,
            &allocation,
            &alloc_result
        );

        if (result != VK_SUCCESS) {
            // Cleanup already created buffers
            for (auto& f : frames) {
                vmaDestroyBuffer(allocator, f.buffer, f.allocation);
                vkDestroyFence(device, f.fence, nullptr);
            }
            return std::unexpected(ReadbackError::BufferCreationFailed);
        }

        // Create fence for transfer completion
        VkFenceCreateInfo fence_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Initially signaled
        };

        VkFence fence;
        result = vkCreateFence(device, &fence_info, nullptr, &fence);

        if (result != VK_SUCCESS) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            for (auto& f : frames) {
                vmaDestroyBuffer(allocator, f.buffer, f.allocation);
                vkDestroyFence(device, f.fence, nullptr);
            }
            return std::unexpected(ReadbackError::FenceCreationFailed);
        }

        frames.push_back({
            .buffer = buffer,
            .allocation = allocation,
            .fence = fence,
            .timeline_value = 0,
            .is_available = true,
            .has_pending_data = false
        });
    }

    // Create timeline semaphore
    VkSemaphoreTypeCreateInfo timeline_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0
    };

    VkSemaphoreCreateInfo sem_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timeline_info
    };

    VkSemaphore timeline_semaphore;
    VkResult result = vkCreateSemaphore(device, &sem_info, nullptr, &timeline_semaphore);

    if (result != VK_SUCCESS) {
        for (auto& f : frames) {
            vmaDestroyBuffer(allocator, f.buffer, f.allocation);
            vkDestroyFence(device, f.fence, nullptr);
        }
        return std::unexpected(ReadbackError::SemaphoreCreationFailed);
    }

    return CAReadbackRing(device, allocator, config, std::move(frames), timeline_semaphore);
}

CAReadbackRing::CAReadbackRing(
    VkDevice device,
    VmaAllocator allocator,
    CAReadbackConfig config,
    std::vector<ReadbackFrame> frames,
    VkSemaphore timeline_semaphore
) noexcept
    : device_(device)
    , allocator_(allocator)
    , config_(config)
    , frames_(std::move(frames))
    , timeline_semaphore_(timeline_semaphore)
{}

CAReadbackRing::~CAReadbackRing() noexcept {
    if (mapped_ptr_) {
        vmaUnmapMemory(allocator_, frames_[oldest_readable_frame_].allocation);
        mapped_ptr_ = nullptr;
    }

    for (auto& f : frames_) {
        vmaDestroyBuffer(allocator_, f.buffer, f.allocation);
        vkDestroyFence(device_, f.fence, nullptr);
    }

    vkDestroySemaphore(device_, timeline_semaphore_, nullptr);
}

auto CAReadbackRing::get_current_buffer() const noexcept -> VkBuffer {
    return frames_[current_frame_].buffer;
}

auto CAReadbackRing::advance_frame(VkCommandBuffer cmd, VkQueue queue) noexcept -> void {
    // Submit transfer command
    VkBufferMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .buffer = frames_[current_frame_].buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);

    // Signal timeline semaphore
    current_timeline_value_++;
    frames_[current_frame_].timeline_value = current_timeline_value_;
    frames_[current_frame_].is_available = false;
    frames_[current_frame_].has_pending_data = true;

    // Signal fence on completion
    VkSemaphoreSignalInfo signal_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = timeline_semaphore_,
        .value = current_timeline_value_
    };

    // Move to next frame
    current_frame_ = (current_frame_ + 1) % frames_.size();
}

auto CAReadbackRing::has_available_data() const noexcept -> bool {
    // Check if oldest frame is signaled
    return frames_[oldest_readable_frame_].has_pending_data;
}

auto CAReadbackRing::map_oldest_frame(uint64_t timeout_ns) noexcept
    -> std::expected<std::span<std::byte const>, ReadbackError> {

    if (mapped_ptr_) {
        return std::unexpected(ReadbackError::AlreadyMapped);
    }

    ReadbackFrame& frame = frames_[oldest_readable_frame_];

    if (!frame.has_pending_data) {
        return std::unexpected(ReadbackError::NoPendingData);
    }

    // Wait for timeline semaphore
    VkSemaphoreWaitInfo wait_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &timeline_semaphore_,
        .pValues = &frame.timeline_value
    };

    VkResult result = vkWaitSemaphores(device_, &wait_info, timeout_ns);

    if (result == VK_TIMEOUT) {
        return std::unexpected(ReadbackError::Timeout);
    }
    if (result != VK_SUCCESS) {
        return std::unexpected(ReadbackError::WaitFailed);
    }

    // Map memory
    result = vmaMapMemory(allocator_, frame.allocation, &mapped_ptr_);

    if (result != VK_SUCCESS) {
        return std::unexpected(ReadbackError::MapFailed);
    }

    // Invalidate cache (for non-coherent memory)
    vmaInvalidateAllocation(allocator_, frame.allocation, 0, VK_WHOLE_SIZE);

    return std::span<std::byte const>(
        static_cast<std::byte const*>(mapped_ptr_),
        frame_size_bytes()
    );
}

auto CAReadbackRing::unmap_and_release() noexcept -> void {
    if (!mapped_ptr_) return;

    vmaUnmapMemory(allocator_, frames_[oldest_readable_frame_].allocation);
    mapped_ptr_ = nullptr;

    frames_[oldest_readable_frame_].has_pending_data = false;
    frames_[oldest_readable_frame_].is_available = true;

    oldest_readable_frame_ = (oldest_readable_frame_ + 1) % frames_.size();
}

auto CAReadbackRing::timeline_semaphore() const noexcept -> VkSemaphore {
    return timeline_semaphore_;
}

} // namespace projectv::simulation
```

---

## 3. Physics Bridge: CA → Jolt

### 3.1 Buoyancy & Viscosity Forces

```cpp
// ProjectV.Simulation.PhysicsBridge.cppm
export module ProjectV.Simulation.PhysicsBridge;

import std;
import glm;
import ProjectV.Simulation.CAReadback;
import ProjectV.Physics.Jolt;

export namespace projectv::simulation {

/// Cell material types (must match GPU enum)
export enum class CellMaterial : uint32_t {
    Air = 0,
    Water,
    Oil,
    Lava,
    Sand,
    Gravel,
    Snow,
    Smoke,
    Fire,
    Steam,
    Acid
};

/// Material properties for physics
export struct MaterialPhysicsProps {
    float density{1000.0f};         ///< kg/m³
    float viscosity{0.001f};        ///< Dynamic viscosity (Pa·s)
    float drag_coefficient{0.47f};  ///< Shape-dependent
    float buoyancy_factor{1.0f};    ///< Multiplier for buoyancy
};

/// Cell state from GPU (must match std430 layout in Slang)
export struct alignas(16) CellStateCPU {
    float density{0.0f};            ///< [0-1] filled amount
    float velocity_x{0.0f};
    float velocity_y{0.0f};
    float velocity_z{0.0f};
    uint32_t material_type{0};
    uint32_t flags{0};
    float temperature{0.0f};
    float pressure{0.0f};

    // Padding to match 32-byte GPU layout
    // Total: 32 bytes
};

static_assert(sizeof(CellStateCPU) == 32, "CellStateCPU must match GPU layout");
static_assert(alignof(CellStateCPU) == 16, "CellStateCPU alignment must match GPU");

/// Physics bridge configuration
export struct PhysicsBridgeConfig {
    float voxel_size{1.0f};                 ///< Size of one voxel in meters
    glm::ivec3 grid_origin{0, 0, 0};        ///< World position of grid[0,0,0]
    uint32_t grid_size_x{256};
    uint32_t grid_size_y{128};
    uint32_t grid_size_z{256};
    float buoyancy_update_rate{60.0f};      ///< Updates per second
    float max_buoyancy_force{1000.0f};      ///< N
    float max_drag_force{500.0f};           ///< N
};

/// Bridge between GPU CA and CPU JoltPhysics
export class PhysicsBridge {
public:
    /// Creates physics bridge.
    [[nodiscard]] static auto create(
        PhysicsBridgeConfig const& config,
        PhysicsSystem& physics
    ) noexcept -> std::expected<PhysicsBridge, BridgeError>;

    ~PhysicsBridge() noexcept = default;

    PhysicsBridge(PhysicsBridge&&) noexcept = default;
    PhysicsBridge& operator=(PhysicsBridge&&) noexcept = default;
    PhysicsBridge(const PhysicsBridge&) = delete;
    PhysicsBridge& operator=(const PhysicsBridge&) = delete;

    /// Updates physics from readback data.
    /// Called after map_oldest_frame() succeeds.
    auto update_from_readback(
        std::span<std::byte const> readback_data,
        float delta_time
    ) noexcept -> void;

    /// Applies buoyancy and drag forces to all registered bodies.
    auto apply_fluid_forces(float delta_time) noexcept -> void;

    /// Registers a body for fluid interaction.
    auto register_body(
        BodyID body_id,
        float radius,
        float density
    ) noexcept -> void;

    /// Unregisters a body.
    auto unregister_body(BodyID body_id) noexcept -> void;

    /// Gets material properties.
    [[nodiscard]] auto get_material_props(CellMaterial material) const noexcept
        -> MaterialPhysicsProps const&;

    /// Sets material properties.
    auto set_material_props(
        CellMaterial material,
        MaterialPhysicsProps const& props
    ) noexcept -> void;

private:
    explicit PhysicsBridge(
        PhysicsBridgeConfig config,
        PhysicsSystem* physics
    ) noexcept;

    /// Gets cell at world position.
    [[nodiscard]] auto get_cell_at(glm::vec3 const& world_pos) const noexcept
        -> CellStateCPU const*;

    /// Calculates buoyancy force on a body.
    [[nodiscard]] auto calculate_buoyancy(
        BodyID body_id,
        float body_volume,
        float body_density
    ) const noexcept -> glm::vec3;

    /// Calculates drag force from fluid velocity.
    [[nodiscard]] auto calculate_drag(
        BodyID body_id,
        float radius,
        glm::vec3 const& fluid_velocity
    ) const noexcept -> glm::vec3;

    /// Checks if position is inside fluid (density > threshold).
    [[nodiscard]] auto is_in_fluid(glm::vec3 const& world_pos) const noexcept -> bool;

    PhysicsBridgeConfig config_;
    PhysicsSystem* physics_{nullptr};
    std::span<std::byte const> current_readback_;

    /// Cell grid (copy from GPU)
    std::vector<CellStateCPU> cell_grid_;

    /// Registered bodies for fluid interaction
    struct RegisteredBody {
        BodyID body_id;
        float radius;
        float density;      ///< Body density (kg/m³)
        float volume;       ///< Body volume (m³)
    };
    std::vector<RegisteredBody> registered_bodies_;

    /// Material properties lookup
    std::unordered_map<CellMaterial, MaterialPhysicsProps> material_props_;
};

} // namespace projectv::simulation
```

### 3.2 Implementation

```cpp
// ProjectV.Simulation.PhysicsBridge.cpp
module ProjectV.Simulation.PhysicsBridge;

import std;
import glm;
import ProjectV.Simulation.CAReadback;
import ProjectV.Physics.Jolt;

namespace projectv::simulation {

PhysicsBridge::PhysicsBridge(
    PhysicsBridgeConfig config,
    PhysicsSystem* physics
) noexcept
    : config_(config)
    , physics_(physics)
{
    // Initialize material properties
    material_props_[CellMaterial::Air] = {
        .density = 1.225f,           // kg/m³ at sea level
        .viscosity = 0.0000181f,     // Air viscosity
        .drag_coefficient = 0.47f,
        .buoyancy_factor = 0.0f      // No buoyancy in air
    };

    material_props_[CellMaterial::Water] = {
        .density = 1000.0f,          // kg/m³
        .viscosity = 0.001f,         // Water at 20°C
        .drag_coefficient = 0.47f,
        .buoyancy_factor = 1.0f
    };

    material_props_[CellMaterial::Oil] = {
        .density = 900.0f,
        .viscosity = 0.03f,
        .drag_coefficient = 0.47f,
        .buoyancy_factor = 0.9f
    };

    material_props_[CellMaterial::Lava] = {
        .density = 3100.0f,          // Basaltic lava
        .viscosity = 100.0f,         // Very viscous
        .drag_coefficient = 0.47f,
        .buoyancy_factor = 3.1f
    };

    material_props_[CellMaterial::Sand] = {
        .density = 1600.0f,          // Dry sand
        .viscosity = 0.0f,           // Granular, not fluid
        .drag_coefficient = 0.0f,
        .buoyancy_factor = 0.0f
    };
}

auto PhysicsBridge::create(
    PhysicsBridgeConfig const& config,
    PhysicsSystem& physics
) noexcept -> std::expected<PhysicsBridge, BridgeError> {
    return PhysicsBridge(config, &physics);
}

auto PhysicsBridge::update_from_readback(
    std::span<std::byte const> readback_data,
    float delta_time
) noexcept -> void {
    current_readback_ = readback_data;

    // Copy to local grid for faster access
    size_t cell_count = config_.grid_size_x * config_.grid_size_y * config_.grid_size_z;
    cell_grid_.resize(cell_count);

    std::memcpy(
        cell_grid_.data(),
        readback_data.data(),
        std::min(readback_data.size(), cell_count * sizeof(CellStateCPU))
    );
}

auto PhysicsBridge::register_body(
    BodyID body_id,
    float radius,
    float density
) noexcept -> void {
    float volume = (4.0f / 3.0f) * glm::pi<float>() * radius * radius * radius;

    registered_bodies_.push_back({
        .body_id = body_id,
        .radius = radius,
        .density = density,
        .volume = volume
    });
}

auto PhysicsBridge::unregister_body(BodyID body_id) noexcept -> void {
    std::erase_if(registered_bodies_, [body_id](auto const& rb) {
        return rb.body_id.native() == body_id.native();
    });
}

auto PhysicsBridge::apply_fluid_forces(float delta_time) noexcept -> void {
    if (!physics_ || cell_grid_.empty()) return;

    for (auto const& rb : registered_bodies_) {
        // Get body position
        auto pos_result = physics_->get_body_position(rb.body_id);
        if (!pos_result) continue;
        glm::vec3 body_pos = *pos_result;

        // Get cell at body position
        CellStateCPU const* cell = get_cell_at(body_pos);
        if (!cell) continue;

        // Check if in fluid (density > threshold)
        if (cell->density < 0.1f) continue;

        CellMaterial material = static_cast<CellMaterial>(cell->material_type);
        MaterialPhysicsProps const& props = get_material_props(material);

        // Calculate and apply buoyancy
        glm::vec3 buoyancy = calculate_buoyancy(rb.body_id, rb.volume, rb.density);

        // Limit buoyancy force
        float buoyancy_mag = glm::length(buoyancy);
        if (buoyancy_mag > config_.max_buoyancy_force) {
            buoyancy = glm::normalize(buoyancy) * config_.max_buoyancy_force;
        }

        physics_->apply_force(rb.body_id, buoyancy);

        // Calculate fluid velocity at body position
        glm::vec3 fluid_vel(
            cell->velocity_x,
            cell->velocity_y,
            cell->velocity_z
        );

        // Calculate and apply drag
        if (glm::length(fluid_vel) > 0.01f) {
            glm::vec3 drag = calculate_drag(rb.body_id, rb.radius, fluid_vel);

            // Limit drag force
            float drag_mag = glm::length(drag);
            if (drag_mag > config_.max_drag_force) {
                drag = glm::normalize(drag) * config_.max_drag_force;
            }

            physics_->apply_force(rb.body_id, drag);
        }

        // Apply viscous damping
        if (props.viscosity > 0.0f) {
            auto vel_result = physics_->get_body_velocity(rb.body_id);
            if (vel_result) {
                glm::vec3 body_vel = *vel_result;
                glm::vec3 relative_vel = body_vel - fluid_vel;

                // Stokes' drag for viscous fluids
                glm::vec3 viscous_drag = -6.0f * glm::pi<float>() * props.viscosity
                                        * rb.radius * relative_vel;

                physics_->apply_force(rb.body_id, viscous_drag);
            }
        }
    }
}

auto PhysicsBridge::get_cell_at(glm::vec3 const& world_pos) const noexcept
    -> CellStateCPU const* {

    // Convert world position to grid coordinates
    glm::ivec3 grid_pos = glm::ivec3(
        (world_pos - glm::vec3(config_.grid_origin)) / config_.voxel_size
    );

    // Check bounds
    if (grid_pos.x < 0 || grid_pos.x >= static_cast<int>(config_.grid_size_x) ||
        grid_pos.y < 0 || grid_pos.y >= static_cast<int>(config_.grid_size_y) ||
        grid_pos.z < 0 || grid_pos.z >= static_cast<int>(config_.grid_size_z)) {
        return nullptr;
    }

    // Calculate linear index
    size_t index = grid_pos.x
                 + grid_pos.y * config_.grid_size_x
                 + grid_pos.z * config_.grid_size_x * config_.grid_size_y;

    if (index >= cell_grid_.size()) return nullptr;

    return &cell_grid_[index];
}

auto PhysicsBridge::calculate_buoyancy(
    BodyID body_id,
    float body_volume,
    float body_density
) const noexcept -> glm::vec3 {

    // Get body position to find fluid density
    auto pos_result = physics_->get_body_position(body_id);
    if (!pos_result) return glm::vec3{0.0f};

    CellStateCPU const* cell = get_cell_at(*pos_result);
    if (!cell || cell->density < 0.1f) return glm::vec3{0.0f};

    CellMaterial material = static_cast<CellMaterial>(cell->material_type);
    MaterialPhysicsProps const& props = get_material_props(material);

    // Buoyancy: F = ρ_fluid * V * g * (ρ_fluid / ρ_body - 1) for submerged body
    // Simplified: F = (ρ_fluid - ρ_body) * V * g
    // But we use density factor from CA (0-1) and buoyancy_factor

    float effective_fluid_density = props.density * cell->density;
    float density_diff = effective_fluid_density - body_density;

    // Archimedes' principle: F_buoyancy = ρ_fluid * V * g
    // For partially submerged: multiply by fill density
    float buoyancy_magnitude = effective_fluid_density * body_volume * 9.81f
                              * props.buoyancy_factor;

    // Direction: upward (positive Y in ProjectV)
    return glm::vec3{0.0f, buoyancy_magnitude, 0.0f};
}

auto PhysicsBridge::calculate_drag(
    BodyID body_id,
    float radius,
    glm::vec3 const& fluid_velocity
) const noexcept -> glm::vec3 {

    auto vel_result = physics_->get_body_velocity(body_id);
    if (!vel_result) return glm::vec3{0.0f};

    glm::vec3 body_vel = *vel_result;
    glm::vec3 relative_vel = fluid_velocity - body_vel;

    float rel_speed = glm::length(relative_vel);
    if (rel_speed < 0.001f) return glm::vec3{0.0f};

    // Drag force: F = 0.5 * ρ * v² * C_d * A
    // For sphere: A = π * r²
    float cross_section_area = glm::pi<float>() * radius * radius;

    // Get fluid density at body position
    auto pos_result = physics_->get_body_position(body_id);
    float fluid_density = 1000.0f;  // Default water
    if (pos_result) {
        CellStateCPU const* cell = get_cell_at(*pos_result);
        if (cell && cell->density > 0.1f) {
            CellMaterial material = static_cast<CellMaterial>(cell->material_type);
            fluid_density = get_material_props(material).density * cell->density;
        }
    }

    float drag_magnitude = 0.5f * fluid_density * rel_speed * rel_speed
                          * 0.47f * cross_section_area;  // 0.47 = sphere Cd

    // Drag opposes relative motion
    return glm::normalize(relative_vel) * drag_magnitude;
}

auto PhysicsBridge::get_material_props(CellMaterial material) const noexcept
    -> MaterialPhysicsProps const& {

    static MaterialPhysicsProps default_props{};

    auto it = material_props_.find(material);
    if (it != material_props_.end()) {
        return it->second;
    }
    return default_props;
}

auto PhysicsBridge::set_material_props(
    CellMaterial material,
    MaterialPhysicsProps const& props
) noexcept -> void {
    material_props_[material] = props;
}

auto PhysicsBridge::is_in_fluid(glm::vec3 const& world_pos) const noexcept -> bool {
    CellStateCPU const* cell = get_cell_at(world_pos);
    return cell && cell->density > 0.1f;
}

} // namespace projectv::simulation
```

---

## 4. Main Loop Integration

### 4.1 Synchronization Sequence

```cpp
// Main simulation loop
auto update_simulation(
    float delta_time,
    VulkanContext& vk,
    CellularAutomataGrid& ca_grid,
    CAReadbackRing& readback_ring,
    PhysicsBridge& physics_bridge,
    PhysicsSystem& physics,
    VkCommandBuffer compute_cmd,
    VkCommandBuffer graphics_cmd
) -> void {

    // 1. Submit CA compute shader
    {
        TracyZoneScopedN("CA Compute");

        // Bind CA pipeline and dispatch
        vkCmdBindPipeline(compute_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ca_pipeline);
        vkCmdBindDescriptorSets(compute_cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                               ca_pipeline_layout, 0, 1, &ca_descriptor_set, 0, nullptr);

        // Push constants
        struct CAPushConstants {
            float delta_time;
            uint32_t grid_size_x;
            uint32_t grid_size_y;
            uint32_t grid_size_z;
        } constants{
            .delta_time = delta_time,
            .grid_size_x = ca_grid.size().x,
            .grid_size_y = ca_grid.size().y,
            .grid_size_z = ca_grid.size().z
        };

        vkCmdPushConstants(compute_cmd, ca_pipeline_layout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(CAPushConstants), &constants);

        // Dispatch compute
        glm::uvec3 dispatch_size = calculate_dispatch_size(ca_grid.size());
        vkCmdDispatch(compute_cmd, dispatch_size.x, dispatch_size.y, dispatch_size.z);

        // Barrier for readback
        VkMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT
        };

        VkDependencyInfo dep{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        };

        vkCmdPipelineBarrier2(compute_cmd, &dep);

        // Copy to readback buffer
        VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = readback_ring.frame_size_bytes()
        };

        vkCmdCopyBuffer(compute_cmd,
                       ca_grid.current_buffer(),
                       readback_ring.get_current_buffer(),
                       1, &copy_region);

        // Advance readback ring
        readback_ring.advance_frame(compute_cmd, vk.compute_queue);
    }

    // 2. Step physics (in parallel with GPU)
    {
        TracyZoneScopedN("Physics Step");
        physics.step(delta_time);
    }

    // 3. Process available readback data
    {
        TracyZoneScopedN("Readback Processing");

        if (readback_ring.has_available_data()) {
            auto data_result = readback_ring.map_oldest_frame(1'000'000);  // 1ms timeout

            if (data_result) {
                // Update physics bridge with new CA data
                physics_bridge.update_from_readback(*data_result, delta_time);

                // Apply fluid forces to bodies
                physics_bridge.apply_fluid_forces(delta_time);

                // Release frame for reuse
                readback_ring.unmap_and_release();

                TracyPlot("CA Readback Latency", delta_time);
            }
        }
    }

    // 4. Swap CA buffers
    ca_grid.swap_buffers();
}
```

### 4.2 Timeline Semaphore Synchronization

```cpp
// Synchronization with timeline semaphores
struct SyncContext {
    VkSemaphore compute_finished;   // Timeline: compute shader completion
    VkSemaphore readback_finished;  // Timeline: readback transfer completion
    VkSemaphore graphics_finished;  // Timeline: graphics frame completion
    uint64_t frame_number{0};
};

auto submit_frame(
    VkDevice device,
    VkQueue compute_queue,
    VkQueue graphics_queue,
    SyncContext& sync,
    VkCommandBuffer compute_cmd,
    VkCommandBuffer graphics_cmd
) -> void {

    sync.frame_number++;

    // Compute submission
    VkSemaphoreSubmitInfo compute_wait{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sync.graphics_finished,
        .value = sync.frame_number - 1,
        .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    };

    VkSemaphoreSubmitInfo compute_signal{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sync.compute_finished,
        .value = sync.frame_number,
        .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    };

    VkCommandBufferSubmitInfo compute_cmd_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = compute_cmd
    };

    VkSubmitInfo2 compute_submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &compute_wait,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &compute_cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &compute_signal
    };

    vkQueueSubmit2(compute_queue, 1, &compute_submit, VK_NULL_HANDLE);

    // Graphics submission (waits for compute)
    VkSemaphoreSubmitInfo graphics_wait{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sync.compute_finished,
        .value = sync.frame_number,
        .stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT
    };

    VkSemaphoreSubmitInfo graphics_signal{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sync.graphics_finished,
        .value = sync.frame_number,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
    };

    VkCommandBufferSubmitInfo graphics_cmd_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = graphics_cmd
    };

    VkSubmitInfo2 graphics_submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &graphics_wait,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &graphics_cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &graphics_signal
    };

    vkQueueSubmit2(graphics_queue, 1, &graphics_submit, VK_NULL_HANDLE);
}
```

---

## 5. Memory Layout Validation

### 5.1 Static Assertions (C++26)

```cpp
// ProjectV.Simulation.CATypes.cppm

export module ProjectV.Simulation.CATypes;

import std;
import glm;

export namespace projectv::simulation {

/// GPU-aligned cell state for std430 layout
/// MUST match Slang shader layout exactly
export struct alignas(16) CellStateGPU {
    float density{0.0f};        ///< Offset: 0
    float velocity_x{0.0f};     ///< Offset: 4
    float velocity_y{0.0f};     ///< Offset: 8
    float velocity_z{0.0f};     ///< Offset: 12
    uint32_t material_type{0};  ///< Offset: 16
    uint32_t flags{0};          ///< Offset: 20
    float temperature{0.0f};    ///< Offset: 24
    float pressure{0.0f};       ///< Offset: 28
};
// Total size: 32 bytes (2 * vec4)

// Static assertions for layout validation
static_assert(sizeof(CellStateGPU) == 32,
    "CellStateGPU must be 32 bytes to match std430 layout");

static_assert(alignof(CellStateGPU) == 16,
    "CellStateGPU must be 16-byte aligned for GPU buffers");

static_assert(offsetof(CellStateGPU, density) == 0,
    "density offset mismatch");

static_assert(offsetof(CellStateGPU, velocity_x) == 4,
    "velocity_x offset mismatch");

static_assert(offsetof(CellStateGPU, velocity_y) == 8,
    "velocity_y offset mismatch");

static_assert(offsetof(CellStateGPU, velocity_z) == 12,
    "velocity_z offset mismatch");

static_assert(offsetof(CellStateGPU, material_type) == 16,
    "material_type offset mismatch");

static_assert(offsetof(CellStateGPU, flags) == 20,
    "flags offset mismatch");

static_assert(offsetof(CellStateGPU, temperature) == 24,
    "temperature offset mismatch");

static_assert(offsetof(CellStateGPU, pressure) == 28,
    "pressure offset mismatch");

/// Validation function for runtime checks
export auto validate_cell_state_layout() noexcept -> bool {
    bool valid = true;

    if (sizeof(CellStateGPU) != 32) {
        std::println(stderr, "ERROR: CellStateGPU size = {}, expected 32",
                    sizeof(CellStateGPU));
        valid = false;
    }

    if (alignof(CellStateGPU) != 16) {
        std::println(stderr, "ERROR: CellStateGPU alignment = {}, expected 16",
                    alignof(CellStateGPU));
        valid = false;
    }

    return valid;
}

} // namespace projectv::simulation
```

### 5.2 Slang Shader Layout

```slang
// VoxelCA.slang
module VoxelCA;

// Cell state structure - MUST match C++ CellStateGPU
struct CellState {
    float density;        // Offset: 0
    float velocity_x;     // Offset: 4
    float velocity_y;     // Offset: 8
    float velocity_z;     // Offset: 12
    uint material_type;   // Offset: 16
    uint flags;           // Offset: 20
    float temperature;    // Offset: 24
    float pressure;       // Offset: 28
};
// Total: 32 bytes (std430 compatible)

// Static assertion in Slang (compile-time validation)
static_assert(sizeof(CellState) == 32);

// Verify layout matches std430
// std430 rules:
// - alignof(float) = 4, alignof(uint) = 4
// - alignof(struct) = max(alignof(members), 16) = 16
// - sizeof(struct) must be multiple of 16 for arrays
// Our layout: 32 bytes = 2 * 16, satisfies std430

// Double buffers
[[vk::binding(0, 0)]]
StructuredBuffer<CellState> currentCells;

[[vk::binding(1, 0)]]
RWStructuredBuffer<CellState> nextCells;

// Parameters (UBO, std140)
struct alignas(16) CASimulationParams {
    uint3 grid_size;
    float delta_time;
    float gravity;
    float viscosity;
    float friction;
    float diffusion;
    uint step_count;
    uint material_count;
    float temperature_decay;
    float burn_rate;
    int3 boundary_min;
    int3 boundary_max;
};

[[vk::binding(2, 0)]]
uniform CASimulationParams params;
```

---

## 6. Performance Considerations

### 6.1 Memory Bandwidth

| Operation        | Bandwidth     | Typical Size    | Time @ 50 GB/s |
|------------------|---------------|-----------------|----------------|
| CA Grid Readback | 32 bytes/cell | 8M cells (256³) | ~5.1 ms        |
| Physics Update   | 64 bytes/body | 10K bodies      | ~1.3 µs        |
| Total Frame      | —             | —               | ~5.2 ms        |

### 6.2 Latency Hiding

1. **Triple Buffering**: 3 frames in-flight hides readback latency
2. **Timeline Semaphores**: Fine-grained GPU-CPU synchronization
3. **Async Compute**: CA compute runs parallel to graphics
4. **Frame Overlap**: Readback from frame N-1 while computing frame N

### 6.3 Optimization Guidelines

```cpp
// DO: Use ring buffers to avoid stalls
CAReadbackRing ring = CAReadbackRing::create(device, allocator, {
    .frame_count = 3,  // Triple buffering
    // ...
});

// DO: Check for available data before blocking
if (ring.has_available_data()) {
    auto data = ring.map_oldest_frame(non_blocking_timeout);
    // Process without blocking
}

// DON'T: Block on every frame
// auto data = ring.map_oldest_frame(UINT64_MAX);  // BAD: stalls pipeline

// DO: Use timeline semaphores for precise sync
VkSemaphoreSubmitInfo signal{
    .semaphore = timeline_sem,
    .value = frame_number,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
};
```

---

## Статус

| Компонент                | Статус         | Приоритет |
|--------------------------|----------------|-----------|
| CAReadbackRing           | Специфицирован | P0        |
| PhysicsBridge            | Специфицирован | P0        |
| Buoyancy Calculation     | Специфицирован | P0        |
| Drag Calculation         | Специфицирован | P1        |
| Timeline Synchronization | Специфицирован | P0        |
| Memory Layout Validation | Специфицирован | P0        |
| Main Loop Integration    | Специфицирован | P1        |

---

## Ссылки

- [GPU Cellular Automata Specification](../04_physics_ca/03_gpu_cellular_automata.md)
- [Jolt-Vulkan Bridge](../04_physics_ca/01_jolt_vulkan_bridge.md)
- [ADR-0004: Build & Modules](../adr/0004-build-and-modules-spec.md)
