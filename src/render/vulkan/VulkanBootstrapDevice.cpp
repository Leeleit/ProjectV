#include "volk.h" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/vulkan/VulkanBootstrapInternal.hpp"
#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include "fmt/format.h"

#include <vulkan/vulkan.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

bool CreatePipelineCache(VulkanContextState *context)
{
	const std::filesystem::path cachePath = std::filesystem::current_path() / ".cache" / "vulkan_pipeline_cache.bin";
	std::vector<char> cacheData;
	if (std::filesystem::exists(cachePath)) {
		std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
		if (file.is_open()) {
			const auto size = file.tellg();
			file.seekg(0, std::ios::beg);
			cacheData.resize(static_cast<size_t>(size));
			file.read(cacheData.data(), size);
		}
	}

	VkPipelineCacheCreateInfo cacheInfo{};
	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (!cacheData.empty()) {
		cacheInfo.initialDataSize = cacheData.size();
		cacheInfo.pInitialData = cacheData.data();
	}
	vkCreatePipelineCache(context->device, &cacheInfo, nullptr, &context->pipelineCache);
	return true;
}

} // namespace

bool InitializeVulkanDevice(
	VulkanContextState *context,
	FrameState *frame,
	const projectv::render::PhysicalDeviceCandidate &selected)
{
	PV_CHECK_OR_RETURN(
		context && frame && context->physicalDevice != VK_NULL_HANDLE,
		"Init",
		"InitializeVulkanDevice.Preconditions",
		"context/frame/physicalDevice is null");

	float queuePriority = 1.0f; // Vulkan max queue priority for predictable scheduling.
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = context->queueFamilyIndex;
	queueInfo.queueCount = 1; // single-queue per family avoids priority inversion; re-evaluate for async compute.
	queueInfo.pQueuePriorities = &queuePriority;

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.push_back(queueInfo);

	if (selected.supportsDedicatedComputeQueue) {
		context->dedicatedComputeQueueFamilyIndex = selected.dedicatedComputeQueueFamilyIndex;
		VkDeviceQueueCreateInfo computeQueueInfo{};
		computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		computeQueueInfo.queueFamilyIndex = selected.dedicatedComputeQueueFamilyIndex;
		computeQueueInfo.queueCount = 1; // mirrors graphics queue policy; may need 2+ for async compute overlap.
		computeQueueInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(computeQueueInfo);
	} else {
		context->dedicatedComputeQueueFamilyIndex = UINT32_MAX; // sentinel for "no dedicated compute queue".
	}

	const std::vector<const char *> deviceExtensions = projectv::render::BuildDeviceExtensionList(selected);

	VkPhysicalDeviceFeatures enabledFeatures = projectv::render::BuildEnabledFeatures(selected);
	VkPhysicalDeviceVulkan12Features enabledFeatures12 = projectv::render::BuildEnabledFeatures12(selected);
	VkPhysicalDeviceVulkan13Features enabledFeatures13 = projectv::render::BuildEnabledFeatures13(selected);
	VkPhysicalDeviceVulkan14Features enabledFeatures14 = projectv::render::BuildEnabledFeatures14(selected);

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT enabledDynamicRenderingUnusedAttachmentsFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT, nullptr};
	if (selected.supportsDynamicRenderingUnusedAttachments) {
		enabledDynamicRenderingUnusedAttachmentsFeatures.dynamicRenderingUnusedAttachments = VK_TRUE;
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT enabledMeshShaderFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT, nullptr};
	const bool meshShaderEnabled = projectv::core::GetEnvVar("PROJECTV_MESH_SHADER_PIPELINE") != nullptr && selected.supportsMeshShader;
	if (meshShaderEnabled) {
		enabledMeshShaderFeatures.meshShader = VK_TRUE;
	}

	VkPhysicalDevicePresentIdFeaturesKHR enabledPresentIdFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, nullptr};
	VkPhysicalDevicePresentWaitFeaturesKHR enabledPresentWaitFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, nullptr};
	enabledPresentIdFeatures.presentId = selected.supportsPresentId ? VK_TRUE : VK_FALSE;
	enabledPresentWaitFeatures.presentWait = selected.supportsPresentWait ? VK_TRUE : VK_FALSE;

	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, nullptr};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr};
	const bool rtxEnabled = selected.supportsAccelerationStructure && selected.supportsRayQuery;
	const char *const serEnv = projectv::core::GetEnvVar("PROJECTV_RTX_SER");
	const bool serEnabled =
		rtxEnabled &&
		selected.rayTracingSupport.shaderInvocationReorder &&
		serEnv != nullptr &&
		serEnv[0] != '\0' &&
		serEnv[0] != '0';
	VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV enabledShaderInvocationReorderFeatures{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV, nullptr};
	if (rtxEnabled) {
		enabledRayQueryFeatures.rayQuery = VK_TRUE;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		if (selected.rayTracingSupport.accelerationStructureHostCommands) {
			enabledAccelerationStructureFeatures.accelerationStructureHostCommands = VK_TRUE;
		}
		if (selected.supportsRayTracingPipeline) {
			enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
		}
	}
	enabledShaderInvocationReorderFeatures.rayTracingInvocationReorder = serEnabled ? VK_TRUE : VK_FALSE;

	enabledFeatures13.pNext = &enabledFeatures14;
	enabledFeatures14.pNext = &enabledFeatures12;
	enabledFeatures12.pNext = &enabledDynamicRenderingUnusedAttachmentsFeatures;
	enabledDynamicRenderingUnusedAttachmentsFeatures.pNext = &enabledMeshShaderFeatures;
	enabledMeshShaderFeatures.pNext = &enabledAccelerationStructureFeatures;
	if (selected.supportsRayTracingPipeline) {
		enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;
		enabledRayTracingPipelineFeatures.pNext = &enabledRayQueryFeatures;
	} else {
		enabledAccelerationStructureFeatures.pNext = &enabledRayQueryFeatures;
	}
	enabledRayQueryFeatures.pNext = &enabledPresentIdFeatures;
	enabledPresentIdFeatures.pNext = &enabledPresentWaitFeatures;
	enabledPresentWaitFeatures.pNext = serEnabled ? &enabledShaderInvocationReorderFeatures : nullptr;
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enabledFeatures13;
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	const VkResult createDeviceResult =
		vkCreateDevice(context->physicalDevice, &deviceCreateInfo, nullptr, &context->device);
	if (createDeviceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanDevice.vkCreateDevice", createDeviceResult);
		return false;
	}

	volkLoadDevice(context->device);
	context->rayTracingInvocationReorderEnabled = serEnabled;
	if (serEnabled) {
		SDL_Log("Render: SER available and enabled (PROJECTV_RTX_SER=%s)", serEnv);
	}
	if (enabledFeatures14.dynamicRenderingLocalRead == VK_TRUE) {
		SDL_Log("Init: dynamicRenderingLocalRead enabled; PostFX conversion is deferred because it is compute-based");
	}
	context->features14 = enabledFeatures14;
	context->maintenance5 = enabledFeatures14.maintenance5 == VK_TRUE;
	context->maintenance6 = enabledFeatures14.maintenance6 == VK_TRUE;
	context->hostImageCopy = enabledFeatures14.hostImageCopy == VK_TRUE;
	context->indexTypeUint8 = enabledFeatures14.indexTypeUint8 == VK_TRUE;
	context->bindless =
		enabledFeatures12.descriptorIndexing == VK_TRUE &&
		enabledFeatures12.runtimeDescriptorArray == VK_TRUE &&
		enabledFeatures12.descriptorBindingPartiallyBound == VK_TRUE &&
		enabledFeatures12.descriptorBindingVariableDescriptorCount == VK_TRUE &&
		enabledFeatures12.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;

	CreatePipelineCache(context);

	vkGetDeviceQueue(context->device, context->queueFamilyIndex, 0, &context->queue);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->queue),
		VK_OBJECT_TYPE_QUEUE,
		"GraphicsPresentQueue");

	if (context->hasDedicatedComputeQueue) {
		vkGetDeviceQueue(
			context->device,
			context->dedicatedComputeQueueFamilyIndex,
			0,
			&context->dedicatedComputeQueue);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(context->dedicatedComputeQueue),
			VK_OBJECT_TYPE_QUEUE,
			"DedicatedComputeQueue");
	}

	VkSemaphoreTypeCreateInfo timelineTypeInfo{};
	timelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineTypeInfo.initialValue = 0;
	VkSemaphoreCreateInfo timelineInfo{};
	timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	timelineInfo.pNext = &timelineTypeInfo;
	const VkResult timelineResult = vkCreateSemaphore(
		context->device,
		&timelineInfo,
		nullptr,
		&context->renderTimelineSemaphore);
	if (timelineResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanDevice.vkCreateSemaphore",
			timelineResult);
		return false;
	}
	context->renderTimelineValue = 0;
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->renderTimelineSemaphore),
		VK_OBJECT_TYPE_SEMAPHORE,
		"RenderTimelineSemaphore");

	VmaAllocatorCreateInfo allocInfo{};
	allocInfo.physicalDevice = context->physicalDevice;
	allocInfo.device = context->device;
	allocInfo.instance = context->instance;
	allocInfo.vulkanApiVersion = projectv::render::GetMinVulkanApiVersion();
	if (selected.supportsAccelerationStructure && selected.supportsRayQuery && selected.rayTracingSupport.bufferDeviceAddress) {
		allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	}

	VmaVulkanFunctions vulkanFunctions{};
	const VkResult importFunctionsResult = vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
	if (importFunctionsResult != VK_SUCCESS) {
		runtime::LogVmaFailure(
			"InitializeVulkanDevice.vmaImportVulkanFunctionsFromVolk",
			importFunctionsResult);
		return false;
	}
	allocInfo.pVulkanFunctions = &vulkanFunctions;

	const VkResult createAllocatorResult = vmaCreateAllocator(&allocInfo, &context->allocator);
	if (createAllocatorResult != VK_SUCCESS) {
		runtime::LogVmaFailure("InitializeVulkanDevice.vmaCreateAllocator", createAllocatorResult);
		return false;
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = context->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	const VkResult createCommandPoolResult =
		vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->commandPool);
	if (createCommandPoolResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanDevice.vkCreateCommandPool", createCommandPoolResult);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->commandPool),
		VK_OBJECT_TYPE_COMMAND_POOL,
		"MainCommandPool");

	frame->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo cmdAllocInfo{};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = context->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	const VkResult allocateCommandBuffersResult =
		vkAllocateCommandBuffers(context->device, &cmdAllocInfo, frame->commandBuffers.data());
	if (allocateCommandBuffersResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanDevice.vkAllocateCommandBuffers", allocateCommandBuffersResult);
		return false;
	}
	for (size_t i = 0; i < frame->commandBuffers.size(); ++i) {
		char name[64]{};
		std::snprintf(name, sizeof(name), "FrameCommandBuffer[%zu]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->commandBuffers[i]),
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			name);
	}

	frame->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		const VkResult imageAvailableSemaphoreResult =
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->imageAvailableSemaphores[i]);
		if (imageAvailableSemaphoreResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"InitializeVulkanDevice.vkCreateSemaphore(imageAvailable)",
				imageAvailableSemaphoreResult);
			return false;
		}
		const VkResult renderFinishedSemaphoreResult =
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->renderFinishedSemaphores[i]);
		if (renderFinishedSemaphoreResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"InitializeVulkanDevice.vkCreateSemaphore(renderFinished)",
				renderFinishedSemaphoreResult);
			return false;
		}
		const VkResult inFlightFenceResult =
			vkCreateFence(context->device, &fenceInfo, nullptr, &frame->inFlightFences[i]);
		if (inFlightFenceResult != VK_SUCCESS) {
			runtime::LogVkFailure("InitializeVulkanDevice.vkCreateFence", inFlightFenceResult);
			return false;
		}

		char imageAvailableName[64]{};
		std::snprintf(imageAvailableName, sizeof(imageAvailableName), "ImageAvailableSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->imageAvailableSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			imageAvailableName);

		char renderFinishedName[64]{};
		std::snprintf(renderFinishedName, sizeof(renderFinishedName), "RenderFinishedSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->renderFinishedSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			renderFinishedName);

		char fenceName[64]{};
		std::snprintf(fenceName, sizeof(fenceName), "InFlightFence[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->inFlightFences[i]),
			VK_OBJECT_TYPE_FENCE,
			fenceName);
	}

	return true;
}

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame)
{
	PV_CHECK_OR_RETURN(
		platform && context && frame,
		"Init",
		"InitializeVulkanBase.Preconditions",
		"platform/context/frame is null");

	if (!InitializeVulkanInstance(platform, context)) {
		return false;
	}

	uint32_t deviceCount = 0;
	const VkResult enumeratePhysicalDevicesCountResult =
		vkEnumeratePhysicalDevices(context->instance, &deviceCount, nullptr);
	if (enumeratePhysicalDevicesCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkEnumeratePhysicalDevices(count)",
			enumeratePhysicalDevicesCountResult);
		return false;
	}
	if (deviceCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitializeVulkanBase.PhysicalDevices",
			"no physical devices found");
		return false;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	const VkResult enumeratePhysicalDevicesDataResult =
		vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices.data());
	if (enumeratePhysicalDevicesDataResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkEnumeratePhysicalDevices(data)",
			enumeratePhysicalDevicesDataResult);
		return false;
	}

	projectv::render::PhysicalDeviceCandidate selected{};
	for (VkPhysicalDevice physicalDevice : devices) {
		projectv::render::PhysicalDeviceCandidate candidate{};
		if (projectv::render::TryPickPhysicalDevice(physicalDevice, context->surface, &candidate)) {
			selected = candidate;
			break;
		}
	}

	if (selected.device == VK_NULL_HANDLE) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitializeVulkanBase.PhysicalDeviceSelection",
			"no suitable physical device found");
		return false;
	}

	context->physicalDevice = selected.device;
	context->queueFamilyIndex = selected.queueFamilyIndex;
	context->supportsDynamicRenderingUnusedAttachments = selected.supportsDynamicRenderingUnusedAttachments;
	context->supportsPresentId = selected.supportsPresentId;
	context->supportsPresentWait = selected.supportsPresentWait;
	context->hasDedicatedComputeQueue = selected.supportsDedicatedComputeQueue;
	context->features14 = selected.features14;
	context->maintenance5 = selected.features14.maintenance5 == VK_TRUE;
	context->maintenance6 = selected.features14.maintenance6 == VK_TRUE;
	context->rayTracing = selected.rayTracingSupport;

	return InitializeVulkanDevice(context, frame, selected);
}
