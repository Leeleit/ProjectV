#include "core/Math.hpp"
#include "core/EnvUtils.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldInternal.hpp"
#include "debug/Profiling.hpp"
#include "core/RuntimeDiagnostics.hpp"

#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

namespace {
bool gFluidCaGpuEnabledForTesting = false;

constexpr int kSideOrder[4][2] = {
	{0, -1}, // N — cardinal only; diagonals cut Glass corners
	{1, 0},	 // E
	{0, 1},	 // S
	{-1, 0}, // W
};

bool IsSolidMaterial(const uint8_t material)
{
	return material != static_cast<uint8_t>(VoxelMaterial::Air) &&
		   material != static_cast<uint8_t>(VoxelMaterial::Fluid); // Glass, FloorWhite, FloorGray, …
}
} // namespace

bool IsFluidCaGpuEnabled()
{
	if (gFluidCaGpuEnabledForTesting) {
		return true;
	}
	if (const char *value = projectv::core::GetEnvVar("PROJECTV_FLUID_CA_GPU")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

void ToggleFluidCaGpuEnabledForTesting(const bool enabled)
{
	gFluidCaGpuEnabledForTesting = enabled;
}

std::vector<uint32_t> BuildActiveChunkIdsForFluidCa(const VoxelWorld &world)
{
	std::vector<uint32_t> active;
	active.reserve(world.chunks.size());
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		if (chunk.nonAirVoxelCount == 0u) {
			continue;
		}
		if (chunk.isStatic && chunk.ticksSinceLastEdit < 30u) {
			continue;
		}
		active.push_back(static_cast<uint32_t>(chunkIndex));
	}
	return active;
}

// Binary Fluid CA: FALL → BASE_HOLE → PRESSURE_TO_AIR → GAP_FILL → ELEVATED.
// Root model (not crutches):
// - H=1 never expands into open Air (lone cell stays; no infinite puddle).
// - H=1 may coalesce into a bay (Air with ≥2 fluid neighbours) — fills holes, no checkerboard wander.
// - H≥2 drains into adjacent hole, else BFS through fluid to nearest Air and DrainBase there
//   (interior towers must not deadlock when Chebyshev-distance-to-edge ≥ 2).
// - Elevated walks only to coalesce on the fluid surface when base cannot reach Air.
uint32_t UpdateFluidCA(VoxelWorld &world)
{
	PV_PROFILE_ZONE_N("UpdateFluidCA");

	if (world.stats.fluidVoxelCount == 0u) {
		return 0u;
	}

	const int width = world.width;
	const int height = world.height;
	const int depth = world.depth;

#if !defined(NDEBUG)
	PV_ASSERT(
		width > 0 && height > 0 && depth > 0,
		"VoxelWorld",
		"UpdateFluidCA",
		"world dimensions must be strictly positive");
#endif

	const auto index = [width, height](const int lx, const int ly, const int lz) -> size_t {
		return static_cast<size_t>(lx) + static_cast<size_t>(ly) * static_cast<size_t>(width) +
			   static_cast<size_t>(lz) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};

	const size_t totalCells = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
	std::vector<uint8_t> next(totalCells, 0u);
	std::vector<uint8_t> claimed(totalCells, 0u);

	const int readMinX = world.min.x;
	const int readMinY = world.min.y;
	const int readMinZ = world.min.z;
	const int readMaxX = world.maxExclusive.x;
	const int readMaxY = world.maxExclusive.y;
	const int readMaxZ = world.maxExclusive.z;

	const int simMinX = readMinX;
	const int simMinY = readMinY;
	const int simMinZ = readMinZ;
	const int simMaxX = readMaxX;
	const int simMaxY = readMaxY;
	const int simMaxZ = readMaxZ;

	profiling::PlotValue(
		"Fluid CA Cells Read",
		static_cast<int64_t>(readMaxX - readMinX) * (readMaxY - readMinY) * (readMaxZ - readMinZ));

	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.ReadPass");
		for (int z = readMinZ; z < readMaxZ; ++z) {
			for (int y = readMinY; y < readMaxY; ++y) {
				for (int x = readMinX; x < readMaxX; ++x) {
					const int lx = x - world.min.x;
					const int ly = y - world.min.y;
					const int lz = z - world.min.z;
					next[index(lx, ly, lz)] = ReadVoxelFromSparseStorage(world, {x, y, z});
				}
			}
		}
	}

	uint32_t moved = 0u;

	const auto isInsideLocal = [width, height, depth](const int lx, const int ly, const int lz) -> bool {
		return lx >= 0 && ly >= 0 && lz >= 0 && lx < width && ly < height && lz < depth;
	};

	const auto columnTopFrom = [&](const int lx, const int bottomLy, const int lz) -> int {
		int top = bottomLy;
		while (top + 1 < height && next[index(lx, top + 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
			++top;
		}
		return top;
	};

	const auto countFluidNeighborsAtY = [&](const int lx, const int ly, const int lz) -> int {
		int n = 0;
		for (const auto &side : kSideOrder) {
			const int nlx = lx + side[0];
			const int nlz = lz + side[1];
			if (!isInsideLocal(nlx, ly, nlz)) {
				continue;
			}
			if (next[index(nlx, ly, nlz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
				++n;
			}
		}
		return n;
	};

	const auto blocksDiagonalCorner = [&](const int fromLx, const int destLy, const int fromLz, const int dx,
										  const int dz) -> bool {
		if (dx == 0 || dz == 0) {
			return false;
		}
		if (!isInsideLocal(fromLx + dx, destLy, fromLz) || !isInsideLocal(fromLx, destLy, fromLz + dz)) {
			return true;
		}
		const uint8_t orthoA = next[index(fromLx + dx, destLy, fromLz)];
		const uint8_t orthoB = next[index(fromLx, destLy, fromLz + dz)];
		return IsSolidMaterial(orthoA) || IsSolidMaterial(orthoB);
	};

	// Placement legality for a dest cell (adjacency optional — BFS pressure uses non-adjacent).
	const auto isValidDest = [&](const int restLy, const int nlx, const int destLy, const int nlz,
								 const bool allowClaimedVacatedDest, const int fromLx, const int fromLz,
								 const bool requireAdjacent) -> bool {
		if (!isInsideLocal(nlx, destLy, nlz)) {
			return false;
		}
		if (destLy != restLy && destLy != restLy - 1) {
			return false;
		}
		if (requireAdjacent) {
			const int adx = nlx - fromLx;
			const int adz = nlz - fromLz;
			if (adx * adx + adz * adz != 1) {
				return false;
			}
		}
		const size_t destIdx = index(nlx, destLy, nlz);
		if (next[destIdx] != static_cast<uint8_t>(VoxelMaterial::Air)) {
			return false;
		}
		if (claimed[destIdx] != 0u && !allowClaimedVacatedDest) {
			return false;
		}
		if (requireAdjacent && blocksDiagonalCorner(fromLx, destLy, fromLz, nlx - fromLx, nlz - fromLz)) {
			return false;
		}
		if (destLy == restLy) {
			if (destLy == 0) {
				return true;
			}
			const size_t belowIdx = index(nlx, destLy - 1, nlz);
			const uint8_t below = next[belowIdx];
			if (!(IsSolidMaterial(below) || below == static_cast<uint8_t>(VoxelMaterial::Fluid))) {
				return false;
			}
			if (claimed[belowIdx] != 0u && below == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
				return false;
			}
			return true;
		}
		// M8 restY-1: do not step under a Solid at (nlx,restY,nlz)
		if (isInsideLocal(nlx, restLy, nlz) && IsSolidMaterial(next[index(nlx, restLy, nlz)])) {
			return false;
		}
		return true;
	};

	const auto sideStart = [](const int lx, const int ly, const int lz) -> int {
		const uint32_t h = static_cast<uint32_t>(lx) * 73856093u ^ static_cast<uint32_t>(ly) * 19349663u ^
						   static_cast<uint32_t>(lz) * 83492791u;
		return static_cast<int>(h & 3u);
	};

	// Adjacent hole pick: restY-1 first, then restY. Fixed rotated side order (no extent heuristic).
	const auto pickAdjacentHole = [&](const int lx, const int restLy, const int lz,
									  const bool allowClaimedVacatedDest, const bool allowSameLevel,
									  int &outNlx, int &outDestLy, int &outNlz) -> bool {
		const int start = sideStart(lx, restLy, lz);
		for (int destLy = restLy - 1; destLy <= restLy; ++destLy) {
			if (destLy < 0) {
				continue;
			}
			if (destLy == restLy && !allowSameLevel) {
				continue;
			}
			for (int d = 0; d < 4; ++d) {
				const auto &side = kSideOrder[(start + d) & 3];
				const int nlx = lx + side[0];
				const int nlz = lz + side[1];
				if (!isValidDest(restLy, nlx, destLy, nlz, allowClaimedVacatedDest, lx, lz, true)) {
					continue;
				}
				outNlx = nlx;
				outDestLy = destLy;
				outNlz = nlz;
				return true;
			}
		}
		return false;
	};

	const auto moveCell = [&](const int slx, const int sly, const int slz, const int dlx, const int dly,
							  const int dlz) {
		const size_t srcIdx = index(slx, sly, slz);
		const size_t destIdx = index(dlx, dly, dlz);
		next[srcIdx] = static_cast<uint8_t>(VoxelMaterial::Air);
		next[destIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
		claimed[srcIdx] = 1u;
		claimed[destIdx] = 1u;
		++moved;
	};

	const auto drainBase = [&](const int lx, const int bottomLy, const int lz, const int topLy, const int dlx,
							   const int dly, const int dlz) {
		const size_t destIdx = index(dlx, dly, dlz);
		next[destIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
		claimed[destIdx] = 1u;
		for (int y = bottomLy; y < topLy; ++y) {
			next[index(lx, y, lz)] = next[index(lx, y + 1, lz)];
			claimed[index(lx, y, lz)] = 1u;
		}
		next[index(lx, topLy, lz)] = static_cast<uint8_t>(VoxelMaterial::Air);
		claimed[index(lx, topLy, lz)] = 1u;
		++moved;
	};

	// BFS through same-level Fluid to nearest valid Air (restY or restY-1). Distance by hops.
	const auto findNearestAirViaFluid = [&](const int baseLx, const int restLy, const int baseLz, int &outNlx,
											int &outDestLy, int &outNlz) -> bool {
		const size_t layerCells = static_cast<size_t>(width) * static_cast<size_t>(depth);
		std::vector<uint8_t> visited(layerCells, 0u);
		std::queue<std::pair<int, int>> q;
		const auto flat = [width](const int lx, const int lz) -> size_t {
			return static_cast<size_t>(lx) + static_cast<size_t>(lz) * static_cast<size_t>(width);
		};
		visited[flat(baseLx, baseLz)] = 1u;
		q.push({baseLx, baseLz});
		while (!q.empty()) {
			const auto [cx, cz] = q.front();
			q.pop();
			const int start = sideStart(cx, restLy, cz);
			for (int d = 0; d < 4; ++d) {
				const auto &side = kSideOrder[(start + d) & 3];
				const int nx = cx + side[0];
				const int nz = cz + side[1];
				if (!isInsideLocal(nx, restLy, nz)) {
					continue;
				}
				// Prefer restY-1 then restY as dest from this frontier cell
				for (int destLy = restLy - 1; destLy <= restLy; ++destLy) {
					if (destLy < 0) {
						continue;
					}
					if (isValidDest(restLy, nx, destLy, nz, false, cx, cz, true)) {
						outNlx = nx;
						outDestLy = destLy;
						outNlz = nz;
						return true;
					}
				}
				if (next[index(nx, restLy, nz)] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				if (claimed[index(nx, restLy, nz)] != 0u) {
					continue;
				}
				const size_t f = flat(nx, nz);
				if (visited[f] != 0u) {
					continue;
				}
				visited[f] = 1u;
				q.push({nx, nz});
			}
		}
		return false;
	};

	const auto tryDrainColumn = [&](const int lx, const int ly, const int lz, const int topLy) -> bool {
		const int h = topLy - ly + 1;
		int nlx = 0;
		int destLy = 0;
		int nlz = 0;
		if (h >= 2) {
			if (pickAdjacentHole(lx, ly, lz, false, true, nlx, destLy, nlz)) {
				drainBase(lx, ly, lz, topLy, nlx, destLy, nlz);
				return true;
			}
			if (findNearestAirViaFluid(lx, ly, lz, nlx, destLy, nlz)) {
				if (!isValidDest(ly, nlx, destLy, nlz, false, lx, lz, false)) {
					return false;
				}
				drainBase(lx, ly, lz, topLy, nlx, destLy, nlz);
				return true;
			}
			return false;
		}
		// H=1: pits only (restY-1), not open-floor expand
		if (pickAdjacentHole(lx, ly, lz, false, false, nlx, destLy, nlz)) {
			moveCell(lx, ly, lz, nlx, destLy, nlz);
			return true;
		}
		return false;
	};

	// Phase 1: FALL
	for (int y = simMinY; y < simMaxY; ++y) {
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid) || claimed[idx] != 0u) {
					continue;
				}
				if (ly <= 0) {
					continue;
				}
				const size_t belowIdx = index(lx, ly - 1, lz);
				if (next[belowIdx] == static_cast<uint8_t>(VoxelMaterial::Air) && claimed[belowIdx] == 0u) {
					next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
					next[belowIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
					claimed[idx] = 1u;
					claimed[belowIdx] = 1u;
					++moved;
				}
			}
		}
	}

	// Phase 2: BASE_HOLE + PRESSURE_TO_AIR (H≥2 BFS)
	for (int y = simMinY; y < simMaxY; ++y) {
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid) || claimed[idx] != 0u) {
					continue;
				}
				if (ly > 0 && next[index(lx, ly - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				if (ly > 0 && next[index(lx, ly - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Air)) {
					continue;
				}
				const int topLy = columnTopFrom(lx, ly, lz);
				tryDrainColumn(lx, ly, lz, topLy);
			}
		}
	}

	// Phase 3: GAP_FILL — H=1 moves into Air bay (≥2 fluid neighbours). Fills holes; no frontier jet.
	for (int y = simMinY; y < simMaxY; ++y) {
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid) || claimed[idx] != 0u) {
					continue;
				}
				if (ly > 0 && next[index(lx, ly - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				if (ly > 0 && next[index(lx, ly - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Air)) {
					continue;
				}
				if (columnTopFrom(lx, ly, lz) != ly) {
					continue; // only height-1 bases
				}
				const int srcN = countFluidNeighborsAtY(lx, ly, lz);
				const int start = sideStart(lx, ly, lz);
				int bestNlx = 0;
				int bestNlz = 0;
				int bestDestN = -1;
				int bestD = 0;
				bool found = false;
				for (int d = 0; d < 4; ++d) {
					const auto &side = kSideOrder[(start + d) & 3];
					const int nlx = lx + side[0];
					const int nlz = lz + side[1];
					if (!isValidDest(ly, nlx, ly, nlz, false, lx, lz, true)) {
						continue;
					}
					const int destN = countFluidNeighborsAtY(nlx, ly, nlz);
					if (destN < 2) {
						continue; // open frontier Air (destN==1) is not a gap
					}
					if (!found || destN > bestDestN || (destN == bestDestN && d < bestD)) {
						found = true;
						bestNlx = nlx;
						bestNlz = nlz;
						bestDestN = destN;
						bestD = d;
					}
				}
				if (found && bestDestN > srcN) {
					moveCell(lx, ly, lz, bestNlx, ly, bestNlz);
				}
			}
		}
	}

	// Phase 4: ELEVATED — restY-1 first; same-level only to coalesce (destN > srcN)
	for (int y = simMinY; y < simMaxY; ++y) {
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid) || claimed[idx] != 0u) {
					continue;
				}
				if (ly <= 0 || next[index(lx, ly - 1, lz)] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				int nlx = 0;
				int destLy = 0;
				int nlz = 0;
				if (pickAdjacentHole(lx, ly, lz, false, false, nlx, destLy, nlz)) {
					moveCell(lx, ly, lz, nlx, destLy, nlz);
					continue;
				}
				const int srcN = countFluidNeighborsAtY(lx, ly, lz);
				if (!pickAdjacentHole(lx, ly, lz, false, true, nlx, destLy, nlz)) {
					continue;
				}
				if (destLy != ly) {
					moveCell(lx, ly, lz, nlx, destLy, nlz);
					continue;
				}
				const int destN = countFluidNeighborsAtY(nlx, ly, nlz);
				if (destN > srcN) {
					moveCell(lx, ly, lz, nlx, destLy, nlz);
				}
			}
		}
	}

	if (moved == 0u) {
		return 0u;
	}

	profiling::PlotValue("Fluid CA Cells Moved", static_cast<int64_t>(moved));
	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.Commit");
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int y = simMinY; y < simMaxY; ++y) {
				for (int x = simMinX; x < simMaxX; ++x) {
					const int lx = x - world.min.x;
					const int ly = y - world.min.y;
					const int lz = z - world.min.z;
					const size_t idx = index(lx, ly, lz);
					const VoxelMaterial currentMaterial = static_cast<VoxelMaterial>(next[idx]);
					const VoxelMaterial previousMaterial = GetVoxelMaterial(world, {x, y, z});
					if (previousMaterial == currentMaterial) {
						continue;
					}
					SetVoxelMaterial(world, {x, y, z}, currentMaterial);
				}
			}
		}
	}

#if !defined(NDEBUG)
	{
		uint32_t actualFluidCount = 0u;
		for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
			for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
				for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
					if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
						++actualFluidCount;
					}
				}
			}
		}
		PV_ASSERT(
			actualFluidCount == world.stats.fluidVoxelCount,
			"VoxelWorld",
			"UpdateFluidCA",
			"stats.fluidVoxelCount diverged from actual fluid voxel count");
	}
#endif

	return moved;
}
