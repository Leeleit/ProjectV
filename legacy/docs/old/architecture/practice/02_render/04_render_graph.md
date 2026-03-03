# Render Graph Specification

---

## Обзор

Render Graph — автоматическая система управления ресурсами и барьерами поверх Vulkan 1.4 Dynamic Rendering.

### Ключевые преимущества

| Преимущество               | Описание                                            |
|----------------------------|-----------------------------------------------------|
| **Автоматические барьеры** | Не нужно вручную указывать pipeline barriers        |
| **Aliasig ресурсов**       | Переиспользование памяти между passes               |
| **Параллелизация**         | Автоматическое распараллеливание независимых passes |
| **Валидация**              | Compile-time проверка зависимостей                  |

---

## Memory Layout

### RenderGraph

```
RenderGraph (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── passes_: std::vector<PassNode>                     │
│      ├── resources_: std::vector<ResourceNode>              │
│      ├── edges_: std::vector<Edge>                          │
│      ├── resource_registry_: ResourceRegistry               │
│      ├── physical_resources_: PhysicalResourcePool          │
│      ├── frame_index_: uint64_t (8 bytes)                   │
│      └── compiled_: bool (1 byte)                           │
│  Total: 8 bytes (external) + ~2KB (internal)                │
└─────────────────────────────────────────────────────────────┘
```

### PassNode

```
PassNode
┌─────────────────────────────────────────────────────────────┐
│  name: std::string (24-32 bytes)                            │
│  execute: std::function<void(PassContext&)> (32 bytes)      │
│  reads: std::vector<ResourceHandle> (24 bytes)              │
│  writes: std::vector<ResourceHandle> (24 bytes)             │
│  creates: std::vector<ResourceHandle> (24 bytes)            │
│  depth: uint32_t (4 bytes)                                  │
│  flags: PassFlags (4 bytes)                                 │
│  pipeline_flags: VkPipelineStageFlags2 (8 bytes)            │
│  Total: ~176 bytes per pass                                 │
└─────────────────────────────────────────────────────────────┘
```

### ResourceNode

```
ResourceNode
┌─────────────────────────────────────────────────────────────┐
│  name: std::string (24-32 bytes)                            │
│  type: ResourceType (4 bytes)                               │
│  format: VkFormat (4 bytes)                                 │
│  extent: VkExtent3D (12 bytes)                              │
│  usage: VkImageUsageFlags / VkBufferUsageFlags (4/8 bytes)  │
│  first_use: uint32_t (4 bytes) → pass index                 │
│  last_use: uint32_t (4 bytes) → pass index                  │
│  physical_id: uint32_t (4 bytes) → allocated resource       │
│  alias_of: uint32_t (4 bytes) → aliased resource            │
│  Total: ~72 bytes per resource                              │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machines

### Render Graph Lifecycle

```
         ┌──────────────┐
         │    EMPTY     │ ←── Default constructed
         └───────┬──────┘
                 │ add_pass() / add_resource()
                 ▼
         ┌──────────────┐
         │  BUILDING    │ ←── Adding passes and resources
         └───────┬──────┘
                 │ compile()
                 ▼
         ┌──────────────┐
         │   COMPILED   │ ←── Dependencies resolved
         └───────┬──────┘
                 │ execute()
                 ▼
    ┌────────────────────────┐
    │      EXECUTING         │ ←── Running passes
    │  (per-frame state)     │
    └───────────┬────────────┘
                │ frame complete
                ▼
    ┌────────────────────────┐
    │   READY_FOR_FRAME      │ ←── Reset for next frame
    └───────────┬────────────┘
                │ resize / invalidate
                ▼
         ┌──────────────┐
         │   INVALID    │ ←── Needs recompile
         └──────────────┘
