## Интеграция FreeType

<!-- anchor: 03_integration -->


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

---

## Интеграция FreeType с ProjectV

<!-- anchor: 09_projectv-integration -->


Связка FreeType с Vulkan рендерером, SDL3, VMA и RmlUi в ProjectV.

## Обзор интеграции

FreeType в ProjectV используется для:
- Рендеринга UI текста через RmlUi (FontEngineInterface)
- Прямого рендеринга текста в Vulkan текстуры
- Генерации glyph atlas для GPU

## Архитектура

```

┌─────────────┐ ┌──────────────┐ ┌─────────────┐
│ RmlUi │────>│ FontEngine │────>│ FreeType │
│  (UI Layer) │ │ Interface │ │  (Library)  │
└─────────────┘ └──────────────┘ └─────────────┘
│
v
┌──────────────┐
│ GlyphAtlas │
│   (VMA)      │
└──────────────┘
│
v
┌──────────────┐
│ Vulkan │
│ Rendering │
└──────────────┘

```

## CMake конфигурация

```cmake
# CMakeLists.txt
add_subdirectory(external/freetype)

target_link_libraries(projectv_core PRIVATE
    freetype
    SDL3::SDL3
    Vulkan::Vulkan
)
```

## FontEngineInterface для RmlUi

RmlUi требует реализацию FontEngineInterface для работы со шрифтами:

```cpp
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/FontGlyph.h>
#include <ft2build.h>
#include <freetype/freetype.h>

class FreeTypeFontEngine : public Rml::FontEngineInterface {
public:
    FreeTypeFontEngine() {
        FT_Init_FreeType(&library);
    }

    ~FreeTypeFontEngine() override {
        FT_Done_FreeType(library);
    }

    Rml::FontFaceHandle LoadFontFace(
        const Rml::String& file_name,
        bool fallback_face,
        Rml::Style::FontWeight weight
    ) override {
        FT_Face face;
        FT_Error error = FT_New_Face(library, file_name.c_str(), 0, &face);
        if (error) return 0;

        return reinterpret_cast<Rml::FontFaceHandle>(face);
    }

    Rml::FontFaceHandle LoadFontFace(
        Rml::Span<const Rml::byte> data,
        bool fallback_face,
        Rml::Style::FontWeight weight
    ) override {
        FT_Face face;
        FT_Error error = FT_New_Memory_Face(
            library,
            reinterpret_cast<const FT_Byte*>(data.data()),
            static_cast<FT_Long>(data.size()),
            0,
            &face
        );
        if (error) return 0;

        return reinterpret_cast<Rml::FontFaceHandle>(face);
    }

    Rml::FontEffectsHandle PrepareFontEffects(
        Rml::FontFaceHandle face,
        const Rml::FontEffects& effects
    ) override {
        return 0;  // Not implemented
    }

    Rml::FontHandle PrepareFont(
        Rml::FontFaceHandle face,
        const Rml::String& charset,
        Rml::Style::FontSize size,
        Rml::Style::FontWeight weight,
        Rml::Style::FontStyle style,
        int effect_layer
    ) override {
        FT_Face ft_face = reinterpret_cast<FT_Face>(face);

        FT_Set_Pixel_Sizes(ft_face, 0, static_cast<FT_UInt>(size));

        return reinterpret_cast<Rml::FontHandle>(ft_face);
    }

    const Rml::FontMetrics GetFontMetrics(
        Rml::FontHandle font
    ) override {
        FT_Face face = reinterpret_cast<FT_Face>(font);

        Rml::FontMetrics metrics;
        metrics.size = static_cast<int>(face->size->metrics.y_ppem);
        metrics.ascent = static_cast<int>(face->size->metrics.ascender >> 6);
        metrics.descent = -static_cast<int>(face->size->metrics.descender >> 6);
        metrics.line_height = static_cast<int>(face->size->metrics.height >> 6);

        return metrics;
    }

    int GetStringWidth(
        Rml::FontHandle font,
        Rml::StringView string,
        Rml::TextShapingContext text_shaping
    ) override {
        FT_Face face = reinterpret_cast<FT_Face>(font);

        int width = 0;
        for (char c : string) {
            FT_UInt glyph_index = FT_Get_Char_Index(face, c);
            if (glyph_index == 0) continue;

            FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            width += static_cast<int>(face->glyph->advance.x >> 6);
        }

        return width;
    }

    int GenerateLayerString(
        Rml::FontHandle font,
        Rml::StringView string,
        const Rml::TextShapingContext& text_shaping,
        const Rml::Colourb colour,
        Rml::LayerList layers
    ) override {
        // Реализация рендеринга текста
        return GetStringWidth(font, string, text_shaping);
    }

    void GenerateLayerConfiguration(
        Rml::FontHandle font,
        Rml::StringView string,
        const Rml::TextShapingContext& text_shaping
    ) override {
        // No-op
    }

    void GenerateLayerData(
        Rml::FontHandle font,
        Rml::StringView string,
        const Rml::TextShapingContext& text_shaping,
        Rml::LayerList layers,
        Rml::TexturedMeshList& mesh
    ) override {
        // Генерация mesh для текста
    }

    void ReleaseFontResources(Rml::FontHandle font) override {
        // Ресурсы освобождаются в ReleaseFontFace
    }

    void ReleaseFontFaceResources(Rml::FontFaceHandle face) override {
        FT_Done_Face(reinterpret_cast<FT_Face>(face));
    }

private:
    FT_Library library;
};
```

