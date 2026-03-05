# Спецификация воксельного пайплайна ProjectV

**Статус:** Утверждено
**Версия:** 2.0
**Дата:** 2026-02-22

---

## Обзор

ProjectV рендерит миллионы вокселей в реальном времени через полностью **GPU-Driven pipeline**:

1. **Compute Shaders** генерируют геометрию на GPU
2. **Indirect Drawing** минимизирует draw calls
3. **Bindless Rendering** позволяет тысячи текстур
4. **Sparse Voxel Octree** хранит разреженные миры
5. **Async Compute** параллелизирует генерацию и рендеринг

---

## Интерфейсы Voxel Data

```cpp
// ProjectV.Voxel.Data.cppm
export module ProjectV.Voxel.Data;

import std;
import glm;

export namespace projectv::voxel {

/// Данные одного вокселя (4 байта).
export struct VoxelData {
    uint16_t material_id{0};   ///< ID материала (0-65535, 0 = air)
    uint8_t flags{0};          ///< Флаги (solid, transparent, etc.)
    uint8_t light{0};          ///< Уровень освещённости (0-255)

    [[nodiscard]] auto is_solid() const noexcept -> bool {
        return material_id != 0;
    }

    [[nodiscard]] auto is_transparent() const noexcept -> bool {
        return (flags & 0x01) != 0;
    }
};

/// Константы чанка.
export constexpr uint32_t CHUNK_SIZE = 32;
export constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
export constexpr float VOXEL_SIZE = 1.0f;

/// Координаты вокселя в чанке.
export using VoxelCoord = glm::ivec3;

/// Координаты чанка в мире.
export struct ChunkCoord {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};

    [[nodiscard]] auto operator==(ChunkCoord const& other) const noexcept -> bool = default;
    [[nodiscard]] auto operator<=>(ChunkCoord const& other) const noexcept = default;
};

/// Hash для ChunkCoord.
export struct ChunkCoordHash {
    [[nodiscard]] auto operator()(ChunkCoord const& c) const noexcept -> size_t;
};

} // namespace projectv::voxel
```

---

## Интерфейсы Chunk Storage (с std::mdspan)

```cpp
// ProjectV.Voxel.Storage.cppm
export module ProjectV.Voxel.Storage;

import std;
import glm;
import ProjectV.Voxel.Data;

export namespace projectv::voxel {

/// Хранилище вокселей чанка с multidimensional access.
export class ChunkStorage {
public:
    /// Создаёт хранилище заданного размера.
    explicit ChunkStorage(uint32_t size = CHUNK_SIZE) noexcept;

    ~ChunkStorage() noexcept;

    ChunkStorage(ChunkStorage&&) noexcept;
    ChunkStorage& operator=(ChunkStorage&&) noexcept;
    ChunkStorage(const ChunkStorage&) = delete;
    ChunkStorage& operator=(const ChunkStorage&) = delete;

    /// Получает воксель по координатам.
    /// @param coord Локальные координаты в чанке
    [[nodiscard]] auto get(VoxelCoord coord) const noexcept
        -> std::expected<VoxelData, VoxelError>;

    /// Устанавливает воксель.
    [[nodiscard]] auto set(VoxelCoord coord, VoxelData voxel) noexcept
        -> std::expected<void, VoxelError>;

    /// Получает mdspan для векторизованного доступа.
    /// @return 3D view данных чанка
    [[nodiscard]] auto as_mdspan() noexcept
        -> std::mdspan<VoxelData, std::dextents<size_t, 3>>;

    /// Получает const mdspan.
    [[nodiscard]] auto as_mdspan() const noexcept
        -> std::mdspan<VoxelData const, std::dextents<size_t, 3>>;

    /// Получает сырые данные.
    [[nodiscard]] auto data() noexcept -> VoxelData*;
    [[nodiscard]] auto data() const noexcept -> VoxelData const*;

    /// Получает размер чанка.
    [[nodiscard]] auto size() const noexcept -> uint32_t;

    /// Заполняет весь чанк одним значением.
    auto fill(VoxelData voxel) noexcept -> void;

    /// Проверяет, все ли воксели air.
    [[nodiscard]] auto is_empty() const noexcept -> bool;

    /// Подсчитывает непустые воксели.
    [[nodiscard]] auto count_solid() const noexcept -> size_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Утилиты для работы с координатами.
export class CoordUtils {
public:
    /// Конвертирует мировые координаты в координаты чанка.
    [[nodiscard]] static auto world_to_chunk(glm::vec3 world_pos) noexcept -> ChunkCoord;

    /// Конвертирует мировые координаты в локальные координаты чанка.
    [[nodiscard]] static auto world_to_local(glm::ivec3 world_coord) noexcept -> VoxelCoord;

    /// Конвертирует координаты чанка в мировые.
    [[nodiscard]] static auto chunk_to_world(ChunkCoord chunk) noexcept -> glm::vec3;

    /// Вычисляет линейный индекс в массиве.
    [[nodiscard]] static auto to_index(VoxelCoord coord, uint32_t size) noexcept -> uint32_t;

    /// Вычисляет 3D координаты из индекса.
    [[nodiscard]] static auto from_index(uint32_t index, uint32_t size) noexcept -> VoxelCoord;

    /// Проверяет валидность координат.
    [[nodiscard]] static auto is_valid(VoxelCoord coord, uint32_t size) noexcept -> bool;
};

} // namespace projectv::voxel
```

