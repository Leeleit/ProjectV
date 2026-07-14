#include "volk.h" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/vulkan/VulkanBootstrapInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/HardwareRayTracingProbe.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include "fmt/format.h"

#include "core/EnvUtils.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <set>
#include <vector>

namespace {

constexpr std::array<const char *, 1> kRequiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char kOptionalTracyCalibratedTimestampsExtension[] = VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME;

constexpr char kOptionalDynamicRenderingUnusedAttachmentsExtension[] =
	"VK_EXT_dynamic_rendering_unused_attachments";

constexpr char kMeshShaderExtension[] = "VK_EXT_mesh_shader";
constexpr char kAccelerationStructureExtension[] = "VK_KHR_acceleration_structure";
constexpr char kRayQueryExtension[] = "VK_KHR_ray_query";
constexpr char kRayTracingPipelineExtension[] = "VK_KHR_ray_tracing_pipeline";
constexpr char kPipelineLibraryExtension[] = "VK_KHR_pipeline_library";
constexpr char kDeferredHostOperationsExtension[] = "VK_KHR_deferred_host_operations";
constexpr char kShaderInvocationReorderExtension[] = VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME;
constexpr char kPresentIdExtension[] = VK_KHR_PRESENT_ID_EXTENSION_NAME;
constexpr char kPresentWaitExtension[] = VK_KHR_PRESENT_WAIT_EXTENSION_NAME;

bool IsEnvEnabled(const char *const name)
{
	const char *const value = projectv::core::GetEnvVar(name);
	return value != nullptr && (std::strcmp(value, "ON") == 0 || std::strcmp(value, "1") == 0);
}

bool IsEnvSetToOn(const char *const name)
{
	const char *const value = projectv::core::GetEnvVar(name);
	return value != nullptr && std::strcmp(value, "ON") == 0;
}

bool CheckDeviceExtensionSupport(const VkPhysicalDevice physicalDevice)
{
	uint32_t extensionCount = 0;
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	if (vkEnumerateDeviceExtensionProperties(
			physicalDevice,
			nullptr,
			&extensionCount,
			availableExtensions.data()) != VK_SUCCESS) {
		return false;
	}

	for (const char *required : kRequiredDeviceExtensions) {
		bool found = false;
		for (const auto &[extensionName, specVersion] : availableExtensions) {
			if (std::strcmp(required, extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			runtime::LogRuntimeFailure(
				"Init",
				"CheckDeviceExtensionSupport",
				fmt::format("missing device extension: {}", required));
			return false;
		}
	}

	return true;
}

bool HasDeviceExtension(
	const VkPhysicalDevice physicalDevice,
	const char *extensionName)
{
	uint32_t extensionCount = 0;
	const VkResult enumerateExtensionCountResult =
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	if (enumerateExtensionCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckDeviceExtensionSupport.vkEnumerateDeviceExtensionProperties(count)",
			enumerateExtensionCountResult);
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	const VkResult enumerateExtensionDataResult = vkEnumerateDeviceExtensionProperties(
		physicalDevice,
		nullptr,
		&extensionCount,
		availableExtensions.data());
	if (enumerateExtensionDataResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckDeviceExtensionSupport.vkEnumerateDeviceExtensionProperties(data)",
			enumerateExtensionDataResult);
		return false;
	}

	for (const auto &[availableExtensionName, specVersion] : availableExtensions) {
		(void)specVersion;
		if (std::strcmp(extensionName, availableExtensionName) == 0) {
			return true;
		}
	}

	return false;
}

void ProbePresentFeatures(
	const VkPhysicalDevice physicalDevice,
	const bool hasPresentIdExtension,
	const bool hasPresentWaitExtension,
	VkPhysicalDevicePresentIdFeaturesKHR *const outPresentIdFeatures,
	VkPhysicalDevicePresentWaitFeaturesKHR *const outPresentWaitFeatures)
{
	if (!hasPresentIdExtension && !hasPresentWaitExtension) {
		return;
	}

	VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, nullptr};
	VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, nullptr};
	if (hasPresentIdExtension) {
		presentIdFeatures.pNext = hasPresentWaitExtension ? &presentWaitFeatures : nullptr;
	}
	VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	features2.pNext = hasPresentIdExtension
						  ? static_cast<void *>(&presentIdFeatures)
						  : static_cast<void *>(&presentWaitFeatures);
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
	if (hasPresentIdExtension) {
		*outPresentIdFeatures = presentIdFeatures;
	}
	if (hasPresentWaitExtension) {
		*outPresentWaitFeatures = presentWaitFeatures;
	}
}

