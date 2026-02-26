# Продвинутые оптимизации FreeType для ProjectV

## Data-Oriented Design

### SoA для Glyph Cache

```cpp
template<size_t Alignment = 64>
struct GlyphCacheSoA {
    alignas(Alignment) std::vector<uint32_t> codepoints;
    alignas(Alignment) std::vector<uint32_t> glyphIndices;
    alignas(Alignment) std::vector<uint32_t> widths;
    alignas(Alignment) std::vector<uint32_t> heights;
    alignas(Alignment) std::vector<int32_t> bearingsX;
    alignas(Alignment) std::vector<int32_t> bearingsY;
    alignas(Alignment) std::vector<uint32_t> advances;
    alignas(Alignment) std::vector<uint32_t> atlasOffsets;

    void add(uint32_t cp, uint32_t gi, uint32_t w, uint32_t h,
             int32_t bx, int32_t by, uint32_t adv, uint32_t offset) {
        codepoints.push_back(cp);
        glyphIndices.push_back(gi);
        widths.push_back(w);
        heights.push_back(h);
        bearingsX.push_back(bx);
        bearingsY.push_back(by);
        advances.push_back(adv);
        atlasOffsets.push_back(offset);
    }

    std::optional<size_t> find(uint32_t codepoint) const {
        for (size_t i = 0; i < codepoints.size(); ++i) {
            if (codepoints[i] == codepoint) return i;
        }
        return std::nullopt;
    }
};
```

### Hot/Cold Separation

```cpp
struct FontDataDOD {
    // Hot data - часто используется при рендеринге
    alignas(64) struct HotData {
        std::vector<uint32_t> asciiGlyphIndices;  // Первые 128 глифов
        std::vector<uint32_t> asciiAdvances;
    } hot;

    // Cold data - метрики, метаданные
    struct ColdData {
        std::string familyName;
        std::string styleName;
        uint32_t unitsPerEM;
        int16_t ascender;
        int16_t descender;
    } cold;

    // GPU данные - текстурный атлас
    struct GpuData {
        VkImage atlasImage;
        VmaAllocation atlasAllocation;
        VkImageView atlasView;
    } gpu;
};
```

## Job System

### Parallel Glyph Preloading

```cpp
#include <flecs.h>

struct GlyphBatchJob {
    FT_Face face;
    std::span<const char32_t> codepoints;
    GlyphCacheSoA<>& cache;
    std::atomic<size_t>* processed;
};

void processGlyphBatch(GlyphBatchJob* job) {
    for (size_t i = 0; i < job->codepoints.size(); ++i) {
        char32_t cp = job->codepoints[i];
        FT_UInt gi = FT_Get_Char_Index(job->face, cp);

        if (gi == 0) continue;

        FT_Error error = FT_Load_Glyph(job->face, gi, FT_LOAD_RENDER);
        if (error) continue;

        auto& slot = job->face->glyph;

        job->cache.add(
            cp, gi,
            slot->bitmap.width, slot->bitmap.rows,
            slot->bitmap_left, slot->bitmap_top,
            slot->advance.x >> 6,
            0  // offset in atlas
        );

        job->processed->fetch_add(1);
    }
}
```

### Multi-threaded Font Loading

```cpp
class ParallelFontLoader {
public:
    void loadFontsAsync(std::span<const std::string> paths) {
        for (const auto& path : paths) {
            std::jthread([this, &path]() {
                FT_Library lib;
                FT_Init_FreeType(&lib);

                FT_Face face;
                FT_New_Face(lib, path.c_str(), 0, &face);

                {
                    std::lock_guard lock(mutex_);
                    loadedFaces_.push_back({lib, face, path});
                }
            });
        }
    }

private:
    std::mutex mutex_;
    std::vector<std::tuple<FT_Library, FT_Face, std::string>> loadedFaces_;
};
```

## Tracy профилирование

