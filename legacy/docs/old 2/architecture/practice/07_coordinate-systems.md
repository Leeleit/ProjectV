# Спецификация систем координат ProjectV

**Статус:** Утверждено
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-22
**Версия:** 3.0 (Enterprise)

---

## Проблема

ProjectV использует несколько библиотек с разными соглашениями о системах координат:

| Библиотека               | Система координат  | Y-ось | Z-ось    | Примечания                   |
|--------------------------|--------------------|-------|----------|------------------------------|
| **JoltPhysics**          | Right-handed       | Up    | Forward  | Y-Up, Z-Forward              |
| **Vulkan**               | Right-handed       | Down  | Forward  | Y-Down, Z-Forward (NDC)      |
| **GLM**                  | Right-handed       | Up    | Forward  | По умолчанию Y-Up            |
| **SDL3**                 | Screen coordinates | Down  | N/A      | Y-Down для оконных координат |
| **OpenGL** (исторически) | Right-handed       | Up    | Backward | Y-Up, Z-Backward             |

Эта разница приводит к путанице при передаче данных между системами и требует явных преобразований.

## Основные концепции

### 1. Right-handed vs Left-handed системы

**Right-handed (правая) система:**

- X: вправо
- Y: вверх (или вниз в Vulkan)
- Z: вперед (или назад в OpenGL)

**Правило правой руки:** Укажите пальцы по X, согните по Y, большой палец показывает Z.

### 2. Преобразования между системами

#### JoltPhysics (Y-Up) → Vulkan (Y-Down)

```cpp
// Матрица преобразования из JoltPhysics в Vulkan
glm::mat4 jolt_to_vulkan = glm::mat4(
    1.0f,  0.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f,  0.0f,  // Инвертируем Y
    0.0f,  0.0f,  1.0f,  0.0f,
    0.0f,  0.0f,  0.0f,  1.0f
);

// Или через scale
glm::mat4 jolt_to_vulkan_scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
```

#### Vulkan (Y-Down) → JoltPhysics (Y-Up)

```cpp
// Обратное преобразование
glm::mat4 vulkan_to_jolt = glm::mat4(
    1.0f,  0.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f,  0.0f,  // Снова инвертируем Y
    0.0f,  0.0f,  1.0f,  0.0f,
    0.0f,  0.0f,  0.0f,  1.0f
);
```

### 3. Нормализованное координатное пространство (NDC)

**Vulkan NDC:**

- X: [-1, 1] (лево → право)
- Y: [-1, 1] (верх → низ) ← **обратите внимание: Y-down!**
- Z: [0, 1] (близко → далеко)

**OpenGL NDC (для сравнения):**

- X: [-1, 1] (лево → право)
- Y: [-1, 1] (низ → верх) ← Y-up!
- Z: [-1, 1] (близко → далеко)

## Практические примеры для ProjectV

### Пример 1: Создание матрицы проекции

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Для Vulkan (Y-down)
glm::mat4 create_vulkan_projection(float fov, float aspect, float near, float far) {
    // GLM по умолчанию создаёт OpenGL-совместимую проекцию (Y-up)
    glm::mat4 proj = glm::perspective(fov, aspect, near, far);

    // Конвертируем в Vulkan NDC (Y-down, Z [0,1])
    glm::mat4 vulkan_correction = glm::mat4(
        1.0f,  0.0f,  0.0f,  0.0f,
        0.0f, -1.0f,  0.0f,  0.0f,  // Инвертируем Y
        0.0f,  0.0f,  0.5f,  0.0f,  // Масштабируем Z к [0,1]
        0.0f,  0.0f,  0.5f,  1.0f   // Сдвигаем Z
    );

    return vulkan_correction * proj;
}

// Альтернативно: используем glm::perspective с reversed-Z
glm::mat4 create_vulkan_projection_revz(float fov, float aspect, float near, float far) {
    return glm::perspective(fov, aspect, far, near);  // reversed-Z для лучшей точности
}
```

### Пример 2: Преобразование позиций между системами

```cpp
#include <glm/glm.hpp>

// Конвертация позиции из JoltPhysics в Vulkan
glm::vec3 jolt_to_vulkan_position(const glm::vec3& jolt_pos) {
    return glm::vec3(jolt_pos.x, -jolt_pos.y, jolt_pos.z);
}

// Конвертация позиции из Vulkan в JoltPhysics
glm::vec3 vulkan_to_jolt_position(const glm::vec3& vulkan_pos) {
    return glm::vec3(vulkan_pos.x, -vulkan_pos.y, vulkan_pos.z);
}

