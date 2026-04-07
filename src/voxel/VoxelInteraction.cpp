#include "voxel/VoxelInteraction.hpp"

#include "app/Camera.hpp"
#include "voxel/VoxelRaycast.hpp"
#include "voxel/VoxelWorld.hpp"

namespace {
InteractionSelectionState BuildInteractionSelection(const VoxelRaycastHit &hit)
{
	InteractionSelectionState selection{};
	selection.hasHit = hit.hasHit;
	selection.hasPlacementVoxel = hit.hasPlacementVoxel;
	selection.targetVoxel = hit.voxel;
	selection.placementVoxel = hit.placementVoxel;
	selection.hitNormal = hit.hitNormal;
	selection.targetMaterial = hit.material;
	selection.hitDistance = hit.distance;
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
	const auto raycast = [&]() {
		return RaycastVoxelWorld(*world, camera.position, direction, interaction->maxInteractionDistance);
	};

	VoxelRaycastHit hit = raycast();
	interaction->selection = BuildInteractionSelection(hit);

	if (allowEditing && input->removePressed && hit.hasHit) {
		SetVoxelMaterial(*world, hit.voxel, VoxelMaterial::Air);
		hit = raycast();
		interaction->selection = BuildInteractionSelection(hit);
	} else if (allowEditing &&
			   input->placePressed &&
			   hit.hasHit &&
			   hit.hasPlacementVoxel &&
			   GetVoxelMaterial(*world, hit.placementVoxel) == VoxelMaterial::Air &&
			   interaction->placementMaterial != VoxelMaterial::Air) {
		SetVoxelMaterial(*world, hit.placementVoxel, interaction->placementMaterial);
		hit = raycast();
		interaction->selection = BuildInteractionSelection(hit);
	}

	ClearInteractionActions(input);
}
