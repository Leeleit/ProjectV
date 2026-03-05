# Основные понятия SDL3

**🟢 Уровень 1: Начинающий**

Фундаментальные концепции API SDL3, необходимые для понимания работы с оконной системой и событиями.

---

## main() vs SDL_MAIN_USE_CALLBACKS

SDL3 предлагает два режима входа в приложение.

### Классический main()

Приложение само управляет циклом событий:

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Game", 1280, 720, 0);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        // Обновление и рендеринг
    }

    SDL_Quit();
    return 0;
}
```

### Callback архитектура

SDL управляет циклом и вызывает callbacks:

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Инициализация
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    // Обработка события
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    // Один кадр: обновление, рендеринг
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    // Освобождение ресурсов
}
```

### Сравнение подходов

| Критерий            | Классический main() | Callback архитектура         |
|---------------------|---------------------|------------------------------|
| Контроль над циклом | Полный              | Ограниченный (SDL управляет) |
| Мобильные платформы | Требует адаптации   | Нативная поддержка           |
| Сложность           | Выше (boilerplate)  | Ниже                         |
| Гибкость            | Полная              | Ограничена интерфейсом       |

Callback архитектура рекомендуется для кроссплатформенных проектов.

---

## Жизненный цикл приложения

```
┌─────────────────────────────────────────────────────┐
│                    Запуск                           │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              SDL_AppInit()                          │
│  - SDL_Init, SDL_CreateWindow                       │
│  - return SDL_APP_CONTINUE | SDL_APP_FAILURE        │
└─────────────────────┬───────────────────────────────┘
                      ▼
          ┌───────────────────────┐
          │   SDL_APP_CONTINUE?   │
          └───────────┬───────────┘
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
┌───────────────┐           ┌───────────────┐
│ SDL_AppEvent  │           │ SDL_AppIterate│
│ (каждое       │           │ (каждый кадр) │
│  событие)     │           │               │
└───────┬───────┘           └───────┬───────┘
        │                           │
        └─────────────┬─────────────┘
                      ▼
          ┌───────────────────────┐
          │  SDL_APP_CONTINUE?    │
          └───────────┬───────────┘
                      │ Да
        ┌─────────────┴─────────────┐
        ▼                           ▼
┌───────────────┐           ┌───────────────┐
│ SDL_AppEvent  │           │ SDL_AppIterate│
│     ...       │           │     ...       │
└───────────────┘           └───────────────┘
                      │
                      │ Нет (SUCCESS/FAILURE)
                      ▼
┌─────────────────────────────────────────────────────┐
│              SDL_AppQuit()                          │
│  - SDL_DestroyWindow, очистка ресурсов             │
└─────────────────────────────────────────────────────┘
```

---

## Event Loop

### SDL_AppEvent vs SDL_PollEvent

| Режим                      | Получение событий                               | Обработка             |
|----------------------------|-------------------------------------------------|-----------------------|
| **SDL_MAIN_USE_CALLBACKS** | SDL вызывает `SDL_AppEvent` для каждого события | В теле `SDL_AppEvent` |
| **main()**                 | `while (SDL_PollEvent(&event))`                 | В теле цикла          |

При callback архитектуре **не вызывайте** `SDL_PollEvent` — события доставляются автоматически.

---

## Типы событий

### SDL_EVENT_QUIT и SDL_EVENT_WINDOW_CLOSE_REQUESTED

Это разные события с разной семантикой:

| Событие                              | Когда приходит                                      | Роль                                          |
|--------------------------------------|-----------------------------------------------------|-----------------------------------------------|
| **SDL_EVENT_WINDOW_CLOSE_REQUESTED** | Пользователь нажал крестик на окне                  | Специфично для окна (`event.window.windowID`) |
| **SDL_EVENT_QUIT**                   | Глобальный выход (закрытие последнего окна, Alt+F4) | Выход из приложения                           |

Порядок: сначала `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, затем возможно `SDL_EVENT_QUIT`.

### Часто используемые события

| Тип                                   | Когда                                   |
|---------------------------------------|-----------------------------------------|
| `SDL_EVENT_QUIT`                      | Глобальный выход                        |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED`    | Крестик на окне                         |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | Изменение размера в пикселях (HiDPI)    |
| `SDL_EVENT_WINDOW_RESIZED`            | Изменение размера в оконных координатах |
| `SDL_EVENT_KEY_DOWN`                  | Клавиша нажата                          |
| `SDL_EVENT_KEY_UP`                    | Клавиша отпущена                        |
| `SDL_EVENT_MOUSE_MOTION`              | Движение мыши                           |
| `SDL_EVENT_MOUSE_BUTTON_DOWN`         | Нажатие кнопки мыши                     |
| `SDL_EVENT_MOUSE_BUTTON_UP`           | Отпускание кнопки мыши                  |