bool FindGraphicsPresentQueueFamily(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	uint32_t *outQueueFamilyIndex)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	if (familyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		VkBool32 presentSupport = VK_FALSE;
		if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport) != VK_SUCCESS) {
			continue;
		}

		if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
			(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
			presentSupport) {
			*outQueueFamilyIndex = i;
			return true;
		}
	}

	return false;
}

bool FindDedicatedComputeQueueFamily(
	const VkPhysicalDevice physicalDevice,
	const uint32_t graphicsFamilyIndex,
	uint32_t *outQueueFamilyIndex)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	if (familyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		if (i == graphicsFamilyIndex) {
			continue;
		}
		const VkQueueFamilyProperties &family = families[i];
		if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) {
			continue;
		}
		if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			continue;
		}
		if (family.queueCount == 0) {
			continue;
		}
		*outQueueFamilyIndex = i;
		return true;
	}

	return false;
}

bool CheckSwapchainSurfaceSupport(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
{
	uint32_t formatCount = 0;
	const VkResult formatCountResult =
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckSwapchainSurfaceSupport.vkGetPhysicalDeviceSurfaceFormatsKHR(count)",
			formatCountResult);
		return false;
	}
	if (formatCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckSwapchainSurfaceSupport.SurfaceFormats",
			"no surface formats found");
		return false;
	}

	uint32_t presentModeCount = 0;
	const VkResult presentModeCountResult =
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckSwapchainSurfaceSupport.vkGetPhysicalDeviceSurfacePresentModesKHR(count)",
			presentModeCountResult);
		return false;
	}
	if (presentModeCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckSwapchainSurfaceSupport.PresentModes",
			"no present modes found");
		return false;
	}

	return true;
}

bool CheckRequiredFeatures(
	const VkPhysicalDevice physicalDevice,
	VkPhysicalDeviceFeatures *outFeatures,
	VkPhysicalDeviceVulkan12Features *outFeatures12,
	VkPhysicalDeviceVulkan13Features *outFeatures13,
	VkPhysicalDeviceVulkan14Features *outFeatures14,
	VkPhysicalDeviceMeshShaderFeaturesEXT *outMeshShaderFeatures,
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT *outDynamicRenderingUnusedAttachmentsFeatures,
	VkPhysicalDeviceAccelerationStructureFeaturesKHR *outAccelerationStructureFeatures,
	VkPhysicalDeviceRayQueryFeaturesKHR *outRayQueryFeatures,
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR *outRayTracingPipelineFeatures)
{
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	VkPhysicalDeviceVulkan14Features features14{};
	features14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
	features13.pNext = &features14;

	VkBaseOutStructure *lastChainTarget = reinterpret_cast<VkBaseOutStructure *>(&features12);

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	if (outDynamicRenderingUnusedAttachmentsFeatures != nullptr) {
		dynamicRenderingUnusedAttachmentsFeatures.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&dynamicRenderingUnusedAttachmentsFeatures);
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	if (outMeshShaderFeatures != nullptr) {
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&meshShaderFeatures);
	}

	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
	if (outRayQueryFeatures != nullptr) {
		rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&rayQueryFeatures);
	}

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	if (outAccelerationStructureFeatures != nullptr) {
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		accelerationStructureFeatures.pNext = reinterpret_cast<VkBaseOutStructure *>(&rayQueryFeatures);
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&accelerationStructureFeatures);
	}

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	if (outRayTracingPipelineFeatures != nullptr) {
		rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&rayTracingPipelineFeatures);
	}
	features14.pNext = &features12;
	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features13;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
	const VkPhysicalDeviceFeatures supportedFeatures = features2.features;

	if (!supportedFeatures.multiDrawIndirect) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.multiDrawIndirect",
			"device does not support multiDrawIndirect");
		return false;
	}

	if (!supportedFeatures.drawIndirectFirstInstance) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.drawIndirectFirstInstance",
			"device does not support drawIndirectFirstInstance");
		return false;
	}

	if (!features12.drawIndirectCount) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.drawIndirectCount",
			"device does not support drawIndirectCount");
		return false;
	}

	if (!features12.hostQueryReset) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.hostQueryReset",
			"device does not support hostQueryReset");
		return false;
	}

	if (!features13.dynamicRendering) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.dynamicRendering",
			"device does not support dynamicRendering");
		return false;
	}

	if (!features13.synchronization2) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.synchronization2",
			"device does not support synchronization2");
		return false;
	}

	*outFeatures = supportedFeatures;
	*outFeatures12 = features12;
	*outFeatures13 = features13;
	if (outFeatures14 != nullptr) {
		*outFeatures14 = features14;
	}
	if (outDynamicRenderingUnusedAttachmentsFeatures != nullptr) {
		*outDynamicRenderingUnusedAttachmentsFeatures = dynamicRenderingUnusedAttachmentsFeatures;
	}
	if (outMeshShaderFeatures != nullptr) {
		*outMeshShaderFeatures = meshShaderFeatures;
	}
	if (outAccelerationStructureFeatures != nullptr) {
		*outAccelerationStructureFeatures = accelerationStructureFeatures;
	}
	if (outRayQueryFeatures != nullptr) {
		*outRayQueryFeatures = rayQueryFeatures;
	}
	if (outRayTracingPipelineFeatures != nullptr) {
		*outRayTracingPipelineFeatures = rayTracingPipelineFeatures;
	}
	return true;
}

} // namespace

