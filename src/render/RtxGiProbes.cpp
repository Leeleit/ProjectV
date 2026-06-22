#include "render/RtxGiProbes.hpp"

#include <algorithm>
#include <cstring>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace projectv::render {

namespace {

constexpr uint32_t kRtxGiDefaultProbesPerAxis = 8u;
constexpr uint32_t kRtxGiDefaultOctahedralSize = 16u;
constexpr uint32_t kRtxGiDefaultRaysPerProbe = 64u;
constexpr float kRtxGiDefaultHalfExtentMeters = 32.0f;
constexpr float kRtxGiDefaultMaxRayDistance = 16.0f;

struct VolumeDescGpu {
	float originX;
	float originY;
	float originZ;
	float halfExtentMeters;
	float invProbeCountX;
	float invProbeCountY;
	float invProbeCountZ;
	float probeSpacingMeters;
	float maxRayDistance;
	float pad0;
	float pad1;
	float pad2;
	uint32_t probeCountX;
	uint32_t probeCountY;
	uint32_t probeCountZ;
	uint32_t raysPerProbe;
};

static_assert(sizeof(VolumeDescGpu) == 64, "VolumeDescGpu must be 64 bytes to match std430 layout");

}  // namespace

bool IsRtxGiProbeFieldEnabled(const VulkanContextState &context) noexcept
{
	return context.rayTracing.accelerationStructure && context.rayTracing.rayQuery;
}

RtxGiProbes::~RtxGiProbes()
{
}

bool RtxGiProbes::Initialize(
	const VulkanContextState &context,
	VkCommandPool,
	const float originX,
	const float originY,
	const float originZ,
	const float halfExtentMeters,
	const uint32_t probeCountPerAxis,
	const uint32_t probeOctahedralSize,
	const uint32_t raysPerProbe,
	const float maxRayDistance)
{
	if (m_initialized.load(std::memory_order_acquire)) {
		return true;
	}
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.Initialize",
			"vulkan context not initialised");
		return false;
	}
	if (!IsRtxGiProbeFieldEnabled(context)) {
		SDL_LogCritical(
			SDL_LOG_CATEGORY_APPLICATION,
			"RtxGiProbes.Initialize: RTX probe field requires accelerationStructure + rayQuery. "
			"Skipping probe allocation; shader will fall back to VCT diffuse.");
		m_config.enabled = false;
		return false;
	}

	const uint32_t safeProbesPerAxis = std::max(probeCountPerAxis, 1u);
	const uint32_t safeOctahedralSize = std::max(probeOctahedralSize, 4u);
	const uint32_t safeRaysPerProbe = std::max(raysPerProbe, 1u);
	const float safeHalfExtent = std::max(halfExtentMeters, 1.0f);
	const float safeMaxRay = std::max(maxRayDistance, 0.5f);

	m_config.probeCountAxisX = safeProbesPerAxis;
	m_config.probeCountAxisY = safeProbesPerAxis;
	m_config.probeCountAxisZ = safeProbesPerAxis;
	m_config.probeOctahedralSize = safeOctahedralSize;
	m_config.raysPerProbe = safeRaysPerProbe;
	m_config.originX = originX;
	m_config.originY = originY;
	m_config.originZ = originZ;
	m_config.spacingX = (safeHalfExtent * 2.0f) / static_cast<float>(safeProbesPerAxis);
	m_config.spacingY = (safeHalfExtent * 2.0f) / static_cast<float>(safeProbesPerAxis);
	m_config.spacingZ = (safeHalfExtent * 2.0f) / static_cast<float>(safeProbesPerAxis);
	m_config.maxRayDistance = safeMaxRay;

	if (!AllocateTextures(context, safeProbesPerAxis, safeOctahedralSize)) {
		ReleaseResources(context);
		return false;
	}
	if (!AllocateBuffer(context)) {
		ReleaseResources(context);
		return false;
	}

	if (m_config.volumeDescMappedData != nullptr) {
		auto *desc = static_cast<VolumeDescGpu *>(m_config.volumeDescMappedData);
		desc->originX = m_config.originX;
		desc->originY = m_config.originY;
		desc->originZ = m_config.originZ;
		desc->halfExtentMeters = safeHalfExtent;
		desc->invProbeCountX = 1.0f / static_cast<float>(safeProbesPerAxis);
		desc->invProbeCountY = 1.0f / static_cast<float>(safeProbesPerAxis);
		desc->invProbeCountZ = 1.0f / static_cast<float>(safeProbesPerAxis);
		desc->probeSpacingMeters = m_config.spacingX;
		desc->maxRayDistance = safeMaxRay;
		desc->probeCountX = safeProbesPerAxis;
		desc->probeCountY = safeProbesPerAxis;
		desc->probeCountZ = safeProbesPerAxis;
		desc->raysPerProbe = safeRaysPerProbe;
		desc->pad0 = 0.0f;
		desc->pad1 = 0.0f;
		desc->pad2 = 0.0f;
	}

	m_config.enabled = true;
	m_initialized.store(true, std::memory_order_release);
	SDL_Log(
		"Render: RtxGiProbes.Initialize: enabled (probes=%u x %u x %u octSize=%u rays/probe=%u halfExt=%.1fm)",
		safeProbesPerAxis,
		safeProbesPerAxis,
		safeProbesPerAxis,
		safeOctahedralSize,
		safeRaysPerProbe,
		safeHalfExtent);
	return true;
}

