# Glaze — Integration

Подключение Glaze к Vulkan-ориентированному движку, базовые паттерны и каноничные структуры конфигурации.

---

## CMake Integration

### Вариант 1: Git Submodule

```cmake
# Add as git submodule
add_subdirectory(external/glaze)

target_link_libraries(engine_core PRIVATE glaze::glaze)
```

### Вариант 2: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  glaze
  # glaze repository configuration
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(engine_core PRIVATE glaze::glaze)
```

### Вариант 3: vcpkg / conan

```cmake
# vcpkg.txt: glaze
find_package(glaze REQUIRED)
target_link_libraries(engine_core PRIVATE glaze::glaze)
```

---

## Каноничные структуры Vulkan-ориентированного движка

### WindowConfig — настройки окна

```cpp
#include <glaze/glaze.hpp>
#include <string>

struct WindowConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    std::string title = "Vulkan Engine";
    bool fullscreen = false;
    bool vsync = true;
    uint32_t msaa_samples = 4;

    struct glaze {
        using T = WindowConfig;
        static constexpr auto value = glz::object(
            "width", &T::width,
            "height", &T::height,
            "title", &T::title,
            "fullscreen", &T::fullscreen,
            "vsync", &T::vsync,
            "msaa_samples", &T::msaa_samples
        );
    };
};
```

### RenderConfig — рендеринг вокселей

```cpp
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

struct RenderConfig {
    float voxel_size = 0.1f;           // Размер вокселя в метрах
    uint32_t svo_max_depth = 12;       // Глубина октодерева (2^12 = 4096)
    uint32_t max_ray_steps = 256;      // Максимум шагов raymarching'а
    bool enable_soft_shadows = true;   // Мягкие тени
    bool enable_gi = false;            // Global Illumination
    bool enable_fog = true;            // Атмосферная дымка
    glm::vec3 ambient_light = {0.1f, 0.1f, 0.1f};
    float exposure = 1.0f;

    struct glaze {
        using T = RenderConfig;
        static constexpr auto value = glz::object(
            "voxel_size", &T::voxel_size,
            "svo_max_depth", &T::svo_max_depth,
            "max_ray_steps", &T::max_ray_steps,
            "enable_soft_shadows", &T::enable_soft_shadows,
            "enable_gi", &T::enable_gi,
            "enable_fog", &T::enable_fog,
            "ambient_light", &T::ambient_light,
            "exposure", &T::exposure
        );
    };
};
```

### PhysicsConfig — физический движок

```cpp
struct PhysicsConfig {
    float fixed_time_step = 0.016f;    // 60Hz physics
    int max_substeps = 4;              // Максимум подшагов
    glm::vec3 gravity = {0.0f, -9.81f, 0.0f};
    float collision_margin = 0.01f;    // Запас для коллизий
    bool enable_ccd = true;            // Continuous Collision Detection
    uint32_t solver_iterations = 8;    // Итерации решателя

    struct glaze {
        using T = PhysicsConfig;
        static constexpr auto value = glz::object(
            "fixed_time_step", &T::fixed_time_step,
            "max_substeps", &T::max_substeps,
            "gravity", &T::gravity,
            "collision_margin", &T::collision_margin,
            "enable_ccd", &T::enable_ccd,
            "solver_iterations", &T::solver_iterations
        );
    };
};
```

### EngineConfig — корневая структура

```cpp
struct EngineConfig {
    WindowConfig window;
    RenderConfig render;
    PhysicsConfig physics;
    // Другие подсистемы...

    struct glaze {
        using T = EngineConfig;
        static constexpr auto value = glz::object(
            "window", &T::window,
            "render", &T::render,
            "physics", &T::physics
        );
    };
};
```

---

## Базовые паттерны использования

### 1. Загрузка конфигурации из файла

```cpp
#include <glaze/glaze.hpp>
#include <filesystem>
#include <expected>
#include <print>

enum class ConfigError {
    FileNotFound,
    ParseError,
    ValidationFailed
};

