# Slang Integration Specification

**Статус:** Спецификация
**Версия Slang:** 2024.x
**Дата:** 2026-02-22

---

## Обзор

Документ описывает инкапсуляцию Slang в архитектуре ProjectV. Slang используется для:

- Компиляции шейдеров в SPIR-V для Vulkan
- Поддержки compute/mesh/amplification shaders
- Типобезопасного взаимодействия CPU ↔ GPU через интерфейсы

---

## 1. Module Interface

```cpp
// ProjectV.Render.Slang.cppm
module;

// Global Module Fragment — Slang C API
#include <slang.h>
#include <slang-com-ptr.h>

export module ProjectV.Render.Slang;

import std;
import ProjectV.Render.Device;

export namespace projectv::render::slang {

// ============================================================================
// Slang Session (Global Compilation Context)
// ============================================================================

/// Slang session configuration
struct SlangConfig {
    std::vector<std::filesystem::path> search_paths;  // Include directories
    std::vector<std::string> defines;                  // Preprocessor macros
    SlangOptimizationLevel optimization{SLANG_OPTIMIZATION_LEVEL_DEFAULT};
    bool debug_info{false};
    SlangTargetType target{SLANG_SPIRV};
    SlangSourceLanguage source_language{SLANG_SOURCE_LANGUAGE_SLANG};
};

/// RAII wrapper for Slang session (ISlangSharedLibraryLoader + IGlobalSession)
class SlangSession {
public:
    /// Creates Slang compilation session.
    [[nodiscard]] static auto create(SlangConfig const& config = {}) noexcept
        -> std::expected<SlangSession, SlangError>;

    ~SlangSession() noexcept;

    // Move-only
    SlangSession(SlangSession&&) noexcept;
    SlangSession& operator=(SlangSession&&) noexcept;
    SlangSession(const SlangSession&) = delete;
    SlangSession& operator=(const SlangSession&) = delete;

    /// Compiles Slang source to SPIR-V.
    [[nodiscard]] auto compile(
        std::string_view source,
        std::string_view entry_point,
        SlangStage stage
    ) const noexcept -> std::expected<SPIRVModule, SlangError>;

    /// Compiles Slang file to SPIR-V.
    [[nodiscard]] auto compile_file(
        std::filesystem::path const& path,
        std::string_view entry_point,
        SlangStage stage
    ) const noexcept -> std::expected<SPIRVModule, SlangError>;

    /// Loads and compiles a module with multiple entry points.
    [[nodiscard]] auto compile_module(
        std::filesystem::path const& path,
        std::span<std::pair<std::string_view, SlangStage> const> entry_points
    ) const noexcept -> std::expected<SPIRVModuleGroup, SlangError>;

    /// Gets native session interface.
    [[nodiscard]] auto native() const noexcept -> SlangSession* { return session_; }

private:
    explicit SlangSession(SlangSession* session, SlangConfig config) noexcept;
    SlangSession* session_{nullptr};
    SlangConfig config_;
};

// ============================================================================
// SPIR-V Module
// ============================================================================

/// Compiled SPIR-V module
struct SPIRVModule {
    std::vector<uint32_t> spirv;
    std::string entry_point;
    SlangStage stage;
    size_t hash{0};  // For pipeline cache key

    [[nodiscard]] auto data() const noexcept -> std::span<const uint32_t> {
        return spirv;
    }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return spirv.size() * sizeof(uint32_t);
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return spirv.empty();
    }
};

/// Group of compiled entry points from single source
struct SPIRVModuleGroup {
    std::filesystem::path source_path;
    std::vector<SPIRVModule> modules;

    /// Find module by entry point name
    [[nodiscard]] auto find(std::string_view entry_point) const noexcept
        -> SPIRVModule const*;
};

// ============================================================================
// Shader Specialization
// ============================================================================

/// Shader specialization constant
struct SpecializationConstant {
    std::string name;
    union Value {
        int32_t i32;
        uint32_t u32;
        float f32;
        double f64;

        Value() : u32(0) {}
    } value;

    enum class Type : uint8_t { Int32, UInt32, Float32, Float64 } type;

    static auto int32(std::string_view name, int32_t value) noexcept -> SpecializationConstant;
    static auto uint32(std::string_view name, uint32_t value) noexcept -> SpecializationConstant;
    static auto float32(std::string_view name, float value) noexcept -> SpecializationConstant;
};

/// Specialization info for pipeline creation
struct SpecializationInfo {
    std::vector<SpecializationConstant> constants;

    /// Converts to VkSpecializationInfo
    [[nodiscard]] auto to_vk_info() const noexcept
        -> std::pair<VkSpecializationInfo, std::vector<uint8_t>>;
};

// ============================================================================
// Pipeline Creation Helpers
// ============================================================================

/// Creates VkShaderModule from SPIR-V
[[nodiscard]] auto create_shader_module(
    RenderDevice const& device,
    SPIRVModule const& module
) noexcept -> std::expected<VkShaderModule, ShaderError>;

/// Creates VkPipelineShaderStageCreateInfo
[[nodiscard]] auto create_shader_stage(
    VkShaderModule module,
    VkShaderStageFlagBits stage,
    std::string_view entry_point = "main",
    SpecializationInfo const* specialization = nullptr
) noexcept -> VkPipelineShaderStageCreateInfo;

// ============================================================================
// Slang Shader Types for SVO
// ============================================================================

/// Common Slang structures shared between CPU and GPU
///
/// Slang code (SVOStructures.slang):
/// ```slang
/// struct SVONode {
///     uint child_ptr;
///     uint voxel_ptr;
///     uint child_mask;
///     uint reserved;
/// };
///
/// struct VoxelData {
///     uint16_t material_id;
///     uint8_t density;
///     uint8_t flags;
/// };
///
/// struct VoxelMaterial {
///     float4 base_color;
///     float4 emissive;
///     float4 pbr_params;  // roughness, metallic, transmission, flags
/// };
/// ```

