#include "voxel/VoxelInteraction.hpp"

#include "app/Camera.hpp"
#include "voxel/VoxelRaycast.hpp"
#include "voxel/VoxelWorld.hpp"

namespace {
InteractionSelectionState BuildInteractionSelection(
	const VoxelRaycastHit &hit,
	const VoxelWorld *world)
{
	InteractionSelectionState selection{};
	selection.hasHit = hit.hasHit;
	selection.hasPlacementVoxel = hit.hasPlacementVoxel;
	selection.targetVoxel = hit.voxel;
	selection.placementVoxel = hit.placementVoxel;
	selection.hitNormal = hit.hitNormal;
	selection.targetMaterial = hit.material;
	selection.hitDistance = hit.distance;
	if (world && hit.hasHit && IsInsideVoxelWorld(*world, hit.voxel)) {
		const Int3 chunkCoord = GetVoxelChunkCoord(*world, hit.voxel);
		const auto &[min, maxExclusive, rebuildQueued, nonAirVoxelCount] = world->chunks[GetVoxelChunkIndex(*world, chunkCoord)];
		selection.hasTargetChunk = true;
		selection.targetChunkCoord = chunkCoord;
		selection.targetChunkMin = min;
		selection.targetChunkMaxExclusive = maxExclusive;
		selection.targetChunkDirty = rebuildQueued;
		selection.targetChunkActive = nonAirVoxelCount > 0;
	}
	return selection;
}

void ClearInteractionActions(InputState *input)
{
	if (!input) {
		return;
	}

	input->removePressed = false;
	input->placePressed = false;
}

bool ApplyClassicInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (input.removePressed && hit.hasHit) {
		SetVoxelMaterial(world, hit.voxel, VoxelMaterial::Air);
		return true;
	}

	if (input.placePressed &&
		hit.hasHit &&
		hit.hasPlacementVoxel &&
		GetVoxelMaterial(world, hit.placementVoxel) == VoxelMaterial::Air &&
		interaction.placementMaterial != VoxelMaterial::Air) {
		SetVoxelMaterial(world, hit.placementVoxel, interaction.placementMaterial);
		return true;
	}

	return false;
}

bool ApplyPaintInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (interaction.placementMaterial == VoxelMaterial::Air || !hit.hasHit) {
		return false;
	}

	if (input.removePressed &&
		GetVoxelMaterial(world, hit.voxel) != interaction.placementMaterial) {
		SetVoxelMaterial(world, hit.voxel, interaction.placementMaterial);
		return true;
	}

	if (input.placePressed &&
		hit.hasPlacementVoxel &&
		GetVoxelMaterial(world, hit.placementVoxel) == VoxelMaterial::Air) {
		SetVoxelMaterial(world, hit.placementVoxel, interaction.placementMaterial);
		return true;
	}

	return false;
}

bool ApplyEraseInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world)
{
	if ((input.removePressed || input.placePressed) && hit.hasHit) {
		SetVoxelMaterial(world, hit.voxel, VoxelMaterial::Air);
		return true;
	}

	return false;
}

bool ApplyFillInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (!(input.removePressed || input.placePressed) ||
		!hit.hasHit ||
		interaction.placementMaterial == VoxelMaterial::Air) {
		return false;
	}

	return FillVoxelMaterial(world, hit.voxel, interaction.placementMaterial) > 0;
}

bool ApplyEditorInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	switch (interaction.editorTool) {
	case DebugEditorTool::Classic:
		return ApplyClassicInteraction(hit, input, world, interaction);
	case DebugEditorTool::Paint:
		return ApplyPaintInteraction(hit, input, world, interaction);
	case DebugEditorTool::Erase:
		return ApplyEraseInteraction(hit, input, world);
	case DebugEditorTool::Fill:
		return ApplyFillInteraction(hit, input, world, interaction);
	case DebugEditorTool::Inspect:
		return false;
	}

	return false;
}
} // namespace

void HandleInteractionEvent(
	InputState *input,
	const SDL_Event *event)
{
	if (!input || !event || event->type != SDL_EVENT_MOUSE_BUTTON_DOWN) {
		return;
	}

	if (event->button.button == SDL_BUTTON_LEFT) {
		input->removePressed = true;
	} else if (event->button.button == SDL_BUTTON_RIGHT) {
		input->placePressed = true;
	}
}

void UpdateVoxelInteraction(
	const CameraState &camera,
	InputState *input,
	VoxelWorld *world,
	InteractionState *interaction,
	const bool allowEditing)
{
	if (!input || !interaction) {
		return;
	}

	interaction->selection = {};
	if (!world) {
		ClearInteractionActions(input);
		return;
	}

	const std::array<float, 3> direction = GetCameraForwardVector(camera);
	const auto raycast = [&] {
		return RaycastVoxelWorld(*world, camera.position, direction, interaction->maxInteractionDistance);
	};

	VoxelRaycastHit hit = raycast();
	interaction->selection = BuildInteractionSelection(hit, world);

	if (allowEditing && ApplyEditorInteraction(hit, *input, *world, *interaction)) {
		hit = raycast();
		interaction->selection = BuildInteractionSelection(hit, world);
	}

	ClearInteractionActions(input);
}
