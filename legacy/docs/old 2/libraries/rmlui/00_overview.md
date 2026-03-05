# Обзор RmlUi

🟢 **Уровень 1: Начинающий**

**RmlUi** — библиотека пользовательского интерфейса для C++ на базе HTML/CSS стандартов. Использует retained mode
архитектуру: состояние UI сохраняется между кадрами. Предназначена для игровых HUD, меню и приложений, требующих
профессионального вида интерфейса.

Версия: **6.0+** (см. [Core.h](../../external/rmlui/Include/RmlUi/Core/Core.h))
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

## Требования

- **C++17** или новее
- **FreeType** (опционально, заменяем через FontEngineInterface)
- **Стандартная библиотека**

## Содержание документации

| Раздел                                         | Описание                                          |
|------------------------------------------------|---------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Минимальный пример: инициализация, контекст, цикл |
| [02_concepts.md](02_concepts.md)               | RML/RCSS, Context, Element, Data Bindings         |
| [03_integration.md](03_integration.md)         | CMake, зависимости, backends, interfaces          |
| [04_api-reference.md](04_api-reference.md)     | Справочник: Core, Context, Element, Event         |
| [05_tools.md](05_tools.md)                     | Debugger, Lua plugin, Lottie, SVG                 |
| [06_performance.md](06_performance.md)         | Retained mode, geometry caching, textures         |
| [07_troubleshooting.md](07_troubleshooting.md) | Частые проблемы, отладка                          |
| [08_glossary.md](08_glossary.md)               | Словарь терминов                                  |

## Для ProjectV

Интеграция с ProjectV (Vulkan, SDL3, VMA, flecs) описана в отдельных файлах:

- [09_projectv-integration.md](09_projectv-integration.md) — связка RmlUi с Vulkan рендерером и SDL3
- [10_projectv-patterns.md](10_projectv-patterns.md) — паттерны HUD для воксельного движка
