# Draco: Интеграция в C++ проекты

**Draco** — библиотека сжатия геометрических данных, которая легко интегрируется в современные C++ проекты. Этот
документ описывает практические аспекты интеграции: настройку CMake, базовую инициализацию и типовой конвейер работы с
данными.

---

## CMake интеграция

### Базовая интеграция через submodules

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MyProject)

# Добавление Draco как submodule
add_subdirectory(external/draco)

# Создание исполняемого файла
add_executable(my_app main.cpp)

# Линковка с Draco
target_link_libraries(my_app PRIVATE draco::draco)

# Настройка стандарта C++
target_compile_features(my_app PRIVATE cxx_std_26)
set_target_properties(my_app PROPERTIES
  CXX_EXTENSIONS OFF
)
```

---

## Базовая инициализация

### Hello World: минимальная инициализация Draco

```cpp
#include <draco/compression/encode.h>
#include <draco/compression/decode.h>
#include <expected>
#include <print>

int main() {
    // 1. Создание минимального mesh
    draco::Mesh mesh;
    mesh.set_num_points(3);

    // 2. Базовая конфигурация encoder
    draco::Encoder encoder;
    encoder.SetSpeedOptions(5, 5);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);

    // 3. Кодирование
    draco::EncoderBuffer buffer;
    auto encode_status = encoder.EncodeMeshToBuffer(mesh, &buffer);

    if (!encode_status.ok()) {
        std::println(stderr, "Ошибка кодирования: {}",
                    encode_status.error_msg());
        return 1;
    }

    // 4. Декодирование для проверки
    draco::DecoderBuffer decode_buffer;
    decode_buffer.Init(buffer.data(), buffer.size());

    draco::Decoder decoder;
    auto decode_result = decoder.DecodeMeshFromBuffer(&decode_buffer);

    if (!decode_result.ok()) {
        std::println(stderr, "Ошибка декодирования: {}",
                    decode_result.status().error_msg());
        return 1;
    }

    std::println("Draco успешно инициализирован: закодировано {} байт",
                buffer.size());
    return 0;
}
```

### Простейший пример декодирования

```cpp
#include <draco/compression/decode.h>
#include <expected>
#include <print>
#include <span>

// Декодирование Draco данных
[[nodiscard]] std::expected<std::unique_ptr<draco::Mesh>, draco::Status>
decode_draco(std::span<const std::byte> compressed_data) noexcept {
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(compressed_data.data()),
                compressed_data.size());

    draco::Decoder decoder;
    auto result = decoder.DecodeMeshFromBuffer(&buffer);

    if (!result.ok()) {
        return std::unexpected(result.status());
    }

    return std::move(result).value();
}

// Пример использования
void process_compressed_mesh(std::span<const std::byte> data) {
    auto mesh_result = decode_draco(data);

    if (!mesh_result) {
        std::println(stderr, "Ошибка декодирования: {}",
                    mesh_result.error().error_msg());
        return;
    }

    auto mesh = std::move(*mesh_result);
    std::println("Декодирован mesh: {} вершин, {} граней",
                mesh->num_points(), mesh->num_faces());

    // Доступ к атрибутам
    if (const auto* pos_attr = mesh->GetNamedAttribute(
            draco::GeometryAttribute::POSITION)) {
        std::println("Атрибут позиций: {} значений", pos_attr->size());
    }
}
```

### Базовый пример работы с Point Cloud

```cpp
#include <draco/compression/encode.h>
#include <draco/point_cloud/point_cloud.h>
#include <expected>
#include <print>

// Минимальный пример кодирования point cloud
[[nodiscard]] std::expected<std::vector<std::byte>, draco::Status>
encode_simple_point_cloud() noexcept {
    draco::PointCloud pc;
    pc.set_num_points(100);

    // Настройка encoder для point cloud
    draco::Encoder encoder;
    encoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
    encoder.SetSpeedOptions(5, 5);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);

    draco::EncoderBuffer buffer;
    auto status = encoder.EncodePointCloudToBuffer(pc, &buffer);

    if (!status.ok()) {
        return std::unexpected(status);
    }

    std::vector<std::byte> result(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());
    return result;
}
```

---

## Базовые примеры использования API

### Пример 1: Кодирование mesh с настройками

```cpp
#include <draco/compression/encode.h>
#include <draco/mesh/mesh.h>
#include <expected>
#include <print>

