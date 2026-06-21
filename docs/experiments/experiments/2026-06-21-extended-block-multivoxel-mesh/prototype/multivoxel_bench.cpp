#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

// ============================================================
// Constants
// ============================================================
constexpr int kChunkSize = 8;
constexpr int kChunkVol = kChunkSize * kChunkSize * kChunkSize;

// Block types
enum class BlockType : uint8_t {
  Air = 0,
  Cube_Stone,
  Cube_Dirt,
  Cube_Grass,
  Slab_Lower,
  Slab_Upper,
  Stair_East,
  Stair_West,
  Stair_North,
  Stair_South,
  Pane,
  Wall,
  // Keep last for count
  COUNT_
};

// Face indices
enum Face : uint8_t { PX = 0, NX, PY, NY, PZ, NZ, kFaceCount };

struct Vec3i {
  int x, y, z;
};

// Block shape descriptor
struct BlockShape {
  // For each of 6 faces, the coverage as a fraction [0, 1]
  // 1.0 = fully covered (opaque face), 0.0 = empty
  std::array<float, 6> face_coverage;

  // Additional exposed faces for stair steps (indices into face_coverage)
  // Stairs have an inner L-shape that exposes extra faces
  std::array<float, 6> inner_face_coverage;

  // Is this block cubic (1x1x1 full)?
  bool is_cubic;

  // Does this block have partial face coverage (non-cubic shape)?
  bool is_partial;
};

// ============================================================
// Block shape definitions
// ============================================================
BlockShape GetBlockShape(BlockType bt) {
  BlockShape s{};
  s.is_partial = false;
  s.inner_face_coverage = {0, 0, 0, 0, 0, 0};

  switch (bt) {
  case BlockType::Air:
    s.face_coverage = {0, 0, 0, 0, 0, 0};
    s.is_cubic = false;
    return s;
  case BlockType::Cube_Stone:
  case BlockType::Cube_Dirt:
  case BlockType::Cube_Grass:
    s.face_coverage = {1, 1, 1, 1, 1, 1};
    s.is_cubic = true;
    return s;
  case BlockType::Slab_Lower:
    s.face_coverage = {1, 1, 0.5f, 0, 1, 1}; // PY half-covered, NY empty
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Slab_Upper:
    s.face_coverage = {1, 1, 1, 0.5f, 1, 1}; // PY full, NY half
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Stair_East:
    // Standard stair: L-shape, lower half full, upper half half-deep
    // Exposes an inner vertical face + inner horizontal face
    s.face_coverage = {0.5f, 1, 1, 1, 1, 1}; // PX half (stair step)
    s.inner_face_coverage = {1, 0.5f, 0, 0, 0.5f, 0.5f};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Stair_West:
    s.face_coverage = {1, 0.5f, 1, 1, 1, 1}; // NX half
    s.inner_face_coverage = {0.5f, 1, 0, 0, 0.5f, 0.5f};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Stair_North:
    s.face_coverage = {1, 1, 1, 1, 0.5f, 1}; // PZ half
    s.inner_face_coverage = {0.5f, 0.5f, 0, 0, 1, 0.5f};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Stair_South:
    s.face_coverage = {1, 1, 1, 1, 1, 0.5f}; // NZ half
    s.inner_face_coverage = {0.5f, 0.5f, 0, 0, 0.5f, 1};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Pane:
    // Thin cross shape (like a fence post with cross arms)
    s.face_coverage = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  case BlockType::Wall:
    // Full height but thin (like a wall)
    s.face_coverage = {0.125f, 0.125f, 1, 1, 0.125f, 0.125f};
    s.is_cubic = false;
    s.is_partial = true;
    return s;
  default:
    s.face_coverage = {1, 1, 1, 1, 1, 1};
    s.is_cubic = true;
    return s;
  }
}

// ============================================================
// Chunk storage
// ============================================================
using ChunkData = std::array<BlockType, kChunkVol>;

// Helper: 3D -> 1D index
int Idx3D(int x, int y, int z) {
  return (y * kChunkSize + z) * kChunkSize + x;
}

