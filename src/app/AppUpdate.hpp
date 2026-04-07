#ifndef APP_UPDATE_HPP
#define APP_UPDATE_HPP

#include "core/Types.hpp"

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	RenderState *render,
	DebugState *debug);

#endif
