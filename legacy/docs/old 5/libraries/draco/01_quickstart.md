# Быстрый старт

🟢 **Уровень 1: Начинающий**

Минимальные примеры кодирования и декодирования геометрических данных.

## Декодирование

### Базовый пример

```cpp
#include <draco/compression/decode.h>
#include <draco/core/decoder_buffer.h>
#include <iostream>

std::unique_ptr<draco::Mesh> decodeMesh(const void* data, size_t size) {
    // 1. Инициализация буфера
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data), size);

    // 2. Определение типа геометрии
    auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
    if (geomType.status().code() != draco::Status::OK) {
        std::cerr << "Invalid Draco data\n";
        return nullptr;
    }

    // 3. Создание декодера
    draco::Decoder decoder;

    // 4. Декодирование
    if (geomType.value() == draco::TRIANGULAR_MESH) {
        auto result = decoder.DecodeMeshFromBuffer(&buffer);
        if (result.ok()) {
            return std::move(result).value();
        }
    }

    return nullptr;
}
```

### Декодирование Point Cloud

```cpp
std::unique_ptr<draco::PointCloud> decodePointCloud(
    const void* data, size_t size) {

    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data), size);

    draco::Decoder decoder;
    auto result = decoder.DecodePointCloudFromBuffer(&buffer);

    if (result.ok()) {
        return std::move(result).value();
    }
    return nullptr;
}
```

### Универсальная функция декодирования

```cpp
#include <variant>

using Geometry = std::variant<
    std::unique_ptr<draco::Mesh>,
    std::unique_ptr<draco::PointCloud>
>;

std::optional<Geometry> decodeGeometry(const void* data, size_t size) {
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data), size);

    auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
    if (!geomType.ok()) {
        return std::nullopt;
    }

    draco::Decoder decoder;

    switch (geomType.value()) {
        case draco::TRIANGULAR_MESH: {
            auto result = decoder.DecodeMeshFromBuffer(&buffer);
            if (result.ok()) {
                return Geometry{std::move(result).value()};
            }
            break;
        }
        case draco::POINT_CLOUD: {
            auto result = decoder.DecodePointCloudFromBuffer(&buffer);
            if (result.ok()) {
                return Geometry{std::move(result).value()};
            }
            break;
        }
        default:
            break;
    }

    return std::nullopt;
}
```

## Кодирование

### Базовый пример для Mesh

```cpp
#include <draco/compression/encode.h>
#include <draco/mesh/mesh.h>

std::vector<char> encodeMesh(const draco::Mesh& mesh) {
    draco::Encoder encoder;

    // Настройка квантования
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12);

    // Настройка скорости (0 = лучший compression, 10 = fastest)
    encoder.SetSpeedOptions(0, 0);

    // Кодирование
    draco::EncoderBuffer buffer;
    auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);

    if (status.ok()) {
        return std::vector<char>(
            buffer.data(),
            buffer.data() + buffer.size()
        );
    }

    return {};
}
```

### Кодирование Point Cloud

```cpp
std::vector<char> encodePointCloud(const draco::PointCloud& pc) {
    draco::Encoder encoder;

    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
    encoder.SetSpeedOptions(5, 5);  // Баланс скорости

    draco::EncoderBuffer buffer;
    auto status = encoder.EncodePointCloudToBuffer(pc, &buffer);

    if (status.ok()) {
        return std::vector<char>(buffer.data(), buffer.data() + buffer.size());
    }

    return {};
}
```

## Создание Mesh с нуля

```cpp
#include <draco/mesh/mesh.h>
#include <draco/attributes/point_attribute.h>

std::unique_ptr<draco::Mesh> createTriangleMesh() {
    auto mesh = std::make_unique<draco::Mesh>();

    // Вершины треугольника
    const float vertices[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 0.0f
    };

    // Создание атрибута позиций
    draco::GeometryAttribute posAttr;
    posAttr.Init(
        draco::GeometryAttribute::POSITION,
        nullptr,                    // buffer (пока нет)
        3,                          // num_components (x, y, z)
        draco::DT_FLOAT32,          // data_type
        false,                      // normalized
        sizeof(float) * 3,          // byte_stride
        0                           // byte_offset
    );

    // Добавление атрибута в меш
    const int posAttrId = mesh->AddAttribute(
        posAttr,
        true,           // identity_mapping
        3               // num_attribute_values
    );

    // Установка значений вершин
    for (draco::AttributeValueIndex i(0); i < 3; ++i) {
        mesh->attribute(posAttrId)->SetAttributeValue(
            i, &vertices[i.value() * 3]
        );
    }

    // Установка граней (один треугольник)
    mesh->SetNumFaces(1);
    draco::Mesh::Face face;
    face[0] = draco::PointIndex(0);
    face[1] = draco::PointIndex(1);
    face[2] = draco::PointIndex(2);
    mesh->SetFace(draco::FaceIndex(0), face);

    // Установка количества точек
    mesh->set_num_points(3);

    return mesh;
}
```

