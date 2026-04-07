#ifndef DEBUG_HUD_HPP
#define DEBUG_HUD_HPP

#include "core/Types.hpp"

uint32_t BuildDebugHudVertices(
	const DebugStats &stats,
	const CameraState &camera,
	const InteractionState &interaction,
	bool hudVisible,
	VkExtent2D extent,
	DebugHudVertex *outVertices,
	uint32_t maxVertexCount);

#endif
