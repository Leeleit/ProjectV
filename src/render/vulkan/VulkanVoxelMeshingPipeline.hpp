#ifndef VULKAN_VOXEL_MESHING_PIPELINE_HPP
#define VULKAN_VOXEL_MESHING_PIPELINE_HPP

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

#endif
