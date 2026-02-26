# Интеграция ImGui с ProjectV

## CMake

```cmake
# CMakeLists.txt
add_library(imgui STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_link_libraries(imgui PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)
```

## Инициализация

```cpp
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

ImGuiContext* imguiInit(VkInstance instance, VkPhysicalDevice physicalDevice,
                        VkDevice device, uint32_t queueFamily, VkQueue queue,
                        SDL_Window* window) {
    // Создание контекста
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Инициализация Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.DescriptorPool = descriptorPool;  // Создай заранее
    initInfo.RenderPass = renderPass;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    // Инициализация SDL3 backend
    ImGui_ImplSDL3_InitForVulkan(window);

    return ImGui::CurrentContext;
}
```

## Интеграция с Vulkan

### Создание Descriptor Pool

```cpp
VkDescriptorPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 1000,
    .poolSizeCount = 1,
    .pPoolSizes = &VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1000
    }
};
vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
```

### Рендеринг в очереди

```cpp
void imguiRender(VkCommandBuffer cmdBuffer) {
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
}
```

## Интеграция с Flecs ECS

```cpp
#include <flecs.h>

struct DebugWindow {
    bool visible = true;
};

class ImGuiSystem {
public:
    ImGuiSystem(flecs::world& world) : world_(world) {
        // Система для debug окон
        world_.system<DebugWindow>("DebugWindows")
            .kind(flecs::OnUpdate)
            .each([this](flecs::entity e, DebugWindow& dw) {
                if (!dw.visible) return;

                ImGui::Begin("Debug");
                ImGui::Text("Entity: %s", e.name().c_str());

                if (ImGui::Button("Toggle")) {
                    dw.visible = false;
                }

                ImGui::End();
            });
    }

private:
    flecs::world& world_;
};
```

## Интеграция с SDL3

```cpp
void handleSdlEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
        // ImGui обрабатывает событие
    } else {
        // Обработка в игре
    }
}
```

## Очистка

```cpp
void imguiShutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
```