```cpp
#include <tracy/Tracy.hpp>

void renderTextProfiling(FT_Face face, const char* text) {
    ZoneScoped;

    FT_Pos penX = 0;

    for (const char* p = text; *p; ++p) {
        {
            ZoneScopedN("FT_Get_Char_Index");
            FT_UInt glyphIndex = FT_Get_Char_Index(face, *p);
        }

        {
            ZoneScopedN("FT_Load_Render");
            FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);
        }

        penX += face->glyph->advance.x;
    }
}
```

## Zero-allocation паттерны

### Staging Buffer для атласа

```cpp
class AtlasUploader {
public:
    AtlasUploader(VmaAllocator allocator, VkDevice device, VkCommandPool cmdPool, VkBuffer atlasBuffer)
        : allocator_(allocator), device_(device), cmdPool_(cmdPool), atlasBuffer_(atlasBuffer) {}

    void uploadGlyphs(GlyphCacheSoA<>& cache, const FT_Bitmap& bitmap, uint32_t offset) {
        // Staging buffer для минимального копирования
        VkBufferCreateInfo stagingInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bitmap.rows * bitmap.pitch,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };

        VmaAllocationCreateInfo stagingAllocInfo{
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU
        };

        VkBuffer staging;
        VmaAllocation stagingAlloc;
        vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo, &staging, &stagingAlloc, nullptr);

        // Копирование bitmap данных в staging буфер
        void* mapped;
        vmaMapMemory(allocator_, stagingAlloc, &mapped);
        std::memcpy(mapped, bitmap.buffer, bitmap.rows * bitmap.pitch);
        vmaUnmapMemory(allocator_, stagingAlloc);

        // Создание command buffer для копирования на GPU
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        // Копирование из staging в GPU буфер атласа
        VkBufferCopy copyRegion{};
        copyRegion.size = bitmap.rows * bitmap.pitch;
        copyRegion.dstOffset = offset;
        vkCmdCopyBuffer(cmdBuffer, staging, atlasBuffer_, 1, &copyRegion);

        vkEndCommandBuffer(cmdBuffer);

        // Submit и ожидание завершения
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device_, 0, 0, &graphicsQueue);
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        // Освобождение staging ресурсов
        vkFreeCommandBuffers(device_, cmdPool_, 1, &cmdBuffer);
        vmaDestroyBuffer(allocator_, staging, stagingAlloc);
    }

private:
    VmaAllocator allocator_;
    VkDevice device_;
    VkCommandPool cmdPool_;
    VkBuffer atlasBuffer_;
};
```

## Batch Rendering

### Glyph Batching

```cpp
struct TextBatch {
    struct GlyphInstance {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec2 uv0;
        glm::vec2 uv1;
        glm::u32vec4 color;  // RGBA u32
    };

    std::vector<GlyphInstance> instances;

    void addGlyph(glm::vec2 pos, const GlyphMetrics& metrics, glm::vec4 color) {
        instances.push_back({
            .position = pos,
            .size = glm::vec2(metrics.width, metrics.height),
            .uv0 = glm::vec2(0, 0),  // from atlas
            .uv1 = glm::vec2(1, 1),  // from atlas
            .color = packColor(color)
        });
    }

    static uint32_t packColor(glm::vec4 c) {
        return (static_cast<uint32_t>(c.a * 255) << 24) |
               (static_cast<uint32_t>(c.b * 255) << 16) |
               (static_cast<uint32_t>(c.g * 255) << 8) |
               static_cast<uint32_t>(c.r * 255);
    }
};
```

## Рекомендации для ProjectV

1. **GlyphCacheSoA** — используй SoA с `alignas(64)` для кэша глифов
2. **Preload ASCII** — загружай первые 128 символов при инициализации шрифта
3. **Parallel loading** — используй std::jthread для асинхронной загрузки шрифтов
4. **Tracy** — профилируй FT_Load_Glyph и FT_Render_Glyph отдельно
5. **Staging buffers** — минимизируй копирование при загрузке в GPU атлас
