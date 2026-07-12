#pragma once

#include "core/EnvUtils.hpp"
#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

#include <vulkan/vulkan.h>

namespace projectv::render {

inline bool IsVolumetricFogEnabled()
{
	const char *value = core::GetEnvVar("PROJECTV_FOG");
	return value != nullptr && value[0] == 'O' && value[1] == 'N' && value[2] == '\0';
}

struct VolumetricFogPushConstants {
	std::array<float, 4> fogColorAndDensity{};
	std::array<float, 4> sunDirectionAndIntensity{};
	std::array<float, 4> cameraPositionAndMaxDistance{};
	std::array<float, 4> viewParams{};
};
static_assert(sizeof(VolumetricFogPushConstants) == 64);

constexpr uint32_t kVolumetricFogFroxelWidth = 160u;
constexpr uint32_t kVolumetricFogFroxelHeight = 90u;
constexpr uint32_t kVolumetricFogFroxelDepth = 64u;
constexpr uint32_t kVolumetricFogRaymarchStepCount = 12u;

bool CreateVolumetricFogFallbackOnly(
	VulkanContextState *context,
	RenderState *render);

bool CreateVolumetricFogResources(
	VulkanContextState *context,
	RenderState *render);

void DestroyVolumetricFogResources(
	VulkanContextState *context,
	RenderState *render);

bool RecordVolumetricFogAccumulationPass(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const VolumetricFogPushConstants &pushConstants,
	uint32_t frameIndex);

} // namespace projectv::render
