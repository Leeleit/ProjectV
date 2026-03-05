# Интеграция fastgltf в C++ проекты

**Архитектурный контекст:** Fastgltf служит архитектурным мостом между иерархическими данными glTF и плоскими массивами
Data-Oriented Design. Библиотека выполняет роль архитектурного трансформатора, преобразующего древовидные
структуры glTF в оптимизированные для кэша процессора SoA (Structure of Arrays) форматы.

**Архитектурные цели интеграции:**

1. **Производительность**: SIMD-оптимизированный парсинг для высокопроизводительных приложений
2. **Data-Oriented Design**: Преобразование иерархии в плоские массивы для ECS систем
3. **GPU-ready архитектура**: Прямая загрузка в GPU буферы
4. **Современный C++26**: Использование `std::expected`, `std::span`, `std::print`

## CMake интеграция

### Базовая интеграция с движком

```cmake
# В корневом CMakeLists.txt вашего движка
add_subdirectory(external/fastgltf)

# В целевом CMakeLists.txt
target_link_libraries(YourEngine PRIVATE fastgltf::fastgltf)
```

### Конфигурация для C++26

```cmake
# Убедитесь, что fastgltf компилируется с C++26
set_target_properties(fastgltf::fastgltf PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
```

### Опции CMake для высокопроизводительных систем

```cmake
# Рекомендуемые опции для Data-Oriented Design систем
set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL "Отключить pmr-аллокатор для совместимости с DOD-аллокаторами" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 OFF CACHE BOOL "Использовать C++26" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Отключить модули C++20" FORCE)

add_subdirectory(external/fastgltf)
```

## Архитектурное преобразование Data-Oriented Design

> **Архитектурная метафора:** Представьте glTF как древовидную организацию компании с CEO (сцена), менеджерами (узлы) и
> сотрудниками (меши). Data-Oriented Design — это переход к плоской матричной структуре, где все сотрудники одного
> отдела
> сидят в одном open-space (SoA массиве) для быстрого общения (кэш-локальность).

### Иерархия glTF → SoA массивы

**Проблема:** glTF использует иерархические структуры (деревья узлов), которые плохо подходят для Data-Oriented Design.
Каждый узел хранит трансформацию и ссылки на детей, что приводит к cache misses при обходе.

**Решение:** Преобразование в плоские SoA массивы, где данные одного типа (позиции, нормали) хранятся в отдельных
выровненных массивах:

```cpp
#include <print>
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

// SoA структура для Data-Oriented Design
struct MeshSoA {
    // Structure of Arrays (SoA) для вершин
    alignas(64) std::vector<glm::vec3> positions;  // Выравнивание для кэш-линий
    alignas(64) std::vector<glm::vec3> normals;
    alignas(64) std::vector<glm::vec2> texcoords;
    alignas(64) std::vector<uint32_t> indices;

    // Метаданные
    alignas(16) std::vector<uint32_t> materialIds;  // Выравнивание для GPU
    alignas(16) std::vector<glm::mat4> transforms;
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

        // Аналогично для нормалей, текстурных координат
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
};
```

## Базовая инициализация и загрузка

### Простейший пример загрузки glTF

```cpp
#include <print>
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

// Базовая загрузка glTF файла
std::expected<fastgltf::Asset, fastgltf::Error> load_gltf_basic(
    const std::filesystem::path& path) {

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    // Базовая загрузка без внешних ресурсов
    return parser.loadGltf(data.get(), path.parent_path());
}

// Пример использования
void example_basic_loading() {
    auto asset = load_gltf_basic("model.gltf");

    if (!asset) {
        std::println(stderr, "Ошибка загрузки: {}",
                    fastgltf::getErrorMessage(asset.error()));
        return;
    }

    const auto& model = *asset;
    std::println("Загружена модель: {} мешей, {} материалов",
                model.meshes.size(), model.materials.size());

    // Проход по мешам
    for (size_t i = 0; i < model.meshes.size(); ++i) {
        const auto& mesh = model.meshes[i];
        std::println("  Меш {}: {} примитивов", i, mesh.primitives.size());
    }
}
```

