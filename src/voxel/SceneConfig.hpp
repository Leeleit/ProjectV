#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "voxel/VoxelWorld.hpp"

namespace projectv::voxel {

struct SceneConfig {
	std::string name = "ProjectV Default";
	VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
	VoxelWorldConfig voxelWorldConfig{};
	float sunDirectionY = 0.80f;
	float exposure = 1.0f;
};


std::string GetDefaultSceneConfigPath();


bool EnsureDefaultSceneConfig(std::string_view path);


bool LoadSceneConfig(std::string_view path, SceneConfig &outConfig);

}  // namespace projectv::voxel

