# Паттерны FreeType в ProjectV

🔴 **Уровень 3: Продвинутый**

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