## Glyph Atlas для Vulkan

Создание атласа глифов в GPU памяти:

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <vk_mem_alloc.h>

struct GlyphInfo {
    FT_UInt glyph_index;
    int x, y;           // Позиция в атласе
    int width, height;  // Размеры
    int bearing_x, bearing_y;
    int advance;
};

class GlyphAtlas {
public:
    GlyphAtlas(VmaAllocator allocator, VkDevice device, uint32_t size = 1024)
        : allocator_(allocator), device_(device), size_(size) {
        create_texture();
    }

    ~GlyphAtlas() {
        vmaDestroyImage(allocator_, image_, allocation_);
        vkDestroyImageView(device_, image_view_, nullptr);
    }

    std::optional<GlyphInfo> add_glyph(FT_Face face, FT_UInt glyph_index) {
        // Проверка кэша
        auto it = glyphs_.find(glyph_index);
        if (it != glyphs_.end()) {
            return it->second;
        }

        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        if (error) return std::nullopt;

        FT_Bitmap& bitmap = face->glyph->bitmap;

        // Проверка места в атласе
        if (cursor_x_ + bitmap.width > size_) {
            cursor_x_ = 0;
            cursor_y_ += row_height_;
            row_height_ = 0;
        }

        if (cursor_y_ + bitmap.rows > size_) {
            return std::nullopt;  // Атлас переполнен
        }

        // Копирование пикселей в staging buffer
        copy_to_atlas(bitmap, cursor_x_, cursor_y_);

        GlyphInfo info;
        info.glyph_index = glyph_index;
        info.x = cursor_x_;
        info.y = cursor_y_;
        info.width = bitmap.width;
        info.height = bitmap.rows;
        info.bearing_x = face->glyph->bitmap_left;
        info.bearing_y = face->glyph->bitmap_top;
        info.advance = static_cast<int>(face->glyph->advance.x >> 6);

        glyphs_[glyph_index] = info;

        cursor_x_ += bitmap.width + 1;  // 1px padding
        row_height_ = std::max(row_height_, bitmap.rows);

        return info;
    }

    VkImageView get_image_view() const { return image_view_; }

private:
    void create_texture() {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = size_;
        image_info.extent.height = size_;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_R8_UNORM;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(allocator_, &image_info, &alloc_info, &image_, &allocation_, nullptr);

        // Create image view
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image_;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8_UNORM;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;

        vkCreateImageView(device_, &view_info, nullptr, &image_view_);
    }

    void copy_to_atlas(const FT_Bitmap& bitmap, int x, int y) {
        // Staging buffer + command buffer для копирования
        // Реализация зависит от системы рендеринга
    }

