# Концепции RmlUi

🟡 **Уровень 2: Средний**

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

```css
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

```css
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