### Загрузка с внешними ресурсами

```cpp
// Загрузка с внешними буферами и изображениями
std::expected<fastgltf::Asset, fastgltf::Error> load_gltf_with_resources(
    const std::filesystem::path& path) {

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    // Загрузка с внешними ресурсами
    auto options = fastgltf::Options::LoadExternalBuffers
                 | fastgltf::Options::LoadExternalImages
                 | fastgltf::Options::DecomposeNodeMatrices;

    return parser.loadGltf(data.get(), path.parent_path(), options);
}
```

### Работа с геометрическими данными

```cpp
// Извлечение вершинных данных из меша
std::expected<std::vector<glm::vec3>, fastgltf::Error> extract_positions(
    const fastgltf::Asset& asset,
    size_t mesh_index,
    size_t primitive_index = 0) {

    if (mesh_index >= asset.meshes.size()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset.meshes[mesh_index];
    if (primitive_index >= mesh.primitives.size()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& primitive = mesh.primitives[primitive_index];

    // Поиск атрибута позиций
    if (auto pos_attr = primitive.findAttribute("POSITION");
        pos_attr != primitive.attributes.cend()) {

        const auto& accessor = asset.accessors[pos_attr->accessorIndex];
        std::vector<glm::vec3> positions(accessor.count);

        // Копирование данных
        if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
            asset, accessor, positions.data()); err != fastgltf::Error::None) {
            return std::unexpected(err);
        }

        return positions;
    }

    return std::unexpected(fastgltf::Error::InvalidGltf);
}
```

### Работа с расширениями glTF

```cpp
// Пример загрузки с поддержкой расширений
std::expected<fastgltf::Asset, fastgltf::Error> load_gltf_with_extensions(
    const std::filesystem::path& path) {

    // Включение необходимых расширений
    auto extensions = fastgltf::Extensions::KHR_draco_mesh_compression
                    | fastgltf::Extensions::EXT_meshopt_compression
                    | fastgltf::Extensions::KHR_texture_basisu
                    | fastgltf::Extensions::KHR_materials_unlit;

    fastgltf::Parser parser(extensions);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    // Проверка поддержки расширений
    auto asset = parser.loadGltf(data.get(), path.parent_path());
    if (!asset) {
        return std::unexpected(asset.error());
    }

    // Проверка, какие расширения использованы в модели
    if (!asset->extensionsUsed.empty()) {
        std::println("Использованные расширения:");
        for (const auto& ext : asset->extensionsUsed) {
            std::println("  - {}", ext);
        }
    }

    // Проверка, какие расширения требуются
    if (!asset->extensionsRequired.empty()) {
        std::println("Требуемые расширения:");
        for (const auto& ext : asset->extensionsRequired) {
            std::println("  - {}", ext);
        }
    }

    return asset;
}

// Работа с конкретными расширениями
void process_draco_compression(const fastgltf::Asset& asset) {
    for (size_t i = 0; i < asset.meshes.size(); ++i) {
        const auto& mesh = asset.meshes[i];
        for (const auto& primitive : mesh.primitives) {
            // Проверка наличия расширения KHR_draco_mesh_compression
            if (primitive.extensions.KHR_draco_mesh_compression.has_value()) {
                const auto& draco = *primitive.extensions.KHR_draco_mesh_compression;
                std::println("Примитив {} использует Draco сжатие:", i);
                std::println("  Буфер: {}, смещение: {}",
                           draco.bufferView, draco.attributes.size());

                // Атрибуты, сжатые через Draco
                for (const auto& [attribute, index] : draco.attributes) {
                    std::println("  Атрибут {}: индекс {}", attribute, index);
                }
            }
        }
    }
}
```

### Использование кастомных коллбеков

