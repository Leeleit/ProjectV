# Game UI Strategy

**🟡 Уровень 2: Средний** — Стратегия пользовательского интерфейса для ProjectV.

---

## Концепция

### Разделение UI на два типа

ProjectV разделяет UI на две категории с принципиально разными требованиями:

| Тип          | Назначение                                         | Инструмент          |
|--------------|----------------------------------------------------|---------------------|
| **Debug UI** | Инструменты разработчика, редактор, профилирование | ImGui               |
| **Game HUD** | Здоровье, прицел, инвентарь, меню                  | Custom Mesh / RmlUi |

### Почему не ImGui для Game HUD?

ImGui — отличная библиотека, но она создана для **инструментов разработчика**, не для игрового интерфейса:

| Проблема ImGui для HUD           | Влияние на игру              |
|----------------------------------|------------------------------|
| Immediate mode                   | Высокий overhead каждый кадр |
| Нет анимаций                     | Нужны кастомные решения      |
| Нет localization-friendly layout | Сложности с переводом        |
| Стиль "developer tools"          | Выглядит непрофессионально   |
| Нет touch support                | Проблемы на мобильных        |

---

## Debug UI: ImGui

### Использование

```cpp
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

class DebugUI {
public:
    DebugUI(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
            VkQueue queue, VkCommandPool commandPool) {
        // Создание ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Настройка style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 2.0f;

        // Инициализация backends
        ImGui_ImplSDL3_InitForVulkan(window_);
        ImGui_ImplVulkan_InitInfo init_info = {
            .Instance = instance,
            .PhysicalDevice = physicalDevice,
            .Device = device,
            .QueueFamilyIndex = queueFamilyIndex,
            .Queue = queue,
            .DescriptorPool = descriptorPool_,
            .MinImageCount = 2,
            .ImageCount = 2,
        };
        ImGui_ImplVulkan_Init(&init_info, renderPass_);

        // Загрузка шрифтов
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
    }

    void beginFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void endFrame(VkCommandBuffer cmd) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }

    void drawDebugOverlay(const GameStats& stats) {
        ImGui::Begin("Debug Overlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("FPS: %.1f", stats.fps);
        ImGui::Text("Frame: %.2f ms", stats.frameTime);
        ImGui::Text("Draw Calls: %d", stats.drawCalls);
        ImGui::Text("Triangles: %d", stats.triangles);
        ImGui::Text("Voxels Visible: %d", stats.visibleVoxels);

        ImGui::End();
    }

    void drawEntityInspector(flecs::entity entity) {
        ImGui::Begin("Entity Inspector");

        ImGui::Text("Entity: %llu", entity.id());
        ImGui::Separator();

        // Показ компонентов
        entity.each([](flecs::id id, void* ptr) {
            const char* typeName = id.str();
            if (ImGui::CollapsingHeader(typeName)) {
                // Рефлексия для редактирования компонента
                // ... кастомный код для каждого типа
            }
        });

        ImGui::End();
    }

    void drawPerformanceGraph(const std::vector<float>& frameTimes) {
        ImGui::Begin("Performance");

        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.2f ms", frameTimes.back());
        ImGui::PlotLines("Frame Time", frameTimes.data(), frameTimes.size(),
                        0, overlay, 0.0f, 50.0f, ImVec2(0, 80));

        ImGui::End();
    }

private:
    SDL_Window* window_;
    VkDescriptorPool descriptorPool_;
    VkRenderPass renderPass_;
};
```

### Debug Features

```cpp
// Включение debug UI только в Debug сборке
#ifdef NDEBUG
    constexpr bool ENABLE_DEBUG_UI = false;
#else
    constexpr bool ENABLE_DEBUG_UI = true;
#endif

void Game::update(float deltaTime) {
    // ... game logic

    if constexpr (ENABLE_DEBUG_UI) {
        debugUI_.beginFrame();

        // Debug overlay (всегда виден)
        debugUI_.drawDebugOverlay(stats_);

        // Entity inspector (по запросу)
        if (debugEntityInspector_) {
            debugUI_.drawEntityInspector(selectedEntity_);
        }

        // Performance graph
        debugUI_.drawPerformanceGraph(frameTimes_);

        debugUI_.endFrame(commandBuffer_);
    }
}
```

---

## Game HUD: Кастомное решение

### Вариант 1: Custom Mesh UI

Для простых HUD элементов (прицел, здоровье) можно использовать генерацию mesh напрямую.

