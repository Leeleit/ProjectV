// 2026-06-22-capture-repair-enemy-equipment — standalone C++26 CPU analytical
// benchmark для Foxhole/Warno-style field capture-repair state machine.
//
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main
// measurements. Build (from prototype/):
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//           capture_repair_bench.cpp -o capture_repair_bench
// Run:   ./capture_repair_bench [iter=1000] [warmup=10] [seed=42]

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

namespace cap {

enum class State : std::uint8_t { Neutral, Capturing, Captured, Repairing, Operational };

struct CaptureOp {
  State state{State::Neutral};
  std::uint32_t target_id{0};
  float pos_x{0.0F}, pos_y{0.0F}, pos_z{0.0F};
  float progress{0.0F};            // 0..1, capture timer 10-30 sec
  float repair_progress{0.0F};     // 0..1, repair progress
  float effectiveness{0.5F};       // 0..1, faction-adaptation penalty starts at 0.5
  float tau_adapt{30.0F};          // adaptation time constant (sec)
  std::array<std::uint8_t, 3> materials{0, 0, 0};  // for repair
  std::uint32_t engineer_id{0};    // for engineer-boosted repair
};

// Anti-DCE sink.
volatile std::uint64_t g_sink{0};

inline constexpr float kDt = 0.033F;  // 33 ms tick (30 Hz)
inline constexpr float kBaseCaptureTime = 20.0F;  // sec, base
inline constexpr float kBaseRepairRate = 1.0F;  // per sec
inline constexpr float kEngineerBoost = 2.5F;    // 2-3x per engineer-capabilities

inline constexpr std::array<std::array<std::uint8_t, 3>, 3> kMaterialsPerOp{{
  {{2, 1, 0}},  // capture (basic + construction)
  {{1, 1, 0}},  // repair (basic + construction)
  {{0, 0, 1}},  // post-capture convert (explosive)
}};

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

// ============ Strategy A: Instant capture, no repair, permanent 50% penalty ============
// Worst-case gameplay: capture instant, no repair, faction-adaptation stuck at 0.5.
inline std::uint64_t strategy_a(std::vector<CaptureOp>& ops, std::uint64_t tick_count,
                                Rng& /*rng*/) {
  std::uint64_t accum = 0;
  for (auto& op : ops) {
    op.state = State::Operational;  // instant capture
    op.effectiveness = 0.5F;  // permanent penalty
    op.progress = 1.0F;
    op.repair_progress = 0.0F;
    accum += static_cast<std::uint64_t>(tick_count);
  }
  return accum;
}

// ============ Strategy B: Capture timer + default repair ============
// Real Foxhole-style: 10-30 sec capture timer + 1x repair speed.
inline std::uint64_t strategy_b(std::vector<CaptureOp>& ops, std::uint64_t tick_count,
                                Rng& /*rng*/) {
  std::uint64_t accum = 0;
  for (auto& op : ops) {
    if (op.state == State::Neutral) {
      op.state = State::Capturing;
      op.progress = 0.0F;
    }
    if (op.state == State::Capturing) {
      op.progress += kDt / kBaseCaptureTime;
      if (op.progress >= 1.0F) {
        op.state = State::Captured;
        op.progress = 1.0F;
        op.repair_progress = 0.0F;
      }
      accum += static_cast<std::uint64_t>(tick_count);
    }
    if (op.state == State::Captured) {
      op.state = State::Repairing;
    }
    if (op.state == State::Repairing) {
      op.repair_progress += kDt * kBaseRepairRate;
      if (op.repair_progress >= 1.0F) {
        op.state = State::Operational;
        op.repair_progress = 1.0F;
      }
      // Faction-adaptation evolution: 0.5 → 1.0 over time.
      op.effectiveness = 0.5F + 0.5F * (1.0F - std::exp(-op.repair_progress * op.tau_adapt / 30.0F));
      accum += static_cast<std::uint64_t>(tick_count);
    }
  }
  return accum;
}

// ============ Strategy C: Capture timer + engineer-boosted repair (2.5x) ============
inline std::uint64_t strategy_c(std::vector<CaptureOp>& ops, std::uint64_t tick_count,
                                Rng& rng) {
  std::uint64_t accum = 0;
  for (auto& op : ops) {
    // Determine if engineer is available (proxy: 50% chance for benchmark).
    const bool has_engineer = (rng.uint32(0, 1) == 1);

    if (op.state == State::Neutral) {
      op.state = State::Capturing;
      op.progress = 0.0F;
    }
    if (op.state == State::Capturing) {
      op.progress += kDt / kBaseCaptureTime;
      if (op.progress >= 1.0F) {
        op.state = State::Captured;
        op.progress = 1.0F;
        op.repair_progress = 0.0F;
      }
      accum += static_cast<std::uint64_t>(tick_count);
    }
    if (op.state == State::Captured) {
      op.state = State::Repairing;
    }
    if (op.state == State::Repairing) {
      const float boost = has_engineer ? kEngineerBoost : 1.0F;
      op.repair_progress += kDt * kBaseRepairRate * boost;
      if (op.repair_progress >= 1.0F) {
        op.state = State::Operational;
        op.repair_progress = 1.0F;
      }
      op.effectiveness = 0.5F + 0.5F * (1.0F - std::exp(-op.repair_progress * op.tau_adapt / 30.0F));
      accum += static_cast<std::uint64_t>(tick_count * (has_engineer ? 3 : 1));
    }
  }
  return accum;
}

// ============ Strategy D: Capture timer + fast repair gated on materials ============
inline std::uint64_t strategy_d(std::vector<CaptureOp>& ops, std::uint64_t tick_count,
                                Rng& rng) {
  std::uint64_t accum = 0;
  for (auto& op : ops) {
    const bool has_materials = (op.materials[0] >= 2 && op.materials[1] >= 1);

    if (op.state == State::Neutral) {
      op.state = State::Capturing;
      op.progress = 0.0F;
    }
    if (op.state == State::Capturing) {
      op.progress += kDt / kBaseCaptureTime;
      if (op.progress >= 1.0F) {
        op.state = State::Captured;
        op.progress = 1.0F;
      }
      accum += static_cast<std::uint64_t>(tick_count);
    }
    if (op.state == State::Captured) {
      op.state = State::Repairing;
    }
    if (op.state == State::Repairing) {
      const float rate = has_materials ? 3.0F : 0.5F;  // fast vs throttled
      op.repair_progress += kDt * kBaseRepairRate * rate;
      if (has_materials) {
        op.materials[0] = op.materials[0] > 1 ? op.materials[0] - 2 : 0;
        op.materials[1] = op.materials[1] > 0 ? op.materials[1] - 1 : 0;
      }
      if (op.repair_progress >= 1.0F) {
        op.state = State::Operational;
      }
      accum += static_cast<std::uint64_t>(tick_count * (has_materials ? 2 : 1));
    }
    (void)rng;
  }
  return accum;
}

// ============ Strategy E: Instant capture + permanent 50% penalty ============
inline std::uint64_t strategy_e(std::vector<CaptureOp>& ops, std::uint64_t tick_count,
                                Rng& /*rng*/) {
  std::uint64_t accum = 0;
  for (auto& op : ops) {
    op.state = State::Operational;
    op.effectiveness = 0.5F;  // permanent
    op.progress = 1.0F;
    op.repair_progress = 0.0F;
    accum += static_cast<std::uint64_t>(tick_count);
  }
  return accum;
}

struct Scene {
  std::string_view name;
  std::uint32_t n_captures;
  std::uint32_t spawn_radius;
};

constexpr std::array<Scene, 5> kScenes{{
  {"skirmish_5cap",    5,   50},
  {"battle_20cap",     20,  100},
  {"offensive_50cap",  50,  200},
  {"sustained_100cap", 100, 300},
  {"massive_200cap",   200, 500},
}};

void build_scene(const Scene& s, std::vector<CaptureOp>& ops, Rng& rng) {
  ops.clear();
  ops.reserve(s.n_captures);
  for (std::uint32_t i = 0; i < s.n_captures; ++i) {
    CaptureOp op{};
    op.target_id = i + 1;
    op.pos_x = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    op.pos_y = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    op.pos_z = rng.uniform(-static_cast<float>(s.spawn_radius), static_cast<float>(s.spawn_radius));
    op.effectiveness = 0.5F;
    op.materials = {5, 3, 0};
    ops.push_back(op);
  }
}

void run(int iters, int warmup, std::uint64_t seed) {
  std::printf("2026-06-22-capture-repair-enemy-equipment — bench\n");
  std::printf("iters=%d warmup=%d seed=%llu\n", iters, warmup,
              static_cast<unsigned long long>(seed));
  std::printf("scene,strategy,n_cap,mean_ns_per_tick,median_ns,p95_ns,p99_ns,std_ns\n");

  for (const auto& scene : kScenes) {
    for (int si = 0; si < 5; ++si) {
      std::vector<std::uint64_t> samples;
      samples.reserve(iters);

      std::vector<CaptureOp> ops;
      ops.reserve(scene.n_captures);

      const std::uint64_t scene_seed = seed ^ (static_cast<std::uint64_t>(scene.n_captures) * 2654435761ULL) ^
                                       (static_cast<std::uint64_t>(si) << 5);
      Rng rng(scene_seed);
      build_scene(scene, ops, rng);

      // Warmup.
      for (int w = 0; w < warmup; ++w) {
        Rng wrng(scene_seed + 1000ULL + static_cast<std::uint64_t>(w));
        std::vector<CaptureOp> wo = ops;
        switch (si) {
          case 0: strategy_a(wo, 1, wrng); break;
          case 1: strategy_b(wo, 1, wrng); break;
          case 2: strategy_c(wo, 1, wrng); break;
          case 3: strategy_d(wo, 1, wrng); break;
          case 4: strategy_e(wo, 1, wrng); break;
        }
        g_sink ^= samples.size();
      }

      for (int it = 0; it < iters; ++it) {
        Rng mrng(scene_seed + 2000ULL + static_cast<std::uint64_t>(it));
        std::vector<CaptureOp> mo = ops;

        auto t0 = std::chrono::steady_clock::now();
        std::uint64_t r = 0;
        switch (si) {
          case 0: r = strategy_a(mo, 1, mrng); break;
          case 1: r = strategy_b(mo, 1, mrng); break;
          case 2: r = strategy_c(mo, 1, mrng); break;
          case 3: r = strategy_d(mo, 1, mrng); break;
          case 4: r = strategy_e(mo, 1, mrng); break;
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
        case 0: strat_name = "A_InstantCapture_NoRepair"; break;
        case 1: strat_name = "B_CaptureTimer_DefaultRepair"; break;
        case 2: strat_name = "C_CaptureTimer_EngineerRepair"; break;
        case 3: strat_name = "D_CaptureTimer_FastRepair_MaterialDep"; break;
        case 4: strat_name = "E_PermanentPenalty_InstantCapture"; break;
      }

      std::printf("%s,%s,%u,%.1f,%llu,%llu,%llu,%.1f\n", scene.name.data(), strat_name,
                  scene.n_captures, mean,
                  static_cast<unsigned long long>(median),
                  static_cast<unsigned long long>(p95),
                  static_cast<unsigned long long>(p99), stddev);
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
}

}  // namespace cap

int main(int argc, char** argv) {
  int iters = 1000;
  int warmup = 10;
  std::uint64_t seed = 42;
  if (argc > 1) iters = std::atoi(argv[1]);
  if (argc > 2) warmup = std::atoi(argv[2]);
  if (argc > 3) seed = std::strtoull(argv[3], nullptr, 10);
  cap::run(iters, warmup, seed);
  return 0;
}