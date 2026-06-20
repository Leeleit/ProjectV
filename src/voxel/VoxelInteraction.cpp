#include "voxel/VoxelInteraction.hpp"

#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelRaycast.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>

namespace {
bool IsSolidInteractionMaterial(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
	case VoxelMaterial::FloorWhite:
	case VoxelMaterial::FloorGray:
		return true;
	case VoxelMaterial::Air:
	case VoxelMaterial::Fluid:
		return false;
	}

	return false;
}

void PopulateChunkSelectionInfo(
	const VoxelWorld &world,
	const Int3 voxel,
	bool &outHasChunk,
	Int3 &outChunkCoord,
	Int3 &outChunkMin,
	Int3 &outChunkMaxExclusive,
	bool &outChunkDirty,
	bool &outChunkActive,
	uint32_t &outChunkIndex,
	uint32_t &outChunkNonAirVoxelCount,
	Int3 &outVoxelInChunk)
{
	if (!IsInsideVoxelWorld(world, voxel)) {
		return;
	}

	const Int3 chunkCoord = GetVoxelChunkCoord(world, voxel);
	const size_t chunkIndex = GetVoxelChunkIndex(world, chunkCoord);
	const auto &[min, maxExclusive, rebuildQueued, isStatic, nonAirVoxelCount, ticksSinceLastEdit, lodLevel, reserved0, reserved1, reserved2] = world.chunks[chunkIndex];
	outHasChunk = true;
	outChunkCoord = chunkCoord;
	outChunkMin = min;
	outChunkMaxExclusive = maxExclusive;
	outChunkDirty = rebuildQueued;
	outChunkActive = nonAirVoxelCount > 0;
	outChunkIndex = static_cast<uint32_t>(chunkIndex);
	outChunkNonAirVoxelCount = nonAirVoxelCount;
	outVoxelInChunk = {
		voxel.x - min.x,
		voxel.y - min.y,
		voxel.z - min.z,
	};
}

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
	selection.targetSolid = IsSolidInteractionMaterial(hit.material);
	if (world && hit.hasHit && IsInsideVoxelWorld(*world, hit.voxel)) {
		PopulateChunkSelectionInfo(
			*world,
			hit.voxel,
			selection.hasTargetChunk,
			selection.targetChunkCoord,
			selection.targetChunkMin,
			selection.targetChunkMaxExclusive,
			selection.targetChunkDirty,
			selection.targetChunkActive,
			selection.targetChunkIndex,
			selection.targetChunkNonAirVoxelCount,
			selection.targetVoxelInChunk);
	}
	if (world && hit.hasPlacementVoxel && IsInsideVoxelWorld(*world, hit.placementVoxel)) {
		PopulateChunkSelectionInfo(
			*world,
			hit.placementVoxel,
			selection.hasPlacementChunk,
			selection.placementChunkCoord,
			selection.placementChunkMin,
			selection.placementChunkMaxExclusive,
			selection.placementChunkDirty,
			selection.placementChunkActive,
			selection.placementChunkIndex,
			selection.placementChunkNonAirVoxelCount,
			selection.placementVoxelInChunk);
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

bool CanPlaceInteractionVoxel(
	const VoxelRaycastHit &hit,
	const CameraState &camera,
	const PhysicsState *physics)
{
	return hit.hasPlacementVoxel &&
		   !DoesPhysicsCharacterOverlapVoxel(physics, camera, hit.placementVoxel);
}

bool CanPlaceInteractionVoxelBox(
	const Int3 first,
	const Int3 second,
	const CameraState &camera,
	const PhysicsState *physics)
{
	const Int3 min{
		std::min(first.x, second.x),
		std::min(first.y, second.y),
		std::min(first.z, second.z),
	};
	const Int3 max{
		std::max(first.x, second.x),
		std::max(first.y, second.y),
		std::max(first.z, second.z),
	};
	for (int z = min.z; z <= max.z; ++z) {
		for (int y = min.y; y <= max.y; ++y) {
			for (int x = min.x; x <= max.x; ++x) {
				if (DoesPhysicsCharacterOverlapVoxel(physics, camera, {x, y, z})) {
					return false;
				}
			}
		}
	}

	return true;
}

bool TryGetDefaultMutationAnchor(
	const InteractionSelectionState &selection,
	const DebugEditorTool tool,
	Int3 &outVoxel,
	bool &outUsesPlacementVoxel)
{
	switch (tool) {
	case DebugEditorTool::Erase:
		if (!selection.hasHit) {
			return false;
		}
		outVoxel = selection.targetVoxel;
		outUsesPlacementVoxel = false;
		return true;
	case DebugEditorTool::Classic:
	case DebugEditorTool::Paint:
	case DebugEditorTool::Fill:
	case DebugEditorTool::Inspect:
		if (selection.hasPlacementVoxel) {
			outVoxel = selection.placementVoxel;
			outUsesPlacementVoxel = true;
			return true;
		}
		if (!selection.hasHit) {
			return false;
		}
		outVoxel = selection.targetVoxel;
		outUsesPlacementVoxel = false;
		return true;
	}

	return false;
}

void UpdateMutationAnchorState(InputState &input, InteractionState &interaction)
{
	if (!ConsumeInputActionPressed(input, InputAction::ToggleMutationAnchor)) {
		return;
	}

	Int3 anchorVoxel{};
	bool usesPlacementVoxel = false;
	if (!TryGetDefaultMutationAnchor(interaction.selection, interaction.editorTool, anchorVoxel, usesPlacementVoxel)) {
		interaction.mutationAnchorValid = false;
		interaction.mutationAnchorVoxel = {};
		interaction.mutationAnchorUsesPlacementVoxel = false;
		return;
	}

	const bool sameAnchor =
		interaction.mutationAnchorValid &&
		interaction.mutationAnchorUsesPlacementVoxel == usesPlacementVoxel &&
		interaction.mutationAnchorVoxel.x == anchorVoxel.x &&
		interaction.mutationAnchorVoxel.y == anchorVoxel.y &&
		interaction.mutationAnchorVoxel.z == anchorVoxel.z;
	if (sameAnchor) {
		interaction.mutationAnchorValid = false;
		interaction.mutationAnchorVoxel = {};
		interaction.mutationAnchorUsesPlacementVoxel = false;
		return;
	}

	interaction.mutationAnchorValid = true;
	interaction.mutationAnchorVoxel = anchorVoxel;
	interaction.mutationAnchorUsesPlacementVoxel = usesPlacementVoxel;
}

void RefreshPickedPlacementMaterial(
	InputState &input,
	const VoxelRaycastHit &hit,
	InteractionState &interaction)
{
	if (ConsumeInputActionPressed(input, InputAction::PickTargetMaterial) &&
		hit.hasHit) {
		interaction.placementMaterial = hit.material;
	}
}

bool ApplyAnchoredPaintInteraction(
	const VoxelRaycastHit &hit,
	const CameraState &camera,
	const InputState &input,
	const PhysicsState *physics,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (!interaction.mutationAnchorValid) {
		return false;
	}

	if (input.removePressed &&
		!interaction.mutationAnchorUsesPlacementVoxel) {
		return FillVoxelBox(world, interaction.mutationAnchorVoxel, hit.voxel, interaction.placementMaterial) > 0;
	}

	if (input.placePressed &&
		interaction.mutationAnchorUsesPlacementVoxel &&
		hit.hasPlacementVoxel &&
		CanPlaceInteractionVoxelBox(interaction.mutationAnchorVoxel, hit.placementVoxel, camera, physics)) {
		return FillVoxelBox(world, interaction.mutationAnchorVoxel, hit.placementVoxel, interaction.placementMaterial) > 0;
	}

	return false;
}

bool ApplyAnchoredEraseInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (!interaction.mutationAnchorValid ||
		interaction.mutationAnchorUsesPlacementVoxel ||
		!(input.removePressed || input.placePressed) ||
		!hit.hasHit) {
		return false;
	}

	return FillVoxelBox(world, interaction.mutationAnchorVoxel, hit.voxel, VoxelMaterial::Air) > 0;
}

