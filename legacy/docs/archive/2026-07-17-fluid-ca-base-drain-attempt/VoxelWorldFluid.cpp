#include "core/Math.hpp"
#include "core/EnvUtils.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldInternal.hpp"
#include "debug/Profiling.hpp"
#include "core/RuntimeDiagnostics.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {
bool gFluidCaGpuEnabledForTesting = false;
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

// Binary Fluid CA with base-drain: any cell of a column height>=2 can trigger drain of
// the base cell (column shifts down in-tick). Placement prefers a direct horizontal
// move at base level before placeYOnColumn (avoids breaking on a glass ring above the floor).
// fluidFill stays 0/255 via SetVoxelMaterial until Phase 2 height rendering.
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

	const auto index = [width, height](const int x, const int y, const int z) -> size_t {
		return static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(width) +
			   static_cast<size_t>(z) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};

	const size_t totalCells = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
	std::vector<uint8_t> next(totalCells, 0u);

	int readMinX = world.fluidCAAabbMin.x - 1;
	int readMinZ = world.fluidCAAabbMin.z - 1;
	int readMaxX = world.fluidCAAabbMaxExclusive.x + 1;
	int readMaxZ = world.fluidCAAabbMaxExclusive.z + 1;
	if (readMinX < world.min.x) {
		readMinX = world.min.x;
	}
	if (readMinZ < world.min.z) {
		readMinZ = world.min.z;
	}
	if (readMaxX > world.maxExclusive.x) {
		readMaxX = world.maxExclusive.x;
	}
	if (readMaxZ > world.maxExclusive.z) {
		readMaxZ = world.maxExclusive.z;
	}
	const int readMinY = world.min.y; // full vertical: connected column can span floor→sphere
	const int readMaxY = world.maxExclusive.y;

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

	std::vector<uint8_t> claimed(totalCells, 0u);
	uint32_t movedCount = 0u;
	// #region agent log
	uint32_t dbgFallMoves = 0u;
	uint32_t dbgDrainDirect = 0u;
	uint32_t dbgDrainStack = 0u;
	uint32_t dbgGateReject = 0u;
	uint32_t dbgFluidBefore = world.stats.fluidVoxelCount;
	// #endregion

	const auto isSolid = [](const uint8_t material) -> bool {
		return material != static_cast<uint8_t>(VoxelMaterial::Air) &&
			   material != static_cast<uint8_t>(VoxelMaterial::Fluid);
	};

	for (int z = simMinZ; z < simMaxZ; ++z) {
		for (int y = simMinY; y < simMaxY; ++y) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				if (claimed[idx] != 0u) {
					continue;
				}

				if (ly > 0) {
					const size_t belowIdx = index(lx, ly - 1, lz);
					if (next[belowIdx] == static_cast<uint8_t>(VoxelMaterial::Air) && claimed[belowIdx] == 0u) {
						next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
						next[belowIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
						claimed[idx] = 1u;
						claimed[belowIdx] = 1u;
						++movedCount;
						++dbgFallMoves; // #region agent log
						continue;
					}
				}

				const auto columnHeightAt = [&](const int colX, const int colZ, const int surfaceLy) -> int {
					if (surfaceLy < 0 || surfaceLy >= height ||
						next[index(colX, surfaceLy, colZ)] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
						return 0;
					}
					int bottom = surfaceLy;
					while (bottom > 0 &&
						   next[index(colX, bottom - 1, colZ)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
						--bottom;
					}
					int top = surfaceLy;
					while (top + 1 < height &&
						   next[index(colX, top + 1, colZ)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
						++top;
					}
					return top - bottom + 1;
				};

				const auto placeYOnColumn = [&](const int colX, const int colZ, const int maxLy) -> int {
					int scanY = 0;
					while (scanY < height) {
						if (next[index(colX, scanY, colZ)] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
							++scanY;
							continue;
						}
						int top = scanY;
						while (top + 1 < height &&
							   next[index(colX, top + 1, colZ)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
							++top;
						}
						const int place = top + 1;
						if (place < height && place <= maxLy &&
							next[index(colX, place, colZ)] == static_cast<uint8_t>(VoxelMaterial::Air)) {
							return place;
						}
						scanY = top + 1;
					}
					for (int place = maxLy; place >= 1; --place) {
						const uint8_t atPlace = next[index(colX, place, colZ)];
						if (isSolid(atPlace)) {
							break; // do not tunnel through glass/opaque to a pocket below
						}
						if (atPlace != static_cast<uint8_t>(VoxelMaterial::Air)) {
							continue;
						}
						const uint8_t below = next[index(colX, place - 1, colZ)];
						if (isSolid(below) || below == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
							return place; // rest on solid/fluid; never teleport to unsupported y=0
						}
					}
					return -1;
				};

				int bottomY = ly;
				while (bottomY > 0 &&
					   next[index(lx, bottomY - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					--bottomY;
				}
				int topY = ly;
				while (topY + 1 < height &&
					   next[index(lx, topY + 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					++topY;
				}
				const int sourceHeight = topY - bottomY + 1; // full contiguous column; interior may trigger base drain
				if (sourceHeight < 2) {
					continue; // lone / flat puddle stays
				}

				{
					const uint32_t h = lx * 73856093u ^ ly * 19349663u ^ lz * 83492791u;
					constexpr int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
					const int startSide = static_cast<int>(h & 0x3u);
					for (int d = 0; d < 4; ++d) {
						const int sideIdx = (startSide + d) & 0x3;
						const int nlx = lx + sides[sideIdx][0];
						const int nlz = lz + sides[sideIdx][1];
						if (nlx < 0 || nlx >= width || nlz < 0 || nlz >= depth) {
							continue;
						}

						int candidateLy = -1;
						if (next[index(nlx, bottomY, nlz)] == static_cast<uint8_t>(VoxelMaterial::Air)) {
							const uint8_t belowBase = (bottomY > 0) ? next[index(nlx, bottomY - 1, nlz)]
																	: static_cast<uint8_t>(VoxelMaterial::FloorWhite);
							if (bottomY == 0 || isSolid(belowBase) ||
								belowBase == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
								candidateLy = bottomY; // direct horizontal at base — no glass-tunnel, exits where column rests
							}
						}
						if (candidateLy < 0) {
							candidateLy = placeYOnColumn(nlx, nlz, topY); // fall back: stack on neighbour fluid, capped at surface
						}
						if (candidateLy < 0 || candidateLy > topY) {
							continue; // I2/I4: no support/tunnel, or would climb above column surface
						}

						int destHeight = 0;
						if (candidateLy > 0 &&
							next[index(nlx, candidateLy - 1, nlz)] == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
							destHeight = columnHeightAt(nlx, nlz, candidateLy - 1);
						}
						if (candidateLy == topY) {
							// I5: ΔΦ_y=0 (placeY==topY) — require Φ_h drop, or one-way lex on ΔΦ_h==0 (2→1)
							if (destHeight == 0) {
								// empty column at same surface Y: Φ_h decreases for src>=2
							} else if (sourceHeight >= destHeight + 2) {
								// strict Φ_h = Σh² decrease
							} else if (sourceHeight == destHeight + 1 &&
									   (lx > nlx || (lx == nlx && lz > nlz))) {
								// tie-break: one-way equalize, kills 2↔1 period-2
							} else {
								++dbgGateReject; // #region agent log
								continue;
							}
						}

						const size_t neighbourIdx = index(nlx, candidateLy, nlz);
						if (claimed[neighbourIdx] != 0u ||
							next[neighbourIdx] != static_cast<uint8_t>(VoxelMaterial::Air)) {
							continue;
						}

						const bool wasDirectBase = (candidateLy == bottomY); // #region agent log
						for (int shiftY = bottomY; shiftY < topY; ++shiftY) {
							next[index(lx, shiftY, lz)] = next[index(lx, shiftY + 1, lz)];
							claimed[index(lx, shiftY, lz)] = 1u;
						}
						next[index(lx, topY, lz)] = static_cast<uint8_t>(VoxelMaterial::Air); // column sank: base exited, surface dropped
						claimed[index(lx, topY, lz)] = 1u;
						next[neighbourIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
						claimed[neighbourIdx] = 1u;
						++movedCount;
						if (wasDirectBase) { // #region agent log
							++dbgDrainDirect;
						} else {
							++dbgDrainStack;
						}
						break;
					}
				}
			}
		}
	}

	// #region agent log
	{
		static uint32_t dbgTick = 0u;
		++dbgTick;
		uint32_t fluidAfter = 0u;
		uint32_t floating = 0u;
		uint32_t colsH2 = 0u;
		int maxH = 0;
		uint32_t h1BesideEmpty = 0u; // height-1 fluid with empty supported cardinal neighbour
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int lz = z - world.min.z;
				int h = 0;
				int bottom = -1;
				for (int y = simMinY; y < simMaxY; ++y) {
					const int ly = y - world.min.y;
					const size_t idx = index(lx, ly, lz);
					if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
						if (h > 0) {
							if (h > maxH) {
								maxH = h;
							}
							if (h >= 2) {
								++colsH2;
							}
							if (h == 1 && bottom >= 0) {
								constexpr int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
								for (int s = 0; s < 4; ++s) {
									const int nlx = lx + sides[s][0];
									const int nlz = lz + sides[s][1];
									if (nlx < 0 || nlx >= width || nlz < 0 || nlz >= depth) {
										continue;
									}
									if (next[index(nlx, bottom, nlz)] != static_cast<uint8_t>(VoxelMaterial::Air)) {
										continue;
									}
									const uint8_t belowN = (bottom > 0) ? next[index(nlx, bottom - 1, nlz)]
																		: static_cast<uint8_t>(VoxelMaterial::FloorWhite);
									if (bottom == 0 || isSolid(belowN) ||
										belowN == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
										++h1BesideEmpty;
										break;
									}
								}
							}
							h = 0;
							bottom = -1;
						}
						continue;
					}
					++fluidAfter;
					if (ly > 0 && next[index(lx, ly - 1, lz)] == static_cast<uint8_t>(VoxelMaterial::Air)) {
						++floating;
					}
					if (h == 0) {
						bottom = ly;
					}
					++h;
				}
				if (h > 0) {
					if (h > maxH) {
						maxH = h;
					}
					if (h >= 2) {
						++colsH2;
					}
				}
			}
		}
		if ((dbgTick % 5u) == 0u || movedCount > 0u) {
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
								std::chrono::system_clock::now().time_since_epoch())
								.count();
			if (FILE *dbg = std::fopen("C:/Users/le1t/Projects/ProjectV/debug-49630e.log", "a")) {
				std::fprintf(
					dbg,
					"{\"sessionId\":\"49630e\",\"timestamp\":%lld,\"location\":\"VoxelWorldFluid.cpp:UpdateFluidCA\","
					"\"message\":\"fluid tick metrics\",\"hypothesisId\":\"A-E\",\"data\":{"
					"\"tick\":%u,\"fluidBefore\":%u,\"fluidAfter\":%u,\"moved\":%u,"
					"\"fall\":%u,\"drainDirect\":%u,\"drainStack\":%u,\"gateReject\":%u,"
					"\"maxH\":%d,\"colsH2\":%u,\"floating\":%u,\"h1BesideEmpty\":%u}}\n",
					static_cast<long long>(ms), dbgTick, dbgFluidBefore, fluidAfter, movedCount, dbgFallMoves,
					dbgDrainDirect, dbgDrainStack, dbgGateReject, maxH, colsH2, floating, h1BesideEmpty);
				std::fclose(dbg);
			}
		}
	}
	// #endregion

	if (movedCount == 0u) {
		return 0u;
	}

	profiling::PlotValue("Fluid CA Cells Moved", static_cast<int64_t>(movedCount));
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
					SetVoxelMaterial(world, {x, y, z}, currentMaterial); // also syncs fluidFill 0/255
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

	return movedCount;
}