// Конвертация вращения (кватерниона)
glm::quat jolt_to_vulkan_rotation(const glm::quat& jolt_rot) {
    // Для инверсии Y оси, вращаем на 180 градусов вокруг X
    glm::quat y_flip = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    return y_flip * jolt_rot * glm::conjugate(y_flip);
}
```

### Пример 3: Интеграция с JoltPhysics

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class PhysicsVulkanBridge {
public:
    // Получение трансформации тела для Vulkan рендеринга
    glm::mat4 get_body_transform_for_vulkan(JPH::BodyID body_id) {
        JPH::BodyInterface& interface = physics_system.GetBodyInterface();

        // Получаем позицию и вращение из JoltPhysics
        JPH::RVec3 jolt_pos = interface.GetCenterOfMassPosition(body_id);
        JPH::Quat jolt_rot = interface.GetRotation(body_id);

        // Конвертируем в glm
        glm::vec3 position(jolt_pos.GetX(), jolt_pos.GetY(), jolt_pos.GetZ());
        glm::quat rotation(jolt_rot.GetW(), jolt_rot.GetX(), jolt_rot.GetY(), jolt_rot.GetZ());

        // Создаём матрицу трансформации
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = transform * glm::mat4_cast(rotation);

        // Применяем коррекцию для Vulkan (инверсия Y)
        glm::mat4 vulkan_correction = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));

        return vulkan_correction * transform;
    }

private:
    JPH::PhysicsSystem& physics_system;
};
```

## Коррекция в шейдерах

### Вершинный шейдер с коррекцией координат

```glsl
#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float time;
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Стандартное преобразование
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    vec4 viewPos = ubo.view * worldPos;
    vec4 clipPos = ubo.proj * viewPos;

    // GLM уже создал проекцию с Vulkan коррекцией,
    // поэтому дополнительная коррекция не нужна

    gl_Position = clipPos;

    // Передача данных во фрагментный шейдер
    fragPos = worldPos.xyz;
    fragNormal = normalize(mat3(transpose(inverse(ubo.model))) * inNormal);
    fragTexCoord = inTexCoord;
}
```

### Compute шейдер для генерации геометрии с учётом систем координат

```glsl
#version 460
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

// Входные данные вокселей (в системе координат мира)
layout(std430, binding = 0) readonly buffer VoxelData {
    uint voxels[];
};

// Выходные вершины (в системе координат Vulkan)
layout(std430, binding = 1) writeonly buffer VertexBuffer {
    vec4 vertices[];
};

// Параметры чанка
layout(std430, binding = 2) uniform ChunkParams {
    mat4 chunkTransform;    // Трансформация чанка (уже с Vulkan коррекцией)
    ivec3 chunkGridPos;     // Позиция чанка в сетке
    uint chunkSize;         // Размер чанка (обычно 16 или 32)
    float voxelSize;        // Размер вокселя
};

// Генерация вершин куба с правильной ориентацией
void generate_cube_vertices(vec3 local_pos, uint voxel_type) {
    // Вершины куба в локальных координатах (Y-up)
    const vec3 cube_vertices[8] = vec3[8](
        vec3(-0.5, -0.5, -0.5), vec3( 0.5, -0.5, -0.5),
        vec3( 0.5,  0.5, -0.5), vec3(-0.5,  0.5, -0.5),
        vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5),
        vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5)
    );

    // Применяем трансформацию чанка (уже включает Vulkan коррекцию)
    for (int i = 0; i < 8; i++) {
        vec4 world_pos = chunkTransform * vec4(local_pos + cube_vertices[i], 1.0);
        // Сохраняем с типом вокселя в w-компоненте
        vertices[atomicAdd(vertex_counter, 1)] = vec4(world_pos.xyz, float(voxel_type));
    }
}

void main() {
    ivec3 voxel_coord = ivec3(gl_GlobalInvocationID);

    if (voxel_coord.x >= chunkSize || voxel_coord.y >= chunkSize || voxel_coord.z >= chunkSize) {
        return;
    }

    uint voxel_index = voxel_coord.x + voxel_coord.y * chunkSize + voxel_coord.z * chunkSize * chunkSize;
    uint voxel_type = voxels[voxel_index];

    if (voxel_type != 0) {  // 0 = воздух
        vec3 local_pos = vec3(voxel_coord) * voxelSize;
        generate_cube_vertices(local_pos, voxel_type);
    }
}
```

## Таблицы преобразований

### Матрицы преобразования между системами

