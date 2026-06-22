#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

// 8³ chunk
inline constexpr int CHUNK_N = 8;
inline constexpr int CHUNK_VOXELS = CHUNK_N * CHUNK_N * CHUNK_N;
inline constexpr int CHUNK_SLICES = CHUNK_N * CHUNK_N;

inline constexpr float T_HOT = 100.0f;
inline constexpr float T_AMB = 20.0f;
inline constexpr float TARGET_RESIDUAL = 1e-3f;
inline constexpr int MAX_GS_ITER = 100;

enum class Mat : uint8_t {
  AIR = 0, STONE, WOOD, INSULATION, IRON, WATER, NUM_MATS
};

struct MaterialProps {
  float k;      // thermal conductivity W/(m·K)
  float alpha;  // thermal diffusivity (m²/s) * 1e6 for numerical scale
};

constexpr std::array<MaterialProps, 7> MAT_PROPS = {{
  {0.026f,  22.0f},    // AIR    (high diffusivity = fast equilibration)
  {2.0f,     1.2f},    // STONE
  {0.15f,    0.12f},   // WOOD
  {0.04f,    0.06f},   // INSULATION
  {80.0f,   23.0f},    // IRON
  {0.6f,     0.14f},   // WATER
  {1.0f,     1.0f}     // default
}};

inline int idx(int x, int y, int z) { return (z * CHUNK_N + y) * CHUNK_N + x; }

struct Chunk {
  std::array<float, CHUNK_VOXELS> T{};
  std::array<Mat, CHUNK_VOXELS> mat{};

  void fill(float val) { T.fill(val); }
  void set_all_mat(Mat m) { mat.fill(m); }

  float get_T(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_N || y < 0 || y >= CHUNK_N || z < 0 || z >= CHUNK_N) return T_AMB;
    return T[idx(x, y, z)];
  }

  Mat get_mat(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_N || y < 0 || y >= CHUNK_N || z < 0 || z >= CHUNK_N) return Mat::STONE;
    return mat[idx(x, y, z)];
  }

  float alpha_at(int x, int y, int z) const {
    auto m = get_mat(x, y, z);
    return MAT_PROPS[static_cast<int>(m)].alpha;
  }
};

// --- Scene definitions ---
struct Scene {
  const char* name;
  void (*build)(Chunk&, std::mt19937&);
  float analytical_steady_state(const Chunk&) { return 0.0f; } // override per scene
};

void build_uniform_stone(Chunk& c, std::mt19937&) {
  c.set_all_mat(Mat::STONE);
  c.fill(T_AMB);
  // heated face at z=0
  for (int x = 0; x < CHUNK_N; ++x)
    for (int y = 0; y < CHUNK_N; ++y)
      c.T[idx(x, y, 0)] = T_HOT;
}

void build_layered_insulation(Chunk& c, std::mt19937&) {
  c.fill(T_AMB);
  for (int z = 0; z < CHUNK_N; ++z) {
    Mat m;
    if (z < 2) m = Mat::STONE;
    else if (z < 4) m = Mat::INSULATION;
    else m = Mat::WOOD;
    for (int x = 0; x < CHUNK_N; ++x)
      for (int y = 0; y < CHUNK_N; ++y)
        c.mat[idx(x, y, z)] = m;
  }
  // heated face at z=0
  for (int x = 0; x < CHUNK_N; ++x)
    for (int y = 0; y < CHUNK_N; ++y)
      c.T[idx(x, y, 0)] = T_HOT;
}

void build_heat_source_center(Chunk& c, std::mt19937&) {
  c.set_all_mat(Mat::STONE);
  c.fill(T_AMB);
  // furnace at center (3,3,3) to (4,4,4) — 2x2x2 hot core
  for (int x = 3; x <= 4; ++x)
    for (int y = 3; y <= 4; ++y)
      for (int z = 3; z <= 4; ++z)
        c.T[idx(x, y, z)] = T_HOT;
}

