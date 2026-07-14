#include "volk.h"
#include "render/RayTracedShadows.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>

namespace projectv::render {

void RayTracedShadows::SetBlasDirtyQueue(std::vector<DirtyChunkRebuild> &&dirtyChunks) noexcept // NOLINT(bugprone-exception-escape): mutex/vector operations are bounded
{
	std::lock_guard lock(m_dirtyQueueMutex);
	if (dirtyChunks.empty()) {
		return;
	}
	m_pendingDirtyChunks.insert(
		m_pendingDirtyChunks.end(),
		std::make_move_iterator(dirtyChunks.begin()),
		std::make_move_iterator(dirtyChunks.end()));
}

void RayTracedShadows::BuildDirtyBlases(
	const VulkanContextState &context,
	const VkCommandPool commandPool)
{
	std::lock_guard lock(m_dirtyQueueMutex);
	if (m_pendingDirtyChunks.empty()) {
		return;
	}
	const uint32_t drainedCount = static_cast<uint32_t>(m_pendingDirtyChunks.size());
	m_config.blasRebuildCount += drainedCount;
	if (!m_config.enabled || context.device == VK_NULL_HANDLE) {
		m_pendingDirtyChunks.clear();
		return;
	}
	if (m_config.scratchDeviceAddress == 0u || m_config.scratchBuffer == VK_NULL_HANDLE) {
		m_pendingDirtyChunks.clear();
		return;
	}

	const VkCommandPool submitPool = commandPool;

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = submitPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1u;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(context.device, &allocInfo, &cmd) != VK_SUCCESS) {
		return;
	}
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	for (const auto &[chunkIndex, aabb] : m_pendingDirtyChunks) {
		if (!BuildChunkBlas(cmd, context, chunkIndex, aabb)) {
			runtime::LogRuntimeFailure("RayTracedShadows", "BuildDirtyBlas", "BuildChunkBlas failed");
		}
	}

	m_pendingDirtyChunks.clear();

	vkEndCommandBuffer(cmd);
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context.device, &fenceInfo, nullptr, &fence) == VK_SUCCESS) {
		VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
		commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		commandBufferSubmitInfo.commandBuffer = cmd;
		VkSubmitInfo2 submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submitInfo.commandBufferInfoCount = 1u;
		submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
		const VkQueue submitQueue = context.queue;
		if (vkQueueSubmit2(submitQueue, 1u, &submitInfo, fence) == VK_SUCCESS) {
			vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
		}
		vkDestroyFence(context.device, fence, nullptr);
	}
	vkFreeCommandBuffers(context.device, submitPool, 1u, &cmd);
}

VkDeviceSize RayTracedShadows::ComputeBlasBuildScratchSize(
	const uint32_t primitiveCount) noexcept
{
	if (primitiveCount == 0u) {
		return 0u;
	}
	constexpr VkDeviceSize kPerPrimitiveBytes = 64u;
	constexpr VkDeviceSize kAlignmentOverhead = 256u;
	return static_cast<VkDeviceSize>(primitiveCount) * kPerPrimitiveBytes + kAlignmentOverhead;
}