```

### Resource State

```
Resource State Transitions

    ┌─────────────────┐
    │    DECLARED     │ ←── Resource added to graph
    └────────┬────────┘
             │ first pass uses it
             ▼
    ┌─────────────────┐
    │    CREATED      │ ←── Physical allocation done
    └────────┬────────┘
             │ pass writes
             ▼
    ┌─────────────────┐
    │    WRITTEN      │ ←── Data in resource valid
    └────────┬────────┘
             │ pass reads
             ▼
    ┌─────────────────┐
    │    READ         │ ←── Resource being used
    └────────┬────────┘
             │ last use
             ▼
    ┌─────────────────┐
    │   TRANSITIONED  │ ←── Layout transition for next use
    └────────┬────────┘
             │ frame end
             ▼
    ┌─────────────────┐
    │    RECYCLED     │ ←── Available for aliasing
    └─────────────────┘
```

### Barrier Generation

```
Automatic Barrier Generation

Pass A writes Resource X
        │
        ▼
    ┌───────────────────────────────────────────┐
    │  Find all passes that read X after A      │
    │  Find all passes that write X after A     │
    └───────────────────────────────────────────┘
        │
        ▼
    ┌───────────────────────────────────────────┐
    │  For each dependent pass B:               │
    │    barrier.srcStage = A.pipelineStage     │
    │    barrier.srcAccess = A.accessMask       │
    │    barrier.dstStage = B.pipelineStage     │
    │    barrier.dstAccess = B.accessMask       │
    │    barrier.layout = X.optimalLayout       │
    └───────────────────────────────────────────┘
        │
        ▼
    Insert barrier before first dependent pass
```

---

## API Contracts

### RenderGraph

```cpp
// ProjectV.Render.Graph.cppm
export module ProjectV.Render.Graph;

import std;
import glm;
import ProjectV.Render.Vulkan.Context;
import ProjectV.Render.Vulkan.DynamicRendering;

export namespace projectv::render {

/// Тип ресурса.
export enum class ResourceType : uint8_t {
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    Buffer
};

/// Описание создания ресурса.
export struct ResourceDesc {
    std::string name;
    ResourceType type{ResourceType::Texture2D};
    VkFormat format{VK_FORMAT_R8G8B8A8_SRGB};
    VkExtent3D extent{1, 1, 1};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
    VkImageUsageFlags image_usage{0};
    VkBufferUsageFlags buffer_usage{0};
    bool transient{false};  ///< Может быть алиасен с другими
};

/// Хендл ресурса.
export struct ResourceHandle {
    uint32_t index{UINT32_MAX};

    [[nodiscard]] auto valid() const noexcept -> bool {
        return index != UINT32_MAX;
    }
};

/// Флаги pass.
export enum class PassFlags : uint32_t {
    None = 0,
    Compute = 1 << 0,       ///< Compute pass (не graphics)
    Transfer = 1 << 1,      ///< Transfer pass
    Present = 1 << 2,       ///< Конечный pass для present
    External = 1 << 3,      ///< Внешний ресурс (swapchain)
};

/// Контекст выполнения pass.
export struct PassContext {
    VkCommandBuffer cmd;
    VulkanContext const& context;
    uint64_t frame_index;

    /// Получает resource как image.
    [[nodiscard]] auto get_image(ResourceHandle handle) const noexcept -> VkImage;

    /// Получает resource как image view.
    [[nodiscard]] auto get_image_view(ResourceHandle handle) const noexcept -> VkImageView;

    /// Получает resource как buffer.
    [[nodiscard]] auto get_buffer(ResourceHandle handle) const noexcept -> VkBuffer;

    /// Получает descriptor для bindless.
    [[nodiscard]] auto get_descriptor_index(ResourceHandle handle) const noexcept -> uint32_t;
};

/// Описание pass.
export struct PassDesc {
    std::string name;
    std::function<void(PassContext const&)> execute;
    std::vector<ResourceHandle> reads;
    std::vector<ResourceHandle> writes;
    std::vector<ResourceHandle> creates;  ///< Создаёт новый ресурс
    PassFlags flags{PassFlags::None};
};

/// Render Graph.
///
/// ## Thread Safety
/// - build() не thread-safe
/// - execute() вызывает passes из одного потока
///
/// ## Invariants
/// - После compile() граф не может быть изменён
/// - Все зависимости должны быть ацикличными (DAG)
/// - Ресурсы создаются перед первым использованием
export class RenderGraph {
public:
    RenderGraph() noexcept;
    ~RenderGraph() noexcept;

    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    /// Объявляет ресурс.
    ///
    /// @param desc Описание ресурса
    /// @return Хендл для использования в passes
    [[nodiscard]] auto declare_resource(ResourceDesc const& desc) noexcept
        -> ResourceHandle;