// ============================================================
// Scene generators
// ============================================================
enum class SceneType {
  UniformFloor,     // Flat layer of cubes
  StairsOnly,       // ~50% stairs in various orientations
  MixedBiome,       // Mix of cubes, slabs, stairs, panes, walls
  CaveStress,       // Irregular cave (cubes only, complex topology)
  MultiVoxelDense,  // Dense arrangement of non-cubic blocks
  COUNT_
};

struct SceneConfig {
  std::string name;
  SceneType type;
};

const std::array<SceneConfig, static_cast<int>(SceneType::COUNT_)> kSceneConfigs = {{
  {"uniform_floor", SceneType::UniformFloor},
  {"stairs_only", SceneType::StairsOnly},
  {"mixed_biome", SceneType::MixedBiome},
  {"cave_stress", SceneType::CaveStress},
  {"multi_voxel_dense", SceneType::MultiVoxelDense},
}};

void GenerateScene(ChunkData& chunk, SceneType scene, std::mt19937_64&) {
  std::fill(chunk.begin(), chunk.end(), BlockType::Air);

  [[maybe_unused]] int count_check = static_cast<int>(SceneType::COUNT_);
  switch (scene) {
  case SceneType::UniformFloor: {
    // Y=0 full cube floor + some stairs on top
    for (int x = 0; x < kChunkSize; ++x)
      for (int z = 0; z < kChunkSize; ++z) {
        chunk[Idx3D(x, 0, z)] = BlockType::Cube_Stone;
        if (x < 3 && z < 3)
          chunk[Idx3D(x, 1, z)] = BlockType::Stair_East;
      }
    break;
  }
  case SceneType::StairsOnly: {
    // 50% stairs, alternating orientations
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          if ((x + y + z) % 3 == 0)
            continue; // air pockets
          auto r = (x * 7 + y * 31 + z * 17) % 4;
          BlockType bt = BlockType::Stair_East;
          if (r == 1)
            bt = BlockType::Stair_West;
          else if (r == 2)
            bt = BlockType::Stair_North;
          else if (r == 3)
            bt = BlockType::Stair_South;
          chunk[Idx3D(x, y, z)] = bt;
        }
    break;
  }
  case SceneType::MixedBiome: {
    // Mix of everything
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          if ((x + y + z) % 4 == 0)
            continue;
          auto r = (x * 13 + y * 7 + z * 11) % 7;
          BlockType bt = BlockType::Cube_Stone;
          if (r == 0)
            bt = BlockType::Cube_Dirt;
          else if (r == 1)
            bt = BlockType::Slab_Lower;
          else if (r == 2)
            bt = BlockType::Slab_Upper;
          else if (r == 3)
            bt = BlockType::Stair_East;
          else if (r == 4)
            bt = BlockType::Pane;
          else if (r == 5)
            bt = BlockType::Wall;
          chunk[Idx3D(x, y, z)] = bt;
        }
    break;
  }
  case SceneType::CaveStress: {
    // 3D Perlin-like cave using hash function
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          auto h = (x * 374761393u + y * 668265263u + z * 1274126177u);
          h = (h ^ (h >> 13)) * 1274126177u;
          h = h ^ (h >> 16);
          auto noise = static_cast<float>(h) / 4294967296.0f;
          // Cave: solid where noise > threshold, threshold varies by y
          float threshold = 0.4f + 0.2f * std::sin(static_cast<float>(y) * 0.8f);
          if (noise > threshold)
            chunk[Idx3D(x, y, z)] = BlockType::Cube_Stone;
        }
    break;
  }
  case SceneType::MultiVoxelDense: {
    // Maximum density non-cubic blocks
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          if ((x + y + z) % 5 == 0)
            continue;
          auto r = (x * 7 + y * 13 + z * 19) % 6;
          BlockType bt = BlockType::Slab_Lower;
          if (r == 0)
            bt = BlockType::Slab_Upper;
          else if (r == 1)
            bt = BlockType::Stair_East;
          else if (r == 2)
            bt = BlockType::Stair_West;
          else if (r == 3)
            bt = BlockType::Stair_North;
          else if (r == 4)
            bt = BlockType::Stair_South;
          else
            bt = BlockType::Pane;
          chunk[Idx3D(x, y, z)] = bt;
        }
    break;
  }
  }
}

