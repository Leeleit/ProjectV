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
	uint32_t shaderGroupHandleSize = 32u;
	uint32_t shaderGroupBaseAlignment = 64u;
	uint32_t shaderGroupHandleAlignment = 16u;
	uint32_t maxRayRecursionDepth = 1u;
	uint32_t maxShaderGroupStride = 4096u;
	uint32_t maxRayHitAttributeSize = 32u;
};

bool ProbeHardwareRayTracingSupport(
	VkPhysicalDevice physicalDevice,
	HardwareRayTracingSupport *outSupport);

}  // namespace projectv::render
