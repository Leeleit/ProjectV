#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>
#include <cstdint>

#include <vulkan/vulkan.h>

namespace projectv::render {

inline bool IsCloudscapeEnabled()
{
	const char *value = std::getenv("PROJECTV_CLOUDS");
	if (value == nullptr || value[0] == '\0') {
		return false;
	}
	return value[0] == 'O' && value[1] == 'N';
}

struct CloudscapePushConstants {
	std::array<float, 4> cloudColorAndCoverage{};
	std::array<float, 4> sunDirectionAndIntensity{};
	std::array<float, 4> cloudLayerParams{};
	std::array<float, 4> viewParams{};
};
static_assert(sizeof(CloudscapePushConstants) == 64);

constexpr uint32_t kCloudscapeNoiseTextureSize = 128u;
constexpr uint32_t kCloudscapeRaymarchStepCount = 24u;

// EVIL: default cloud coverage (0..1). Higher = denser sky. 0.65 per Nubis 2017 reference.
constexpr float kDefaultCloudCoverage = 0.65f;
// EVIL: default cloud layer contrast (0..1). 0.5 = balanced (matches halfway between flat and crisp).
constexpr float kDefaultCloudContrast = 0.5f;
// EVIL: default cloud color (linear RGB). 0.92/0.94/0.98 = slightly warm cumulus white. Per Hillaire 2020 reference.
constexpr float kDefaultCloudColorR = 0.92f;
constexpr float kDefaultCloudColorG = 0.94f;
constexpr float kDefaultCloudColorB = 0.98f;

bool CreateCloudscapeResources(
	VulkanContextState *context,
	RenderState *render);

void DestroyCloudscapeResources(
	VulkanContextState *context,
	RenderState *render);

bool RecordCloudscapeRaymarchPass(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const CloudscapePushConstants &pushConstants,
	VkImageView sceneColorView,
	VkImageView depthView,
	VkExtent2D extent,
	uint32_t frameIndex);

} // namespace projectv::render
