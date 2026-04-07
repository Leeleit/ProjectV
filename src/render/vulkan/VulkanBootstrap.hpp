#ifndef VULKAN_BOOTSTRAP_HPP
#define VULKAN_BOOTSTRAP_HPP

#include "core/Types.hpp"

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame);

#endif