// ============================================================================
// Error Handling
// ============================================================================

export enum class SlangError : uint8_t {
    SessionCreationFailed,
    CompilationFailed,
    InvalidSource,
    EntryPointNotFound,
    IncludeNotFound,
    SPIRVGenerationFailed,
    FileNotFound
};

export enum class ShaderError : uint8_t {
    ModuleCreationFailed,
    InvalidSPIRV,
    DeviceNotValid
};

/// Gets detailed error message from last Slang operation
[[nodiscard]] auto get_last_error_message() noexcept -> std::string_view;

} // namespace projectv::render::slang
```

---

## 2. Slang Shaders Architecture

### 2.1 Module Structure

```
shaders/
├── core/
│   ├── SVOStructures.slang       # Shared data structures
│   ├── Math.slang                # GLSL-style math utilities
│   └── Common.slang              # Common utilities
│
├── svo/
│   ├── SVORayMarch.slang         # Ray marching through SVO
│   ├── SVOMeshGen.slang          # Mesh shader generation
│   └── SVODebug.slang            # Debug visualization
│
├── voxel/
│   ├── VoxelCA.slang             # Cellular automata simulation
│   ├── VoxelLight.slang          # Light propagation
│   └── VoxelAO.slang             # Ambient occlusion
│
└── post/
    ├── ToneMapping.slang         # HDR → SDR
    ├── Bloom.slang               # Bloom effect
    └── FXAA.slang                # Anti-aliasing
```

### 2.2 Core Structures (Slang)

```slang
// core/SVOStructures.slang
module SVOStructures;

// GPU-совместимые структуры (std430 layout)
struct SVONode {
    uint child_ptr;
    uint voxel_ptr;
    uint child_mask;
    uint reserved;
};

struct VoxelData {
    uint material_id;  // Packed: uint16 + uint8 + uint8
    // Unpack: (material_id >> 16) & 0xFFFF = material
    //         (material_id >> 8) & 0xFF = density
    //         material_id & 0xFF = flags
};

struct VoxelMaterial {
    float4 base_color;
    float4 emissive;
    float4 pbr_params;  // x=roughness, y=metallic, z=transmission, w=flags
};

// Push constants for ray marching
struct RayMarchParams {
    float4x4 inv_view_proj;
    float3 camera_pos;
    float max_distance;
    uint root_node;
    uint max_depth;
    float voxel_size;
    uint2 pad;
};

// Utility functions
uint pack_voxel_data(uint material_id, uint density, uint flags) {
    return (material_id << 16) | (density << 8) | flags;
}

uint unpack_material_id(uint packed) { return (packed >> 16) & 0xFFFF; }
uint unpack_density(uint packed) { return (packed >> 8) & 0xFF; }
uint unpack_flags(uint packed) { return packed & 0xFF; }
```

### 2.3 SVO Ray Marching (Slang)

```slang
// svo/SVORayMarch.slang
module SVORayMarch;

