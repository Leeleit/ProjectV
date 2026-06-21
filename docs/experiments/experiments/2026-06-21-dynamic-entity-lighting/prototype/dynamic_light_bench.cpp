#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

static constexpr int kChunkSize = 8;
static constexpr int kGridX = 8;  // chunks
static constexpr int kGridY = 4;
static constexpr int kGridZ = 8;
static constexpr int kVoxelsX = kGridX * kChunkSize;
static constexpr int kVoxelsY = kGridY * kChunkSize;
static constexpr int kVoxelsZ = kGridZ * kChunkSize;
static constexpr int kMaxLight = 15;
static constexpr int kLightRange = 8;     // blocks, OptiFine uses 7.5
static constexpr int kLightRangeSq = kLightRange * kLightRange;
static constexpr int kLightRangeChunks = (kLightRange + kChunkSize - 1) / kChunkSize;

// For budget BFS: max queue entries per frame
static constexpr int kBudgetQueueSize = 2048;

// For rate-limited strategy: min ms between updates
static constexpr double kRateLimitMs = 50.0;

// ---------------------------------------------------------------------------
// Scene types
// ---------------------------------------------------------------------------

enum class Scene : uint8_t {
  UniformFloor, ForestFloor, CaveStress, MixedBiome, UniformAir, kCount
};

static constexpr std::string_view kSceneNames[] = {
  "uniform_floor", "forest_floor", "cave_stress", "mixed_biome", "uniform_air"
};

struct SceneData {
  std::vector<uint8_t> solid;   // 0 = air, 1 = solid (opaque)
  std::vector<uint8_t> emissive; // 0-15 block light emission
};

