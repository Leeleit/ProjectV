# Glaze — Quickstart

🟢 **Уровень 1: Базовый** — Быстрый старт с glaze в ProjectV.

## Установка

### CMake (FetchContent)

```cmake
include(FetchContent)

FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG main
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(your_target PRIVATE glaze::glaze)
```

### Подмодуль Git

```bash
git submodule add https://github.com/stephenberry/glaze.git external/glaze
```

```cmake
add_subdirectory(external/glaze)
target_link_libraries(your_target PRIVATE glaze::glaze)
```

---

## Пример 1: Базовая структура

```cpp
#include <glaze/glaze.hpp>
#include <iostream>
#include <string>

struct GameSettings {
    std::string playerName;
    int screenWidth{1920};
    int screenHeight{1080};
    bool fullscreen{false};
    float volume{1.0f};
    
    // Определение metadata для reflection
    struct glaze {
        using T = GameSettings;
        static constexpr auto value = glz::object(
            "playerName", &T::playerName,
            "screenWidth", &T::screenWidth,
            "screenHeight", &T::screenHeight,
            "fullscreen", &T::fullscreen,
            "volume", &T::volume
        );
    };
};

int main() {
    GameSettings settings{
        .playerName = "Player1",
        .screenWidth = 2560,
        .screenHeight = 1440
    };
    
    // Сериализация в JSON
    std::string json;
    glz::write_json(settings, json);
    
    std::cout << json << std::endl;
    // {"playerName":"Player1","screenWidth":2560,"screenHeight":1440,"fullscreen":false,"volume":1.0}
    
    return 0;
}
```

---

## Пример 2: Загрузка из файла

```cpp
#include <glaze/glaze.hpp>
#include <fstream>
#include <expected>

enum class ConfigError {
    FileNotFound,
    ParseError
};

template<typename T>
using Result = std::expected<T, ConfigError>;

// Загрузка конфигурации
template<typename T>
Result<T> loadConfig(const std::filesystem::path& path) {
    T config{};
    
    auto err = glz::read_file_json(config, path.string());
    
    if (err) {
        return std::unexpected(ConfigError::ParseError);
    }
    
    return config;
}

// Сохранение конфигурации
template<typename T>
Result<void> saveConfig(const T& config, const std::filesystem::path& path) {
    auto err = glz::write_file_json(config, path.string());
    
    if (err) {
        return std::unexpected(ConfigError::ParseError);
    }
    
    return {};
}

// Использование
int main() {
    auto settings = loadConfig<GameSettings>("settings.json");
    
    if (settings) {
        std::cout << "Player: " << settings->playerName << std::endl;
        std::cout << "Resolution: " << settings->screenWidth 
                  << "x" << settings->screenHeight << std::endl;
    } else {
        // Создать настройки по умолчанию
        GameSettings defaults;
        saveConfig(defaults, "settings.json");
    }
    
    return 0;
}
```

---

## Пример 3: Вложенные структуры

```cpp
#include <glaze/glaze.hpp>
#include <glm/glm.hpp>

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

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // Euler angles
    glm::vec3 scale{1.0f};
    
    struct glaze {
        using T = Transform;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

struct Entity {
    std::string name;
    Transform transform;
    std::vector<std::string> tags;
    
    struct glaze {
        using T = Entity;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "transform", &T::transform,
            "tags", &T::tags
        );
    };
};

int main() {
    Entity entity{
        .name = "Cube",
        .transform = {
            .position = {10.0f, 5.0f, 20.0f},
            .scale = {2.0f, 2.0f, 2.0f}
        },
        .tags = {"enemy", "destructible"}
    };
    
    std::string json;
    glz::write_json(entity, json);
    
    std::cout << json << std::endl;
    
    return 0;
}
```

---

## Пример 4: Enum

