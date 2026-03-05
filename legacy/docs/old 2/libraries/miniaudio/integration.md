# Интеграция miniaudio

**🟡 Уровень 2: Средний**

## На этой странице

- [CMake интеграция](#cmake-интеграция)
- [Single-file реализация](#single-file-реализация)
- [Линковка для разных платформ](#линковка-для-разных-платформ)
- [Конфигурация макросов](#конфигурация-макросов)
- [Интеграция с другими библиотеками](#интеграция-с-другими-библиотеками)
- [Примеры сборки](#примеры-сборки)
- [См. также](#см-также)

---

## CMake интеграция

### Простейшая интеграция

```cmake
add_subdirectory(external/miniaudio)
target_link_libraries(YourApp PRIVATE miniaudio)
```

### Настройка параметров CMake

miniaudio предоставляет несколько CMake опций:

```cmake
option(MA_ENABLE_ONLY_SPECIFIC_BACKENDS "Enable only specific backends" OFF)
option(MA_NO_WAV "Disable WAV decoder" OFF)
option(MA_NO_FLAC "Disable FLAC decoder" OFF)
option(MA_NO_MP3 "Disable MP3 decoder" OFF)
```

### Пример полной конфигурации

```cmake
# Включение только нужных бэкендов
set(MA_ENABLE_ONLY_SPECIFIC_BACKENDS ON)
set(MA_ENABLE_WASAPI ON)
set(MA_ENABLE_DSOUND OFF)
set(MA_ENABLE_WINMM OFF)

add_subdirectory(external/miniaudio)
target_link_libraries(YourApp PRIVATE miniaudio)
```

---

## Single-file реализация

Если вы не используете CMake или хотите максимально простую интеграцию:

### Базовый способ

```c
// В одном файле (.c или .cpp):
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

int main() {
    ma_engine engine;
    ma_engine_init(NULL, &engine);
    // ...
    ma_engine_uninit(&engine);
    return 0;
}
```

### Кастомизация через макросы

```c
// Перед включением заголовочного файла
#define MA_NO_DECODING    // Отключить декодирование
#define MA_NO_ENCODING    // Отключить кодирование
#define MA_NO_GENERATION  // Отключить генераторы шума
#define MA_NO_RESOURCE_MANAGER  // Отключить resource manager
#define MA_NO_NODE_GRAPH  // Отключить node graph
#define MA_NO_ENGINE      // Отключить engine API

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

### Выбор бэкендов

```c
// Включение только определённых бэкендов
#define MA_ENABLE_WASAPI
#define MA_ENABLE_DSOUND
// #define MA_ENABLE_WINMM  // Отключен
// #define MA_ENABLE_ALSA   // Отключен

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

---

## Линковка для разных платформ

miniaudio требует минимальной линковки с системными библиотеками.

### Windows

```cmake
# Ничего дополнительного не требуется
# (WASAPI, DirectSound, WinMM входят в Windows SDK)
```

### Linux

```cmake
# Основные зависимости
target_link_libraries(YourApp PRIVATE dl pthread m)

# Дополнительные зависимости для специфичных бэкендов
if(MA_ENABLE_JACK)
    target_link_libraries(YourApp PRIVATE jack)
endif()
if(MA_ENABLE_PULSEAUDIO)
    target_link_libraries(YourApp PRIVATE pulse)
endif()
```

### macOS/iOS

```cmake
# Framework зависимости (CMake автоматически их обрабатывает)
# Ничего дополнительного указывать не нужно
```

### Android

```cmake
# OpenSL|ES
target_link_libraries(YourApp PRIVATE OpenSLES)

# Или AAudio (если используется)
# target_link_libraries(YourApp PRIVATE aaudio)
```

### Emscripten (WebAssembly)

```cmake
# Ничего дополнительного не требуется
# Web Audio API доступен через JavaScript bindings
```

### BSD системы

```cmake
# OpenBSD (sndio)
target_link_libraries(YourApp PRIVATE sndio)

# FreeBSD (OSS)
target_link_libraries(YourApp PRIVATE ossaudio)
```

---

## Конфигурация макросов

### Основные макросы для кастомизации

| Макрос                   | Описание                    | Значение по умолчанию |
|--------------------------|-----------------------------|-----------------------|
| `MA_NO_DECODING`         | Отключить все декодеры      | OFF                   |
| `MA_NO_ENCODING`         | Отключить все энкодеры      | OFF                   |
| `MA_NO_WAV`              | Отключить декодер WAV       | OFF                   |
| `MA_NO_FLAC`             | Отключить декодер FLAC      | OFF                   |
| `MA_NO_MP3`              | Отключить декодер MP3       | OFF                   |
| `MA_NO_GENERATION`       | Отключить генераторы шума   | OFF                   |
| `MA_NO_RESOURCE_MANAGER` | Отключить resource manager  | OFF                   |
| `MA_NO_NODE_GRAPH`       | Отключить node graph        | OFF                   |
| `MA_NO_ENGINE`           | Отключить engine API        | OFF                   |
| `MA_NO_THREADING`        | Отключить threading support | OFF                   |
| `MA_NO_DEVICE_IO`        | Отключить device I/O        | OFF                   |

### Пример минимальной конфигурации

```c
// Только Low-level API без декодеров и resource manager
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

### Конфигурация для embedded систем

```c
// Максимально минимальная конфигурация
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_THREADING
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_NULL  // Null backend для тестирования

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

---

## Интеграция с другими библиотеками

### SDL3

```c
#include "SDL3/SDL.h"
#include "miniaudio.h"

// miniaudio и SDL могут работать вместе
// SDL для окна и ввода, miniaudio для звука
```

### flecs (ECS)

```c
#include "flecs.h"
#include "miniaudio.h"

// См. integration-flecs.md для подробностей
```

### GLM

```c
#include "glm/glm.hpp"
#include "miniaudio.h"

// GLM для 3D математики, miniaudio для spatial audio
```

### Tracy (профилирование)

```c
#include "tracy/TracyC.h"
#include "miniaudio.h"

// Профилирование audio thread
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ZoneScoped;
    // ... обработка
    FrameMark;
}
```

---

## Примеры сборки

### Windows (Visual Studio)

```cmake
cmake_minimum_required(VERSION 3.25)
project(AudioApp)

add_subdirectory(external/miniaudio)

add_executable(AudioApp main.cpp)
target_link_libraries(AudioApp PRIVATE miniaudio)
```

### Linux (GCC/Clang)

```makefile
# Makefile пример
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
LDFLAGS = -ldl -lpthread -lm

all: audio_app

audio_app: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f audio_app *.o
```

### Кроссплатформенная сборка

```cmake
cmake_minimum_required(VERSION 3.25)
project(CrossPlatformAudioApp)

# Определение платформы
if(WIN32)
    message(STATUS "Building for Windows")
elseif(APPLE)
    message(STATUS "Building for macOS/iOS")
elseif(ANDROID)
    message(STATUS "Building for Android")
    find_library(OPENSLES_LIB OpenSLES)
elseif(EMSCRIPTEN)
    message(STATUS "Building for WebAssembly")
else()
    message(STATUS "Building for Linux/BSD")
    find_library(PTHREAD_LIB pthread)
    find_library(DL_LIB dl)
    find_library(M_LIB m)
endif()

add_subdirectory(external/miniaudio)

add_executable(AudioApp main.cpp)
target_link_libraries(AudioApp PRIVATE miniaudio)

if(ANDROID)
    target_link_libraries(AudioApp PRIVATE ${OPENSLES_LIB})
elseif(NOT WIN32 AND NOT APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(AudioApp PRIVATE ${PTHREAD_LIB} ${DL_LIB} ${M_LIB})
endif()
```

---

## См. также

- [Глоссарий](glossary.md) — терминология miniaudio
- [Основные понятия](concepts.md) — архитектура и паттерны
- [Быстрый старт](quickstart.md) — минимальные примеры кода
- [Decision Trees](decision-trees.md) — выбор конфигурации
- [Интеграция с flecs](integration-flecs.md) — ECS интеграция
- [Troubleshooting](troubleshooting.md) — решение проблем сборки