| Преобразование            | Матрица                                          | Описание                      |
|---------------------------|--------------------------------------------------|-------------------------------|
| **Jolt → Vulkan**         | `[[1,0,0,0],[0,-1,0,0],[0,0,1,0],[0,0,0,1]]`     | Инверсия Y оси                |
| **Vulkan → Jolt**         | `[[1,0,0,0],[0,-1,0,0],[0,0,1,0],[0,0,0,1]]`     | Та же матрица (инволюция)     |
| **OpenGL → Vulkan**       | `[[1,0,0,0],[0,-1,0,0],[0,0,0.5,0.5],[0,0,0,1]]` | Инверсия Y + преобразование Z |
| **World → View (Vulkan)** | Использовать `glm::lookAt` с `up = {0, -1, 0}`   | Камера смотрит вниз по Y      |

### Векторы "up" для разных систем

| Система                | Вектор "up"  | Вектор "forward" | Вектор "right" |
|------------------------|--------------|------------------|----------------|
| **JoltPhysics**        | `{0, 1, 0}`  | `{0, 0, 1}`      | `{1, 0, 0}`    |
| **Vulkan**             | `{0, -1, 0}` | `{0, 0, 1}`      | `{1, 0, 0}`    |
| **GLM (по умолчанию)** | `{0, 1, 0}`  | `{0, 0, -1}`     | `{1, 0, 0}`    |
| **OpenGL**             | `{0, 1, 0}`  | `{0, 0, -1}`     | `{1, 0, 0}`    |

---

## Winding Order и Front Face

### Критически важная проблема

При инверсии Y-оси (преобразование из JoltPhysics Y-Up в Vulkan Y-Down) происходит важный побочный эффект:
**треугольники "выворачиваются" (change winding order).**

**Почему это происходит:**

```
Исходный треугольник (Y-Up):       После Y-инверсии (Y-Down):
     A (top)                           A (bottom)
    /\                                /\
   /  \                              /  \
  B----C                            B----C

Winding: A→B→C = Counter-Clockwise   Winding: A→B→C = Clockwise
Front Face: CCW                      Front Face: CW (инвертирован!)
```

**Визуально:** При Y-flip вершина A перемещается сверху вниз. Порядок вершин A→B→C остаётся тем же, но
относительно наблюдателя этот порядок меняется с CCW на CW.

### Решение в Vulkan

```cpp
VkPipelineRasterizationStateCreateInfo rasterizationInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,  // <-- КРИТИЧЕСКИ ВАЖНО!
    .depthBiasEnable = VK_FALSE,
    .lineWidth = 1.0f
};
```

### Проверка в коде

```cpp
// Утилитарная функция для определения frontFace
VkFrontFace determine_front_face(bool y_flipped) {
    // Если Y-ось инвертирована (JoltPhysics → Vulkan), нужно использовать CW
    // Если Y-ось не инвертирована (OpenGL-стиль), используем CCW
    return y_flipped ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

// Пример использования при создании pipeline
void create_vulkan_pipeline(VkDevice device, bool useYFlip) {
    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {
        .frontFace = determine_front_face(useYFlip),
        // ... остальные параметры
    };
    // ... создание pipeline
}
```

### Распространённые ошибки

| Симптом                                 | Причина                                       | Решение                                             |
|-----------------------------------------|-----------------------------------------------|-----------------------------------------------------|
| Объекты рендерятся "наизнанку"          | Winding order не соответствует Y-flip         | Установите `frontFace = VK_FRONT_FACE_CLOCKWISE`    |
| Backface culling отсекает лицевые грани | Противоположный winding order                 | Проверьте соответствие `frontFace` и Y-flip         |
| Модели из glTF отображаются некорректно | glTF использует CCW, Vulkan NDC инвертирует Y | Добавьте Y-flip в projection matrix И установите CW |

### Интеграция с glTF

glTF использует правую систему координат с Y-Up и CCW winding order. При загрузке в Vulkan:

```cpp
// Коррекция projection matrix для Vulkan
glm::mat4 projection = glm::perspective(fov, aspect, near, far);

// Vulkan NDC: Y направлен вниз
glm::mat4 vulkanCorrection = glm::mat4(
    1.0f,  0.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f,  0.0f,  // Y-flip
    0.0f,  0.0f,  0.5f,  0.0f,
    0.0f,  0.0f,  0.5f,  1.0f
);

projection = vulkanCorrection * projection;

// И установите frontFace = VK_FRONT_FACE_CLOCKWISE в pipeline!
```

> **Важно:** Y-flip в projection matrix и `frontFace = VK_FRONT_FACE_CLOCKWISE` должны использоваться **вместе**.
> Использование только одного из них приведёт к некорректному рендерингу.

---

## Рекомендации для ProjectV

### 1. Выберите основную систему координат

**Рекомендация:** Используйте **JoltPhysics (Y-Up)** как основную систему для:

- Хранения позиций сущностей
- Физической симуляции
- Игровой логики

**Vulkan (Y-Down)** использовать только для:

- Финального рендеринга
- Проекционных матриц
- Координат экрана

### 2. Создайте слой преобразования

