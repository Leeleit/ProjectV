#pragma once

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
// as the model's AABB min — the voxel corner directly under the
// model — since the M5.1c semantic change, 2026-06-12). After the
// voxel world is up, this pass does two things:
//
//  1. **Per-axis auto-scale** to integer voxel dimensions
//     (`targetDim_i = max(1, round(srcDim_i))`) so the model's
//     footprint covers exactly N×M×P voxel columns — the
//     "автоматическое изменение размера, чтобы длина или ширина
//     были кратны длинам вокселей" contract from the M5.1b
//     follow-up.
//
//  2. **Corner snap**: lifts the model so its AABB min lands on
//     `topVoxelY + 1` (Y, the top of the floor voxel column) and
//     rounds the XZ AABB min to the nearest integer — the
//     "начало на углу вокселя под моделькой" rule. For an
//     integer-valued manifest `position` (e.g. `box.glb@0,1,0`
//     or `Untitled.colonada.glb@-9,0,9`) the XZ corner snap is a
//     no-op and the operator's coordinate is preserved verbatim.
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

// **M5.1d, 2026-06-12: centred snap (opt-in).** Same
// clamp-to-world + snap-to-grid contract as the default
// `SnapModelInstancesAboveGround`, but the snap anchor is
// the model's AABB **centre** (the geometric middle of the
// model) rather than the AABB min (the bottom-left corner).
// Lets the operator position a model by its centre voxel for
// cases where the centre is the natural reference (floating
// decorations, skybox elements, furniture-style objects).
//
// **Not the default.** The M5.1b/M5.1d convention is
// `position = AABB min` (bottom-anchored), which is the right
// choice for objects that sit on the floor — operators read
// the manifest `position` as "the corner of the voxel under
// the model". The centred snap exists for the cases where
// the operator wants "the centre of the model at this point".
// The caller (e.g. a future "centre placement" mode) opts in
// by calling this function *instead of*
// `SnapModelInstancesAboveGround`.
//
// **Implementation:** the function re-anchors the AABB so
// its centre is at the integer voxel grid (the X/Y/Z of the
// centre are rounded to nearest integer), then clamps the
// AABB to fit within `world.min` and `world.maxExclusive -
// dim`. The AABB dims are preserved. The model basis
// translation column is updated to match the new AABB min
// (the load path stores the translation as the AABB min in
// world space, which is what the renderer expects).
//
// **Idempotent.** Calling it twice is the same as calling
// it once. Both snaps (centred and bottom-anchored) can be
// applied in either order; the result is the same
// integer-grid AABB clamped to the world bounds, just
// anchored at a different reference point.
void SnapModelInstancesCenterAnchored(
	const VoxelWorld &world,
	RenderState *render);

// **M5.1d, 2026-06-12: snap dispatch wrapper.** Reads the
// env var `PROJECTV_MODEL_SNAP` and dispatches to the
// appropriate snap:
//   - `PROJECTV_MODEL_SNAP=centre` → `SnapModelInstancesCenterAnchored`
//     (centre-anchored: the operator's `position` becomes the
//     integer AABB centre)
//   - any other value or unset → `SnapModelInstancesAboveGround`
//     (bottom-anchored: the operator's `position` is the
//     integer AABB min, the corner under the model — the
//     M5.1b/M5.1d default)
// Both snaps are then applied in sequence so the AABB
// ends up clamped to the world bounds AND on the integer
// grid, regardless of which anchor the operator picks.
void SnapModelInstancesAboveGroundDispatch(
	const VoxelWorld &world,
	RenderState *render);

} // namespace projectv::asset

