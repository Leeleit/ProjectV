#pragma once

#include <string> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <string_view>

#include "voxel/VoxelWorld.hpp"

namespace projectv::voxel {

struct SceneConfig {
	std::string name = "ProjectV Default";
	VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
	VoxelWorldConfig voxelWorldConfig{};
	float sunDirectionY = 0.80f;
	float exposure = 1.0f;
	std::string msaaMode = "MSAA4";
	bool smaaEnabled = true;
	std::string renderScale = "1.00";
};


std::string GetDefaultSceneConfigPath();


bool EnsureDefaultSceneConfig(std::string_view path);


bool LoadSceneConfig(std::string_view path, SceneConfig &outConfig);
bool SaveSceneConfig(std::string_view path, const SceneConfig &config);

}  // namespace projectv::voxel

