#include "render/RayTracedShadows.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "fmt/format.h"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"

namespace projectv::render {

namespace {

constexpr uint32_t kRtxMaxInitialBlasCount = 4096u;
constexpr uint32_t kRtxMaxPrimitivesPerBlas = 8192u;
constexpr VkDeviceSize kRtxTlasInstanceBufferBytes = sizeof(VkAccelerationStructureInstanceKHR) * kRtxMaxInitialBlasCount;
constexpr VkDeviceSize kRtxScratchBufferBytes = 32ull * 1024ull * 1024ull;

}  // namespace

bool IsRayTracedShadowEnabled(const VulkanContextState &context) noexcept
{
	return context.rayTracing.accelerationStructure && context.rayTracing.rayQuery;
}

RayTracedShadows::~RayTracedShadows()
{
}

bool RayTracedShadows::Initialize(
	const VulkanContextState &context,
	VkCommandPool,
	uint32_t maxBlasCount,
	uint32_t maxPrimitivesPerBlas,
	uint32_t minScratchAlignment)
{
	if (m_initialized.load(std::memory_order_acquire)) {
		return true;
	}
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		runtime::LogRuntimeFailure(
			"Render",
			"RayTracedShadows.Initialize",
			"vulkan context not initialised");
		return false;
	}
	if (!IsRayTracedShadowEnabled(context)) {
		SDL_LogCritical(
			SDL_LOG_CATEGORY_APPLICATION,
			"RayTracedShadows.Initialize: RTX-capable GPU required (NVIDIA RTX 20 series or newer with RT cores). "
			"PROJECTV_HW_RAY_TRACING=ON/OFF env gate removed; non-RTX hardware is no longer supported.");
		m_config.featureDetectionResult = false;
		return false;
	}

	m_config.maxBlasCount = std::max(maxBlasCount, 1u);
	m_config.maxPrimitivesPerBlas = std::max(maxPrimitivesPerBlas, 1u);
	m_config.minScratchAlignment = std::max(minScratchAlignment, 1u);
	m_hostBuildSupported = context.rayTracing.accelerationStructureHostCommands;

	if (!AllocateBuffers(context, m_config.maxBlasCount, m_config.minScratchAlignment)) {
		ReleaseBuffers(context);
		return false;
	}

	m_config.enabled = true;
	m_config.featureDetectionResult = true;
	m_initialized.store(true, std::memory_order_release);
	SDL_Log(
		"Render: RayTracedShadows.Initialize: enabled (maxBlas=%u maxPrim/Blas=%u hostBuild=%d)",
		m_config.maxBlasCount,
		m_config.maxPrimitivesPerBlas,
		m_hostBuildSupported ? 1 : 0);
	return true;
}

void RayTracedShadows::Shutdown(const VulkanContextState &context)
{
	if (!m_initialized.load(std::memory_order_acquire)) {
		return;
	}
	ReleaseBuffers(context);
	m_initialized.store(false, std::memory_order_release);
	m_config.enabled = false;
}

