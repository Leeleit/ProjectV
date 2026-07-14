#pragma once

#include "core/Types.hpp"

namespace projectv::audio {
class AudioEngine;
} // namespace projectv::audio

namespace projectv::ui {

struct HudFrameContext {
	PlatformState *platform = nullptr;
	SimulationState *simulation = nullptr;
	CameraState *camera = nullptr;
	InputState *input = nullptr;
	InteractionState *interaction = nullptr;
	WorldState *world = nullptr;
	PhysicsState *physics = nullptr;
	RenderState *render = nullptr;
	DebugState *debug = nullptr;
	projectv::audio::AudioEngine *audio = nullptr;
};

void DrawHudFrame(HudFrameContext &ctx);

} // namespace projectv::ui
