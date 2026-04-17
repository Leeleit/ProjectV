# Интеграция Tracy

**🟢 Уровень 1: Начинающий**

## На этой странице

- [CMake интеграция](#cmake-интеграция)
- [Базовые настройки](#базовые-настройки)
- [Конфигурационные макросы](#конфигурационные-макросы)
- [Настройка под разные платформы](#настройка-под-разные-платформы)
- [Управление данными профилирования](#управление-данными-профилирования)
- [Интеграция с графическими API](#интеграция-с-графическими-api)
- [Интеграция с языками программирования](#интеграция-с-языками-программирования)
- [Следующие шаги](#следующие-шаги)

---

## CMake интеграция

### Простейшая интеграция

```cmake
# Включение Tracy в проект
add_subdirectory(external/tracy)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Условная компиляция

```cmake
# Опция для включения/выключения Tracy
option(TRACY_ENABLE "Enable Tracy profiling" ON)

if (TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif ()
```

### Настройка под разные конфигурации сборки

```cmake
# Разные настройки для Debug и Release
if (TRACY_ENABLE)
    add_subdirectory(external/tracy)

    # Общие настройки
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)

    # Debug сборка - полное профилирование
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(YourApp PRIVATE
                TRACY_CALLSTACK=8
                TRACY_MEMORY
                TRACY_VERBOSE
        )
    endif ()

    # Release сборка - минимальный overhead
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(YourApp PRIVATE
                TRACY_ONLY_FRAME
                TRACY_NO_CALLSTACK
                TRACY_LOW_OVERHEAD
        )
    endif ()

    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif ()
```

### Использование пакетных менеджеров

```cmake
# vcpkg
find_package(Tracy CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)

# Conan
# Добавьте tracy/0.x в conanfile.txt
```

---

## Базовые настройки

### Подключение заголовочных файлов

```cpp
// Основной заголовочный файл
#include "tracy/Tracy.hpp"

// Для C проектов
#include "tracy/TracyC.h"

// Для конкретных графических API
#include "tracy/TracyVulkan.hpp"  // Vulkan
#include "tracy/TracyOpenGL.hpp"  // OpenGL
#include "tracy/TracyD3D11.hpp"   // Direct3D 11
#include "tracy/TracyD3D12.hpp"   // Direct3D 12
```

### Минимальная инициализация

```cpp
// Никакой явной инициализации не требуется
// Tracy автоматически инициализируется при первом использовании

int main() {
    // Tracy уже готов к работе
    while (running) {
        FrameMark;  // Отметка кадра
        // ...
    }
    return 0;
}
```

### Ручное управление временем жизни

```cpp
// Если требуется контроль над инициализацией/очисткой
#include "tracy/Tracy.hpp"

int main() {
    // Ручная инициализация (опционально)
    tracy::StartCapture();
    
    try {
        // Основной код приложения...
        while (running) {
            FrameMark;
            // ...
        }
    } catch (...) {
        // Обработка исключений
    }
    
    // Ручная очистка
    tracy::StopCapture();
    return 0;
}
```

---

## Конфигурационные макросы

### Основные макросы

| Макрос                       | Значение по умолчанию | Описание                                      |
|------------------------------|-----------------------|-----------------------------------------------|
| `TRACY_ENABLE`               | Не определено         | Включить Tracy                                |
| `TRACY_ON_DEMAND`            | 0                     | On-demand profiling                           |
| `TRACY_NO_FRAME_IMAGE`       | 0                     | Отключить захват кадров                       |
| `TRACY_NO_VSYNC_CAPTURE`     | 0                     | Отключить vsync capture                       |
| `TRACY_NO_CONTEXT_SWITCH`    | 0                     | Отключить отслеживание переключений контекста |
| `TRACY_NO_SAMPLING`          | 0                     | Отключить sampling                            |
| `TRACY_NO_BROADCAST`         | 0                     | Отключить broadcast сообщения                 |
| `TRACY_NO_CALLSTACK`         | 0                     | Отключить стек вызовов                        |
| `TRACY_NO_CALLSTACK_INLINES` | 0                     | Отключить inline функции в стеке              |
| `TRACY_NO_EXIT`              | 0                     | Не завершать приложение при ошибках           |
| `TRACY_NO_SYSTEM_TRACING`    | 0                     | Отключить системное трассирование             |
| `TRACY_NO_CODE_TRANSFER`     | 0                     | Отключить передачу кода                       |
| `TRACY_NO_STATISTICS`        | 0                     | Отключить статистику                          |

### Расширенные макросы

| Макрос                  | Значение по умолчанию | Описание                         |
|-------------------------|-----------------------|----------------------------------|
| `TRACY_MANUAL_LIFETIME` | 0                     | Ручное управление временем жизни |
| `TRACY_DELAYED_INIT`    | 0                     | Отложенная инициализация         |
| `TRACY_FIBERS`          | 0                     | Поддержка fibers                 |
| `TRACY_VULKAN`          | 0                     | Поддержка Vulkan                 |
| `TRACY_OPENGL`          | 0                     | Поддержка OpenGL                 |
| `TRACY_DIRECTX`         | 0                     | Поддержка DirectX                |
| `TRACY_MEMORY`          | 0                     | Поддержка memory profiling       |
| `TRACY_LOW_OVERHEAD`    | 0                     | Низкий overhead режим            |
| `TRACY_CALLSTACK`       | 0                     | Глубина стека вызовов (0-62)     |
| `TRACY_ONLY_FRAME`      | 0                     | Только отметки кадров            |
| `TRACY_VERBOSE`         | 0                     | Подробные сообщения              |

### Пример конфигурации

```cpp
// В коде или через CMake
#define TRACY_ENABLE
#define TRACY_CALLSTACK 8      // Глубина стека 8 фреймов
#define TRACY_MEMORY          // Включить memory profiling
#define TRACY_NO_FRAME_IMAGE  // Отключить захват кадров (экономия памяти)
#define TRACY_LOW_OVERHEAD    // Низкий overhead режим
```

---

## Настройка под разные платформы

### Windows

```cmake
# Дополнительные настройки для Windows
if (WIN32)
    target_compile_definitions(YourApp PRIVATE
            _WIN32_WINNT=0x0601  # Windows 7+
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
endif ()
```

### Linux

```cmake
# Дополнительные настройки для Linux
if (UNIX AND NOT APPLE)
    # Tracy требует libatomic на некоторых архитектурах
    find_library(ATOMIC_LIBRARY atomic)
    if (ATOMIC_LIBRARY)
        target_link_libraries(YourApp PRIVATE ${ATOMIC_LIBRARY})
    endif ()

    # Для sampling профилирования могут потребоваться права
    target_compile_definitions(YourApp PRIVATE TRACY_NO_SAMPLING)
endif ()
```

### macOS

```cmake
# Дополнительные настройки для macOS
if (APPLE)
    target_compile_definitions(YourApp PRIVATE
            TRACY_NO_SAMPLING  # Sampling требует root права
    )
endif ()
```

### Android

```cmake
# Дополнительные настройки для Android
if (ANDROID)
    target_compile_definitions(YourApp PRIVATE
            TRACY_NO_SAMPLING
            TRACY_NO_CONTEXT_SWITCH
            TRACY_NO_SYSTEM_TRACING
    )

    # Использование ATrace для системного профилирования
    find_library(log-lib log)
    target_link_libraries(YourApp PRIVATE ${log-lib})
endif ()
```

---

## Управление данными профилирования

### Настройка буфера

```cpp
// Установка размера буфера (по умолчанию 64 MB)
tracy::SetBufferSize(32 * 1024 * 1024);  // 32 MB

// Использование circular buffer
tracy::SetCircularBufferSize(64 * 1024 * 1024);  // 64 MB circular
```

### Управление подключением

```cpp
// Установка адреса сервера (по умолчанию localhost:8947)
tracy::SetServerAddress("192.168.1.100:8947");

// Установка пароля для подключения
tracy::SetServerPassword("mysecretpassword");

// Проверка подключения
if (tracy::IsConnected()) {
    TracyMessageL("Connected to Tracy server");
}
```

### Включение/выключение типов данных

```cpp
// Включение/выключение отслеживания зон
tracy::EnableZoneTracking(true);  // По умолчанию true

// Включение/выключение отслеживания кадров
tracy::EnableFrameTracking(true);  // По умолчанию true

// Включение/выключение отслеживания графиков
tracy::EnablePlotTracking(false);  // Экономия CPU

// Включение/выключение отслеживания памяти
tracy::EnableMemoryTracking(true);  // Только для отладки
```

### Управление сессиями профилирования

```cpp
// Начало сессии профилирования
tracy::StartCapture();
TracyMessageL("Profiling session started");

// Остановка сессии
tracy::StopCapture();
TracyMessageL("Profiling session stopped");

// Сохранение данных в файл (через сервер Tracy)
// Запустите сервер с флагом: tracy-profiler -o session.tracy
```

---

## Интеграция с графическими API

### Vulkan

```cpp
// Требуется определение TRACY_VULKAN
#include "tracy/TracyVulkan.hpp"

// Инициализация Vulkan контекста
TracyVkContext tracyCtx = TracyVkContext(
    physicalDevice, device, queue, cmdbuf,
    vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
    vkGetCalibratedTimestampsEXT
);

// Использование в командных буферах
{
    TracyVkZone(tracyCtx, cmdBuffer, "RenderPass");
    vkCmdDrawIndexed(cmdBuffer, ...);
}

// Сбор данных
TracyVkCollect(tracyCtx, cmdBuffer);
```

### OpenGL

```cpp
// Требуется определение TRACY_OPENGL
#include "tracy/TracyOpenGL.hpp"

// Инициализация OpenGL контекста
auto* tracyCtx = TracyGpuContext;

// Профилирование
TracyGpuZone("OpenGL Draw");
glDrawElements(...);
```

### Direct3D 11/12

```cpp
// Требуется определение TRACY_DIRECTX
#include "tracy/TracyD3D11.hpp"  // или TracyD3D12.hpp

// Инициализация
auto* tracyCtx = TracyD3D11Context(device, immediateContext);

// Профилирование
TracyD3D11Zone(tracyCtx, "D3D11 Draw");
immediateContext->DrawIndexed(...);
```

### Metal

```cpp
// Требуется определение TRACY_METAL
#include "tracy/TracyMetal.hpp"

// Инициализация
TracyMetalContext tracyCtx = TracyMetalContext(device, queue);

// Профилирование
TracyMetalZone(tracyCtx, "Metal Render");
[commandBuffer drawIndexedPrimitives...];
```

---

## Интеграция с языками программирования

### C

```c
#include "tracy/TracyC.h"

void my_function(void) {
    TracyCZoneN(ctx, "My Function", 1);  // 1 = активна
    // ...
    TracyCZoneEnd(ctx);
}
```

### Lua

```lua
-- Требуется tracy.lua
local tracy = require "tracy"

function my_function()
    tracy.ZoneBegin("Lua Function")
    -- ...
    tracy.ZoneEnd()
end
```

### Python

```python
# Требуется pytracy
import pytracy


@pytracy.zone("Python Function")
def my_function():
    # ...
    pass
```

### Rust

```rust
// Требуется crate tracy-client
use tracy_client::*;

fn my_function() {
    let _span = span!("Rust Function");
    // ...
}
```

---

## Следующие шаги

### После настройки интеграции:

1. **Изучите инструменты профилирования:** [Быстрый старт](../tracy/quickstart.md) — основные макросы и их использование
2. **Освойте продвинутые возможности:** [Справочник API](../tracy/api-reference.md) — все макросы и функции
3. **Решите проблемы:** [Решение проблем](../tracy/troubleshooting.md) — диагностика и исправление ошибок
4. **Оптимизируйте производительность:** [Основные понятия](../tracy/concepts.md#overhead-и-производительность) —
   снижение overhead

### Для специфичных сценариев воксельных движков:

- **[Специализированные паттерны интеграции](projectv-integration.md)** — продвинутые сценарии для воксельных движков

---

**См. также:**

- [Быстрый старт](../tracy/quickstart.md) — минимальные примеры использования
- [Глоссарий](../tracy/glossary.md) — термины и определения
- [Основные понятия](../tracy/concepts.md) — архитектура и типы инструментов

← [На главную документацию Tracy](../tracy/README.md)