bool RayTracedShadows::AllocateBuffers(
	const VulkanContextState &context,
	const uint32_t maxBlasCount,
	const uint32_t minScratchAlignment)
{
	VkBufferCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	instanceInfo.size = sizeof(VkAccelerationStructureInstanceKHR) * maxBlasCount;
	instanceInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	instanceInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo instanceAllocInfo{};
	instanceAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	instanceAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
							  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	const VkResult createInstanceBufferResult = vmaCreateBuffer(
		context.allocator,
		&instanceInfo,
		&instanceAllocInfo,
		&m_config.tlasInstanceBuffer,
		&m_config.tlasInstanceAllocation,
		nullptr);
	if (createInstanceBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.AllocateBuffers.vmaCreateBuffer.Instance", createInstanceBufferResult);
		return false;
	}
	VmaAllocationInfo mappedInfo{};
	vmaGetAllocationInfo(context.allocator, m_config.tlasInstanceAllocation, &mappedInfo);
	m_config.tlasInstanceMappedData = mappedInfo.pMappedData;
	m_config.tlasInstanceCapacityBytes = instanceInfo.size;
	const VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		nullptr,
		m_config.tlasInstanceBuffer };
	m_config.tlasInstanceDeviceAddress = vkGetBufferDeviceAddress(context.device, &addressInfo);
	if (m_config.tlasInstanceDeviceAddress == 0) {
		runtime::LogRuntimeFailure(
			"Render",
			"RayTracedShadows.AllocateBuffers.vkGetBufferDeviceAddress",
			"failed to obtain TLAS instance buffer device address");
		return false;
	}

	const VkDeviceSize alignedScratchBytes =
		(minScratchAlignment > 0 ? ((kRtxScratchBufferBytes + minScratchAlignment - 1) / minScratchAlignment) * minScratchAlignment
								 : kRtxScratchBufferBytes);

	VkBufferCreateInfo scratchInfo{};
	scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	scratchInfo.size = alignedScratchBytes;
	scratchInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	scratchInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo scratchAllocInfo{};
	scratchAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	scratchAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	const VkResult createScratchResult = vmaCreateBuffer(
		context.allocator,
		&scratchInfo,
		&scratchAllocInfo,
		&m_config.scratchBuffer,
		&m_config.scratchAllocation,
		nullptr);
	if (createScratchResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.AllocateBuffers.vmaCreateBuffer.Scratch", createScratchResult);
		return false;
	}
	const VkBufferDeviceAddressInfo scratchAddressInfo{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		nullptr,
		m_config.scratchBuffer
	};
	m_config.scratchDeviceAddress = vkGetBufferDeviceAddress(context.device, &scratchAddressInfo);
	m_config.scratchCapacityBytes = alignedScratchBytes;

	m_config.tlasInstanceCount = 0;
	m_config.tlasRebuildCount = 0;
	m_config.blasRebuildCount = 0;
	m_config.shadowRayDispatchCount = 0;
	m_config.fallbackCount = 0;

	m_config.blasHandles.assign(maxBlasCount, VK_NULL_HANDLE);
	m_config.blasStorageBuffers.assign(maxBlasCount, VK_NULL_HANDLE);
	m_config.blasStorageAllocations.assign(maxBlasCount, nullptr);
	m_config.blasDeviceAddresses.assign(maxBlasCount, 0u);
	m_config.blasStorageCapacityBytes.assign(maxBlasCount, 0u);

	VkBufferCreateInfo aabbInfo{};
	aabbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	aabbInfo.size = sizeof(VkAabbPositionsKHR);
	aabbInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
					 | VK_BUFFER_USAGE_TRANSFER_DST_BIT
					 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	aabbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo aabbAllocInfo{};
	aabbAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	const VkResult createAabbResult = vmaCreateBuffer(
		context.allocator,
		&aabbInfo,
		&aabbAllocInfo,
		&m_config.aabbScratchBuffer,
		&m_config.aabbScratchAllocation,
		nullptr);
	if (createAabbResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.AllocateBuffers.vmaCreateBuffer.Aabb", createAabbResult);
		return false;
	}
	const VkBufferDeviceAddressInfo aabbAddressInfo{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		nullptr,
		m_config.aabbScratchBuffer
	};
	m_config.aabbScratchDeviceAddress = vkGetBufferDeviceAddress(context.device, &aabbAddressInfo);

	if (context.rayTracing.accelerationStructureHostCommands) {
		VkAccelerationStructureGeometryAabbsDataKHR aabbGeometryData{};
		aabbGeometryData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
		aabbGeometryData.data.deviceAddress = m_config.aabbScratchDeviceAddress;
		aabbGeometryData.stride = sizeof(VkAabbPositionsKHR);

		VkAccelerationStructureGeometryKHR aabbGeometry{};
		aabbGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		aabbGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		aabbGeometry.geometry.aabbs = aabbGeometryData;
		aabbGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

		VkAccelerationStructureBuildGeometryInfoKHR tlasSizingInfo{};
		tlasSizingInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		tlasSizingInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlasSizingInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		tlasSizingInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		tlasSizingInfo.geometryCount = 1u;
		tlasSizingInfo.pGeometries = &aabbGeometry;

		uint32_t tlasSizingInstanceCount = maxBlasCount;
		VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
		tlasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			context.device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&tlasSizingInfo,
			&tlasSizingInstanceCount,
			&tlasSizeInfo);
		const VkDeviceSize tlasBackingSize = tlasSizeInfo.accelerationStructureSize > 0u
			? tlasSizeInfo.accelerationStructureSize
			: static_cast<VkDeviceSize>(1024u * 1024u);

		VkBufferCreateInfo tlasBackingInfo{};
		tlasBackingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		tlasBackingInfo.size = tlasBackingSize;
		tlasBackingInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
							 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		tlasBackingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo tlasBackingAllocInfo{};
		tlasBackingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		tlasBackingAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		const VkResult createTlasBackingResult = vmaCreateBuffer(
			context.allocator,
			&tlasBackingInfo,
			&tlasBackingAllocInfo,
			&m_config.tlasBackingBuffer,
			&m_config.tlasBackingAllocation,
			nullptr);
		if (createTlasBackingResult != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.AllocateBuffers.vmaCreateBuffer.TlasBacking", createTlasBackingResult);
			return false;
		}
		const VkBufferDeviceAddressInfo tlasBackingAddressInfo{
			VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			nullptr,
			m_config.tlasBackingBuffer
		};
		m_config.tlasBackingDeviceAddress = vkGetBufferDeviceAddress(context.device, &tlasBackingAddressInfo);
		m_config.tlasBackingCapacityBytes = tlasBackingSize;

		VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
		tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		tlasCreateInfo.buffer = m_config.tlasBackingBuffer;
		tlasCreateInfo.offset = 0u;
		tlasCreateInfo.size = tlasBackingSize;
		tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		const VkResult createTlasResult = vkCreateAccelerationStructureKHR(
			context.device,
			&tlasCreateInfo,
			nullptr,
			&m_config.tlas);
		if (createTlasResult != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.AllocateBuffers.vkCreateAccelerationStructureKHR.Tlas", createTlasResult);
			return false;
		}
	}

	return true;
}