[[nodiscard]] std::expected<std::vector<std::byte>, draco::Status>
encode_mesh_with_settings() noexcept {
    draco::Mesh mesh;
    mesh.set_num_points(100);

    draco::Encoder encoder;
    // Настройка скорости: 0 = лучшее сжатие, 10 = быстрее
    encoder.SetSpeedOptions(3, 7);
    // Квантование позиций: 12 бит на компонент
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
    // Выбор метода кодирования
    encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

    draco::EncoderBuffer buffer;
    auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);

    if (!status.ok()) {
        return std::unexpected(status);
    }

    std::vector<std::byte> result(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());
    return result;
}
```

### Пример 2: Декодирование с проверкой типа геометрии

```cpp
#include <draco/compression/decode.h>
#include <expected>
#include <print>

void process_compressed_data(std::span<const std::byte> data) {
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());

    // Проверка типа геометрии перед декодированием
    auto geometry_type = draco::Decoder::GetEncodedGeometryType(&buffer);

    if (!geometry_type.ok()) {
        std::println(stderr, "Ошибка определения типа геометрии");
        return;
    }

    draco::Decoder decoder;

    switch (*geometry_type) {
        case draco::POINT_CLOUD: {
            auto result = decoder.DecodePointCloudFromBuffer(&buffer);
            if (result.ok()) {
                std::println("Декодирован point cloud: {} точек",
                            result->get()->num_points());
            }
            break;
        }
        case draco::MESH: {
            auto result = decoder.DecodeMeshFromBuffer(&buffer);
            if (result.ok()) {
                std::println("Декодирован mesh: {} вершин, {} граней",
                            result->get()->num_points(),
                            result->get()->num_faces());
            }
            break;
        }
        default:
            std::println(stderr, "Неизвестный тип геометрии");
    }
}
```

---

## Расширенные примеры

### Пример 3: Использование ExpertEncoder для точного контроля

```cpp
#include <draco/compression/expert_encode.h>
#include <draco/mesh/mesh.h>
#include <expected>
#include <print>

[[nodiscard]] std::expected<std::vector<std::byte>, draco::Status>
encode_mesh_with_expert_encoder() noexcept {
    draco::Mesh mesh;
    mesh.set_num_points(100);
    mesh.SetNumFaces(50);

    // Создание ExpertEncoder для конкретного mesh
    draco::ExpertEncoder expert_encoder(mesh);

    // Настройка скорости (0-10)
    expert_encoder.SetSpeedOptions(3, 7);

    // Получение ID атрибутов
    int pos_attr_id = mesh.GetNamedAttributeId(draco::GeometryAttribute::POSITION);

    // Разные настройки квантования для разных атрибутов
    expert_encoder.SetAttributeQuantization(pos_attr_id, 14);  // Позиции: 14 бит

    // Явное квантование с bounding box
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float range = 100.0f;
    expert_encoder.SetAttributeExplicitQuantization(pos_attr_id, 14, 3, origin, range);

    // Выбор prediction scheme
    expert_encoder.SetAttributePredictionScheme(
        pos_attr_id,
        draco::MESH_PREDICTION_PARALLELOGRAM
    );

    // Выбор метода кодирования и подметода
    expert_encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
    expert_encoder.SetEncodingSubmethod(draco::MESH_EDGEBREAKER_VALENCE_ENCODING);

    // Кодирование
    draco::EncoderBuffer buffer;
    auto status = expert_encoder.EncodeToBuffer(&buffer);

    if (!status.ok()) {
        return std::unexpected(status);
    }

    std::vector<std::byte> result(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());
    return result;
}
```

### Пример 4: Работа с метаданными

```cpp
#include <draco/metadata/geometry_metadata.h>
#include <draco/point_cloud/point_cloud.h>
#include <print>

