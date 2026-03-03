# Быстрый старт Dear ImGui

**🟢 Уровень 1: Начинающий**

Предполагается, что SDL3 и Vulkan уже инициализированы.

## Шаг 1: CMake

Подключение библиотеки:

```cmake
add_subdirectory(external/imgui)
add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE imgui)
```

(Предполагается, что в `external/imgui/CMakeLists.txt` уже настроена сборка с бекендами SDL3 и Vulkan, либо вы
добавляете их файлы вручную).

Пример ручной сборки:

```cmake
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Vulkan)
```

## Шаг 2: Инициализация

```cpp
// 1. Создание контекста
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

// 2. Инициализация Platform (SDL3)
ImGui_ImplSDL3_InitForVulkan(window);

// 3. Инициализация Renderer (Vulkan)
ImGui_ImplVulkan_InitInfo init_info = {};
init_info.Instance = instance;
init_info.PhysicalDevice = physical_device;
init_info.Device = device;
init_info.QueueFamily = queue_family;
init_info.Queue = queue;
init_info.DescriptorPool = descriptor_pool;
init_info.MinImageCount = 2;
init_info.ImageCount = swapchain_image_count;
init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

ImGui_ImplVulkan_Init(&init_info);
```

## Шаг 3: Цикл кадра

```cpp
// 1. Обработка событий
SDL_Event event;
while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT) running = false;
}

// 2. Начало кадра
ImGui_ImplVulkan_NewFrame();
ImGui_ImplSDL3_NewFrame();
ImGui::NewFrame();

// 3. Описание интерфейса
ImGui::Begin("Hello");
ImGui::Text("Hello, World!");
if (ImGui::Button("Close")) running = false;
ImGui::End();

// 4. Рендеринг
ImGui::Render();
// ... Начало Render Pass ...
ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
// ... Конец Render Pass ...
```

## Шаг 4: Очистка

```cpp
vkDeviceWaitIdle(device);
ImGui_ImplVulkan_Shutdown();
ImGui_ImplSDL3_Shutdown();
ImGui::DestroyContext();
```
