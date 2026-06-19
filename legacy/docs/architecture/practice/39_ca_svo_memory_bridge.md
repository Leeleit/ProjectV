# Спецификация SVO ↔ CA Memory Bridge [🔴 Уровень 3]

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ описывает архитектурный мост между **Sparse Voxel Octree (SVO)** и **Cellular Automata (CA)**. SVO —
разреженная древовидная структура, идеальная для рендеринга, но CA требует плотной 3D-сетки для эффективного
итерирования по соседям. Спецификация решает этот конфликт через GPU-резидентные структуры и минимизацию PCIe traffic.

---

## 1. Проблема и решение

### 1.1 Архитектурный конфликт

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SVO vs CA Architecture Conflict                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SVO (Sparse Voxel Octree)           CA (Cellular Automata)             │
│  ┌─────────────────────────┐        ┌─────────────────────────┐         │
│  │ Tree structure          │        │ Dense 3D Grid           │         │
│  │                         │        │                         │         │
│  │     ○                   │        │  ┌─┬─┬─┬─┐             │         │
│  │    /|\                  │        │  ├─┼─┼─┼─┤             │         │
│  │   ○ ○ ○                 │        │  ├─┼─┼─┼─┤             │         │
│  │  /|\                    │        │  └─┴─┴─┴─┘             │         │
│  │ ○ ○ ○                   │        │                         │         │
│  │                         │        │  neighbor access:       │         │
│  │ Access: O(log n)        │        │  O(1) contiguous        │         │
│  │ Memory: Sparse          │        │  Memory: Dense          │         │
│  └─────────────────────────┘        └─────────────────────────┘         │
│                                                                          │
│  Rendering: ✅ Optimal              CA Simulation: ✅ Optimal            │
│  CA Simulation: ❌ Slow             Rendering: ❌ Memory heavy           │
│  (random access, cache misses)      (many empty cells)                  │
│                                                                          │
│  SOLUTION: Hybrid approach with on-demand unpacking on GPU              │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Решение: GPU-резидентный Bridge

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SVO ↔ CA Memory Bridge                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                         GPU Memory                                       │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                                                                   │  │
│  │   SVO Buffer              Dense Grid Pool              Output    │  │
│  │   (Resident)              (Allocated per region)       (Physics) │  │
│  │                                                                   │  │
│  │   ┌─────────┐            ┌─────────────────┐          ┌───────┐ │  │
│  │   │ Nodes   │──Unpack──▶│ 64³ Grid SSBO   │──CA─────▶│AABBs  │ │  │
│  │   │ Voxels  │   Compute  │ (Active Region) │  Steps   │Surface│ │  │
│  │   └─────────┘   Shader   └─────────────────┘          └───────┘ │  │
│  │        │                      │                           │     │  │
│  │        │                      │ Pack                      │     │  │
│  │        │◄─────────────────────┘ Shader                    │     │  │
│  │        │                                                  │     │  │
│  │        │                                    Async Read    │     │  │
│  │        │                                    ───────────────┘     │  │
│  │        │                                         to CPU         │  │
│  │        │                                                        │  │
│  └────────┴────────────────────────────────────────────────────────┘  │
│           │                                                            │
│           │ GPU-resident only                                          │
│           │ No per-frame CPU-GPU transfer                             │
│           ▼                                                            │
│   CPU Memory (minimal)                                                 │
│   ┌───────────────────────────────────────────────────────────────┐   │
│   │ Active CA Regions: [RegionHeader; 64 bytes per region]        │   │
│   │ Fluid Surface: Dynamic Heightfield (2D, not 3D)               │   │
│   │ Physics AABBs: Max 64 active fluid clusters                   │   │
│   └───────────────────────────────────────────────────────────────┘   │
│                                                                        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Структуры данных GPU

### 2.1 SVO GPU Layout

```cpp
// ProjectV.Voxel.SVOGPU.cppm
export module ProjectV.Voxel.SVOGPU;

import std;
import glm;
import vulkan;

export namespace projectv::voxel::gpu {

/// SVO Node для GPU (48 bytes, std430 aligned).
///
/// ## Layout
/// - children[8]: индексы дочерних узлов (0 = null)
/// - parent: индекс родителя
/// - voxel_index: индекс в voxel buffer (только для leaf)
/// - flags: битовые флаги
export struct alignas(16) SVONodeGPU {
    uint32_t children[8];     // 32 bytes
    uint32_t parent;          // 4 bytes
    uint32_t voxel_index;     // 4 bytes
    uint32_t flags;           // 4 bytes
    uint32_t padding;         // 4 bytes
};

static_assert(sizeof(SVONodeGPU) == 48);

/// Voxel Data для GPU (4 bytes).
export struct alignas(4) VoxelDataGPU {
    uint16_t material_id;     // Material ID
    uint8_t flags;            // Flags (solid, fluid, etc.)
    uint8_t light;            // Light level
};

static_assert(sizeof(VoxelDataGPU) == 4);

/// CA Cell State для GPU (32 bytes).
///
/// ## Layout
/// Плотный формат для SIMD-friendly CA симуляции.
export struct alignas(16) CACellGPU {
    float density;            // 4 bytes - fluid density [0, 1]
    float velocity_x;         // 4 bytes
    float velocity_y;         // 4 bytes
    float velocity_z;         // 4 bytes
    uint32_t material_type;   // 4 bytes - 0=air, 1=water, 2=lava, etc.
    uint32_t flags;           // 4 bytes - active, boundary, etc.
    float temperature;        // 4 bytes - for thermal CA
    float pressure;           // 4 bytes - for pressure calculation
};

static_assert(sizeof(CACellGPU) == 32);

/// Активный регион CA (описывает dense grid).
export struct CARegionHeader {
    uint32_t node_index;      // SVO node index (region root)
    uint32_t grid_offset;     // Offset in DenseGridPool
    uint16_t size_x;          // Grid dimensions (typically 32 or 64)
    uint16_t size_y;
    uint16_t size_z;
    uint16_t lod_level;       // LOD for this region
    uint32_t flags;           // Active, dirty, etc.
    uint32_t padding;
};

static_assert(sizeof(CARegionHeader) == 24);

/// Dense Grid Pool Header.
export struct DenseGridPoolHeader {
    uint32_t total_cells;     // Total cells in pool
    uint32_t active_regions;  // Number of active regions
    uint32_t max_regions;     // Maximum regions
    uint32_t padding;
};

/// Результат CA для CPU (минимальный набор данных).
export struct alignas(16) CAPhysicsResult {
    glm::vec3 aabb_min;       // Bounding box of fluid
    float _pad0;
    glm::vec3 aabb_max;
    float _pad1;
    float total_mass;         // Total fluid mass
    uint32_t cell_count;      // Number of active cells
    uint32_t surface_cells;   // Cells on surface
    uint32_t region_id;       // Region identifier
};

static_assert(sizeof(CAPhysicsResult) == 48);

/// Dynamic Heightfield для воды (2D representation).
export struct alignas(4) FluidHeightfieldCell {
    float height;             // Water surface height
    float velocity_y;         // Vertical velocity
    uint16_t material;        // Material type
    uint16_t flags;           // Flags
};

static_assert(sizeof(FluidHeightfieldCell) == 12);

} // namespace projectv::voxel::gpu
```

