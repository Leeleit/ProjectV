## Интеграция fastgltf

<!-- anchor: 02_integration -->

> **Для понимания:** Интеграция fastgltf — это не просто подключение библиотеки. Это создание моста между иерархическими
> данными glTF и плоскими массивами Data-Oriented Design. Представьте, что fastgltf — это таможня, которая проверяет и
> декларирует груз (glTF файл), а ваша система — это склад, где всё должно быть разложено по полкам (SoA массивы) для
> быстрого доступа.

Подключение fastgltf в современный C++ проект с использованием C++26, Data-Oriented Design и интеграцией с графическими
API.

## CMake

### Базовая интеграция

```cmake
add_subdirectory(external/fastgltf)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE fastgltf::fastgltf)
```

simdjson подгружается автоматически через FetchContent или `find_package`.

### Проект уже использует simdjson

```cmake
# Добавьте simdjson ДО fastgltf
find_package(simdjson REQUIRED)  # или add_subdirectory

# Fastgltf обнаружит системный simdjson
add_subdirectory(external/fastgltf)
```

### Конфликт версий simdjson

```cmake
include(FetchContent)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG v3.6.2  # Конкретная версия
  OVERRIDE_FIND_PACKAGE
)

add_subdirectory(external/fastgltf)
```

## CMake-опции fastgltf

| Опция                                      | По умолчанию | Описание                                      |
|--------------------------------------------|--------------|-----------------------------------------------|
| `FASTGLTF_USE_64BIT_FLOAT`                 | OFF          | Использовать `double` вместо `float`          |
| `FASTGLTF_ENABLE_DEPRECATED_EXT`           | OFF          | Включить устаревшие расширения                |
| `FASTGLTF_USE_CUSTOM_SMALLVECTOR`          | OFF          | Использовать SmallVector в большем числе мест |
| `FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL`      | OFF          | Отключить pmr-аллокатор                       |
| `FASTGLTF_COMPILE_AS_CPP20`                | OFF          | Компилировать как C++20                       |
| `FASTGLTF_ENABLE_CPP_MODULES`              | OFF          | Включить поддержку C++20 modules              |
| `FASTGLTF_USE_STD_MODULE`                  | OFF          | Использовать std module (C++23)               |
| `FASTGLTF_ENABLE_TESTS`                    | OFF          | Сборка тестов                                 |
| `FASTGLTF_ENABLE_EXAMPLES`                 | OFF          | Сборка примеров                               |
| `FASTGLTF_ENABLE_DOCS`                     | OFF          | Сборка документации                           |
| `FASTGLTF_ENABLE_IMPLICIT_SHAPES`          | OFF          | Draft extension KHR_implicit_shapes           |
| `FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES` | OFF          | Draft extension KHR_physics_rigid_bodies      |

Пример:

```cmake
set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL "" FORCE)
add_subdirectory(external/fastgltf)
```

## Заголовки

| Заголовок                            | Содержание                                   |
|--------------------------------------|----------------------------------------------|
| `fastgltf/core.hpp`                  | Parser, Exporter, Error — основной заголовок |
| `fastgltf/types.hpp`                 | POD типы и перечисления                      |
| `fastgltf/tools.hpp`                 | Accessor tools, node transform utilities     |
| `fastgltf/math.hpp`                  | Встроенная математика (векторы, матрицы)     |
| `fastgltf/base64.hpp`                | SIMD-оптимизированный base64 декодер         |
| `fastgltf/glm_element_traits.hpp`    | Интеграция с glm для accessor tools          |
| `fastgltf/dxmath_element_traits.hpp` | Интеграция с DirectXMath                     |

`core.hpp` уже включает `types.hpp`.

## Options

Флаги для `loadGltf` (побитовое ИЛИ):

| Опция                         | Описание                                                |
|-------------------------------|---------------------------------------------------------|
| `None`                        | Только парсинг JSON. GLB-буферы загружаются в память.   |
| `LoadExternalBuffers`         | Загрузить внешние .bin в `sources::Vector`              |
| `LoadExternalImages`          | Загрузить внешние изображения                           |
| `DecomposeNodeMatrices`       | Разложить матрицы узлов на TRS                          |
| `GenerateMeshIndices`         | Сгенерировать индексы для примитивов без index accessor |
| `AllowDouble`                 | Разрешить component type `GL_DOUBLE` (5130)             |
| `DontRequireValidAssetMember` | Не проверять поле `asset` в JSON                        |

