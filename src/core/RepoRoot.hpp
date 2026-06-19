
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
