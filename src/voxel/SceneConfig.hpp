#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "voxel/VoxelWorld.hpp"

namespace projectv::voxel {

// **Scene configuration (defense r0, 2026-06-13).** Runtime-readable
// JSON config that overrides the hard-coded scene defaults in
// `VoxelWorld::CreateVoxelSceneWorld`. The minimal contract is the
// subset of `VoxelWorldConfig` + `VoxelScenePreset` we care about for
// first-day reproducibility (per ТЗ 4.5.1 "Использование
// структурированных форматов для сохранения сцен").
struct SceneConfig {
	std::string name = "ProjectV Default";
	VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
	VoxelWorldConfig voxelWorldConfig{};
	float sunDirectionY = 0.80f;
	float exposure = 1.0f;
};

// Default location for the scene config JSON. We pick `runtime/scene.json`
// relative to the project working directory so the file ships next to the
// binary in the build tree, and so a fresh `git clone` produces a clean
// repo without a stray committed config.
std::string GetDefaultSceneConfigPath();

// If `path` does not exist, write a default JSON document to it so
// users have a discoverable starting point. Returns true on a no-op
// (file already present) or on a successful write.
bool EnsureDefaultSceneConfig(std::string_view path);

// Parse the JSON file at `path` into `outConfig`. Returns false (and
// leaves `outConfig` untouched) on missing-file, parse-error, or
// schema-mismatch conditions. Errors are streamed to stderr in
// nlohmann/json's native format.
bool LoadSceneConfig(std::string_view path, SceneConfig &outConfig);

}  // namespace projectv::voxel