namespace projectv::render {

VkPhysicalDeviceFeatures BuildEnabledFeatures(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceFeatures enabled{};
	enabled.multiDrawIndirect = selected.features.multiDrawIndirect ? VK_TRUE : VK_FALSE;
	enabled.drawIndirectFirstInstance = selected.features.drawIndirectFirstInstance ? VK_TRUE : VK_FALSE;
	enabled.logicOp = selected.features.logicOp ? VK_TRUE : VK_FALSE;
	return enabled;
}

VkPhysicalDeviceVulkan12Features BuildEnabledFeatures12(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan12Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabled.drawIndirectCount = selected.features12.drawIndirectCount ? VK_TRUE : VK_FALSE;
	enabled.hostQueryReset = selected.features12.hostQueryReset ? VK_TRUE : VK_FALSE;
	enabled.timelineSemaphore = selected.features12.timelineSemaphore ? VK_TRUE : VK_FALSE;
	if (selected.supportsAccelerationStructure && selected.supportsRayQuery && selected.rayTracingSupport.bufferDeviceAddress) {
		enabled.bufferDeviceAddress = VK_TRUE;
	}
	if (IsEnvEnabled("PROJECTV_BINDLESS")) {
		enabled.descriptorIndexing = selected.features12.descriptorIndexing ? VK_TRUE : VK_FALSE;
		enabled.runtimeDescriptorArray = selected.features12.runtimeDescriptorArray ? VK_TRUE : VK_FALSE;
		enabled.descriptorBindingPartiallyBound = selected.features12.descriptorBindingPartiallyBound ? VK_TRUE : VK_FALSE;
		enabled.descriptorBindingVariableDescriptorCount =
			selected.features12.descriptorBindingVariableDescriptorCount ? VK_TRUE : VK_FALSE;
		enabled.descriptorBindingSampledImageUpdateAfterBind =
			selected.features12.descriptorBindingSampledImageUpdateAfterBind ? VK_TRUE : VK_FALSE;
		enabled.shaderSampledImageArrayNonUniformIndexing =
			selected.features12.shaderSampledImageArrayNonUniformIndexing ? VK_TRUE : VK_FALSE;
	}
	return enabled;
}

VkPhysicalDeviceVulkan13Features BuildEnabledFeatures13(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan13Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled.dynamicRendering = selected.features13.dynamicRendering ? VK_TRUE : VK_FALSE;
	enabled.synchronization2 = selected.features13.synchronization2 ? VK_TRUE : VK_FALSE;
	enabled.shaderDemoteToHelperInvocation = selected.features13.shaderDemoteToHelperInvocation ? VK_TRUE : VK_FALSE;
	return enabled;
}

VkPhysicalDeviceVulkan14Features BuildEnabledFeatures14(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan14Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
	enabled.maintenance5 = selected.features14.maintenance5 ? VK_TRUE : VK_FALSE;
	enabled.maintenance6 = selected.features14.maintenance6 ? VK_TRUE : VK_FALSE;
	enabled.pushDescriptor = selected.features14.pushDescriptor ? VK_TRUE : VK_FALSE;
	enabled.indexTypeUint8 = selected.features14.indexTypeUint8 ? VK_TRUE : VK_FALSE;
	enabled.dynamicRenderingLocalRead =
		selected.features14.dynamicRenderingLocalRead && IsEnvSetToOn("PROJECTV_DYNAMIC_RENDERING_LOCAL_READ")
			? VK_TRUE
			: VK_FALSE;
	enabled.shaderFloatControls2 = selected.features14.shaderFloatControls2 ? VK_TRUE : VK_FALSE;
	if (IsEnvEnabled("PROJECTV_HOST_IMAGE_COPY")) {
		enabled.hostImageCopy = selected.features14.hostImageCopy ? VK_TRUE : VK_FALSE;
	}
	return enabled;
}

std::vector<const char *> BuildDeviceExtensionList(const PhysicalDeviceCandidate &selected)
{
	std::vector deviceExtensions(
		kRequiredDeviceExtensions.begin(),
		kRequiredDeviceExtensions.end());
	if (selected.supportsTracyCalibratedTimestamps) {
		deviceExtensions.push_back(kOptionalTracyCalibratedTimestampsExtension);
	}
	if (selected.supportsDynamicRenderingUnusedAttachments) {
		deviceExtensions.push_back(kOptionalDynamicRenderingUnusedAttachmentsExtension);
	}
	if (selected.supportsPresentId) {
		deviceExtensions.push_back(kPresentIdExtension);
	}
	if (selected.supportsPresentWait) {
		deviceExtensions.push_back(kPresentWaitExtension);
	}
	const bool meshShaderRequested = projectv::core::GetEnvVar("PROJECTV_MESH_SHADER_PIPELINE") != nullptr;
	if (meshShaderRequested && selected.supportsMeshShader) {
		deviceExtensions.push_back(kMeshShaderExtension);
	}
	const bool rtxSupported = selected.supportsAccelerationStructure && selected.supportsRayQuery;
	if (rtxSupported) {
		deviceExtensions.push_back(kAccelerationStructureExtension);
		deviceExtensions.push_back(kRayQueryExtension);
		if (selected.supportsRayTracingPipeline) {
			deviceExtensions.push_back(kRayTracingPipelineExtension);
		}
		if (selected.rayTracingSupport.pipelineLibrary) {
			deviceExtensions.push_back(kPipelineLibraryExtension);
		}
		if (selected.rayTracingSupport.deferredHostOperations) {
			deviceExtensions.push_back(kDeferredHostOperationsExtension);
		}
		const char *serEnv = projectv::core::GetEnvVar("PROJECTV_RTX_SER");
		if (serEnv != nullptr && serEnv[0] != '\0' && serEnv[0] != '0' && selected.rayTracingSupport.shaderInvocationReorder) {
			deviceExtensions.push_back(kShaderInvocationReorderExtension);
		}
	}

	// Dedup: same extension may be both in kRequired and conditionally added.
	std::set uniqueExtensions(deviceExtensions.begin(), deviceExtensions.end());
	deviceExtensions.assign(uniqueExtensions.begin(), uniqueExtensions.end());
	return deviceExtensions;
}

bool TryPickPhysicalDevice(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	PhysicalDeviceCandidate *outCandidate)
{
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion < projectv::render::GetMinVulkanApiVersion()) {
		return false;
	}