import SVOStructures;

// Bindings
[[vk::binding(0, 0)]]
StructuredBuffer<SVONode> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> voxelData;  // Packed VoxelData

[[vk::binding(2, 0)]]
StructuredBuffer<VoxelMaterial> materials;

[[vk::binding(3, 0)]]
RWTexture2D<float4> outputImage;

[[vk::push_constant]]
RayMarchParams params;

// Stack-based iterative SVO traversal
struct TraversalStack {
    uint node_index;
    float t_min;
    float t_max;
    uint depth;
};

// AABB intersection
bool intersectAABB(
    float3 origin, float3 dir,
    float3 box_min, float3 box_max,
    out float t_min, out float t_max
) {
    float3 t0 = (box_min - origin) / dir;
    float3 t1 = (box_max - origin) / dir;

    float3 t_near = min(t0, t1);
    float3 t_far = max(t0, t1);

    t_min = max(max(t_near.x, t_near.y), t_near.z);
    t_max = min(min(t_far.x, t_far.y), t_far.z);

    return t_max >= max(t_min, 0.0);
}

// Child AABB computation
void getChildAABB(
    float3 parent_min,
    float3 parent_max,
    uint octant,
    out float3 child_min,
    out float3 child_max
) {
    float3 mid = (parent_min + parent_max) * 0.5;

    child_min = float3(
        (octant & 1) ? mid.x : parent_min.x,
        (octant & 2) ? mid.y : parent_min.y,
        (octant & 4) ? mid.z : parent_min.z
    );

    child_max = float3(
        (octant & 1) ? parent_max.x : mid.x,
        (octant & 2) ? parent_max.y : mid.y,
        (octant & 4) ? parent_max.z : mid.z
    );
}

// Main ray marching kernel
[numthreads(8, 8, 1)]
void csMain(uint3 tid: SV_DispatchThreadID) {
    uint2 resolution;
    outputImage.GetDimensions(resolution.x, resolution.y);

    if (any(tid.xy >= resolution)) return;

    // Generate ray
    float2 uv = (float2(tid.xy) + 0.5) / float2(resolution);
    float2 ndc = uv * 2.0 - 1.0;

    float4 world_pos = mul(params.inv_view_proj, float4(ndc, 1.0, 1.0));
    world_pos /= world_pos.w;

    float3 ro = params.camera_pos;
    float3 rd = normalize(world_pos.xyz - ro);

    // Initialize stack
    const uint MAX_STACK_SIZE = 24;
    TraversalStack stack[MAX_STACK_SIZE];
    int stack_ptr = 0;

    // World bounds
    float3 world_min = float3(-params.voxel_size * 512.0);
    float3 world_max = float3(params.voxel_size * 512.0);

    float t_min, t_max;
    if (!intersectAABB(ro, rd, world_min, world_max, t_min, t_max)) {
        outputImage[tid.xy] = float4(0.1, 0.2, 0.3, 1.0);  // Sky color
        return;
    }

    stack[0].node_index = params.root_node;
    stack[0].t_min = t_min;
    stack[0].t_max = t_max;
    stack[0].depth = 0;

    float3 hit_color = float3(0.1, 0.2, 0.3);
    float3 hit_normal = float3(0, 1, 0);
    bool hit = false;

    // SVO traversal
    while (stack_ptr >= 0 && !hit) {
        TraversalStack current = stack[stack_ptr];
        stack_ptr--;

        SVONode node = svoNodes[current.node_index];

        // Leaf node — check voxel
        if (node.child_mask == 0 && node.voxel_ptr != 0) {
            uint packed = voxelData[node.voxel_ptr];
            uint material_id = unpack_material_id(packed);

            if (material_id != 0) {
                VoxelMaterial mat = materials[material_id];
                hit_color = mat.base_color.rgb;
                hit = true;
            }
            continue;
        }

        // Internal node — push children
        if (node.child_mask != 0) {
            uint child_base = node.child_ptr;

            // Current node bounds
            float3 node_size = (world_max - world_min) / float3(1 << current.depth);
            float3 node_min = world_min;  // Should be computed from traversal
            float3 node_max = node_min + node_size;

            for (uint i = 0; i < 8; ++i) {
                if ((node.child_mask & (1 << i)) == 0) continue;

                float3 child_min, child_max;
                getChildAABB(node_min, node_max, i, child_min, child_max);

                float child_t_min, child_t_max;
                if (intersectAABB(ro, rd, child_min, child_max, child_t_min, child_t_max)) {
                    stack_ptr++;
                    stack[stack_ptr].node_index = child_base + i;
                    stack[stack_ptr].t_min = child_t_min;
                    stack[stack_ptr].t_max = child_t_max;
                    stack[stack_ptr].depth = current.depth + 1;
                }
            }
        }
    }

    // Simple shading
    if (hit) {
        float3 L = normalize(float3(1, 2, 1));
        float diffuse = max(dot(hit_normal, L), 0.0);
        float ambient = 0.1;
        hit_color = hit_color * (ambient + diffuse);
    }

    outputImage[tid.xy] = float4(hit_color, 1.0);
}
```

---

## 3. C++ Integration

```cpp
// Shader compilation and usage pattern
import std;
import ProjectV.Render.Slang;
import ProjectV.Render.Pipeline;

