#include "voxel/VoxelWorldAscii.hpp"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <string>

namespace {

VoxelAsciiBounds ClipBoundsToWorld(const VoxelWorld &world, VoxelAsciiBounds bounds)
{
	bounds.min.x = std::max(bounds.min.x, world.min.x);
	bounds.min.y = std::max(bounds.min.y, world.min.y);
	bounds.min.z = std::max(bounds.min.z, world.min.z);
	bounds.maxExclusive.x = std::min(bounds.maxExclusive.x, world.maxExclusive.x);
	bounds.maxExclusive.y = std::min(bounds.maxExclusive.y, world.maxExclusive.y);
	bounds.maxExclusive.z = std::min(bounds.maxExclusive.z, world.maxExclusive.z);
	return bounds;
}

bool IsEmptyBounds(const VoxelAsciiBounds &bounds)
{
	return bounds.min.x >= bounds.maxExclusive.x || bounds.min.y >= bounds.maxExclusive.y ||
		   bounds.min.z >= bounds.maxExclusive.z;
}

std::optional<VoxelAsciiBounds> ResolveBounds(
	const VoxelWorld &world,
	const std::optional<VoxelAsciiBounds> &bounds,
	const VoxelAsciiOptions &options)
{
	if (bounds.has_value()) {
		const VoxelAsciiBounds clipped = ClipBoundsToWorld(world, *bounds);
		if (IsEmptyBounds(clipped)) {
			return std::nullopt;
		}
		return clipped;
	}
	return ComputeVoxelAsciiBounds(world, options.padding);
}

void AppendLegend(std::string &out)
{
	out += "legend: .=Air G=Glass ~=Fluid #=FloorWhite %=FloorGray\n";
}

void AppendYLayer(
	std::string &out,
	const VoxelWorld &world,
	const int y,
	const VoxelAsciiBounds &bounds,
	const VoxelAsciiOptions &options)
{
	char header[96]{};
	std::snprintf(
		header,
		sizeof(header),
		"y=%d (x=%d..%d, z=%d..%d)\n",
		y,
		bounds.min.x,
		bounds.maxExclusive.x - 1,
		bounds.min.z,
		bounds.maxExclusive.z - 1);
	out += header;

	for (int z = bounds.min.z; z < bounds.maxExclusive.z; ++z) {
		if (options.includeZRowLabels) {
			char label[32]{};
			std::snprintf(label, sizeof(label), "z=%3d: ", z);
			out += label;
		}
		for (int x = bounds.min.x; x < bounds.maxExclusive.x; ++x) {
			out += VoxelMaterialToAscii(GetVoxelMaterial(world, {x, y, z}));
		}
		out += '\n';
	}
}

} // namespace

char VoxelMaterialToAscii(const VoxelMaterial material) noexcept
{
	switch (material) {
	case VoxelMaterial::Air:
		return '.';
	case VoxelMaterial::Glass:
		return 'G';
	case VoxelMaterial::Fluid:
		return '~';
	case VoxelMaterial::FloorWhite:
		return '#';
	case VoxelMaterial::FloorGray:
		return '%';
	}
	return '?';
}

std::optional<VoxelAsciiBounds> ComputeVoxelAsciiBounds(const VoxelWorld &world, const int padding)
{
	VoxelAsciiBounds bounds{
		.min = {INT32_MAX, INT32_MAX, INT32_MAX},
		.maxExclusive = {INT32_MIN, INT32_MIN, INT32_MIN},
	};
	bool found = false;
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
			for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
				if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Air) {
					continue;
				}
				found = true;
				bounds.min.x = std::min(bounds.min.x, x);
				bounds.min.y = std::min(bounds.min.y, y);
				bounds.min.z = std::min(bounds.min.z, z);
				bounds.maxExclusive.x = std::max(bounds.maxExclusive.x, x + 1);
				bounds.maxExclusive.y = std::max(bounds.maxExclusive.y, y + 1);
				bounds.maxExclusive.z = std::max(bounds.maxExclusive.z, z + 1);
			}
		}
	}
	if (!found) {
		return std::nullopt;
	}

	const int pad = std::max(0, padding);
	bounds.min.x -= pad;
	bounds.min.y -= pad;
	bounds.min.z -= pad;
	bounds.maxExclusive.x += pad;
	bounds.maxExclusive.y += pad;
	bounds.maxExclusive.z += pad;
	bounds = ClipBoundsToWorld(world, bounds);
	if (IsEmptyBounds(bounds)) {
		return std::nullopt;
	}
	return bounds;
}

std::string FormatVoxelAsciiYLayer(
	const VoxelWorld &world,
	const int y,
	std::optional<VoxelAsciiBounds> bounds,
	const VoxelAsciiOptions &options)
{
	if (y < world.min.y || y >= world.maxExclusive.y) {
		return {};
	}
	const auto resolved = ResolveBounds(world, bounds, options);
	if (!resolved.has_value()) {
		return {};
	}

	std::string out;
	if (options.includeLegend) {
		AppendLegend(out);
	}
	AppendYLayer(out, world, y, *resolved, options); // XZ from bounds; y may sit outside auto Y span
	return out;
}

std::string FormatVoxelAsciiYLayers(
	const VoxelWorld &world,
	std::optional<int> yMin,
	std::optional<int> yMaxExclusive,
	std::optional<VoxelAsciiBounds> bounds,
	const VoxelAsciiOptions &options)
{
	const auto resolved = ResolveBounds(world, bounds, options);
	if (!resolved.has_value()) {
		return {};
	}

	const int lo = yMin.value_or(resolved->min.y);
	const int hi = yMaxExclusive.value_or(resolved->maxExclusive.y);
	if (lo >= hi) {
		return {};
	}

	std::string out;
	if (options.includeLegend) {
		AppendLegend(out);
	}
	for (int y = hi - 1; y >= lo; --y) {
		if (!IsInsideVoxelWorld(world, {world.min.x, y, world.min.z})) {
			continue;
		}
		AppendYLayer(out, world, y, *resolved, options);
		if (y != lo) {
			out += '\n';
		}
	}
	return out;
}
