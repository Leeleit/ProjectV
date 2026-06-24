#pragma once

#include <array>
#include <atomic>
#include <cstdint>
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

	[[nodiscard]] VkDeviceSize ComputeBlasBuildScratchSize(
		uint32_t primitiveCount) const noexcept;

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
		VkAccessFlags waitAccess);

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

	bool CreateShadowMaskFallback(const VulkanContextState &context, RenderState *render);
	void ReleaseShadowMaskFallback(const VulkanContextState &context, RenderState *render) noexcept;

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

	bool InitializeShadowMaskClear(const VulkanContextState &context);

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

	RtxShadowPipeline m_rtxPipeline{};
	RtxShadowSBT m_rtxSbt{};
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
	const struct VoxelWorld &world,
	const std::vector<VkDeviceAddress> &blasDeviceAddresses,
	std::vector<uint32_t> *outChunkIndices);

bool RecordRayTracedShadowPass(
	VkCommandBuffer commandBuffer,
	RayTracedShadows *rayTracedShadows,
	VkPipelineStageFlags waitStage,
	VkAccessFlags waitAccess);

}  // namespace projectv::render
