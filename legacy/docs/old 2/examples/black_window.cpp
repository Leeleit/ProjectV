// Минимальный пример чёрного окна: SDL3 + Volk (Vulkan) + VMA
// Документация: docs/README.md, docs/vulkan/integration.md, docs/vulkan/quickstart.md

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
#include <cstring>
#include <vector>

struct AppState {
	SDL_Window *window = nullptr;
	bool windowResized = false;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> framebuffers;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

// --- Функции управления Swapchain ---

static void cleanupSwapchain(AppState *state)
{
	if (!state->device)
		return;

	for (const VkFramebuffer fb : state->framebuffers) {
		vkDestroyFramebuffer(state->device, fb, nullptr);
	}
	state->framebuffers.clear();

	for (const VkImageView iv : state->swapchainImageViews) {
		vkDestroyImageView(state->device, iv, nullptr);
	}
	state->swapchainImageViews.clear();

	if (state->swapchain) {
		vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
		state->swapchain = VK_NULL_HANDLE;
	}
}

static bool createSwapchainAndFramebuffers(AppState *state)
{
	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &caps);

	// Строго по спецификации: обработка размеров, навязанных оконным менеджером (Wayland/X11)
	if (caps.currentExtent.width != 0xFFFFFFFF) {
		state->extent = caps.currentExtent;
	} else {
		int w = 1280, h = 720;
		SDL_GetWindowSizeInPixels(state->window, &w, &h);
		state->extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
		state->extent.width = std::clamp(state->extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
		state->extent.height = std::clamp(state->extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}

	// Если окно свёрнуто, ширина или высота будут 0. Мы пропускаем создание, ничего не ломая.
	if (state->extent.width == 0 || state->extent.height == 0) {
		return true;
	}

	uint32_t presentModeCount = 0;
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

	uint32_t imageCount = std::max(2u, caps.minImageCount);
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = state->surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = state->swapchainFormat;
	swapchainInfo.imageColorSpace = state->swapchainColorSpace;
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
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(state->device, &viewInfo, nullptr, &state->swapchainImageViews[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateImageView failed");
			return false;
		}
	}

	state->framebuffers.resize(actualImageCount);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = state->renderPass;
		fbInfo.attachmentCount = 1;
		fbInfo.pAttachments = &state->swapchainImageViews[i];
		fbInfo.width = state->extent.width;
		fbInfo.height = state->extent.height;
		fbInfo.layers = 1;

		if (vkCreateFramebuffer(state->device, &fbInfo, nullptr, &state->framebuffers[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateFramebuffer failed");
			return false;
		}
	}

	return true;
}

static bool recreateSwapchain(AppState *state)
{
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(state->window, &w, &h);
	if (w == 0 || h == 0) {
		return true; // Окно свёрнуто, просто ставим на паузу
	}

	vkDeviceWaitIdle(state->device);
	cleanupSwapchain(state);
	return createSwapchainAndFramebuffers(state);
}

// --- Инициализация и очистка основного стейта ---

static void cleanupAppState(AppState *state)
{
	// This block unnecessary if you absolutely sure that there will not be nullptr
	// if (!state)
	// 	return;

	if (state->device) {
		vkDeviceWaitIdle(state->device);
	}

	cleanupSwapchain(state);

	if (state->device) {
		vkDestroySemaphore(state->device, state->imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(state->device, state->renderFinishedSemaphore, nullptr);
		vkDestroyFence(state->device, state->inFlightFence, nullptr);
		vkDestroyCommandPool(state->device, state->commandPool, nullptr);
		vkDestroyRenderPass(state->device, state->renderPass, nullptr);
	}

	if (state->allocator) {
		vmaDestroyAllocator(state->allocator);
	}

	if (state->device) {
		vkDestroyDevice(state->device, nullptr);
	}

	if (state->instance && state->surface) {
		vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
	}

	if (state->window) {
		SDL_DestroyWindow(state->window);
	}

	if (state->instance) {
		vkDestroyInstance(state->instance, nullptr);
	}
}

static bool InitVulkan(AppState *state)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	// Добавлен флаг RESIZABLE для проверки пересоздания swapchain
	state->window = SDL_CreateWindow("Black Window", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!state->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return false;
	}

	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		return false;
	}

	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!sdlExtNames) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return false;
	}

