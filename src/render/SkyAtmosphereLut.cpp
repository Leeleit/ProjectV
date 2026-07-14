#include "render/SkyAtmosphere.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

namespace projectv::render {

bool IsSkyLutPrecomputeEnabled()
{
	const char *value = core::GetEnvVar("PROJECTV_SKY_LUT");
	return value != nullptr && value[0] == 'O' && value[1] == 'N' && value[2] == '\0';
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

	bool ok = true;
	bool creationAttempted = false;
	if (!IsSkyLutPrecomputeEnabled()) {
		DestroySkyLutResources(context, render);
		ok = false;
	} else {
		DestroySkyLutResources(context, render);
		creationAttempted = true;

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
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyLutLinearSampler), VK_OBJECT_TYPE_SAMPLER, "SkyLutLinearSampler");
			}
		}

		if (ok) {
			if (!CreateSkyViewLut(context, render)) {
				ok = false;
			}
		}
		if (ok) {
			if (!CreateMultiScatteringLut(context, render)) {
				ok = false;
			}
		}
		if (ok) {
			render->skyLutPrecomputeEnabled = true;
		}
	}

	if (!ok && creationAttempted) {
		DestroySkyLutResources(context, render);
	}
	return ok;
}

float CosThetaClamped(const float cosTheta)
{
	return std::max(cosTheta, -1.0f);
}

float SchlickPhaseFunction(const float cosTheta, const float g)
{
	const float k = 1.55f + g * (-50.0f + g * (230.0f + g * (-490.0f + g * 425.0f)));
	const float numerator = 1.0f + k * cosTheta;
	const float denominator = 1.0f + k;
	return numerator / denominator;
}