template<typename T>
using ConfigResult = std::expected<T, ConfigError>;

template<typename T>
ConfigResult<T> load_config(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::print(stderr, "Config file not found: {}\n", path.string());
        return std::unexpected(ConfigError::FileNotFound);
    }

    T config{};
    auto error = glz::read_file_json(config, path.string());

    if (error) {
        std::print(stderr, "Failed to parse config: {} at byte {}\n",
                   glz::format_error(error, path.string()), error.location);
        return std::unexpected(ConfigError::ParseError);
    }

    return config;
}

// Использование
int main() {
    auto config = load_config<EngineConfig>("config/engine.json");

    if (!config) {
        // Создаём конфиг по умолчанию
        EngineConfig defaults;
        glz::write_file_json(defaults, "config/engine.json");
        std::print("Created default config\n");
    } else {
        std::print("Loaded config: {}x{}, voxel size: {}\n",
                   config->window.width, config->window.height,
                   config->render.voxel_size);
    }

    return 0;
}
```

### 2. Сохранение конфигурации

```cpp
template<typename T>
bool save_config(const T& config, const std::filesystem::path& path) {
    // Создаём директорию если нужно
    std::filesystem::create_directories(path.parent_path());

    auto error = glz::write_file_json(config, path.string(), glz::opts{.prettify = true});

    if (error) {
        std::print(stderr, "Failed to save config: {}\n", glz::format_error(error, path.string()));
        return false;
    }

    std::print("Config saved to {}\n", path.string());
    return true;
}
```

### 3. Hot Reload паттерн: Double-Buffered Config State

```cpp
#include <atomic>
#include <chrono>
#include <expected>
#include <thread>
#include <mutex>
#include <condition_variable>

class ConfigManager {
    // Double-buffered конфиг (lock-free подход)
    alignas(64) EngineConfig configs_[2];
    std::atomic<size_t> current_index_{0};
    std::atomic<bool> config_updated_{false};
    std::filesystem::file_time_type last_write_time_;
    std::filesystem::path config_path_;
    
    // Мониторинг файлов
    std::thread monitoring_thread_;
    std::atomic<bool> stop_monitoring_{false};
    std::mutex monitoring_mutex_;
    std::condition_variable monitoring_cv_;

public:
    ConfigManager(std::filesystem::path path)
        : config_path_(std::move(path)) {

        // Загружаем начальный конфиг
        if (auto config = load_config<EngineConfig>(config_path_)) {
            configs_[0] = *config;
            last_write_time_ = std::filesystem::last_write_time(config_path_);
        }

        // Запускаем мониторинг в отдельном потоке
        start_monitoring();
    }

    ~ConfigManager() {
        stop_monitoring();
    }

    const EngineConfig& get_config() const {
        return configs_[current_index_.load(std::memory_order_acquire)];
    }

    bool has_update() const {
        return config_updated_.load(std::memory_order_acquire);
    }

    void apply_update() {
        if (!config_updated_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }

        // Атомарно переключаем индекс
        size_t old_index = current_index_.load(std::memory_order_relaxed);
        size_t new_index = 1 - old_index;  // Переключаем 0<->1
        current_index_.store(new_index, std::memory_order_release);
        
        std::print("Config hot-reloaded (lock-free double buffer)\n");
    }

private:
    void start_monitoring() {
        monitoring_thread_ = std::thread([this]() {
            while (!stop_monitoring_.load(std::memory_order_acquire)) {
                check_for_updates();
                
                // Ждем 500ms между проверками
                std::unique_lock lock(monitoring_mutex_);
                monitoring_cv_.wait_for(lock, std::chrono::milliseconds(500),
                    [this]() { return stop_monitoring_.load(std::memory_order_acquire); });
            }
        });
    }

    void stop_monitoring() {
        stop_monitoring_.store(true, std::memory_order_release);
        monitoring_cv_.notify_all();
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }

