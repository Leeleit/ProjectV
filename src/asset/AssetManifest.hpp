#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "core/StringId.hpp"

namespace projectv::asset {

struct ManifestEntry {
	projectv::core::StringID id;
	std::string path;
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	float scale{1.0f};
};

std::vector<ManifestEntry> ParseAssetManifestString(const std::string &raw);

// Reads the `PROJECTV_MODELS` environment variable and parses it.
// Returns an empty vector if the variable is unset or empty.
std::vector<ManifestEntry> ParseAssetManifestFromEnv();

} // namespace projectv::asset

