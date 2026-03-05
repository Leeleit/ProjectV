# Интеграция fastgltf

**🟡 Уровень 2: Средний**

Пошаговое руководство по подключению fastgltf в C++ проекте.

## Оглавление

- [1. CMake](#1-cmake)
- [2. Options](#2-options)
- [3. Category](#3-category)
- [4. Расширения glTF](#4-расширения-gltf)
- [5. Загрузка в Vulkan](#5-загрузка-в-vulkan)
- [6. Include и заголовки](#6-include-и-заголовки)
- [7. Связь с glm](#7-связь-с-glm)
- [8. Android](#8-android)
- [9. Extras и determineGltfFileType](#9-extras-и-determinegltffiletype)
- [10. GltfDataGetter: MappedGltfFile](#10-gltfdatagetter-mappedgltffile)
- [11. CMake-опции fastgltf](#11-cmake-опции-fastgltf)

---

## 1. CMake

### Базовая интеграция

```cmake
# Добавление fastgltf как подпроекта
add_subdirectory(external/fastgltf)
# ...
target_link_libraries(YourApp PRIVATE fastgltf::fastgltf)
```

simdjson подгружается автоматически через CMake's FetchContent или `find_package(simdjson)`.

### Сложные сценарии сборки

#### Случай 1: Проект уже использует simdjson

```cmake
# Добавьте simdjson ДО fastgltf
find_package(simdjson REQUIRED)  # или add_subdirectory для вашей версии

# Fastgltf обнаружит системный simdjson через find_dependency
add_subdirectory(external/fastgltf)
```

#### Случай 2: Конфликт версий simdjson

Если ваш проект требует определённой версии simdjson, а fastgltf использует другую:

```cmake
# 1. Установите нужную версию simdjson через FetchContent с точным хешем
include(FetchContent)
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.6.2  # Конкретная версия, совместимая с fastgltf
    OVERRIDE_FIND_PACKAGE
)

# 2. Добавьте fastgltf
add_subdirectory(external/fastgltf)
```

#### Случай 3: Кроссплатформенная линковка

Для Windows/Linux/macOS с поддержкой статической/динамической линковки:

```cmake
# Пример из CMakeLists.txt:
add_subdirectory(external/fastgltf EXCLUDE_FROM_ALL)

# Fastgltf может быть статической или динамической библиотекой
target_link_libraries(YourApp PRIVATE fastgltf::fastgltf)

# При необходимости экспортируйте символы
if(WIN32)
    target_compile_definitions(fastgltf::fastgltf INTERFACE "FASTGLTF_EXPORT=__declspec(dllimport)")
endif()
```

### Интеграция в проекты

В вашем проекте fastgltf добавляется как подмодуль в `external/fastgltf`. Пример из `CMakeLists.txt`:

```cmake
# fastgltf (закомментирован, но готов к использованию)
# add_subdirectory(external/fastgltf)

# Использование в примерах (docs/examples/CMakeLists.txt):
if (EXISTS ${PROJECT_ROOT}/external/fastgltf/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/fastgltf ${CMAKE_BINARY_DIR}/external/fastgltf EXCLUDE_FROM_ALL)
    
    add_executable(example_fastgltf_load_mesh ${EXAMPLES_DIR}/fastgltf_load_mesh.cpp)
    target_link_libraries(example_fastgltf_load_mesh PRIVATE fastgltf::fastgltf)
endif ()
```

### CMake-опции fastgltf (см. раздел 11)

Управляйте сборкой через переменные CMake:

- `FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=ON/OFF` - отключение pmr-аллокатора
- `FASTGLTF_ENABLE_EXAMPLES=ON/OFF` - сборка примеров fastgltf
- `FASTGLTF_ENABLE_TESTS=ON/OFF` - сборка тестов

### Устранение проблем

1. **Ошибка "Could not find simdjson"**: Убедитесь, что интернет доступен для FetchContent, или установите simdjson
   вручную.
2. **Конфликты версий C++**: Fastgltf требует C++17, ваш проект использует C++26 — совместимо.
3. **Проблемы линковки на Windows**: Убедитесь, что `FASTGLTF_EXPORT` правильно определён для DLL.
4. **Большие модели (>1GB)**: Установите `FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=ON` для стандартного аллокатора.

### Интеграция с другими библиотеками

Fastgltf хорошо работает в стеке современных игровых движков:

- **Vulkan/VMA**: Для загрузки буферов прямо в GPU (см. раздел 5)
- **glm**: Для математических операций (см. раздел 7)
- **flecs**: Для ECS интеграции (см. projectv-integration.md)
- **Tracy**: Для профилирования загрузки

---

## 2. Options

Опции передаются в `loadGltf`, `loadGltfJson`, `loadGltfBinary`:

| Опция                         | Описание                                                                                           |
|-------------------------------|----------------------------------------------------------------------------------------------------|
| `Options::None`               | Только парсинг JSON. Буферы GLB по умолчанию загружаются в память (ByteView/Array); внешние — URI. |
| `LoadExternalBuffers`         | Загрузить внешние .bin в `sources::Vector`.                                                        |
| `LoadExternalImages`          | Загрузить внешние изображения в память.                                                            |
| `DecomposeNodeMatrices`       | Разложить матрицы узлов на translation, rotation, scale.                                           |
| `GenerateMeshIndices`         | Сгенерировать индексы для примитивов без index accessor.                                           |
| `AllowDouble`                 | Разрешить component type 5130 (GL_DOUBLE).                                                         |
| `DontRequireValidAssetMember` | Не проверять строго поле asset в JSON.                                                             |

`LoadGLBBuffers` deprecated — GLB-буферы по умолчанию загружаются в память.

Комбинация:

```cpp
auto asset = parser.loadGltf(data.get(), basePath,
    fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);
```

---

## 3. Category

Параметр `Category` позволяет парсить только нужные части модели:

```cpp
// Только рендеринг (без анимаций и скинов)
auto asset = parser.loadGltf(data.get(), basePath, options,
    fastgltf::Category::OnlyRenderable);

// Только анимации (для извлечения ключевых кадров)
auto asset = parser.loadGltf(data.get(), basePath, options,
    fastgltf::Category::OnlyAnimations);
```

| Значение                   | Описание                                                  |
|----------------------------|-----------------------------------------------------------|
| `Category::All`            | Всё (по умолчанию).                                       |
| `Category::OnlyRenderable` | Buffers, Meshes, Materials и т.д., без Animations, Skins. |
| `Category::OnlyAnimations` | Animations, Accessors, BufferViews, Buffers.              |

---

## 4. Расширения glTF

Расширения glTF — это механизм добавления новых возможностей к базовой спецификации формата. Для их использования
необходимо указать поддерживаемые расширения при создании Parser.

### Передача расширений в Parser

```cpp
// Указать, какие расширения вы готовы обрабатывать
fastgltf::Extensions extensionsToLoad = fastgltf::Extensions::KHR_texture_basisu
    | fastgltf::Extensions::KHR_materials_unlit
    | fastgltf::Extensions::EXT_mesh_gpu_instancing;

fastgltf::Parser parser(extensionsToLoad);
```

### Проверка используемых расширений

После загрузки модели проверьте, какие расширения она использует:

```cpp
auto asset = parser.loadGltf(data, path, options);

if (!asset->extensionsRequired.empty()) {
    std::cout << "Обязательные расширения:" << std::endl;
    for (const auto& ext : asset->extensionsRequired) {
        std::cout << " - " << ext << std::endl;
    }
}
```

---

## 5. Интеграция с графическими API

fastgltf предоставляет гибкие возможности для интеграции с различными графическими API (Vulkan, Direct3D, Metal,
OpenGL).

### Основные стратегии

#### 1. Стандартный подход через системную память

Самый простой способ: fastgltf загружает данные в RAM, затем вы копируете их в GPU буферы.

```cpp
auto asset = parser.loadGltf(data, path, fastgltf::Options::LoadExternalBuffers);

// 1. Копирование данных из fastgltf в промежуточные массивы
std::vector<glm::vec3> positions;
fastgltf::copyFromAccessor<glm::vec3>(asset.get(), positionAccessor, positions);

// 2. Создание GPU буферов и копирование данных
// Пример для OpenGL:
glGenBuffers(1, &vbo);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), 
             positions.data(), GL_STATIC_DRAW);
```

#### 2. Прямое маппирование через setBufferAllocationCallback

Для минимизации копирований можно использовать `setBufferAllocationCallback`, чтобы fastgltf записывал данные напрямую в
GPU-буфер. Этот подход особенно полезен для интеграции с Vulkan, Direct3D, Metal или OpenGL с persistent mapping.

```cpp
parser.setBufferAllocationCallback(
    [](std::uint64_t bufferSize, void* userPointer) -> fastgltf::BufferInfo {
        // Создание GPU буфера с mapped памятью
        void* mappedPtr;
        // GPU-specific memory allocation
        // Пример для Vulkan: vkMapMemory
        // Пример для Direct3D 12: Map
        // Пример для OpenGL: glMapBufferRange с GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT
        
        return fastgltf::BufferInfo{
            .mappedMemory = mappedPtr,
            .customId = reinterpret_cast<std::uint64_t>(buffer)
        };
    },
    [](fastgltf::BufferInfo* info, void* userPointer) {
        // Анмап памяти после записи (опционально)
        // GPU-specific unmap operation
    }
);
```

#### 3. Zero-copy подход с кастомным DataSource

Для самых требовательных сценариев можно использовать кастомный `DataSource`, позволяющий fastgltf читать данные
напрямую из GPU-памяти или других нестандартных источников.

---

## 6. Include и заголовки

**Выбор include по задаче:**

| Задача                                                                | Подключать                                   |
|-----------------------------------------------------------------------|----------------------------------------------|
| Базовая загрузка (Parser, Asset, Error)                               | `#include <fastgltf/core.hpp>`               |
| Accessor tools (iterateAccessor, copyFromAccessor, iterateSceneNodes) | `#include <fastgltf/tools.hpp>`              |
| glm с accessor tools                                                  | `#include <fastgltf/glm_element_traits.hpp>` |
| Только типы без Parser                                                | `#include <fastgltf/types.hpp>`              |

`core.hpp` уже включает `types.hpp`.

---

## 7. Связь с glm

fastgltf имеет встроенную математику. Для использования **glm** с accessor tools подключите traits:

```cpp
#include <glm/glm.hpp>
#include <fastgltf/glm_element_traits.hpp>
```

Тогда `iterateAccessor<glm::vec3>` будет работать.

---

## 8. Android

На Android для загрузки glTF из APK assets:

```cpp
#include <android/asset_manager_jni.h>

AAssetManager* manager = AAssetManager_fromJava(env, assetManager);
fastgltf::setAndroidAssetManager(manager);

auto data = fastgltf::AndroidGltfDataBuffer::FromAsset("models/scene.gltf");
```

---

## 9. Extras и determineGltfFileType

**Extras:** fastgltf не сохраняет `extras` в Asset. Используйте `setExtrasParseCallback`.

**determineGltfFileType:** если нужно заранее узнать тип файла (glTF/GLB/Invalid).

---

## 10. GltfDataGetter: MappedGltfFile

`MappedGltfFile` — memory-mapped чтение файла (только desktop).

---

## 11. CMake-опции fastgltf

| Опция                                 | По умолчанию | Описание                |
|---------------------------------------|--------------|-------------------------|
| `FASTGLTF_ENABLE_TESTS`               | OFF          | Сборка тестов           |
| `FASTGLTF_ENABLE_EXAMPLES`            | OFF          | Сборка примеров         |
| `FASTGLTF_ENABLE_DEPRECATED_EXT`      | OFF          | Устаревшие расширения   |
| `FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL` | OFF          | Отключить pmr-аллокатор |
