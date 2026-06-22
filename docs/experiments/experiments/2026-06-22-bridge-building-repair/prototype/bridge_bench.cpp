#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

static constexpr int WORLD_X = 256;
static constexpr int WORLD_Y = 64;
static constexpr int WORLD_Z = 256;

struct GenerationWorld {
  uint16_t gen[WORLD_X][WORLD_Y][WORLD_Z]{};
  uint64_t epoch = 1;
  int terrain[WORLD_X][WORLD_Z]{};

  void init_flat(int y_base) {
    epoch = 1;
    std::memset(gen, 0, sizeof(gen));
    for (int x = 0; x < WORLD_X; x++)
      for (int z = 0; z < WORLD_Z; z++)
        terrain[x][z] = y_base;
  }

  void carve_gap(int z0, int z1, int floor_y) {
    for (int x = 0; x < WORLD_X; x++)
      for (int z = z0; z < z1; z++)
        terrain[x][z] = floor_y;
  }

  void fill_water(int z0, int z1, int level) {
    for (int x = 0; x < WORLD_X; x++)
      for (int z = z0; z < z1; z++)
        for (int y = 0; y < level; y++)
          gen[x][y][z] = uint16_t(epoch);
  }

  void init_for_scene(int gap_z0, int gap_z1, int water_y) {
    init_flat(8);
    carve_gap(gap_z0, gap_z1, 5);
    if (water_y >= 0)
      fill_water(gap_z0, gap_z1, water_y);
  }

  bool is_solid(int x, int y, int z) const {
    if (x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z)
      return false;
    return gen[x][y][z] == epoch;
  }

  void set(int x, int y, int z) {
    gen[x][y][z] = uint16_t(epoch);
  }

  void set_run(int x, int y, int z0, int z1) {
    for (int z = z0; z < z1; z++)
      gen[x][y][z] = uint16_t(epoch);
  }

  void next_epoch() {
    epoch++;
  }
};

struct RLESpan {
  int x, y, z0, z1;
};

struct BridgeTemplate {
  std::vector<RLESpan> spans;
  int nx = 0, ny = 0, nz = 0;
  int total_voxels = 0;
};

struct Scene {
  std::string_view name;
  int ox, oy, oz;
  int nx, ny, nz;
  int gap_z0, gap_z1;
  int water_y;
  BridgeTemplate tmpl;
  std::vector<uint8_t> dense;
};

struct StrategyResult {
  int voxels_placed = 0;
  int largest_component = 0;
  int segments = 0;
};

static auto tidx(int x, int y, int z, int ny, int nz) -> int {
  return (x * ny + y) * nz + z;
}

static auto make_template(int nx, int ny, int nz, int scene_type) -> std::vector<uint8_t> {
  std::vector<uint8_t> t(nx * ny * nz, 0);
  if (scene_type == 0 || scene_type == 2) {
    for (int x = 0; x < nx; x++)
      for (int z = 0; z < nz; z++)
        t[tidx(x, 0, z, ny, nz)] = 1;
  } else if (scene_type == 1) {
    for (int x = 0; x < nx; x++)
      for (int z = 0; z < nz; z++)
        t[tidx(x, 0, z, ny, nz)] = 1;
    for (int y = 1; y < ny; y++)
      for (int z = 0; z < nz; z++)
        if ((y + z) % 2 == 0) {
          t[tidx(0, y, z, ny, nz)] = 1;
          t[tidx(nx - 1, y, z, ny, nz)] = 1;
        }
  } else if (scene_type == 3) {
    for (int x = 0; x < nx; x++)
      for (int z = 0; z < nz; z++)
        t[tidx(x, 0, z, ny, nz)] = 1;
    int cx = nx / 2;
    for (int y = 1; y < ny; y++) {
      t[tidx(cx, y, 0, ny, nz)] = 1;
      t[tidx(cx, y, nz - 1, ny, nz)] = 1;
    }
    for (int z = 1; z < nz - 1; z++) {
      float f = float(z) / float(nz - 1) - 0.5f;
      float cable_y = float(ny - 1) * (1.0f - 4.0f * f * f);
      int cy = int(cable_y + 0.5f);
      if (cy > 0)
        t[tidx(cx, cy, z, ny, nz)] = 1;
    }
  } else if (scene_type == 4) {
    for (int x = 0; x < nx; x++)
      for (int z = 0; z < nz; z++)
        t[tidx(x, 0, z, ny, nz)] = 1;
    for (int y = 1; y < ny; y++)
      for (int z = 0; z < nz; z++)
        if ((y + z) % 3 == 0) {
          t[tidx(0, y, z, ny, nz)] = 1;
          t[tidx(nx - 1, y, z, ny, nz)] = 1;
        }
    int d0 = int(nz * 0.3f);
    int d1 = int(nz * 0.7f);
    for (int x = 0; x < nx; x++)
      for (int y = 0; y < ny; y++)
        for (int z = d0; z < d1; z++)
          t[tidx(x, y, z, ny, nz)] = 0;
  }
  return t;
}

