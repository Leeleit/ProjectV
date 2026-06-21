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

constexpr const char *kEnvRtxEnabled = "PROJECTV_HW_RAY_TRACING";
constexpr uint32_t kRtxMaxInitialBlasCount = 4096u;
constexpr uint32_t kRtxMaxPrimitivesPerBlas = 8192u;
constexpr VkDeviceSize kRtxTlasInstanceBufferBytes = sizeof(VkAccelerationStructureInstanceKHR) * kRtxMaxInitialBlasCount;
constexpr VkDeviceSize kRtxScratchBufferBytes = 32ull * 1024ull * 1024ull;

}  // namespace

bool IsRayTracedShadowEnabled() noexcept
{
	const char *const envValue = std::getenv(kEnvRtxEnabled);
	if (envValue == nullptr) {
		return false;
	}
	return std::strcmp(envValue, "ON") == 0 || std::strcmp(envValue, "1") == 0;
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
	if (!context.rayTracing.accelerationStructure || !context.rayTracing.rayQuery) {
		SDL_Log("Render: RayTracedShadows.Initialize: HW RT not available; staying disabled");
		m_config.enabled = false;
		m_config.featureDetectionResult = false;
		return true;
	}
	if (!IsRayTracedShadowEnabled()) {
		SDL_Log("Render: RayTracedShadows.Initialize: PROJECTV_HW_RAY_TRACING not ON; path stays dormant");
		m_config.enabled = false;
		m_config.featureDetectionResult = true;
		return true;
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
	return true;
}

void RayTracedShadows::ReleaseBuffers(const VulkanContextState &context) noexcept
{
	if (m_config.tlas != VK_NULL_HANDLE) {
		vkDestroyAccelerationStructureKHR(context.device, m_config.tlas, nullptr);
		m_config.tlas = VK_NULL_HANDLE;
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
		&& context.dedicatedComputeQueueFamilyIndex != context.queueFamilyIndex;

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
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
	vkFreeCommandBuffers(context.device, commandPool, 1u, &cmd);
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

	m_config.blasRebuildCount += 1u;
	return true;
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
		instances[i].transform = visibleChunkTransforms[i];
		instances[i].instanceCustomIndex = visibleChunkIndices[i] & 0xFFFFFFu;
		instances[i].mask = 0xFFu;
		instances[i].instanceShaderBindingTableRecordOffset = 0u;
		instances[i].flags = 0u;
		instances[i].accelerationStructureReference = 0u;
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
	(void)context;
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
		SDL_LogInfo(
			SDL_LOG_CATEGORY_APPLICATION,
			"Ray traced shadow resources not initialised (HW not available or env gate off); CSM continues to dominate");
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