## Создание Point Cloud

```cpp
std::unique_ptr<draco::PointCloud> createPointCloud() {
    auto pc = std::make_unique<draco::PointCloud>();

    // Позиции точек
    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    draco::GeometryAttribute posAttr;
    posAttr.Init(
        draco::GeometryAttribute::POSITION,
        nullptr, 3, draco::DT_FLOAT32, false,
        sizeof(float) * 3, 0
    );

    pc->AddAttribute(posAttr, true, 4);

    for (draco::AttributeValueIndex i(0); i < 4; ++i) {
        pc->attribute(0)->SetAttributeValue(i, &positions[i.value() * 3]);
    }

    pc->set_num_points(4);

    return pc;
}
```

## Доступ к данным декодированного Mesh

```cpp
void processDecodedMesh(const draco::Mesh& mesh) {
    std::cout << "Faces: " << mesh.num_faces() << "\n";
    std::cout << "Points: " << mesh.num_points() << "\n";
    std::cout << "Attributes: " << mesh.num_attributes() << "\n";

    // Получение позиций вершин
    const auto* posAttr = mesh.GetNamedAttribute(
        draco::GeometryAttribute::POSITION
    );

    if (posAttr) {
        for (draco::PointIndex i(0); i < mesh.num_points(); ++i) {
            const draco::AttributeValueIndex valIdx =
                posAttr->mapped_index(i);

            std::array<float, 3> pos;
            posAttr->GetValue(valIdx, &pos);

            std::cout << "Point " << i.value() << ": "
                      << pos[0] << ", " << pos[1] << ", " << pos[2] << "\n";
        }
    }

    // Обход граней
    for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
        const auto& face = mesh.face(f);
        // face[0], face[1], face[2] — индексы вершин треугольника
    }
}
```

## Полный цикл: создание → кодирование → декодирование

```cpp
#include <draco/compression/encode.h>
#include <draco/compression/decode.h>
#include <draco/mesh/mesh.h>

int main() {
    // 1. Создание меша
    auto originalMesh = createTriangleMesh();

    // 2. Кодирование
    draco::Encoder encoder;
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 11);
    encoder.SetSpeedOptions(0, 0);

    draco::EncoderBuffer encodedBuffer;
    auto encodeStatus = encoder.EncodeMeshToBuffer(*originalMesh, &encodedBuffer);

    if (!encodeStatus.ok()) {
        std::cerr << "Encoding failed\n";
        return 1;
    }

    std::cout << "Encoded size: " << encodedBuffer.size() << " bytes\n";

    // 3. Декодирование
    draco::DecoderBuffer decoderBuffer;
    decoderBuffer.Init(encodedBuffer.data(), encodedBuffer.size());

    draco::Decoder decoder;
    auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);

    if (!decodeResult.ok()) {
        std::cerr << "Decoding failed\n";
        return 1;
    }

    auto decodedMesh = std::move(decodeResult).value();

    std::cout << "Decoded mesh:\n";
    std::cout << "  Faces: " << decodedMesh->num_faces() << "\n";
    std::cout << "  Points: " << decodedMesh->num_points() << "\n";

    return 0;
}
```

## Настройки квантования по умолчанию

| Атрибут   | Default | Высокое качество | Низкое качество |
|-----------|---------|------------------|-----------------|
| Position  | 11      | 14-16            | 8-10            |
| Normal    | 7       | 10               | 5-6             |
| Tex coord | 10      | 12               | 8               |
| Color     | 8       | 10               | 6               |

## Типичные ошибки

| Ошибка                  | Причина              | Решение                     |
|-------------------------|----------------------|-----------------------------|
| `Invalid geometry type` | Неверные данные      | Проверьте заголовок файла   |
| `Encoding failed`       | Некорректный меш     | Проверьте num_points, faces |
| `Attribute not found`   | Отсутствует атрибут  | Проверьте GetNamedAttribute |
| `Decode error`          | Несовместимая версия | Обновите Draco              |
