# Интеграция FreeType с Vulkan-ориентированным движком

## CMake

### Подключение как подмодуль

```cmake
# CMakeLists.txt
add_subdirectory(external/freetype)

target_link_libraries(engine_core PRIVATE freetype)
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

## SDF (Signed Distance Field) рендеринг

### Генерация SDF текстур

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

struct SDFGlyph {
    std::vector<float> distanceField;
    uint32_t width;
    uint32_t height;
    glm::vec2 bearing;
    float advance;
};

class SDFGenerator {
public:
    SDFGlyph generateSDF(FT_Face face, char32_t codepoint, uint32_t size, float spread = 8.0f) {
        // Устанавливаем размер
        FT_Set_Pixel_Sizes(face, 0, size);

        // Загружаем глиф
        FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
        FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);

        // Получаем контур
        FT_Outline& outline = face->glyph->outline;

        // Создаем SDF текстуру
        uint32_t textureSize = size + spread * 2;
        SDFGlyph glyph;
        glyph.width = textureSize;
        glyph.height = textureSize;
        glyph.distanceField.resize(textureSize * textureSize, 0.0f);

        // Генерируем SDF
        generateDistanceField(outline, glyph.distanceField, textureSize, spread);

        // Сохраняем метрики
        glyph.bearing = glm::vec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
        glyph.advance = face->glyph->advance.x / 64.0f;

        return glyph;
    }

private:
    void generateDistanceField(FT_Outline& outline, std::vector<float>& field,
                              uint32_t size, float spread) {
        // Простой алгоритм генерации SDF
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                glm::vec2 point(x - spread, y - spread);
                float distance = signedDistanceToOutline(outline, point);

                // Нормализуем расстояние
                float normalized = 0.5f + 0.5f * (distance / spread);
                field[y * size + x] = std::clamp(normalized, 0.0f, 1.0f);
            }
        }
    }

    float signedDistanceToOutline(FT_Outline& outline, const glm::vec2& point) {
        float minDistance = std::numeric_limits<float>::max();
        bool inside = false;

        // Обходим все контуры
        for (int contour = 0; contour < outline.n_contours; ++contour) {
            int start = (contour == 0) ? 0 : outline.contours[contour - 1] + 1;
            int end = outline.contours[contour];

            // Проверяем точку относительно контура
            if (isPointInContour(outline, start, end, point)) {
                inside = !inside;
            }

            // Находим минимальное расстояние до сегментов
            for (int i = start; i <= end; ++i) {
                int next = (i == end) ? start : i + 1;
                glm::vec2 p1(outline.points[i].x / 64.0f, outline.points[i].y / 64.0f);
                glm::vec2 p2(outline.points[next].x / 64.0f, outline.points[next].y / 64.0f);

                float distance = distanceToSegment(point, p1, p2);
                minDistance = std::min(minDistance, distance);
            }
        }

        return inside ? -minDistance : minDistance;
    }

    bool isPointInContour(FT_Outline& outline, int start, int end, const glm::vec2& point) {
        // Реализация алгоритма winding number
        int winding = 0;

        for (int i = start; i <= end; ++i) {
            int next = (i == end) ? start : i + 1;
            glm::vec2 p1(outline.points[i].x / 64.0f, outline.points[i].y / 64.0f);
            glm::vec2 p2(outline.points[next].x / 64.0f, outline.points[next].y / 64.0f);

            if (p1.y <= point.y) {
                if (p2.y > point.y && isLeft(p1, p2, point) > 0) {
                    winding++;
                }
            } else {
                if (p2.y <= point.y && isLeft(p1, p2, point) < 0) {
                    winding--;
                }
            }
        }

        return winding != 0;
    }

    float isLeft(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& point) {
        return (p1.x - p0.x) * (point.y - p0.y) - (point.x - p0.x) * (p1.y - p0.y);
    }

    float distanceToSegment(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b) {
        glm::vec2 ab = b - a;
        glm::vec2 ap = point - a;

        float t = glm::dot(ap, ab) / glm::dot(ab, ab);
        t = std::clamp(t, 0.0f, 1.0f);

        glm::vec2 closest = a + t * ab;
        return glm::distance(point, closest);
    }
};
```

### Vulkan шейдер для SDF рендеринга

