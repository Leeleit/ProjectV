# ADR-0002: Sparse Voxel Octree (SVO) Storage Architecture

---

## Контекст

ProjectV требует эффективного хранения и рендеринга воксельных миров размером до 1 км³ (~30 млрд вокселей). Традиционные
плотные массивы неприемлемы по памяти. Требуется:

- Компактное представление разреженных данных
- GPU-friendly структура для ray marching
- Поддержка динамических изменений (разрушение/строительство)
- Интеграция с JoltPhysics для точных коллизий

## Решение

Принята **гибридная архитектура SVO + Chunks** с DAG-сжатием для статических регионов.

---

## 1. Data Layout: SVO Node

### 1.1 CPU-Side Node (64-bit)

```cpp
// Спецификация интерфейса
export module ProjectV.Voxel.SVOTypes;

import std;
import glm;

export namespace projectv::voxel {

/// Узел SVO — 64 бита (8 байт).
/// Битовая структура оптимизирована для GPU и cache locality.
///
/// ┌─────────────────────────────────────────────────────────────────┐
/// │ Bits 63-56: child_mask (8 bits) — маска существующих детей      │
/// │ Bits 55-28: child_ptr (28 bits) — индекс первого дочернего узла │
/// │ Bits 27-00: voxel_ptr (28 bits) — индекс данных вокселя         │
/// └─────────────────────────────────────────────────────────────────┘
struct SVONode {
    uint64_t data{0};

    // --- Getters ---

    [[nodiscard]] auto child_mask() const noexcept -> uint8_t {
        return static_cast<uint8_t>((data >> 56) & 0xFF);
    }

    [[nodiscard]] auto child_ptr() const noexcept -> uint32_t {
        return static_cast<uint32_t>((data >> 28) & 0x0FFFFFFF);
    }

    [[nodiscard]] auto voxel_ptr() const noexcept -> uint32_t {
        return static_cast<uint32_t>(data & 0x0FFFFFFF);
    }

    // --- Setters ---

    auto set_child_mask(uint8_t mask) noexcept -> void {
        data = (data & ~(0xFFULL << 56)) | (static_cast<uint64_t>(mask) << 56);
    }

    auto set_child_ptr(uint32_t ptr) noexcept -> void {
        data = (data & ~(0x0FFFFFFFULL << 28)) | (static_cast<uint64_t>(ptr) << 28);
    }

    auto set_voxel_ptr(uint32_t ptr) noexcept -> void {
        data = (data & ~0x0FFFFFFFULL) | static_cast<uint64_t>(ptr);
    }

    // --- Predicates ---

    [[nodiscard]] auto has_children() const noexcept -> bool {
        return child_mask() != 0;
    }

    [[nodiscard]] auto is_leaf() const noexcept -> bool {
        return child_mask() == 0 && voxel_ptr() != 0;
    }

    [[nodiscard]] auto is_empty() const noexcept -> bool {
        return data == 0;
    }

    [[nodiscard]] auto has_child(uint8_t index) const noexcept -> bool {
        return (child_mask() & (1 << index)) != 0;
    }

    // --- Child Index Calculation ---

    /// Вычисляет индекс дочернего узла по октанту (0-7).
    /// Октанты нумеруются по Z-кривой:
    ///   0: (0,0,0), 1: (1,0,0), 2: (0,1,0), 3: (1,1,0)
    ///   4: (0,0,1), 5: (1,0,1), 6: (0,1,1), 7: (1,1,1)
    [[nodiscard]] auto child_index(uint8_t octant) const noexcept -> uint32_t {
        // Popcount для маски младших битов
        uint8_t mask_before = child_mask() & ((1 << octant) - 1);
        uint8_t offset = std::popcount(mask_before);
        return child_ptr() + offset;
    }
};

static_assert(sizeof(SVONode) == 8, "SVONode must be 8 bytes");
static_assert(std::is_trivially_copyable_v<SVONode>, "SVONode must be trivially copyable");

} // namespace projectv::voxel
```

### 1.2 GPU-Side Node (std430 aligned)