### 2.2 Vulkan Buffer Layout

```cpp
// ProjectV.Voxel.SVOBuffers.cppm
export module ProjectV.Voxel.SVOBuffers;

import std;
import vulkan;
import ProjectV.Voxel.SVOGPU;

export namespace projectv::voxel {

/// GPU буферы для SVO и CA.
export struct SVOGPUBuffers {
    /// SVO Node buffer (SSBO).
    /// Size: max_nodes * sizeof(SVONodeGPU)
    VkBuffer svo_node_buffer{VK_NULL_HANDLE};
    VmaAllocation svo_node_allocation{VK_NULL_HANDLE};
    uint64_t svo_node_address{0};  // BDA for shader access

    /// Voxel data buffer (SSBO).
    /// Size: max_voxels * sizeof(VoxelDataGPU)
    VkBuffer voxel_buffer{VK_NULL_HANDLE};
    VmaAllocation voxel_allocation{VK_NULL_HANDLE};
    uint64_t voxel_buffer_address{0};

    /// Dense Grid Pool (SSBO).
    /// Size: max_grid_cells * sizeof(CACellGPU)
    /// Typically 64³ = 262,144 cells = 8 MB
    VkBuffer dense_grid_pool{VK_NULL_HANDLE};
    VmaAllocation dense_grid_allocation{VK_NULL_HANDLE};
    uint64_t dense_grid_address{0};

    /// Region Headers (SSBO).
    /// Active CA regions
    VkBuffer region_headers{VK_NULL_HANDLE};
    VmaAllocation region_headers_allocation{VK_NULL_HANDLE};
    uint64_t region_headers_address{0};

    /// CA Physics Results (SSBO, read back to CPU).
    VkBuffer physics_results{VK_NULL_HANDLE};
    VmaAllocation physics_results_allocation{VK_NULL_HANDLE};
    uint64_t physics_results_address{0};

    /// Fluid Heightfield (SSBO, read back to CPU).
    /// Size: heightfield_resolution² * sizeof(FluidHeightfieldCell)
    VkBuffer fluid_heightfield{VK_NULL_HANDLE};
    VmaAllocation fluid_heightfield_allocation{VK_NULL_HANDLE};

    /// Staging buffer for async readback.
    VkBuffer staging_buffer{VK_NULL_HANDLE};
    VmaAllocation staging_allocation{VK_NULL_HANDLE};
    void* staging_mapped{nullptr};

    /// Command pool for compute.
    VkCommandPool compute_pool{VK_NULL_HANDLE};

    /// Fence for async readback.
    VkFence readback_fence{VK_NULL_HANDLE};
};

/// Конфигурация буферов.
export struct SVOBufferConfig {
    uint32_t max_svo_nodes{1 << 20};      // 1M nodes = 48 MB
    uint32_t max_voxels{1 << 24};         // 16M voxels = 64 MB
    uint32_t max_grid_cells{64 * 64 * 64 * 8}; // 8 regions of 64³ = 8 MB
    uint32_t max_regions{64};
    uint32_t heightfield_resolution{256}; // 256x256 heightfield
};

/// Создаёт GPU буферы.
export auto create_svo_buffers(
    VkDevice device,
    VmaAllocator allocator,
    SVOBufferConfig const& config
) noexcept -> std::expected<SVOGPUBuffers, VkResult>;

/// Уничтожает GPU буферы.
export auto destroy_svo_buffers(
    VkDevice device,
    VmaAllocator allocator,
    SVOGPUBuffers& buffers
) noexcept -> void;

} // namespace projectv::voxel
```

---

## 3. Slang Compute Shaders

### 3.1 Unpack SVO to Dense Grid

