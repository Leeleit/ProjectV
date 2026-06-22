#include "render/RtxShadowSBT.hpp"

#include <algorithm>
#include <vector>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"

namespace projectv::render {

RtxShadowSBT::~RtxShadowSBT()
{
}

bool RtxShadowSBT::Initialize(
	const VulkanContextState &context,
	VkPipeline rayTracingPipeline,
	uint32_t rayGenGroupIndex,
	uint32_t missGroupIndex,
	uint32_t hitGroupIndex)
{
	if (m_buffer != VK_NULL_HANDLE) {
		return true;
	}
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr || rayTracingPipeline == VK_NULL_HANDLE) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxShadowSBT.Initialize",
			"vulkan device, allocator, or pipeline is null");
		return false;
	}

	const uint32_t handleSize = context.rayTracing.shaderGroupHandleSize;
	const uint32_t handleAlignment = context.rayTracing.shaderGroupHandleAlignment;
	const uint32_t baseAlignment = context.rayTracing.shaderGroupBaseAlignment;
	if (handleSize == 0u || handleAlignment == 0u || baseAlignment == 0u) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxShadowSBT.Initialize",
			"shader group alignment values not populated");
		return false;
	}

	const uint32_t raygenStride = baseAlignment;
	const uint32_t missStride = baseAlignment;
	const uint32_t hitStride = baseAlignment;

	const VkDeviceSize raygenSize = baseAlignment;
	const VkDeviceSize missSize = baseAlignment;
	const VkDeviceSize hitSize = baseAlignment;
	const VkDeviceSize totalSize = raygenSize + missSize + hitSize;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = totalSize;
	bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
					 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
					  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	if (vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS) {
		runtime::LogVkFailure("RtxShadowSBT.vmaCreateBuffer", VK_ERROR_INITIALIZATION_FAILED);
		return false;
	}
	m_bufferSize = totalSize;

	VmaAllocationInfo mappedInfo{};
	vmaGetAllocationInfo(context.allocator, m_allocation, &mappedInfo);
	void *mappedData = mappedInfo.pMappedData;
	std::memset(mappedData, 0, static_cast<size_t>(totalSize));

	const uint32_t groupCount = hitGroupIndex + 1u;
	std::vector<uint8_t> handles(static_cast<size_t>(groupCount) * handleSize, 0u);
	if (vkGetRayTracingShaderGroupHandlesKHR(
			context.device,
			rayTracingPipeline,
			0u,
			groupCount,
			handles.size(),
			handles.data()) != VK_SUCCESS) {
		runtime::LogVkFailure("RtxShadowSBT.vkGetRayTracingShaderGroupHandlesKHR", VK_ERROR_INITIALIZATION_FAILED);
		Shutdown(context);
		return false;
	}

	auto writeHandle = [&](uint8_t *base, VkDeviceSize regionStart, uint32_t groupIndex, uint32_t stride) {
		uint8_t *dst = base + regionStart;
		const uint8_t *src = handles.data() + static_cast<size_t>(groupIndex) * handleSize;
		std::memcpy(dst, src, handleSize);
		(void)stride;
	};

	writeHandle(static_cast<uint8_t *>(mappedData), 0u, rayGenGroupIndex, raygenStride);
	writeHandle(static_cast<uint8_t *>(mappedData), raygenSize, missGroupIndex, missStride);
	writeHandle(static_cast<uint8_t *>(mappedData), raygenSize + missSize, hitGroupIndex, hitStride);

	vmaFlushAllocation(context.allocator, m_allocation, 0u, totalSize);

	VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_buffer };
	m_deviceAddress = vkGetBufferDeviceAddress(context.device, &addressInfo);
	if (m_deviceAddress == 0u) {
		runtime::LogVkFailure("RtxShadowSBT.vkGetBufferDeviceAddress", VK_ERROR_INITIALIZATION_FAILED);
		Shutdown(context);
		return false;
	}

	m_raygenRegion = VkStridedDeviceAddressRegionKHR{ m_deviceAddress, raygenStride, raygenSize };
	m_missRegion = VkStridedDeviceAddressRegionKHR{ m_deviceAddress + raygenSize, missStride, missSize };
	m_hitRegion = VkStridedDeviceAddressRegionKHR{
		m_deviceAddress + raygenSize + missSize, hitStride, hitSize };
	m_callableRegion = VkStridedDeviceAddressRegionKHR{ 0u, 0u, 0u };

	SDL_Log(
		"Render: RtxShadowSBT: ready (size=%llu bytes, rgen=%llu miss=%llu hit=%llu)",
		static_cast<unsigned long long>(totalSize),
		static_cast<unsigned long long>(raygenSize),
		static_cast<unsigned long long>(missSize),
		static_cast<unsigned long long>(hitSize));
	return true;
}

void RtxShadowSBT::Shutdown(const VulkanContextState &context) noexcept
{
	if (m_buffer != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyBuffer(context.allocator, m_buffer, m_allocation);
	}
	m_buffer = VK_NULL_HANDLE;
	m_allocation = nullptr;
	m_deviceAddress = 0u;
	m_bufferSize = 0u;
	m_raygenRegion = {};
	m_missRegion = {};
	m_hitRegion = {};
	m_callableRegion = {};
}

}  // namespace projectv::render