void build_multi_material_sphere(Chunk& c, std::mt19937&) {
  c.set_all_mat(Mat::AIR);
  c.fill(T_HOT); // hot interior air
  // stone shell: radius 3, center at (3.5, 3.5, 3.5)
  int cx = 3, cy = 3, cz = 3;
  for (int x = 0; x < CHUNK_N; ++x)
    for (int y = 0; y < CHUNK_N; ++y)
      for (int z = 0; z < CHUNK_N; ++z) {
        int dx = x - cx, dy = y - cy, dz = z - cz;
        float r2 = float(dx*dx + dy*dy + dz*dz);
        if (r2 >= 4.0f && r2 <= 12.0f) { // shell
          c.mat[idx(x, y, z)] = Mat::STONE;
          c.T[idx(x, y, z)] = T_AMB;
        }
        if (r2 > 12.0f) { // outside — cold
          c.T[idx(x, y, z)] = T_AMB - 10.0f;
        }
      }
}

void build_edge_chunk(Chunk& c, std::mt19937&) {
  c.set_all_mat(Mat::STONE);
  c.fill(T_AMB);
  // one half (z < 4) hot, other half cold — sharp gradient at z=4 boundary
  // also hot face at x=0
  for (int x = 0; x < CHUNK_N; ++x)
    for (int y = 0; y < CHUNK_N; ++y)
      for (int z = 0; z < CHUNK_N; ++z) {
        if (z < 4 || x == 0) c.T[idx(x, y, z)] = T_HOT;
      }
}

constexpr std::array SCENES = {
  Scene{"uniform_stone", build_uniform_stone},
  Scene{"layered_insulation", build_layered_insulation},
  Scene{"heat_source_center", build_heat_source_center},
  Scene{"multi_material_sphere", build_multi_material_sphere},
  Scene{"edge_chunk", build_edge_chunk},
};

// --- Strategies ---
struct StrategyResult {
  double mean_us;   // mean µs per tick
  double p50_us;
  double p95_us;
  double p99_us;
  double std_us;
  double final_psnr; // PSNR vs analytical steady-state (for S1)
  double convergence_ticks; // ticks to reach <1% residual change
};

// A: no conduction (baseline)
void strat_none(Chunk&, int) {}

// B: explicit Euler 6-neighbor
void strat_explicit_euler(Chunk& c, int ticks) {
  auto T2 = c.T;
  float dt = 0.1f; // sub-tick for stability (CFL: dt ≤ dx²/(2α·d) with α≈23 max → need small dt)
  int sub_ticks = ticks * 10; // 10 sub-steps per tick

  for (int s = 0; s < sub_ticks; ++s) {
    T2 = c.T;
    for (int z = 0; z < CHUNK_N; ++z)
      for (int y = 0; y < CHUNK_N; ++y)
        for (int x = 0; x < CHUNK_N; ++x) {
          int i = idx(x, y, z);
          if (c.mat[i] == Mat::AIR) { c.T[i] = T_AMB; continue; }
          float a = c.alpha_at(x, y, z);
          // 6-neighbor Laplacian
          float lap = c.get_T(x-1,y,z) + c.get_T(x+1,y,z)
                    + c.get_T(x,y-1,z) + c.get_T(x,y+1,z)
                    + c.get_T(x,y,z-1) + c.get_T(x,y,z+1)
                    - 6.0f * T2[i];
          c.T[i] = T2[i] + a * dt * lap * 0.1f;
        }
  }
}

