#include "audio/MusicDirectoryPath.hpp"

#include "core/RepoRoot.hpp"
#include "SDL3/SDL.h"

#include <filesystem>

namespace projectv::audio {

namespace {
constexpr char kMusicDirectoryEnvVar[] = "PROJECTV_MUSIC_DIR";
constexpr char kDefaultMusicDirectoryName[] = "music";
}  // namespace

std::filesystem::path GetMusicDirectoryPath()
{
	// 1. Env-var override (highest priority). The
	// operator can point at a folder anywhere on
	// disk without rebuilding.
	if (const char *overridePath = SDL_getenv(kMusicDirectoryEnvVar);
		overridePath && *overridePath) {
		return std::filesystem::path(overridePath);
	}

	// 2. **Repo-root walk-up from the binary's
	// location.** The operator's "папка music в
	// корне" intent is the ProjectV repo root.
	// The binary lives at e.g.
	// `<repo_root>/build/linux-clang-debug/bin/`,
	// so walking up from `SDL_GetBasePath()`
	// toward the filesystem root passes
	// `bin/` → `linux-clang-debug/` → `build/` →
	// `<repo_root>` (which has both `.git` and
	// `AGENTS.md`). When the repo root is found,
	// the music folder is `<repo_root>/music/`,
	// regardless of CWD. This is what makes
	// `/home/le1t/Projects/ProjectV/build/.../bin/ProjectV`
	// find the right music folder when the
	// operator runs the binary by absolute path
	// from anywhere (shell prompt at `/tmp`, IDE
	// run button, CI, etc.).
	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		if (auto repoRoot = projectv::core::FindRepoRoot(basePath)) {
			return *repoRoot / kDefaultMusicDirectoryName;
		}
	}

	// 3. CWD-relative `./music`. Works when the
	// operator runs the binary from the repo root
	// (the canonical "I cloned the repo and built
	// it, now `./build/.../bin/ProjectV` from the
	// repo root" path). Catches the operator who
	// builds the binary in-place and launches
	// from the repo root, even when the repo
	// walk-up above fails (e.g. the operator
	// stripped `.git/` from the working tree, or
	// installed the binary system-wide).
	{
		const std::filesystem::path cwdCandidate =
			std::filesystem::path(kDefaultMusicDirectoryName);
		std::error_code ec;
		if (std::filesystem::is_directory(cwdCandidate, ec) && !ec) {
			return cwdCandidate;
		}
	}

	// 4. `SDL_GetBasePath() / "music"`. Last
	// resort. `SDL_GetBasePath` returns the
	// directory containing the executable, so
	// this resolves to e.g.
	// `build/linux-clang-debug/bin/music/` when
	// the operator runs from there. Matches the
	// screenshot/snapshot convention as a
	// final-fallback. We only return this if
	// step 2's repo walk-up didn't find a root AND
	// step 3's CWD-relative `./music` doesn't
	// exist — otherwise the CWD-relative path
	// wins (so the operator can launch from the
	// repo root without the engine looking at the
	// build tree's bin/ subdirectory).
	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		const std::filesystem::path resolvedPath =
			std::filesystem::path(basePath) / kDefaultMusicDirectoryName;
		return resolvedPath;
	}

	// 5. Last-ditch: CWD-relative `./music` even
	// if it doesn't exist. The engine's
	// `loadMusicFolder` creates the directory on
	// first use, so this is safe.
	return std::filesystem::path(kDefaultMusicDirectoryName);
}

}  // namespace projectv::audio