```cpp
// Кастомный коллбек для маппинга буферов в GPU память
struct GpuBufferMapper {
    VkDevice device;
    VkDeviceMemory memory;
    std::unordered_map<std::uint64_t, VkBuffer> buffers;

    fastgltf::BufferInfo mapCallback(std::uint64_t bufferSize, void* userPointer) {
        // Создание GPU буфера через Vulkan
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VkBuffer buffer;
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

        // Выделение памяти
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory bufferMemory;
        vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
        vkBindBufferMemory(device, buffer, bufferMemory, 0);

        // Маппинг памяти
        void* mappedMemory;
        vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);

        // Сохранение для последующего анмаппинга
        buffers[bufferSize] = buffer;

        return fastgltf::BufferInfo{
            .mappedMemory = mappedMemory,
            .customId = reinterpret_cast<fastgltf::CustomBufferId>(buffer)
        };
    }

    static void unmapCallback(fastgltf::BufferInfo* bufferInfo, void* userPointer) {
        auto* self = static_cast<GpuBufferMapper*>(userPointer);
        auto buffer = reinterpret_cast<VkBuffer>(bufferInfo->customId);

        // Анмаппинг памяти
        vkUnmapMemory(self->device, buffer);

        // Очистка
        self->buffers.erase(bufferInfo->customId);
        vkDestroyBuffer(self->device, buffer, nullptr);
    }

private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        // Реализация поиска типа памяти
        return 0;
    }
};

// Кастомный коллбек для декодирования base64
struct ParallelBase64Decoder {
    std::vector<std::thread> workers;

    static void decodeCallback(std::string_view base64, std::uint8_t* dataOutput,
                              std::size_t padding, std::size_t dataOutputSize,
                              void* userPointer) {
        // Параллельное декодирование base64
        auto* self = static_cast<ParallelBase64Decoder*>(userPointer);

        // Разделение работы между потоками
        const size_t chunkSize = 1024 * 1024; // 1MB chunks
        const size_t numChunks = (base64.size() + chunkSize - 1) / chunkSize;

        for (size_t i = 0; i < numChunks; ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, base64.size());

            self->workers.emplace_back([=]() {
                // Декодирование части данных
                // Реализация декодирования base64
            });
        }

        // Ожидание завершения всех потоков
        for (auto& worker : self->workers) {
            if (worker.joinable()) worker.join();
        }
        self->workers.clear();
    }
};

// Пример использования кастомных коллбеков
std::expected<fastgltf::Asset, fastgltf::Error> load_with_custom_callbacks(
    const std::filesystem::path& path) {

    GpuBufferMapper gpuMapper{/* инициализация */};
    ParallelBase64Decoder parallelDecoder;

    fastgltf::Parser parser;

    // Установка кастомных коллбеков
    parser.setBufferAllocationCallback(
        [](std::uint64_t size, void* user) -> fastgltf::BufferInfo {
            auto* mapper = static_cast<GpuBufferMapper*>(user);
            return mapper->mapCallback(size, user);
        },
        [](fastgltf::BufferInfo* info, void* user) {
            auto* mapper = static_cast<GpuBufferMapper*>(user);
            GpuBufferMapper::unmapCallback(info, user);
        }
    );

    parser.setBase64DecodeCallback(ParallelBase64Decoder::decodeCallback);
    parser.setUserPointer(&parallelDecoder);

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    return parser.loadGltf(data.get(), path.parent_path(),
                          fastgltf::Options::LoadExternalBuffers);
}
```

### Работа с различными DataSource вариантами

