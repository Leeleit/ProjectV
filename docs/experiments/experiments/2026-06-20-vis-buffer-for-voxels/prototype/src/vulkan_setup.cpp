#include "vulkan_setup.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace vb {

bool InitVulkan(VkContext &ctx)
{
	// ---- Instance ----
	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "visbuffer_bench",
		.applicationVersion = 1,
		.pEngineName = "vb-engine",
		.engineVersion = 1,
		.apiVersion = VK_API_VERSION_1_4,
	};
	VkInstanceCreateInfo instanceCI{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
	};
	if (vkCreateInstance(&instanceCI, nullptr, &ctx.instance) != VK_SUCCESS) {
		std::fprintf(stderr, "vkCreateInstance failed\n");
		return false;
	}

	// ---- Physical device ----
	uint32_t physCount = 0;
	vkEnumeratePhysicalDevices(ctx.instance, &physCount, nullptr);
	if (physCount == 0) {
		std::fprintf(stderr, "no Vulkan physical devices\n");
		return false;
	}
	std::vector<VkPhysicalDevice> physDevs(physCount);
	vkEnumeratePhysicalDevices(ctx.instance, &physCount, physDevs.data());
	ctx.phys = physDevs[0];

	vkGetPhysicalDeviceProperties(ctx.phys, &ctx.props);
	vkGetPhysicalDeviceMemoryProperties(ctx.phys, &ctx.memProps);

	// ---- Queue family ----
	uint32_t qfCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.phys, &qfCount, nullptr);
	std::vector<VkQueueFamilyProperties> qfs(qfCount);
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.phys, &qfCount, qfs.data());
	ctx.graphicsFamily = UINT32_MAX;
	for (uint32_t i = 0; i < qfCount; ++i) {
		if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			ctx.graphicsFamily = i;
			break;
		}
	}
	if (ctx.graphicsFamily == UINT32_MAX) {
		std::fprintf(stderr, "no graphics queue family\n");
		return false;
	}

	// ---- Device ----
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo qci{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = ctx.graphicsFamily,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceFeatures(ctx.phys, &features);

	VkDeviceCreateInfo dci{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = nullptr,
		.pEnabledFeatures = &features,
	};
	if (vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.device) != VK_SUCCESS) {
		std::fprintf(stderr, "vkCreateDevice failed\n");
		return false;
	}
	vkGetDeviceQueue(ctx.device, ctx.graphicsFamily, 0, &ctx.graphicsQueue);

	// ---- Command pool ----
	VkCommandPoolCreateInfo cpci{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = ctx.graphicsFamily,
	};
	vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool);

	ctx.minUboAlignment = ctx.props.limits.minUniformBufferOffsetAlignment;
	std::fprintf(stderr, "Vulkan OK: %s (api %u.%u.%u), UBO align %u B\n",
				 ctx.props.deviceName,
				 VK_API_VERSION_MAJOR(ctx.props.apiVersion),
				 VK_API_VERSION_MINOR(ctx.props.apiVersion),
				 VK_API_VERSION_PATCH(ctx.props.apiVersion),
				 ctx.minUboAlignment);
	return true;
}

void DestroyVulkan(VkContext &ctx)
{
	if (ctx.cmdPool)
		vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
	if (ctx.device)
		vkDestroyDevice(ctx.device, nullptr);
	if (ctx.instance)
		vkDestroyInstance(ctx.instance, nullptr);
	ctx = {};
}

uint32_t FindMemoryType(const VkContext &ctx, uint32_t typeBits, VkMemoryPropertyFlags props)
{
	for (uint32_t i = 0; i < ctx.memProps.memoryTypeCount; ++i) {
		if ((typeBits & (1u << i)) &&
			(ctx.memProps.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	throw std::runtime_error("FindMemoryType: no match");
}

AllocatedBuffer CreateBuffer(const VkContext &ctx, VkDeviceSize size,
							 VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
							 bool hostMap)
{
	AllocatedBuffer buf{};
	buf.size = size;
	VkBufferCreateInfo bci{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	vkCreateBuffer(ctx.device, &bci, nullptr, &buf.buffer);

	VkMemoryRequirements req{};
	vkGetBufferMemoryRequirements(ctx.device, buf.buffer, &req);
	uint32_t memType = FindMemoryType(ctx, req.memoryTypeBits, memProps);
	VkMemoryAllocateInfo mai{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req.size,
		.memoryTypeIndex = memType,
	};
	vkAllocateMemory(ctx.device, &mai, nullptr, &buf.memory);
	vkBindBufferMemory(ctx.device, buf.buffer, buf.memory, 0);
	if (hostMap) {
		vkMapMemory(ctx.device, buf.memory, 0, size, 0, &buf.mapped);
	}
	return buf;
}

void DestroyBuffer(const VkContext &ctx, AllocatedBuffer &buf)
{
	if (buf.mapped)
		vkUnmapMemory(ctx.device, buf.memory);
	if (buf.buffer)
		vkDestroyBuffer(ctx.device, buf.buffer, nullptr);
	if (buf.memory)
		vkFreeMemory(ctx.device, buf.memory, nullptr);
	buf = {};
}

AllocatedImage CreateImage2D(const VkContext &ctx, VkExtent2D extent, VkFormat format,
							 VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
	AllocatedImage img{};
	img.format = format;
	img.extent = extent;

	VkImageCreateInfo ici{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {extent.width, extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	vkCreateImage(ctx.device, &ici, nullptr, &img.image);

	VkMemoryRequirements req{};
	vkGetImageMemoryRequirements(ctx.device, img.image, &req);
	VkMemoryAllocateInfo mai{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req.size,
		.memoryTypeIndex = FindMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};
	vkAllocateMemory(ctx.device, &mai, nullptr, &img.memory);
	vkBindImageMemory(ctx.device, img.image, img.memory, 0);

	VkImageViewCreateInfo ivci{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = img.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {aspect, 0, 1, 0, 1},
	};
	vkCreateImageView(ctx.device, &ivci, nullptr, &img.view);
	return img;
}

void DestroyImage(const VkContext &ctx, AllocatedImage &img)
{
	if (img.view)
		vkDestroyImageView(ctx.device, img.view, nullptr);
	if (img.image)
		vkDestroyImage(ctx.device, img.image, nullptr);
	if (img.memory)
		vkFreeMemory(ctx.device, img.memory, nullptr);
	img = {};
}

VkShaderModule LoadShaderSPIRV(const VkContext &ctx, const uint32_t *code, size_t bytes)
{
	VkShaderModuleCreateInfo smci{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = bytes,
		.pCode = code,
	};
	VkShaderModule sm = VK_NULL_HANDLE;
	vkCreateShaderModule(ctx.device, &smci, nullptr, &sm);
	return sm;
}

VkCommandBuffer BeginOneShot(const VkContext &ctx)
{
	VkCommandBufferAllocateInfo cbai{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ctx.cmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(ctx.device, &cbai, &cmd);
	VkCommandBufferBeginInfo cbbi{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(cmd, &cbbi);
	return cmd;
}

void EndOneShot(const VkContext &ctx, VkCommandBuffer cmd)
{
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};
	vkQueueSubmit(ctx.graphicsQueue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(ctx.graphicsQueue);
	vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1, &cmd);
}

} // namespace vb
