#include "debug/DebugOverlays.hpp"

#include "voxel/VoxelWorld.hpp"

namespace {
constexpr std::array<float, 4> kSelectionOverlayColor{1.0f, 0.82f, 0.22f, 0.95f};
constexpr std::array<float, 4> kInspectChunkOverlayColor{0.27f, 0.87f, 1.0f, 0.90f};
constexpr std::array<float, 4> kChunkBoundsOverlayColor{0.16f, 0.52f, 0.95f, 0.32f};
constexpr std::array<float, 4> kDirtyChunkOverlayColor{1.0f, 0.33f, 0.16f, 0.78f};

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

	size_t requiredBoxCount = interaction.selection.hasHit ? 1u : 0u;
	if (interaction.editorTool == DebugEditorTool::Inspect &&
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
		AppendOverlayBox(
			*outBoxes,
			interaction.selection.targetVoxel,
			{
				interaction.selection.targetVoxel.x + 1,
				interaction.selection.targetVoxel.y + 1,
				interaction.selection.targetVoxel.z + 1,
			},
			kSelectionOverlayColor);
	}

	if (interaction.editorTool == DebugEditorTool::Inspect &&
		interaction.selection.hasTargetChunk) {
		AppendOverlayBox(
			*outBoxes,
			interaction.selection.targetChunkMin,
			interaction.selection.targetChunkMaxExclusive,
			kInspectChunkOverlayColor);
	}
}