SceneData generate_scene(Scene scene, int seed) {
  std::mt19937 rng(static_cast<uint32_t>(seed));
  SceneData d;
  d.solid.resize(static_cast<size_t>(kVoxelsX) * kVoxelsY * kVoxelsZ, 0);
  d.emissive.resize(d.solid.size(), 0);

  auto idx = [](int x, int y, int z) -> size_t {
    return static_cast<size_t>(y) * kVoxelsX * kVoxelsZ + static_cast<size_t>(z) * kVoxelsX + x;
  };

  switch (scene) {
    case Scene::UniformFloor: {
      int floor_y = kVoxelsY / 4;
      for (int x = 0; x < kVoxelsX; ++x)
        for (int z = 0; z < kVoxelsZ; ++z)
          for (int y = 0; y < floor_y; ++y)
            d.solid[idx(x, y, z)] = 1;
      // scattered glowstone patches on surface
      for (int i = 0; i < 12; ++i) {
        int px = std::uniform_int_distribution<int>(0, kVoxelsX - 1)(rng);
        int pz = std::uniform_int_distribution<int>(0, kVoxelsZ - 1)(rng);
        d.emissive[idx(px, floor_y - 1, pz)] = 15;
      }
      break;
    }
    case Scene::ForestFloor: {
      int floor_y = kVoxelsY / 4;
      for (int x = 0; x < kVoxelsX; ++x)
        for (int z = 0; z < kVoxelsZ; ++z)
          for (int y = 0; y < floor_y; ++y)
            d.solid[idx(x, y, z)] = 1;
      // trees (solid pillars + leaf canopies)
      for (int t = 0; t < 30; ++t) {
        int tx = std::uniform_int_distribution<int>(2, kVoxelsX - 3)(rng);
        int tz = std::uniform_int_distribution<int>(2, kVoxelsZ - 3)(rng);
        int th = std::uniform_int_distribution<int>(3, 6)(rng);
        for (int h = 1; h <= th; ++h)
          d.solid[idx(tx, floor_y + h, tz)] = 1;
        for (int dx = -2; dx <= 2; ++dx)
          for (int dz = -2; dz <= 2; ++dz)
            for (int dh = th; dh <= th + 2; ++dh) {
              int lx = tx + dx, lz = tz + dz, ly = floor_y + dh;
              if (lx >= 0 && lx < kVoxelsX && lz >= 0 && lz < kVoxelsZ && ly >= 0 && ly < kVoxelsY)
                if (std::abs(dx) < 2 || std::abs(dz) < 2 || dh == th + 2)
                  d.solid[idx(lx, ly, lz)] = 1;
            }
        // some trees have glowstone in canopy
        if (std::uniform_int_distribution<int>(0, 3)(rng) == 0) {
          int gx = tx + std::uniform_int_distribution<int>(-1, 1)(rng);
          int gz = tz + std::uniform_int_distribution<int>(-1, 1)(rng);
          int gy = floor_y + th + 1;
          if (gx >= 0 && gx < kVoxelsX && gz >= 0 && gz < kVoxelsZ)
            d.emissive[idx(gx, gy, gz)] = 15;
        }
      }
      break;
    }
    case Scene::CaveStress: {
      // Dense cave system: mostly solid with tunnels
      for (size_t i = 0; i < d.solid.size(); ++i)
        d.solid[i] = (std::uniform_int_distribution<int>(0, 2)(rng) != 0) ? 1 : 0;
      // carve some tunnels
      for (int t = 0; t < 20; ++t) {
        int cx = std::uniform_int_distribution<int>(1, kVoxelsX - 2)(rng);
        int cy = std::uniform_int_distribution<int>(1, kVoxelsY - 2)(rng);
        int cz = std::uniform_int_distribution<int>(1, kVoxelsZ - 2)(rng);
        for (int step = 0; step < 50; ++step) {
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              for (int dz = -1; dz <= 1; ++dz) {
                int tx = cx + dx, ty = cy + dy, tz = cz + dz;
                if (tx >= 0 && tx < kVoxelsX && ty >= 0 && ty < kVoxelsY && tz >= 0 && tz < kVoxelsZ)
                  d.solid[idx(tx, ty, tz)] = 0;
              }
          cx += std::uniform_int_distribution<int>(-1, 1)(rng);
          cy += std::uniform_int_distribution<int>(-1, 1)(rng);
          cz += std::uniform_int_distribution<int>(-1, 1)(rng);
          cx = std::clamp(cx, 1, kVoxelsX - 2);
          cy = std::clamp(cy, 1, kVoxelsY - 2);
          cz = std::clamp(cz, 1, kVoxelsZ - 2);
        }
      }
      // some glowstone in caves
      for (int i = 0; i < 15; ++i) {
        int lx = std::uniform_int_distribution<int>(0, kVoxelsX - 1)(rng);
        int ly = std::uniform_int_distribution<int>(0, kVoxelsY - 1)(rng);
        int lz = std::uniform_int_distribution<int>(0, kVoxelsZ - 1)(rng);
        if (d.solid[idx(lx, ly, lz)])
          d.emissive[idx(lx, ly, lz)] = 15;
      }
      break;
    }
    case Scene::MixedBiome: {
      for (int x = 0; x < kVoxelsX; ++x)
        for (int z = 0; z < kVoxelsZ; ++z) {
          int h = static_cast<int>(std::sin(x * 0.3) * 2 + std::cos(z * 0.3) * 2 + 4);
          h = std::clamp(h, 1, kVoxelsY - 1);
          for (int y = 0; y < h; ++y)
            d.solid[idx(x, y, z)] = 1;
          // some trees on higher ground
          if (h > 4 && std::uniform_int_distribution<int>(0, 5)(rng) == 0) {
            int th = std::uniform_int_distribution<int>(3, 5)(rng);
            for (int hh = 1; hh <= th; ++hh)
              d.solid[idx(x, h + hh, z)] = 1;
            // canopy
            for (int dx = -1; dx <= 1; ++dx)
              for (int dz = -1; dz <= 1; ++dz) {
                int lx = x + dx, lz = z + dz;
                if (lx >= 0 && lx < kVoxelsX && lz >= 0 && lz < kVoxelsZ) {
                  int ly = h + th;
                  d.solid[idx(lx, ly, lz)] = 1;
                }
              }
          }
        }
      break;
    }
    case Scene::UniformAir: {
      // completely empty
      break;
    }
  }
  return d;
}

// ---------------------------------------------------------------------------
// Light propagation (BFS on 3D grid)
// ---------------------------------------------------------------------------

struct LightGrid {
  std::vector<uint8_t> light; // 0-15 per voxel
  int width, height, depth;

  LightGrid(int w, int h, int d) : light(static_cast<size_t>(w) * h * d, 0),
    width(w), height(h), depth(d) {}

  uint8_t& at(int x, int y, int z) {
    return light[static_cast<size_t>(y) * width * depth + static_cast<size_t>(z) * width + x];
  }
  const uint8_t& at(int x, int y, int z) const {
    return light[static_cast<size_t>(y) * width * depth + static_cast<size_t>(z) * width + x];
  }
};