```cpp
// Пример использования различных источников данных
void data_source_examples() {
    // 1. Загрузка из файла (стандартный способ)
    auto fileData = fastgltf::GltfDataBuffer::FromPath("model.gltf");

    // 2. Загрузка из памяти
    std::vector<std::byte> memoryBuffer = loadFileToMemory("model.gltf");
    auto memoryData = fastgltf::GltfDataBuffer::FromBytes(
        memoryBuffer.data(), memoryBuffer.size());

    // 3. Memory-mapped файлы (только Windows/Linux)
#if FASTGLTF_HAS_MEMORY_MAPPED_FILE
    auto mappedData = fastgltf::MappedGltfFile::FromPath("model.gltf");
#endif

    // 4. Потоковая загрузка
    fastgltf::GltfFileStream streamData("model.gltf");
}

// Использование категорий загрузки для оптимизации
std::expected<fastgltf::Asset, fastgltf::Error> load_with_categories(
    const std::filesystem::path& path) {

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    // Загрузка только рендеринг-компонентов (без анимаций, камер и т.д.)
    auto category = fastgltf::Category::OnlyRenderable;

    return parser.loadGltf(data.get(), path.parent_path(),
                          fastgltf::Options::LoadExternalBuffers,
                          category);
}

// Экспорт glTF моделей
void export_gltf_examples() {
    // Создание простой модели
    fastgltf::Asset asset;
    asset.assetVersion = "2.0";
    asset.assetGenerator = "MyEngine";

    // Создание буфера с вершинами
    std::vector<float> vertices = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f
    };

    fastgltf::Buffer buffer;
    buffer.data = fastgltf::sources::Vector{std::vector<std::byte>(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(float)
    )};
    buffer.byteLength = vertices.size() * sizeof(float);
    asset.buffers.push_back(std::move(buffer));

    // Создание BufferView
    fastgltf::BufferView bufferView;
    bufferView.bufferIndex = 0;
    bufferView.byteOffset = 0;
    bufferView.byteLength = vertices.size() * sizeof(float);
    bufferView.byteStride = 3 * sizeof(float);
    bufferView.target = fastgltf::BufferTarget::ArrayBuffer;
    asset.bufferViews.push_back(std::move(bufferView));

    // Создание Accessor
    fastgltf::Accessor accessor;
    accessor.bufferViewIndex = 0;
    accessor.byteOffset = 0;
    accessor.componentType = fastgltf::ComponentType::Float;
    accessor.count = 3;
    accessor.type = fastgltf::AccessorType::Vec3;
    asset.accessors.push_back(std::move(accessor));

    // Создание меша
    fastgltf::Mesh mesh;
    fastgltf::Primitive primitive;
    primitive.attributes["POSITION"] = 0; // Индекс accessor'а
    primitive.mode = fastgltf::PrimitiveMode::Triangles;
    mesh.primitives.push_back(std::move(primitive));
    asset.meshes.push_back(std::move(mesh));

    // Создание узла
    fastgltf::Node node;
    node.meshIndex = 0;
    asset.nodes.push_back(std::move(node));

    // Создание сцены
    fastgltf::Scene scene;
    scene.nodeIndices.push_back(0);
    asset.scenes.push_back(std::move(scene));
    asset.defaultScene = 0;

    // Экспорт в JSON
    fastgltf::Exporter exporter;
    auto result = exporter.writeGltfJson(asset, fastgltf::ExportOptions::PrettyPrintJson);

    if (result) {
        std::println("Экспортирован JSON размером {} байт", result->output.size());
        // Сохранение в файл
        std::ofstream file("exported.gltf");
        file << result->output;
    }

    // Экспорт в бинарный GLB
    fastgltf::FileExporter fileExporter;
    auto error = fileExporter.writeGltfBinary(asset, "exported.glb");

    if (error == fastgltf::Error::None) {
        std::println("Успешно экспортирован GLB файл");
    }
}

// Продвинутая работа с accessor'ами через tools.hpp
void advanced_accessor_usage(const fastgltf::Asset& asset) {
    if (asset.accessors.empty()) return;

    const auto& accessor = asset.accessors[0];

    // 1. Итерация по accessor'у
    std::vector<glm::vec3> positions;
    positions.reserve(accessor.count);

    fastgltf::iterateAccessor<glm::vec3>(asset, accessor, [&](glm::vec3 pos) {
        positions.push_back(pos);
    });

    // 2. Использование итераторов
    auto iterable = fastgltf::iterateAccessor<glm::vec3>(asset, accessor);
    for (const auto& pos : iterable) {
        // Обработка каждой позиции
    }

    // 3. Копирование данных с кастомным stride
    std::vector<glm::vec3> customLayout(accessor.count);
    fastgltf::copyFromAccessor<glm::vec3, 48>(asset, accessor, customLayout.data());
    // 48 байт на элемент (выравнивание для GPU)

    // 4. Работа с разреженными (sparse) accessor'ами
    if (accessor.sparse.has_value()) {
        std::println("Accessor использует sparse хранение: {} элементов",
                    accessor.sparse->count);

        // Итерация с поддержкой sparse данных
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, accessor, [](glm::vec3 pos, std::size_t index) {
                // pos содержит либо данные из sparse буфера,
                // либо нули для не-sparse индексов
            }
        );
    }

    // 5. Кастомный адаптер для данных буферов
    struct CustomBufferAdapter {
        std::unordered_map<std::size_t, span<const std::byte>> cachedViews;

        auto operator()(const fastgltf::Asset& asset, std::size_t bufferViewIdx) const {
            // Кэширование buffer view для производительности
            if (auto it = cachedViews.find(bufferViewIdx); it != cachedViews.end()) {
                return it->second;
            }

            const auto& bufferView = asset.bufferViews[bufferViewIdx];
            const auto& buffer = asset.buffers[bufferView.bufferIndex];

            return std::visit(fastgltf::visitor {
                [](auto&) -> span<const std::byte> {
                    return {};
                },
                [&](const fastgltf::sources::Array& array) -> span<const std::byte> {
                    return span(array.bytes.data(), array.bytes.size_bytes())
                        .subspan(bufferView.byteOffset, bufferView.byteLength);
                },
                [&](const fastgltf::sources::Vector& vec) -> span<const std::byte> {
                    return span(vec.bytes.data(), vec.bytes.size())
                        .subspan(bufferView.byteOffset, bufferView.byteLength);
                },
                [&](const fastgltf::sources::ByteView& bv) -> span<const std::byte> {
                    return bv.bytes.subspan(bufferView.byteOffset, bufferView.byteLength);
                },
            }, buffer.data);
        }
    };

    CustomBufferAdapter adapter;
    auto element = fastgltf::getAccessorElement<glm::vec3>(asset, accessor, 0, adapter);
}

// Вычисление трансформаций узлов
void compute_node_transforms(const fastgltf::Asset& asset) {
    if (asset.scenes.empty()) return;

    const auto& scene = asset.scenes[0];

    // Итерация по всем узлам сцены с вычислением мировых трансформаций
    fastgltf::iterateSceneNodes(asset, 0, glm::mat4(1.0f),
        [](fastgltf::Node& node, const glm::mat4& worldMatrix) {
            // worldMatrix - мировая трансформация для данного узла

            // Получение локальной трансформации узла
            auto localMatrix = fastgltf::getLocalTransformMatrix(node);

            // Пример: применение трансформации к мешу
            if (node.meshIndex.has_value()) {
                // Здесь можно применить worldMatrix к вершинам меша
            }
        }
    );

    // Получение трансформации конкретного узла
    if (!asset.nodes.empty()) {
        const auto& node = asset.nodes[0];

        // Получение локальной матрицы трансформации
        auto localMatrix = fastgltf::getLocalTransformMatrix(node);

        // Вычисление мировой матрицы с заданной базовой трансформацией
        auto worldMatrix = fastgltf::getTransformMatrix(node, glm::mat4(1.0f));

        std::println("Локальная матрица: {}x{}",
                    localMatrix.columns(), localMatrix.rows());
        std::println("Мировая матрица: {}x{}",
                    worldMatrix.columns(), worldMatrix.rows());
    }
}

// Работа с ElementTraits для типобезопасного доступа к данным
void element_traits_examples() {
    // Проверка traits для различных типов данных
    static_assert(fastgltf::ElementTraits<glm::vec3>::type ==
                  fastgltf::AccessorType::Vec3, "Неверный тип для vec3");

    static_assert(fastgltf::ElementTraits<glm::vec3>::enum_component_type ==
                  fastgltf::ComponentType::Float, "Неверный component type");

    static_assert(fastgltf::ElementTraits<glm::mat4>::needs_transpose == true,
                  "Матрицы требуют транспонирования");

    // Использование traits для generic кода
    template<typename T>
    void process_accessor(const fastgltf::Asset& asset,
                         const fastgltf::Accessor& accessor) {
        using Traits = fastgltf::ElementTraits<T>;

        // Проверка совместимости типов
        if (accessor.type != Traits::type) {
            throw std::runtime_error("Несовместимые типы accessor'а");
        }

        // Получение данных с правильным типом
        std::vector<T> data(accessor.count);
        fastgltf::copyFromAccessor<T>(asset, accessor, data.data());

        // Обработка данных...
    }
}

## Типовой конвейер работы

### Полный конвейер загрузки и обработки

```cpp
#include <print>
#include <expected>
#include <vector>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

