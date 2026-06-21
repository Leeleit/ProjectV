#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace projectv::render {

struct HardwareRayTracingSupport {
	bool accelerationStructure = false;
	bool rayQuery = false;
	bool rayTracingPipeline = false;
	bool deferredHostOperations = false;
	bool accelerationStructureHostCommands = false;
	bool bufferDeviceAddress = false;
	uint32_t maxRecursionDepth = 0u;
	uint64_t maxPrimitiveCount = 0u;
	uint64_t maxInstanceCount = 0u;
	uint32_t maxPerStageDescriptorAccelerationStructures = 0u;
	uint32_t minAccelerationStructureScratchOffsetAlignment = 1u;
};

bool ProbeHardwareRayTracingSupport(
	VkPhysicalDevice physicalDevice,
	HardwareRayTracingSupport *outSupport);

}  // namespace projectv::render
