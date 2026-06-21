#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace depth_quant {

enum class DepthFormat : std::uint32_t {
    D32_SFLOAT = 0,
    D16_UNORM = 1,
    D16_UNORM_REVERSE_Z = 2,
};

enum class CullPattern : std::uint32_t {
    HZB_MIPCHAIN = 0,
    DIRECT_DEPTH = 1,
};

struct Voxel {
    std::uint8_t material;
    bool solid() const { return material != 0; }
};

struct Chunk {
    static constexpr int SIZE = 8;
    Voxel voxels[SIZE * SIZE * SIZE];

    Voxel& at(int x, int y, int z) { return voxels[(z * SIZE + y) * SIZE + x]; }
    const Voxel& at(int x, int y, int z) const { return voxels[(z * SIZE + y) * SIZE + x]; }

    bool isEmpty() const;
    int materialCount() const;
};

struct Scene {
    static constexpr int CHUNK_DIM = 4;  // 4x4x4 chunks = 32^3 voxels
    Chunk chunks[CHUNK_DIM * CHUNK_DIM * CHUNK_DIM];

    Chunk& chunkAt(int cx, int cy, int cz) {
        return chunks[(cz * CHUNK_DIM + cy) * CHUNK_DIM + cx];
    }
    const Chunk& chunkAt(int cx, int cy, int cz) const {
        return chunks[(cz * CHUNK_DIM + cy) * CHUNK_DIM + cx];
    }
};

void fillSceneForest(Scene& scene, std::uint32_t seed);
void fillSceneCave(Scene& scene, std::uint32_t seed);
void fillSceneUniform(Scene& scene, std::uint32_t seed);
void fillSceneMixed(Scene& scene, std::uint32_t seed);

const char* sceneName(int idx);
int sceneCount();
const char* depthFormatName(DepthFormat fmt);
const char* cullPatternName(CullPattern pat);

}  // namespace depth_quant
