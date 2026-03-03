# Производительность FreeType

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
