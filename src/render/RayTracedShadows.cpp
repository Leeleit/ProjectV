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

	if (!CreateVoxelAwareRtxResources(context)) {
		SDL_Log("Render: RayTracedShadows: voxel-aware RT pipeline unavailable; falling back to ray-query AABB shadows");
	} else if (!InitializeShadowMaskClear(context)) {
		SDL_Log("Render: RayTracedShadows: shadow mask clear failed; first frame may sample undefined data");
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
	ReleaseVoxelAwareRtxResources(context);
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

	// EVIL: TLAS handle + backing buffer must always be created regardless of
	// accelerationStructureHostCommands support. hostCommands only affects the
	// build path (vkBuildAccelerationStructuresKHR vs vkCmdBuildAccelerationStructuresKHR);
	// the handle itself is needed for ray query dispatch via rtxTlas binding.
	// The previous gate skipped TLAS creation when hostCommands=0, leaving
	// m_config.tlas as VK_NULL_HANDLE and disabling the entire RTX shadow path.
	{
		VkAccelerationStructureGeometryAabbsDataKHR aabbGeometryData{};
		aabbGeometryData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
		aabbGeometryData.data.deviceAddress = m_config.aabbScratchDeviceAddress;
		aabbGeometryData.stride = sizeof(VkAabbPositionsKHR);

		// EVIL: TLAS sizing must use VK_GEOMETRY_TYPE_INSTANCES_KHR (top-level
		// references BLAS handles via VkAccelerationStructureInstanceKHR array).
		// The previous code used AABBS geometry which violates Vulkan spec
		// (AABBS is bottom-level only). Sizing uses the tlasInstanceBuffer's
		// device address as a placeholder; the real build in RecordTlasBuild
		// overwrites with actual instance data per frame.
		VkAccelerationStructureGeometryInstancesDataKHR instancesSizingData{};
		instancesSizingData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instancesSizingData.arrayOfPointers = VK_FALSE;
		instancesSizingData.data.deviceAddress = m_config.tlasInstanceDeviceAddress;

		VkAccelerationStructureGeometryKHR instancesGeometry{};
		instancesGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		instancesGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		instancesGeometry.geometry.instances = instancesSizingData;

		VkAccelerationStructureBuildGeometryInfoKHR tlasSizingInfo{};
		tlasSizingInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		tlasSizingInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlasSizingInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		tlasSizingInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		tlasSizingInfo.geometryCount = 1u;
		tlasSizingInfo.pGeometries = &instancesGeometry;

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
	m_config.blasRebuildCount += drainedCount;
	if (!m_config.enabled || context.device == VK_NULL_HANDLE) {
		m_pendingDirtyChunks.clear();
		return;
	}
	if (m_config.scratchDeviceAddress == 0u || m_config.scratchBuffer == VK_NULL_HANDLE) {
		m_pendingDirtyChunks.clear();
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

	m_pendingDirtyChunks.clear();

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

	static int diagCount = 0;
	if (diagCount < 3 && clamped > 0) {
		uint32_t withNonZeroAddr = 0u;
		for (size_t i = 0; i < clamped; ++i) {
			if (instances[i].accelerationStructureReference != 0u) {
				++withNonZeroAddr;
			}
		}
		SDL_Log("UpdateTlas diag: clamped=%zu tlasInstanceCount=%u withNonZeroRef=%u tlas=%p",
			clamped, m_config.tlasInstanceCount, withNonZeroAddr, (void*)m_config.tlas);
		++diagCount;
	}

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

	// EVIL: scratch buffer synchronization. BuildDirtyBlases writes the scratch
	// buffer during per-frame BLAS builds (one-shot cmd buffer, vkWaitForFences
	// blocks the CPU but the GPU side is not explicitly synced with the main cmd
	// buffer that drives the TLAS build). Without this barrier the TLAS build can
	// race on the scratch memory and produce a stale acceleration structure that
	// fails every ray query (NVIDIA dev forum: "What i was doing is create an
	// empty TLAS... i called only an UPDATE... Now i'm rebuilding the TLAS every
	// frame and eveything is working fine"). Both BLAS and TLAS builds use the
	// same scratchBuffer — barrier AS_write → AS_read on the scratch buffer to
	// serialize them.
	if (m_config.scratchBuffer != VK_NULL_HANDLE && m_config.scratchCapacityBytes > 0u) {
		VkBufferMemoryBarrier scratchBarrier{};
		scratchBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		scratchBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		scratchBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		scratchBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scratchBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scratchBarrier.buffer = m_config.scratchBuffer;
		scratchBarrier.offset = 0u;
		scratchBarrier.size = m_config.scratchCapacityBytes;
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0u,
			0u,
			nullptr,
			1u,
			&scratchBarrier,
			0u,
			nullptr);
	}

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
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
	// EVIL: ray query is dispatched inside voxel.frag.rtx.spv (graphicsPipelineRtx),
	// so a separate shadow pass is unnecessary. This method exists as a hook for
	// future fullscreen RTX post-passes (e.g. RTX refraction, multi-bounce GI) and
	// for backward-compatible callers. Returning false signals "no separate work
	// recorded this frame" — the real per-frame RTX work is the TLAS build
	// (RecordTlasBuild), which independently increments shadowRayDispatchCount.
	return false;
}

bool RayTracedShadows::RecordVoxelAwareRtxShadowPass(
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
	uint32_t screenHeight)
{
	if (!m_config.enabled || !m_voxelAwareRtxActive) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE
			|| m_rtxPipeline.GetPipeline() == VK_NULL_HANDLE
			|| !m_rtxSbt.IsReady()) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}
	const RtxFrameResources &frame = m_rtxFrames[frameIndex];
	if (frame.cameraUboMappedData == nullptr || frame.descriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (inverseViewProjection == nullptr || cameraPosition == nullptr || cameraForward == nullptr) {
		return false;
	}
	if (chunkDescriptorBuffer == VK_NULL_HANDLE
			|| sceneLightingBuffer == VK_NULL_HANDLE
			|| chunkVoxelPayloadBuffer == VK_NULL_HANDLE) {
		return false;
	}

	uint8_t *uboMapped = static_cast<uint8_t *>(frame.cameraUboMappedData);
	std::memcpy(uboMapped + 0, inverseViewProjection, 64u);
	const float positionAndWidth[4] = { cameraPosition[0], cameraPosition[1], cameraPosition[2],
		static_cast<float>(screenWidth) };
	const float forwardAndHeight[4] = { cameraForward[0], cameraForward[1], cameraForward[2],
		static_cast<float>(screenHeight) };
	std::memcpy(uboMapped + 64, positionAndWidth, 16u);
	std::memcpy(uboMapped + 80, forwardAndHeight, 16u);
	vmaFlushAllocation(context.allocator, frame.cameraUboAllocation, 0u, 96u);

	VkDescriptorBufferInfo chunkDescriptorInfo{ chunkDescriptorBuffer, 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo sceneLightingInfo{ sceneLightingBuffer, 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo chunkVoxelPayloadInfo{ chunkVoxelPayloadBuffer, 0, VK_WHOLE_SIZE };
	VkDescriptorImageInfo shadowMaskImageInfo{ VK_NULL_HANDLE, m_shadowMaskImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorBufferInfo cameraUboInfo{ frame.cameraUboBuffer, 0, 96u };
	VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{};
	tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	tlasInfo.accelerationStructureCount = 1u;
	tlasInfo.pAccelerationStructures = &m_config.tlas;

	std::array<VkWriteDescriptorSet, 6> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = frame.descriptorSet;
	writes[0].dstBinding = 1;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1u;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &chunkDescriptorInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = frame.descriptorSet;
	writes[1].dstBinding = 3;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1u;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &sceneLightingInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = frame.descriptorSet;
	writes[2].dstBinding = 4;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorCount = 1u;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &chunkVoxelPayloadInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = frame.descriptorSet;
	writes[3].dstBinding = 13;
	writes[3].dstArrayElement = 0;
	writes[3].descriptorCount = 1u;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writes[3].pNext = &tlasInfo;

	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = frame.descriptorSet;
	writes[4].dstBinding = 18;
	writes[4].dstArrayElement = 0;
	writes[4].descriptorCount = 1u;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &shadowMaskImageInfo;

	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = frame.descriptorSet;
	writes[5].dstBinding = 19;
	writes[5].dstArrayElement = 0;
	writes[5].descriptorCount = 1u;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[5].pBufferInfo = &cameraUboInfo;

	vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);

	VkImageMemoryBarrier imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.srcAccessMask = VK_ACCESS_NONE;
	imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.image = m_shadowMaskImage;
	imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0u,
		0u, nullptr,
		0u, nullptr,
		1u, &imageBarrier);

	vkCmdBindPipeline(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_rtxPipeline.GetPipeline());
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_rtxPipeline.GetPipelineLayout(),
		0u,
		1u,
		&frame.descriptorSet,
		0u,
		nullptr);

	vkCmdTraceRaysKHR(
		commandBuffer,
		&m_rtxSbt.GetRaygenRegion(),
		&m_rtxSbt.GetMissRegion(),
		&m_rtxSbt.GetHitRegion(),
		&m_rtxSbt.GetCallableRegion(),
		m_shadowMaskWidth,
		m_shadowMaskHeight,
		1u);

	VkImageMemoryBarrier readBarrier{};
	readBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	readBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	readBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	readBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	readBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	readBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	readBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	readBarrier.image = m_shadowMaskImage;
	readBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		0u, nullptr,
		0u, nullptr,
		1u, &readBarrier);

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

