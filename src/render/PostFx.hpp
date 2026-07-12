#pragma once

#include "core/EnvUtils.hpp"
#include "core/Types.hpp"

#include <array>
#include <vulkan/vulkan.h>

namespace projectv::render {

inline bool IsBloomEnabled()
{
	const char *value = core::GetEnvVar("PROJECTV_BLOOM");
	return value != nullptr && value[0] == '1' && value[1] == '\0';
}

inline bool IsAerialPerspectiveEnabled()
{
	const char *value = core::GetEnvVar("PROJECTV_AERIAL_PERSPECTIVE");
	return value != nullptr && value[0] == '1' && value[1] == '\0';
}

inline bool IsPostFxEnabled()
{
	return IsBloomEnabled() || IsAerialPerspectiveEnabled();
}

struct PostFxPushConstants {
	std::array<float, 4> params0{}; // x=threshold, y=softKnee, z=bloomIntensity, w=mipLevel/aerialEnabled
	std::array<float, 4> params1{}; // x=fogDensity, y=fogMax, z=exposure, w=time
	std::array<float, 4> params2{}; // xyz=sunDirection
	std::array<float, 4> params3{}; // xyz=cameraPosition
	std::array<float, 4> params4{}; // xyz=fogColor, w=reserved
};
static_assert(sizeof(PostFxPushConstants) == 80);

constexpr uint32_t kBloomMipCount = 5u;
constexpr uint32_t kPostFxDescriptorSetCount = 16u;

bool CreatePostFxResources(
	VulkanContextState *context,
	RenderState *render,
	VkExtent2D extent);

void DestroyPostFxResources(
	VulkanContextState *context,
	RenderState *render);

bool RecordPostFxPass(
	VkCommandBuffer commandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const VoxelSceneLighting &lighting,
	const FrameRenderData &frameRenderData,
	VkExtent2D extent,
	uint32_t frameIndex);

} // namespace projectv::render
