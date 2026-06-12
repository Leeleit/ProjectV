#ifndef ASSET_MODEL_MANIFEST_LOADER_HPP
#define ASSET_MODEL_MANIFEST_LOADER_HPP

#include "core/Types.hpp"

namespace projectv::asset {

// Parses PROJECTV_MODELS env var, loads each .glb, bakes it via
// MeshBaker, and uploads the result to a device-local GPU buffer.
// On success the entry lands in `render->modelRegistry`. The
// `modelInstances` list (consumed by the model pass in Renderer.cpp)
// is rebuilt each frame from this registry + the camera in
// `BuildModelInstanceList`.
//
// `worldAabbMin` / `worldAabbMax` are the rotated, scaled, translated
// corners of each entry's source AABB. The frame preparation uses
// those for frustum culling; the M4 MVP path is conservative and
// emits every model that's loaded, so the world AABB is the
// identity-transformed source AABB. The M5 frustum cull pass will
// honour the manifest transform.
bool LoadAndRegisterModelsFromManifest(
	VulkanContextState *context,
	VkCommandPool commandPool,
	VkQueue queue,
	RenderState *render);

void UnloadAllModels(VulkanContextState *context, RenderState *render);

// Voxel-grid "ground snap" for the loaded `modelInstances`. The
// model loader at init time has no access to the voxel world, so
// the manifest's `position` field is taken at face value (treated
// as the model's center). After the voxel world is up, this pass
// lifts every model so its bottom is just above the top voxel
// under its AABB, and snaps the XZ position to the nearest integer
// voxel column. The result: `box.glb@0,1,0` in VoxelLab ends up
// sitting cleanly on top of the Y=0 floor voxel instead of half-
// submerged in it (the "half in textures" symptom reported
// 2026-06-12 after the M5.1b revert + spawn-position fix).
//
// Idempotent. Cheap (one `GetVoxelMaterial` probe per instance).
// Must be called after the world is loaded (post `SyncPhysicsWorld`
// in `FinalizeActiveVoxelWorldReload`) and after every world
// reload / preset switch. `modelInstances[].modelTransform` and
// `worldAabbMin/Max` are updated in place; the GPU descriptor set
// does not need to be re-bound (mat4 is uploaded per draw via
// push constants in `Renderer.cpp`).
void SnapModelInstancesAboveGround(
	const VoxelWorld &world,
	RenderState *render);

} // namespace projectv::asset

#endif