```slang
// shaders/ca_unpack_svo_to_grid.cs.slang

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References (BDA)
// ============================================================================

layout(buffer_reference, scalar) buffer SVONodeBuffer {
    SVONodeGPU nodes[];
};

layout(buffer_reference, scalar) buffer VoxelDataBuffer {
    VoxelDataGPU voxels[];
};

layout(buffer_reference, scalar) buffer DenseGridBuffer {
    CACellGPU cells[];
};

layout(buffer_reference, scalar) buffer RegionHeaderBuffer {
    CARegionHeader regions[];
};

// ============================================================================
// Structures
// ============================================================================

struct SVONodeGPU {
    uint children[8];
    uint parent;
    uint voxel_index;
    uint flags;
    uint padding;
};

struct VoxelDataGPU {
    uint material_id_flags_light;  // packed: material:16 | flags:8 | light:8
};

struct CACellGPU {
    float density;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint material_type;
    uint flags;
    float temperature;
    float pressure;
};

struct CARegionHeader {
    uint node_index;
    uint grid_offset;
    uint size_x_size_y;  // packed: size_x:16 | size_y:16
    uint size_z_lod;     // packed: size_z:16 | lod:16
    uint flags;
    uint padding;
};

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    SVONodeBuffer svo_nodes;
    VoxelDataBuffer voxels;
    DenseGridBuffer dense_grid;
    RegionHeaderBuffer regions;
    uint region_id;
    uint max_depth;
    uint _pad[2];
} pc;

// ============================================================================
// Helper Functions
// ============================================================================

// Unpack voxel data
uint get_material_id(uint packed) {
    return packed & 0xFFFF;
}

uint get_flags(uint packed) {
    return (packed >> 16) & 0xFF;
}

// Unpack region header
uint get_size_x(CARegionHeader h) {
    return h.size_x_size_y & 0xFFFF;
}

uint get_size_y(CARegionHeader h) {
    return (h.size_x_size_y >> 16) & 0xFFFF;
}

uint get_size_z(CARegionHeader h) {
    return h.size_z_lod & 0xFFFF;
}

// ============================================================================
// Unpack Shader
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

void main() {
    uint region_id = pc.region_id;
    CARegionHeader region = pc.regions.regions[region_id];

    uint size_x = get_size_x(region);
    uint size_y = get_size_y(region);
    uint size_z = get_size_z(region);

    uvec3 local_id = gl_LocalInvocationID;
    uvec3 global_id = gl_GlobalInvocationID;

    // Bounds check
    if (global_id.x >= size_x || global_id.y >= size_y || global_id.z >= size_z) {
        return;
    }

    uint svo_node_index = region.node_index;

    // Traverse SVO to find voxel at this position
    // Morton code for efficient traversal
    uint target_depth = pc.max_depth;
    uint current_depth = 0;
    uint current_node = svo_node_index;

    while (current_depth < target_depth && current_node != 0) {
        SVONodeGPU node = pc.svo_nodes.nodes[current_node];

        // Determine child index from position
        uint child_x = (global_id.x >> (target_depth - current_depth - 1)) & 1;
        uint child_y = (global_id.y >> (target_depth - current_depth - 1)) & 1;
        uint child_z = (global_id.z >> (target_depth - current_depth - 1)) & 1;
        uint child_index = child_x | (child_y << 1) | (child_z << 2);

        current_node = node.children[child_index];
        current_depth++;
    }

    // Calculate output index
    uint cell_index = region.grid_offset +
                      global_id.x +
                      global_id.y * size_x +
                      global_id.z * size_x * size_y;

    CACellGPU cell;
    cell.density = 0.0;
    cell.velocity_x = 0.0;
    cell.velocity_y = 0.0;
    cell.velocity_z = 0.0;
    cell.material_type = 0;  // Air by default
    cell.flags = 0;
    cell.temperature = 0.0;
    cell.pressure = 0.0;

    if (current_node != 0) {
        SVONodeGPU leaf = pc.svo_nodes.nodes[current_node];

        if (leaf.voxel_index != 0) {
            uint voxel_data = pc.voxels.voxels[leaf.voxel_index].material_id_flags_light;
            uint material_id = get_material_id(voxel_data);
            uint flags = get_flags(voxel_data);

            // Determine if this is a fluid material
            bool is_fluid = (flags & 0x02) != 0;  // Flag bit 1 = fluid

            if (is_fluid) {
                cell.density = 1.0;
                cell.material_type = material_id;
                cell.flags = 1;  // Active
            } else if (material_id != 0) {
                // Solid material
                cell.material_type = material_id | 0x80000000;  // Mark as solid
            }
        }
    }

    // Write to dense grid
    pc.dense_grid.cells[cell_index] = cell;
}
```

### 3.2 CA Step Shader

