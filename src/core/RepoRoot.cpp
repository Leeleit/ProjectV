#include "core/RepoRoot.hpp"

#include <system_error>

namespace projectv::core {

namespace {
constexpr char kRepoMarkerName[] = ".git";
constexpr char kProjectMarkerName[] = "AGENTS.md";
}  // namespace

std::optional<std::filesystem::path> FindRepoRoot(
    const std::filesystem::path &start)
{
    std::error_code ec;
    std::filesystem::path current = std::filesystem::absolute(start, ec);
    if (ec) {
        // `std::filesystem::absolute` failed (rare — usually a
        // permission / encoding issue). Fall back to `start`
        // verbatim and let the walk-up proceed.
        current = start;
    }
    while (!current.empty()) {
        const std::filesystem::path gitPath = current / kRepoMarkerName;
        const std::filesystem::path agentsPath = current / kProjectMarkerName;
        const bool hasGit = std::filesystem::exists(gitPath, ec);
        const bool hasAgents = std::filesystem::exists(agentsPath, ec);
        if (hasGit && hasAgents && !ec) {
            return current;
        }
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root.
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

}  // namespace projectv::core
