## Интеграция fastgltf

<!-- anchor: 02_integration -->


Подключение fastgltf в C++ проект.

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
    std::cout << "Uses: " << ext << "\n";
}

// Обязательные расширения (без которых модель не работает)
for (const auto& ext : asset->extensionsRequired) {
    std::cout << "Requires: " << ext << "\n";
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

## Интеграция fastgltf в ProjectV

<!-- anchor: 10_projectv-integration -->


Данный документ содержит ProjectV-специфичные рекомендации и примеры кода для интеграции библиотеки fastgltf в
воксельный движок.

## Роль fastgltf в ProjectV

fastgltf используется для загрузки 3D-моделей (glTF/GLB) в Vulkan-рендерер ProjectV. Данные буферов можно писать
напрямую в GPU через `setBufferAllocationCallback`. Библиотека работает с VMA и Vulkan для создания vertex/index buffers
и текстур.

### Жизненный цикл загрузки

```
Файл .gltf/.glb
      ↓
GltfDataBuffer / GltfFileStream
      ↓
Parser::loadGltf
      ↓
Asset
      ↓
iterateAccessor / copyFromAccessor
      ↓
VMA / Vulkan vertex/index buffers
      ↓
Создание компонентов flecs
      ↓
Система рендеринга ProjectV
```

---

## Интеграция с Vulkan и VMA

### Стратегии загрузки данных в GPU

| Стратегия                        | Использование               | Преимущества              | Недостатки                 |
|----------------------------------|-----------------------------|---------------------------|----------------------------|
| **Zero-copy через CustomBuffer** | Воксельные чанки, streaming | Нулевые накладные расходы | Требует управления памятью |
| **Staging буферы**               | Статические меши, ландшафты | Простота, совместимость   | Двойное копирование        |
| **Прямое маппирование**          | Анимированные персонажи     | Минимальная задержка      | Сложность синхронизации    |

### Загрузка меша через VMA

```cpp
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

std::optional<VulkanMeshData> loadGltfMeshToVulkan(
    const std::filesystem::path& path,
    VmaAllocator allocator) {

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) return std::nullopt;

    auto asset = parser.loadGltf(
        data.get(),
        path.parent_path(),
        fastgltf::Options::LoadExternalBuffers,
        fastgltf::Category::OnlyRenderable
    );

    if (asset.error() != fastgltf::Error::None) return std::nullopt;

    VulkanMeshData result = {};

    if (!asset->meshes.empty()) {
        const auto& mesh = asset->meshes[0];

        for (const auto& primitive : mesh.primitives) {
            // ПОЗИЦИИ ВЕРШИН
            if (auto* posAttr = primitive.findAttribute("POSITION");
                posAttr != primitive.attributes.cend()) {

                const auto& accessor = asset->accessors[posAttr->accessorIndex];
                result.vertexCount = static_cast<uint32_t>(accessor.count);

                std::vector<glm::vec3> positions(accessor.count);
                fastgltf::copyFromAccessor<glm::vec3>(
                    asset.get(), accessor, positions.data());

                VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                bufferInfo.size = positions.size() * sizeof(glm::vec3);
                bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT;

                VmaAllocationCreateInfo allocCreateInfo = {};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

                vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo,
                    &result.vertexBuffer, &result.vertexAllocation, nullptr);

                void* mappedData;
                vmaMapMemory(allocator, result.vertexAllocation, &mappedData);
                memcpy(mappedData, positions.data(), bufferInfo.size);
                vmaUnmapMemory(allocator, result.vertexAllocation);
            }

            // ИНДЕКСЫ
            if (primitive.indicesAccessor.has_value()) {
                const auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
                result.indexCount = static_cast<uint32_t>(accessor.count);

                std::vector<uint32_t> indices(accessor.count);
                if (accessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                    fastgltf::copyFromAccessor<uint32_t>(
                        asset.get(), accessor, indices.data());
                } else if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                    std::vector<uint16_t> shortIndices(accessor.count);
                    fastgltf::copyFromAccessor<uint16_t>(
                        asset.get(), accessor, shortIndices.data());
                    for (size_t i = 0; i < accessor.count; ++i) {
                        indices[i] = shortIndices[i];
                    }
                }

                // Создание index buffer...
            }
        }
    }

    return result;
}
```

### Zero-copy через BufferAllocationCallback

```cpp
struct VulkanContext {
    VmaAllocator allocator;
    std::vector<VkBuffer> allocatedBuffers;
    std::vector<VmaAllocation> allocations;
};

fastgltf::Parser createGpuParser(VulkanContext& ctx) {
    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize, fastgltf::BufferAllocateFlags)
        -> fastgltf::BufferInfo {

            auto* ctx = static_cast<VulkanContext*>(userPointer);

            VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocInfoOut;
            vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo,
                &buffer, &allocation, &allocInfoOut);

            ctx->allocatedBuffers.push_back(buffer);
            ctx->allocations.push_back(allocation);

            return fastgltf::BufferInfo{
                .mappedMemory = allocInfoOut.pMappedData,
                .customId = ctx->allocatedBuffers.size() - 1
            };
        },
        nullptr,
        nullptr
    );

    return parser;
}
```

---

## Работа с ECS (flecs)

### Компоненты для анимационных данных

```cpp
#include <flecs.h>

struct AnimationComponent {
    std::vector<glm::mat4> jointMatrices;
    float currentTime = 0.0f;
    float duration = 0.0f;
    bool playing = false;
};

struct SkeletonComponent {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<std::string> jointNames;
};

struct MorphTargetsComponent {
    std::vector<fastgltf::MorphTarget> targets;
    std::vector<float> weights;
};
```

### Извлечение анимаций для ECS

```cpp
void extractAnimationsForECS(
    const fastgltf::Asset& asset,
    flecs::world& world,
    flecs::entity entity) {

    for (const auto& animation : asset.animations) {
        AnimationComponent animComp;

        for (const auto& channel : animation.channels) {
            if (!channel.nodeIndex.has_value()) continue;

            const auto& sampler = animation.samplers[channel.samplerIndex];

            // Извлечение ключевых кадров
            std::vector<float> keyframeTimes;
            fastgltf::iterateAccessor<float>(
                asset,
                asset.accessors[sampler.inputAccessor],
                [&](float time) { keyframeTimes.push_back(time); }
            );

            // Определение длительности
            if (!keyframeTimes.empty()) {
                animComp.duration = std::max(animComp.duration,
                    keyframeTimes.back());
            }
        }

        entity.set<AnimationComponent>(animComp);
    }
}
```

### Загрузка скелета

```cpp
void extractSkeletonForECS(
    const fastgltf::Asset& asset,
    flecs::world& world,
    flecs::entity entity) {

    if (asset.skins.empty()) return;

    const auto& skin = asset.skins[0];
    SkeletonComponent skeleton;

    if (skin.inverseBindMatrices.has_value()) {
        const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
        skeleton.inverseBindMatrices.resize(accessor.count);
        fastgltf::copyFromAccessor<glm::mat4>(
            asset, accessor, skeleton.inverseBindMatrices.data());
    }

    for (size_t jointIdx : skin.joints) {
        skeleton.jointNames.push_back(asset.nodes[jointIdx].name);
    }

    entity.set<SkeletonComponent>(skeleton);
}
```

---

## Система материалов

### Конвертация glTF материалов в формат ProjectV

```cpp
struct ProjectVMaterial {
    glm::vec4 albedo;
    float roughness;
    float metallic;
    float emission;
    uint32_t voxelType;
};

ProjectVMaterial convertGltfMaterial(const fastgltf::Material& gltfMat) {
    ProjectVMaterial mat{};

    // Базовый цвет
    if (gltfMat.pbrData.baseColorFactor.size() >= 3) {
        mat.albedo = glm::vec4(
            gltfMat.pbrData.baseColorFactor[0],
            gltfMat.pbrData.baseColorFactor[1],
            gltfMat.pbrData.baseColorFactor[2],
            gltfMat.pbrData.baseColorFactor.size() > 3 ?
                gltfMat.pbrData.baseColorFactor[3] : 1.0f
        );
    }

    // Metallic-roughness
    if (gltfMat.pbrData.metallicFactor.has_value()) {
        mat.metallic = *gltfMat.pbrData.metallicFactor;
    }
    if (gltfMat.pbrData.roughnessFactor.has_value()) {
        mat.roughness = *gltfMat.pbrData.roughnessFactor;
    }

    // Эмиссия для светящихся вокселей
    if (gltfMat.emissiveFactor.size() >= 3) {
        mat.emission = std::max({
            gltfMat.emissiveFactor[0],
            gltfMat.emissiveFactor[1],
            gltfMat.emissiveFactor[2]
        });
    }

    return mat;
}
```

### Расширения для воксельных материалов

```cpp
fastgltf::Extensions voxelMaterialExtensions =
    fastgltf::Extensions::KHR_materials_volume |      // Объёмные материалы
    fastgltf::Extensions::KHR_materials_transmission | // Пропускание света
    fastgltf::Extensions::KHR_materials_clearcoat |    // Clearcoat
    fastgltf::Extensions::KHR_materials_anisotropy;    // Анизотропия

fastgltf::Parser parser(voxelMaterialExtensions);
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

auto loadWithProfiling(const std::filesystem::path& path) {
    ZoneScopedN("GltfLoadTotal");

    {
        ZoneScopedN("LoadFileData");
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
    }

    fastgltf::Parser parser;

    {
        ZoneScopedN("ParseJson");
        auto asset = parser.loadGltf(data.get(), path.parent_path(),
            fastgltf::Options::LoadExternalBuffers);
    }

    {
        ZoneScopedN("CopyToVulkan");
        TracyPlot("VertexCount", vertexCount);
        TracyPlot("BufferSizeMB", bufferSize / (1024 * 1024));
    }

    FrameMark;
}
```

---

## Лучшие практики для ProjectV

1. Используйте `Category::OnlyRenderable` для статического контента — экономит 30-40% времени загрузки
2. Применяйте zero-copy стратегии с VMA — минимизируйте копирование между CPU и GPU
3. Используйте sparse accessors для partial updates — критично для динамических воксельных миров
4. Интегрируйте с Tracy для профилирования — отслеживайте bottleneck'и
5. Кэшируйте загруженные модели — переиспользуйте Asset структуры
6. Используйте асинхронную загрузку — не блокируйте основной поток

---

## Паттерны использования fastgltf в ProjectV

<!-- anchor: 11_projectv-patterns -->


Практические рецепты для типичных задач в воксельном движке.

## Обзор паттернов

| Паттерн                  | Когда использовать               | Ключевое преимущество       |
|--------------------------|----------------------------------|-----------------------------|
| **Асинхронная загрузка** | Большие модели, множество файлов | Не блокирует основной поток |
| **Кэширование моделей**  | Переиспользование ресурсов       | Минимизация загрузок        |
| **Streaming загрузка**   | Open World, большие сцены        | Экономия памяти             |
| **Пул парсеров**         | Многопоточная загрузка           | Параллелизм                 |

---

## Паттерн 1: Асинхронная загрузка моделей

### Задача

Загрузка больших glTF моделей без блокировки рендеринга.

### Решение

```cpp
#include <future>
#include <mutex>
#include <queue>

class AsyncModelLoader {
public:
    struct LoadResult {
        std::string modelName;
        std::shared_ptr<fastgltf::Asset> asset;
        std::vector<VulkanMeshData> meshes;
        bool success = false;
    };

    AsyncModelLoader(size_t threadCount = 2) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(&AsyncModelLoader::workerThread, this);
        }
    }

    ~AsyncModelLoader() {
        {
            std::unique_lock lock(queueMutex);
            shutdown = true;
        }
        cv.notify_all();
        for (auto& t : workers) t.join();
    }

    std::future<LoadResult> loadAsync(const std::filesystem::path& path) {
        auto promise = std::make_shared<std::promise<LoadResult>>();
        auto future = promise->get_future();

        {
            std::unique_lock lock(queueMutex);
            taskQueue.emplace(Task{path, promise});
        }
        cv.notify_one();

        return future;
    }

private:
    struct Task {
        std::filesystem::path path;
        std::shared_ptr<std::promise<LoadResult>> promise;
    };

    void workerThread() {
        thread_local fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);

        while (true) {
            Task task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this] { return shutdown || !taskQueue.empty(); });

                if (shutdown) return;

                task = std::move(taskQueue.front());
                taskQueue.pop();
            }

            LoadResult result;
            result.modelName = task.path.stem().string();

            auto data = fastgltf::GltfDataBuffer::FromPath(task.path);
            if (data.error() != fastgltf::Error::None) {
                task.promise->set_value(result);
                continue;
            }

            auto asset = parser.loadGltf(
                data.get(),
                task.path.parent_path(),
                fastgltf::Options::LoadExternalBuffers,
                fastgltf::Category::OnlyRenderable
            );

            if (asset.error() == fastgltf::Error::None) {
                result.asset = std::make_shared<fastgltf::Asset>(std::move(asset.get()));
                result.success = true;
            }

            task.promise->set_value(std::move(result));
        }
    }

    std::vector<std::thread> workers;
    std::queue<Task> taskQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool shutdown = false;
};
```

### Использование

```cpp
AsyncModelLoader loader(2);

auto future = loader.loadAsync("models/character.glb");

// Продолжаем рендеринг...

if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    auto result = future.get();
    if (result.success) {
        // Создаём GPU ресурсы в основном потоке
        createGpuResources(result.asset);
    }
}
```

---

## Паттерн 2: Кэширование моделей

### Задача

Избежать повторной загрузки одинаковых моделей.

### Решение

```cpp
#include <unordered_map>
#include <memory>
#include <shared_mutex>

class ModelCache {
public:
    struct CachedModel {
        std::shared_ptr<fastgltf::Asset> asset;
        VulkanMeshData meshData;
        std::chrono::steady_clock::time_point lastAccess;
    };

    std::shared_ptr<fastgltf::Asset> getOrLoad(
        const std::filesystem::path& path,
        fastgltf::Parser& parser) {

        std::string key = path.string();

        // Сначала пытаемся найти в кэше (shared lock)
        {
            std::shared_lock lock(cacheMutex);
            auto it = cache.find(key);
            if (it != cache.end()) {
                it->second.lastAccess = std::chrono::steady_clock::now();
                return it->second.asset;
            }
        }

        // Загружаем (без блокировки кэша)
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) return nullptr;

        auto asset = parser.loadGltf(
            data.get(),
            path.parent_path(),
            fastgltf::Options::LoadExternalBuffers
        );

        if (asset.error() != fastgltf::Error::None) return nullptr;

        auto sharedAsset = std::make_shared<fastgltf::Asset>(std::move(asset.get()));

        // Добавляем в кэш (unique lock)
        {
            std::unique_lock lock(cacheMutex);
            cache[key] = CachedModel{
                .asset = sharedAsset,
                .lastAccess = std::chrono::steady_clock::now()
            };
        }

        return sharedAsset;
    }

    void evictUnused(std::chrono::seconds maxAge = std::chrono::seconds(300)) {
        auto now = std::chrono::steady_clock::now();

        std::unique_lock lock(cacheMutex);
        for (auto it = cache.begin(); it != cache.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.lastAccess);
            if (age > maxAge && it->second.asset.use_count() == 1) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<std::string, CachedModel> cache;
    mutable std::shared_mutex cacheMutex;
};
```

---

## Паттерн 3: Streaming загрузка для Open World

### Задача

Загрузка только видимой части большого мира.

### Решение

```cpp
#include <fastgltf/GltfFileStream.hpp>

class StreamingWorldLoader {
public:
    struct Chunk {
        glm::ivec3 coord;
        std::shared_ptr<fastgltf::Asset> asset;
        bool loaded = false;
        bool priority = false;
    };

    void updatePlayerPosition(const glm::vec3& position) {
        glm::ivec3 currentChunk = worldToChunk(position);

        // Определяем какие чанки нужны
        std::vector<glm::ivec3> neededChunks;
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            for (int dy = -loadRadius; dy <= loadRadius; ++dy) {
                for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
                    neededChunks.push_back(currentChunk + glm::ivec3(dx, dy, dz));
                }
            }
        }

        // Загружаем новые чанки
        for (const auto& coord : neededChunks) {
            std::string key = chunkToKey(coord);
            if (loadedChunks.find(key) == loadedChunks.end()) {
                requestChunkLoad(coord);
            }
        }

        // Выгружаем далёкие чанки
        std::erase_if(loadedChunks, [&](const auto& pair) {
            auto coord = keyToChunk(pair.first);
            float distance = glm::distance(glm::vec3(coord), glm::vec3(currentChunk));
            return distance > unloadRadius;
        });
    }

private:
    void requestChunkLoad(const glm::ivec3& coord) {
        std::string key = chunkToKey(coord);
        std::filesystem::path path = chunkToPath(coord);

        if (!std::filesystem::exists(path)) return;

        // Потоковая загрузка через GltfFileStream
        fastgltf::Parser parser;
        auto stream = fastgltf::GltfFileStream(path);

        auto asset = parser.loadGltf(
            stream,
            path.parent_path(),
            fastgltf::Options::None,  // Без LoadExternalBuffers для streaming
            fastgltf::Category::OnlyRenderable
        );

        if (asset.error() == fastgltf::Error::None) {
            loadedChunks[key] = std::make_shared<fastgltf::Asset>(std::move(asset.get()));
        }
    }

    glm::ivec3 worldToChunk(const glm::vec3& pos) const {
        return glm::ivec3(
            static_cast<int>(std::floor(pos.x / chunkSize)),
            static_cast<int>(std::floor(pos.y / chunkSize)),
            static_cast<int>(std::floor(pos.z / chunkSize))
        );
    }

    std::string chunkToKey(const glm::ivec3& coord) const {
        return std::format("chunk_{}_{}_{}", coord.x, coord.y, coord.z);
    }

    std::filesystem::path chunkToPath(const glm::ivec3& coord) const {
        return worldPath / std::format("chunk_{}_{}_{}.glb", coord.x, coord.y, coord.z);
    }

    std::unordered_map<std::string, std::shared_ptr<fastgltf::Asset>> loadedChunks;
    std::filesystem::path worldPath = "world";
    float chunkSize = 16.0f;
    int loadRadius = 2;
    int unloadRadius = 4;
};
```

---

## Паттерн 4: Пул парсеров для многопоточности

### Задача

Parser не потокобезопасен, но нужен параллелизм.

### Решение

```cpp
class ParserPool {
public:
    explicit ParserPool(size_t count,
                        fastgltf::Extensions extensions = fastgltf::Extensions::None) {
        for (size_t i = 0; i < count; ++i) {
            pool.emplace_back(std::make_unique<fastgltf::Parser>(extensions));
            available.push(i);
        }
    }

    class ParserHandle {
    public:
        ParserHandle(fastgltf::Parser* p, std::mutex& m, size_t idx, ParserPool* pool)
            : parser(p), mutex(&m), index(idx), owner(pool) {}

        ~ParserHandle() {
            if (owner) {
                owner->returnParser(index);
            }
        }

        fastgltf::Parser* operator->() { return parser; }
        fastgltf::Parser& operator*() { return *parser; }

    private:
        fastgltf::Parser* parser;
        std::mutex* mutex;
        size_t index;
        ParserPool* owner;
    };

    ParserHandle acquire() {
        std::unique_lock lock(poolMutex);
        cv.wait(lock, [this] { return !available.empty(); });

        size_t idx = available.front();
        available.pop();

        return ParserHandle(pool[idx].get(), parserMutexes[idx], idx, this);
    }

private:
    void returnParser(size_t idx) {
        std::unique_lock lock(poolMutex);
        available.push(idx);
        cv.notify_one();
    }

    std::vector<std::unique_ptr<fastgltf::Parser>> pool;
    std::queue<size_t> available;
    std::mutex poolMutex;
    std::vector<std::mutex> parserMutexes;
    std::condition_variable cv;
};
```

### Использование

```cpp
ParserPool pool(4, fastgltf::Extensions::KHR_texture_basisu);

std::vector<std::future<void>> futures;

for (const auto& file : modelFiles) {
    futures.push_back(std::async(std::launch::async, [&pool, file] {
        auto parser = pool.acquire();

        auto data = fastgltf::GltfDataBuffer::FromPath(file);
        auto asset = parser->loadGltf(data.get(), file.parent_path(),
            fastgltf::Options::LoadExternalBuffers);

        // Обработка...
    }));
}

for (auto& f : futures) f.wait();
```

---

## Паттерн 5: Progressive Loading с приоритетами

### Задача

Загрузка LOD0 сначала, затем более высоких LOD по запросу.

### Решение

```cpp
class ProgressiveModelLoader {
public:
    enum class LodLevel {
        LOD0 = 0,  // Низкая детализация
        LOD1 = 1,  // Средняя
        LOD2 = 2   // Высокая
    };

    struct LodModel {
        std::filesystem::path path;
        std::shared_ptr<fastgltf::Asset> asset;
        bool loaded = false;
        int priority = 0;
    };

    void loadModel(const std::string& modelId,
                   const std::filesystem::path& basePath) {
        // Загружаем LOD0 синхронно
        auto lod0Path = basePath / "lod0.glb";
        auto parser = acquireParser();

        auto data = fastgltf::GltfDataBuffer::FromPath(lod0Path);
        auto asset = parser->loadGltf(data.get(), lod0Path.parent_path(),
            fastgltf::Options::LoadExternalBuffers,
            fastgltf::Category::OnlyRenderable);

        models[modelId][LodLevel::LOD0] = {
            .path = lod0Path,
            .asset = std::make_shared<fastgltf::Asset>(std::move(asset.get())),
            .loaded = true
        };

        // Ставим в очередь более высокие LOD
        for (int lod = 1; lod <= 2; ++lod) {
            auto lodPath = basePath / std::format("lod{}.glb", lod);
            if (std::filesystem::exists(lodPath)) {
                loadQueue.push({
                    .modelId = modelId,
                    .lod = static_cast<LodLevel>(lod),
                    .path = lodPath
                });
            }
        }
    }

    void requestHigherLod(const std::string& modelId, LodLevel lod) {
        // Увеличиваем приоритет загрузки
        std::unique_lock lock(queueMutex);
        for (auto& task : loadQueue) {
            if (task.modelId == modelId && task.lod == lod) {
                task.priority = 10;  // Высокий приоритет
                break;
            }
        }
        cv.notify_one();
    }

private:
    struct LoadTask {
        std::string modelId;
        LodLevel lod;
        std::filesystem::path path;
        int priority = 0;
    };

    std::map<std::string, std::map<LodLevel, LodModel>> models;
    std::priority_queue<LoadTask> loadQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
};
```

---

## Резюме

| Паттерн              | Сложность | Память      | Параллелизм |
|----------------------|-----------|-------------|-------------|
| Асинхронная загрузка | Средняя   | Низкая      | Высокий     |
| Кэширование моделей  | Низкая    | Высокая     | Низкий      |
| Streaming загрузка   | Высокая   | Оптимальная | Средний     |
| Пул парсеров         | Низкая    | Низкая      | Высокий     |
| Progressive Loading  | Высокая   | Оптимальная | Средний     |

Выбирайте паттерн в зависимости от требований проекта к производительности и сложности реализации.
