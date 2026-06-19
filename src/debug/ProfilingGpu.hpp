#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include "debug/Profiling.hpp"
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

/// \brief `VK_EXT_debug_utils` is enabled in `VulkanBootstrap` regardless of the
///
/// \details
///  `PROJECTV_ENABLE_RENDERDOC_MARKERS` compile-time option, so the function

///  pointers are always loaded; the option only gates *whether* the labels

///  get emitted at runtime. Gated through `ScopedGpuDebugLabel` /

///  `BeginGpuDebugLabel` / `EndGpuDebugLabel` so the call sites stay short.

///  `__COUNTER__` gives a fresh identifier per macro expansion so the same

///  label can be nested without `redefinition` warnings.

#if defined(PROJECTV_ENABLE_RENDERDOC_MARKERS)
#define PV_PROFILE_GPU_LABEL(cmd, name) \
	profiling::ScopedGpuDebugLabel _pvGpuLabel##__COUNTER__(cmd, name)
#define PV_PROFILE_GPU_LABEL_COLOR(cmd, name, r, g, b, a)    \
	profiling::ScopedGpuDebugLabel _pvGpuLabel##__COUNTER__( \
		cmd,                                                 \
		name,                                                \
		(r),                                                 \
		(g),                                                 \
		(b),                                                 \
		(a))
#else
#define PV_PROFILE_GPU_LABEL(cmd, name) \
	do {                                \
		(void)(cmd);                    \
		(void)sizeof(name);             \
	} while (false)
#define PV_PROFILE_GPU_LABEL_COLOR(cmd, name, r, g, b, a) \
	do {                                                  \
		(void)(cmd);                                      \
		(void)sizeof(name);                               \
		(void)(r);                                        \
		(void)(g);                                        \
		(void)(b);                                        \
		(void)(a);                                        \
	} while (false)
#endif

#if defined(PROJECTV_ENABLE_TRACY)
#define PV_PROFILE_GPU_ZONE(contextHandle, commandBuffer, name) \
	TracyVkZone(static_cast<TracyVkCtx>(contextHandle), commandBuffer, name)
#else
#define PV_PROFILE_GPU_ZONE(contextHandle, commandBuffer, name) \
	do {                                                        \
		(void)(contextHandle);                                  \
		(void)(commandBuffer);                                  \
		(void)sizeof(name);                                     \
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

#if defined(PROJECTV_ENABLE_RENDERDOC_MARKERS)
/// \brief RAII wrapper around `vkCmdBeginDebugUtilsLabelEXT` /
///
/// \details
///  `vkCmdEndDebugUtilsLabelEXT`. Drawn by RenderDoc, Tracy, validation

///  layers, and any Vulkan tool that hooks the debug-utils extension. Use

///  through the `PV_PROFILE_GPU_LABEL` / `PV_PROFILE_GPU_LABEL_COLOR` macros

///  at hot sites in `Renderer.cpp`; the macros are no-ops when

///  `PROJECTV_ENABLE_RENDERDOC_MARKERS=0`, so call sites don't need their

///  own `#if` guards.

class ScopedGpuDebugLabel {
  public:
	explicit ScopedGpuDebugLabel(
		const VkCommandBuffer cmd,
		const char *name,
		const float r = 1.0f,
		const float g = 1.0f,
		const float b = 1.0f,
		const float a = 1.0f) noexcept
		: cmd_(cmd)
	{
		if (cmd_ == VK_NULL_HANDLE || !name || !vkCmdBeginDebugUtilsLabelEXT) {
			cmd_ = VK_NULL_HANDLE;
			return;
		}
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		label.color[0] = r;
		label.color[1] = g;
		label.color[2] = b;
		label.color[3] = a;
		vkCmdBeginDebugUtilsLabelEXT(cmd_, &label);
	}

	~ScopedGpuDebugLabel()
	{
		if (cmd_ != VK_NULL_HANDLE && vkCmdEndDebugUtilsLabelEXT) {
			vkCmdEndDebugUtilsLabelEXT(cmd_);
		}
	}

	ScopedGpuDebugLabel(const ScopedGpuDebugLabel &) = delete;
	ScopedGpuDebugLabel &operator=(const ScopedGpuDebugLabel &) = delete;
	ScopedGpuDebugLabel(ScopedGpuDebugLabel &&) = delete;
	ScopedGpuDebugLabel &operator=(ScopedGpuDebugLabel &&) = delete;

  private:
	VkCommandBuffer cmd_;
};
#endif // PROJECTV_ENABLE_RENDERDOC_MARKERS

} // namespace profiling

