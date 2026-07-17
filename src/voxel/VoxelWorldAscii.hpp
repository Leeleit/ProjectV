#pragma once

#include "voxel/VoxelWorld.hpp"

#include <optional>
#include <string>

struct VoxelAsciiBounds {
	Int3 min{};
	Int3 maxExclusive{};
};

struct VoxelAsciiOptions {
	bool includeLegend = false;
	bool includeZRowLabels = true;
	int padding = 0; // cells; applied only when bounds are auto-computed
};

[[nodiscard]] char VoxelMaterialToAscii(VoxelMaterial material) noexcept;

[[nodiscard]] std::optional<VoxelAsciiBounds> ComputeVoxelAsciiBounds(const VoxelWorld &world, int padding = 0);

[[nodiscard]] std::string FormatVoxelAsciiYLayer(
	const VoxelWorld &world,
	int y,
	std::optional<VoxelAsciiBounds> bounds = std::nullopt,
	const VoxelAsciiOptions &options = {});

[[nodiscard]] std::string FormatVoxelAsciiYLayers(
	const VoxelWorld &world,
	std::optional<int> yMin = std::nullopt,
	std::optional<int> yMaxExclusive = std::nullopt,
	std::optional<VoxelAsciiBounds> bounds = std::nullopt,
	const VoxelAsciiOptions &options = {});