---

## Интерфейсы SIMD Processing

```cpp
// ProjectV.Voxel.SIMD.cppm
export module ProjectV.Voxel.SIMD;

import std;
import glm;
import ProjectV.Voxel.Data;

export namespace projectv::voxel {

/// SIMD-ускоренная обработка вокселей.
export class VoxelSIMD {
public:
    /// Векторизованное заполнение чанка.
    static auto fill_simd(
        std::mdspan<VoxelData, std::dextents<size_t, 3>> data,
        VoxelData value
    ) noexcept -> void;

    /// Векторизованный подсчёт solid вокселей.
    [[nodiscard]] static auto count_solid_simd(
        std::mdspan<VoxelData const, std::dextents<size_t, 3>> data
    ) noexcept -> size_t;

    /// Векторизованная проверка на пустоту.
    [[nodiscard]] static auto is_empty_simd(
        std::mdspan<VoxelData const, std::dextents<size_t, 3>> data
    ) noexcept -> bool;

    /// Векторизованное копирование чанка.
    static auto copy_simd(
        std::mdspan<VoxelData const, std::dextents<size_t, 3>> src,
        std::mdspan<VoxelData, std::dextents<size_t, 3>> dst
    ) noexcept -> void;

    /// Векторизованная проверка соседей для mesh generation.
    struct NeighborMask {
        std::simd_mask<uint8_t, std::simd_abi::native> mask;
    };

    [[nodiscard]] static auto check_neighbors_simd(
        std::mdspan<VoxelData const, std::dextents<size_t, 3>> data,
        VoxelCoord coord
    ) noexcept -> NeighborMask;

private:
    static constexpr size_t SIMD_WIDTH = std::simd_abi::native<uint8_t>::size;
};

/// Векторизованный итератор чанка.
export class ChunkIteratorSIMD {
public:
    explicit ChunkIteratorSIMD(uint32_t size) noexcept;

    /// Обрабатывает batch вокселей.
    /// @param process Функция обработки (вызывается для SIMD_WIDTH вокселей)
    template<typename ProcessFunc>
    auto for_each_batch(ProcessFunc&& process) noexcept -> void;

    /// @return Текущая позиция в чанке
    [[nodiscard]] auto position() const noexcept -> VoxelCoord;

    /// @return Количество обработанных вокселей
    [[nodiscard]] auto processed_count() const noexcept -> size_t;

private:
    uint32_t size_;
    size_t current_index_{0};
    size_t total_count_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы Mesh Generation

```cpp
// ProjectV.Voxel.MeshGen.cppm
export module ProjectV.Voxel.MeshGen;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.Storage;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel {

/// Вершина воксельного меша.
export struct VoxelVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    uint16_t material_id{0};
    uint8_t ao{0};  // Ambient Occlusion
    uint8_t _pad{0};
};

