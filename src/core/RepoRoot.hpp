// **Repo root discovery (`2026-06-15`).**
//
// Helper extracted from `src/audio/MusicDirectoryPath.cpp` so the
// same walk-up algorithm can be shared by `src/voxel/SceneConfig.cpp`
// (which also needs to find the ProjectV repo root to locate
// `runtime/scene.json` regardless of CWD). Previously duplicated
// logic existed only in MusicDirectoryPath; SceneConfig relied on
// a CWD-relative fallback that broke when ProjectV.exe was launched
// from `build\windows-clang-debug\bin\` via Explorer (no CWD = repo
// root, but the binary lives 2 directories deeper).

#pragma once

#include <filesystem>
#include <optional>

namespace projectv::core {

// **Walk up from `start` looking for the ProjectV repo root.** The
// repo root is the first ancestor directory that contains BOTH a
// `.git/` (or `.git` file for submodule checkouts) AND an
// `AGENTS.md` (a ProjectV-specific file). Returns `std::nullopt`
// when no repo root is found.
//
// The "both markers" check is more specific than `.git` alone
// (which catches other VCS worktrees in the file system) and more
// reliable than `CMakeLists.txt` alone (which the build tree also
// contains).
std::optional<std::filesystem::path> FindRepoRoot(
    const std::filesystem::path &start);

}  // namespace projectv::core
