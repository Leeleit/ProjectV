# Решение проблем

🟡 **Уровень 2: Средний**

Типичные ошибки и их решения.

## Ошибки декодирования

### Invalid geometry type

**Симптом:**

```cpp
auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
// geomType.status().code() != OK
```

**Причины:**

- Данные не являются Draco bitstream
- Повреждённый заголовок
- Неверная версия Draco

**Решения:**

```cpp
// Проверка заголовка
bool isDracoData(const void* data, size_t size) {
    if (size < 5) return false;
    const char* header = static_cast<const char*>(data);
    return strncmp(header, "DRACO", 5) == 0;
}

// Проверка перед декодированием
if (!isDracoData(data, size)) {
    std::cerr << "Not a valid Draco file\n";
    return;
}
```

### DecodeMeshFromBuffer failed

**Симптом:**

```cpp
auto result = decoder.DecodeMeshFromBuffer(&buffer);
// result.ok() == false
```

**Причины:**

- Point cloud вместо mesh
- Несовместимая версия bitstream
- Недостаточно данных

**Решения:**

```cpp
// Проверка типа геометрии
auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
if (!geomType.ok()) {
    std::cerr << "Cannot determine geometry type\n";
    return;
}

draco::Decoder decoder;

if (geomType.value() == draco::TRIANGULAR_MESH) {
    auto result = decoder.DecodeMeshFromBuffer(&buffer);
} else if (geomType.value() == draco::POINT_CLOUD) {
    auto result = decoder.DecodePointCloudFromBuffer(&buffer);
} else {
    std::cerr << "Invalid geometry type\n";
}
```

### Attribute not found

**Симптом:**

```cpp
const auto* attr = mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
// attr == nullptr
```

**Причины:**

- Атрибут не был закодирован
- Неправильный тип атрибута

**Решения:**

```cpp
// Проверка наличия атрибута
const auto* attr = mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
if (!attr) {
    std::cout << "Warning: Normal attribute not found\n";
    // Создать default normals или продолжить без них
}

// Перебор всех атрибутов
for (int i = 0; i < mesh.num_attributes(); ++i) {
    const auto* attr = mesh.attribute(i);
    std::cout << "Attribute " << i << ": "
              << draco::GeometryAttribute::TypeToString(attr->attribute_type())
              << "\n";
}
```

## Ошибки кодирования

### Encoding failed

**Симптом:**

```cpp
auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);
// status.ok() == false
```

**Причины:**

- Некорректный mesh (нет граней, нет точек)
- Отсутствует position attribute
- Несовместимые настройки

**Решения:**

```cpp
// Проверка mesh перед кодированием
bool validateMesh(const draco::Mesh& mesh) {
    if (mesh.num_points() == 0) {
        std::cerr << "Mesh has no points\n";
        return false;
    }

    if (mesh.num_faces() == 0) {
        std::cerr << "Mesh has no faces\n";
        return false;
    }

    const auto* pos = mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
    if (!pos) {
        std::cerr << "Mesh has no position attribute\n";
        return false;
    }

    return true;
}

if (!validateMesh(mesh)) {
    return;
}
```

### Quantization out of range

**Симптом:**

```cpp
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 20);
// Ошибка или неожиданный результат
```

**Причина:** Квантование > 16 бит не поддерживается для float атрибутов.

**Решение:**

```cpp
// Допустимые значения: 1-16 для position/normal/texcoord
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);  // OK
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);    // OK
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12); // OK
```

### Prediction scheme failed

**Симптом:**

```cpp
auto status = encoder.SetAttributePredictionScheme(type, scheme);
// status.ok() == false
```

**Причина:** Prediction scheme несовместим с типом атрибута или геометрии.

**Решение:**

```cpp
// Используйте автоматический выбор
draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);  // Автоматический выбор prediction schemes

// Или проверяйте совместимость
// MESH_PREDICTION_PARALLELOGRAM — только для mesh positions
// MESH_PREDICTION_GEOMETRIC_NORMAL — только для normals
// PREDICTION_DIFFERENCE — универсальный
```

## Проблемы производительности

### Медленное декодирование

**Причина:** Высокий compression level или Edgebreaker.

**Решения:**

```cpp
// При кодировании оптимизируйте под decode speed
encoder.SetSpeedOptions(5, 10);  // encoding_speed=5, decoding_speed=10

// Используйте Sequential вместо Edgebreaker
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

### Большой размер файла

**Причина:** Низкое квантование или неоптимальные настройки.

**Решения:**

```cpp
// Увеличьте compression level
encoder.SetSpeedOptions(0, 0);  // Максимальный compression

// Уменьшите quantization bits
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 6);

// Используйте Edgebreaker
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

// Deduplication перед кодированием
mesh.DeduplicatePointIds();
mesh.DeduplicateAttributeValues();
```

### Высокое потребление памяти

**Причина:** Большие mesh или множественное кодирование.

**Решения:**

```cpp
// Обработка по частям
// Разбейте большие модели на chunks

// Освобождение промежуточных данных
{
    draco::EncoderBuffer buffer;
    encoder.EncodeMeshToBuffer(mesh, &buffer);
    // Сохранение buffer
}  // buffer уничтожен

