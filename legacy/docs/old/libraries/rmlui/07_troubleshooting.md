# Устранение неполадок RmlUi

🟡 **Уровень 2: Средний**

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

```css
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