/// Результат генерации меша.
export struct MeshGenResult {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 bounds_min{};
    glm::vec3 bounds_max{};
    uint32_t opaque_face_count{0};
    uint32_t transparent_face_count{0};
};

/// Алгоритмы генерации мешей.
export enum class MeshGenAlgorithm : uint8_t {
    Naive,          ///< По одной грани на каждый видимый воксель
    Greedy,         ///< Объединение граней (50-90% меньше вершин)
    MarchingCubes,  ///< Плавные поверхности из density field
    DualContouring  ///< Острые углы + плавные поверхности
};

/// Конфигурация генератора.
export struct MeshGenConfig {
    MeshGenAlgorithm algorithm{MeshGenAlgorithm::Greedy};
    bool calculate_ao{true};        ///< Вычислять ambient occlusion
    bool separate_transparent{true}; ///< Отдельные меши для прозрачных
    float voxel_size{1.0f};
};

/// Коды ошибок генерации.
export enum class MeshGenError : uint8_t {
    InvalidChunk,
    EmptyChunk,
    AllocationFailed,
    GpuUploadFailed
};

/// CPU генератор мешей.
export class CPUMeshGenerator {
public:
    explicit CPUMeshGenerator(MeshGenConfig const& config = {}) noexcept;

    /// Генерирует меш для чанка.
    [[nodiscard]] auto generate(ChunkStorage const& chunk) noexcept
        -> std::expected<MeshGenResult, MeshGenError>;

    /// Генерирует меш с учётом соседних чанков.
    [[nodiscard]] auto generate_with_neighbors(
        ChunkStorage const& chunk,
        ChunkStorage const* neighbor_px,
        ChunkStorage const* neighbor_nx,
        ChunkStorage const* neighbor_py,
        ChunkStorage const* neighbor_ny,
        ChunkStorage const* neighbor_pz,
        ChunkStorage const* neighbor_nz
    ) noexcept -> std::expected<MeshGenResult, MeshGenError>;

    /// Устанавливает конфигурацию.
    auto set_config(MeshGenConfig const& config) noexcept -> void;

private:
    auto generate_naive(ChunkStorage const& chunk) noexcept -> MeshGenResult;
    auto generate_greedy(ChunkStorage const& chunk) noexcept -> MeshGenResult;
    auto calculate_vertex_ao(
        ChunkStorage const& chunk,
        VoxelCoord coord,
        glm::ivec3 normal
    ) noexcept -> uint8_t;

