#include "volk.h"
#include "render/RayTracedShadows.hpp"

#include <algorithm>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"

namespace projectv::render {

namespace {

constexpr uint32_t kRtxMaxInitialBlasCount = 4096u;
constexpr uint32_t kRtxMaxPrimitivesPerBlas = 8192u;
constexpr VkDeviceSize kRtxScratchBufferBytes = 32ull * 1024ull * 1024ull;

} // namespace

bool IsRayTracedShadowEnabled(const VulkanContextState &context) noexcept
{
	return context.rayTracing.accelerationStructure && context.rayTracing.rayQuery;
}

RayTracedShadows::~RayTracedShadows() = default;

bool RayTracedShadows::Initialize(
	const VulkanContextState &context,
	VkCommandPool,
	const uint32_t maxBlasCount,
	const uint32_t maxPrimitivesPerBlas,
	const uint32_t minScratchAlignment)
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
	instanceInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	instanceInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo instanceAllocInfo{};
	instanceAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	instanceAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
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
	const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
												nullptr,
												m_config.tlasInstanceBuffer};
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
	scratchInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
		m_config.scratchBuffer};
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
	aabbInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
		m_config.aabbScratchBuffer};
	m_config.aabbScratchDeviceAddress = vkGetBufferDeviceAddress(context.device, &aabbAddressInfo);

	{
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
		tlasBackingInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
			m_config.tlasBackingBuffer};
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

void RayTracedShadows::ReleaseBuffers(const VulkanContextState &context) noexcept // NOLINT(bugprone-exception-escape): vector assignments are bounded
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
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
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

} // namespace projectv::render