bool RayTracedShadows::BuildChunkBlas(
	VkCommandBuffer commandBuffer,
	const VulkanContextState &context,
	const uint32_t chunkIndex,
	VkAabbPositionsKHR aabb)
{
	if (!m_config.enabled || commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (m_config.aabbScratchBuffer == VK_NULL_HANDLE || m_config.aabbScratchDeviceAddress == 0u) {
		m_config.fallbackCount += 1u;
		return false;
	}
	if (chunkIndex >= m_config.maxBlasCount) {
		m_config.fallbackCount += 1u;
		return false;
	}
	if (aabb.maxX < aabb.minX || aabb.maxY < aabb.minY || aabb.maxZ < aabb.minZ) {
		m_config.fallbackCount += 1u;
		return false;
	}
	const VkDeviceSize requiredScratch = ComputeBlasBuildScratchSize(1u);
	if (requiredScratch > m_config.scratchCapacityBytes) {
		m_config.fallbackCount += 1u;
		return false;
	}

	if (!EnsureBlasHandle(context, chunkIndex, aabb)) {
		m_config.fallbackCount += 1u;
		return false;
	}

	vkCmdUpdateBuffer(commandBuffer, m_config.aabbScratchBuffer, 0u, sizeof(VkAabbPositionsKHR), &aabb);

	VkBufferMemoryBarrier2 updateBarrier{};
	updateBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	updateBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	updateBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	updateBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	updateBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	updateBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	updateBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	updateBarrier.buffer = m_config.aabbScratchBuffer;
	updateBarrier.offset = 0u;
	updateBarrier.size = sizeof(VkAabbPositionsKHR);
	VkDependencyInfo updateDep{};
	updateDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	updateDep.bufferMemoryBarrierCount = 1u;
	updateDep.pBufferMemoryBarriers = &updateBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &updateDep);

	VkAccelerationStructureGeometryAabbsDataKHR aabbData{};
	aabbData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
	aabbData.data.deviceAddress = m_config.aabbScratchDeviceAddress;
	aabbData.stride = sizeof(VkAabbPositionsKHR);

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
	geometry.geometry.aabbs = aabbData;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1u;
	buildInfo.pGeometries = &geometry;
	buildInfo.scratchData.deviceAddress = m_config.scratchDeviceAddress;
	buildInfo.dstAccelerationStructure = m_config.blasHandles[chunkIndex];

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = 1u;
	rangeInfo.primitiveOffset = 0u;
	rangeInfo.firstVertex = 0u;
	rangeInfo.transformOffset = 0u;
	const VkAccelerationStructureBuildRangeInfoKHR *rangeInfos[1] = {&rangeInfo};

	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1u,
		&buildInfo,
		rangeInfos);

	VkBufferMemoryBarrier2 blasWriteBarrier{};
	blasWriteBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	blasWriteBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	blasWriteBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	blasWriteBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	blasWriteBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	blasWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	blasWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	blasWriteBarrier.buffer = m_config.blasStorageBuffers[chunkIndex];
	blasWriteBarrier.offset = 0u;
	blasWriteBarrier.size = m_config.blasStorageCapacityBytes[chunkIndex];

	VkBufferMemoryBarrier2 scratchBarrier{};
	scratchBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	scratchBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	scratchBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	scratchBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	scratchBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	scratchBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scratchBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scratchBarrier.buffer = m_config.scratchBuffer;
	scratchBarrier.offset = 0u;
	scratchBarrier.size = m_config.scratchCapacityBytes;

	std::array buildBarriers = {blasWriteBarrier, scratchBarrier};
	VkDependencyInfo buildDep{};
	buildDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	buildDep.bufferMemoryBarrierCount = static_cast<uint32_t>(buildBarriers.size());
	buildDep.pBufferMemoryBarriers = buildBarriers.data();
	vkCmdPipelineBarrier2(commandBuffer, &buildDep);

	VkBufferMemoryBarrier2 aabbScratchBarrier{};
	aabbScratchBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	aabbScratchBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	aabbScratchBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	aabbScratchBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	aabbScratchBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	aabbScratchBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	aabbScratchBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	aabbScratchBarrier.buffer = m_config.aabbScratchBuffer;
	aabbScratchBarrier.offset = 0u;
	aabbScratchBarrier.size = sizeof(VkAabbPositionsKHR);
	VkDependencyInfo aabbDep{};
	aabbDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	aabbDep.bufferMemoryBarrierCount = 1u;
	aabbDep.pBufferMemoryBarriers = &aabbScratchBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &aabbDep);

	m_config.blasRebuildCount += 1u;
	return true;
}

