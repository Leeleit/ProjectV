#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

bool CreateVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render);
bool RefreshVoxelMeshingResourceBindings(
	VulkanContextState *context,
	RenderState *render);
void DestroyVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render);

