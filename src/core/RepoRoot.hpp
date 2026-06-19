
#pragma once

#include <filesystem>
#include <optional>

namespace projectv::core {


std::optional<std::filesystem::path> FindRepoRoot(
    const std::filesystem::path &start);

}  // namespace projectv::core