static auto extract_rle(const std::vector<uint8_t>& t, int nx, int ny, int nz) -> BridgeTemplate {
  BridgeTemplate bt;
  bt.nx = nx; bt.ny = ny; bt.nz = nz;
  for (int x = 0; x < nx; x++) {
    for (int y = 0; y < ny; y++) {
      int z = 0;
      while (z < nz) {
        if (t[tidx(x, y, z, ny, nz)]) {
          int z0 = z;
          while (z < nz && t[tidx(x, y, z, ny, nz)])
            z++;
          bt.spans.push_back({x, y, z0, z});
          bt.total_voxels += (z - z0);
        } else {
          z++;
        }
      }
    }
  }
  return bt;
}

static auto make_scene(std::string_view name, int ox, int oy, int oz,
                       int nx, int ny, int nz,
                       int gap_z0, int gap_z1, int water_y,
                       int scene_type) -> Scene {
  Scene s;
  s.name = name;
  s.ox = ox; s.oy = oy; s.oz = oz;
  s.nx = nx; s.ny = ny; s.nz = nz;
  s.gap_z0 = gap_z0; s.gap_z1 = gap_z1;
  s.water_y = water_y;
  s.dense = make_template(nx, ny, nz, scene_type);
  s.tmpl = extract_rle(s.dense, nx, ny, nz);
  return s;
}

static void init_scenes(std::vector<Scene>& scenes) {
  scenes.push_back(make_scene("assault_bridge_20m",  124, 9, 60,  8, 1, 40,  60, 100,  -1, 0));
  scenes.push_back(make_scene("bailey_60t_40m",      124, 9, 40,  8, 6, 80,  40, 120,  -1, 1));
  scenes.push_back(make_scene("pontoon_water_100m",  124,12, 28,  8, 1, 200, 28, 228,  12, 2));
  scenes.push_back(make_scene("suspension_cable_80m",124, 9, 40,  8,20, 160, 40, 200,  -1, 3));
  scenes.push_back(make_scene("damaged_bridge_repair",124,9, 60,  8, 4, 80,  60, 140,  -1, 4));
}

static void strategy_A(GenerationWorld& w, const Scene& s, StrategyResult& r) {
  r.voxels_placed = 0;
  for (int x = 0; x < s.nx; x++) {
    int wx = s.ox + x;
    if (wx < 0 || wx >= WORLD_X) continue;
    for (int y = 0; y < s.ny; y++) {
      int wy = s.oy + y;
      if (wy < 0 || wy >= WORLD_Y) continue;
      for (int z = 0; z < s.nz; z++) {
        int wz = s.oz + z;
        if (wz < 0 || wz >= WORLD_Z) continue;
        if (s.dense[tidx(x, y, z, s.ny, s.nz)]) {
          w.set(wx, wy, wz);
          r.voxels_placed++;
        }
      }
    }
  }
  r.largest_component = r.voxels_placed;
  r.segments = 0;
}