struct LightPos {
  int x, y, z, level;
};

// Full BFS propagation from sources
void bfs_propagate(LightGrid& grid, std::span<const LightPos> sources,
                   const std::vector<uint8_t>& solid, bool use_budget, int budget) {
  struct Entry { int x, y, z, level; };
  std::vector<Entry> queue;
  queue.reserve(65536);

  for (const auto& s : sources) {
    int lv = std::min(s.level, kMaxLight);
    if (lv <= grid.at(s.x, s.y, s.z))
      continue;
    grid.at(s.x, s.y, s.z) = static_cast<uint8_t>(lv);
    queue.push_back({s.x, s.y, s.z, lv});
  }

  size_t processed = 0;
  auto idx = [&](int x, int y, int z) -> size_t {
    return static_cast<size_t>(y) * grid.width * grid.depth + static_cast<size_t>(z) * grid.width + x;
  };
  const auto& sol = solid;

  while (processed < queue.size()) {
    if (use_budget && static_cast<int>(processed) >= budget)
      break;

    auto [cx, cy, cz, cl] = queue[processed++];
    int nl = cl - 1;
    if (nl <= 0) continue;

    static constexpr int kDirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (auto [dx, dy, dz] : kDirs) {
      int nx = cx + dx, ny = cy + dy, nz = cz + dz;
      if (nx < 0 || nx >= grid.width || ny < 0 || ny >= grid.height || nz < 0 || nz >= grid.depth)
        continue;
      // solid blocks block light (air = 0, opaque = full absorption)
      if (sol[idx(nx, ny, nz)])
        continue;
      if (nl > grid.at(nx, ny, nz)) {
        grid.at(nx, ny, nz) = static_cast<uint8_t>(nl);
        queue.push_back({nx, ny, nz, nl});
      }
    }
  }
}

// Distance-based falloff (for shader-based / rate-limited)
double falloff_light(int lx, int ly, int lz, int ex, int ey, int ez, int luminance) {
  double dx = static_cast<double>(lx - ex);
  double dy = static_cast<double>(ly - ey);
  double dz = static_cast<double>(lz - ez);
  double dist_sq = dx * dx + dy * dy + dz * dz;
  if (dist_sq >= kLightRangeSq)
    return 0.0;
  double dist = std::sqrt(dist_sq);
  // LambDynamicLights formula: luminance * (1 - sqrt(distance) / range)
  return static_cast<double>(luminance) * (1.0 - dist / static_cast<double>(kLightRange));
}

// ---------------------------------------------------------------------------
// Entity light source (dynamic)
// ---------------------------------------------------------------------------

struct EntitySource {
  double x, y, z;        // continuous position
  double last_x, last_y, last_z;
  int luminance;          // 0-15
  int chunk_x, chunk_y, chunk_z;
  double time_since_update; // ms since last light update

  bool moved_significantly() const {
    double dx = x - last_x, dy = y - last_y, dz = z - last_z;
    return (dx * dx + dy * dy + dz * dz) > 0.01; // > 0.1 blocks
  }

  void sync_chunk() {
    chunk_x = static_cast<int>(x) / kChunkSize;
    chunk_y = static_cast<int>(y) / kChunkSize;
    chunk_z = static_cast<int>(z) / kChunkSize;
  }
};

// ---------------------------------------------------------------------------
// Strategy types
// ---------------------------------------------------------------------------

enum class Strategy : uint8_t {
  None,
  FullBFS,
  BudgetBFS,
  RateLimited,
  GPUInjection,
  kCount
};

static constexpr std::string_view kStrategyNames[] = {
  "A_None", "B_FullBFS", "C_BudgetBFS", "D_RateLimited", "E_GPUInjection"
};

struct StrategyResult {
  double wall_time_us;
  double psnr;
  double chunk_rebuilds_frac; // fraction of chunks that were updated
  double lights_per_entity;   // how many voxels were updated per entity
};

// ---------------------------------------------------------------------------
// PSNR calculation (vs FullBFS as reference)
// ---------------------------------------------------------------------------