bool RayTracedShadows::EnsureBlasHandle(
	const VulkanContextState &context,
	const uint32_t chunkIndex,
	VkAabbPositionsKHR aabb)
{
	(void)aabb;
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		return false;
	}
	if (chunkIndex >= m_config.blasHandles.size()) {
		return false;
	}
	if (m_config.blasHandles[chunkIndex] != VK_NULL_HANDLE) {
		return true;
	}
	VkAccelerationStructureGeometryAabbsDataKHR aabbGeometryData{};
	aabbGeometryData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
	aabbGeometryData.data.deviceAddress = m_config.aabbScratchDeviceAddress;
	aabbGeometryData.stride = sizeof(VkAabbPositionsKHR);

	VkAccelerationStructureGeometryKHR aabbGeometry{};
	aabbGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	aabbGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
	aabbGeometry.geometry.aabbs = aabbGeometryData;
	aabbGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildGeometryInfoKHR blasSizingInfo{};
	blasSizingInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	blasSizingInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	blasSizingInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	blasSizingInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	blasSizingInfo.geometryCount = 1u;
	blasSizingInfo.pGeometries = &aabbGeometry;

	uint32_t blasSizingPrimitiveCount = 1u;
	VkAccelerationStructureBuildSizesInfoKHR blasSizeInfo{};
	blasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasSizingInfo,
		&blasSizingPrimitiveCount,
		&blasSizeInfo);
	const VkDeviceSize blasBackingSize = blasSizeInfo.accelerationStructureSize > 0u
											 ? blasSizeInfo.accelerationStructureSize
											 : static_cast<VkDeviceSize>(1024u);

	VkBufferCreateInfo blasBackingInfo{};
	blasBackingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	blasBackingInfo.size = blasBackingSize;
	blasBackingInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	blasBackingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo blasBackingAllocInfo{};
	blasBackingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	const VkResult createBlasBackingResult = vmaCreateBuffer(
		context.allocator,
		&blasBackingInfo,
		&blasBackingAllocInfo,
		&m_config.blasStorageBuffers[chunkIndex],
		&m_config.blasStorageAllocations[chunkIndex],
		nullptr);
	if (createBlasBackingResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"RayTracedShadows.EnsureBlasHandle.vmaCreateBuffer.BlasBacking",
			createBlasBackingResult);
		return false;
	}
	m_config.blasStorageCapacityBytes[chunkIndex] = blasBackingSize;

	VkAccelerationStructureCreateInfoKHR blasCreateInfo{};
	blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	blasCreateInfo.buffer = m_config.blasStorageBuffers[chunkIndex];
	blasCreateInfo.offset = 0u;
	blasCreateInfo.size = blasBackingSize;
	blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	const VkResult createBlasResult = vkCreateAccelerationStructureKHR(
		context.device,
		&blasCreateInfo,
		nullptr,
		&m_config.blasHandles[chunkIndex]);
	if (createBlasResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"RayTracedShadows.EnsureBlasHandle.vkCreateAccelerationStructureKHR",
			createBlasResult);
		return false;
	}
	const VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		nullptr,
		m_config.blasHandles[chunkIndex]};
	m_config.blasDeviceAddresses[chunkIndex] = vkGetAccelerationStructureDeviceAddressKHR(
		context.device,
		&blasAddressInfo);
	return m_config.blasDeviceAddresses[chunkIndex] != 0u;
}

void CollectNonBuiltBlasChunksForRayTracing(
	const VoxelWorld &world,
	const std::vector<VkDeviceAddress> &blasDeviceAddresses,
	std::vector<uint32_t> *outChunkIndices)
{
	if (outChunkIndices == nullptr) {
		return;
	}
	const size_t addressCount = blasDeviceAddresses.size();
	for (size_t i = 0; i < world.chunks.size(); ++i) {
		const VoxelChunk &chunk = world.chunks[i];
		if (chunk.nonAirVoxelCount == 0u) {
			continue;
		}
		const bool hasBlas =
			i < addressCount && blasDeviceAddresses[i] != 0u;
		if (hasBlas) {
			continue;
		}
		outChunkIndices->push_back(static_cast<uint32_t>(i));
	}
}

} // namespace projectv::render
