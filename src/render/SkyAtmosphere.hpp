#pragma once

#include "core/EnvUtils.hpp"
#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

#include <vulkan/vulkan.h>

namespace projectv::render {

inline bool IsSkyAtmosphereEnabled()
{
	const char *value = core::GetEnvVar("PROJECTV_SKY");
	return value != nullptr && value[0] == 'O' && value[1] == 'N' && value[2] == '\0';
}

struct SkyAtmospherePushConstants {
	std::array<float, 4> zenithColorAndIntensity{};
	std::array<float, 4> horizonColorAndSunIntensity{};
	std::array<float, 4> sunDirectionAndAngularSize{};
	std::array<float, 4> viewParams{};
};
static_assert(sizeof(SkyAtmospherePushConstants) == 64);

constexpr uint32_t kSkyAtmosphereResolution = 256u;

constexpr uint32_t kSkyViewLutWidth = 256u;	 // Sky-View LUT width per Hillaire 2020 EGSR lat-long mapping.
constexpr uint32_t kSkyViewLutHeight = 128u; // 256×128 RGBA16F captures single-scattering transmittance + inscattering.
constexpr uint32_t kSkyViewLutChannels = 4u;
constexpr VkFormat kSkyViewLutFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

constexpr uint32_t kMultiScatteringLutWidth = 32u;	// Multi-Scattering LUT width per Hillaire 2020 EGSR.
constexpr uint32_t kMultiScatteringLutHeight = 32u; // 32×32 RGBA16F for the 2D multiple-scattering approximation.
constexpr uint32_t kMultiScatteringLutChannels = 4u;
constexpr VkFormat kMultiScatteringLutFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

// Rayleigh coefficients per Hillaire 2020 (lambda^-4 scaling at R=680nm, G=550nm, B=440nm).
namespace sky_atmosphere_constants {
constexpr float kPlanetRadius = 6371000.0f;
constexpr float kAtmosphereHeight = 100000.0f;
constexpr float kRayleighBetaR = 5.8e-6f;
constexpr float kRayleighBetaG = 13.5e-6f;
constexpr float kRayleighBetaB = 33.1e-6f;
constexpr float kMieBeta = 0.005f;
constexpr float kMieG = 0.8f;
constexpr int kRaymarchStepCount = 16;
} // namespace sky_atmosphere_constants

bool IsSkyLutPrecomputeEnabled();
bool CreateSkyLutResources(VulkanContextState *context, RenderState *render);
void DestroySkyLutResources(VulkanContextState *context, RenderState *render);

bool CreateSkyViewLut(VulkanContextState *context, RenderState *render);
bool CreateMultiScatteringLut(VulkanContextState *context, RenderState *render);

bool CreateSkyAtmospherePipelines(
	VulkanContextState *context,
	RenderState *render);

void DestroySkyAtmospherePipelines(
	VulkanContextState *context,
	RenderState *render);

bool RecordSkyAtmosphereDraw(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const SkyAtmospherePushConstants &pushConstants,
	uint32_t frameIndex);

bool RecordSkyAtmospherePass(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const SkyAtmospherePushConstants &pushConstants,
	VkImageView sceneColorView,
	VkImageView depthView,
	VkExtent2D extent,
	uint32_t frameIndex);

} // namespace projectv::render