// C: BFS from heat sources with material attenuation
void strat_bfs_propagation(Chunk& c, int ticks) {
  // find hot sources: any voxel with T > T_AMB + 1
  struct Source { int x,y,z; float T; };
  std::vector<Source> sources;
  for (int z = 0; z < CHUNK_N; ++z)
    for (int y = 0; y < CHUNK_N; ++y)
      for (int x = 0; x < CHUNK_N; ++x)
        if (c.T[idx(x,y,z)] > T_AMB + 1.0f)
          sources.push_back({x,y,z,c.T[idx(x,y,z)]});

  struct Node { int x,y,z; float T; int dist; };

  for (int t = 0; t < ticks; ++t) {
    auto T_new = c.T;
    for (auto& src : sources) {
      // BFS from each source
      std::vector<Node> queue;
      std::array<bool, CHUNK_VOXELS> visited{};
      queue.push_back({src.x, src.y, src.z, src.T, 0});
      visited[idx(src.x, src.y, src.z)] = true;

      size_t head = 0;
      while (head < queue.size()) {
        auto [x,y,z,Tv,dist] = queue[head++];
        // store temperature (distance-attenuated)
        float atten = 1.0f / (1.0f + float(dist) * 0.5f);
        T_new[idx(x,y,z)] = std::max(T_new[idx(x,y,z)], Tv * atten + T_AMB * (1.0f - atten));

        if (dist >= CHUNK_N * 2) continue;
        constexpr int dx[] = {1,-1,0,0,0,0};
        constexpr int dy[] = {0,0,1,-1,0,0};
        constexpr int dz[] = {0,0,0,0,1,-1};
        for (int d = 0; d < 6; ++d) {
          int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
          if (nx < 0 || nx >= CHUNK_N || ny < 0 || ny >= CHUNK_N || nz < 0 || nz >= CHUNK_N) continue;
          int ni = idx(nx, ny, nz);
          if (visited[ni]) continue;
          if (c.mat[ni] == Mat::AIR) continue; // air blocks conduction
          visited[ni] = true;
          float mat_atten = MAT_PROPS[static_cast<int>(c.mat[ni])].k / MAT_PROPS[static_cast<int>(Mat::STONE)].k;
          queue.push_back({nx, ny, nz, Tv * mat_atten, dist + 1});
        }
      }
    }
    c.T = T_new;
  }
}

// D: Gauss-Seidel iterative solver per chunk
void strat_gauss_seidel(Chunk& c, int ticks) {
  for (int t = 0; t < ticks; ++t) {
    for (int gs = 0; gs < MAX_GS_ITER; ++gs) {
      float max_res = 0.0f;
      for (int z = 0; z < CHUNK_N; ++z)
        for (int y = 0; y < CHUNK_N; ++y)
          for (int x = 0; x < CHUNK_N; ++x) {
            int i = idx(x, y, z);
            if (c.mat[i] == Mat::AIR) continue;
            float Tn = c.get_T(x-1,y,z) + c.get_T(x+1,y,z)
                     + c.get_T(x,y-1,z) + c.get_T(x,y+1,z)
                     + c.get_T(x,y,z-1) + c.get_T(x,y,z+1);
            float old = c.T[i];
            c.T[i] = Tn / 6.0f; // Gauss-Seidel update: T_new = average of 6 neighbors
            float res = std::abs(c.T[i] - old);
            if (res > max_res) max_res = res;
          }
      if (max_res < TARGET_RESIDUAL) break;
    }
  }
}

// E: GPU compute analytical projection
void strat_gpu_analytical(Chunk& c, int ticks) {
  // Analytical GPU model: measure no actual GPU work
  // Just do a memory-bound loop to simulate VRAM bandwidth cost
  // RTX 3060 Ti: 448 GB/s, 8 GiB VRAM
  // 8³ chunk = 512 floats = 2 KB per T field, 512 bytes for materials
  // GPU kernel launch: ~5 µs overhead, plus ALU for 6-neighbor stencil
  volatile float sink = 0.0f;
  for (int t = 0; t < ticks; ++t) {
    // simulate 512 FLOPs (stencil) + memory read/write
    for (int i = 0; i < CHUNK_VOXELS; ++i) {
      sink += c.T[i]; // memory read
    }
    // projected GPU time = 5 µs launch + 0.1 µs compute = negligible
  }
  (void)sink;
}

using StrategyFn = void(*)(Chunk&, int);

struct Strategy {
  const char* name;
  StrategyFn fn;
};

constexpr std::array STRATEGIES_DEF = {
  Strategy{"A_NoConduction", strat_none},
  Strategy{"B_ExplicitEuler", strat_explicit_euler},
  Strategy{"C_BFS_Propagation", strat_bfs_propagation},
  Strategy{"D_GaussSeidel", strat_gauss_seidel},
  Strategy{"E_GPU_Analytical", strat_gpu_analytical},
};

