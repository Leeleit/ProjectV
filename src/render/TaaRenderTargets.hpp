#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include "core/Types.hpp"


struct VulkanContextState;


namespace projectv::taa {
using VmaAllocationHandle = void *;

enum class TaaError : std::uint8_t {
	PreconditionFailed = 0,
	ImageCreateFailed,
	ImageViewCreateFailed,
	SamplerCreateFailed,
};

constexpr std::string_view toString(TaaError const e) noexcept {
	switch (e) {
	case TaaError::PreconditionFailed: return "PreconditionFailed";
	case TaaError::ImageCreateFailed: return "ImageCreateFailed";
	case TaaError::ImageViewCreateFailed: return "ImageViewCreateFailed";
	case TaaError::SamplerCreateFailed: return "SamplerCreateFailed";
	}
	return "Unknown";
}
}

#include <vulkan/vulkan.h>

namespace projectv::taa {


inline constexpr VkFormat kTaaSceneColorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;


inline constexpr VkFormat kTaaLayerHistoryColorFormat = VK_FORMAT_R8G8B8A8_UNORM;


struct OffscreenColorTarget {
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocationHandle allocation = nullptr;
};

std::expected<void, projectv::taa::TaaError> CreateOrRecreateTaaRenderTargets(
	VulkanContextState *context,
	VkExtent2D extent,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	VkSampler &linearSampler);


void DestroyTaaRenderTargets(
	VulkanContextState *context,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	VkSampler &linearSampler);


void TransitionTaaSceneColorForWrite(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor);
void TransitionTaaSceneColorForSample(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor);
void TransitionTaaHistoryForSample(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &historyColor);

void RecordTaaHistoryCopy(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor,
	const OffscreenColorTarget &historyColor,
	VkExtent2D extent);

} // namespace projectv::taa

