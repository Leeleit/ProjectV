#ifndef APP_UPDATE_HPP
#define APP_UPDATE_HPP

#include "core/Types.hpp"

namespace projectv::audio {
class AudioEngine;
} // namespace projectv::audio

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug,
	// Nullable. `nullptr` is the test default (no
	// audio engine in the unit-test path); the
	// runtime `SDL_AppIterate` path passes
	// `state->audio.get()`. Forward-declared in
	// `core/Types.hpp` so the include stays
	// header-only.
	projectv::audio::AudioEngine *audio = nullptr);

#endif
