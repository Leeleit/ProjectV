#include "debug/DebugOverlays.hpp"

#include "app/Camera.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr std::array kSelectionOverlayColor{1.0f, 0.82f, 0.22f, 0.95f};
constexpr std::array kPlacementOverlayColor{0.28f, 0.94f, 0.54f, 0.88f};
constexpr std::array kMutationAnchorOverlayColor{0.96f, 0.56f, 0.18f, 0.92f};
constexpr std::array kMutationPreviewOverlayColor{0.42f, 0.92f, 0.72f, 0.26f};
constexpr std::array kInspectChunkOverlayColor{0.27f, 0.87f, 1.0f, 0.90f};
constexpr std::array kChunkBoundsOverlayColor{0.16f, 0.52f, 0.95f, 0.32f};
constexpr std::array kDirtyChunkOverlayColor{1.0f, 0.33f, 0.16f, 0.78f};
// 5.2 debug gizmos. Cascade split planes get four distinct hues
// (red, orange, cyan, magenta) so the operator can tell cascade
// 0/1/2/3 apart at a glance. The cursor hit normal is a single
// dim-white shaft so it doesn't compete with the yellow selection
// box.
constexpr std::array<std::array<float, 4>, kSunShadowCascadeCount> kCascadeSplitPlaneColors = {{
	{0.95f, 0.20f, 0.20f, 0.55f},
	{0.96f, 0.62f, 0.18f, 0.55f},
	{0.30f, 0.85f, 0.95f, 0.55f},
	{0.85f, 0.40f, 0.95f, 0.55f},
}};
constexpr std::array kCursorHitNormalOverlayColor{0.92f, 0.92f, 0.92f, 0.70f};
constexpr float kCascadeSplitPlaneThinVoxels = 0.10f;
constexpr float kCascadeSplitPlaneSizePadding = 4.0f;
constexpr int32_t kCursorHitNormalShaftLength = 2;

void AppendOverlayBox(
	std::vector<DebugOverlayBox> &outBoxes,
	const Int3 min,
	const Int3 maxExclusive,
	const std::array<float, 4> &color)
{
	outBoxes.push_back({
		.min = min,
		.maxExclusive = maxExclusive,
		.color = color,
	});
}

size_t CountDirtyChunkOverlays(const VoxelWorld &world)
{
	size_t dirtyChunkCount = 0;
	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.rebuildQueued) {
			++dirtyChunkCount;
		}
	}

	return dirtyChunkCount;
}

bool TryGetMutationPreviewVoxel(const InteractionState &interaction, Int3 &outVoxel)
{
	if (interaction.mutationAnchorUsesPlacementVoxel) {
		if (!interaction.selection.hasPlacementVoxel) {
			return false;
		}
		outVoxel = interaction.selection.placementVoxel;
		return true;
	}

	if (!interaction.selection.hasHit) {
		return false;
	}
	outVoxel = interaction.selection.targetVoxel;
	return true;
}

Int3 MakeVoxelMaxExclusive(const Int3 voxel)
{
	return {
		voxel.x + 1,
		voxel.y + 1,
		voxel.z + 1,
	};
}

void AppendVoxelOverlayBox(
	std::vector<DebugOverlayBox> &outBoxes,
	const Int3 voxel,
	const std::array<float, 4> &color)
{
	AppendOverlayBox(outBoxes, voxel, MakeVoxelMaxExclusive(voxel), color);
}

void AppendMutationPreviewOverlayBox(
	std::vector<DebugOverlayBox> &outBoxes,
	const Int3 first,
	const Int3 second)
{
	const Int3 min{
		std::min(first.x, second.x),
		std::min(first.y, second.y),
		std::min(first.z, second.z),
	};
	const Int3 maxExclusive{
		std::max(first.x, second.x) + 1,
		std::max(first.y, second.y) + 1,
		std::max(first.z, second.z) + 1,
	};
	AppendOverlayBox(outBoxes, min, maxExclusive, kMutationPreviewOverlayColor);
}