    MeshGenConfig config_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы GPU Mesh Generation

```cpp
// ProjectV.Voxel.GPUMesh.cppm
export module ProjectV.Voxel.GPUMesh;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.Storage;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel {

/// GPU ресурсы для генерации мешей.
export struct GPUMeshResources {
    VkBuffer voxel_buffer{VK_NULL_HANDLE};
    VkBuffer vertex_buffer{VK_NULL_HANDLE};
    VkBuffer index_buffer{VK_NULL_HANDLE};
    VkBuffer indirect_buffer{VK_NULL_HANDLE};
    VkBuffer counter_buffer{VK_NULL_HANDLE};
};

/// GPU генератор мешей через Compute Shaders.
export class GPUMeshGenerator {
public:
    /// Создаёт GPU генератор.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        uint32_t max_vertices_per_chunk = 65536
    ) noexcept -> std::expected<GPUMeshGenerator, MeshGenError>;

    ~GPUMeshGenerator() noexcept;

    GPUMeshGenerator(GPUMeshGenerator&&) noexcept;
    GPUMeshGenerator& operator=(GPUMeshGenerator&&) noexcept;
    GPUMeshGenerator(const GPUMeshGenerator&) = delete;
    GPUMeshGenerator& operator=(const GPUMeshGenerator&) = delete;

    /// Загружает данные чанка на GPU.
    [[nodiscard]] auto upload_chunk(
        VkCommandBuffer cmd,
        ChunkStorage const& chunk,
        ChunkCoord coord
    ) noexcept -> std::expected<void, MeshGenError>;

    /// Генерирует меш на GPU.
    auto dispatch_generation(
        VkCommandBuffer cmd,
        ChunkCoord coord
    ) noexcept -> void;

    /// Получает результаты генерации.
    [[nodiscard]] auto read_results(
        VkCommandBuffer cmd,
        ChunkCoord coord
    ) noexcept -> std::expected<MeshGenResult, MeshGenError>;

    /// Получает indirect draw command.
    [[nodiscard]] auto get_indirect_command(ChunkCoord coord) const noexcept
        -> VkDrawIndexedIndirectCommand;

    /// Получает GPU буферы.
    [[nodiscard]] auto resources(ChunkCoord coord) const noexcept
        -> GPUMeshResources const*;

private:
    GPUMeshGenerator() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Mesh Shader генератор (VK_EXT_mesh_shader).
export class MeshShaderGenerator {
public:
    /// Создаёт генератор.
    [[nodiscard]] static auto create(
        VkDevice device,
        VkPhysicalDevice physical_device
    ) noexcept -> std::expected<MeshShaderGenerator, MeshGenError>;

    ~MeshShaderGenerator() noexcept;

    MeshShaderGenerator(MeshShaderGenerator&&) noexcept;
    MeshShaderGenerator& operator=(MeshShaderGenerator&&) noexcept;
    MeshShaderGenerator(const MeshShaderGenerator&) = delete;
    MeshShaderGenerator& operator=(const MeshShaderGenerator&) = delete;

    /// Проверяет поддержку mesh shaders.
    [[nodiscard]] static auto is_supported(VkPhysicalDevice physical_device) noexcept -> bool;

    /// Генерирует и рендерит чанк через mesh shaders.
    auto render_chunk(
        VkCommandBuffer cmd,
        ChunkCoord coord,
        ChunkStorage const& chunk
    ) noexcept -> void;

    /// Получает pipeline.
    [[nodiscard]] auto pipeline() const noexcept -> VkPipeline;

private:
    MeshShaderGenerator() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы SVO (Sparse Voxel Octree)

```cpp
// ProjectV.Voxel.SVO.cppm
export module ProjectV.Voxel.SVO;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel {

/// Сжатый узел SVO (64 бита).
export struct SVONode {
    uint64_t data{0};

    // Bit fields (access через методы для portability)
    // children_mask: 8 бит - какие дети существуют
    // leaf_mask: 8 бит - какие дети являются leaf
    // child_ptr: 24 бита - указатель на детей
    // material_ptr: 24 бита - указатель на материалы

    [[nodiscard]] auto has_child(uint8_t index) const noexcept -> bool;
    [[nodiscard]] auto is_leaf(uint8_t index) const noexcept -> bool;
    [[nodiscard]] auto child_ptr() const noexcept -> uint32_t;
    [[nodiscard]] auto material_ptr() const noexcept -> uint32_t;

    auto set_child(uint8_t index, bool exists) noexcept -> void;
    auto set_leaf(uint8_t index, bool is_leaf) noexcept -> void;
    auto set_child_ptr(uint32_t ptr) noexcept -> void;
    auto set_material_ptr(uint32_t ptr) noexcept -> void;
};

/// Конфигурация SVO.
export struct SVOConfig {
    uint32_t max_depth{10};            ///< Максимальная глубина (1024³ max size)
    uint32_t initial_node_capacity{65536};
    bool enable_dag_compression{true}; ///< Сжатие одинаковых subtree
};

/// Sparse Voxel Octree.
export class SVOTree {
public:
    /// Создаёт пустое SVO.
    explicit SVOTree(SVOConfig const& config = {}) noexcept;

    ~SVOTree() noexcept;

    SVOTree(SVOTree&&) noexcept;
    SVOTree& operator=(SVOTree&&) noexcept;
    SVOTree(const SVOTree&) = delete;
    SVOTree& operator=(const SVOTree&) = delete;