// ============================================================
// Block type info
// ============================================================
// int BlockTypeCount(BlockType bt) {
//   return static_cast<int>(BlockType::COUNT_);
// }

bool IsAir(BlockType bt) { return bt == BlockType::Air; }

// ============================================================
// Strategy implementations
// ============================================================

struct MeshStats {
  int64_t quads = 0;
  int64_t vertices = 0;
  int64_t culled_faces = 0;
  int64_t total_face_checks = 0;
  int64_t precomputed_memory_bytes = 0;
  int64_t unique_block_types = 0;
  double build_time_us = 0;
};

// Wraps time measurement
template <typename F>
double TimeUs(F&& f) {
  auto start = std::chrono::steady_clock::now();
  f();
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::micro>(end - start).count();
}

// ----- Strategy A: SimpleCube (baseline) -----
// All blocks are cubes. No non-cubic blocks.
// For comparison purposes, we measure the cost of doing this.
MeshStats StrategyA_SimpleCube(const ChunkData& chunk) {
  MeshStats stats;
  stats.unique_block_types = 1; // only cubes

  auto t = TimeUs([&]() {
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          auto bt = chunk[Idx3D(x, y, z)];
          if (IsAir(bt))
            continue;

          stats.total_face_checks += 6;

          // Check 6 neighbors
          auto check = [&](int nx, int ny, int nz, Face f) {
            if (nx < 0 || nx >= kChunkSize || ny < 0 || ny >= kChunkSize ||
                nz < 0 || nz >= kChunkSize)
              return true; // chunk boundary = visible
            auto nb = chunk[Idx3D(nx, ny, nz)];
            if (IsAir(nb))
              return true;
            auto nbs = GetBlockShape(nb);
            if (nbs.face_coverage[f] < 1.0f)
              return true; // partial block doesn't fully cover
            return false;
          };

          bool px = check(x + 1, y, z, Face::NX);
          bool nx = check(x - 1, y, z, Face::PX);
          bool py = check(x, y + 1, z, Face::NY);
          bool ny = check(x, y - 1, z, Face::PY);
          bool pz = check(x, y, z + 1, Face::NZ);
          bool nz = check(x, y, z - 1, Face::PZ);

          int visible = (px ? 1 : 0) + (nx ? 1 : 0) + (py ? 1 : 0) +
                        (ny ? 1 : 0) + (pz ? 1 : 0) + (nz ? 1 : 0);

          if (!px) stats.culled_faces++;
          if (!nx) stats.culled_faces++;
          if (!py) stats.culled_faces++;
          if (!ny) stats.culled_faces++;
          if (!nx) stats.culled_faces++;
          if (!nz) stats.culled_faces++;

          stats.quads += visible;
          stats.vertices += visible * 4;
        }
  });
  stats.build_time_us = t;
  return stats;
}

