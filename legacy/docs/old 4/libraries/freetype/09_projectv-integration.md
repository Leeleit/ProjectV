# Интеграция FreeType с ProjectV

🔴 **Уровень 3: Продвинутый**

Связка FreeType с Vulkan рендерером, SDL3, VMA и RmlUi в ProjectV.

## Обзор интеграции

FreeType в ProjectV используется для:

- Рендеринга UI текста через RmlUi (FontEngineInterface)
- Прямого рендеринга текста в Vulkan текстуры
- Генерации glyph atlas для GPU

## Архитектура

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│   RmlUi     │────>│ FontEngine   │────>│  FreeType   │
│  (UI Layer) │     │  Interface   │     │  (Library)  │
└─────────────┘     └──────────────┘     └─────────────┘
                            │
                            v
                     ┌──────────────┐
                     │ GlyphAtlas   │
                     │   (VMA)      │
                     └──────────────┘
                            │
                            v
                     ┌──────────────┐
                     │   Vulkan     │
                     │  Rendering   │
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