    void check_for_updates() {
        // Проверяем существование файла без исключений
        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(config_path_, ec);
        
        if (ec) {
            return;
        }

        if (current_time > last_write_time_) {
            last_write_time_ = current_time;

            // Парсим конфиг
            EngineConfig new_config;
            auto error = glz::read_file_json(new_config, config_path_.string());

            if (!error) {
                // Успешный парсинг — сохраняем в буфер
                size_t old_index = current_index_.load(std::memory_order_relaxed);
                size_t new_index = 1 - old_index;
                configs_[new_index] = std::move(new_config);
                config_updated_.store(true, std::memory_order_release);
            }
        }
    }
};
```

### 4. Интеграция с ECS (flecs)

```cpp
#include <flecs.h>

// Компонент конфигурации в ECS
struct ConfigComponent {
    EngineConfig config;
};

// Система применения обновлений конфига
void config_update_system(flecs::iter& it, ConfigComponent* configs) {
    auto* manager = static_cast<ConfigManager*>(it.world().get_context());

    if (manager->has_update()) {
        manager->apply_update();

        // Обновляем все компоненты ConfigComponent
        for (auto i : it) {
            configs[i].config = manager->get_config();
        }

        std::print("ECS config updated for {} entities\n", it.count());
    }
}

// Система рендеринга, читающая конфиг каждый кадр
void render_system(flecs::iter& it, ConfigComponent* configs) {
    for (auto i : it) {
        const auto& config = configs[i].config;

        // Используем актуальные настройки рендеринга
        float voxel_size = config.render.voxel_size;
        uint32_t ray_steps = config.render.max_ray_steps;
        bool enable_soft_shadows = config.render.enable_soft_shadows;
        bool enable_gi = config.render.enable_gi;
        bool enable_fog = config.render.enable_fog;
        glm::vec3 ambient_light = config.render.ambient_light;
        float exposure = config.render.exposure;
        
        // Пример использования настроек в рендеринге
        // Здесь будет реальный код рендеринга с использованием конфигурации
        // Например: setup_rendering_parameters(voxel_size, ray_steps, enable_soft_shadows, ambient_light, exposure);
    }
}

// Регистрация в мире ECS
void register_config_systems(flecs::world& world, ConfigManager* manager) {
    world.set_context(manager);

    world.system<ConfigComponent>("ConfigUpdateSystem")
        .kind(flecs::OnLoad)  // В начале кадра
        .iter(config_update_system);

    world.system<ConfigComponent>("RenderSystem")
        .kind(flecs::OnStore)  // В конце кадра
        .iter(render_system);
}
```

---

## Метаданные ассетов

### Структура метаданных воксельного материала

```cpp
#include <glaze/glaze.hpp>
#include <vector>
#include <string>

enum class VoxelMaterialType : uint8_t {
    Solid = 0,
    Liquid = 1,
    Gas = 2,
    Emissive = 3
};

template<>
struct glz::meta<VoxelMaterialType> {
    static constexpr auto value = glz::enum_range<VoxelMaterialType,
        VoxelMaterialType::Solid, VoxelMaterialType::Emissive>;
};

struct VoxelMaterial {
    std::string name;
    VoxelMaterialType type = VoxelMaterialType::Solid;
    glm::vec4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission_strength = 0.0f;
    std::vector<std::string> tags;  // "flammable", "conductive", etc.

    struct glaze {
        using T = VoxelMaterial;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "type", &T::type,
            "albedo", &T::albedo,
            "roughness", &T::roughness,
            "metallic", &T::metallic,
            "emission_strength", &T::emission_strength,
            "tags", &T::tags
        );
    };
};

// Загрузка библиотеки материалов
std::vector<VoxelMaterial> load_material_library(const std::filesystem::path& path) {
    std::vector<VoxelMaterial> materials;

    if (auto error = glz::read_file_json(materials, path.string())) {
        std::print(stderr, "Failed to load material library: {}\n",
                   glz::format_error(error, path.string()));
        return {};
    }

    return materials;
}
```

### Метаданные чанка мира

```cpp
struct ChunkMetadata {
    glm::ivec3 world_coord;
    uint64_t version = 1;
    std::string author;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    uint32_t voxel_count = 0;
    uint32_t compressed_size = 0;
    std::vector<std::string> tags;

