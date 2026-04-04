#include "VulkanBootstrap.hpp"
#include "VulkanDebug.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {
// Имя приложения видно и в заголовке окна, и в логах Vulkan.
inline constexpr char PROJECT_NAME[] = "ProjectV v0.0.1";

#ifndef NDEBUG
// В debug-сборке просим Vulkan включить валидацию.
constexpr bool kEnableValidation = true;
#else
// В release-сборке лишние слои не нужны.
constexpr bool kEnableValidation = false;
#endif

// Базовый набор слоёв для отладки.
constexpr std::array<const char *, 1> kValidationLayers{"VK_LAYER_KHRONOS_validation"};
// Для нашего рендера достаточно swapchain-расширения.
constexpr std::array<const char *, 1> kRequiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Callback, через который Vulkan присылает предупреждения и ошибки прямо в SDL-лог.
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	const VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *)
{
	SDL_Log("Vulkan validation: [%u][%u] %s",
			static_cast<unsigned>(messageSeverity),
			static_cast<unsigned>(messageTypes),
			pCallbackData && pCallbackData->pMessage ? pCallbackData->pMessage : "no message");
	return VK_FALSE;
}

// Проверяем, доступны ли те слои, которые мы попросим у instance.
bool CheckValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());

	for (const char *requiredLayer : kValidationLayers) {
		bool found = false;
		for (const auto &layer : available) {
			if (std::strcmp(requiredLayer, layer.layerName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			SDL_Log("Missing validation layer: %s", requiredLayer);
			return false;
		}
	}

	return true;
}

// Конфигурируем messenger заранее, чтобы Vulkan смог отправлять сообщения уже на этапе vkCreateInstance.
VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

// Создаём сам messenger после instance.
bool CreateDebugMessenger(VulkanContextState *context)
{
#ifdef NDEBUG
	(void)context;
	return true;
#else
	const VkDebugUtilsMessengerCreateInfoEXT info = MakeDebugMessengerCreateInfo();
	if (vkCreateDebugUtilsMessengerEXT(context->instance, &info, nullptr, &context->debugMessenger) != VK_SUCCESS) {
		SDL_Log("vkCreateDebugUtilsMessengerEXT failed");
		return false;
	}
	return true;
#endif
}

// Проверяем, что у физического устройства есть нужные расширения.
bool CheckDeviceExtensionSupport(const VkPhysicalDevice physicalDevice)
{
	uint32_t extensionCount = 0;
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	if (vkEnumerateDeviceExtensionProperties(
			physicalDevice,
			nullptr,
			&extensionCount,
			availableExtensions.data()) != VK_SUCCESS) {
		return false;
	}

	for (const char *required : kRequiredDeviceExtensions) {
		bool found = false;
		for (const auto &[extensionName, specVersion] : availableExtensions) {
			if (std::strcmp(required, extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			SDL_Log("Missing device extension: %s", required);
			return false;
		}
	}

	return true;
}

// Ищем очередь, которая умеет и рисовать, и презентовать изображение в окно.
bool FindGraphicsPresentQueueFamily(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	uint32_t *outQueueFamilyIndex)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	if (familyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		VkBool32 presentSupport = VK_FALSE;
		if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport) != VK_SUCCESS) {
			continue;
		}

		if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && presentSupport) {
			*outQueueFamilyIndex = i;
			return true;
		}
	}

	return false;
}

// Без форматов и present mode swapchain мы просто не соберём.
bool CheckSwapchainSurfaceSupport(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
{
	uint32_t formatCount = 0;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr) != VK_SUCCESS) {
		return false;
	}
	if (formatCount == 0) {
		SDL_Log("No surface formats found");
		return false;
	}

	uint32_t presentModeCount = 0;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS) {
		return false;
	}
	if (presentModeCount == 0) {
		SDL_Log("No present modes found");
		return false;
	}

	return true;
}

// Проверяем фичи Vulkan 1.3, которые использует рендерер.
bool CheckRequiredFeatures(
	const VkPhysicalDevice physicalDevice,
	VkPhysicalDeviceVulkan13Features *outFeatures13)
{
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features13;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (!features13.dynamicRendering) {
		SDL_Log("Device does not support dynamicRendering");
		return false;
	}

	if (!features13.synchronization2) {
		SDL_Log("Device does not support synchronization2");
		return false;
	}

	*outFeatures13 = features13;
	return true;
}

// Кандидат на выбор физического устройства: сам устройство, его очередь и поддерживаемые фичи.
struct PhysicalDeviceCandidate {
	VkPhysicalDevice device = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = UINT32_MAX;
	VkPhysicalDeviceVulkan13Features features13{};
};

// Переносим в enabled только те фичи, которые реально поддержаны устройством.
VkPhysicalDeviceVulkan13Features BuildEnabledFeatures13(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan13Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled.dynamicRendering = selected.features13.dynamicRendering ? VK_TRUE : VK_FALSE;
	enabled.synchronization2 = selected.features13.synchronization2 ? VK_TRUE : VK_FALSE;
	return enabled;
}