```cpp
namespace CoordinateSystems {
    // Конвертация для рендеринга
    glm::mat4 world_to_vulkan(const glm::mat4& world_transform) {
        static const glm::mat4 y_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        return y_flip * world_transform;
    }

    // Конвертация для физики
    glm::mat4 vulkan_to_world(const glm::mat4& vulkan_transform) {
        // Та же матрица (инволюция)
        static const glm::mat4 y_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        return y_flip * vulkan_transform;
    }

    // Создание Vulkan-совместимой проекции
    glm::mat4 vulkan_projection(float fov, float aspect, float near, float far) {
        glm::mat4 proj = glm::perspective(fov, aspect, near, far);
        glm::mat4 vulkan_corr = glm::mat4(
            1.0f,  0.0f,  0.0f,  0.0f,
            0.0f, -1.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.5f,  0.0f,
            0.0f,  0.0f,  0.5f,  1.0f
        );
        return vulkan_corr * proj;
    }
}
```

### 3. Документируйте преобразования

Всегда документируйте, в какой системе координат находятся данные:

```cpp
struct Transform {
    glm::vec3 position;     // В системе JoltPhysics (Y-Up)
    glm::quat rotation;     // В системе JoltPhysics (Y-Up)
    glm::vec3 scale;        // Безразмерно

    // Получить матрицу для Vulkan рендеринга
    glm::mat4 get_vulkan_matrix() const {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, position);
        m = m * glm::mat4_cast(rotation);
        m = glm::scale(m, scale);

        // Применяем коррекцию для Vulkan
        static const glm::mat4 y_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        return y_flip * m;
    }
};

### 4. Тестирование преобразований

Создайте unit tests для проверки преобразований:

```cpp
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

TEST(CoordinateSystems, JoltToVulkanRoundTrip) {
    glm::vec3 original(1.0f, 2.0f, 3.0f);

    // Jolt → Vulkan
    glm::vec3 vulkan = CoordinateSystems::jolt_to_vulkan_position(original);
    EXPECT_FLOAT_EQ(vulkan.x, 1.0f);
    EXPECT_FLOAT_EQ(vulkan.y, -2.0f);  // Y инвертирован
    EXPECT_FLOAT_EQ(vulkan.z, 3.0f);

    // Vulkan → Jolt (обратное преобразование)
    glm::vec3 roundtrip = CoordinateSystems::vulkan_to_jolt_position(vulkan);
    EXPECT_FLOAT_EQ(roundtrip.x, original.x);
    EXPECT_FLOAT_EQ(roundtrip.y, original.y);
    EXPECT_FLOAT_EQ(roundtrip.z, original.z);
}

TEST(CoordinateSystems, MatrixTransformConsistency) {
    glm::mat4 jolt_transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));

    // Преобразование через матрицу
    glm::mat4 vulkan_transform = CoordinateSystems::world_to_vulkan(jolt_transform);

    // Проверяем, что позиция корректно преобразована
    glm::vec4 jolt_pos = jolt_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 vulkan_pos = vulkan_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(vulkan_pos.x, jolt_pos.x);
    EXPECT_FLOAT_EQ(vulkan_pos.y, -jolt_pos.y);  // Y инвертирован
    EXPECT_FLOAT_EQ(vulkan_pos.z, jolt_pos.z);
}
```

## Диаграммы переходов

### Визуализация систем координат

```
JoltPhysics (Y-Up)        Vulkan (Y-Down)         Screen (SDL)
      ↑                         ↓                       ↓
     Y+                        Y-                      Y+
      |                         |                       |
      ·----→ X+           ·----→ X+              ·----→ X+
     /                    /                      /
    Z+                   Z+
```

### Поток преобразований в ProjectV

```
[Game Logic] → [JoltPhysics] → [Coordinate Conversion] → [Vulkan Renderer] → [Screen]
     ↑              ↑                  ↓                      ↓               ↓
  Y-Up World    Y-Up Physics      Y-flip Matrix         Y-Down NDC       Y-Down Pixels
```

## Распространённые ошибки и решения

### Ошибка 1: Объекты отображаются вверх ногами

**Симптом:** Модели рендерятся перевёрнутыми по Y.
**Причина:** Не применена коррекция Y-flip для Vulkan.
**Решение:** Добавьте `y_flip` матрицу перед отправкой в Vulkan.

### Ошибка 2: Физика не совпадает с рендерингом

**Симптом:** Объекты физики и рендеринга находятся в разных местах.
**Причина:** Используются разные системы координат без преобразования.
**Решение:** Создайте единый источник истины (JoltPhysics) и преобразуйте для рендеринга.

### Ошибка 3: Некорректное отсечение

**Симптом:** Объекты отсекаются слишком рано или поздно.
**Причина:** Неправильная проекционная матрица для Vulkan NDC.
**Решение:** Используйте `create_vulkan_projection()` из примеров выше.

## Интеграция с другими компонентами ProjectV

### Flecs ECS компоненты

```cpp
// Компонент трансформации в системе координат JoltPhysics
struct Transform {
    glm::vec3 position;  // Y-Up
    glm::quat rotation;  // Y-Up
    glm::vec3 scale;
};

