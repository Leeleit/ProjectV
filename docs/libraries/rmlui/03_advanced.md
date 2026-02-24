## Инструменты RmlUi

<!-- anchor: 05_tools -->


Встроенные инструменты RmlUi: Debugger, Lua plugin, Lottie, SVG.

## RmlUi Debugger

Визуальный отладчик для инспекции документов в реальном времени.

### Активация

```cpp
#include <RmlUi/Debugger.h>

// Инициализация (после создания контекста)
Rml::Debugger::Initialise(context);

// Показать/скрыть отладчик
Rml::Debugger::SetVisible(true);

// Проверка видимости
bool isVisible = Rml::Debugger::IsVisible();
```

### Возможности

| Функция               | Описание                                       |
|-----------------------|------------------------------------------------|
| **Element Inspector** | Просмотр DOM дерева, свойств, атрибутов        |
| **Style Editor**      | Редактирование RCSS свойств в реальном времени |
| **Info Overlay**      | FPS, размер контекста, количество элементов    |
| **Log Console**       | Вывод логов RmlUi                              |
| **Visual Debug**      | Подсветка регионов, границ элементов           |

### Горячие клавиши

| Клавиша        | Действие                 |
|----------------|--------------------------|
| `F8`           | Показать/скрыть отладчик |
| `Ctrl+Shift+I` | Element Inspector        |
| `Ctrl+Shift+S` | Style Editor             |
| `Ctrl+Shift+L` | Log Console              |

### Программное управление

```cpp
// Установка контекста для отладки
Rml::Debugger::SetContext(context);

// Включение info overlay
Rml::Debugger::SetInfoVisible(true);

// Выбор элемента для инспекции
Rml::Debugger::SetHoverElement(element);
```

## Lua Plugin

Скриптование UI на Lua.

### Подключение

```cmake
set(RMLUI_LUA ON CACHE BOOL "")
```

```cpp
#include <RmlUi/Lua.h>

// Инициализация Lua plugin
Rml::Lua::Initialise();

// Загрузка Lua скрипта
Rml::Lua::LoadFile("scripts/ui_logic.lua");
```

### Пример Lua скрипта

```lua
-- Доступ к элементам
local document = context:LoadDocument("ui/menu.rml")
local button = document:GetElementById("start_button")

-- Обработка событий
button:AddEventListener("click", function(event)
    print("Button clicked!")
    document:Hide()
end)

-- Изменение свойств
button:SetProperty("background-color", "#4a9eff")

-- Data bindings
local player = {
    health = 100,
    name = "Player"
}

local model = context:CreateDataModel("player")
model:Bind("health", player, "health")
model:Bind("name", player, "name")
```

### API Lua

| Модуль           | Описание                             |
|------------------|--------------------------------------|
| `Rml`            | Core API: Context, Element, Document |
| `Rml.Math`       | Vector2, Vector3, Matrix4            |
| `Rml.Animations` | Transitions, keyframe animations     |

## SVG Plugin

Отображение SVG изображений.

### Подключение

```cmake
set(RMLUI_SVG ON CACHE BOOL "")
# Требует lunasvg
```

```cpp
#include <RmlUi/SVG.h>

// Инициализация плагина (автоматически при включении)
```

### Использование в RML

```html
<img src="icons/logo.svg"/>

<!-- Или через decorator -->
<div style="decorator: image(icon.svg); width: 100px; height: 100px;"></div>
```

### Особенности

- Поддержка SVG 1.1
- Рендеринг через lunasvg
- Масштабирование без потери качества
- Поддержка анимированных SVG

## Lottie Plugin

Векторные анимации Lottie (JSON).

### Подключение

```cmake
set(RMLUI_LOTTIE ON CACHE BOOL "")
# Требует rlottie
```

```cpp
#include <RmlUi/Lottie.h>

// Инициализация плагина (автоматически при включении)
```

### Использование в RML

```html
<lottie src="animations/loading.json"
        style="width: 200px; height: 200px;"
        loop="true"
        autoplay="true"/>
```

### Управление анимацией через C++

```cpp
// Получение элемента Lottie
Rml::Element* lottieElement = document->GetElementById("my_animation");

// Управление воспроизведением
lottieElement->SetAttribute("playing", "true");
lottieElement->SetAttribute("loop", "true");

// Остановка
lottieElement->SetAttribute("playing", "false");

// Переход к кадру
lottieElement->SetAttribute("frame", "30");
```

### Поддерживаемые функции Lottie

| Функция     | Поддержка |
|-------------|-----------|
| Shapes      | Да        |
| Transforms  | Да        |
| Gradients   | Да        |
| Masks       | Частично  |
| Expressions | Нет       |