// Переиспользование encoder
draco::Encoder encoder;  // Создаётся один раз
for (const auto& mesh : meshes) {
    draco::EncoderBuffer buffer;
    encoder.EncodeMeshToBuffer(mesh, &buffer);
    // ...
    encoder.Reset();  // Сброс для следующего mesh
}
```

## Проблемы совместимости

### Версия bitstream

**Симптом:** Старый decoder не может прочитать новые файлы.

**Решение:**

```cpp
// Проверка версии при кодировании
// Draco гарантирует обратную совместимость decoder

// Mesh bitstream: 2.2
// Point cloud bitstream: 2.3

// Для максимальной совместимости используйте стандартные настройки
draco::Encoder encoder;
encoder.SetSpeedOptions(7, 7);  // Default compression level
```

### Кроссплатформенность

**Проблема:** Разные результаты на разных платформах.

**Решения:**

```cpp
// Избегайте платформенно-зависимых типов
// Используйте явные типы размеров
static_assert(sizeof(float) == 4, "float must be 32-bit");

// Проверяйте endianness при передаче данных между системами
```

## Проблемы glTF

### EXT_mesh_draco не распознаётся

**Причина:** glTF loader не поддерживает расширение.

**Решение:**

```cpp
// Проверьте extensionsUsed
// "KHR_draco_mesh_compression" должен быть в списке

// Убедитесь, что loader поддерживает Draco
// three.js: используйте DRACOLoader
// Filament: встроенная поддержка
// Babylon.js: встроенная поддержка
```

### Текстуры потеряны после transcoding

**Причина:** Текстуры не включены в Draco-сжатие.

**Решение:**

```bash
# Draco сжимает только геометрию
# Текстуры обрабатываются отдельно

# При использовании draco_transcoder:
draco_transcoder -i input.glb -o output.glb
# Текстуры сохраняются в output.glb
```

## Диагностика

### Вывод информации о mesh

```cpp
void diagnoseMesh(const draco::Mesh& mesh) {
    std::cout << "=== Mesh Diagnostics ===\n";
    std::cout << "Points: " << mesh.num_points() << "\n";
    std::cout << "Faces: " << mesh.num_faces() << "\n";
    std::cout << "Attributes: " << mesh.num_attributes() << "\n";

    for (int i = 0; i < mesh.num_attributes(); ++i) {
        const auto* attr = mesh.attribute(i);
        std::cout << "  [" << i << "] "
                  << draco::GeometryAttribute::TypeToString(attr->attribute_type())
                  << " (components: " << static_cast<int>(attr->num_components())
                  << ", values: " << attr->size() << ")\n";
    }

    auto bbox = mesh.ComputeBoundingBox();
    std::cout << "Bounding box: ["
              << bbox.GetMinPoint()[0] << ", " << bbox.GetMinPoint()[1] << ", " << bbox.GetMinPoint()[2]
              << "] - ["
              << bbox.GetMaxPoint()[0] << ", " << bbox.GetMaxPoint()[1] << ", " << bbox.GetMaxPoint()[2]
              << "]\n";
}
```

### Проверка целостности

```cpp
bool validateMeshIntegrity(const draco::Mesh& mesh) {
    // Проверка граней
    for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
        const auto& face = mesh.face(f);
        for (int i = 0; i < 3; ++i) {
            if (face[i].value() >= mesh.num_points()) {
                std::cerr << "Face " << f.value() << " references invalid point\n";
                return false;
            }
        }
    }

    // Проверка атрибутов
    for (int i = 0; i < mesh.num_attributes(); ++i) {
        const auto* attr = mesh.attribute(i);
        if (attr->size() == 0) {
            std::cerr << "Attribute " << i << " is empty\n";
            return false;
        }
    }

    return true;
}
```

### Отладка квантования

```cpp
void analyzeQuantization(const draco::Mesh& original, const draco::Mesh& decoded) {
    const auto* origPos = original.GetNamedAttribute(draco::GeometryAttribute::POSITION);
    const auto* decodedPos = decoded.GetNamedAttribute(draco::GeometryAttribute::POSITION);

    if (!origPos || !decodedPos) return;

    double maxError = 0.0;
    double avgError = 0.0;

    for (draco::PointIndex i(0); i < original.num_points(); ++i) {
        std::array<float, 3> orig, dec;
        origPos->GetValue(origPos->mapped_index(i), &orig);
        decodedPos->GetValue(decodedPos->mapped_index(i), &dec);

        double error = 0.0;
        for (int c = 0; c < 3; ++c) {
            error += std::abs(orig[c] - dec[c]);
        }
        error /= 3.0;

        maxError = std::max(maxError, error);
        avgError += error;
    }
    avgError /= original.num_points();

    std::cout << "Quantization error: max=" << maxError << ", avg=" << avgError << "\n";
}
```

## Частые вопросы

### Как уменьшить размер файла?

1. Уменьшите quantization bits
2. Увеличьте compression level (lower speed)
3. Используйте Edgebreaker
4. Выполните deduplication
5. Удалите неиспользуемые атрибуты

### Как ускорить загрузку?

1. Используйте Sequential encoding
2. Увеличьте decoding_speed в SetSpeedOptions
3. Предзагрузка в фоновом потоке
4. Кэширование декодированных mesh

### Можно ли использовать Draco для real-time streaming?

Да, с правильными настройками:

```cpp
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);  // Fastest
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

### Поддерживает ли Draco morph targets?

Через DRACO_TRANSCODER_SUPPORTED и glTF EXT_mesh_features.
