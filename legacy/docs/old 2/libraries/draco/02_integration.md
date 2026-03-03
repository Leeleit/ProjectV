# Интеграция

🟢 **Уровень 1: Начинающий**

Подключение Draco к проекту через CMake.

## CMake интеграция

### Через add_subdirectory

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(MyProject)

# Добавление Draco как подмодуля
add_subdirectory(external/draco)

# Ваша цель
add_executable(my_app src/main.cpp)

# Линковка
target_link_libraries(my_app PRIVATE draco::draco)

# Include directories добавляются автоматически
```

### Через find_package (установленный Draco)

```cmake
# После установки Draco в систему
find_package(draco CONFIG REQUIRED)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE draco::draco)
```

### FetchContent (CMake 3.14+)

```cmake
include(FetchContent)

FetchContent_Declare(
    draco
    GIT_REPOSITORY https://github.com/google/draco.git
    GIT_TAG 1.5.7
)

FetchContent_MakeAvailable(draco)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE draco::draco)
```

## CMake опции

| Опция                           | Default | Описание                 |
|---------------------------------|---------|--------------------------|
| `BUILD_SHARED_LIBS`             | OFF     | Сборка shared library    |
| `DRACO_TRANSCODER_SUPPORTED`    | OFF     | Включить glTF transcoder |
| `DRACO_ANIMATION_ENCODING`      | OFF     | Поддержка анимаций       |
| `DRACO_POINT_CLOUD_COMPRESSION` | ON      | Сжатие point cloud       |
| `DRACO_MESH_COMPRESSION`        | ON      | Сжатие mesh              |
| `DRACO_BUILD_EXECUTABLES`       | ON      | CLI инструменты          |
| `DRACO_TESTS`                   | OFF     | Сборка тестов            |

### Пример с опциями

```cmake
set(DRACO_TRANSCODER_SUPPORTED ON CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING ON CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/draco)
```

## DRACO_TRANSCODER_SUPPORTED

Опция включает расширенные возможности:

- glTF загрузка/сохранение
- Materials и textures
- Animations
- Skins
- Scene graph
- EXT_mesh_features / EXT_structural_metadata

> **Примечание:** Требует C++17 и дополнительные зависимости (tinygltf).

### Зависимости для transcoder

```cmake
# При DRACO_TRANSCODER_SUPPORTED=ON
# Draco автоматически подтянет:
# - tinygltf (встроен)
# - libpng (опционально, для текстур)
# - libjpeg (опционально, для текстур)
```

## Минимальная конфигурация

Для декодирования только mesh:

```cmake
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING OFF CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/draco)

# Линкуется только decoder functionality
target_link_libraries(my_app PRIVATE draco::draco)
```

## Структура библиотеки

При сборке создаётся одна библиотека `draco`:

```
libdraco.a / draco.lib
├── Core (decoder_buffer, encoder_buffer)
├── Attributes (geometry_attribute, point_attribute)
├── Compression
│   ├── decode, encode
│   ├── mesh (edgebreaker, sequential)
│   └── point_cloud (kd_tree, sequential)
├── Mesh (mesh, corner_table)
├── PointCloud (point_cloud)
└── IO (obj, ply, stl) - при DRACO_TRANSCODER_SUPPORTED
```

## Заголовочные файлы

Основные include пути:

```cpp
// Декодирование
#include <draco/compression/decode.h>

// Кодирование
#include <draco/compression/encode.h>
#include <draco/compression/expert_encode.h>

// Типы данных
#include <draco/mesh/mesh.h>
#include <draco/point_cloud/point_cloud.h>
#include <draco/attributes/geometry_attribute.h>
#include <draco/attributes/point_attribute.h>

// Буферы
#include <draco/core/decoder_buffer.h>
#include <draco/core/encoder_buffer.h>

// Опции
#include <draco/compression/config/encoder_options.h>
#include <draco/compression/config/decoder_options.h>
```

## CLI инструменты

При `DRACO_BUILD_EXECUTABLES=ON`:

| Инструмент         | Назначение                                    |
|--------------------|-----------------------------------------------|
| `draco_encoder`    | Кодирование OBJ/PLY/STL в .drc                |
| `draco_decoder`    | Декодирование .drc в OBJ/PLY                  |
| `draco_transcoder` | glTF transcoding (DRACO_TRANSCODER_SUPPORTED) |

### Примеры команд

```bash
# Кодирование
draco_encoder -i model.obj -o model.drc -qp 14

# С параметрами сжатия
draco_encoder -i model.ply -o model.drc -cl 8 -qp 12 -qt 10

# Декодирование
draco_decoder -i model.drc -o model.obj

# glTF transcoding
draco_transcoder -i scene.glb -o compressed.glb -qp 12
```

## Кросс-компиляция

### Android NDK

```cmake
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-24)

set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/draco)
```

### iOS

```cmake
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_ARCHITECTURES arm64)

set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/draco)
```

### Emscripten (WebAssembly)

```bash
emcmake cmake -B build \
    -DDRACO_BUILD_EXECUTABLES=OFF \
    -DDRACO_TESTS=OFF

emmake make -C build
```

## Sanitizers

```cmake
set(DRACO_SANITIZE address CACHE STRING "" FORCE)
# Доступные значения: address, memory, thread, undefined

add_subdirectory(external/draco)
```

## Сборка с SIMD оптимизациями

Draco автоматически определяет SIMD возможности:

```cmake
# Автоопределение при конфигурации
# Включает AVX/SSE на x86, NEON на ARM
# Явное управление не требуется
```

## Отключение ненужных компонентов

Для минимального бинарника только decoder:

```cmake
# Минимальный decoder-only build
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "" FORCE)
set(DRACO_MESH_COMPRESSION ON CACHE BOOL "" FORCE)
set(DRACO_POINT_CLOUD_COMPRESSION OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING OFF CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)
```

## Размер библиотеки

Примерные размеры (Release, x64):

| Конфигурация             | Размер  |
|--------------------------|---------|
| Decoder only             | ~800 KB |
| Full (encoder + decoder) | ~1.2 MB |
| With transcoder          | ~2.5 MB |

## Совместимость ABI

Draco гарантирует обратную совместимость decoder:

- Новый decoder читает старые bitstreams
- Старый decoder НЕ читает новые bitstreams

Версия bitstream в заголовке:

```cpp
// Mesh bitstream version: 2.2
// Point cloud bitstream version: 2.3
