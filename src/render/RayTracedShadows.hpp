#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
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

struct alignas(16) RtxTraversalCounters {
	uint32_t primaryAabbCandidates = 0u;
	uint32_t primaryDdaSteps = 0u;
	uint32_t sunAabbCandidates = 0u;
	uint32_t sunDdaSteps = 0u;
};

static_assert(std::is_standard_layout_v<RtxTraversalCounters>);
static_assert(sizeof(RtxTraversalCounters) == 16u);

[[nodiscard]] std::optional<VkAabbPositionsKHR> TryBuildTightChunkAabb(
	const PackedSceneChunkAabb &packedAabb) noexcept;

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

	// Record pending BLAS builds into the frame CB (no mid-frame submit/fence). Returns builds recorded.
	[[nodiscard]] uint32_t RecordDirtyBlasBuilds(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		uint32_t maxBuilds);

	void MarkTlasInstancesDirty() noexcept { m_tlasInstancesDirty = true; }

	[[nodiscard]] static VkDeviceSize ComputeBlasBuildScratchSize(
		uint32_t primitiveCount) noexcept;

	[[nodiscard]] bool BuildChunkBlas(
		VkCommandBuffer commandBuffer,
		const VulkanContextState &context,
		uint32_t chunkIndex,
		VkAabbPositionsKHR aabb);

	// Returns true when instance buffer changed and TLAS must be rebuilt/refit this frame.
	[[nodiscard]] bool UpdateTlas(
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
		void *tracyGraphicsContext,
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

	void SyncTraversalCountersAfterFence(
		const VulkanContextState &context,
		uint32_t frameIndex);

	void RecordDebugReport() const noexcept;

	[[nodiscard]] VkImageView GetShadowMaskImageView() const noexcept { return m_shadowMaskImageView; }
	[[nodiscard]] bool IsVoxelAwareRtxActive() const noexcept { return m_voxelAwareRtxActive; }
	[[nodiscard]] bool IsVoxelAwareRtxPending() const noexcept { return m_voxelAwareRtxPending; }
	[[nodiscard]] bool IsTraversalMetricsEnabled() const noexcept { return m_traversalMetricsEnabled; }
	[[nodiscard]] RtxTraversalCounters GetLastTraversalCounters() const noexcept { return m_lastTraversalCounters; }
	[[nodiscard]] VkAccelerationStructureKHR GetTlas() const noexcept { return m_config.tlas; }

	// Poll once per frame until SBT/mask ready. Do not run deferred-ops on a worker while submitting (knowledge §37).
	bool TryFinishVoxelAwareRtxResources(const VulkanContextState &context);

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

	[[nodiscard]] uint32_t GetShadowMaskWidth() const noexcept { return m_shadowMaskWidth; }
	[[nodiscard]] uint32_t GetShadowMaskHeight() const noexcept { return m_shadowMaskHeight; }

	// PROJECTV_RTX_SHADOW_MASK_SCALE (default 1.0). Full extent in → mask extent out.
	[[nodiscard]] static VkExtent2D ResolveShadowMaskExtent(VkExtent2D fullExtent);

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
		VkBuffer traversalCounterBuffer = VK_NULL_HANDLE;
		VmaAllocation traversalCounterAllocation = nullptr;
		void *traversalCounterMappedData = nullptr;
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
	uint32_t m_previousTlasInstanceCount = 0u;
	bool m_tlasInstancesDirty = true;
	std::vector<uint32_t> m_cachedVisibleChunkIndices;
	std::vector<VkDeviceAddress> m_cachedVisibleBlasAddresses;
	VkImage m_shadowMaskImage = VK_NULL_HANDLE;
	VkImageView m_shadowMaskImageView = VK_NULL_HANDLE;
	VmaAllocation m_shadowMaskAllocation = nullptr;
	uint32_t m_shadowMaskWidth = 0u;
	uint32_t m_shadowMaskHeight = 0u;
	VkFormat m_shadowMaskFormat = VK_FORMAT_R8_UNORM;
	std::array<RtxFrameResources, MAX_FRAMES_IN_FLIGHT> m_rtxFrames{};
	std::array<bool, MAX_FRAMES_IN_FLIGHT> m_traversalCountersWritten{};
	RtxTraversalCounters m_lastTraversalCounters{};
	bool m_traversalMetricsEnabled = false;
	bool m_voxelAwareRtxActive = false;
	bool m_voxelAwareRtxPending = false; // true after Initialize until Finish succeeds or permanently fails
	bool m_voxelAwareRtxFailed = false;
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