```cpp
// Спецификация для GPU (SSBO, layout std430)
export module ProjectV.Voxel.SVOGPU;

import std;
import glm;

export namespace projectv::voxel::gpu {

/// GPU-узел SVO с выравниванием std430 для SSBO.
/// Совместим с Slang: struct SVONode { uint child_ptr; uint voxel_ptr; uint child_mask; uint reserved; }
struct alignas(16) GPUSVONode {
    uint32_t child_ptr{0};      ///< Индекс первого дочернего узла
    uint32_t voxel_ptr{0};      ///< Индекс данных вокселя
    uint32_t child_mask{0};     ///< Маска существующих детей
    uint32_t reserved{0};       ///< Reserved for alignment
};

static_assert(sizeof(GPUSVONode) == 16);
static_assert(alignof(GPUSVONode) == 16);

/// Данные вокселя для GPU (std430 aligned)
struct alignas(4) GPUVoxelData {
    uint16_t material_id{0};    ///< ID материала
    uint8_t density{0};         ///< Плотность (для физики)
    uint8_t flags{0};           ///< Флаги (solid, transparent, etc.)
};

static_assert(sizeof(GPUVoxelData) == 4);

/// Материал вокселя для GPU (std430 aligned)
struct alignas(16) GPUVoxelMaterial {
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};     ///< RGBA
    glm::vec4 emissive{0.0f, 0.0f, 0.0f, 0.0f};      ///< Emissive RGB + intensity
    glm::vec4 pbr_params{0.5f, 0.0f, 0.0f, 0.0f};    ///< roughness, metallic, transmission, flags
};

static_assert(sizeof(GPUVoxelMaterial) == 48);
static_assert(alignof(GPUVoxelMaterial) == 16);

} // namespace projectv::voxel::gpu
```

---

## 2. Data Layout: Voxel Data & Materials

```cpp
// Спецификация интерфейса
export module ProjectV.Voxel.VoxelData;

import std;
import glm;
import glaze; // Сериализация

export namespace projectv::voxel {

/// CPU-side данные вокселя
struct VoxelData {
    uint16_t material_id{0};    ///< 0 = воздух, 1+ = solid materials
    uint8_t density{255};       ///< 0-255, используется для разрушаемости
    uint8_t flags{0};           ///< Флаги (см. VoxelFlags)

    [[nodiscard]] auto is_solid() const noexcept -> bool {
        return material_id != 0;
    }

    [[nodiscard]] auto is_transparent() const noexcept -> bool {
        return (flags & static_cast<uint8_t>(VoxelFlags::Transparent)) != 0;
    }

    [[nodiscard]] auto is_liquid() const noexcept -> bool {
        return (flags & static_cast<uint8_t>(VoxelFlags::Liquid)) != 0;
    }

    /// Сериализация через Glaze
    struct glaze {
        using T = VoxelData;
        static constexpr auto value = glz::object(
            "material_id", &T::material_id,
            "density", &T::density,
            "flags", &T::flags
        );
    };
};

static_assert(sizeof(VoxelData) == 4);

/// Флаги вокселя
export enum class VoxelFlags : uint8_t {
    None        = 0,
    Transparent = 1 << 0,   ///< Полупрозрачный (стекло, вода)
    Liquid      = 1 << 1,   ///< Жидкость (вода, лава)
    Emissive    = 1 << 2,   ///< Излучает свет
    Destructible = 1 << 3,  ///< Можно разрушить
    Gravity     = 1 << 4,   ///< Подвержен гравитации (сыпучие)
    Organic     = 1 << 5,   ///< Органический (растения)
};

/// Материал вокселя
struct VoxelMaterial {
    glm::vec4 base_color{1.0f};
    glm::vec4 emissive{0.0f};
    float roughness{0.5f};
    float metallic{0.0f};
    float transmission{0.0f};
    float ior{1.5f};         ///< Index of refraction для прозрачных
    uint32_t flags{0};
    uint32_t texture_id{0};  ///< ID текстуры (0 = procedural)

    struct glaze {
        using T = VoxelMaterial;
        static constexpr auto value = glz::object(
            "base_color", &T::base_color,
            "emissive", &T::emissive,
            "roughness", &T::roughness,
            "metallic", &T::metallic,
            "transmission", &T::transmission,
            "ior", &T::ior,
            "flags", &T::flags,
            "texture_id", &T::texture_id
        );
    };
};

} // namespace projectv::voxel
```

---

## 3. SVO Tree Interface

