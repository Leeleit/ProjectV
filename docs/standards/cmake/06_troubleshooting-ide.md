# СТВ-CMAKE-007: Руководство по устранению неполадок

---

## 1. Область применения

Настоящий документ содержит решения типичных проблем, возникающих при настройке IDE и системы сборки для ProjectV. Все
разработчики ДОЛЖНЫ следовать данным рекомендациям при возникновении проблем совместимости.

---

## 2. Типичные проблемы IDE

### 2.1 CLion

#### Проблема: Ошибки распознавания модулей C++26

**Симптомы:**

- CLion не распознаёт `import std;`
- Подчёркивание синтаксиса в файлах `.cppm`
- Ошибки "module not found"

**Решение:**

1. Обновить CLion до версии 2024.3+
2. Настроить toolchain:

```
Settings → Build → Toolchains
- C++ Compiler: /usr/bin/clang++-18
- CMake: /usr/bin/cmake (3.30+)
- Build Tool: /usr/bin/ninja
```

3. Добавить переменные окружения CMake:

```
Settings → Build → CMake
- CMake options: -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -fmodules -stdlib=libc++"
```

#### Проблема: Ошибки линковки с libc++

**Решение:**

```cmake
# Убедиться, что libc++abi подключена
target_link_options(MyTarget PRIVATE -stdlib=libc++ -lc++abi)
```

---

### 2.2 Visual Studio Code

#### Проблема: IntelliSense не работает с модулями C++26

**Симптомы:**

- Ошибки в #include директивах
- IntelliSense не распознаёт типы из модулей

**Решение:**

Обновить `c_cpp_properties.json`:

```json
{
    "configurations": [
        {
            "name": "Linux-Clang",
            "compilerPath": "/usr/bin/clang++-18",
            "cStandard": "c23",
            "cppStandard": "c++26",
            "intelliSenseMode": "linux-clang-x64",
            "compilerArgs": [
                "-std=c++26",
                "-fmodules",
                "-stdlib=libc++"
            ],
            "compileCommands": "${workspaceFolder}/build/compile_commands.json"
        }
    ],
    "version": 4
}
```

#### Проблема: CMake Tools не обнаруживает Clang

**Решение:**

1. Установить kit вручную:
  - `Ctrl+Shift+P` → "CMake: Select a Kit"
  - Выбрать "Scan for Kits"
  - Выбрать "Clang 18.x"

2. Или настроить `settings.json`:

```json
{
    "cmake.generator": "Ninja",
    "cmake.configureSettings": {
        "CMAKE_CXX_COMPILER": "/usr/bin/clang++-18",
        "CMAKE_C_COMPILER": "/usr/bin/clang-18"
    },
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

---

### 2.3 Visual Studio 2022

#### Проблема: Несовместимость с Clang-модулями

**Примечание:** Visual Studio 2022 НЕ поддерживает модули C++26 в полном объёме. Используйте CLion или VS Code с
расширением CMake Tools.

**Альтернативное решение:**

Использовать Clang Power Tools:

1. Установить расширение "Clang Power Tools"
2. Настроить использование Clang 18+
3. Отключить IntelliSense MSVC:

```
Tools → Options → Text Editor → C/C++ → Advanced
- Disable IntelliSense: True
```

---

## 3. Проблемы модулей C++26

### 3.1 Ошибка: "module 'std' not found"

**Причина:** Clang не может найти прекомпилированный модуль стандартной библиотеки.

**Решение:**

1. Скомпилировать модуль std вручную:

```bash
cd build
clang++ -std=c++26 -stdlib=libc++ -c -x c++ std.cppm -o std.pcm
```

2. Указать путь к модулю в CMake:

```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprebuilt-module-path=${CMAKE_BINARY_DIR}")
```

### 3.2 Ошибка: "BMI file is corrupt"

**Причина:** Несоответствие версий прекомпилированных модулей.

**Решение:**

Полная пересборка:

```bash
rm -rf build/*
cmake -B build -GNinja -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build
```

### 3.3 Ошибка: "import declaration must be a global declaration"

**Причина:** Использование `import` внутри пространства имён или функции.

**Решение:**

```cpp
// НЕПРАВИЛЬНО
namespace mylib {
    import std;  // Ошибка
}

// ПРАВИЛЬНО
import std;
namespace mylib {
    // ...
}
```

---

## 4. Проблемы сборки

### 4.1 Ошибка: "undefined reference to ..."

**Причина:** Нарушение порядка зависимостей модулей.

**Решение:**

Проверить порядок компиляции в CMakeLists.txt:

```cmake
# Модули должны быть объявлены в порядке зависимостей
add_subdirectory(external/volk)      # Уровень 0
add_subdirectory(src/core)           # Уровень 1
add_subdirectory(src/render)         # Уровень 2
add_subdirectory(src/app)            # Уровень 3
```

### 4.2 Ошибка: "multiple definition of ..."

**Причина:** Нарушение ODR при использовании `#include` в файлах `.cppm`.

**Решение:**

Использовать PIMPL паттерн для C++ библиотек:

```cpp
// .cppm (интерфейс)
export module ProjectV.Physics;

// БЕЗ #include <Jolt/Jolt.h>

export class PhysicsWorld {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// .cpp (реализация)
module ProjectV.Physics;
#include <Jolt/Jolt.h>  // ЗДЕСЬ разрешено

struct PhysicsWorld::Impl {
    JPH::PhysicsSystem system;
};
```

### 4.3 Ошибка: "relocation truncated to fit"

**Причина:** Превышение размеров секций при большом количестве шаблонов.

**Решение:**

Добавить флаги в CMake:

```cmake
target_compile_options(MyTarget PRIVATE
    -mcmodel=medium
    -ffunction-sections
)
target_link_options(MyTarget PRIVATE
    -Wl,--gc-sections
)
```

---

## 5. Проблемы зависимостей

### 5.1 Ошибка: "Vulkan not found"

**Причина:** Переменная VULKAN_SDK не установлена.

**Решение:**

Linux/macOS:

```bash
export VULKAN_SDK=/path/to/vulkan-sdk/1.4.341.1
```

Windows (PowerShell):

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.341.1"
```

### 5.2 Ошибка: "Could not find SDL3"

**Решение:**

Инициализировать сабмодули:

```bash
git submodule update --init --recursive
```

---

## 6. Проблемы производительности

### 6.1 Медленная компиляция

**Решения:**

1. Включить ccache:

```cmake
option(ENABLE_CCACHE "Включить ccache" ON)
if(ENABLE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    endif()
endif()
```

2. Использовать предкомпилированные заголовки:

```cmake
target_precompile_headers(MyTarget PRIVATE
    <vector>
    <string>
    <memory>
)
```

3. Включить unity-сборку:

```cmake
set_target_properties(MyTarget PROPERTIES UNITY_BUILD ON)
```

### 6.2 Медленная линковка

**Решение:**

Использовать параллельную линковку с mold (Linux):

```cmake
find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
endif()
```

---

## 7. Диагностические команды

### 7.1 Проверка версии компилятора

```bash
clang++ --version
cmake --version
ninja --version
```

### 7.2 Проверка конфигурации CMake

```bash
cmake -B build --debug-output
cat build/CMakeCache.txt | grep CMAKE_CXX_COMPILER
```

### 7.3 Генерация графа зависимостей

```bash
cmake --graphviz=deps.dot build
dot -Tpng deps.dot -o dependencies.png
```

---

## 8. Контакты для поддержки

При возникновении проблем, не описанных в данном документе:

1. Создать issue в репозитории проекта
2. Приложить вывод команд диагностики (раздел 7)
3. Описать шаги для воспроизведения проблемы
