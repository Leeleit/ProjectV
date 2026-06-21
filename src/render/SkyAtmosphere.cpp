#include "render/SkyAtmosphere.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <cstring>
#include <vector>

#include "SDL3/SDL_log.h"
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

namespace {
constexpr char kSkyAtmosphereVertexShaderFilename[] = "sky_atmosphere.vert.spv";
constexpr char kSkyAtmosphereFragmentShaderFilename[] = "sky_atmosphere.frag.spv";

uint16_t FloatToHalf(float value)
{
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(uint32_t));
	const uint32_t sign = (bits >> 16) & 0x8000u;
	int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;
	uint32_t mantissa = bits & 0x7fffffu;
	if (exponent <= 0) {
		if (exponent < -10) {
			return static_cast<uint16_t>(sign);
		}
		mantissa = (mantissa | 0x800000u) >> (1 - exponent);
		return static_cast<uint16_t>(sign | (mantissa >> 13));
	}
	if (exponent >= 31) {
		return static_cast<uint16_t>(sign | 0x7c00u | (mantissa ? 1u : 0u));
	}
	return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

constexpr VkDescriptorSetLayoutCreateInfo kSkyAtmosphereDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = 2,
	.pBindings = nullptr,
};

constexpr std::array<VkDescriptorSetLayoutBinding, 2> kSkyAtmosphereDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
};

constexpr std::array<VkDescriptorPoolSize, 1> kSkyAtmosphereDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2u,
	},
};

}  // namespace

namespace projectv::render {

bool IsSkyLutPrecomputeEnabled()
{
	const char *value = std::getenv("PROJECTV_SKY_LUT");
	if (value == nullptr || value[0] == '\0') {
		return false;
	}
	return value[0] == 'O' && value[1] == 'N';
}

void DestroySkyLutResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->skyViewLutView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->skyViewLutView, nullptr);
		render->skyViewLutView = VK_NULL_HANDLE;
	}
	if (render->skyViewLutImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->skyViewLutImage, render->skyViewLutAllocation);
		render->skyViewLutImage = VK_NULL_HANDLE;
		render->skyViewLutAllocation = nullptr;
	}
	if (render->multiScatteringLutView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->multiScatteringLutView, nullptr);
		render->multiScatteringLutView = VK_NULL_HANDLE;
	}
	if (render->multiScatteringLutImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->multiScatteringLutImage, render->multiScatteringLutAllocation);
		render->multiScatteringLutImage = VK_NULL_HANDLE;
		render->multiScatteringLutAllocation = nullptr;
	}
	if (render->skyLutLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->skyLutLinearSampler, nullptr);
		render->skyLutLinearSampler = VK_NULL_HANDLE;
	}
}

bool CreateSkyLutResources(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateSkyLutResources");
	profiling::PlotValue("Sky LUT Precompute (ms)", 1.0);
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateSkyLutResources.Preconditions", "missing context");
	if (!IsSkyLutPrecomputeEnabled()) {
		DestroySkyLutResources(context, render);
		return false;
	}

	DestroySkyLutResources(context, render);

	if (render->skyLutLinearSampler == VK_NULL_HANDLE) {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		if (vkCreateSampler(context->device, &samplerInfo, nullptr, &render->skyLutLinearSampler) != VK_SUCCESS) {
			runtime::LogRuntimeFailure(
				"Render", "CreateSkyLutResources.vkCreateSampler", "failed");
			DestroySkyLutResources(context, render);
			return false;
		}
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyLutLinearSampler), VK_OBJECT_TYPE_SAMPLER, "SkyLutLinearSampler");
	}

	if (!CreateSkyViewLut(context, render)) {
		DestroySkyLutResources(context, render);
		return false;
	}
	if (!CreateMultiScatteringLut(context, render)) {
		DestroySkyLutResources(context, render);
		return false;
	}
	render->skyLutPrecomputeEnabled = true;
	return true;
}

float CosThetaClamped(float cosTheta)
{
	return std::max(cosTheta, -1.0f);
}