// 5.2 debug gizmo: draw 4 axis-aligned "split plane" boxes — one
// per CSM cascade — at the camera's `viewDepthSplits[i]` distance
// along the camera forward vector. The box XZ size is taken from
// the cascade's ortho width/height (so the operator gets a
// "shadow frustum footprint" cue on the ground plane), and the
// Y extent is one voxel. Caveat: `DebugOverlayBox` is
// axis-aligned, so the box is world-aligned, not camera-aligned.
// This is a deliberate trade-off — the visual cue is "somewhere
// in this XY column, the cascade transition happens at this
// depth", which is enough to tune split lambda by eye.
void AppendCascadeSplitPlaneOverlayBoxes(
	std::vector<DebugOverlayBox> &outBoxes,
	const CameraState &camera,
	const RenderState &render)
{
	const std::array<float, 3> forward = GetCameraForwardVector(camera);
	const auto &splits = render.currentSunShadowCascadeSplits.viewDepthSplits;
	const auto &orthoWidths = render.currentSunShadowCascadeDiagnostics.orthoWidths;
	const auto &orthoHeights = render.currentSunShadowCascadeDiagnostics.orthoHeights;

	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const float splitDepth = splits[cascadeIndex];
		if (splitDepth <= 0.0f) {
			continue;
		}

		// World-space position: camera eye + splitDepth * forward.
		// Floored to Int3 for the box min.
		const float worldX = camera.position[0] + forward[0] * splitDepth;
		const float worldY = camera.position[1] + forward[1] * splitDepth;
		const float worldZ = camera.position[2] + forward[2] * splitDepth;

		const float halfWidth = std::max(orthoWidths[cascadeIndex] * 0.5f, 1.0f)
			+ kCascadeSplitPlaneSizePadding;
		const float halfHeight = std::max(orthoHeights[cascadeIndex] * 0.5f, 1.0f)
			+ kCascadeSplitPlaneSizePadding;

		// AABB on the world XZ plane, centered at (worldX, _, worldZ),
		// thin in Y. We pick a thin slab around the camera-relative
		// Y so the box stays visible from any camera angle (it's
		// not a true camera-aligned frustum, but the XZ footprint
		// matches the cascade's ortho extent which is the useful
		// diagnostic for split tuning).
		const Int3 min{
			static_cast<int32_t>(std::floor(worldX - halfWidth)),
			static_cast<int32_t>(std::floor(worldY - kCascadeSplitPlaneThinVoxels)),
			static_cast<int32_t>(std::floor(worldZ - halfHeight)),
		};
		const Int3 maxExclusive{
			static_cast<int32_t>(std::ceil(worldX + halfWidth)) + 1,
			static_cast<int32_t>(std::ceil(worldY + kCascadeSplitPlaneThinVoxels)) + 1,
			static_cast<int32_t>(std::ceil(worldZ + halfHeight)) + 1,
		};
		AppendOverlayBox(
			outBoxes,
			min,
			maxExclusive,
			kCascadeSplitPlaneColors[cascadeIndex]);
	}
}

// 5.2 debug gizmo: draw a 1-voxel-wide shaft along the cursor
// hit normal (an axis-aligned Int3 in {-1, 0, 1}) for 2 voxels.
// Helps disambiguate "which face is selected" on top-down or
// side-on views where the yellow selection box alone can be
// confusing.
void AppendCursorHitNormalOverlayBox(
	std::vector<DebugOverlayBox> &outBoxes,
	const Int3 targetVoxel,
	const Int3 hitNormal)
{
	// hitNormal is guaranteed to be a unit axis by VoxelRaycast
	// (one component is ±1, others 0). Build a thin box from
	// targetVoxel to targetVoxel + normal * length.
	if (hitNormal.x == 0 && hitNormal.y == 0 && hitNormal.z == 0) {
		return;
	}

	const Int3 shaftEnd{
		targetVoxel.x + hitNormal.x * kCursorHitNormalShaftLength,
		targetVoxel.y + hitNormal.y * kCursorHitNormalShaftLength,
		targetVoxel.z + hitNormal.z * kCursorHitNormalShaftLength,
	};

	// Shaft: a 1x1x1 box at each voxel along the normal. Avoid
	// the yellow selection box overlap by emitting the shaft
	// only beyond the hit voxel (the selection box already
	// covers the hit voxel itself).
	for (int32_t step = 1; step <= kCursorHitNormalShaftLength; ++step) {
		const Int3 voxel{
			targetVoxel.x + hitNormal.x * step,
			targetVoxel.y + hitNormal.y * step,
			targetVoxel.z + hitNormal.z * step,
		};
		AppendVoxelOverlayBox(outBoxes, voxel, kCursorHitNormalOverlayColor);
	}

	// Suppress the unused-shaftEnd warning: kept for
	// documentation of where the shaft conceptually ends.
	(void)shaftEnd;
}
} // namespace