void add_metadata_to_point_cloud(draco::PointCloud& pc) {
    // Метаданные для всей геометрии
    auto geometry_metadata = std::make_unique<draco::GeometryMetadata>();
    geometry_metadata->AddEntryString("author", "3D Artist");
    geometry_metadata->AddEntryInt("version", 2);
    geometry_metadata->AddEntryDouble("scale", 1.0);

    pc.AddMetadata(std::move(geometry_metadata));

    // Метаданные для конкретного атрибута
    int pos_attr_id = pc.GetNamedAttributeId(draco::GeometryAttribute::POSITION);
    if (pos_attr_id != -1) {
        auto attr_metadata = std::make_unique<draco::AttributeMetadata>(pos_attr_id);
        attr_metadata->AddEntryString("name", "vertex_positions");
        attr_metadata->AddEntryString("units", "meters");

        pc.AddAttributeMetadata(pos_attr_id, std::move(attr_metadata));
    }

    // Чтение метаданных
    if (const auto* metadata = pc.GetMetadata()) {
        std::string author;
        if (metadata->GetEntryString("author", &author)) {
            std::println("Автор модели: {}", author);
        }
    }
}
```

### Пример 5: Пропуск трансформаций при декодировании

```cpp
#include <draco/compression/decode.h>
#include <print>

void decode_without_dequantization(std::span<const std::byte> data) {
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());

    draco::Decoder decoder;

    // Пропуск де-квантования для позиций
    decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);

    auto result = decoder.DecodeMeshFromBuffer(&buffer);
    if (!result.ok()) {
        std::println(stderr, "Ошибка декодирования: {}",
                    result.status().error_msg());
        return;
    }

    auto mesh = std::move(result).value();

    // Атрибут позиций теперь содержит квантованные значения
    if (const auto* pos_attr = mesh->GetNamedAttribute(
            draco::GeometryAttribute::POSITION)) {

        // Проверка, есть ли трансформация
        if (const auto* transform = pos_attr->GetAttributeTransformData()) {
            std::println("Атрибут имеет трансформацию типа: {}",
                        static_cast<int>(transform->transform_type()));

            // Можно применить трансформацию вручную позже
        }

        // Доступ к квантованным данным
        for (draco::AttributeValueIndex i(0); i < pos_attr->size(); ++i) {
            // Чтение квантованных значений
            // ...
        }
    }
}
```

### Пример 6: Создание mesh с атрибутами

```cpp
#include <draco/mesh/mesh.h>
#include <draco/attributes/geometry_attribute.h>
#include <vector>
#include <print>

std::unique_ptr<draco::Mesh> create_simple_triangle_mesh() {
    auto mesh = std::make_unique<draco::Mesh>();

    // Установка количества вершин и граней
    mesh->set_num_points(3);
    mesh->SetNumFaces(1);

    // Создание атрибута позиций
    draco::GeometryAttribute pos_attr;
    pos_attr.Init(draco::GeometryAttribute::POSITION,
                  nullptr, 3, draco::DT_FLOAT32, false,
                  sizeof(float) * 3, 0);

    int pos_attr_id = mesh->AddAttribute(pos_attr, true, 3);

    // Заполнение позиций вершин
    std::vector<float> positions = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    auto* pos_attribute = mesh->attribute(pos_attr_id);
    for (int i = 0; i < 3; ++i) {
        pos_attribute->SetAttributeValue(
            draco::AttributeValueIndex(i),
            positions.data() + i * 3
        );
    }

    // Установка грани
    draco::Mesh::Face face;
    face[0] = draco::PointIndex(0);
    face[1] = draco::PointIndex(1);
    face[2] = draco::PointIndex(2);
    mesh->SetFace(draco::FaceIndex(0), face);

    return mesh;
}
```

### Пример 7: Интеграция с glTF через Draco Transcoder

```cmake
# CMakeLists.txt с поддержкой трансформера
cmake_minimum_required(VERSION 3.15)
project(MyProject)

# Включение поддержки трансформера
set(DRACO_TRANSCODER_SUPPORTED ON CACHE BOOL "Enable Draco transcoder")
add_subdirectory(external/draco)

# Линковка с draco_transcoder
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE draco::draco_transcoder)
```

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/compression/draco_compression_options.h>

void configure_compression_for_gltf(draco::PointCloud& pc) {
    // Включение сжатия для этой геометрии
    pc.SetCompressionEnabled(true);

    // Настройка параметров сжатия
    draco::DracoCompressionOptions options;
    options.compression_level = 7;
    options.quantization_position = 12;
    options.quantization_normal = 10;
    options.quantization_tex_coord = 10;
    options.quantization_color = 8;
    options.quantization_generic = 8;

    pc.SetCompressionOptions(options);
}
#endif
```

---

## Практические рекомендации по настройке