    /// Получает воксель по координатам.
    [[nodiscard]] auto get(VoxelCoord coord) const noexcept
        -> std::expected<VoxelData, SVOError>;

    /// Устанавливает воксель.
    [[nodiscard]] auto set(VoxelCoord coord, VoxelData voxel) noexcept
        -> std::expected<void, SVOError>;

    /// Строит SVO из плотного массива.
    auto build_from_dense(
        std::mdspan<VoxelData const, std::dextents<size_t, 3>> data
    ) noexcept -> void;

    /// Выполняет DAG сжатие.
    [[nodiscard]] auto compress_dag() noexcept -> size_t;

    /// Загружает на GPU.
    [[nodiscard]] auto upload_to_gpu(
        VkDevice device,
        VmaAllocator allocator
    ) noexcept -> std::expected<void, SVOError>;

    /// @return Количество узлов
    [[nodiscard]] auto node_count() const noexcept -> size_t;

    /// @return Использование памяти
    [[nodiscard]] auto memory_usage() const noexcept -> size_t;

    /// @return GPU буфер с узлами
    [[nodiscard]] auto gpu_buffer() const noexcept -> VkBuffer;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// GPU SVO traversal для ray tracing.
export class SVORayTracer {
public:
    /// Создаёт ray tracer.
    [[nodiscard]] static auto create(
        VkDevice device,
        SVOTree const& svo
    ) noexcept -> std::expected<SVORayTracer, SVOError>;

    ~SVORayTracer() noexcept;

    SVORayTracer(SVORayTracer&&) noexcept;
    SVORayTracer& operator=(SVORayTracer&&) noexcept;
    SVORayTracer(const SVORayTracer&) = delete;
    SVORayTracer& operator=(const SVORayTracer&) = delete;

    /// Traces ray через SVO.
    struct RayResult {
        bool hit{false};
        glm::vec3 hit_point{0.0f};
        glm::vec3 hit_normal{0.0f};
        float distance{0.0f};
        VoxelData voxel{};
    };

    [[nodiscard]] auto trace_ray(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance
    ) const noexcept -> RayResult;

