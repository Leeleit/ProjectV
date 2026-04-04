#include "AppUpdate.hpp"

#include "Camera.hpp"
#include "SceneResources.hpp"

#include <cmath>
#include <cstdio>

namespace {
constexpr uint32_t kMaxSimulationStepsPerFrame = 5;
constexpr float kMaxFrameDeltaSeconds = 0.25f;

float ComputeFrameDeltaSeconds(SimulationState &simulation)
{
	const Uint64 now = SDL_GetPerformanceCounter();
	if (simulation.lastFrameCounter == 0) {
		simulation.lastFrameCounter = now;
		return 0.0f;
	}

	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const Uint64 deltaCounter = now - simulation.lastFrameCounter;
	simulation.lastFrameCounter = now;
	return std::min(
		static_cast<float>(deltaCounter) / static_cast<float>(frequency),
		kMaxFrameDeltaSeconds);
}
} // namespace

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	WorldState *world,
	RenderState *render,
	DebugState *debug)
{
	if (!platform || !simulation || !camera || !input || !world || !render || !debug) {
		return false;
	}

	simulation->frameDeltaSeconds = ComputeFrameDeltaSeconds(*simulation);
	simulation->simulationAccumulatorSeconds += simulation->frameDeltaSeconds;
	simulation->simulationStepsLastFrame = 0;
	debug->stats.simulationStepsLastFrame = 0;
	debug->stats.sceneTriangleCount = render->sceneTriangleCount;

	ConsumeCameraLookInput(camera, input);

	while (simulation->simulationAccumulatorSeconds >= simulation->fixedSimulationDeltaSeconds &&
		   simulation->simulationStepsLastFrame < kMaxSimulationStepsPerFrame) {
		TickCamera(camera, simulation->fixedSimulationDeltaSeconds);
		simulation->simulationAccumulatorSeconds -= simulation->fixedSimulationDeltaSeconds;
		++simulation->simulationStepsLastFrame;
		++simulation->simulationTick;
	}

	if (simulation->simulationAccumulatorSeconds >= simulation->fixedSimulationDeltaSeconds) {
		simulation->simulationAccumulatorSeconds =
			std::fmod(simulation->simulationAccumulatorSeconds, simulation->fixedSimulationDeltaSeconds);
	}

	if (!UpdateSceneResources(world, render)) {
		return false;
	}

	if (world->voxelWorld) {
		debug->stats.simulationStepsLastFrame = simulation->simulationStepsLastFrame;
		debug->stats.dirtyChunkCount = world->voxelWorld->stats.dirtyChunkCount;
		debug->stats.activeChunkCount = world->voxelWorld->stats.activeChunkCount;
		debug->stats.glassVoxelCount = world->voxelWorld->stats.glassVoxelCount;
		debug->stats.fluidVoxelCount = world->voxelWorld->stats.fluidVoxelCount;
		debug->stats.floorVoxelCount =
			world->voxelWorld->stats.floorWhiteVoxelCount +
			world->voxelWorld->stats.floorGrayVoxelCount;
		debug->stats.nonAirVoxelCount = world->voxelWorld->stats.nonAirVoxelCount;
		debug->stats.sceneTriangleCount = render->sceneTriangleCount;
	}

	debug->titleUpdateAccumulatorSeconds += simulation->frameDeltaSeconds;
	if (platform->window && debug->titleUpdateAccumulatorSeconds >= 0.25f) {
		debug->titleUpdateAccumulatorSeconds = 0.0f;
		const float fps = simulation->frameDeltaSeconds > 0.0f ? 1.0f / simulation->frameDeltaSeconds : 0.0f;
		char windowTitle[192]{};
		std::snprintf(
			windowTitle,
			sizeof(windowTitle),
			"ProjectV v0.0.1 | FPS %.1f | Tri %u | Dirty %u | Active %u | Voxels %u",
			fps,
			debug->stats.sceneTriangleCount,
			debug->stats.dirtyChunkCount,
			debug->stats.activeChunkCount,
			debug->stats.nonAirVoxelCount);
		SDL_SetWindowTitle(platform->window, windowTitle);
	}

	return true;
}
