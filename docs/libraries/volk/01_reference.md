# Volk: Мета-лоадер Vulkan

> **Для понимания:** Volk — это как прямой провод к GPU вместо коммутатора. Обычный Vulkan loader — это коммутатор,
> который маршрутизирует каждый вызов через несколько узлов. Volk — это прямой кабель, подключённый прямо к драйверу.
> Меньше задержек, больше скорости.

**Volk — это мост между вашим кодом и драйвером GPU.** Мета-загрузчик, который превращает статическую зависимость от
Vulkan loader в динамический выбор во время выполнения.

---

## Назначение

Volk — библиотека для динамической загрузки функций Vulkan без статической или динамической линковки с системным Vulkan
loader (`vulkan-1.dll` / `libvulkan.so`).

### Проблема традиционного подхода

При стандартной линковке с Vulkan loader:

1. Приложение зависит от `vulkan-1.dll` / `libvulkan.so` при запуске
2. Каждый вызов функции проходит через dispatch цепочку loader'а
3. Dispatch overhead может достигать 7% для device-intensive workloads

### Решение volk

Volk загружает Vulkan loader динамически во время выполнения и предоставляет прямые указатели на функции:

1. Приложение запускается без установленного Vulkan (проверка в runtime)
2. После `volkLoadDevice()` вызовы идут напрямую в драйвер
3. Dispatch overhead устранён

---

## Архитектура

### Vulkan Loader

Vulkan использует динамическую загрузку функций через loader — системную библиотеку `vulkan-1.dll` (Windows) или
`libvulkan.so` (Linux).

```
Приложение
    ↓
vulkan-1.dll (loader) — диспетчеризация
    ↓
Validation Layers — проверки (если включены)
    ↓
Драйвер GPU — выполнение
```

### Мета-загрузчик volk

Volk загружает Vulkan loader динамически во время выполнения и предоставляет прямые указатели на функции.

```
volkInitialize()
    ↓
Загрузка vulkan-1.dll/libvulkan.so
    ↓
Получение vkGetInstanceProcAddr
    ↓
volkLoadInstance(instance)
    ↓
Загрузка instance-функций
    ↓
volkLoadDevice(device)
    ↓
Прямые указатели на драйвер
```

---

## Возможности

| Возможность               | Описание                                   |
|---------------------------|--------------------------------------------|
| Динамическая загрузка     | Загрузка Vulkan loader во время выполнения |
| Автоматические расширения | Загрузка всех entrypoints расширений       |
| Прямые вызовы драйвера    | Оптимизация производительности до 7%       |
| Кроссплатформенность      | Windows, Linux, macOS, Android             |

---

## Характеристики

| Параметр | Значение  |
|----------|-----------|
| Язык     | C89 / C++ |
| Версия   | 2.0+      |
| Vulkan   | 1.0–1.4   |
| Лицензия | MIT       |

---

## Совместимость

Volk совместим со всеми версиями Vulkan от 1.0 до 1.4 и автоматически загружает функции расширений через
`vkGetInstanceProcAddr`.

### Поддерживаемые платформы

- **Windows (Win32)**
- **Linux (X11, Wayland)**
- **macOS** (через MoltenVK)
- **Android**

---

## Entrypoints

Vulkan функции загружаются по имени через `vkGetInstanceProcAddr`:

```cpp
PFN_vkCreateInstance vkCreateInstance =
    (PFN_vkCreateInstance)vkGetInstanceProcAddr(NULL, "vkCreateInstance");
```

Volk автоматизирует этот процесс для всех функций.

### Типы entrypoints

| Тип      | Загрузка                               | Примеры                                                      |
|----------|----------------------------------------|--------------------------------------------------------------|
| Global   | `vkGetInstanceProcAddr(NULL, ...)`     | `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties` |
| Instance | `vkGetInstanceProcAddr(instance, ...)` | `vkCreateDevice`, `vkDestroyInstance`                        |
| Device   | `vkGetDeviceProcAddr(device, ...)`     | `vkCmdDraw`, `vkQueueSubmit`                                 |

---

## Dispatch цепочка

### Без volk

```cpp
// Глобальная функция через loader
vkCmdDraw(cmd, vertexCount, 1, 0, 0);

// Путь: Приложение → Loader → Layers → Драйвер
```

### С volk после volkLoadDevice

```cpp
// Указатель на функцию драйвера
vkCmdDraw(cmd, vertexCount, 1, 0, 0);

// Путь: Приложение → Драйвер (напрямую)
```

---

## Глобальные указатели

Volk объявляет все Vulkan функции как extern-указатели:

```c
// В volk.h
extern PFN_vkCreateInstance vkCreateInstance;
extern PFN_vkCmdDraw vkCmdDraw;
extern PFN_vkQueueSubmit vkQueueSubmit;
// Остальные ~250 глобальных функций Vulkan: vkEnumerateInstanceVersion, vkEnumerateInstanceExtensionProperties, vkEnumerateInstanceLayerProperties, vkGetInstanceProcAddr, vkCreateInstance, vkDestroyInstance, vkEnumeratePhysicalDevices, vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceFeatures, vkGetPhysicalDeviceFormatProperties, vkGetPhysicalDeviceImageFormatProperties, vkGetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceSparseImageFormatProperties, vkCreateDevice, vkDestroyDevice, vkEnumerateDeviceExtensionProperties, vkEnumerateDeviceLayerProperties, vkGetDeviceQueue, vkQueueSubmit, vkQueueWaitIdle, vkDeviceWaitIdle, vkAllocateMemory, vkFreeMemory, vkMapMemory, vkUnmapMemory, vkFlushMappedMemoryRanges, vkInvalidateMappedMemoryRanges, vkGetDeviceMemoryCommitment, vkBindBufferMemory, vkBindImageMemory, vkGetBufferMemoryRequirements, vkGetImageMemoryRequirements, vkGetImageSparseMemoryRequirements, vkGetPhysicalDeviceSparseImageFormatProperties, vkQueueBindSparse, vkCreateFence, vkDestroyFence, vkResetFences, vkGetFenceStatus, vkWaitForFences, vkCreateSemaphore, vkDestroySemaphore, vkCreateEvent, vkDestroyEvent, vkGetEventStatus, vkSetEvent, vkResetEvent, vkCreateQueryPool, vkDestroyQueryPool, vkGetQueryPoolResults, vkCreateBuffer, vkDestroyBuffer, vkCreateBufferView, vkDestroyBufferView, vkCreateImage, vkDestroyImage, vkGetImageSubresourceLayout, vkCreateImageView, vkDestroyImageView, vkCreateShaderModule, vkDestroyShaderModule, vkCreatePipelineCache, vkDestroyPipelineCache, vkGetPipelineCacheData, vkMergePipelineCaches, vkCreateGraphicsPipelines, vkCreateComputePipelines, vkDestroyPipeline, vkCreatePipelineLayout, vkDestroyPipelineLayout, vkCreateSampler, vkDestroySampler, vkCreateDescriptorSetLayout, vkDestroyDescriptorSetLayout, vkCreateDescriptorPool, vkDestroyDescriptorPool, vkResetDescriptorPool, vkAllocateDescriptorSets, vkFreeDescriptorSets, vkUpdateDescriptorSets, vkCreateFramebuffer, vkDestroyFramebuffer, vkCreateRenderPass, vkDestroyRenderPass, vkGetRenderAreaGranularity, vkCreateCommandPool, vkDestroyCommandPool, vkResetCommandPool, vkAllocateCommandBuffers, vkFreeCommandBuffers, vkBeginCommandBuffer, vkEndCommandBuffer, vkResetCommandBuffer, vkCmdBindPipeline, vkCmdSetViewport, vkCmdSetScissor, vkCmdSetLineWidth, vkCmdSetDepthBias, vkCmdSetBlendConstants, vkCmdSetDepthBounds, vkCmdSetStencilCompareMask, vkCmdSetStencilWriteMask, vkCmdSetStencilReference, vkCmdBindDescriptorSets, vkCmdBindIndexBuffer, vkCmdBindVertexBuffers, vkCmdDraw, vkCmdDrawIndexed, vkCmdDrawIndirect, vkCmdDrawIndexedIndirect, vkCmdDispatch, vkCmdDispatchIndirect, vkCmdCopyBuffer, vkCmdCopyImage, vkCmdBlitImage, vkCmdCopyBufferToImage, vkCmdCopyImageToBuffer, vkCmdUpdateBuffer, vkCmdFillBuffer, vkCmdClearColorImage, vkCmdClearDepthStencilImage, vkCmdClearAttachments, vkCmdResolveImage, vkCmdSetEvent, vkCmdResetEvent, vkCmdWaitEvents, vkCmdPipelineBarrier, vkCmdBeginQuery, vkCmdEndQuery, vkCmdResetQueryPool, vkCmdWriteTimestamp, vkCmdCopyQueryPoolResults, vkCmdPushConstants, vkCmdBeginRenderPass, vkCmdNextSubpass, vkCmdEndRenderPass, vkCmdExecuteCommands, vkCreateSwapchainKHR, vkDestroySwapchainKHR, vkGetSwapchainImagesKHR, vkAcquireNextImageKHR, vkQueuePresentKHR, vkGetPhysicalDeviceSurfaceSupportKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceFormatsKHR, vkGetPhysicalDeviceSurfacePresentModesKHR, vkCreateDebugReportCallbackEXT, vkDestroyDebugReportCallbackEXT, vkDebugReportMessageEXT, vkCreateDebugUtilsMessengerEXT, vkDestroyDebugUtilsMessengerEXT, vkSubmitDebugUtilsMessageEXT, vkCmdBeginDebugUtilsLabelEXT, vkCmdEndDebugUtilsLabelEXT, vkCmdInsertDebugUtilsLabelEXT, vkSetDebugUtilsObjectNameEXT, vkSetDebugUtilsObjectTagEXT, vkGetPhysicalDeviceToolPropertiesEXT, vkCreateAccelerationStructureKHR, vkDestroyAccelerationStructureKHR, vkCmdBuildAccelerationStructuresKHR, vkCmdBuildAccelerationStructuresIndirectKHR, vkBuildAccelerationStructuresKHR, vkCopyAccelerationStructureKHR, vkCopyAccelerationStructureToMemoryKHR, vkCopyMemoryToAccelerationStructureKHR, vkWriteAccelerationStructuresPropertiesKHR, vkCmdCopyAccelerationStructureKHR, vkCmdCopyAccelerationStructureToMemoryKHR, vkCmdCopyMemoryToAccelerationStructureKHR, vkGetAccelerationStructureDeviceAddressKHR, vkCmdWriteAccelerationStructuresPropertiesKHR, vkGetDeviceAccelerationStructureCompatibilityKHR, vkGetAccelerationStructureBuildSizesKHR, vkCmdTraceRaysKHR, vkCreateRayTracingPipelinesKHR, vkGetRayTracingShaderGroupHandlesKHR, vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, vkCmdTraceRaysIndirectKHR, vkGetRayTracingShaderGroupStackSizeKHR, vkCmdSetRayTracingPipelineStackSizeKHR, vkCmdDrawMeshTasksEXT, vkCmdDrawMeshTasksIndirectEXT, vkCmdDrawMeshTasksIndirectCountEXT, vkCmdSetFragmentShadingRateKHR, vkGetPhysicalDeviceFragmentShadingRatesKHR, vkCmdSetFragmentShadingRateEnumNV, vkGetAccelerationStructureMemoryRequirementsNV, vkBindAccelerationStructureMemoryNV, vkCmdBuildAccelerationStructureNV, vkCmdCopyAccelerationStructureNV, vkCmdTraceRaysNV, vkCreateAccelerationStructureNV, vkDestroyAccelerationStructureNV, vkGetAccelerationStructureHandleNV, vkCmdWriteAccelerationStructuresPropertiesNV, vkCompileDeferredNV, vkGetMemoryWin32HandleNV, vkCreateWin32SurfaceKHR, vkGetPhysicalDeviceWin32PresentationSupportKHR, vkGetPhysicalDeviceSurfaceSupportKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceFormatsKHR, vkGetPhysicalDeviceSurfacePresentModesKHR, vkCreateSwapchainKHR, vkDestroySwapchainKHR, vkGetSwapchainImagesKHR, vkAcquireNextImageKHR, vkQueuePresentKHR, vkGetDeviceGroupSurfacePresentModesKHR, vkGetPhysicalDevicePresentRectanglesKHR, vkAcquireNextImage2KHR, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayPropertiesKHR, vkGetPhysicalDeviceDisplayPlanePropertiesKHR, vkGetDisplayPlaneSupportedDisplaysKHR, vkGetDisplayModePropertiesKHR, vkCreateDisplayModeKHR, vkGetDisplayPlaneCapabilitiesKHR, vkCreateDisplayPlaneSurfaceKHR, vkCreateSharedSwapchainsKHR, vkGetPhysicalDeviceExternalBufferProperties, vkGetPhysicalDeviceExternalFenceProperties, vkGetPhysicalDeviceExternalSemaphoreProperties, vkImportFenceWin32HandleKHR, vkGetFenceWin32HandleKHR, vkImportFenceFdKHR, vkGetFenceFdKHR, vkImportSemaphoreWin32HandleKHR, vkGetSemaphoreWin32HandleKHR, vkImportSemaphoreFdKHR, vkGetSemaphoreFdKHR, vkGetMemoryWin32HandleKHR, vkGetMemoryWin32HandlePropertiesKHR, vkGetMemoryFdKHR, vkGetMemoryFdPropertiesKHR, vkGetPhysicalDeviceSurfaceCapabilities2EXT, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, vkGetDisplayModeProperties2KHR, vkGetDisplayPlaneCapabilities2KHR, vkGetSwapchainStatusKHR, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, vkGetDisplayModeProperties2KHR, vkGetDisplayPlaneCapabilities2KHR, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateSamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, vkCreateDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, vkUpdateDescriptorSetWithTemplate, vkGetDescriptorSetLayoutSupport, vkGetPhysicalDeviceExternalBufferProperties, vkGetPhysicalDeviceExternalFenceProperties, vkGetPhysicalDeviceExternalSemaphoreProperties, vkGetPhysicalDeviceFeatures2, vkGetPhysicalDeviceProperties2, vkGetPhysicalDeviceFormatProperties2, vkGetPhysicalDeviceImageFormatProperties2, vkGetPhysicalDeviceQueueFamilyProperties2, vkGetPhysicalDeviceMemoryProperties2, vkGetPhysicalDeviceSparseImageFormatProperties2, vkTrimCommandPool, vkGetDeviceQueue2, vkCreateSamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, vkCreateDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, vkUpdateDescriptorSetWithTemplate, vkGetDescriptorSetLayoutSupport, vkGetDeviceGroupPeerMemoryFeatures, vkCmdSetDeviceMask, vkCmdDispatchBase, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateRenderPass2, vkCmdBeginRenderPass2, vkCmdNextSubpass2, vkCmdEndRenderPass2, vkGetSemaphoreCounterValue, vkWaitSemaphores, vkSignalSemaphore, vkGetBufferDeviceAddress, vkGetBufferOpaqueCaptureAddress, vkGetDeviceMemoryOpaqueCaptureAddress, vkGetPhysicalDeviceToolProperties, vkCreatePrivateDataSlot, vkDestroyPrivateDataSlot, vkSetPrivateData, vkGetPrivateData, vkCmdSetEvent2, vkCmdResetEvent2, vkCmdWaitEvents2, vkCmdPipelineBarrier2, vkCmdWriteTimestamp2, vkQueueSubmit2, vkCmdCopyBuffer2, vkCmdCopyImage2, vkCmdCopyBufferToImage2, vkCmdCopyImageToBuffer2, vkCmdBlitImage2, vkCmdResolveImage2, vkCmdBeginRendering, vkCmdEndRendering, vkCmdSetCullMode, vkCmdSetFrontFace, vkCmdSetPrimitiveTopology, vkCmdSetViewportWithCount, vkCmdSetScissorWithCount, vkCmdBindVertexBuffers2, vkCmdSetDepthTestEnable, vkCmdSetDepthWriteEnable, vkCmdSetDepthCompareOp, vkCmdSetDepthBoundsTestEnable, vkCmdSetStencilTestEnable, vkCmdSetStencilOp, vkCmdSetRasterizerDiscardEnable, vkCmdSetDepthBiasEnable, vkCmdSetPrimitiveRestartEnable, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, vkGetDisplayModeProperties2KHR, vkGetDisplayPlaneCapabilities2KHR, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateSamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, vkCreateDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, vkUpdateDescriptorSetWithTemplate, vkGetDescriptorSetLayoutSupport, vkGetDeviceGroupPeerMemoryFeatures, vkCmdSetDeviceMask, vkCmdDispatchBase, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateRenderPass2, vkCmdBeginRenderPass2, vkCmdNextSubpass2, vkCmdEndRenderPass2, vkGetSemaphoreCounterValue, vkWaitSemaphores, vkSignalSemaphore, vkGetBufferDeviceAddress, vkGetBufferOpaqueCaptureAddress, vkGetDeviceMemoryOpaqueCaptureAddress, vkGetPhysicalDeviceToolProperties, vkCreatePrivateDataSlot, vkDestroyPrivateDataSlot, vkSetPrivateData, vkGetPrivateData, vkCmdSetEvent2, vkCmdResetEvent2, vkCmdWaitEvents2, vkCmdPipelineBarrier2, vkCmdWriteTimestamp2, vkQueueSubmit2, vkCmdCopyBuffer2, vkCmdCopyImage2, vkCmdCopyBufferToImage2, vkCmdCopyImageToBuffer2, vkCmdBlitImage2, vkCmdResolveImage2, vkCmdBeginRendering, vkCmdEndRendering, vkCmdSetCullMode, vkCmdSetFrontFace, vkCmdSetPrimitiveTopology, vkCmdSetViewportWithCount, vkCmdSetScissorWithCount, vkCmdBindVertexBuffers2, vkCmdSetDepthTestEnable, vkCmdSetDepthWriteEnable, vkCmdSetDepthCompareOp, vkCmdSetDepthBoundsTestEnable, vkCmdSetStencilTestEnable, vkCmdSetStencilOp, vkCmdSetRasterizerDiscardEnable, vkCmdSetDepthBiasEnable, vkCmdSetPrimitiveRestartEnable, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, vkGetDisplayModeProperties2KHR, vkGetDisplayPlaneCapabilities2KHR, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateSamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, vkCreateDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, vkUpdateDescriptorSetWithTemplate, vkGetDescriptorSetLayoutSupport, vkGetDeviceGroupPeerMemoryFeatures, vkCmdSetDeviceMask, vkCmdDispatchBase, vkGetImageMemoryRequirements2, vkGetBufferMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetPhysicalDeviceMemoryProperties2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateRenderPass2, vkCmdBeginRenderPass2, vkCmdNextSubpass2, vkCmdEndRenderPass2, vkGetSemaphoreCounterValue, vkWaitSemaphores, vkSignalSemaphore, vkGetBufferDeviceAddress, vkGetBufferOpaqueCaptureAddress, vkGetDeviceMemoryOpaqueCaptureAddress, vkGetPhysicalDeviceToolProperties, vkCreatePrivateDataSlot, vkDestroyPrivateDataSlot, vkSetPrivateData, vkGetPrivateData, vkCmdSetEvent2, vkCmdResetEvent2, vkCmdWaitEvents2, vkCmdPipelineBarrier2, vkCmdWriteTimestamp2, vkQueueSubmit2, vkCmdCopyBuffer2, vkCmdCopyImage2, vkCmdCopyBufferToImage2, vkCmdCopyImageToBuffer2, vkCmdBlitImage2, vkCmdResolveImage2, vkCmdBeginRendering, vkCmdEndRendering
```