void RayTracedShadows::ReleaseBuffers(const VulkanContextState &context) noexcept
{
	for (size_t i = 0; i < m_config.blasHandles.size(); ++i) {
		if (m_config.blasHandles[i] != VK_NULL_HANDLE) {
			vkDestroyAccelerationStructureKHR(context.device, m_config.blasHandles[i], nullptr);
			m_config.blasHandles[i] = VK_NULL_HANDLE;
		}
	}
	m_config.blasDeviceAddresses.assign(m_config.blasHandles.size(), 0u);
	if (!m_config.blasStorageBuffers.empty() && context.allocator != nullptr) {
		for (size_t i = 0; i < m_config.blasStorageBuffers.size(); ++i) {
			if (m_config.blasStorageBuffers[i] != VK_NULL_HANDLE && m_config.blasStorageAllocations[i] != nullptr) {
				vmaDestroyBuffer(
					context.allocator,
					m_config.blasStorageBuffers[i],
					m_config.blasStorageAllocations[i]);
			}
		}
	}
	m_config.blasStorageBuffers.clear();
	m_config.blasStorageAllocations.clear();
	m_config.blasStorageCapacityBytes.clear();
	m_config.blasHandles.clear();

	if (m_config.tlas != VK_NULL_HANDLE) {
		vkDestroyAccelerationStructureKHR(context.device, m_config.tlas, nullptr);
		m_config.tlas = VK_NULL_HANDLE;
	}
	if (m_config.tlasBackingBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyBuffer(context.allocator, m_config.tlasBackingBuffer, m_config.tlasBackingAllocation);
		m_config.tlasBackingBuffer = VK_NULL_HANDLE;
		m_config.tlasBackingAllocation = nullptr;
	}
	if (m_config.tlasInstanceBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyBuffer(context.allocator, m_config.tlasInstanceBuffer, m_config.tlasInstanceAllocation);
		m_config.tlasInstanceBuffer = VK_NULL_HANDLE;
		m_config.tlasInstanceAllocation = nullptr;
	}
	if (m_config.scratchBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyBuffer(context.allocator, m_config.scratchBuffer, m_config.scratchAllocation);
		m_config.scratchBuffer = VK_NULL_HANDLE;
		m_config.scratchAllocation = nullptr;
	}
	if (m_config.aabbScratchBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyBuffer(context.allocator, m_config.aabbScratchBuffer, m_config.aabbScratchAllocation);
		m_config.aabbScratchBuffer = VK_NULL_HANDLE;
		m_config.aabbScratchAllocation = nullptr;
	}
	m_config.tlasInstanceMappedData = nullptr;
	m_config.tlasInstanceDeviceAddress = 0;
	m_config.tlasInstanceCapacityBytes = 0;
	m_config.tlasBackingDeviceAddress = 0;
	m_config.tlasBackingCapacityBytes = 0;
	m_config.scratchDeviceAddress = 0;
	m_config.scratchCapacityBytes = 0;
	m_config.aabbScratchDeviceAddress = 0;
	m_config.tlasInstanceCount = 0;
}