bool ApplyClassicInteraction(
	const VoxelRaycastHit &hit,
	const CameraState &camera,
	const InputState &input,
	const PhysicsState *physics,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (input.removePressed && hit.hasHit) {
		SetVoxelMaterial(world, hit.voxel, VoxelMaterial::Air, const_cast<PhysicsState *>(physics));
		return true;
	}

	if (input.placePressed &&
		hit.hasHit &&
		CanPlaceInteractionVoxel(hit, camera, physics) &&
		GetVoxelMaterial(world, hit.placementVoxel) == VoxelMaterial::Air &&
		interaction.placementMaterial != VoxelMaterial::Air) {
		SetVoxelMaterial(world, hit.placementVoxel, interaction.placementMaterial, const_cast<PhysicsState *>(physics));
		return true;
	}

	return false;
}

bool ApplyPaintInteraction(
	const VoxelRaycastHit &hit,
	const CameraState &camera,
	const InputState &input,
	const PhysicsState *physics,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (interaction.placementMaterial == VoxelMaterial::Air || !hit.hasHit) {
		return false;
	}

	if (ApplyAnchoredPaintInteraction(hit, camera, input, physics, world, interaction)) {
		return true;
	}

	if (input.removePressed &&
		GetVoxelMaterial(world, hit.voxel) != interaction.placementMaterial) {
		SetVoxelMaterial(world, hit.voxel, interaction.placementMaterial, const_cast<PhysicsState *>(physics));
		return true;
	}

	if (input.placePressed &&
		CanPlaceInteractionVoxel(hit, camera, physics) &&
		GetVoxelMaterial(world, hit.placementVoxel) == VoxelMaterial::Air) {
		SetVoxelMaterial(world, hit.placementVoxel, interaction.placementMaterial, const_cast<PhysicsState *>(physics));
		return true;
	}

	return false;
}

