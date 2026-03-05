// Пример: Vulkan compute shader и uniform buffers для воксельного рендеринга
// Документация: docs/vulkan/integration.md, docs/vulkan/performance.md
// Демонстрирует: compute pipeline, uniform buffers, descriptor sets, синхронизация compute/graphics

#define NOMINMAX
#define VK_NO_PROTOTYPES
#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <fstream>
#include <random>
#include <vector>

struct AppState {
	SDL_Window *window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;
	uint32_t computeQueueFamily = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	// Compute pipeline
	VkPipeline computePipeline = VK_NULL_HANDLE;
	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	VkShaderModule computeShader = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

	// Buffers
	VkBuffer voxelDataBuffer = VK_NULL_HANDLE;
	VmaAllocation voxelDataAllocation = VK_NULL_HANDLE;
	VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VmaAllocation uniformAllocation = VK_NULL_HANDLE;
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	// Sync objects
	VkSemaphore computeFinishedSemaphore = VK_NULL_HANDLE;
	VkSemaphore graphicsReadySemaphore = VK_NULL_HANDLE;
	VkFence computeFence = VK_NULL_HANDLE;
	VkFence graphicsFence = VK_NULL_HANDLE;

	// Command buffers
	VkCommandPool computeCommandPool = VK_NULL_HANDLE;
	VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer computeCommandBuffer = VK_NULL_HANDLE;
	VkCommandBuffer graphicsCommandBuffer = VK_NULL_HANDLE;
};

// Uniform buffer структура (совместима с std140)
struct UniformData {
	glm::mat4 viewProjection;
	glm::vec4 cameraPosition;
	glm::vec4 voxelGridSize; // x,y,z = размер, w = cell size
	float time;
	float deltaTime;
	float padding[2]; // Для выравнивания до 16 байт
};

// Данные вокселя (16 байт для выравнивания)
struct VoxelData {
	glm::vec3 position;
	float density;
	glm::vec3 color;
	float emission;
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

	// Sync objects
	vkDestroySemaphore(state->device, state->computeFinishedSemaphore, nullptr);
	vkDestroySemaphore(state->device, state->graphicsReadySemaphore, nullptr);
	vkDestroyFence(state->device, state->computeFence, nullptr);
	vkDestroyFence(state->device, state->graphicsFence, nullptr);

	// Command pools
	vkDestroyCommandPool(state->device, state->computeCommandPool, nullptr);
	vkDestroyCommandPool(state->device, state->graphicsCommandPool, nullptr);

	// Buffers
	vmaDestroyBuffer(state->allocator, state->voxelDataBuffer, state->voxelDataAllocation);
	vmaDestroyBuffer(state->allocator, state->uniformBuffer, state->uniformAllocation);
	vmaDestroyBuffer(state->allocator, state->stagingBuffer, state->stagingAllocation);

