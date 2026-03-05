# Кросс-платформенность

**🔴 Уровень 3: Продвинутый** — Конфигурации для Windows/Linux и кросс-компиляция.

---

## 1. Конфигурация для Windows

### Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(ProjectV VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific
if(WIN32)
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Зависимости
find_package(Vulkan REQUIRED)
add_subdirectory(external/SDL)
add_subdirectory(external/volk)
add_subdirectory(external/VMA)

add_executable(ProjectV src/main.cpp)
target_link_libraries(ProjectV PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    volk
    GPUOpen::VulkanMemoryAllocator
)
```

### Флаги MSVC

```cmake
if(MSVC)
    target_compile_options(ProjectV PRIVATE
        /W4           # Уровень предупреждений
        /permissive-  # Строгое соответствие стандартам
        /Zc:__cplusplus
        /utf-8
    )

    # ProjectV: Отключение исключений и RTTI (КРИТИЧНО)
    target_compile_options(ProjectV PRIVATE
        /EHs-c-       # Отключить исключения
        /GR-          # Отключить RTTI
    )

    # Оптимизации
    target_compile_options(ProjectV PRIVATE
        $<$<CONFIG:Release>:/O2 /fp:fast>
        $<$<CONFIG:Debug>:/Od /Zi>
    )

    # SIMD
    target_compile_options(ProjectV PRIVATE /arch:AVX2)
endif()
```

### Копирование DLL

```cmake
if(WIN32)
    add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:ProjectV>
    )
endif()
```

---

## 2. Конфигурация для Linux

### Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(ProjectV VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Linux-specific
if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Зависимости через pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(VULKAN REQUIRED vulkan)

add_subdirectory(external/SDL)
add_subdirectory(external/volk)
add_subdirectory(external/VMA)

add_executable(ProjectV src/main.cpp)
target_include_directories(ProjectV PRIVATE ${VULKAN_INCLUDE_DIRS})
target_link_libraries(ProjectV PRIVATE
    ${VULKAN_LIBRARIES}
    SDL3::SDL3
    volk
    GPUOpen::VulkanMemoryAllocator
    pthread dl
)
```

### Флаги GCC/Clang

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProjectV PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror=return-type
        -Werror=non-virtual-dtor
    )

    # ProjectV: Отключение исключений и RTTI (КРИТИЧНО)
    target_compile_options(ProjectV PRIVATE
        -fno-exceptions
        -fno-rtti
    )

    # Оптимизации
    target_compile_options(ProjectV PRIVATE
        $<$<CONFIG:Release>:-O3 -ffast-math -flto>
        $<$<CONFIG:Debug>:-O0 -g>
    )

    # SIMD
    target_compile_options(ProjectV PRIVATE
        -mavx2 -mfma -march=native
    )
endif()
```

---

## 3. Платформенно-специфичный код

### Условная компиляция в CMake

```cmake
target_compile_definitions(ProjectV PRIVATE
    $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS=1>
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
)

# Vulkan surface
if(WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
endif()
```

### В коде

```cpp
// platform_config.h
#pragma once

#ifdef _WIN32
    #define PLATFORM_WINDOWS 1
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define VK_USE_PLATFORM_XLIB_KHR
#endif
```

---

## 4. Кросс-компиляция

### Toolchain для MinGW (Windows из Linux)

**cmake/mingw-w64.cmake:**

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
```

**Использование:**

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64.cmake ..
```

---

## 5. Решение проблем

### Разные пути к Vulkan SDK

```cmake
find_package(Vulkan QUIET)

if(NOT Vulkan_FOUND)
    if(WIN32)
        set(VULKAN_SDK "$ENV{VULKAN_SDK}")
        if(EXISTS "${VULKAN_SDK}/Include/vulkan/vulkan.h")
            set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK}/Include")
            set(Vulkan_LIBRARIES "${VULKAN_SDK}/Lib/vulkan-1.lib")
        endif()
    elseif(UNIX)
        find_path(Vulkan_INCLUDE_DIRS vulkan/vulkan.h
            PATHS /usr/include /usr/local/include
        )
        find_library(Vulkan_LIBRARIES vulkan
            PATHS /usr/lib /usr/local/lib
        )
    endif()
endif()
```

### Разные флаги линковки

```cmake
target_link_libraries(ProjectV PRIVATE
    $<$<PLATFORM_ID:Windows>:user32 gdi32 shell32>
    $<$<PLATFORM_ID:Linux>:pthread dl X11>
)
```

---

## 6. CI/CD мультиплатформа

```yaml
# .github/workflows/build.yml
name: Build ProjectV
on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [windows-latest, ubuntu-latest]

    runs-on: ${{ matrix.os }}

    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive

    - name: Install dependencies (Linux)
      if: runner.os == 'Linux'
      run: |
        sudo apt-get update
        sudo apt-get install -y libvulkan-dev libx11-dev

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --config Release --parallel
```