    struct glaze {
        using T = ChunkMetadata;
        static constexpr auto value = glz::object(
            "world_coord", &T::world_coord,
            "version", &T::version,
            "author", &T::author,
            "created", &T::created,
            "modified", &T::modified,
            "voxel_count", &T::voxel_count,
            "compressed_size", &T::compressed_size,
            "tags", &T::tags
        );
    };
};
```

---

## Кастомные форматы сериализации

### Binary формат (glz::opts{.format = glz::binary})

```cpp
#include <glaze/glaze.hpp>
#include <vector>
#include <span>

// Сериализация в бинарный формат
std::vector<std::byte> serialize_binary(const EngineConfig& config) {
    std::vector<std::byte> buffer;
    auto error = glz::write_binary(config, buffer);
    
    if (error) {
        std::print(stderr, "Binary serialization failed: {}\n", glz::format_error(error));
        return {};
    }
    
    return buffer;
}

// Десериализация из бинарного формата
std::expected<EngineConfig, glz::error_code> deserialize_binary(std::span<const std::byte> data) {
    EngineConfig config;
    auto error = glz::read_binary(config, data);
    
    if (error) {
        return std::unexpected(error);
    }
    
    return config;
}

// Использование для сетевой передачи
void send_config_over_network(const EngineConfig& config) {
    auto binary_data = serialize_binary(config);
    
    // Отправка по сети (пример с Vulkan-совместимым сокетом)
    // send_packet(binary_data.data(), binary_data.size());
}

// Сохранение в файл с бинарным форматом
bool save_config_binary(const EngineConfig& config, const std::filesystem::path& path) {
    return !glz::write_file_binary(config, path.string());
}
```

### MsgPack формат

```cpp
#include <glaze/glaze.hpp>

// Сериализация в MsgPack
std::vector<std::byte> serialize_msgpack(const EngineConfig& config) {
    std::vector<std::byte> buffer;
    auto error = glz::write_msgpack(config, buffer);
    
    if (error) {
        std::print(stderr, "MsgPack serialization failed: {}\n", glz::format_error(error));
        return {};
    }
    
    return buffer;
}

// Десериализация из MsgPack
std::expected<EngineConfig, glz::error_code> deserialize_msgpack(std::span<const std::byte> data) {
    EngineConfig config;
    auto error = glz::read_msgpack(config, data);
    
    if (error) {
        return std::unexpected(error);
    }
    
    return config;
}
```

### YAML формат

```cpp
#include <glaze/glaze.hpp>

// Чтение YAML конфигурации
std::expected<EngineConfig, glz::error_code> load_yaml_config(const std::filesystem::path& path) {
    EngineConfig config;
    auto error = glz::read_file_yaml(config, path.string());
    
    if (error) {
        return std::unexpected(error);
    }
    
    return config;
}

// Запись YAML конфигурации
bool save_yaml_config(const EngineConfig& config, const std::filesystem::path& path) {
    return !glz::write_file_yaml(config, path.string(), glz::opts{.prettify = true});
}
```

## Оптимизация производительности

### Compile-time reflection и метаданные

```cpp
#include <glaze/glaze.hpp>
#include <type_traits>

// Проверка наличия метаданных Glaze в compile-time
template<typename T>
concept GlazeSerializable = requires {
    typename T::glaze;
    { T::glaze::value } -> std::convertible_to<decltype(glz::detail::make_object<T>())>;
};

// Compile-time валидация структур
static_assert(GlazeSerializable<WindowConfig>, "WindowConfig must be Glaze-serializable");
static_assert(GlazeSerializable<RenderConfig>, "RenderConfig must be Glaze-serializable");
static_assert(GlazeSerializable<PhysicsConfig>, "PhysicsConfig must be Glaze-serializable");

