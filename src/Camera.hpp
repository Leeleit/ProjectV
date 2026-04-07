#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "Types.hpp"

void InitializeCamera(
	CameraState *camera,
	SimulationState *simulation,
	InputState *input);
void HandleCameraEvent(
	CameraState *camera,
	InputState *input,
	const SDL_Event *event);
void ConsumeCameraLookInput(
	CameraState *camera,
	InputState *input);
void TickCamera(
	CameraState *camera,
	float deltaSeconds);
std::array<float, 3> GetCameraForwardVector(
	const CameraState &camera);
GraphicsPushConstants BuildGraphicsPushConstants(
	const CameraState &camera,
	VkExtent2D extent);

#endif