// Структура для хранения обработанных данных меша
struct ProcessedMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> material_ids;
};

// Конвейер обработки glTF модели
class GltfProcessingPipeline {
public:
    struct ProcessingOptions {
        bool load_external_buffers = true;
        bool load_external_images = false;
        bool decompose_matrices = true;
        bool generate_tangents = false;
        fastgltf::Category category = fastgltf::Category::OnlyRenderable;
    };

    explicit GltfProcessingPipeline(ProcessingOptions options = {})
        : options_(std::move(options)) {}

    // Загрузка и обработка модели
    std::expected<ProcessedMesh, fastgltf::Error> process_model(
        const std::filesystem::path& path,
        size_t mesh_index = 0) {

        // 1. Загрузка glTF
        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            return std::unexpected(data.error());
        }

        // 2. Настройка опций загрузки
        fastgltf::Options load_options = fastgltf::Options::None;
        if (options_.load_external_buffers) {
            load_options |= fastgltf::Options::LoadExternalBuffers;
        }
        if (options_.load_external_images) {
            load_options |= fastgltf::Options::LoadExternalImages;
        }
        if (options_.decompose_matrices) {
            load_options |= fastgltf::Options::DecomposeNodeMatrices;
        }

        // 3. Парсинг
        auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                     load_options, options_.category);
        if (asset.error() != fastgltf::Error::None) {
            return std::unexpected(asset.error());
        }

        // 4. Проверка наличия мешей
        if (mesh_index >= asset->meshes.size()) {
            return std::unexpected(fastgltf::Error::InvalidGltf);
        }

        // 5. Обработка меша
        return process_mesh(*asset, mesh_index);
    }

