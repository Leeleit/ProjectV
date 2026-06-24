import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "app/Camera.hpp"

bool IsCreativeMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Creative;
}

bool IsSpectatorMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Spectator;
}

bool IsWalkMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Walk;
}

#include "app/InputActions.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinMoveSpeed = 2.0f;
constexpr float kMaxMoveSpeed = 40.0f;
constexpr float kBoostMoveSpeedMultiplier = 3.0f;
constexpr float kSlowMoveSpeedMultiplier = 0.25f;
constexpr float kMaxLookPitchRadians = 1.553343f;
constexpr float kMainlineVisibleSceneMaxDistance = 64.0f;

using Float3 = projectv::math::Vec3;

Float3 GetForwardVector(const CameraState &camera)
{
	return Float3{
		std::cos(camera.pitchRadians) * std::sin(camera.yawRadians),
		std::sin(camera.pitchRadians),
		-std::cos(camera.pitchRadians) * std::cos(camera.yawRadians),
		0.0f,
	};
}

Float3 FlattenToPlane(const Float3 vector)
{
	return projectv::math::normalize(Float3{vector.x, 0.0f, vector.z, 0.0f});
}

void AddScaled(Float3 *target, const Float3 vector, const float scale)
{
	target->x += vector.x * scale;
	target->y += vector.y * scale;
	target->z += vector.z * scale;
}
} // namespace

void InitializeCamera(
	CameraState *camera,
	SimulationState *simulation,
	InputState *input)
{
	if (!camera || !simulation || !input) {
		return;
	}

	ResetCameraState(camera);
	simulation->lastFrameCounter = SDL_GetPerformanceCounter();
	simulation->frameDeltaSeconds = 0.0f;
	simulation->simulationAccumulatorSeconds = 0.0f;
	simulation->simulationStepsLastFrame = 0;
	simulation->simulationTick = 0;
	simulation->paused = false;
	InitializeInputState(*input);
}

void ResetCameraState(CameraState *camera)
{
	if (!camera) {
		return;
	}

	const CameraState::ControlMode controlMode = camera->controlMode;
	*camera = {};
	camera->controlMode = controlMode;
}

void HandleCameraEvent(
	CameraState *camera,
	InputState *input,
	const SDL_Event *event)
{
	if (!camera || !input || !event) {
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_MOTION && input->relativeMouseModeEnabled) {
		if (input->skipFirstMouseMotion) {
			input->skipFirstMouseMotion = false;
			input->mouseDeltaX = 0.0f;
			input->mouseDeltaY = 0.0f;
			return;
		}
		if (input->mouseMotionFreezeCount > 0) {
			--input->mouseMotionFreezeCount;
			return;
		}
		constexpr float kMaxMouseDeltaPerEvent = 100.0f;
		const float dx = std::clamp(
			event->motion.xrel,
			-kMaxMouseDeltaPerEvent,
			kMaxMouseDeltaPerEvent);
		const float dy = std::clamp(
			event->motion.yrel,
			-kMaxMouseDeltaPerEvent,
			kMaxMouseDeltaPerEvent);
		input->mouseDeltaX += dx;
		input->mouseDeltaY += dy;
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_WHEEL) {
		camera->moveSpeed = std::clamp(
			camera->moveSpeed + event->wheel.y,
			kMinMoveSpeed,
			kMaxMoveSpeed);
	}
}

void ConsumeCameraLookInput(
	CameraState *camera,
	InputState *input)
{
	if (!camera || !input) {
		return;
	}

	camera->yawRadians += input->mouseDeltaX * camera->mouseSensitivity;
	camera->pitchRadians -= input->mouseDeltaY * camera->mouseSensitivity;
	camera->pitchRadians = std::clamp(camera->pitchRadians, -kMaxLookPitchRadians, kMaxLookPitchRadians);
	input->mouseDeltaX = 0.0f;
	input->mouseDeltaY = 0.0f;
}

void TickCamera(
	CameraState *camera,
	const InputState &input,
	const float deltaSeconds)
{
	if (!camera) {
		return;
	}

	const Float3 forward = projectv::math::normalize(GetForwardVector(*camera));
	const Float3 planarForward = FlattenToPlane(forward);
	constexpr Float3 worldUp{0.0f, 1.0f, 0.0f, 0.0f};
	const Float3 right = projectv::math::normalize(projectv::math::cross(planarForward, worldUp));
	float moveSpeed = camera->moveSpeed;
	if (IsInputActionDown(input, InputAction::SpeedBoost)) {
		moveSpeed *= kBoostMoveSpeedMultiplier;
	}
	if (IsInputActionDown(input, InputAction::SpeedSlow)) {
		moveSpeed *= kSlowMoveSpeedMultiplier;
	}
	const float moveStep = moveSpeed * deltaSeconds;

	if (IsInputActionDown(input, InputAction::MoveForward)) {
		AddScaled(&camera->position, planarForward, moveStep);
	}
	if (IsInputActionDown(input, InputAction::MoveBackward)) {
		AddScaled(&camera->position, planarForward, -moveStep);
	}
	if (IsInputActionDown(input, InputAction::MoveRight)) {
		AddScaled(&camera->position, right, moveStep);
	}
	if (IsInputActionDown(input, InputAction::MoveLeft)) {
		AddScaled(&camera->position, right, -moveStep);
	}
	if (IsInputActionDown(input, InputAction::MoveUp)) {
		camera->position[1] += moveStep;
	}
	if (IsInputActionDown(input, InputAction::MoveDown)) {
		camera->position[1] -= moveStep;
	}
}