    /// Traces ray на GPU через compute shader.
    auto dispatch_trace(
        VkCommandBuffer cmd,
        VkBuffer ray_buffer,
        VkBuffer result_buffer,
        uint32_t ray_count
    ) noexcept -> void;

private:
    SVORayTracer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы DDA Raycast

```cpp
// ProjectV.Voxel.Raycast.cppm
export module ProjectV.Voxel.Raycast;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.Storage;

export namespace projectv::voxel {

/// Результат raycast.
export struct VoxelHit {
    bool hit{false};
    VoxelCoord position{0};
    VoxelCoord normal{0};      ///< Нормаль грани (для размещения блоков)
    float distance{0.0f};
    VoxelData voxel{};
};

/// DDA (Digital Differential Analyzer) raycast.
/// O(distance) вместо O(volume) для ray-box intersection.
export class DDARaycast {
public:
    /// Выполняет raycast через чанк.
    /// @param origin Начальная точка
    /// @param direction Направление (нормализованное)
    /// @param max_distance Максимальная дистанция
    /// @param chunk Хранилище вокселей
    [[nodiscard]] static auto cast(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance,
        ChunkStorage const& chunk
    ) noexcept -> VoxelHit;

    /// Выполняет raycast через мир (несколько чанков).
    template<typename WorldAccessor>
    [[nodiscard]] static auto cast_world(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance,
        WorldAccessor&& world
    ) noexcept -> VoxelHit;

private:
    /// Вычисляет расстояние до следующей границы сетки.
    [[nodiscard]] static auto intbound(float s, float ds) noexcept -> float;
};

/// SIMD версия raycast для множества лучей.
export class DDARaycastSIMD {
public:
    /// Результат для 8 лучей.
    struct HitBatch {
        std::array<bool, 8> hits{};
        std::array<VoxelCoord, 8> positions{};
        std::array<VoxelCoord, 8> normals{};
        std::array<float, 8> distances{};
    };

    /// Raycast для 8 лучей одновременно.
    [[nodiscard]] static auto cast_batch(
        std::array<glm::vec3, 8> const& origins,
        std::array<glm::vec3, 8> const& directions,
        float max_distance,
        ChunkStorage const& chunk
    ) noexcept -> HitBatch;
};

} // namespace projectv::voxel
```

---

## Интерфейсы Voxel Renderer

```cpp
// ProjectV.Voxel.Render.cppm
export module ProjectV.Voxel.Render;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.Storage;
import ProjectV.Voxel.MeshGen;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel {

/// Метод рендеринга вокселей.
export enum class RenderMethod : uint8_t {
    MeshShaders,      ///< VK_EXT_mesh_shader (самый эффективный)
    ComputeToVertex,  ///< Compute генерация + vertex shader
    Traditional,      ///< CPU генерация + vertex buffer
    SoftwareFallback  ///< CPU рендеринг (для отладки)
};

/// Конфигурация рендерера.
export struct VoxelRendererConfig {
    RenderMethod method{RenderMethod::ComputeToVertex};
    uint32_t max_chunks{4096};
    uint32_t max_vertices_per_chunk{65536};
    bool use_bindless_textures{true};
    bool enable_lod{true};
    float lod_distances[4] = {50.0f, 100.0f, 200.0f, 400.0f};
};

/// Воксельный рендерер.
export class VoxelRenderer {
public:
    /// Создаёт рендерер.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        VoxelRendererConfig const& config = {}
    ) noexcept -> std::expected<VoxelRenderer, VoxelError>;

    ~VoxelRenderer() noexcept;

    VoxelRenderer(VoxelRenderer&&) noexcept;
    VoxelRenderer& operator=(VoxelRenderer&&) noexcept;
    VoxelRenderer(const VoxelRenderer&) = delete;
    VoxelRenderer& operator=(const VoxelRenderer&) = delete;

    /// Регистрирует чанк для рендеринга.
    [[nodiscard]] auto register_chunk(
        ChunkCoord coord,
        ChunkStorage const& storage
    ) noexcept -> std::expected<void, VoxelError>;

    /// Обновляет данные чанка.
    [[nodiscard]] auto update_chunk(
        ChunkCoord coord,
        ChunkStorage const& storage
    ) noexcept -> std::expected<void, VoxelError>;

    /// Удаляет чанк.
    auto unregister_chunk(ChunkCoord coord) noexcept -> void;

    /// Выполняет frustum culling на GPU.
    auto perform_culling(
        VkCommandBuffer cmd,
        glm::mat4 const& view_proj
    ) noexcept -> void;

    /// Рендерит все видимые чанки.
    auto render(
        VkCommandBuffer cmd,
        glm::mat4 const& view_proj,
        glm::vec3 const& camera_position
    ) noexcept -> void;

    /// Получает статистику.
    struct Stats {
        uint32_t visible_chunks{0};
        uint32_t total_vertices{0};
        uint32_t draw_calls{0};
        float gpu_time_ms{0.0f};
    };
    [[nodiscard]] auto stats() const noexcept -> Stats;

    /// Включает/выключает LOD.
    auto set_lod_enabled(bool enabled) noexcept -> void;

private:
    VoxelRenderer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы Indirect Drawing

```cpp
// ProjectV.Voxel.Indirect.cppm
export module ProjectV.Voxel.Indirect;

import std;
import ProjectV.Render.Vulkan;
import ProjectV.Voxel.Data;

export namespace projectv::voxel {

/// Indirect draw command для чанка.
export struct alignas(16) ChunkDrawCommand {
    uint32_t index_count{0};
    uint32_t instance_count{1};
    uint32_t first_index{0};
    int32_t vertex_offset{0};
    uint32_t first_instance{0};

    // Дополнительные данные для шейдера
    ChunkCoord chunk_coord;
    uint32_t lod_level{0};
    uint32_t material_offset{0};
};

/// Менеджер indirect draw commands.
export class IndirectDrawManager {
public:
    /// Создаёт менеджер.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        uint32_t max_commands = 4096
    ) noexcept -> std::expected<IndirectDrawManager, VoxelError>;

