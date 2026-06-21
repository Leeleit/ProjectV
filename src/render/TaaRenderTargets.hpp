#pragma once

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


// Per `2026-06-21-taa-motion-vectors` verdict=yes Pipeline A + `TODO.md §5.3` line 425 explicit format prescription.
// 16:16 RG velocity buffer per Karis 2014 SIGGRAPH ("16:16 RG velocity buffer" + "velocity accuracy is super important"
// drives vertex-out recommendation). VRAM cost 8 MiB/frame double-buffered @ 1080p = 0.16% of 5.06 GiB budget per
// `hardware-profile.md §3`. R16G16_SFLOAT is signed; motion vector = NDC delta between current and previous frame.
inline constexpr VkFormat kTaaMotionVectorFormat = VK_FORMAT_R16G16_SFLOAT;


struct OffscreenColorTarget {
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocationHandle allocation = nullptr;
};

std::expected<void, TaaError> CreateOrRecreateTaaRenderTargets(
	VulkanContextState *context,
	VkExtent2D extent,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	OffscreenColorTarget &motionVectorColor,
	OffscreenColorTarget &motionVectorHistoryColor,
	VkSampler &linearSampler);


void DestroyTaaRenderTargets(
	VulkanContextState *context,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	OffscreenColorTarget &motionVectorColor,
	OffscreenColorTarget &motionVectorHistoryColor,
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
void TransitionTaaMotionVectorForSample(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &motionVectorColor);

void RecordTaaHistoryCopy(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor,
	const OffscreenColorTarget &historyColor,
	VkExtent2D extent);
void RecordTaaMotionVectorHistoryCopy(
	VkCommandBuffer cmd,
	const OffscreenColorTarget &motionVectorColor,
	const OffscreenColorTarget &motionVectorHistoryColor,
	VkExtent2D extent);

} // namespace projectv::taa

