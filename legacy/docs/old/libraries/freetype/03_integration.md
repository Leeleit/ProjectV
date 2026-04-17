# Интеграция FreeType

🟢 **Уровень 1: Начинающий**

Сборка и подключение FreeType к проекту: CMake, Meson, системные пакеты и платформенные особенности.

## CMake

### Подключение как подмодуль

```cmake
# CMakeLists.txt
add_subdirectory(external/freetype)

target_link_libraries(your_target PRIVATE freetype)
```

### Поиск установленной библиотеки

```cmake
find_package(Freetype REQUIRED)

target_link_libraries(your_target PRIVATE Freetype::Freetype)
```

### Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(freetype_example CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Freetype REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE Freetype::Freetype)
```

### Опции конфигурации

При сборке из исходников:

```cmake
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)

add_subdirectory(freetype)
```

| Опция                     | По умолчанию | Описание                   |
|---------------------------|--------------|----------------------------|
| `FT_DISABLE_BROTLI`       | OFF          | Отключить Brotli для WOFF2 |
| `FT_DISABLE_BZIP2`        | OFF          | Отключить Bzip2            |
| `FT_DISABLE_PNG`          | OFF          | Отключить PNG              |
| `FT_DISABLE_HARFBUZZ`     | OFF          | Отключить HarfBuzz         |
| `FT_DISABLE_ZLIB`         | OFF          | Отключить Zlib             |
| `FT_ENABLE_ERROR_STRINGS` | OFF          | Включить строки ошибок     |

## Meson

```meson
# meson.build
project('example', 'cpp')

freetype_dep = dependency('freetype2')

executable('example', 'main.cpp',
    dependencies: freetype_dep
)
```

## Системные пакеты

### Windows (vcpkg)

```bash
vcpkg install freetype:x64-windows
```

CMake с vcpkg:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake ..
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install libfreetype6-dev
```

### Linux (Fedora)

```bash
sudo dnf install freetype-devel
```

### macOS (Homebrew)

```bash
brew install freetype
```

## Заголовки

### Стандартное включение

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
```

### Дополнительные модули

```cpp
#include <freetype/ftglyph.h>      // Работа с глифами
#include <freetype/ftoutln.h>      // Контуры
#include <freetype/ftbitmap.h>     // Битмапы
#include <freetype/ftcache.h>      // Кэширование
#include <freetype/ftstroke.h>     // Обводка
#include <freetype/ftsynth.h>      // Синтез жирного/курсива
#include <freetype/ftmodapi.h>     // Управление модулями
#include <freetype/ftsystem.h>     // Системный интерфейс
#include <freetype/fterrors.h>     // Коды ошибок
#include <freetype/fterrdef.h>     // Определения ошибок
```

### Формат-специфичные заголовки

```cpp
#include <freetype/tttables.h>     // TrueType таблицы
#include <freetype/tttags.h>       // TrueType теги
#include <freetype/t1tables.h>     // Type 1 таблицы
#include <freetype/ftcid.h>        // CID fonts
#include <freetype/ftmm.h>         // Multiple Masters
```

## Компиляция

### GCC/Clang

```bash
g++ -o example main.cpp -lfreetype
```

### MSVC

```bash
cl main.cpp /link freetype.lib
```

### Статическая линковка

При статической линковке определите макрос перед включением заголовков:

```cpp
#define FT_STATIC 1
#include <ft2build.h>
#include <freetype/freetype.h>
```

## Платформенные особенности

### Windows

- DLL: `freetype.dll`
- Импортная библиотека: `freetype.lib`
- Статическая библиотека: `freetype.lib`

Для Unicode путей используйте `FT_New_Memory_Face` с данными из `CreateFileW` + `ReadFile`.

### Linux

- Shared library: `libfreetype.so`
- Статическая: `libfreetype.a`

Fontconfig часто используется вместе с FreeType для поиска шрифтов.

### macOS

- Framework: `FreeType.framework`
- Shared library: `libfreetype.dylib`

## Кастомный аллокатор

```cpp
static void* my_alloc(FT_Memory memory, long size) {
    return malloc(size);
}

static void my_free(FT_Memory memory, void* block) {
    free(block);
}

static void* my_realloc(FT_Memory memory, long cur_size,
                        long new_size, void* block) {
    return realloc(block, new_size);
}

FT_MemoryRec_ memory_rec = {
    nullptr,
    my_alloc,
    my_free,
    my_realloc
};

FT_Library library;
FT_Error error = FT_New_Library(&memory_rec, &library);
if (!error) {
    FT_Add_Default_Modules(library);
}
```

## Кастомный поток ввода

```cpp
static unsigned long my_io(FT_Stream stream, unsigned long offset,
                           unsigned char* buffer, unsigned long count) {
    // Чтение count байт по смещению offset
    // Вернуть количество прочитанных байт
    return count;
}

static void my_close(FT_Stream stream) {
    // Закрытие потока
}

FT_StreamRec_ stream_rec = {};
stream_rec.base = nullptr;
stream_rec.size = file_size;
stream_rec.pos = 0;
stream_rec.descriptor.pointer = my_file_handle;
stream_rec.read = my_io;
stream_rec.close = my_close;

FT_Open_Args args = {};
args.flags = FT_OPEN_STREAM;
args.stream = &stream_rec;

FT_Face face;
FT_Open_Face(library, &args, 0, &face);
```

## Зависимости

FreeType может использовать опциональные зависимости:

| Библиотека   | Назначение         | Опция CMake           |
|--------------|--------------------|-----------------------|
| **Zlib**     | Сжатие в WOFF      | `FT_DISABLE_ZLIB`     |
| **Bzip2**    | Сжатие в BDF/PCF   | `FT_DISABLE_BZIP2`    |
| **libpng**   | PNG в sbix fonts   | `FT_DISABLE_PNG`      |
| **HarfBuzz** | Продвинутый shaper | `FT_DISABLE_HARFBUZZ` |
| **Brotli**   | WOFF2 сжатие       | `FT_DISABLE_BROTLI`   |

## Версионирование

```cpp
FT_Int major, minor, patch;
FT_Library_Version(library, &major, &minor, &patch);

printf("FreeType %d.%d.%d\n", major, minor, patch);

// Макросы времени компиляции
#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 10
    // FT_Error_String доступен
#endif
```

## Конфигурация сборки

### Минимальная конфигурация

Для встраиваемых систем или минимального footprint:

```cmake
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_ENABLE_ERROR_STRINGS OFF CACHE BOOL "" FORCE)
```

### Полная конфигурация

Для максимальной совместимости:

```cmake
set(FT_DISABLE_BROTLI OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB OFF CACHE BOOL "" FORCE)
```

## Сборка из исходников

### CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .
```

### Meson

```bash
meson setup build --prefix=/usr/local
meson compile -C build
meson install -C build
```

### Autotools (устаревший)

```bash
./configure --prefix=/usr/local
make
make install