private:
    ProcessingOptions options_;

    std::expected<ProcessedMesh, fastgltf::Error> process_mesh(
        const fastgltf::Asset& asset,
        size_t mesh_index) {

        ProcessedMesh result;
        const auto& mesh = asset.meshes[mesh_index];

        for (const auto& primitive : mesh.primitives) {
            // Извлечение позиций
            if (auto positions = extract_attribute<glm::vec3>(
                    asset, primitive, "POSITION")) {
                result.positions.insert(result.positions.end(),
                    positions->begin(), positions->end());
            }

            // Извлечение нормалей
            if (auto normals = extract_attribute<glm::vec3>(
                    asset, primitive, "NORMAL")) {
                result.normals.insert(result.normals.end(),
                    normals->begin(), normals->end());
            }

            // Извлечение текстурных координат
            if (auto texcoords = extract_attribute<glm::vec2>(
                    asset, primitive, "TEXCOORD_0")) {
                result.texcoords.insert(result.texcoords.end(),
                    texcoords->begin(), texcoords->end());
            }

            // Извлечение индексов
            if (primitive.indicesAccessor.has_value()) {
                if (auto indices = extract_indices(
                        asset, asset.accessors[*primitive.indicesAccessor])) {
                    result.indices.insert(result.indices.end(),
                        indices->begin(), indices->end());
                }
            }

            // Сохранение ID материала
            if (primitive.materialIndex.has_value()) {
                result.material_ids.push_back(
                    static_cast<uint32_t>(*primitive.materialIndex));
            }
        }

        return result;
    }

    template<typename T>
    std::expected<std::vector<T>, fastgltf::Error> extract_attribute(
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        std::string_view attribute_name) {

        if (auto attr = primitive.findAttribute(attribute_name);
            attr != primitive.attributes.cend()) {

            const auto& accessor = asset.accessors[attr->accessorIndex];
            std::vector<T> data(accessor.count);

            if (auto err = fastgltf::copyFromAccessor<T>(
                asset, accessor, data.data()); err != fastgltf::Error::None) {
                return std::unexpected(err);
            }

            return data;
        }

        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    std::expected<std::vector<uint32_t>, fastgltf::Error> extract_indices(
        const fastgltf::Asset& asset,
        const fastgltf::Accessor& accessor) {

        std::vector<uint32_t> indices(accessor.count);

        if (auto err = fastgltf::copyFromAccessor<uint32_t>(
            asset, accessor, indices.data()); err != fastgltf::Error::None) {
            return std::unexpected(err);
        }

        return indices;
    }
};

