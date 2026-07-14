#include "volk.h" // VOLK_HEADER_VERSION must be visible to vk_mem_alloc.h for vmaImportVulkanFunctionsFromVolk

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
// ReSharper disable once CppUnusedIncludeDirective
#include "vk_mem_alloc.h"