```cpp
auto options = fastgltf::Options::LoadExternalBuffers
             | fastgltf::Options::LoadExternalImages
             | fastgltf::Options::DecomposeNodeMatrices;
```

Примечание: `LoadGLBBuffers` deprecated — GLB-буферы загружаются по умолчанию.

## Category

Маска того, что парсить:

| Значение         | Описание                                           |
|------------------|----------------------------------------------------|
| `All`            | Всё (по умолчанию)                                 |
| `OnlyRenderable` | Всё, кроме Animations и Skins                      |
| `OnlyAnimations` | Только Animations, Accessors, BufferViews, Buffers |

```cpp
// Для статических моделей — быстрее на 30-40%
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);

// Для извлечения только анимаций
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyAnimations);
```

## Extensions

Битовая маска расширений передаётся в конструктор Parser:

```cpp
fastgltf::Extensions extensions =
    fastgltf::Extensions::KHR_texture_basisu
    | fastgltf::Extensions::KHR_materials_unlit
    | fastgltf::Extensions::KHR_mesh_quantization
    | fastgltf::Extensions::EXT_mesh_gpu_instancing;

fastgltf::Parser parser(extensions);
```

### Проверка расширений модели

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);

// Расширения, которые модель использует
for (const auto& ext : asset->extensionsUsed) {
    std::println("Uses: {}", ext);
}

// Обязательные расширения (без которых модель не работает)
for (const auto& ext : asset->extensionsRequired) {
    std::println("Requires: {}", ext);
}
```

### Утилиты для отладки

```cpp
// Имя первого установленного бита
std::string_view name = fastgltf::stringifyExtension(extensions);

// Список всех имён установленных битов
std::vector<std::string> names = fastgltf::stringifyExtensionBits(extensions);
```

## Связь с glm

Для использования glm с accessor tools подключите traits:

```cpp
#include <glm/glm.hpp>
#include <fastgltf/glm_element_traits.hpp>

// Теперь iterateAccessor<glm::vec3> работает
std::vector<glm::vec3> positions;
fastgltf::iterateAccessor<glm::vec3>(asset, accessor,
    [&](glm::vec3 pos) { positions.push_back(pos); });
```

## Android

Загрузка glTF из APK assets:

```cpp
#include <android/asset_manager_jni.h>

// Инициализация (вызывается один раз)
AAssetManager* manager = AAssetManager_fromJava(env, assetManager);
fastgltf::setAndroidAssetManager(manager);

// Загрузка из assets
auto data = fastgltf::AndroidGltfDataBuffer::FromAsset("models/scene.gltf");
if (data.error() != fastgltf::Error::None) {
    return false;
}

// LoadExternalBuffers и LoadExternalImages работают с APK
auto asset = parser.loadGltf(data.get(), "",
                              fastgltf::Options::LoadExternalBuffers);
```

## Callbacks

### BufferAllocationCallback

Для прямой записи в GPU-память:

```cpp
struct Context {
    // Ваш GPU context
};

Context ctx;
parser.setUserPointer(&ctx);

parser.setBufferAllocationCallback(
    [](void* userPointer, size_t bufferSize, fastgltf::BufferAllocateFlags flags)
    -> fastgltf::BufferInfo {

        auto* ctx = static_cast<Context*>(userPointer);

        // Выделение GPU буфера
        void* mappedPtr = allocateGPUBuffer(ctx, bufferSize);

        return fastgltf::BufferInfo{
            .mappedMemory = mappedPtr,
            .customId = /* ваш ID буфера */
        };
    },
    nullptr,  // unmap callback (опционально)
    nullptr   // deallocate callback (опционально)
);
```

### ExtrasParseCallback

Для чтения extras из JSON:

```cpp
auto extrasCallback = [](simdjson::dom::object* extras,
                         std::size_t objectIndex,
                         fastgltf::Category category,
                         void* userPointer) {
    if (category != fastgltf::Category::Nodes)
        return;

    std::string_view customName;
    if ((*extras)["customName"].get_string().get(customName) == simdjson::SUCCESS) {
        // Сохраните customName
    }
};

