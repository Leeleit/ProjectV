#pragma once

#include <atomic> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <cstdint>

#include "core/Types.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace projectv::render {

struct RtxGiProbeConfig {
	VkImage irradianceImage = VK_NULL_HANDLE;
	VmaAllocation irradianceAllocation = nullptr;
	VkImageView irradianceView = VK_NULL_HANDLE;
	VkSampler irradianceSampler = VK_NULL_HANDLE;
	VkImage distanceImage = VK_NULL_HANDLE;
	VmaAllocation distanceAllocation = nullptr;
	VkImageView distanceView = VK_NULL_HANDLE;
	VkImage probeDataImage = VK_NULL_HANDLE;
	VmaAllocation probeDataAllocation = nullptr;
	VkImageView probeDataView = VK_NULL_HANDLE;
	VkBuffer volumeDescBuffer = VK_NULL_HANDLE;
	VmaAllocation volumeDescAllocation = nullptr;
	void *volumeDescMappedData = nullptr;
	VkDescriptorImageInfo irradianceInfo{};
	VkDescriptorImageInfo distanceInfo{};
	VkDescriptorImageInfo probeDataInfo{};
	VkDescriptorBufferInfo volumeDescBufferInfo{};
	uint32_t probeCountAxisX = 0u;
	uint32_t probeCountAxisY = 0u;
	uint32_t probeCountAxisZ = 0u;
	uint32_t probeOctahedralSize = 0u;
	uint32_t raysPerProbe = 0u;
	uint32_t totalRaysDispatched = 0u;
	uint32_t updateDispatchCount = 0u;
	float originX = 0.0f;
	float originY = 0.0f;
	float originZ = 0.0f;
	float spacingX = 0.0f;
	float spacingY = 0.0f;
	float spacingZ = 0.0f;
	float maxRayDistance = 0.0f;
	bool enabled = false;
};

class RtxGiProbes {
  public:
	RtxGiProbes() = default;
	~RtxGiProbes() = default;

	RtxGiProbes(const RtxGiProbes &) = delete;
	RtxGiProbes &operator=(const RtxGiProbes &) = delete;

	friend struct RtxGiProbesTestAccess;

	bool Initialize(
		const VulkanContextState &context,
		VkCommandPool commandPool,
		float originX,
		float originY,
		float originZ,
		float halfExtentMeters,
		uint32_t probeCountPerAxis,
		uint32_t probeOctahedralSize,
		uint32_t raysPerProbe,
		float maxRayDistance);

	void Shutdown(const VulkanContextState &context);

	bool IsEnabled() const noexcept { return m_config.enabled; }

	const RtxGiProbeConfig &GetConfig() const noexcept { return m_config; }

	void SetVolumeOrigin(
		float originX,
		float originY,
		float originZ) noexcept;

	void SetVolumeHalfExtent(float halfExtentMeters) noexcept;

	[[nodiscard]] bool RecordUpdatePass(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		uint32_t frameIndex,
		VkBuffer chunkDescriptorBuffer,
		VkBuffer sceneLightingBuffer,
		VkBuffer chunkVoxelPayloadBuffer,
		VkBuffer materialVisualBuffer,
		VkAccelerationStructureKHR tlas,
		const FrameRenderData &renderData);

	[[nodiscard]] bool RecordUpdatePass(
		const VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		const VkAccelerationStructureKHR tlas)
	{
		const FrameRenderData dummy{};
		return RecordUpdatePass(
			commandBuffer,
			context,
			0u,
			VK_NULL_HANDLE,
			VK_NULL_HANDLE,
			VK_NULL_HANDLE,
			VK_NULL_HANDLE,
			tlas,
			dummy);
	}

	[[nodiscard]] static float SampleIrradianceTestValue(uint32_t probeIndex) noexcept;

  private:
	bool AllocateTextures(
		const VulkanContextState &context,
		uint32_t probeCountPerAxis,
		uint32_t probeOctahedralSize);

	bool AllocateBuffer(
		const VulkanContextState &context);

	void ReleaseResources(const VulkanContextState &context) noexcept;

	bool CreateComputePipeline(const VulkanContextState &context);
	void DestroyComputePipeline(const VulkanContextState &context) noexcept;

	RtxGiProbeConfig m_config{};
	std::atomic<bool> m_initialized{false};

	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptorSets{};
};

struct RtxGiProbesTestAccess {
	static RtxGiProbeConfig &Config(RtxGiProbes &probes) noexcept
	{
		return probes.m_config;
	}
};

bool IsRtxGiProbeFieldEnabled(const VulkanContextState &context) noexcept;

bool CreateRtxGiProbeResources(VulkanContextState *context, RenderState *render);
void DestroyRtxGiProbeResources(VulkanContextState *context, RenderState *render);

bool RecordRtxGiProbeUpdatePass(
	VkCommandBuffer commandBuffer,
	RtxGiProbes *probes,
	const VulkanContextState &context,
	uint32_t frameIndex,
	VkBuffer chunkDescriptorBuffer,
	VkBuffer sceneLightingBuffer,
	VkBuffer chunkVoxelPayloadBuffer,
	VkBuffer materialVisualBuffer,
	VkAccelerationStructureKHR tlas,
	const FrameRenderData &renderData);

inline bool RecordRtxGiProbeUpdatePass(
	const VkCommandBuffer commandBuffer,
	RtxGiProbes *probes,
	const VulkanContextState &context,
	const VkAccelerationStructureKHR tlas)
{
	const FrameRenderData dummy{};
	return RecordRtxGiProbeUpdatePass(
		commandBuffer,
		probes,
		context,
		0u,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		tlas,
		dummy);
}

} // namespace projectv::render
