# Продвинутые оптимизации GLM для ProjectV

## Data-Oriented Design

### SoA для трансформов

```cpp
template<size_t Alignment = 64>
struct TransformSoA {
    alignas(Alignment) std::vector<glm::vec3> positions;
    alignas(Alignment) std::vector<glm::vec3> rotations;  // Euler angles
    alignas(Alignment) std::vector<glm::vec3> scales;
    alignas(Alignment) std::vector<glm::mat4> matrices;

    void add(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
        positions.push_back(pos);
        rotations.push_back(rot);
        scales.push_back(scale);

        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, pos);
        m = glm::rotate(m, rot.x, glm::vec3(1, 0, 0));
        m = glm::rotate(m, rot.y, glm::vec3(0, 1, 0));
        m = glm::rotate(m, rot.z, glm::vec3(0, 0, 1));
        m = glm::scale(m, scale);
        matrices.push_back(m);
    }

    void updateMatrix(size_t index) {
        const auto& pos = positions[index];
        const auto& rot = rotations[index];
        const auto& s = scales[index];

        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, pos);
        m = glm::rotate(m, rot.x, glm::vec3(1, 0, 0));
        m = glm::rotate(m, rot.y, glm::vec3(0, 1, 0));
        m = glm::rotate(m, rot.z, glm::vec3(0, 0, 1));
        m = glm::scale(m, s);
        matrices[index] = m;
    }
};
```

### Hot/Cold Separation

```cpp
struct CameraDataDOD {
    // Hot data - каждый кадр
    alignas(64) struct HotData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;  // precomputed
        glm::vec3 position;
        glm::vec3 forward;
        glm::vec3 right;
        glm::vec3 up;
    } hot;

    // Cold data - редко меняется
    struct ColdData {
        float fov;
        float aspect;
        float near;
        float far;
        glm::vec3 target;
    } cold;
};
```

## SIMD оптимизации

### Векторные операции

```cpp
// GLM автоматически использует SIMD если доступно
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SSE2  // или AVX, AVX2

#include <glm/glm.hpp>

// GLM использует SSE2/AVX для vec4 операций
glm::vec4 a(1, 2, 3, 4);
glm::vec4 b(5, 6, 7, 8);
glm::vec4 c = a + b;  // SIMD если доступно
```

### Ручной SIMD

```cpp
#include <glm/vec4.hpp>
#include <glm/ext/matrix_double4x4.hpp>

__m128 transformPosition(__m128 pos, __m128 col0, __m128 col1, __m128 col2, __m128 col3) {
    __m128 x = _mm_shuffle_ps(pos, pos, _MM_SHUFFLE(0,0,0,0));
    __m128 y = _mm_shuffle_ps(pos, pos, _MM_SHUFFLE(1,1,1,1));
    __m128 z = _mm_shuffle_ps(pos, pos, _MM_SHUFFLE(2,2,2,2));
    __m128 w = _mm_shuffle_ps(pos, pos, _MM_SHUFFLE(3,3,3,3));

    return _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(x, col0), _mm_mul_ps(y, col1)),
        _mm_add_ps(_mm_mul_ps(z, col2), w)
    );
}
```

## Batch обработка

### Instanced rendering

```cpp
struct InstanceData {
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
};

class BatchRenderer {
public:
    void addInstance(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale) {
        InstanceData data;
        data.modelMatrix = glm::translate(glm::mat4(1.0f), pos)
                         * glm::mat4_cast(rot)
                         * glm::scale(glm::mat4(1.0f), scale);
        data.normalMatrix = glm::transpose(glm::inverse(data.modelMatrix));

        instances_.push_back(data);
    }

    void uploadToGPU(VkBuffer buffer) {
        // Загрузка в GPU буфер
    }

private:
    std::vector<InstanceData> instances_;
};
```

## Интеграция с Tracy

```cpp
#include <tracy/Tracy.hpp>

void updateCamera(Camera& cam) {
    ZoneScoped;

    {
        ZoneScopedN("LookAt");
        cam.setView(glm::lookAt(cam.position(), cam.target(), cam.up()));
    }

    {
        ZoneScopedN("Perspective");
        cam.setProj(glm::perspective(cam.fov(), cam.aspect(), cam.near(), cam.far()));
    }

    {
        ZoneScopedN("ViewProj");
        cam.setViewProj(cam.proj() * cam.view());
    }
}
```

## Рекомендации для ProjectV

1. **GLM_FORCE_INTRINSICS** — включи SIMD оптимизации
2. **Precompute matrices** — ViewProj, normalMatrix кэшируй
3. **Batch processing** — собирай инстансы в SoA
4. **Tracy** — профилируй матричные операции отдельно
5. **alignas(16)** — выравнивай для SSE/AVX
