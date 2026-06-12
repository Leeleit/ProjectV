#include "audio/MusicDirectoryPath.hpp"

#include "SDL3/SDL.h"

#include <filesystem>

namespace projectv::audio {

namespace {
constexpr char kMusicDirectoryEnvVar[] = "PROJECTV_MUSIC_DIR";
constexpr char kDefaultMusicDirectoryName[] = "music";
} // namespace

std::filesystem::path GetMusicDirectoryPath()
{
	// 1. Env-var override (highest priority). The
	// operator can point at a folder anywhere on
	// disk without rebuilding.
	if (const char *overridePath = SDL_getenv(kMusicDirectoryEnvVar);
		overridePath && *overridePath) {
		return std::filesystem::path(overridePath);
	}

	// 2. CWD-relative `./music`. **This is the
	// primary fallback** because the operator's
	// "папка music в корне" intent is the repo
	// root, and the operator runs the binary
	// from the repo root (e.g.
	// `./build/linux-clang-debug/bin/ProjectV`).
	// The CWD at launch time is the repo root;
	// `./music` resolves to the right place. If
	// the operator launched from somewhere else,
	// they set `PROJECTV_MUSIC_DIR` (or use the
	// SDL_GetBasePath fallback below).
	{
		const std::filesystem::path cwdCandidate =
			std::filesystem::path(kDefaultMusicDirectoryName);
		std::error_code ec;
		if (std::filesystem::is_directory(cwdCandidate, ec) && !ec) {
			return cwdCandidate;
		}
	}

	// 3. `SDL_GetBasePath() / "music"`. Last
	// resort. `SDL_GetBasePath` returns the
	// directory containing the executable, so
	// this resolves to e.g.
	// `build/linux-clang-debug/bin/music/` when
	// the operator runs from there. Matches the
	// screenshot/snapshot convention as a
	// final-fallback. We only return this if
	// step 2's `./music` doesn't exist on disk
	// — otherwise the CWD-relative path wins
	// (so the operator can launch from the repo
	// root without the engine looking at the
	// build tree's bin/ subdirectory).
	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		const std::filesystem::path resolvedPath =
			std::filesystem::path(basePath) / kDefaultMusicDirectoryName;
		return resolvedPath;
	}

	// 4. Last-ditch: CWD-relative `./music` even
	// if it doesn't exist. The engine's
	// `loadMusicFolder` creates the directory on
	// first use, so this is safe.
	return std::filesystem::path(kDefaultMusicDirectoryName);
}

} // namespace projectv::audio
