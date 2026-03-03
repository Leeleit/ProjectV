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
	std::vector<VkFramebuffer> framebuffers;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

static void cleanupAppState(AppState *state)
{
	vkDeviceWaitIdle(state->device);

	vkDestroySemaphore(state->device, state->imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(state->device, state->renderFinishedSemaphore, nullptr);
	vkDestroyFence(state->device, state->inFlightFence, nullptr);
	vkDestroyCommandPool(state->device, state->commandPool, nullptr);
	for (const VkFramebuffer fb : state->framebuffers)
		vkDestroyFramebuffer(state->device, fb, nullptr);
	vkDestroyRenderPass(state->device, state->renderPass, nullptr);
	for (const VkImageView iv : state->swapchainImageViews)
		vkDestroyImageView(state->device, iv, nullptr);
	vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
	vmaDestroyAllocator(state->allocator);
	vkDestroyDevice(state->device, nullptr);
	vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
	if (state->window)
		SDL_DestroyWindow(state->window);
	vkDestroyInstance(state->instance, nullptr);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Window *window = SDL_CreateWindow("Black Window", 1280, 720, SDL_WINDOW_VULKAN);
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

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = extCount;
	instanceCreateInfo.ppEnabledExtensionNames = extNames;

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

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

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
	if (vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions) != VK_SUCCESS) {
		SDL_Log("vmaImportVulkanFunctionsFromVolk failed");
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}
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
	if (!SDL_GetWindowSizeInPixels(window, &w, &h)) {
		w = 1280;
		h = 720;
	}

	VkExtent2D extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCount == 0) {
		SDL_Log("No surface formats available");
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}
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
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
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

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = surfaceFormat.format;
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

	VkRenderPass renderPass = VK_NULL_HANDLE;
	if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS) {
		SDL_Log("vkCreateRenderPass failed");
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

	std::vector<VkFramebuffer> framebuffers(actualImageCount);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = renderPass;
		fbInfo.attachmentCount = 1;
		fbInfo.pAttachments = &swapchainImageViews[i];
		fbInfo.width = extent.width;
		fbInfo.height = extent.height;
		fbInfo.layers = 1;
		if (vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateFramebuffer failed");
			for (uint32_t j = 0; j < i; ++j)
				vkDestroyFramebuffer(device, framebuffers[j], nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);
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
	}

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		SDL_Log("vkCreateCommandPool failed");
		for (VkFramebuffer fb : framebuffers)
			vkDestroyFramebuffer(device, fb, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
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

	VkCommandBufferAllocateInfo cmdAllocInfo = {};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
		SDL_Log("vkAllocateCommandBuffers failed");
		vkDestroyCommandPool(device, commandPool, nullptr);
		for (VkFramebuffer fb : framebuffers)
			vkDestroyFramebuffer(device, fb, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
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

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
		SDL_Log("Failed to create sync objects");
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroyFence(device, inFlightFence, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		for (VkFramebuffer fb : framebuffers)
			vkDestroyFramebuffer(device, fb, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
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
	state->framebuffers = std::move(framebuffers);
	state->renderPass = renderPass;
	state->commandPool = commandPool;
	state->commandBuffer = commandBuffer;
	state->imageAvailableSemaphore = imageAvailableSemaphore;
	state->renderFinishedSemaphore = renderFinishedSemaphore;
	state->inFlightFence = inFlightFence;

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
	const VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
													  state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
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
	rpBegin.renderArea = {{0, 0}, {state->extent.width, state->extent.height}};
	rpBegin.clearValueCount = 1;
	VkClearValue clearColor = {};
	clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	rpBegin.pClearValues = &clearColor;

	vkCmdBeginRenderPass(state->commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(state->commandBuffer);
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