```slang
// shaders/ca_step.cs.slang

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References
// ============================================================================

layout(buffer_reference, scalar) buffer DenseGridBuffer {
    CACellGPU cells[];
};

layout(buffer_reference, scalar) buffer RegionHeaderBuffer {
    CARegionHeader regions[];
};

// ============================================================================
// Structures
// ============================================================================

struct CACellGPU {
    float density;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint material_type;
    uint flags;
    float temperature;
    float pressure;
};

struct CARegionHeader {
    uint node_index;
    uint grid_offset;
    uint size_x_size_y;
    uint size_z_lod;
    uint flags;
    uint padding;
};

// ============================================================================
// Constants
// ============================================================================

const float GRAVITY = 9.81;
const float VISCOSITY = 0.1;
const float DT = 0.016;  // 16ms timestep

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    DenseGridBuffer current_grid;
    DenseGridBuffer next_grid;
    RegionHeaderBuffer regions;
    uint region_id;
    uint step_count;
    float delta_time;
    float gravity;
} pc;

// ============================================================================
// Helper Functions
// ============================================================================

bool is_solid(uint material_type) {
    return (material_type & 0x80000000) != 0;
}

bool is_fluid(uint material_type) {
    return (material_type != 0) && !is_solid(material_type);
}

bool is_active(uint flags) {
    return (flags & 1) != 0;
}

// Get neighbor cell with boundary checking
CACellGPU get_neighbor(DenseGridBuffer grid, uint base_idx, int dx, int dy, int dz,
                        uint size_x, uint size_y, uint size_z,
                        int x, int y, int z) {
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;

    // Boundary: treat as solid
    if (nx < 0 || nx >= int(size_x) ||
        ny < 0 || ny >= int(size_y) ||
        nz < 0 || nz >= int(size_z)) {
        CACellGPU boundary;
        boundary.density = 0.0;
        boundary.material_type = 0x80000000;  // Solid
        boundary.flags = 0;
        return boundary;
    }

    uint idx = base_idx + nx + ny * size_x + nz * size_x * size_y;
    return grid.cells[idx];
}

// ============================================================================
// CA Step Shader
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

shared CACellGPU local_cells[10][10][10];  // 8x8x8 + 1 border on each side

void main() {
    CARegionHeader region = pc.regions.regions[pc.region_id];

    uint size_x = region.size_x_size_y & 0xFFFF;
    uint size_y = (region.size_x_size_y >> 16) & 0xFFFF;
    uint size_z = region.size_z_lod & 0xFFFF;
    uint base_offset = region.grid_offset;

    ivec3 local_id = ivec3(gl_LocalInvocationID);
    ivec3 global_id = ivec3(gl_GlobalInvocationID);

    // Load into shared memory with halo
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                ivec3 load_pos = local_id + ivec3(dx * 8, dy * 8, dz * 8);
                ivec3 global_load = global_id + ivec3(dx * 8, dy * 8, dz * 8);

                if (load_pos.x < 10 && load_pos.y < 10 && load_pos.z < 10) {
                    uint idx = base_offset +
                               clamp(global_load.x, 0, int(size_x)-1) +
                               clamp(global_load.y, 0, int(size_y)-1) * size_x +
                               clamp(global_load.z, 0, int(size_z)-1) * size_x * size_y;

                    local_cells[load_pos.x][load_pos.y][load_pos.z] =
                        pc.current_grid.cells[idx];
                }
            }
        }
    }

    barrier();

    // Bounds check
    if (global_id.x >= int(size_x) || global_id.y >= int(size_y) || global_id.z >= int(size_z)) {
        return;
    }

    // Get current cell from shared memory
    ivec3 s = local_id + ivec3(1, 1, 1);  // Shift for halo
    CACellGPU current = local_cells[s.x][s.y][s.z];

    // Skip solid and empty cells
    if (is_solid(current.material_type) || !is_fluid(current.material_type)) {
        uint out_idx = base_offset + global_id.x + global_id.y * size_x + global_id.z * size_x * size_y;
        pc.next_grid.cells[out_idx] = current;
        return;
    }

    // Fluid dynamics (simplified Navier-Stokes)
    float density_sum = current.density;
    float velocity_x_sum = current.velocity_x * current.density;
    float velocity_y_sum = current.velocity_y * current.density;
    float velocity_z_sum = current.velocity_z * current.density;
    int fluid_neighbors = 1;

    // 6-neighbor stencil
    CACellGPU neighbors[6];
    neighbors[0] = local_cells[s.x-1][s.y][s.z];  // -X
    neighbors[1] = local_cells[s.x+1][s.y][s.z];  // +X
    neighbors[2] = local_cells[s.x][s.y-1][s.z];  // -Y
    neighbors[3] = local_cells[s.x][s.y+1][s.z];  // +Y
    neighbors[4] = local_cells[s.x][s.y][s.z-1];  // -Z
    neighbors[5] = local_cells[s.x][s.y][s.z+1];  // +Z

    // Process neighbors
    float pressure_diff_x = 0.0;
    float pressure_diff_y = 0.0;
    float pressure_diff_z = 0.0;

    for (int i = 0; i < 6; ++i) {
        CACellGPU n = neighbors[i];

        if (is_solid(n.material_type)) {
            // Boundary condition: reflect velocity
            if (i == 0 || i == 1) pressure_diff_x = 0.0;
            if (i == 2 || i == 3) pressure_diff_y = 0.0;
            if (i == 4 || i == 5) pressure_diff_z = 0.0;
            continue;
        }

        if (is_fluid(n.material_type)) {
            density_sum += n.density;
            velocity_x_sum += n.velocity_x * n.density;
            velocity_y_sum += n.velocity_y * n.density;
            velocity_z_sum += n.velocity_z * n.density;
            fluid_neighbors++;
        }
    }

    // Update velocity (gravity + pressure gradient)
    float new_velocity_x = current.velocity_x;
    float new_velocity_y = current.velocity_y - pc.gravity * pc.delta_time;
    float new_velocity_z = current.velocity_z;

    // Viscosity (velocity diffusion)
    if (fluid_neighbors > 1) {
        float avg_vx = velocity_x_sum / density_sum;
        float avg_vy = velocity_y_sum / density_sum;
        float avg_vz = velocity_z_sum / density_sum;

        new_velocity_x = mix(new_velocity_x, avg_vx, VISCOSITY);
        new_velocity_y = mix(new_velocity_y, avg_vy, VISCOSITY);
        new_velocity_z = mix(new_velocity_z, avg_vz, VISCOSITY);
    }

    // Flow to lower neighbors (gravity-driven)
    float outflow = 0.0;

    // Check cell below
    if (global_id.y > 0 && !is_solid(neighbors[2].material_type) &&
        neighbors[2].density < current.density) {
        float space = 1.0 - neighbors[2].density;
        float flow = min(current.density * 0.5, space) * pc.delta_time * 10.0;
        outflow += flow;
    }

    // Horizontal spreading
    for (int i = 0; i < 6; ++i) {
        if (i == 2 || i == 3) continue;  // Skip vertical
        CACellGPU n = neighbors[i];
        if (!is_solid(n.material_type) && n.density < current.density - 0.1) {
            float space = 1.0 - n.density;
            float flow = min((current.density - n.density) * 0.25, space) * pc.delta_time * 2.0;
            outflow += flow * 0.5;
        }
    }

    // Update density
    float new_density = max(0.0, current.density - outflow);

    // Write result
    CACellGPU next;
    next.density = new_density;
    next.velocity_x = new_velocity_x;
    next.velocity_y = new_velocity_y;
    next.velocity_z = new_velocity_z;
    next.material_type = new_density > 0.01 ? current.material_type : 0;
    next.flags = new_density > 0.01 ? 1 : 0;
    next.temperature = current.temperature;
    next.pressure = current.density;  // Simplified pressure

    uint out_idx = base_offset + global_id.x + global_id.y * size_x + global_id.z * size_x * size_y;
    pc.next_grid.cells[out_idx] = next;
}
```

### 3.3 Pack Dense Grid to SVO