```cpp
class HUDRenderer {
public:
    void renderHealthBar(VkCommandBuffer cmd, float health, float maxHealth) {
        // Генерация quad для health bar
        float width = 200.0f;
        float height = 20.0f;
        float healthPercent = health / maxHealth;

        // Background quad
        glm::vec2 bgPos = glm::vec2(20.0f, 20.0f);
        glm::vec2 bgSize = glm::vec2(width, height);

        // Foreground quad (здоровье)
        glm::vec2 fgPos = bgPos;
        glm::vec2 fgSize = glm::vec2(width * healthPercent, height);

        // Генерация вершин
        std::vector<UINVertex> vertices;
        std::vector<uint16_t> indices;

        // Background (тёмно-серый)
        addQuad(vertices, indices, bgPos, bgSize, glm::vec4(0.2f, 0.2f, 0.2f, 0.8f));

        // Foreground (зелёный → красный в зависимости от здоровья)
        glm::vec4 healthColor = glm::mix(
            glm::vec4(1.0f, 0.2f, 0.2f, 1.0f),  // Красный
            glm::vec4(0.2f, 1.0f, 0.2f, 1.0f),  // Зелёный
            healthPercent
        );
        addQuad(vertices, indices, fgPos, fgSize, healthColor);

        // Загрузка в vertex buffer и рендеринг
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline_);
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &vertexOffset_);
        vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, indices.size(), 1, 0, 0, 0);
    }

    void renderCrosshair(VkCommandBuffer cmd, const glm::vec2& screenCenter) {
        // Простой прицел (4 линии или текстурированный quad)
        // ...
    }

private:
    struct UIVertex {
        glm::vec2 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

    void addQuad(std::vector<UIVertex>& vertices, std::vector<uint16_t>& indices,
                 const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
        uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

        vertices.push_back({pos, {0.0f, 0.0f}, color});
        vertices.push_back({{pos.x + size.x, pos.y}, {1.0f, 0.0f}, color});
        vertices.push_back({{pos.x + size.x, pos.y + size.y}, {1.0f, 1.0f}, color});
        vertices.push_back({{pos.x, pos.y + size.y}, {0.0f, 1.0f}, color});

        indices.insert(indices.end(), {baseIndex, baseIndex + 1, baseIndex + 2,
                                       baseIndex, baseIndex + 2, baseIndex + 3});
    }

    VkPipeline uiPipeline_;
    VkBuffer vertexBuffer_;
    VkBuffer indexBuffer_;
    VkDeviceSize vertexOffset_;
};
```

### Вариант 2: RmlUi (для сложного HUD)

RmlUi — современная библиотека UI на базе HTML/CSS, созданная для игр.

**Преимущества RmlUi:**

- HTML/CSS синтаксис (знакомый художникам и дизайнерам)
- Анимации через CSS transitions
- Локализация через data-attributes
- Отделение дизайна от кода
- Retained mode (эффективнее для сложного UI)

**Интеграция RmlUi:**

```cpp
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

class RmlUiRenderer : public Rml::RenderInterface {
public:
    RmlUiRenderer(VkDevice device, VmaAllocator allocator)
        : device_(device), allocator_(allocator) {
        // Создание pipelines, buffers, etc.
    }

    void RenderGeometry(Rml::Vertex* vertices, int numVertices,
                       int* indices, int numIndices,
                       Rml::TextureHandle texture,
                       const Rml::Vector2f& translation) override {
        // Конвертация RmlUi vertices в Vulkan vertex buffer
        // Установка scissor rectangle
        // Вызов vkCmdDrawIndexed
    }

    void EnableScissorRegion(bool enable) override {
        scissorEnabled_ = enable;
    }

    void SetScissorRegion(int x, int y, int width, int height) override {
        scissorRect_ = {{x, y}, {width, height}};
    }

private:
    VkDevice device_;
    VmaAllocator allocator_;
    bool scissorEnabled_ = false;
    VkRect2D scissorRect_;
};

class GameHUD {
public:
    GameHUD() {
        // Инициализация RmlUi
        Rml::SetRenderInterface(&renderer_);
        Rml::SetSystemInterface(&system_);
        Rml::Initialise();

        // Загрузка шрифтов
        Rml::LoadFontFace("fonts/Roboto-Regular.ttf");

        // Создание контекста
        context_ = Rml::CreateContext("main", {0, 0, 1920, 1080});

        // Загрузка HUD документа
        document_ = context_->LoadDocument("ui/hud.rml");
        document_->Show();
    }

    void update(float deltaTime, const PlayerState& player) {
        // Обновление данных в UI
        auto healthElement = document_->GetElementById("health-value");
        if (healthElement) {
            healthElement->SetAttribute("value",
                Rml::ToString(static_cast<int>(player.health)));
        }

        auto staminaBar = document_->GetElementById("stamina-bar");
        if (staminaBar) {
            staminaBar->SetAttribute("style",
                Rml::String("width: ") + Rml::ToString(player.stamina * 100) + "%;");
        }

        // Обновление контекста RmlUi
        context_->Update();
    }

    void render(VkCommandBuffer cmd) {
        context_->Render();
        // renderer_ уже записывает команды в cmd
    }

private:
    RmlUiRenderer renderer_;
    RmlUiSystem system_;
    Rml::Context* context_;
    Rml::ElementDocument* document_;
};
```