// Оптимизация: reuse буферов для избежания аллокаций
class SerializationBuffer {
    std::vector<std::byte> buffer_;
    size_t position_ = 0;
    
public:
    void clear() { position_ = 0; }
    
    template<typename T>
    bool serialize(const T& obj) {
        // Проверяем, достаточно ли места в буфере
        constexpr size_t estimated_size = sizeof(T) * 2; // Консервативная оценка
        if (buffer_.size() - position_ < estimated_size) {
            buffer_.resize(buffer_.size() + estimated_size);
        }
        
        auto span = std::span<std::byte>(buffer_.data() + position_, buffer_.size() - position_);
        auto error = glz::write_binary(obj, span);
        
        if (error) {
            return false;
        }
        
        position_ += error.location; // error.location содержит количество записанных байт
        return true;
    }
    
    std::span<const std::byte> data() const {
        return {buffer_.data(), position_};
    }
};

// Оптимизация: thread-local буферы для многопоточности
thread_local SerializationBuffer thread_buffer;

void process_config_in_thread(const EngineConfig& config) {
    thread_buffer.clear();
    thread_buffer.serialize(config);
    
    // Использование сериализованных данных
    auto data = thread_buffer.data();
    // ...
}
```

### Partial сериализация (только измененные поля)

```cpp
#include <glaze/glaze.hpp>
#include <unordered_set>
#include <string_view>

class PartialSerializer {
    std::unordered_set<std::string_view> modified_fields_;
    
public:
    template<typename T>
    void mark_field_modified(std::string_view field_name) {
        modified_fields_.insert(field_name);
    }
    
    template<typename T>
    std::vector<std::byte> serialize_partial(const T& obj) {
        std::vector<std::byte> buffer;
        
        // Создаем кастомный writer, который сериализует только измененные поля
        auto writer = [&](auto&&... args) {
            // Реализация partial сериализации
            // ...
        };
        
        // glz::write_json_custom(obj, writer);
        return buffer;
    }
};

// Использование для инкрементальных обновлений
void update_render_settings(RenderConfig& config, PartialSerializer& serializer) {
    if (config.voxel_size != previous_voxel_size) {
        serializer.mark_field_modified<RenderConfig>("voxel_size");
    }
    
    if (config.enable_gi != previous_enable_gi) {
        serializer.mark_field_modified<RenderConfig>("enable_gi");
    }
    
    // Отправляем только измененные поля по сети
    auto partial_data = serializer.serialize_partial(config);
    // send_partial_update(partial_data);
}
```

## Обработка ошибок и валидация

### Расширенная обработка ошибок

```cpp
#include <glaze/glaze.hpp>
#include <expected>
#include <print>

enum class ConfigLoadError {
    FileNotFound,
    ParseError,
    ValidationError,
    VersionMismatch,
    SchemaViolation
};

template<typename T>
using ConfigResult = std::expected<T, std::pair<ConfigLoadError, std::string>>;

template<typename T>
ConfigResult<T> load_config_with_validation(const std::filesystem::path& path) {
    // Проверка существования файла
    if (!std::filesystem::exists(path)) {
        return std::unexpected({ConfigLoadError::FileNotFound, 
                               fmt::format("File not found: {}", path.string())});
    }
    
    T config{};
    auto error = glz::read_file_json(config, path.string());
    
    if (error) {
        std::string error_msg = glz::format_error(error, path.string());
        error_msg += fmt::format(" at byte {}", error.location);
        
        return std::unexpected({ConfigLoadError::ParseError, error_msg});
    }
    
    // Валидация конфигурации
    if constexpr (requires { validate_config(config); }) {
        if (!validate_config(config)) {
            return std::unexpected({ConfigLoadError::ValidationError,
                                   "Configuration validation failed"});
        }
    }
    
    return config;
}