bool RayTracedShadows::RecreateShadowMaskForExtent(const VulkanContextState &context, uint32_t width, uint32_t height)
{
	if (!m_voxelAwareRtxActive) {
		return false;
	}
	if (width == 0u || height == 0u) {
		return false;
	}
	if (width == m_shadowMaskWidth && height == m_shadowMaskHeight
			&& m_shadowMaskImage != VK_NULL_HANDLE) {
		return true;
	}
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		return false;
	}

	if (m_shadowMaskImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, m_shadowMaskImageView, nullptr);
		m_shadowMaskImageView = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
	}

	m_shadowMaskWidth = width;
	m_shadowMaskHeight = height;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_shadowMaskFormat;
	imageInfo.extent = { width, height, 1u };
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo imageAllocInfo{};
	imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	const VkResult createImageResult = vmaCreateImage(
		context.allocator,
		&imageInfo,
		&imageAllocInfo,
		&m_shadowMaskImage,
		&m_shadowMaskAllocation,
		nullptr);
	if (createImageResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.RecreateShadowMaskForExtent.vmaCreateImage", createImageResult);
		m_shadowMaskWidth = 0u;
		m_shadowMaskHeight = 0u;
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_shadowMaskImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_shadowMaskFormat;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	if (vkCreateImageView(context.device, &viewInfo, nullptr, &m_shadowMaskImageView) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.RecreateShadowMaskForExtent.vkCreateImageView", VK_ERROR_INITIALIZATION_FAILED);
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
		m_shadowMaskWidth = 0u;
		m_shadowMaskHeight = 0u;
		return false;
	}

	if (!InitializeShadowMaskClear(context)) {
		SDL_Log("Render: RayTracedShadows: shadow mask clear failed after recreate; first frame may sample undefined data");
	}

	SDL_Log("Render: RayTracedShadows: shadow mask resized to %ux%u", width, height);
	return true;
}

