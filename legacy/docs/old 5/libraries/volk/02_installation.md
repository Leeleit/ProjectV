# Установка volk

**🟢 Уровень 1: Базовый**

Сборка и подключение к проекту.

---

## CMake targets

volk предоставляет два CMake target'а:

| Target         | Описание               |
|----------------|------------------------|
| `volk`         | Статическая библиотека |
| `volk_headers` | Header-only режим      |

---

## Статическая библиотека

### CMakeLists.txt

```cmake
add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk)
```

### Платформенные defines

Передайте через `VOLK_STATIC_DEFINES`:

```cmake
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    # Или для Wayland: VK_USE_PLATFORM_WAYLAND_KHR
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()

add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk)
```

---

## Header-only режим

### CMakeLists.txt

```cmake
add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk_headers)
```

### Код

В одном .cpp файле:

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"
```

В остальных файлах:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
```

**Важно:** Файл `volk.c` не компилируется в этом режиме. Он должен находиться в той же директории, что и `volk.h`.

---

## Установка и find_package

### Сборка с установкой

```bash
cmake -DVOLK_INSTALL=ON ..
cmake --build . --target install
```

### Использование в проекте

```cmake
find_package(volk CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE volk::volk)
```

Импортированные targets:

- `volk::volk` — статическая библиотека
- `volk::volk_headers` — header-only

---

## VK_NO_PROTOTYPES

Обязательный макрос для работы volk.

### Проблема без макроса

При включении `vulkan.h` без `VK_NO_PROTOTYPES`:

```cpp
#include <vulkan/vulkan.h>  // Объявляет vkCreateInstance как extern-функцию
// Линкер ищет реализацию в vulkan-1.dll
```

volk объявляет функции как указатели. Без макроса возникают конфликты символов.

### Решение

Определите макрос до включения любого заголовка, который тянет `vulkan.h`:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"  // volk сам включит vulkan.h без прототипов
```

### CMake

```cmake
target_compile_definitions(YourApp PRIVATE VK_NO_PROTOTYPES)
```

---

## Конфигурационные макросы

| Макрос                      | Описание                                 |
|-----------------------------|------------------------------------------|
| `VOLK_IMPLEMENTATION`       | Включает реализацию в header-only режиме |
| `VOLK_NAMESPACE`            | Помещает символы в namespace `volk::`    |
| `VOLK_NO_DEVICE_PROTOTYPES` | Скрывает прототипы device-функций        |
| `VOLK_VULKAN_H_PATH`        | Кастомный путь к vulkan.h                |

---

## VOLK_NAMESPACE

Помещает все символы volk в namespace `volk::`. Требует компиляции volk.c как C++.

### CMake

```cmake
set(VOLK_NAMESPACE ON)
add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk)
```

### Код

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

volk::volkInitialize();
volk::volkLoadInstance(instance);
volk::volkLoadDevice(device);
```

Используется для избежания конфликтов при смешивании volk с прямым использованием Vulkan.

---

## VOLK_NO_DEVICE_PROTOTYPES

Скрывает прототипы device-функций. Используется при table-based интерфейсе.

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_NO_DEVICE_PROTOTYPES
#include "volk.h"

// Device-функции недоступны напрямую
// vkCmdDraw(...); // Ошибка компиляции

// Нужно использовать таблицы
VolkDeviceTable table;
volkLoadDeviceTable(&table, device);
table.vkCmdDraw(...);
```

---

## Проверка установки

```cpp
#include "volk.h"
#include <stdio.h>

int main() {
    if (volkInitialize() != VK_SUCCESS) {
        printf("Vulkan loader not found\n");
        return 1;
    }

    uint32_t version = volkGetInstanceVersion();
    printf("Vulkan %u.%u.%u loaded\n",
           VK_VERSION_MAJOR(version),
           VK_VERSION_MINOR(version),
           VK_VERSION_PATCH(version));

    return 0;
}
