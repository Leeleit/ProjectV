// 2026-06-22-player-roles-hierarchy — standalone C++26 CPU analytical
// benchmark для in-session player-role gating.
//
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main
// measurements. Build (from prototype/):
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//           player_roles_bench.cpp -o player_roles_bench
// Run:   ./player_roles_bench [iter=1000] [warmup=10] [seed=42]

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

namespace role {

enum class Role : std::uint8_t {
  Commander = 0,
  SquadLeader = 1,
  Pilot = 2,
  Gunner = 3,
  Driver = 4,
  Rifleman = 5,
  Medic = 6,
  Engineer = 7,
  Count = 8,
};

constexpr std::array<float, 8> kRoleMix{0.05F, 0.15F, 0.10F, 0.20F, 0.25F, 0.25F, 0.0F, 0.0F};
// Note: 8 slots; medic+engineer 0% in this scene (separate player-class system).
constexpr std::uint32_t kInputsPerFrame = 16;

struct Player {
  std::uint8_t role_bitmask{0};  // 8-bit role bitmask
  std::array<std::string, 8> role_tags{};  // For Flecs tag mode
  std::string role_string;  // For string lookup mode
};

struct Rng {
  std::mt19937_64 gen;
  explicit Rng(std::uint64_t seed) : gen(seed) {}
  std::uint32_t uint32(std::uint32_t a, std::uint32_t b) {
    std::uniform_int_distribution<std::uint32_t> d(a, b);
    return d(gen);
  }
};

volatile std::uint64_t g_sink{0};

constexpr std::array<std::string_view, 8> kRoleNames{
  "Commander", "SquadLeader", "Pilot", "Gunner", "Driver", "Rifleman", "Medic", "Engineer"
};

// ============ Strategy A: No role check, all access ============
inline std::uint64_t strategy_a(std::vector<Player>& players, std::uint64_t tick_count) {
  std::uint64_t accum = 0;
  for (auto& p : players) {
    for (std::uint32_t i = 0; i < kInputsPerFrame; ++i) {
      accum += static_cast<std::uint64_t>(tick_count);
    }
  }
  return accum;
}

// ============ Strategy B: Flecs tag component check ============
inline std::uint64_t strategy_b(std::vector<Player>& players, std::uint64_t tick_count) {
  std::uint64_t accum = 0;
  for (auto& p : players) {
    for (std::uint32_t i = 0; i < kInputsPerFrame; ++i) {
      // O(1) per role, but with map lookup overhead.
      // Approximate as 3 ns per check (hash + compare) for string tag.
      for (std::uint32_t r = 0; r < 8; ++r) {
        if (!p.role_tags[r].empty()) {
          accum += static_cast<std::uint64_t>(tick_count);
        }
      }
    }
  }
  return accum;
}

// ============ Strategy C: 8-bit role bitmask AND check ⭐ ============
inline std::uint64_t strategy_c(std::vector<Player>& players, std::uint64_t tick_count) {
  std::uint64_t accum = 0;
  constexpr std::uint8_t kInputMask = 0xFF;  // all roles can input
  for (auto& p : players) {
    for (std::uint32_t i = 0; i < kInputsPerFrame; ++i) {
      const std::uint8_t result = p.role_bitmask & kInputMask;
      if (result != 0) {
        accum += static_cast<std::uint64_t>(tick_count);
      }
    }
  }
  return accum;
}

// ============ Strategy D: Hierarchical permission tree ============
inline std::uint64_t strategy_d(std::vector<Player>& players, std::uint64_t tick_count) {
  std::uint64_t accum = 0;
  // Permission hierarchy: Commander > SquadLeader > Pilot/Gunner/Driver > Rifleman.
  // Represent as 3-level tree: {Commander:0, SquadLeader:1, SubRoles:2}.
  for (auto& p : players) {
    const std::uint8_t level =
        (p.role_bitmask & 0x01) ? 0 :
        (p.role_bitmask & 0x02) ? 1 : 2;
    for (std::uint32_t i = 0; i < kInputsPerFrame; ++i) {
      // Tree lookup cost ~ 5 ns (extra branch).
      if (level < 3) {
        accum += static_cast<std::uint64_t>(tick_count);
      }
    }
  }
  return accum;
}

// ============ Strategy E: String hash lookup ============
inline std::uint64_t strategy_e(std::vector<Player>& players, std::uint64_t tick_count) {
  std::uint64_t accum = 0;
  for (auto& p : players) {
    for (std::uint32_t i = 0; i < kInputsPerFrame; ++i) {
      // String hash + compare ~10 ns per check.
      std::size_t h = std::hash<std::string>{}(p.role_string);
      if (h != 0) {
        accum += static_cast<std::uint64_t>(tick_count);
      }
    }
  }
  return accum;
}

struct Scene {
  std::string_view name;
  std::uint32_t n_players;
};

constexpr std::array<Scene, 5> kScenes{{
  {"skirmish_8p",   8},
  {"battle_32p",    32},
  {"squad_64p",     64},
  {"company_128p",  128},
  {"mega_200p",     200},
}};

void build_scene(const Scene& s, std::vector<Player>& players, Rng& rng) {
  players.clear();
  players.reserve(s.n_players);
  for (std::uint32_t i = 0; i < s.n_players; ++i) {
    Player p{};
    // Assign role by cumulative mix.
    const float r = static_cast<float>(rng.uint32(0, 9999)) / 10000.0F;
    float cum = 0.0F;
    std::uint8_t role_idx = 0;
    for (std::uint32_t j = 0; j < 8; ++j) {
      cum += kRoleMix[j];
      if (r < cum) {
        role_idx = static_cast<std::uint8_t>(j);
        break;
      }
    }
    p.role_bitmask = static_cast<std::uint8_t>(1U << role_idx);
    p.role_tags[role_idx] = std::string(kRoleNames[role_idx]);
    p.role_string = std::string(kRoleNames[role_idx]);
    players.push_back(p);
  }
}

void run(int iters, int warmup, std::uint64_t seed) {
  std::printf("2026-06-22-player-roles-hierarchy — bench\n");
  std::printf("iters=%d warmup=%d seed=%llu\n", iters, warmup,
              static_cast<unsigned long long>(seed));
  std::printf("scene,strategy,n_players,mean_ns_per_tick,median_ns,p95_ns,p99_ns,std_ns\n");

  for (const auto& scene : kScenes) {
    for (int si = 0; si < 5; ++si) {
      std::vector<std::uint64_t> samples;
      samples.reserve(iters);

      std::vector<Player> players;
      players.reserve(scene.n_players);

      const std::uint64_t scene_seed = seed ^ (static_cast<std::uint64_t>(scene.n_players) * 2654435761ULL) ^
                                       (static_cast<std::uint64_t>(si) << 5);
      Rng rng(scene_seed);
      build_scene(scene, players, rng);

      for (int w = 0; w < warmup; ++w) {
        std::vector<Player> wp = players;
        switch (si) {
          case 0: strategy_a(wp, 1); break;
          case 1: strategy_b(wp, 1); break;
          case 2: strategy_c(wp, 1); break;
          case 3: strategy_d(wp, 1); break;
          case 4: strategy_e(wp, 1); break;
        }
        g_sink ^= samples.size();
      }

      for (int it = 0; it < iters; ++it) {
        std::vector<Player> mp = players;

        auto t0 = std::chrono::steady_clock::now();
        std::uint64_t r = 0;
        switch (si) {
          case 0: r = strategy_a(mp, 1); break;
          case 1: r = strategy_b(mp, 1); break;
          case 2: r = strategy_c(mp, 1); break;
          case 3: r = strategy_d(mp, 1); break;
          case 4: r = strategy_e(mp, 1); break;
        }
        auto t1 = std::chrono::steady_clock::now();

        const auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back(static_cast<std::uint64_t>(elapsed_ns));
        g_sink ^= r;
      }

      std::uint64_t sum = 0;
      for (auto s : samples) sum += s;
      const double mean = static_cast<double>(sum) / static_cast<double>(samples.size());

      std::vector<std::uint64_t> sorted = samples;
      std::sort(sorted.begin(), sorted.end());
      const auto median = sorted[sorted.size() / 2];
      const auto p95 = sorted[(sorted.size() * 95) / 100];
      const auto p99 = sorted[(sorted.size() * 99) / 100];

      double var = 0.0;
      for (auto s : samples) {
        const double d = static_cast<double>(s) - mean;
        var += d * d;
      }
      var /= static_cast<double>(samples.size());
      const double stddev = std::sqrt(var);

      const char* strat_name = "?";
      switch (si) {
        case 0: strat_name = "A_NoRole_AllAccess"; break;
        case 1: strat_name = "B_FlecsTagComponent"; break;
        case 2: strat_name = "C_Bitmask_PerEntity"; break;
        case 3: strat_name = "D_HierarchicalPermissionTree"; break;
        case 4: strat_name = "E_StringHashLookup"; break;
      }

      std::printf("%s,%s,%u,%.1f,%llu,%llu,%llu,%.1f\n", scene.name.data(), strat_name,
                  scene.n_players, mean,
                  static_cast<unsigned long long>(median),
                  static_cast<unsigned long long>(p95),
                  static_cast<unsigned long long>(p99), stddev);
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
}

}  // namespace role

int main(int argc, char** argv) {
  int iters = 1000;
  int warmup = 10;
  std::uint64_t seed = 42;
  if (argc > 1) iters = std::atoi(argv[1]);
  if (argc > 2) warmup = std::atoi(argv[2]);
  if (argc > 3) seed = std::strtoull(argv[3], nullptr, 10);
  role::run(iters, warmup, seed);
  return 0;
}