bool RayTracedShadows::CreateVoxelAwareRtxResources(const VulkanContextState &context)
{
	if (!context.rayTracing.rayTracingPipeline) {
		return false;
	}
	if (m_voxelAwareRtxActive) {
		return true;
	}
	const VkDevice device = context.device;
	if (device == VK_NULL_HANDLE || context.allocator == nullptr) {
		return false;
	}

	RtxShadowPipelineConfig pipelineConfig{};
	pipelineConfig.shaderGroupHandleSize = context.rayTracing.shaderGroupHandleSize;
	pipelineConfig.shaderGroupBaseAlignment = context.rayTracing.shaderGroupBaseAlignment;
	pipelineConfig.shaderGroupHandleAlignment = context.rayTracing.shaderGroupHandleAlignment;
	if (!m_rtxPipeline.Initialize(context, pipelineConfig)) {
		SDL_Log("Render: RtxShadowPipeline.Initialize failed; voxel-aware RT shadows disabled");
		return false;
	}
	if (!m_rtxSbt.Initialize(
			context,
			m_rtxPipeline.GetPipeline(),
			m_rtxPipeline.GetRayGenGroupIndex(),
			m_rtxPipeline.GetMissGroupIndex(),
			m_rtxPipeline.GetHitGroupIndex())) {
		m_rtxSbt.Shutdown(context);
		m_rtxPipeline.Shutdown(context);
		return false;
	}

	m_shadowMaskWidth = 1920u;
	m_shadowMaskHeight = 1080u;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_shadowMaskFormat;
	imageInfo.extent = { m_shadowMaskWidth, m_shadowMaskHeight, 1u };
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo imageAllocInfo{};
	imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	const VkResult createImageResult = vmaCreateImage(
		context.allocator,
		&imageInfo,
		&imageAllocInfo,
		&m_shadowMaskImage,
		&m_shadowMaskAllocation,
		nullptr);
	if (createImageResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vmaCreateImage", createImageResult);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_shadowMaskImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_shadowMaskFormat;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	if (vkCreateImageView(device, &viewInfo, nullptr, &m_shadowMaskImageView) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkCreateImageView", VK_ERROR_INITIALIZATION_FAILED);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	std::array<VkDescriptorPoolSize, 4> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 2u * MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[3].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_rtxDescriptorPool) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkCreateDescriptorPool", VK_ERROR_INITIALIZATION_FAILED);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		RtxFrameResources &frame = m_rtxFrames[i];

		VkBufferCreateInfo uboInfo{};
		uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		uboInfo.size = 96u;
		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo uboAllocInfo{};
		uboAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		uboAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
							 | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if (vmaCreateBuffer(context.allocator, &uboInfo, &uboAllocInfo,
				&frame.cameraUboBuffer, &frame.cameraUboAllocation, nullptr) != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vmaCreateBuffer.Ubo", VK_ERROR_INITIALIZATION_FAILED);
			ReleaseVoxelAwareRtxResources(context);
			return false;
		}
		VmaAllocationInfo mappedInfo{};
		vmaGetAllocationInfo(context.allocator, frame.cameraUboAllocation, &mappedInfo);
		frame.cameraUboMappedData = mappedInfo.pMappedData;
		std::memset(frame.cameraUboMappedData, 0, 96u);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_rtxDescriptorPool;
		allocInfo.descriptorSetCount = 1u;
		allocInfo.pSetLayouts = &m_rtxPipeline.GetDescriptorSetLayout();
		if (vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet) != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkAllocateDescriptorSets", VK_ERROR_INITIALIZATION_FAILED);
			ReleaseVoxelAwareRtxResources(context);
			return false;
		}
	}

	m_voxelAwareRtxActive = true;
	SDL_Log("Render: VoxelAwareRtxShadows: ready (image=%ux%u, frames=%u)", m_shadowMaskWidth, m_shadowMaskHeight, MAX_FRAMES_IN_FLIGHT);
	return true;
}

