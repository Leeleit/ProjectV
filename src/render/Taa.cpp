#include "render/Taa.hpp"

#include <array>

namespace projectv::taa {

namespace {

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