```glsl
// SDF шейдер (GLSL)
#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(push_constant) uniform PushConstants {
    vec4 color;
    float smoothing;
    float outlineWidth;
    vec4 outlineColor;
} push;

void main() {
    float distance = texture(fontAtlas, inTexCoord).r;

    // SDF рендеринг с антиалиасингом
    float smoothing = push.smoothing / fwidth(distance);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);

    // Outline эффект
    if (push.outlineWidth > 0.0) {
        float outlineAlpha = smoothstep(
            0.5 - push.outlineWidth - smoothing,
            0.5 - push.outlineWidth + smoothing,
            distance
        );

        vec4 finalColor = mix(push.outlineColor, push.color * inColor, alpha);
        finalColor.a *= max(alpha, outlineAlpha);
        outColor = finalColor;
    } else {
        outColor = push.color * inColor;
        outColor.a *= alpha;
    }
}
```

## Многопоточность и кэширование

### Thread-safe Font Manager

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <memory>

class ThreadSafeFontCache {
    struct FontEntry {
        FT_Face face;
        std::chrono::steady_clock::time_point lastAccess;
        size_t size;
    };

public:
    ThreadSafeFontCache(size_t maxCacheSize = 1024 * 1024 * 100) // 100MB
        : maxCacheSize_(maxCacheSize), currentSize_(0) {
        FT_Init_FreeType(&library_);
    }

    ~ThreadSafeFontCache() {
        std::unique_lock lock(mutex_);
        for (auto& [path, entry] : cache_) {
            FT_Done_Face(entry.face);
        }
        FT_Done_FreeType(library_);
    }

    std::expected<FT_Face, std::errc> getFont(const std::string& path, uint32_t size) {
        std::string key = path + ":" + std::to_string(size);

        {
            std::shared_lock lock(mutex_);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                it->second.lastAccess = std::chrono::steady_clock::now();
                return it->second.face;
            }
        }

        // Загружаем новый шрифт
        FT_Face face;
        FT_Error error = FT_New_Face(library_, path.c_str(), 0, &face);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        FT_Set_Pixel_Sizes(face, 0, size);

        // Оцениваем размер в кэше
        size_t estimatedSize = estimateFontSize(face);

        {
            std::unique_lock lock(mutex_);

            // Проверяем, есть ли место в кэше
            if (currentSize_ + estimatedSize > maxCacheSize_) {
                evictOldEntries();
            }

            // Добавляем в кэш
            cache_[key] = FontEntry{
                .face = face,
                .lastAccess = std::chrono::steady_clock::now(),
                .size = estimatedSize
            };

            currentSize_ += estimatedSize;
        }

