# Производительность RmlUi

🟡 **Уровень 2: Средний**

Оптимизация производительности RmlUi: retained mode, geometry caching, texture management.

## Retained Mode vs Immediate Mode

RmlUi использует retained mode архитектуру:

| Аспект            | Retained Mode (RmlUi)      | Immediate Mode (ImGui)    |
|-------------------|----------------------------|---------------------------|
| Состояние         | Сохраняется между кадрами  | Пересоздаётся каждый кадр |
| Обновление        | Только изменённые элементы | Все элементы              |
| Память            | Больше (хранит состояние)  | Меньше                    |
| Обновление данных | Dirty flags                | Автоматически             |

```cpp
// RmlUi: состояние сохраняется
element->SetProperty("width", "200px");  // Применяется один раз

// Обновление только при изменении
if (healthChanged) {
    element->SetProperty("width",
        Rml::String(std::to_string(health) + "px"));
}
```

## Geometry Caching

RmlUi кеширует геометрию для неизменённых элементов.

### Compiled Geometry

```cpp
// RenderInterface: опциональная поддержка compiled geometry
class OptimizedRenderInterface : public Rml::RenderInterface {
public:
    // Компиляция геометрии в GPU buffers
    CompiledGeometryHandle CompileGeometry(
        Rml::Vertex* vertices,
        int numVertices,
        int* indices,
        int numIndices
    ) override {
        // Создание VBO/IBO один раз
        VkBuffer vertexBuffer, indexBuffer;
        // ... allocation

        return reinterpret_cast<CompiledGeometryHandle>(new Geometry{vertexBuffer, indexBuffer, numIndices});
    }

    // Рендеринг скомпилированной геометрии
    void RenderCompiledGeometry(
        CompiledGeometryHandle geometry,
        const Rml::Vector2f& translation
    ) override {
        Geometry* g = reinterpret_cast<Geometry*>(geometry);

        // Просто bind и draw - без загрузки данных каждый кадр
        vkCmdBindVertexBuffers(cmd, 0, 1, &g->vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, g->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, g->indexCount, 1, 0, 0, 0);
    }

    // Освобождение
    void ReleaseCompiledGeometry(CompiledGeometryHandle geometry) override {
        Geometry* g = reinterpret_cast<Geometry*>(geometry);
        vmaDestroyBuffer(allocator, g->vertexBuffer, nullptr);
        vmaDestroyBuffer(allocator, g->indexBuffer, nullptr);
        delete g;
    }
};
```

### Когда геометрия перекомпилируется

| Событие                    | Перекомпиляция      |
|----------------------------|---------------------|
| Изменение текста           | Да (элемент)        |
| Изменение размера          | Да (элемент + дети) |
| Hover эффект (только цвет) | Нет                 |
| Анимация opacity           | Нет                 |
| Scroll                     | Частично            |

## Texture Management

### Атлас текстур

Объединение мелких текстур в один атлас:

```cpp
// Вместо множества мелких текстур
// ui/icons/health.png      (32x32)
// ui/icons/stamina.png     (32x32)
// ui/icons/mana.png        (32x32)

// Создайте атлас
// ui/atlas.png             (256x256) со всеми иконками
```

```css
/* Использование через sprite sheet */
@sprite_sheet icons {
    src: ui/atlas.png;
    health:   0px 0px 32px 32px;
    stamina: 32px 0px 32px 32px;
    mana:    64px 0px 32px 32px;
}

.health-icon { decorator: image(icons:health); }
```

### Освобождение неиспользуемых текстур

```cpp
// Освободить все текстуры
Rml::ReleaseTextures();

// Освободить конкретную текстуру
Rml::ReleaseTexture("old_ui/background.png");

// Освободить шрифты (полная перезагрузка)
Rml::ReleaseFontResources();
```

## Batch Rendering

Группировка вызовов отрисовки.

### Сортировка по текстуре

```cpp
class BatchRenderInterface : public Rml::RenderInterface {
    struct DrawCall {
        Rml::Vertex* vertices;
        int* indices;
        int numIndices;
        Rml::TextureHandle texture;
        Rml::Vector2f translation;
    };

    std::vector<DrawCall> drawCalls;

    void RenderGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation
    ) override {
        // Накапливаем draw calls
        drawCalls.push_back({vertices, indices, numIndices, texture, translation});
    }

    void Flush() {
        // Сортировка по текстуре
        std::sort(drawCalls.begin(), drawCalls.end(),
            [](const DrawCall& a, const DrawCall& b) {
                return a.texture < b.texture;
            });

        // Группировка и отрисовка
        Rml::TextureHandle currentTexture = 0;
        for (auto& call : drawCalls) {
            if (call.texture != currentTexture) {
                currentTexture = call.texture;
                bindTexture(currentTexture);
            }
            drawVertices(call.vertices, call.indices, call.numIndices);
        }

        drawCalls.clear();
    }
};
```

