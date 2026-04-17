## Инструменты FreeType

<!-- anchor: 05_tools -->

🟢 **Уровень 1: Начинающий**

FreeType включает утилиты для визуализации, отладки и анализа шрифтов. Находятся в директории `src/tools/` исходников.

## ftview

Визуализация глифов шрифта.

```bash
ftview [options] pt fontfile [indices]
```

### Основные параметры

| Параметр | Описание                        |
|----------|---------------------------------|
| `-e`     | Кодировка (например, `-e unic`) |
| `-f`     | Индекс face                     |
| `-r`     | DPI (по умолчанию 72)           |
| `-H`     | Режим hinting (0-2)             |
| `-l`     | Режим LCD                       |
| `-m`     | Отобразить метрики              |
| `-v`     | Версия                          |

### Примеры

```bash
# Просмотр шрифта размером 16pt
ftview 16 font.ttf

# Просмотр с LCD-рендерингом
ftview -l 1 24 font.ttf

# Конкретные глифы
ftview 48 font.ttf 65 66 67
```

## ftdiff

Сравнение рендеринга между разными версиями или режимами.

```bash
ftdiff [options] fontfile
```

### Основные параметры

| Параметр | Описание                |
|----------|-------------------------|
| `-f`     | Первый режим рендеринга |
| `-s`     | Второй режим рендеринга |
| `-H`     | Режим hinting           |

### Режимы рендеринга

```
0 - Normal
1 - Light
2 - Mono
3 - LCD
4 - LCD vertical
```

### Примеры

```bash
# Сравнение normal vs light hinting
ftdiff -f 0 -s 1 font.ttf

# Сравнение монохромного и grayscale
ftdiff -f 2 -s 0 font.ttf
```

## ftgrid

Отображение контуров глифов с сеткой.

```bash
ftgrid [options] pt fontfile [indices]
```

### Основные параметры

| Параметр | Описание         |
|----------|------------------|
| `-r`     | DPI              |
| `-f`     | Индекс face      |
| `-H`     | Режим hinting    |
| `-o`     | Показать outline |
| `-c`     | Показать контуры |

### Примеры

```bash
# Контур глифа 'A' (код 65)
ftgrid 48 font.ttf 65

# С сеткой и контурами
ftgrid -o -c 72 font.ttf
```

## ftinspect

Графический инструмент для инспекции шрифтов (Qt-based).

```bash
ftinspect [fontfile]
```

Возможности:

- Просмотр всех глифов
- Изменение размера в реальном времени
- Переключение режимов hinting
- Просмотр метрик
- Просмотр таблиц TrueType

## Официальные инструменты

### ftdump

Утилита для вывода информации о шрифте (встроена в ftview).

```bash
ftview -m font.ttf
```

### apinames

Генерация списка экспортируемых символов (для Windows DLL).

```bash
apinames freetype.dll
```

## Сторонние инструменты

### fonttools

Python-библиотека для работы со шрифтами:

```bash
pip install fonttools

# Вывод таблиц шрифта
ttx -t cmap font.ttf

# Дамп всех таблиц
ttx font.ttf
```

### HarfBuzz

Для продвинутого text shaping:

```bash
hb-view --font-file font.ttf --text "Hello"
```

### Microsoft Font Validator

Проверка соответствия спецификации OpenType:

```bash
FontValidator.exe font.ttf
```

## Анализ производительности

