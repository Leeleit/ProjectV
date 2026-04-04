#ifndef APP_UPDATE_HPP
#define APP_UPDATE_HPP

#include "Types.hpp"

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	WorldState *world,
	RenderState *render,
	DebugState *debug);

#endif