    VmaAllocator allocator_;
    VkDevice device_;
    VkImage image_;
    VmaAllocation allocation_;
    VkImageView image_view_;
    uint32_t size_;

    int cursor_x_ = 0;
    int cursor_y_ = 0;
    int row_height_ = 0;

    std::unordered_map<FT_UInt, GlyphInfo> glyphs_;
};
```

## Интеграция с SDL3

Загрузка шрифтов через SDL файловую систему:

```cpp
#include <SDL3/SDL.h>
#include <ft2build.h>
#include <freetype/freetype.h>

class FontLoader {
public:
    FontLoader(FT_Library library) : library_(library) {}

    FT_Face load_from_file(const char* path) {
        FT_Face face;
        FT_Error error = FT_New_Face(library_, path, 0, &face);
        return error ? nullptr : face;
    }

    FT_Face load_from_sdl_storage(SDL_Storage* storage, const char* path) {
        SDL_StorageEnumerator* enumerator = SDL_OpenStorageStorageEnumerator(storage, path);
        if (!enumerator) return nullptr;

        // Чтение файла через SDL
        void* data = nullptr;
        size_t size = 0;

        // ... SDL_ReadStorageFile ...

        FT_Face face;
        FT_Error error = FT_New_Memory_Face(
            library_,
            static_cast<const FT_Byte*>(data),
            static_cast<FT_Long>(size),
            0,
            &face
        );

        SDL_free(data);
        return error ? nullptr : face;
    }

    FT_Face load_from_memory(const std::vector<char>& data) {
        FT_Face face;
        FT_Error error = FT_New_Memory_Face(
            library_,
            reinterpret_cast<const FT_Byte*>(data.data()),
            static_cast<FT_Long>(data.size()),
            0,
            &face
        );
        return error ? nullptr : face;
    }

private:
    FT_Library library_;
};
```

## Потокобезопасность

Для многопоточной загрузки шрифтов:

```cpp
class ThreadSafeFontSystem {
public:
    ThreadSafeFontSystem() {
        FT_Init_FreeType(&library_);
    }

    ~ThreadSafeFontSystem() {
        FT_Done_FreeType(library_);
    }

    FT_Face load_font(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        FT_Face face;
        FT_New_Face(library_, path.c_str(), 0, &face);
        return face;
    }

    // Загрузка глифов безопасна без mutex (для разных face)
    void render_glyph(FT_Face face, FT_UInt glyph_index) {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
    }

private:
    FT_Library library_;
    std::mutex mutex_;
};
```

## Tracy профилирование

```cpp
#include <tracy/Tracy.hpp>

void render_text_profiled(FT_Face face, const char* text) {
    ZoneScoped;

    FT_Pos pen_x = 0;

    for (const char* p = text; *p; p++) {
        FT_UInt glyph_index;
        {
            ZoneScopedN("FT_Get_Char_Index");
            glyph_index = FT_Get_Char_Index(face, *p);
        }

        {
            ZoneScopedN("FT_Load_Render");
            FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        }

        pen_x += face->glyph->advance.x;
    }
}

---

## Паттерны FreeType в ProjectV

<!-- anchor: 10_projectv-patterns -->


Архитектурные паттерны для работы со шрифтами в воксельном движке: FontCache, GlyphAtlas, TextRenderer.

## FontCache

Кэширование загруженных шрифтов с reference counting.

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <unordered_map>
#include <memory>
#include <string>

class FontCache {
public:
    FontCache() {
        FT_Init_FreeType(&library_);
    }

    ~FontCache() {
        faces_.clear();
        FT_Done_FreeType(library_);
    }

    class FontHandle {
    public:
        FontHandle() = default;
        FontHandle(FT_Face face, FontCache* cache)
            : face_(face), cache_(cache) {
            if (face_) cache_->add_ref(face_);
        }

        FontHandle(const FontHandle& other)
            : face_(other.face_), cache_(other.cache_) {
            if (face_) cache_->add_ref(face_);
        }

        FontHandle(FontHandle&& other) noexcept
            : face_(other.face_), cache_(other.cache_) {
            other.face_ = nullptr;
        }

