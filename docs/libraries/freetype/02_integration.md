# Интеграция FreeType с ProjectV

## CMake

### Подключение как подмодуль

```cmake
# CMakeLists.txt
add_subdirectory(external/freetype)

target_link_libraries(projectv_core PRIVATE freetype)
```

### Опции конфигурации

```cmake
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)

add_subdirectory(external/freetype)
```

## Заголовки

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
```

## Интеграция с Vulkan

### GlyphAtlas с VMA

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <vk_mem_alloc.h>

struct GlyphMetrics {
    uint32_t width;
    uint32_t height;
    int32_t bearingX;
    int32_t bearingY;
    uint32_t advance;
};

class GlyphAtlas {
public:
    GlyphAtlas(VmaAllocator allocator, VkDevice device, uint32_t size = 2048)
        : allocator_(allocator), device_(device), size_(size) {
        createImage();
    }

    ~GlyphAtlas() {
        vmaDestroyImage(allocator_, image_, allocation_);
        vkDestroyImageView(device_, imageView_, nullptr);
    }

    std::expected<GlyphMetrics, std::errc> addGlyph(FT_Face face, char32_t codepoint) {
        FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
        if (glyphIndex == 0) {
            return std::unexpected(std::errc::no_such_file_or_directory);
        }

        FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        FT_Bitmap& bitmap = face->glyph->bitmap;

        // Проверка места в атласе
        if (cursorX_ + bitmap.width > size_) {
            cursorX_ = 0;
            cursorY_ += maxRowHeight_;
            maxRowHeight_ = 0;
        }

        if (cursorY_ + bitmap.rows > size_) {
            return std::unexpected(std::errc::no_space_on_device);
        }

        // Копирование в staging и загрузка на GPU
        copyToAtlas(bitmap, cursorX_, cursorY_);

        GlyphMetrics metrics{
            .width = bitmap.width,
            .height = bitmap.rows,
            .bearingX = face->glyph->bitmap_left,
            .bearingY = face->glyph->bitmap_top,
            .advance = static_cast<uint32_t>(face->glyph->advance.x >> 6)
        };

        cursorX_ += bitmap.width + 1;
        maxRowHeight_ = std::max(maxRowHeight_, bitmap.rows);

        return metrics;
    }

    VkImageView imageView() const { return imageView_; }

private:
    void createImage() {
        VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .extent = {size_, size_, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        VmaAllocationCreateInfo allocInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image_, &allocation_, nullptr);

        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image_,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
        };
        vkCreateImageView(device_, &viewInfo, nullptr, &imageView_);
    }

    void copyToAtlas(const FT_Bitmap& bitmap, uint32_t x, uint32_t y) {
        // Staging buffer + vkCmdCopyBuffer2
    }

    VmaAllocator allocator_;
    VkDevice device_;
    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    uint32_t size_;
    uint32_t cursorX_ = 0;
    uint32_t cursorY_ = 0;
    uint32_t maxRowHeight_ = 0;
};
```

## Интеграция с Flecs ECS

```cpp
#include <flecs.h>

struct FontResource {
    FT_Library library;
    FT_Face face;
    std::string path;
};

struct FontHandle {
    flecs::entity entity;
};

class FontSystem {
public:
    FontSystem(flecs::world& world) : world_(world) {
        FT_Init_FreeType(&library_);

        world_.system<FontResource>("FontLoader")
            .with<FontHandle>()
            .each([this](flecs::entity e, FontResource& font) {
                if (font.face) return;
                FT_New_Face(library_, font.path.c_str(), 0, &font.face);
            });
    }

    ~FontSystem() {
        FT_Done_FreeType(library_);
    }

    std::expected<FontHandle, std::errc> loadFont(const std::string& path) {
        FT_Face face;
        FT_Error error = FT_New_Face(library_, path.c_str(), 0, &face);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        auto entity = world_.entity();
        entity.set<FontResource>({library_, face, path});
        entity.set<FontHandle>({entity});

        return FontHandle{entity};
    }

private:
    flecs::world& world_;
    FT_Library library_;
};
```

## SDL3 интеграция

```cpp
#include <SDL3/SDL.h>
#include <ft2build.h>
#include <freetype/freetype.h>

class SdlFontLoader {
public:
    explicit SdlFontLoader(FT_Library library) : library_(library) {}

    std::expected<FT_Face, std::errc> loadFromStorage(SDL_Storage* storage, const char* path) {
        SDL_StorageEnumerator* enumerator = SDL_OpenStorageStorageEnumerator(storage, path);
        if (!enumerator) {
            return std::unexpected(std::errc::no_such_file_or_directory);
        }

        // Чтение через SDL_* storage API
        SDL_PropertiesID props = SDL_GetStorageEnumeratorProperties(enumerator);
        if (!SDL_Storage_Enumerate(props, nullptr)) {
            SDL_CloseStorageEnumerator(enumerator);
            return std::unexpected(std::errc::no_such_file_or_directory);
        }

        // Читаем данные файла
        int64_t size = 0;
        void* data = SDL_LoadFileFromStorage(storage, path, &size);
        if (!data) {
            SDL_CloseStorageEnumerator(enumerator);
            return std::unexpected(std::errc::io_error);
        }

        FT_Face face;
        FT_Error error = FT_New_Memory_Face(library_, data, size, 0, &face);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        return face;
    }

private:
    FT_Library library_;
};
```