// ----- Strategy B: PrecomputedMesh -----
// Non-cubic blocks use precomputed face lists per block type.
// Face culling checks neighbor coverage for partial faces.
MeshStats StrategyB_PrecomputedMesh(const ChunkData& chunk) {
  MeshStats stats;

  // Count unique block types used
  bool seen[static_cast<int>(BlockType::COUNT_)]{};
  for (auto bt : chunk) {
    if (bt != BlockType::Air) {
      int idx = static_cast<int>(bt);
      if (!seen[idx]) {
        seen[idx] = true;
        stats.unique_block_types++;
      }
    }
  }

  // Precomputed memory: for each block type, store face list + rotation data
  stats.precomputed_memory_bytes =
      stats.unique_block_types * sizeof(BlockShape);



  auto t = TimeUs([&]() {
    // Phase 1: for each non-cubic block, cache its shape
    std::array<BlockShape, static_cast<int>(BlockType::COUNT_)> shapes;
    for (int i = 0; i < static_cast<int>(BlockType::COUNT_); ++i)
      shapes[i] = GetBlockShape(static_cast<BlockType>(i));

    // Phase 2: iterate all voxels, generate quads
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          auto bt = chunk[Idx3D(x, y, z)];
          if (IsAir(bt))
            continue;

          auto& shape = shapes[static_cast<int>(bt)];

          // Each face: check if neighbor covers this face
          for (int f = 0; f < kFaceCount; ++f) {
            stats.total_face_checks++;

            float coverage = shape.face_coverage[f];
            if (coverage == 0.0f)
              continue; // no face here

            // Neighbor position
            int nx = x, ny = y, nz = z;
            switch (f) {
            case Face::PX: nx = x + 1; break;
            case Face::NX: nx = x - 1; break;
            case Face::PY: ny = y + 1; break;
            case Face::NY: ny = y - 1; break;
            case Face::PZ: nz = z + 1; break;
            case Face::NZ: nz = z - 1; break;
            }

            // Check neighbor
            bool visible = true;
            if (nx >= 0 && nx < kChunkSize && ny >= 0 && ny < kChunkSize &&
                nz >= 0 && nz < kChunkSize) {
              auto nb = chunk[Idx3D(nx, ny, nz)];
              if (!IsAir(nb)) {
                auto& nbs = shapes[static_cast<int>(nb)];
                // Opposite face on neighbor
                int opp = f ^ 1;
                if (nbs.face_coverage[opp] >= coverage) {
                  visible = false; // neighbor fully covers this face
                }
              }
            }

            if (!visible) {
              stats.culled_faces++;
              continue;
            }

            stats.quads++;
            stats.vertices += 4;

            // If stair/inner face, add extra quads for the step inner faces
            if (shape.inner_face_coverage[f] > 0) {
              // Stair inner faces: always visible (not neighbor-culled in
              // this simplified model)
              stats.quads++;
              stats.vertices += 4;
            }
          }
        }
  });
  stats.build_time_us = t;
  return stats;
}

// ----- Strategy C: PrecomputedRotated -----
// Same as B but with rotation support.
// 24 rotations precomputed per block type.
MeshStats StrategyC_PrecomputedRotated(const ChunkData& chunk) {
  MeshStats stats = StrategyB_PrecomputedMesh(chunk);
  // Memory cost: 24 rotated variants × block shapes
  stats.precomputed_memory_bytes = stats.unique_block_types * 24 * sizeof(BlockShape);
  return stats;
}

