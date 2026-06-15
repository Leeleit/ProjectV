#pragma once

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::app {

// **M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
// style "physics gun" for interactively positioning loaded glTF
// models in the VoxelLab world. Lets the operator see exactly
// which integer voxel coordinates a model snaps to when they
// drag it around with the crosshair, so the snap math in
// `SnapModelInstancesAboveGround` (or its M5.1c successor) can
// be debugged by eye and by reading the coordinates out of the
// smoke log.
//
// **Contract:**
//
//   - **F (pick/drop).** Tap F while looking at a loaded
//     model: the closest model whose AABB intersects the
//     camera ray is "picked" and logged as
//     `[Gravigun-DBG] PICKED: <id> aabbMin=(x,y,z)`. While
//     F is held, the picked model is anchored to the
//     crosshair and its AABB min is snapped to the integer
//     voxel grid on a horizontal plane (default Y=0, the
//     bottom of the VoxelLab floor voxel — see the "Y offset"
//     note below). The AABB dims are preserved across drag
//     — only the AABB min and the translation column of the
//     model basis are updated, so the model keeps its shape.
//     Release F: the model is dropped at the current snap
//     position, and the log records
//     `[Gravigun-DBG] DROPPED: <id> final_aabbMin=(x,y,z)`.
//
// **Y offset, 2026-06-12:** the first gravigun prototype used
// `targetY=1.0` (the top of the VoxelLab floor voxel), which
// lifted the model's AABB min by 1 voxel relative to the
// `position.y` value the operator had specified in the
// manifest. The operator's manifest convention is
// `position = AABB min in world space` (no implicit lift):
// `position.y=0` means the AABB min is at world Y=0 (the
// bottom of the floor voxel — the model's base is embedded
// in the floor), and `position.y=1` puts the base on top of
// the floor. With the fix, the gravigun respects this
// convention by starting at `targetY=0`. Future work could
// query the voxel world for the top of the floor voxel under
// the cursor's XZ and project to that plane instead, for
// "sit on the floor" behaviour without forcing the operator
// to think in `position.y` offsets.
//
// All gravigun work happens in `TickModelGravigun`, which is
// called once per frame from `PrepareFrameRenderData`. The
// state struct is small and stack-allocated by the frame
// prep; the only mutable state is the picked instance index
// and the target Y.
//
// **Notes for future work:**
//
//   - The current implementation re-derives the AABB from
//     the source positions (which are stored in
//     `loaded->primitives` after node-hierarchy walk) rather
//     than rotating the rendered vertex buffer. This means
//     the AABB is slightly conservative for rotated models
//     (uses the un-rotated source AABB) but the rendered
//     mesh is correctly rotated. For voxel-grid snapping
//     that's the right trade-off — the AABB only needs to
//     be "axis-aligned envelope", not pixel-perfect.
struct ModelGravigunState {
	int pickedInstanceIndex = -1;
	// Horizontal plane the cursor projects onto during drag.
	// **Y offset, 2026-06-12:** default changed from 1.0
	// (top of the floor voxel — implicit +1-voxel lift) to
	// 0.0 (bottom of the floor voxel) to honour the manifest
	// convention `position = AABB min`. Operators who want
	// the model's base to sit on top of the floor should set
	// `position.y=1` in the manifest; the gravigun then
	// carries that Y through and drops the model at AABB
	// min.y=1. Operators who want the model embedded in
	// the floor (the lamp-post look) leave `position.y=0`.
	float targetY = 0.0f;
	// **Pick anchor, 2026-06-12:** the AABB min of the
	// picked model at the moment F was pressed, and the
	// cursor's ground-plane hit at the same instant. The
	// drag then computes `newMin = pickAnchorAabbMin +
	// (currentHit - pickAnchorHit)`, so the model only
	// moves when the cursor moves relative to the pick
	// position — not on the first frame of F-held, where
	// the previous implementation snapped the AABB min
	// to `round(crosshair)` and teleported the model away
	// from the cursor (e.g. picking a column whose
	// visual centre is between voxels could round to the
	// nearest integer and shift the model by half a voxel
	// the moment F was pressed).
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