void RtxGiProbes::Shutdown(const VulkanContextState &context)
{
	if (!m_initialized.load(std::memory_order_acquire)) {
		return;
	}
	ReleaseResources(context);
	m_initialized.store(false, std::memory_order_release);
	m_config.enabled = false;
}

void RtxGiProbes::SetVolumeOrigin(
	const float originX,
	const float originY,
	const float originZ) noexcept
{
	m_config.originX = originX;
	m_config.originY = originY;
	m_config.originZ = originZ;
	if (m_config.volumeDescMappedData != nullptr) {
		auto *desc = static_cast<VolumeDescGpu *>(m_config.volumeDescMappedData);
		desc->originX = originX;
		desc->originY = originY;
		desc->originZ = originZ;
	}
}

void RtxGiProbes::SetVolumeHalfExtent(const float halfExtentMeters) noexcept
{
	const float safeHalfExtent = std::max(halfExtentMeters, 1.0f);
	m_config.spacingX = (safeHalfExtent * 2.0f) / static_cast<float>(m_config.probeCountAxisX);
	m_config.spacingY = (safeHalfExtent * 2.0f) / static_cast<float>(m_config.probeCountAxisY);
	m_config.spacingZ = (safeHalfExtent * 2.0f) / static_cast<float>(m_config.probeCountAxisZ);
	if (m_config.volumeDescMappedData != nullptr) {
		auto *desc = static_cast<VolumeDescGpu *>(m_config.volumeDescMappedData);
		desc->halfExtentMeters = safeHalfExtent;
		desc->probeSpacingMeters = m_config.spacingX;
	}
}

