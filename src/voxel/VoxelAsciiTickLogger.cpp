#include "voxel/VoxelAsciiTickLogger.hpp"

#include "core/EnvUtils.hpp"

#include "SDL3/SDL.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr char kDefaultLogFileName[] = "voxel-ascii-tick.log";

bool BoundsXzEqual(const VoxelAsciiBounds &a, const VoxelAsciiBounds &b) noexcept
{
	return a.min.x == b.min.x && a.min.z == b.min.z && a.maxExclusive.x == b.maxExclusive.x &&
		   a.maxExclusive.z == b.maxExclusive.z;
}

uint64_t HashYLayer(const VoxelWorld &world, const int y, const VoxelAsciiBounds &bounds)
{
	uint64_t hash = kFnvOffsetBasis;
	for (int z = bounds.min.z; z < bounds.maxExclusive.z; ++z) {
		for (int x = bounds.min.x; x < bounds.maxExclusive.x; ++x) {
			const auto material = static_cast<uint8_t>(GetVoxelMaterial(world, {x, y, z}));
			hash ^= material;
			hash *= kFnvPrime;
		}
	}
	return hash;
}

bool EnvMeansOff(const char *value)
{
	if (value == nullptr || value[0] == '\0') {
		return false; // unset/empty → default ON
	}
	if (std::strcmp(value, "0") == 0) {
		return true;
	}
	char upper[8]{};
	const size_t n = std::min(sizeof(upper) - 1, std::strlen(value));
	for (size_t i = 0; i < n; ++i) {
		upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
	}
	return std::strcmp(upper, "OFF") == 0 || std::strcmp(upper, "FALSE") == 0 || std::strcmp(upper, "NO") == 0;
}

bool EnvMeansDefaultPath(const char *value)
{
	if (value == nullptr || value[0] == '\0') {
		return true;
	}
	if (std::strcmp(value, "1") == 0) {
		return true;
	}
	char upper[8]{};
	const size_t n = std::min(sizeof(upper) - 1, std::strlen(value));
	for (size_t i = 0; i < n; ++i) {
		upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
	}
	return std::strcmp(upper, "ON") == 0 || std::strcmp(upper, "TRUE") == 0 || std::strcmp(upper, "YES") == 0;
}

std::string StripWrappingQuotes(std::string path)
{
	if (path.size() >= 2) {
		const char a = path.front();
		const char b = path.back();
		if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
			path = path.substr(1, path.size() - 2);
		}
	}
	return path;
}

std::string DefaultAsciiTickLogPath()
{
	if (const char *basePath = SDL_GetBasePath(); basePath != nullptr && basePath[0] != '\0') {
		std::filesystem::path path = std::filesystem::path{basePath} / kDefaultLogFileName;
		return path.string();
	}
	return kDefaultLogFileName;
}

bool EnsureParentDirectory(const std::filesystem::path &filePath)
{
	const std::filesystem::path parent = filePath.parent_path();
	if (parent.empty()) {
		return true;
	}
	std::error_code ec;
	std::filesystem::create_directories(parent, ec);
	return !ec;
}

VoxelAsciiTickLogger &GlobalAsciiTickLogger()
{
	static VoxelAsciiTickLogger logger;
	return logger;
}

} // namespace

bool VoxelAsciiTickLogger::Enable(std::string path, const VoxelAsciiOptions options)
{
	Disable();
	path = StripWrappingQuotes(std::move(path));
	const std::filesystem::path fsPath{path};
	if (!EnsureParentDirectory(fsPath)) {
		std::fprintf(stderr, "[VoxelAsciiTickLog] failed to create parent dir for '%s'\n", path.c_str());
		return false;
	}
	auto file = std::make_unique<std::ofstream>(fsPath, std::ios::out | std::ios::trunc | std::ios::binary);
	if (!file || !*file) {
		std::fprintf(stderr, "[VoxelAsciiTickLog] failed to open '%s'\n", path.c_str());
		return false;
	}
	file_ = std::move(file);
	options_ = options;
	options_.includeLegend = false; // legend written once below; per-layer dumps stay clean
	enabled_ = true;
	legendWritten_ = false;
	hasLastBounds_ = false;
	layerHash_.clear();
	layerHashValid_.clear();
	*file_ << "PROJECTV_ASCII_TICK_LOG path=" << path << '\n';
	*file_ << "legend: .=Air G=Glass ~=Fluid #=FloorWhite %=FloorGray\n";
	legendWritten_ = true;
	file_->flush();
	std::fprintf(stderr, "[VoxelAsciiTickLog] writing to '%s'\n", path.c_str());
	return true;
}