float RayleighDensity(const float altitude)
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

	std::vector<glm::vec4> lutData(static_cast<std::size_t>(kSkyViewLutWidth) * kSkyViewLutHeight);
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

			float opticalDepth = 0.0f;
			for (int step = 0; step < sky_atmosphere_constants::kRaymarchStepCount; ++step) {
				const float t = (static_cast<float>(step) + 0.5f) /
								static_cast<float>(sky_atmosphere_constants::kRaymarchStepCount);
				const float sampleAltitude = t * sky_atmosphere_constants::kAtmosphereHeight;
				const float density = RayleighDensity(sampleAltitude);
				opticalDepth += density * 0.01f;
			}
			const glm::vec3 transmittance(
				std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaR),
				std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaG),
				std::exp(-opticalDepth * sky_atmosphere_constants::kRayleighBetaB));

			float miePhase = 0.0f;
			float rayleighPhase = 0.0f;
			if (cosScattering > -1.0f) {
				miePhase = SchlickPhaseFunction(cosScattering, sky_atmosphere_constants::kMieG);
				rayleighPhase = 0.75f * (1.0f + cosScattering * cosScattering);
			}

			static constexpr glm::vec3 rayleighBeta(
				sky_atmosphere_constants::kRayleighBetaR,
				sky_atmosphere_constants::kRayleighBetaG,
				sky_atmosphere_constants::kRayleighBetaB);
			const glm::vec3 inscatter = transmittance * (rayleighBeta * 60000.0f * rayleighPhase +
														 sky_atmosphere_constants::kMieBeta * 60000.0f * miePhase);

			lutData[y * kSkyViewLutWidth + x] = glm::vec4(inscatter, transmittance.r);
		}
	}

	bool ok = true;

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
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
		ok = false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->skyViewLutImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = kSkyViewLutFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (ok && vkCreateImageView(context->device, &viewInfo, nullptr, &render->skyViewLutView) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render", "CreateSkyViewLut.vkCreateImageView", "failed");
		ok = false;
	}

	void *mapped = nullptr;
	if (ok) {
		const VkResult mapResult = vmaMapMemory(context->allocator, render->skyViewLutAllocation, &mapped);
		if (mapResult != VK_SUCCESS || mapped == nullptr) {
			runtime::LogVkFailure("CreateSkyViewLut.vmaMapMemory", mapResult);
			ok = false;
		}
	}

	if (ok && mapped != nullptr) {
		std::vector<uint16_t> halfFloats(static_cast<std::size_t>(kSkyViewLutWidth) * kSkyViewLutHeight * 4);
		for (size_t i = 0; i < lutData.size(); ++i) {
			halfFloats[i * 4 + 0] = glm::packHalf1x16(lutData[i].r);
			halfFloats[i * 4 + 1] = glm::packHalf1x16(lutData[i].g);
			halfFloats[i * 4 + 2] = glm::packHalf1x16(lutData[i].b);
			halfFloats[i * 4 + 3] = glm::packHalf1x16(lutData[i].a);
		}
		std::memcpy(mapped, halfFloats.data(), halfFloats.size() * sizeof(uint16_t));
		vmaUnmapMemory(context->allocator, render->skyViewLutAllocation);
		vmaInvalidateAllocation(context->allocator, render->skyViewLutAllocation, 0u, VK_WHOLE_SIZE);
	}

	if (ok) {
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyViewLutImage), VK_OBJECT_TYPE_IMAGE, "SkyViewLutImage");
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyViewLutView), VK_OBJECT_TYPE_IMAGE_VIEW, "SkyViewLutView");
	} else {
		if (render->skyViewLutView != VK_NULL_HANDLE) {
			vkDestroyImageView(context->device, render->skyViewLutView, nullptr);
			render->skyViewLutView = VK_NULL_HANDLE;
		}
		if (render->skyViewLutImage != VK_NULL_HANDLE) {
			vmaDestroyImage(context->allocator, render->skyViewLutImage, render->skyViewLutAllocation);
			render->skyViewLutImage = VK_NULL_HANDLE;
			render->skyViewLutAllocation = nullptr;
		}
	}
	return ok;
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

	std::vector<glm::vec4> lutData(static_cast<std::size_t>(kMultiScatteringLutWidth) * kMultiScatteringLutHeight);
	for (uint32_t y = 0; y < kMultiScatteringLutHeight; ++y) {
		for (uint32_t x = 0; x < kMultiScatteringLutWidth; ++x) {
			const float altitude = static_cast<float>(x) / static_cast<float>(kMultiScatteringLutWidth - 1);
			const float altitudeMeters = altitude * sky_atmosphere_constants::kAtmosphereHeight;
			const float density = RayleighDensity(altitudeMeters);

			glm::vec3 multiScatter(0.0f);
			for (int octave = 0; octave < 2; ++octave) {
				const float weight = octave == 0 ? 0.5f : 0.3f;
				multiScatter += glm::vec3(weight) * glm::vec3(density * density * 0.06f);
			}
			multiScatter *= glm::vec3(0.7f, 0.85f, 1.0f);

			lutData[y * kMultiScatteringLutWidth + x] = glm::vec4(multiScatter, density);
		}
	}

	bool ok = true;

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
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
		ok = false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->multiScatteringLutImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = kMultiScatteringLutFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (ok && vkCreateImageView(context->device, &viewInfo, nullptr, &render->multiScatteringLutView) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render", "CreateMultiScatteringLut.vkCreateImageView", "failed");
		ok = false;
	}

	void *mapped = nullptr;
	if (ok) {
		const VkResult mapResult = vmaMapMemory(context->allocator, render->multiScatteringLutAllocation, &mapped);
		if (mapResult != VK_SUCCESS || mapped == nullptr) {
			runtime::LogVkFailure("CreateMultiScatteringLut.vmaMapMemory", mapResult);
			ok = false;
		}
	}

	if (ok && mapped != nullptr) {
		std::vector<uint16_t> halfFloats(static_cast<std::size_t>(kMultiScatteringLutWidth) * kMultiScatteringLutHeight * 4);
		for (size_t i = 0; i < lutData.size(); ++i) {
			halfFloats[i * 4 + 0] = glm::packHalf1x16(lutData[i].r);
			halfFloats[i * 4 + 1] = glm::packHalf1x16(lutData[i].g);
			halfFloats[i * 4 + 2] = glm::packHalf1x16(lutData[i].b);
			halfFloats[i * 4 + 3] = glm::packHalf1x16(lutData[i].a);
		}
		std::memcpy(mapped, halfFloats.data(), halfFloats.size() * sizeof(uint16_t));
		vmaUnmapMemory(context->allocator, render->multiScatteringLutAllocation);
		vmaInvalidateAllocation(context->allocator, render->multiScatteringLutAllocation, 0u, VK_WHOLE_SIZE);
	}

	if (ok) {
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->multiScatteringLutImage), VK_OBJECT_TYPE_IMAGE, "MultiScatteringLutImage");
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->multiScatteringLutView), VK_OBJECT_TYPE_IMAGE_VIEW, "MultiScatteringLutView");
	} else {
		if (render->multiScatteringLutView != VK_NULL_HANDLE) {
			vkDestroyImageView(context->device, render->multiScatteringLutView, nullptr);
			render->multiScatteringLutView = VK_NULL_HANDLE;
		}
		if (render->multiScatteringLutImage != VK_NULL_HANDLE) {
			vmaDestroyImage(context->allocator, render->multiScatteringLutImage, render->multiScatteringLutAllocation);
			render->multiScatteringLutImage = VK_NULL_HANDLE;
			render->multiScatteringLutAllocation = nullptr;
		}
	}
	return ok;
}
} // namespace projectv::render