        return face;
    }

    void preloadFont(const std::string& path, const std::vector<uint32_t>& sizes) {
        std::vector<std::thread> threads;

        for (uint32_t size : sizes) {
            threads.emplace_back([this, path, size]() {
                getFont(path, size);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

private:
    size_t estimateFontSize(FT_Face face) {
        // Простая оценка размера шрифта в памяти
        size_t size = sizeof(FT_FaceRec);

        // Оцениваем размер глифов (грубая оценка)
        size += face->num_glyphs * 1024; // ~1KB на глиф

        return size;
    }

    void evictOldEntries() {
        // Удаляем старые записи пока не освободим достаточно места
        std::vector<std::string> toRemove;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [key, entry] : cache_) {
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - entry.lastAccess);
            if (age > std::chrono::minutes(5)) { // 5 минут без использования
                toRemove.push_back(key);
                currentSize_ -= entry.size;

                if (currentSize_ <= maxCacheSize_ * 0.7) { // Освободили 30%
                    break;
                }
            }
        }

        for (const auto& key : toRemove) {
            FT_Done_Face(cache_[key].face);
            cache_.erase(key);
        }
    }

    FT_Library library_;
    std::shared_mutex mutex_;
    std::unordered_map<std::string, FontEntry> cache_;
    size_t maxCacheSize_;
    size_t currentSize_;
};
```

## Вариативные шрифты

### Работа с Variable Fonts

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftmm.h>

struct FontAxis {
    std::string tag;
    float minValue;
    float defaultValue;
    float maxValue;
};

class VariableFontManager {
public:
    VariableFontManager(FT_Library library) : library_(library) {}

    std::expected<std::vector<FontAxis>, std::errc> getAxes(const std::string& path) {
        FT_Face face;
        FT_Error error = FT_New_Face(library_, path.c_str(), 0, &face);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        if (!FT_IS_VARIATION(face)) {
            FT_Done_Face(face);
            return std::unexpected(std::errc::invalid_argument);
        }

        FT_MM_Var* mmVar = nullptr;
        error = FT_Get_MM_Var(face, &mmVar);
        if (error) {
            FT_Done_Face(face);
            return std::unexpected(std::errc::io_error);
        }

        std::vector<FontAxis> axes;
        axes.reserve(mmVar->num_axis);

        for (FT_UInt i = 0; i < mmVar->num_axis; ++i) {
            FT_Var_Axis& axis = mmVar->axis[i];

            FontAxis fontAxis{
                .tag = std::string(axis.name, 4),
                .minValue = axis.minimum / 65536.0f,
                .defaultValue = axis.def / 65536.0f,
                .maxValue = axis.maximum / 65536.0f
            };

            axes.push_back(fontAxis);
        }

        FT_Done_MM_Var(library_, mmVar);
        FT_Done_Face(face);

        return axes;
    }

    std::expected<FT_Face, std::errc> createInstance(
        const std::string& path,
        const std::unordered_map<std::string, float>& axisValues) {

        FT_Face face;
        FT_Error error = FT_New_Face(library_, path.c_str(), 0, &face);
        if (error) {
            return std::unexpected(std::errc::io_error);
        }

        if (!FT_IS_VARIATION(face)) {
            FT_Done_Face(face);
            return std::unexpected(std::errc::invalid_argument);
        }

        // Подготавливаем массив значений осей
        FT_MM_Var* mmVar = nullptr;
        error = FT_Get_MM_Var(face, &mmVar);
        if (error) {
            FT_Done_Face(face);
            return std::unexpected(std::errc::io_error);
        }

        std::vector<FT_Fixed> coords(mmVar->num_axis);

        // Устанавливаем значения по умолчанию
        for (FT_UInt i = 0; i < mmVar->num_axis; ++i) {
            coords[i] = mmVar->axis[i].def;
        }

        // Применяем пользовательские значения
        for (const auto& [tag, value] : axisValues) {
            for (FT_UInt i = 0; i < mmVar->num_axis; ++i) {
                if (std::string(mmVar->axis[i].name, 4) == tag) {
                    // Конвертируем в fixed point
                    coords[i] = static_cast<FT_Fixed>(value * 65536.0f);
                    coords[i] = std::clamp(coords[i], mmVar->axis[i].minimum, mmVar->axis[i].maximum);
                    break;
                }
            }
        }

        // Применяем координаты к шрифту
        error = FT_Set_Var_Design_Coordinates(face, coords.size(), coords.data());
        if (error) {
            FT_Done_MM_Var(library_, mmVar);
            FT_Done_Face(face);
            return std::unexpected(std::errc::io_error);
        }

        FT_Done_MM_Var(library_, mmVar);
        return face;
    }

private:
    FT_Library library_;
};
```

## Оптимизация для Vulkan

### Batch рендеринг текста

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <vector>
#include <glm/glm.hpp>

struct TextVertex {
    glm::vec2 position;
    glm::vec2 texCoord;
    glm::vec4 color;
};

class TextBatchRenderer {
public:
    TextBatchRenderer(size_t maxQuads = 4096) : maxQuads_(maxQuads) {
        vertices_.reserve(maxQuads * 4);
        indices_.reserve(maxQuads * 6);
    }

    void addText(const std::string& text, FT_Face face,
                 const glm::vec2& position, const glm::vec4& color,
                 float scale = 1.0f) {
        glm::vec2 cursor = position;

        for (char32_t codepoint : text) {
            FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
            if (glyphIndex == 0) continue;

            FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);

            FT_GlyphSlot glyph = face->glyph;
            FT_Bitmap& bitmap = glyph->bitmap;

            float x = cursor.x + glyph->bitmap_left * scale;
            float y = cursor.y - glyph->bitmap_top * scale;
            float w = bitmap.width * scale;
            float h = bitmap.rows * scale;

            // Добавляем квад
            addQuad(x, y, w, h, color);

            // Сдвигаем курсор
            cursor.x += (glyph->advance.x >> 6) * scale;
        }
    }

    void clear() {
        vertices_.clear();
        indices_.clear();
        indexOffset_ = 0;
    }

    const std::vector<TextVertex>& getVertices() const { return vertices_; }
    const std::vector<uint32_t>& getIndices() const { return indices_; }

private:
    void addQuad(float x, float y, float w, float h, const glm::vec4& color) {
        // Вершины квада
        vertices_.push_back({{x, y}, {0.0f, 0.0f}, color});
        vertices_.push_back({{x + w, y}, {1.0f, 0.0f}, color});
        vertices_.push_back({{x + w, y + h}, {1.0f, 1.0f}, color});
        vertices_.push_back({{x, y + h}, {0.0f, 1.0f}, color});

        // Индексы
        uint32_t base = indexOffset_;
        indices_.push_back(base);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);

        indexOffset_ += 4;
    }

    std::vector<TextVertex> vertices_;
    std::vector<uint32_t> indices_;
    size_t maxQuads_;
    uint32_t indexOffset_ = 0;
};
```

### Compute shader для генерации SDF

```glsl
// Compute shader для генерации SDF (GLSL)
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, r32f) uniform image2D outputImage;
layout(binding = 1) uniform sampler2D outlineTexture;

