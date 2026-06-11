#ifndef ASSET_MANIFEST_HPP
#define ASSET_MANIFEST_HPP

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace projectv::asset {

struct ManifestEntry {
	std::string id;
	std::string path;
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	float scale{1.0f};
};

// Parses a manifest string of the form
//   "pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s;pathC.glb"
// per the U2=c (CLI env var) contract from session
// 2026-06-11-asset-pipeline-m0-m5. The id defaults to the path
// basename (without extension) when the caller does not supply one.
std::vector<ManifestEntry> ParseAssetManifestString(const std::string &raw);

// Reads the `PROJECTV_MODELS` environment variable and parses it.
// Returns an empty vector if the variable is unset or empty.
std::vector<ManifestEntry> ParseAssetManifestFromEnv();

} // namespace projectv::asset

#endif
