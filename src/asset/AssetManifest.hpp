#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

import projectv.string_id;

namespace projectv::asset {

struct ManifestEntry {
	core::StringID id;
	std::string path;
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	float scale{1.0f};
};

std::vector<ManifestEntry> ParseAssetManifestString(const std::string &raw);


std::vector<ManifestEntry> ParseAssetManifestFromEnv();

} // namespace projectv::asset