layout(push_constant) uniform PushConstants {
    ivec2 imageSize;
    float spread;
} push;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    if (pixelCoord.x >= push.imageSize.x || pixelCoord.y >= push.imageSize.y) {
        return;
    }

    vec2 coord = vec2(pixelCoord) / vec2(push.imageSize);
    float distance = texture(outlineTexture, coord).r;

    // Генерация SDF (упрощенный алгоритм)
    float sdfValue = 0.5 + 0.5 * (distance / push.spread);
    sdfValue = clamp(sdfValue, 0.0, 1.0);

    imageStore(outputImage, pixelCoord, vec4(sdfValue, 0.0, 0.0, 1.0));
}
```

### Vulkan pipeline для текста

```cpp
#include <vulkan/vulkan.h>
#include <vector>

class TextPipeline {
public:
    TextPipeline(VkDevice device, VkRenderPass renderPass)
        : device_(device) {
        createPipeline(renderPass);
    }

    ~TextPipeline() {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    }

    VkPipeline getPipeline() const { return pipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    void createPipeline(VkRenderPass renderPass) {
        // Шейдерные модули
        VkShaderModule vertShader = createShaderModule(vertexShaderCode);
        VkShaderModule fragShader = createShaderModule(fragmentShaderCode);

        // Pipeline layout
        VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange_
        };

        vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_);

        // Graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages_,
            .pVertexInputState = &vertexInputInfo_,
            .pInputAssemblyState = &inputAssembly_,
            .pViewportState = &viewportState_,
            .pRasterizationState = &rasterizer_,
            .pMultisampleState = &multisampling_,
            .pDepthStencilState = &depthStencil_,
            .pColorBlendState = &colorBlending_,
            .pDynamicState = &dynamicState_,
            .layout = pipelineLayout_,
            .renderPass = renderPass,
            .subpass = 0
        };

        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);

        vkDestroyShaderModule(device_, vertShader, nullptr);
        vkDestroyShaderModule(device_, fragShader, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<uint32_t>& code) {
        VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size() * sizeof(uint32_t),
            .pCode = code.data()
        };

        VkShaderModule shaderModule;
        vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule);
        return shaderModule;
    }

    VkDevice device_;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // Конфигурация pipeline
    VkPushConstantRange pushConstantRange_{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineShaderStageCreateInfo shaderStages_[2];
    VkPipelineVertexInputStateCreateInfo vertexInputInfo_;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly_;
    VkPipelineViewportStateCreateInfo viewportState_;
    VkPipelineRasterizationStateCreateInfo rasterizer_;
    VkPipelineMultisampleStateCreateInfo multisampling_;
    VkPipelineDepthStencilStateCreateInfo depthStencil_;
    VkPipelineColorBlendStateCreateInfo colorBlending_;
    VkPipelineDynamicStateCreateInfo dynamicState_;

    struct PushConstants {
        glm::vec4 color;
        float smoothing;
        float outlineWidth;
        glm::vec4 outlineColor;
    };
};
```

## Лучшие практики

### 1. Кэширование глифов

- Используйте LRU кэш для часто используемых глифов
- Предзагружайте основные символы (A-Z, 0-9, пунктуация)
- Используйте SDF для динамического масштабирования

### 2. Оптимизация памяти

- Используйте VMA для управления памятью Vulkan
- Сжимайте текстуры атласов (BC4 для монохромных шрифтов)
- Освобождайте неиспользуемые шрифты из кэша

### 3. Производительность рендеринга

- Используйте batch рендеринг для минимизации draw calls
- Применяйте instancing для статического текста
- Используйте compute shaders для генерации SDF

### 4. Качество рендеринга

- Для UI используйте SDF с антиалиасингом
- Для больших текстов используйте стандартный рендеринг с hinting
- Настраивайте LCD subpixel rendering для мониторов

### 5. Поддержка платформ

- Windows: DirectWrite для системных шрифтов
- Linux: Fontconfig для поиска шрифтов
- Все платформы: встраивание шрифтов в приложение
