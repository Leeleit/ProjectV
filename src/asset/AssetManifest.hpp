#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "core/StringId.hpp"

namespace projectv::asset {

struct ManifestEntry {
	// **Tier 1.D/E (`2026-06-13`).** Replaced `std::string` with
	// `projectv::core::StringID` for the manifest entry id. The id
	// is set once at parse time and then carried through
	// `ModelRegistryEntry::id` for the lifetime of the manifest —
	// StringID's 16 B (hash + length + pad) is the right shape
	// (no allocation, O(1) equality, hashable for
	// `unordered_map<StringID, …>`). `path` stays `std::string`
	// because it's a filesystem path that's only used at parse
	// + load time (cold path); the hot path only sees
	// `ModelRegistryEntry::id`.
	projectv::core::StringID id;
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

