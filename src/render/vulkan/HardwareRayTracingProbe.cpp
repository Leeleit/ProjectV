#include "render/vulkan/HardwareRayTracingProbe.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <vector>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"

namespace projectv::render {

namespace {

constexpr const char *kAccelerationStructureExtension = "VK_KHR_acceleration_structure";
constexpr const char *kRayQueryExtension = "VK_KHR_ray_query";
constexpr const char *kRayTracingPipelineExtension = "VK_KHR_ray_tracing_pipeline";
constexpr const char *kPipelineLibraryExtension = "VK_KHR_pipeline_library";
constexpr const char *kDeferredHostOperationsExtension = "VK_KHR_deferred_host_operations";

bool HasDeviceExtension(
	const VkPhysicalDevice physicalDevice,
	const char *extensionName)
{
	uint32_t extensionCount = 0;
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
		return false;
	}
	std::vector<VkExtensionProperties> extensions(extensionCount);
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
		return false;
	}
	for (const auto &[extName, specVersion] : extensions) {
		(void)specVersion;
		if (std::strcmp(extName, extensionName) == 0) {
			return true;
		}
	}
	return false;
}

} // namespace

bool ProbeHardwareRayTracingSupport(
	const VkPhysicalDevice physicalDevice,
	HardwareRayTracingSupport *outSupport)
{
	if (physicalDevice == VK_NULL_HANDLE || outSupport == nullptr) {
		return false;
	}
	*outSupport = HardwareRayTracingSupport{};

	outSupport->accelerationStructure = HasDeviceExtension(physicalDevice, kAccelerationStructureExtension);
	outSupport->rayQuery = HasDeviceExtension(physicalDevice, kRayQueryExtension);
	outSupport->rayTracingPipeline = HasDeviceExtension(physicalDevice, kRayTracingPipelineExtension);
	outSupport->pipelineLibrary = HasDeviceExtension(physicalDevice, kPipelineLibraryExtension);
	outSupport->deferredHostOperations = HasDeviceExtension(physicalDevice, kDeferredHostOperationsExtension);

	if (outSupport->rayQuery) {
		outSupport->maxRecursionDepth = 1u;
	}

	if (!outSupport->accelerationStructure || !outSupport->rayQuery) {
		runtime::LogRuntimeFailure(
			"Render",
			"ProbeHardwareRayTracingSupport",
			"VK_KHR_acceleration_structure + VK_KHR_ray_query not supported");
		return false;
	}

	VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{};
	asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtpProperties{};
	if (outSupport->rayTracingPipeline) {
		rtpProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		asProperties.pNext = reinterpret_cast<VkBaseOutStructure *>(&rtpProperties);
	}
	VkPhysicalDeviceProperties2 deviceProperties2{};
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &asProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);
	outSupport->maxPrimitiveCount = asProperties.maxPrimitiveCount;
	outSupport->maxInstanceCount = asProperties.maxInstanceCount;
	outSupport->maxPerStageDescriptorAccelerationStructures = asProperties.maxPerStageDescriptorAccelerationStructures;
	outSupport->minAccelerationStructureScratchOffsetAlignment =
		asProperties.minAccelerationStructureScratchOffsetAlignment != 0u
			? asProperties.minAccelerationStructureScratchOffsetAlignment
			: 1u;
	if (outSupport->rayTracingPipeline) {
		outSupport->shaderGroupHandleSize =
			rtpProperties.shaderGroupHandleSize != 0u ? rtpProperties.shaderGroupHandleSize : 32u;
		outSupport->shaderGroupBaseAlignment =
			rtpProperties.shaderGroupBaseAlignment != 0u ? rtpProperties.shaderGroupBaseAlignment : 64u;
		outSupport->shaderGroupHandleAlignment =
			rtpProperties.shaderGroupHandleAlignment != 0u ? rtpProperties.shaderGroupHandleAlignment : 16u;
		outSupport->maxRayRecursionDepth =
			rtpProperties.maxRayRecursionDepth != 0u ? rtpProperties.maxRayRecursionDepth : 1u;
		outSupport->maxShaderGroupStride =
			rtpProperties.maxShaderGroupStride != 0u ? rtpProperties.maxShaderGroupStride : 4096u;
		outSupport->maxRayHitAttributeSize =
			rtpProperties.maxRayHitAttributeSize != 0u ? rtpProperties.maxRayHitAttributeSize : 32u;
	}

	VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures{};
	rqFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
	asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	asFeatures.pNext = &rqFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
	bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bdaFeatures.pNext = &asFeatures;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpFeaturesStruct{};
	if (outSupport->rayTracingPipeline) {
		rtpFeaturesStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtpFeaturesStruct.pNext = &bdaFeatures;
	}
	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = outSupport->rayTracingPipeline
						  ? reinterpret_cast<void *>(&rtpFeaturesStruct)
						  : reinterpret_cast<void *>(&bdaFeatures);
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
	outSupport->accelerationStructureHostCommands = asFeatures.accelerationStructureHostCommands == VK_TRUE;
	outSupport->bufferDeviceAddress =
		bdaFeatures.bufferDeviceAddress == VK_TRUE || bdaFeatures.bufferDeviceAddressCaptureReplay == VK_TRUE;

	SDL_Log(
		"Render: ProbeHardwareRayTracingSupport: RTX path available (accelerationStructure=%d rayQuery=%d "
		"rayTracingPipeline=%d pipelineLibrary=%d deferredHostOps=%d hostCommands=%d bufferDeviceAddress=%d "
		"maxPrimitives=%llu minScratchAlign=%u sbtHandle=%u sbtBaseAlign=%u sbtHandleAlign=%u)",
		1,
		1,
		outSupport->rayTracingPipeline ? 1 : 0,
		outSupport->pipelineLibrary ? 1 : 0,
		outSupport->deferredHostOperations ? 1 : 0,
		outSupport->accelerationStructureHostCommands ? 1 : 0,
		outSupport->bufferDeviceAddress ? 1 : 0,
		static_cast<unsigned long long>(outSupport->maxPrimitiveCount),
		outSupport->minAccelerationStructureScratchOffsetAlignment,
		outSupport->shaderGroupHandleSize,
		outSupport->shaderGroupBaseAlignment,
		outSupport->shaderGroupHandleAlignment);
	return true;
}

} // namespace projectv::render