## Context Update Optimization

### Минимизация обновлений

```cpp
// ПРАВИЛЬНО: обновлять только при изменении данных
void updateHealthUI(int newHealth) {
    if (newHealth != lastHealth_) {
        healthElement_->SetInnerRML(std::to_string(newHealth));
        lastHealth_ = newHealth;
    }
}

// НЕЭФФЕКТИВНО: обновлять каждый кадр
void updateUI() {
    healthElement_->SetInnerRML(std::to_string(player.health));  // Каждый кадр!
}
```

### Data Bindings

Data bindings автоматически отслеживают изменения:

```cpp
// Только при изменении player.health - UI обновляется
model.Bind("health", &player.health);

// Ручное уведомление при сложных изменениях
model.DirtyVariable("health");
```

### Отключение обновлений для скрытых документов

```cpp
// Скрытый документ не обновляется
document->Hide();

// Или удалить из контекста
context->UnloadDocument(hiddenDocument);
```

## Scroll Optimization

### Виртуализация длинных списков

```cpp
// Для списков с тысячами элементов
// Показывать только видимые элементы
class VirtualList {
    int visibleStart = 0;
    int visibleCount = 20;  // Видимых элементов
    int itemHeight = 30;

    void onScroll(float scrollOffset) {
        int newStart = static_cast<int>(scrollOffset / itemHeight);
        if (newStart != visibleStart) {
            visibleStart = newStart;
            rebuildVisibleItems();
        }
    }

    void rebuildVisibleItems() {
        containerElement->SetInnerRML("");
        for (int i = visibleStart; i < visibleStart + visibleCount; ++i) {
            if (i < items.size()) {
                // Создать элемент только для видимых
                addItemElement(items[i]);
            }
        }
    }
};
```

## Font Performance

### Загрузка шрифтов

```cpp
// Загружать только нужные начертания
Rml::LoadFontFace("fonts/Roboto-Regular.ttf");
Rml::LoadFontFace("fonts/Roboto-Bold.ttf");
// Не загружать все 10+ начертаний Roboto

// Fallback шрифт для эмодзи и спецсимволов
Rml::LoadFontFace("fonts/NotoEmoji-Regular.ttf", true);
```

### Кеширование glyph atlas

RmlUi автоматически кеширует отрендеренные глифы. При большом количестве символов:

```cpp
// После загрузки уровня/меню можно освободить старые шрифты
Rml::ReleaseFontResources();  // Полная перезагрузка
```

## Memory Management

### Мониторинг памяти

```cpp
// В SystemInterface
size_t GetMemoryUsage() {
    // Вернуть используемую память
}
```

### Освобождение при переключении сцен

```cpp
void switchToMainMenu() {
    // 1. Скрыть/выгрузить игровые документы
    gameHUD->Hide();
    context->UnloadDocument(gameHUD);

    // 2. Освободить ресурсы
    Rml::ReleaseTextures();          // Текстуры игры
    Rml::ReleaseCompiledGeometry();  // Геометрия

    // 3. Загрузить меню
    mainMenu = context->LoadDocument("ui/main_menu.rml");
    mainMenu->Show();
}
```

## Профилирование

### Tracy Integration

```cpp
#include <tracy/Tracy.hpp>

void updateUI() {
    ZoneScopedN("RmlUi Update");
    context->Update();
}

void renderUI() {
    ZoneScopedN("RmlUi Render");
    context->Render();
}
```

### Встроенный profiling API

```cpp
#include <RmlUi/Core/Profiling.h>

// Включить profiling
Rml::SetProfilingEnabled(true);

// Получить статистику
auto stats = Rml::GetProfilingStats();
// stats.updateTime, stats.renderTime, stats.elementCount
```

## Рекомендации

| Ситуация                 | Решение                           |
|--------------------------|-----------------------------------|
| Частые обновления текста | Data bindings вместо SetInnerRML  |
| Много мелких текстур     | Sprite sheet / атлас              |
| Длинные списки           | Виртуализация                     |
| Анимации                 | CSS transitions (GPU accelerated) |
| Смена сцены              | ReleaseTextures + UnloadDocument  |
| Debug                    | Rml::Debugger для инспекции       |

## Типичные bottlenecks

1. **Частые SetInnerRML** — использовать data bindings
2. **Много текстур** — атласизация
3. **Сложные селекторы RCSS** — упростить, использовать классы
4. **Отсутствие compiled geometry** — реализовать в RenderInterface
5. **Обновление скрытых элементов** — скрывать/выгружать документы