    /// Объявляет внешний ресурс (swapchain image).
    ///
    /// @param name Имя ресурса
    /// @param image External image
    /// @param view External image view
    /// @return Хендл для использования в passes
    [[nodiscard]] auto declare_external(
        std::string_view name,
        VkImage image,
        VkImageView view
    ) noexcept -> ResourceHandle;

    /// Добавляет pass в граф.
    ///
    /// @param desc Описание pass
    /// @pre Все ресурсы из reads/writes/creates объявлены
    auto add_pass(PassDesc const& desc) noexcept -> void;

    /// Компилирует граф.
    ///
    /// @param context Vulkan context
    /// @return void или ошибка
    ///
    /// @pre Хоть один pass добавлен
    /// @post Граф готов к execute()
    ///
    /// @note Вычисляет барьеры, алиасинг, порядок выполнения
    [[nodiscard]] auto compile(VulkanContext const& context) noexcept
        -> std::expected<void, GraphError>;

    /// Выполняет граф.
    ///
    /// @param cmd Command buffer
    /// @param context Vulkan context
    ///
    /// @pre compile() успешно вызван
    /// @post Все passes выполнены в оптимальном порядке
    auto execute(VkCommandBuffer cmd, VulkanContext const& context) noexcept -> void;

    /// Сбрасывает граф для перестроения.
    auto reset() noexcept -> void;

    /// Устанавливает размер backbuffer.
    auto set_backbuffer_extent(VkExtent2D extent) noexcept -> void;

    /// @return Количество passes
    [[nodiscard]] auto pass_count() const noexcept -> size_t;

    /// @return Количество ресурсов
    [[nodiscard]] auto resource_count() const noexcept -> size_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /// Топологическая сортировка.
    auto topological_sort() noexcept -> void;

    /// Вычисление времени жизни ресурсов.
    auto compute_resource_lifetimes() noexcept -> void;

    /// Генерация барьеров.
    auto generate_barriers() noexcept -> void;

    /// Алиасинг ресурсов.
    auto compute_resource_aliasing() noexcept -> void;
};

/// Коды ошибок Render Graph.
export enum class GraphError : uint8_t {
    CyclicDependency,
    UndefinedResource,
    InvalidPass,
    CompilationFailed,
    ResourceAllocationFailed
};

} // namespace projectv::render
```

---

### Pass Builder (Fluent API)

```cpp
// ProjectV.Render.Graph.PassBuilder.cppm
export module ProjectV.Render.Graph.PassBuilder;

import std;
import ProjectV.Render.Graph;

export namespace projectv::render {

/// Builder для создания passes.
///
/// ## Usage
/// ```cpp
/// graph.add_pass(
///     PassBuilder("gbuffer")
///         .read(depth_buffer)
///         .write(albedo, normal)
///         .execute([&](PassContext const& ctx) {
///             // render commands
///         })
/// );
/// ```
export class PassBuilder {
public:
    explicit PassBuilder(std::string_view name) noexcept;

    /// Добавляет читаемый ресурс.
    auto read(ResourceHandle resource) noexcept -> PassBuilder&;

    /// Добавляет записываемый ресурс.
    auto write(ResourceHandle resource) noexcept -> PassBuilder&;

    /// Добавляет создаваемый ресурс.
    auto create(ResourceHandle resource) noexcept -> PassBuilder&;

    /// Устанавливает функцию выполнения.
    auto execute(std::function<void(PassContext const&)> func) noexcept -> PassBuilder&;

