#pragma once

#include "voxel/VoxelWorldAscii.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

struct VoxelAsciiTickLogger {
	bool Enable(std::string path, VoxelAsciiOptions options = {});
	void Disable();
	[[nodiscard]] bool IsEnabled() const noexcept;

	void OnSimulationTick(const VoxelWorld &world, uint64_t simulationTick);
	void OnSimulationTickTo(const VoxelWorld &world, uint64_t simulationTick, std::ostream &out);

  private:
	void WriteTickDiff(const VoxelWorld &world, uint64_t simulationTick, std::ostream &out);

	bool enabled_ = false;
	bool legendWritten_ = false;
	VoxelAsciiOptions options_{};
	std::unique_ptr<std::ofstream> file_{}; // heap: ofstream ctor may throw; keep logger noexcept-friendly
	VoxelAsciiBounds lastBounds_{};
	bool hasLastBounds_ = false;
	std::vector<uint64_t> layerHash_{};
	std::vector<uint8_t> layerHashValid_{}; // 1 = slot has cached hash
};

void MaybeLogVoxelAsciiTick(const VoxelWorld &world, uint64_t simulationTick);
