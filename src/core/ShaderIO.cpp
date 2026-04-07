#include "core/ShaderIO.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include "SDL3/SDL.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::vector<char> ReadFileFromPath(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return {};
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize <= 0) {
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(fileSize));
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	return buffer;
}

std::vector<std::filesystem::path> BuildShaderSearchPaths(const char *filename)
{
	std::vector<std::filesystem::path> candidates;
	if (!filename || !*filename) {
		return candidates;
	}

	const char *overrideBaseDir = SDL_getenv("PROJECTV_SHADER_BASE_DIR");
	if (overrideBaseDir && *overrideBaseDir) {
		candidates.emplace_back(std::filesystem::path(overrideBaseDir) / filename);
		return candidates;
	}

	candidates.emplace_back(filename);

	const char *basePath = SDL_GetBasePath();
	if (basePath && *basePath) {
		candidates.emplace_back(std::filesystem::path(basePath) / filename);
	}

	return candidates;
}
} // namespace

std::vector<char> ReadShaderFile(const char *filename)
{
	const std::vector<std::filesystem::path> candidates = BuildShaderSearchPaths(filename);
	for (const std::filesystem::path &candidate : candidates) {
		if (std::vector<char> buffer = ReadFileFromPath(candidate); !buffer.empty()) {
			return buffer;
		}
	}

	std::vector<std::string> printableCandidates;
	printableCandidates.reserve(candidates.size());
	for (const std::filesystem::path &candidate : candidates) {
		printableCandidates.push_back(candidate.string());
	}

	runtime::LogRuntimeFailure(
		"Shader",
		filename ? filename : "<null>",
		fmt::format("shader blob not found; searched: {}", fmt::join(printableCandidates, ", ")));
	return {};
}