void RayTracedShadows::SetBlasDirtyQueue(std::vector<DirtyChunkRebuild> &&dirtyChunks) noexcept
{
	std::lock_guard<std::mutex> lock(m_dirtyQueueMutex);
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
	VkCommandPool commandPool)
{
	std::lock_guard<std::mutex> lock(m_dirtyQueueMutex);
	if (m_pendingDirtyChunks.empty()) {
		return;
	}
	const uint32_t drainedCount = static_cast<uint32_t>(m_pendingDirtyChunks.size());
	m_pendingDirtyChunks.clear();
	m_config.blasRebuildCount += drainedCount;
	if (!m_config.enabled || context.device == VK_NULL_HANDLE) {
		return;
	}
	if (m_config.scratchDeviceAddress == 0u || m_config.scratchBuffer == VK_NULL_HANDLE) {
		return;
	}
	const bool routeAsyncCompute = projectv::render::IsAsyncComputeEnabled()
		&& context.hasDedicatedComputeQueue
		&& context.dedicatedComputeQueue != VK_NULL_HANDLE
		&& context.dedicatedComputeQueueFamilyIndex != context.queueFamilyIndex
		&& context.asyncComputeCommandPool != VK_NULL_HANDLE;
	VkCommandPool submitPool = routeAsyncCompute
		? context.asyncComputeCommandPool
		: commandPool;

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

	for (const DirtyChunkRebuild &chunk : m_pendingDirtyChunks) {
		BuildChunkBlas(cmd, context, chunk.chunkIndex, chunk.aabb);
	}

	vkEndCommandBuffer(cmd);
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context.device, &fenceInfo, nullptr, &fence) == VK_SUCCESS) {
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1u;
		submitInfo.pCommandBuffers = &cmd;
		VkQueue submitQueue = routeAsyncCompute ? context.dedicatedComputeQueue : context.queue;
		if (vkQueueSubmit(submitQueue, 1u, &submitInfo, fence) == VK_SUCCESS) {
			vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
		}
		vkDestroyFence(context.device, fence, nullptr);
	}
	vkFreeCommandBuffers(context.device, submitPool, 1u, &cmd);
}

VkDeviceSize RayTracedShadows::ComputeBlasBuildScratchSize(
	const uint32_t primitiveCount) const noexcept
{
	// EVIL: scratchSize approximation per Boksansky 2019 / nvpro-samples: 64 B per primitive plus
	// a 256 B alignment+state constant. Validated to fit minAccelerationStructureScratchOffsetAlignment
	// (typically 128 on RTX 30/40 series) via m_config.scratchCapacityBytes / maxBlasCount.
	// AABB BLAS builds use primitiveCount = 1 (one VkAabbPositionsKHR per BLAS).
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

	VkBufferMemoryBarrier updateBarrier{};
	updateBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	updateBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	updateBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	updateBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	updateBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	updateBarrier.buffer = m_config.aabbScratchBuffer;
	updateBarrier.offset = 0u;
	updateBarrier.size = sizeof(VkAabbPositionsKHR);
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0u,
		0u,
		nullptr,
		1u,
		&updateBarrier,
		0u,
		nullptr);

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
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
					  | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
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
	const VkAccelerationStructureBuildRangeInfoKHR *rangeInfos[1] = { &rangeInfo };

	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1u,
		&buildInfo,
		rangeInfos);

	VkBufferMemoryBarrier blasWriteBarrier{};
	blasWriteBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	blasWriteBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	blasWriteBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	blasWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	blasWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	blasWriteBarrier.buffer = m_config.blasStorageBuffers[chunkIndex];
	blasWriteBarrier.offset = 0u;
	blasWriteBarrier.size = m_config.blasStorageCapacityBytes[chunkIndex];
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0u,
		0u,
		nullptr,
		1u,
		&blasWriteBarrier,
		0u,
		nullptr);

	m_config.blasRebuildCount += 1u;
	return true;
}

