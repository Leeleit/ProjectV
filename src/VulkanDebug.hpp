#ifndef VULKAN_DEBUG_HPP
#define VULKAN_DEBUG_HPP

#include "Types.hpp"

void SetVulkanObjectName(
	const VulkanContextState &context,
	uint64_t handle,
	VkObjectType objectType,
	const char *name);

#endif
