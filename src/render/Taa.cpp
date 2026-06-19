#include "render/Taa.hpp"

#include <array>

namespace projectv::taa {

namespace {

/// \brief Halton sequence helper.
///
/// \details
/// Returns the `index`-th value in the low-discrepancy
///  `base`-Halton sequence. Indices start at 0; the first call returns 0.

float HaltonSequence(uint32_t index, const uint32_t base)
{
	float result = 0.0f;
	float inverseBase = 1.0f / static_cast<float>(base);
	while (index > 0u) {
		result += static_cast<float>(index % base) * inverseBase;
		index /= base;
		inverseBase /= static_cast<float>(base);
	}
	return result;
}

} // namespace

std::array<float, 2> AdvanceTaaPixelJitter(uint32_t *frameCounter)
{
	if (!frameCounter) {
		return {0.0f, 0.0f};
	}
	const uint32_t index = *frameCounter;
	*frameCounter = (index + 1u) % 8u;
	/// \brief Halton(2,3) outputs [0, 1); remap to [-0.5, +0.5] so the projection-matrix
	///
	/// \details
	///  jitter sits within a single sub-pixel cell. The +0.5/W, +0.5/H offsets

	///  are applied by `BuildGraphicsPushConstants` when it scales the pixel

	///  offset into NDC; this helper stays in *pixel* units because that is the

	///  contract `VoxelSceneLighting.taaParams` documents in the header.

	const float hx = HaltonSequence(index, 2u) - 0.5f;
	const float hy = HaltonSequence(index, 3u) - 0.5f;
	return {hx, hy};
}

std::array<float, 4> BuildTaaHistoryParams(
	const VkExtent2D extent,
	const bool historyValid)
{
	if (extent.width == 0u || extent.height == 0u) {
		return {0.0f, 0.0f, historyValid ? 1.0f : 0.0f, 0.0f};
	}
	return {
		1.0f / static_cast<float>(extent.width),
		1.0f / static_cast<float>(extent.height),
		historyValid ? 1.0f : 0.0f,
		0.0f,
	};
}

/// \brief 1.5 anti-flicker:
///
/// \details
/// per-layer history parameters. Layout matches
///  `taaHistoryParams` (texelX, texelY, valid, blend) but the last

///  slot is the layer blend factor instead of the TAA neighbourhood

///  radius — those are independent. The layer history matches the

///  colour history's resolution (both render targets are allocated

///  at the swapchain extent), so the texel-size is the same.

std::array<float, 4> BuildTaaLayerHistoryParams(
	const VkExtent2D extent,
	const bool historyValid,
	const float blendFactor)
{
	if (extent.width == 0u || extent.height == 0u) {
		return {0.0f, 0.0f, historyValid ? 1.0f : 0.0f, blendFactor};
	}
	return {
		1.0f / static_cast<float>(extent.width),
		1.0f / static_cast<float>(extent.height),
		historyValid ? 1.0f : 0.0f,
		blendFactor,
	};
}

} // namespace projectv::taa