// --- Benchmark harness ---
template<typename Container>
double mean(const Container& v) {
  return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

template<typename Container>
double median(Container v) {
  std::nth_element(v.begin(), v.begin() + v.size()/2, v.end());
  return v[v.size()/2];
}

template<typename Container>
double percentile(Container v, double p) {
  size_t idx = size_t(v.size() * p / 100.0);
  std::nth_element(v.begin(), v.begin() + idx, v.end());
  return v[idx];
}

template<typename Container>
double stddev(const Container& v, double m) {
  double sq = 0.0;
  for (auto x : v) sq += (x - m) * (x - m);
  return std::sqrt(sq / v.size());
}

double compute_psnr(const Chunk& result, const Chunk& reference) {
  double mse = 0.0;
  int cnt = 0;
  for (int i = 0; i < CHUNK_VOXELS; ++i) {
    if (result.mat[i] != Mat::AIR) {
      double diff = result.T[i] - reference.T[i];
      mse += diff * diff;
      cnt++;
    }
  }
  if (cnt == 0 || mse < 1e-10) return 99.0;
  mse /= cnt;
  double max_val = 100.0;
  return 10.0 * std::log10((max_val * max_val) / mse);
}

int main() {
  constexpr int N_ITER = 1000;
  constexpr int N_WARMUP = 10;
  constexpr int SEEDS[] = {1, 7, 42, 1234, 31337};
  constexpr int N_SEEDS = 5;

  std::printf("strategy,scene,seed,mean_us,p50_us,p95_us,p99_us,std_us,psnr_db,conv_ticks\n");

  for (auto& strat : STRATEGIES_DEF) {
    for (auto& scene : SCENES) {
      for (int s = 0; s < N_SEEDS; ++s) {
        int seed = SEEDS[s];

        // Prepare analytical reference (for S1: linear gradient)
        Chunk ref;
        ref.set_all_mat(Mat::STONE);
        ref.fill(T_AMB);
        for (int z = 0; z < CHUNK_N; ++z) {
          float t = T_HOT - (T_HOT - T_AMB) * float(z) / (CHUNK_N - 1);
          for (int x = 0; x < CHUNK_N; ++x)
            for (int y = 0; y < CHUNK_N; ++y)
              ref.T[idx(x, y, z)] = t;
        }

        std::array<double, N_ITER> times{};

        // Build chunk
        Chunk chunk;
        std::mt19937 rng(seed);
        scene.build(chunk, rng);
        auto chunk_init = chunk;

        // Warmup
        for (int w = 0; w < N_WARMUP; ++w) {
          chunk = chunk_init;
          strat.fn(chunk, 1);
        }

        // Benchmark
        for (int i = 0; i < N_ITER; ++i) {
          chunk = chunk_init;
          auto t0 = std::chrono::high_resolution_clock::now();
          strat.fn(chunk, 1);
          auto t1 = std::chrono::high_resolution_clock::now();
          double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
          times[i] = us;
        }

        // PSNR vs analytical reference (uniform_stone scene)
        double psnr = compute_psnr(chunk, ref);

        // Convergence: run again with longer sim to get convergence rate
        chunk = chunk_init;
        float prev_mse = 1e10;
        int conv_ticks = -1;
        for (int t = 1; t <= 100; ++t) {
          strat.fn(chunk, 1);
          double mse = 0.0; int cnt = 0;
          for (int i = 0; i < CHUNK_VOXELS; ++i) {
            if (chunk.mat[i] != Mat::AIR) {
              double d = chunk.T[i] - ref.T[i];
              mse += d*d; cnt++;
            }
          }
          if (cnt > 0) mse /= cnt;
          float rel_change = float(std::abs(mse - prev_mse) / std::max(prev_mse, 1e-10f));
          if (rel_change < 0.01f && t > 5) { conv_ticks = t; break; }
          prev_mse = float(mse);
        }
        if (conv_ticks < 0) conv_ticks = 100;

        double m = mean(times);
        double p50 = median(times);
        double p95 = percentile(times, 95);
        double p99 = percentile(times, 99);
        double sd = stddev(times, m);

        std::printf("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%d\n",
          strat.name, scene.name, seed, m, p50, p95, p99, sd, psnr, conv_ticks);
      }
    }
  }

  std::fprintf(stderr, "DONE\n");
  return 0;
}