static void strategy_B(GenerationWorld& w, const Scene& s, StrategyResult& r) {
  r.voxels_placed = 0;
  for (const auto& sp : s.tmpl.spans) {
    int wx = s.ox + sp.x;
    int wy = s.oy + sp.y;
    if (wx < 0 || wx >= WORLD_X || wy < 0 || wy >= WORLD_Y) continue;
    int w0 = s.oz + sp.z0;
    int w1 = s.oz + sp.z1;
    if (w0 < 0) w0 = 0;
    if (w1 > WORLD_Z) w1 = WORLD_Z;
    if (w0 >= w1) continue;
    w.set_run(wx, wy, w0, w1);
    r.voxels_placed += (w1 - w0);
  }
  r.largest_component = r.voxels_placed;
  r.segments = 0;
}

static void strategy_C(GenerationWorld& w, const Scene& s, StrategyResult& r) {
  r.voxels_placed = 0;
  for (const auto& sp : s.tmpl.spans) {
    int wx = s.ox + sp.x;
    int wy = s.oy + sp.y;
    if (wx < 0 || wx >= WORLD_X || wy < 0 || wy >= WORLD_Y) continue;
    int w0 = s.oz + sp.z0;
    int w1 = s.oz + sp.z1;
    if (w0 < 0) w0 = 0;
    if (w1 > WORLD_Z) w1 = WORLD_Z;
    if (w0 >= w1) continue;
    if (sp.y == 0) {
      for (int z = w0; z < w1; z++) {
        int t_h = w.terrain[wx][z];
        if (t_h < wy) {
          for (int fy = t_h; fy < wy; fy++) {
            w.set(wx, fy, z);
            r.voxels_placed++;
          }
        }
        w.set(wx, wy, z);
        r.voxels_placed++;
      }
    } else {
      w.set_run(wx, wy, w0, w1);
      r.voxels_placed += (w1 - w0);
    }
  }
  r.largest_component = r.voxels_placed;
  r.segments = 0;
}

static void strategy_D(GenerationWorld& w, const Scene& s, StrategyResult& r) {
  r.voxels_placed = 0;
  for (const auto& sp : s.tmpl.spans) {
    int wx = s.ox + sp.x;
    int wy = s.oy + sp.y;
    if (wx < 0 || wx >= WORLD_X || wy < 0 || wy >= WORLD_Y) continue;
    if (wy != s.water_y) continue;
    int w0 = s.oz + sp.z0;
    int w1 = s.oz + sp.z1;
    if (w0 < 0) w0 = 0;
    if (w1 > WORLD_Z) w1 = WORLD_Z;
    if (w0 >= w1) continue;
    w.set_run(wx, wy, w0, w1);
    r.voxels_placed += (w1 - w0);
  }
  r.largest_component = r.voxels_placed;
  r.segments = 0;
}

static int ccl_largest(GenerationWorld& w, int x0, int y0, int z0, int x1, int y1, int z1) {
  int nx = x1 - x0;
  int ny = y1 - y0;
  int nz = z1 - z0;
  if (nx <= 0 || ny <= 0 || nz <= 0) return 0;
  std::vector<uint8_t> visited(nx * ny * nz, 0);
  auto vi = [&](int x, int y, int z) { return (x * ny + y) * nz + z; };
  static constexpr int DX[6] = {1, -1, 0, 0, 0, 0};
  static constexpr int DY[6] = {0, 0, 1, -1, 0, 0};
  static constexpr int DZ[6] = {0, 0, 0, 0, 1, -1};
  std::vector<int> stack;
  int largest = 0;
  for (int sx = 0; sx < nx; sx++) {
    for (int sy = 0; sy < ny; sy++) {
      for (int sz = 0; sz < nz; sz++) {
        int idx = vi(sx, sy, sz);
        if (visited[idx]) continue;
        visited[idx] = 1;
        if (!w.is_solid(x0 + sx, y0 + sy, z0 + sz)) continue;
        int count = 0;
        stack.clear();
        stack.push_back(idx);
        while (!stack.empty()) {
          int c = stack.back();
          stack.pop_back();
          count++;
          int cx = c / (ny * nz);
          int rem = c % (ny * nz);
          int cy = rem / nz;
          int cz = rem % nz;
          for (int d = 0; d < 6; d++) {
            int nx2 = cx + DX[d];
            int ny2 = cy + DY[d];
            int nz2 = cz + DZ[d];
            if (nx2 < 0 || nx2 >= nx || ny2 < 0 || ny2 >= ny || nz2 < 0 || nz2 >= nz) continue;
            int nid = vi(nx2, ny2, nz2);
            if (visited[nid]) continue;
            visited[nid] = 1;
            if (w.is_solid(x0 + nx2, y0 + ny2, z0 + nz2))
              stack.push_back(nid);
          }
        }
        if (count > largest) largest = count;
      }
    }
  }
  return largest;
}