        FontHandle& operator=(const FontHandle& other) {
            if (this != &other) {
                release();
                face_ = other.face_;
                cache_ = other.cache_;
                if (face_) cache_->add_ref(face_);
            }
            return *this;
        }

        FontHandle& operator=(FontHandle&& other) noexcept {
            if (this != &other) {
                release();
                face_ = other.face_;
                cache_ = other.cache_;
                other.face_ = nullptr;
            }
            return *this;
        }

        ~FontHandle() {
            release();
        }

        FT_Face get() const { return face_; }
        explicit operator bool() const { return face_ != nullptr; }

    private:
        void release() {
            if (face_ && cache_) {
                cache_->release_ref(face_);
                face_ = nullptr;
            }
        }

        FT_Face face_ = nullptr;
        FontCache* cache_ = nullptr;
    };

    FontHandle load(const std::string& path) {
        auto it = faces_.find(path);
        if (it != faces_.end()) {
            return FontHandle(it->second.face, this);
        }

        FT_Face face;
        FT_Error error = FT_New_Face(library_, path.c_str(), 0, &face);
        if (error) return FontHandle();

        faces_[path] = {face, 0};
        return FontHandle(face, this);
    }

    FontHandle load_from_memory(const std::vector<uint8_t>& data,
                                 const std::string& key) {
        auto it = faces_.find(key);
        if (it != faces_.end()) {
            return FontHandle(it->second.face, this);
        }

        FT_Face face;
        FT_Error error = FT_New_Memory_Face(
            library_,
            data.data(),
            static_cast<FT_Long>(data.size()),
            0,
            &face
        );
        if (error) return FontHandle();

        faces_[key] = {face, 0};
        return FontHandle(face, this);
    }

private:
    struct Entry {
        FT_Face face;
        int ref_count;
    };

    void add_ref(FT_Face face) {
        for (auto& [key, entry] : faces_) {
            if (entry.face == face) {
                entry.ref_count++;
                break;
            }
        }
    }

    void release_ref(FT_Face face) {
        for (auto it = faces_.begin(); it != faces_.end(); ++it) {
            if (it->second.face == face) {
                it->second.ref_count--;
                if (it->second.ref_count == 0) {
                    FT_Done_Face(face);
                    faces_.erase(it);
                }
                break;
            }
        }
    }

