## Обзор RmlUi

<!-- anchor: 00_overview -->


**RmlUi** — библиотека пользовательского интерфейса для C++ на базе HTML/CSS стандартов. Использует retained mode
архитектуру: состояние UI сохраняется между кадрами. Предназначена для игровых HUD, меню и приложений, требующих
профессионального вида интерфейса.

Версия: **6.0+** (см. Core.h)
Исходники: [mikke89/RmlUi](https://github.com/mikke89/RmlUi)
Документация: [mikke89.github.io/RmlUiDoc](https://mikke89.github.io/RmlUiDoc/)

## Основные возможности

- **HTML/CSS синтаксис** — RML (RmlUi Markup Language) и RCSS (RmlUi Cascading Style Sheets)
- **Retained mode** — состояние сохраняется, не пересоздаётся каждый кадр
- **Data bindings** — синхронизация данных приложения с UI (MVC)
- **Анимации** — CSS transitions, transforms, keyframe animations
- **Flexbox** — современная система компоновки
- **Decorators** — кастомные эффекты и стилизация элементов
- **Локализация** — встроенная поддержка перевода текста
- **Плагины** — Lua scripting, Lottie animations, SVG images

## Типичное применение

- Игровой HUD (health bar, inventory, minimap)
- Главные меню и меню настроек
- Dialog boxes и tooltips
- Внутриигровые консоли
- GUI приложения (редакторы, инструменты)

## Архитектура: Interfaces + Backends

RmlUi разделён на ядро и абстрактные интерфейсы:

| Интерфейс               | Назначение                                 | Обязательность                     |
|-------------------------|--------------------------------------------|------------------------------------|
| **RenderInterface**     | Отрисовка геометрии, управление текстурами | Обязателен                         |
| **SystemInterface**     | Время, перевод, clipboard                  | Опционален                         |
| **FileInterface**       | Загрузка файлов (RML, RCSS, шрифты)        | Опционален                         |
| **FontEngineInterface** | Рендеринг шрифтов                          | Опционален (FreeType по умолчанию) |

Backends комбинируют Platform и Renderer:

| Platform  | Renderer | Backend name |
|-----------|----------|--------------|
| SDL3/SDL2 | Vulkan   | `SDL_VK`     |
| SDL3/SDL2 | OpenGL 3 | `SDL_GL3`    |
| SDL3/SDL2 | SDL_GPU  | `SDL_GPU`    |
| GLFW      | Vulkan   | `GLFW_VK`    |
| GLFW      | OpenGL 3 | `GLFW_GL3`   |
| Win32     | Vulkan   | `Win32_VK`   |

## Карта заголовков

| Заголовок                        | Содержимое                               |
|----------------------------------|------------------------------------------|
| `<RmlUi/Core.h>`                 | Точка входа: Initialise, CreateContext   |
| `<RmlUi/Core/Context.h>`         | Контекст: LoadDocument, Update, Render   |
| `<RmlUi/Core/Element.h>`         | DOM элемент: GetElementById, SetProperty |
| `<RmlUi/Core/ElementDocument.h>` | Документ: Show, Hide, PullToFront        |
| `<RmlUi/Core/RenderInterface.h>` | Абстрактный интерфейс рендерера          |
| `<RmlUi/Core/SystemInterface.h>` | Абстрактный интерфейс системы            |
| `<RmlUi/Core/DataModelHandle.h>` | Data bindings: Bind, BindEventFunc       |
| `<RmlUi/Debugger.h>`             | Визуальный отладчик                      |

## Концепции RmlUi

<!-- anchor: 02_concepts -->


Ключевые концепции RmlUi: RML/RCSS, Context, Element, Data Bindings, Decorators.

## RML (RmlUi Markup Language)

RML — язык разметки на базе XHTML. Описывает структуру документа.

```html
<rml>
<head>
    <title>Document Title</title>
    <link type="text/rcss" href="styles.rcss"/>
</head>
<body>
    <div id="container" class="panel">
        <h1>Heading</h1>
        <p>Paragraph text</p>
        <button onclick="handle_click">Click Me</button>
    </div>
</body>
</rml>
```

### Основные элементы

| Элемент       | Назначение                     |
|---------------|--------------------------------|
| `<rml>`       | Корневой элемент               |
| `<head>`      | Метаданные: title, link, style |
| `<body>`      | Тело документа                 |
| `<div>`       | Универсальный контейнер        |
| `<span>`      | Inline контейнер               |
| `<p>`         | Параграф                       |
| `<h1>`-`<h6>` | Заголовки                      |
| `<button>`    | Кнопка                         |
| `<input>`     | Поле ввода                     |
| `<select>`    | Выпадающий список              |
| `<textarea>`  | Многострочное поле ввода       |
| `<img>`       | Изображение                    |

### Атрибуты

| Атрибут   | Назначение                        |
|-----------|-----------------------------------|
| `id`      | Уникальный идентификатор элемента |
| `class`   | CSS классы (через пробел)         |
| `style`   | Inline RCSS стили                 |
| `onclick` | Обработчик клика                  |
| `data-*`  | Data attributes для bindings      |

## RCSS (RmlUi Cascading Style Sheets)

RCSS — стили на базе CSS2 с расширениями CSS3.

```css
/* Селектор по ID */
#main-menu {
    position: absolute;
    width: 400px;
    height: 300px;
    background: rgba(0, 0, 0, 0.8);
    border: 2px #444;
    border-radius: 8px;
}

/* Селектор по классу */
.button {
    display: block;
    width: 200px;
    height: 40px;
    background: #4a9eff;
    color: white;
    text-align: center;
    cursor: pointer;
}

.button:hover {
    background: #6bb3ff;
}

/* Transition */
.button {
    transition: background 0.2s ease-out;
}

/* Transform */
#rotated {
    transform: rotate(45deg) scale(1.2);
}
```

### Поддерживаемые свойства

| Категория  | Свойства                                                                        |
|------------|---------------------------------------------------------------------------------|
| Layout     | `display`, `position`, `width`, `height`, `margin`, `padding`, `float`          |
| Flexbox    | `flex-direction`, `flex-wrap`, `justify-content`, `align-items`, `flex-grow`    |
| Background | `background-color`, `background-image`                                          |
| Border     | `border`, `border-radius`                                                       |
| Text       | `font-family`, `font-size`, `font-weight`, `color`, `text-align`, `line-height` |
| Transform  | `transform`, `transform-origin`, `perspective`                                  |
| Animation  | `transition`, `animation`, `animation-duration`                                 |
| Effects    | `filter`, `opacity`                                                             |

### DP единицы

RmlUi использует `dp` (density-independent pixels) для масштабирования под DPI:

```css
.element {
    width: 200dp;    /* Масштабируется под DPI */
    font-size: 16dp; /* Масштабируется под DPI */
}
```

## Context

Context — изолированный контейнер для UI документов.

```cpp
// Создание контекста
Rml::Context* context = Rml::CreateContext(
    "main",                    // Уникальное имя
    Rml::Vector2i(1920, 1080)  // Размеры в пикселях
);

// Загрузка документа в контекст
Rml::ElementDocument* doc = context->LoadDocument("ui/menu.rml");
doc->Show();

// Обновление и рендеринг
context->Update();
context->Render();

// Обработка ввода
context->ProcessMouseMove(x, y, 0);
context->ProcessMouseButtonDown(0);

// Удаление контекста
Rml::RemoveContext("main");
```

### Множественные контексты

```cpp
// HUD контекст (всегда виден)
Rml::Context* hudContext = Rml::CreateContext("hud", {1920, 1080});

// Меню контекст (поверх HUD)
Rml::Context* menuContext = Rml::CreateContext("menu", {1920, 1080});

// Рендеринг в порядке слоёв
hudContext->Render();
menuContext->Render();
```

## Element

Element — узел DOM дерева. Базовый класс для всех элементов UI.

```cpp
// Получение элемента по ID
Rml::Element* element = document->GetElementById("health-bar");

// Изменение свойств
element->SetProperty("width", "150px");
element->SetProperty("background-color", "#ff4444");

// Изменение содержимого
element->SetInnerRML("<span>100</span>");

// Атрибуты
element->SetAttribute("data-value", "100");
Rml::String value = element->GetAttribute<Rml::String>("data-value", "");

// Классы
element->SetClass("active", true);
bool isActive = element->HasClass("active");

// События
element->AddEventListener("click", myListener);
```

### Навигация по DOM

```cpp
Rml::Element* parent = element->GetParentNode();
Rml::Element* firstChild = element->GetFirstChild();
Rml::Element* nextSibling = element->GetNextSibling();

int numChildren = element->GetNumChildren();
for (int i = 0; i < numChildren; ++i) {
    Rml::Element* child = element->GetChild(i);
}
```

## Data Bindings

Data bindings синхронизируют данные приложения с UI.

### Создание Data Model

```cpp
// Данные приложения
struct PlayerData {
    int health = 100;
    int maxHealth = 100;
    Rml::String name = "Player";
};

PlayerData playerData;

// Создание модели
Rml::DataModelConstructor model = context->CreateDataModel("player");
if (model) {
    // Привязка простых типов
    model.Bind("health", &playerData.health);
    model.Bind("max_health", &playerData.maxHealth);
    model.Bind("name", &playerData.name);
}
```

### Использование в RML

```html
<body data-model="player">
    <h1>{{name}}</h1>
    <div>Health: {{health}} / {{max_health}}</div>

    <!-- Условный рендеринг -->
    <div data-if="health < 30">Low Health Warning!</div>

    <!-- Цикл -->
    <ul>
        <li data-for="item in inventory">{{item.name}}</li>
    </ul>

    <!-- Двусторонняя привязка -->
    <input type="text" data-value="name"/>
</body>
```

### Сложные типы

```cpp
struct Item {
    Rml::String name;
    int count;
};

struct PlayerData {
    Rml::Vector<Item> inventory;
};

// Регистрация структур
if (model) {
    // Регистрация типа Item
    if (auto itemModel = model.RegisterStruct<Item>()) {
        itemModel.RegisterMember("name", &Item::name);
        itemModel.RegisterMember("count", &Item::count);
    }

    // Регистрация массива
    model.Bind("inventory", &playerData.inventory);
}
```

### События из Data Model

```cpp
// Функция обработчик
model.BindEventFunc("use_item",
    [](Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& args) {
        int itemIndex = args[0].Get<int>();
        // Обработка использования предмета
    }
);
```

```html
<button data-event-click="use_item(item_index)">Use</button>
```

## Decorators

Decorators — кастомные эффекты для стилизации элементов.

### Встроенные decorators

```css
/* Gradient decorator */
.gradient-box {
    decorator: linear-gradient(180deg, #4a9eff, #1a5eff);
}

/* Image decorator */
.image-panel {
    decorator: image(background.png);
}

/* Tiled image */
.tiled-bg {
    decorator: image(tile.png, tiled);
}

/* Nine-patch */
.nine-patch {
    decorator: nine-patch(border.png, 10px 10px 10px 10px);
}
```

### Прямоугольник с тенью

```
.card {
    decorator: complex {
        /* Фон */
        gradient: linear-gradient(180deg, #444, #222);

        /* Тень */
        box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.5);

        /* Граница */
        border: 1px #555;
        border-radius: 4px;
    };
}
```

## Templates

Templates — переиспользуемые шаблоны для окон.

```html
<!-- templates/window.rml -->
<rml>
    <template name="window">
        <div class="window">
            <div class="window-header">
                <h2>{title}</h2>
                <button class="close">×</button>
            </div>
            <div class="window-content">
                {content}
            </div>
        </div>
    </template>
</rml>
```

```html
<!-- Использование -->
<rml>
    <head>
        <template href="templates/window.rml"/>
    </head>
    <body>
        <window title="Settings">
            <p>Window content here</p>
        </window>
    </body>
</rml>
```

## Sprites

Sprites — анимированные изображения из sprite sheet.

```
/* Определение sprite sheet */
@sprite_sheet ui_sprites {
    src: sprites.png;
    resolution: 2dp;  /* High DPI */

    /* Спрайты */
    button-normal:   0px 0px 100px 40px;
    button-hover:    0px 40px 100px 40px;
    button-active:   0px 80px 100px 40px;

    /* Анимация */
    icon-loading: 0px 120px 32px 32px, 32px 120px 32px 32px, ...;
}

/* Использование */
.button {
    decorator: image(ui_sprites:button-normal);
}
.button:hover {
    decorator: image(ui_sprites:button-hover);
}

.loading {
    decorator: image(ui_sprites:icon-loading);
    animation: sprite 0.5s steps(4) infinite;
}
```

## Localization

RmlUi поддерживает локализацию через строки-заполнители.

```cpp
// Установка языка
Rml::SystemInterface* system = Rml::GetSystemInterface();
system->SetLanguage("ru");

// Реализация перевода в SystemInterface
Rml::String TranslateString(const Rml::String& key) override {
    static std::unordered_map<Rml::String, Rml::String> translations = {
        {"menu.start", "Начать игру"},
        {"menu.settings", "Настройки"},
        {"menu.quit", "Выход"}
    };
    return translations.value(key, key);
}
```

```html
<button>menu.start</button>
<button>menu.settings</button>
<button>menu.quit</button>

---

## Глоссарий RmlUi

<!-- anchor: 08_glossary -->


Термины и определения, используемые в RmlUi.

## Основные термины

| Термин | Определение |
|--------|-------------|
| **RML** | RmlUi Markup Language — язык разметки на базе XHTML для описания структуры UI |
| **RCSS** | RmlUi Cascading Style Sheets — язык стилей на базе CSS для оформления UI |
| **Context** | Изолированный контейнер для UI документов; имеет собственные размеры, DPI, очередь рендеринга |
| **Document** | Загруженный RML файл; корневой элемент типа `ElementDocument` |
| **Element** | Узел DOM дерева; базовый класс для всех UI элементов |
| **DOM** | Document Object Model — дерево элементов документа |

## Архитектура

| Термин | Определение |
|--------|-------------|
| **Retained Mode** | Режим, при котором состояние UI сохраняется между кадрами (в отличие от immediate mode) |
| **RenderInterface** | Абстрактный интерфейс для отрисовки геометрии через графический API |
| **SystemInterface** | Абстрактный интерфейс для системных функций: время, логирование, clipboard |
| **FileInterface** | Абстрактный интерфейс для загрузки файлов |
| **FontEngineInterface** | Абстрактный интерфейс для рендеринга шрифтов |
| **Backend** | Комбинация Platform + Renderer для конкретной платформы и графического API |
| **Platform** | Часть backend'а для ввода и управления окном (SDL, GLFW, Win32) |
| **Renderer** | Часть backend'а для отрисовки (Vulkan, OpenGL, SDL_GPU) |

## Элементы

| Термин | Определение |
|--------|-------------|
| **ElementDocument** | Корневой элемент документа; управляет отображением, модальностью |
| **ElementText** | Текстовый элемент; содержит строку текста |
| **ElementInstancer** | Фабрика для создания кастомных элементов |
| **Decorator** | Визуальный эффект для стилизации элемента (gradient, image, nine-patch) |
| **Filter** | Графический эффект: blur, drop-shadow, brightness |
| **Pseudo-element** | Виртуальный элемент: `::before`, `::after`, `::selection` |

## Стилизация

| Термин | Определение |
|--------|-------------|
| **Property** | RCSS свойство: `width`, `color`, `margin` и т.д. |
| **Selector** | Правило выбора элементов: `#id`, `.class`, `tag` |
| **Specificity** | Приоритет селектора при конфликте стилей |
| **Computed Style** | Итоговые значения свойств после применения всех правил |
| **Inheritance** | Наследование свойств от родителя к потомку |
| **dp** | Density-independent pixel — единица измерения, масштабируемая под DPI |

## Data Bindings

| Термин | Определение |
|--------|-------------|
| **Data Model** | Модель данных, привязанная к контексту; содержит переменные и функции |
| **Data Binding** | Связь между переменной C++ и элементом UI |
| **Dirty Variable** | Переменная, помеченная как изменённая; требует обновления UI |
| `{{variable}}` | Синтаксис подстановки значения переменной в RML |
| `data-model` | Атрибут для привязки data model к элементу |
| `data-if` | Атрибут для условного отображения элемента |
| `data-for` | Атрибут для итерации по массиву |

## События

| Термин | Определение |
|--------|-------------|
| **Event** | Событие: click, mouseover, keydown, submit и т.д. |
| **EventListener** | Обработчик события; вызывается при наступлении события |
| **EventId** | Числовой идентификатор типа события |
| **Bubbling** | Всплытие события от элемента к родителям |
| **Capture Phase** | Фаза перехвата события от корня к целевому элементу |
| **Default Action** | Стандартное действие браузера при событии |
| **StopPropagation** | Остановка распространения события |

## Ресурсы

| Термин | Определение |
|--------|-------------|
| **TextureHandle** | Дескриптор текстуры; integer, возвращаемый RenderInterface |
| **CompiledGeometry** | Скомпилированная геометрия; GPU buffers для быстрого рендеринга |
| **FontFaceHandle** | Дескриптор шрифта; ссылка на загруженный шрифт |
| **Sprite Sheet** | Текстура с множеством спрайтов; эффективнее отдельных изображений |
| **Nine-patch** | Техника масштабирования изображения с сохранением границ |

## Анимация

| Термин | Определение |
|--------|-------------|
| **Transition** | Анимация изменения свойства при событии (hover, focus) |
| **Transform** | 2D/3D трансформация: rotate, scale, translate |
| **Keyframe Animation** | Покадровая анимация через `@keyframes` |
| **Tween** | Функция интерполяции: ease, linear, ease-in-out |
| **Animation Duration** | Длительность анимации |

## Layout

| Термин | Определение |
|--------|-------------|
| **Block** | Блочный элемент; занимает всю ширину |
| **Inline** | Строчный элемент; занимает только необходимую ширину |
| **Flexbox** | Модель компоновки с гибкими размерами |
| **Flow** | Направление размещения элементов |
| **Containing Block** | Блок-контейнер для позиционирования потомков |
| **Overflow** | Поведение при выходе содержимого за границы элемента |

## Отладка

| Термин | Определение |
|--------|-------------|
| **Debugger** | Встроенный визуальный отладчик |
| **Element Inspector** | Инструмент для просмотра DOM и свойств |
| **Style Editor** | Инструмент для редактирования стилей в реальном времени |
| **Log Console** | Консоль логов RmlUi |

## Сравнение с ImGui

| Термин RmlUi | Аналог ImGui | Отличие |
|--------------|--------------|---------|
| Context | ImGuiContext | RmlUi: retained state, ImGui: immediate |
| Document | N/A | ImGui не имеет документов |
| Element | N/A | ImGui не имеет объектов элементов |
| Data Model | N/A | ImGui не имеет data bindings |
| RCSS | Style | RmlUi: CSS-синтаксис, ImGui: C++ API |
| EventListener | Callback | RmlUi: объект, ImGui: функция |