static void strategy_E(GenerationWorld& w, const Scene& s, StrategyResult& r) {
  static constexpr int SEGMENT_VOXELS = 10;
  r.voxels_placed = 0;
  int n_seg = (s.nz + SEGMENT_VOXELS - 1) / SEGMENT_VOXELS;
  for (int seg = 0; seg < n_seg; seg++) {
    int sz0 = s.oz + seg * SEGMENT_VOXELS;
    int sz1 = std::min(s.oz + (seg + 1) * SEGMENT_VOXELS, s.oz + s.nz);
    for (const auto& sp : s.tmpl.spans) {
      int wx = s.ox + sp.x;
      int wy = s.oy + sp.y;
      if (wx < 0 || wx >= WORLD_X || wy < 0 || wy >= WORLD_Y) continue;
      int iz0 = std::max(s.oz + sp.z0, sz0);
      int iz1 = std::min(s.oz + sp.z1, sz1);
      if (iz0 >= iz1) continue;
      w.set_run(wx, wy, iz0, iz1);
      r.voxels_placed += (iz1 - iz0);
    }
  }
  r.segments = n_seg;
  r.largest_component = ccl_largest(w, s.ox, s.oy, s.oz,
                                      s.ox + s.nx, s.oy + s.ny, s.oz + s.nz);
}

struct BenchResult {
  double mean_us;
  int voxels_placed;
  int largest_component;
  int segments;
};

static auto run_bench(GenerationWorld& w, const Scene& scene, int strategy,
                      int seed, int warmup, int iterations) -> BenchResult {
  std::mt19937_64 rng(seed);
  w.init_for_scene(scene.gap_z0, scene.gap_z1, scene.water_y);

  void (*fn)(GenerationWorld&, const Scene&, StrategyResult&) = nullptr;
  switch (strategy) {
    case 0: fn = strategy_A; break;
    case 1: fn = strategy_B; break;
    case 2: fn = strategy_C; break;
    case 3: fn = strategy_D; break;
    case 4: fn = strategy_E; break;
  }

  StrategyResult result;
  for (int i = 0; i < warmup; i++) {
    w.next_epoch();
    fn(w, scene, result);
  }

  using Clock = std::chrono::high_resolution_clock;
  auto t0 = Clock::now();
  for (int i = 0; i < iterations; i++) {
    w.next_epoch();
    fn(w, scene, result);
  }
  auto t1 = Clock::now();

  double total_us = double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / 1000.0;

  BenchResult br;
  br.mean_us = total_us / double(iterations);
  br.voxels_placed = result.voxels_placed;
  br.largest_component = result.largest_component;
  br.segments = result.segments;
  return br;
}

int main() {
  std::vector<Scene> scenes;
  init_scenes(scenes);

  auto w = std::make_unique<GenerationWorld>();

  printf("strategy,scene,seed,time_us,voxels_placed,largest_component,segments\n");

  static constexpr int WARMUP = 10;
  static constexpr int ITERATIONS = 1000;
  static constexpr int N_SEEDS = 5;

  for (int si = 0; si < 5; si++) {
    for (const auto& scene : scenes) {
      for (int seed = 0; seed < N_SEEDS; seed++) {
        auto r = run_bench(*w, scene, si, seed, WARMUP, ITERATIONS);
        printf("%c,%s,%d,%.2f,%d,%d,%d\n",
               'A' + si, scene.name.data(), seed,
               r.mean_us, r.voxels_placed,
               r.largest_component, r.segments);
      }
    }
  }

  return 0;
}
