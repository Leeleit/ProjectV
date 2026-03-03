# Глоссарий Dear ImGui

**🟢 Уровень 1: Начинающий**

Словарь терминов ImGui. Общие термины Vulkan и SDL — см. [Vulkan — Глоссарий](../vulkan/glossary.md)
и [SDL — Глоссарий](../sdl/glossary.md).

---

## Ядро

| Термин                | Объяснение                                                                                                                                                                                                         |
|-----------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Immediate mode**    | Парадигма UI: виджеты создаются в коде каждый кадр. Нет отдельного «создания» и «разрушения» — если код не вызвал `ImGui::Button()`, кнопки нет. Состояние хранится в ImGui, а не в вашем коде.                    |
| **ImGuiContext**      | Непрозрачная структура контекста ImGui (состояние, окна, стили, шрифты). Создаётся через `ImGui::CreateContext()`, передаётся неявно через вызовы `ImGui::*`. Один контекст на приложение обычно достаточно.       |
| **ImGuiIO**           | Структура ввода-вывода между вашим приложением и ImGui. Через неё передаются размер экрана, время кадра, позиция мыши, нажатия клавиш и т.д. Platform backend заполняет её в `ImGui_ImplXXX_NewFrame()`.           |
| **ImGuiPlatformIO**   | Расширение ImGuiIO: доступ через `GetPlatformIO()`. Содержит хуки для clipboard, IME, кастомных backend-функций. Platform/Renderer backends регистрируют там свои колбэки.                                         |
| **ImGuiStyle**        | Визуальный стиль: цвета, отступы, скругления. `ImGui::GetStyle()` возвращает ссылку; можно менять поля напрямую или вызывать `ImGui::StyleColorsDark()` / `StyleColorsLight()`.                                    |
| **ImGuiID**           | Уникальный идентификатор виджета (хеш строки в стеке ID). ImGui использует его для различения кнопок, окон и т.д. При конфликтах ID — неожиданное поведение; `PushID`/`PopID` помогает.                            |
| **PushID / PopID**    | Стек ID для различения виджетов. В циклах: `PushID(i); ImGui::Button(...); PopID();`. Альтернатива — синтаксис `"Label##unique_id"` в label (текст до `##` виден, после — только ID).                              |
| **ImGuiCond**         | Условие применения для SetNextWindowPos/Size: `ImGuiCond_Once` (при первом применении), `ImGuiCond_FirstUseEver` (если ещё не было), `ImGuiCond_Always` (каждый кадр), `ImGuiCond_Appearing` (при появлении окна). |
| **ImGuiWindowFlags**  | Флаги окна для `Begin()`: `NoTitleBar`, `NoResize`, `NoMove`, `AlwaysAutoResize`, `NoDecoration` (без заголовка, resize и т.д.), `NoInputs` (без ввода), `MenuBar` и др.                                           |
| **Begin/End-паттерн** | Многие функции (Begin, BeginMenu, BeginTable, BeginCombo, BeginPopup) возвращают `bool`. Вызывать соответствующий `End` только если `Begin` вернул `true`: `if (ImGui::Begin(...)) { ... } ImGui::End();`.         |

---

## Ввод

| Термин                  | Объяснение                                                                                                                                                  |
|-------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **WantCaptureMouse**    | `io.WantCaptureMouse == true` означает, что ImGui хочет получать ввод мыши (окно под курсором). Не передавайте события мыши в игру, пока true.              |
| **WantCaptureKeyboard** | `io.WantCaptureKeyboard == true` — ImGui «владеет» клавиатурой (например, поле ввода). Не передавайте клавиши в игру.                                       |
| **ImGuiConfigFlags**    | Флаги конфигурации в `io.ConfigFlags`: `NavEnableKeyboard`, `NavEnableGamepad`, `NoMouseCursorChange` и др.                                                 |
| **ImGuiBackendFlags**   | Флаги возможностей backend в `io.BackendFlags`: `HasGamepad`, `HasMouseCursors`, `RendererHasTextures`, `RendererHasVtxOffset`. Устанавливаются backend'ом. |

---

## Отрисовка

| Термин               | Объяснение                                                                                                                                                                                                                                   |
|----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **ImDrawData**       | Результат `ImGui::Render()`: список команд отрисовки (вершины, индексы, текстуры, clip rect). Передаётся в `ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer)`. `command_buffer` — см. [Vulkan — Глоссарий](../vulkan/glossary.md). |
| **ImDrawList**       | Список draw-команд для одного окна (вершины + индексы). ImGui собирает их в ImDrawData. Редко нужен напрямую; используется при custom rendering через `ImGui::GetWindowDrawList()`.                                                          |
| **ImTextureID**      | Низкоуровневый идентификатор текстуры. В Vulkan backend — `VkDescriptorSet` (см. [Vulkan — Глоссарий](../vulkan/glossary.md)). Регистрация своих текстур: `ImGui_ImplVulkan_AddTexture(sampler, image_view, layout)`.                        |
| **ImTextureRef**     | Обёртка над ImTextureID или ImTextureData* (1.92). Используется в `Image()`, `ImageButton()`. Для своих текстур: `ImTextureRef(descriptor_set)` или через `AddTexture`. ImTextureID устарел для этих функций.                                |
| **ImFontAtlas**      | Атлас шрифтов: одна текстура со всеми глифами. `io.Fonts` — указатель. Backend загружает текстуру на GPU.                                                                                                                                    |
| **ImVec2, ImVec4**   | Векторы: `ImVec2(x, y)` — позиция/размер; `ImVec4(x, y, z, w)` — цвет (RGBA) или прямоугольник. По умолчанию без арифметических операторов; `#define IMGUI_DEFINE_MATH_OPERATORS` в imconfig.h включает их.                                  |
| **ImGuiPayload**     | Данные drag-and-drop. Возвращается из `AcceptDragDropPayload()`; поля `Data`, `DataSize`, `IsDataType("type")`. ImGui копирует данные при `SetDragDropPayload()`, хранит до завершения операции.                                             |
| **ImGuiListClipper** | Вспомогательный объект для отрисовки больших списков: вычисляет видимый диапазон (`DisplayStart`..`DisplayEnd`), чтобы не создавать виджеты для невидимых элементов. `Begin(items_count)` → цикл `for` → `End()`.                            |

---

## Backends

| Термин                      | Объяснение                                                                                                                                                                                          |
|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Backend (Platform)**      | Код, отвечающий за ввод и окно: мышь, клавиатура, геймпад, курсор, размер дисплея. Примеры: `imgui_impl_sdl3`, `imgui_impl_win32`, `imgui_impl_glfw`. Используется в ProjectV через SDL3.           |
| **Backend (Renderer)**      | Код, отвечающий за отрисовку: создание шрифтовой текстуры, запись вершин и вызовы GPU. Примеры: `imgui_impl_vulkan`, `imgui_impl_dx11`, `imgui_impl_opengl3`. Используется в ProjectV через Vulkan. |
| **ImGui::ShowDemoWindow()** | Демо-окно со всеми виджетами и примерами кода. Запускайте при разработке; исходники — в [imgui_demo.cpp](../../external/imgui/imgui_demo.cpp).                                                      |

---

## См. также

- [Основные понятия](concepts.md) — immediate mode, цикл кадра, связь Platform + Renderer.
- [Справочник API](api-reference.md) — функции и структуры, [imgui.h](../../external/imgui/imgui.h).

← [На главную imgui](README.md) | [На главную документации](../README.md)
