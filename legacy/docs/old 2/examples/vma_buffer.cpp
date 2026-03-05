// Пример: VMA — аллокатор, host-visible буфер, map/unmap
// Документация: docs/vma/quickstart.md
// Требует: instance, physicalDevice, device (headless Vulkan)

#define NOMINMAX
#define VK_NO_PROTOTYPES
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <cstdio>
#include <cstring>
#include <vector>

int main()
{
	if (volkInitialize() != VK_SUCCESS) {
		std::fprintf(stderr, "volkInitialize failed\n");
		return 1;
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = 0;
	instanceInfo.enabledLayerCount = 0;

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
		std::fprintf(stderr, "vkCreateInstance failed\n");
		return 1;
	}
	volkLoadInstance(instance);

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		vkDestroyInstance(instance, nullptr);
		return 1;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	VkPhysicalDevice physicalDevice = devices[0];

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.data());

	uint32_t queueFamilyIndex = 0;
	for (uint32_t i = 0; i < queueFamilyCount; ++i) {
		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queueFamilyIndex = i;
			break;
		}
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;

	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
		vkDestroyInstance(instance, nullptr);
		return 1;
	}
	volkLoadDevice(device);

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
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
		return 1;
	}

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = 1024;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	if (vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
		return 1;
	}

	void *mappedData = nullptr;
	if (vmaMapMemory(allocator, allocation, &mappedData) != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, buffer, allocation);
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
		return 1;
	}
	const char testData[] = "VMA map/unmap test";
	std::memcpy(mappedData, testData, sizeof(testData));
	vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
	vmaUnmapMemory(allocator, allocation);

	vmaDestroyBuffer(allocator, buffer, allocation);
	vmaDestroyAllocator(allocator);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);

	std::printf("VMA buffer: создан, записан, освобождён\n");
	return 0;
}
