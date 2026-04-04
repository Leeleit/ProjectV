#include "Camera.hpp"

#include <algorithm>
#include <cmath>

namespace {
struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

Float3 GetForwardVector(const CameraState &camera)
{
	return {
		std::cos(camera.pitchRadians) * std::sin(camera.yawRadians),
		std::sin(camera.pitchRadians),
		-std::cos(camera.pitchRadians) * std::cos(camera.yawRadians),
	};
}

Float3 Normalize(const Float3 vector)
{
	const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	if (length <= 0.00001f) {
		return {};
	}

	return {vector.x / length, vector.y / length, vector.z / length};
}

Float3 Cross(const Float3 a, const Float3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

void AddScaled(std::array<float, 3> *target, const Float3 vector, const float scale)
{
	target->at(0) += vector.x * scale;
	target->at(1) += vector.y * scale;
	target->at(2) += vector.z * scale;
}

float Dot(const Float3 a, const Float3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

std::array<float, 16> MultiplyMatrices(
	const std::array<float, 16> &a,
	const std::array<float, 16> &b)
{
	std::array<float, 16> result{};
	for (int column = 0; column < 4; ++column) {
		for (int row = 0; row < 4; ++row) {
			float value = 0.0f;
			for (int index = 0; index < 4; ++index) {
				value += a[index * 4 + row] * b[column * 4 + index];
			}
			result[column * 4 + row] = value;
		}
	}
	return result;
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

	*camera = {};
	simulation->lastFrameCounter = SDL_GetPerformanceCounter();
	simulation->frameDeltaSeconds = 0.0f;
	simulation->simulationAccumulatorSeconds = 0.0f;
	simulation->simulationStepsLastFrame = 0;
	simulation->simulationTick = 0;
	input->mouseDeltaX = 0.0f;
	input->mouseDeltaY = 0.0f;
}

void HandleCameraEvent(
	CameraState *camera,
	InputState *input,
	const SDL_Event *event)
{
	if (!camera || !input || !event) {
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_MOTION) {
		input->mouseDeltaX += event->motion.xrel;
		input->mouseDeltaY += event->motion.yrel;
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_WHEEL) {
		camera->moveSpeed = std::clamp(
			camera->moveSpeed + event->wheel.y,
			2.0f,
			40.0f);
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
	camera->pitchRadians = std::clamp(camera->pitchRadians, -1.4f, 1.4f);
	input->mouseDeltaX = 0.0f;
	input->mouseDeltaY = 0.0f;
}

void TickCamera(
	CameraState *camera,
	const float deltaSeconds)
{
	if (!camera) {
		return;
	}

	int keyCount = 0;
	const bool *keys = SDL_GetKeyboardState(&keyCount);
	if (!keys || keyCount <= SDL_SCANCODE_D) {
		return;
	}

	const Float3 forward = Normalize(GetForwardVector(*camera));
	constexpr Float3 worldUp{0.0f, 1.0f, 0.0f};
	const Float3 right = Normalize(Cross(forward, worldUp));
	const float moveStep = camera->moveSpeed * deltaSeconds;

	if (keys[SDL_SCANCODE_W]) {
		AddScaled(&camera->position, forward, moveStep);
	}
	if (keys[SDL_SCANCODE_S]) {
		AddScaled(&camera->position, forward, -moveStep);
	}
	if (keys[SDL_SCANCODE_D]) {
		AddScaled(&camera->position, right, moveStep);
	}
	if (keys[SDL_SCANCODE_A]) {
		AddScaled(&camera->position, right, -moveStep);
	}
	if (keys[SDL_SCANCODE_SPACE]) {
		camera->position[1] += moveStep;
	}
	if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
		camera->position[1] -= moveStep;
	}
}

GraphicsPushConstants BuildGraphicsPushConstants(
	const CameraState &camera,
	const VkExtent2D extent)
{
	const Float3 cameraPosition{
		camera.position[0],
		camera.position[1],
		camera.position[2],
	};
	const Float3 forward = Normalize(GetForwardVector(camera));
	const Float3 right = Normalize(Cross(forward, Float3{0.0f, 1.0f, 0.0f}));
	const Float3 up = Normalize(Cross(right, forward));

	const std::array view{
		right.x,
		up.x,
		-forward.x,
		0.0f,
		right.y,
		up.y,
		-forward.y,
		0.0f,
		right.z,
		up.z,
		-forward.z,
		0.0f,
		-Dot(right, cameraPosition),
		-Dot(up, cameraPosition),
		Dot(forward, cameraPosition),
		1.0f,
	};

	const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	const float tanHalfFov = std::tan(camera.verticalFovRadians * 0.5f);
	const float nearPlane = camera.nearPlane;
	const float farPlane = camera.farPlane;

	const std::array projection{
		1.0f / (aspect * tanHalfFov),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		-1.0f / tanHalfFov,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		farPlane / (nearPlane - farPlane),
		-1.0f,
		0.0f,
		0.0f,
		nearPlane * farPlane / (nearPlane - farPlane),
		0.0f,
	};

	GraphicsPushConstants pushConstants{};
	pushConstants.viewProjection = MultiplyMatrices(projection, view);
	return pushConstants;
}