    FT_Library library_;
    std::unordered_map<std::string, Entry> faces_;
};
```

## GlyphAtlas

Текстурный атлас для эффективного рендеринга глифов.

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <unordered_map>
#include <vector>

class GlyphAtlas {
public:
    struct Glyph {
        float u0, v0, u1, v1;  // UV координаты
        int16_t width, height;
        int16_t bearing_x, bearing_y;
        int16_t advance;
    };

    GlyphAtlas(uint32_t width, uint32_t height)
        : width_(width), height_(height),
          pixels_(width * height, 0) {
    }

    std::optional<Glyph> get_or_add(FT_Face face, FT_UInt glyph_index, uint32_t size) {
        CacheKey key{face, glyph_index, size};

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        FT_Set_Pixel_Sizes(face, 0, size);

        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        if (error) return std::nullopt;

        FT_Bitmap& bitmap = face->glyph->bitmap;
        if (bitmap.width == 0 || bitmap.rows == 0) {
            // Пробельный символ
            Glyph glyph{};
            glyph.advance = static_cast<int16_t>(face->glyph->advance.x >> 6);
            cache_[key] = glyph;
            return glyph;
        }

        // Проверка места
        if (cursor_x_ + bitmap.width > width_) {
            cursor_x_ = 0;
            cursor_y_ += row_height_ + 1;
            row_height_ = 0;
        }

        if (cursor_y_ + bitmap.rows > height_) {
            return std::nullopt;  // Атлас переполнен
        }

        // Копирование пикселей
        for (unsigned int y = 0; y < bitmap.rows; y++) {
            for (unsigned int x = 0; x < bitmap.width; x++) {
                uint8_t pixel = bitmap.buffer[y * bitmap.pitch + x];
                uint32_t dst_x = cursor_x_ + x;
                uint32_t dst_y = cursor_y_ + y;
                pixels_[dst_y * width_ + dst_x] = pixel;
            }
        }

        dirty_ = true;

        Glyph glyph;
        glyph.u0 = static_cast<float>(cursor_x_) / width_;
        glyph.v0 = static_cast<float>(cursor_y_) / height_;
        glyph.u1 = static_cast<float>(cursor_x_ + bitmap.width) / width_;
        glyph.v1 = static_cast<float>(cursor_y_ + bitmap.rows) / height_;
        glyph.width = static_cast<int16_t>(bitmap.width);
        glyph.height = static_cast<int16_t>(bitmap.rows);
        glyph.bearing_x = static_cast<int16_t>(face->glyph->bitmap_left);
        glyph.bearing_y = static_cast<int16_t>(face->glyph->bitmap_top);
        glyph.advance = static_cast<int16_t>(face->glyph->advance.x >> 6);

        cache_[key] = glyph;

        cursor_x_ += bitmap.width + 1;
        row_height_ = std::max(row_height_, bitmap.rows);

        return glyph;
    }

    const std::vector<uint8_t>& pixels() const { return pixels_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    bool dirty() const { return dirty_; }
    void clear_dirty() { dirty_ = false; }

private:
    struct CacheKey {
        FT_Face face;
        FT_UInt glyph_index;
        uint32_t size;

        bool operator==(const CacheKey& other) const {
            return face == other.face &&
                   glyph_index == other.glyph_index &&
                   size == other.size;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const {
            return std::hash<void*>{}(k.face) ^
                   (std::hash<FT_UInt>{}(k.glyph_index) << 1) ^
                   (std::hash<uint32_t>{}(k.size) << 2);
        }
    };

    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> pixels_;
    std::unordered_map<CacheKey, Glyph, CacheKeyHash> cache_;

    uint32_t cursor_x_ = 0;
    uint32_t cursor_y_ = 0;
    uint32_t row_height_ = 0;
    bool dirty_ = false;
};
```

## TextRenderer

Система рендеринга текста с использованием glyph atlas.

```cpp
#include <glm/glm.hpp>
#include <vector>

class TextRenderer {
public:
    struct Vertex {
        glm::vec2 position;
        glm::vec2 uv;
        uint8_t color_r, color_g, color_b, color_a;
    };

    struct TextMesh {
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
    };

    TextRenderer(GlyphAtlas& atlas) : atlas_(atlas) {}

    TextMesh build_text_mesh(
        FT_Face face,
        const std::string& text,
        uint32_t font_size,
        glm::vec2 position,
        glm::vec4 color = {1, 1, 1, 1}
    ) {
        TextMesh mesh;
        float x = position.x;
        float y = position.y;

        // Получаем baseline
        FT_Set_Pixel_Sizes(face, 0, font_size);
        float baseline = y + face->size->metrics.ascender / 64.0f;

        uint16_t vertex_offset = 0;

        for (char c : text) {
            if (c == '\n') {
                x = position.x;
                baseline -= face->size->metrics.height / 64.0f;
                continue;
            }

            FT_UInt glyph_index = FT_Get_Char_Index(face, c);
            auto glyph = atlas_.get_or_add(face, glyph_index, font_size);

            if (!glyph) continue;

            if (glyph->width > 0 && glyph->height > 0) {
                float gx = x + glyph->bearing_x;
                float gy = baseline - glyph->bearing_y;

                // 4 vertices for quad
                mesh.vertices.push_back({{gx, gy},
                    {glyph->u0, glyph->v0},
                    static_cast<uint8_t>(color.r * 255),
                    static_cast<uint8_t>(color.g * 255),
                    static_cast<uint8_t>(color.b * 255),
                    static_cast<uint8_t>(color.a * 255)});

                mesh.vertices.push_back({{gx + glyph->width, gy},
                    {glyph->u1, glyph->v0},
                    static_cast<uint8_t>(color.r * 255),
                    static_cast<uint8_t>(color.g * 255),
                    static_cast<uint8_t>(color.b * 255),
                    static_cast<uint8_t>(color.a * 255)});

                mesh.vertices.push_back({{gx + glyph->width, gy + glyph->height},
                    {glyph->u1, glyph->v1},
                    static_cast<uint8_t>(color.r * 255),
                    static_cast<uint8_t>(color.g * 255),
                    static_cast<uint8_t>(color.b * 255),
                    static_cast<uint8_t>(color.a * 255)});

                mesh.vertices.push_back({{gx, gy + glyph->height},
                    {glyph->u0, glyph->v1},
                    static_cast<uint8_t>(color.r * 255),
                    static_cast<uint8_t>(color.g * 255),
                    static_cast<uint8_t>(color.b * 255),
                    static_cast<uint8_t>(color.a * 255)});

                // 6 indices for 2 triangles
                mesh.indices.push_back(vertex_offset + 0);
                mesh.indices.push_back(vertex_offset + 1);
                mesh.indices.push_back(vertex_offset + 2);
                mesh.indices.push_back(vertex_offset + 0);
                mesh.indices.push_back(vertex_offset + 2);
                mesh.indices.push_back(vertex_offset + 3);

                vertex_offset += 4;
            }

            x += glyph->advance;
        }

        return mesh;
    }

    int measure_width(FT_Face face, const std::string& text, uint32_t font_size) {
        int width = 0;
        int max_width = 0;

        FT_Set_Pixel_Sizes(face, 0, font_size);

        for (char c : text) {
            if (c == '\n') {
                max_width = std::max(max_width, width);
                width = 0;
                continue;
            }

            FT_UInt glyph_index = FT_Get_Char_Index(face, c);
            auto glyph = atlas_.get_or_add(face, glyph_index, font_size);

            if (glyph) {
                width += glyph->advance;
            }
        }

        return std::max(max_width, width);
    }

private:
    GlyphAtlas& atlas_;
};
```

