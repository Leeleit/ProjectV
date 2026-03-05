# Инструменты RmlUi

🟡 **Уровень 2: Средний**

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
