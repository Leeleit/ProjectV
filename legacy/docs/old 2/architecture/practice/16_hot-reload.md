# Hot-Reload: Шейдеры и Ассеты

**🟡 Уровень 2: Средний** — Быстрая итерация без перезапуска приложения.

---

## Концепция

### Проблема

```
Изменение шейдера → Компиляция SPIR-V (2 сек) → Перезапуск приложения (5 сек) → Тест
```

### Решение

```
Изменение шейдера → Компиляция SPIR-V (2 сек) → Hot-Reload (<0.1 сек)
```

**Результат:** Итерации в 10-50 раз быстрее. Актуально для графических программистов и технических художников.

> **Почему не C++ Hot-Reload:** DLL hot-reload требует сложной архитектуры (Engine/Game split, interface segregation,
> state serialization). Для небольшой команды это overengineering с высоким риском багов. Мы фокусируемся на том, что
> даёт реальную пользу при минимальных затратах.

---

## Часть 1: Hot-Reload шейдеров

### Shader Hot-Reloader

```cpp
#include <filesystem>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <SDL3/SDL_log.h>

class ShaderHotReloader {
public:
    ShaderHotReloader(VkDevice device) : device_(device) {
        watcher_thread_ = std::thread([this]() {
            while (running_) {
                check_for_changes();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }

    ~ShaderHotReloader() {
        running_ = false;
        if (watcher_thread_.joinable()) {
            watcher_thread_.join();
        }
    }

    void register_shader(const std::string& path, VkShaderModule* module) {
        auto last_write = std::filesystem::last_write_time(path);
        shaders_[path] = {module, last_write};
    }

    void register_slang_shader(const std::string& path, VkShaderModule* module) {
        auto last_write = std::filesystem::last_write_time(path);
        slang_shaders_[path] = {module, last_write};
    }

private:
    void check_for_changes() {
        for (auto& [path, info] : shaders_) {
            if (!std::filesystem::exists(path)) continue;

            auto current_write = std::filesystem::last_write_time(path);
            if (current_write != info.last_write) {
                info.last_write = current_write;
                reload_glsl_shader(path, info.module);
                SDL_Log("Shader reloaded: %s", path.c_str());
            }
        }

        for (auto& [path, info] : slang_shaders_) {
            if (!std::filesystem::exists(path)) continue;

            auto current_write = std::filesystem::last_write_time(path);
            if (current_write != info.last_write) {
                info.last_write = current_write;
                reload_slang_shader(path, info.module);
                SDL_Log("Slang shader reloaded: %s", path.c_str());
            }
        }
    }

    void reload_glsl_shader(const std::string& path, VkShaderModule* module) {
        // Компиляция GLSL → SPIR-V
        std::string compile_cmd =
            "glslangValidator -V " + path + " -o " + path + ".spv";
        int result = system(compile_cmd.c_str());

        if (result != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                        "Shader compilation failed: %s", path.c_str());
            return;
        }

        // Пересоздание VkShaderModule
        vkDestroyShaderModule(device_, *module, nullptr);
        *module = create_shader_module(path + ".spv");
    }

    void reload_slang_shader(const std::string& path, VkShaderModule* module) {
        // Компиляция Slang → SPIR-V
        std::string compile_cmd =
            "slangc " + path + " -o " + path + ".spv";
        int result = system(compile_cmd.c_str());

        if (result != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                        "Slang compilation failed: %s", path.c_str());
            return;
        }

        vkDestroyShaderModule(device_, *module, nullptr);
        *module = create_shader_module(path + ".spv");
    }

    VkShaderModule create_shader_module(const std::string& spv_path) {
        std::ifstream file(spv_path, std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        file.seekg(0);

        std::vector<char> code(size);
        file.read(code.data(), size);

        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = size,
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        VkShaderModule module;
        vkCreateShaderModule(device_, &create_info, nullptr, &module);
        return module;
    }

    VkDevice device_;
    std::atomic<bool> running_{true};
    std::thread watcher_thread_;

    struct ShaderInfo {
        VkShaderModule* module;
        std::filesystem::file_time_type last_write;
    };

    std::unordered_map<std::string, ShaderInfo> shaders_;
    std::unordered_map<std::string, ShaderInfo> slang_shaders_;
};
```

### Использование