float SchlickPhaseFunction(float cosTheta, float g)
{
	const float k = 1.55f + g * (-50.0f + g * (230.0f + g * (-490.0f + g * 425.0f)));
	const float numerator = 1.0f + k * cosTheta;
	const float denominator = 1.0f + k;
	return numerator / denominator;
}

float RayleighDensity(float altitude)
{
	return std::exp(-altitude / 8500.0f);
}

bool CreateSkyViewLut(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateSkyViewLut");
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render", "CreateSkyViewLut.Preconditions", "missing context");
	if (render->skyViewLutImage != VK_NULL_HANDLE) {
		return true;
	}

	std::vector<glm::vec4> lutData(kSkyViewLutWidth * kSkyViewLutHeight);
	constexpr float kPi = 3.14159265f;
	for (uint32_t y = 0; y < kSkyViewLutHeight; ++y) {
		const float sunZenith = static_cast<float>(y) / static_cast<float>(kSkyViewLutHeight - 1);
		const float cosSunZenith = std::cos(sunZenith * kPi * 0.5f);
		const float sinSunZenith = std::sin(sunZenith * kPi * 0.5f);

		for (uint32_t x = 0; x < kSkyViewLutWidth; ++x) {
			const float azimuth = static_cast<float>(x) / static_cast<float>(kSkyViewLutWidth - 1);
			const float viewZenith = azimuth;
			const float cosViewZenith = std::cos(viewZenith * kPi * 0.5f);
			const float sinViewZenith = std::sin(viewZenith * kPi * 0.5f);

			const float cosScattering = cosViewZenith * cosSunZenith +
									   sinViewZenith * sinSunZenith * std::cos(0.0f);

			glm::vec3 transmittance(1.0f);
			glm::vec3 inscatter(0.0f);

			float opticalDepth = 0.0f;
			for (int step = 0; step < sky_atmosphere_constants::kRaymarchStepCount; ++step) {
				const float t = (static_cast<float>(step) + 0.5f) /
								static_cast<float>(sky_atmosphere_constants::kRaymarchStepCount);
				const float sampleAltitude = t * sky_atmosphere_constants::kAtmosphereHeight;
				const float density = RayleighDensity(sampleAltitude);
				opticalDepth += density * 0.01f;
			}
			transmittance = glm::vec3(std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaR),
									  std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaG),
									  std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaB));

			float miePhase = 0.0f;
			float rayleighPhase = 0.0f;
			if (cosScattering > -1.0f) {
				miePhase = SchlickPhaseFunction(cosScattering, sky_atmosphere_constants::kMieG);
				rayleighPhase = 0.75f * (1.0f + cosScattering * cosScattering);
			}

			const glm::vec3 rayleighBeta(
				sky_atmosphere_constants::kRayleighBetaR,
				sky_atmosphere_constants::kRayleighBetaG,
				sky_atmosphere_constants::kRayleighBetaB);
			inscatter = transmittance * (rayleighBeta * 60000.0f * rayleighPhase +
										sky_atmosphere_constants::kMieBeta * 60000.0f * miePhase);

			lutData[y * kSkyViewLutWidth + x] = glm::vec4(inscatter, transmittance.r);
		}
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = kSkyViewLutFormat;
	imageInfo.extent = {kSkyViewLutWidth, kSkyViewLutHeight, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->skyViewLutImage,
		&render->skyViewLutAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateSkyViewLut.vmaCreateImage", createResult);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->skyViewLutImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = kSkyViewLutFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->skyViewLutView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->skyViewLutImage, render->skyViewLutAllocation);
		render->skyViewLutImage = VK_NULL_HANDLE;
		render->skyViewLutAllocation = nullptr;
		runtime::LogRuntimeFailure(
			"Render", "CreateSkyViewLut.vkCreateImageView", "failed");
		return false;
	}

	void *mapped = nullptr;
	const VkResult mapResult = vmaMapMemory(context->allocator, render->skyViewLutAllocation, &mapped);
	if (mapResult != VK_SUCCESS || mapped == nullptr) {
		vkDestroyImageView(context->device, render->skyViewLutView, nullptr);
		vmaDestroyImage(context->allocator, render->skyViewLutImage, render->skyViewLutAllocation);
		render->skyViewLutImage = VK_NULL_HANDLE;
		render->skyViewLutView = VK_NULL_HANDLE;
		render->skyViewLutAllocation = nullptr;
		runtime::LogVkFailure("CreateSkyViewLut.vmaMapMemory", mapResult);
		return false;
	}

	std::vector<uint16_t> halfFloats(kSkyViewLutWidth * kSkyViewLutHeight * 4);
	for (size_t i = 0; i < lutData.size(); ++i) {
		halfFloats[i * 4 + 0] = FloatToHalf(lutData[i].r);
		halfFloats[i * 4 + 1] = FloatToHalf(lutData[i].g);
		halfFloats[i * 4 + 2] = FloatToHalf(lutData[i].b);
		halfFloats[i * 4 + 3] = FloatToHalf(lutData[i].a);
	}
	std::memcpy(mapped, halfFloats.data(), halfFloats.size() * sizeof(uint16_t));
	vmaUnmapMemory(context->allocator, render->skyViewLutAllocation);
	vmaInvalidateAllocation(context->allocator, render->skyViewLutAllocation, 0u, VK_WHOLE_SIZE);

	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyViewLutImage), VK_OBJECT_TYPE_IMAGE, "SkyViewLutImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyViewLutView), VK_OBJECT_TYPE_IMAGE_VIEW, "SkyViewLutView");
	return true;
}

