#ifndef PROJECTV_TAA_HPP
#define PROJECTV_TAA_HPP

#include "core/Types.hpp"

#include <cstdint>

namespace projectv::taa {

// 8-tap Halton(2,3) sub-pixel jitter sequence, in *pixel* units relative to
// the rasterization center (so the value is roughly `[-0.5, +0.5]` before the
// caller scales by the swapchain extent). The sequence is base-2/3 and has
// reasonable low-discrepancy properties for a 1-pixel neighbourhood, which is
// what TAA's 3x3 history clamp needs to converge cleanly.
//
// Returns the *previous* jitter, then advances the internal index by one. The
// caller is expected to call this once per frame; the first 8 calls cover
// the full 2x2 sub-pixel cell in a non-repeating, non-grid pattern.
std::array<float, 2> AdvanceTaaPixelJitter(uint32_t *frameCounter);

// Compute the (texel-size, validity) tuple stored in
// `VoxelSceneLighting.taaHistoryParams` for the TAA resolve pass. Caller must
// guarantee `extent.width` and `extent.height` are > 0; the function returns
// zero texel sizes if not.
std::array<float, 4> BuildTaaHistoryParams(
	const VkExtent2D extent,
	bool historyValid);

} // namespace projectv::taa

#endif