## HarfBuzz Integration

Продвинутый text shaping для сложных скриптов.

### Подключение

```cmake
# HarfBuzz определяется автоматически при наличии
find_package(HarfBuzz REQUIRED)
```

### Поддержка скриптов

| Скрипт           | Особенности                 |
|------------------|-----------------------------|
| Arabic           | Лигатуры, контекстные формы |
| Hebrew           | Направление RTL             |
| Hindi/Devanagari | Лигатуры, matras            |
| Thai             | Повторяющиеся гласные       |

### Пример использования

```cpp
// Загрузка шрифта с поддержкой арабского
Rml::LoadFontFace("fonts/NotoSansArabic-Regular.ttf", true);

// RML с арабским текстом
// <p>مرحبا بالعالم</p>
```

## Font Engine Customization

Замена FreeType на кастомный font engine.

```cpp
class CustomFontEngine : public Rml::FontEngineInterface {
public:
    FontFaceHandle LoadFontFace(
        const String& family,
        Style::FontStyle style,
        Style::FontWeight weight,
        Span<const byte> data
    ) override {
        // Парсинг и подготовка шрифта
    }

    FontFaceHandle GetFontFaceHandle(
        const String& family,
        Style::FontStyle style,
        Style::FontWeight weight,
        int size
    ) override {
        // Возврат handle для конкретного размера
    }

    int GetSize(FontFaceHandle handle) override {
        // Размер шрифта
    }

    int GetLineHeight(FontFaceHandle handle) override {
        // Высота строки
    }

    int GetBaseline(FontFaceHandle handle) override {
        // Базовая линия
    }

    float GetUnderline(FontFaceHandle handle, float& thickness) override {
        // Позиция подчёркивания
    }

    int GetStringWidth(
        FontFaceHandle handle,
        const String& string,
        float letter_spacing
    ) override {
        // Ширина строки в пикселях
    }

    int GenerateString(
        FontFaceHandle handle,
        const String& string,
        const Vector2f& position,
        float letter_spacing,
        Geometry& geometry
    ) override {
        // Генерация геометрии для текста
    }

    FontMetrics GetFontMetrics(FontFaceHandle handle) override {
        // Метрики шрифта
    }
};

// Установка до Rml::Initialise()
Rml::SetFontEngineInterface(new CustomFontEngine());
```

## Примеры в репозитории

RmlUi включает примеры в папке `Samples/`:

| Пример                | Описание                       |
|-----------------------|--------------------------------|
| `basic/load_document` | Минимальная загрузка документа |
| `basic/animation`     | CSS анимации и transitions     |
| `basic/data_binding`  | Data bindings с C++ данными    |
| `basic/drag`          | Drag & drop элементы           |
| `basic/effects`       | Фильтры, blur, drop-shadow     |
| `basic/lottie`        | Lottie анимации                |
| `basic/svg`           | SVG изображения                |
| `basic/tree_view`     | Иерархический список           |
| `basic/transform`     | 2D/3D трансформации            |
| `invaders`            | Полный пример игры             |
| `lua_invaders`        | Игра с Lua логикой             |

---

## Производительность RmlUi

<!-- anchor: 06_performance -->


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

```
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

---

## Устранение неполадок RmlUi

<!-- anchor: 07_troubleshooting -->


Распространённые проблемы и их решения.

## Проблемы инициализации

### Rml::Initialise() возвращает false

**Причины:**

1. **Интерфейсы уничтожены до Shutdown**

```cpp
// ОШИБКА
void init() {
    MyRenderInterface renderer;  // Локальная переменная!
    Rml::SetRenderInterface(&renderer);
    Rml::Initialise();
}  // renderer уничтожается здесь, но RmlUi его использует

// ПРАВИЛЬНО
class UIManager {
    MyRenderInterface renderer;  // Член класса
public:
    void init() {
        Rml::SetRenderInterface(&renderer);
        Rml::Initialise();
    }
    ~UIManager() {
        Rml::Shutdown();  // renderer ещё жив
    }
};
```

2. **Несовместимая версия FreeType**

```cpp
// Проверить версию FreeType
#include <ft2build.h>
#include FT_FREETYPE_H
// Должна быть >= 2.7
```

### CreateContext возвращает nullptr

**Причины:**

1. **Контекст с таким именем уже существует**

```cpp
// Проверить существование
if (Rml::GetContext("main")) {
    Rml::RemoveContext("main");
}
Rml::Context* context = Rml::CreateContext("main", {1920, 1080});
```

2. **Нулевые размеры**

```cpp
// ОШИБКА
Rml::CreateContext("main", {0, 0});  // nullptr

