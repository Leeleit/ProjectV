#include "audio/MusicDirectoryPath.hpp"

#include "core/RepoRoot.hpp"
#include "SDL3/SDL.h"

#include <filesystem>

namespace projectv::audio {

namespace {
constexpr char kMusicDirectoryEnvVar[] = "PROJECTV_MUSIC_DIR";
constexpr char kDefaultMusicDirectoryName[] = "music";
} // namespace

std::filesystem::path GetMusicDirectoryPath()
{

	if (const char *overridePath = SDL_getenv(kMusicDirectoryEnvVar);
		overridePath && *overridePath) {
		return std::filesystem::path(overridePath);
	}

	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		if (const auto repoRoot = core::FindRepoRoot(basePath)) {
			return *repoRoot / kDefaultMusicDirectoryName;
		}
	}

	{
		const std::filesystem::path cwdCandidate =
			std::filesystem::path(kDefaultMusicDirectoryName);
		std::error_code ec;
		if (std::filesystem::is_directory(cwdCandidate, ec) && !ec) {
			return cwdCandidate;
		}
	}

	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		const std::filesystem::path resolvedPath =
			std::filesystem::path(basePath) / kDefaultMusicDirectoryName;
		return resolvedPath;
	}

	return std::filesystem::path(kDefaultMusicDirectoryName);
}

} // namespace projectv::audio
