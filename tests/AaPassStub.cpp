#include "core/Types.hpp"
#include "render/AaPass.hpp"

namespace projectv::render {

void InvalidateProgressiveAccum(RenderState &render)
{
	render.progressiveAccumFrameIndex = 0u;
	render.progressiveAccumHistoryValid = false;
	render.progressiveAccumUpdateThisFrame = false;
	render.progressiveAccumApplyHalton = false;
	render.progressiveHaltonNdcX = 0.0f;
	render.progressiveHaltonNdcY = 0.0f;
	render.progressiveAccumPrevCameraValid = false;
	render.progressiveAccumPrevLightingValid = false;
}

} // namespace projectv::render