void VoxelAsciiTickLogger::Disable()
{
	file_.reset();
	enabled_ = false;
	legendWritten_ = false;
	hasLastBounds_ = false;
	layerHash_.clear();
	layerHashValid_.clear();
}

bool VoxelAsciiTickLogger::IsEnabled() const noexcept
{
	return enabled_;
}

void VoxelAsciiTickLogger::OnSimulationTick(const VoxelWorld &world, const uint64_t simulationTick)
{
	if (!enabled_ || !file_ || !*file_) {
		return;
	}
	WriteTickDiff(world, simulationTick, *file_);
	file_->flush(); // keep file readable while ProjectV runs
}

void VoxelAsciiTickLogger::OnSimulationTickTo(
	const VoxelWorld &world,
	const uint64_t simulationTick,
	std::ostream &out)
{
	WriteTickDiff(world, simulationTick, out);
}

void VoxelAsciiTickLogger::WriteTickDiff(
	const VoxelWorld &world,
	const uint64_t simulationTick,
	std::ostream &out)
{
	const auto boundsOpt = ComputeVoxelAsciiBounds(world, options_.padding);
	if (!boundsOpt.has_value()) {
		hasLastBounds_ = false;
		layerHash_.assign(static_cast<size_t>(std::max(0, world.height)), 0);
		layerHashValid_.assign(layerHash_.size(), 0);
		return;
	}
	const VoxelAsciiBounds &bounds = *boundsOpt;

	const size_t heightSlots = static_cast<size_t>(std::max(0, world.height));
	if (layerHash_.size() != heightSlots) {
		layerHash_.assign(heightSlots, 0);
		layerHashValid_.assign(heightSlots, 0);
		hasLastBounds_ = false;
	}

	if (hasLastBounds_ && !BoundsXzEqual(lastBounds_, bounds)) {
		std::fill(layerHashValid_.begin(), layerHashValid_.end(), 0); // XZ window shifted → invalidate
	}

	std::string changedLayers;
	VoxelAsciiOptions layerOptions = options_;
	layerOptions.includeLegend = false;

	for (int y = bounds.maxExclusive.y - 1; y >= bounds.min.y; --y) {
		if (y < world.min.y || y >= world.maxExclusive.y) {
			continue;
		}
		const size_t slot = static_cast<size_t>(y - world.min.y);
		const uint64_t hash = HashYLayer(world, y, bounds);
		const bool changed = layerHashValid_[slot] == 0 || layerHash_[slot] != hash;
		if (!changed) {
			continue;
		}
		changedLayers += FormatVoxelAsciiYLayer(world, y, bounds, layerOptions);
		if (!changedLayers.empty() && changedLayers.back() != '\n') {
			changedLayers += '\n';
		}
		layerHash_[slot] = hash;
		layerHashValid_[slot] = 1;
	}

	lastBounds_ = bounds;
	hasLastBounds_ = true;

	if (changedLayers.empty()) {
		return;
	}

	out << "# tick=" << simulationTick << '\n';
	out << changedLayers;
	if (!legendWritten_ && options_.includeLegend) {
		out << "legend: .=Air G=Glass ~=Fluid #=FloorWhite %=FloorGray\n";
		legendWritten_ = true;
	}
}

void MaybeLogVoxelAsciiTick(const VoxelWorld &world, const uint64_t simulationTick)
{
	static bool resolved = false;
	static bool active = false;
	if (!resolved) {
		resolved = true;
		const char *value = projectv::core::GetEnvVar("PROJECTV_ASCII_TICK_LOG");
		if (EnvMeansOff(value)) {
			active = false;
			std::fprintf(stderr, "[VoxelAsciiTickLog] disabled by PROJECTV_ASCII_TICK_LOG\n");
			return;
		}
		std::string path = DefaultAsciiTickLogPath();
		if (!EnvMeansDefaultPath(value)) {
			path = StripWrappingQuotes(value);
		}
		VoxelAsciiOptions options{};
		options.includeLegend = true;
		active = GlobalAsciiTickLogger().Enable(std::move(path), options);
	}
	if (!active) {
		return;
	}
	GlobalAsciiTickLogger().OnSimulationTick(world, simulationTick);
}