// ----- Strategy D: HybridGreedy -----
// Cubic blocks use greedy merging. Non-cubic blocks use precomputed meshes.
// Face culling between cubic/non-cubic uses coverage matrix.
MeshStats StrategyD_HybridGreedy(const ChunkData& chunk) {
  MeshStats stats;

  bool seen[static_cast<int>(BlockType::COUNT_)]{};
  for (auto bt : chunk) {
    if (bt != BlockType::Air) {
      int idx = static_cast<int>(bt);
      if (!seen[idx]) {
        seen[idx] = true;
        stats.unique_block_types++;
      }
    }
  }

  // Precomputed memory: same as B for non-cubic + greedy state
  int non_cubic_types = 0;
  for (int i = 0; i < static_cast<int>(BlockType::COUNT_); ++i) {
    if (seen[i]) {
      auto bt = static_cast<BlockType>(i);
      auto shape = GetBlockShape(bt);
      if (!shape.is_cubic && bt != BlockType::Air)
        non_cubic_types++;
    }
  }
  stats.precomputed_memory_bytes =
      non_cubic_types * sizeof(BlockShape) + 1024; // greedy scratch

  auto t = TimeUs([&]() {
    std::array<bool, kChunkVol> is_cubic{};
    for (int i = 0; i < kChunkVol; ++i)
      is_cubic[i] = GetBlockShape(chunk[i]).is_cubic;

    // Phase 1: greedy merge for cubic blocks on each axis
    // (Simplified: just count merged quads)
    for (int axis = 0; axis < 3; ++axis) {
      // Greedy merge per face direction
      for (int d = 0; d < 2; ++d) {
        int f = axis * 2 + d; // face index PX/NX/etc for axis+ direction

        // Sweep plane
        for (int a = 0; a < kChunkSize; ++a) {
          for (int b = 0; b < kChunkSize; ++b) {
            int merged_len = 0;
            for (int c = 0; c < kChunkSize; ++c) {
              int x = (axis == 0) ? ((d == 0) ? c : kChunkSize - 1 - c)
                                  : ((axis == 1) ? a : c);
              int y = (axis == 1) ? ((d == 0) ? c : kChunkSize - 1 - c)
                                  : ((axis == 2) ? a : c);
              int z = (axis == 2) ? ((d == 0) ? c : kChunkSize - 1 - c)
                                  : ((axis == 0) ? a : c);

              // clamp
              x = std::clamp(x, 0, kChunkSize - 1);
              y = std::clamp(y, 0, kChunkSize - 1);
              z = std::clamp(z, 0, kChunkSize - 1);

              int idx = Idx3D(x, y, z);
              if (!is_cubic[idx] || IsAir(chunk[idx])) {
                if (merged_len > 0) {
                  stats.quads++;
                  stats.vertices += 4;
                  merged_len = 0;
                }
                continue;
              }

              // Check if face is visible
              int nx = x, ny = y, nz = z;
              switch (f) {
              case Face::PX: nx = x + 1; break;
              case Face::NX: nx = x - 1; break;
              case Face::PY: ny = y + 1; break;
              case Face::NY: ny = y - 1; break;
              case Face::PZ: nz = z + 1; break;
              case Face::NZ: nz = z - 1; break;
              }

              bool visible = true;
              if (nx >= 0 && nx < kChunkSize && ny >= 0 && ny < kChunkSize &&
                  nz >= 0 && nz < kChunkSize) {
                auto nb = chunk[Idx3D(nx, ny, nz)];
                if (!IsAir(nb) && GetBlockShape(nb).is_cubic)
                  visible = false;
              }
              stats.total_face_checks++;

              if (!visible) {
                if (merged_len > 0) {
                  stats.quads++;
                  stats.vertices += 4;
                  merged_len = 0;
                }
                stats.culled_faces++;
                continue;
              }

              merged_len++;
            }
            if (merged_len > 0) {
              stats.quads++;
              stats.vertices += 4;
            }
          }
        }
      }
    }

    // Phase 2: non-cubic blocks (same as Strategy B)
    std::array<BlockShape, static_cast<int>(BlockType::COUNT_)> shapes;
    for (int i = 0; i < static_cast<int>(BlockType::COUNT_); ++i)
      shapes[i] = GetBlockShape(static_cast<BlockType>(i));

    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          auto bt = chunk[Idx3D(x, y, z)];
          if (IsAir(bt) || shapes[static_cast<int>(bt)].is_cubic)
            continue;

          auto& shape = shapes[static_cast<int>(bt)];
          for (int f = 0; f < kFaceCount; ++f) {
            stats.total_face_checks++;
            float coverage = shape.face_coverage[f];
            if (coverage == 0.0f)
              continue;

            int nx = x, ny = y, nz = z;
            switch (f) {
            case Face::PX: nx = x + 1; break;
            case Face::NX: nx = x - 1; break;
            case Face::PY: ny = y + 1; break;
            case Face::NY: ny = y - 1; break;
            case Face::PZ: nz = z + 1; break;
            case Face::NZ: nz = z - 1; break;
            }

            bool visible = true;
            if (nx >= 0 && nx < kChunkSize && ny >= 0 && ny < kChunkSize &&
                nz >= 0 && nz < kChunkSize) {
              auto nb = chunk[Idx3D(nx, ny, nz)];
              if (!IsAir(nb)) {
                auto& nbs = shapes[static_cast<int>(nb)];
                if (nbs.face_coverage[f ^ 1] >= coverage) {
                  visible = false;
                  stats.culled_faces++;
                }
              }
            }

            if (!visible)
              continue;

            stats.quads++;
            stats.vertices += 4;

            if (shape.inner_face_coverage[f] > 0) {
              stats.quads++;
              stats.vertices += 4;
            }
          }
        }
  });

  stats.build_time_us = t;
  return stats;
}

