#pragma once

#include "core/Types.hpp"

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

}  // namespace projectv::render