```cpp
// Спецификация интерфейса
export module ProjectV.Voxel.SVOTree;

import std;
import glm;
import ProjectV.Voxel.SVOTypes;
import ProjectV.Voxel.VoxelData;
import ProjectV.Render.MemoryAllocator;

export namespace projectv::voxel {

/// Результат операции с SVO
export enum class SVOError : uint8_t {
    InvalidCoordinate,      ///< Координата вне границ мира
    DepthExceeded,          ///< Превышена максимальная глубина
    NodeNotFound,           ///< Узел не найден
    AllocationFailed,       ///< Не удалось выделить память
    GPUUploadFailed,        ///< Не удалось загрузить на GPU
    InvalidVoxelData        ///< Некорректные данные вокселя
};

/// SVO Tree — основная структура хранения вокселей
class SVOTree final {
public:
    /// Создаёт пустое SVO с указанной максимальной глубиной.
    /// Глубина 10 = мир 1024³ вокселей, глубина 12 = мир 4096³.
    [[nodiscard]] static auto create(uint32_t max_depth = 10) noexcept
        -> std::expected<SVOTree, SVOError>;

    ~SVOTree() noexcept = default;

    // Move-only
    SVOTree(SVOTree&&) noexcept = default;
    SVOTree& operator=(SVOTree&&) noexcept = default;
    SVOTree(const SVOTree&) = delete;
    SVOTree& operator=(const SVOTree&) = delete;

    // --- Query Operations ---

    /// Получает данные вокселя по мировым координатам.
    /// @param coord Мировые координаты (x, y, z)
    /// @returns VoxelData или SVOError::InvalidCoordinate
    [[nodiscard]] auto get(glm::ivec3 coord) const noexcept
        -> std::expected<VoxelData, SVOError>;

    /// Устанавливает воксель по координатам.
    /// @param coord Мировые координаты
    /// @param data Данные вокселя
    [[nodiscard]] auto set(glm::ivec3 coord, VoxelData data) noexcept
        -> std::expected<void, SVOError>;

    /// Проверяет существование вокселя.
    [[nodiscard]] auto exists(glm::ivec3 coord) const noexcept -> bool;

    /// Трассирует луч через SVO (DDA algorithm).
    /// @returns Точка пересечения или std::nullopt
    [[nodiscard]] auto raycast(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance
    ) const noexcept -> std::optional<RaycastHit>;

    // --- Bulk Operations ---

    /// Строит SVO из плотного 3D-массива.
    /// @param data Плотный массив вокселей
    /// @param extent Размеры массива (x, y, z)
    [[nodiscard]] auto build_from_dense(
        std::span<const VoxelData> data,
        glm::uvec3 extent
    ) noexcept -> std::expected<void, SVOError>;

    /// Экспортирует SVO в плотный массив (для тестов).
    [[nodiscard]] auto to_dense(glm::uvec3 extent) const noexcept
        -> std::expected<std::vector<VoxelData>, SVOError>;

    // --- Compression ---

    /// Сжимает SVO в DAG (Directed Acyclic Graph).
    /// Объединяет идентичные поддеревья.
    auto compress_dag() noexcept -> void;

    /// Возвращает коэффициент сжатия после DAG.
    [[nodiscard]] auto compression_ratio() const noexcept -> float;

    // --- GPU Upload ---

    /// Загружает SVO на GPU для ray marching.
    [[nodiscard]] auto upload_to_gpu(
        GPUAllocator const& allocator
    ) noexcept -> std::expected<void, SVOError>;

    /// Освобождает GPU ресурсы.
    auto release_gpu() noexcept -> void;

    // --- Statistics ---

    [[nodiscard]] auto node_count() const noexcept -> size_t { return nodes_.size(); }
    [[nodiscard]] auto voxel_count() const noexcept -> size_t { return voxels_.size(); }
    [[nodiscard]] auto max_depth() const noexcept -> uint32_t { return max_depth_; }
    [[nodiscard]] auto memory_usage() const noexcept -> size_t {
        return nodes_.size() * sizeof(SVONode) + voxels_.size() * sizeof(VoxelData);
    }
    [[nodiscard]] auto gpu_buffer_size() const noexcept -> size_t;

private:
    explicit SVOTree(uint32_t max_depth) noexcept;

    std::vector<SVONode> nodes_;
    std::vector<VoxelData> voxels_;
    uint32_t max_depth_;
    uint32_t root_index_{0};

    // GPU data
    struct GPUData;
    std::unique_ptr<GPUData> gpu_data_;
};

/// Результат raycast
struct RaycastHit {
    glm::ivec3 voxel_coord;     ///< Координата вокселя
    glm::vec3 hit_point;        ///< Точка пересечения
    glm::vec3 hit_normal;       ///< Нормаль грани
    float distance;             ///< Расстояние от origin
    VoxelData voxel_data;       ///< Данные вокселя
};

} // namespace projectv::voxel
```

---

## 4. Chunk Integration