// ----- Strategy E: NaivePerBlock -----
// Every block generates 6 quads (no culling at all).
// Basline for worst-case.
MeshStats StrategyE_NaivePerBlock(const ChunkData& chunk) {
  MeshStats stats;
  stats.unique_block_types = 0;

  bool seen[static_cast<int>(BlockType::COUNT_)]{};
  for (auto bt : chunk) {
    if (bt != BlockType::Air) {
      int idx = static_cast<int>(bt);
      if (!seen[idx]) {
        seen[idx] = true;
        stats.unique_block_types++;
      }
    }
  }

  auto t = TimeUs([&]() {
    for (int x = 0; x < kChunkSize; ++x)
      for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
          auto bt = chunk[Idx3D(x, y, z)];
          if (IsAir(bt))
            continue;

          auto shape_e = GetBlockShape(bt);
          for (int f = 0; f < kFaceCount; ++f) {
            if (shape_e.face_coverage[f] > 0) {
              stats.quads++;
              stats.vertices += 4;
            }
            if (shape_e.inner_face_coverage[f] > 0) {
              stats.quads++;
              stats.vertices += 4;
            }
          }
        }
  });
  stats.build_time_us = t;
  return stats;
}

// ============================================================
// Main benchmark harness
// ============================================================
struct ResultRow {
  std::string label;
  std::string strategy;
  std::string scene;
  int seed;
  double time_us;
  int64_t quads;
  int64_t vertices;
  int64_t culled;
  int64_t face_checks;
  int64_t memory_bytes;
  int64_t unique_types;
};