auto create_ray_march_pipeline(
    RenderDevice const& device,
    std::filesystem::path const& shader_dir
) -> std::expected<VkPipeline, SlangError> {

    // 1. Create Slang session
    auto session = slang::SlangSession::create({
        .search_paths = {shader_dir / "core", shader_dir / "svo"},
        .optimization = SLANG_OPTIMIZATION_LEVEL_DEFAULT,
        .target = SLANG_SPIRV
    });
    if (!session) {
        return std::unexpected(session.error());
    }

    // 2. Compile ray march shader
    auto spirv = session->compile_file(
        shader_dir / "svo/SVORayMarch.slang",
        "csMain",
        SLANG_STAGE_COMPUTE
    );
    if (!spirv) {
        return std::unexpected(spirv.error());
    }

    // 3. Create shader module
    auto shader_module = slang::create_shader_module(device, *spirv);
    if (!shader_module) {
        return std::unexpected(SlangError::SPIRVGenerationFailed);
    }

    // 4. Create compute pipeline
    auto pipeline = ComputePipelineBuilder{}
        .set_shader(&*shader_module, "csMain")
        .set_layout(/* pipeline_layout */)
        .build(device);

    return pipeline;
}
```

---

## 4. Hot Reload Support

```cpp
// Hot reload system for development
export module ProjectV.Render.ShaderHotReload;

import std;
import ProjectV.Render.Slang;

export namespace projectv::render {

/// Watches shader files for changes and triggers recompilation
class ShaderHotReloader {
public:
    /// Creates hot reloader watching specified directories.
    [[nodiscard]] static auto create(
        std::span<std::filesystem::path const> watch_dirs,
        slang::SlangSession const& session
    ) noexcept -> std::expected<ShaderHotReloader, HotReloadError>;

    ~ShaderHotReloader() noexcept;

    /// Checks for file changes. Returns true if any shaders were recompiled.
    [[nodiscard]] auto check_for_changes() noexcept -> bool;

    /// Registers callback for when a shader is recompiled.
    auto on_recompile(std::string_view shader_name,
                      std::move_only_function<void(SPIRVModule const&)> callback) noexcept -> void;

    /// Gets current module for shader.
    [[nodiscard]] auto get_module(std::string_view name) const noexcept
        -> SPIRVModule const*;

private:
    struct WatchedFile {
        std::filesystem::path path;
        std::filesystem::file_time_type last_modified;
        std::string entry_point;
        SlangStage stage;
    };

    std::unordered_map<std::string, WatchedFile> watched_;
    std::unordered_map<std::string, SPIRVModule> compiled_;
    std::unordered_map<std::string, std::vector<std::move_only_function<void(SPIRVModule const&)>>> callbacks_;
    slang::SlangSession const* session_;
};

} // namespace projectv::render
```

---

## Статус

| Компонент          | Статус         | Приоритет |
|--------------------|----------------|-----------|
| SlangSession       | Специфицирован | P0        |
| SPIRVModule        | Специфицирован | P0        |
| SVORayMarch shader | Специфицирован | P0        |
| Hot Reload         | Специфицирован | P1        |
| Specialization     | Специфицирован | P1        |

---

## Ссылки

- [ADR-0001: Vulkan Renderer](../../architecture/adr/0001-vulkan-renderer.md)
- [ADR-0002: SVO Storage](../../architecture/adr/0002-svo-storage.md)
- [SVO Architecture](../../architecture/practice/00_svo-architecture.md)