// ПРАВИЛЬНО
Rml::CreateContext("main", {1920, 1080});
```

## Проблемы с документами

### Документ не загружается

**Отладка:**

```cpp
// Включить логирование
class DebugSystemInterface : public Rml::SystemInterface {
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        std::fprintf(stderr, "[RmlUi] %s\n", message.c_str());
        return true;  // Не подавлять
    }
};
```

**Частые причины:**

1. **Неверный путь к файлу**

```cpp
// Проверить через FileInterface
Rml::FileHandle file = Rml::GetFileInterface()->Open("ui/menu.rml");
if (!file) {
    // Файл не найден
}
Rml::GetFileInterface()->Close(file);
```

2. **Незагруженные шрифты**

```cpp
// Шрифты должны быть загружены ДО показа документа
Rml::LoadFontFace("fonts/Roboto-Regular.ttf");  // Обязательно!
document->Show();
```

3. **Ошибки синтаксиса RML**

```cpp
// Использовать отладчик
Rml::Debugger::Initialise(context);
Rml::Debugger::SetVisible(true);
// F8 -> посмотреть Log Console
```

### Элементы не отображаются

**Причины:**

1. **Нулевой размер элемента**

```css
/* Проблема: width/height не заданы */
.invisible-element {
    /* width: auto (0 для пустого элемента) */
}

/* Решение */
.visible-element {
    width: 200px;
    height: 100px;
}
```

2. **display: none**

```css
/* Проверить computed style */
#my-element {
    display: block;  /* Не none */
}
```

3. **Позиционирование за пределами экрана**

```css
/* Проверить position и координаты */
#my-element {
    position: absolute;
    left: 0px;
    top: 0px;  /* Не -9999px */
}
```

4. **Прозрачный цвет**

```css
/* Проверить opacity и color */
#my-element {
    opacity: 1.0;           /* Не 0 */
    color: white;           /* Не transparent */
    background-color: #fff; /* Не transparent */
}
```

## Проблемы со стилями

### RCSS не применяется

**Причины:**

1. **Неверный путь к RCSS**

```html
<!-- Проверить href -->
<link type="text/rcss" href="styles/main.rcss"/>
```

2. **Ошибки синтаксиса RCSS**

```
/* ОШИБКА: пропущена точка с запятой */
.element {
    width: 200px
    height: 100px;
}

/* ПРАВИЛЬНО */
.element {
    width: 200px;
    height: 100px;
}
```

3. **Специфичность селекторов**

```css
/* Низкая специфичность */
.element { color: white; }

/* Выигрывает этот селектор */
body .container .element { color: red; }
```

### Hover/Active не работают

```css
/* Порядок важен! */
.button { background: blue; }
.button:hover { background: lightblue; }
.button:active { background: darkblue; }

/* ОШИБКА: hover после active перезапишет active */
.button { background: blue; }
.button:active { background: darkblue; }
.button:hover { background: lightblue; }  /* Перекрывает active! */
```

## Проблемы с событиями

### Клик не обрабатывается

**Причины:**

1. **pointer-events: none**

```css
/* Элемент не получает события */
.non-clickable {
    pointer-events: none;
}
```

2. **Другой элемент перекрывает**

```css
/* Проверить z-index */
.overlay {
    position: absolute;
    z-index: 100;  /* Перекрывает элементы ниже */
}
```

3. **Событие не передаётся в контекст**

```cpp
// Убедиться, что ProcessMouseButtonDown вызывается
context->ProcessMouseButtonDown(0);  // 0 = left button
```

### EventListener не вызывается

**Причины:**

1. **Listener уничтожен**

```cpp
// ОШИБКА: локальный listener
void setupButton() {
    ClickListener listener;  // Уничтожается при выходе!
    button->AddEventListener("click", &listener);
}

// ПРАВИЛЬНО: долгоживущий listener
class UIManager {
    ClickListener clickListener;
public:
    void setupButton() {
        button->AddEventListener("click", &clickListener);
    }
};
```

2. **Неверное имя события**

```cpp
// Правильные имена событий
button->AddEventListener("click", &listener);
button->AddEventListener("mousedown", &listener);
button->AddEventListener("mouseup", &listener);
button->AddEventListener("mouseover", &listener);
button->AddEventListener("mouseout", &listener);

// Не "onclick"!
```

## Проблемы с Data Bindings

### Переменная не обновляется в UI

**Причины:**

1. **Модель не привязана к документу**

```html
<body data-model="player">
    <!-- Элементы внутри смогут использовать переменные модели -->