```slang
// shaders/ca_pack_grid_to_svo.cs.slang

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References
// ============================================================================

layout(buffer_reference, scalar) buffer SVONodeBuffer {
    SVONodeGPU nodes[];
};

layout(buffer_reference, scalar) buffer VoxelDataBuffer {
    VoxelDataGPU voxels[];
};

layout(buffer_reference, scalar) buffer DenseGridBuffer {
    CACellGPU cells[];
};

layout(buffer_reference, scalar) buffer RegionHeaderBuffer {
    CARegionHeader regions[];
};

layout(buffer_reference, scalar) buffer AtomicCounters {
    uint node_counter;
    uint voxel_counter;
    uint padding[2];
};

// ============================================================================
// Structures
// ============================================================================

struct SVONodeGPU {
    uint children[8];
    uint parent;
    uint voxel_index;
    uint flags;
    uint padding;
};

struct VoxelDataGPU {
    uint material_id_flags_light;
};

struct CACellGPU {
    float density;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint material_type;
    uint flags;
    float temperature;
    float pressure;
};

struct CARegionHeader {
    uint node_index;
    uint grid_offset;
    uint size_x_size_y;
    uint size_z_lod;
    uint flags;
    uint padding;
};

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    SVONodeBuffer svo_nodes;
    VoxelDataBuffer voxels;
    DenseGridBuffer dense_grid;
    RegionHeaderBuffer regions;
    AtomicCounters counters;
    uint region_id;
    uint max_depth;
    uint _pad[2];
} pc;

// ============================================================================
// Pack Shader
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

void main() {
    CARegionHeader region = pc.regions.regions[pc.region_id];

    uint size_x = region.size_x_size_y & 0xFFFF;
    uint size_y = (region.size_x_size_y >> 16) & 0xFFFF;
    uint size_z = region.size_z_lod & 0xFFFF;
    uint base_offset = region.grid_offset;

    uvec3 global_id = gl_GlobalInvocationID;

    // Bounds check
    if (global_id.x >= size_x || global_id.y >= size_y || global_id.z >= size_z) {
        return;
    }

    uint cell_idx = base_offset + global_id.x + global_id.y * size_x + global_id.z * size_x * size_y;
    CACellGPU cell = pc.dense_grid.cells[cell_idx];

    // Skip empty cells
    if (cell.density < 0.01 && !is_solid(cell.material_type)) {
        return;
    }

    // Navigate to leaf node in SVO
    uint target_depth = pc.max_depth;
    uint current_depth = 0;
    uint current_node = region.node_index;

    // Traverse/create path to leaf
    while (current_depth < target_depth) {
        SVONodeGPU node = pc.svo_nodes.nodes[current_node];

        uint child_x = (global_id.x >> (target_depth - current_depth - 1)) & 1;
        uint child_y = (global_id.y >> (target_depth - current_depth - 1)) & 1;
        uint child_z = (global_id.z >> (target_depth - current_depth - 1)) & 1;
        uint child_index = child_x | (child_y << 1) | (child_z << 2);

        uint child_node = node.children[child_index];

        if (child_node == 0) {
            // Allocate new node
            uint new_node_idx = atomicAdd(pc.counters.node_counter, 1);
            pc.svo_nodes.nodes[new_node_idx] = SVONodeGPU(
                {0, 0, 0, 0, 0, 0, 0, 0},  // children
                current_node,               // parent
                0,                          // voxel_index
                0,                          // flags
                0                           // padding
            );

            // Link to parent (atomic exchange to handle race)
            atomicExchange(pc.svo_nodes.nodes[current_node].children[child_index], new_node_idx);
            child_node = new_node_idx;
        }

        current_node = child_node;
        current_depth++;
    }

    // Allocate and write voxel data
    uint voxel_idx = atomicAdd(pc.counters.voxel_counter, 1);

    uint material_id = cell.material_type & 0x7FFF;
    uint flags = is_solid(cell.material_type) ? 0x01 : 0x02;  // Solid or fluid flag
    uint packed = material_id | (flags << 16) | (uint(cell.density * 255.0) << 24);

    pc.voxels.voxels[voxel_idx].material_id_flags_light = packed;

    // Update leaf node
    pc.svo_nodes.nodes[current_node].voxel_index = voxel_idx;
    pc.svo_nodes.nodes[current_node].flags = 1;  // Mark as leaf
}

bool is_solid(uint material_type) {
    return (material_type & 0x80000000) != 0;
}
```

### 3.4 Generate Physics Results