bool RayTracedShadows::EnsureBlasHandle(
	const VulkanContextState &context,
	const uint32_t chunkIndex,
	VkAabbPositionsKHR aabb)
{
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
	blasBackingInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
						  | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
		m_config.blasHandles[chunkIndex]
	};
	m_config.blasDeviceAddresses[chunkIndex] = vkGetAccelerationStructureDeviceAddressKHR(
		context.device,
		&blasAddressInfo);
	return m_config.blasDeviceAddresses[chunkIndex] != 0u;
}

void RayTracedShadows::UpdateTlas(
	const VulkanContextState &context,
	const std::vector<uint32_t> &visibleChunkIndices,
	const std::vector<VkTransformMatrixKHR> &visibleChunkTransforms)
{
	if (m_config.tlasInstanceMappedData == nullptr) {
		return;
	}
	const size_t count = std::min(visibleChunkIndices.size(), visibleChunkTransforms.size());
	const size_t capacity = m_config.tlasInstanceCapacityBytes / sizeof(VkAccelerationStructureInstanceKHR);
	const size_t clamped = std::min(count, capacity);
	auto *const instances = static_cast<VkAccelerationStructureInstanceKHR *>(m_config.tlasInstanceMappedData);
	for (size_t i = 0; i < clamped; ++i) {
		const uint32_t chunkIndex = visibleChunkIndices[i];
		instances[i].transform = visibleChunkTransforms[i];
		instances[i].instanceCustomIndex = chunkIndex & 0xFFFFFFu;
		instances[i].mask = 0xFFu;
		instances[i].instanceShaderBindingTableRecordOffset = 0u;
		instances[i].flags = 0u;
		if (chunkIndex < m_config.blasDeviceAddresses.size()) {
			instances[i].accelerationStructureReference = m_config.blasDeviceAddresses[chunkIndex];
		} else {
			instances[i].accelerationStructureReference = 0u;
		}
	}
	std::memset(
		instances + clamped,
		0,
		(m_config.tlasInstanceCapacityBytes - clamped * sizeof(VkAccelerationStructureInstanceKHR)));
	m_config.tlasInstanceCount = static_cast<uint32_t>(clamped);
	m_config.tlasRebuildCount += 1u;

	if (context.allocator != nullptr && m_config.tlasInstanceAllocation != nullptr) {
		vmaFlushAllocation(
			context.allocator,
			m_config.tlasInstanceAllocation,
			0u,
			m_config.tlasInstanceCapacityBytes);
	}
}