### Заполнение указателей

```cpp
// После volkInitialize()
vkCreateInstance != nullptr  // Глобальные функции

// После volkLoadInstance(instance)
vkCreateDevice != nullptr    // Instance функции
vkDestroyInstance != nullptr

// После volkLoadDevice(device)
vkCmdDraw != nullptr         // Device функции (прямые)
vkQueueSubmit != nullptr
```

---

## VolkDeviceTable

Структура для хранения указателей device-функций. Используется при работе с несколькими `VkDevice`.

### Объявление

```cpp
struct VolkDeviceTable {
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkCreateBuffer vkCreateBuffer;
    // Остальные ~400 device функций Vulkan: vkCmdDraw, vkCmdDrawIndexed, vkCmdDrawIndirect, vkCmdDrawIndexedIndirect, vkCmdDispatch, vkCmdDispatchIndirect, vkCmdCopyBuffer, vkCmdCopyImage, vkCmdBlitImage, vkCmdCopyBufferToImage, vkCmdCopyImageToBuffer, vkCmdUpdateBuffer, vkCmdFillBuffer, vkCmdClearColorImage, vkCmdClearDepthStencilImage, vkCmdClearAttachments, vkCmdResolveImage, vkCmdSetEvent, vkCmdResetEvent, vkCmdWaitEvents, vkCmdPipelineBarrier, vkCmdBeginQuery, vkCmdEndQuery, vkCmdResetQueryPool, vkCmdWriteTimestamp, vkCmdCopyQueryPoolResults, vkCmdPushConstants, vkCmdBeginRenderPass, vkCmdNextSubpass, vkCmdEndRenderPass, vkCmdExecuteCommands, vkCmdBindPipeline, vkCmdSetViewport, vkCmdSetScissor, vkCmdSetLineWidth, vkCmdSetDepthBias, vkCmdSetBlendConstants, vkCmdSetDepthBounds, vkCmdSetStencilCompareMask, vkCmdSetStencilWriteMask, vkCmdSetStencilReference, vkCmdBindDescriptorSets, vkCmdBindIndexBuffer, vkCmdBindVertexBuffers, vkCmdSetCullMode, vkCmdSetFrontFace, vkCmdSetPrimitiveTopology, vkCmdSetViewportWithCount, vkCmdSetScissorWithCount, vkCmdBindVertexBuffers2, vkCmdSetDepthTestEnable, vkCmdSetDepthWriteEnable, vkCmdSetDepthCompareOp, vkCmdSetDepthBoundsTestEnable, vkCmdSetStencilTestEnable, vkCmdSetStencilOp, vkCmdSetRasterizerDiscardEnable, vkCmdSetDepthBiasEnable, vkCmdSetPrimitiveRestartEnable, vkCmdSetEvent2, vkCmdResetEvent2, vkCmdWaitEvents2, vkCmdPipelineBarrier2, vkCmdWriteTimestamp2, vkCmdCopyBuffer2, vkCmdCopyImage2, vkCmdCopyBufferToImage2, vkCmdCopyImageToBuffer2, vkCmdBlitImage2, vkCmdResolveImage2, vkCmdBeginRendering, vkCmdEndRendering, vkCmdSetFragmentShadingRateKHR, vkCmdSetFragmentShadingRateEnumNV, vkCmdDrawMeshTasksEXT, vkCmdDrawMeshTasksIndirectEXT, vkCmdDrawMeshTasksIndirectCountEXT, vkCmdTraceRaysKHR, vkCmdTraceRaysIndirectKHR, vkCmdBuildAccelerationStructuresKHR, vkCmdBuildAccelerationStructuresIndirectKHR, vkCmdCopyAccelerationStructureKHR, vkCmdCopyAccelerationStructureToMemoryKHR, vkCmdCopyMemoryToAccelerationStructureKHR, vkCmdWriteAccelerationStructuresPropertiesKHR, vkCmdTraceRaysNV, vkCmdBuildAccelerationStructureNV, vkCmdCopyAccelerationStructureNV, vkCmdWriteAccelerationStructuresPropertiesNV, vkCmdSetRayTracingPipelineStackSizeKHR, vkCmdBeginDebugUtilsLabelEXT, vkCmdEndDebugUtilsLabelEXT, vkCmdInsertDebugUtilsLabelEXT, vkQueueSubmit, vkQueueSubmit2, vkQueueWaitIdle, vkQueueBindSparse, vkQueuePresentKHR, vkDeviceWaitIdle, vkAllocateMemory, vkFreeMemory, vkMapMemory, vkUnmapMemory, vkFlushMappedMemoryRanges, vkInvalidateMappedMemoryRanges, vkGetDeviceMemoryCommitment, vkBindBufferMemory, vkBindImageMemory, vkGetBufferMemoryRequirements, vkGetImageMemoryRequirements, vkGetImageSparseMemoryRequirements, vkGetBufferMemoryRequirements2, vkGetImageMemoryRequirements2, vkGetImageSparseMemoryRequirements2, vkGetDeviceBufferMemoryRequirements, vkGetDeviceImageMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, vkCreateBuffer, vkDestroyBuffer, vkCreateBufferView, vkDestroyBufferView, vkCreateImage, vkDestroyImage, vkGetImageSubresourceLayout, vkCreateImageView, vkDestroyImageView, vkCreateSampler, vkDestroySampler, vkCreateSamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, vkCreateShaderModule, vkDestroyShaderModule, vkCreatePipelineCache, vkDestroyPipelineCache, vkGetPipelineCacheData, vkMergePipelineCaches, vkCreateGraphicsPipelines, vkCreateComputePipelines, vkDestroyPipeline, vkCreatePipelineLayout, vkDestroyPipelineLayout, vkCreateDescriptorSetLayout, vkDestroyDescriptorSetLayout, vkCreateDescriptorPool, vkDestroyDescriptorPool, vkResetDescriptorPool, vkAllocateDescriptorSets, vkFreeDescriptorSets, vkUpdateDescriptorSets, vkUpdateDescriptorSetWithTemplate, vkCreateDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, vkGetDescriptorSetLayoutSupport, vkCreateFramebuffer, vkDestroyFramebuffer, vkCreateRenderPass, vkDestroyRenderPass, vkCreateRenderPass2, vkDestroyRenderPass2, vkGetRenderAreaGranularity, vkCreateCommandPool, vkDestroyCommandPool, vkResetCommandPool, vkAllocateCommandBuffers, vkFreeCommandBuffers, vkBeginCommandBuffer, vkEndCommandBuffer, vkResetCommandBuffer, vkCreateFence, vkDestroyFence, vkResetFences, vkGetFenceStatus, vkWaitForFences, vkCreateSemaphore, vkDestroySemaphore, vkGetSemaphoreCounterValue, vkWaitSemaphores, vkSignalSemaphore, vkCreateEvent, vkDestroyEvent, vkGetEventStatus, vkSetEvent, vkResetEvent, vkCreateQueryPool, vkDestroyQueryPool, vkGetQueryPoolResults, vkCreateSwapchainKHR, vkDestroySwapchainKHR, vkGetSwapchainImagesKHR, vkAcquireNextImageKHR, vkAcquireNextImage2KHR, vkGetSwapchainStatusKHR, vkCreateSharedSwapchainsKHR, vkCreateWin32SurfaceKHR, vkGetPhysicalDeviceWin32PresentationSupportKHR, vkGetPhysicalDeviceSurfaceSupportKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceFormatsKHR, vkGetPhysicalDeviceSurfacePresentModesKHR, vkGetPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceDisplayPropertiesKHR, vkGetPhysicalDeviceDisplayPlanePropertiesKHR, vkGetDisplayPlaneSupportedDisplaysKHR, vkGetDisplayModePropertiesKHR, vkCreateDisplayModeKHR, vkGetDisplayPlaneCapabilitiesKHR, vkCreateDisplayPlaneSurfaceKHR, vkGetPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, vkGetDisplayModeProperties2KHR, vkGetDisplayPlaneCapabilities2KHR, vkCreateDebugReportCallbackEXT, vkDestroyDebugReportCallbackEXT, vkDebugReportMessageEXT, vkCreateDebugUtilsMessengerEXT, vkDestroyDebugUtilsMessengerEXT, vkSubmitDebugUtilsMessageEXT, vkSetDebugUtilsObjectNameEXT, vkSetDebugUtilsObjectTagEXT, vkGetPhysicalDeviceToolPropertiesEXT, vkGetPhysicalDeviceToolProperties, vkCreateAccelerationStructureKHR, vkDestroyAccelerationStructureKHR, vkBuildAccelerationStructuresKHR, vkCopyAccelerationStructureKHR, vkCopyAccelerationStructureToMemoryKHR, vkCopyMemoryToAccelerationStructureKHR, vkWriteAccelerationStructuresPropertiesKHR, vkGetAccelerationStructureDeviceAddressKHR, vkGetDeviceAccelerationStructureCompatibilityKHR, vkGetAccelerationStructureBuildSizesKHR, vkCreateRayTracingPipelinesKHR, vkGetRayTracingShaderGroupHandlesKHR, vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, vkGetRayTracingShaderGroupStackSizeKHR, vkCreateAccelerationStructureNV, vkDestroyAccelerationStructureNV, vkGetAccelerationStructureHandleNV, vkGetAccelerationStructureMemoryRequirementsNV, vkBindAccelerationStructureMemoryNV, vkCompileDeferredNV, vkGetMemoryWin32HandleNV, vkGetMemoryWin32HandleKHR, vkGetMemoryWin32HandlePropertiesKHR, vkGetMemoryFdKHR, vkGetMemoryFdPropertiesKHR, vkImportFenceWin32HandleKHR, vkGetFenceWin32HandleKHR, vkImportFenceFdKHR, vkGetFenceFdKHR, vkImportSemaphoreWin32HandleKHR, vkGetSemaphoreWin32HandleKHR, vkImportSemaphoreFdKHR, vkGetSemaphoreFdKHR, vkGetPhysicalDeviceExternalBufferProperties, vkGetPhysicalDeviceExternalFenceProperties, vkGetPhysicalDeviceExternalSemaphoreProperties, vkGetPhysicalDeviceFeatures2, vkGetPhysicalDeviceProperties2, vkGetPhysicalDeviceFormatProperties2, vkGetPhysicalDeviceImageFormatProperties2, vkGetPhysicalDeviceQueueFamilyProperties2, vkGetPhysicalDeviceMemoryProperties2, vkGetPhysicalDeviceSparseImageFormatProperties2, vkGetPhysicalDeviceFragmentShadingRatesKHR, vkGetPhysicalDeviceSurfaceCapabilities2EXT, vkGetPhysicalDevicePresentRectanglesKHR, vkGetDeviceGroupSurfacePresentModesKHR, vkGetDeviceGroupPeerMemoryFeatures, vkGetDeviceQueue2, vkTrimCommandPool, vkGetBufferDeviceAddress, vkGetBufferOpaqueCaptureAddress, vkGetDeviceMemoryOpaqueCaptureAddress, vkCreatePrivateDataSlot, vkDestroyPrivateDataSlot, vkSetPrivateData, vkGetPrivateData
};
```

