// Пример: Vulkan треугольник с Vulkan 1.4, Dynamic Rendering и Synchronization2 (ProjectV Modern)
// Документация: docs/architecture/modern-vulkan-guide.md
// Шейдеры: docs/examples/shaders/triangle.vert, triangle.frag
// Уровень: 🟡 Средний (требует знания Vulkan basics)

// ============================================================================
// ВАЖНО: Этот пример использует стандарты Vulkan 1.3 / 1.4:
// - Dynamic Rendering (без RenderPass и Framebuffer)
// - Synchronization2 (современные барьеры памяти и vkQueueSubmit2)
// - Включенные Validation Layers для отладки
// - Правильная обработка изменения размера окна (Swapchain Recreation)
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
#include <iostream>
#include <string>
#include <vector>

struct AppState {
	SDL_Window *window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	// Ресурсы Swapchain
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	// Ресурсы пайплайна
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkShaderModule vertModule = VK_NULL_HANDLE;
	VkShaderModule fragModule = VK_NULL_HANDLE;

	// Команды и синхронизация
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

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

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

// Коллбэк для Validation Layers
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
													VkDebugUtilsMessageTypeFlagsEXT messageType,
													const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
													void *pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan Validation: %s", pCallbackData->pMessage);
	}
	return VK_FALSE;
}

// Очистка Swapchain (вынесено отдельно для возможности пересоздания)
static void cleanupSwapchain(AppState *state)
{
	for (VkImageView iv : state->swapchainImageViews) {
		vkDestroyImageView(state->device, iv, nullptr);
	}
	state->swapchainImageViews.clear();
	if (state->swapchain) {
		vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
		state->swapchain = VK_NULL_HANDLE;
	}
}

// Пересоздание Swapchain при изменении размера окна
static bool recreateSwapchain(AppState *state)
{
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(state->window, &w, &h);
	// Если окно свернуто, ждем
	if (w == 0 || h == 0)
		return false;

	// Ждем завершения всех операций на GPU
	vkDeviceWaitIdle(state->device);
	cleanupSwapchain(state);

	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &caps);

	state->extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	state->extent.width = std::clamp(state->extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	state->extent.height = std::clamp(state->extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

	uint32_t imageCount = std::max(2u, caps.minImageCount);
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = state->surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = state->swapchainFormat;
	// В реальном приложении нужно сохранить colorSpace с момента инициализации
	swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainInfo.imageExtent = state->extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = caps.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync on
	swapchainInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(state->device, &swapchainInfo, nullptr, &state->swapchain) != VK_SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain!");
		return false;
	}

	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, nullptr);
	state->swapchainImages.resize(actualImageCount);
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, state->swapchainImages.data());

	state->swapchainImageViews.resize(actualImageCount);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = state->swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = state->swapchainFormat;
		viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCreateImageView(state->device, &viewInfo, nullptr, &state->swapchainImageViews[i]);
	}

	return true;
}