    ~IndirectDrawManager() noexcept;

    IndirectDrawManager(IndirectDrawManager&&) noexcept;
    IndirectDrawManager& operator=(IndirectDrawManager&&) noexcept;
    IndirectDrawManager(const IndirectDrawManager&) = delete;
    IndirectDrawManager& operator=(const IndirectDrawManager&) = delete;

    /// Добавляет draw command.
    [[nodiscard]] auto add_command(ChunkDrawCommand const& cmd) noexcept
        -> std::expected<uint32_t, VoxelError>;

    /// Обновляет draw command.
    auto update_command(uint32_t index, ChunkDrawCommand const& cmd) noexcept -> void;

    /// Удаляет draw command.
    auto remove_command(uint32_t index) noexcept -> void;

    /// Очищает все команды.
    auto clear() noexcept -> void;

    /// Получает GPU буфер.
    [[nodiscard]] auto buffer() const noexcept -> VkBuffer;

    /// Получает количество команд.
    [[nodiscard]] auto count() const noexcept -> uint32_t;

    /// Выполняет multi-draw indirect.
    auto draw_indirect(VkCommandBuffer cmd, VkPipelineLayout layout) noexcept -> void;

private:
    IndirectDrawManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Интерфейсы Bindless Textures

```cpp
// ProjectV.Voxel.Bindless.cppm
export module ProjectV.Voxel.Bindless;

import std;
import glm;
import ProjectV.Render.Vulkan;
import ProjectV.Resource.Manager;

export namespace projectv::voxel {

/// Материал вокселя.
export struct VoxelMaterial {
    resource::ResourceHandle albedo_texture;
    resource::ResourceHandle normal_texture;
    resource::ResourceHandle roughness_texture;
    glm::vec4 base_color{1.0f};
    float roughness{0.5f};
    float metallic{0.0f};
    uint16_t material_id{0};
};

/// Менеджер bindless текстур для вокселей.
export class VoxelTextureManager {
public:
    /// Создаёт менеджер.
    [[nodiscard]] static auto create(
        VkDevice device,
        VkDescriptorPool pool,
        uint32_t max_textures = 1024
    ) noexcept -> std::expected<VoxelTextureManager, VoxelError>;

    ~VoxelTextureManager() noexcept;

    VoxelTextureManager(VoxelTextureManager&&) noexcept;
    VoxelTextureManager& operator=(VoxelTextureManager&&) noexcept;
    VoxelTextureManager(const VoxelTextureManager&) = delete;
    VoxelTextureManager& operator=(const VoxelTextureManager&) = delete;

    /// Регистрирует текстуру.
    [[nodiscard]] auto register_texture(
        VkImageView view,
        VkSampler sampler
    ) noexcept -> std::expected<uint32_t, VoxelError>;

    /// Удаляет текстуру.
    auto unregister_texture(uint32_t index) noexcept -> void;

    /// Получает descriptor set.
    [[nodiscard]] auto descriptor_set() const noexcept -> VkDescriptorSet;

    /// Получает descriptor set layout.
    [[nodiscard]] auto layout() const noexcept -> VkDescriptorSetLayout;

    /// Получает количество текстур.
    [[nodiscard]] auto texture_count() const noexcept -> uint32_t;

private:
    VoxelTextureManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## Push Constants и Shader Interface

```cpp
// ProjectV.Voxel.ShaderInterface.cppm
export module ProjectV.Voxel.ShaderInterface;

import std;
import glm;

export namespace projectv::voxel {

/// Push constants для воксельного шейдера.
/// Размер: 128 байт (максимум для push constants).
export struct alignas(16) VoxelPushConstants {
    glm::mat4 view_proj{1.0f};        // 64 bytes
    glm::vec3 camera_position{0.0f};  // 12 bytes
    float time{0.0f};                 // 4 bytes
    glm::ivec3 chunk_offset{0};       // 12 bytes
    uint32_t lod_level{0};            // 4 bytes
    glm::vec3 sun_direction{0.0f, 1.0f, 0.0f}; // 12 bytes
    float voxel_size{1.0f};           // 4 bytes
    uint32_t texture_array_index{0};  // 4 bytes
    uint32_t material_count{0};       // 4 bytes
    uint32_t _pad[2];                 // 8 bytes
};

static_assert(sizeof(VoxelPushConstants) <= 128, "Push constants exceed 128 bytes");

/// Uniform buffer для кадра.
export struct VoxelFrameUniforms {
    glm::mat4 view_matrix{1.0f};
    glm::mat4 proj_matrix{1.0f};
    glm::mat4 view_proj_matrix{1.0f};
    glm::mat4 inverse_view_matrix{1.0f};
    glm::vec3 camera_position{0.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};
    float time{0.0f};
    float delta_time{0.0f};
    glm::vec3 sun_direction{0.0f, 1.0f, 0.0f};
    float sun_intensity{1.0f};
    glm::vec3 ambient_color{0.2f};
    uint32_t frame_index{0};
};

/// Storage buffer для материалов.
export struct VoxelMaterialBuffer {
    struct MaterialData {
        glm::vec4 base_color{1.0f};
        glm::vec4 emissive{0.0f};
        float roughness{0.5f};
        float metallic{0.0f};
        float normal_scale{1.0f};
        float ao_strength{1.0f};
        uint32_t albedo_index{0};
        uint32_t normal_index{0};
        uint32_t roughness_index{0};
        uint32_t ao_index{0};
        uint32_t _pad[2];
    };

