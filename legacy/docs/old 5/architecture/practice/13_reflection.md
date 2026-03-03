# Reflection с Glaze [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Автоматическая генерация UI и сериализация на основе типов.

## Обзор

ProjectV использует библиотеку **glaze** для:

- **Compile-time reflection** без макросов
- **JSON и бинарной сериализации**
- **Автоматической генерации ImGui UI**
- **Валидации данных**

Glaze предоставляет современный подход к reflection в C++23, используя `glz::meta` для описания структур.

---

## 1. Базовая Reflection с Glaze

### 1.1 Определение метаданных

```cpp
#include <glaze/glaze.hpp>
#include <glm/glm.hpp>

namespace projectv {

// Простая структура с reflection
struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    // Metadata для glaze — определяется внутри структуры
    struct glaze {
        using T = Transform;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

// Альтернативный способ: внешняя специализация
struct Material {
    std::string name;
    glm::vec4 baseColor{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
};

} // namespace projectv

// Внешняя специализация glz::meta
template<>
struct glz::meta<projectv::Material> {
    using T = projectv::Material;
    static constexpr auto value = glz::object(
        "name", &T::name,
        "baseColor", &T::baseColor,
        "metallic", &T::metallic,
        "roughness", &T::roughness
    );
};
```

### 1.2 Специализации для glm типов

```cpp
// Специализация для glm::vec3
template<>
struct glz::meta<glm::vec3> {
    using T = glm::vec3;
    static constexpr auto value = glz::object(
        "x", &T::x,
        "y", &T::y,
        "z", &T::z
    );
};

// Специализация для glm::vec4
template<>
struct glz::meta<glm::vec4> {
    using T = glm::vec4;
    static constexpr auto value = glz::object(
        "x", &T::x,
        "y", &T::y,
        "z", &T::z,
        "w", &T::w
    );
};

// Специализация для glm::quat
template<>
struct glz::meta<glm::quat> {
    using T = glm::quat;
    static constexpr auto value = glz::object(
        "x", &T::x,
        "y", &T::y,
        "z", &T::z,
        "w", &T::w
    );
};

// Специализация для glm::mat4
template<>
struct glz::meta<glm::mat4> {
    static constexpr auto value = [](auto& self) {
        // Сериализуем как массив из 4 vec4
        return std::array<glm::vec4, 4>{
            glm::row(self, 0),
            glm::row(self, 1),
            glm::row(self, 2),
            glm::row(self, 3)
        };
    };
};
```

---

## 2. JSON Сериализация

### 2.1 Базовое использование

```cpp
#include <glaze/glaze.hpp>
#include <expected>

namespace projectv {

enum class SerializeError {
    InvalidFormat,
    BufferTooSmall,
    MissingField
};

template<typename T>
using Result = std::expected<T, SerializeError>;

class JsonSerializer {
public:
    // Сериализация в JSON строку
    template<typename T>
    Result<std::string> serialize(const T& obj) const {
        std::string buffer;
        buffer.resize(1024);

        auto err = glz::write_json(obj, buffer);

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return buffer;
    }

    // Десериализация из JSON строки
    template<typename T>
    Result<T> deserialize(std::string_view json) const {
        T obj{};
        auto err = glz::read_json(obj, json);

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return obj;
    }
};

// Пример использования
void exampleJson() {
    Transform transform{
        .position = {1.0f, 2.0f, 3.0f},
        .rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    };

    JsonSerializer serializer;
    auto json = serializer.serialize(transform);

    if (json) {
        std::cout << *json << std::endl;
        // {"position":{"x":1.0,"y":2.0,"z":3.0},"rotation":{...},"scale":{...}}
    }

    auto parsed = serializer.deserialize<Transform>(*json);
    if (parsed) {
        // Используем *parsed
    }
}

} // namespace projectv
```

### 2.2 Работа с файлами

```cpp
namespace projectv {

class JsonFileIO {
public:
    template<typename T>
    Result<void> save(const T& obj, const std::filesystem::path& path) const {
        std::string buffer;
        auto err = glz::write_file_json(obj, path.string());

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return {};
    }

    template<typename T>
    Result<T> load(const std::filesystem::path& path) const {
        T obj{};
        auto err = glz::read_file_json(obj, path.string());

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return obj;
    }
};

} // namespace projectv
```

---

## 3. Бинарная сериализация

### 3.1 Компактный формат