static void cleanupAppState(AppState *state)
{
	if (!state || !state->device)
		return;

	vkDeviceWaitIdle(state->device);

#ifdef TRACY_ENABLE
	if (state->tracyContext) {
		TracyVkDestroy(state->tracyContext);
	}
#endif

	vkDestroySemaphore(state->device, state->imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(state->device, state->renderFinishedSemaphore, nullptr);
	vkDestroyFence(state->device, state->inFlightFence, nullptr);
	vkDestroyCommandPool(state->device, state->commandPool, nullptr);
	vkDestroyPipeline(state->device, state->pipeline, nullptr);
	vkDestroyPipelineLayout(state->device, state->pipelineLayout, nullptr);
	vkDestroyShaderModule(state->device, state->vertModule, nullptr);
	vkDestroyShaderModule(state->device, state->fragModule, nullptr);

	cleanupSwapchain(state);

	vmaDestroyAllocator(state->allocator);
	vkDestroyDevice(state->device, nullptr);
	vkDestroySurfaceKHR(state->instance, state->surface, nullptr);

	if (state->debugMessenger) {
		vkDestroyDebugUtilsMessengerEXT(state->instance, state->debugMessenger, nullptr);
	}

	if (state->window) {
		SDL_DestroyWindow(state->window);
	}
	vkDestroyInstance(state->instance, nullptr);
}

// ============================================================================
// ОСНОВНАЯ ЛОГИКА (SDL_AppInit, SDL_AppIterate)
// ============================================================================

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Window *window =
		SDL_CreateWindow("Vulkan 1.4 Triangle (Modern)", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		return SDL_APP_FAILURE;
	}

	// 1. Создание Instance (Запрашиваем Vulkan 1.3 минимум, лучше 1.4)
	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	std::vector<const char *> instanceExtensions(sdlExtNames, sdlExtNames + extCount);
	instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Для Validation Layers

	const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "ProjectV Modern Triangle";
	appInfo.apiVersion = VK_API_VERSION_1_3; // В 1.3 Dynamic Rendering и Sync2 стали стандартом

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	instanceCreateInfo.enabledLayerCount = 1;
	instanceCreateInfo.ppEnabledLayerNames = validationLayers; // Включаем слои валидации

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
		SDL_Log("vkCreateInstance failed. Check if Validation Layers are installed.");
		return SDL_APP_FAILURE;
	}
	volkLoadInstance(instance);

	// 2. Настройка Debug Messenger
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
							VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugInfo.pfnUserCallback = debugCallback;
	vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debugMessenger);

	// 3. Создание Surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// 4. Выбор физического устройства
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
		if (physicalDevice)
			break;
	}

	if (!physicalDevice) {
		SDL_Log("No suitable physical device found");
		return SDL_APP_FAILURE;
	}

	// 5. Запрос фич Vulkan 1.3 (Dynamic Rendering и Sync2)
	// Эти фичи заменяют устаревшие расширения.
	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features13;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (!features13.dynamicRendering || !features13.synchronization2) {
		SDL_Log("GPU does not support Vulkan 1.3 Dynamic Rendering or Synchronization2.");
		return SDL_APP_FAILURE;
	}

	// 6. Создание логического устройства
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	// Нам нужен ТОЛЬКО swapchain, так как DynamicRendering и Sync2 уже в ядре 1.3
	const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	deviceCreateInfo.pNext = &features13; // Передаем структуру с фичами Vulkan 1.3

	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		return SDL_APP_FAILURE;
	}
	volkLoadDevice(device);

	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	// 7. Инициализация VMA
	VmaAllocatorCreateInfo allocInfo = {};
	allocInfo.physicalDevice = physicalDevice;
	allocInfo.device = device;
	allocInfo.instance = instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;
#ifdef VOLK_HEADER_VERSION
	VmaVulkanFunctions vulkanFunctions = {};
	vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
	allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	VmaAllocator allocator = VK_NULL_HANDLE;
	if (vmaCreateAllocator(&allocInfo, &allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		return SDL_APP_FAILURE;
	}

	// Инициализируем AppState
	AppState *state = new AppState{};
	state->window = window;
	state->instance = instance;
	state->debugMessenger = debugMessenger;
	state->physicalDevice = physicalDevice;
	state->device = device;
	state->surface = surface;
	state->queue = queue;
	state->queueFamilyIndex = queueFamilyIndex;
	state->allocator = allocator;

	// 8. Создание Swapchain (определяем формат)
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

	state->swapchainFormat = formats[0].format;
	for (const auto &f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			state->swapchainFormat = f.format;
			break;
		}
	}

	// Первичное создание Swapchain
	if (!recreateSwapchain(state)) {
		cleanupAppState(state);
		delete state;
		return SDL_APP_FAILURE;
	}

	// 9. Загрузка шейдеров
	char shaderPath[1024];
	const char *basePath = SDL_GetBasePath();

	SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.vert.spv", basePath ? basePath : "");
	std::vector<uint32_t> vertCode = loadSpirv(shaderPath);

	SDL_snprintf(shaderPath, sizeof(shaderPath), "%striangle.frag.spv", basePath ? basePath : "");
	std::vector<uint32_t> fragCode = loadSpirv(shaderPath);

	if (basePath)
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));

	if (vertCode.empty() || fragCode.empty()) {
		SDL_Log("Failed to load shaders! Make sure .spv files are near executable.");
		cleanupAppState(state);
		delete state;
		return SDL_APP_FAILURE;
	}

	state->vertModule = createShaderModule(device, vertCode);
	state->fragModule = createShaderModule(device, fragCode);

	// 10. Настройка Pipeline (Dynamic Rendering подход)
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vkCreatePipelineLayout(device, &layoutInfo, nullptr, &state->pipelineLayout);

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = state->vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = state->fragModule;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterization = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &blendAttachment;

	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	// ПРИМЕЧАНИЕ: Это важнейшая структура для Dynamic Rendering.
	// Она заменяет привязку к VkRenderPass.
	VkPipelineRenderingCreateInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &state->swapchainFormat;

	VkGraphicsPipelineCreateInfo pipeInfo = {};
	pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeInfo.pNext = &renderingInfo;	  // Подключаем Dynamic Rendering
	pipeInfo.renderPass = VK_NULL_HANDLE; // Устанавливаем в NULL!
	pipeInfo.stageCount = 2;
	pipeInfo.pStages = stages;
	pipeInfo.pVertexInputState = &vertexInput;
	pipeInfo.pInputAssemblyState = &inputAssembly;
	pipeInfo.pViewportState = &viewportState;
	pipeInfo.pRasterizationState = &rasterization;
	pipeInfo.pColorBlendState = &colorBlend;
	pipeInfo.pDynamicState = &dynamicState;
	pipeInfo.layout = state->pipelineLayout;

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &state->pipeline);

	// 11. Команды и синхронизация
	VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vkCreateCommandPool(device, &poolInfo, nullptr, &state->commandPool);

	VkCommandBufferAllocateInfo cmdAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cmdAllocInfo.commandPool = state->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;
	vkAllocateCommandBuffers(device, &cmdAllocInfo, &state->commandBuffer);

	VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	vkCreateSemaphore(device, &semaphoreInfo, nullptr, &state->imageAvailableSemaphore);
	vkCreateSemaphore(device, &semaphoreInfo, nullptr, &state->renderFinishedSemaphore);
	vkCreateFence(device, &fenceInfo, nullptr, &state->inFlightFence);