	// Descriptor sets
	vkDestroyDescriptorPool(state->device, state->descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(state->device, state->descriptorSetLayout, nullptr);

	// Pipeline
	vkDestroyPipeline(state->device, state->computePipeline, nullptr);
	vkDestroyPipelineLayout(state->device, state->computePipelineLayout, nullptr);
	vkDestroyShaderModule(state->device, state->computeShader, nullptr);

	// Core Vulkan
	vmaDestroyAllocator(state->allocator);
	vkDestroyDevice(state->device, nullptr);
	vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
	if (state->window)
		SDL_DestroyWindow(state->window);
	vkDestroyInstance(state->instance, nullptr);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	// Инициализация SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Window *window = SDL_CreateWindow("Vulkan Compute + Uniform", 1280, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Инициализация volk
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание instance
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

	// Surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Выбор физического устройства с поддержкой compute
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = UINT32_MAX;
	uint32_t computeQueueFamily = UINT32_MAX;

	for (VkPhysicalDevice pd : devices) {
		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, families.data());

		for (uint32_t i = 0; i < familyCount; ++i) {
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
				SDL_Vulkan_GetPresentationSupport(instance, pd, i)) {
				graphicsQueueFamily = i;
			}
			if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				computeQueueFamily = i;
			}
		}

		if (graphicsQueueFamily != UINT32_MAX && computeQueueFamily != UINT32_MAX) {
			physicalDevice = pd;
			break;
		}
	}

	if (!physicalDevice) {
		SDL_Log("No suitable physical device with compute support");
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание device с поддержкой compute
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::vector<float> queuePriorities = {1.0f};

	VkDeviceQueueCreateInfo graphicsQueueInfo = {};
	graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueInfo.queueFamilyIndex = graphicsQueueFamily;
	graphicsQueueInfo.queueCount = 1;
	graphicsQueueInfo.pQueuePriorities = queuePriorities.data();
	queueInfos.push_back(graphicsQueueInfo);

	VkDeviceQueueCreateInfo computeQueueInfo = {};
	computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	computeQueueInfo.queueFamilyIndex = computeQueueFamily;
	computeQueueInfo.queueCount = 1;
	computeQueueInfo.pQueuePriorities = queuePriorities.data();
	queueInfos.push_back(computeQueueInfo);

	const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueInfos.data();
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

	// Получение очередей
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, computeQueueFamily, 0, &computeQueue);

	// Создание VMA
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

	// Создание буферов
	const uint32_t voxelCount = 1024; // 1024 вокселя для примера
	const size_t voxelDataSize = voxelCount * sizeof(VoxelData);
	const size_t uniformDataSize = sizeof(UniformData);

	// Voxel data buffer (GPU only, для compute shader)
	VkBufferCreateInfo voxelBufferInfo = {};
	voxelBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	voxelBufferInfo.size = voxelDataSize;
	voxelBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	voxelBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo voxelAllocInfo = {};
	voxelAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBuffer voxelDataBuffer = VK_NULL_HANDLE;
	VmaAllocation voxelDataAllocation = VK_NULL_HANDLE;
	if (vmaCreateBuffer(allocator, &voxelBufferInfo, &voxelAllocInfo, &voxelDataBuffer, &voxelDataAllocation,
						nullptr) != VK_SUCCESS) {
		SDL_Log("Failed to create voxel data buffer");
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Uniform buffer (CPU to GPU, часто обновляется)
	VkBufferCreateInfo uniformBufferInfo = {};
	uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferInfo.size = uniformDataSize;
	uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	uniformBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo uniformAllocInfo = {};
	uniformAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	uniformAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VmaAllocation uniformAllocation = VK_NULL_HANDLE;
	if (vmaCreateBuffer(allocator, &uniformBufferInfo, &uniformAllocInfo, &uniformBuffer, &uniformAllocation,
						nullptr) != VK_SUCCESS) {
		SDL_Log("Failed to create uniform buffer");
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Staging buffer для загрузки данных вокселей
	VkBufferCreateInfo stagingBufferInfo = {};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.size = voxelDataSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo stagingAllocInfo = {};
	stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;
	if (vmaCreateBuffer(allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation,
						nullptr) != VK_SUCCESS) {
		SDL_Log("Failed to create staging buffer");
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Заполнение staging buffer тестовыми данными вокселей
	void *mappedData = nullptr;
	vmaMapMemory(allocator, stagingAllocation, &mappedData);
	VoxelData *voxels = static_cast<VoxelData *>(mappedData);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> posDist(-10.0, 10.0);
	std::uniform_real_distribution<> colorDist(0.0, 1.0);
	std::uniform_real_distribution<> densityDist(0.1, 1.0);

	for (uint32_t i = 0; i < voxelCount; ++i) {
		voxels[i].position = glm::vec3(posDist(gen), posDist(gen), posDist(gen));
		voxels[i].density = static_cast<float>(densityDist(gen));
		voxels[i].color = glm::vec3(colorDist(gen), colorDist(gen), colorDist(gen));
		voxels[i].emission = static_cast<float>(colorDist(gen));
	}

	vmaUnmapMemory(allocator, stagingAllocation);

	// Создание descriptor set layout
	VkDescriptorSetLayoutBinding bindings[2] = {};

	// Binding 0: Uniform buffer
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 1: Storage buffer (voxel data)
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHader_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		SDL_Log("Failed to create descriptor set layout");
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание descriptor pool
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		SDL_Log("Failed to create descriptor pool");
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Выделение descriptor set
	VkDescriptorSetAllocateInfo allocSetInfo = {};
	allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocSetInfo.descriptorPool = descriptorPool;
	allocSetInfo.descriptorSetCount = 1;
	allocSetInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(device, &allocSetInfo, &descriptorSet) != VK_SUCCESS) {
		SDL_Log("Failed to allocate descriptor set");
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Обновление descriptor set
	VkDescriptorBufferInfo uniformBufferInfoDesc = {};
	uniformBufferInfoDesc.buffer = uniformBuffer;
	uniformBufferInfoDesc.range = uniformDataSize;

	VkDescriptorBufferInfo storageBufferInfo = {};
	storageBufferInfo.buffer = voxelDataBuffer;
	storageBufferInfo.range = voxelDataSize;

	VkWriteDescriptorSet descriptorWrites[2] = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].pBufferInfo = &uniformBufferInfoDesc;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = descriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrites[1].pBufferInfo = &storageBufferInfo;

	vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);

	// Загрузка compute shader
	char shaderPath[1024];
	const char *basePath = SDL_GetBasePath();
	if (basePath) {
		SDL_snprintf(shaderPath, sizeof(shaderPath), "%svoxel_compute.comp.spv", basePath);
		SDL_free(const_cast<void *>(static_cast<const void *>(basePath)));
	} else {
		SDL_strlcpy(shaderPath, "voxel_compute.comp.spv", sizeof(shaderPath));
	}

	std::vector<uint32_t> computeCode = loadSpirv(shaderPath);
	if (computeCode.empty()) {
		SDL_Log("Failed to load voxel_compute.comp.spv. Ensure shaders are next to the executable.");
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkShaderModule computeShader = createShaderModule(device, computeCode);
	if (!computeShader) {
		SDL_Log("Failed to create compute shader module");
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание compute pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
		SDL_Log("Failed to create compute pipeline layout");
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание compute pipeline
	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module = computeShader;
	pipelineInfo.stage.pName = "main";
	pipelineInfo.layout = computePipelineLayout;

	VkPipeline computePipeline = VK_NULL_HANDLE;
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
		SDL_Log("Failed to create compute pipeline");
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание command pools
	VkCommandPoolCreateInfo computePoolInfo = {};
	computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	computePoolInfo.queueFamilyIndex = computeQueueFamily;
	computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool computeCommandPool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(device, &computePoolInfo, nullptr, &computeCommandPool) != VK_SUCCESS) {
		SDL_Log("Failed to create compute command pool");
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkCommandPoolCreateInfo graphicsPoolInfo = {};
	graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsPoolInfo.queueFamilyIndex = graphicsQueueFamily;
	graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
		SDL_Log("Failed to create graphics command pool");
		vkDestroyCommandPool(device, computeCommandPool, nullptr);
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание command buffers
	VkCommandBufferAllocateInfo computeCmdAllocInfo = {};
	computeCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	computeCmdAllocInfo.commandPool = computeCommandPool;
	computeCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	computeCmdAllocInfo.commandBufferCount = 1;

	VkCommandBuffer computeCommandBuffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(device, &computeCmdAllocInfo, &computeCommandBuffer) != VK_SUCCESS) {
		SDL_Log("Failed to allocate compute command buffer");
		vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
		vkDestroyCommandPool(device, computeCommandPool, nullptr);
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkCommandBufferAllocateInfo graphicsCmdAllocInfo = {};
	graphicsCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	graphicsCmdAllocInfo.commandPool = graphicsCommandPool;
	graphicsCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	graphicsCmdAllocInfo.commandBufferCount = 1;

	VkCommandBuffer graphicsCommandBuffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(device, &graphicsCmdAllocInfo, &graphicsCommandBuffer) != VK_SUCCESS) {
		SDL_Log("Failed to allocate graphics command buffer");
		vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
		vkDestroyCommandPool(device, computeCommandPool, nullptr);
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Создание synchronization объектов
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore computeFinishedSemaphore = VK_NULL_HANDLE;
	VkSemaphore graphicsReadySemaphore = VK_NULL_HANDLE;
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsReadySemaphore) != VK_SUCCESS) {
		SDL_Log("Failed to create semaphores");
		vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
		vkDestroyCommandPool(device, computeCommandPool, nullptr);
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence computeFence = VK_NULL_HANDLE;
	VkFence graphicsFence = VK_NULL_HANDLE;
	if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS ||
		vkCreateFence(device, &fenceInfo, nullptr, &graphicsFence) != VK_SUCCESS) {
		SDL_Log("Failed to create fences");
		vkDestroySemaphore(device, computeFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, graphicsReadySemaphore, nullptr);
		vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
		vkDestroyCommandPool(device, computeCommandPool, nullptr);
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
		vkDestroyShaderModule(device, computeShader, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		vmaDestroyBuffer(allocator, voxelDataBuffer, voxelDataAllocation);
		vmaDestroyBuffer(allocator, uniformBuffer, uniformAllocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Запись compute command buffer (копирование данных + диспатч)
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	vkBeginCommandBuffer(computeCommandBuffer, &beginInfo);

	// Барьер для перехода staging -> voxel buffer
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = stagingBuffer;
	barrier.offset = 0;
	barrier.size = voxelDataSize;

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
						 nullptr, 1, &barrier, 0, nullptr);

	// Копирование данных
	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = voxelDataSize;
	vkCmdCopyBuffer(computeCommandBuffer, stagingBuffer, voxelDataBuffer, 1, &copyRegion);

	// Барьер для перехода voxel buffer -> compute shader
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.buffer = voxelDataBuffer;

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
						 0, nullptr, 1, &barrier, 0, nullptr);

	// Диспатч compute shader
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1,
							&descriptorSet, 0, nullptr);

	// 1024 вокселя / 64 (work group size) = 16 work groups
	vkCmdDispatch(computeCommandBuffer, 16, 1, 1);

	vkEndCommandBuffer(computeCommandBuffer);

	// Создание состояния приложения
	AppState *state = new AppState{};
	state->window = window;
	state->instance = instance;
	state->physicalDevice = physicalDevice;
	state->device = device;
	state->surface = surface;
	state->graphicsQueue = graphicsQueue;
	state->computeQueue = computeQueue;
	state->graphicsQueueFamily = graphicsQueueFamily;
	state->computeQueueFamily = computeQueueFamily;
	state->allocator = allocator;
	state->computePipeline = computePipeline;
	state->computePipelineLayout = computePipelineLayout;
	state->computeShader = computeShader;
	state->descriptorSetLayout = descriptorSetLayout;
	state->descriptorPool = descriptorPool;
	state->descriptorSet = descriptorSet;
	state->voxelDataBuffer = voxelDataBuffer;
	state->voxelDataAllocation = voxelDataAllocation;
	state->uniformBuffer = uniformBuffer;
	state->uniformAllocation = uniformAllocation;
	state->stagingBuffer = stagingBuffer;
	state->stagingAllocation = stagingAllocation;
	state->computeFinishedSemaphore = computeFinishedSemaphore;
	state->graphicsReadySemaphore = graphicsReadySemaphore;
	state->computeFence = computeFence;
	state->graphicsFence = graphicsFence;
	state->computeCommandPool = computeCommandPool;
	state->graphicsCommandPool = graphicsCommandPool;
	state->computeCommandBuffer = computeCommandBuffer;
	state->graphicsCommandBuffer = graphicsCommandBuffer;

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

	static float time = 0.0f;
	time += 0.016f; // ~60 FPS

	// Обновление uniform buffer
	void *mappedData = nullptr;
	vmaMapMemory(state->allocator, state->uniformAllocation, &mappedData);
	UniformData *uniforms = static_cast<UniformData *>(mappedData);

	uniforms->viewProjection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f) *
							   glm::lookAt(glm::vec3(0, 0, 20), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	uniforms->cameraPosition = glm::vec4(0, 0, 20, 1);
	uniforms->voxelGridSize = glm::vec4(32, 32, 32, 1.0f); // 32x32x32 grid, cell size 1.0
	uniforms->time = time;
	uniforms->deltaTime = 0.016f;

	vmaFlushAllocation(state->allocator, state->uniformAllocation, 0, sizeof(UniformData));
	vmaUnmapMemory(state->allocator, state->uniformAllocation);

	// Выполнение compute shader
	vkWaitForFences(state->device, 1, &state->computeFence, VK_TRUE, UINT64_MAX);
	vkResetFences(state->device, 1, &state->computeFence);

	VkSubmitInfo computeSubmitInfo = {};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &state->computeCommandBuffer;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &state->computeFinishedSemaphore;

	if (vkQueueSubmit(state->computeQueue, 1, &computeSubmitInfo, state->computeFence) != VK_SUCCESS) {
		SDL_Log("Compute queue submit failed");
		return SDL_APP_FAILURE;
	}

	// Здесь можно добавить графический рендеринг с использованием результатов compute shader
	// Для простоты примера просто ждём завершения compute операций

	vkWaitForFences(state->device, 1, &state->computeFence, VK_TRUE, UINT64_MAX);

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
