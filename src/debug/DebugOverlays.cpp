#include "debug/DebugOverlays.hpp"

#include "voxel/VoxelWorld.hpp"

#include <algorithm>

namespace {
constexpr std::array kSelectionOverlayColor{1.0f, 0.82f, 0.22f, 0.95f};
constexpr std::array kPlacementOverlayColor{0.28f, 0.94f, 0.54f, 0.88f};
constexpr std::array kMutationAnchorOverlayColor{0.96f, 0.56f, 0.18f, 0.92f};
constexpr std::array kMutationPreviewOverlayColor{0.42f, 0.92f, 0.72f, 0.26f};
constexpr std::array kInspectChunkOverlayColor{0.27f, 0.87f, 1.0f, 0.90f};
constexpr std::array kChunkBoundsOverlayColor{0.16f, 0.52f, 0.95f, 0.32f};
constexpr std::array kDirtyChunkOverlayColor{1.0f, 0.33f, 0.16f, 0.78f};

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
} // namespace

void BuildDebugOverlayBoxes(
	const VoxelWorld *world,
	const InteractionState &interaction,
	const DebugState &debug,
	std::vector<DebugOverlayBox> *outBoxes)
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

	if (interaction.selection.hasHit) {
		AppendVoxelOverlayBox(*outBoxes, interaction.selection.targetVoxel, kSelectionOverlayColor);
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