double compute_psnr(const LightGrid& ref, const LightGrid& test) {
  double mse = 0.0;
  size_t n = 0;
  for (int y = 0; y < ref.height; ++y)
    for (int z = 0; z < ref.depth; ++z)
      for (int x = 0; x < ref.width; ++x) {
        double d = static_cast<double>(ref.at(x, y, z)) - static_cast<double>(test.at(x, y, z));
        mse += d * d;
        ++n;
      }
  if (n == 0) return 100.0;
  mse /= static_cast<double>(n);
  if (mse < 1e-10) return 100.0;
  return 10.0 * std::log10(static_cast<double>(kMaxLight * kMaxLight) / mse);
}

// ---------------------------------------------------------------------------
// Benchmark runner
// ---------------------------------------------------------------------------

struct BenchmarkConfig {
  Strategy strategy;
  Scene scene;
  int seed;
  int entity_count;
  int iter;
};

StrategyResult run_strategy(const BenchmarkConfig& cfg, const SceneData& scene_data, int frame) {
  LightGrid ref_grid(kVoxelsX, kVoxelsY, kVoxelsZ);
  LightGrid test_grid(kVoxelsX, kVoxelsY, kVoxelsZ);

  // Static light sources from scene
  std::vector<LightPos> static_sources;
  for (size_t i = 0; i < scene_data.emissive.size(); ++i) {
    if (scene_data.emissive[i] > 0) {
      int y = static_cast<int>(i / (kVoxelsX * kVoxelsZ));
      int rem = static_cast<int>(i % (kVoxelsX * kVoxelsZ));
      int z = rem / kVoxelsX;
      int x = rem % kVoxelsX;
      static_sources.push_back({x, y, z, scene_data.emissive[i]});
    }
  }

  // Apply static light everywhere first (shared baseline)
  bfs_propagate(ref_grid, static_sources, scene_data.solid, false, 0);
  bfs_propagate(test_grid, static_sources, scene_data.solid, false, 0);

  // Generate dynamic entity positions
  std::mt19937 rng(static_cast<uint32_t>(cfg.seed + frame * 7));
  std::vector<EntitySource> entities;
  for (int e = 0; e < cfg.entity_count; ++e) {
    EntitySource es;
    // position changes every frame to simulate movement
    double t = static_cast<double>(frame) * 0.1 + static_cast<double>(e) * 1.7;
    es.x = (std::sin(t * 0.5 + e) * 0.5 + 0.5) * (kVoxelsX - 2);
    es.y = (std::cos(t * 0.3 + e * 1.3) * 0.3 + 0.5) * (kVoxelsY - 2);
    es.z = (std::sin(t * 0.7 + e * 0.9) * 0.5 + 0.5) * (kVoxelsZ - 2);
    es.last_x = (std::sin((t - 0.1) * 0.5 + e) * 0.5 + 0.5) * (kVoxelsX - 2);
    es.last_y = (std::cos((t - 0.1) * 0.3 + e * 1.3) * 0.3 + 0.5) * (kVoxelsY - 2);
    es.last_z = (std::sin((t - 0.1) * 0.7 + e * 0.9) * 0.5 + 0.5) * (kVoxelsZ - 2);
    es.luminance = std::uniform_int_distribution<int>(10, 15)(rng);
    es.time_since_update = 0.0;
    es.sync_chunk();
    entities.push_back(es);
  }

  auto start = std::chrono::steady_clock::now();

  switch (cfg.strategy) {
    case Strategy::None: {
      // No dynamic lights - do nothing
      // test_grid already has static light
      break;
    }
    case Strategy::FullBFS: {
      // Full BFS from all entity positions every frame
      std::vector<LightPos> dyn_sources;
      for (const auto& e : entities) {
        int bx = static_cast<int>(std::round(e.x));
        int by = static_cast<int>(std::round(e.y));
        int bz = static_cast<int>(std::round(e.z));
        dyn_sources.push_back({bx, by, bz, e.luminance});
      }
      bfs_propagate(test_grid, dyn_sources, scene_data.solid, false, 0);
      break;
    }
    case Strategy::BudgetBFS: {
      // Budget-limited BFS from entities
      std::vector<LightPos> dyn_sources;
      for (const auto& e : entities) {
        int bx = static_cast<int>(std::round(e.x));
        int by = static_cast<int>(std::round(e.y));
        int bz = static_cast<int>(std::round(e.z));
        dyn_sources.push_back({bx, by, bz, e.luminance});
      }
      bfs_propagate(test_grid, dyn_sources, scene_data.solid, true, kBudgetQueueSize);
      break;
    }
    case Strategy::RateLimited: {
      // OptiFine-style: batch BFS every 3rd frame (33% frequency)
      // Simulates per-source timers: each entity updates at most every ~50ms
      if (frame % 3 == 0) {
        std::vector<LightPos> dyn_sources;
        for (const auto& e : entities) {
          dyn_sources.push_back({static_cast<int>(std::round(e.x)),
                                 static_cast<int>(std::round(e.y)),
                                 static_cast<int>(std::round(e.z)),
                                 e.luminance});
        }
        bfs_propagate(test_grid, dyn_sources, scene_data.solid, false, 0);
      }
      break;
    }
    case Strategy::GPUInjection: {
      // Shader-based: lightmap injection via uniform/SSBO update
      // CPU cost = O(entities) for memcpy into SSBO
      volatile int sink = 0;
      for (const auto& e : entities) {
        int buf[4] = {static_cast<int>(std::round(e.x)),
                      static_cast<int>(std::round(e.y)),
                      static_cast<int>(std::round(e.z)), e.luminance};
        sink += buf[0] + buf[1] + buf[2] + buf[3];
      }
      // NOTE: GPU distance-falloff computation is NOT measured here.
      // It runs as a shader on GPU and costs ~entities-per-pixel * screen
      // The PSNR is computed analytically below from distance-based falloff.
      break;
    }
  }

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double, std::micro>(end - start).count();

  // For GPUInjection: fill test_grid with distance-based falloff for PSNR (not timed)
  if (cfg.strategy == Strategy::GPUInjection) {
    for (int y = 0; y < test_grid.height; ++y)
      for (int z = 0; z < test_grid.depth; ++z)
        for (int x = 0; x < test_grid.width; ++x) {
          if (scene_data.solid[static_cast<size_t>(y) * kVoxelsX * kVoxelsZ +
                              static_cast<size_t>(z) * kVoxelsX + x])
            continue;
          double max_val = 0.0;
          for (const auto& e : entities) {
            int bx = static_cast<int>(std::round(e.x));
            int by = static_cast<int>(std::round(e.y));
            int bz = static_cast<int>(std::round(e.z));
            double dx = static_cast<double>(x - bx);
            double dy = static_cast<double>(y - by);
            double dz = static_cast<double>(z - bz);
            double dist_sq = dx * dx + dy * dy + dz * dz;
            if (dist_sq >= kLightRangeSq) continue;
            double dist = std::sqrt(dist_sq);
            double val = static_cast<double>(e.luminance) * (1.0 - dist / static_cast<double>(kLightRange));
            if (val > max_val) max_val = val;
          }
          if (max_val > 0.0) {
            uint8_t lv = std::min(static_cast<int>(max_val), kMaxLight);
            if (lv > test_grid.at(x, y, z))
              test_grid.at(x, y, z) = lv;
          }
        }
  }

  // Compute PSNR vs FullBFS (reference)
  LightGrid ref_dyn(kVoxelsX, kVoxelsY, kVoxelsZ);
  bfs_propagate(ref_dyn, static_sources, scene_data.solid, false, 0);
  std::vector<LightPos> all_dyn;
  for (const auto& e : entities) {
    all_dyn.push_back({static_cast<int>(std::round(e.x)),
                       static_cast<int>(std::round(e.y)),
                       static_cast<int>(std::round(e.z)),
                       e.luminance});
  }
  bfs_propagate(ref_dyn, all_dyn, scene_data.solid, false, 0);
  double psnr = compute_psnr(ref_dyn, test_grid);

  // Count chunk rebuilds
  double chunk_rebuilds = 0.0;
  if (cfg.strategy == Strategy::FullBFS || cfg.strategy == Strategy::BudgetBFS) {
    chunk_rebuilds = static_cast<double>(kGridX * kGridY * kGridZ);
  } else if (cfg.strategy == Strategy::RateLimited) {
    int updated = 0;
    for (const auto& e : entities) {
      int cx = static_cast<int>(e.x) / kChunkSize;
      int cy = static_cast<int>(e.y) / kChunkSize;
      int cz = static_cast<int>(e.z) / kChunkSize;
      for (int dx = -kLightRangeChunks; dx <= kLightRangeChunks; ++dx)
        for (int dy = -kLightRangeChunks; dy <= kLightRangeChunks; ++dy)
          for (int dz = -kLightRangeChunks; dz <= kLightRangeChunks; ++dz) {
            int nx = cx + dx, ny = cy + dy, nz = cz + dz;
            if (nx >= 0 && nx < kGridX && ny >= 0 && ny < kGridY && nz >= 0 && nz < kGridZ)
              ++updated;
          }
    }
    chunk_rebuilds = static_cast<double>(updated);
  }

  double lights_per_entity = 0.0;
  size_t total_lit = 0;
  for (size_t i = 0; i < test_grid.light.size(); ++i)
    if (test_grid.light[i] > 0)
      ++total_lit;
  if (cfg.entity_count > 0)
    lights_per_entity = static_cast<double>(total_lit) / static_cast<double>(cfg.entity_count);

  return {elapsed, psnr, chunk_rebuilds, lights_per_entity};
}