### Оптимальные настройки квантования

Для различных типов данных рекомендуются следующие настройки квантования:

```cpp
// Рекомендуемые настройки квантования для разных атрибутов
struct DracoQuantizationSettings {
    // Позиции: 11-14 бит (точность 0.05%-0.006%)
    int position_bits = 12;

    // Нормали: 10-12 бит (точность 0.1%-0.025%)
    int normal_bits = 10;

    // Текстурные координаты: 10-12 бит
    int tex_coord_bits = 10;

    // Цвета: 8-10 бит (для нормализованных uint8)
    int color_bits = 8;

    // Пользовательские атрибуты: 8-12 бит
    int generic_bits = 8;
};

// Применение настроек
void apply_quantization_settings(draco::Encoder& encoder,
                                 const DracoQuantizationSettings& settings) {
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION,
                                     settings.position_bits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL,
                                     settings.normal_bits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD,
                                     settings.tex_coord_bits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR,
                                     settings.color_bits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC,
                                     settings.generic_bits);
}
```

### Выбор prediction schemes

Для разных типов атрибутов рекомендуются следующие prediction schemes:

```cpp
// Настройка prediction schemes для mesh
void configure_prediction_schemes(draco::ExpertEncoder& expert_encoder,
                                  int pos_attr_id,
                                  int normal_attr_id,
                                  int tex_coord_attr_id) {
    // Для позиций: параллелограммное предсказание
    expert_encoder.SetAttributePredictionScheme(
        pos_attr_id,
        draco::MESH_PREDICTION_PARALLELOGRAM
    );

    // Для нормалей: геометрическое предсказание
    if (normal_attr_id != -1) {
        expert_encoder.SetAttributePredictionScheme(
            normal_attr_id,
            draco::MESH_PREDICTION_GEOMETRIC_NORMAL
        );
    }

    // Для текстурных координат: портативное предсказание
    if (tex_coord_attr_id != -1) {
        expert_encoder.SetAttributePredictionScheme(
            tex_coord_attr_id,
            draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
        );
    }
}
```

### Настройки скорости кодирования/декодирования

Параметры скорости (0-10) влияют на компромисс между скоростью и степенью сжатия:

```cpp
struct DracoSpeedSettings {
    // Скорость кодирования (0 = лучшее сжатие, 10 = быстрее)
    int encoding_speed = 5;

    // Скорость декодирования (0 = лучшее сжатие, 10 = быстрее)
    int decoding_speed = 7;

    // Оптимальные профили для разных сценариев
    enum Profile {
        MAX_COMPRESSION = 0,      // encoding=0, decoding=0
        BALANCED = 1,             // encoding=5, decoding=7
        FAST_DECODING = 2,        // encoding=7, decoding=10
        REAL_TIME = 3             // encoding=10, decoding=10
    };
};

// Применение профиля скорости
void apply_speed_profile(draco::Encoder& encoder,
                         DracoSpeedSettings::Profile profile) {
    switch (profile) {
        case DracoSpeedSettings::MAX_COMPRESSION:
            encoder.SetSpeedOptions(0, 0);
            break;
        case DracoSpeedSettings::BALANCED:
            encoder.SetSpeedOptions(5, 7);
            break;
        case DracoSpeedSettings::FAST_DECODING:
            encoder.SetSpeedOptions(7, 10);
            break;
        case DracoSpeedSettings::REAL_TIME:
            encoder.SetSpeedOptions(10, 10);
            break;
    }
}
```

### Выбор методов кодирования

Для разных типов геометрии рекомендуются следующие методы кодирования:

```cpp
// Выбор метода кодирования на основе типа геометрии и требований
void select_encoding_method(draco::Encoder& encoder,
                            draco::EncodedGeometryType geometry_type,
                            bool prioritize_compression) {
    switch (geometry_type) {
        case draco::POINT_CLOUD:
            if (prioritize_compression) {
                // KD-Tree для лучшего сжатия
                encoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
            } else {
                // Sequential для быстрого декодирования
                encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
            }
            break;

        case draco::TRIANGULAR_MESH:
            if (prioritize_compression) {
                // Edgebreaker для лучшего сжатия
                encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
            } else {
                // Sequential для быстрого декодирования
                encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
            }
            break;
    }
}
```

## Интеграция в production pipeline

