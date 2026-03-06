# Vulkan Quickstart

**🟢 Уровень 1: Базовый** — Минимальный пример для понимания структуры Vulkan-приложения.

Полный рабочий пример отрисовки треугольника с использованием Dynamic Rendering (Vulkan 1.2+).

---

## Содержание

1. [Полный пример: Vulkan Triangle](#полный-пример-vulkan-triangle)
2. [Структура Vulkan-приложения](#структура-vulkan-приложения)
3. [Основные шаги инициализации](#основные-шаги-инициализации)
4. [Рендер-луп](#рендер-луп)
5. [Очистка ресурсов](#очистка-ресурсов)
6. [Шейдеры](#шейдеры)

---

## Полный пример: Vulkan Triangle

### Минимальные зависимости

- SDL3 (window, surface)
- volk (Vulkan function loader)
- VMA (memory allocation)
- Vulkan 1.2+ (Dynamic Rendering support)

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(VulkanTriangle)

set(CMAKE_CXX_STANDARD 20)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# Volk
add_subdirectory(external/volk)

# VMA
add_subdirectory(external/VMA)

# SDL3
add_subdirectory(external/SDL)

add_executable(VulkanTriangle src/main.cpp)

target_link_libraries(VulkanTriangle PRIVATE
    Vulkan::Vulkan
    volk
    GPUOpen::VulkanMemoryAllocator
    SDL3::SDL3
)

# Копируем шейдеры рядом с исполняемым файлом
add_custom_command(TARGET VulkanTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.spv"
    "$<TARGET_FILE_DIR:VulkanTriangle>"
)
add_custom_command(TARGET VulkanTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.spv"
    "$<TARGET_FILE_DIR:VulkanTriangle>"
)
```

### main.cpp

```cpp
// ProjectV Example: Vulkan Triangle with Dynamic Rendering
// Description: Vulkan 1.2 треугольник с Dynamic Rendering вместо RenderPass/Framebuffer.
//              Демонстрирует: modern Vulkan patterns, vkCmdBeginRendering, pipeline creation.

#define NOMINMAX
#define VK_NO_PROTOTYPES
#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Состояние приложения (RAII)
// ============================================================================

struct AppState {
	SDL_Window *window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	std::uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkShaderModule vertModule = VK_NULL_HANDLE;
	VkShaderModule fragModule = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;

	~AppState() { cleanup(); }

	void cleanup()
	{
		if (device) {
			vkDeviceWaitIdle(device);
		}

		if (imageAvailableSemaphore) {
			vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		}
		if (renderFinishedSemaphore) {
			vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		}
		if (inFlightFence) {
			vkDestroyFence(device, inFlightFence, nullptr);
		}
		if (commandPool) {
			vkDestroyCommandPool(device, commandPool, nullptr);
		}
		if (pipeline) {
			vkDestroyPipeline(device, pipeline, nullptr);
		}
		if (pipelineLayout) {
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		}
		if (vertModule) {
			vkDestroyShaderModule(device, vertModule, nullptr);
		}
		if (fragModule) {
			vkDestroyShaderModule(device, fragModule, nullptr);
		}
		for (const VkImageView iv : swapchainImageViews) {
			vkDestroyImageView(device, iv, nullptr);
		}
		if (swapchain) {
			vkDestroySwapchainKHR(device, swapchain, nullptr);
		}
		if (allocator) {
			vmaDestroyAllocator(allocator);
		}
		if (device) {
			vkDestroyDevice(device, nullptr);
		}
		if (surface && instance) {
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}
		if (window) {
			SDL_DestroyWindow(window);
		}
		if (instance) {
			vkDestroyInstance(instance, nullptr);
		}
		SDL_Quit();
	}

	AppState(const AppState &) = delete;
	AppState &operator=(const AppState &) = delete;
};

// ============================================================================
// Helper functions
// ============================================================================

static std::vector<std::uint32_t> load_spirv(const char *path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) {
		return {};
	}
	auto size = f.tellg();
	f.seekg(0);
	if (size <= 0 || (size % 4) != 0) {
		return {};
	}
	std::vector<std::uint32_t> code(size / 4);
	f.read(reinterpret_cast<char *>(code.data()), size);
	return code;
}

static VkShaderModule create_shader_module(VkDevice device, const std::vector<std::uint32_t> &code)
{
	if (code.empty()) {
		return VK_NULL_HANDLE;
	}
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code.size() * sizeof(std::uint32_t);
	info.pCode = code.data();
	VkShaderModule module = VK_NULL_HANDLE;
	vkCreateShaderModule(device, &info, nullptr, &module);
	return module;
}

// ============================================================================
// SDL callbacks
// ============================================================================

SDL_AppResult SDL_AppInit(void **appstate, int /*argc*/, char ** /*argv*/)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	auto state = std::make_unique<AppState>();

	state->window = SDL_CreateWindow("Vulkan Triangle - ProjectV", 1280, 720, SDL_WINDOW_VULKAN);
	if (!state->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		return SDL_APP_FAILURE;
	}

	// Instance extensions
	Uint32 extCount = 0;
	const char *const *extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!extNames) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	std::vector<const char *> instanceExtensions(extNames, extNames + extCount);
	instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &state->instance) != VK_SUCCESS) {
		SDL_Log("vkCreateInstance failed");
		return SDL_APP_FAILURE;
	}
	volkLoadInstance(state->instance);

	if (!SDL_Vulkan_CreateSurface(state->window, state->instance, nullptr, &state->surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// Select physical device
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, devices.data());

	state->queueFamilyIndex = UINT32_MAX;
	for (VkPhysicalDevice pd : devices) {
		std::uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, families.data());

		for (std::uint32_t i = 0; i < familyCount; ++i) {
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
				SDL_Vulkan_GetPresentationSupport(state->instance, pd, i)) {
				state->physicalDevice = pd;
				state->queueFamilyIndex = i;
				break;
			}
		}
		if (state->queueFamilyIndex != UINT32_MAX) {
			break;
		}
	}
	if (state->queueFamilyIndex == UINT32_MAX) {
		SDL_Log("No suitable physical device found");
		return SDL_APP_FAILURE;
	}

	// Check Dynamic Rendering support
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
	dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &dynamicRenderingFeatures;
	vkGetPhysicalDeviceFeatures2(state->physicalDevice, &features2);

	if (!dynamicRenderingFeatures.dynamicRendering) {
		SDL_Log("Dynamic Rendering not supported");
		return SDL_APP_FAILURE;
	}

	// Create device
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = state->queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = 2;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	deviceCreateInfo.pNext = &dynamicRenderingFeatures;

	if (vkCreateDevice(state->physicalDevice, &deviceCreateInfo, nullptr, &state->device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		return SDL_APP_FAILURE;
	}
	volkLoadDevice(state->device);

	vkGetDeviceQueue(state->device, state->queueFamilyIndex, 0, &state->queue);

	// VMA
	VmaAllocatorCreateInfo allocInfo = {};
	allocInfo.physicalDevice = state->physicalDevice;
	allocInfo.device = state->device;
	allocInfo.instance = state->instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
#ifdef VOLK_HEADER_VERSION
	VmaVulkanFunctions vulkanFunctions = {};
	vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
	allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	if (vmaCreateAllocator(&allocInfo, &state->allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		return SDL_APP_FAILURE;
	}

	// Swapchain
	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &caps);

	int w = 1280, h = 720;
	SDL_GetWindowSizeInPixels(state->window, &w, &h);

	state->extent.width = static_cast<std::uint32_t>(w);
	state->extent.height = static_cast<std::uint32_t>(h);
	state->extent.width = std::clamp(state->extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	state->extent.height = std::clamp(state->extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

	std::uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats.data());

	VkSurfaceFormatKHR surfaceFormat = formats[0];
	for (const auto &f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = f;
			break;
		}
	}
	state->swapchainFormat = surfaceFormat.format;

	std::uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount,
											  presentModes.data());

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR pm : presentModes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = pm;
			break;
		}
	}

	std::uint32_t imageCount = std::max(2u, caps.minImageCount);
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = state->surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = surfaceFormat.format;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = state->extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = caps.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(state->device, &swapchainInfo, nullptr, &state->swapchain) != VK_SUCCESS) {
		SDL_Log("vkCreateSwapchainKHR failed");
		return SDL_APP_FAILURE;
	}

	std::uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, nullptr);
	state->swapchainImages.resize(actualImageCount);
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, state->swapchainImages.data());

	state->swapchainImageViews.resize(actualImageCount);
	for (std::uint32_t i = 0; i < actualImageCount; ++i) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = state->swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = surfaceFormat.format;
		viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		if (vkCreateImageView(state->device, &viewInfo, nullptr, &state->swapchainImageViews[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateImageView failed");
			return SDL_APP_FAILURE;
		}
	}

	// Load shaders
	char shaderPath[1024];
	const char *basePath = SDL_GetBasePath();
	if (basePath) {
		SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.vert.spv", basePath);
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));
	} else {
		SDL_strlcpy(shaderPath, "triangle.vert.spv", sizeof(shaderPath));
	}

	std::vector<std::uint32_t> vertCode = load_spirv(shaderPath);
	if (vertCode.empty()) {
		SDL_Log("Failed to load triangle.vert.spv. Ensure shaders are next to the executable.");
		return SDL_APP_FAILURE;
	}

	basePath = SDL_GetBasePath();
	if (basePath) {
		SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.frag.spv", basePath);
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));
	} else {
		SDL_strlcpy(shaderPath, "triangle.frag.spv", sizeof(shaderPath));
	}
	std::vector<std::uint32_t> fragCode = load_spirv(shaderPath);
	if (fragCode.empty()) {
		SDL_Log("Failed to load triangle.frag.spv");
		return SDL_APP_FAILURE;
	}

	state->vertModule = create_shader_module(state->device, vertCode);
	state->fragModule = create_shader_module(state->device, fragCode);
	if (!state->vertModule || !state->fragModule) {
		SDL_Log("Failed to create shader modules");
		return SDL_APP_FAILURE;
	}

	// Pipeline Layout
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(state->device, &layoutInfo, nullptr, &state->pipelineLayout) != VK_SUCCESS) {
		SDL_Log("vkCreatePipelineLayout failed");
		return SDL_APP_FAILURE;
	}

	// Pipeline with Dynamic Rendering
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = state->vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = state->fragModule;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterization = {};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlend = {};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &blendAttachment;

	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkFormat colorAttachmentFormat = surfaceFormat.format;
	VkPipelineRenderingCreateInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;

	VkGraphicsPipelineCreateInfo pipeInfo = {};
	pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeInfo.stageCount = 2;
	pipeInfo.pStages = stages;
	pipeInfo.pVertexInputState = &vertexInput;
	pipeInfo.pInputAssemblyState = &inputAssembly;
	pipeInfo.pViewportState = &viewportState;
	pipeInfo.pRasterizationState = &rasterization;
	pipeInfo.pColorBlendState = &colorBlend;
	pipeInfo.pDynamicState = &dynamicState;
	pipeInfo.layout = state->pipelineLayout;
	pipeInfo.pNext = &renderingInfo;
	pipeInfo.renderPass = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(state->device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &state->pipeline) !=
		VK_SUCCESS) {
		SDL_Log("vkCreateGraphicsPipelines failed");
		return SDL_APP_FAILURE;
	}

	// Command pool & buffer
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = state->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(state->device, &poolInfo, nullptr, &state->commandPool) != VK_SUCCESS) {
		SDL_Log("Command pool failed");
		return SDL_APP_FAILURE;
	}

	VkCommandBufferAllocateInfo cmdAllocInfo = {};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = state->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(state->device, &cmdAllocInfo, &state->commandBuffer) != VK_SUCCESS) {
		SDL_Log("Command buffer failed");
		return SDL_APP_FAILURE;
	}

	// Sync objects
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(state->device, &fenceInfo, nullptr, &state->inFlightFence) != VK_SUCCESS) {
		SDL_Log("Sync objects failed");
		return SDL_APP_FAILURE;
	}

	*appstate = state.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
		return SDL_APP_SUCCESS;
	}
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
		return SDL_APP_SUCCESS;
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	const AppState *state = static_cast<AppState *>(appstate);
	if (!state) {
		return SDL_APP_CONTINUE;
	}

	vkWaitForFences(state->device, 1, &state->inFlightFence, VK_TRUE, UINT64_MAX);

	std::uint32_t imageIndex = 0;
	VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
												state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		return SDL_APP_CONTINUE;
	}

	vkResetFences(state->device, 1, &state->inFlightFence);
	vkResetCommandBuffer(state->commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(state->commandBuffer, &beginInfo);

	// Dynamic Rendering
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = state->swapchainImageViews[imageIndex];
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.2f, 1.0f}};

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = {{0, 0}, {state->extent.width, state->extent.height}};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(state->commandBuffer, &renderingInfo);

	vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);

	VkViewport viewport = {0,	 0,	  static_cast<float>(state->extent.width), static_cast<float>(state->extent.height),
						   0.0f, 1.0f};
	vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {{0, 0}, {state->extent.width, state->extent.height}};
	vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);

	vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(state->commandBuffer);

	vkEndCommandBuffer(state->commandBuffer);

	constexpr VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &state->imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = &stageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &state->commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &state->renderFinishedSemaphore;

	if (vkQueueSubmit(state->queue, 1, &submitInfo, state->inFlightFence) != VK_SUCCESS) {
		SDL_Log("vkQueueSubmit failed");
		return SDL_APP_FAILURE;
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &state->renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &state->swapchain;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(state->queue, &presentInfo);
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult /*result*/)
{
	auto *state = static_cast<AppState *>(appstate);
	if (state) {
		delete state;
	}
}
```

---

## Шейдеры

### triangle.vert (GLSL)

```glsl
#version 450