```slang
// shaders/ca_generate_physics_results.cs.slang

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References
// ============================================================================

layout(buffer_reference, scalar) buffer DenseGridBuffer {
    CACellGPU cells[];
};

layout(buffer_reference, scalar) buffer RegionHeaderBuffer {
    CARegionHeader regions[];
};

layout(buffer_reference, scalar) buffer PhysicsResultsBuffer {
    CAPhysicsResult results[];
};

layout(buffer_reference, scalar) buffer FluidHeightfieldBuffer {
    FluidHeightfieldCell heightfield[];
};

// ============================================================================
// Structures
// ============================================================================

struct CACellGPU {
    float density;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint material_type;
    uint flags;
    float temperature;
    float pressure;
};

struct CARegionHeader {
    uint node_index;
    uint grid_offset;
    uint size_x_size_y;
    uint size_z_lod;
    uint flags;
    uint padding;
};

struct CAPhysicsResult {
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    float _pad1;
    float total_mass;
    uint cell_count;
    uint surface_cells;
    uint region_id;
};

struct FluidHeightfieldCell {
    float height;
    float velocity_y;
    uint material;
    uint flags;
};

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    DenseGridBuffer dense_grid;
    RegionHeaderBuffer regions;
    PhysicsResultsBuffer results;
    FluidHeightfieldBuffer heightfield;
    uint region_id;
    uint heightfield_resolution;
    float voxel_size;
    float _pad;
} pc;

// ============================================================================
// Reduction Shader
// ============================================================================

// Shared memory for reduction
shared vec3 shared_aabb_min[256];
shared vec3 shared_aabb_max[256];
shared float shared_mass[256];
shared uint shared_count[256];
shared uint shared_surface[256];

layout(local_size_x = 256) in;

void main() {
    uint region_id = pc.region_id;
    CARegionHeader region = pc.regions.regions[region_id];

    uint size_x = region.size_x_size_y & 0xFFFF;
    uint size_y = (region.size_x_size_y >> 16) & 0xFFFF;
    uint size_z = region.size_z_lod & 0xFFFF;
    uint base_offset = region.grid_offset;
    uint total_cells = size_x * size_y * size_z;

    // Initialize shared memory
    uint lid = gl_LocalInvocationID.x;
    shared_aabb_min[lid] = vec3(1e30);
    shared_aabb_max[lid] = vec3(-1e30);
    shared_mass[lid] = 0.0;
    shared_count[lid] = 0;
    shared_surface[lid] = 0;

    barrier();

    // Process cells
    for (uint i = gl_GlobalInvocationID.x; i < total_cells; i += gl_WorkGroupSize.x) {
        uint z = i / (size_x * size_y);
        uint rem = i % (size_x * size_y);
        uint y = rem / size_x;
        uint x = rem % size_x;

        CACellGPU cell = pc.dense_grid.cells[base_offset + i];

        if (cell.density > 0.01) {
            // Update AABB
            vec3 pos = vec3(float(x), float(y), float(z)) * pc.voxel_size;

            atomicMin(shared_aabb_min[lid].x, pos.x);
            atomicMin(shared_aabb_min[lid].y, pos.y);
            atomicMin(shared_aabb_min[lid].z, pos.z);
            atomicMax(shared_aabb_max[lid].x, pos.x + pc.voxel_size);
            atomicMax(shared_aabb_max[lid].y, pos.y + pc.voxel_size);
            atomicMax(shared_aabb_max[lid].z, pos.z + pc.voxel_size);

            // Accumulate mass
            atomicAdd(shared_mass[lid], cell.density);
            atomicAdd(shared_count[lid], 1);

            // Check if surface cell (has air neighbor)
            bool is_surface = false;
            if (x == 0 || x == size_x - 1 ||
                y == 0 || y == size_y - 1 ||
                z == 0 || z == size_z - 1) {
                is_surface = true;
            }
            if (is_surface) {
                atomicAdd(shared_surface[lid], 1);
            }

            // Update heightfield (top surface only)
            if (y == size_y - 1 ||
                pc.dense_grid.cells[base_offset + x + (y+1) * size_x + z * size_x * size_y].density < 0.01) {
                uint hf_idx = x + z * pc.heightfield_resolution;
                if (hf_idx < pc.heightfield_resolution * pc.heightfield_resolution) {
                    // Take maximum height
                    atomicMax(pc.heightfield.heightfield[hf_idx].height, float(y) * pc.voxel_size);
                    pc.heightfield.heightfield[hf_idx].velocity_y = cell.velocity_y;
                    pc.heightfield.heightfield[hf_idx].material = cell.material_type;
                }
            }
        }
    }

    barrier();

    // Reduction
    if (lid == 0) {
        vec3 final_min = vec3(1e30);
        vec3 final_max = vec3(-1e30);
        float final_mass = 0.0;
        uint final_count = 0;
        uint final_surface = 0;

        for (uint i = 0; i < gl_WorkGroupSize.x; ++i) {
            final_min = min(final_min, shared_aabb_min[i]);
            final_max = max(final_max, shared_aabb_max[i]);
            final_mass += shared_mass[i];
            final_count += shared_count[i];
            final_surface += shared_surface[i];
        }

        // Write result
        CAPhysicsResult result;
        result.aabb_min = final_min;
        result.aabb_max = final_max;
        result.total_mass = final_mass * 1000.0;  // Assuming water density
        result.cell_count = final_count;
        result.surface_cells = final_surface;
        result.region_id = region_id;

        pc.results.results[region_id] = result;
    }
}
```

---

## 4. PCIe Bandwidth Protection

### 4.1 Стратегия минимизации transfer

```cpp
// ProjectV.Simulation.CATransfer.cppm
export module ProjectV.Simulation.CATransfer;

import std;
import glm;
import vulkan;
import ProjectV.Voxel.SVOGPU;

export namespace projectv::simulation {

/// Стратегия transfer для CA результатов.
///
/// ## Problem
/// Full 64³ grid = 262,144 cells × 32 bytes = 8 MB per region
/// At 60 FPS with 4 regions = 1.9 GB/s PCIe bandwidth (unacceptable)
///
/// ## Solution
/// Transfer only minimal data for CPU physics:
/// - AABB: 48 bytes per region (fluid bounds)
/// - Heightfield: 256² × 12 bytes = 768 KB (2D, not 3D)
/// - Total: < 1 MB per frame
export class CATransferManager {
public:
    /// Конфигурация transfer.
    struct Config {
        uint32_t max_regions{64};
        uint32_t heightfield_resolution{256};
        bool async_transfer{true};
        double max_transfer_time_ms{0.5};  // Max time for transfer
    };

    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        Config const& config
    ) noexcept -> std::expected<CATransferManager, VkResult>;

    /// Запускает асинхронный readback.
    auto start_async_readback(
        VkCommandBuffer cmd,
        voxel::gpu::SVOGPUBuffers const& buffers
    ) noexcept -> void;

    /// Ждёт завершения readback.
    auto wait_for_readback(uint64_t timeout_ns = 1000000) noexcept -> bool;

    /// Получает результаты (только после wait).
    [[nodiscard]] auto get_physics_results() const noexcept
        -> std::span<voxel::gpu::CAPhysicsResult const>;

    /// Получает heightfield.
    [[nodiscard]] auto get_heightfield() const noexcept
        -> std::span<voxel::gpu::FluidHeightfieldCell const>;

    /// Возвращает размер transfer в байтах.
    [[nodiscard]] auto transfer_size() const noexcept -> size_t;

private:
    CATransferManager() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Данные для Jolt Physics (минимальные).
export struct CAJoltPhysicsData {
    /// Active fluid AABBs (up to 64).
    std::array<voxel::gpu::CAPhysicsResult, 64> fluid_regions;
    uint32_t active_region_count{0};

    /// Water surface heightfield (2D).
    std::vector<voxel::gpu::FluidHeightfieldCell> heightfield;
    uint32_t heightfield_resolution{256};

    /// Total fluid mass (for buoyancy calculations).
    float total_fluid_mass{0.0f};
};

} // namespace projectv::simulation
```

