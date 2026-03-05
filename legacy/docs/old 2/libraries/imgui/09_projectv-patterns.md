# Паттерны ImGui для ProjectV

🔴 **Уровень 3: Продвинутый**

Паттерны и оптимизации ImGui для воксельного движка ProjectV.

## Debug UI для воксельного движка

### Статистика чанков

```cpp
struct VoxelStats {
    int loadedChunks = 0;
    int totalChunks = 0;
    int visibleChunks = 0;
    size_t memoryUsed = 0;
    std::vector<ChunkInfo> chunks;
};

void renderVoxelStats(VoxelStats& stats) {
    if (ImGui::Begin("Voxel Statistics")) {
        // Основная статистика
        ImGui::Text("Loaded Chunks: %d / %d", stats.loadedChunks, stats.totalChunks);
        ImGui::ProgressBar(static_cast<float>(stats.loadedChunks) / stats.totalChunks);

        ImGui::Text("Visible: %d", stats.visibleChunks);
        ImGui::Text("Memory: %.2f MB", stats.memoryUsed / (1024.0f * 1024.0f));

        // Детали чанков
        if (ImGui::CollapsingHeader("Chunk Details")) {
            if (ImGui::BeginTable("chunks", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Position");
                ImGui::TableSetupColumn("State");
                ImGui::TableSetupColumn("Memory");
                ImGui::TableSetupColumn("LOD");
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(stats.chunks.size()));

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto& chunk = stats.chunks[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("(%d,%d,%d)", chunk.x, chunk.y, chunk.z);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", chunkStateToString(chunk.state));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%.1f KB", chunk.memory / 1024.0f);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("LOD %d", chunk.lod);
                    }
                }
                ImGui::EndTable();
            }
        }

        // Настройки рендеринга
        if (ImGui::CollapsingHeader("Render Settings")) {
            static bool wireframe = false;
            static float lodBias = 1.0f;
            static int maxDistance = 1000;

            ImGui::Checkbox("Wireframe", &wireframe);
            ImGui::SliderFloat("LOD Bias", &lodBias, 0.1f, 4.0f);
            ImGui::SliderInt("Max Distance", &maxDistance, 100, 5000);
        }
    }
    ImGui::End();
}
```

### Настройки рендеринга в реальном времени

```cpp
struct RenderSettings {
    bool wireframe = false;
    bool showBoundingBoxes = false;
    float lodBias = 1.0f;
    int maxDistance = 1000;
    float fogDensity = 0.01f;
    float fogColor[3] = {0.5f, 0.6f, 0.7f};
};

void renderSettingsUI(RenderSettings& settings) {
    if (ImGui::Begin("Render Settings")) {
        if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Wireframe", &settings.wireframe);
            ImGui::Checkbox("Bounding Boxes", &settings.showBoundingBoxes);
            ImGui::SliderFloat("LOD Bias", &settings.lodBias, 0.1f, 4.0f);
            ImGui::SliderInt("Max Distance", &settings.maxDistance, 100, 5000);
        }

        if (ImGui::CollapsingHeader("Atmosphere")) {
            ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.1f);
            ImGui::ColorEdit3("Fog Color", settings.fogColor);
        }
    }
    ImGui::End();
}
```

---

## Интеграция с flecs ECS

### Инспектор сущностей

```cpp
void renderECSInspector(flecs::world& world) {
    if (ImGui::Begin("ECS Inspector")) {
        // Фильтр
        static char filter[128] = "";
        ImGui::InputText("Filter", filter, sizeof(filter));

        // Список сущностей
        ImGui::BeginChild("entities", ImVec2(0, 200));
        world.each([&](flecs::entity e) {
            const char* name = e.name().c_str();
            if (filter[0] && !strstr(name, filter)) return;

            if (ImGui::Selectable(name)) {
                // Выбрать сущность
            }
        });
        ImGui::EndChild();

        // Компоненты выбранной сущности
        // ...
    }
    ImGui::End();
}
```

### Создание сущностей через UI

