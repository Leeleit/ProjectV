# RmlUi: Справочник по библиотеке UI

> **Для понимания:** RmlUi — это как профессиональный редактор документов для игр. Представьте, что вы работаете в
> Photoshop: у вас есть слои (contexts), документы (documents), и каждый элемент на слое — это объект с настраиваемыми
> свойствами. В отличие от ImGui, где интерфейс перерисовывается каждый кадр (как анимационный мультик), RmlUi сохраняет
> состояние между кадрами (как документ Word), и вы只是 обновляете данные.

## Что такое RmlUi?

**RmlUi** — это C++ библиотека для создания пользовательского интерфейса на основе стандартов HTML и CSS. Это форк
libRocket с новыми функциями, исправлениями багов и улучшениями производительности.

## Архитектура Retained Mode

> **Для понимания:** Retained mode — это как таблица в Excel. Вы один раз настроили ячейки, формулы и стили, а дальше
> Excel сам обновляет значения. В ImGui (immediate mode) вы бы рисовали таблицу заново каждый кадр, а здесь состояние
> сохраняется.

| Аспект     | Retained Mode (RmlUi)      | Immediate Mode (ImGui)    |
|------------|----------------------------|---------------------------|
| Состояние  | Сохраняется между кадрами  | Пересоздаётся каждый кадр |
| Обновление | Только изменённые элементы | Все элементы              |
| Сложность  | Выше (нужно понимать DOM)  | Ниже (просто рисуй)       |
| Память     | Больше (хранит дерево)     | Меньше                    |

## Два уровня абстракции

RmlUi разделена на ядро и абстрактные интерфейсы:

| Интерфейс               | Назначение                    | Обязательность                     |
|-------------------------|-------------------------------|------------------------------------|
| **RenderInterface**     | Отрисовка вершин и текстур    | Обязателен                         |
| **SystemInterface**     | Время, логирование, clipboard | Опционален                         |
| **FileInterface**       | Загрузка файлов               | Опционален                         |
| **FontEngineInterface** | Рендеринг шрифтов             | Опционален (FreeType по умолчанию) |

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

## RML: RmlUi Markup Language

> **Для понимания:** RML — это как XML для описания структуры интерфейса. Это не полноценный HTML, а специализированный
> язык разметки для UI.

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
| `<textarea>`  | Многострочное поле             |
| `<img>`       | Изображение                    |

## RCSS: RmlUi Cascading Style Sheets

> **Для понимания:** RCSS — это как CSS, но для игрового UI. Вы определяете правила стилизации, а RmlUi вычисляет
> итоговый вид каждого элемента.

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

/* Flexbox */
.menu-items {
    display: flex;
    flex-direction: column;
    gap: 8px;
}
```

### Поддерживаемые свойства

| Категория  | Свойства                                                        |
|------------|-----------------------------------------------------------------|
| Layout     | `display`, `position`, `width`, `height`, `margin`, `padding`   |
| Flexbox    | `flex-direction`, `flex-wrap`, `justify-content`, `align-items` |
| Background | `background-color`, `background-image`                          |
| Border     | `border`, `border-radius`                                       |
| Text       | `font-family`, `font-size`, `font-weight`, `color`              |
| Transform  | `transform`, `transform-origin`, `perspective`                  |
| Animation  | `transition`, `animation`                                       |
| Effects    | `filter`, `opacity`                                             |

### DP единицы

RmlUi использует `dp` (density-independent pixels) для масштабирования под DPI:

```css
.element {
    width: 200dp;
    font-size: 16dp;
}
```

## Context: Изолированный контейнер

> **Для понимания:** Context — это как слой в графическом редакторе. У вас может быть слой для HUD (всегда виден), слой
> для меню (поверх HUD), слой для диалогов. Каждый слой имеет свои размеры и DPI.

```cpp
#include <RmlUi/Core.h>
#include <print>

// Создание контекста
Rml::Context* context = Rml::CreateContext(
    "main",
    Rml::Vector2i(1920, 1080)
);

if (!context) {
    std::println(stderr, "Failed to create context");
    return;
}

// Загрузка документа
Rml::ElementDocument* doc = context->LoadDocument("ui/menu.rml");
if (doc) {
    doc->Show();
}

// В игровом цикле:
context->Update();   // Обновление состояния
context->Render();   // Генерация геометрии

// Очистка
Rml::RemoveContext("main");
```

### Множественные контексты

```cpp
// HUD контекст (всегда видим)
Rml::Context* hudContext = Rml::CreateContext("hud", {1920, 1080});

// Меню контекст (поверх HUD)
Rml::Context* menuContext = Rml::CreateContext("menu", {1920, 1080});

// Рендеринг в порядке слоёв
hudContext->Render();
menuContext->Render();
```

## Element: Узел DOM-дерева

> **Для понимания:** Element — это как объект в DOM-дереве браузера. У него есть родитель, дети, свойства, атрибуты. Вы
> можете получить любой элемент по ID и изменить его.

```cpp
#include <RmlUi/Core.h>

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

## Data Bindings: Синхронизация данных

> **Для понимания:** Data Bindings — это как связь ячеек в Excel. Изменили значение в программе — UI автоматически
> обновился. Никаких ручных вызовов SetInnerRML.

