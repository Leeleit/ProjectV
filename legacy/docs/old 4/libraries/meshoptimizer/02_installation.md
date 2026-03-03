# Установка

🟢 **Уровень 1: Базовый**

Интеграция meshoptimizer в проект: CMake, header-only, amalgamated build.

---

## Варианты интеграции

| Способ      | Описание                            | Рекомендуется          |
|-------------|-------------------------------------|------------------------|
| CMake       | Полная интеграция с системой сборки | Да                     |
| Header-only | Подключение исходников напрямую     | Для простых проектов   |
| Amalgamated | Один .cpp файл                      | Для быстрой интеграции |
| Vcpkg/Conan | Пакетные менеджеры                  | Для Windows            |

---

## CMake

### Как подмодуль

```bash
git submodule add https://github.com/zeux/meshoptimizer.git external/meshoptimizer
```

```cmake
# CMakeLists.txt
add_subdirectory(external/meshoptimizer)

target_link_libraries(YourTarget PRIVATE meshoptimizer)
```

### find_package

```cmake
find_package(meshoptimizer REQUIRED)

target_link_libraries(YourTarget PRIVATE meshoptimizer::meshoptimizer)
```

### Опции CMake

| Опция                       | По умолчанию | Описание                             |
|-----------------------------|--------------|--------------------------------------|
| `MESHOPT_BUILD_SHARED_LIBS` | OFF          | Сборка как shared library            |
| `MESHOPT_BUILD_DEMO`        | OFF          | Сборка демо                          |
| `MESHOPT_BUILD_EXAMPLES`    | OFF          | Сборка примеров                      |
| `MESHOPT_BUILD_TESTS`       | OFF          | Сборка тестов                        |
| `MESHOPT_STABLE_EXPORTS`    | OFF          | Экспортировать только стабильные API |

---

## Header-only стиль

Библиотека состоит из заголовка `meshoptimizer.h` и набора `.cpp` файлов.

```cpp
// meshoptimizer.h — C/C++ заголовок
#include <meshoptimizer.h>

// .cpp файлы (подключить нужные):
// - allocator.cpp
// - clusterizer.cpp
// - indexanalyzer.cpp
// - indexcodec.cpp
// - indexgenerator.cpp
// - meshletcodec.cpp
// - overdrawoptimizer.cpp
// - quantization.cpp
// - simplifier.cpp
// - spatialorder.cpp
// - stripifier.cpp
// - vcacheoptimizer.cpp
// - vertexcodec.cpp
// - vertexfilter.cpp
// - vfetchoptimizer.cpp
```

### Выбор файлов для сборки

| Функциональность | Файлы                                                  |
|------------------|--------------------------------------------------------|
| Indexing         | `indexgenerator.cpp`, `allocator.cpp`                  |
| Vertex Cache     | `vcacheoptimizer.cpp`, `allocator.cpp`                 |
| Overdraw         | `overdrawoptimizer.cpp`, `allocator.cpp`               |
| Vertex Fetch     | `vfetchoptimizer.cpp`, `allocator.cpp`                 |
| Compression      | `indexcodec.cpp`, `vertexcodec.cpp`, `allocator.cpp`   |
| Simplification   | `simplifier.cpp`, `allocator.cpp`                      |
| Meshlets         | `clusterizer.cpp`, `meshletutils.cpp`, `allocator.cpp` |
| Analysis         | `indexanalyzer.cpp`, `rasterizer.cpp`, `allocator.cpp` |

---

## Amalgamated Build

Конкатенация всех файлов в один для упрощения сборки:

```bash
# Linux/macOS
cat src/meshoptimizer.h > meshoptimizer_all.h
echo '#include "meshoptimizer_all.h"' > meshoptimizer_all.cpp
cat src/*.cpp >> meshoptimizer_all.cpp

# Windows PowerShell
Get-Content src/meshoptimizer.h | Set-Content meshoptimizer_all.h
"`#include `"meshoptimizer_all.h`"" | Set-Content meshoptimizer_all.cpp
Get-Content src/*.cpp | Add-Content meshoptimizer_all.cpp
```

Использование:

```cpp
// Только один файл для компиляции
#include "meshoptimizer_all.h"

// meshoptimizer_all.cpp компилируется как часть проекта
```

---

## Vcpkg

```bash
vcpkg install meshoptimizer
```

```cmake
find_package(meshoptimizer REQUIRED)
target_link_libraries(YourTarget PRIVATE meshoptimizer::meshoptimizer)
```

---

## Conan

```bash
conan install meshoptimizer/1.0@
```

---

## SIMD-оптимизации

Библиотека автоматически использует SIMD при доступности:

| Архитектура | SIMD      | Условие                                   |
|-------------|-----------|-------------------------------------------|
| x86-64      | SSE4.1    | Компилятор поддерживает, CPU имеет SSE4.1 |
| x86-64      | AVX/AVX2  | Компилятор поддерживает, CPU имеет AVX    |
| ARM         | NEON      | Компилятор поддерживает                   |
| WebAssembly | WASM SIMD | Компилятор поддерживает                   |

### Явное управление SIMD

```cpp
// Проверка поддержки во время выполнения
// Библиотека автоматически выбирает оптимальный путь
// Нет необходимости в ручной настройке
```

---

## Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(MeshOptimizerExample)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/meshoptimizer)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE meshoptimizer)
```

---

## Требования к компилятору

| Компилятор | Минимальная версия |
|------------|--------------------|
| GCC        | 4.8                |
| Clang      | 3.4                |
| MSVC       | 2015 (1900)        |
| Intel C++  | 16                 |

---

## Проверка установки

```cpp
#include <meshoptimizer.h>
#include <cstdio>

int main() {
    printf("meshoptimizer version: %d\n", MESHOPTIMIZER_VERSION);

    // Тестовая квантизация
    float value = 0.5f;
    unsigned short half = meshopt_quantizeHalf(value);
    float restored = meshopt_dequantizeHalf(half);
    printf("Quantize test: %.3f -> %u -> %.3f\n", value, half, restored);

    return 0;
}
