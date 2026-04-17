# Практические рецепты GLM

🟡 **Уровень 2: Средний**

Готовые решения для типичных задач: трансформации, камера, кватернионы.

## MVP-трансформация

Полный пример вычисления матрицы Model-View-Projection:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 computeMVP(
    glm::vec3 position,      // Позиция объекта
    glm::vec3 rotation,      // Углы Эйлера (радианы)
    glm::vec3 scale,         // Масштаб
    glm::vec3 cameraPos,     // Позиция камеры
    glm::vec3 cameraTarget,  // Точка взгляда
    float fov,               // Поле зрения (радианы)
    float aspect,            // Соотношение сторон
    float nearPlane,         // Ближняя плоскость
    float farPlane           // Дальняя плоскость
) {
    // Model: локальные → мировые координаты
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotation.x, glm::vec3(1, 0, 0));  // Pitch
    model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));  // Yaw
    model = glm::rotate(model, rotation.z, glm::vec3(0, 0, 1));  // Roll
    model = glm::scale(model, scale);

    // View: мировые → координаты камеры
    glm::mat4 view = glm::lookAt(
        cameraPos,
        cameraTarget,
        glm::vec3(0, 1, 0)  // Вектор "вверх"
    );

    // Projection: 3D → 2D экран
    glm::mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);

    return proj * view * model;
}
```

---

## FPS-камера

Камера от первого лица с управлением через WASD + мышь:

```cpp
class FPSCamera {
public:
    glm::vec3 position{0, 0, 5};
    glm::vec3 front{0, 0, -1};
    glm::vec3 up{0, 1, 0};
    glm::vec3 right{1, 0, 0};
    glm::vec3 worldUp{0, 1, 0};

    float yaw{-90.0f};      // Горизонтальный угол (радианы)
    float pitch{0.0f};      // Вертикальный угол (радианы)
    float speed{2.5f};      // Скорость перемещения
    float sensitivity{0.1f}; // Чувствительность мыши

    void processMouse(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        // Ограничение по вертикали
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        updateVectors();
    }

    void processKeyboard(const char direction, float dt) {
        float velocity = speed * dt;
        switch (direction) {
            case 'W': position += front * velocity; break;
            case 'S': position -= front * velocity; break;
            case 'A': position -= right * velocity; break;
            case 'D': position += right * velocity; break;
        }
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

private:
    void updateVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};
```

---

## Orbit-камера

Камера, вращающаяся вокруг цели:

```cpp
class OrbitCamera {
public:
    glm::vec3 target{0, 0, 0};
    float distance{5.0f};
    float minDistance{1.0f};
    float maxDistance{50.0f};

    float yaw{0.0f};
    float pitch{30.0f};

    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = glm::clamp(pitch + dPitch, -89.0f, 89.0f);
    }

    void zoom(float delta) {
        distance = glm::clamp(distance - delta, minDistance, maxDistance);
    }

    glm::mat4 getViewMatrix() const {
        glm::vec3 position;
        position.x = target.x + distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        position.y = target.y + distance * sin(glm::radians(pitch));
        position.z = target.z + distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));

        return glm::lookAt(position, target, glm::vec3(0, 1, 0));
    }

    glm::vec3 getPosition() const {
        glm::vec3 pos;
        pos.x = target.x + distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        pos.y = target.y + distance * sin(glm::radians(pitch));
        pos.z = target.z + distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));
        return pos;
    }
};
```

---

## Плавные повороты с кватернионами

Интерполяция между двумя ориентациями:

```cpp
#include <glm/gtc/quaternion.hpp>

class SmoothRotation {
    glm::quat current{1.0f, 0.0f, 0.0f, 0.0f};  // Текущая ориентация
    glm::quat target{1.0f, 0.0f, 0.0f, 0.0f};   // Целевая ориентация
    float speed{2.0f};                           // Скорость поворота

public:
    void setTarget(glm::vec3 eulerAngles) {
        target = glm::quat(eulerAngles);
    }

    void setTargetAxisAngle(float angle, glm::vec3 axis) {
        target = glm::angleAxis(angle, axis);
    }

    void setTargetDirection(glm::vec3 direction, glm::vec3 up = glm::vec3(0, 1, 0)) {
        target = glm::quatLookAt(glm::normalize(direction), up);
    }

    void update(float dt) {
        float t = glm::clamp(dt * speed, 0.0f, 1.0f);
        current = glm::slerp(current, target, t);
        current = glm::normalize(current);
    }

