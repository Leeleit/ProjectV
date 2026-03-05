# Интеграция Dear ImGui

**🟡 Уровень 2: Средний**

## Оглавление

- [1. CMake](#1-cmake)
- [2. Конфигурация (imconfig.h)](#2-конфигурация-imconfigh)
- [3. Порядок инициализации](#3-порядок-инициализации)
- [4. Шрифты и DPI](#4-шрифты-и-dpi)
- [5. Descriptor Pool](#5-descriptor-pool)

---

## 1. CMake

ImGui обычно собирается как статическая библиотека из исходников.

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

---

## 2. Конфигурация (imconfig.h)

Настройка через макросы препроцессора или файл `imconfig.h`.

### Интеграция с volk

Если используется `volk`, определите это перед включением заголовков ImGui (или в CMake):

```cmake
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
```

Это заставит backend использовать загруженные через volk функции вместо статической линковки.

---

## 3. Порядок инициализации

1. **Context**: `ImGui::CreateContext()`
2. **Platform**: `ImGui_ImplSDL3_InitForVulkan(window)`
3. **Renderer**: `ImGui_ImplVulkan_Init(&init_info)`

При изменении размера окна:

1. `ImGui_ImplVulkan_SetMinImageCount()`
2. Обновление swapchain.

---

## 4. Шрифты и DPI

Для поддержки HiDPI:

```cpp
float scale = SDL_GetDisplayContentScale(display_id);
ImGui::GetStyle().ScaleAllSizes(scale);
// Загрузка шрифта с учетом масштаба
io.Fonts->AddFontFromFileTTF("font.ttf", 16.0f * scale);
```

---

## 5. Descriptor Pool

Vulkan backend требует `VkDescriptorPool` для текстур шрифтов и изображений.
Флаг: `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`.
Тип: `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`.

```cpp
VkDescriptorPoolSize pool_sizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
};
// ... создание pool
```