```cpp
// Спецификация интерфейса
export module ProjectV.Voxel.Chunk;

import std;
import glm;
import ProjectV.Voxel.SVOTree;
import ProjectV.Voxel.VoxelData;

export namespace projectv::voxel {

/// Размер чанка в вокселях (32³ = 32768 вокселей)
export constexpr uint32_t CHUNK_SIZE = 32;

/// Координаты чанка в мире
struct ChunkCoord {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};

    [[nodiscard]] auto to_world_origin() const noexcept -> glm::ivec3 {
        return {x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE};
    }

    [[nodiscard]] static auto from_world_pos(glm::vec3 world_pos) noexcept -> ChunkCoord {
        return {
            static_cast<int32_t>(std::floor(world_pos.x / CHUNK_SIZE)),
            static_cast<int32_t>(std::floor(world_pos.y / CHUNK_SIZE)),
            static_cast<int32_t>(std::floor(world_pos.z / CHUNK_SIZE))
        };
    }

    auto operator<=>(ChunkCoord const&) const = default;
};

/// Хэш для ChunkCoord (для unordered_map)
struct ChunkCoordHash {
    [[nodiscard]] auto operator()(ChunkCoord const& c) const noexcept -> size_t {
        // Z-order curve для лучшей локальности
        uint64_t combined =
            (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32) |
            (static_cast<uint64_t>(static_cast<uint32_t>(c.y)) << 16) |
            static_cast<uint64_t>(static_cast<uint32_t>(c.z));
        return std::hash<uint64_t>{}(combined);
    }
};

/// Гибридный чанк: плотные данные + локальное SVO + Mesh
struct HybridChunk {
    ChunkCoord coord;

    /// Плотные данные чанка (32³ = 32768 вокселей)
    /// Используется для: быстрых локальных изменений, физики, Greedy Meshing
    std::unique_ptr<std::array<VoxelData, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE>> voxels;

    /// Локальное SVO (опционально)
    /// Используется для: ray marching внутри чанка, LOD
    std::optional<SVOTree> local_svo;

    /// Плотный доступ через std::mdspan
    [[nodiscard]] auto voxel_grid() noexcept -> std::mdspan<VoxelData, std::dextents<size_t, 3>> {
        return std::mdspan<VoxelData, std::dextents<size_t, 3>>(
            voxels->data(), CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE
        );
    }

    /// Получение вокселя по локальным координатам
    [[nodiscard]] auto get(uint32_t x, uint32_t y, uint32_t z) const noexcept
        -> VoxelData const& {
        return (*voxels)[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE];
    }

    /// Установка вокселя по локальным координатам
    auto set(uint32_t x, uint32_t y, uint32_t z, VoxelData data) noexcept -> void {
        (*voxels)[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = data;
        dirty = true;
    }

    // --- State Flags ---

    bool dirty{false};             ///< Требует перестроения mesh
    bool svo_dirty{false};         ///< Требует перестроения SVO
    bool loaded{false};            ///< Загружен с диска
    bool gpu_uploaded{false};      ///< Загружен на GPU
};

/// Менеджер чанков
class ChunkManager final {
public:
    /// Создаёт менеджер с указанным радиусом загрузки.
    /// @param load_radius Радиус в чанках (load_radius=10 → 21x21x21 чанков)
    [[nodiscard]] static auto create(uint32_t load_radius = 10) noexcept
        -> std::expected<ChunkManager, ChunkError>;

    /// Обновляет чанки по позиции камеры.
    /// Загружает новые, выгружает дальние.
    auto update(glm::vec3 camera_position) noexcept -> void;

    /// Получает чанк по координатам.
    [[nodiscard]] auto get_chunk(ChunkCoord coord) noexcept -> HybridChunk*;
    [[nodiscard]] auto get_chunk(ChunkCoord coord) const noexcept -> HybridChunk const*;

    /// Получает воксель по мировым координатам.
    [[nodiscard]] auto get_voxel(glm::ivec3 world_pos) const noexcept
        -> std::expected<VoxelData, ChunkError>;

    /// Устанавливает воксель по мировым координатам.
    [[nodiscard]] auto set_voxel(glm::ivec3 world_pos, VoxelData data) noexcept
        -> std::expected<void, ChunkError>;

    /// Возвращает все видимые чанки для рендеринга.
    [[nodiscard]] auto get_visible_chunks(glm::vec3 camera_pos, glm::vec3 camera_dir) const noexcept
        -> std::vector<HybridChunk const*>;

    /// Количество загруженных чанков.
    [[nodiscard]] auto chunk_count() const noexcept -> size_t { return chunks_.size(); }

private:
    explicit ChunkManager(uint32_t load_radius) noexcept;

    std::unordered_map<ChunkCoord, HybridChunk, ChunkCoordHash> chunks_;
    uint32_t load_radius_;
    glm::ivec3 last_camera_chunk_{INT32_MAX, INT32_MAX, INT32_MAX};

    auto load_chunk(ChunkCoord coord) noexcept -> HybridChunk*;
    auto unload_chunk(ChunkCoord coord) noexcept -> void;
};

export enum class ChunkError : uint8_t {
    ChunkNotLoaded,         ///< Чанк не загружен
    InvalidCoordinate,      ///< Координата вне границ
    AllocationFailed,       ///< Не удалось выделить память
    GenerationFailed,       ///< Ошибка генерации чанка
    IOFailed               ///< Ошибка чтения/записи
};

} // namespace projectv::voxel
```