### Создание Data Model

```cpp
#include <RmlUi/Core.h>

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
    if (auto itemModel = model.RegisterStruct<Item>()) {
        itemModel.RegisterMember("name", &Item::name);
        itemModel.RegisterMember("count", &Item::count);
    }

    model.Bind("inventory", &playerData.inventory);
}
```

### События из Data Model

```cpp
model.BindEventFunc("use_item",
    [](Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& args) {
        int itemIndex = args[0].Get<int>();
        // Обработка
    }
);
```

```html
<button data-event-click="use_item(item_index)">Use</button>
```

## Decorators: Кастомные эффекты

> **Для понимания:** Decorators — это как стили в графическом редакторе. Вы определяете эффект (градиент, изображение,
> тень), а потом применяете его к любому элементу.

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

### Complex decorator

```
.card {
    decorator: complex {
        gradient: linear-gradient(180deg, #444, #222);
        box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.5);
        border: 1px #555;
        border-radius: 4px;
    };
}
```

## Templates: Переиспользуемые шаблоны

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

## Sprites: Анимированные спрайты

```
@sprite_sheet ui_sprites {
    src: sprites.png;
    resolution: 2dp;

    button-normal:   0px 0px 100px 40px;
    button-hover:    0px 40px 100px 40px;
    button-active:   0px 80px 100px 40px;
}

.button {
    decorator: image(ui_sprites:button-normal);
}
.button:hover {
    decorator: image(ui_sprites:button-hover);
}
```

## Localization: Локализация

```cpp
// Реализация в SystemInterface
Rml::String TranslateString(const Rml::String& key) override {
    static std::unordered_map<Rml::String, Rml::String> translations = {
        {"menu.start", "Start Game"},
        {"menu.settings", "Settings"},
        {"menu.quit", "Quit"}
    };
    return translations.value(key, key);
}
```

```html
<button>menu.start</button>
<button>menu.settings</button>
<button>menu.quit</button>
```

## Анимации

### Transitions

```css
.button {
    transition: background 0.2s ease-out, transform 0.1s ease-in;
}

.button:hover {
    transform: scale(1.05);
}
```

### Keyframes

```css
@keyframes pulse {
    0% { transform: scale(1); }
    50% { transform: scale(1.1); }
    100% { transform: scale(1); }
}

.pulsing {
    animation: pulse 1s ease-in-out infinite;
}
```

## Входные события

```cpp
// Мышь
context->ProcessMouseMove(x, y, keyModifiers);
context->ProcessMouseButtonDown(0);  // 0 = левая кнопка
context->ProcessMouseButtonUp(0);
context->ProcessMouseWheel(delta, keyModifiers);

// Клавиатура
context->ProcessKeyDown(Rml::Input::KeyCode::K_w, 0);
context->ProcessKeyUp(Rml::Input::KeyCode::K_w, 0);

// Текст
context->ProcessTextInput("text");
```

## Debugger

```cpp
#include <RmlUi/Debugger.h>

// Активация
Rml::Debugger::Initialise(context);
Rml::Debugger::SetVisible(true);

// Горячие клавиши:
// F8 - показать/скрыть отладчик
// Ctrl+Shift+I - Element Inspector
// Ctrl+Shift+S - Style Editor
```

## Жизненный цикл

```cpp
// 1. Создание интерфейсов
MyRenderInterface renderInterface;
MySystemInterface systemInterface;

// 2. Установка интерфейсов
Rml::SetRenderInterface(&renderInterface);
Rml::SetSystemInterface(&systemInterface);

// 3. Инициализация RmlUi
Rml::Initialise();

// 4. Создание контекста
Rml::Context* context = Rml::CreateContext("main", {1920, 1080});

// 5. Загрузка шрифтов
Rml::LoadFontFace("fonts/Roboto-Regular.ttf");

// 6. Загрузка документа
Rml::ElementDocument* doc = context->LoadDocument("ui/menu.rml");
doc->Show();

// 7. Главный цикл
while (running) {
    // ... обработка ввода ...

    context->Update();
    context->Render();
}

// 8. Очистка
Rml::Shutdown();
```

## Глоссарий

| Термин           | Определение                                |
|------------------|--------------------------------------------|
| **RML**          | RmlUi Markup Language — язык разметки      |
| **RCSS**         | RmlUi Cascading Style Sheets — язык стилей |
| **Context**      | Изолированный контейнер для UI документов  |
| **Element**      | Узел DOM-дерева                            |
| **Data Binding** | Связь между данными и UI                   |
| **Decorator**    | Визуальный эффект                          |
| **dp**           | Density-independent pixel                  |

## Сравнение с ImGui

| Аспект        | RmlUi       | ImGui         |
|---------------|-------------|---------------|
| Режим         | Retained    | Immediate     |
| Состояние     | Сохраняется | Пересоздаётся |
| Стили         | CSS/RCSS    | C++ код       |
| Data Bindings | Встроены    | Ручные        |
| Размер        | ~1MB        | ~200KB        |
| Сложность     | Выше        | Ниже          |

RmlUi идеален для сложных игровых интерфейсов: меню, инвентари, диалоги. ImGui — для отладки и инструментов.