bool CreateMultiScatteringLut(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateMultiScatteringLut");
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render", "CreateMultiScatteringLut.Preconditions", "missing context");
	if (render->multiScatteringLutImage != VK_NULL_HANDLE) {
		return true;
	}

	std::vector<glm::vec4> lutData(kMultiScatteringLutWidth * kMultiScatteringLutHeight);
	for (uint32_t y = 0; y < kMultiScatteringLutHeight; ++y) {
		const float sunZenith = static_cast<float>(y) / static_cast<float>(kMultiScatteringLutHeight - 1);
		for (uint32_t x = 0; x < kMultiScatteringLutWidth; ++x) {
			const float altitude = static_cast<float>(x) / static_cast<float>(kMultiScatteringLutWidth - 1);
			const float altitudeMeters = altitude * sky_atmosphere_constants::kAtmosphereHeight;
			const float density = RayleighDensity(altitudeMeters);

			glm::vec3 multiScatter(0.0f);
			for (int octave = 0; octave < 2; ++octave) {
				const float weight = (octave == 0) ? 0.5f : 0.3f;
				multiScatter += glm::vec3(weight) * glm::vec3(density * density * 0.06f);
			}
			multiScatter *= glm::vec3(0.7f, 0.85f, 1.0f);

			lutData[y * kMultiScatteringLutWidth + x] = glm::vec4(multiScatter, density);
		}
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = kMultiScatteringLutFormat;
	imageInfo.extent = {kMultiScatteringLutWidth, kMultiScatteringLutHeight, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->multiScatteringLutImage,
		&render->multiScatteringLutAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMultiScatteringLut.vmaCreateImage", createResult);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->multiScatteringLutImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = kMultiScatteringLutFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->multiScatteringLutView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->multiScatteringLutImage, render->multiScatteringLutAllocation);
		render->multiScatteringLutImage = VK_NULL_HANDLE;
		render->multiScatteringLutAllocation = nullptr;
		runtime::LogRuntimeFailure(
			"Render", "CreateMultiScatteringLut.vkCreateImageView", "failed");
		return false;
	}

	void *mapped = nullptr;
	const VkResult mapResult = vmaMapMemory(context->allocator, render->multiScatteringLutAllocation, &mapped);
	if (mapResult != VK_SUCCESS || mapped == nullptr) {
		vkDestroyImageView(context->device, render->multiScatteringLutView, nullptr);
		vmaDestroyImage(context->allocator, render->multiScatteringLutImage, render->multiScatteringLutAllocation);
		render->multiScatteringLutImage = VK_NULL_HANDLE;
		render->multiScatteringLutView = VK_NULL_HANDLE;
		render->multiScatteringLutAllocation = nullptr;
		runtime::LogVkFailure("CreateMultiScatteringLut.vmaMapMemory", mapResult);
		return false;
	}

	std::vector<uint16_t> halfFloats(kMultiScatteringLutWidth * kMultiScatteringLutHeight * 4);
	for (size_t i = 0; i < lutData.size(); ++i) {
		halfFloats[i * 4 + 0] = FloatToHalf(lutData[i].r);
		halfFloats[i * 4 + 1] = FloatToHalf(lutData[i].g);
		halfFloats[i * 4 + 2] = FloatToHalf(lutData[i].b);
		halfFloats[i * 4 + 3] = FloatToHalf(lutData[i].a);
	}
	std::memcpy(mapped, halfFloats.data(), halfFloats.size() * sizeof(uint16_t));
	vmaUnmapMemory(context->allocator, render->multiScatteringLutAllocation);
	vmaInvalidateAllocation(context->allocator, render->multiScatteringLutAllocation, 0u, VK_WHOLE_SIZE);

	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->multiScatteringLutImage), VK_OBJECT_TYPE_IMAGE, "MultiScatteringLutImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->multiScatteringLutView), VK_OBJECT_TYPE_IMAGE_VIEW, "MultiScatteringLutView");
	return true;
}

