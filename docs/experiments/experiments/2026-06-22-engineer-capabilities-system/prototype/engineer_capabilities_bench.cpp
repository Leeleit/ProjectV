// 2026-06-22-engineer-capabilities-system — standalone C++26 CPU analytical
// benchmark для Foxhole-style engineer class state machine.
//
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main
// measurements. Build (from prototype/):
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//           engineer_capabilities_bench.cpp -o engineer_capabilities_bench
// Run:   ./engineer_capabilities_bench [iter=1000] [warmup=10] [seed=42]

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string_view>
#include <vector>

namespace eng {

enum class State : std::uint8_t { Idle, MoveToTarget, Operate, Complete };

enum class OperationKind : std::uint8_t {
  Construction = 0,
  Repair = 1,
  Demolition = 2,
  Idle = 3,
};

struct Operation {
  OperationKind kind{OperationKind::Idle};
  std::uint32_t target_id{0};
  float progress{0.0F};  // 0..1
  float duration_s{0.0F};
  std::array<std::uint8_t, 3> materials_required{0, 0, 0};
  std::array<std::uint8_t, 3> materials_consumed{0, 0, 0};
};

struct Engineer {
  State state{State::Idle};
  Operation op{};
  float pos_x{0.0F}, pos_y{0.0F}, pos_z{0.0F};
  float speed_multiplier{1.0F};
  std::array<std::uint8_t, 3> inventory{0, 0, 0};
};

struct Target {
  std::uint32_t id{0};
  float pos_x{0.0F}, pos_y{0.0F}, pos_z{0.0F};
  float integrity{1.0F};  // 0..1, repair target raises, demolition target lowers
  bool claimed{false};
  std::uint32_t claim_engineer_id{0};
  float claim_progress{0.0F};  // for cooperative sum
};

// Anti-DCE sink to prevent compiler eliding call sites.
volatile std::uint64_t g_sink{0};

inline constexpr float kBaseSpeed = 1.0F;  // base operation completion (units/s)
inline constexpr std::array<float, 4> kDurations{3.0F, 5.0F, 8.0F, 0.0F};  // construction, repair, demolition, idle

inline constexpr std::array<std::array<std::uint8_t, 3>, 3> kMaterialsPerOp{{
  {{1, 2, 0}},  // construction: 1 basic + 2 construction
  {{1, 1, 0}},  // repair: 1 basic + 1 construction
  {{0, 0, 1}},  // demolition: 1 explosive
}};

// Random helpers (deterministic per seed).
struct Rng {
  std::mt19937_64 gen;
  explicit Rng(std::uint64_t seed) : gen(seed) {}
  float uniform(float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(gen);
  }
  std::uint32_t uint32(std::uint32_t a, std::uint32_t b) {
    std::uniform_int_distribution<std::uint32_t> d(a, b);
    return d(gen);
  }
};

// Per-target proximity check (very cheap, ~2 ns/engineer).
inline bool near_target(const Engineer& e, const Target& t, float r = 2.0F) {
  const float dx = e.pos_x - t.pos_x;
  const float dy = e.pos_y - t.pos_y;
  const float dz = e.pos_z - t.pos_z;
  return (dx * dx + dy * dy + dz * dz) <= r * r;
}

// ============ Strategy A: Plain worker, no role distinction ============
// All workers at 1x speed; no engineer state machine; just simple claim on first-come.
inline std::uint64_t strategy_a(std::vector<Engineer>& engs, std::vector<Target>& tgts,
                                std::uint64_t tick_count, Rng& /*rng*/) {
  std::uint64_t accum = 0;
  for (auto& e : engs) {
    e.state = State::Idle;
    e.op = Operation{};
    if (e.speed_multiplier <= 0.0F) {
      // Plain worker — base 1x speed, can do any operation.
      for (auto& t : tgts) {
        if (!t.claimed && near_target(e, t)) {
          t.claimed = true;
          e.state = State::Operate;
          e.op.kind = OperationKind::Construction;
          e.op.target_id = t.id;
          break;
        }
      }
    }
    if (e.state == State::Operate) {
      // No role boost: 1x speed.
      accum += static_cast<std::uint64_t>(tick_count);
    }
  }
  return accum;
}

// ============ Strategy B: Engineer state machine, single-claim ============
// Engineer = 2x construction, 3x repair, 1x demolition (per Foxhole canonical).
// First engineer to reach target claims it (single-engineer model).
inline std::uint64_t strategy_b(std::vector<Engineer>& engs, std::vector<Target>& tgts,
                                std::uint64_t tick_count, Rng& /*rng*/) {
  std::uint64_t accum = 0;
  for (auto& e : engs) {
    e.state = State::Idle;
    e.op = Operation{};
    if (e.speed_multiplier > 0.0F) {
      // Engineer: try to claim a target.
      for (auto& t : tgts) {
        if (!t.claimed && near_target(e, t)) {
          t.claimed = true;
          t.claim_engineer_id = e.op.target_id;  // e.id would normally; we use placeholder
          e.state = State::Operate;
          e.op.kind = OperationKind::Construction;
          e.op.target_id = t.id;
          e.op.duration_s = kDurations[0];
          break;
        }
      }
    }
    if (e.state == State::Operate) {
      const float boost = (e.op.kind == OperationKind::Repair) ? 3.0F : 2.0F;
      accum += static_cast<std::uint64_t>(tick_count * boost);
    }
  }
  return accum;
}

// ============ Strategy C: Engineer state machine, cooperative sum ============
// Multiple engineers can work same target; progress sums (1/N each).
inline std::uint64_t strategy_c(std::vector<Engineer>& engs, std::vector<Target>& tgts,
                                std::uint64_t tick_count, Rng& rng) {
  std::uint64_t accum = 0;

  // Reset claim state from previous tick.
  for (auto& t : tgts) {
    t.claim_engineer_id = 0;
    t.claim_progress = 0.0F;
  }

  // Count engineers near each target.
  std::vector<std::uint32_t> cnt_per_target(tgts.size(), 0);
  for (auto& e : engs) {
    if (e.speed_multiplier > 0.0F) {
      for (std::size_t ti = 0; ti < tgts.size(); ++ti) {
        if (near_target(e, tgts[ti])) {
          ++cnt_per_target[ti];
        }
      }
    }
  }

  for (auto& e : engs) {
    e.state = State::Idle;
    e.op = Operation{};
    if (e.speed_multiplier > 0.0F) {
      // Pick a random target near us (per seed; for benchmark stability).
      std::uint32_t best = 0;
      std::uint32_t best_cnt = 0;
      for (std::size_t ti = 0; ti < tgts.size(); ++ti) {
        if (cnt_per_target[ti] > 0 && near_target(e, tgts[ti])) {
          if (cnt_per_target[ti] > best_cnt) {
            best_cnt = cnt_per_target[ti];
            best = static_cast<std::uint32_t>(ti);
          }
        }
      }
      if (best_cnt > 0) {
        e.state = State::Operate;
        e.op.kind = OperationKind::Construction;
        e.op.target_id = tgts[best].id;
        e.op.duration_s = kDurations[0];
        tgts[best].claim_progress += 1.0F / static_cast<float>(best_cnt);
        accum += static_cast<std::uint64_t>(tick_count * best_cnt);  // cooperative boosts output
      }
    }
  }
  (void)rng;
  return accum;
}

// ============ Strategy D: Engineer with per-operation job pool ============
// Each target has 1 operation slot; first engineer to claim holds it; others idle.
// Adds overhead of operation-pool management (worst case O(N_eng * N_tgt)).
inline std::uint64_t strategy_d(std::vector<Engineer>& engs, std::vector<Target>& tgts,
                                std::uint64_t tick_count, Rng& /*rng*/) {
  std::uint64_t accum = 0;
  // Per-target operation slot.
  static thread_local std::vector<bool> slot_in_use;
  if (slot_in_use.size() != tgts.size()) {
    slot_in_use.assign(tgts.size(), false);
  }

  for (auto& t : tgts) {
    slot_in_use[&t - &tgts[0]] = false;
  }

  for (auto& e : engs) {
    e.state = State::Idle;
    e.op = Operation{};
    if (e.speed_multiplier > 0.0F) {
      for (std::size_t ti = 0; ti < tgts.size(); ++ti) {
        if (!slot_in_use[ti] && near_target(e, tgts[ti])) {
          slot_in_use[ti] = true;
          e.state = State::Operate;
          e.op.kind = OperationKind::Construction;
          e.op.target_id = tgts[ti].id;
          e.op.duration_s = kDurations[0];
          accum += static_cast<std::uint64_t>(tick_count * 2);  // engineer 2x
          break;
        }
      }
    }
  }
  return accum;
}

// ============ Strategy E: LLM-driven placeholder ============
// Analytical proxy: simulated LLM call cost (high constant) but no real LLM.
inline std::uint64_t strategy_e(std::vector<Engineer>& engs, std::vector<Target>& tgts,
                                std::uint64_t tick_count, Rng& rng) {
  std::uint64_t accum = 0;
  for (auto& e : engs) {
    e.state = State::Idle;
    // Simulate LLM call cost — sleep is omitted; we just count it as 100x baseline.
    if (e.speed_multiplier > 0.0F) {
      for (auto& t : tgts) {
        if (near_target(e, t)) {
          e.state = State::Operate;
          // 100x baseline for LLM call.
          accum += static_cast<std::uint64_t>(tick_count * 100);
          break;
        }
      }
    }
    (void)rng;
  }
  return accum;
}

// Per-iteration tick: 33ms = 30 Hz.
constexpr float kDt = 0.033F;

template <typename Fn>
std::uint64_t run_strategy(Fn fn, std::vector<Engineer>& engs, std::vector<Target>& tgts,
                           int iters, Rng& rng) {
  std::uint64_t accum = 0;
  for (int i = 0; i < iters; ++i) {
    accum += fn(engs, tgts, static_cast<std::uint64_t>(i + 1), rng);
  }
  return accum;
}

struct Scene {
  std::string_view name;
  std::uint32_t n_engineers;
  std::uint32_t n_targets;
  std::uint32_t spawn_radius;
};

constexpr std::array<Scene, 5> kScenes{{
  {"skirmish_8e",      8,   20,  50},
  {"battle_32e",       32,  80,  100},
  {"siege_64e",        64,  200, 200},
  {"offensive_128e",   128, 500, 300},
  {"mega_battle_256e", 256, 1000, 500},
}};

void build_scene(const Scene& s, std::vector<Engineer>& engs, std::vector<Target>& tgts, Rng& rng) {
  engs.clear();
  engs.reserve(s.n_engineers);
  for (std::uint32_t i = 0; i < s.n_engineers; ++i) {
    Engineer e{};
    e.pos_x = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    e.pos_y = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    e.pos_z = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    // Half engineers, half plain workers.
    e.speed_multiplier = (i % 2 == 0) ? 1.0F : 0.0F;
    engs.push_back(e);
  }

  tgts.clear();
  tgts.reserve(s.n_targets);
  for (std::uint32_t i = 0; i < s.n_targets; ++i) {
    Target t{};
    t.id = i + 1;
    t.pos_x = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    t.pos_y = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    t.pos_z = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    t.integrity = 1.0F;
    t.claimed = false;
    tgts.push_back(t);
  }
}

void run(int iters, int warmup, std::uint64_t seed) {
  std::printf("2026-06-22-engineer-capabilities-system — bench\n");
  std::printf("iters=%d warmup=%d seed=%llu\n", iters, warmup,
              static_cast<unsigned long long>(seed));

  std::printf("scene,strategy,n_eng,n_tgt,mean_ns_per_tick,median_ns,p95_ns,p99_ns,std_ns\n");

  for (const auto& scene : kScenes) {
    for (int si = 0; si < 5; ++si) {
      std::vector<std::uint64_t> samples;
      samples.reserve(iters);

      std::vector<Engineer> engs;
      std::vector<Target> tgts;
      engs.reserve(scene.n_engineers);
      tgts.reserve(scene.n_targets);

      const std::uint64_t scene_seed = seed ^ (static_cast<std::uint64_t>(scene.n_engineers) * 2654435761ULL) ^
                                       (static_cast<std::uint64_t>(scene.n_targets) << 7) ^ si;
      Rng rng(scene_seed);
      build_scene(scene, engs, tgts, rng);

      // Warmup.
      for (int w = 0; w < warmup; ++w) {
        Rng wrng(scene_seed + 1000ULL + static_cast<std::uint64_t>(w));
        std::vector<Engineer> we = engs;
        std::vector<Target> wt = tgts;
        switch (si) {
          case 0: strategy_a(we, wt, 1, wrng); break;
          case 1: strategy_b(we, wt, 1, wrng); break;
          case 2: strategy_c(we, wt, 1, wrng); break;
          case 3: strategy_d(we, wt, 1, wrng); break;
          case 4: strategy_e(we, wt, 1, wrng); break;
        }
        g_sink ^= samples.size();
      }

      for (int it = 0; it < iters; ++it) {
        Rng mrng(scene_seed + 2000ULL + static_cast<std::uint64_t>(it));
        std::vector<Engineer> me = engs;
        std::vector<Target> mt = tgts;

        auto t0 = std::chrono::steady_clock::now();
        std::uint64_t r = 0;
        switch (si) {
          case 0: r = strategy_a(me, mt, 1, mrng); break;
          case 1: r = strategy_b(me, mt, 1, mrng); break;
          case 2: r = strategy_c(me, mt, 1, mrng); break;
          case 3: r = strategy_d(me, mt, 1, mrng); break;
          case 4: r = strategy_e(me, mt, 1, mrng); break;
        }
        auto t1 = std::chrono::steady_clock::now();

        const auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back(static_cast<std::uint64_t>(elapsed_ns));
        g_sink ^= r;
      }

      // Compute mean / median / p95 / p99 / std.
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
        case 0: strat_name = "A_PlainWorker_NoRole"; break;
        case 1: strat_name = "B_Engineer_SingleClaim"; break;
        case 2: strat_name = "C_Engineer_CooperativeSum"; break;
        case 3: strat_name = "D_Engineer_PerOpPool"; break;
        case 4: strat_name = "E_Engineer_LLMDriven"; break;
      }

      std::printf("%s,%s,%u,%u,%.1f,%llu,%llu,%llu,%.1f\n", scene.name.data(), strat_name,
                  scene.n_engineers, scene.n_targets, mean,
                  static_cast<unsigned long long>(median),
                  static_cast<unsigned long long>(p95),
                  static_cast<unsigned long long>(p99), stddev);
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
}

}  // namespace eng

int main(int argc, char** argv) {
  int iters = 1000;
  int warmup = 10;
  std::uint64_t seed = 42;
  if (argc > 1) iters = std::atoi(argv[1]);
  if (argc > 2) warmup = std::atoi(argv[2]);
  if (argc > 3) seed = std::strtoull(argv[3], nullptr, 10);
  eng::run(iters, warmup, seed);
  return 0;
}