// Полная проверка кандидата на роль GPU для нашего приложения.
bool TryPickPhysicalDevice(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	PhysicalDeviceCandidate *outCandidate)
{
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion < VK_API_VERSION_1_4) {
		return false;
	}

	uint32_t queueFamilyIndex = UINT32_MAX;
	if (!FindGraphicsPresentQueueFamily(physicalDevice, surface, &queueFamilyIndex)) {
		return false;
	}

	if (!CheckDeviceExtensionSupport(physicalDevice)) {
		return false;
	}

	if (!CheckSwapchainSurfaceSupport(physicalDevice, surface)) {
		return false;
	}

	VkPhysicalDeviceVulkan13Features supportedFeatures13{};
	if (!CheckRequiredFeatures(physicalDevice, &supportedFeatures13)) {
		return false;
	}

	outCandidate->device = physicalDevice;
	outCandidate->queueFamilyIndex = queueFamilyIndex;
	outCandidate->features13 = supportedFeatures13;
	return true;
}
} // namespace

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame)
{
	// SDL нужен нам до Vulkan, потому что именно он создаёт окно и surface-совместимость.
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	platform->window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!platform->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return false;
	}

	// Volk поднимает таблицу функций Vulkan, чтобы дальше не делать ручной загрузки указателей.
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		return false;
	}

	// SDL сообщает, какие instance extensions нужны именно для этого окна и платформы.
	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!sdlExtNames) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return false;
	}

	std::vector instanceExtensions(sdlExtNames, sdlExtNames + extCount);
	if (kEnableValidation) {
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		if (!CheckValidationLayerSupport()) {
			SDL_Log("Validation layers requested, but not available");
			return false;
		}
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_4;

	// Базовая "анкета" для vkCreateInstance.
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (kEnableValidation) {
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();
		debugCreateInfo = MakeDebugMessengerCreateInfo();
		instanceCreateInfo.pNext = &debugCreateInfo;
	}

	// Instance — это верхний объект Vulkan, от которого стартует вся остальная графическая жизнь.
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &context->instance) != VK_SUCCESS) {
		SDL_Log("vkCreateInstance failed");
		return false;
	}

	volkLoadInstance(context->instance);

	if (!CreateDebugMessenger(context)) {
		return false;
	}

	// Surface связывает окно SDL и Vulkan-instance в один канал вывода.
	if (!SDL_Vulkan_CreateSurface(platform->window, context->instance, nullptr, &context->surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return false;
	}

	// Ищем физическое устройство, которое вообще умеет работать с нашим surface.
	uint32_t deviceCount = 0;
	if (vkEnumeratePhysicalDevices(context->instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
		SDL_Log("No physical devices found");
		return false;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	if (vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices.data()) != VK_SUCCESS) {
		SDL_Log("vkEnumeratePhysicalDevices failed");
		return false;
	}

	PhysicalDeviceCandidate selected{};
	for (VkPhysicalDevice physicalDevice : devices) {
		PhysicalDeviceCandidate candidate{};
		if (TryPickPhysicalDevice(physicalDevice, context->surface, &candidate)) {
			selected = candidate;
			break;
		}
	}

	if (selected.device == VK_NULL_HANDLE) {
		SDL_Log("No suitable physical device found");
		return false;
	}

	// Фиксируем выбранную видеокарту и семейство очереди в общем состоянии.
	context->physicalDevice = selected.device;
	context->queueFamilyIndex = selected.queueFamilyIndex;

	// Логическое устройство создаёт то API, которым мы будем реально пользоваться.
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = context->queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	std::vector deviceExtensions(
		kRequiredDeviceExtensions.begin(),
		kRequiredDeviceExtensions.end());

	VkPhysicalDeviceVulkan13Features enabledFeatures13 = BuildEnabledFeatures13(selected);
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enabledFeatures13;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(context->physicalDevice, &deviceCreateInfo, nullptr, &context->device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		return false;
	}

	// После создания device Vulkan-вызывам нужен device-level loader.
	volkLoadDevice(context->device);
	vkGetDeviceQueue(context->device, context->queueFamilyIndex, 0, &context->queue);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->queue),
		VK_OBJECT_TYPE_QUEUE,
		"GraphicsPresentQueue");

	// VMA берет на себя буферы и аллокации памяти, чтобы не писать это руками в каждом месте.
	VmaAllocatorCreateInfo allocInfo{};
	allocInfo.physicalDevice = context->physicalDevice;
	allocInfo.device = context->device;
	allocInfo.instance = context->instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;

	VmaVulkanFunctions vulkanFunctions{};
	if (vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions) != VK_SUCCESS) {
		SDL_Log("vmaImportVulkanFunctionsFromVolk failed");
		return false;
	}
	allocInfo.pVulkanFunctions = &vulkanFunctions;

	if (vmaCreateAllocator(&allocInfo, &context->allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		return false;
	}

	// Command pool хранит временные command buffer'ы.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = context->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->commandPool) != VK_SUCCESS) {
		SDL_Log("vkCreateCommandPool failed");
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->commandPool),
		VK_OBJECT_TYPE_COMMAND_POOL,
		"MainCommandPool");

	// На каждый кадр держим отдельный primary command buffer.
	frame->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo cmdAllocInfo{};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = context->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	if (vkAllocateCommandBuffers(context->device, &cmdAllocInfo, frame->commandBuffers.data()) != VK_SUCCESS) {
		return false;
	}
	for (size_t i = 0; i < frame->commandBuffers.size(); ++i) {
		char name[64]{};
		std::snprintf(name, sizeof(name), "FrameCommandBuffer[%zu]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->commandBuffers[i]),
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			name);
	}

	// Семафоры и fence'ы синхронизируют CPU и GPU между кадрами.
	frame->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(context->device, &fenceInfo, nullptr, &frame->inFlightFences[i]) != VK_SUCCESS) {
			return false;
		}

		char imageAvailableName[64]{};
		std::snprintf(imageAvailableName, sizeof(imageAvailableName), "ImageAvailableSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->imageAvailableSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			imageAvailableName);

		char renderFinishedName[64]{};
		std::snprintf(renderFinishedName, sizeof(renderFinishedName), "RenderFinishedSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->renderFinishedSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			renderFinishedName);

		char fenceName[64]{};
		std::snprintf(fenceName, sizeof(fenceName), "InFlightFence[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->inFlightFences[i]),
			VK_OBJECT_TYPE_FENCE,
			fenceName);
	}

	return true;
}