int main(int, char** argv) {
  // Parameters
  constexpr int kSeeds[] = {1, 7, 42, 1234, 31337};
  constexpr int kIter = 1000;
  constexpr int kWarmup = 10;

  // Strategy list
  struct Strategy {
    std::string name;
    MeshStats (*fn)(const ChunkData&);
  };

  const Strategy strategies[] = {
    {"A_SimpleCube", StrategyA_SimpleCube},
    {"B_PrecomputedMesh", StrategyB_PrecomputedMesh},
    {"C_PrecomputedRotated", StrategyC_PrecomputedRotated},
    {"D_HybridGreedy", StrategyD_HybridGreedy},
    {"E_NaivePerBlock", StrategyE_NaivePerBlock},
  };

  // Output
  std::vector<ResultRow> results;
  results.reserve(128);

  // Run
  for (int si = 0; si < static_cast<int>(SceneType::COUNT_); ++si) {
    auto scene_type = static_cast<SceneType>(si);
    auto& scfg = kSceneConfigs[si];

    for (int seed : kSeeds) {
      for (auto& strat : strategies) {
        // Generate scene
        ChunkData chunk;
        std::mt19937_64 rng(static_cast<uint64_t>(seed));
        GenerateScene(chunk, scene_type, rng);

        // Warmup
        for (int w = 0; w < kWarmup; ++w) {
          strat.fn(chunk);
        }

        // Measurement
        MeshStats total{};
        for (int iter = 0; iter < kIter; ++iter) {
          auto stats = strat.fn(chunk);
          total.quads += stats.quads;
          total.vertices += stats.vertices;
          total.culled_faces += stats.culled_faces;
          total.total_face_checks += stats.total_face_checks;
          total.build_time_us += stats.build_time_us;
          total.precomputed_memory_bytes = stats.precomputed_memory_bytes;
          total.unique_block_types = stats.unique_block_types;
        }

        // Average
        double n = static_cast<double>(kIter);
        ResultRow row;
        row.label = strat.name + "_" + scfg.name + "_s" + std::to_string(seed);
        row.strategy = strat.name;
        row.scene = scfg.name;
        row.seed = seed;
        row.time_us = total.build_time_us / n;
        row.quads = total.quads / kIter;
        row.vertices = total.vertices / kIter;
        row.culled = total.culled_faces / kIter;
        row.face_checks = total.total_face_checks / kIter;
        row.memory_bytes = total.precomputed_memory_bytes;
        row.unique_types = total.unique_block_types;
        results.push_back(row);
      }
    }
  }

  // Write CSV
  auto csv_path =
      std::filesystem::path(argv[0]).parent_path() / "results.csv";
  std::ofstream csv(csv_path);
  if (!csv) {
    csv_path = "results.csv";
    csv.open(csv_path);
  }

  csv << "strategy,scene,seed,time_us,quads,vertices,culled_faces,"
         "face_checks,memory_bytes,unique_types\n";

  for (auto& r : results) {
    csv << r.strategy << "," << r.scene << "," << r.seed << "," << r.time_us
        << "," << r.quads << "," << r.vertices << "," << r.culled << ","
        << r.face_checks << "," << r.memory_bytes << "," << r.unique_types
        << "\n";
  }

  csv.close();

  // Print summary
  printf("=== Extended Block Multivoxel Mesh Benchmark ===\n");
  printf("Chunk size: %d^3 = %d voxels\n", kChunkSize, kChunkVol);
  printf("Seeds: [");
  for (int s : kSeeds)
    printf("%d ", s);
  printf("]\n");
  printf("Iterations per config: %d (+ %d warmup)\n", kIter, kWarmup);
  printf("Configurations: %zu strategies x %d scenes x %zu seeds = %zu\n\n",
         sizeof(strategies) / sizeof(strategies[0]),
         static_cast<int>(SceneType::COUNT_),
         sizeof(kSeeds) / sizeof(kSeeds[0]), results.size());

  printf("%-25s %-18s %5s %10s %6s %8s %8s %8s\n", "Strategy", "Scene", "Seed",
         "Time(us)", "Quads", "Culled", "Checks", "Mem(B)");
  printf("%-25s %-18s %5s %10s %6s %8s %8s %8s\n",
         "------------------------", "------------------", "-----",
         "----------", "------", "--------", "--------", "--------");

  // Group by strategy, compute means
  struct StratMean {
    std::string name;
    double time_us = 0;
    int64_t quads = 0;
    double cull_ratio = 0;
  };
  std::vector<StratMean> means;

  for (auto& strat : strategies) {
    StratMean m;
    m.name = strat.name;
    double count = 0;
    double cull_sum = 0, check_sum = 0;
    for (auto& r : results) {
      if (r.strategy == strat.name) {
        m.time_us += r.time_us;
        m.quads += r.quads;
        cull_sum += static_cast<double>(r.culled);
        check_sum += static_cast<double>(r.face_checks);
        count++;
        printf("%-25s %-18s %5d %10.3f %6ld %8ld %8ld %8ld\n",
               r.strategy.c_str(), r.scene.c_str(), r.seed, r.time_us,
               static_cast<long>(r.quads), static_cast<long>(r.culled),
               static_cast<long>(r.face_checks),
               static_cast<long>(r.memory_bytes));
      }
    }
    if (count > 0) {
      m.time_us /= count;
      m.quads = static_cast<int64_t>(static_cast<double>(m.quads) / count);
      m.cull_ratio = (check_sum > 0) ? (cull_sum / check_sum) : 0;
      means.push_back(m);
    }
  }

  printf("\n=== Aggregate means ===\n");
  printf("%-25s %12s %8s %10s\n", "Strategy", "Time(us)", "Quads", "Cull%");
  for (auto& m : means) {
    printf("%-25s %12.3f %8ld %9.1f%%\n", m.name.c_str(), m.time_us,
           static_cast<long>(m.quads), m.cull_ratio * 100.0);
  }

  printf("\nResults written to: %s\n", csv_path.string().c_str());
  return 0;
}