### Конвейер обработки геометрических данных

```cpp
#include <draco/compression/encode.h>
#include <draco/compression/decode.h>
#include <expected>
#include <print>
#include <span>

struct GeometryProcessingPipeline {
    // Конфигурация
    DracoQuantizationSettings quantization;
    DracoSpeedSettings::Profile speed_profile;
    bool prioritize_compression = true;

    // Обработка mesh
    [[nodiscard]] std::expected<std::vector<std::byte>, draco::Status>
    process_mesh(const draco::Mesh& mesh) noexcept {
        // 1. Настройка encoder
        draco::Encoder encoder;
        apply_speed_profile(encoder, speed_profile);
        apply_quantization_settings(encoder, quantization);

        // 2. Выбор метода кодирования
        select_encoding_method(encoder, draco::TRIANGULAR_MESH,
                              prioritize_compression);

        // 3. Кодирование
        draco::EncoderBuffer buffer;
        auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);

        if (!status.ok()) {
            return std::unexpected(status);
        }

        // 4. Возврат результата
        std::vector<std::byte> result(buffer.size());
        std::memcpy(result.data(), buffer.data(), buffer.size());
        return result;
    }

    // Обработка point cloud
    [[nodiscard]] std::expected<std::vector<std::byte>, draco::Status>
    process_point_cloud(const draco::PointCloud& pc) noexcept {
        // Аналогичная логика для point cloud
        draco::Encoder encoder;
        apply_speed_profile(encoder, speed_profile);
        apply_quantization_settings(encoder, quantization);

        select_encoding_method(encoder, draco::POINT_CLOUD,
                              prioritize_compression);

        draco::EncoderBuffer buffer;
        auto status = encoder.EncodePointCloudToBuffer(pc, &buffer);

        if (!status.ok()) {
            return std::unexpected(status);
        }

        std::vector<std::byte> result(buffer.size());
        std::memcpy(result.data(), buffer.data(), buffer.size());
        return result;
    }

    // Декодирование
    [[nodiscard]] std::expected<std::unique_ptr<draco::Mesh>, draco::Status>
    decode_mesh(std::span<const std::byte> data) noexcept {
        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data.data()),
                    data.size());

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (!result.ok()) {
            return std::unexpected(result.status());
        }

        return std::move(result).value();
    }
};
```

### Обработка ошибок и валидация

```cpp
#include <draco/compression/decode.h>
#include <expected>
#include <print>

struct DracoErrorHandler {
    // Проверка валидности Draco данных
    [[nodiscard]] static bool validate_draco_data(
        std::span<const std::byte> data) noexcept {
        if (data.empty()) {
            return false;
        }

        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data.data()),
                    data.size());

        // Проверка типа геометрии
        auto geometry_type = draco::Decoder::GetEncodedGeometryType(&buffer);
        return geometry_type.ok() &&
               (*geometry_type == draco::POINT_CLOUD ||
                *geometry_type == draco::TRIANGULAR_MESH);
    }

    // Детальная обработка ошибок
    [[nodiscard]] static std::string error_to_string(
        const draco::Status& status) noexcept {
        switch (status.code()) {
            case draco::Status::Code::OK:
                return "Успешно";
            case draco::Code::IO_ERROR:
                return "Ошибка ввода-вывода";
            case draco::Code::INVALID_PARAMETER:
                return "Неверный параметр";
            case draco::Code::UNSUPPORTED_FEATURE:
                return "Неподдерживаемая функция";
            case draco::Code::DRACO_ERROR:
                return "Ошибка Draco: " + status.error_msg();
            default:
                return "Неизвестная ошибка";
        }
    }

    // Проверка совместимости версий
    [[nodiscard]] static bool check_version_compatibility(
        std::span<const std::byte> data) noexcept {
        // Проверка заголовка Draco
        if (data.size() < sizeof(draco::DracoHeader)) {
            return false;
        }

        const auto* header = reinterpret_cast<const draco::DracoHeader*>(
            data.data());

        // Проверка магического числа "DRACO"
        if (std::memcmp(header->draco_string, "DRACO", 5) != 0) {
            return false;
        }

        // Проверка версии mesh (2.2) или point cloud (2.3)
        return (header->version_major == 2 &&
               (header->version_minor == 2 || header->version_minor == 3));
    }
};
```

### Производительность и оптимизация