// Hardcoded triangle vertices (no vertex buffer needed)
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(-0.5, 0.5),
    vec2(0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
```

### triangle.frag (GLSL)

```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

### Компиляция шейдеров

```bash
glslangValidator -V triangle.vert -o triangle.vert.spv
glslangValidator -V triangle.frag -o triangle.frag.spv
```

---

## Структура Vulkan-приложения

```
┌─────────────────────────────────────────────┐
│              Vulkan Application             │
├─────────────────────────────────────────────┤
│  1. Instance + Physical Device Selection     │
│  2. Logical Device + Queues                  │
│  3. Surface + Swapchain                      │
│  4. Pipeline + Shaders                       │
│  5. Command Buffers + Synchronization        │
│  6. Render Loop                              │
│  7. Cleanup                                  │
└─────────────────────────────────────────────┘
```

---

## Основные шаги инициализации

### Шаг 1: Instance

```cpp
VkApplicationInfo appInfo = {};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "Triangle App";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_2;

VkInstanceCreateInfo instanceCreateInfo = {};
instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
instanceCreateInfo.pApplicationInfo = &appInfo;

vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
```

### Шаг 2: Physical Device

```cpp
std::uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
std::vector<VkPhysicalDevice> devices(deviceCount);
vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

// Выбор GPU с поддержкой graphics и presentation
for (VkPhysicalDevice pd : devices) {
    // Проверка queue families
    // ...
}
```

### Шаг 3: Device & Queues

```cpp
float queuePriority = 1.0f;

VkDeviceQueueCreateInfo queueCreateInfo = {};
queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
queueCreateInfo.queueCount = 1;
queueCreateInfo.pQueuePriorities = &queuePriority;

VkDeviceCreateInfo deviceCreateInfo = {};
deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceCreateInfo.queueCreateInfoCount = 1;
deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
```

---

## Рендер-луп

```cpp
void drawFrame() {
    // 1. Ожидание предыдущего кадра
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    // 2. Получение следующего изображения swapchain
    std::uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                          imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // 3. Запись команд
    recordCommandBuffer(imageIndex);

    // 4. Submit
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // ...
    vkQueueSubmit(queue, 1, &submitInfo, inFlightFence);

    // 5. Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    // ...
    vkQueuePresentKHR(queue, &presentInfo);
}
```

---

## Очистка ресурсов

```cpp
void cleanup() {
    vkDeviceWaitIdle(device);  // Ждать завершения всех операций

    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}
```

---

## Ключевые моменты

1. **Dynamic Rendering** — не требует RenderPass и Framebuffer (Vulkan 1.2+)
2. **volk** — статическая загрузка Vulkan функций без системного loader
3. **VMA** — управление памятью GPU через VulkanMemoryAllocator
4. **SDL callback архитектура** — современный подход к игровому циклу
5. **Порядок уничтожения** — обратный порядку создания