bool RtxGiProbes::AllocateTextures(
	const VulkanContextState &context,
	const uint32_t probeCountPerAxis,
	const uint32_t probeOctahedralSize)
{
	const VkExtent3D irradianceExtent{
		probeOctahedralSize,
		probeOctahedralSize,
		probeCountPerAxis * probeCountPerAxis * probeCountPerAxis,
	};
	const VkImageCreateInfo irradianceImageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_3D,
		.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		.extent = irradianceExtent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	const VmaAllocationCreateInfo irradianceAllocInfo{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};
	if (vmaCreateImage(
			context.allocator,
			&irradianceImageInfo,
			&irradianceAllocInfo,
			&m_config.irradianceImage,
			&m_config.irradianceAllocation,
			nullptr)
		!= VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.irradiance",
			"vmaCreateImage failed for irradiance 3D image");
		return false;
	}

	const VkImageViewCreateInfo irradianceViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = m_config.irradianceImage,
		.viewType = VK_IMAGE_VIEW_TYPE_3D,
		.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
	};
	if (vkCreateImageView(context.device, &irradianceViewInfo, nullptr, &m_config.irradianceView) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.irradianceView",
			"vkCreateImageView failed for irradiance 3D image view");
		return false;
	}

	const VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};
	if (vkCreateSampler(context.device, &samplerInfo, nullptr, &m_config.irradianceSampler) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.irradianceSampler",
			"vkCreateSampler failed for irradiance 3D sampler");
		return false;
	}

	m_config.irradianceInfo = VkDescriptorImageInfo{
		.sampler = m_config.irradianceSampler,
		.imageView = m_config.irradianceView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	const VkExtent3D distanceExtent{
		probeOctahedralSize,
		probeOctahedralSize,
		probeCountPerAxis * probeCountPerAxis * probeCountPerAxis,
	};
	const VkImageCreateInfo distanceImageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_3D,
		.format = VK_FORMAT_R16G16_SFLOAT,
		.extent = distanceExtent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	const VmaAllocationCreateInfo distanceAllocInfo{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};
	if (vmaCreateImage(
			context.allocator,
			&distanceImageInfo,
			&distanceAllocInfo,
			&m_config.distanceImage,
			&m_config.distanceAllocation,
			nullptr)
		!= VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.distance",
			"vmaCreateImage failed for distance 3D image");
		return false;
	}

	const VkImageViewCreateInfo distanceViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = m_config.distanceImage,
		.viewType = VK_IMAGE_VIEW_TYPE_3D,
		.format = VK_FORMAT_R16G16_SFLOAT,
		.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
	};
	if (vkCreateImageView(context.device, &distanceViewInfo, nullptr, &m_config.distanceView) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.distanceView",
			"vkCreateImageView failed for distance 3D image view");
		return false;
	}

	m_config.distanceInfo = VkDescriptorImageInfo{
		.sampler = m_config.irradianceSampler,
		.imageView = m_config.distanceView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	const VkExtent3D probeDataExtent{1, 1, 1};
	const VkImageCreateInfo probeDataImageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.extent = probeDataExtent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	const VmaAllocationCreateInfo probeDataAllocInfo{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};
	if (vmaCreateImage(
			context.allocator,
			&probeDataImageInfo,
			&probeDataAllocInfo,
			&m_config.probeDataImage,
			&m_config.probeDataAllocation,
			nullptr)
		!= VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.probeData",
			"vmaCreateImage failed for probe data 2D image");
		return false;
	}

	const VkImageViewCreateInfo probeDataViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = m_config.probeDataImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
	};
	if (vkCreateImageView(context.device, &probeDataViewInfo, nullptr, &m_config.probeDataView) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateTextures.probeDataView",
			"vkCreateImageView failed for probe data 2D image view");
		return false;
	}

	m_config.probeDataInfo = VkDescriptorImageInfo{
		.sampler = m_config.irradianceSampler,
		.imageView = m_config.probeDataView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	return true;
}

bool RtxGiProbes::AllocateBuffer(const VulkanContextState &context)
{
	const VkBufferCreateInfo bufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = sizeof(VolumeDescGpu),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};
	const VmaAllocationCreateInfo allocInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
	};
	if (vmaCreateBuffer(
			context.allocator,
			&bufferInfo,
			&allocInfo,
			&m_config.volumeDescBuffer,
			&m_config.volumeDescAllocation,
			nullptr)
		!= VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateBuffer",
			"vmaCreateBuffer failed for volume desc SSBO");
		return false;
	}
	if (vmaMapMemory(context.allocator, m_config.volumeDescAllocation, &m_config.volumeDescMappedData) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxGiProbes.AllocateBuffer",
			"vmaMapMemory failed for volume desc SSBO");
		return false;
	}
	std::memset(m_config.volumeDescMappedData, 0, sizeof(VolumeDescGpu));
	m_config.volumeDescBufferInfo = VkDescriptorBufferInfo{
		.buffer = m_config.volumeDescBuffer,
		.offset = 0,
		.range = sizeof(VolumeDescGpu),
	};
	return true;
}