	std::vector<const char *> instanceExtensions(sdlExtNames, sdlExtNames + extCount);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &state->instance) != VK_SUCCESS) {
		SDL_Log("vkCreateInstance failed");
		return false;
	}
	volkLoadInstance(state->instance);

	if (!SDL_Vulkan_CreateSurface(state->window, state->instance, nullptr, &state->surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return false;
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		SDL_Log("No physical devices found");
		return false;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, devices.data());

	state->queueFamilyIndex = UINT32_MAX;
	for (VkPhysicalDevice pd : devices) {
		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, families.data());

		for (uint32_t i = 0; i < familyCount; ++i) {
			// Современный способ проверки презентации
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, state->surface, &presentSupport);
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
				state->physicalDevice = pd;
				state->queueFamilyIndex = i;
				break;
			}
		}
		if (state->queueFamilyIndex != UINT32_MAX)
			break;
	}

	if (state->queueFamilyIndex == UINT32_MAX) {
		SDL_Log("No suitable physical device found");
		return false;
	}

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, nullptr);
	if (formatCount == 0)
		return false;

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats.data());

	state->swapchainFormat = formats[0].format;
	state->swapchainColorSpace = formats[0].colorSpace;
	for (const auto &f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			state->swapchainFormat = f.format;
			state->swapchainColorSpace = f.colorSpace;
			break;
		}
	}

	// Проверка расширения VK_KHR_portability_subset (необходимо для работы на macOS/MoltenVK)
	uint32_t devExtCount = 0;
	vkEnumerateDeviceExtensionProperties(state->physicalDevice, nullptr, &devExtCount, nullptr);
	std::vector<VkExtensionProperties> availableDevExts(devExtCount);
	vkEnumerateDeviceExtensionProperties(state->physicalDevice, nullptr, &devExtCount, availableDevExts.data());

	bool hasPortabilitySubset = false;
	for (const auto &ext : availableDevExts) {
		if (std::strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
			hasPortabilitySubset = true;
			break;
		}
	}

	std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	if (hasPortabilitySubset) {
		deviceExtensions.push_back("VK_KHR_portability_subset");
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = state->queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(state->physicalDevice, &deviceCreateInfo, nullptr, &state->device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		return false;
	}
	volkLoadDevice(state->device);

	vkGetDeviceQueue(state->device, state->queueFamilyIndex, 0, &state->queue);

	VmaAllocatorCreateInfo allocInfo = {};
	allocInfo.physicalDevice = state->physicalDevice;
	allocInfo.device = state->device;
	allocInfo.instance = state->instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;

#ifdef VOLK_HEADER_VERSION
	VmaVulkanFunctions vulkanFunctions = {};
	if (vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions) != VK_SUCCESS) {
		SDL_Log("vmaImportVulkanFunctionsFromVolk failed");
		return false;
	}
	allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	if (vmaCreateAllocator(&allocInfo, &state->allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		return false;
	}

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = state->swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorRef = {};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments = &colorAttachment;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;
	rpInfo.dependencyCount = 1;
	rpInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(state->device, &rpInfo, nullptr, &state->renderPass) != VK_SUCCESS) {
		SDL_Log("vkCreateRenderPass failed");
		return false;
	}

	if (!createSwapchainAndFramebuffers(state))
		return false;

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = state->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(state->device, &poolInfo, nullptr, &state->commandPool) != VK_SUCCESS) {
		SDL_Log("vkCreateCommandPool failed");
		return false;
	}

	VkCommandBufferAllocateInfo cmdAllocInfo = {};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = state->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(state->device, &cmdAllocInfo, &state->commandBuffer) != VK_SUCCESS) {
		SDL_Log("vkAllocateCommandBuffers failed");
		return false;
	}

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(state->device, &fenceInfo, nullptr, &state->inFlightFence) != VK_SUCCESS) {
		SDL_Log("Failed to create sync objects");
		return false;
	}

	return true;
}

// --- Коллбэки SDL3 ---

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	AppState *state = new AppState{};
	*appstate = state;

	if (!InitVulkan(state)) {
		cleanupAppState(state);
		delete state;
		*appstate = nullptr;
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	AppState *state = static_cast<AppState *>(appstate);

	if (event->type == SDL_EVENT_QUIT)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
		return SDL_APP_SUCCESS;

	// Перехватываем изменение размеров
	if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		if (state)
			state->windowResized = true;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	AppState *state = static_cast<AppState *>(appstate);
	if (!state)
		return SDL_APP_CONTINUE;

	// Ожидание разворачивания (если свернуто)
	if (state->extent.width == 0 || state->extent.height == 0) {
		recreateSwapchain(state);
		return SDL_APP_CONTINUE;
	}

	vkWaitForFences(state->device, 1, &state->inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	const VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
													  state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	// Обработка устаревания Swapchain при получении изображения
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain(state);
		return SDL_APP_CONTINUE;
	} else if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		return SDL_APP_CONTINUE;
	}

	vkResetFences(state->device, 1, &state->inFlightFence);
	vkResetCommandBuffer(state->commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(state->commandBuffer, &beginInfo);

	VkRenderPassBeginInfo rpBegin = {};
	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.renderPass = state->renderPass;
	rpBegin.framebuffer = state->framebuffers[imageIndex];
	rpBegin.renderArea.offset = {0, 0};
	rpBegin.renderArea.extent = state->extent;

	// Безопасная кроссплатформенная инициализация union'а
	VkClearValue clearColor = {};
	clearColor.color.float32[0] = 0.0f;
	clearColor.color.float32[1] = 0.0f;
	clearColor.color.float32[2] = 0.0f;
	clearColor.color.float32[3] = 1.0f;

	rpBegin.clearValueCount = 1;
	rpBegin.pClearValues = &clearColor;

	vkCmdBeginRenderPass(state->commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(state->commandBuffer);
	vkEndCommandBuffer(state->commandBuffer);

	constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &state->imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = &waitStage;
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

	const VkResult presentRes = vkQueuePresentKHR(state->queue, &presentInfo);

	// Обработка устаревания Swapchain после презентации (или если ОС послала эвент изменения окна)
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || state->windowResized) {
		state->windowResized = false;
		recreateSwapchain(state);
	} else if (presentRes != VK_SUCCESS) {
		SDL_Log("vkQueuePresentKHR failed");
		return SDL_APP_FAILURE;
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
}