---

## 5. GPU Upload Spec (SSBO Layout)

```slang
// Slang шейдер: SVOStructures.slang

module SVOStructures;

/// GPU-узел SVO (std430 layout)
struct SVONode {
    uint child_ptr;
    uint voxel_ptr;
    uint child_mask;
    uint reserved;
};

/// Данные вокселя (std430 layout)
struct VoxelData {
    uint16_t material_id;
    uint8_t density;
    uint8_t flags;
    // Padded to 4 bytes
};

/// Материал вокселя (std430 layout)
struct VoxelMaterial {
    float4 base_color;    // offset 0
    float4 emissive;      // offset 16
    float4 pbr_params;    // offset 32: roughness, metallic, transmission, flags
};

/// SVO данные для ray marching
[[vk::binding(0, 0)]]
StructuredBuffer<SVONode> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<VoxelData> voxelData;

[[vk::binding(2, 0)]]
StructuredBuffer<VoxelMaterial> materials;

/// Параметры ray marching
[[vk::push_constant]]
struct RayMarchParams {
    float4x4 inv_view_proj;
    float3 camera_pos;
    float max_distance;
    uint root_node;
    uint max_depth;
    float voxel_size;
    uint32_t pad0;
    uint32_t pad1;
};
```

---

## 6. SVO → GPU Buffer Layout

```cpp
// Спецификация буферов для GPU
export module ProjectV.Voxel.SVOBuffers;

import std;
import ProjectV.Render.MemoryAllocator;
import ProjectV.Voxel.SVOTypes;

export namespace projectv::voxel {

/// GPU буферы для SVO
struct SVOGPUBuffers {
    // SSBO 0: SVO Nodes
    BufferAllocation node_buffer;
    uint32_t node_count{0};

    // SSBO 1: Voxel Data
    BufferAllocation voxel_buffer;
    uint32_t voxel_count{0};

    // SSBO 2: Materials
    BufferAllocation material_buffer;
    uint32_t material_count{0};

    // Push Constants: RayMarchParams
    struct PushConstants {
        glm::mat4 inv_view_proj;
        glm::vec3 camera_pos;
        float max_distance;
        uint32_t root_node;
        uint32_t max_depth;
        float voxel_size;
        uint32_t pad0;
        uint32_t pad1;
    };

    /// Вычисляет общий размер GPU памяти.
    [[nodiscard]] auto total_size() const noexcept -> size_t {
        return node_buffer.size + voxel_buffer.size + material_buffer.size;
    }
};

/// Загрузчик SVO на GPU
class SVOGPUUploader final {
public:
    /// Загружает SVO данные на GPU.
    [[nodiscard]] static auto upload(
        GPUAllocator const& allocator,
        std::span<SVONode const> nodes,
        std::span<VoxelData const> voxels,
        std::span<VoxelMaterial const> materials
    ) noexcept -> std::expected<SVOGPUBuffers, SVOError>;

    /// Обновляет часть узлов (инкрементальное обновление).
    [[nodiscard]] static auto update_nodes(
        SVOGPUBuffers& buffers,
        GPUAllocator const& allocator,
        std::span<SVONode const> nodes,
        uint32_t offset
    ) noexcept -> std::expected<void, SVOError>;
};

} // namespace projectv::voxel
```

---

## Статус

| Компонент            | Статус         | Приоритет |
|----------------------|----------------|-----------|
| SVONode (64-bit)     | Специфицирован | P0        |
| GPU SVONode (std430) | Специфицирован | P0        |
| VoxelData            | Специфицирован | P0        |
| SVOTree              | Специфицирован | P0        |
| HybridChunk + mdspan | Специфицирован | P0        |
| ChunkManager         | Специфицирован | P1        |
| SVOGPUBuffers        | Специфицирован | P1        |

---

## Последствия

### Положительные:

- Компактное хранение (8 байт/узел против 32+ для плотных массивов)
- GPU-friendly layout с std430 выравниванием
- std::mdspan для zero-overhead 3D доступа
- Гибридный подход: чанки для физики, SVO для рендеринга

### Риски:

- DAG-сжатие требует дополнительного прохода
- Инкрементальные обновления SVO сложнее чем для чанков
