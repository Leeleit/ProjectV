#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

namespace projectv::taa {


std::array<float, 2> AdvanceTaaPixelJitter(uint32_t *frameCounter);


std::array<float, 4> BuildTaaHistoryParams(
	VkExtent2D extent,
	bool historyValid);


std::array<float, 4> BuildTaaLayerHistoryParams(
	VkExtent2D extent,
	bool historyValid,
	float blendFactor);

} // namespace projectv::taa