</body>
```

2. **Изменение через указатель**

```cpp
// После изменения данных уведомить модель
player.health = 50;
model.DirtyVariable("health");
```

3. **Структура не зарегистрирована**

```cpp
// Для сложных типов нужна регистрация
if (auto itemModel = model.RegisterStruct<Item>()) {
    itemModel.RegisterMember("name", &Item::name);
}
```

### Data Model не создаётся

```cpp
auto model = context->CreateDataModel("player");
if (!model) {
    // Модель с таким именем уже существует
    // Или контекст nullptr
}
```

## Проблемы рендеринга

### Артефакты, мерцание

**Причины:**

1. **Неверный scissor rectangle**

```cpp
// Отключить scissor после рендеринга RmlUi
context->Render();
glDisable(GL_SCISSOR_TEST);  // OpenGL
// или vkCmdSetScissor(cmd, 0, 1, &fullScreenScissor);  // Vulkan
```

2. **Неверный viewport**

```cpp
// Восстановить viewport после RmlUi
int viewport[4];
glGetIntegerv(GL_VIEWPORT, viewport);
context->Render();
glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
```

### Текст размытый

**Причины:**

1. **Несоответствие DPI**

```cpp
// Установить правильный dp ratio
float dpi = getMonitorDPI();
float dpRatio = dpi / 96.0f;  // 96 DPI = базовый
context->SetDensityIndependentPixelRatio(dpRatio);
```

2. **Неверное позиционирование пикселей**

```cpp
// Элементы должны быть на целых пикселях
element->SetProperty("left", "100px");  // Не 100.5px
```

### Текстуры не отображаются

**Причины:**

1. **Неверный формат изображения**

```cpp
// RmlUi поддерживает только TGA из коробки
// Для PNG/JPEG расширить RenderInterface::LoadTexture
```

2. **Текстура не загрузилась**

```cpp
bool success = Rml::GetRenderInterface()->LoadTexture(
    handle, dimensions, "texture.png"
);
if (!success) {
    // Текстура не загружена
}
```

## Проблемы с памятью

### Утечки памяти

**Проверка:**

```cpp
// После Shutdown все ресурсы должны быть освобождены
Rml::Shutdown();

// Если есть утечки, проверьте:
// 1. EventListener не удалён
// 2. Compiled geometry не освобождена
// 3. Текстуры не освобождены
```

### Высокое потребление памяти

**Решения:**

```cpp
// Освободить неиспользуемые ресурсы
Rml::ReleaseTextures();
Rml::ReleaseFontResources();

// Выгрузить неиспользуемые документы
context->UnloadDocument(oldDocument);
```

## Отладка

### Включить логирование

```cpp
class VerboseSystemInterface : public Rml::SystemInterface {
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        const char* prefix = "";
        switch (type) {
            case Rml::Log::LT_ERROR:   prefix = "ERROR"; break;
            case Rml::Log::LT_WARNING: prefix = "WARN"; break;
            case Rml::Log::LT_INFO:    prefix = "INFO"; break;
            case Rml::Log::LT_DEBUG:   prefix = "DEBUG"; break;
        }
        std::fprintf(stderr, "[RmlUi %s] %s\n", prefix, message.c_str());
        return true;
    }
};
```

### Использовать Debugger

```cpp
Rml::Debugger::Initialise(context);
Rml::Debugger::SetVisible(true);

// F8 - toggle debugger
// Ctrl+Shift+I - Element Inspector
// Ctrl+Shift+S - Style Editor
```

### Проверить состояние элемента

```cpp
void debugElement(Rml::Element* element) {
    std::printf("Element: %s\n", element->GetTagName().c_str());
    std::printf("  ID: %s\n", element->GetId().c_str());
    std::printf("  Classes: %s\n", element->GetClassNames().c_str());
    std::printf("  Visible: %s\n", element->IsVisible() ? "yes" : "no");
    std::printf("  Size: %.0fx%.0f\n",
        element->GetBoxSize().x, element->GetBoxSize().y);
}
```

## Коды ошибок

| Сообщение                  | Причина                       | Решение                           |
|----------------------------|-------------------------------|-----------------------------------|
| `Failed to load font face` | Файл шрифта не найден         | Проверить путь                    |
| `No font family`           | Шрифт не загружен             | LoadFontFace перед использованием |
| `Failed to load document`  | Файл RML не найден            | Проверить путь                    |
| `Invalid texture handle`   | Текстура не создана           | Проверить GenerateTexture         |
| `Context not found`        | Контекст удалён или не создан | Проверить CreateContext           |
| `Data model not found`     | Модель не создана             | CreateDataModel перед привязкой   |