## SDF Text Renderer

Рендеринг текста с Signed Distance Field для масштабирования:

```cpp
class SDFTextRenderer {
public:
    std::optional<GlyphAtlas::Glyph> add_glyph_sdf(
        FT_Face face,
        FT_UInt glyph_index,
        uint32_t size,
        float spread = 8.0f
    ) {
        FT_Set_Pixel_Sizes(face, 0, size);

        // Загрузка в SDF режиме (FreeType 2.10+)
        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        if (error) return std::nullopt;

        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);

        FT_Bitmap& bitmap = face->glyph->bitmap;

        // Добавление в атлас
        // ... аналогично обычному glyph atlas

        return {};
    }

    // Шейдер для SDF рендеринга:
    // float distance = texture(sdf_atlas, uv).r;
    // float alpha = smoothstep(0.5 - threshold, 0.5 + threshold, distance);
    // frag_color = vec4(color.rgb, color.a * alpha);
};
```

## Воксельный HUD

Паттерн для HUD в воксельном движке:

```cpp
class VoxelHUD {
public:
    VoxelHUD(FontCache& fonts, GlyphAtlas& atlas)
        : fonts_(fonts), renderer_(atlas) {}

    void render_debug_info(
        FT_Face font,
        const glm::vec3& player_pos,
        int fps,
        int chunk_count
    ) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
            "FPS: %d\n"
            "Pos: %.1f %.1f %.1f\n"
            "Chunks: %d",
            fps, player_pos.x, player_pos.y, player_pos.z, chunk_count);

        auto mesh = renderer_.build_text_mesh(
            font,
            buffer,
            16,
            {10, 10},
            {1, 1, 1, 0.8f}
        );

        // Рендеринг mesh через Vulkan
        submit_mesh(mesh);
    }

private:
    FontCache& fonts_;
    TextRenderer renderer_;
};
```

## Рекомендации

1. **Один GlyphAtlas на шрифт/размер** — минимизирует переключения текстур
2. **Предзагрузка частых символов** — ASCII + язык интерфейса
3. **SDF для масштабируемого текста** — особенно для 3D текста в мире
4. **Batch рендеринг** — группировка текста в один draw call
5. **Пул GlyphAtlas** — для динамических размеров шрифтов