```cpp
#include <chrono>
#include <print>

struct DracoPerformanceMonitor {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    struct Metrics {
        std::chrono::microseconds encode_time;
        std::chrono::microseconds decode_time;
        size_t original_size;
        size_t compressed_size;
        double compression_ratio;
    };

    // Измерение производительности кодирования
    [[nodiscard]] static Metrics measure_encode_performance(
        const draco::Mesh& mesh,
        const draco::Encoder& encoder) noexcept {
        Metrics metrics;
        metrics.original_size = estimate_mesh_size(mesh);

        auto start = Clock::now();
        draco::EncoderBuffer buffer;
        auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);
        auto end = Clock::now();

        if (status.ok()) {
            metrics.encode_time =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            metrics.compressed_size = buffer.size();
            metrics.compression_ratio =
                static_cast<double>(metrics.original_size) / metrics.compressed_size;
        }

        return metrics;
    }

    // Оценка размера mesh
    [[nodiscard]] static size_t estimate_mesh_size(
        const draco::Mesh& mesh) noexcept {
        size_t size = 0;

        // Позиции
        if (const auto* pos_attr = mesh.GetNamedAttribute(
                draco::GeometryAttribute::POSITION)) {
            size += pos_attr->size() * pos_attr->byte_stride();
        }

        // Нормали
        if (const auto* normal_attr = mesh.GetNamedAttribute(
                draco::GeometryAttribute::NORMAL)) {
            size += normal_attr->size() * normal_attr->byte_stride();
        }

        // Текстурные координаты
        if (const auto* tex_attr = mesh.GetNamedAttribute(
                draco::GeometryAttribute::TEX_COORD)) {
            size += tex_attr->size() * tex_attr->byte_stride();
        }

        // Индексы граней
        size += mesh.num_faces() * 3 * sizeof(uint32_t);

        return size;
    }

    // Логирование метрик
    static void log_metrics(const Metrics& metrics) {
        std::println("Производительность Draco:");
        std::println("  Время кодирования: {} мкс",
                    metrics.encode_time.count());
        std::println("  Время декодирования: {} мкс",
                    metrics.decode_time.count());
        std::println("  Исходный размер: {} байт", metrics.original_size);
        std::println("  Сжатый размер: {} байт", metrics.compressed_size);
        std::println("  Коэффициент сжатия: {:.2f}x",
                    metrics.compression_ratio);
    }
};
```

## Заключение

Draco предоставляет минималистичный и эффективный API для сжатия геометрических данных. Ключевые аспекты интеграции:

### 1. **Минимальная CMake интеграция**

- Только базовое подключение через submodules
- Линковка с `draco::draco` target
- Поддержка C++26 с современными фичами

### 2. **Прямое использование API**

- `draco::Encoder` для кодирования mesh и point cloud
- `draco::Decoder` для декодирования
- `draco::ExpertEncoder` для точного контроля над атрибутами
- Настройка через `SetSpeedOptions()` и `SetAttributeQuantization()`

### 3. **Практические рекомендации**

- **Квантование**: 12 бит для позиций, 10 бит для нормалей, 8-10 бит для цветов
- **Prediction schemes**: Параллелограмм для позиций, геометрическое для нормалей
- **Скорость**: Баланс 5/7 для большинства приложений
- **Методы кодирования**: Edgebreaker для mesh, KD-Tree для point cloud

### 4. **Обработка ошибок**

- Использование `std::expected` для типобезопасной обработки ошибок
- Валидация данных перед декодированием
- Проверка совместимости версий

### 5. **Производительность**

- Измерение времени кодирования/декодирования
- Мониторинг коэффициента сжатия
- Оптимизация на основе профиля использования

### 6. **Минимальные примеры кода**

- Без сложных обёрток и фабрик
- Прямое использование `std::expected` для обработки ошибок
- Чистый C++26 с современными фичами: `std::span`, `std::print`, `std::expected`

### Рекомендации по использованию

1. **Для сетевой передачи**: Используйте баланс 5/7 скорости с квантованием 12 бит
2. **Для долгосрочного хранения**: Максимальное сжатие (0/0) с минимальным квантованием
3. **Для real-time приложений**: Быстрое декодирование (7/10) с Sequential encoding
4. **Для glTF интеграции**: Включите `DRACO_TRANSCO