void RayTracedShadows::ReleaseVoxelAwareRtxResources(const VulkanContextState &context) noexcept
{
	const VkDevice device = context.device;
	for (RtxFrameResources &frame : m_rtxFrames) {
		if (frame.cameraUboBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
			vmaDestroyBuffer(context.allocator, frame.cameraUboBuffer, frame.cameraUboAllocation);
		}
		frame.cameraUboBuffer = VK_NULL_HANDLE;
		frame.cameraUboAllocation = nullptr;
		frame.cameraUboMappedData = nullptr;
		frame.descriptorSet = VK_NULL_HANDLE;
	}
	if (m_rtxDescriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, m_rtxDescriptorPool, nullptr);
		m_rtxDescriptorPool = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImageView != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroyImageView(device, m_shadowMaskImageView, nullptr);
		m_shadowMaskImageView = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImage != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
	}
	m_rtxSbt.Shutdown(context);
	m_rtxPipeline.Shutdown(context);
	m_shadowMaskWidth = 0u;
	m_shadowMaskHeight = 0u;
	m_voxelAwareRtxActive = false;
}

bool RayTracedShadows::InitializeShadowMaskClear(const VulkanContextState &context)
{
	if (m_shadowMaskImage == VK_NULL_HANDLE || context.device == VK_NULL_HANDLE
			|| context.commandPool == VK_NULL_HANDLE || context.queue == VK_NULL_HANDLE) {
		return false;
	}

	VkCommandBufferAllocateInfo cmdInfo{};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdInfo.commandPool = context.commandPool;
	cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdInfo.commandBufferCount = 1u;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(context.device, &cmdInfo, &cmd) != VK_SUCCESS) {
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
		return false;
	}

	VkImageMemoryBarrier toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toTransfer.srcAccessMask = VK_ACCESS_NONE;
	toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.image = m_shadowMaskImage;
	toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0u, 0u, nullptr, 0u, nullptr, 1u, &toTransfer);

	VkClearColorValue clearValue{};
	clearValue.float32[0] = 1.0f;
	clearValue.float32[1] = 1.0f;
	clearValue.float32[2] = 1.0f;
	clearValue.float32[3] = 1.0f;
	VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	vkCmdClearColorImage(cmd, m_shadowMaskImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &range);

	VkImageMemoryBarrier toGeneral{};
	toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toGeneral.dstAccessMask = VK_ACCESS_NONE;
	toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.image = m_shadowMaskImage;
	toGeneral.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0u, 0u, nullptr, 0u, nullptr, 1u, &toGeneral);

	vkEndCommandBuffer(cmd);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
		vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
		return false;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1u;
	submitInfo.pCommandBuffers = &cmd;
	const VkResult queueSubmitResult = vkQueueSubmit(context.queue, 1u, &submitInfo, fence);
	if (queueSubmitResult == VK_SUCCESS) {
		vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
	}

	vkDestroyFence(context.device, fence, nullptr);
	vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
	return queueSubmitResult == VK_SUCCESS;
}