**Пример RmlUi документа:**

```html
<!-- ui/hud.rml -->
<rml>
<head>
    <link type="text/rcss" href="hud.css"/>
</head>
<body>
    <div id="hud-container">
        <!-- Health Bar -->
        <div id="health-section">
            <div id="health-icon"></div>
            <div id="health-bar-bg">
                <div id="health-bar" style="width: 100%;">
                    <span id="health-value">100</span>
                </div>
            </div>
        </div>

        <!-- Stamina Bar -->
        <div id="stamina-section">
            <div id="stamina-bar"></div>
        </div>

        <!-- Crosshair -->
        <div id="crosshair">
            <div class="crosshair-line horizontal"></div>
            <div class="crosshair-line vertical"></div>
        </div>

        <!-- Inventory Quick Access -->
        <div id="quick-inventory">
            <div class="slot" id="slot-1">1</div>
            <div class="slot" id="slot-2">2</div>
            <div class="slot" id="slot-3">3</div>
            <div class="slot" id="slot-4">4</div>
        </div>
    </div>
</body>
</rml>
```

```
/* ui/hud.css */
body {
    width: 100%;
    height: 100%;
    background-color: transparent;
}

#hud-container {
    width: 100%;
    height: 100%;
}

#health-section {
    position: absolute;
    left: 20px;
    bottom: 20px;
    display: flex;
    align-items: center;
}

#health-bar-bg {
    width: 200px;
    height: 24px;
    background-color: rgba(0, 0, 0, 0.5);
    border-radius: 4px;
    overflow: hidden;
}

#health-bar {
    height: 100%;
    background: linear-gradient(to right, #ff4444, #44ff44);
    transition: width 0.3s ease-out;
}

#health-value {
    display: block;
    text-align: center;
    color: white;
    font-size: 16px;
    font-weight: bold;
    line-height: 24px;
}

#crosshair {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 20px;
    height: 20px;
}

.crosshair-line {
    position: absolute;
    background-color: rgba(255, 255, 255, 0.8);
}

.crosshair-line.horizontal {
    width: 20px;
    height: 2px;
    top: 50%;
    left: 0;
    transform: translateY(-50%);
}

.crosshair-line.vertical {
    width: 2px;
    height: 20px;
    left: 50%;
    top: 0;
    transform: translateX(-50%);
}

#quick-inventory {
    position: absolute;
    bottom: 20px;
    left: 50%;
    transform: translateX(-50%);
    display: flex;
    gap: 8px;
}

.slot {
    width: 48px;
    height: 48px;
    background-color: rgba(0, 0, 0, 0.5);
    border: 2px solid rgba(255, 255, 255, 0.3);
    border-radius: 4px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: white;
    font-size: 14px;
}

.slot.selected {
    border-color: #44ff44;
    background-color: rgba(68, 255, 68, 0.2);
}
```

---

## Сравнение подходов

| Критерий                 | ImGui           | Custom Mesh | RmlUi           |
|--------------------------|-----------------|-------------|-----------------|
| **Сложность интеграции** | Низкая          | Средняя     | Высокая         |
| **Производительность**   | Средняя         | Высокая     | Средняя         |
| **Анимации**             | Ограничены      | Кастомные   | CSS transitions |
| **Локализация**          | Вручную         | Вручную     | Data attributes |
| **Дизайн**               | Developer-style | Кастомный   | CSS styling     |
| **Лучше всего для**      | Debug UI        | Простой HUD | Сложный HUD     |

---

## DOD/Zero-Cost обоснование

### Почему не Chromium/CEF?

Многие современные игры используют CEF (Chromium Embedded Framework) для UI:

| Аспект                  | Chromium/CEF                   | RmlUi                    |
|-------------------------|--------------------------------|--------------------------|
| **Память**              | 100-500 MB                     | 5-20 MB                  |
| **CPU overhead**        | Высокий (отдельный процесс)    | Минимальный (in-process) |
| **Интеграция**          | Сложная (IPC, texture sharing) | Простая (native C++)     |
| **Размер дистрибутива** | +50-100 MB                     | +2-5 MB                  |
| **Производительность**  | Зависит от web-контента        | Детерминированная        |

**Вывод**: CEF приемлем для AAA-игр с большими бюджетами, но для инди-проекта RmlUi — более разумный выбор.

### RmlUi как native C++ решение

RmlUi — **нативная C++ библиотека**, не требующая внешних зависимостей:

```cpp
// RmlUi не требует:
// - Отдельного процесса
// - IPC коммуникации
// - Браузерного движка
// - JavaScript runtime (опционально)

// RmlUi предоставляет:
// - HTML/CSS синтаксис (знакомый дизайнерам)
// - Нативную интеграцию с Vulkan
// - Полный контроль над памятью
// - DOD-friendly архитектуру
```

### Zero-Cost абстракции в RmlUi

```cpp
// Пример: Zero-cost transformation для UI элементов
class RmlUiDODBridge {
public:
    // SoA布局 для UI элементов (cache-friendly)
    struct UIElementsSoA {
        std::vector<glm::vec2> positions;
        std::vector<glm::vec2> sizes;
        std::vector<glm::vec4> colors;
        std::vector<uint32_t> textureIndices;
        std::vector<uint32_t> sortKeys;  // Z-order + material
    };

    // Batch rendering без overhead
    void renderBatch(VkCommandBuffer cmd, const UIElementsSoA& elements) {
        // Сортировка по material/texture (один раз на кадр)
        sortElements(elements);

        // Единственный draw call для всех элементов с одной текстурой
        for (auto& batch : batches_) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   layout_, 0, 1, &batch.descriptorSet, 0, nullptr);
            vkCmdDrawIndexed(cmd, batch.indexCount, 1, batch.firstIndex, 0, 0);
        }
    }

private:
    std::vector<UIBatch> batches_;
};
```

### Memory footprint сравнение

```
ImGui (Debug UI):
- Базовый overhead: ~2 MB
- На кадр: ~100 KB (в зависимости от элементов)
- Общий для типичного debug UI: ~5-10 MB

RmlUi (Game HUD):
- Базовый overhead: ~5 MB
- На кадр: ~50 KB (retained mode)
- Общий для типичного HUD: ~10-20 MB

Chromium/CEF:
- Базовый overhead: ~100 MB
- На кадр: переменный (GC, reflow)
- Общий: ~150-500 MB
```

---

## Рекомендация для ProjectV

### Фаза 1: MVP (Курсовой проект)

```
Debug UI: ImGui (всегда)
Game HUD: ImGui (временно, для простоты)
```

**Обоснование**: MVP требует минимальных затрат времени. ImGui достаточен для демонстрации.

### Фаза 2: Production (Post-MVP)

```
Debug UI: ImGui (только debug builds)
Game HUD: RmlUi (полноценный HUD с инвентарём, меню)
```

**Обоснование**: RmlUi требует времени на интеграцию, но даёт профессиональный вид.

---

## UI Pipeline

### Atlas Generation

Для оптимизации UI текстуры объединяются в atlas:

```
ui/
├── textures/
│   ├── health_icon.png
│   ├── stamina_icon.png
│   └── crosshair.png
│
        ↓ texture-atlas-generator

compiled/ui/
└── ui_atlas.tex       # Все UI текстуры в одном файле
```

### Font Loading

```cpp
class FontManager {
public:
    void loadFont(const std::string& name, const std::string& path, float size) {
        // Загрузка через stb_truetype или FreeType
        // Генерация SDF (Signed Distance Field) для качественного рендеринга
        // Кэширование glyph atlas
    }

    void renderText(VkCommandBuffer cmd, const std::string& text,
                   const std::string& fontName, const glm::vec2& position,
                   const glm::vec4& color) {
        // Поиск glyph-ов в кэше
        // Генерация quads для каждого символа
        // Batch rendering
    }

private:
    struct Font {
        std::vector<Glyph> glyphs;
        VkImageView atlasView;
        float lineHeight;
    };

    std::unordered_map<std::string, Font> fonts_;
};
```

---

## Интеграция с ECS

```cpp
// Компонент для UI элементов
struct UIElement {
    std::string elementId;
    bool visible = true;
    int zIndex = 0;
};

// Система обновления HUD
void HUDUpdateSystem(flecs::iter& it) {
    auto* players = it.field<PlayerState>(0);
    auto* huds = it.field<GameHUD>(1);

    for (auto i : it) {
        huds[i].update(it.delta_time(), players[i]);
    }
}

// Система рендеринга HUD (после основного рендеринга)
void HUDRenderSystem(flecs::iter& it) {
    auto* huds = it.field<GameHUD>(0);
    auto cmd = static_cast<VkCommandBuffer>(it.ctx);

    for (auto i : it) {
        huds[i].render(cmd);
    }
}
```

---

## Резюме

**Принцип:** Разделяйте Debug UI и Game HUD. Используйте правильный инструмент для каждой задачи.

**Debug UI:** ImGui — быстро, удобно, для разработчиков.

**Game HUD:**

- Простой: Custom Mesh (максимальный контроль, минимальный overhead)
- Сложный: RmlUi (HTML/CSS, анимации, локализация)

**Выгода:** Чистое разделение, правильные инструменты, профессиональный вид.
