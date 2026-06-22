#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "core/Types.hpp"

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

	void RecordDebugReport() const noexcept;

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

	RayTracedShadowConfig m_config{};
	std::vector<DirtyChunkRebuild> m_pendingDirtyChunks;
	std::mutex m_dirtyQueueMutex;
	bool m_hostBuildSupported = false;
	std::atomic<bool> m_initialized{false};
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

bool RecordRayTracedShadowPass(
	VkCommandBuffer commandBuffer,
	RayTracedShadows *rayTracedShadows,
	VkPipelineStageFlags waitStage,
	VkAccessFlags waitAccess);

}  // namespace projectv::render