### Использование

```cpp
VolkDeviceTable table;
volkLoadDeviceTable(&table, device);

// Вызов через таблицу
table.vkCmdDraw(cmd, vertexCount, 1, 0, 0);
```

---

## VK_NO_PROTOTYPES

Макрос Vulkan, запрещающий объявления прототипов функций в `vulkan.h`.

### Без макроса

```cpp
// vulkan.h объявляет:
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);

// Линкер ожидает реализацию в vulkan-1.dll
```

### С макросом

```cpp
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// vulkan.h объявляет только типы:
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);

// Нет прототипов — линкер не ищет реализации
```

Volk заполняет указатели во время выполнения, поэтому прототипы не нужны.

---

## Validation Layers

Volk полностью совместим с validation layers.

### Как это работает

```cpp
const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
createInfo.ppEnabledLayerNames = layers;
createInfo.enabledLayerCount = 1;

vkCreateInstance(&createInfo, nullptr, &instance);
volkLoadInstance(instance);
```

### Dispatch с layers

```
Приложение
    ↓
volk указатель
    ↓
Validation Layer (перехват)
    ↓
Драйвер
```

---

## Выбор стратегии

### Одно устройство

```cpp
volkInitialize();
vkCreateInstance(...);
volkLoadInstance(instance);
vkCreateDevice(...);
volkLoadDevice(device);  // Глобальные указатели → драйвер
```