### 4.2 Реализация Transfer

```cpp
// ProjectV.Simulation.CATransfer.cpp
module ProjectV.Simulation.CATransfer;

import std;
import vulkan;
import ProjectV.Simulation.CATransfer;
import ProjectV.Voxel.SVOGPU;

namespace projectv::simulation {

struct CATransferManager::Impl {
    // Staging buffer for readback
    VkBuffer staging_buffer{VK_NULL_HANDLE};
    VmaAllocation staging_allocation{VK_NULL_HANDLE};
    void* mapped_data{nullptr};
    size_t staging_size{0};

    // Fence for async operations
    VkFence transfer_fence{VK_NULL_HANDLE};

    // Results (CPU-visible)
    std::vector<voxel::gpu::CAPhysicsResult> physics_results;
    std::vector<voxel::gpu::FluidHeightfieldCell> heightfield;

    Config config;
};

auto CATransferManager::create(
    VkDevice device,
    VmaAllocator allocator,
    Config const& config
) noexcept -> std::expected<CATransferManager, VkResult> {

    CATransferManager result;
    result.impl_ = std::make_unique<Impl>();
    result.impl_->config = config;

    // Calculate staging size
    // Physics results: max_regions * sizeof(CAPhysicsResult)
    // Heightfield: resolution² * sizeof(FluidHeightfieldCell)
    size_t physics_size = config.max_regions * sizeof(voxel::gpu::CAPhysicsResult);
    size_t heightfield_size = config.heightfield_resolution *
                              config.heightfield_resolution *
                              sizeof(voxel::gpu::FluidHeightfieldCell);

    result.impl_->staging_size = physics_size + heightfield_size;
    result.impl_->physics_results.resize(config.max_regions);
    result.impl_->heightfield.resize(
        config.heightfield_resolution * config.heightfield_resolution
    );

    // Create staging buffer (host-visible, coherent)
    VkBufferCreateInfo staging_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = result.impl_->staging_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
    };

    VkResult vk_result = vmaCreateBuffer(
        allocator,
        &staging_info,
        &alloc_info,
        &result.impl_->staging_buffer,
        &result.impl_->staging_allocation,
        nullptr
    );

    if (vk_result != VK_SUCCESS) {
        return std::unexpected(vk_result);
    }

    // Map persistently
    vmaMapMemory(allocator, result.impl_->staging_allocation,
                 &result.impl_->mapped_data);

    // Create fence
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Initially signaled
    };

    vkCreateFence(device, &fence_info, nullptr, &result.impl_->transfer_fence);

    return result;
}

auto CATransferManager::start_async_readback(
    VkCommandBuffer cmd,
    voxel::gpu::SVOGPUBuffers const& buffers
) noexcept -> void {

    // Reset fence
    vkResetFences(vk_device, 1, &impl_->transfer_fence);

    // Barrier: ensure CA results are written
    VkBufferMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .buffer = buffers.physics_results,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    // Copy physics results to staging
    size_t physics_size = impl_->config.max_regions * sizeof(voxel::gpu::CAPhysicsResult);
    VkBufferCopy physics_copy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = physics_size
    };
    vkCmdCopyBuffer(cmd, buffers.physics_results, impl_->staging_buffer, 1, &physics_copy);

    // Copy heightfield to staging
    size_t heightfield_size = impl_->heightfield.size() * sizeof(voxel::gpu::FluidHeightfieldCell);
    VkBufferCopy heightfield_copy{
        .srcOffset = 0,
        .dstOffset = physics_size,
        .size = heightfield_size
    };
    vkCmdCopyBuffer(cmd, buffers.fluid_heightfield, impl_->staging_buffer, 1, &heightfield_copy);

    // End command buffer and submit with fence
    // (caller handles this)
}

auto CATransferManager::wait_for_readback(uint64_t timeout_ns) noexcept -> bool {
    VkResult result = vkWaitForFences(
        vk_device, 1, &impl_->transfer_fence, VK_TRUE, timeout_ns
    );

    if (result != VK_SUCCESS) {
        return false;
    }

    // Copy from staging to CPU structures
    size_t physics_size = impl_->physics_results.size() * sizeof(voxel::gpu::CAPhysicsResult);
    memcpy(impl_->physics_results.data(), impl_->mapped_data, physics_size);

    size_t heightfield_size = impl_->heightfield.size() * sizeof(voxel::gpu::FluidHeightfieldCell);
    memcpy(impl_->heightfield.data(),
           static_cast<char*>(impl_->mapped_data) + physics_size,
           heightfield_size);

    return true;
}

auto CATransferManager::get_physics_results() const noexcept
    -> std::span<voxel::gpu::CAPhysicsResult const> {
    return impl_->physics_results;
}

auto CATransferManager::get_heightfield() const noexcept
    -> std::span<voxel::gpu::FluidHeightfieldCell const> {
    return impl_->heightfield;
}

auto CATransferManager::transfer_size() const noexcept -> size_t {
    return impl_->staging_size;
}

} // namespace projectv::simulation
```

---

## 5. C++26 Pipeline Orchestration

### 5.1 CA Pipeline

