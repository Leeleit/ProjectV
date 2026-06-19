#pragma once

#include "core/Types.hpp"

namespace projectv::asset {


bool LoadAndRegisterModelsFromManifest(
	VulkanContextState *context,
	VkCommandPool commandPool,
	VkQueue queue,
	RenderState *render);

void UnloadAllModels(VulkanContextState *context, RenderState *render);

void SnapModelInstancesAboveGround(
	const VoxelWorld &world,
	RenderState *render);

void SnapModelInstancesCenterAnchored(
	const VoxelWorld &world,
	RenderState *render);

void SnapModelInstancesAboveGroundDispatch(
	const VoxelWorld &world,
	RenderState *render);

} // namespace projectv::asset

