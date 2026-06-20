#include "render/vulkan/HardwareRayTracingProbe.hpp"

#include <cstring>
#include <vector>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"

namespace projectv::render {

namespace {

constexpr const char *kAccelerationStructureExtension = "VK_KHR_acceleration_structure";
constexpr const char *kRayQueryExtension = "VK_KHR_ray_query";
constexpr const char *kRayTracingPipelineExtension = "VK_KHR_ray_tracing_pipeline";

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
	for (const VkExtensionProperties &ext : extensions) {
		if (std::strcmp(ext.extensionName, extensionName) == 0) {
			return true;
		}
	}
	return false;
}

}  // namespace

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

	SDL_Log(
		"Render: ProbeHardwareRayTracingSupport: RTX path available (accelerationStructure + rayQuery)");
	return true;
}

}  // namespace projectv::render