```cpp
namespace projectv {

class BinarySerializer {
public:
    // Сериализация в бинарный формат (компактнее JSON)
    template<typename T>
    Result<std::vector<uint8_t>> serialize(const T& obj) const {
        std::vector<uint8_t> buffer;

        auto err = glz::write_binary(obj, buffer);

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return buffer;
    }

    // Десериализация из бинарного формата
    template<typename T>
    Result<T> deserialize(std::span<const uint8_t> data) const {
        T obj{};
        auto err = glz::read_binary(obj, data);

        if (err) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return obj;
    }

    // Получение размера без сериализации
    template<typename T>
    static constexpr size_t getSize() {
        return glz::binary_size<T>();
    }
};

// Сравнение размеров
void compareFormats() {
    Transform t;

    JsonSerializer json;
    BinarySerializer binary;

    auto jsonData = json.serialize(t);
    auto binaryData = binary.serialize(t);

    std::cout << "JSON: " << jsonData->size() << " bytes\n";
    std::cout << "Binary: " << binaryData->size() << " bytes\n";
    // Binary обычно в 2-3 раза компактнее
}

} // namespace projectv
```

---

## 4. Автоматический ImGui Inspector

### 4.1 Генератор UI

```cpp
#include <imgui.h>
#include <glaze/glaze.hpp>

namespace projectv {

class GlazeInspector {
public:
    // Главная точка входа
    template<typename T>
    void inspect(const char* name, T& obj) {
        ImGui::PushID(name);

        if constexpr (glz::detail::is_object<T>) {
            inspectStruct(name, obj);
        } else if constexpr (std::is_same_v<T, float>) {
            ImGui::DragFloat(name, &obj, 0.1f);
        } else if constexpr (std::is_same_v<T, int>) {
            ImGui::DragInt(name, &obj, 1);
        } else if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(name, &obj);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            ImGui::DragFloat3(name, &obj.x, 0.1f);
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            ImGui::DragFloat4(name, &obj.x, 0.01f);
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            inspectQuaternion(name, obj);
        } else if constexpr (std::is_same_v<T, std::string>) {
            inspectString(name, obj);
        }

        ImGui::PopID();
    }

private:
    // Инспектирование структуры через glaze metadata
    template<typename T>
    void inspectStruct(const char* name, T& obj) {
        if (ImGui::CollapsingHeader(name)) {
            ImGui::Indent();

            // Итерация по полям через glaze
            // Glaze предоставляет compile-time итерацию
            glz::for_each_field(obj, [this](auto& field, auto name) {
                using FieldType = std::decay_t<decltype(field)>;
                this->inspect(name, field);
            });

            ImGui::Unindent();
        }
    }

    // Специальное отображение для кватерниона
    void inspectQuaternion(const char* name, glm::quat& q) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(q));

        if (ImGui::DragFloat3(name, &euler.x, 1.0f)) {
            q = glm::quat(glm::radians(euler));
        }
    }

    // Строка с ограничением размера
    void inspectString(const char* name, std::string& str) {
        char buffer[256];
        std::strncpy(buffer, str.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        if (ImGui::InputText(name, buffer, sizeof(buffer))) {
            str = buffer;
        }
    }
};

} // namespace projectv
```

### 4.2 Интеграция с ECS

```cpp
namespace projectv {

class ECSInspector {
public:
    ECSInspector(flecs::world& world) : world_(world) {}

    void draw() {
        ImGui::Begin("Entity Inspector");

        // Список сущностей
        drawEntityList();

        // Инспектор выбранной сущности
        if (selectedEntity_.is_alive()) {
            ImGui::Separator();
            drawEntityInspector(selectedEntity_);
        }

        ImGui::End();
    }

private:
    flecs::world& world_;
    flecs::entity selectedEntity_;
    GlazeInspector inspector_;

    void drawEntityList() {
        ImGui::Text("Entities:");

        world_.each([this](flecs::entity e) {
            char label[64];
            snprintf(label, sizeof(label), "%s (id: %u)",
                     e.name().c_str(), e.id());

            if (ImGui::Selectable(label, e == selectedEntity_)) {
                selectedEntity_ = e;
            }
        });
    }

    void drawEntityInspector(flecs::entity e) {
        ImGui::Text("Entity: %s", e.name().c_str());

        // Transform
        if (e.has<Transform>()) {
            auto* transform = e.get_mut<Transform>();
            inspector_.inspect("Transform", *transform);
        }

        // Material
        if (e.has<Material>()) {
            auto* material = e.get_mut<Material>();
            inspector_.inspect("Material", *material);
        }

        // Добавьте другие компоненты
    }
};

} // namespace projectv
```

---

## 5. Валидация данных

### 5.1 Автоматическая валидация

