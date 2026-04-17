# Установка Tracy

**🟢 Уровень 1: Начинающий** — Интеграция Tracy в проект через CMake и пакетные менеджеры.

---

## CMake интеграция

### Простейшая интеграция

```cmake
add_subdirectory(external/tracy)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Условное включение

```cmake
option(TRACY_ENABLE "Enable Tracy profiling" ON)

if(TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif()
```

### Конфигурация для Debug/Release

```cmake
if(TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Debug: полное профилирование
        target_compile_definitions(YourApp PRIVATE
            TRACY_CALLSTACK=8
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Release: минимальный overhead
        target_compile_definitions(YourApp PRIVATE
            TRACY_ONLY_FRAME
            TRACY_NO_CALLSTACK
            TRACY_LOW_OVERHEAD
        )
    endif()

    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif()
```

---

## Пакетные менеджеры

### vcpkg

```cmake
find_package(Tracy CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

Установка пакета:

```bash
vcpkg install tracy
```

### Conan

В `conanfile.txt`:

```ini
[requires]
tracy/0.11

[generators]
CMakeDeps
CMakeToolchain
```

В `CMakeLists.txt`:

```cmake
find_package(tracy REQUIRED)
target_link_libraries(YourApp PRIVATE tracy::tracy)
```

---

## Конфигурационные макросы

### Основные макросы

| Макрос                       | По умолчанию | Описание                                      |
|------------------------------|--------------|-----------------------------------------------|
| `TRACY_ENABLE`               | Не определён | Включить Tracy                                |
| `TRACY_ON_DEMAND`            | 0            | Профилирование по запросу                     |
| `TRACY_NO_FRAME_IMAGE`       | 0            | Отключить захват изображений кадра            |
| `TRACY_NO_VSYNC_CAPTURE`     | 0            | Отключить захват vsync                        |
| `TRACY_NO_CONTEXT_SWITCH`    | 0            | Отключить отслеживание переключений контекста |
| `TRACY_NO_SAMPLING`          | 0            | Отключить sampling                            |
| `TRACY_NO_BROADCAST`         | 0            | Отключить broadcast                           |
| `TRACY_NO_CALLSTACK`         | 0            | Отключить стек вызовов                        |
| `TRACY_NO_CALLSTACK_INLINES` | 0            | Отключить inline-функции в стеке              |
| `TRACY_NO_EXIT`              | 0            | Не завершать приложение при ошибках           |
| `TRACY_NO_SYSTEM_TRACING`    | 0            | Отключить системную трассировку               |
| `TRACY_NO_CODE_TRANSFER`     | 0            | Отключить передачу кода                       |
| `TRACY_NO_STATISTICS`        | 0            | Отключить статистику                          |

### Расширенные макросы

| Макрос                  | По умолчанию | Описание                         |
|-------------------------|--------------|----------------------------------|
| `TRACY_MANUAL_LIFETIME` | 0            | Ручное управление временем жизни |
| `TRACY_DELAYED_INIT`    | 0            | Отложенная инициализация         |
| `TRACY_FIBERS`          | 0            | Поддержка fibers                 |
| `TRACY_CALLSTACK`       | 0            | Глубина стека (0-62)             |
| `TRACY_ONLY_FRAME`      | 0            | Только отметки кадров            |
| `TRACY_LOW_OVERHEAD`    | 0            | Режим низкого overhead           |
| `TRACY_VERBOSE`         | 0            | Подробные сообщения              |

### Пример конфигурации

```cpp
// В коде или через CMake
#define TRACY_ENABLE
#define TRACY_CALLSTACK 8      // Глубина стека
#define TRACY_MEMORY          // Memory profiling
#define TRACY_NO_FRAME_IMAGE  // Экономия памяти
#define TRACY_LOW_OVERHEAD    // Минимальный overhead
```

---

## Платформо-специфичные настройки

### Windows

```cmake
if(WIN32)
    target_compile_definitions(YourApp PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7+
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()
```

### Linux

```cmake
if(UNIX AND NOT APPLE)
    # Tracy требует libatomic на некоторых архитектурах
    find_library(ATOMIC_LIBRARY atomic)
    if(ATOMIC_LIBRARY)
        target_link_libraries(YourApp PRIVATE ${ATOMIC_LIBRARY})
    endif()

    # Sampling требует прав root
    target_compile_definitions(YourApp PRIVATE TRACY_NO_SAMPLING)
endif()
```

### macOS

```cmake
if(APPLE)
    # Sampling требует root права
    target_compile_definitions(YourApp PRIVATE TRACY_NO_SAMPLING)
endif()
```

### Android

```cmake
if(ANDROID)
    target_compile_definitions(YourApp PRIVATE
        TRACY_NO_SAMPLING
        TRACY_NO_CONTEXT_SWITCH
        TRACY_NO_SYSTEM_TRACING
    )

    find_library(log-lib log)
    target_link_libraries(YourApp PRIVATE ${log-lib})
endif()
```

---

## Заголовочные файлы

### Основные

```cpp
#include "tracy/Tracy.hpp"    // C++ интерфейс
#include "tracy/TracyC.h"     // C интерфейс
```

### GPU профилирование

```cpp
#include "tracy/TracyVulkan.hpp"   // Vulkan
#include "tracy/TracyOpenGL.hpp"   // OpenGL
#include "tracy/TracyD3D11.hpp"    // Direct3D 11
#include "tracy/TracyD3D12.hpp"    // Direct3D 12
#include "tracy/TracyMetal.hmm"    // Metal
#include "tracy/TracyCUDA.hpp"     // CUDA
#include "tracy/TracyOpenCL.hpp"   // OpenCL
```

### Языковые биндинги

```cpp
#include "tracy/TracyLua.hpp"      // Lua
```

---

## Сборка сервера Tracy

### Зависимости

**Windows:**

- Visual Studio 2019+ или MSVC
- Windows SDK

**Linux:**

- GCC 9+ или Clang 10+
- libpthread, libdl
- libgtk-3-dev, libtbb-dev (опционально)

**macOS:**

- Xcode 12+
- Homebrew зависимости

### Сборка

```bash
cd external/tracy/profiler
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Альтернативы

**Web-версия:**

```bash
# Требует Emscripten
cd external/tracy/profiler
emcmake cmake .
emmake make
```

---

## Проверка установки

### Код для проверки

```cpp
#include "tracy/Tracy.hpp"

int main() {
    ZoneScoped;

    FrameMark;
    TracyMessageL("Tracy is working!");

    return 0;
}
```

### Компиляция и запуск

```bash
# Сборка
cmake --build build

# Запуск приложения
./build/YourApp &

# Запуск сервера
./tracy-profiler
```

Если подключение прошло успешно, в интерфейсе Tracy появятся зоны и сообщения.