// Система рендеринга преобразует в Vulkan
system<const Transform, const MeshRenderer>("RenderSystem")
    .each( {
        glm::mat4 vulkan_matrix = transform.get_vulkan_matrix();
        // Отправка в Vulkan...
    });
```

### Tracy профилирование

Добавьте трассировку преобразований:

```cpp
void update_transforms() {
    ZoneScopedN("CoordinateTransforms");

    {
        ZoneScopedN("JoltToVulkan");
        // Преобразование всех трансформаций
        for (auto& transform : transforms) {
            vulkan_transforms.push_back(transform.get_vulkan_matrix());
        }
    }

    TracyPlot("TransformCount", transforms.size());
}
```

## Заключение

Работа с системами координат в ProjectV требует внимательности, но следуя этим рекомендациям, вы сможете:

1. **Чётко разделять системы координат** между компонентами
2. **Создавать корректные преобразования** с минимальным overhead
3. **Избегать распространённых ошибок** через unit testing
4. **Интегрировать с экосистемой ProjectV** (Flecs, Tracy, JoltPhysics)

**Ключевые принципы:**

- Используйте JoltPhysics (Y-Up) как основную систему
- Применяйте Y-flip только при рендеринге в Vulkan
- Документируйте систему координат для всех данных
- Тестируйте преобразования на round-trip consistency

---

## Обзор

ProjectV использует **Floating Origin** архитектуру для поддержки гигантских миров (64+ км) с точностью до миллиметра.

### Ключевые решения

| Решение                | Обоснование                                   |
|------------------------|-----------------------------------------------|
| **Floating Origin**    | Камера всегда в (0,0,0), мир двигается вокруг |
| **JoltPhysics Y-Up**   | Основная система для игровой логики           |
| **Vulkan Y-Down**      | Только для финального рендеринга              |
| **SVO max_depth = 16** | 2^16 = 65536³ вокселей = 64км³ при 1м/воксель |

---

## Floating Origin Architecture

### Проблема больших миров

```
IEEE 754 Single Precision (float):
- 23 бита мантиссы
- Точность: ~1/2^23 ≈ 0.000000119

При позиции X = 100,000 (100 км от центра):
- Точность: 100,000 × 0.000000119 ≈ 0.012 метров
- На 1000 км: точность ≈ 0.12 метров (12 см!)

Результат: Z-fighting, jitter, некорректная физика
```

### Решение: Floating Origin

```
Камера всегда в (0, 0, 0)
Мир сдвигается относительно камеры

Frame N:                          Frame N+1:
Камера в (0, 0, 0)                Камера в (0, 0, 0)
Объект A в (100, 50, 200)         Объект A в (95, 50, 195)
Origin offset: (0, 0, 0)          Origin offset: (5, 0, 5)

Физическая позиция объекта (абсолютная):
(5, 50, 5) + (95, 50, 195) = (100, 50, 200) — неизменна
```

---

## Memory Layout

### WorldCoordinate

```
WorldCoordinate (16 bytes, 128-bit)
┌─────────────────────────────────────────────────────────────┐
│  sector_x: int32_t (4 bytes)    — сектор мира X             │
│  sector_y: int32_t (4 bytes)    — сектор мира Y             │
│  sector_z: int32_t (4 bytes)    — сектор мира Z             │
│  local_pos: glm::vec3 (12 bytes) — позиция внутри сектора   │
│  padding: 4 bytes                                            │
│  Total: 28 bytes                                             │
└─────────────────────────────────────────────────────────────┘