    std::array<MaterialData, 256> materials;
};

} // namespace projectv::voxel
```

---

## Диаграммы

### GPU-Driven Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Voxel Pipeline                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CPU                                GPU                                  │
│  ───                                ───                                  │
│  VoxelData ──────────────────────▶ SSBO (Voxels)                        │
│                                    │                                     │
│  ChunkCoord ─────────────────────▶ Culling Compute                      │
│                                    │                                     │
│                                    ▼                                     │
│                              Mesh Generation                             │
│                              (Compute/Mesh Shader)                       │
│                                    │                                     │
│                                    ▼                                     │
│                              Indirect Buffer                            │
│                              Vertex/Index Buffer                        │
│                                    │                                     │
│                                    ▼                                     │
│                              Bindless Textures                          │
│                                    │                                     │
│                                    ▼                                     │
│                              vkCmdDrawIndexedIndirect                   │
│                                    │                                     │
│                                    ▼                                     │
│                              Graphics Queue                             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### LOD System

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         LOD Levels                                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Distance    LOD Level    Resolution    Vertices per Chunk              │
│  ────────    ─────────    ──────────    ──────────────────              │
│                                                                          │
│  0 - 50m     LOD 0        32³           ~50K vertices                   │
│  50 - 100m   LOD 1        16³           ~12K vertices                   │
│  100 - 200m  LOD 2        8³            ~3K vertices                    │
│  200 - 400m  LOD 3        4³            ~500 vertices                   │
│  > 400m      Culled       -             0                               │
│                                                                          │
│  Memory: LOD 0 = 100% → LOD 1 = 25% → LOD 2 = 6% → LOD 3 = 1%          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Критерии реализации

### Обязательные требования

- [ ] GPU-Driven mesh generation через compute shaders
- [ ] Indirect drawing с минимальным количеством draw calls
- [ ] std::mdspan для доступа к 3D данным чанков
- [ ] std::simd для векторизованных операций
- [ ] DDA raycast для взаимодействия с вокселями

### Метрики производительности

| Операция              | Целевое время     |
|-----------------------|-------------------|
| Mesh Generation (GPU) | < 2ms @ 1M voxels |
| Indirect Draw         | < 0.1ms           |
| DDA Raycast           | < 0.01ms          |
| LOD Update            | < 0.5ms           |

---

## Ссылки

- [ADR-0004: Build System & C++26 Modules](../adr/0004-build-and-modules-spec.md)
- [Engine Structure](./00_engine-structure.md)
- [Resource Management](./02_resource-management.md)
