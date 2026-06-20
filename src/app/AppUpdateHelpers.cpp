#include "app/AppUpdateHelpers.hpp"

#include <cmath>

#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::app {

void UpdateFrameStatistics(SimulationState &simulation, DebugState &debug, const RenderState &render)
{
	simulation.simulationStepsLastFrame = 0;
	debug.stats.framesPerSecond = simulation.frameDeltaSeconds > 0.0f ? 1.0f / simulation.frameDeltaSeconds : 0.0f;
	debug.stats.frameTimeMilliseconds = simulation.frameDeltaSeconds * 1000.0f;
	debug.stats.simulationStepsLastFrame = 0;
	debug.stats.sceneTriangleCount = render.sceneTriangleCount;
}

bool UpdateEffectivePausedAndEditing(
	const CameraState &camera,
	WorldState &world,
	SimulationState &simulation)
{
	const bool frameStepRequestedNow = simulation.frameStepRequested;
	simulation.frameStepRequested = false;
	const bool effectivePaused = simulation.paused && !frameStepRequestedNow;
	simulation.effectivePaused = effectivePaused;

	const bool creativeMode = IsCreativeMode(camera);
	const bool walkMode = IsWalkMode(camera);
	const bool allowWorldEditing =
		(creativeMode || walkMode) &&
		!world.scenePresetReloadRequested &&
		!world.snapshotLoadRequested;
	world.allowWorldEditing = allowWorldEditing;

	simulation.frameDeltaSeconds *= simulation.timeScale;

	if (frameStepRequestedNow) {
		simulation.simulationAccumulatorSeconds = simulation.fixedSimulationDeltaSeconds;
	} else if (!effectivePaused) {
		simulation.simulationAccumulatorSeconds += simulation.frameDeltaSeconds;
	} else {
		simulation.simulationAccumulatorSeconds = 0.0f;
	}

	return effectivePaused;
}

bool RunSimulationTickLoop(
	CameraState *camera,
	InputState *input,
	WorldState &world,
	SimulationState &simulation,
	PhysicsState *physics)
{
	const bool walkMode = IsWalkMode(*camera);
	const bool creativeMode = IsCreativeMode(*camera);
	const bool spectatorMode = IsSpectatorMode(*camera);
	const bool effectivePaused = simulation.effectivePaused;

	while (simulation.simulationAccumulatorSeconds >= simulation.fixedSimulationDeltaSeconds &&
		   simulation.simulationStepsLastFrame < kMaxSimulationStepsPerFrame &&
		   !effectivePaused) {
		if (walkMode) {
			if (!physics || !world.voxelWorld) {
				return false;
			}
			if (!TickWalkCharacter(
					physics,
					world.voxelWorld.get(),
					camera,
					input,
					simulation.fixedSimulationDeltaSeconds)) {
				return false;
			}
		} else if (creativeMode) {
			if (!physics || !world.voxelWorld) {
				return false;
			}
			if (!TickCreativeCharacter(
					physics,
					world.voxelWorld.get(),
					camera,
					input,
					simulation.fixedSimulationDeltaSeconds)) {
				return false;
			}
		} else {
			TickCamera(camera, *input, simulation.fixedSimulationDeltaSeconds);
		}
		simulation.simulationAccumulatorSeconds -= simulation.fixedSimulationDeltaSeconds;
		++simulation.simulationStepsLastFrame;
		++simulation.simulationTick;
	}

	if (simulation.simulationAccumulatorSeconds >= simulation.fixedSimulationDeltaSeconds) {
		simulation.simulationAccumulatorSeconds =
			std::fmod(simulation.simulationAccumulatorSeconds, simulation.fixedSimulationDeltaSeconds);
	}

	if (effectivePaused && spectatorMode && simulation.frameDeltaSeconds > 0.0f) {
		TickCamera(camera, *input, simulation.frameDeltaSeconds);
	}

	return true;
}

} // namespace projectv::app
