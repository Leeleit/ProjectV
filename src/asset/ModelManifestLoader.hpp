#pragma once

#include "core/Types.hpp"

namespace projectv::asset {

/// \brief Parses PROJECTV_MODELS env var, loads each .glb, bakes it via
///
/// \details
///  MeshBaker, and uploads the result to a device-local GPU buffer.

///  On success the entry lands in `render->modelRegistry`. The

///  `modelInstances` list (consumed by the model pass in Renderer.cpp)

///  is rebuilt each frame from this registry + the camera in

///  `BuildModelInstanceList`.

///  `worldAabbMin` / `worldAabbMax` are the rotated, scaled, translated

///  corners of each entry's source AABB. The frame preparation uses

///  those for frustum culling; the M4 MVP path is conservative and

///  emits every model that's loaded, so the world AABB is the

///  identity-transformed source AABB. The M5 frustum cull pass will

///  honour the manifest transform.

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