    glm::mat4 getMatrix() const {
        return glm::mat4_cast(current);
    }

    glm::quat getQuat() const {
        return current;
    }
};
```

---

## LookAt для объектов

Направить объект в сторону цели:

```cpp
glm::mat4 lookAtObject(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0, 1, 0)) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 newUp = glm::cross(right, forward);

    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(right, 0.0f);
    rotation[1] = glm::vec4(newUp, 0.0f);
    rotation[2] = glm::vec4(-forward, 0.0f);  // Отрицательный forward

    glm::mat4 translation(1.0f);
    translation[3] = glm::vec4(position, 1.0f);

    return translation * rotation;
}
```

---

## Билборды

Поворот объекта так, чтобы он всегда смотрел на камеру:

```cpp
glm::mat4 createBillboard(glm::vec3 objectPos, glm::vec3 cameraPos, glm::vec3 up = glm::vec3(0, 1, 0)) {
    glm::vec3 toCamera = cameraPos - objectPos;
    toCamera.y = 0;  // Y-билборд (только вращение вокруг Y)
    toCamera = glm::normalize(toCamera);

    glm::vec3 right = glm::normalize(glm::cross(up, toCamera));
    glm::vec3 forward = glm::normalize(glm::cross(right, up));

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0);
    model[1] = glm::vec4(up, 0);
    model[2] = glm::vec4(forward, 0);
    model[3] = glm::vec4(objectPos, 1);

    return model;
}
```

---

## Преобразование координат

### Локальные → мировые

```cpp
glm::vec3 localToWorld(glm::vec3 localPos, glm::mat4 modelMatrix) {
    glm::vec4 worldPos = modelMatrix * glm::vec4(localPos, 1.0f);
    return glm::vec3(worldPos);
}
```

### Мировые → локальные

```cpp
glm::vec3 worldToLocal(glm::vec3 worldPos, glm::mat4 modelMatrix) {
    glm::mat4 invModel = glm::inverse(modelMatrix);
    glm::vec4 localPos = invModel * glm::vec4(worldPos, 1.0f);
    return glm::vec3(localPos);
}
```

### Экранные → мировые (Ray casting)

```cpp
glm::vec3 screenToWorldRay(
    glm::vec2 screenPos,      // Позиция мыши в пикселях
    glm::vec2 screenSize,     // Размер экрана
    glm::mat4 viewMatrix,
    glm::mat4 projMatrix
) {
    // Нормализованные координаты устройства (NDC)
    glm::vec2 ndc(
        (2.0f * screenPos.x) / screenSize.x - 1.0f,
        1.0f - (2.0f * screenPos.y) / screenSize.y
    );

    // Clip-координаты (z = -1 для ближней плоскости)
    glm::vec4 clipCoords(ndc.x, ndc.y, -1.0f, 1.0f);

    // Eye-координаты
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::vec4 eyeCoords = invProj * clipCoords;
    eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);

    // World-координаты
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec4 worldDir = invView * eyeCoords;

    return glm::normalize(glm::vec3(worldDir));
}
```

---

## Проверки и утилиты

### Проверка видимости (Frustum culling)

```cpp
bool isPointInFrustum(glm::vec3 point, const glm::mat4& vpMatrix) {
    glm::vec4 clip = vpMatrix * glm::vec4(point, 1.0f);

    // Проверка NDC границ: [-1, 1] для OpenGL, [0, 1] для Vulkan (z)
    return (clip.x >= -clip.w && clip.x <= clip.w) &&
           (clip.y >= -clip.w && clip.y <= clip.w) &&
           (clip.z >= 0.0f    && clip.z <= clip.w);  // Vulkan: [0, 1]
}
```

### Расстояние до плоскости

```cpp
float distanceToPlane(glm::vec3 point, glm::vec3 planePoint, glm::vec3 planeNormal) {
    return glm::dot(point - planePoint, planeNormal);
}
```

### Проекция точки на плоскость

```cpp
glm::vec3 projectPointOnPlane(glm::vec3 point, glm::vec3 planePoint, glm::vec3 planeNormal) {
    float distance = glm::dot(point - planePoint, planeNormal);
    return point - planeNormal * distance;
}
```

### Отражение вектора скорости

```cpp
glm::vec3 reflectVelocity(glm::vec3 velocity, glm::vec3 normal, float restitution = 1.0f) {
    return velocity - (1.0f + restitution) * glm::dot(velocity, normal) * normal;
}