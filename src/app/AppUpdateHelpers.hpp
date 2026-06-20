#pragma once

#include "core/Types.hpp"

namespace projectv {
}

namespace projectv::app {

void UpdateFrameStatistics(SimulationState &simulation, DebugState &debug, const RenderState &render);

bool UpdateEffectivePausedAndEditing(
	const CameraState &camera,
	WorldState &world,
	SimulationState &simulation);

bool RunSimulationTickLoop(
	CameraState *camera,
	InputState *input,
	WorldState &world,
	SimulationState &simulation,
	PhysicsState *physics);

} // namespace projectv::app
