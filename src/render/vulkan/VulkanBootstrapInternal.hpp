#pragma once

#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/HardwareRayTracingProbe.hpp"

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

namespace projectv::render {

struct PhysicalDeviceCandidate {
	VkPhysicalDevice device = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = UINT32_MAX;
	uint32_t dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	VkPhysicalDeviceFeatures features{};
	VkPhysicalDeviceVulkan12Features features12{};
	VkPhysicalDeviceVulkan13Features features13{};
	VkPhysicalDeviceVulkan14Features features14{};
	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	HardwareRayTracingSupport rayTracingSupport{};
	bool supportsTracyCalibratedTimestamps = false;
	bool supportsDynamicRenderingUnusedAttachments = false;
	bool supportsDedicatedComputeQueue = false;
	bool supportsMeshShader = false;
	bool supportsAccelerationStructure = false;
	bool supportsRayQuery = false;
	bool supportsRayTracingPipeline = false;
};

VkPhysicalDeviceFeatures BuildEnabledFeatures(const PhysicalDeviceCandidate &selected);
VkPhysicalDeviceVulkan12Features BuildEnabledFeatures12(const PhysicalDeviceCandidate &selected);
VkPhysicalDeviceVulkan13Features BuildEnabledFeatures13(const PhysicalDeviceCandidate &selected);
VkPhysicalDeviceVulkan14Features BuildEnabledFeatures14(const PhysicalDeviceCandidate &selected);

std::vector<const char *> BuildDeviceExtensionList(const PhysicalDeviceCandidate &selected);

bool TryPickPhysicalDevice(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	PhysicalDeviceCandidate *outCandidate);

} // namespace projectv::render

bool InitializeVulkanInstance(
	PlatformState *platform,
	VulkanContextState *context);

bool InitializeVulkanDevice(
	VulkanContextState *context,
	FrameState *frame,
	const projectv::render::PhysicalDeviceCandidate &selected);