bool RayTracedShadows::CreateShadowMaskFallback(const VulkanContextState &context, RenderState *render)
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

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8_UNORM;
	imageInfo.extent = { 1u, 1u, 1u };
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
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	if (vkCreateImageView(context.device, &viewInfo, nullptr, &render->rtxShadowMaskFallbackView) != VK_SUCCESS) {
		vmaDestroyImage(context.allocator, render->rtxShadowMaskFallbackImage, render->rtxShadowMaskFallbackAllocation);
		render->rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
		render->rtxShadowMaskFallbackAllocation = nullptr;
		render->rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
		return false;
	}
	return true;
}

void RayTracedShadows::ReleaseShadowMaskFallback(const VulkanContextState &context, RenderState *render) noexcept
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

bool CreateRtxShadowMaskFallbackOnly(VulkanContextState *context, RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context != nullptr && render != nullptr,
		"Render",
		"CreateRtxShadowMaskFallbackOnly.Preconditions",
		"vulkan context or render state is null");
	if (context->device == VK_NULL_HANDLE || context->allocator == nullptr) {
		return false;
	}
	if (render->rtxShadowMaskFallbackView != VK_NULL_HANDLE) {
		return true;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8_UNORM;
	imageInfo.extent = { 1u, 1u, 1u };
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
			context->allocator,
			&imageInfo,
			&allocInfo,
			&render->rtxShadowMaskFallbackImage,
			&render->rtxShadowMaskFallbackAllocation,
			nullptr) != VK_SUCCESS) {
		return false;
	}
	VmaAllocationInfo mappedInfo{};
	vmaGetAllocationInfo(context->allocator, render->rtxShadowMaskFallbackAllocation, &mappedInfo);
	render->rtxShadowMaskFallbackMemory = mappedInfo.deviceMemory;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->rtxShadowMaskFallbackImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8_UNORM;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->rtxShadowMaskFallbackView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->rtxShadowMaskFallbackImage, render->rtxShadowMaskFallbackAllocation);
		render->rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
		render->rtxShadowMaskFallbackAllocation = nullptr;
		render->rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
		return false;
	}
	return true;
}

void DestroyRtxShadowMaskFallbackOnly(VulkanContextState *context, RenderState *render) noexcept
{
	if (context == nullptr || render == nullptr) {
		return;
	}
	if (render->rtxShadowMaskFallbackView != VK_NULL_HANDLE && context->device != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->rtxShadowMaskFallbackView, nullptr);
	}
	render->rtxShadowMaskFallbackView = VK_NULL_HANDLE;
	if (render->rtxShadowMaskFallbackImage != VK_NULL_HANDLE && context->allocator != nullptr) {
		vmaDestroyImage(context->allocator, render->rtxShadowMaskFallbackImage, render->rtxShadowMaskFallbackAllocation);
	}
	render->rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
	render->rtxShadowMaskFallbackAllocation = nullptr;
	render->rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
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

}  // namespace projectv::render
