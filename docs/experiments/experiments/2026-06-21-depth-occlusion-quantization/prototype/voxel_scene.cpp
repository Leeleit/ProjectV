#include "voxel_scene.hpp"

#include <array>
#include <cstring>
#include <random>

namespace depth_quant {

bool Chunk::isEmpty() const {
    for (const auto& v : voxels) {
        if (v.solid()) return false;
    }
    return true;
}

int Chunk::materialCount() const {
    std::array<int, 256> hist{};
    for (const auto& v : voxels) {
        hist[v.material]++;
    }
    int n = 0;
    for (int c : hist) if (c > 0) ++n;
    return n;
}

void fillSceneForest(Scene& scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> mat(1, 3);
    for (int cz = 0; cz < Scene::CHUNK_DIM; ++cz) {
        for (int cy = 0; cy < Scene::CHUNK_DIM; ++cy) {
            for (int cx = 0; cx < Scene::CHUNK_DIM; ++cx) {
                auto& c = scene.chunkAt(cx, cy, cz);
                for (int z = 0; z < Chunk::SIZE; ++z) {
                    for (int y = 0; y < Chunk::SIZE; ++y) {
                        for (int x = 0; x < Chunk::SIZE; ++x) {
                            int gx = cx * Chunk::SIZE + x;
                            int gy = cy * Chunk::SIZE + y;
                            int gz = cz * Chunk::SIZE + z;
                            bool ground = (gy < 3);
                            bool tree = (gy < 6) && (gx % 3 == 0) && (gz % 3 == 0);
                            bool leaves = (gy >= 5) && (gy < 7) && (std::abs(gx - 4) < 3) && (std::abs(gz - 4) < 3);
                            if (ground) c.at(x, y, z).material = 1;
                            else if (leaves) c.at(x, y, z).material = 2;
                            else if (tree) c.at(x, y, z).material = 3;
                            else c.at(x, y, z).material = 0;
                        }
                    }
                }
            }
        }
    }
    (void)mat; (void)rng;
}

void fillSceneCave(Scene& scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> noise(0.0f, 1.0f);
    for (int cz = 0; cz < Scene::CHUNK_DIM; ++cz) {
        for (int cy = 0; cy < Scene::CHUNK_DIM; ++cy) {
            for (int cx = 0; cx < Scene::CHUNK_DIM; ++cx) {
                auto& c = scene.chunkAt(cx, cy, cz);
                for (int z = 0; z < Chunk::SIZE; ++z) {
                    for (int y = 0; y < Chunk::SIZE; ++y) {
                        for (int x = 0; x < Chunk::SIZE; ++x) {
                            float n = noise(rng);
                            bool solid = (n > 0.45f);
                            c.at(x, y, z).material = solid ? 1 : 0;
                        }
                    }
                }
            }
        }
    }
}

void fillSceneUniform(Scene& scene, std::uint32_t seed) {
    for (auto& c : scene.chunks) {
        for (auto& v : c.voxels) v.material = (v.material == 0) ? 1 : 1;
    }
    for (auto& c : scene.chunks) {
        for (auto& v : c.voxels) v.material = 1;
    }
    (void)seed;
}

void fillSceneMixed(Scene& scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> mat(1, 4);
    for (auto& c : scene.chunks) {
        for (auto& v : c.voxels) v.material = (mat(rng) < 3) ? 0 : mat(rng);
    }
}

const char* sceneName(int idx) {
    static const char* names[] = {"forest", "cave", "uniform", "mixed"};
    return names[idx];
}

int sceneCount() { return 4; }

const char* depthFormatName(DepthFormat fmt) {
    switch (fmt) {
        case DepthFormat::D32_SFLOAT: return "D32_SFLOAT";
        case DepthFormat::D16_UNORM: return "D16_UNORM";
        case DepthFormat::D16_UNORM_REVERSE_Z: return "D16_UNORM_REVERSE_Z";
    }
    return "unknown";
}

const char* cullPatternName(CullPattern pat) {
    switch (pat) {
        case CullPattern::HZB_MIPCHAIN: return "HZB_MIPCHAIN";
        case CullPattern::DIRECT_DEPTH: return "DIRECT_DEPTH";
    }
    return "unknown";
}

}  // namespace depth_quant
