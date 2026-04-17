# Интеграция Dear ImGui

🟡 **Уровень 2: Средний**

## Оглавление

- [1. CMake](#1-cmake)
- [2. Конфигурация (imconfig.h)](#2-конфигурация-imconfigh)
- [3. Порядок инициализации](#3-порядок-инициализации)
- [4. Шрифты и DPI](#4-шрифты-и-dpi)
- [5. Descriptor Pool для Vulkan](#5-descriptor-pool-для-vulkan)
- [6. Обработка изменения размера окна](#6-обработка-изменения-размера-окна)
- [7. Несколько контекстов](#7-несколько-контекстов)

---

## 1. CMake

ImGui обычно собирается как статическая библиотека из исходников.

### Базовая конфигурация

```cmake
set(IMGUI_DIR "external/imgui")
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Vulkan)

target_link_libraries(YourApp PRIVATE imgui)
```

### Дополнительные файлы

Для расширенной функциональности добавьте:

```cmake
set(IMGUI_SOURCES
    # ... базовые файлы ...
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp  # InputText с std::string
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp  # FreeType рендеринг шрифтов
)
```

### Опции компиляции

```cmake
# Отключить demo windows в релизе
target_compile_definitions(imgui PRIVATE IMGUI_DISABLE_DEMO_WINDOWS)

# Включить арифметические операторы для ImVec2/ImVec4
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
```

---

## 2. Конфигурация (imconfig.h)

Настройка через макросы препроцессора в файле `imconfig.h` или через CMake.

### Ключевые опции

| Опция                         | Описание                                 |
|-------------------------------|------------------------------------------|
| `IMGUI_DISABLE_DEMO_WINDOWS`  | Отключить ShowDemoWindow и др.           |
| `IMGUI_DISABLE_DEBUG_TOOLS`   | Отключить Metrics, DebugLog, IDStackTool |
| `IMGUI_DISABLE_DEFAULT_FONT`  | Без встроенных шрифтов                   |
| `IMGUI_DEFINE_MATH_OPERATORS` | Арифметика для ImVec2/ImVec4             |
| `IMGUI_ENABLE_FREETYPE`       | FreeType вместо stb_truetype             |

### Кастомный конфиг

```cpp
// В своём файле my_imgui_config.h
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_USER_CONFIG "my_imgui_config.h"
```

Или через CMake:

```cmake
target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="<path/to/my_config.h>")
```

### Переопределение аллокатора

```cpp
// До CreateContext()
ImGui::SetAllocatorFunctions(
    [](size_t size, void* user_data) { return my_alloc(size); },
    [](void* ptr, void* user_data) { my_free(ptr); },
    nullptr
);
```

---

## 3. Порядок инициализации

### Инициализация

```cpp
bool initImGui(SDL_Window* window, VulkanContext& vk) {
    // 1. Проверка версии
    IMGUI_CHECKVERSION();

    // 2. Создание контекста
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // 3. Конфигурация
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Опционально

    // 4. Стиль
    ImGui::StyleColorsDark();

    // 5. Platform backend
    if (!ImGui_ImplSDL3_InitForVulkan(window)) {
        return false;
    }

    // 6. Renderer backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vk.instance;
    init_info.PhysicalDevice = vk.physicalDevice;
    init_info.Device = vk.device;
    init_info.QueueFamily = vk.queueFamily;
    init_info.Queue = vk.queue;
    init_info.DescriptorPool = vk.descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = vk.swapchainImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        return false;
    }

    // 7. Загрузка шрифтов (если нужны кастомные)
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);

    return true;
}
```

### Shutdown

```cpp
void shutdownImGui() {
    // Обратный порядок инициализации
    vkDeviceWaitIdle(device);  // Важно для Vulkan!

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
```

---

## 4. Шрифты и DPI

### Загрузка шрифтов

```cpp
ImGuiIO& io = ImGui::GetIO();

// Шрифт по умолчанию
io.Fonts->AddFontDefault();

// Из файла
io.Fonts->AddFontFromFileTTF("path/to/font.ttf", 16.0f);

// Из памяти
io.Fonts->AddFontFromMemoryTTF(font_data, font_size, 16.0f);

// Несколько шрифтов
ImFont* font_large = io.Fonts->AddFontFromFileTTF("font.ttf", 24.0f);
ImFont* font_small = io.Fonts->AddFontFromFileTTF("font.ttf", 12.0f);
```

### Использование шрифтов

```cpp
ImGui::PushFont(font_large);
ImGui::Text("Large text");
ImGui::PopFont();
```

### HiDPI масштабирование

```cpp
// Получить DPI scale (SDL3)
float scale = SDL_GetDisplayContentScale(display_id);

// Масштабировать стили
ImGui::GetStyle().ScaleAllSizes(scale);

// Масштабировать шрифт
io.FontGlobalScale = scale;
// Или загрузить шрифт с учётом масштаба
io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f * scale);
```

---

## 5. Descriptor Pool для Vulkan

Vulkan backend требует `VkDescriptorPool` для текстур шрифтов и пользовательских изображений.

### Создание Descriptor Pool

```cpp
VkDescriptorPool createDescriptorPool(VkDevice device) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // Обязательно!
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool pool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
    return pool;
}
```

### Альтернатива: авто-создание pool

```cpp
// ImGui сам создаст pool если DescriptorPoolSize > 0
ImGui_ImplVulkan_InitInfo init_info = {};
// ...
init_info.DescriptorPool = VK_NULL_HANDLE;  // Не используется
init_info.DescriptorPoolSize = 1000;         // Backend создаст pool сам
```

---

## 6. Обработка изменения размера окна

При изменении размера swapchain:

```cpp
void onSwapchainRecreated(uint32_t new_image_count) {
    // Уведомить ImGui о новом количестве изображений
    ImGui_ImplVulkan_SetMinImageCount(new_image_count);
}
```

### Полный пример пересоздания

```cpp
void handleResize(VulkanContext& vk) {
    vkDeviceWaitIdle(vk.device);

    // Пересоздание swapchain
    destroySwapchain(vk);
    createSwapchain(vk);

    // Уведомление ImGui
    ImGui_ImplVulkan_SetMinImageCount(vk.minImageCount);
}
```

---

## 7. Несколько контекстов

Обычно один контекст на приложение, но можно создать несколько:

```cpp
// Создание
ImGuiContext* ctx1 = ImGui::CreateContext();
ImGuiContext* ctx2 = ImGui::CreateContext();

// Переключение
ImGui::SetCurrentContext(ctx1);
ImGui::Begin("Window in Context 1");
ImGui::End();

ImGui::SetCurrentContext(ctx2);
ImGui::Begin("Window in Context 2");
ImGui::End();

// Уничтожение
ImGui::DestroyContext(ctx1);
ImGui::DestroyContext(ctx2);
```

### Использование с несколькими окнами

```cpp
// Каждое окно может иметь свой контекст
struct WindowUI {
    SDL_Window* window;
    ImGuiContext* context;
};

void renderWindowUI(WindowUI& ui) {
    ImGui::SetCurrentContext(ui.context);
    // ... рендеринг UI для этого окна
}
```