```cpp
#include <glaze/glaze.hpp>

enum class BlockType : uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Water = 4
};

// Glaze автоматически поддерживает enum
// Можно явно указать диапазон
template<>
struct glz::meta<BlockType> {
    static constexpr auto value = glz::enum_range<BlockType, 
        BlockType::Air, BlockType::Water>;
};

struct Voxel {
    BlockType type{BlockType::Air};
    uint8_t metadata{0};
    
    struct glaze {
        using T = Voxel;
        static constexpr auto value = glz::object(
            "type", &T::type,
            "metadata", &T::metadata
        );
    };
};

int main() {
    Voxel voxel{.type = BlockType::Grass, .metadata = 1};
    
    std::string json;
    glz::write_json(voxel, json);
    
    std::cout << json << std::endl;
    // {"type":"Grass","metadata":1}
    
    return 0;
}
```

---

## Пример 5: Бинарная сериализация

```cpp
#include <glaze/glaze.hpp>
#include <vector>

struct ChunkData {
    int32_t x, y, z;
    std::vector<uint8_t> blocks;
    
    struct glaze {
        using T = ChunkData;
        static constexpr auto value = glz::object(
            "x", &T::x,
            "y", &T::y,
            "z", &T::z,
            "blocks", &T::blocks
        );
    };
};

int main() {
    ChunkData chunk{
        .x = 10,
        .y = 5,
        .z = 20,
        .blocks = std::vector<uint8_t>(4096, 1)  // 16³ voxels
    };
    
    // Бинарная сериализация (компактнее JSON)
    std::vector<uint8_t> buffer;
    glz::write_binary(chunk, buffer);
    
    std::cout << "Binary size: " << buffer.size() << " bytes\n";
    
    // Десериализация
    ChunkData loaded;
    glz::read_binary(loaded, buffer);
    
    std::cout << "Loaded chunk at (" << loaded.x 
              << ", " << loaded.y << ", " << loaded.z << ")\n";
    
    return 0;
}
```

---

## Пример 6: Интеграция с ImGui

```cpp
#include <glaze/glaze.hpp>
#include <imgui.h>

struct Material {
    std::string name;
    glm::vec4 baseColor{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    
    struct glaze {
        using T = Material;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "baseColor", &T::baseColor,
            "metallic", &T::metallic,
            "roughness", &T::roughness
        );
    };
};

void drawMaterialInspector(Material& mat) {
    ImGui::InputText("Name", &mat.name);
    ImGui::ColorEdit4("Base Color", &mat.baseColor.x);
    ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &mat.roughness, 0.01f, 1.0f);
}

// Автоматический UI на основе glaze metadata (псевдокод)
void drawGlazeInspector(auto& obj) {
    glz::for_each_field(obj, [](auto& field, auto name) {
        using T = std::decay_t<decltype(field)>;
        
        if constexpr (std::is_same_v<T, float>) {
            ImGui::DragFloat(name, &field, 0.1f);
        } else if constexpr (std::is_same_v<T, int>) {
            ImGui::DragInt(name, &field, 1);
        } else if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(name, &field);
        }
        // ... другие типы
    });
}
```

---

## Обработка ошибок

```cpp
#include <glaze/glaze.hpp>
#include <iostream>

int main() {
    std::string badJson = R"({"name": 123})";  // name должен быть строкой
    
    GameSettings settings;
    auto err = glz::read_json(settings, badJson);
    
    if (err) {
        // Получаем информацию об ошибке
        std::cerr << "Parse error at position: " << err.location << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

## Советы

1. **Всегда определяйте `struct glaze`** внутри структуры для удобства
2. **Используйте `glz::meta<T>`** для типов, которые вы не можете изменить (glm)
3. **Бинарный формат** — быстрее и компактнее, используйте для сохранений
4. **JSON формат** — читаемый, используйте для конфигурации
5. **Валидируйте данные после загрузки** — glaze не проверяет ограничения

---

## Ссылки

- [Glaze Overview](./00_overview.md)
- [Serialization Strategy](../../architecture/practice/11_serialization.md)
- [Reflection с Glaze](../../architecture/practice/13_reflection.md)