void DestroySkyAtmospherePipelines(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->skyAtmospherePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->skyAtmospherePipeline, nullptr);
		render->skyAtmospherePipeline = VK_NULL_HANDLE;
	}
	if (render->skyAtmospherePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->skyAtmospherePipelineLayout, nullptr);
		render->skyAtmospherePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereVertexShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->skyAtmosphereVertexShaderModule, nullptr);
		render->skyAtmosphereVertexShaderModule = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereFragmentShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->skyAtmosphereFragmentShaderModule, nullptr);
		render->skyAtmosphereFragmentShaderModule = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->skyAtmosphereDescriptorPool, nullptr);
		render->skyAtmosphereDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->skyAtmosphereDescriptorSetLayout, nullptr);
		render->skyAtmosphereDescriptorSetLayout = VK_NULL_HANDLE;
	}
	render->skyAtmospherePipelineEnabled = false;
}

bool CreateSkyAtmospherePipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateSkyAtmospherePipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateSkyAtmospherePipelines.Preconditions", "missing context");
	if (!IsSkyAtmosphereEnabled()) {
		return false;
	}

	DestroySkyAtmospherePipelines(context, render);

	const std::vector<char> vertexShaderCode = ReadShaderFile(kSkyAtmosphereVertexShaderFilename);
	if (vertexShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateSkyAtmospherePipelines.ReadVertexShader", "sky_atmosphere.vert.spv not found");
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}

	const std::vector<char> fragmentShaderCode = ReadShaderFile(kSkyAtmosphereFragmentShaderFilename);
	if (fragmentShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateSkyAtmospherePipelines.ReadFragmentShader", "sky_atmosphere.frag.spv not found");
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}

	VkShaderModuleCreateInfo vertexModuleInfo{};
	vertexModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexModuleInfo.codeSize = vertexShaderCode.size();
	vertexModuleInfo.pCode = reinterpret_cast<const uint32_t *>(vertexShaderCode.data());
	if (vkCreateShaderModule(context->device, &vertexModuleInfo, nullptr, &render->skyAtmosphereVertexShaderModule) != VK_SUCCESS) {
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereVertexShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "SkyAtmosphereVertexShader");

	VkShaderModuleCreateInfo fragmentModuleInfo{};
	fragmentModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentModuleInfo.codeSize = fragmentShaderCode.size();
	fragmentModuleInfo.pCode = reinterpret_cast<const uint32_t *>(fragmentShaderCode.data());
	if (vkCreateShaderModule(context->device, &fragmentModuleInfo, nullptr, &render->skyAtmosphereFragmentShaderModule) != VK_SUCCESS) {
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereFragmentShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "SkyAtmosphereFragmentShader");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kSkyAtmosphereDescriptorSetLayoutInfo,
		nullptr,
		&render->skyAtmosphereDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateSkyAtmospherePipelines.vkCreateDescriptorSetLayout", layoutResult);
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "SkyAtmosphereDescriptorSetLayout");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kSkyAtmosphereDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kSkyAtmosphereDescriptorPoolSizes.data();
	if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->skyAtmosphereDescriptorPool) != VK_SUCCESS) {
		runtime::LogVkFailure("CreateSkyAtmospherePipelines.vkCreateDescriptorPool", VK_ERROR_OUT_OF_DEVICE_MEMORY);
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "SkyAtmosphereDescriptorPool");

	std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		layouts[i] = render->skyAtmosphereDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->skyAtmosphereDescriptorPool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
	if (vkAllocateDescriptorSets(context->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		const bool useLut = render->skyLutPrecomputeEnabled && render->skyViewLutView != VK_NULL_HANDLE;
		VkDescriptorImageInfo skyViewInfo{};
		skyViewInfo.sampler = render->skyLutLinearSampler;
		skyViewInfo.imageView = useLut ? render->skyViewLutView : VK_NULL_HANDLE;
		skyViewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo multiScatteringInfo{};
		multiScatteringInfo.sampler = render->skyLutLinearSampler;
		multiScatteringInfo.imageView = useLut ? render->multiScatteringLutView : VK_NULL_HANDLE;
		multiScatteringInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 2> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &skyViewInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &multiScatteringInfo;

		vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
	}

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &render->skyAtmosphereDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(SkyAtmospherePushConstants);
	layoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->skyAtmospherePipelineLayout) != VK_SUCCESS) {
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmospherePipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "SkyAtmospherePipelineLayout");

	const VkPipelineShaderStageCreateInfo vertexStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = render->skyAtmosphereVertexShaderModule,
		.pName = "main",
	};
	const VkPipelineShaderStageCreateInfo fragmentStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = render->skyAtmosphereFragmentShaderModule,
		.pName = "main",
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	const VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisampleState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_ALWAYS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	const VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
		.pDynamicStates = dynamicStates,
	};

	VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = (VkPipelineShaderStageCreateInfo[2]){vertexStage, fragmentStage};
	pipelineInfo.pVertexInputState = &vertexInputState;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizationState;
	pipelineInfo.pMultisampleState = &multisampleState;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render->skyAtmospherePipelineLayout;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &render->skyAtmospherePipeline) != VK_SUCCESS) {
		DestroySkyAtmospherePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmospherePipeline), VK_OBJECT_TYPE_PIPELINE, "SkyAtmospherePipeline");

	render->skyAtmospherePipelineEnabled = true;
	return true;
}

