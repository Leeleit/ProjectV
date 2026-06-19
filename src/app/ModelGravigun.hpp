#pragma once

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::app {

struct ModelGravigunState {
	int pickedInstanceIndex = -1;
	float targetY = 0.0f;
	glm::vec3 pickAnchorAabbMin{0.0f};
	glm::vec3 pickAnchorHit{0.0f};
};

void TickModelGravigun(
	ModelGravigunState *state,
	const VoxelWorld &world,
	const CameraState &camera,
	VkExtent2D extent,
	RenderState *render,
	InputState *input);

} // namespace projectv::app

