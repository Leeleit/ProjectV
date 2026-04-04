#ifndef SCENE_RESOURCES_HPP
#define SCENE_RESOURCES_HPP

#include "Types.hpp"

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render);
bool UpdateSceneResources(
	WorldState *world,
	RenderState *render);
bool UploadSceneFrameResources(
	const WorldState *world,
	RenderState *render,
	uint32_t frameIndex);
void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render);

#endif
