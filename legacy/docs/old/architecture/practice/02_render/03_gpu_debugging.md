# GPU Debugging: RenderDoc и NVIDIA Nsight

Отладка Vulkan шейдеров и GPU-кода.

---

## Проблема

Вы написали шейдер, но экран черный. `std::cout` на GPU не работает. Без инструментов отладки вы не сможете:

- Посмотреть содержимое буферов и текстур
- Отладить пайплайн Vulkan
- Найти ошибки в шейдерах
- Оптимизировать производительность GPU

---

## Инструменты

| Инструмент        | Назначение                        | Платформы                   |
|-------------------|-----------------------------------|-----------------------------|
| **RenderDoc**     | Захват кадров, инспекция ресурсов | Windows, Linux, Android     |
| **NVIDIA Nsight** | Продвинутая профилировка          | Windows, Linux (NVIDIA GPU) |
| **Tracy**         | CPU/GPU временные метки           | Кроссплатформенно           |

---

## Настройка RenderDoc

### Установка

1. Скачайте с [renderdoc.org](https://renderdoc.org/)
2. Установите и запустите RenderDoc UI

### Интеграция в код

```cpp
#ifdef RENDERDOC_ENABLED
#include <renderdoc_app.h>
RENDERDOC_API_1_6_0* rdoc_api = nullptr;

void init_renderdoc() {
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        if (RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api) == 1) {
            rdoc_api->SetCaptureFilePathTemplate("captures/projectv_");
        }
    }
}
#endif
```

### Захват кадра

**Горячие клавиши:**

- `F12` — захват следующего кадра
- `Print Screen` — захват текущего кадра

**Программный захват:**

```cpp
void capture_frame() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(nullptr, nullptr);
        // Рендеринг...
        rdoc_api->EndFrameCapture(nullptr, nullptr);
    }
}
```

---

## Анализ захваченного кадра

### 1. Event Browser

- 🟢 Зеленый — успешное выполнение
- 🟡 Желтый — предупреждения
- 🔴 Красный — ошибки

### 2. Pipeline State

- **Vertex Shader**: Входные атрибуты, uniform буферы
- **Fragment Shader**: Текстуры, samplers
- **Output Merger**: Blend state, depth/stencil

### 3. Texture Viewer

- Mip Level: Уровни детализации
- Channel: R, G, B, A, RGB
- Zoom: Пиксельный анализ

### 4. Mesh Viewer

- Vertex Positions: Корректность координат
- Normals/UVs: Корректность данных
- Primitive Topology: Тип примитивов

---

## Распространённые проблемы

### Проблема 1: Черный экран

**Диагностика:**

1. Проверьте Event Browser на ошибки
2. Проверьте Pipeline State:

- Вершинный шейдер компилируется?
- Uniform буферы привязаны?
- Текстуры загружены?

**Решение:**

```cpp
VkShaderModule create_shader_module(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}
```

### Проблема 2: Артефакты рендеринга

**Диагностика:**

1. Mesh Viewer: Геометрия корректна?
2. Texture Viewer: Текстуры загружены?
3. Buffer Viewer: Uniform данные корректны?

**Решение:**

```cpp
void validate_indices(const std::vector<uint32_t>& indices, uint32_t vertexCount) {
    for (uint32_t index : indices) {
        if (index >= vertexCount) {
            std::cerr << "Invalid index: " << index << " >= " << vertexCount << std::endl;
        }
    }
}
```

### Проблема 3: Низкая производительность

**Диагностика:**

1. Tracy для CPU/GPU временных меток
2. RenderDoc:

- GPU Duration каждого draw call
- Pipeline Barriers
- Texture Uploads

**Решение:**

```cpp
void render_frame() {
    ZoneScopedN("RenderFrame");

    {
        ZoneScopedN("CommandBufferRecording");
        // Запись команд...
    }

    TracyPlot("DrawCalls", drawCallCount);
    TracyPlot("TriangleCount", triangleCount);
}
```

---

## NVIDIA Nsight Graphics

### Настройка

1. Установите NVIDIA Nsight Graphics
2. Target Application: `projectv.exe`
3. Working Directory: Путь к ProjectV

### Ключевые возможности

- **Frame Debugger**: Детальная информация о GPU
- **GPU Trace**: Временная шкала выполнения
- **Shader Profiler**: Анализ производительности шейдеров

### Маркировка областей

```cpp
#ifdef NVTX_ENABLED
#include <nvtx3/nvtx3.hpp>

void render_scene() {
    nvtx3::scoped_range range{"RenderScene"};

    {
        nvtx3::scoped_range range2{"UpdateTransforms"};
        // Обновление...
    }

    {
        nvtx3::scoped_range range2{"DrawMeshes"};
        // Отрисовка...
    }
}
#endif
```

---

## Best Practices

### Регулярные захваты

- Каждый новый шейдер: Захватывайте сразу
- Каждое изменение пайплайна: Проверяйте в RenderDoc
- Оптимизации: Сравнивайте "до" и "после"

### Организация

```
captures/
├── 2025-02-18_voxel_shader_debug.rdc
├── 2025-02-18_pipeline_barrier_fix.rdc
└── README.md  # Описание каждого захвата
```

