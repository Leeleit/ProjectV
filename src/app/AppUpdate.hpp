#pragma once

#include "core/Types.hpp"

namespace projectv::audio {
class AudioEngine;
} // namespace projectv::audio

[[nodiscard]] bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug,

	projectv::audio::AudioEngine *audio = nullptr);