```cpp
class VoxelRenderer {
    ShaderHotReloader hot_reloader_;
    VkShaderModule vert_shader_;
    VkShaderModule frag_shader_;
    VkPipeline pipeline_;

public:
    VoxelRenderer(VkDevice device) : hot_reloader_(device) {
        // Регистрация шейдеров для hot-reload
        hot_reloader_.register_shader("shaders/voxel.vert", &vert_shader_);
        hot_reloader_.register_shader("shaders/voxel.frag", &frag_shader_);

        // Или Slang шейдеры
        hot_reloader_.register_slang_shader("shaders/voxel.slang", &vert_shader_);
    }

    void rebuild_pipeline() {
        // Вызывается после hot-reload для пересборки pipeline
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = create_pipeline(vert_shader_, frag_shader_);
    }
};
```

### Slang Shader Hot-Reload

ProjectV использует Slang для шейдеров. Преимущества Slang для hot-reload:

| Возможность              | Описание                                       |
|--------------------------|------------------------------------------------|
| **Быстрая компиляция**   | Slang компилирует быстрее GLSL                 |
| **Модульность**          | `#include` и модули для переиспользования кода |
| **Кроссплатформенность** | SPIR-V, HLSL, Metal, WGSL                      |
| **Отладка**              | Лучшие сообщения об ошибках                    |

```glsl
// shaders/voxel.slang
#pragma target "spirv"

#include "common/materials.slang"
#include "common/lighting.slang"

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
PSInput vertexMain(VSInput input) {
    PSInput output;
    output.position = mul(viewProj, float4(input.position, 1.0));
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(PSInput input) : SV_Target {
    Material mat = materials[materialIndex];
    return float4(mat.baseColor * calculateLighting(input.normal), 1.0);
}
```

---

## Часть 2: Hot-Reload Ассетов

### Текстуры

```cpp
class TextureHotReloader {
public:
    void register_texture(const std::string& path, VkImageView* view, VkImage* image) {
        auto last_write = std::filesystem::last_write_time(path);
        textures_[path] = {view, image, last_write};
    }

    void check_for_changes(VkDevice device, VmaAllocator allocator) {
        for (auto& [path, info] : textures_) {
            if (!std::filesystem::exists(path)) continue;

            auto current_write = std::filesystem::last_write_time(path);
            if (current_write != info.last_write) {
                info.last_write = current_write;
                reload_texture(device, allocator, path, info);
                SDL_Log("Texture reloaded: %s", path.c_str());
            }
        }
    }

private:
    void reload_texture(VkDevice device, VmaAllocator allocator,
                       const std::string& path, TextureInfo& info) {
        // Уничтожение старой текстуры
        vkDestroyImageView(device, *info.view, nullptr);
        vmaDestroyImage(allocator, *info.image, info.allocation);

        // Загрузка новой текстуры (через stb_image или аналогичную библиотеку)
        auto [new_image, new_view, new_allocation] = load_texture(device, allocator, path);

        *info.image = new_image;
        *info.view = new_view;
        info.allocation = new_allocation;
    }

    struct TextureInfo {
        VkImageView* view;
        VkImage* image;
        VmaAllocation allocation;
        std::filesystem::file_time_type last_write;
    };

    std::unordered_map<std::string, TextureInfo> textures_;
};
```

### Параметры в JSON

```cpp
#include <nlohmann/json.hpp>

class LiveParameters {
public:
    template<typename T>
    void register_param(const std::string& name, T* ptr) {
        params_[name] = {ptr, typeid(T).hash_code()};
    }

    void load_from_file(const std::string& path) {
        std::ifstream file(path);
        nlohmann::json json;
        file >> json;

        for (auto& [name, info] : params_) {
            if (json.contains(name)) {
                apply_value(name, info, json[name]);
            }
        }
    }

    void watch_and_reload(const std::string& path) {
        if (!std::filesystem::exists(path)) return;

        auto current_write = std::filesystem::last_write_time(path);
        if (current_write != last_write_) {
            last_write_ = current_write;
            load_from_file(path);
            SDL_Log("Parameters reloaded from %s", path.c_str());
        }
    }

private:
    void apply_value(const std::string& name, ParamInfo& info, const nlohmann::json& value) {
        if (info.type_hash == typeid(float).hash_code()) {
            *static_cast<float*>(info.ptr) = value.get<float>();
        } else if (info.type_hash == typeid(int).hash_code()) {
            *static_cast<int*>(info.ptr) = value.get<int>();
        } else if (info.type_hash == typeid(bool).hash_code()) {
            *static_cast<bool*>(info.ptr) = value.get<bool>();
        } else if (info.type_hash == typeid(glm::vec3).hash_code()) {
            auto arr = value.get<std::vector<float>>();
            *static_cast<glm::vec3*>(info.ptr) = glm::vec3(arr[0], arr[1], arr[2]);
        }
    }

    struct ParamInfo {
        void* ptr;
        size_t type_hash;
    };

    std::unordered_map<std::string, ParamInfo> params_;
    std::filesystem::file_time_type last_write_;
};
```