    /// Устанавливает флаги.
    auto set_flags(PassFlags flags) noexcept -> PassBuilder&;

    /// Помечает как compute pass.
    auto compute() noexcept -> PassBuilder&;

    /// Помечает как present pass.
    auto present(ResourceHandle output) noexcept -> PassBuilder&;

    /// Строит PassDesc.
    [[nodiscard]] auto build() const noexcept -> PassDesc;

private:
    PassDesc desc_;
};

} // namespace projectv::render
```

---

### Resource Pool

```cpp
// ProjectV.Render.Graph.ResourcePool.cppm
export module ProjectV.Render.Graph.ResourcePool;

import std;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render {

/// Алиасируемый ресурс.
struct AliasedResource {
    VkImage image{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    uint32_t first_use{0};
    uint32_t last_use{0};
    bool in_use{false};
};

/// Пул физических ресурсов с алиасингом.
///
/// ## Memory Efficiency
/// Ресурсы с непересекающимися lifetimes разделяют одну память.
///
/// ## Invariants
/// - Один physical resource используется только одним logical resource за раз
/// - Memory aliasing происходит через vkBindImageMemory2
export class ResourcePool {
public:
    /// Создаёт pool.
    [[nodiscard]] static auto create(
        VulkanContext const& context,
        size_t initial_capacity = 64
    ) noexcept -> std::expected<ResourcePool, GraphError>;

    ~ResourcePool() noexcept;

    ResourcePool(ResourcePool&&) noexcept;
    ResourcePool& operator=(ResourcePool&&) noexcept;
    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;

    /// Аллоцирует ресурс.
    ///
    /// @param desc Описание ресурса
    /// @param first_use Индекс первого использующего pass
    /// @param last_use Индекс последнего использующего pass
    /// @return ID ресурса или ошибка
    [[nodiscard]] auto allocate(
        ResourceDesc const& desc,
        uint32_t first_use,
        uint32_t last_use
    ) noexcept -> std::expected<uint32_t, GraphError>;

    /// Получает image.
    [[nodiscard]] auto get_image(uint32_t id) const noexcept -> VkImage;

    /// Получает image view.
    [[nodiscard]] auto get_image_view(uint32_t id) const noexcept -> VkImageView;

    /// Освобождает ресурсы, не используемые в текущем кадре.
    auto gc(uint32_t current_pass) noexcept -> void;

    /// Сбрасывает pool.
    auto reset() noexcept -> void;

    /// @return Использованная VRAM
    [[nodiscard]] auto memory_used() const noexcept -> VkDeviceSize;

private:
    ResourcePool() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render
```

---

## Barrier Algorithm

### Pseudocode

```
function GENERATE_BARRIERS(graph):
    barriers = []

    for each resource R in graph.resources:
        uses = FIND_ALL_USES(R)

        for i = 0 to len(uses) - 1:
            use_a = uses[i]
            use_b = uses[i + 1]

            if use_a.access != use_b.access OR use_a.layout != use_b.layout:
                barrier = VkImageMemoryBarrier2{
                    srcStage: use_a.pipeline_stage,
                    srcAccess: use_a.access_mask,
                    dstStage: use_b.pipeline_stage,
                    dstAccess: use_b.access_mask,
                    oldLayout: use_a.layout,
                    newLayout: use_b.layout,
                    image: R.image
                }
                barriers.append((use_b.pass_index, barrier))

    return barriers
```

### Complexity

| Операция           | Сложность     |
|--------------------|---------------|
| Topological Sort   | O(V + E)      |
| Lifetime Analysis  | O(V × R)      |
| Barrier Generation | O(E × R)      |
| Aliasing           | O(R² × log R) |

Где:

- V = количество passes
- E = количество edges (зависимостей)
- R = количество ресурсов

---

## Example: Deferred Rendering Graph

```cpp
// Создание render graph
RenderGraph graph;

// Объявление ресурсов
auto depth = graph.declare_resource({
    .name = "depth_buffer",
    .type = ResourceType::Texture2D,
    .format = VK_FORMAT_D32_SFLOAT,
    .extent = {1920, 1080, 1},
    .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT,
    .transient = true
});

auto albedo = graph.declare_resource({
    .name = "gbuffer_albedo",
    .type = ResourceType::Texture2D,
    .format = VK_FORMAT_R8G8B8A8_SRGB,
    .extent = {1920, 1080, 1},
    .image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT,
    .transient = true
});

auto lighting = graph.declare_resource({
    .name = "lighting_output",
    .type = ResourceType::Texture2D,
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
    .extent = {1920, 1080, 1},
    .image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT
});

auto output = graph.declare_external("swapchain", swapchain_image, swapchain_view);

// G-Buffer Pass
graph.add_pass(
    PassBuilder("gbuffer")
        .write(depth)
        .write(albedo)
        .execute([&](PassContext const& ctx) {
            // G-buffer rendering commands
            RenderingInfo info;
            info.set_render_area(extent)
                .add_color_attachment({ctx.get_image_view(albedo)})
                .set_depth_attachment({ctx.get_image_view(depth)});

            ScopedRendering scope(ctx.cmd, info);
            // Draw geometry...
        })
        .build()
);

// Lighting Pass
graph.add_pass(
    PassBuilder("lighting")
        .read(depth)
        .read(albedo)
        .write(lighting)
        .execute([&](PassContext const& ctx) {
            // Lighting calculation
        })
        .build()
);

// Present Pass
graph.add_pass(
    PassBuilder("present")
        .read(lighting)
        .write(output)
        .present(output)
        .execute([&](PassContext const& ctx) {
            // Copy to swapchain
        })
        .build()
);

// Компиляция
auto result = graph.compile(context);
if (!result) {
    // Handle error
}

// Выполнение (каждый кадр)
graph.execute(cmd, context);
```

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Render Graph                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                         Graph Building                             │  │
│  │  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐        │  │
│  │  │ Declare │───▶│  Add    │───▶│Compile  │───▶│Execute  │        │  │
│  │  │Resource │    │ Passes  │    │ Graph   │    │ Frame   │        │  │
│  │  └─────────┘    └─────────┘    └─────────┘    └─────────┘        │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Compilation Phase                           │  │
│  │                                                                    │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │  │
│  │  │ Topological  │  │  Lifetime    │  │  Barrier     │            │  │
│  │  │    Sort      │─▶│  Analysis    │─▶│ Generation   │            │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘            │  │
│  │          │                                    │                   │  │
│  │          ▼                                    ▼                   │  │
│  │  ┌──────────────┐                   ┌──────────────┐            │  │
│  │  │   Resource   │                   │   Barrier    │            │  │
│  │  │   Aliasing   │                   │   Queue      │            │  │
│  │  └──────────────┘                   └──────────────┘            │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Execution Phase                             │  │
│  │                                                                    │  │
│  │  ┌─────────────────────────────────────────────────────────────┐  │  │
│  │  │  Pass 0 (G-Buffer)                                          │  │  │
│  │  │  [Write depth, albedo]                                      │  │  │
│  │  └───────────────────────────┬─────────────────────────────────┘  │  │
│  │                              │ Barrier: COLOR → SHADER_READ       │  │
│  │                              ▼                                    │  │
│  │  ┌─────────────────────────────────────────────────────────────┐  │  │
│  │  │  Pass 1 (Lighting)                                          │  │  │
│  │  │  [Read depth, albedo] → [Write lighting]                    │  │  │
│  │  └───────────────────────────┬─────────────────────────────────┘  │  │
│  │                              │ Barrier: COLOR → TRANSFER_SRC      │  │
│  │                              ▼                                    │  │
│  │  ┌─────────────────────────────────────────────────────────────┐  │  │
│  │  │  Pass 2 (Present)                                           │  │  │
│  │  │  [Read lighting] → [Write swapchain]                        │  │  │
│  │  └─────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```
