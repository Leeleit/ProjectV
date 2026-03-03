# Dear ImGui: Библиотека UI

> **Для понимания:** Представьте, что ImGui — это "магнитофон": вы говорите "кнопка здесь", и она появляется. В
> традиционном UI вы "создаёте" кнопку один раз, а потом управляете её состоянием. В ImGui вы просто говорите
`Button("Click me")` каждый кадр — и кнопка появляется. Если не сказали — кнопки нет. Это как-live coding для
> интерфейсов.

**Dear ImGui** — библиотека графического интерфейса с архитектурой immediate mode для C++.

## Основные возможности

- **Immediate mode** — виджеты создаются вызовами функций каждый кадр
- **Минимальные зависимости** — только стандартная библиотека C++
- **Backend-архитектура** — раздельные Platform и Renderer backends
- **Кроссплатформенность** — Windows, Linux, macOS

## Архитектура

```
ImGui Core (imgui.cpp)
    ├── Platform Backend (ввод: мышь, клавиатура)
    │       └── imgui_impl_sdl3.cpp
    └── Renderer Backend (отрисовка)
            └── imgui_impl_vulkan.cpp
```

## Цикл кадра

```cpp
// 1. NewFrame
ImGui_ImplVulkan_NewFrame();
ImGui_ImplSDL3_NewFrame();
ImGui::NewFrame();

// 2. Виджеты
if (ImGui::Begin("Window")) {
    ImGui::Text("Hello");
    ImGui::Button("Click");
}
ImGui::End();

// 3. Рендеринг
ImGui::Render();
ImDrawData* drawData = ImGui::GetDrawData();
ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
```

## Основные виджеты

```cpp
ImGui::Text("Text");
ImGui::Button("Click");
ImGui::Checkbox("Flag", &boolValue);
ImGui::SliderFloat("Float", &floatValue, 0.0f, 1.0f);
ImGui::InputText("Input", buffer, sizeof(buffer));
ImGui::BeginTabBar("Tabs");
ImGui::BeginTable("Table", 3);
```

## Begin/End паттерн

```cpp
// Окно
if (ImGui::Begin("My Window")) {
    // содержимое
}
ImGui::End();

// Меню
if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open")) { }
    ImGui::EndMenu();
}

// Таблица (End только если Begin вернул true)
if (ImGui::BeginTable("Table", 2)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::EndTable();
}
```

## ID Stack

```cpp
// Конфликт ID в цикле — используй PushID
for (int i = 0; i < 10; ++i) {
    ImGui::PushID(i);
    ImGui::Button("Delete");
    ImGui::PopID();
}
```

## WantCapture

```cpp
ImGuiIO& io = ImGui::GetIO();

if (io.WantCaptureMouse) {
    // ImGui обрабатывает мышь
} else {
    // Камера/игра обрабатывает мышь
}
```

## Глоссарий

### Immediate mode

Парадигма UI: виджеты создаются каждый кадр. Нет отдельного "создания" и "разрушения".

### ImGuiContext

Контекст ImGui, содержит состояние всех окон и виджетов.

### ImDrawData

Результат рендеринга: список команд отрисовки для передачи в backend.

### WantCaptureMouse/Keyboard

Флаги, показывающие, что ImGui обрабатывает ввод.
