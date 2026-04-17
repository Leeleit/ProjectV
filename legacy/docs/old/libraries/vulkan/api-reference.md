# Справочник API Vulkan 1.4

Краткое описание ключевых функций и структур Vulkan 1.4 для ProjectV. Полная
спецификация: [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/), локальная копия SDK: `chunked_spec/` (
chap1–chap66).

**🟢** — База Vulkan (треугольник, буферы, простой рендеринг)  
**🟡** — Оптимизации (async compute, GPU-driven, bindless)  
**🔴** — Продвинутые фичи Vulkan 1.4 (mesh shading, ray tracing, timeline semaphores)

## Оглавление

- [Когда что использовать](#когда-что-использовать)
- [Уровни сложности API](#уровни-сложности-api)
- [Vulkan 1.4 для ProjectV](#vulkan-14-для-projectv)
- [SDL3 Vulkan](#sdl3-vulkan)
- [volk](#volk)
- [Physical Device и Surface](#physical-device-и-surface)
- [Initialization](#initialization)
- [Devices](#devices)
- [Swapchain](#swapchain)
- [Command Buffers](#command-buffers)
- [Synchronization](#synchronization)
- [Render Pass](#render-pass)
- [Pipelines](#pipelines)
- [Ресурсы (Buffers, Images)](#ресурсы-buffers-images)
- [Vulkan 1.4 Расширения](#vulkan-14-расширения)
- [Примеры для воксельного рендеринга](#примеры-для-воксельного-рендеринга)

---

## Когда что использовать

| Задача                         | Функция / API                                             | Spec   | Уровень | ProjectV интеграция  |
|--------------------------------|-----------------------------------------------------------|--------|---------|----------------------|
| Загрузить Vulkan loader        | `volkInitialize`                                          | —      | 🟢      | volk + SDL3          |
| Создать Instance               | `vkCreateInstance`                                        | chap4  | 🟢      | validation layers    |
| Перечислить Physical Devices   | `vkEnumeratePhysicalDevices`                              | chap5  | 🟢      | выбор GPU            |
| Создать Device                 | `vkCreateDevice`                                          | chap5  | 🟢      | graphics + compute   |
| Получить очередь               | `vkGetDeviceQueue`                                        | chap5  | 🟢      | async compute        |
| Создать Surface                | `SDL_Vulkan_CreateSurface`                                | chap29 | 🟢      | SDL3 интеграция      |
| Создать Swapchain              | `vkCreateSwapchainKHR`                                    | chap27 | 🟢      | VK_KHR_swapchain     |
| Получить изображения swapchain | `vkGetSwapchainImagesKHR`                                 | chap27 | 🟢      | double buffering     |
| Создать Image View             | `vkCreateImageView`                                       | chap12 | 🟢      | render target        |
| Создать Render Pass            | `vkCreateRenderPass` / `vkCreateRenderPass2`              | chap8  | 🟢      | воксельный рендер    |
| Создать Framebuffer            | `vkCreateFramebuffer`                                     | chap8  | 🟢      | swapchain images     |
| Создать Shader Module          | `vkCreateShaderModule`                                    | chap9  | 🟢      | SPIR-V               |
| Создать Pipeline Layout        | `vkCreatePipelineLayout`                                  | chap15 | 🟢      | descriptor sets      |
| Создать Graphics Pipeline      | `vkCreateGraphicsPipelines`                               | chap10 | 🟢      | triangle             |
| Создать Command Pool           | `vkCreateCommandPool`                                     | chap6  | 🟢      | multi-threading      |
| Выделить Command Buffer        | `vkAllocateCommandBuffers`                                | chap6  | 🟢      | GPU-driven           |
| Записать команды               | `vkBeginCommandBuffer` → `vkCmd*` → `vkEndCommandBuffer`  | chap6  | 🟢      | async recording      |
| Отправить в очередь            | `vkQueueSubmit`                                           | chap6  | 🟢      | timeline semaphores  |
| Показать кадр                  | `vkQueuePresentKHR`                                       | chap27 | 🟢      | present modes        |
| Создать Fence / Semaphore      | `vkCreateFence`, `vkCreateSemaphore`                      | chap7  | 🟢      | sync                 |
| Получить swapchain image       | `vkAcquireNextImageKHR`                                   | chap27 | 🟢      | semaphore wait       |
| Создать буфер / изображение    | `vkCreateBuffer`, `vkCreateImage`                         | chap12 | 🟢      | VMA                  |
| Timeline Semaphore             | `vkCreateSemaphore` с `VkSemaphoreTypeCreateInfo`         | chap7  | 🟡      | async compute        |
| Descriptor Indexing            | `vkCreateDescriptorSetLayout` с `VK_DESCRIPTOR_BINDING_*` | chap15 | 🟡      | bindless             |
| Multi-Draw Indirect            | `vkCmdDrawIndirect`, `vkCmdDrawIndexedIndirect`           | chap10 | 🟡      | GPU-driven           |
| Mesh Shading                   | `vkCmdDrawMeshTasksEXT`                                   | chap10 | 🔴      | воксельная геометрия |
| Ray Tracing                    | `vkCreateAccelerationStructureKHR`                        | chap35 | 🔴      | SVO трассировка      |

---

## Уровни сложности API

### 🟢 Уровень 1: База Vulkan

- Основные объекты: Instance, Device, Queue, Swapchain
- Рендеринг треугольника
- Простое управление памятью
- Базовая синхронизация (fence, semaphore)
- См. [quickstart.md](quickstart.md)

### 🟡 Уровень 2: Оптимизации для вокселей

- GPU Driven Rendering (Multi-Draw Indirect)
- Async Compute с Timeline Semaphores
- Bindless Rendering (Descriptor Indexing)
- Compute Shader Culling и LOD
- Memory Aliasing и Sparse Memory
- См. [performance.md](performance.md)

### 🔴 Уровень 3: Продвинутые фичи Vulkan 1.4

- Mesh Shading для генерации геометрии
- Ray Tracing для SVO и ambient occlusion
- Device Groups (multi-GPU)
- Synchronization2 (более тонкий контроль)
- Pipeline Library (reusable shaders)
- См. [projectv-integration.md](projectv-integration.md)

---

## Vulkan 1.4 для ProjectV

### Ключевые расширения

| Расширение                     | Уровень | Назначение в ProjectV          | Spec   |
|--------------------------------|---------|--------------------------------|--------|
| `VK_KHR_timeline_semaphore`    | 🟡      | Async Compute очереди          | chap7  |
| `VK_EXT_descriptor_indexing`   | 🟡      | Bindless текстуры и буферы     | chap15 |
| `VK_KHR_buffer_device_address` | 🟡      | GPU-driven индексация          | chap12 |
| `VK_EXT_mesh_shader`           | 🔴      | Генерация воксельной геометрии | chap10 |
| `VK_KHR_ray_tracing_pipeline`  | 🔴      | SVO трассировка лучей          | chap35 |
| `VK_KHR_synchronization2`      | 🟡      | Улучшенная синхронизация       | chap7  |
| `VK_EXT_memory_budget`         | 🟡      | Мониторинг памяти VMA          | chap11 |
| `VK_EXT_memory_priority`       | 🟡      | Приоритеты выделения           | chap11 |

### Структуры Vulkan 1.4

```cpp
// Timeline semaphore для async compute
VkSemaphoreTypeCreateInfo timelineInfo{};
timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
timelineInfo.initialValue = 0;

VkSemaphoreCreateInfo semaphoreInfo{};
semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
semaphoreInfo.pNext = &timelineInfo;

vkCreateSemaphore(device, &semaphoreInfo, nullptr, &timelineSemaphore);

// Descriptor indexing для bindless
VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
bindingFlags.bindingCount = 1;
std::vector<VkDescriptorBindingFlags> flags = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
bindingFlags.pBindingFlags = flags.data();
```

---

## SDL3 Vulkan

Функции из [SDL_vulkan.h](../../external/SDL/include/SDL3/SDL_vulkan.h). Окно должно быть с `SDL_WINDOW_VULKAN`.

### SDL_Vulkan_GetInstanceExtensions 🟢

```c
char const * const * SDL_Vulkan_GetInstanceExtensions(Uint32 *count);
```

Возвращает массив имён расширений для `VkInstanceCreateInfo::ppEnabledExtensionNames`. Массив принадлежит SDL — не
освобождать. В SDL3 **не требует** window (в отличие от SDL2). `count` заполняется количеством расширений. При ошибке
возвращает `NULL` — проверить `SDL_GetError()`.

**ProjectV пример:**

```cpp
uint32_t extensionCount = 0;
const char** extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
if (!extensions) {
    SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
    return;
}

VkInstanceCreateInfo instanceInfo{};
instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
instanceInfo.enabledExtensionCount = extensionCount;
instanceInfo.ppEnabledExtensionNames = extensions;
```

### SDL_Vulkan_CreateSurface 🟢

```c
bool SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance instance,
    const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);
```

Создаёт `VkSurfaceKHR` для окна. Instance должен быть создан с расширениями из `SDL_Vulkan_GetInstanceExtensions`.
`allocator` — `NULL` для системного.

### SDL_Vulkan_DestroySurface 🟢

```c
void SDL_Vulkan_DestroySurface(VkInstance instance, VkSurfaceKHR surface,
    const VkAllocationCallbacks *allocator);
```

Уничтожает surface. Вызывать **до** `SDL_DestroyWindow`.

### SDL_Vulkan_GetPresentationSupport 🟢

```c
bool SDL_Vulkan_GetPresentationSupport(VkInstance instance,
    VkPhysicalDevice physicalDevice, Uint32 queueFamilyIndex);
```

Проверяет поддержку present для queue family. Использовать при выборе Physical Device и queue family.

### SDL_Vulkan_GetVkGetInstanceProcAddr 🟢

```c
SDL_FunctionPointer SDL_Vulkan_GetVkGetInstanceProcAddr(void);
```

Возвращает указатель на `vkGetInstanceProcAddr`. Привести к `PFN_vkGetInstanceProcAddr`. Передать в
`volkInitializeCustom` если SDL уже загрузил loader.

### SDL_Vulkan_LoadLibrary / SDL_Vulkan_UnloadLibrary 🟢

```c
bool SDL_Vulkan_LoadLibrary(const char *path);
void SDL_Vulkan_UnloadLibrary(void);
```

Явная загрузка (`path == NULL` — системный loader) и выгрузка Vulkan loader. SDL загружает при создании первого окна с
`SDL_WINDOW_VULKAN`, если не вызвано явно.

---

## volk

Функции из [volk.h](../../external/volk/volk.h). Версия: `VOLK_HEADER_VERSION` 343.

### volkInitialize 🟢

```c
VkResult volkInitialize(void);
```

Загружает Vulkan loader. Вызывать до `vkCreateInstance`. Возвращает `VK_SUCCESS` или `VK_ERROR_INITIALIZATION_FAILED`.

**ProjectV пример с SDL3:**

```cpp
// Вариант 1: SDL загружает loader
SDL_Vulkan_LoadLibrary(nullptr);
PFN_vkGetInstanceProcAddr getProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
volkInitializeCustom(getProcAddr);

// Вариант 2: volk сам загружает
VkResult result = volkInitialize();
if (result != VK_SUCCESS) {
    // Ошибка загрузки Vulkan loader
}
```

### volkInitializeCustom 🟢

```c
void volkInitializeCustom(PFN_vkGetInstanceProcAddr handler);
```

Инициализация с кастомным handler (например, `SDL_Vulkan_GetVkGetInstanceProcAddr`). Loader не загружается volk.

### volkLoadInstance / volkLoadInstanceOnly 🟢

```c
void volkLoadInstance(VkInstance instance);
void volkLoadInstanceOnly(VkInstance instance);
```

Загрузка instance-функций. `volkLoadInstanceOnly` — без device-функций; затем вызвать `volkLoadDevice` или
`volkLoadDeviceTable`.

### volkLoadDevice 🟢

```c
void volkLoadDevice(VkDevice device);
```

Загрузка device-функций. Вызывать после `vkCreateDevice`. Для multi-device — `volkLoadDeviceTable`.

### volkGetLoadedInstance / volkGetLoadedDevice 🟢

```c
VkInstance volkGetLoadedInstance(void);
VkDevice volkGetLoadedDevice(void);
```

Последние загруженные instance/device или `VK_NULL_HANDLE`.

---

## Physical Device и Surface

### vkGetPhysicalDeviceQueueFamilyProperties 🟢

```c
void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
    uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties *pQueueFamilyProperties);
```

Возвращает queue families. Искать индекс с `VK_QUEUE_GRAPHICS_BIT` и
`SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, index)`.

**ProjectV пример для async compute:**

```cpp
uint32_t queueFamilyCount = 0;
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

uint32_t graphicsFamily = UINT32_MAX;
uint32_t computeFamily = UINT32_MAX;
uint32_t presentFamily = UINT32_MAX;

for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
    if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) computeFamily = i;
    if (SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, i)) presentFamily = i;
}
```

### vkGetPhysicalDeviceSurfaceCapabilitiesKHR 🟢

```c
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities);
```

Возможности surface: `minImageCount`, `maxImageCount`, `minImageExtent`, `maxImageExtent`. Использовать для clamping
extent swapchain.

### vkGetPhysicalDeviceSurfaceFormatsKHR 🟢

```c
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface, uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats);
```

Поддерживаемые форматы и color space для swapchain.

### vkGetPhysicalDeviceSurfacePresentModesKHR 🟢

```c
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface, uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes);
```

Поддерживаемые present modes (`VK_PRESENT_MODE_FIFO_KHR` и др.).

### VkSurfaceCapabilitiesKHR и extent 🟢

Для swapchain `imageExtent` должен быть в диапазоне `minImageExtent`–`maxImageExtent`:

```cpp
VkSurfaceCapabilitiesKHR caps;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
VkExtent2D extent = { width, height };
extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
```

`minImageCount` — минимум изображений (обычно 2 для double buffering). Проверить
`minImageCount <= desiredCount <= maxImageCount`.

---

## Initialization

### vkCreateInstance 🟢

```c
VkResult vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);
```

Создаёт подключение приложения к Vulkan. Вызывать после [volkInitialize](#volkinitialize). В `pCreateInfo` —
расширения (из [SDL_Vulkan_GetInstanceExtensions](#sdl_vulkan_getinstanceextensions)), слои (validation).

**ProjectV пример:**

```cpp
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "ProjectV";
appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
appInfo.pEngineName = "ProjectV Engine";
appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
appInfo.apiVersion = VK_API_VERSION_1_4; // Vulkan 1.4

// Получить расширения от SDL
uint32_t extensionCount = 0;
const char** extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

// Добавить validation layers в debug
std::vector<const char*> layers;
#ifdef _DEBUG
layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

VkInstanceCreateInfo instanceInfo{};
instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
instanceInfo.pApplicationInfo = &appInfo;
instanceInfo.enabledExtensionCount = extensionCount;
instanceInfo.ppEnabledExtensionNames = extensions;
instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
instanceInfo.ppEnabledLayerNames = layers.data();

VkInstance instance = VK_NULL_HANDLE;
VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
```

### VkInstanceCreateInfo 🟢

| Поле                      | Описание                                              | ProjectV специфика   |
|---------------------------|-------------------------------------------------------|----------------------|
| `pApplicationInfo`        | `VkApplicationInfo` — имя приложения, версия API      | Vulkan 1.4           |
| `enabledExtensionCount`   | Количество расширений                                 | SDL3 + validation    |
| `ppEnabledExtensionNames` | Имена расширений (surface, debug и т.д.)              | `VK_EXT_debug_utils` |
| `enabledLayerCount`       | Количество слоёв                                      | debug только         |
| `ppEnabledLayerNames`     | Имена слоёв (например, `VK_LAYER_KHRONOS_validation`) | validation           |

---

## Devices

### vkEnumeratePhysicalDevices 🟢

```c
VkResult vkEnumeratePhysicalDevices(
    VkInstance instance,
    uint32_t* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices);
```

Перечисляет доступные GPU. Сначала вызвать с `pPhysicalDevices = NULL` для получения `pPhysicalDeviceCount`, затем
выделить массив и вызвать снова.

**ProjectV пример с выбором GPU:**

```cpp
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
std::vector<VkPhysicalDevice> devices(deviceCount);
vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

// Выбрать GPU с поддержкой Vulkan 1.4 и нужных расширений
VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
for (VkPhysicalDevice device : devices) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    
    // Проверить версию Vulkan
    if (props.apiVersion < VK_API_VERSION_1_4) continue;
    
    // Проверить нужные расширения (timeline semaphores, descriptor indexing)
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
    
    bool hasTimeline = false;
    bool hasDescriptorIndexing = false;
    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, "VK_KHR_timeline_semaphore") == 0) hasTimeline = true;
        if (strcmp(ext.extensionName, "VK_EXT_descriptor_indexing") == 0) hasDescriptorIndexing = true;
    }
    
    if (hasTimeline && hasDescriptorIndexing) {
        selectedDevice = device;
        break;
    }
}
```

### vkCreateDevice 🟢

```c
VkResult vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice);
```

Создаёт логическое устройство. В `pCreateInfo` — очереди (`pQueueCreateInfos`), расширения (`VK_KHR_swapchain` и др.).

**ProjectV пример с async compute очередями:**

```cpp
std::vector<VkDeviceQueueCreateInfo> queueInfos;
std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, computeFamily, presentFamily};
float queuePriority = 1.0f;

for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;
    queueInfos.push_back(queueInfo);
}

// Включить расширения Vulkan 1.4
std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

VkDeviceCreateInfo deviceInfo{};
deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
deviceInfo.pQueueCreateInfos = queueInfos.data();
deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

// Включить фичи Vulkan 1.4
VkPhysicalDeviceVulkan12Features features12{};
features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
features12.timelineSemaphore = VK_TRUE;
features12.descriptorIndexing = VK_TRUE;
features12.bufferDeviceAddress = VK_TRUE;

deviceInfo.pNext = &features12;

VkDevice device = VK_NULL_HANDLE;
VkResult result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
```

### vkGetDeviceQueue 🟢

```c
void vkGetDeviceQueue(
    VkDevice device,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue);
```

Получает handle очереди. `queueFamilyIndex` — индекс семейства с нужными возможностями (graphics, present).

**ProjectV пример для multi-queue:**

```cpp
VkQueue graphicsQueue;
VkQueue computeQueue;
VkQueue presentQueue;

vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
```

---

## Swapchain

### vkCreateSwapchainKHR 🟢

```c
VkResult vkCreateSwapchainKHR(
    VkDevice device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSwapchainKHR* pSwapchain);
```

Создаёт цепочку изображений для вывода. Требует `VK_KHR_swapchain`. В `pCreateInfo`: `surface`, `minImageCount` (
ограничен `VkSurfaceCapabilitiesKHR`), `imageFormat`, `imageExtent` (clamp по `minImageExtent`/`maxImageExtent`),
`imageUsage`, `presentMode` и т.д.

**ProjectV пример с mailbox present mode:**

```cpp
VkSwapchainCreateInfoKHR swapchainInfo{};
swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
swapchainInfo.surface = surface;
swapchainInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
swapchainInfo.imageFormat = surfaceFormat.format;
swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
swapchainInfo.imageExtent = extent;
swapchainInfo.imageArrayLayers = 1;
swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
swapchainInfo.preTransform = surfaceCaps.currentTransform;
swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR; // Triple buffering
swapchainInfo.clipped = VK_TRUE;
swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

VkSwapchainKHR swapchain;
vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
```

### vkAcquireNextImageKHR 🟢

```c
VkResult vkAcquireNextImageKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    uint64_t timeout,
    VkSemaphore semaphore,
    VkFence fence,
    uint32_t* pImageIndex);
```

Получает индекс следующего изображения для рисования. `semaphore` — сигнализируется, когда image готов. `timeout` =
`UINT64_MAX` — бесконечно.

### vkQueuePresentKHR 🟢

```c
VkResult vkQueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo);
```

Показывает кадр на экран. В `pPresentInfo`: `waitSemaphoreCount`, `pWaitSemaphores` (дождаться завершения рендеринга),
`swapchainCount`, `pSwapchains`, `pImageIndices`.

---

## Command Buffers

### vkCreateCommandPool 🟢

```c
VkResult vkCreateCommandPool(
    VkDevice device,
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkCommandPool* pCommandPool);
```

Создаёт пул для command buffers. `queueFamilyIndex` — семейство, в которое будут отправляться буферы.

**ProjectV пример с transient буферами:**

```cpp
VkCommandPoolCreateInfo poolInfo{};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.queueFamilyIndex = graphicsFamily;
poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |  // Буферы перезаписываются часто
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Можно сбрасывать

vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
```

### vkAllocateCommandBuffers 🟢

```c
VkResult vkAllocateCommandBuffers(
    VkDevice device,
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer* pCommandBuffers);
```

Выделяет command buffers из пула.

### vkBeginCommandBuffer / vkEndCommandBuffer 🟢

```c
VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer);
```

Начало и конец записи команд.

### vkQueueSubmit 🟢

```c
VkResult vkQueueSubmit(
    VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence fence);
```

Отправляет command buffers в очередь. `fence` — сигнализируется при завершении. В `VkSubmitInfo`: `waitSemaphoreCount`,
`pWaitSemaphores`, `pWaitDstStageMask`, `commandBufferCount`, `pCommandBuffers`, `signalSemaphoreCount`,
`pSignalSemaphores`.

### Основные команды записи 🟢

| Команда                       | Назначение                                   | ProjectV использование     |
|-------------------------------|----------------------------------------------|----------------------------|
| `vkCmdBeginRenderPass`        | Начать render pass                           | Воксельный рендер          |
| `vkCmdEndRenderPass`          | Закончить render pass                        |                            |
| `vkCmdBindPipeline`           | Привязать pipeline                           | Graphics/Compute           |
| `vkCmdSetViewport`            | Установить viewport (если dynamic)           |                            |
| `vkCmdSetScissor`             | Установить scissor (если dynamic)            |                            |
| `vkCmdDraw`                   | Отрисовать примитивы                         | Простой рендер             |
| `vkCmdDrawIndexed`            | Отрисовать с индексным буфером               | Меши                       |
| `vkCmdDrawIndirect`           | Indirect drawing                             | GPU-driven воксели         |
| `vkCmdDrawIndexedIndirect`    | Indirect indexed drawing                     | GPU-driven индексированные |
| `vkCmdDispatch`               | Выполнить compute shader                     | Генерация вокселей         |
| `vkCmdPipelineBarrier`        | Барьер для layout transition и синхронизации | Async compute              |
| `vkCmdCopyBuffer`             | Копирование буфера                           | Staging buffers            |
| `vkCmdCopyImage`              | Копирование изображения                      | Texture upload             |
| `vkCmdClearColorImage`        | Очистка цвета                                | Очистка render target      |
| `vkCmdClearDepthStencilImage` | Очистка depth/stencil                        | Очистка depth buffer       |

---

## Synchronization

### vkCreateFence 🟢

```c
VkResult vkCreateFence(
    VkDevice device,
    const VkFenceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkFence* pFence);
```

Создаёт fence. Host ждёт сигнала через `vkWaitForFences`.

### vkCreateSemaphore 🟢

```c
VkResult vkCreateSemaphore(
    VkDevice device,
    const VkSemaphoreCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSemaphore* pSemaphore);
```

Создаёт semaphore. Используется в `vkQueueSubmit` и `vkQueuePresentKHR` для ordering.

### vkWaitForFences 🟢

```c
VkResult vkWaitForFences(
    VkDevice device,
    uint32_t fenceCount,
    const VkFence* pFences,
    VkBool32 waitAll,
    uint64_t timeout);
```

Блокирует host до сигнала fence. Перед повторным использованием ресурсов кадра.

### vkResetFences 🟢

```c
VkResult vkResetFences(
    VkDevice device,
    uint32_t fenceCount,
    const VkFence* pFences);
```

Сбрасывает fence в несигнальное состояние. Вызывать перед следующим `vkQueueSubmit` с тем же fence.

### vkCmdPipelineBarrier 🟡

```c
void vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers);
```

Синхронизация внутри command buffer. Для смены layout изображения — `VkImageMemoryBarrier` с `oldLayout`, `newLayout`,
`image`.

**ProjectV пример для async compute:**

```cpp
// Барьер между compute и graphics очередями
VkBufferMemoryBarrier barrier{};
barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Compute записал
barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT; // Graphics читает
barrier.srcQueueFamilyIndex = computeFamily;
barrier.dstQueueFamilyIndex = graphicsFamily;
barrier.buffer = indirectBuffer;
barrier.offset = 0;
barrier.size = VK_WHOLE_SIZE;

vkCmdPipelineBarrier(
    computeCommandBuffer,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Compute завершил
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,  // Graphics начинает чтение
    0,
    0, nullptr,
    1, &barrier,
    0, nullptr
);
```

---

## Render Pass

### vkCreateRenderPass 🟢

```c
VkResult vkCreateRenderPass(
    VkDevice device,
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkRenderPass* pRenderPass);
```

Создаёт render pass. В `pCreateInfo`: `attachmentCount`, `pAttachments`, `subpassCount`, `pSubpasses`,
`dependencyCount`, `pDependencies`.

### vkCreateFramebuffer 🟢

```c
VkResult vkCreateFramebuffer(
    VkDevice device,
    const VkFramebufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkFramebuffer* pFramebuffer);
```

Создаёт framebuffer. В `pCreateInfo`: `renderPass`, `attachmentCount`, `pAttachments` (image views), `width`, `height`,
`layers`.

### vkCmdBeginRenderPass 🟢

```c
void vkCmdBeginRenderPass(
    VkCommandBuffer commandBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents contents);
```

Начинает render pass. `pRenderPassBegin`: `renderPass`, `framebuffer`, `renderArea`, `clearValueCount`, `pClearValues`.
`contents` = `VK_SUBPASS_CONTENTS_INLINE` для inline-команд.

---

## Pipelines

### vkCreateShaderModule 🟢

```c
VkResult vkCreateShaderModule(
    VkDevice device,
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkShaderModule* pShaderModule);
```

Создаёт shader module из SPIR-V. `pCode` — указатель на байты SPIR-V, `codeSize` — размер в байтах.

### vkCreatePipelineLayout 🟢

```c
VkResult vkCreatePipelineLayout(
    VkDevice device,
    const VkPipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkPipelineLayout* pPipelineLayout);
```

Создаёт layout пайплайна (descriptor set layouts, push constant ranges). Для простого треугольника —
`setLayoutCount = 0`, `pushConstantRangeCount = 0`.

### vkCreateGraphicsPipelines 🟢

```c
VkResult vkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines);
```

Создаёт graphics pipeline(s). `pipelineCache` может быть `VK_NULL_HANDLE`. В `VkGraphicsPipelineCreateInfo`:
`stageCount`, `pStages`, `pVertexInputState`, `pInputAssemblyState`, `pViewportState`, `pRasterizationState`,
`pMultisampleState`, `pColorBlendState`, `pDynamicState`, `layout`, `renderPass`, `subpass`.

**ProjectV пример для воксельного рендеринга:**

```cpp
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.stageCount = 2;
pipelineInfo.pStages = shaderStages; // vertex + fragment
pipelineInfo.pVertexInputState = &vertexInputInfo; // vertex buffer layout
pipelineInfo.pInputAssemblyState = &inputAssembly; // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
pipelineInfo.pViewportState = &viewportState;
pipelineInfo.pRasterizationState = &rasterizer;
pipelineInfo.pMultisampleState = &multisampling;
pipelineInfo.pColorBlendState = &colorBlending;
pipelineInfo.pDynamicState = &dynamicState;
pipelineInfo.layout = pipelineLayout;
pipelineInfo.renderPass = renderPass;
pipelineInfo.subpass = 0;

vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
```

### vkCreateComputePipelines 🟡

```c
VkResult vkCreateComputePipelines(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t createInfoCount,
    const VkComputePipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines);
```

Создаёт compute pipeline для compute shaders.

---

## Ресурсы (Buffers, Images)

В ProjectV для буферов и изображений рекомендуется [VMA](../vma/api-reference.md). Ниже — базовый Vulkan API.

### vkCreateBuffer 🟢

```c
VkResult vkCreateBuffer(
    VkDevice device,
    const VkBufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBuffer* pBuffer);
```

Создаёт буфер. После создания: `vkGetBufferMemoryRequirements`, `vkAllocateMemory`, `vkBindBufferMemory`.

### vkCreateImage 🟢

```c
VkResult vkCreateImage(
    VkDevice device,
    const VkImageCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkImage* pImage);
```

Создаёт изображение. Аналогично — memory requirements, allocate, bind. Для image views — `vkCreateImageView`.

### vkCreateImageView 🟢

```c
VkResult vkCreateImageView(
    VkDevice device,
    const VkImageViewCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkImageView* pView);
```

Создаёт представление изображения для shader или framebuffer. `image`, `viewType`, `format`, `subresourceRange`.

---

## Vulkan 1.4 Расширения

### Timeline Semaphores 🟡

```cpp
// Создание timeline semaphore
VkSemaphoreTypeCreateInfo timelineInfo{};
timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
timelineInfo.initialValue = 0;

VkSemaphoreCreateInfo semaphoreInfo{};
semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
semaphoreInfo.pNext = &timelineInfo;

vkCreateSemaphore(device, &semaphoreInfo, nullptr, &timelineSemaphore);

// Ожидание на host
VkSemaphoreWaitInfo waitInfo{};
waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
waitInfo.semaphoreCount = 1;
waitInfo.pSemaphores = &timelineSemaphore;
uint64_t waitValue = 1;
waitInfo.pValues = &waitValue;

vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

// Сигнализация из GPU
VkSemaphoreSignalInfo signalInfo{};
signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
signalInfo.semaphore = timelineSemaphore;
signalInfo.value = 1;

vkSignalSemaphore(device, &signalInfo);
```

### Descriptor Indexing 🟡

```cpp
// Создание bindless descriptor set layout
VkDescriptorSetLayoutBinding binding{};
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
binding.descriptorCount = 1000; // Максимум текстур
binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorBindingFlags bindingFlag = 
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
flagsInfo.bindingCount = 1;
flagsInfo.pBindingFlags = &bindingFlag;

VkDescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.pNext = &flagsInfo;
layoutInfo.bindingCount = 1;
layoutInfo.pBindings = &binding;
layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
```

### Buffer Device Address 🟡

```cpp
// Запрос адреса буфера
VkBufferDeviceAddressInfo addressInfo{};
addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
addressInfo.buffer = buffer;

VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addressInfo);

// Использование в шейдере
// GLSL: layout(buffer_reference) buffer VoxelData { vec4 colors[]; };
// C++: uint64_t address передать через push constant
```

---

## Примеры для воксельного рендеринга

### GPU Driven Rendering 🟡

```cpp
// 1. Создание indirect draw buffer
VkBufferCreateInfo bufferInfo{};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAWS;
bufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | 
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

vkCreateBuffer(device, &bufferInfo, nullptr, &indirectBuffer);

// 2. Compute shader заполняет indirect commands
// 3. Graphics пайплайн использует multi-draw indirect
VkCommandBuffer cmd = ...;
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
vkCmdDrawIndexedIndirect(cmd, indirectBuffer, 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
```

### Async Compute с Timeline Semaphores 🟡

```cpp
// 1. Compute очередь генерирует данные
VkSubmitInfo computeSubmit{};
computeSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
computeSubmit.commandBufferCount = 1;
computeSubmit.pCommandBuffers = &computeCmdBuffer;
computeSubmit.signalSemaphoreCount = 1;
computeSubmit.pSignalSemaphores = &timelineSemaphore;

uint64_t computeSignalValue = 1;
VkTimelineSemaphoreSubmitInfo timelineInfo{};
timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
timelineInfo.signalSemaphoreValueCount = 1;
timelineInfo.pSignalSemaphoreValues = &computeSignalValue;

computeSubmit.pNext = &timelineInfo;
vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

// 2. Graphics очередь ждёт compute
VkSubmitInfo graphicsSubmit{};
graphicsSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
graphicsSubmit.waitSemaphoreCount = 1;
graphicsSubmit.pWaitSemaphores = &timelineSemaphore;
graphicsSubmit.pWaitDstStageMask = &waitStage;
graphicsSubmit.commandBufferCount = 1;
graphicsSubmit.pCommandBuffers = &graphicsCmdBuffer;

uint64_t waitValue = 1;
timelineInfo.waitSemaphoreValueCount = 1;
timelineInfo.pWaitSemaphoreValues = &waitValue;

graphicsSubmit.pNext = &timelineInfo;
vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, VK_NULL_HANDLE);
```

---

## Ссылки на спецификацию

| Тема                                     | Глава chunked_spec | Уровень |
|------------------------------------------|--------------------|---------|
| Introduction, Conventions                | chap2              | 🟢      |
| Fundamentals (Host/Device, Object Model) | chap3              | 🟢      |
| Initialization, Instances                | chap4              | 🟢      |
| Devices and Queues                       | chap5              | 🟢      |
| Command Buffers                          | chap6              | 🟢      |
| Synchronization                          | chap7              | 🟢      |
| Render Pass                              | chap8              | 🟢      |
| Shaders                                  | chap9              | 🟢      |
| Pipelines                                | chap10             | 🟢      |
| Memory                                   | chap11             | 🟢      |
| Buffers, Images                          | chap12             | 🟢      |
| Descriptor Sets                          | chap15             | 🟡      |
| Compute                                  | chap28             | 🟡      |
| Swapchain (VK_KHR_swapchain)             | chap27             | 🟢      |
| Present                                  | chap27             | 🟢      |
| Timeline Semaphores                      | chap7              | 🟡      |
| Descriptor Indexing                      | chap15             | 🟡      |
| Mesh Shading                             | chap10             | 🔴      |
| Ray Tracing                              | chap35             | 🔴      |

---

## Интеграция с ProjectV

См. подробнее:

- [ProjectV Integration](projectv-integration.md) — архитектура воксельного рендерера
- [Performance](performance.md) — оптимизации GPU-driven rendering
- [Troubleshooting](troubleshooting.md) — решение проблем синхронизации
- [Decision Trees](decision-trees.md) — выбор API и архитектуры
- [Use Cases](use-cases.md) — примеры compute shaders для вокселей

**Ключевые библиотеки ProjectV:**

- **VMA** — управление памятью GPU для воксельных буферов
- **volk** — динамическая загрузка Vulkan функций
- **SDL3** — окна и поверхности
- **Tracy** — GPU profiling для оптимизации
- **flecs** — ECS для организации рендеринга