Sector Size = 4096 метров (relocatable unit)
Sector Coordinates: int32_t (±2^15 секторов = ±134 миллионов км)
```

### FloatingOrigin

```
FloatingOrigin (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── current_sector_: SectorCoord (12 bytes)            │
│      ├── local_offset_: glm::vec3 (12 bytes)                │
│      ├── accumulated_offset_: glm::dvec3 (24 bytes)         │
│      ├── threshold_: float (4 bytes) = 1024.0f              │
│      └── on_origin_shift_: callback (32 bytes)              │
│  Total: 8 bytes (external) + ~100 bytes (internal)          │
└─────────────────────────────────────────────────────────────┘
```

### Transform (GPU-aligned)

```
TransformComponent (64 bytes, GPU-aligned)
┌─────────────────────────────────────────────────────────────┐
│  position: glm::vec3 (12 bytes)    — локальная позиция      │
│  padding0: 4 bytes                                           │
│  rotation: glm::quat (16 bytes)    — вращение               │
│  scale: glm::vec3 (12 bytes)       — масштаб                │
│  sector_delta: int32_t[3] (12 bytes) — смещение сектора     │
│  padding1: 4 bytes                                           │
│  Total: 64 bytes (cache-line friendly)                      │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machines

### Origin Shift State

```
Origin Shift Decision

    ┌──────────────────┐
    │  CHECK_POSITION  │ ←── Каждый кадр
    └────────┬─────────┘
             │ |local_pos| > threshold?
             │ YES
             ▼
    ┌──────────────────┐
    │   CALCULATE_NEW  │
    │     ORIGIN       │
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ SHIFT_ALL_ENTITIES│ ←── Вычесть offset из всех позиций
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ UPDATE_SECTOR    │ ←── Обновить current_sector_
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ NOTIFY_SYSTEMS   │ ←── Callback для физики, рендера
    └──────────────────┘
```

### Sector Loading State

```
Sector Lifecycle

    ┌─────────────┐
    │   UNLOADED  │ ←── Сектор не существует
    └──────┬──────┘
           │ player enters load radius
           ▼
    ┌─────────────┐
    │   LOADING   │ ←── Генерация/загрузка данных
    └──────┬──────┘
           │ data ready
           ▼
    ┌─────────────┐
    │   ACTIVE    │ ←── Сектор полностью загружен
    └──────┬──────┘
           │ player exits unload radius
           ▼
    ┌─────────────┐
    │  UNLOADING  │ ←── Сохранение/освобождение
    └──────┬──────┘
           │ cleanup complete
           ▼
    ┌─────────────┐
    │   UNLOADED  │ ←── Сектор удалён из памяти
    └─────────────┘
```

---

## API Contracts

### WorldCoordinate

```cpp
// ProjectV.World.Coordinate.cppm
export module ProjectV.World.Coordinate;

import std;
import glm;

export namespace projectv::world {

/// Размер сектора в метрах.
export constexpr float SECTOR_SIZE = 4096.0f;

/// Порог сдвига origin.
export constexpr float ORIGIN_SHIFT_THRESHOLD = 1024.0f;

/// Максимальная глубина SVO.
export constexpr uint32_t SVO_MAX_DEPTH = 16;

/// Координаты сектора.
export struct SectorCoord {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};

    /// Хеш для unordered_map.
    [[nodiscard]] auto hash() const noexcept -> size_t;

    /// Сравнение.
    [[nodiscard]] auto operator==(SectorCoord const&) const noexcept -> bool = default;
    [[nodiscard]] auto operator<=>(SectorCoord const&) const noexcept = default;

    /// Конвертация в мировые координаты.
    [[nodiscard]] auto to_world_position() const noexcept -> glm::dvec3;

    /// Из мировых координат.
    [[nodiscard]] static auto from_world_position(glm::dvec3 const& pos) noexcept -> SectorCoord;
};

/// Мировые координаты с двойной точностью.
///
/// ## Precision
/// - Сектор: int32_t → ±2^15 секторов = ±134M км
/// - Локальная позиция: float → точность < 1мм при SECTOR_SIZE=4096
///
/// ## Invariants
/// - local_position всегда в пределах [-SECTOR_SIZE/2, SECTOR_SIZE/2]
export struct WorldCoordinate {
    SectorCoord sector;
    glm::vec3 local_position{0.0f};

    /// Получает позицию как double (абсолютную).
    [[nodiscard]] auto to_absolute() const noexcept -> glm::dvec3;

    /// Создаёт из абсолютной позиции.
    [[nodiscard]] static auto from_absolute(glm::dvec3 const& pos) noexcept -> WorldCoordinate;

    /// Вычисляет относительную позицию к другому сектору.
    [[nodiscard]] auto relative_to(SectorCoord const& origin_sector) const noexcept -> glm::vec3;

    /// Нормализует local_position (держит в пределах сектора).
    auto normalize() noexcept -> void;
};

} // namespace projectv::world
```

---

### FloatingOrigin

```cpp
// ProjectV.World.FloatingOrigin.cppm
export module ProjectV.World.FloatingOrigin;

import std;
import glm;
import ProjectV.World.Coordinate;
import ProjectV.ECS.Flecs;

export namespace projectv::world {

/// Callback для уведомления о сдвиге origin.
export using OriginShiftCallback = std::move_only_function<void(glm::vec3 const& offset)>;

/// Floating Origin Manager.
///
/// ## Purpose
/// Поддерживает камеру в (0, 0, 0), сдвигая все объекты при необходимости.
///
/// ## Thread Safety
/// - update() вызывается из main thread
/// - callbacks вызываются синхронно
///
/// ## Invariants
/// - Камера всегда в local_position (0, 0, 0)
/// - current_sector_ отражает сектор камеры
/// - accumulated_offset_ для отладки/визуализации
export class FloatingOrigin {
public:
    FloatingOrigin() noexcept;
    ~FloatingOrigin() noexcept;

    FloatingOrigin(FloatingOrigin&&) noexcept;
    FloatingOrigin& operator=(FloatingOrigin&&) noexcept;
    FloatingOrigin(const FloatingOrigin&) = delete;
    FloatingOrigin& operator=(const FloatingOrigin&) = delete;

    /// Обновляет origin при необходимости.
    ///
    /// @param camera_local_position Позиция камеры в локальных координатах
    /// @return true если произошёл сдвиг origin
    ///
    /// @pre camera_local_position — позиция относительно текущего origin
    /// @post Если |position| > threshold, все позиции сдвинуты
    auto update(glm::vec3 const& camera_local_position) noexcept -> bool;

    /// Регистрирует callback для сдвига origin.
    ///
    /// @param callback Функция, вызываемая при сдвиге
    auto on_origin_shift(OriginShiftCallback callback) noexcept -> void;

    /// Получает текущий сектор.
    [[nodiscard]] auto current_sector() const noexcept -> SectorCoord;

    /// Получает накопленный offset (для отладки).
    [[nodiscard]] auto accumulated_offset() const noexcept -> glm::dvec3;

    /// Устанавливает порог сдвига.
    auto set_threshold(float threshold) noexcept -> void;

    /// Принудительно сдвигает origin.
    auto force_shift(glm::vec3 const& offset) noexcept -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::world
```

---

### Coordinate Transform

```cpp
// ProjectV.World.Transform.cppm
export module ProjectV.World.Transform;

import std;
import glm;
import ProjectV.World.Coordinate;

export namespace projectv::world {

/// JoltPhysics Y-Up → Vulkan Y-Down conversion.
export namespace coord_convert {

/// Матрица преобразования Jolt → Vulkan.
[[nodiscard]] inline auto jolt_to_vulkan_matrix() noexcept -> glm::mat4 {
    static const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    return matrix;
}

/// Преобразует позицию Jolt → Vulkan.
[[nodiscard]] inline auto jolt_to_vulkan(glm::vec3 const& pos) noexcept -> glm::vec3 {
    return {pos.x, -pos.y, pos.z};
}

/// Преобразует позицию Vulkan → Jolt.
[[nodiscard]] inline auto vulkan_to_jolt(glm::vec3 const& pos) noexcept -> glm::vec3 {
    return {pos.x, -pos.y, pos.z};
}

/// Преобразует quaternion Jolt → Vulkan.
[[nodiscard]] auto jolt_to_vulkan_quat(glm::quat const& q) noexcept -> glm::quat;

/// Создаёт Vulkan-совместимую projection matrix.
///
/// @param fov Field of view в радианах
/// @param aspect Aspect ratio
/// @param near Ближняя плоскость
/// @param far Дальняя плоскость
/// @return Projection matrix для Vulkan NDC
[[nodiscard]] inline auto vulkan_projection(
    float fov,
    float aspect,
    float near,
    float far
) noexcept -> glm::mat4 {
    glm::mat4 proj = glm::perspective(fov, aspect, near, far);
    // Vulkan NDC: Y-down, Z [0,1]
    glm::mat4 const correction = {
        1.0f,  0.0f,  0.0f,  0.0f,
        0.0f, -1.0f,  0.0f,  0.0f,
        0.0f,  0.0f,  0.5f,  0.0f,
        0.0f,  0.0f,  0.5f,  1.0f
    };
    return correction * proj;
}

} // namespace coord_convert

} // namespace projectv::world
```

---

## Coordinate Systems Integration

### Systems Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Coordinate Systems in ProjectV                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Absolute World Space                           │    │
│  │  WorldCoordinate { sector: int32_t[3], local: vec3 }             │    │
│  │  Precision: ~1mm within ±134 million km from origin              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Floating Origin Space                          │    │
│  │  Camera always at (0, 0, 0)                                       │    │
│  │  All entities positioned relative to camera                      │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│           ┌────────────────────────┼────────────────────────┐           │
│           ▼                        ▼                        ▼           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ Physics (Jolt)  │  │ Gameplay (ECS)  │  │ UI (ImGui)      │          │
│  │ Y-Up, Right-handed│ Y-Up, Right-handed│ Screen coords    │          │
│  │ Sector-relative │ │ Sector-relative │ │ Pixels          │          │
│  └────────┬────────┘  └────────┬────────┘  └─────────────────┘          │
│           │                    │                                        │
│           │                    │                                        │
│           ▼                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Render Transform                               │    │
│  │  Y-flip for Vulkan NDC                                           │    │
│  │  FrontFace = CLOCKWISE (due to Y-flip)                           │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Vulkan NDC                                     │    │
│  │  X: [-1, 1] left → right                                         │    │
│  │  Y: [-1, 1] top → bottom (Y-DOWN!)                               │    │
│  │  Z: [0, 1] near → far                                            │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Transformation Pipeline

```
Entity Position Flow:

1. Storage (ECS):
   WorldCoordinate { sector, local_position }

2. Physics Update (JoltPhysics):
   position = local_position (sector-relative)
   physics_world.set_origin(current_sector)

3. Origin Shift Check:
   if (|camera_position| > threshold):
       shift_all_entities(-camera_position)
       current_sector += camera_position / SECTOR_SIZE

4. Render Preparation:
   view_matrix = lookAt(camera_position, target, {0, -1, 0})  // Y-down for Vulkan
   model_matrix = entity_transform * jolt_to_vulkan_matrix()

5. GPU Submission:
   MVP = projection * view * model
   gl_Position = MVP * vertex_position
```

---

## SVO Integration (max_depth = 16)

### World Scale

```
SVO Configuration:
- max_depth = 16
- max_extent = 2^16 = 65536 voxels
- voxel_size = 1.0 meter
- total_extent = 65.536 km per axis

With Floating Origin:
- SVO covers one sector (4096m = 2^12 voxels)
- SVO depth per sector = 12
- Entities can span multiple sectors
- Seamless transitions via sector loading
```

### Sector-based SVO

```cpp
/// SVO Configuration for sectors.
export struct SectorSVOConfig {
    uint32_t depth{12};           ///< Depth for one sector (4096 voxels)
    float voxel_size{1.0f};       ///< 1 meter per voxel
    bool enable_dag_compression{true};

    /// Вычисляет физический размер сектора.
    [[nodiscard]] auto sector_size() const noexcept -> float {
        return static_cast<float>(1 << depth) * voxel_size;
    }
};

/// Sector-based voxel storage.
export class SectorVoxelStorage {
public:
    /// Получает SVO для сектора.
    [[nodiscard]] auto get_svo(SectorCoord const& coord) noexcept -> SVOTree*;

    /// Загружает сектор.
    auto load_sector(SectorCoord const& coord) noexcept -> void;

    /// Выгружает сектор.
    auto unload_sector(SectorCoord const& coord) noexcept -> void;

    /// Получает воксель по мировым координатам.
    [[nodiscard]] auto get_voxel(WorldCoordinate const& coord) const noexcept
        -> std::expected<VoxelData, SVOError>;

    /// Устанавливает воксель по мировым координатам.
    auto set_voxel(WorldCoordinate const& coord, VoxelData const& data) noexcept
        -> std::expected<void, SVOError>;

private:
    std::unordered_map<SectorCoord, SVOTree, SectorCoordHash> sectors_;
    SectorSVOConfig config_;
};
```

---

## Winding Order (Front Face)

### Critical Issue

При Y-flip инвертируется winding order:

```
Исходный треугольник (Y-Up):       После Y-flip (Y-Down):
     A (top)                           A (bottom)
    /\                                /\
   /  \                              /  \
  B----C                            B----C

Winding: A→B→C = CCW                 Winding: A→B→C = CW
```

### Vulkan Configuration

```cpp
VkPipelineRasterizationStateCreateInfo rasterization{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,  // Из-за Y-flip!
    .lineWidth = 1.0f
};
```

---

## Transformation Tables

### System Conventions

| Система       | Y-ось | Z-ось    | Winding         |
|---------------|-------|----------|-----------------|
| JoltPhysics   | Up    | Forward  | CCW             |
| GLM (default) | Up    | Backward | CCW             |
| Vulkan NDC    | Down  | Forward  | CW (after flip) |
| glTF          | Up    | Forward  | CCW             |

### Conversion Matrices

| Преобразование  | Матрица                                                     |
|-----------------|-------------------------------------------------------------|
| Jolt → Vulkan   | `scale(1, -1, 1)`                                           |
| Vulkan → Jolt   | `scale(1, -1, 1)` (инволюция)                               |
| OpenGL → Vulkan | `scale(1, -1, 1) × translate(0, 0, 0.5) × scale(1, 1, 0.5)` |

---

## Ссылки

- [SVO Architecture](./00_svo-architecture.md)
- [Vulkan 1.4 Specification](./04_vulkan_spec.md)
- [Physics Bridge](./06_jolt-vulkan-bridge.md)