#ifdef TRACY_ENABLE
	state->tracyContext = TracyVkContext(physicalDevice, device, queue, state->commandBuffer);
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
	AppState *state = static_cast<AppState *>(appstate);
	if (!state)
		return SDL_APP_CONTINUE;

	vkWaitForFences(state->device, 1, &state->inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
												state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	// Обработка изменения размера окна
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
		recreateSwapchain(state);
		return SDL_APP_CONTINUE;
	} else if (acquireRes != VK_SUCCESS) {
		SDL_Log("vkAcquireNextImageKHR failed");
		return SDL_APP_FAILURE;
	}

	vkResetFences(state->device, 1, &state->inFlightFence);
	vkResetCommandBuffer(state->commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	vkBeginCommandBuffer(state->commandBuffer, &beginInfo);

#ifdef TRACY_ENABLE
	TracyVkZone(state->tracyContext, state->commandBuffer, "Vulkan Triangle Rendering");
#endif

	// ========================================================================
	// БАРЬЕР 1: Undefined -> ColorAttachmentOptimal
	// Перед тем как рисовать в картинку, мы обязаны перевести ее в правильный Layout.
	// Используется Synchronization2 (Vulkan 1.3).
	// ========================================================================
	VkImageMemoryBarrier2 imgBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	imgBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imgBarrier.srcAccessMask = 0;
	imgBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imgBarrier.image = state->swapchainImages[imageIndex];
	imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imgBarrier;

	vkCmdPipelineBarrier2(state->commandBuffer, &depInfo);

	// ========================================================================
	// DYNAMIC RENDERING
	// Не требует вызовов vkCmdBeginRenderPass / vkCmdEndRenderPass.
	// ========================================================================
	VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	colorAttachment.imageView = state->swapchainImageViews[imageIndex];
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.2f, 1.0f}};

	VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
	renderingInfo.renderArea = {{0, 0}, {state->extent.width, state->extent.height}};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(state->commandBuffer, &renderingInfo);

	vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);

	VkViewport viewport = {0, 0, (float)state->extent.width, (float)state->extent.height, 0.0f, 1.0f};
	vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {{0, 0}, {state->extent.width, state->extent.height}};
	vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);

	vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(state->commandBuffer);

	// ========================================================================
	// БАРЬЕР 2: ColorAttachmentOptimal -> PresentSrcKHR
	// Переводим Layout обратно, чтобы движок отображения окна мог его прочитать.
	// ========================================================================
	imgBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imgBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imgBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	imgBarrier.dstAccessMask = 0;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	vkCmdPipelineBarrier2(state->commandBuffer, &depInfo);

#ifdef TRACY_ENABLE
	TracyVkZoneEnd(state->tracyContext, state->commandBuffer);
	TracyVkCollect(state->tracyContext, state->commandBuffer); // Обязательный вызов для сбора данных
#endif

	vkEndCommandBuffer(state->commandBuffer);

	// ========================================================================
	// MODERN VULKAN: Используем vkQueueSubmit2
	// Намного понятнее и безопаснее, чем старый vkQueueSubmit
	// ========================================================================
	VkSemaphoreSubmitInfo waitSemaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	waitSemaphoreInfo.semaphore = state->imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	signalSemaphoreInfo.semaphore = state->renderFinishedSemaphore;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

	VkCommandBufferSubmitInfo cmdBufferInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
	cmdBufferInfo.commandBuffer = state->commandBuffer;

	VkSubmitInfo2 submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	if (vkQueueSubmit2(state->queue, 1, &submitInfo, state->inFlightFence) != VK_SUCCESS) {
		SDL_Log("vkQueueSubmit2 failed");
		return SDL_APP_FAILURE;
	}

	// Отображение кадра
	VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &state->renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &state->swapchain;
	presentInfo.pImageIndices = &imageIndex;

	VkResult presentRes = vkQueuePresentKHR(state->queue, &presentInfo);
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
		recreateSwapchain(state);
	}

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