parser.setExtrasParseCallback(extrasCallback);
parser.setUserPointer(&yourDataStorage);
```

### Base64DecodeCallback

Для кастомного base64-декодера:

```cpp
parser.setBase64DecodeCallback([](std::string_view encoded, void* userPointer)
    -> std::vector<std::byte> {
    // Ваш декодер
});
```

## C++20 Modules

При включении `FASTGLTF_ENABLE_CPP_MODULES`:

```cmake
set(FASTGLTF_ENABLE_CPP_MODULES ON CACHE BOOL "" FORCE)
add_subdirectory(external/fastgltf)

# Использование
target_link_libraries(YourApp PRIVATE fastgltf::module)
```

```cpp
// В коде
import fastgltf;
```

Примечание: поддержка модулей зависит от компилятора и генератора CMake.

---

## Data-Oriented Design: Преобразование иерархии в SoA

> **Для понимания:** Иерархические данные glTF — это как склад с коробками внутри коробок. DOD требует разложить всё на
> плоские полки: одна полка для позиций, другая для нормалей, третья для индексов. Fastgltf даёт вам доступ к содержимому
> коробок, а ваша задача — эффективно разложить их по полкам.

### Базовый DOD подход

```cpp
#include <print>
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

struct MeshSoA {
    // Structure of Arrays (SoA) для вершин
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;

    // Метаданные
    std::vector<uint32_t> materialIds;
    std::vector<glm::mat4> transforms;
};