// Специализированная валидация для RenderConfig
bool validate_config(const RenderConfig& config) {
    std::vector<std::string> errors;
    
    if (config.voxel_size <= 0.0f || config.voxel_size > 10.0f) {
        errors.push_back(fmt::format("Voxel size {} is out of range (0.0-10.0)", 
                                    config.voxel_size));
    }
    
    if (config.svo_max_depth > 16) {
        errors.push_back(fmt::format("SVO depth {} exceeds maximum 16", 
                                    config.svo_max_depth));
    }
    
    if (config.max_ray_steps > 1024) {
        errors.push_back(fmt::format("Ray steps {} exceeds maximum 1024", 
                                    config.max_ray_steps));
    }
    
    if (!errors.empty()) {
        std::print(stderr, "RenderConfig validation errors:\n");
        for (const auto& err : errors) {
            std::print(stderr, "  - {}\n", err);
        }
        return false;
    }
    
    return true;
}
```

### Schema валидация

```cpp
#include <glaze/glaze.hpp>

// Определение JSON Schema для конфигурации
struct ConfigSchema {
    static constexpr std::string_view window_schema = R"({
        "type": "object",
        "properties": {
            "width": {"type": "integer", "minimum": 640, "maximum": 7680},
            "height": {"type": "integer", "minimum": 480, "maximum": 4320},
            "title": {"type": "string", "maxLength": 256},
            "fullscreen": {"type": "boolean"},
            "vsync": {"type": "boolean"},
            "msaa_samples": {"type": "integer", "enum": [1, 2, 4, 8, 16]}
        },
        "required": ["width", "height", "title"]
    })";
    
    static constexpr std::string_view render_schema = R"({
        "type": "object",
        "properties": {
            "voxel_size": {"type": "number", "minimum": 0.001, "maximum": 10.0},
            "svo_max_depth": {"type": "integer", "minimum": 1, "maximum": 16},
            "max_ray_steps": {"type": "integer", "minimum": 16, "maximum": 1024},
            "enable_soft_shadows": {"type": "boolean"},
            "enable_gi": {"type": "boolean"},
            "enable_fog": {"type": "boolean"},
            "ambient_light": {
                "type": "array",
                "items": {"type": "number", "minimum": 0.0, "maximum": 1.0},
                "minItems": 3,
                "maxItems": 3
            },
            "exposure": {"type": "number", "minimum": 0.01, "maximum": 10.0}
        },
        "required": ["voxel_size", "svo_max_depth", "max_ray_steps"]
    })";
};

// Валидация против схемы (упрощенный пример)
bool validate_against_schema(const std::string& json, std::string_view schema) {
    // В реальности используем библиотеку для валидации JSON Schema
    // Например: nlohmann::json_schema::validate()
    return true; // Заглушка
}
```

## Интеграция с системами сборки

### Modern CMake интеграция

```cmake
# Modern CMake target для Glaze
add_library(glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze)

target_include_directories(glaze INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glaze/include
)

# Опции компиляции для C++26
target_compile_features(glaze INTERFACE cxx_std_26)

# Предупреждения и оптимизации
if(MSVC)
    target_compile_options(glaze INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
else()
    target_compile_options(glaze INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

# Интеграция с движком
target_link_libraries(engine_core PRIVATE glaze::glaze)

# Тестирование интеграции
add_executable(test_glaze_integration tests/glaze_integration.cpp)
target_link_libraries(test_glaze_integration PRIVATE engine_core)

# Установка конфигурационных файлов
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/defaults/engine.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/config/engine.json
    @ONLY
)

# Копирование конфигов в бинарную директорию
add_custom_command(
    TARGET engine_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:engine_core>/config
)
```

### Conan интеграция

```python
# conanfile.py для движка с Glaze
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout

class EngineRecipe(ConanFile):
    name = "vulkan-voxel-engine"
    version = "1.0.0"
    
    # Настройки
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    
    def requirements(self):
        self.requires("glaze/1.3.0")
        self.requires("glm/0.9.9.8")
        self.requires("sdl/3.0.0")
        
    def layout(self):
        cmake_layout(self)
        
    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["USE_CONAN"] = True
        tc.variables["BUILD_TESTS"] = True
        tc.generate()
        
    def build(self):
        cmake = CMake(self)