std::array<float, 3> GetCameraForwardVector(const CameraState &camera)
{
	const Float3 forward = projectv::math::normalize(GetForwardVector(camera));
	return {forward.x, forward.y, forward.z};
}

float GetCameraVisibleSceneMaxDistance(const CameraState &camera)
{
	return std::max(
		camera.nearPlane,
		std::min(camera.farPlane, kMainlineVisibleSceneMaxDistance));
}

GraphicsPushConstants BuildGraphicsPushConstants(
	const CameraState &camera,
	const VkExtent2D extent,
	const float taaJitterNdcX,
	const float taaJitterNdcY)
{
	const Float3 cameraPosition{
		camera.position[0],
		camera.position[1],
		camera.position[2],
		0.0f,
	};
	const Float3 forward = projectv::math::normalize(GetForwardVector(camera));
	const Float3 right = projectv::math::normalize(projectv::math::cross(forward, Float3{0.0f, 1.0f, 0.0f, 0.0f}));
	const Float3 up = projectv::math::normalize(projectv::math::cross(right, forward));

	projectv::math::Mat4 view{};
	view.c[0] = projectv::math::Vec4{right.x, up.x, -forward.x, 0.0f};
	view.c[1] = projectv::math::Vec4{right.y, up.y, -forward.y, 0.0f};
	view.c[2] = projectv::math::Vec4{right.z, up.z, -forward.z, 0.0f};
	view.c[3] = projectv::math::Vec4{
		-projectv::math::dot(right, cameraPosition),
		-projectv::math::dot(up, cameraPosition),
		projectv::math::dot(forward, cameraPosition),
		1.0f,
	};

	const float aspect = extent.height > 0u
		? static_cast<float>(extent.width) / static_cast<float>(extent.height)
		: 1.0f;
	const float tanHalfFov = std::tan(camera.verticalFovRadians * 0.5f);
	const float nearPlane = camera.nearPlane;
	const float farPlane = camera.farPlane;
	[[assume(nearPlane > 0.0f)]];
	[[assume(farPlane > nearPlane)]];
	[[assume(extent.width > 0u)]];

	const float jitterNdcX = extent.width > 0 ? taaJitterNdcX * 2.0f / static_cast<float>(extent.width) : 0.0f;
	const float jitterNdcY = extent.height > 0 ? taaJitterNdcY * 2.0f / static_cast<float>(extent.height) : 0.0f;
	projectv::math::Mat4 projection{};
	projection.c[0] = projectv::math::Vec4{1.0f / (aspect * tanHalfFov), 0.0f, 0.0f, 0.0f};
	projection.c[1] = projectv::math::Vec4{0.0f, -1.0f / tanHalfFov, 0.0f, 0.0f};
	projection.c[2] = projectv::math::Vec4{jitterNdcX, jitterNdcY, farPlane / (nearPlane - farPlane), -1.0f};
	projection.c[3] = projectv::math::Vec4{0.0f, 0.0f, nearPlane * farPlane / (nearPlane - farPlane), 0.0f};

	GraphicsPushConstants pushConstants{};
	pushConstants.viewProjection = projection * view;
	const Float3 cameraForward = projectv::math::normalize(GetForwardVector(camera));
	pushConstants.cameraPosition = {
		camera.position[0],
		camera.position[1],
		camera.position[2],
		nearPlane,
	};
	pushConstants.cameraForward = {
		cameraForward.x,
		cameraForward.y,
		cameraForward.z,
		farPlane,
	};
	return pushConstants;
}

ChunkCullingParameters BuildChunkCullingParameters(
	const CameraState &camera,
	const VkExtent2D extent,
	const float maxDistance)
{
	const Float3 cameraPosition{
		camera.position[0],
		camera.position[1],
		camera.position[2],
		0.0f,
	};
	const Float3 forward = projectv::math::normalize(GetForwardVector(camera));
	const Float3 right = projectv::math::normalize(projectv::math::cross(forward, Float3{0.0f, 1.0f, 0.0f, 0.0f}));
	const Float3 up = projectv::math::normalize(projectv::math::cross(right, forward));
	const float aspect = extent.height > 0
							 ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
							 : 1.0f;
	const float tanHalfVerticalFov = std::tan(camera.verticalFovRadians * 0.5f);
	const float tanHalfHorizontalFov = tanHalfVerticalFov * aspect;

	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = projectv::math::Vec4{
		cameraPosition.x,
		cameraPosition.y,
		cameraPosition.z,
		maxDistance,
	};
	parameters.cameraForwardAndTanHalfVerticalFov = projectv::math::Vec4{
		forward.x,
		forward.y,
		forward.z,
		tanHalfVerticalFov,
	};
	parameters.cameraRightAndTanHalfHorizontalFov = projectv::math::Vec4{
		right.x,
		right.y,
		right.z,
		tanHalfHorizontalFov,
	};
	parameters.cameraUpAndNearPlane = projectv::math::Vec4{
		up.x,
		up.y,
		up.z,
		camera.nearPlane,
	};
	return parameters;
}