// Пример использования конвейера
void example_pipeline_usage() {
    GltfProcessingPipeline::ProcessingOptions options;
    options.load_external_buffers = true;
    options.decompose_matrices = true;

    GltfProcessingPipeline pipeline(options);

    auto result = pipeline.process_model("scene.gltf", 0);

    if (!result) {
        std::println(stderr, "Ошибка обработки: {}",
                    fastgltf::getErrorMessage(result.error()));
        return;
    }

    const auto& mesh = *result;
    std::println("Обработан меш: {} вершин, {} индексов, {} материалов",
                mesh.positions.size(), mesh.indices.size(),
                mesh.material_ids.size());
}
```

## Заключение

Fastgltf предоставляет мощный и гибкий инструментарий для работы с glTF 2.0 моделями в современных C++ проектах.

### Ключевые аспекты интеграции:

1. **Простая CMake интеграция**: Поддержка submodules для минимальной конфигурации
2. **Высокая производительность**: SIMD-оптимизированный парсинг через simdjson
3. **Типобезопасный API**: Использование `std::expected`, `std::optional`, `std::span`
4. **Гибкая архитектура**: Модульные опции загрузки и обработки
5. **Data-Oriented Design**: Поддержка преобразования в SoA форматы

### Рекомендации по использованию:

1. **Для начала**: Используйте базовую загрузку через `load_gltf_basic()`
2. **Для production**: Настройте `GltfProcessingPipeline` под ваши нужды
3. **Для максимальной производительности**: Используйте SoA преобразование для Data-Oriented Design
4. **Для совместимости**: Придерживайтесь стандартных glTF 2.0 спецификаций

### Пример минимальной интеграции:

```cpp
#include <fastgltf/core.hpp>
#include <print>

int main() {
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath("model.gltf");

    if (auto asset = parser.loadGltf(data.get(), ".")) {
        std::println("Успешно загружено: {} мешей", asset->meshes.size());
        return 0;
    } else {
        std::println(stderr, "Ошибка: {}",
                    fastgltf::getErrorMessage(asset.error()));
        return 1;
    }
}
```

Fastgltf устанавливает новый стандарт для работы с glTF в C++, сочетая современные языковые возможности с высокой
производительностью и типобезопасностью.