### Пример файла параметров

```json
// config/render_params.json
{
    "render.draw_distance": 150,
    "render.fov": 75.0,
    "render.shadow_quality": 2,
    "physics.gravity": -20.0,
    "player.walk_speed": 5.0,
    "player.jump_force": 8.0,
    "voxel.lod_bias": 1.5
}
```

### Использование

```cpp
// Глобальные параметры (или через ECS)
namespace RenderParams {
    float draw_distance = 100.0f;
    float fov = 70.0f;
}

namespace PhysicsParams {
    float gravity = -9.81f;
}

void setup_live_params() {
    LiveParameters live_params;

    live_params.register_param("render.draw_distance", &RenderParams::draw_distance);
    live_params.register_param("render.fov", &RenderParams::fov);
    live_params.register_param("physics.gravity", &PhysicsParams::gravity);

    // В главном цикле
    while (running) {
        live_params.watch_and_reload("config/render_params.json");
        // ... рендеринг
    }
}
```

---

## Часть 3: Интеграция с Tracy

```cpp
#include <tracy/Tracy.hpp>

void render_voxels(VkCommandBuffer cmd) {
    ZoneScopedN("RenderVoxels");

    // Параметры можно менять через Tracy UI
    static int draw_distance = 100;
    static float lod_bias = 1.0f;

    TracyPlot("DrawDistance", draw_distance);
    TracyPlot("LodBias", lod_bias);

    for (int i = 0; i < draw_distance; ++i) {
        render_chunk_at_distance(i, lod_bias);
    }
}
```

Tracy позволяет менять `static` переменные через UI без перезапуска приложения.

---

## Workflow с Hot-Reload

| Роль                        | Применение                                        |
|-----------------------------|---------------------------------------------------|
| **Графический программист** | Изменяет шейдеры → видит результат мгновенно      |
| **Геймплей программист**    | Итерирует параметры в JSON без пересборки         |
| **Технический художник**    | Обновляет текстуры и материалы в реальном времени |

### Best Practices

1. **Только Debug-сборка:** Hot-reload добавляет overhead, отключайте в Release
2. **Проверка ошибок:** Всегда проверяйте результат компиляции шейдера перед перезагрузкой
3. **Backup pipelines:** Храните старый pipeline до успешной перезагрузки
4. **Файловый watchers:** Используйте OS-native API (inotify на Linux, ReadDirectoryChanges на Windows) для production

---

## Ограничения

### Что НЕ перезагружается

| Тип данных          | Причина                              |
|---------------------|--------------------------------------|
| **C++ код**         | Требует перезапуска приложения       |
| **Pipeline layout** | Требует пересоздания pipeline        |
| **Descriptor sets** | Меняется только при изменении layout |
| **Mesh данные**     | Требует явной перезагрузки           |

### Что перезагружается

| Тип данных         | Механизм                            |
|--------------------|-------------------------------------|
| **SPIR-V шейдеры** | VkShaderModule + pipeline rebuild   |
| **Slang шейдеры**  | slangc → SPIR-V → VkShaderModule    |
| **Текстуры**       | Пересоздание VkImage + VkImageView  |
| **JSON параметры** | Перечитывание файла                 |
| **Tracy plots**    | Runtime изменение static переменных |

---

## Резюме

**Принцип:** Фокусируемся на практичном hot-reload для шейдеров и ассетов. C++ hot-reload — это overengineering для
нашего масштаба.

**Выгода:** Сокращение цикла итерации с минут до секунд для графических программистов и художников.

**Затраты:** Минимальные — один класс `ShaderHotReloader` и один `LiveParameters`.
