# Обзор Dear ImGui

🟢 **Уровень 1: Начинающий**

**Dear ImGui** — библиотека графического интерфейса для C++ с архитектурой immediate mode. Используется для создания
инструментов отладки, редакторов и игровых UI. Отличается отсутствием сохранённого состояния виджетов: интерфейс
создаётся и отрисовывается каждый кадр заново.

Версия: **1.92+**
Исходники: [ocornut/imgui](https://github.com/ocornut/imgui)

## Основные возможности

- **Immediate mode архитектура** — виджеты создаются вызовами функций каждый кадр
- **Минимальные зависимости** — только стандартная библиотека C++
- **Кроссплатформенность** — Windows, Linux, macOS
- **Backend-архитектура** — раздельные Platform и Renderer backends
- **Отладочные инструменты** — встроенные демо-окна, метрики, инспектор стилей

## Типичное применение

- Debug UI для движков и приложений
- Инспекторы объектов и свойств
- Профилировщики и консоли
- Редакторы уровней и инструментов
- Внутриигровые меню и HUD

## Структура библиотеки

```
imgui/
├── imgui.cpp          # Ядро
├── imgui_draw.cpp     # Отрисовка
├── imgui_widgets.cpp  # Виджеты
├── imgui_tables.cpp   # Таблицы
├── backends/          # Platform и Renderer backends
│   ├── imgui_impl_sdl3.cpp
│   ├── imgui_impl_vulkan.cpp
│   └── ...
└── misc/              # Утилиты (cpp/imgui_stdlib.h и др.)
```

## Архитектура: Platform + Renderer

ImGui разделён на ядро и два типа backend'ов:

| Backend      | Назначение                                              | Примеры                 |
|--------------|---------------------------------------------------------|-------------------------|
| **Platform** | Ввод (мышь, клавиатура), размер окна, курсор, clipboard | SDL3, GLFW, Win32       |
| **Renderer** | Создание шрифтовой текстуры, отрисовка вершин           | Vulkan, OpenGL, DirectX |

Один Platform backend + один Renderer backend.

## Карта заголовков

| Заголовок                        | Содержимое                                            |
|----------------------------------|-------------------------------------------------------|
| `<imgui.h>`                      | Основной API: Context, Begin/End, виджеты             |
| `<imgui_internal.h>`             | Внутренние структуры (для продвинутого использования) |
| `<backends/imgui_impl_sdl3.h>`   | Platform backend для SDL3                             |
| `<backends/imgui_impl_vulkan.h>` | Renderer backend для Vulkan                           |
| `<misc/cpp/imgui_stdlib.h>`      | InputText с std::string                               |

## Требования

- **C++11** или новее (рекомендуется C++17)
- **Platform backend** для ввода (SDL3, GLFW, Win32)
- **Renderer backend** для отрисовки (Vulkan, OpenGL, DirectX)

## Содержание документации

| Раздел                                         | Описание                                           |
|------------------------------------------------|----------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Минимальный пример: инициализация, цикл кадра      |
| [02_concepts.md](02_concepts.md)               | Immediate mode, Begin/End, ID Stack, WantCapture*  |
| [03_integration.md](03_integration.md)         | CMake, imconfig.h, backends, порядок инициализации |
| [04_api-reference.md](04_api-reference.md)     | Справочник функций и структур                      |
| [05_widgets.md](05_widgets.md)                 | Виджеты: кнопки, слайдеры, таблицы, меню           |
| [06_troubleshooting.md](06_troubleshooting.md) | Диагностика и частые проблемы                      |
| [07_glossary.md](07_glossary.md)               | Словарь терминов ImGui                             |