---

## SDL_Event

Union всех типов событий. Поле `type` определяет активное поле union:

```cpp
SDL_Event event;
// После получения события:
switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        // event.key — структура SDL_KeyboardEvent
        if (event.key.key == SDLK_ESCAPE) { /* ... */ }
        break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        // event.window — структура SDL_WindowEvent
        // event.window.windowID — ID окна
        break;

    case SDL_EVENT_MOUSE_MOTION:
        // event.motion — структура SDL_MouseMotionEvent
        // event.motion.x, event.motion.y — координаты
        break;
}
```

Данные события **не сохраняются** после возврата из callback — нельзя хранить указатели на поля события.

---

## Клавиатура

### event.key (SDL_KeyboardEvent)

| Поле         | Тип            | Описание                                                   |
|--------------|----------------|------------------------------------------------------------|
| **key**      | `SDL_Keycode`  | Виртуальный код (зависит от раскладки): `SDLK_a`, `SDLK_w` |
| **scancode** | `SDL_Scancode` | Физическая клавиша (не зависит от раскладки)               |
| **mod**      | `SDL_Keymod`   | Модификаторы (Shift, Ctrl, Alt)                            |
| **repeat**   | `bool`         | Автоповтор нажатой клавиши                                 |

Для игр (WASD) предпочтительнее `scancode` — одна клавиша на любой раскладке.

---

## Окно

### Размеры окна

Два способа получения размера:

| Функция                       | Возвращает                                        |
|-------------------------------|---------------------------------------------------|
| `SDL_GetWindowSize()`         | Размер в оконных координатах (логические единицы) |
| `SDL_GetWindowSizeInPixels()` | Размер в пикселях (для HiDPI)                     |

Для создания Vulkan/OpenGL framebuffer используйте `SDL_GetWindowSizeInPixels()`.

### SDL_WindowFlags

| Флаг                            | Описание                 |
|---------------------------------|--------------------------|
| `SDL_WINDOW_VULKAN`             | Окно поддерживает Vulkan |
| `SDL_WINDOW_OPENGL`             | Окно поддерживает OpenGL |
| `SDL_WINDOW_RESIZABLE`          | Окно можно изменять      |
| `SDL_WINDOW_FULLSCREEN`         | Полноэкранный режим      |
| `SDL_WINDOW_HIDDEN`             | Окно изначально скрыто   |
| `SDL_WINDOW_BORDERLESS`         | Без рамки                |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | Запрос высокого DPI      |

---

## Мышь

### event.motion (SDL_MouseMotionEvent)

| Поле           | Описание               |
|----------------|------------------------|
| `x`, `y`       | Координаты курсора     |
| `xrel`, `yrel` | Относительное смещение |
| `state`        | Состояние кнопок       |

### event.button (SDL_MouseButtonEvent)

| Поле     | Описание                                             |
|----------|------------------------------------------------------|
| `button` | Кнопка (`SDL_BUTTON_LEFT`, `SDL_BUTTON_RIGHT` и др.) |
| `clicks` | Количество кликов (double click = 2)                 |
| `x`, `y` | Координаты                                           |

---

## Результаты callbacks

### SDL_AppResult

```cpp
typedef enum SDL_AppResult {
    SDL_APP_FAILURE,  // Ошибка, завершить
    SDL_APP_SUCCESS,  // Успешное завершение
    SDL_APP_CONTINUE  // Продолжить работу
} SDL_AppResult;
```

| Значение           | Семантика                           |
|--------------------|-------------------------------------|
| `SDL_APP_CONTINUE` | Приложение продолжает работу        |
| `SDL_APP_SUCCESS`  | Нормальное завершение (exit code 0) |
| `SDL_APP_FAILURE`  | Завершение с ошибкой (exit code 1)  |

---

## Многопоточность

SDL не является полностью thread-safe:

- Большинство функций должны вызываться из main thread
- События обрабатываются в main thread
- При callback архитектуре все callbacks выполняются в main thread

Для фоновых задач используйте отдельные потоки, но не вызывайте SDL функции из них.