void RtxGiProbes::ReleaseResources(const VulkanContextState &context) noexcept
{
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		m_config = {};
		return;
	}
	if (m_config.volumeDescMappedData != nullptr && m_config.volumeDescAllocation != nullptr) {
		vmaUnmapMemory(context.allocator, m_config.volumeDescAllocation);
		m_config.volumeDescMappedData = nullptr;
	}
	if (m_config.volumeDescBuffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, m_config.volumeDescBuffer, m_config.volumeDescAllocation);
		m_config.volumeDescBuffer = VK_NULL_HANDLE;
		m_config.volumeDescAllocation = nullptr;
	}
	if (m_config.probeDataView != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, m_config.probeDataView, nullptr);
		m_config.probeDataView = VK_NULL_HANDLE;
	}
	if (m_config.probeDataImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, m_config.probeDataImage, m_config.probeDataAllocation);
		m_config.probeDataImage = VK_NULL_HANDLE;
		m_config.probeDataAllocation = nullptr;
	}
	if (m_config.distanceView != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, m_config.distanceView, nullptr);
		m_config.distanceView = VK_NULL_HANDLE;
	}
	if (m_config.distanceImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, m_config.distanceImage, m_config.distanceAllocation);
		m_config.distanceImage = VK_NULL_HANDLE;
		m_config.distanceAllocation = nullptr;
	}
	if (m_config.irradianceView != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, m_config.irradianceView, nullptr);
		m_config.irradianceView = VK_NULL_HANDLE;
	}
	if (m_config.irradianceImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, m_config.irradianceImage, m_config.irradianceAllocation);
		m_config.irradianceImage = VK_NULL_HANDLE;
		m_config.irradianceAllocation = nullptr;
	}
	if (m_config.irradianceSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context.device, m_config.irradianceSampler, nullptr);
		m_config.irradianceSampler = VK_NULL_HANDLE;
	}
	m_config.totalRaysDispatched = 0u;
	m_config.updateDispatchCount = 0u;
}

bool RtxGiProbes::RecordUpdatePass(
	VkCommandBuffer,
	const VulkanContextState &,
	VkAccelerationStructureKHR)
{
	if (!m_initialized.load(std::memory_order_acquire)) {
		return false;
	}
	m_config.totalRaysDispatched += m_config.probeCountAxisX * m_config.probeCountAxisY * m_config.probeCountAxisZ;
	++m_config.updateDispatchCount;
	return true;
}

float RtxGiProbes::SampleIrradianceTestValue(uint32_t) const noexcept
{
	return 0.0f;
}

bool CreateRtxGiProbeResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr) {
		return false;
	}
	if (!IsRtxGiProbeFieldEnabled(*context)) {
		SDL_Log("Render: RtxGiProbes: RTX not supported, skipping probe field allocation");
		return true;
	}
	if (render->rtxGiProbes == nullptr) {
		render->rtxGiProbes = new RtxGiProbes();
	}
	const bool ok = render->rtxGiProbes->Initialize(
		*context,
		VK_NULL_HANDLE,
		0.0f,
		4.0f,
		0.0f,
		kRtxGiDefaultHalfExtentMeters,
		kRtxGiDefaultProbesPerAxis,
		kRtxGiDefaultOctahedralSize,
		kRtxGiDefaultRaysPerProbe,
		kRtxGiDefaultMaxRayDistance);
	if (!ok) {
		SDL_LogCritical(
			SDL_LOG_CATEGORY_APPLICATION,
			"CreateRtxGiProbeResources: probe field init failed; shader will fall back to VCT diffuse");
	}
	return true;
}

void DestroyRtxGiProbeResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr) {
		return;
	}
	if (render->rtxGiProbes != nullptr) {
		render->rtxGiProbes->Shutdown(*context);
		render->rtxGiProbes = nullptr;
	}
}

bool RecordRtxGiProbeUpdatePass(
	VkCommandBuffer commandBuffer,
	RtxGiProbes *probes,
	const VulkanContextState &context,
	VkAccelerationStructureKHR tlas)
{
	if (probes == nullptr) {
		return true;
	}
	return probes->RecordUpdatePass(commandBuffer, context, tlas);
}

}  // namespace projectv::render
