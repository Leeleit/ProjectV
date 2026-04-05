#ifndef PROFILING_GPU_HPP
#define PROFILING_GPU_HPP

// ReSharper disable once CppUnusedIncludeDirective
#include "Profiling.hpp"
#include "volk.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cstdint>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring>

#if defined(PROJECTV_ENABLE_TRACY)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <tracy/TracyVulkan.hpp>
#pragma clang diagnostic pop
#endif

#if defined(PROJECTV_ENABLE_TRACY)
#define PV_PROFILE_GPU_ZONE(contextHandle, commandBuffer, name) \
	TracyVkZone(static_cast<TracyVkCtx>(contextHandle), commandBuffer, name)
#else
#define PV_PROFILE_GPU_ZONE(contextHandle, commandBuffer, name) \
	do { \
		(void)(contextHandle); \
		(void)(commandBuffer); \
		(void)sizeof(name); \
	} while (false)
#endif

namespace profiling {

inline void *CreateVulkanGpuContext(
	const VkPhysicalDevice physicalDevice,
	const VkDevice device,
	const VkQueue queue,
	const VkCommandBuffer commandBuffer)
{
#if defined(PROJECTV_ENABLE_TRACY)
	return TracyVkContext(physicalDevice, device, queue, commandBuffer);
#else
	(void)physicalDevice;
	(void)device;
	(void)queue;
	(void)commandBuffer;
	return nullptr;
#endif
}

inline void *CreateVulkanGpuContextCalibrated(
	const VkPhysicalDevice physicalDevice,
	const VkDevice device,
	const VkQueue queue,
	const VkCommandBuffer commandBuffer,
	const PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT getPhysicalDeviceCalibrateableTimeDomains,
	const PFN_vkGetCalibratedTimestampsEXT getCalibratedTimestamps)
{
#if defined(PROJECTV_ENABLE_TRACY)
	return TracyVkContextCalibrated(
		physicalDevice,
		device,
		queue,
		commandBuffer,
		getPhysicalDeviceCalibrateableTimeDomains,
		getCalibratedTimestamps);
#else
	(void)physicalDevice;
	(void)device;
	(void)queue;
	(void)commandBuffer;
	(void)getPhysicalDeviceCalibrateableTimeDomains;
	(void)getCalibratedTimestamps;
	return nullptr;
#endif
}

inline void NameVulkanGpuContext(
	void *contextHandle,
	const char *name)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!contextHandle || !name) {
		return;
	}

	TracyVkContextName(
		static_cast<TracyVkCtx>(contextHandle),
		name,
		static_cast<uint16_t>(std::strlen(name)));
#else
	(void)contextHandle;
	(void)name;
#endif
}

inline void CollectVulkanGpu(
	void *contextHandle,
	const VkCommandBuffer commandBuffer)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!contextHandle || commandBuffer == VK_NULL_HANDLE) {
		return;
	}

	TracyVkCollect(static_cast<TracyVkCtx>(contextHandle), commandBuffer);
#else
	(void)contextHandle;
	(void)commandBuffer;
#endif
}

inline void CollectVulkanGpuHost(void *contextHandle)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!contextHandle) {
		return;
	}

	TracyVkCollectHost(static_cast<TracyVkCtx>(contextHandle));
#else
	(void)contextHandle;
#endif
}

inline void DestroyVulkanGpuContext(void *contextHandle)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!contextHandle) {
		return;
	}

	TracyVkDestroy(static_cast<TracyVkCtx>(contextHandle));
#else
	(void)contextHandle;
#endif
}

} // namespace profiling

#endif
