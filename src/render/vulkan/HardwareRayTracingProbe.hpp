#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace projectv::render {

struct HardwareRayTracingSupport {
	bool accelerationStructure = false;
	bool rayQuery = false;
	bool rayTracingPipeline = false;
	uint32_t maxRecursionDepth = 0u;
};

bool ProbeHardwareRayTracingSupport(
	VkPhysicalDevice physicalDevice,
	HardwareRayTracingSupport *outSupport);

}  // namespace projectv::render