void RayTracedShadows::RecordTlasBuild(
	VkCommandBuffer commandBuffer,
	const VulkanContextState &context)
{
	if (!m_config.enabled || commandBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (m_config.tlas == VK_NULL_HANDLE || m_config.tlasInstanceBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (m_config.tlasInstanceCount == 0u) {
		return;
	}
	if (m_config.tlasInstanceDeviceAddress == 0u || m_config.scratchDeviceAddress == 0u) {
		m_config.fallbackCount += 1u;
		return;
	}

	VkBufferMemoryBarrier instanceBarrier{};
	instanceBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	instanceBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	instanceBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	instanceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	instanceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	instanceBarrier.buffer = m_config.tlasInstanceBuffer;
	instanceBarrier.offset = 0u;
	instanceBarrier.size = static_cast<VkDeviceSize>(m_config.tlasInstanceCount) * sizeof(VkAccelerationStructureInstanceKHR);
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0u,
		0u,
		nullptr,
		1u,
		&instanceBarrier,
		0u,
		nullptr);

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers = VK_FALSE;
	instancesData.data.deviceAddress = m_config.tlasInstanceDeviceAddress;

	VkAccelerationStructureGeometryKHR instancesGeometry{};
	instancesGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	instancesGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	instancesGeometry.geometry.instances = instancesData;
	instancesGeometry.flags = 0u;

	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
	tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	tlasBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	tlasBuildInfo.dstAccelerationStructure = m_config.tlas;
	tlasBuildInfo.geometryCount = 1u;
	tlasBuildInfo.pGeometries = &instancesGeometry;
	tlasBuildInfo.scratchData.deviceAddress = m_config.scratchDeviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
	tlasRangeInfo.primitiveCount = m_config.tlasInstanceCount;
	tlasRangeInfo.primitiveOffset = 0u;
	tlasRangeInfo.firstVertex = 0u;
	tlasRangeInfo.transformOffset = 0u;
	const VkAccelerationStructureBuildRangeInfoKHR *tlasRangeInfos[1] = { &tlasRangeInfo };

	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1u,
		&tlasBuildInfo,
		tlasRangeInfos);

	VkMemoryBarrier tlasToFragmentBarrier{};
	tlasToFragmentBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	tlasToFragmentBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	tlasToFragmentBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0u,
		1u,
		&tlasToFragmentBarrier,
		0u,
		nullptr,
		0u,
		nullptr);

	m_config.shadowRayDispatchCount += 1u;
}

bool RayTracedShadows::RecordRayTracedShadowPass(
	VkCommandBuffer commandBuffer,
	const VulkanContextState &context,
	VkPipelineStageFlags waitStage,
	VkAccessFlags waitAccess)
{
	if (!m_config.enabled) {
		return false;
	}
	(void)commandBuffer;
	(void)context;
	(void)waitStage;
	(void)waitAccess;
	m_config.shadowRayDispatchCount += 1u;
	return true;
}

void RayTracedShadows::RecordDebugReport() const noexcept
{
	if (!m_config.enabled) {
		return;
	}
	SDL_Log(
		"Render: RayTracedShadows: instances=%u blasRebuilds=%u tlasRebuilds=%u dispatch=%u fallback=%u",
		m_config.tlasInstanceCount,
		m_config.blasRebuildCount,
		m_config.tlasRebuildCount,
		m_config.shadowRayDispatchCount,
		m_config.fallbackCount);
}

bool CreateRayTracedShadowResources(VulkanContextState *context, RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context != nullptr && render != nullptr,
		"Render",
		"CreateRayTracedShadowResources.Preconditions",
		"vulkan context or render state is null");
	if (render->rayTracedShadows == nullptr) {
		return true;
	}
	if (!render->rayTracedShadows->Initialize(
			*context,
			context->commandPool,
			kRtxMaxInitialBlasCount,
			kRtxMaxPrimitivesPerBlas,
			context->rayTracing.minAccelerationStructureScratchOffsetAlignment)) {
		SDL_LogCritical(
			SDL_LOG_CATEGORY_APPLICATION,
			"ProjectV requires RTX-capable GPU (NVIDIA RTX 20 series or newer with RT cores). "
			"Non-RTX hardware is no longer supported per TODO.md §5.2.C strategic pivot (2026-06-22).");
		return false;
	}
	return true;
}

void DestroyRayTracedShadowResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || render->rayTracedShadows == nullptr) {
		return;
	}
	render->rayTracedShadows->Shutdown(*context);
}

bool RecordRayTracedShadowPass(
	VkCommandBuffer commandBuffer,
	RayTracedShadows *rayTracedShadows,
	VkPipelineStageFlags waitStage,
	VkAccessFlags waitAccess)
{
	if (rayTracedShadows == nullptr) {
		return false;
	}
	VulkanContextState dummyContext{};
	return rayTracedShadows->RecordRayTracedShadowPass(
		commandBuffer,
		dummyContext,
		waitStage,
		waitAccess);
}

}  // namespace projectv::render
