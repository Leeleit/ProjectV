# Quickstart: RmlUi

🟢 **Уровень 1: Начинающий**

Минимальный пример интеграции RmlUi: инициализация, создание контекста, загрузка документа, цикл обновления и
рендеринга.

## Минимальный пример

```cpp
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>

// Простейший render interface (заглушка)
class MinimalRenderInterface : public Rml::RenderInterface {
public:
    void RenderGeometry(Rml::Vertex* vertices, int numVertices,
                       int* indices, int numIndices,
                       Rml::TextureHandle texture,
                       const Rml::Vector2f& translation) override {
        // Реальная реализация: отрисовка через Vulkan/OpenGL
        // Здесь — заглушка для демонстрации
    }

    void EnableScissorRegion(bool enable) override {}
    void SetScissorRegion(int x, int y, int width, int height) override {}
};

int main(int argc, char** argv) {
    // 1. Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "RmlUi Quickstart",
        1280, 720,
        SDL_WINDOW_VULKAN
    );

    // 2. Установка интерфейсов RmlUi
    MinimalRenderInterface renderInterface;
    Rml::SetRenderInterface(&renderInterface);

    // 3. Инициализация RmlUi
    if (!Rml::Initialise()) {
        std::fprintf(stderr, "Rml::Initialise failed\n");
        return 1;
    }

    // 4. Создание контекста
    Rml::Context* context = Rml::CreateContext(
        "main",
        Rml::Vector2i(1280, 720)
    );
    if (!context) {
        std::fprintf(stderr, "Rml::CreateContext failed\n");
        Rml::Shutdown();
        return 1;
    }

    // 5. Загрузка шрифтов
    Rml::LoadFontFace("fonts/Roboto-Regular.ttf");

    // 6. Загрузка документа
    Rml::ElementDocument* document = context->LoadDocument("ui/hello.rml");
    if (document) {
        document->Show();
    }

    // 7. Главный цикл
    bool running = true;
    while (running) {
        // Обработка событий SDL
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                context->ProcessMouseMove(
                    static_cast<int>(event.motion.x),
                    static_cast<int>(event.motion.y),
                    0  // key modifier state
                );
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                int button = 0;
                if (event.button.button == SDL_BUTTON_LEFT)   button = 0;
                if (event.button.button == SDL_BUTTON_RIGHT)  button = 1;
                if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
                context->ProcessMouseButtonDown(button);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                int button = 0;
                if (event.button.button == SDL_BUTTON_LEFT)   button = 0;
                if (event.button.button == SDL_BUTTON_RIGHT)  button = 1;
                if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
                context->ProcessMouseButtonUp(button);
            }
        }

        // Обновление контекста
        context->Update();

        // Рендеринг (здесь должна быть реальная отрисовка)
        context->Render();
    }

    // 8. Очистка
    Rml::Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

## Документ RML

Файл `ui/hello.rml`:

```html
<rml>
<head>
    <title>Hello RmlUi</title>
    <style>
        body {
            font-family: Roboto;
            font-size: 18px;
            color: white;
            background: #333;
            width: 100%;
            height: 100%;
        }
        h1 {
            color: #4a9eff;
            font-size: 32px;
            text-align: center;
            margin-top: 100px;
        }
        p {
            text-align: center;
        }
    </style>
</head>
<body>
    <h1>Hello, RmlUi!</h1>
    <p>This is a minimal example.</p>
</body>
</rml>
```

## Порядок инициализации

1. **SDL/Platform** — создание окна, получение размеров
2. **SetRenderInterface** — установка интерфейса рендеринга
3. **Rml::Initialise** — инициализация ядра RmlUi
4. **CreateContext** — создание контекста с размерами окна
5. **LoadFontFace** — загрузка шрифтов (обязательно!)
6. **LoadDocument** — загрузка RML документа
7. **document->Show()** — отображение документа

## Обработка ввода

RmlUi требует передачи событий ввода в контекст:

| Событие SDL                   | Метод RmlUi Context                   |
|-------------------------------|---------------------------------------|
| `SDL_EVENT_MOUSE_MOTION`      | `ProcessMouseMove(x, y, modifiers)`   |
| `SDL_EVENT_MOUSE_BUTTON_DOWN` | `ProcessMouseButtonDown(button)`      |
| `SDL_EVENT_MOUSE_BUTTON_UP`   | `ProcessMouseButtonUp(button)`        |
| `SDL_EVENT_MOUSE_WHEEL`       | `ProcessMouseWheel(delta, modifiers)` |
| `SDL_EVENT_KEY_DOWN`          | `ProcessKeyDown(key_code, modifiers)` |
| `SDL_EVENT_KEY_UP`            | `ProcessKeyUp(key_code, modifiers)`   |
| `SDL_EVENT_TEXT_INPUT`        | `ProcessTextInput(text)`              |

## Коды кнопок мыши

| Кнопка | Индекс |
|--------|--------|
| Left   | 0      |
| Right  | 1      |
| Middle | 2      |

## Цикл кадра

```cpp
while (running) {
    pollEvents();        // SDL_PollEvent → ProcessMouse/Key
    context->Update();   // Обновление анимаций, данных
    context->Render();   // Вызовы RenderInterface
    present();           // Swap buffers
}
```

`context->Update()` обрабатывает анимации, data bindings и внутреннее состояние.

`context->Render()` отправляет геометрию через `RenderInterface`.

## Следующие шаги

- Реализовать `RenderInterface` для Vulkan/OpenGL
- Добавить data bindings для синхронизации с игровыми данными
- Изучить RCSS для стилизации элементов
