# Продвинутые оптимизации ImGui для ProjectV

## Data-Oriented Design

### SoA для виджетов

```cpp
template<size_t Alignment = 64>
struct WidgetBatchSoA {
    alignas(Alignment) std::vector<glm::vec2> positions;
    alignas(Alignment) std::vector<glm::vec2> sizes;
    alignas(Alignment) std::vector<uint32_t> colors;
    alignas(Alignment) std::vector<bool> states;
    alignas(Alignment) std::vector<uint32_t> ids;

    void add(glm::vec2 pos, glm::vec2 size, uint32_t color, bool state) {
        positions.push_back(pos);
        sizes.push_back(size);
        colors.push_back(color);
        states.push_back(state);
    }
};
```

### ImGui Vulkan Uploader (Production-Ready Interface)

Класс для передачи данных ImGui в Vulkan буферы с поддержкой SoA и zero-copy:

```cpp
#pragma once

#include <imgui.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <array>
#include <cstdint>
#include <expected>

namespace ImGuiVulkan {

// SoA структура для batched vertices
struct VertexBufferSoA {
    std::vector<ImVec2> positions;      // SoA: все позиции
    std::vector<ImVec2> uvs;             // SoA: все UV
    std::vector<uint32_t> colors;         // SoA: все цвета (ABGR packed)

    void reserve(size_t count) {
        positions.reserve(count);
        uvs.reserve(count);
        colors.reserve(count);
    }

    void clear() {
        positions.clear();
        uvs.clear();
        colors.clear();
    }
};

// SoA структура для индексов
struct IndexBufferSoA {
    std::vector<ImDrawIdx> indices;

    void reserve(size_t count) {
        indices.reserve(count);
    }

    void clear() {
        indices.clear();
    }
};

// Результат upload операции
struct UploadResult {
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexMemory;
    VkDeviceMemory indexMemory;
    uint32_t vertexCount;
    uint32_t indexCount;
};

// Конфигурация для Vulkan ресурсов
struct UploadConfig {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    VkDeviceSize stagingBufferSize = 16 * 1024 * 1024; // 16MB default
    bool useDedicatedAllocation = true;
};

// Основной класс для upload данных ImGui
class ImGuiVulkanUploader {
public:
    // Конструктор с конфигурацией
    explicit ImGuiVulkanUploader(const UploadConfig& config);

    // Деструктор - освобождает все ресурсы
    ~ImGuiVulkanUploader();

    // Удалить копирующие операции
    ImGuiVulkanUploader(const ImGuiVulkanUploader&) = delete;
    ImGuiVulkanUploader& operator=(const ImGuiVulkanUploader&) = delete;
    ImGuiVulkanUploader(ImGuiVulkanUploader&&) noexcept;
    ImGuiVulkanUploader& operator=(ImGuiVulkanUploader&&) noexcept;

    // Добавить quad в текущий batch
    void addQuad(
        const ImVec2& pos,
        const ImVec2& size,
        uint32_t color,        // ABGR packed
        const ImVec2& uvMin,   // UV левого верхнего
        const ImVec2& uvMax    // UV правого нижнего
    );

    // Добавить quad из ImDrawCmd
    void addQuadFromDrawCmd(
        const ImDrawCmd* cmd,
        const ImVec2& clipOffset,
        const ImVec2& clipScale
    );

    // Начать новый batch
    void beginBatch();

    // Завершить batch и загрузить в GPU
    // Возвращает результат с буферами или ошибку
    std::expected<UploadResult, std::string> uploadToGPU(VkCommandBuffer cmd);

    // Освободить GPU ресурсы предыдущего upload
    void freePreviousBuffers();

    // Получить текущий vertex count
    [[nodiscard]] uint32_t vertexCount() const noexcept {
        return static_cast<uint32Idx>(vertexBuffer_.positions.size());
    }

    // Получить текущий index count
    [[nodiscard]] uint32_t indexCount() const noexcept {
        return static_cast<uint32_t>(vertexBuffer_.indices.size());
    }

private:
    UploadConfig config_;

    // SoA buffers
    VertexBufferSoA vertexBuffer_;
    IndexBufferSoA indexBuffer_;

    // GPU ресурсы предыдущего upload (нужно освободить после использования)
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    UploadResult previousResult_{};

    // Создать staging буфер
    std::expected<void, std::string> createStagingBuffer(VkDeviceSize size);

    // Освободить staging буфер
    void destroyStagingBuffer();

    // Аллоцировать GPU буферы через VMA-подобный аллокатор
    std::expected<UploadResult, std::string> allocateAndCopyBuffers(
        VkCommandBuffer cmd,
        VkBuffer srcBuffer,
        VkDeviceSize srcSize
    );
};

// Реализация: добавление quad
void ImGuiVulkanUploader::addQuad(
    const ImVec2& pos,
    const ImVec2& size,
    uint32_t color,
    const ImVec2& uvMin,
    const ImVec2& uvMax
) {
    const uint32_t baseVertex = static_cast<uint32_t>(vertexBuffer_.positions.size());

    // Добавляем 4 вершины (SoA layout)
    // Vertex 0: top-left
    vertexBuffer_.positions.push_back(ImVec2(pos.x, pos.y));
    vertexBuffer_.uvs.push_back(ImVec2(uvMin.x, uvMin.y));
    vertexBuffer_.colors.push_back(color);

    // Vertex 1: top-right
    vertexBuffer_.positions.push_back(ImVec2(pos.x + size.x, pos.y));
    vertexBuffer_.uvs.push_back(ImVec2(uvMax.x, uvMin.y));
    vertexBuffer_.colors.push_back(color);

    // Vertex 2: bottom-left
    vertexBuffer_.positions.push_back(ImVec2(pos.x, pos.y + size.y));
    vertexBuffer_.uvs.push_back(ImVec2(uvMin.x, uvMax.y));
    vertexBuffer_.colors.push_back(color);

    // Vertex 3: bottom-right
    vertexBuffer_.positions.push_back(ImVec2(pos.x + size.x, pos.y + size.y));
    vertexBuffer_.uvs.push_back(ImVec2(uvMax.x, uvMax.y));
    vertexBuffer_.colors.push_back(color);

    // Добавляем 6 индексов (2 треугольника)
    // Triangle 0: 0-1-2
    vertexBuffer_.indices.push_back(baseVertex + 0);
    vertexBuffer_.indices.push_back(baseVertex + 1);
    vertexBuffer_.indices.push_back(baseVertex + 2);

    // Triangle 1: 1-3-2
    vertexBuffer_.indices.push_back(baseVertex + 1);
    vertexBuffer_.indices.push_back(baseVertex + 3);
    vertexBuffer_.indices.push_back(baseVertex + 2);
}

// Реализация: upload в GPU
std::expected<UploadResult, std::string> ImGuiVulkanUploader::uploadToGPU(
    VkCommandBuffer cmd
) {
    if (vertexBuffer_.positions.empty()) {
        return std::unexpected("No vertices to upload");
    }

    // Вычисляем размеры данных
    const size_t vertexDataSize =
        vertexBuffer_.positions.size() * sizeof(ImVec2) +
        vertexBuffer_.uvs.size() * sizeof(ImVec2) +
        vertexBuffer_.colors.size() * sizeof(uint32_t);

    const size_t indexDataSize =
        vertexBuffer_.indices.size() * sizeof(ImDrawIdx);

    // Конвертируем SoA в interleaved (для Vulkan vertex binding)
    std::vector<ImDrawVert> interleavedVerts(vertexBuffer_.positions.size());
    for (size_t i = 0; i < vertexBuffer_.positions.size(); ++i) {
        interleavedVerts[i].pos = vertexBuffer_.positions[i];
        interleavedVerts[i].uv = vertexBuffer_.uvs[i];
        interleavedVerts[i].col = vertexBuffer_.colors[i];
    }

    // Создаём staging буфер если нужно
    const VkDeviceSize totalSize = vertexDataSize + indexDataSize;
    if (stagingBuffer_ == VK_NULL_HANDLE) {
        auto result = createStagingBuffer(totalSize);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    // Копируем данные в staging буфер
    void* mappedData = nullptr;
    auto mapResult = vkMapMemory(
        config_.device,
        stagingMemory_,
        0,
        totalSize,
        0,
        &mappedData
    );

    if (mapResult != VK_SUCCESS) {
        return std::unexpected("Failed to map staging memory");
    }

    // Копируем vertex данные
    std::memcpy(mappedData, interleavedVerts.data(), vertexDataSize);
    // Копируем index данные
    std::memcpy(
        static_cast<std::byte*>(mappedData) + vertexDataSize,
        vertexBuffer_.indices.data(),
        indexDataSize
    );

    VkMappedMemoryRange flushRange{};
    flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    flushRange.memory = stagingMemory_;
    flushRange.size = totalSize;
    vkFlushMappedMemoryRanges(config_.device, 1, &flushRange);

    vkUnmapMemory(config_.device, stagingMemory_);

    // Аллоцируем device local буферы и копируем
    auto gpuResult = allocateAndCopyBuffers(cmd, stagingBuffer_, totalSize);
    if (!gpuResult) {
        return std::unexpected(gpuResult.error());
    }

    // Возвращаем результат с перемещением
    previousResult_ = std::move(*gpuResult);

    return previousResult_;
}

} // namespace ImGuiVulkan
```

