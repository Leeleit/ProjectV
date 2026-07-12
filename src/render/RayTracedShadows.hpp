#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <atomic>
#include <mutex>
#include <vector>

#include "core/Types.hpp"
#include "render/RtxShadowPipeline.hpp"
#include "render/RtxShadowSBT.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace projectv::render {

struct RayTracedShadowConfig {
	VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
	VkAccelerationStructureKHR blasTemplate = VK_NULL_HANDLE;
	VkBuffer tlasInstanceBuffer = VK_NULL_HANDLE;
	VmaAllocation tlasInstanceAllocation = nullptr;
	void *tlasInstanceMappedData = nullptr;
	VkDeviceAddress tlasInstanceDeviceAddress = 0;
	VkDeviceSize tlasInstanceCapacityBytes = 0;
	VkBuffer tlasBackingBuffer = VK_NULL_HANDLE;
	VmaAllocation tlasBackingAllocation = nullptr;
	VkDeviceAddress tlasBackingDeviceAddress = 0;
	VkDeviceSize tlasBackingCapacityBytes = 0;
	VkBuffer scratchBuffer = VK_NULL_HANDLE;
	VmaAllocation scratchAllocation = nullptr;
	VkDeviceAddress scratchDeviceAddress = 0;
	VkDeviceSize scratchCapacityBytes = 0;
	VkBuffer aabbScratchBuffer = VK_NULL_HANDLE;
	VmaAllocation aabbScratchAllocation = nullptr;
	VkDeviceAddress aabbScratchDeviceAddress = 0;
	std::vector<VkAccelerationStructureKHR> blasHandles;
	std::vector<VkBuffer> blasStorageBuffers;
	std::vector<VmaAllocation> blasStorageAllocations;
	std::vector<VkDeviceAddress> blasDeviceAddresses;
	std::vector<VkDeviceSize> blasStorageCapacityBytes;
	uint32_t tlasInstanceCount = 0;
	uint32_t tlasRebuildCount = 0;
	uint32_t blasRebuildCount = 0;
	uint32_t shadowRayDispatchCount = 0;
	uint32_t fallbackCount = 0;
	uint32_t maxBlasCount = 0;
	uint32_t maxPrimitivesPerBlas = 0;
	uint32_t minScratchAlignment = 1u;
	bool enabled = false;
	bool featureDetectionResult = false;
};

struct DirtyChunkRebuild {
	uint32_t chunkIndex = 0;
	VkAabbPositionsKHR aabb{};
};

class RayTracedShadows {
  public:
	RayTracedShadows() = default;
	~RayTracedShadows();

	RayTracedShadows(const RayTracedShadows &) = delete;
	RayTracedShadows &operator=(const RayTracedShadows &) = delete;

	friend struct RayTracedShadowTestAccess;

	bool Initialize(
		const VulkanContextState &context,
		VkCommandPool commandPool,
		uint32_t maxBlasCount,
		uint32_t maxPrimitivesPerBlas,
		uint32_t minScratchAlignment);

	void Shutdown(
		const VulkanContextState &context);

	bool IsEnabled() const noexcept { return m_config.enabled; }

	const RayTracedShadowConfig &GetConfig() const noexcept { return m_config; }

	[[nodiscard]] bool IsHostBuildSupported() const noexcept { return m_hostBuildSupported; }

	void SetBlasDirtyQueue(std::vector<DirtyChunkRebuild> &&dirtyChunks) noexcept;

	void BuildDirtyBlases(
		const VulkanContextState &context,
		VkCommandPool commandPool);

	[[nodiscard]] static VkDeviceSize ComputeBlasBuildScratchSize(
		uint32_t primitiveCount) noexcept;

	[[nodiscard]] bool BuildChunkBlas(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		uint32_t chunkIndex,
		VkAabbPositionsKHR aabb);

	void UpdateTlas(
		const VulkanContextState &context,
		const std::vector<uint32_t> &visibleChunkIndices,
		const std::vector<VkTransformMatrixKHR> &visibleChunkTransforms);

	void RecordTlasBuild(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context);

	[[nodiscard]] bool RecordRayTracedShadowPass(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		VkPipelineStageFlags waitStage,
		VkAccessFlags waitAccess) const;

	bool RecordVoxelAwareRtxShadowPass(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		uint32_t frameIndex,
		VkBuffer chunkDescriptorBuffer,
		VkBuffer sceneLightingBuffer,
		VkBuffer chunkVoxelPayloadBuffer,
		const float *inverseViewProjection,
		const float *cameraPosition,
		const float *cameraForward,
		uint32_t screenWidth,
		uint32_t screenHeight);

	void RecordDebugReport() const noexcept;

	[[nodiscard]] VkImageView GetShadowMaskImageView() const noexcept { return m_shadowMaskImageView; }
	[[nodiscard]] bool IsVoxelAwareRtxActive() const noexcept { return m_voxelAwareRtxActive; }
	[[nodiscard]] VkAccelerationStructureKHR GetTlas() const noexcept { return m_config.tlas; }

	static bool CreateShadowMaskFallback(const VulkanContextState &context, RenderState *render)
	{
		if (render == nullptr) {
			return false;
		}
		if (render->rtxShadowMaskFallbackView != VK_NULL_HANDLE) {
			return true;
		}
		if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
			return false;
		}

		VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8_UNORM;
		imageInfo.extent = {1u, 1u, 1u};
		imageInfo.mipLevels = 1u;
		imageInfo.arrayLayers = 1u;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (vmaCreateImage(
				context.allocator,
				&imageInfo,
				&allocInfo,
				&render->rtxShadowMaskFallbackImage,
				&render->rtxShadowMaskFallbackAllocation,
				nullptr) != VK_SUCCESS) {
			return false;
		}
		VmaAllocationInfo mappedInfo{};
		vmaGetAllocationInfo(context.allocator, render->rtxShadowMaskFallbackAllocation, &mappedInfo);
		render->rtxShadowMaskFallbackMemory = mappedInfo.deviceMemory;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = render->rtxShadowMaskFallbackImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8_UNORM;
		viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		if (vkCreateImageView(context.device, &viewInfo, nullptr, &render->rtxShadowMaskFallbackView) != VK_SUCCESS) {
			vmaDestroyImage(context.allocator, render->rtxShadowMaskFallbackImage, render->rtxShadowMaskFallbackAllocation);
			render->rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
			render->rtxShadowMaskFallbackAllocation = nullptr;
			render->rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
			return false;
		}
		return true;
	}

	static void ReleaseShadowMaskFallback(const VulkanContextState &context, RenderState *render) noexcept
	{
		if (render == nullptr) {
			return;
		}
		if (render->rtxShadowMaskFallbackView != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE) {
			vkDestroyImageView(context.device, render->rtxShadowMaskFallbackView, nullptr);
		}
		render->rtxShadowMaskFallbackView = VK_NULL_HANDLE;
		if (render->rtxShadowMaskFallbackImage != VK_NULL_HANDLE && context.allocator != nullptr) {
			vmaDestroyImage(context.allocator, render->rtxShadowMaskFallbackImage, render->rtxShadowMaskFallbackAllocation);
		}
		render->rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
		render->rtxShadowMaskFallbackAllocation = nullptr;
		render->rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
	}

	bool RecreateShadowMaskForExtent(const VulkanContextState &context, uint32_t width, uint32_t height);

  private:
	bool AllocateBuffers(
		const VulkanContextState &context,
		uint32_t maxBlasCount,
		uint32_t minScratchAlignment);

	void ReleaseBuffers(const VulkanContextState &context) noexcept;

	bool EnsureBlasHandle(
		const VulkanContextState &context,
		uint32_t chunkIndex,
		VkAabbPositionsKHR aabb);

	bool CreateVoxelAwareRtxResources(const VulkanContextState &context);
	void ReleaseVoxelAwareRtxResources(const VulkanContextState &context) noexcept;

	bool InitializeShadowMaskClear(const VulkanContextState &context) const;

	struct RtxFrameResources {
		VkBuffer cameraUboBuffer = VK_NULL_HANDLE;
		VmaAllocation cameraUboAllocation = nullptr;
		void *cameraUboMappedData = nullptr;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	RayTracedShadowConfig m_config{};
	std::vector<DirtyChunkRebuild> m_pendingDirtyChunks;
	std::mutex m_dirtyQueueMutex;
	bool m_hostBuildSupported = false;
	std::atomic<bool> m_initialized{false};

	RtxShadowPipeline m_rtxPipeline;
	RtxShadowSBT m_rtxSbt;
	VkDescriptorPool m_rtxDescriptorPool = VK_NULL_HANDLE;
	VkImage m_shadowMaskImage = VK_NULL_HANDLE;
	VkImageView m_shadowMaskImageView = VK_NULL_HANDLE;
	VmaAllocation m_shadowMaskAllocation = nullptr;
	uint32_t m_shadowMaskWidth = 0u;
	uint32_t m_shadowMaskHeight = 0u;
	VkFormat m_shadowMaskFormat = VK_FORMAT_R8_UNORM;
	std::array<RtxFrameResources, MAX_FRAMES_IN_FLIGHT> m_rtxFrames{};
	bool m_voxelAwareRtxActive = false;
};

struct RayTracedShadowTestAccess {
	static RayTracedShadowConfig &Config(RayTracedShadows &shadows) noexcept
	{
		return shadows.m_config;
	}
};

bool IsRayTracedShadowEnabled(const VulkanContextState &context) noexcept;

bool CreateRayTracedShadowResources(VulkanContextState *context, RenderState *render);
void DestroyRayTracedShadowResources(VulkanContextState *context, RenderState *render);

bool CreateRtxShadowMaskFallbackOnly(VulkanContextState *context, RenderState *render);
void DestroyRtxShadowMaskFallbackOnly(VulkanContextState *context, RenderState *render) noexcept;

void CollectNonBuiltBlasChunksForRayTracing(
	const VoxelWorld &world,
	const std::vector<VkDeviceAddress> &blasDeviceAddresses,
	std::vector<uint32_t> *outChunkIndices);

bool RecordRayTracedShadowPass(
	VkCommandBuffer commandBuffer,
	RayTracedShadows *rayTracedShadows,
	VkPipelineStageFlags waitStage,
	VkAccessFlags waitAccess);

} // namespace projectv::render