### Несколько устройств

```cpp
volkInitialize();
vkCreateInstance(...);
volkLoadInstanceOnly(instance);  // Только instance-функции

for (auto& device : devices) {
    vkCreateDevice(...);
    volkLoadDeviceTable(&device.table, device.handle);
}

// Вызовы через таблицы
devices[0].table.vkCmdDraw(...);
devices[1].table.vkCmdDraw(...);
```

---

## Почему Volk?

**Для понимания:** Представьте, что вы звоните другу. Можно набрать номер напрямую (Volk), а можно позвонить через
оператора, который переключит вас на нужный номер (Vulkan loader). Разница в 7% производительности — это как раз плата
за услуги оператора.

### Проблемы стандартного Vulkan loader:

1. **Dispatch overhead:** Каждый вызов проходит через 2-3 уровня индирекции
2. **Зависимость от DLL:** Приложение не запустится без `vulkan-1.dll`
3. **Нет контроля:** Нельзя выбрать драйвер в runtime

### Что даёт Volk:

1. **Прямые вызовы:** `vkCmdDraw` → драйвер, без промежуточных звеньев
2. **Динамическая загрузка:** Проверка Vulkan в runtime, а не при запуске
3. **Контроль:** Можно загружать разные драйверы для разных устройств
4. **Производительность:** До 7% прироста для device-intensive workloads

