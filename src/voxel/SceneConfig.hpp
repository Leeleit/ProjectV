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

/// \brief Default location for the scene config JSON.
///
/// \details
/// We pick `runtime/scene.json`
///  relative to the project working directory so the file ships next to the

///  binary in the build tree, and so a fresh `git clone` produces a clean

///  repo without a stray committed config.

std::string GetDefaultSceneConfigPath();

/// \brief If `path` does not exist, write a default JSON document to it so
///
/// \details
///  users have a discoverable starting point. Returns true on a no-op

///  (file already present) or on a successful write.

bool EnsureDefaultSceneConfig(std::string_view path);

/// \brief Parse the JSON file at `path` into `outConfig`.
///
/// \details
/// Returns false (and
///  leaves `outConfig` untouched) on missing-file, parse-error, or

///  schema-mismatch conditions. Errors are streamed to stderr in

///  nlohmann/json's native format.

bool LoadSceneConfig(std::string_view path, SceneConfig &outConfig);

}  // namespace projectv::voxel