// ---------------------------------------------------------------------------
// CSV writer
// ---------------------------------------------------------------------------

void write_csv_header(std::ofstream& f) {
  f << "strategy,scene,seed,entity_count,iter,"
       "wall_time_us,psnr_db,chunk_rebuild_count,lights_per_entity\n";
}

void write_csv_row(std::ofstream& f, const BenchmarkConfig& cfg, int iter,
                   const StrategyResult& r) {
  char buf[256];
  auto res = std::to_chars(buf, buf + sizeof(buf), r.wall_time_us, std::chars_format::fixed, 3);
  *res.ptr = '\0';
  std::string t(buf, res.ptr);

  auto res2 = std::to_chars(buf, buf + sizeof(buf), r.psnr, std::chars_format::fixed, 3);
  *res2.ptr = '\0';
  std::string p(buf, res2.ptr);

  auto res3 = std::to_chars(buf, buf + sizeof(buf), r.chunk_rebuilds_frac, std::chars_format::fixed, 1);
  *res3.ptr = '\0';
  std::string cr(buf, res3.ptr);

  auto res4 = std::to_chars(buf, buf + sizeof(buf), r.lights_per_entity, std::chars_format::fixed, 1);
  *res4.ptr = '\0';
  std::string lpe(buf, res4.ptr);

  f << kStrategyNames[static_cast<int>(cfg.strategy)] << ','
     << kSceneNames[static_cast<int>(cfg.scene)] << ','
     << cfg.seed << ','
     << cfg.entity_count << ','
     << iter << ','
     << t << ','
     << p << ','
     << cr << ','
     << lpe << '\n';
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  bool debug_mode = false;
  std::string out_dir = "build";

  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--debug") debug_mode = true;
    else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
  }

  std::filesystem::create_directories(out_dir);

  int warmup = 10;
  int main_iter = 100;
  int seeds[] = {1, 7, 42, 1234, 31337};
  int entity_counts[] = {1, 3, 5, 10, 20};

  std::ofstream csv(out_dir + "/results.csv");
  write_csv_header(csv);

  double total_wall = 0.0;

  for (int si = 0; si < static_cast<int>(Scene::kCount); ++si) {
    Scene scene = static_cast<Scene>(si);
    for (int seed : seeds) {
      SceneData sd = generate_scene(scene, seed);
      for (int strategy_idx = 0; strategy_idx < static_cast<int>(Strategy::kCount); ++strategy_idx) {
        Strategy strat = static_cast<Strategy>(strategy_idx);
        for (int ec : entity_counts) {
          BenchmarkConfig cfg{strat, scene, seed, ec, 0};

          // Warmup
          for (int w = 0; w < warmup; ++w) {
            run_strategy(cfg, sd, w);
          }

          // Main iterations
          for (int iter = 0; iter < main_iter; ++iter) {
            auto result = run_strategy(cfg, sd, iter);
            if (!debug_mode || iter < 5)
              write_csv_row(csv, cfg, iter, result);
            total_wall += result.wall_time_us;
          }
        }
      }
    }
  }

  // Summary row
  csv << "# total_configs="
       << (static_cast<int>(Scene::kCount) * 5 * static_cast<int>(Strategy::kCount) * 5)
       << " total_measurements="
       << (static_cast<int>(Scene::kCount) * 5 * static_cast<int>(Strategy::kCount) * 5 * main_iter)
       << " wall_time_sec=";
  char buf[64];
  auto res = std::to_chars(buf, buf + sizeof(buf), total_wall / 1e6, std::chars_format::fixed, 3);
  *res.ptr = '\0';
  csv << std::string_view(buf, res.ptr) << '\n';

  csv.close();

  std::printf("Output: %s/results.csv\n", out_dir.c_str());
  return 0;
}
