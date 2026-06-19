#include "voxel/SceneConfig.hpp"

#include "core/RepoRoot.hpp"
#include "SDL3/SDL.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace projectv::voxel {

namespace {

using nlohmann::json;

constexpr const char *kDefaultPath = "runtime/scene.json";


bool ParseScenePreset(std::string_view text, VoxelScenePreset &outPreset)
{
	if (text == "VoxelLab") { outPreset = VoxelScenePreset::VoxelLab; return true; }
	if (text == "FlatBenchmark") { outPreset = VoxelScenePreset::FlatBenchmark; return true; }
	if (text == "TransparencyStress") { outPreset = VoxelScenePreset::TransparencyStress; return true; }
	if (text == "ChunkGrid") { outPreset = VoxelScenePreset::ChunkGrid; return true; }
	if (text == "MeshingStress") { outPreset = VoxelScenePreset::MeshingStress; return true; }
	return false;
}

}  // namespace

std::string GetDefaultSceneConfigPath()
{
	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		if (auto repoRoot = projectv::core::FindRepoRoot(basePath)) {
			return (*repoRoot / "runtime" / "scene.json").string();
		}
	}
	return std::string(kDefaultPath);
}

bool EnsureDefaultSceneConfig(const std::string_view path)
{
	const std::filesystem::path fsPath{std::string{path}};
	if (std::filesystem::exists(fsPath)) {
		return true;
	}


	if (fsPath.has_parent_path()) {
		std::error_code ec;
		std::filesystem::create_directories(fsPath.parent_path(), ec);

	}

	const json defaultDoc = {
		{"name", "ProjectV Default"},
		{"scenePreset", "VoxelLab"},
		{"voxelWorld", {
			{"floorSize", 18},
			{"floorY", 0},
			{"worldTopY", 14},
			{"padding", 3},
			{"chunkSize", 8},
		}},
		{"lighting", {
			{"sunDirectionY", 0.80},
			{"exposure", 1.0},
		}},
	};

	std::ofstream out{fsPath};
	if (!out.is_open()) {
		std::fprintf(stderr, "[ProjectV][SceneConfig] cannot open %s for writing\n", std::string{path}.c_str());
		return false;
	}
	out << defaultDoc.dump(2) << "\n";
	return static_cast<bool>(out);
}

bool LoadSceneConfig(const std::string_view path, SceneConfig &outConfig)
{
	const std::filesystem::path fsPath{std::string{path}};
	std::ifstream in{fsPath};
	if (!in.is_open()) {
		std::fprintf(stderr, "[ProjectV][SceneConfig] cannot open %s for reading\n", std::string{path}.c_str());
		return false;
	}

	json doc;
	try {
		in >> doc;
	} catch (const json::parse_error &error) {
		std::fprintf(
			stderr,
			"[ProjectV][SceneConfig] parse error in %s: %s\n",
			std::string{path}.c_str(),
			error.what());
		return false;
	}

	SceneConfig loaded = outConfig;
	if (doc.contains("name") && doc["name"].is_string()) {
		loaded.name = doc["name"].get<std::string>();
	}
	if (doc.contains("scenePreset") && doc["scenePreset"].is_string()) {
		VoxelScenePreset parsed = VoxelScenePreset::VoxelLab;
		if (ParseScenePreset(doc["scenePreset"].get<std::string>(), parsed)) {
			loaded.scenePreset = parsed;
		} else {
			std::fprintf(
				stderr,
				"[ProjectV][SceneConfig] unknown scenePreset label in %s, defaulting to VoxelLab\n",
				std::string{path}.c_str());
		}
	}
	if (doc.contains("voxelWorld") && doc["voxelWorld"].is_object()) {
		const auto &vw = doc["voxelWorld"];
		if (vw.contains("floorSize") && vw["floorSize"].is_number_integer()) {
			loaded.voxelWorldConfig.floorSize = vw["floorSize"].get<int>();
		}
		if (vw.contains("floorY") && vw["floorY"].is_number_integer()) {
			loaded.voxelWorldConfig.floorY = vw["floorY"].get<int>();
		}
		if (vw.contains("worldTopY") && vw["worldTopY"].is_number_integer()) {
			loaded.voxelWorldConfig.worldTopY = vw["worldTopY"].get<int>();
		}
		if (vw.contains("padding") && vw["padding"].is_number_integer()) {
			loaded.voxelWorldConfig.padding = vw["padding"].get<int>();
		}
		if (vw.contains("chunkSize") && vw["chunkSize"].is_number_integer()) {
			loaded.voxelWorldConfig.chunkSize = vw["chunkSize"].get<int>();
		}
	}
	if (doc.contains("lighting") && doc["lighting"].is_object()) {
		const auto &lt = doc["lighting"];
		if (lt.contains("sunDirectionY") && lt["sunDirectionY"].is_number()) {
			loaded.sunDirectionY = lt["sunDirectionY"].get<float>();
		}
		if (lt.contains("exposure") && lt["exposure"].is_number()) {
			loaded.exposure = lt["exposure"].get<float>();
		}
	}

	outConfig = loaded;
	return true;
}

}  // namespace projectv::voxel