	uint32_t queueFamilyIndex = UINT32_MAX;
	if (!FindGraphicsPresentQueueFamily(physicalDevice, surface, &queueFamilyIndex)) {
		return false;
	}

	uint32_t dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	const bool supportsDedicatedComputeQueue =
		FindDedicatedComputeQueueFamily(
			physicalDevice,
			queueFamilyIndex,
			&dedicatedComputeQueueFamilyIndex);

	if (!CheckDeviceExtensionSupport(physicalDevice)) {
		return false;
	}

	if (!CheckSwapchainSurfaceSupport(physicalDevice, surface)) {
		return false;
	}

	VkPhysicalDeviceFeatures supportedFeatures{};
	VkPhysicalDeviceVulkan12Features supportedFeatures12{};
	VkPhysicalDeviceVulkan13Features supportedFeatures13{};
	VkPhysicalDeviceVulkan14Features supportedFeatures14{};
	const bool deviceHasDynamicRenderingUnusedAttachments =
		HasDeviceExtension(physicalDevice, kOptionalDynamicRenderingUnusedAttachmentsExtension);
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT supportedDynamicRenderingUnusedAttachmentsFeatures{};
	const bool deviceHasMeshShader = HasDeviceExtension(physicalDevice, kMeshShaderExtension);
	VkPhysicalDeviceMeshShaderFeaturesEXT supportedMeshShaderFeatures{};
	const bool deviceHasPresentId = HasDeviceExtension(physicalDevice, kPresentIdExtension);
	const bool deviceHasPresentWait = HasDeviceExtension(physicalDevice, kPresentWaitExtension);
	VkPhysicalDevicePresentIdFeaturesKHR supportedPresentIdFeatures{};
	VkPhysicalDevicePresentWaitFeaturesKHR supportedPresentWaitFeatures{};
	ProbePresentFeatures(
		physicalDevice,
		deviceHasPresentId,
		deviceHasPresentWait,
		&supportedPresentIdFeatures,
		&supportedPresentWaitFeatures);
	HardwareRayTracingSupport rtxSupport{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR supportedAccelerationStructureFeatures{};
	VkPhysicalDeviceRayQueryFeaturesKHR supportedRayQueryFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR supportedRayTracingPipelineFeatures{};
	const bool deviceHasAccelerationStructure =
		HasDeviceExtension(physicalDevice, kAccelerationStructureExtension);
	const bool deviceHasRayQuery = HasDeviceExtension(physicalDevice, kRayQueryExtension);
	const bool rtxProbeSucceeded =
		deviceHasAccelerationStructure && deviceHasRayQuery
			? projectv::render::ProbeHardwareRayTracingSupport(physicalDevice, &rtxSupport)
			: false;
	if (!CheckRequiredFeatures(
			physicalDevice,
			&supportedFeatures,
			&supportedFeatures12,
			&supportedFeatures13,
			&supportedFeatures14,
			deviceHasMeshShader ? &supportedMeshShaderFeatures : nullptr,
			deviceHasDynamicRenderingUnusedAttachments ? &supportedDynamicRenderingUnusedAttachmentsFeatures : nullptr,
			rtxProbeSucceeded ? &supportedAccelerationStructureFeatures : nullptr,
			rtxProbeSucceeded ? &supportedRayQueryFeatures : nullptr,
			rtxProbeSucceeded && rtxSupport.rayTracingPipeline ? &supportedRayTracingPipelineFeatures : nullptr)) {
		return false;
	}

	outCandidate->device = physicalDevice;
	outCandidate->queueFamilyIndex = queueFamilyIndex;
	outCandidate->dedicatedComputeQueueFamilyIndex = dedicatedComputeQueueFamilyIndex;
	outCandidate->supportsDedicatedComputeQueue = supportsDedicatedComputeQueue;
	outCandidate->features = supportedFeatures;
	outCandidate->features12 = supportedFeatures12;
	outCandidate->features13 = supportedFeatures13;
	outCandidate->features14 = supportedFeatures14;
	outCandidate->meshShaderFeatures = supportedMeshShaderFeatures;
	outCandidate->supportsMeshShader = deviceHasMeshShader;
	outCandidate->dynamicRenderingUnusedAttachmentsFeatures = supportedDynamicRenderingUnusedAttachmentsFeatures;
	outCandidate->presentIdFeatures = supportedPresentIdFeatures;
	outCandidate->presentWaitFeatures = supportedPresentWaitFeatures;
	outCandidate->accelerationStructureFeatures = supportedAccelerationStructureFeatures;
	outCandidate->rayQueryFeatures = supportedRayQueryFeatures;
	outCandidate->rayTracingPipelineFeatures = supportedRayTracingPipelineFeatures;
	outCandidate->rayTracingSupport = rtxSupport;
	outCandidate->supportsAccelerationStructure = deviceHasAccelerationStructure && rtxProbeSucceeded;
	outCandidate->supportsRayQuery = deviceHasRayQuery && rtxProbeSucceeded;
	outCandidate->supportsRayTracingPipeline =
		rtxProbeSucceeded && rtxSupport.rayTracingPipeline && supportedRayTracingPipelineFeatures.rayTracingPipeline == VK_TRUE;
	outCandidate->supportsTracyCalibratedTimestamps =
		HasDeviceExtension(physicalDevice, kOptionalTracyCalibratedTimestampsExtension);
	outCandidate->supportsDynamicRenderingUnusedAttachments = deviceHasDynamicRenderingUnusedAttachments;
	outCandidate->supportsPresentId = deviceHasPresentId && supportedPresentIdFeatures.presentId == VK_TRUE;
	outCandidate->supportsPresentWait =
		outCandidate->supportsPresentId && deviceHasPresentWait && supportedPresentWaitFeatures.presentWait == VK_TRUE;
	return true;
}

} // namespace projectv::render
