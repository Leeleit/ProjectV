#pragma once
// Vulkan 1.4 headless setup + minimal helpers.
// No swapchain, no surface. Render to internal color + depth images, readback via host-visible buffer.

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace vb {

struct VkContext {
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	uint32_t graphicsFamily = 0;

	VkPhysicalDeviceProperties props{};
	VkPhysicalDeviceMemoryProperties memProps{};

	uint32_t minUboAlignment = 16;
};

bool InitVulkan(VkContext &ctx);
void DestroyVulkan(VkContext &ctx);

// Find first memory type matching typeBits + properties.
uint32_t FindMemoryType(const VkContext &ctx, uint32_t typeBits, VkMemoryPropertyFlags props);

// Create a buffer with given usage + memory properties, returns buffer + memory.
struct AllocatedBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceSize size = 0;
	void *mapped = nullptr;
};

AllocatedBuffer CreateBuffer(const VkContext &ctx,
							 VkDeviceSize size,
							 VkBufferUsageFlags usage,
							 VkMemoryPropertyFlags memProps,
							 bool hostMap = false);

void DestroyBuffer(const VkContext &ctx, AllocatedBuffer &buf);

// Create a 2D image (color or depth).
struct AllocatedImage {
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent2D extent{};
};

AllocatedImage CreateImage2D(const VkContext &ctx,
							 VkExtent2D extent,
							 VkFormat format,
							 VkImageUsageFlags usage,
							 VkImageAspectFlags aspect);

void DestroyImage(const VkContext &ctx, AllocatedImage &img);

VkShaderModule LoadShaderSPIRV(const VkContext &ctx, const uint32_t *code, size_t bytes);

VkCommandBuffer BeginOneShot(const VkContext &ctx);
void EndOneShot(const VkContext &ctx, VkCommandBuffer cmd);

} // namespace vb