```cpp
void renderEntityCreator(flecs::world& world) {
    if (ImGui::Begin("Entity Creator")) {
        static char name[128] = "NewEntity";

        ImGui::InputText("Name", name, sizeof(name));

        static bool addTransform = true;
        static bool addRenderable = true;
        static bool addPhysics = false;

        ImGui::Checkbox("Transform", &addTransform);
        ImGui::Checkbox("Renderable", &addRenderable);
        ImGui::Checkbox("Physics", &addPhysics);

        if (ImGui::Button("Create")) {
            auto entity = world.entity(name);

            if (addTransform) {
                entity.set<TransformComponent>({});
            }
            if (addRenderable) {
                entity.set<RenderableComponent>({});
            }
            if (addPhysics) {
                entity.set<PhysicsComponent>({});
            }
        }
    }
    ImGui::End();
}
```

---

## Интеграция с JoltPhysics

### Debug UI для физики

```cpp
void renderPhysicsDebugUI(JPH::PhysicsSystem& physicsSystem) {
    if (ImGui::Begin("Physics Debug")) {
        // Статистика
        ImGui::Text("Active Bodies: %d", physicsSystem.GetNumActiveBodies());
        ImGui::Text("Total Bodies: %d", physicsSystem.GetNumBodies());

        // Управление
        static bool paused = false;
        if (ImGui::Checkbox("Pause", &paused)) {
            physicsSystem.SetPaused(paused);
        }

        // Гравитация
        static float gravity[3] = {0.0f, -9.81f, 0.0f};
        if (ImGui::DragFloat3("Gravity", gravity, 0.1f)) {
            physicsSystem.SetGravity(JPH::Vec3(gravity[0], gravity[1], gravity[2]));
        }

        // Debug draw
        static int debugFlags = 0;
        ImGui::Text("Debug Draw:");
        ImGui::CheckboxFlags("Shapes", &debugFlags, 1);
        ImGui::CheckboxFlags("Bounds", &debugFlags, 2);
        ImGui::CheckboxFlags("Constraints", &debugFlags, 4);
    }
    ImGui::End();
}
```

---

## Профилирование с Tracy

### Профилирование UI рендеринга

```cpp
#include <tracy/Tracy.hpp>

void renderUIWithProfiling() {
    ZoneScopedN("RenderUI");

    {
        ZoneScopedN("ImGui NewFrame");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    {
        ZoneScopedN("Widget Rendering");
        renderVoxelStats();
        renderECSInspector();
        renderPhysicsDebugUI();
    }

    {
        ZoneScopedN("ImGui Render");
        ImGui::Render();
    }

    FrameMark;
}
```

### Профилирование Vulkan рендеринга UI

```cpp
void renderVulkanUI(VkCommandBuffer cmdBuffer) {
    TracyVkZone(GetTracyVkCtx(), cmdBuffer, "UI Render");

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

    TracyVkCollect(GetTracyVkCtx(), cmdBuffer);
}
```

### Отображение FPS в меню

```cpp
void renderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // ... меню ...

        // FPS справа
        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        ImGui::EndMainMenuBar();
    }
}
```

---

## Оптимизации для больших списков

### ImGuiListClipper для списков чанков

```cpp
void renderChunkList(const std::vector<ChunkInfo>& chunks) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(chunks.size()));

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const auto& chunk = chunks[i];

            ImGui::PushID(&chunk);
            ImGui::Text("(%d,%d,%d) - %s",
                       chunk.x, chunk.y, chunk.z,
                       chunkStateToString(chunk.state));
            ImGui::PopID();
        }
    }
}
```

### Оптимизация ID Stack

```cpp
// Неоптимально: PushID(int) в цикле 1000+ элементов
for (int i = 0; i < 1000; i++) {
    ImGui::PushID(i);
    // ...
    ImGui::PopID();
}

// Оптимально: PushID(const void*) для уникальных указателей
for (const auto& item : items) {
    ImGui::PushID(&item);  // Указатель как ID
    // ...
    ImGui::PopID();
}

// Оптимально: группировка
ImGui::PushID("list");
for (int i = 0; i < items.size(); i++) {
    ImGui::PushID(i);
    // ...
    ImGui::PopID();
}
ImGui::PopID();
```

### Кэширование строк