```cpp
namespace projectv {

// Ограничения для полей
struct Validator {
    template<typename T>
    static bool inRange(T& value, T min, T max) {
        if (value < min) { value = min; return false; }
        if (value > max) { value = max; return false; }
        return true;
    }
};

// Материал с валидацией
struct PBRMaterial {
    std::string name;
    glm::vec4 baseColor{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emissiveStrength{0.0f};

    // Валидация после загрузки
    void validate() {
        Validator::inRange(metallic, 0.0f, 1.0f);
        Validator::inRange(roughness, 0.01f, 1.0f);  // Минимум 0.01 для PBR
        Validator::inRange(emissiveStrength, 0.0f, 100.0f);
        Validator::inRange(baseColor.r, 0.0f, 1.0f);
        Validator::inRange(baseColor.g, 0.0f, 1.0f);
        Validator::inRange(baseColor.b, 0.0f, 1.0f);
        Validator::inRange(baseColor.a, 0.0f, 1.0f);
    }

    struct glaze {
        using T = PBRMaterial;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "baseColor", &T::baseColor,
            "metallic", &T::metallic,
            "roughness", &T::roughness,
            "emissiveStrength", &T::emissiveStrength
        );
    };
};

} // namespace projectv
```

---

## 6. Расширенные возможности

### 6.1 Версионирование формата

```cpp
namespace projectv {

struct VoxelChunkV1 {
    int32_t x, y, z;
    std::vector<uint8_t> blocks;

    struct glaze {
        using T = VoxelChunkV1;
        static constexpr auto value = glz::object(
            "x", &T::x,
            "y", &T::y,
            "z", &T::z,
            "blocks", &T::blocks
        );
    };
};

struct VoxelChunkV2 {
    int32_t x, y, z;
    std::vector<uint8_t> blocks;
    std::vector<uint8_t> metadata;  // Новое поле
    uint32_t version = 2;

    struct glaze {
        using T = VoxelChunkV2;
        static constexpr auto value = glz::object(
            "x", &T::x,
            "y", &T::y,
            "z", &T::z,
            "blocks", &T::blocks,
            "metadata", &T::metadata,
            "version", &T::version
        );
    };
};

// Миграция V1 -> V2
VoxelChunkV2 migrate(const VoxelChunkV1& v1) {
    VoxelChunkV2 v2;
    v2.x = v1.x;
    v2.y = v1.y;
    v2.z = v1.z;
    v2.blocks = v1.blocks;
    v2.metadata.resize(256, 0);  // Значение по умолчанию
    return v2;
}

} // namespace projectv
```

### 6.2 Enum reflection

```cpp
namespace projectv {

enum class BlockType : uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Water = 4,
    Sand = 5
};

// Glaze автоматически поддерживает enum, но можно настроить имена
template<>
struct glz::meta<BlockType> {
    static constexpr auto value = glz::enum_range<BlockType,
        BlockType::Air, BlockType::Sand>;
};

// В ImGui: Combo box из enum
void inspectBlockType(const char* name, BlockType& type) {
    const char* items[] = {"Air", "Stone", "Dirt", "Grass", "Water", "Sand"};
    int current = static_cast<int>(type);

    if (ImGui::Combo(name, &current, items, IM_ARRAYSIZE(items))) {
        type = static_cast<BlockType>(current);
    }
}

} // namespace projectv
```

### 6.3 Вложенные структуры

```cpp
namespace projectv {

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    struct glaze {
        using T = BoundingBox;
        static constexpr auto value = glz::object(
            "min", &T::min,
            "max", &T::max
        );
    };
};

struct MeshInfo {
    std::string name;
    BoundingBox bounds;
    uint32_t vertexCount;
    uint32_t indexCount;

    struct glaze {
        using T = MeshInfo;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "bounds", &T::bounds,  // Вложенная структура
            "vertexCount", &T::vertexCount,
            "indexCount", &T::indexCount
        );
    };
};

} // namespace projectv
```

---

## 7. Сравнение с альтернативами

| Подход                         | Макросы | Runtime overhead | Compile-time | Поддержка  |
|--------------------------------|---------|------------------|--------------|------------|
| **glaze**                      | Нет     | Минимальный      | Да           | Активная   |
| Ручной макрос `REFLECT_FIELDS` | Да      | Нет              | Да           | Самописный |
| C++26 `std::meta`              | Нет     | Нет              | Да           | Будущее    |
| RTTI                           | Нет     | Высокий          | Нет          | Стандарт   |

## Рекомендации

1. **Используйте внутренний `struct glaze`** для типов в вашем коде
2. **Используйте внешнюю специализацию `glz::meta`** для сторонних типов (glm)
3. **Валидируйте данные после десериализации** — glaze не делает этого автоматически
4. **Бинарный формат** для сохранений, **JSON** для конфигурации

---

## Ссылки

- [Glaze Overview](../../libraries/glaze/00_overview.md)
- [Serialization Strategy](./11_serialization.md)
- [Banned Features](../guides/cpp/11_banned-features.md)
