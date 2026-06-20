#pragma once
// Voxel scene generator + greedy meshing for vis-buffer prototype.
// Mirrors ProjectV's PackedFace layout (see src/shaders/voxel.vert + voxel_mesh.comp).
// Each "voxel face" after greedy meshing is a quad → 2 triangles → 6 vertices.
// We pack each quad with (chunkIdx, voxelIdx, face, extentsU, extentsV) so the
// vertex shader can reconstruct world position. Material is per-face (per quad).

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace vb {

// Matches ProjectV's PackedFace struct layout (4 uint32 words = 16 bytes).
struct PackedFace {
	uint32_t localVoxelFace;	 // [faceIdx(8) | z(8) | y(8) | x(8)]
	uint32_t chunkIndexMaterial; // [materialIdx(8) | chunkIdx(24)]
	uint32_t lightingData;		 // reserved
	uint32_t packedExtents;		 // [extentsV(8) | extentsU(8) | _reserved(16)]
};

struct ChunkDescriptor {
	std::array<int32_t, 4> chunkOrigin;			  // (x, y, z, _)
	std::array<uint32_t, 4> chunkExtentAndNonAir; // (extentX, extentY, extentZ, nonAir)
	std::array<uint32_t, 4> voxelDataInfo;		  // (offsetInWords, _, _, _)
	std::array<uint32_t, 4> drawRanges;			  // (firstInstance, instanceCount, _, _)
};

// Material visual: matches ProjectV's VoxelMaterialVisual (64 bytes / 4×vec4).
struct MaterialVisual {
	std::array<float, 4> baseColor{};
	std::array<float, 4> surface{};
	std::array<float, 4> medium{};
	std::array<float, 4> shading{};
};

struct SceneConfig {
	uint32_t chunkDim = 8;		// 8×8×8 voxels per chunk (ProjectV default).
	uint32_t chunksPerSide = 4; // 4×4×4 = 64 chunks = ~32K voxels.
	uint32_t materialCount = 5; // start: 5 materials like ProjectV.
	uint32_t seed = 0xDEADBEEF;
};

// Generate a synthetic voxel scene (procedural — small hills + cube structures).
// Returns:
//   chunks: vector of chunks, each 8×8×8 = 512 bytes (1 byte/voxel, 0=air, 1..N=material).
//   faces: pre-greedy-meshing output (one PackedFace per visible quad after greedy merge).
//   materials: material palette (size = materialCount).
void GenerateScene(const SceneConfig &cfg,
				   std::vector<std::vector<uint8_t>> &chunksOut,
				   std::vector<PackedFace> &facesOut,
				   std::vector<ChunkDescriptor> &chunkDescriptorsOut,
				   std::vector<MaterialVisual> &materialsOut);

} // namespace vb