## Интеграция с Tracy

```cpp
#include <tracy/Tracy.hpp>

void renderImGui() {
    ZoneScoped;

    {
        ZoneScopedN("NewFrame");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    {
        ZoneScopedN("Widgets");
        // Виджеты
    }

    {
        ZoneScopedN("Render");
        ImGui::Render();
    }

    {
        ZoneScopedN("RenderDrawData");
        ImDrawData* drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
    }
}
```

## Zero-allocation паттерны

### Пул для текста

```cpp
class TextBufferPool {
public:
    static constexpr size_t BUFFER_SIZE = 256;
    static constexpr size_t POOL_SIZE = 64;

    std::array<std::array<char, BUFFER_SIZE>, POOL_SIZE> buffers_;
    std::bitset<POOL_SIZE> used_;
    size_t current_ = 0;

    char* allocate() {
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            size_t idx = (current_ + i) % POOL_SIZE;
            if (!used_[idx]) {
                used_[idx] = true;
                current_ = idx + 1;
                return buffers_[idx].data();
            }
        }
        return nullptr;  // Пул переполнен
    }

    void deallocate(char* ptr) {
        size_t idx = (ptr - buffers_[0].data()) / BUFFER_SIZE;
        if (idx < POOL_SIZE) {
            used_[idx] = false;
        }
    }
};
```

## Рекомендации для ProjectV

1. **WantCapture** — всегда проверяй перед обработкой ввода
2. **Descriptor pool** — создай достаточно большой пул
3. **Batch rendering** — ImGui делает это автоматически
4. **Tracy** — профилируй этапы рендеринга отдельно