## Примеры с современным C++26

Для использования с современным C++26, Volk можно обернуть в RAII-классы с `std::expected`:

```cpp
// volk/result.hpp
#pragma once
#include <expected>
#include <print>
#include <string>

enum class VolkError {
    InitializeFailed,
    LoadInstanceFailed,
    LoadDeviceFailed,
    NoVulkanLoader,
    InvalidVersion
};

template<typename T>
using VolkResult = std::expected<T, VolkError>;

inline std::string to_string(VolkError error) {
    switch (error) {
        case VolkError::InitializeFailed: return "volkInitialize failed";
        case VolkError::LoadInstanceFailed: return "volkLoadInstance failed";
        case VolkError::LoadDeviceFailed: return "volkLoadDevice failed";
        case VolkError::NoVulkanLoader: return "No Vulkan loader found";
        case VolkError::InvalidVersion: return "Unsupported Vulkan version";
        default: return "Unknown Volk error";
    }
}
```

### RAII-обёртка для Volk:

```cpp
// volk/context.hpp
#pragma once
#include <volk.h>
#include <print>
#include <expected>
#include "result.hpp"

class VolkContext {
public:
    VolkContext() = default;
    ~VolkContext() = default;

    // Non-copyable
    VolkContext(const VolkContext&) = delete;
    VolkContext& operator=(const VolkContext&) = delete;

    // Movable
    VolkContext(VolkContext&& other) noexcept = default;
    VolkContext& operator=(VolkContext&& other) noexcept = default;

    VolkResult<void> initialize() {
        if (volkInitialize() != VK_SUCCESS) {
            std::println(stderr, "volkInitialize failed");
            return std::unexpected(VolkError::InitializeFailed);
        }

        uint32_t version = volkGetInstanceVersion();
        std::println("Vulkan loader version: {}.{}.{}",
                     VK_VERSION_MAJOR(version),
                     VK_VERSION_MINOR(version),
                     VK_VERSION_PATCH(version));

        if (version < VK_MAKE_VERSION(1, 3, 0)) {
            std::println(stderr, "Vulkan 1.3+ required, got {}.{}.{}",
                         VK_VERSION_MAJOR(version),
                         VK_VERSION_MINOR(version),
                         VK_VERSION_PATCH(version));
            return std::unexpected(VolkError::InvalidVersion);
        }

        return {};
    }

    VolkResult<void> load_instance(VkInstance instance) {
        volkLoadInstance(instance);

        // Проверяем, что ключевые функции загружены
        if (!vkCreateDevice || !vkDestroyInstance) {
            return std::unexpected(VolkError::LoadInstanceFailed);
        }

        std::println("Loaded {} instance functions", count_instance_functions());
        return {};
    }

    VolkResult<void> load_device(VkDevice device) {
        volkLoadDevice(device);

        // Проверяем, что ключевые device функции загружены
        if (!vkCmdDraw || !vkQueueSubmit) {
            return std::unexpected(VolkError::LoadDeviceFailed);
        }

        std::println("Loaded {} device functions (direct driver calls)",
                     count_device_functions());
        return {};
    }

private:
    size_t count_instance_functions() const {
        // Примерная логика подсчёта
        return 150;  // ~150 instance функций в Vulkan 1.3
    }

    size_t count_device_functions() const {
        return 300;  // ~300 device функций в Vulkan 1.3
    }
};
```

## Производительность

### Dispatch Overhead

При каждом вызове Vulkan функции через loader:

```
Приложение
    ↓
vulkan-1.dll (loader) — диспетчеризация
    ↓
Validation Layers — проверки (если включены)
    ↓
Драйвер GPU — выполнение
```

Каждый переход добавляет накладные расходы.

### Величина overhead

| Тип нагрузки                                | Overhead через loader  |
|---------------------------------------------|------------------------|
| Device-intensive (vkCmdDraw, vkCmdDispatch) | До 7%                  |
| Instance-intensive (vkCreateDevice)         | Минимальный            |
| Смешанная                                   | Зависит от соотношения |

### Когда использовать volkLoadDevice

**Обязательно:**

- Частые draw/dispatch вызовы (более 100 на кадр)
- Compute-intensive приложения
- Real-time рендеринг

**Не критично:**

- Редкие Vulkan вызовы
- UI-приложения
- Tools и утилиты

---

