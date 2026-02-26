# Интеграция GLM с ProjectV

## CMake

GLM — header-only библиотека, подключается через include:

```cmake
# CMakeLists.txt
# Не требует add_subdirectory или find_package
# Просто добавь в include_directories

target_include_directories(projectv_core PRIVATE external/glm)
```

## Конфигурация для Vulkan

Добавь в главный заголовочный файл:

```cpp
// config.hpp
#ifndef PROJECTV_CONFIG_HPP
#define PROJECTV_CONFIG_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan: [0, 1] вместо [-1, 1]
#define GLM_FORCE_LEFT_HANDED         // Левая система координат (Vulkan)
#define GLM_FORCE_RADIANS             // Все углы в радианах
#define GLM_FORCE_INTRINSICS          // SIMD оптимизации
#define GLM_FORCE_ALIGNED_GENTYPES    // Выровненные типы для SIMD

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#endif
```

## Интеграция с Vulkan

### Передача матриц в шейдер

```cpp
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

// Структура для uniform буфера (соответствует шейдеру)
struct SceneUniforms {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 lightDir;
    alignas(16) glm::vec4 cameraPos;
};

struct ObjectUniforms {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 normalMatrix;  // transpose(inverse(model))
};

// Создание буфера
VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = sizeof(SceneUniforms),
    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
};
```

### Вычисление normalMatrix

```cpp
glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
                * glm::rotate(glm::mat4(1.0f), angle, axis)
                * glm::scale(glm::mat4(1.0f), scale);

glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));
```

### Создание матриц камеры

```cpp
class Camera {
public:
    Camera(glm::vec3 position, glm::vec3 target, glm::vec3 up)
        : position_(position) {
        view_ = glm::lookAt(position_, target, up);
    }

    void setPerspective(float fov, float aspect, float near, float far) {
        proj_ = glm::perspective(glm::radians(fov), aspect, near, far);

        // Для Vulkan — flip Y
        proj_[1][1] *= -1;
    }

    const glm::mat4& view() const { return view_; }
    const glm::mat4& proj() const { return proj_; }

private:
    glm::mat4 view_;
    glm::mat4 proj_;
    glm::vec3 position_;
};
```

## Интеграция с Flecs ECS

```cpp
#include <flecs.h>
#include <glm/glm.hpp>

struct Transform {
    glm::vec3 position;
    glm::vec3 rotation;  // Euler angles
    glm::vec3 scale;
};

struct LocalToWorld {
    glm::mat4 matrix;
};

// Система обновления матриц
class TransformSystem {
public:
    static void update(flecs::world& world) {
        world.system<Transform, LocalToWorld>()
            .each([](Transform& local, LocalToWorld& ltw) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, local.position);
                model = glm::rotate(model, local.rotation.x, glm::vec3(1, 0, 0));
                model = glm::rotate(model, local.rotation.y, glm::vec3(0, 1, 0));
                model = glm::rotate(model, local.rotation.z, glm::vec3(0, 0, 1));
                model = glm::scale(model, local.scale);

                ltw.matrix = model;
            });
    }
};
```

## Интеграция с SDL3

```cpp
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class SdlInputCamera {
public:
    void handleEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.state & SDL_BUTTON_MMASK) {
                    // Вращение камеры
                    yaw_ += event.motion.xrel * sensitivity_;
                    pitch_ -= event.motion.yrel * sensitivity_;
                    pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_W) forward_ = true;
                if (event.key.key == SDLK_S) backward_ = true;
                if (event.key.key == SDLK_A) left_ = true;
                if (event.key.key == SDLK_D) right_ = true;
                break;

            case SDL_EVENT_KEY_UP:
                if (event.key.key == SDLK_W) forward_ = false;
                if (event.key.key == SDLK_S) backward_ = false;
                if (event.key.key == SDLK_A) left_ = false;
                if (event.key.key == SDLK_D) right_ = false;
                break;
        }
    }

    glm::vec3 getForward() const {
        return glm::normalize(glm::vec3(
            glm::cos(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_)),
            glm::sin(glm::radians(pitch_)),
            glm::sin(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_))
        ));
    }

private:
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float sensitivity_ = 0.1f;
    bool forward_ = false, backward_ = false;
    bool left_ = false, right_ = false;
};
```

## Интеграция с JoltPhysics

```cpp
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>

// Конвертация Jolt <-> GLM
namespace JoltGLM {
    inline glm::vec3 toGLM(JPH::Vec3 v) {
        return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
    }

    inline JPH::Vec3 toJolt(glm::vec3 v) {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    inline glm::quat toGLM(JPH::Quat q) {
        return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }

    inline JPH::Quat toJolt(glm::quat q) {
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }
}
```