bool RecordSkyAtmospherePass(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const SkyAtmospherePushConstants &pushConstants,
	VkImageView sceneColorView,
	VkImageView depthView,
	VkExtent2D extent,
	uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordSkyAtmospherePass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.skyAtmospherePipeline == VK_NULL_HANDLE ||
		render.skyAtmospherePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (sceneColorView == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE) {
		return false;
	}
	if (extent.width == 0u || extent.height == 0u) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}

	const VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor{
		.offset = {0, 0},
		.extent = extent,
	};

	const VkRenderingAttachmentInfo colorAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = sceneColorView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	const VkRenderingAttachmentInfo depthAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {.depthStencil = {1.0f, 0}},
	};

	const VkRenderingInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment,
	};

	vkCmdBeginRendering(commandBuffer, &renderingInfo);
	vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);
	vkCmdSetScissor(commandBuffer, 0u, 1u, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.skyAtmospherePipeline);
	if (render.skyAtmosphereDescriptorSets[frameIndex] != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			render.skyAtmospherePipelineLayout,
			0u,
			1u,
			&render.skyAtmosphereDescriptorSets[frameIndex],
			0u,
			nullptr);
	}
	vkCmdPushConstants(
		commandBuffer,
		render.skyAtmospherePipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		sizeof(SkyAtmospherePushConstants),
		&pushConstants);
	vkCmdDraw(commandBuffer, 3u, 1u, 0u, 0u);
	vkCmdEndRendering(commandBuffer);

	profiling::PlotValue("Sky Atmosphere Pass", 1.0);
	return true;
}

}  // namespace projectv::render