```cpp
// Неоптимально: форматирование каждый кадр
ImGui::Text("Position: (%.1f, %.1f, %.1f)", x, y, z);

// Оптимально: обновлять только при изменении
static std::string cachedPosition;
static glm::vec3 lastPosition;

if (position != lastPosition) {
    cachedPosition = fmt::format("Position: ({:.1f}, {:.1f}, {:.1f})", position.x, position.y, position.z);
    lastPosition = position;
}
ImGui::TextUnformatted(cachedPosition.c_str());
```

---

## Воксельный редактор

### Палитра блоков

```cpp
struct VoxelType {
    uint32_t id;
    std::string name;
    VkDescriptorSet previewTexture;
    uint32_t color;
};

void renderVoxelPalette(const std::vector<VoxelType>& types, uint32_t& selected) {
    if (ImGui::Begin("Voxel Palette")) {
        const float buttonSize = 64.0f;
        const int columns = 6;

        for (size_t i = 0; i < types.size(); i++) {
            const auto& type = types[i];
            ImGui::PushID(static_cast<int>(type.id));

            bool isSelected = (selected == type.id);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
            }

            if (ImGui::ImageButton("##btn",
                                   ImTextureRef(type.previewTexture),
                                   ImVec2(buttonSize, buttonSize))) {
                selected = type.id;
            }

            if (isSelected) {
                ImGui::PopStyleColor();
            }

            // Drag-and-drop
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("VOXEL_TYPE", &type.id, sizeof(type.id));
                ImGui::Text("Place %s", type.name.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();

            // Подпись
            ImGui::Text("%s", type.name.c_str());

            if ((i + 1) % columns != 0) {
                ImGui::SameLine();
            }
        }
    }
    ImGui::End();
}
```

### Инспектор свойств

```cpp
void renderVoxelInspector(const VoxelData& voxel) {
    if (ImGui::Begin("Voxel Inspector")) {
        ImGui::Text("Position: (%d, %d, %d)", voxel.x, voxel.y, voxel.z);
        ImGui::Text("Type: %s", voxel.typeName.c_str());

        ImGui::Separator();

        // Свойства
        ImGui::Text("Solid: %s", voxel.solid ? "Yes" : "No");
        ImGui::Text("Opaque: %s", voxel.opaque ? "Yes" : "No");

        // Цвет
        float color[3] = {
            voxel.r / 255.0f,
            voxel.g / 255.0f,
            voxel.b / 255.0f
        };
        if (ImGui::ColorEdit3("Color", color)) {
            voxel.r = static_cast<uint8_t>(color[0] * 255);
            voxel.g = static_cast<uint8_t>(color[1] * 255);
            voxel.b = static_cast<uint8_t>(color[2] * 255);
        }
    }
    ImGui::End();
}
```

---

## Игровой UI

### HUD

```cpp
void renderHUD(const PlayerState& player) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.5f));
    ImGui::Begin("HUD", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    // Здоровье
    ImGui::Text("Health");
    ImGui::ProgressBar(player.health / 100.0f, ImVec2(-1, 0));

    // Координаты
    ImGui::Text("Pos: (%.1f, %.1f, %.1f)",
               player.position.x,
               player.position.y,
               player.position.z);

    ImGui::End();
    ImGui::PopStyleColor();
}
```

### Инвентарь

```cpp
void renderInventory(const Inventory& inventory) {
    if (ImGui::Begin("Inventory")) {
        const int cols = 10;
        const float slotSize = 48.0f;

        if (ImGui::BeginTable("slots", cols)) {
            for (int i = 0; i < inventory.size; i++) {
                ImGui::TableNextColumn();

                ImGui::PushID(i);

                const auto& slot = inventory.slots[i];

                if (ImGui::Button("##slot", ImVec2(slotSize, slotSize))) {
                    // Выбрать слот
                }

                if (slot.count > 0) {
                    // Иконка предмета
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - slotSize + 4);
                    ImGui::Image(ImTextureRef(slot.icon), ImVec2(slotSize - 8, slotSize - 8));

                    // Количество
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 20);
                    ImGui::Text("%d", slot.count);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
```
