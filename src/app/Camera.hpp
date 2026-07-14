#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

inline constexpr uint32_t kMaxSimulationStepsPerFrame = 5;

void InitializeCamera(
	CameraState *camera,
	SimulationState *simulation,
	InputState *input);
void ResetCameraState(CameraState *camera);
void HandleCameraEvent(
	CameraState *camera,
	InputState *input,
	const SDL_Event *event);
void ConsumeCameraLookInput(
	CameraState *camera,
	InputState *input);
void TickCamera(
	CameraState *camera,
	const InputState &input,
	float deltaSeconds);
bool IsCreativeMode(const CameraState &camera);
bool IsWalkMode(const CameraState &camera);
bool IsSpectatorMode(const CameraState &camera);
std::array<float, 3> GetCameraForwardVector(
	const CameraState &camera);
float GetCameraVisibleSceneMaxDistance(
	const CameraState &camera);
GraphicsPushConstants BuildGraphicsPushConstants(
	const CameraState &camera,
	VkExtent2D extent,
	const RenderState *render = nullptr);
ChunkCullingParameters BuildChunkCullingParameters(
	const CameraState &camera,
	VkExtent2D extent,
	float maxDistance);

