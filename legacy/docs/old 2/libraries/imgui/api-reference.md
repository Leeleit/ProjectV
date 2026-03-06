# Справочник API Dear ImGui

**🟡 Уровень 2: Средний**

Полные объявления — в [imgui.h](../../external/imgui/imgui.h) и
backend-хедерах.

## Оглавление

- [Какую функцию когда вызывать](#какую-функцию-когда-вызывать)
- [Ядро ImGui](#ядро-imgui)
- [Окна](#окна)
- [Layout](#layout)
- [Виджеты: текст](#виджеты-текст)
- [Виджеты: кнопки и выбор](#виджеты-кнопки-и-выбор)
- [Виджеты: слайдеры и ввод](#виджеты-слайдеры-и-ввод)
- [Виджеты: Combo, TreeNode](#виджеты-combo-treenode)
- [Виджеты: изображения](#виджеты-изображения)
- [Таблицы](#таблицы)
- [Меню](#меню)
- [Tab bar](#tab-bar)
- [Popups](#popups)
- [Drag-and-drop](#drag-and-drop)
- [ImGuiListClipper](#imguilistclipper)
- [Структуры](#структуры)
- [Backend SDL3](#backend-sdl3-platform)
- [Backend Vulkan](#backend-vulkan-renderer)
- [imconfig.h](#imconfigh--ключевые-опции)

---

## Какую функцию когда вызывать

| Ситуация            | Функция                                                                                                                         |
|---------------------|---------------------------------------------------------------------------------------------------------------------------------|
| Инициализация       | `ImGui::CreateContext()`, `ImGui_ImplSDL3_InitForVulkan()`, `ImGui_ImplVulkan_Init()`                                           |
| Начало кадра        | `ImGui_ImplVulkan_NewFrame()`, `ImGui_ImplSDL3_NewFrame()`, `ImGui::NewFrame()`                                                 |
| Создать окно        | `ImGui::Begin(name, ...)`, `ImGui::End()`                                                                                       |
| Текст               | `ImGui::Text()`, `ImGui::TextDisabled()`, `ImGui::TextColored()`                                                                |
| Кнопка, чекбокс     | `ImGui::Button()`, `ImGui::Checkbox()`                                                                                          |
| Слайдер, ввод числа | `ImGui::SliderFloat()`, `ImGui::SliderInt()`, `ImGui::InputFloat()`                                                             |
| Таблица             | `ImGui::BeginTable()`, `ImGui::TableNextRow()`, `ImGui::TableNextColumn()`, `ImGui::EndTable()`                                 |
| Меню                | `ImGui::BeginMenuBar()`, `ImGui::BeginMenu()`, `ImGui::MenuItem()`, `ImGui::EndMenu()`, `ImGui::EndMenuBar()`                   |
| Tab bar             | `ImGui::BeginTabBar()`, `ImGui::BeginTabItem()`, `ImGui::EndTabItem()`, `ImGui::EndTabBar()`                                    |
| Popup               | `ImGui::OpenPopup()`, `ImGui::BeginPopup()`, `ImGui::EndPopup()`                                                                |
| Drag-and-drop       | `ImGui::BeginDragDropSource()`, `ImGui::SetDragDropPayload()`, `ImGui::BeginDragDropTarget()`, `ImGui::AcceptDragDropPayload()` |
| Большой список      | `ImGuiListClipper::Begin()`, цикл по `DisplayStart`/`DisplayEnd`, `End()`                                                       |
| Цвет                | `ImGui::ColorEdit3()`, `ImGui::ColorEdit4()`                                                                                    |
| Закончить кадр      | `ImGui::Render()`, `ImGui::GetDrawData()`, `ImGui_ImplVulkan_RenderDrawData()`                                                  |
| Завершение          | `ImGui_ImplVulkan_Shutdown()`, `ImGui_ImplSDL3_Shutdown()`, `ImGui::DestroyContext()`                                           |

Подробный порядок: [Интеграция](integration.md), [Основные понятия — Цикл кадра](concepts.md#цикл-кадра-imgui).

---

## Ядро ImGui

### CreateContext

```cpp
ImGuiContext* ImGui::CreateContext(ImFontAtlas* shared_font_atlas = NULL);
```

Создаёт контекст ImGui. Один контекст на приложение обычно достаточно. `shared_font_atlas` — опционально, для общего
атласа шрифтов.

---

### DestroyContext

```cpp
void ImGui::DestroyContext(ImGuiContext* ctx = NULL);
```

Уничтожает контекст. NULL — текущий. Вызывать после shutdown backends.

---

### SetAllocatorFunctions

```cpp
void ImGui::SetAllocatorFunctions(ImGuiMemAllocFunc alloc_func, ImGuiMemFreeFunc free_func, void* user_data = NULL);
```

Кастомные аллокаторы. Вызывать **до** `CreateContext()`.

---

### GetIO

```cpp
ImGuiIO& ImGui::GetIO();
```

Доступ к вводу и настройкам. См. [ImGuiIO](#imguiio) ниже.

---

### GetPlatformIO

```cpp
ImGuiPlatformIO& ImGui::GetPlatformIO();
```

Доступ к clipboard, IME, backend-хукам.

---

### GetStyle

```cpp
ImGuiStyle& ImGui::GetStyle();
```

Стили (цвета, отступы). Менять через `PushStyleColor`/`PopStyleColor` или напрямую после `NewFrame()`.

---

### NewFrame

```cpp
void ImGui::NewFrame();
```

Начало нового кадра. Вызывать после `ImGui_ImplVulkan_NewFrame()` и `ImGui_ImplSDL3_NewFrame()`.

---

### EndFrame

```cpp
void ImGui::EndFrame();
```

Завершает кадр без рендеринга. Вызывается автоматически внутри `Render()`. Использовать при пропуске отрисовки (
например, окно свёрнуто), но уже потрачен CPU на виджеты.

---

### Render

```cpp
void ImGui::Render();
```

Генерирует draw data. Данные действительны до следующего `NewFrame()`.

---

### GetDrawData

```cpp
ImDrawData* ImGui::GetDrawData();
```

Возвращает данные для отрисовки. Проверять `DisplaySize.x/y > 0` перед рендером.

---

### StyleColorsDark / StyleColorsLight / StyleColorsClassic

```cpp
void ImGui::StyleColorsDark(ImGuiStyle* dst = NULL);
void ImGui::StyleColorsLight(ImGuiStyle* dst = NULL);
void ImGui::StyleColorsClassic(ImGuiStyle* dst = NULL);
```

Применить тему. NULL — к текущему стилю.

---

### Debug tools

```cpp
void ImGui::ShowDemoWindow(bool* p_open = NULL);
void ImGui::ShowMetricsWindow(bool* p_open = NULL);
void ImGui::ShowDebugLogWindow(bool* p_open = NULL);
void ImGui::ShowIDStackToolWindow(bool* p_open = NULL);
void ImGui::ShowAboutWindow(bool* p_open = NULL);
void ImGui::ShowStyleEditor(ImGuiStyle* ref = NULL);
bool ImGui::ShowStyleSelector(const char* label);
void ImGui::ShowFontSelector(const char* label);
```

`ShowDemoWindow` — все виджеты; `ShowMetricsWindow` — внутреннее состояние; `ShowIDStackToolWindow` — отладка ID.
Исходники — [imgui_demo.cpp](../../external/imgui/imgui_demo.cpp).

---

## Окна

### Begin / End

```cpp
bool ImGui::Begin(const char* name, bool* p_open = NULL, ImGuiWindowFlags flags = 0);
void ImGui::End();
```

`Begin` — создать/открыть окно. Возвращает `false`, если окно свёрнуто или скрыто (всё равно вызывать `End()`).
`p_open` — при нажатии [X] станет false.

---

### SetNextWindowPos / SetNextWindowSize

```cpp
void ImGui::SetNextWindowPos(const ImVec2& pos, ImGuiCond cond = 0, const ImVec2& pivot = ImVec2(0,0));
void ImGui::SetNextWindowSize(const ImVec2& size, ImGuiCond cond = 0);
```

Настройка следующего окна. Вызывать **до** `Begin()`. `pivot` — (0.5, 0.5) для центрирования.

---

### ImGuiWindowFlags (ключевые)

| Флаг                                | Описание                                            |
|-------------------------------------|-----------------------------------------------------|
| `ImGuiWindowFlags_NoTitleBar`       | Без заголовка                                       |
| `ImGuiWindowFlags_NoResize`         | Без изменения размера                               |
| `ImGuiWindowFlags_NoMove`           | Без перетаскивания                                  |
| `ImGuiWindowFlags_AlwaysAutoResize` | Авто-размер по содержимому                          |
| `ImGuiWindowFlags_NoDecoration`     | NoTitleBar \| NoResize \| NoScrollbar \| NoCollapse |
| `ImGuiWindowFlags_NoInputs`         | Без мыши и клавиатуры                               |
| `ImGuiWindowFlags_MenuBar`          | Меню-бар в окне                                     |

---

### ImGuiCond

| Значение                 | Описание                    |
|--------------------------|-----------------------------|
| `ImGuiCond_Once`         | Применить при первом вызове |
| `ImGuiCond_FirstUseEver` | Если ещё не сохранено       |
| `ImGuiCond_Always`       | Каждый кадр                 |
| `ImGuiCond_Appearing`    | При появлении окна          |

---

### BeginChild / EndChild

```cpp
bool ImGui::BeginChild(const char* str_id, const ImVec2& size = ImVec2(0,0), ImGuiChildFlags child_flags = 0);
void ImGui::EndChild();
```

Дочернее окно — область со скроллом. `size == ImVec2(0,0)` — занять оставшееся пространство.

---

## Layout

### Позиционирование

```cpp
ImVec2 ImGui::GetCursorScreenPos();      // Позиция курсора (экранные координаты). Рекомендуется.
ImVec2 ImGui::GetContentRegionAvail();   // Доступное пространство от текущей позиции.
void   ImGui::SetCursorScreenPos(const ImVec2& pos);
ImVec2 ImGui::GetCursorPos();             // Локальные координаты окна
void   ImGui::SetCursorPos(const ImVec2& local_pos);
```

---

### SameLine, Separator, Spacing, Dummy

```cpp
void ImGui::SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
void ImGui::Separator();
void ImGui::Spacing();
void ImGui::Dummy(const ImVec2& size);
```

---

### PushStyleColor / PopStyleColor, PushStyleVar / PopStyleVar

```cpp
void ImGui::PushStyleColor(ImGuiCol idx, ImU32 col);
void ImGui::PushStyleColor(ImGuiCol idx, const ImVec4& col);
void ImGui::PopStyleColor(int count = 1);

void ImGui::PushStyleVar(ImGuiStyleVar idx, float val);
void ImGui::PushStyleVar(ImGuiStyleVar idx, const ImVec2& val);
void ImGui::PopStyleVar(int count = 1);
```

Стиль применяется к последующим виджетам до Pop.

---

### PushFont / PopFont

```cpp
void ImGui::PushFont(ImFont* font, float font_size_base_unscaled);
void ImGui::PopFont();
```

`PushFont(NULL, 0.0f)` — оставить текущий шрифт и размер. `font_size_base_unscaled` — базовый размер до глобальных
масштабов (не `GetFontSize()`).

---

### PushID / PopID

```cpp
void ImGui::PushID(const char* str_id);
void ImGui::PushID(const void* ptr_id);
void ImGui::PushID(int int_id);
void ImGui::PopID();
```

Различает виджеты в циклах. Пример: `for (int i = 0; i < n; i++) { PushID(i); Button("Item"); PopID(); }`.
Альтернатива — `"Label##unique_id"` в label.

---

## Виджеты: текст

```cpp
void ImGui::Text(const char* fmt, ...);
void ImGui::TextUnformatted(const char* text, const char* text_end = NULL);
void ImGui::TextDisabled(const char* fmt, ...);
void ImGui::TextColored(const ImVec4& col, const char* fmt, ...);
void ImGui::TextWrapped(const char* fmt, ...);
void ImGui::LabelText(const char* label, const char* fmt, ...);
void ImGui::BulletText(const char* fmt, ...);
void ImGui::SeparatorText(const char* label);
```

---

## Виджеты: кнопки и выбор

```cpp
bool ImGui::Button(const char* label, const ImVec2& size = ImVec2(0,0));
bool ImGui::SmallButton(const char* label);
bool ImGui::Checkbox(const char* label, bool* v);
bool ImGui::RadioButton(const char* label, bool active);
bool ImGui::RadioButton(const char* label, int* v, int v_button);
bool ImGui::Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0);
```

---

## Виджеты: слайдеры и ввод

```cpp
bool ImGui::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");
bool ImGui::SliderInt(const char* label, int* v, int v_min, int v_max);
bool ImGui::DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0, float v_max = 0, const char* format = "%.3f");
bool ImGui::DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0);
bool ImGui::InputFloat(const char* label, float* v, float step = 0.0f, const char* format = "%.3f");
bool ImGui::InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
bool ImGui::InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0,0));
```

Для `InputText` с `std::string` — использовать [imgui_stdlib.h](../../external/imgui/misc/cpp/imgui_stdlib.h).

---

## Виджеты: Combo, TreeNode

```cpp
bool ImGui::BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0);
void ImGui::EndCombo();   // Только если BeginCombo вернул true

bool ImGui::Combo(const char* label, int* current_item, const char* const items[], int items_count);

bool ImGui::TreeNode(const char* label);
bool ImGui::TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags = 0);
void ImGui::TreePop();
bool ImGui::CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);
```

---

## Виджеты: изображения

```cpp
void ImGui::Image(ImTextureRef tex_ref, const ImVec2& image_size, const ImVec2& uv0 = ImVec2(0,0), const ImVec2& uv1 = ImVec2(1,1));
bool ImGui::ImageButton(const char* str_id, ImTextureRef tex_ref, const ImVec2& image_size, ...);
```

Для своих текстур в Vulkan: `ImGui_ImplVulkan_AddTexture(sampler, image_view, layout)` → `VkDescriptorSet`, затем
`ImTextureRef(descriptor_set)`.

---

## Виджеты: прочее

```cpp
void ImGui::ProgressBar(float fraction, const ImVec2& size = ImVec2(-FLT_MIN,0), const char* overlay = NULL);
void ImGui::Bullet();
bool ImGui::TextLink(const char* label);
```

---

## Таблицы

```cpp
bool ImGui::BeginTable(const char* str_id, int columns, ImGuiTableFlags flags = 0, const ImVec2& outer_size = ImVec2(0,0), float inner_width = 0.0f);
void ImGui::EndTable();   // Только если BeginTable вернул true

void ImGui::TableNextRow(ImGuiTableRowFlags row_flags = 0, float min_row_height = 0.0f);
bool ImGui::TableNextColumn();   // или TableSetColumnIndex(int column_n)
void ImGui::TableSetupColumn(const char* label, ImGuiTableColumnFlags flags = 0, float init_width_or_weight = 0.0f);
void ImGui::TableHeadersRow();
```

Пример:

```cpp
if (ImGui::BeginTable("table", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("A");
    ImGui::TableSetupColumn("B");
    ImGui::TableSetupColumn("C");
    ImGui::TableHeadersRow();
    for (int row = 0; row < 4; row++) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("A%d", row);
        ImGui::TableSetColumnIndex(1); ImGui::Text("B%d", row);
        ImGui::TableSetColumnIndex(2); ImGui::Text("C%d", row);
    }
    ImGui::EndTable();
}
```

---

## Меню

```cpp
bool ImGui::BeginMenuBar();   // В окне с ImGuiWindowFlags_MenuBar
void ImGui::EndMenuBar();     // Только если BeginMenuBar вернул true

bool ImGui::BeginMenu(const char* label, bool enabled = true);
void ImGui::EndMenu();        // Только если BeginMenu вернул true

bool ImGui::MenuItem(const char* label, const char* shortcut = NULL, bool selected = false, bool enabled = true);
```

`BeginMainMenuBar()` — полноэкранное меню вверху экрана. Паттерн:
`if (BeginMenuBar()) { if (BeginMenu("File")) { if (MenuItem("Open")) { ... } EndMenu(); } EndMenuBar(); }`.

---

## Tab bar

```cpp
bool ImGui::BeginTabBar(const char* str_id, ImGuiTabBarFlags flags = 0);
void ImGui::EndTabBar();      // Только если BeginTabBar вернул true

bool ImGui::BeginTabItem(const char* label, bool* p_open = NULL, ImGuiTabItemFlags flags = 0);
void ImGui::EndTabItem();     // Только если BeginTabItem вернул true
```

---

## Popups

```cpp
void ImGui::OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags = 0);   // Открыть popup (не каждый кадр!)
bool ImGui::BeginPopup(const char* str_id, ImGuiWindowFlags flags = 0);
void ImGui::EndPopup();       // Только если BeginPopup вернул true

bool ImGui::BeginPopupModal(const char* name, bool* p_open = NULL, ImGuiWindowFlags flags = 0);
void ImGui::EndPopup();

bool ImGui::IsPopupOpen(const char* str_id, ImGuiPopupFlags flags = 0);
void ImGui::CloseCurrentPopup();
```

`BeginPopupContextItem()` — открыть при клике правой кнопкой на последний виджет. `BeginPopupContextWindow()` — при
клике в пустую область окна.

---

## Drag-and-drop

```cpp
// Источник (элемент, который перетаскивают)
bool ImGui::BeginDragDropSource(ImGuiDragDropFlags flags = 0);
bool ImGui::SetDragDropPayload(const char* type, const void* data, size_t sz, ImGuiCond cond = 0);
void ImGui::EndDragDropSource();   // Только если BeginDragDropSource вернул true

// Приёмник (элемент, на который бросают)
bool ImGui::BeginDragDropTarget();
const ImGuiPayload* ImGui::AcceptDragDropPayload(const char* type, ImGuiDragDropFlags flags = 0);
void ImGui::EndDragDropTarget();   // Только если BeginDragDropTarget вернул true

const ImGuiPayload* ImGui::GetDragDropPayload();   // Текущий payload (из любой точки)
```

**ImGuiPayload:** поля `Data`, `DataSize`, `IsDataType("type")`, `Preview`, `Delivery`. `type` — строка до 32 символов;
строки с `_` зарезервированы.

Пример:

```cpp
if (ImGui::Selectable("Item")) { /* ... */ }
if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload("MY_TYPE", &item_id, sizeof(item_id));
    ImGui::Text("Dragging...");
    ImGui::EndDragDropSource();
}
if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MY_TYPE")) {
        int id = *(const int*)p->Data;
        // Обработать drop
    }
    ImGui::EndDragDropTarget();
}
```

---

## ImGuiListClipper

Оптимизация для больших списков: рисуются только видимые элементы.

```cpp
struct ImGuiListClipper {
    void Begin(int items_count, float items_height = -1.0f);
    void End();   // Вызывается автоматически при последнем Step() == false
    bool Step();  // Возвращает true пока есть блоки; false — конец, End() вызван
    
    int DisplayStart;   // Индекс первого видимого элемента в текущем блоке
    int DisplayEnd;     // Индекс за последним видимым
    int ItemsCount;
};
```

Пример:

```cpp
ImGuiListClipper clipper;
clipper.Begin(10000);   // 10000 элементов
while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        ImGui::Text("Item %d", i);
    }
}
```

---

## Структуры

### ImGuiIO

Главная структура ввода-вывода. Ключевые поля:

| Поле                       | Описание                                    |
|----------------------------|---------------------------------------------|
| `DeltaTime`                | Время с прошлого кадра (сек)                |
| `DisplaySize`              | Размер окна в пикселях                      |
| `DisplayFramebufferScale`  | Масштаб (1,1) или HiDPI                     |
| `ConfigFlags`              | ImGuiConfigFlags                            |
| `BackendFlags`             | ImGuiBackendFlags (устанавливается backend) |
| `WantCaptureMouse`         | true — ImGui владеет мышью                  |
| `WantCaptureKeyboard`      | true — ImGui владеет клавиатурой            |
| `WantTextInput`            | true — ожидается ввод текста (IME)          |
| `Fonts`                    | ImFontAtlas*                                |
| `FontDefault`              | Шрифт по умолчанию (NULL = Fonts[0])        |
| `Framerate`                | Оценка FPS (только для отображения)         |
| `IniFilename`              | Путь к .ini (NULL — отключить)              |
| `UserData`                 | Пользовательские данные                     |
| `NavActive`                | Навигация с клавиатуры/геймпада активна     |
| `NavVisible`               | Курсор навигации виден                      |
| `ConfigNavMoveSetMousePos` | Навигация перемещает мышь                   |
| `ConfigNavCaptureKeyboard` | NavActive → WantCaptureKeyboard             |

---

### ImGuiStyle

Стили. Ключевые поля и методы:

| Поле / метод                      | Описание                            |
|-----------------------------------|-------------------------------------|
| `WindowPadding`                   | Отступ окна                         |
| `FramePadding`                    | Отступ рамки виджета                |
| `ItemSpacing`                     | Расстояние между виджетами          |
| `WindowRounding`, `FrameRounding` | Скругление углов                    |
| `Colors[ImGuiCol_*]`              | Массив цветов                       |
| `FontSizeBase`                    | Базовый размер шрифта               |
| `FontScaleDpi`                    | Масштаб шрифта для DPI              |
| `ScaleAllSizes(scale)`            | Масштабировать отступы и скругления |

---

### ImDrawData

Результат `Render()`. `CmdLists`, `DisplaySize`, `DisplayPos`, `FramebufferScale`. `Textures` — массив текстур для
обновления (1.92+).

---

## Backend SDL3 (Platform)

Файлы: [imgui_impl_sdl3.h](../../external/imgui/backends/imgui_impl_sdl3.h), [imgui_impl_sdl3.cpp](../../external/imgui/backends/imgui_impl_sdl3.cpp).

| Функция                                                | Описание                                   |
|--------------------------------------------------------|--------------------------------------------|
| `ImGui_ImplSDL3_InitForVulkan(window)`                 | Vulkan                                     |
| `ImGui_ImplSDL3_InitForOpenGL(window, gl_context)`     | OpenGL                                     |
| `ImGui_ImplSDL3_InitForMetal(window)`                  | Metal                                      |
| `ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)`  | SDL_Renderer                               |
| `ImGui_ImplSDL3_InitForSDLGPU(window)`                 | SDL_GPU                                    |
| `ImGui_ImplSDL3_InitForOther(window)`                  | Без рендера (только ввод)                  |
| `ImGui_ImplSDL3_Shutdown()`                            | Shutdown                                   |
| `ImGui_ImplSDL3_NewFrame()`                            | Каждый кадр перед `ImGui::NewFrame()`      |
| `ImGui_ImplSDL3_ProcessEvent(event)`                   | При каждом событии SDL                     |
| `ImGui_ImplSDL3_SetGamepadMode(mode, gamepads, count)` | Режим геймпада: AutoFirst, AutoAll, Manual |

---

## Backend Vulkan (Renderer)

Файлы: [imgui_impl_vulkan.h](../../external/imgui/backends/imgui_impl_vulkan.h), [imgui_impl_vulkan.cpp](../../external/imgui/backends/imgui_impl_vulkan.cpp).

### ImGui_ImplVulkan_PipelineInfo

| Поле                          | Описание                                  |
|-------------------------------|-------------------------------------------|
| `RenderPass`                  | Игнорируется при dynamic rendering        |
| `Subpass`                     | Индекс subpass                            |
| `MSAASamples`                 | VK_SAMPLE_COUNT_1_BIT по умолчанию        |
| `PipelineRenderingCreateInfo` | Для Vulkan 1.3 / VK_KHR_dynamic_rendering |

### ImGui_ImplVulkan_InitInfo

Структура (обнулить перед заполнением):

| Поле                                                       | Описание                                                      |
|------------------------------------------------------------|---------------------------------------------------------------|
| `ApiVersion`                                               | 0 = заголовок по умолчанию                                    |
| `Instance`, `PhysicalDevice`, `Device`                     | Vulkan handles                                                |
| `QueueFamily`, `Queue`                                     | Очередь                                                       |
| `DescriptorPool`                                           | Внешний pool (игнорируется если DescriptorPoolSize > 0)       |
| `DescriptorPoolSize`                                       | > 0 — backend создаёт pool сам                                |
| `MinImageCount`, `ImageCount`                              | >= 2                                                          |
| `PipelineCache`                                            | Опционально                                                   |
| `PipelineInfoMain`                                         | RenderPass, Subpass, MSAASamples, PipelineRenderingCreateInfo |
| `UseDynamicRendering`                                      | true для VK_KHR_dynamic_rendering                             |
| `Allocator`                                                | VkAllocationCallbacks                                         |
| `CheckVkResultFn`                                          | Callback для отладки                                          |
| `MinAllocationSize`                                        | Для validation (например, 1024*1024)                          |
| `CustomShaderVertCreateInfo`, `CustomShaderFragCreateInfo` | Кастомные шейдеры                                             |

### Функции

| Функция                                                     | Описание                                               |
|-------------------------------------------------------------|--------------------------------------------------------|
| `ImGui_ImplVulkan_Init(info)`                               | Инициализация                                          |
| `ImGui_ImplVulkan_Shutdown()`                               | Shutdown                                               |
| `ImGui_ImplVulkan_NewFrame()`                               | Каждый кадр перед SDL3 NewFrame                        |
| `ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, pipeline)` | Внутри render pass                                     |
| `ImGui_ImplVulkan_SetMinImageCount(count)`                  | При пересоздании swapchain                             |
| `ImGui_ImplVulkan_AddTexture(sampler, image_view, layout)`  | Своя текстура → VkDescriptorSet                        |
| `ImGui_ImplVulkan_RemoveTexture(descriptor_set)`            | Удалить текстуру                                       |
| `ImGui_ImplVulkan_CreateMainPipeline(info)`                 | Пересоздать pipeline (продвинутое)                     |
| `ImGui_ImplVulkan_UpdateTexture(tex)`                       | Обновить ImTextureData (продвинутое)                   |
| `ImGui_ImplVulkan_LoadFunctions(...)`                       | Кастомный loader (при IMGUI_IMPL_VULKAN_NO_PROTOTYPES) |

---

## imconfig.h — ключевые опции

Файл: [imconfig.h](../../external/imgui/imconfig.h). Или свой файл: `#define IMGUI_USER_CONFIG "my_config.h"`.

| Опция                         | Описание                                             |
|-------------------------------|------------------------------------------------------|
| `IMGUI_DISABLE_DEMO_WINDOWS`  | Отключить ShowDemoWindow и др.                       |
| `IMGUI_DISABLE_DEBUG_TOOLS`   | Отключить Metrics, DebugLog, IDStackTool             |
| `IMGUI_DISABLE_DEFAULT_FONT`  | Без встроенных шрифтов; AddFontDefault* будут assert |
| `IMGUI_DEFINE_MATH_OPERATORS` | Арифметика для ImVec2/ImVec4                         |
| `IMGUI_ENABLE_FREETYPE`       | FreeType вместо stb_truetype (misc/freetype)         |
| `ImDrawIdx unsigned int`      | 32-bit индексы для >64K вершин                       |
| `IMGUI_USER_CONFIG "path"`    | Путь к кастомному конфигу                            |
| `IMGUI_IMPL_VULKAN_USE_VOLK`  | Vulkan через volk (в imconfig)                       |

---

## Макросы

| Макрос                 | Описание                              |
|------------------------|---------------------------------------|
| `IMGUI_VERSION`        | Строка ("1.92.6 WIP")                 |
| `IMGUI_VERSION_NUM`    | Число (19259)                         |
| `IMGUI_CHECKVERSION()` | Проверка структур. Вызывать в начале. |
| `IM_COUNTOF(arr)`      | Размер статического массива           |
| `IM_ASSERT(expr)`      | Assert (переопределить в imconfig.h)  |

← [На главную imgui](README.md) | [На главную документации](../README.md)