### Benchmarking

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 1000; i++) {
    FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

### Memory tracking

```cpp
// Кастомный аллокатор для отслеживания
static size_t total_allocated = 0;

static void* tracking_alloc(FT_Memory memory, long size) {
    total_allocated += size;
    return malloc(size);
}

static void tracking_free(FT_Memory memory, void* block) {
    // Нужно отслеживать размер для точного подсчёта
    free(block);
}
```

## Отладка

### FT_DEBUG_LEVEL_TRACE

Скомпилируйте с `-DFT_DEBUG_LEVEL_TRACE` для включения логирования:

```bash
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_LEVEL_TRACE" ..
```

### FT_DEBUG_MEMORY

Для отслеживания утечек памяти:

```bash
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_MEMORY" ..
```

### Переменные окружения

```bash
# Управление свойствами драйверов
export FREETYPE_PROPERTIES="truetype:interpreter-version=35"

# Уровень логирования
export FT2_DEBUG="any:7"
```

## Примеры из исходников

В директории `examples/` исходников FreeType:

| Файл         | Описание                      |
|--------------|-------------------------------|
| `example1.c` | Базовый рендеринг глифа       |
| `example2.c` | Рендеринг строки              |
| `example3.c` | Конвертация в OpenGL текстуру |

---

## Производительность FreeType

<!-- anchor: 06_performance -->

🔴 **Уровень 3: Продвинутый**

Оптимизация работы FreeType: кэширование, batch rendering, управление памятью и многопоточность.

## Кэширование

FreeType предоставляет встроенный механизм кэширования через `ftcache.h`.

### FTC_Manager

Центральный объект кэша:

```cpp
FTC_Manager cache_manager;

FT_Error error = FTC_Manager_New(
    library,
    2,              // max_faces
    4,              // max_sizes
    200000,         // max_bytes (~200KB)
    nullptr,        // requester (default file-based)
    nullptr,        // req_data
    &cache_manager
);
```

Параметры:

- `max_faces` — максимальное количество FT_Face в кэше
- `max_sizes` — максимальное количество FT_Size в кэше
- `max_bytes` — максимальный объём памяти для битмапов

### FTC_ImageCache

Кэш для отрендеренных глифов:

```cpp
FTC_ImageCache image_cache;
FTC_ImageCache_New(cache_manager, &image_cache);

// Получение глифа из кэша
FTC_ImageTypeRec image_type;
image_type.face_id = (FTC_FaceID)face_ptr;
image_type.width = 16;
image_type.height = 16;
image_type.flags = FT_LOAD_RENDER;

FT_Glyph glyph;
FTC_ImageCache_Lookup(image_cache, &image_type, glyph_index, &glyph, nullptr);
```

### FTC_CMapCache

Кэш для преобразования charcode → glyph_index:

```cpp
FTC_CMapCache cmap_cache;
FTC_CMapCache_New(cache_manager, &cmap_cache);

// Быстрый lookup
FT_UInt glyph_index = FTC_CMapCache_Lookup(
    cmap_cache,
    (FTC_FaceID)face_ptr,
    cmap_index,
    charcode
);
```

### FTC_SBitCache

Кэш для small bitmaps (прямая работа с пикселями):

```cpp
FTC_SBitCache sbit_cache;
FTC_SBitCache_New(cache_manager, &sbit_cache);

FTC_SBit sbit;
FTC_SBitCache_Lookup(sbit_cache, &image_type, glyph_index, &sbit, nullptr);

// Прямой доступ к пикселям
for (unsigned int y = 0; y < sbit->height; y++) {
    for (unsigned int x = 0; x < sbit->width; x++) {
        unsigned char pixel = sbit->buffer[y * sbit->pitch + x];
    }
}
```

### Очистка кэша

```cpp
// Сброс всего кэша
FTC_Manager_Reset(cache_manager);

// Освобождение
FTC_Manager_Done(cache_manager);
```

## Пользовательский Face requester

Для загрузки шрифтов по требованию:

```cpp
FT_Error my_face_requester(
    FTC_FaceID  face_id,
    FT_Library  library,
    FT_Pointer  request_data,
    FT_Face    *aface
) {
    // face_id может быть указателем на структуру с путём
    FontInfo* info = (FontInfo*)face_id;

    FT_Error error = FT_New_Face(
        library,
        info->filepath,
        info->face_index,
        aface
    );

    return error;
}

FTC_Manager_New(library, 2, 4, 200000, my_face_requester, nullptr, &manager);
```

## Batch Rendering

Рендеринг нескольких глифов за один проход:

```cpp
struct GlyphData {
    FT_UInt index;
    int x, y;
    FT_Bitmap bitmap;
};

void render_text_batch(
    FT_Face face,
    const char* text,
    std::vector<GlyphData>& glyphs
) {
    // Фаза 1: загрузка всех глифов
    FT_Pos pen_x = 0;

    for (const char* p = text; *p; p++) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, *p);

        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        if (error) continue;

        GlyphData gd;
        gd.index = glyph_index;
        gd.x = (pen_x >> 6) + face->glyph->bitmap_left;
        gd.y = -face->glyph->bitmap_top;
        gd.bitmap = face->glyph->bitmap;

        glyphs.push_back(gd);

        pen_x += face->glyph->advance.x;
    }

    // Фаза 2: отрисовка в целевой буфер
    // (может быть оптимизирована с SIMD)
}
```

## Управление памятью

### Кастомный аллокатор

Для игр с известными паттернами выделения:

```cpp
class FreeTypeAllocator {
    static constexpr size_t POOL_SIZE = 1024 * 1024;  // 1MB pool
    std::byte pool[POOL_SIZE];
    size_t offset = 0;

public:
    void* alloc(size_t size) {
        if (offset + size > POOL_SIZE) return nullptr;
        void* ptr = pool + offset;
        offset += size;
        return ptr;
    }

    void free(void*) {
        // Pool-based: frees all at once
    }
};
```

### Предвыделение

```cpp
// Предзагрузка часто используемых глифов
void preload_glyphs(FT_Face face, const char* common_chars) {
    for (const char* p = common_chars; *p; p++) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, *p);
        FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
    }
}
```

## Многопоточность

### Одна библиотека на поток

Простейший подход:

```cpp
thread_local FT_Library tl_library;

FT_Library get_library() {
    if (!tl_library) {
        FT_Init_FreeType(&tl_library);
    }
    return tl_library;
}
```

### Общая библиотека с mutex

```cpp
std::mutex ft_mutex;

FT_Face load_face_safe(const char* path) {
    std::lock_guard<std::mutex> lock(ft_mutex);
    FT_Face face;
    FT_New_Face(library, path, 0, &face);
    return face;
}

// FT_Load_Glyph безопасен без mutex для разных face
void render_glyph_safe(FT_Face face, FT_UInt glyph_index) {
    FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
}
```

### Параллельный рендеринг

```cpp
void render_text_parallel(
    const std::vector<TextRun>& runs,
    std::vector<RenderTarget>& targets
) {
    #pragma omp parallel for
    for (size_t i = 0; i < runs.size(); i++) {
        // Каждый поток использует свой FT_Library
        FT_Library lib = get_thread_local_library();

        for (char c : runs[i].text) {
            render_character(lib, c, targets[i]);
        }
    }
}
```

## Оптимизация настроек

### Минимизация overhead

```cpp
// Отключить ненужные фичи
FT_Load_Glyph(face, glyph_index,
    FT_LOAD_RENDER |
    FT_LOAD_NO_AUTOHINT |    // Если нативный hinting быстрее
    FT_LOAD_TARGET_LIGHT     // Light hinting быстрее normal
);
```

### Размер кэша

```cpp
// Экспериментально определить оптимальный размер
// Для игры с 5 шрифтами, 3 размерами:
FTC_Manager_New(library,
    5,      // max_faces = количество шрифтов
    15,     // max_sizes = шрифты * размеры
    500000, // max_bytes зависит от памяти
    nullptr, nullptr, &manager
);
```

### Ленивая загрузка

```cpp
class LazyFont {
    FT_Face face = nullptr;
    std::string path;

public:
    FT_Face get() {
        if (!face) {
            FT_New_Face(library, path.c_str(), 0, &face);
        }
        return face;
    }

    ~LazyFont() {
        if (face) FT_Done_Face(face);
    }
};
```

## Профилирование

### Измерение времени

```cpp
struct Profiler {
    const char* name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    Profiler(const char* n) : name(n) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Profiler() {
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        printf("%s: %lld us\n", name, us.count());
    }
};

void benchmark_glyph(FT_Face face, FT_UInt glyph_index) {
    {
        Profiler p("FT_Load_Glyph");
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    }
    {
        Profiler p("FT_Render_Glyph");
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    }
}
```

### Tracy integration

```cpp
#include <tracy/Tracy.hpp>

void render_glyph_profiled(FT_Face face, FT_UInt glyph_index) {
    ZoneScoped;

    {
        ZoneScopedN("FT_Load_Glyph");
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    }

    {
        ZoneScopedN("FT_Render_Glyph");
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    }
}
```

## Рекомендации

1. **Используйте кэширование** — FTC_Manager для всех повторяющихся операций
2. **Пакетная обработка** — группируйте загрузку глифов
3. **Правильные флаги** — FT_LOAD_TARGET_LIGHT для UI текста
4. **Пул памяти** — для предсказуемого аллокации в играх
5. **Параллелизм** — отдельная FT_Library на поток
6. **Измеряйте** — профилируйте реальные сценарии использования

---

## Устранение неполадок FreeType

<!-- anchor: 07_troubleshooting -->

🟡 **Уровень 2: Средний**

Типичные проблемы при работе с FreeType и их решения.

## Ошибки загрузки шрифтов

### FT_Err_Unknown_File_Format

**Проблема**: Формат файла не поддерживается.

**Причины**:

- Файл повреждён
- Формат не поддерживается (например, EOT)
- Файл пустой

**Решение**:

```cpp
FT_Error error = FT_New_Face(library, "font.ttf", 0, &face);
if (error == FT_Err_Unknown_File_Format) {
    // Проверить файл
    FILE* f = fopen("font.ttf", "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        if (size == 0) {
            printf("File is empty\n");
        }
    }
}
```

### FT_Err_Cannot_Open_Resource

**Проблема**: Файл не найден или нет доступа.

**Решение**:

```cpp
// Проверка существования
#include <sys/stat.h>

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// На Windows с Unicode путями
#ifdef _WIN32
#include <windows.h>
std::wstring utf8_to_wide(const std::string& utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    return wide;
}

// Чтение файла и загрузка из памяти
HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
// ... read and use FT_New_Memory_Face
#endif
```

### FT_Err_Invalid_Argument

**Проблема**: Неверный индекс face или аргумент.

**Причины**:

- `face_index` больше `num_faces`
- Неверный `FT_Open_Args`

**Решение**:

```cpp
// Проверка количества faces
FT_Face temp_face;
FT_Error error = FT_New_Face(library, "font.ttc", -1, &temp_face);
if (!error) {
    FT_Long num_faces = temp_face->num_faces;
    FT_Done_Face(temp_face);

    for (FT_Long i = 0; i < num_faces; i++) {
        FT_New_Face(library, "font.ttc", i, &face);
        // ...
    }
}
```

## Проблемы с кодировками

### Символ не найден (glyph_index = 0)

**Проблема**: `FT_Get_Char_Index` возвращает 0.

**Причины**:

- Нет Unicode charmap
- Символ отсутствует в шрифте
- Неверная кодировка

**Решение**:

```cpp
// Проверка доступных charmaps
for (FT_Int i = 0; i < face->num_charmaps; i++) {
    FT_CharMap cmap = face->charmaps[i];
    printf("Charmap %d: platform=%d, encoding=%d\n",
           i, cmap->platform_id, cmap->encoding_id);
}

// Выбор Unicode charmap
FT_Error error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
if (error) {
    // Попробовать другие
    for (FT_Int i = 0; i < face->num_charmaps; i++) {
        if (face->charmaps[i]->encoding == FT_ENCODING_APPLE_ROMAN) {
            FT_Set_Charmap(face, face->charmaps[i]);
            break;
        }
    }
}
```

### Неправильные символы

**Проблема**: Отображаются не те символы.

**Причины**:

- Неверная кодировка входного текста
- Неправильный charmap

**Решение**:

```cpp
// Убедиться в Unicode
const char* text = u8"Привет";  // UTF-8

// Конвертация UTF-8 в UTF-32
#include <codecvt>
std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
std::u32string utf32 = converter.from_bytes(text);

for (char32_t c : utf32) {
    FT_UInt glyph_index = FT_Get_Char_Index(face, c);
    // ...
}
```

## Проблемы рендеринга

### Пустой битмап

**Проблема**: `bitmap.width` или `bitmap.rows` равны 0.

**Причины**:

- Пробельный символ (space)
- Glyph не рендерился

**Решение**:

```cpp
FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

// Для пробельных символов advance работает, но битмап пустой
if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
}

FT_Bitmap& bitmap = face->glyph->bitmap;
if (bitmap.width == 0 || bitmap.rows == 0) {
    // Использовать advance для позиционирования
    pen_x += face->glyph->advance.x;
    continue;
}
```

### Нечитаемый текст

**Проблема**: Текст размытый или слишком мелкий.

**Причины**:

- Неправильный размер
- Неподходящий режим hinting

**Решение**:

```cpp
// Увеличить DPI для мобильных устройств
FT_Set_Char_Size(face, 0, 16 * 64, 144, 144);  // 144 DPI

// Попробовать разные режимы
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LIGHT);  // Для UI
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_NORMAL); // Для текста
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO);   // Для печати
```

### Обрезанные глифы

**Проблема**: Части глифов обрезаны.

**Причины**:

- Неправильный расчёт bounding box
- Игнорирование negative bearing

**Решение**:

```cpp
// Правильный расчёт размеров
int min_x = INT_MAX, min_y = INT_MAX;
int max_x = INT_MIN, max_y = INT_MIN;

for (each glyph) {
    int x = pen_x + slot->bitmap_left;
    int y = pen_y - slot->bitmap_top;

    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
    max_x = std::max(max_x, x + (int)slot->bitmap.width);
    max_y = std::max(max_y, y + (int)slot->bitmap.rows);

    pen_x += slot->advance.x >> 6;
}

int width = max_x - min_x;
int height = max_y - min_y;
```

## Проблемы с памятью

### Утечка памяти

**Проблема**: Память растёт при загрузке шрифтов.

**Причины**:

- Забыт `FT_Done_Face`
- Забыт `FT_Done_Glyph`
- Незакрытый `FT_New_Memory_Face`

**Решение**:

```cpp
// RAII wrapper
class Face {
    FT_Face face_ = nullptr;
public:
    Face(FT_Library lib, const char* path) {
        FT_New_Face(lib, path, 0, &face_);
    }
    ~Face() {
        if (face_) FT_Done_Face(face_);
    }
    Face(const Face&) = delete;
    Face& operator=(const Face&) = delete;
    FT_Face get() const { return face_; }
};
```

### FT_Err_Out_Of_Memory

**Проблема**: FreeType не может выделить память.

**Причины**:

- Слишком большой шрифт
- Повреждённый шрифт с огромными значениями

**Решение**:

```cpp
// Ограничить максимальный размер
const size_t MAX_FONT_SIZE = 50 * 1024 * 1024;  // 50MB

FILE* f = fopen(path, "rb");
fseek(f, 0, SEEK_END);
long size = ftell(f);
fclose(f);

if (size > MAX_FONT_SIZE) {
    printf("Font too large: %ld bytes\n", size);
    return false;
}
```

## Проблемы многопоточности

### Краш при загрузке

**Проблема**: Segfault при одновременной загрузке шрифтов.

**Причины**:

- Одновременный доступ к одному `FT_Library`

**Решение**:

```cpp
// Вариант 1: один library на поток
thread_local FT_Library tl_library;

// Вариант 2: mutex для создания/удаления face
std::mutex g_ft_mutex;

FT_Face load_face(FT_Library lib, const char* path) {
    std::lock_guard<std::mutex> lock(g_ft_mutex);
    FT_Face face;
    FT_New_Face(lib, path, 0, &face);
    return face;
}
```

## Tricky fonts

### Игнорируются флаги hinting

**Проблема**: `FT_LOAD_NO_HINTING` не работает.

**Причины**:

- Шрифт в списке "tricky fonts"

**Решение**:

```cpp
if (FT_IS_TRICKY(face)) {
    // Эти шрифты требуют нативного hinting
    // FT_LOAD_NO_HINTING будет проигнорирован

    // Использовать нативный hinter
    FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
}
```

### Список tricky fonts

Некоторые старые китайские шрифты:

- `mingli.ttf` (но не `mingliu.ttc`)
- И другие (~12 шрифтов)

Определены в `ttobjs.c` исходников FreeType.

## Отладка

### Включение логирования

```cpp
// При компиляции
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_LEVEL_TRACE" ..

// Или установка переменной окружения
set FT2_DEBUG=any:7
```

### Вывод информации о шрифте

```cpp
void print_face_info(FT_Face face) {
    printf("Family: %s\n", face->family_name);
    printf("Style: %s\n", face->style_name);
    printf("Glyphs: %ld\n", face->num_glyphs);
    printf("Units per EM: %d\n", face->units_per_EM);
    printf("Ascender: %d\n", face->ascender);
    printf("Descender: %d\n", face->descender);
    printf("Height: %d\n", face->height);

    printf("Flags:\n");
    if (FT_IS_SCALABLE(face)) printf("  Scalable\n");
    if (FT_IS_FIXED_WIDTH(face)) printf("  Fixed width\n");
    if (FT_HAS_KERNING(face)) printf("  Kerning\n");
    if (FT_HAS_GLYPH_NAMES(face)) printf("  Glyph names\n");
    if (FT_IS_TRICKY(face)) printf("  Tricky\n");
}
```

### Проверка ошибки

```cpp
const char* error_string(FT_Error error) {
    #undef __FTERRORS_H__
    #define FT_ERRORDEF(e, v, s) case e: return s;
    #define FT_ERROR_START_LIST switch (error) {
    #define FT_ERROR_END_LIST }
    #include <freetype/fterrors.h>
    return "unknown error";
}
```

## Коды ошибок

| Код  | Имя                           | Описание                |
|------|-------------------------------|-------------------------|
| 0x01 | `FT_Err_Cannot_Open_Resource` | Не удалось открыть файл |
| 0x02 | `FT_Err_Unknown_File_Format`  | Неизвестный формат      |
| 0x03 | `FT_Err_Invalid_File_Format`  | Повреждённый файл       |
| 0x10 | `FT_Err_Invalid_Argument`     | Неверный аргумент       |
| 0x17 | `FT_Err_Invalid_Glyph_Index`  | Неверный индекс глифа   |
| 0x22 | `FT_Err_Invalid_Pixel_Size`   | Неверный размер пикселя |
| 0x40 | `FT_Err_Invalid_Handle`       | Неверный handle         |
| 0x41 | `FT_Err_Out_Of_Memory`        | Нехватка памяти         |
