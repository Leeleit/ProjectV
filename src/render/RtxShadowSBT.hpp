#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "core/Types.hpp"

namespace projectv::render {

class RtxShadowSBT {
  public:
	RtxShadowSBT() = default;
	~RtxShadowSBT() = default;

	RtxShadowSBT(const RtxShadowSBT &) = delete;
	RtxShadowSBT &operator=(const RtxShadowSBT &) = delete;

	bool Initialize(
		const VulkanContextState &context,
		VkPipeline rayTracingPipeline,
		uint32_t rayGenGroupIndex,
		uint32_t missGroupIndex,
		uint32_t hitGroupIndex);

	void Shutdown(const VulkanContextState &context) noexcept;

	[[nodiscard]] bool IsReady() const noexcept { return m_buffer != VK_NULL_HANDLE; }

	[[nodiscard]] const VkStridedDeviceAddressRegionKHR &GetRaygenRegion() const noexcept { return m_raygenRegion; }
	[[nodiscard]] const VkStridedDeviceAddressRegionKHR &GetMissRegion() const noexcept { return m_missRegion; }
	[[nodiscard]] const VkStridedDeviceAddressRegionKHR &GetHitRegion() const noexcept { return m_hitRegion; }
	[[nodiscard]] const VkStridedDeviceAddressRegionKHR &GetCallableRegion() const noexcept { return m_callableRegion; }

  private:
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = nullptr;
	VkDeviceAddress m_deviceAddress = 0u;
	VkDeviceSize m_bufferSize = 0u;

	VkStridedDeviceAddressRegionKHR m_raygenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	VkStridedDeviceAddressRegionKHR m_callableRegion{};
};

} // namespace projectv::render