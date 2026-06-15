#pragma once

#include "core/Types.hpp"

void SetVulkanObjectName(
	const VulkanContextState &context,
	uint64_t handle,
	VkObjectType objectType,
	const char *name);