```cpp
// ProjectV.Simulation.CAPipeline.cppm
export module ProjectV.Simulation.CAPipeline;

import std;
import glm;
import vulkan;
import ProjectV.Voxel.SVOGPU;
import ProjectV.Simulation.CATransfer;

export namespace projectv::simulation {

/// CA Pipeline: управляет полным циклом SVO ↔ CA.
///
/// ## Pipeline Steps (per frame)
/// 1. Detect active regions (areas with fluid)
/// 2. Unpack SVO to dense grids
/// 3. Run CA steps (N iterations)
/// 4. Pack results back to SVO
/// 5. Generate physics data
/// 6. Async readback to CPU
export class CAPipeline {
public:
    /// Конфигурация pipeline.
    struct Config {
        uint32_t ca_steps_per_frame{4};    // CA iterations per frame
        uint32_t max_active_regions{8};    // Maximum concurrent CA regions
        float region_activation_threshold{0.1f};  // Min fluid to activate
        bool enable_async_transfer{true};
    };

    /// Создаёт pipeline.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        voxel::SVOBufferConfig const& buffer_config,
        Config const& config = {}
    ) noexcept -> std::expected<CAPipeline, VkResult>;

    /// Выполняет один кадр pipeline.
    auto execute_frame(
        VkCommandBuffer cmd,
        voxel::SVOGPUBuffers const& svo_buffers,
        float delta_time
    ) noexcept -> void;

    /// Получает результаты для Jolt Physics.
    [[nodiscard]] auto get_jolt_data() const noexcept -> CAJoltPhysicsData;

    /// Обновляет активные регионы.
    auto update_active_regions(
        std::span<glm::ivec3 const> dirty_chunks
    ) noexcept -> void;

private:
    CAPipeline() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Implementation
// ============================================================================

struct CAPipeline::Impl {
    VkDevice device{VK_NULL_HANDLE};
    VmaAllocator allocator{VK_NULL_HANDLE};

    // Pipeline resources
    VkPipeline unpack_pipeline{VK_NULL_HANDLE};
    VkPipeline ca_step_pipeline{VK_NULL_HANDLE};
    VkPipeline pack_pipeline{VK_NULL_HANDLE};
    VkPipeline results_pipeline{VK_NULL_HANDLE};

    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptor_layout{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};

    // Transfer manager
    std::unique_ptr<CATransferManager> transfer_manager;

    // Active regions
    std::vector<uint32_t> active_regions;

    // Config
    Config config;
};

auto CAPipeline::execute_frame(
    VkCommandBuffer cmd,
    voxel::SVOGPUBuffers const& svo_buffers,
    float delta_time
) noexcept -> void {

    // Barrier: ensure SVO is written
    VkMemoryBarrier2 svo_barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };

    // Step 1: Unpack active regions
    for (uint32_t region_id : impl_->active_regions) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->unpack_pipeline);

        struct UnpackPushConstants {
            uint64_t svo_nodes;
            uint64_t voxels;
            uint64_t dense_grid;
            uint64_t regions;
            uint32_t region_id;
            uint32_t max_depth;
            uint32_t pad[2];
        } push{
            .svo_nodes = svo_buffers.svo_node_address,
            .voxels = svo_buffers.voxel_buffer_address,
            .dense_grid = svo_buffers.dense_grid_address,
            .regions = svo_buffers.region_headers_address,
            .region_id = region_id,
            .max_depth = 8  // TODO: from region header
        };

        vkCmdPushConstants(cmd, impl_->pipeline_layout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

        // Dispatch: 64³ / 8³ = 512 workgroups
        vkCmdDispatch(cmd, 8, 8, 8);
    }

    // Barrier between unpack and CA
    VkMemoryBarrier2 ca_barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };

    // Step 2: CA steps
    for (uint32_t step = 0; step < impl_->config.ca_steps_per_frame; ++step) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->ca_step_pipeline);

        // Ping-pong between current and next grid
        // TODO: Implement double buffering

        for (uint32_t region_id : impl_->active_regions) {
            // Dispatch CA step
            vkCmdDispatch(cmd, 8, 8, 8);
        }

        // Barrier between steps
        // ...
    }

    // Step 3: Pack back to SVO
    for (uint32_t region_id : impl_->active_regions) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->pack_pipeline);
        vkCmdDispatch(cmd, 8, 8, 8);
    }

    // Step 4: Generate physics results
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->results_pipeline);
    vkCmdDispatch(cmd, 1, 1, 1);  // Single workgroup for reduction

    // Step 5: Start async readback
    impl_->transfer_manager->start_async_readback(cmd, svo_buffers);
}

} // namespace projectv::simulation
```

---

## 6. Bandwidth Summary

| Transfer Type           | Data Size                | Frequency  | Bandwidth        |
|-------------------------|--------------------------|------------|------------------|
| Physics Results (AABBs) | 64 × 48 bytes = 3 KB     | Per frame  | 180 KB/s @ 60fps |
| Heightfield (2D)        | 256² × 12 bytes = 768 KB | Per frame  | 46 MB/s @ 60fps  |
| **Total to CPU**        | **~771 KB**              | Per frame  | **~46 MB/s**     |
|                         |                          |            |                  |
| Full 64³ Grid (avoided) | 8 MB                     | Per region | 1.9 GB/s @ 60fps |

**Result: 40x bandwidth reduction** through smart data extraction.

---

## Статус

| Компонент              | Статус         | Приоритет |
|------------------------|----------------|-----------|
| Unpack Shader          | Специфицирован | P0        |
| CA Step Shader         | Специфицирован | P0        |
| Pack Shader            | Специфицирован | P0        |
| Physics Results Shader | Специфицирован | P1        |
| Transfer Manager       | Специфицирован | P0        |
| Pipeline Orchestration | Специфицирован | P1        |

---

## Ссылки

- [00_engine-structure.md](./00_engine-structure.md)
- [30_ca_physics_bridge.md](./30_ca_physics_bridge.md)
- [38_dynamic_voxel_entities_spec.md](./38_dynamic_voxel_entities_spec.md)
- [Vulkan BDA](https://docs.vulkan.org/guide/latest/descriptor_buffer.html)
