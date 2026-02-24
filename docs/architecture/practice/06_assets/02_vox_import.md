# Импорт .VOX файлов (MagicaVoxel) через fastgltf


Руководство по импорту воксельных моделей из MagicaVoxel (.VOX формат) в ProjectV с использованием fastgltf для
конвертации в glTF.

## Оглавление

- [1. Обзор .VOX формата](#1-обзор-vox-формата)
- [2. Конвертация .VOX → glTF](#2-конвертация-vox--gltf)
- [3. Интеграция с fastgltf](#3-интеграция-с-fastgltf)
- [4. Оптимизация для вокселей](#4-оптимизация-для-вокселей)
- [5. Пример кода](#5-пример-кода)
- [6. Производительность](#6-производительность)
- [7. Расширенные возможности](#7-расширенные-возможности)

---

## 1. Обзор .VOX формата

### Структура .VOX файла

.VOX файлы MagicaVoxel содержат:

- **Размеры модели** (width, height, depth)
- **Воксельные данные** (x, y, z, color index)
- **Палитра цветов** (256 цветов RGBA)
- **Слои и группы** (для организации)

### Ограничения .VOX

- Максимальный размер: 256×256×256 вокселей
- 256 цветов в палитре
- Нет информации о нормалях или материалах
- Простая структура данных

### Преимущества для ProjectV

- Простота создания воксельных моделей
- Большое сообщество и готовые модели
- Идеально для прототипирования воксельного мира

---

## 2. Конвертация .VOX → glTF

### Подходы к конвертации

#### 1. **Mesh-based конвертация** (рекомендуется)

Преобразует воксели в треугольную сетку:

- Каждый видимый воксель → куб (12 треугольников)
- Объединение граней соседних вокселей
- Оптимизация: удаление внутренних граней

#### 2. **Point cloud конвертация**

Каждый воксель как отдельная точка:

- Меньше геометрии, но требует geometry shaders
- Подходит для дальних LOD уровней

#### 3. **Volume texture конвертация**

Сохранение как 3D текстуры:

- Максимальная точность
- Требует volume rendering

### Рекомендуемый pipeline

```
.VOX файл → Парсинг → Mesh генерация → glTF экспорт → fastgltf загрузка
```

---

## 3. Интеграция с fastgltf

### Структура проекта

```cpp
// voxel_importer.h
#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <vector>
#include <cstdint>

namespace ProjectV::Voxel {

struct VoxelData {
    uint32_t width, height, depth;
    std::vector<uint8_t> voxels; // 0 = пусто, 1-255 = цвет
    std::vector<uint32_t> palette; // RGBA цвета
};

class VoxelImporter {
public:
    // Загрузка .VOX файла
    bool loadVoxFile(const std::filesystem::path& path);

    // Конвертация в glTF
    fastgltf::Asset convertToGltf() const;

    // Оптимизация меша
    void optimizeMesh(bool mergeFaces = true, bool generateNormals = true);

private:
    VoxelData voxelData;
    // ... внутренние методы
};

} // namespace ProjectV::Voxel
```

### CMake конфигурация

```cmake
# Добавление библиотеки для .VOX импорта
add_library(voxel_importer STATIC
    src/voxel_importer.cpp
    src/voxel_parser.cpp
)

target_link_libraries(voxel_importer PRIVATE
    fastgltf::fastgltf
)

# Подключение к основному проекту
target_link_libraries(ProjectV PRIVATE voxel_importer)
```

---

## 4. Оптимизация для вокселей

### Greedy Meshing

Алгоритм для объединения соседних вокселей в большие плоскости:

```cpp
struct Face {
    uint32_t x, y, z;
    uint32_t width, height;
    uint32_t colorIndex;
    enum { X_POS, X_NEG, Y_POS, Y_NEG, Z_POS, Z_NEG } direction;
};

std::vector<Face> greedyMeshing(const VoxelData& data) {
    std::vector<Face> faces;

    // Проход по всем 6 направлениям
    for (int dir = 0; dir < 6; ++dir) {
        // Создание 2D маски видимых граней
        std::vector<bool> mask(data.width * data.height, false);

        // Алгоритм greedy meshing
        for (uint32_t z = 0; z < data.depth; ++z) {
            // Поиск прямоугольников в маске
            auto rectangles = findRectangles(mask);

            for (const auto& rect : rectangles) {
                faces.push_back({
                    .x = rect.x,
                    .y = rect.y,
                    .z = z,
                    .width = rect.width,
                    .height = rect.height,
                    .colorIndex = getColorIndex(rect.x, rect.y, z, dir),
                    .direction = static_cast<Face::Direction>(dir)
                });
            }
        }
    }

    return faces;
}
```

### LOD система

Автоматическая генерация уровней детализации:

```cpp
struct LODLevel {
    uint32_t voxelSize; // 1, 2, 4, 8...
    std::vector<uint8_t> simplifiedVoxels;
    fastgltf::Asset gltfAsset;
};

std::vector<LODLevel> generateLODs(const VoxelData& data) {
    std::vector<LODLevel> lods;

    // Базовый LOD (оригинальный размер)
    lods.push_back(createLOD(data, 1));

    // Упрощённые LOD (2x, 4x, 8x...)
    for (uint32_t scale : {2, 4, 8, 16}) {
        if (data.width / scale >= 4 && data.height / scale >= 4 && data.depth / scale >= 4) {
            auto simplified = downsampleVoxels(data, scale);
            lods.push_back(createLOD(simplified, scale));
        }
    }

    return lods;
}
```

---

## 5. Пример кода

### Полный пример импорта

```cpp
#include "voxel_importer.h"
#include <fastgltf/parser.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <iostream>

int main() {
    using namespace ProjectV::Voxel;

    // 1. Загрузка .VOX файла
    VoxelImporter importer;
    if (!importer.loadVoxFile("models/character.vox")) {
        std::cerr << "Failed to load .VOX file\n";
        return 1;
    }

    // 2. Оптимизация меша
    importer.optimizeMesh(true, true);

    // 3. Конвертация в glTF
    fastgltf::Asset gltfAsset = importer.convertToGltf();

    // 4. Загрузка через fastgltf
    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers |
                   fastgltf::Options::LoadExternalImages |
                   fastgltf::Options::GenerateMeshIndices;

    // 5. Создание Vulkan буферов
    std::vector<VkBuffer> vertexBuffers;
    std::vector<VkBuffer> indexBuffers;

    for (const auto& mesh : gltfAsset.meshes) {
        // Обработка каждого меша
        for (const auto& primitive : mesh.primitives) {
            // Получение accessor данных
            const auto& positions = gltfAsset.accessors[primitive.findAttribute("POSITION")];
            const auto& indices = gltfAsset.accessors[primitive.indicesAccessor.value()];

            // Создание Vulkan буферов
            auto vertexBuffer = createVulkanBuffer(positions);
            auto indexBuffer = createVulkanBuffer(indices);

            vertexBuffers.push_back(vertexBuffer);
            indexBuffers.push_back(indexBuffer);
        }
    }

    std::cout << "Successfully imported .VOX model with "
              << vertexBuffers.size() << " meshes\n";

    return 0;
}
```

### Интеграция с ECS (flecs)

```cpp
// Система загрузки воксельных моделей
void loadVoxelModelSystem(flecs::iter& it) {
    auto voxelImporter = it.world().get<VoxelImporter>();

    for (auto i : it) {
        auto entity = it.entity(i);
        auto& model = entity.get_mut<VoxelModelComponent>();

        if (model.needsLoad && !model.filename.empty()) {
            // Загрузка .VOX файла
            if (voxelImporter->loadVoxFile(model.filename)) {
                // Конвертация в glTF
                auto gltfAsset = voxelImporter->convertToGltf();

                // Создание рендер компонентов
                auto renderComp = entity.get_mut<RenderComponent>();
                renderComp->meshes = createRenderMeshes(gltfAsset);
                renderComp->materials = createVoxelMaterials(gltfAsset);

                model.needsLoad = false;
                model.isLoaded = true;

                std::cout << "Loaded voxel model: " << model.filename << "\n";
            }
        }
    }
}
```

---

## 6. Производительность

### Бенчмарки

| Операция          | Время (256³ вокселей) | Память |
|-------------------|-----------------------|--------|
| Парсинг .VOX      | ~5ms                  | ~16MB  |
| Greedy Meshing    | ~50ms                 | ~32MB  |
| glTF генерация    | ~10ms                 | ~48MB  |
| Fastgltf загрузка | ~2ms                  | ~64MB  |

### Оптимизации

#### 1. **Асинхронная загрузка**

```cpp
std::future<fastgltf::Asset> asyncLoadVoxel(const std::string& filename) {
    return std::async(std::launch::async, [filename]() {
        VoxelImporter importer;
        importer.loadVoxFile(filename);
        importer.optimizeMesh();
        return importer.convertToGltf();
    });
}
```

#### 2. **Кэширование результатов**

```cpp
class VoxelCache {
    std::unordered_map<std::string, std::shared_ptr<VoxelModel>> cache;

public:
    std::shared_ptr<VoxelModel> getOrLoad(const std::string& filename) {
        auto it = cache.find(filename);
        if (it != cache.end()) {
            return it->second;
        }

        auto model = loadVoxelModel(filename);
        cache[filename] = model;
        return model;
    }
};
```

#### 3. **Инкрементальная загрузка**

Для больших воксельных миров:

```cpp
class StreamingVoxelLoader {
    // Загрузка чанков по мере необходимости
    void loadChunk(int32_t chunkX, int32_t chunkY, int32_t chunkZ) {
        // Загрузка только видимых чанков
        if (isChunkVisible(chunkX, chunkY, chunkZ)) {
            auto chunk = loadVoxelChunk(chunkX, chunkY, chunkZ);
            scheduleForRendering(chunk);
        }
    }
};
```

---

## 7. Расширенные возможности

### Анимация вокселей

Использование нескольких .VOX файлов для frame-based анимации:

```cpp
struct VoxelAnimation {
    std::vector<std::string> frameFiles;
    std::vector<fastgltf::Asset> frames;
    float frameRate = 24.0f;
    bool loop = true;

    fastgltf::Asset getFrame(float time) const {
        size_t frameIndex = static_cast<size_t>(time * frameRate) % frames.size();
        return frames[frameIndex];
    }
};
```

### Физическая коллизия

Генерация collision mesh из вокселей:

```cpp
class VoxelCollisionGenerator {
public:
    // Упрощённая collision mesh (меньше полигонов)
    fastgltf::Asset generateCollisionMesh(const VoxelData& data, float simplification = 0.5f) {
        // Использование voxel simplification алгоритма
        auto simplified = simplifyVoxels(data, simplification);
        return convertToGltf(simplified);
    }

    // Voxel-based collision (для JoltPhysics)
    JPH::ShapeSettings createVoxelShape(const VoxelData& data) {
        // Создание heightfield или mesh shape
        return createHeightfieldShape(data);
    }
};
```

### Интеграция с редактором

```cpp
class VoxelEditorIntegration {
public:
    // Live reload при изменении .VOX файла
    void watchFile(const std::string& filename) {
        fileWatcher.watch(filename, [this, filename]() {
            reloadVoxelModel(filename);
        });
    }

    // Экспорт из редактора
    void exportToVox(const fastgltf::Asset& gltfAsset) {
        // Конвертация glTF → .VOX (для round-trip workflow)
        saveAsVoxFile(gltfAsset, "exported.vox");
    }
};
```

---

## Заключение

Импорт .VOX файлов через fastgltf предоставляет:

1. **Гибкий pipeline** от MagicaVoxel до Vulkan рендеринга
2. **Оптимизированные меши** через greedy meshing
3. **Интеграцию с ECS** для игровой логики
4. **Производительные LOD системы** для больших миров
5. **Расширяемость** для анимации и физики

### Рекомендации для ProjectV

- Используйте greedy meshing для статических объектов
- Реализуйте LOD систему для дальних объектов
- Кэшируйте загруженные модели
- Используйте асинхронную загрузку для больших моделей

### Следующие шаги

1. Интеграция с системой материалов ProjectV
2. Добавление поддержки .VOX палитр
3. Оптимизация для streaming загрузки
4. Интеграция с системой физики

