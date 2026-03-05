# Glaze — Integration

Подключение Glaze к ProjectV, базовые паттерны и каноничные структуры конфигурации.

---

## CMake Integration

### Вариант 1: Git Submodule

```cmake
# Add as git submodule
add_subdirectory(external/glaze)

target_link_libraries(ProjectV PRIVATE glaze::glaze)
```

### Вариант 2: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  glaze
  # glaze repository configuration
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(ProjectV PRIVATE glaze::glaze)
```

### Вариант 3: vcpkg / conan

```cmake
# vcpkg.txt: glaze
find_package(glaze REQUIRED)
target_link_libraries(ProjectV PRIVATE glaze::glaze)
```

---

## Каноничные структуры ProjectV

### WindowConfig — настройки окна

```cpp
#include <glaze/glaze.hpp>
#include <string>

struct WindowConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    std::string title = "ProjectV";
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
#include <stdexec/execution.hpp>
#include <expected>

class ConfigManager {
    // Double-buffered конфиг (lock-free подход)
    alignas(64) EngineConfig configs_[2];
    std::atomic<size_t> current_index_{0};
    std::atomic<bool> config_updated_{false};
    std::filesystem::file_time_type last_write_time_;
    std::filesystem::path config_path_;
    stdexec::scheduler auto scheduler_;

public:
    ConfigManager(std::filesystem::path path, stdexec::scheduler auto scheduler = stdexec::get_default_scheduler())
        : config_path_(std::move(path)), scheduler_(scheduler) {

        // Загружаем начальный конфиг
        if (auto config = load_config<EngineConfig>(config_path_)) {
            configs_[0] = *config;
            last_write_time_ = std::filesystem::last_write_time(config_path_);
        }

        // Запускаем мониторинг через stdexec
        start_monitoring();
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
        // Создаем периодическую задачу через stdexec
        auto monitoring_task = stdexec::schedule(scheduler_)
                             | stdexec::then([this]() -> std::expected<EngineConfig, std::string> {
                                   return check_for_updates();
                               })
                             | stdexec::then([this](std::expected<EngineConfig, std::string> result) {
                                   if (result) {
                                       // Успешный парсинг — сохраняем в буфер
                                       size_t old_index = current_index_.load(std::memory_order_relaxed);
                                       size_t new_index = 1 - old_index;
                                       configs_[new_index] = std::move(*result);
                                       config_updated_.store(true, std::memory_order_release);
                                   }
                                   return result;
                               });

        // Запускаем периодически (каждые 500ms)
        auto periodic_task = stdexec::schedule(scheduler_)
                           | stdexec::then([this, task = std::move(monitoring_task)]() mutable {
                                 // Используем stdexec для периодического выполнения
                                 return stdexec::schedule_after(scheduler_, std::chrono::milliseconds(500))
                                      | stdexec::then([task = std::move(task)]() mutable {
                                            // Перезапускаем задачу
                                            return task;
                                        });
                             });

        // Запускаем асинхронно
        stdexec::start_detached(std::move(periodic_task));
    }

    std::expected<EngineConfig, std::string> check_for_updates() {
        // Проверяем существование файла без исключений
        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(config_path_, ec);
        
        if (ec) {
            return std::unexpected(std::string("Filesystem error: ") + ec.message());
        }

        if (current_time > last_write_time_) {
            last_write_time_ = current_time;

            // Парсим конфиг
            EngineConfig new_config;
            auto error = glz::read_file_json(new_config, config_path_.string());

            if (!error) {
                return new_config;
            } else {
                return std::unexpected(glz::format_error(error, config_path_.string()));
            }
        }

        return std::unexpected("No update");
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

## Лучшие практики для ProjectV

### 1. Разделение конфигов по ответственности

```
config/
├── engine.json          # EngineConfig (корневой)
├── render.json          # RenderConfig (отдельно для hot reload)
├── physics.json         # PhysicsConfig
├── audio.json          # AudioConfig
├── materials/          # Библиотеки материалов
│   ├── basic.json
│   └── special.json
└── worlds/             # Конфиги миров
    ├── default.json
    └── tutorial.json
```

### 2. Валидация после загрузки

```cpp
bool validate_render_config(const RenderConfig& config) {
    if (config.voxel_size <= 0.0f) {
        std::print(stderr, "Invalid voxel size: {}\n", config.voxel_size);
        return false;
    }

    if (config.svo_max_depth > 16) {
        std::print(stderr, "SVО depth too high: {} (max 16)\n", config.svo_max_depth);
        return false;
    }

    if (config.max_ray_steps > 1024) {
        std::print(stderr, "Ray steps too high: {} (max 1024)\n", config.max_ray_steps);
        return false;
    }

    return true;
}
```

### 3. Миграция версий конфигов

```cpp
struct ConfigVersion {
    uint32_t major = 1;
    uint32_t minor = 0;
    uint32_t patch = 0;

    struct glaze {
        using T = ConfigVersion;
        static constexpr auto value = glz::object(
            "major", &T::major,
            "minor", &T::minor,
            "patch", &T::patch
        );
    };
};

bool migrate_config(EngineConfig& config, const ConfigVersion& loaded_version) {
    ConfigVersion current_version{1, 1, 0};

    if (loaded_version.major < current_version.major) {
        // Major version migration
        std::print("Migrating config from {}.{}.{} to {}.{}.{}\n",
                   loaded_version.major, loaded_version.minor, loaded_version.patch,
                   current_version.major, current_version.minor, current_version.patch);

        // Пример: добавление новых полей
        if (loaded_version.major == 1 && loaded_version.minor == 0) {
            // Например, добавили новое поле в 1.1, которого не было в 1.0
            config.render.enable_gi = false;
            std::println("Migrated from 1.0 to 1.1: enable_gi set to false");
        }
    }
    return true;
}
