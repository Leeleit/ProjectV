// Пример: Vulkan треугольник с Vulkan 1.4 и Dynamic Rendering (ProjectV Modern)
// Документация: docs/architecture/modern-vulkan-guide.md
// Шейдеры: docs/examples/shaders/triangle.vert, triangle.frag
// Уровень: 🟡 Средний (требует знания Vulkan basics)

// ============================================================================
// ВАЖНО: Этот пример использует Vulkan 1.4 с Dynamic Rendering вместо
// традиционного RenderPass/Framebuffer подхода. Это современный подход,
// рекомендованный для ProjectV.
// ============================================================================

#define NOMINMAX
#define VK_NO_PROTOTYPES
#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

// Tracy profiling (опционально)
// Для включения раскомментируйте следующую строку и добавьте Tracy в CMake
// #define TRACY_ENABLE
#ifdef TRACY_ENABLE
#include "TracyVulkan.hpp"
#endif

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

struct AppState {
	SDL_Window *window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	// ПРИМЕЧАНИЕ: Dynamic Rendering не требует RenderPass и Framebuffer
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkShaderModule vertModule = VK_NULL_HANDLE;
	VkShaderModule fragModule = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;

	// Tracy контекст для профилирования (опционально)
#ifdef TRACY_ENABLE
	TracyVkCtx tracyContext = nullptr;
#endif
};

static std::vector<uint32_t> loadSpirv(const char *path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return {};
	auto size = f.tellg();
	f.seekg(0);
	if (size <= 0 || (size % 4) != 0)
		return {};
	std::vector<uint32_t> code(size / 4);
	f.read(reinterpret_cast<char *>(code.data()), size);
	return code;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t> &code)
{
	if (code.empty())
		return VK_NULL_HANDLE;
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code.size() * sizeof(uint32_t);
	info.pCode = code.data();
	VkShaderModule module = VK_NULL_HANDLE;
	vkCreateShaderModule(device, &info, nullptr, &module);
	return module;
}

