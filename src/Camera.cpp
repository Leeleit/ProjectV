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
} // namespace

void InitializeCamera(AppState *state)
{
	if (!state) {
		return;
	}

	state->camera = {};
	state->lastFrameCounter = SDL_GetPerformanceCounter();
	state->deltaTimeSeconds = 0.0f;
	state->mouseDeltaX = 0.0f;
	state->mouseDeltaY = 0.0f;
}

void HandleCameraEvent(AppState *state, const SDL_Event *event)
{
	if (!state || !event) {
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_MOTION) {
		state->mouseDeltaX += event->motion.xrel;
		state->mouseDeltaY += event->motion.yrel;
		return;
	}

	if (event->type == SDL_EVENT_MOUSE_WHEEL) {
		state->camera.moveSpeed = std::clamp(
			state->camera.moveSpeed + event->wheel.y,
			2.0f,
			40.0f);
	}
}

void UpdateCamera(AppState *state)
{
	if (!state) {
		return;
	}

	const Uint64 now = SDL_GetPerformanceCounter();
	if (state->lastFrameCounter == 0) {
		state->lastFrameCounter = now;
	}

	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const Uint64 deltaCounter = now - state->lastFrameCounter;
	state->lastFrameCounter = now;
	state->deltaTimeSeconds = std::min(
		static_cast<float>(deltaCounter) / static_cast<float>(frequency),
		0.05f);

	state->camera.yawRadians += state->mouseDeltaX * state->camera.mouseSensitivity;
	state->camera.pitchRadians -= state->mouseDeltaY * state->camera.mouseSensitivity;
	state->camera.pitchRadians = std::clamp(state->camera.pitchRadians, -1.4f, 1.4f);
	state->mouseDeltaX = 0.0f;
	state->mouseDeltaY = 0.0f;

	int keyCount = 0;
	const bool *keys = SDL_GetKeyboardState(&keyCount);
	if (!keys || keyCount <= SDL_SCANCODE_D) {
		return;
	}

	const Float3 forward = Normalize(GetForwardVector(state->camera));
	constexpr Float3 worldUp{0.0f, 1.0f, 0.0f};
	const Float3 right = Normalize(Cross(forward, worldUp));
	const float moveStep = state->camera.moveSpeed * state->deltaTimeSeconds;

	if (keys[SDL_SCANCODE_W]) {
		AddScaled(&state->camera.position, forward, moveStep);
	}
	if (keys[SDL_SCANCODE_S]) {
		AddScaled(&state->camera.position, forward, -moveStep);
	}
	if (keys[SDL_SCANCODE_D]) {
		AddScaled(&state->camera.position, right, moveStep);
	}
	if (keys[SDL_SCANCODE_A]) {
		AddScaled(&state->camera.position, right, -moveStep);
	}
	if (keys[SDL_SCANCODE_SPACE]) {
		state->camera.position[1] += moveStep;
	}
	if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
		state->camera.position[1] -= moveStep;
	}
}