bool ApplyEraseInteraction(
	const VoxelRaycastHit &hit,
	const InputState &input,
	const PhysicsState *physics,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	if (ApplyAnchoredEraseInteraction(hit, input, world, interaction)) {
		return true;
	}

	if ((input.removePressed || input.placePressed) && hit.hasHit) {
		SetVoxelMaterial(world, hit.voxel, VoxelMaterial::Air, const_cast<PhysicsState *>(physics));
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
	const CameraState &camera,
	const InputState &input,
	const PhysicsState *physics,
	VoxelWorld &world,
	const InteractionState &interaction)
{
	switch (interaction.editorTool) {
	case DebugEditorTool::Classic:
		return ApplyClassicInteraction(hit, camera, input, physics, world, interaction);
	case DebugEditorTool::Paint:
		return ApplyPaintInteraction(hit, camera, input, physics, world, interaction);
	case DebugEditorTool::Erase:
		return ApplyEraseInteraction(hit, input, physics, world, interaction);
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
	const bool allowEditing,
	const PhysicsState *physics)
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
	const std::array<float, 3> origin{camera.position[0], camera.position[1], camera.position[2]};
	const auto raycast = [&] {
		return RaycastVoxelWorld(*world, origin, direction, interaction->maxInteractionDistance);
	};

	if (interaction->mutationAnchorValid &&
		!IsInsideVoxelWorld(*world, interaction->mutationAnchorVoxel)) {
		interaction->mutationAnchorValid = false;
		interaction->mutationAnchorVoxel = {};
		interaction->mutationAnchorUsesPlacementVoxel = false;
	}

	VoxelRaycastHit hit = raycast();
	interaction->selection = BuildInteractionSelection(hit, world);
	UpdateMutationAnchorState(*input, *interaction);
	RefreshPickedPlacementMaterial(*input, hit, *interaction);

	if (allowEditing && ApplyEditorInteraction(hit, camera, *input, physics, *world, *interaction)) {
		hit = raycast();
		interaction->selection = BuildInteractionSelection(hit, world);
	}

	ClearInteractionActions(input);
}