std::expected<MeshSoA, fastgltf::Error> convertToSoA(
    const fastgltf::Asset& asset,
    std::size_t meshIndex = 0) {

    if (meshIndex >= asset.meshes.size()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset.meshes[meshIndex];
    MeshSoA result;

    for (const auto& primitive : mesh.primitives) {
        // Извлечение позиций
        if (auto posAttr = primitive.findAttribute("POSITION");
            posAttr != primitive.attributes.cend()) {

            const auto& accessor = asset.accessors[posAttr->accessorIndex];
            std::vector<glm::vec3> primitivePositions(accessor.count);

            if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {
                return std::unexpected(err);
            }

            // Добавляем в SoA массив
            result.positions.insert(result.positions.end(),
                primitivePositions.begin(), primitivePositions.end());
        }

        // Аналогично для нормалей, текстурных координат и т.д.
    }

    return result;
}
```

### Пакетная конвертация для производительности

```cpp
struct BatchSoAConverter {
    // Пакетная обработка нескольких мешей
    std::expected<std::vector<MeshSoA>, fastgltf::Error> convertBatch(
        const fastgltf::Asset& asset,
        std::span<const std::size_t> meshIndices) {

        std::vector<MeshSoA> results;
        results.reserve(meshIndices.size());

        for (auto meshIdx : meshIndices) {
            if (auto meshSoA = convertToSoA(asset, meshIdx)) {
                results.push_back(std::move(*meshSoA));
            } else {
                return std::unexpected(meshSoA.error());
            }
        }

        return results;
    }

    // Многопоточная конвертация
    std::expected<std::vector<MeshSoA>, fastgltf::Error> convertParallel(
        const fastgltf::Asset& asset,
        std::size_t threadCount = std::thread::hardware_concurrency()) {

        std::vector<std::future<std::expected<MeshSoA, fastgltf::Error>>> futures;
        std::vector<MeshSoA> results(asset.meshes.size());

        for (std::size_t i = 0; i < asset.meshes.size(); ++i) {
            futures.push_back(std::async(std::launch::async,
                [&asset, i]() { return convertToSoA(asset, i); }));
        }

        for (std::size_t i = 0; i < futures.size(); ++i) {
            if (auto result = futures[i].get()) {
                results[i] = std::move(*result);
            } else {
                return std::unexpected(result.error());
            }
        }

        return results;
    }
};
```

---

## Интеграция с Vulkan и VMA

> **Для понимания:** VMA (Vulkan Memory Allocator) — это умный складской менеджер для GPU памяти. Fastgltf может писать
> данные напрямую в выделенные VMA регионы, минуя промежуточные копии. Это как если бы грузовик с товарами (glTF данные)
> разгружался прямо на полки склада (GPU буферы), без промежуточного хранения.

### Стратегии загрузки данных в GPU

| Стратегия                        | Использование               | Преимущества              | Недостатки                 |
|----------------------------------|-----------------------------|---------------------------|----------------------------|
| **Zero-copy через CustomBuffer** | Статические меши, ландшафты | Нулевые накладные расходы | Требует управления памятью |
| **Staging буферы**               | Динамический контент        | Простота, совместимость   | Двойное копирование        |
| **Прямое маппирование**          | Анимированные меши          | Минимальная задержка      | Сложность синхронизации    |

### Загрузка меша через VMA с C++26

```cpp
#include <print>
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/glm.hpp>

struct VulkanMeshData {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t vertexCount;
    uint32_t indexCount;
};

std::expected<VulkanMeshData, fastgltf::Error> loadGltfMeshToVulkan(
    const std::filesystem::path& path,
    VmaAllocator allocator) {

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    auto options = fastgltf::Options::LoadExternalBuffers
                 | fastgltf::Options::DecomposeNodeMatrices;
    auto asset = parser.loadGltf(data.get(), path.parent_path(), options,
                                  fastgltf::Category::OnlyRenderable);
    if (asset.error() != fastgltf::Error::None) {
        return std::unexpected(asset.error());
    }

    if (asset->meshes.empty()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset->meshes[0];
    VulkanMeshData result{};
    uint32_t totalVertexCount = 0;
    uint32_t totalIndexCount = 0;

    // Подсчёт общего количества вершин и индексов
    for (const auto& primitive : mesh.primitives) {
        if (auto posAttr = primitive.findAttribute("POSITION");
            posAttr != primitive.attributes.cend()) {
            const auto& accessor = asset->accessors[posAttr->accessorIndex];
            totalVertexCount += static_cast<uint32_t>(accessor.count);
        }
        if (primitive.indicesAccessor.has_value()) {
            const auto& accessor = asset->accessors[*primitive.indicesAccessor];
            totalIndexCount += static_cast<uint32_t>(accessor.count);
        }
    }

    // Создание VMA буферов
    VkBufferCreateInfo vertexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = totalVertexCount * sizeof(glm::vec3),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo vertexAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    if (vmaCreateBuffer(allocator, &vertexBufferInfo, &vertexAllocInfo,
                        &result.vertexBuffer, &result.vertexAllocation, nullptr) != VK_SUCCESS) {
        return std::unexpected(fastgltf::Error::FileBufferAllocationFailed);
    }

    // Аналогично для index buffer
    VkBufferCreateInfo indexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = totalIndexCount * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo indexAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    if (vmaCreateBuffer(allocator, &indexBufferInfo, &indexAllocInfo,
                        &result.indexBuffer, &result.indexAllocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, result.vertexBuffer, result.vertexAllocation);
        return std::unexpected(fastgltf::Error::FileBufferAllocationFailed);
    }

    result.vertexCount = totalVertexCount;
    result.indexCount = totalIndexCount;

    return result;
}

### Zero-copy через BufferAllocationCallback

> **Для понимания:** BufferAllocationCallback — это VIP-пропуск для данных. Вместо того чтобы стоять в очереди на таможне (загрузка в RAM), данные сразу попадают в специальный коридор (GPU память). Это как дипломатическая почта — минует все стандартные процедуры.

```cpp
struct GpuContext {
    VmaAllocator allocator;
    std::vector<std::pair<VkBuffer, VmaAllocation>> allocatedBuffers;
};

std::expected<fastgltf::Asset, fastgltf::Error> loadGltfWithZeroCopy(
    const std::filesystem::path& path,
    GpuContext& ctx) {

    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize,
           fastgltf::BufferAllocateFlags flags) -> fastgltf::BufferInfo {

            auto* ctx = static_cast<GpuContext*>(userPointer);

            VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VmaAllocationCreateInfo allocInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            };

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocInfoResult;

            if (vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo,
                                &buffer, &allocation, &allocInfoResult) != VK_SUCCESS) {
                return fastgltf::BufferInfo{};
            }

            ctx->allocatedBuffers.emplace_back(buffer, allocation);

            return fastgltf::BufferInfo{
                .mappedMemory = allocInfoResult.pMappedData,
                .customId = reinterpret_cast<fastgltf::CustomBufferId>(buffer)
            };
        },
        nullptr,  // unmap
        nullptr   // deallocate
    );

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    return parser.loadGltf(data.get(), path.parent_path(),
                           fastgltf::Options::LoadExternalBuffers);
}
```

---

## Интеграция с ECS (Flecs)

> **Для понимания:** Flecs — это система управления сущностями, где каждая сущность — это сотрудник на складе, а
> компоненты — это его навыки (может грузить позиции, нормали, индексы). Fastgltf поставляет товары (данные), а Flecs
> распределяет их по сотрудникам для эффективной обработки.

### SoA-компоненты для мешей

```cpp
#include <flecs.h>
#include <print>
#include <span>
#include <fastgltf/core.hpp>

// SoA компоненты для Data-Oriented Design
struct PositionComponent {
    std::vector<glm::vec3> positions;
};

struct NormalComponent {
    std::vector<glm::vec3> normals;
};

struct TexcoordComponent {
    std::vector<glm::vec2> texcoords;
};

struct IndexComponent {
    std::vector<uint32_t> indices;
};

struct MeshBoundsComponent {
    glm::vec3 min;
    glm::vec3 max;
    float radius;
};

// Система загрузки glTF в ECS
class GltfLoaderSystem {
public:
    static std::expected<flecs::entity, fastgltf::Error> loadMeshToEcs(
        flecs::world& world,
        const std::filesystem::path& path,
        flecs::entity parent = flecs::entity()) {

        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            return std::unexpected(data.error());
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                     fastgltf::Options::LoadExternalBuffers,
                                     fastgltf::Category::OnlyRenderable);
        if (asset.error() != fastgltf::Error::None) {
            return std::unexpected(asset.error());
        }

        if (asset->meshes.empty()) {
            return std::unexpected(fastgltf::Error::InvalidGltf);
        }

        flecs::entity meshEntity;
        if (parent) {
            meshEntity = parent.child("mesh");
        } else {
            meshEntity = world.entity("loaded_mesh");
        }

        // Создание SoA компонентов
        auto& posComp = meshEntity.set<PositionComponent>({});
        auto& normComp = meshEntity.set<NormalComponent>({});
        auto& texComp = meshEntity.set<TexcoordComponent>({});
        auto& idxComp = meshEntity.set<IndexComponent>({});

        const auto& mesh = asset->meshes[0];
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (const auto& primitive : mesh.primitives) {
            // Загрузка позиций
            if (auto posAttr = primitive.findAttribute("POSITION");
                posAttr != primitive.attributes.cend()) {

                const auto& accessor = asset->accessors[posAttr->accessorIndex];
                std::vector<glm::vec3> primitivePositions(accessor.count);

                if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                    *asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {
                    return std::unexpected(err);
                }

                // Обновление границ
                for (const auto& pos : primitivePositions) {
                    minBounds = glm::min(minBounds, pos);
                    maxBounds = glm::max(maxBounds, pos);
                }

                posComp.positions.insert(posComp.positions.end(),
                    primitivePositions.begin(), primitivePositions.end());
            }

            // Аналогично для нормалей и текстурных координат
            // ...
        }

        // Установка компонента границ
        meshEntity.set<MeshBoundsComponent>({
            .min = minBounds,
            .max = maxBounds,
            .radius = glm::length(maxBounds - minBounds) * 0.5f
        });

        return meshEntity;
    }

    // Пакетная загрузка
    static std::expected<std::vector<flecs::entity>, fastgltf::Error> loadBatchToEcs(
        flecs::world& world,
        std::span<const std::filesystem::path> paths) {

        std::vector<flecs::entity> entities;
        entities.reserve(paths.size());

        for (const auto& path : paths) {
            if (auto entity = loadMeshToEcs(world, path)) {
                entities.push_back(*entity);
            } else {
                return std::unexpected(entity.error());
            }
        }

        return entities;
    }
};
```

### Система рендеринга с SoA

```cpp
// Система рендеринга, оптимизированная для SoA
class MeshRenderSystem {
public:
    MeshRenderSystem(flecs::world& world) {
        world.system<const PositionComponent, const NormalComponent,
                     const TexcoordComponent, const IndexComponent,
                     const MeshBoundsComponent>()
            .kind(flecs::OnUpdate)
            .each([](flecs::entity e,
                     const PositionComponent& pos,
                     const NormalComponent& norm,
                     const TexcoordComponent& tex,
                     const IndexComponent& idx,
                     const MeshBoundsComponent& bounds) {

                // Batch rendering с SoA данными
                // Все позиции в одном непрерывном массиве
                // Все нормали в другом и т.д.

                if (pos.positions.empty() || idx.indices.empty()) {
                    return;
                }

                // GPU-driven рендеринг
                // 1. Передача SoA данных в GPU буферы
                // 2. Indirect drawing с compute shaders
                // 3. Frustum culling на основе bounds

                renderMeshSoA(pos.positions, norm.normals, tex.texcoords,
                              idx.indices, bounds);
            });
    }

private:
    void renderMeshSoA(const std::vector<glm::vec3>& positions,
                       const std::vector<glm::vec3>& normals,
                       const std::vector<glm::vec2>& texcoords,
                       const std::vector<uint32_t>& indices,
                       const MeshBoundsComponent& bounds) {
        // Реализация рендеринга
    }
};
```

---

## Паттерны интеграции

### Паттерн 1: Lazy Loading

```cpp
class LazyGltfLoader {
    struct MeshData {
        std::filesystem::path path;
        std::optional<fastgltf::Asset> asset;
        std::future<std::expected<fastgltf::Asset, fastgltf::Error>> loadingFuture;
        bool isLoaded = false;
    };

    std::unordered_map<std::string, MeshData> meshCache;
    std::mutex cacheMutex;

public:
    std::expected<fastgltf::Asset, fastgltf::Error> loadOrGet(
        const std::string& key,
        const std::filesystem::path& path) {

        std::lock_guard lock(cacheMutex);

        if (auto it = meshCache.find(key); it != meshCache.end()) {
            if (it->second.isLoaded && it->second.asset.has_value()) {
                return *it->second.asset;
            }

            if (it->second.loadingFuture.valid()) {
                return it->second.loadingFuture.get();
            }
        }

        // Асинхронная загрузка
        auto future = std::async(std::launch::async, [path]() {
            fastgltf::Parser parser;
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            if (data.error() != fastgltf::Error::None) {
                return std::unexpected(data.error());
            }
            return parser.loadGltf(data.get(), path.parent_path(),
                                   fastgltf::Options::LoadExternalBuffers);
        });

        meshCache[key] = MeshData{
            .path = path,
            .asset = std::nullopt,
            .loadingFuture = std::move(future),
            .isLoaded = false
        };

        return future.get();
    }
};
```

### Паттерн 2: Mesh Batcher

```cpp
class MeshBatcher {
    struct Batch {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> materialIds;
        std::vector<uint32_t> drawOffsets;  // Смещения для каждого меша в батче
    };

    std::vector<Batch> batches;
    size_t maxVerticesPerBatch = 65536;  // Ограничение по размеру буфера

public:
    void addMesh(const fastgltf::Asset& asset, size_t meshIndex) {
        if (meshIndex >= asset.meshes.size()) return;

        const auto& mesh = asset.meshes[meshIndex];
        Batch* currentBatch = nullptr;

        // Поиск батча с достаточным местом
        for (auto& batch : batches) {
            if (batch.positions.size() + estimateVertexCount(mesh) <= maxVerticesPerBatch) {
                currentBatch = &batch;
                break;
            }
        }

        // Создание нового батча при необходимости
        if (!currentBatch) {
            batches.emplace_back();
            currentBatch = &batches.back();
        }

        uint32_t baseVertex = static_cast<uint32_t>(currentBatch->positions.size());
        currentBatch->drawOffsets.push_back(baseVertex);

        // Добавление данных меша в батч
        for (const auto& primitive : mesh.primitives) {
            // Извлечение и добавление данных...
        }
    }

    const std::vector<Batch>& getBatches() const { return batches; }
};
```

### Паттерн 3: Streaming Loader

```cpp
class StreamingGltfLoader {
    struct StreamingContext {
        std::atomic<bool> cancelFlag{false};
        std::queue<std::filesystem::path> loadQueue;
        std::mutex queueMutex;
        std::condition_variable queueCV;
        std::vector<std::thread> workerThreads;

        // Callback для уведомления о загрузке
        std::function<void(std::filesystem::path, std::expected<fastgltf::Asset, fastgltf::Error>)> callback;
    };

    std::unique_ptr<StreamingContext> ctx;

public:
    StreamingGltfLoader(size_t threadCount = std::thread::hardware_concurrency()) {
        ctx = std::make_unique<StreamingContext>();

        for (size_t i = 0; i < threadCount; ++i) {
            ctx->workerThreads.emplace_back([this, i]() { workerThread(i); });
        }
    }

    ~StreamingGltfLoader() {
        ctx->cancelFlag = true;
        ctx->queueCV.notify_all();

        for (auto& thread : ctx->workerThreads) {
            if (thread.joinable()) thread.join();
        }
    }

    void enqueueLoad(const std::filesystem::path& path) {
        std::lock_guard lock(ctx->queueMutex);
        ctx->loadQueue.push(path);
        ctx->queueCV.notify_one();
    }

    void setCallback(auto&& callback) {
        ctx->callback = std::forward<decltype(callback)>(callback);
    }

private:
    void workerThread(size_t threadId) {
        fastgltf::Parser parser;  // Каждый поток имеет свой парсер

        while (!ctx->cancelFlag) {
            std::filesystem::path path;

            {
                std::unique_lock lock(ctx->queueMutex);
                ctx->queueCV.wait(lock, [this]() {
                    return ctx->cancelFlag || !ctx->loadQueue.empty();
                });

                if (ctx->cancelFlag) break;
                if (ctx->loadQueue.empty()) continue;

                path = ctx->loadQueue.front();
                ctx->loadQueue.pop();
            }

            // Загрузка
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            if (data.error() != fastgltf::Error::None) {
                if (ctx->callback) {
                    ctx->callback(path, std::unexpected(data.error()));
                }
                continue;
            }

            auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                         fastgltf::Options::LoadExternalBuffers);

            if (ctx->callback) {
                ctx->callback(path, std::move(asset));
            }
        }
    }
};
```

---

## Лучшие практики

### 1. Переиспользование Parser

```cpp
// ХОРОШО: Один парсер на поток
thread_local fastgltf::Parser threadParser;

// ПЛОХО: Создание парсера для каждого файла
for (auto& file : files) {
    fastgltf::Parser parser;  // Дорогая операция!
    // ...
}
```

### 2. Оптимальные Options

```cpp
// Для статических моделей
auto staticOptions = fastgltf::Options::LoadExternalBuffers
                   | fastgltf::Options::DecomposeNodeMatrices;

// Для анимированных моделей
auto animatedOptions = fastgltf::Options::LoadExternalBuffers;

// Для редактора (нужно всё)
auto editorOptions = fastgltf::Options::LoadExternalBuffers
                   | fastgltf::Options::LoadExternalImages
                   | fastgltf::Options::DecomposeNodeMatrices;
```

### 3. Memory Mapping для больших файлов

```cpp
// Для файлов > 100MB используйте memory mapping
auto data = fastgltf::MappedGltfFile::FromPath("large_model.glb");
// Меньше потребления RAM, быстрее загрузка
```

### 4. Категории для оптимизации

```cpp
// Только рендеринг (быстрее на 30-40%)
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);

// Только анимации
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyAnimations);
```

### 5. Обработка ошибок с C++26

```cpp
auto result = loadGltfMeshToVulkan(path, allocator);
if (!result) {
    std::println(stderr, "Ошибка загрузки: {}",
                 fastgltf::getErrorMessage(result.error()));
    return;
}

// Использование std::expected с pattern matching (C++26)
auto handleResult = [](auto&& result) {
    using T = std::decay_t<decltype(result)>;

    if constexpr (std::is_same_v<T, std::expected<VulkanMeshData, fastgltf::Error>>) {
        if (!result) {
            std::println("Ошибка: {}", fastgltf::getErrorMessage(result.error()));
            return;
        }
        // Работа с данными
        auto& meshData = *result;
        // ...
    }
};
```

### 6. Профилирование и оптимизация

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

// Загрузка
auto asset = parser.loadGltf(data.get(), basePath, options);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::println("Загрузка заняла: {}ms", duration.count());
std::println("Мешей: {}", asset->meshes.size());
std::println("Вершин: {}", calculateTotalVertices(*asset));
std::println("Треугольников: {}", calculateTotalTriangles(*asset));
```

### 7. Кэширование загруженных моделей

```cpp
class MeshCache {
    struct CachedMesh {
        std::shared_ptr<fastgltf::Asset> asset;
        std::chrono::steady_clock::time_point lastAccess;
        size_t memoryUsage;
    };

    std::unordered_map<std::string, CachedMesh> cache;
    size_t maxMemoryBytes = 1024 * 1024 * 1024; // 1GB
    size_t currentMemoryUsage = 0;
    std::mutex mutex;

public:
    std::shared_ptr<fastgltf::Asset> getOrLoad(const std::string& key,
                                               const std::filesystem::path& path) {
        std::lock_guard lock(mutex);

        if (auto it = cache.find(key); it != cache.end()) {
            it->second.lastAccess = std::chrono::steady_clock::now();
            return it->second.asset;
        }

        // Загрузка
        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            return nullptr;
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                     fastgltf::Options::LoadExternalBuffers);
        if (asset.error() != fastgltf::Error::None) {
            return nullptr;
        }

        // Сохранение в кэш
        auto sharedAsset = std::make_shared<fastgltf::Asset>(std::move(asset.get()));
        size_t assetSize = estimateAssetSize(*sharedAsset);

        // Освобождение памяти при необходимости
        while (currentMemoryUsage + assetSize > maxMemoryBytes && !cache.empty()) {
            auto oldest = std::min_element(cache.begin(), cache.end(),
                [](const auto& a, const auto& b) {
                    return a.second.lastAccess < b.second.lastAccess;
                });

            currentMemoryUsage -= oldest->second.memoryUsage;
            cache.erase(oldest);
        }

        cache[key] = CachedMesh{
            .asset = sharedAsset,
            .lastAccess = std::chrono::steady_clock::now(),
            .memoryUsage = assetSize
        };
        currentMemoryUsage += assetSize;

        return sharedAsset;
    }
};
```

---

## Заключение

### Ключевые моменты интеграции

1. **Data-Oriented Design**: Преобразуйте иерархические данные glTF в плоские SoA массивы для максимальной
   производительности.
2. **Zero-copy загрузка**: Используйте `BufferAllocationCallback` для прямой записи в GPU память, минуя промежуточные
   копии.
3. **Интеграция с ECS**: Используйте Flecs для управления сущностями с SoA компонентами, оптимизированными для кэша
   процессора.
4. **Асинхронная загрузка**: Реализуйте streaming и lazy loading для плавного пользовательского опыта.
5. **Профилирование**: Всегда измеряйте время загрузки и используйте оптимальные `Options` и `Category`.

### Рекомендации для ProjectV

- **Для статических воксельных миров**: Используйте `Category::OnlyRenderable` и zero-copy загрузку.
- **Для анимированных персонажей**: Используйте полную загрузку с анимациями и скиннингом.
- **Для редактора уровней**: Реализуйте streaming loader с прогресс-баром.
- **Для мобильных устройств**: Используйте memory mapping для больших файлов.

### Пример полного пайплайна

```cpp
// 1. Асинхронная загрузка
auto future = std::async(std::launch::async, [path]() {
    return loadGltfWithZeroCopy(path, gpuContext);
});

// 2. Конвертация в SoA
auto asset = future.get();
if (!asset) { /* обработка ошибки */ }

auto soaData = convertToSoA(*asset);

// 3. Загрузка в ECS
auto entity = GltfLoaderSystem::loadMeshToEcs(world, soaData);

// 4. Создание GPU ресурсов
auto vulkanData = createVulkanResources(soaData, allocator);

// 5. Регистрация в системе рендеринга
registerMeshForRendering(entity, vulkanData);
```

### Дальнейшие шаги

1. **GPU-driven рендеринг**: Используйте compute shaders для frustum culling и indirect drawing.
2. **Mesh streaming**: Реализуйте progressive loading деталей меша.
3. **Compression**: Интегрируйте Draco или meshopt для сжатия геометрии.
4. **LOD система**: Автоматическая генерация уровней детализации.

Fastgltf предоставляет мощный фундамент для высокопроизводительной загрузки glTF. Правильная интеграция с Data-Oriented
Design, Vulkan и ECS позволяет создавать системы, способные обрабатывать сложные сцены с тысячами объектов в реальном
времени.