static void cleanupAppState(AppState *state)
{
	vkDeviceWaitIdle(state->device);
	vkDestroySemaphore(state->device, state->imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(state->device, state->renderFinishedSemaphore, nullptr);
	vkDestroyFence(state->device, state->inFlightFence, nullptr);
	vkDestroyCommandPool(state->device, state->commandPool, nullptr);
	vkDestroyPipeline(state->device, state->pipeline, nullptr);
	vkDestroyPipelineLayout(state->device, state->pipelineLayout, nullptr);
	vkDestroyShaderModule(state->device, state->vertModule, nullptr);
	vkDestroyShaderModule(state->device, state->fragModule, nullptr);
	for (const VkImageView iv : state->swapchainImageViews)
		vkDestroyImageView(state->device, iv, nullptr);
	vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
	vmaDestroyAllocator(state->allocator);
	vkDestroyDevice(state->device, nullptr);
	vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
	if (state->window)
		SDL_DestroyWindow(state->window);
	vkDestroyInstance(state->instance, nullptr);

	// Tracy cleanup (опционально)
#ifdef TRACY_ENABLE
	if (state->tracyContext) {
		TracyVkDestroy(state->tracyContext);
	}
#endif
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Window *window = SDL_CreateWindow("Vulkan Triangle", 1280, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	Uint32 extCount = 0;
	const char *const *extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!extNames) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Добавляем расширение динамического рендеринга
	std::vector<const char *> instanceExtensions(extNames, extNames + extCount);
	instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2; // Минимум для Dynamic Rendering

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
		SDL_Log("vkCreateInstance failed");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}
	volkLoadInstance(instance);

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = UINT32_MAX;
	for (VkPhysicalDevice pd : devices) {
		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, families.data());
		for (uint32_t i = 0; i < familyCount; ++i) {
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
				SDL_Vulkan_GetPresentationSupport(instance, pd, i)) {
				physicalDevice = pd;
				queueFamilyIndex = i;
				break;
			}
		}
		if (queueFamilyIndex != UINT32_MAX)
			break;
	}
	if (queueFamilyIndex == UINT32_MAX) {
		SDL_Log("No suitable physical device found");
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Проверяем поддержку Dynamic Rendering
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
	dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &dynamicRenderingFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (!dynamicRenderingFeatures.dynamicRendering) {
		SDL_Log("Dynamic Rendering not supported");
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = queueFamilyIndex;
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

	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}
	volkLoadDevice(device);

	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	VmaAllocatorCreateInfo allocInfo = {};
	allocInfo.physicalDevice = physicalDevice;
	allocInfo.device = device;
	allocInfo.instance = instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
#ifdef VOLK_HEADER_VERSION
	VmaVulkanFunctions vulkanFunctions = {};
	vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
	allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	VmaAllocator allocator = VK_NULL_HANDLE;
	if (vmaCreateAllocator(&allocInfo, &allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

	int w = 1280, h = 720;
	SDL_GetWindowSizeInPixels(window, &w, &h);

	VkExtent2D extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

	VkSurfaceFormatKHR surfaceFormat = formats[0];
	for (const auto &f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = f;
			break;
		}
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR pm : presentModes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = pm;
			break;
		}
	}

	uint32_t imageCount = std::max(2u, caps.minImageCount);
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
		imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = surfaceFormat.format;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = caps.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS) {
		SDL_Log("vkCreateSwapchainKHR failed");
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, nullptr);
	std::vector<VkImage> swapchainImages(actualImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data());

	std::vector<VkImageView> swapchainImageViews(actualImageCount);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = surfaceFormat.format;
		viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateImageView failed");
			for (uint32_t j = 0; j < i; ++j)
				vkDestroyImageView(device, swapchainImageViews[j], nullptr);
			vkDestroySwapchainKHR(device, swapchain, nullptr);
			vmaDestroyAllocator(allocator);
			vkDestroyDevice(device, nullptr);
			vkDestroySurfaceKHR(instance, surface, nullptr);
			vkDestroyInstance(instance, nullptr);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return SDL_APP_FAILURE;
		}
	}

	// Загрузка шейдеров
	char shaderPath[1024];
	const char *basePath = SDL_GetBasePath();
	if (basePath) {
		SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.vert.spv", basePath);
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));
	} else {
		SDL_strlcpy(shaderPath, "triangle.vert.spv", sizeof(shaderPath));
	}

	std::vector<uint32_t> vertCode = loadSpirv(shaderPath);
	if (vertCode.empty()) {
		SDL_Log("Failed to load triangle.vert.spv. Ensure shaders are next to the executable.");
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	basePath = SDL_GetBasePath();
	if (basePath) {
		SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.frag.spv", basePath);
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));
	} else {
		SDL_strlcpy(shaderPath, "triangle.frag.spv", sizeof(shaderPath));
	}
	std::vector<uint32_t> fragCode = loadSpirv(shaderPath);
	if (fragCode.empty()) {
		SDL_Log("Failed to load triangle.frag.spv");
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkShaderModule vertModule = createShaderModule(device, vertCode);
	VkShaderModule fragModule = createShaderModule(device, fragCode);
	if (!vertModule || !fragModule) {
		SDL_Log("Failed to create shader modules");
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание pipeline layout
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		SDL_Log("vkCreatePipelineLayout failed");
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание pipeline с Dynamic Rendering
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragModule;
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

	// Dynamic Rendering attachment info
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
	pipeInfo.layout = pipelineLayout;
	pipeInfo.pNext = &renderingInfo;	  // Dynamic Rendering вместо renderPass
	pipeInfo.renderPass = VK_NULL_HANDLE; // Явно указываем NULL для Dynamic Rendering

	VkPipeline pipeline = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline) != VK_SUCCESS) {
		SDL_Log("vkCreateGraphicsPipelines failed");
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание командного пула и буфера
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		SDL_Log("Command pool/buffer failed");
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo cmdAllocInfo = {};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
		SDL_Log("Sync objects failed");
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroyFence(device, inFlightFence, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyShaderModule(device, vertModule, nullptr);
		vkDestroyShaderModule(device, fragModule, nullptr);
		for (VkImageView iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Tracy инициализация (опционально)
#ifdef TRACY_ENABLE
	TracyVkCtx tracyContext = TracyVkContext(physicalDevice, device, queue, commandBuffer);
#else
	TracyVkCtx tracyContext = nullptr;
#endif

	AppState *state = new AppState{};
	state->window = window;
	state->instance = instance;
	state->physicalDevice = physicalDevice;
	state->device = device;
	state->surface = surface;
	state->queue = queue;
	state->queueFamilyIndex = queueFamilyIndex;
	state->allocator = allocator;
	state->swapchain = swapchain;
	state->swapchainFormat = surfaceFormat.format;
	state->extent = extent;
	state->swapchainImages = std::move(swapchainImages);
	state->swapchainImageViews = std::move(swapchainImageViews);
	// НЕ ИНИЦИАЛИЗИРУЕМ framebuffers и renderPass
	state->pipelineLayout = pipelineLayout;
	state->pipeline = pipeline;
	state->vertModule = vertModule;
	state->fragModule = fragModule;
	state->commandPool = commandPool;
	state->commandBuffer = commandBuffer;
	state->imageAvailableSemaphore = imageAvailableSemaphore;
	state->renderFinishedSemaphore = renderFinishedSemaphore;
	state->inFlightFence = inFlightFence;
#ifdef TRACY_ENABLE
	state->tracyContext = tracyContext;
#endif

	*appstate = state;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
		return SDL_APP_SUCCESS;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	const AppState *state = static_cast<AppState *>(appstate);
	if (!state)
		return SDL_APP_CONTINUE;

	vkWaitForFences(state->device, 1, &state->inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
												state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR)
		return SDL_APP_CONTINUE;

	vkResetFences(state->device, 1, &state->inFlightFence);
	vkResetCommandBuffer(state->commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(state->commandBuffer, &beginInfo);

	// Tracy zone (опционально)
#ifdef TRACY_ENABLE
	TracyVkZone(state->tracyContext, state->commandBuffer, "Vulkan Triangle Rendering");
#endif

	// Dynamic Rendering вместо RenderPass
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

	// Начало Dynamic Rendering
	vkCmdBeginRendering(state->commandBuffer, &renderingInfo);

	vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);

	VkViewport viewport = {0, 0, (float)state->extent.width, (float)state->extent.height, 0.0f, 1.0f};
	vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {{0, 0}, {state->extent.width, state->extent.height}};
	vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);

	vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);

	// Конец Dynamic Rendering
	vkCmdEndRendering(state->commandBuffer);

#ifdef TRACY_ENABLE
	TracyVkZoneEnd(state->tracyContext, state->commandBuffer);
#endif

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

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	AppState *state = static_cast<AppState *>(appstate);
	if (state) {
		cleanupAppState(state);
		delete state;
	}
	SDL_Quit();
}
