#pragma once

#include "core/Types.hpp"

bool CreateVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render);
bool RefreshVoxelMeshingResourceBindings(
	VulkanContextState *context,
	RenderState *render);
void DestroyVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render);