void BuildDebugOverlayBoxes(
	const VoxelWorld *world,
	const InteractionState &interaction,
	const DebugState &debug,
	std::vector<DebugOverlayBox> *outBoxes,
	const CameraState &camera,
	const RenderState &render)
{
	if (!outBoxes) {
		return;
	}

	outBoxes->clear();
	if (!debug.hudVisible || !world) {
		return;
	}

	const bool detailedHudVisible = debug.detailedHudVisible;
	size_t requiredBoxCount = interaction.selection.hasHit ? 1u : 0u;
	if (detailedHudVisible && interaction.selection.hasPlacementVoxel) {
		++requiredBoxCount;
	}
	if (detailedHudVisible && interaction.mutationAnchorValid) {
		++requiredBoxCount;
	}
	if (detailedHudVisible && interaction.mutationAnchorValid) {
		Int3 previewVoxel{};
		if (TryGetMutationPreviewVoxel(interaction, previewVoxel)) {
			++requiredBoxCount;
		}
	}
	if (detailedHudVisible &&
		interaction.editorTool == DebugEditorTool::Inspect &&
		interaction.selection.hasTargetChunk) {
		++requiredBoxCount;
	}
	if (debug.showChunkBounds) {
		requiredBoxCount += world->chunks.size();
	}
	if (debug.showDirtyChunkOverlay) {
		requiredBoxCount += CountDirtyChunkOverlays(*world);
	}
	// 5.2 gizmos: 4 cascade split plane boxes + 1 cursor hit normal
	// shaft (≤2 boxes). They emit only when the corresponding flag
	// is on; the flag is keyed by `L` / `Z` so a clean run with no
	// keypress allocates zero extra boxes.
	if (debug.showCascadeSplitPlanes) {
		requiredBoxCount += kSunShadowCascadeCount;
	}
	if (debug.showCursorHitNormal && interaction.selection.hasHit) {
		requiredBoxCount += kCursorHitNormalShaftLength;
	}
	if (outBoxes->capacity() < requiredBoxCount) {
		outBoxes->reserve(requiredBoxCount);
	}

	if (debug.showChunkBounds) {
		for (const VoxelChunk &chunk : world->chunks) {
			AppendOverlayBox(
				*outBoxes,
				chunk.min,
				chunk.maxExclusive,
				kChunkBoundsOverlayColor);
		}
	}

	if (debug.showDirtyChunkOverlay) {
		for (const VoxelChunk &chunk : world->chunks) {
			if (!chunk.rebuildQueued) {
				continue;
			}

			AppendOverlayBox(
				*outBoxes,
				chunk.min,
				chunk.maxExclusive,
				kDirtyChunkOverlayColor);
		}
	}

	// 5.2 cascade split plane overlay. Emit before the selection
	// box so the yellow selection box (when present) wins Z-test
	// for ties against the dimmer cascade boxes.
	if (debug.showCascadeSplitPlanes) {
		AppendCascadeSplitPlaneOverlayBoxes(*outBoxes, camera, render);
	}

	if (interaction.selection.hasHit) {
		AppendVoxelOverlayBox(*outBoxes, interaction.selection.targetVoxel, kSelectionOverlayColor);
		// 5.2 cursor hit normal shaft. Emit *after* the selection
		// box so the dim-white shaft reads as a "next to selection"
		// arrow, not as a replacement marker.
		if (debug.showCursorHitNormal) {
			AppendCursorHitNormalOverlayBox(
				*outBoxes,
				interaction.selection.targetVoxel,
				interaction.selection.hitNormal);
		}
	}

	if (detailedHudVisible && interaction.selection.hasPlacementVoxel) {
		AppendVoxelOverlayBox(*outBoxes, interaction.selection.placementVoxel, kPlacementOverlayColor);
	}

	if (detailedHudVisible && interaction.mutationAnchorValid) {
		AppendVoxelOverlayBox(*outBoxes, interaction.mutationAnchorVoxel, kMutationAnchorOverlayColor);
		Int3 previewVoxel{};
		if (TryGetMutationPreviewVoxel(interaction, previewVoxel)) {
			AppendMutationPreviewOverlayBox(*outBoxes, interaction.mutationAnchorVoxel, previewVoxel);
		}
	}

	if (detailedHudVisible &&
		interaction.editorTool == DebugEditorTool::Inspect &&
		interaction.selection.hasTargetChunk) {
		AppendOverlayBox(
			*outBoxes,
			interaction.selection.targetChunkMin,
			interaction.selection.targetChunkMaxExclusive,
			kInspectChunkOverlayColor);
	}
}
