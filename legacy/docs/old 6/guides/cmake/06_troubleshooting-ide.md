# Решение проблем, IDE и шаблоны

**🟡 Уровень 2: Средний** — Диагностика ошибок, настройка CLion и готовые шаблоны.

---

# Часть 1: Решение проблем

## 1. Ошибки поиска зависимостей

### "Could NOT find Vulkan"

```bash
# Установите Vulkan SDK, затем:
cmake -DCMAKE_PREFIX_PATH="C:/VulkanSDK/1.3.250.0" ..
# Или:
set VULKAN_SDK=C:\VulkanSDK\1.3.250.0
```

### "SDL3::SDL3 not found"

```bash
git submodule update --init external/SDL
cmake --build build --target clean
cmake -B build
```

---

## 2. Ошибки линковки

### "undefined reference to `vkCreateInstance`"

**Причина:** Неправильный порядок линковки.
**Решение:**

```cmake
target_link_libraries(ProjectV PRIVATE
    Vulkan::Vulkan
    volk  # После Vulkan!
)
```

### "multiple definition" при линковке volk

**Решение:**

```cmake
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(external/volk)
```

### "SDL3.dll not found" при запуске

```cmake
add_custom_command(TARGET ProjectV POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3>
    $<TARGET_FILE_DIR:ProjectV>
)
```

---

## 3. Ошибки компиляции

### "C++26 feature not supported"

Обновите компилятор:

- **MSVC:** Visual Studio 2022 v17.8+
- **GCC:** 13+
- **Clang:** 16+

Или понизьте стандарт: `set(CMAKE_CXX_STANDARD 20)`

### "CMake 3.20+ required"

```bash
cmake --version
# Обновите CMake
```

---

## 4. Проблемы с подмодулями

### "submodule not found"

```bash
git clone --recursive https://github.com/user/ProjectV
# Или:
git submodule update --init --recursive
```

### "detached HEAD" в подмодулях

```bash
git submodule foreach git checkout main
git submodule foreach git pull origin main
```

---

## 5. Отладка CMake

```bash
# Показать все переменные
cmake -LA build

# Подробный вывод
cmake --trace ..
cmake --build build --verbose

# Граф зависимостей
cmake --graphviz=deps.dot ..
dot -Tpng deps.dot -o deps.png
```

---

# Часть 2: Интеграция с CLion

## 6. Начальная настройка

1. **File → Open** → выберите папку `ProjectV`
2. CLion автоматически обнаружит `CMakeLists.txt`
3. Нажмите **Open as Project**

### Рекомендуемые профили

| Профиль        | Build type     | Использование                   |
|----------------|----------------|---------------------------------|
| Debug          | Debug          | Отладка                         |
| Release        | Release        | Тестирование производительности |
| RelWithDebInfo | RelWithDebInfo | Основная разработка             |

---

## 7. Настройка инструментов

### Форматирование

1. **File → Settings → Editor → Code Style → C/C++**
2. Нажмите **Set from...** → `.clang-format`
3. Включите **Reformat on save**

### clang-tidy

1. **File → Settings → Editor → Inspections → Clang-Tidy**
2. Включите **Use clang-tidy**
3. Укажите: `${projectDir}/.clang-tidy`

---

## 8. Отладка Vulkan

**Run → Edit Configurations:**

```
Program arguments: --validation --gpu-debug

Environment variables:
VK_LOADER_DEBUG=all
VK_LAYER_PATH=C:/VulkanSDK/1.3.250.0/Bin
```

---

## 9. Горячие клавиши

| Действие         | Сочетание     |
|------------------|---------------|
| Сборка           | Ctrl+F9       |
| Запуск           | Shift+F10     |
| Отладка          | Shift+F9      |
| Переконфигурация | Ctrl+Shift+F9 |
| Найти везде      | Double Shift  |

---

## 10. Решение проблем CLion

### "CMake executable not found"

**File → Settings → Build → CMake** → укажите путь к CMake.

### Медленная индексация

**File → Settings → CMake** → добавьте в **Exclude paths**:

- `external/`
- `build/`

---

# Часть 3: Шаблоны CMakeLists.txt

## 11. Минимальный шаблон

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(MyProject main.cpp)

if(MSVC)
    target_compile_options(MyProject PRIVATE /W4)
else()
    target_compile_options(MyProject PRIVATE -Wall -Wextra)
endif()
```

---

## 12. Шаблон для библиотеки

```cmake
cmake_minimum_required(VERSION 3.20)
project(ModuleName VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(ModuleName STATIC
    src/module.cpp
)

target_include_directories(ModuleName
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ModuleName PRIVATE
    # Зависимости
)
```

---

## 13. Шаблон для примера

```cmake
cmake_minimum_required(VERSION 3.20)
project(ExampleName LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME}
    example_main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
)

# Копирование DLL (Windows)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()
```

---

## 14. Шаблон для тестов

```cmake
cmake_minimum_required(VERSION 3.20)
project(ModuleTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME}
    test_main.cpp
    test_module.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../tests  # doctest.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ModuleName  # Тестируемый модуль
)

enable_testing()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
```

---

## 15. Полный шаблон ProjectV

```cmake
cmake_minimum_required(VERSION 3.20)
project(ProjectV VERSION 0.1.0 LANGUAGES CXX C)

# Стандарты
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Платформенные настройки
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Пути вывода
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Зависимости
find_package(Vulkan REQUIRED)
add_subdirectory(external/SDL)
add_subdirectory(external/volk)
add_subdirectory(external/VMA)

# Приложение
add_executable(ProjectV src/main.cpp)

target_link_libraries(ProjectV PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    volk
    GPUOpen::VulkanMemoryAllocator
)

# Оптимизации
if(MSVC)
    target_compile_options(ProjectV PRIVATE /W4 /permissive-)
    target_compile_options(ProjectV PRIVATE /arch:AVX2)
else()
    target_compile_options(ProjectV PRIVATE -Wall -Wextra)
    target_compile_options(ProjectV PRIVATE -mavx2 -mfma)
endif()

# Unity build
set_target_properties(ProjectV PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)

# Копирование DLL
if(WIN32)
    add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:ProjectV>
    )
endif()
```
