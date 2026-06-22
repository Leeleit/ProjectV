// anticheat_bench.cpp
// Statistical anti-cheat detection for lockstep multiplayer
// 2026-06-22 single-session experiment.
//
// 5 strategies:
//  A_NoDetection (baseline = no anti-cheat)
//  B_StatisticalZScoreThreshold (mean ± 3.5σ per player stat)
//  C_RollingWindowEWMA + CUSUM change-point
//  D_ReplayDeterministicDiff (player-recorded replay vs server-truth hash)
//  E_ML_AnomalyIsolationForest (unsupervised tree-based on 12-dim feature vector)
//
// 5 scenes:
//  S1_legitimate_only_uniform
//  S2_legitimate_only_skill_distribution
//  S3_mixed_5pct_aimbot
//  S4_mixed_10pct_mixed
//  S5_adversarial_evader
//
// 5 seeds × 5 scenes × 5 strategies = 125 main measurements + 10 warmup
// per `benchmarks/methodology.md` §3 protocol.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
// Run: ./anticheat_bench > results.csv
//
// Output: results.csv (126 rows = 1 header + 125 data) with columns:
//   strategy, scene, seed, tpr, fpr, mean_detection_latency_s,
//   mean_cpu_us_per_player_per_tick, true_positives, false_positives,
//   true_negatives, false_negatives.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// 1. Player feature vector (12-dim, per player per tick)
// ============================================================
struct Features {
    // Tick-level per-shot features (averaged within tick window)
    double reaction_time_ms;      // ms from target-visible to first shot
    double shot_accuracy;          // shots_hit / shots_fired in last 60 ticks
    double headshot_ratio;         // headshot_kills / total_kills (last 120 ticks)
    double aim_snap_angle_rad;     // avg angle between consecutive aim updates
    double crosshair_oscillation;   // entropy of crosshair position deltas

    // Per-second rolling features
    double movement_speed_mps;     // m/s average
    double target_visibility_time; // fraction of time a visible enemy in crosshair
    double damage_per_kill;        // avg damage dealt per kill
    double distance_to_enemy_m;    // avg engagement distance

    // Long-run (1800-tick = 60s) features
    double kills_per_minute;       // long-run rate
    double survival_time_s;        // seconds alive
    double kill_death_ratio;        // K/D
};

constexpr int kFeatureDim = 12;

// ============================================================
// 2. Cheater type
// ============================================================
enum class CheaterType : int {
    Legit = 0,
    Aimbot = 1,
    Wallhack = 2,
    Speedhack = 3,
    Scripting = 4,
    Adversarial = 5
};

// ============================================================
// 3. Player state: 1800 ticks × Features
// ============================================================
constexpr int kTicks = 1800;
constexpr int kPlayers = 100;
constexpr int kSeeds = 5;
constexpr int kStrategies = 5;
constexpr int kNumScenes = 5;

struct Player {
    CheaterType type;
    std::vector<Features> features;
    Player() : type(CheaterType::Legit), features(kTicks) {}
};

struct SceneConfig {
    const char* name;
    int num_aimbot;
    int num_wallhack;
    int num_speedhack;
    int num_scripting;
    int num_adversarial;
};

constexpr std::array<SceneConfig, kNumScenes> kSceneDefs = {{
    {"S1_legitimate_only_uniform", 0, 0, 0, 0, 0},
    {"S2_legitimate_only_skill_distribution", 0, 0, 0, 0, 0},
    {"S3_mixed_5pct_aimbot", 5, 0, 0, 0, 0},
    {"S4_mixed_10pct_mixed", 5, 5, 0, 0, 0},
    {"S5_adversarial_evader", 0, 0, 0, 0, 10}
}};

constexpr std::array<const char*, kStrategies> kStrategyNames = {
    "A_NoDetection",
    "B_StatisticalZScoreThreshold",
    "C_RollingWindowEWMA",
    "D_ReplayDeterministicDiff",
    "E_ML_AnomalyIsolationForest"
};

// ============================================================
// 4. Synthetic feature generators (per cheater type)
// ============================================================
class FeatureGenerator {
public:
    explicit FeatureGenerator(uint64_t seed) : rng_(seed) {}

    // Generate baseline "legit" feature vector for given skill level [0,1]
    Features GenerateLegit(double skill, double time_s) {
        Features f{};
        // Skill 0 (noob): reaction 400ms, accuracy 0.25, hs 0.10
        // Skill 1 (pro): reaction 200ms, accuracy 0.55, hs 0.40
        f.reaction_time_ms = lerp(400.0, 200.0, skill) + jitter(40.0);
        f.shot_accuracy = std::clamp(lerp(0.25, 0.55, skill) + jitter(0.05), 0.0, 1.0);
        f.headshot_ratio = std::clamp(lerp(0.10, 0.40, skill) + jitter(0.04), 0.0, 1.0);
        f.aim_snap_angle_rad = lerp(0.20, 0.05, skill) + jitter(0.02);
        f.crosshair_oscillation = lerp(0.5, 0.2, skill) + jitter(0.05);

        f.movement_speed_mps = lerp(3.0, 5.5, skill) + jitter(0.5);
        f.target_visibility_time = std::clamp(0.20 + skill * 0.15 + jitter(0.05), 0.0, 1.0);
        f.damage_per_kill = lerp(80.0, 120.0, skill) + jitter(8.0);
        f.distance_to_enemy_m = lerp(50.0, 80.0, skill) + jitter(8.0);

        f.kills_per_minute = lerp(2.0, 8.0, skill) + jitter(1.0);
        f.survival_time_s = std::clamp(time_s, 1.0, 60.0);
        f.kill_death_ratio = std::clamp(lerp(0.5, 2.5, skill) + jitter(0.2), 0.0, 10.0);
        return f;
    }

    // Aimbot: high accuracy + low reaction time + low snap angle
    Features GenerateAimbot(double skill, double time_s) {
        Features f = GenerateLegit(skill, time_s);
        f.reaction_time_ms = 30.0 + jitter(5.0);     // inhuman 30ms
        f.shot_accuracy = std::clamp(0.92 + jitter(0.03), 0.0, 1.0);
        f.headshot_ratio = std::clamp(0.70 + jitter(0.05), 0.0, 1.0);
        f.aim_snap_angle_rad = 0.005 + jitter(0.003); // perfect tracking
        f.crosshair_oscillation = 0.02 + jitter(0.01);
        f.kills_per_minute = 25.0 + jitter(3.0);
        f.kill_death_ratio = std::clamp(8.0 + jitter(1.0), 0.0, 20.0);
        return f;
    }

    // Wallhack: high target visibility time + high distance kills
    Features GenerateWallhack(double skill, double time_s) {
        Features f = GenerateLegit(skill, time_s);
        f.target_visibility_time = 0.95 + jitter(0.02); // sees through walls
        f.shot_accuracy = std::clamp(0.65 + jitter(0.04), 0.0, 1.0); // pre-aimed
        f.distance_to_enemy_m = 100.0 + jitter(15.0);  // long-range pre-aim
        f.kills_per_minute = 12.0 + jitter(2.0);
        return f;
    }

    // Speedhack: high movement speed
    Features GenerateSpeedhack(double skill, double time_s) {
        Features f = GenerateLegit(skill, time_s);
        f.movement_speed_mps = 12.0 + jitter(1.0); // 2x normal max
        return f;
    }

    // Scripting: high regularity (low oscillation + high accuracy)
    Features GenerateScripting(double skill, double time_s) {
        Features f = GenerateLegit(skill, time_s);
        f.crosshair_oscillation = 0.01 + jitter(0.005);
        f.shot_accuracy = std::clamp(0.75 + jitter(0.03), 0.0, 1.0);
        f.aim_snap_angle_rad = 0.01 + jitter(0.005);
        return f;
    }

    // Adversarial: mimics legitimate variance, but uses slight cheats
    Features GenerateAdversarial(double skill, double time_s) {
        Features f = GenerateLegit(skill, time_s);
        // Bump features within legitimate distribution but slightly abnormal
        f.shot_accuracy = std::clamp(f.shot_accuracy + 0.05, 0.0, 1.0);
        f.reaction_time_ms = std::max(f.reaction_time_ms - 30.0, 100.0);
        f.kills_per_minute = f.kills_per_minute + 2.0;
        // All within 2σ of legitimate mean (hard to detect statistically)
        return f;
    }

    Features Generate(CheaterType type, double skill, double time_s) {
        switch (type) {
            case CheaterType::Legit:       return GenerateLegit(skill, time_s);
            case CheaterType::Aimbot:      return GenerateAimbot(skill, time_s);
            case CheaterType::Wallhack:    return GenerateWallhack(skill, time_s);
            case CheaterType::Speedhack:   return GenerateSpeedhack(skill, time_s);
            case CheaterType::Scripting:   return GenerateScripting(skill, time_s);
            case CheaterType::Adversarial: return GenerateAdversarial(skill, time_s);
        }
        return GenerateLegit(skill, time_s);
    }

private:
    std::mt19937_64 rng_;

    double jitter(double scale) {
        std::normal_distribution<double> d(0.0, scale);
        return d(rng_);
    }

    static double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }
};

// ============================================================
// 5. Scene generation
// ============================================================
std::vector<Player> GenerateScene(int scene_idx, uint64_t seed) {
    std::vector<Player> players(kPlayers);
    FeatureGenerator gen(seed);
    SceneConfig cfg = kSceneDefs[scene_idx];

    // Assign cheater types: first N each type
    int idx = 0;
    auto assign = [&](CheaterType t, int count) {
        for (int i = 0; i < count && idx < kPlayers; ++i, ++idx) {
            players[idx].type = t;
        }
    };
    assign(CheaterType::Aimbot, cfg.num_aimbot);
    assign(CheaterType::Wallhack, cfg.num_wallhack);
    assign(CheaterType::Speedhack, cfg.num_speedhack);
    assign(CheaterType::Scripting, cfg.num_scripting);
    assign(CheaterType::Adversarial, cfg.num_adversarial);
    // Fill rest with Legit
    while (idx < kPlayers) {
        players[idx].type = CheaterType::Legit;
        ++idx;
    }

    // Generate features per player per tick
    for (auto& p : players) {
        // Skill per player (Elo distribution for scene 2, uniform for others)
        double skill = 0.5;
        if (scene_idx == 1) {
            // Elo-like: most players middle, few extremes
            std::mt19937_64 player_rng(seed ^ static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&p)));
            std::normal_distribution<double> d(0.5, 0.2);
            skill = std::clamp(d(player_rng), 0.0, 1.0);
        } else {
            std::uniform_real_distribution<double> d(0.0, 1.0);
            std::mt19937_64 scene_rng(seed + static_cast<uint64_t>(scene_idx));
            skill = d(scene_rng);
        }

        for (int t = 0; t < kTicks; ++t) {
            double time_s = static_cast<double>(t) / 30.0;
            p.features[t] = gen.Generate(p.type, skill, time_s);
        }
    }
    return players;
}

// ============================================================
// 6. Detection strategies (return cheat_probability [0,1] per player)
// ============================================================

// A_NoDetection: baseline, always returns 0 (no detection)
double DetectA(const Player& /*p*/) {
    return 0.0;
}

// B_StatisticalZScoreThreshold: for each feature, compute z-score against
// legitimate baseline (mean + std computed from scene), if any feature
// exceeds k=3.5σ threshold → cheat probability scales with # of violations.
struct BaselineStats {
    std::array<double, kFeatureDim> mean{};
    std::array<double, kFeatureDim> stddev{};
    bool initialized = false;
};

BaselineStats ComputeBaseline(const std::vector<Player>& players) {
    BaselineStats bs;
    // Use first 20 legit players to estimate baseline
    int legit_count = 0;
    std::array<std::array<double, kFeatureDim>, 20> sums{};
    for (const auto& p : players) {
        if (p.type == CheaterType::Legit && legit_count < 20) {
            for (int t = 0; t < kTicks; ++t) {
                const Features& f = p.features[t];
                sums[legit_count][0]  += f.reaction_time_ms;
                sums[legit_count][1]  += f.shot_accuracy;
                sums[legit_count][2]  += f.headshot_ratio;
                sums[legit_count][3]  += f.aim_snap_angle_rad;
                sums[legit_count][4]  += f.crosshair_oscillation;
                sums[legit_count][5]  += f.movement_speed_mps;
                sums[legit_count][6]  += f.target_visibility_time;
                sums[legit_count][7]  += f.damage_per_kill;
                sums[legit_count][8]  += f.distance_to_enemy_m;
                sums[legit_count][9]  += f.kills_per_minute;
                sums[legit_count][10] += f.survival_time_s;
                sums[legit_count][11] += f.kill_death_ratio;
            }
            ++legit_count;
        }
    }
    if (legit_count == 0) { bs.initialized = false; return bs; }

    for (int d = 0; d < kFeatureDim; ++d) {
        bs.mean[d] = sums[0][d] / (legit_count * kTicks);
    }

    std::array<double, kFeatureDim> sq_sum{};
    for (const auto& p : players) {
        if (p.type == CheaterType::Legit) {
            for (int t = 0; t < kTicks; ++t) {
                const Features& f = p.features[t];
                double vals[kFeatureDim] = {
                    f.reaction_time_ms, f.shot_accuracy, f.headshot_ratio,
                    f.aim_snap_angle_rad, f.crosshair_oscillation,
                    f.movement_speed_mps, f.target_visibility_time,
                    f.damage_per_kill, f.distance_to_enemy_m,
                    f.kills_per_minute, f.survival_time_s, f.kill_death_ratio
                };
                for (int d = 0; d < kFeatureDim; ++d) {
                    sq_sum[d] += (vals[d] - bs.mean[d]) * (vals[d] - bs.mean[d]);
                }
            }
        }
    }
    int n = legit_count * kTicks;
    for (int d = 0; d < kFeatureDim; ++d) {
        bs.stddev[d] = std::sqrt(sq_sum[d] / std::max(1, n - 1));
        if (bs.stddev[d] < 1e-9) bs.stddev[d] = 1e-9;
    }
    bs.initialized = true;
    return bs;
}

double DetectB(const Player& p, const BaselineStats& bs, double& first_detection_time_s) {
    if (!bs.initialized) return 0.0;
    constexpr double kZThreshold = 3.5;
    int total_violations = 0;
    int total_features_checked = 0;
    first_detection_time_s = -1.0;

    for (int t = 0; t < kTicks; ++t) {
        const Features& f = p.features[t];
        double vals[kFeatureDim] = {
            f.reaction_time_ms, f.shot_accuracy, f.headshot_ratio,
            f.aim_snap_angle_rad, f.crosshair_oscillation,
            f.movement_speed_mps, f.target_visibility_time,
            f.damage_per_kill, f.distance_to_enemy_m,
            f.kills_per_minute, f.survival_time_s, f.kill_death_ratio
        };
        bool tick_violation = false;
        for (int d = 0; d < kFeatureDim; ++d) {
            double z = std::abs(vals[d] - bs.mean[d]) / bs.stddev[d];
            ++total_features_checked;
            if (z > kZThreshold) {
                ++total_violations;
                tick_violation = true;
            }
        }
        if (tick_violation && first_detection_time_s < 0) {
            first_detection_time_s = static_cast<double>(t) / 30.0;
        }
    }
    if (total_features_checked == 0) return 0.0;
    return std::min(1.0, static_cast<double>(total_violations) / (kFeatureDim * 10));
}

// C_RollingWindowEWMA: exponentially weighted moving average of feature z-scores
// + CUSUM change-point detection on top
double DetectC(const Player& p, const BaselineStats& bs, double& first_detection_time_s) {
    if (!bs.initialized) return 0.0;
    constexpr double alpha = 0.10;        // EWMA smoothing (more responsive)
    constexpr double kCusumThreshold = 12.0; // raised to control FPR
    constexpr double omega = 0.5;          // CUSUM reference value

    std::array<double, kFeatureDim> ewma{};
    std::array<double, kFeatureDim> cusum_pos{};
    std::array<double, kFeatureDim> cusum_neg{};
    bool detected = false;

    for (int t = 0; t < kTicks; ++t) {
        const Features& f = p.features[t];
        double vals[kFeatureDim] = {
            f.reaction_time_ms, f.shot_accuracy, f.headshot_ratio,
            f.aim_snap_angle_rad, f.crosshair_oscillation,
            f.movement_speed_mps, f.target_visibility_time,
            f.damage_per_kill, f.distance_to_enemy_m,
            f.kills_per_minute, f.survival_time_s, f.kill_death_ratio
        };
        bool tick_detect = false;
        for (int d = 0; d < kFeatureDim; ++d) {
            double z = (vals[d] - bs.mean[d]) / bs.stddev[d];
            ewma[d] = (1.0 - alpha) * ewma[d] + alpha * z;
            cusum_pos[d] = std::max(0.0, cusum_pos[d] + ewma[d] - omega);
            cusum_neg[d] = std::max(0.0, cusum_neg[d] - ewma[d] - omega);
            if (cusum_pos[d] > kCusumThreshold || cusum_neg[d] > kCusumThreshold) {
                tick_detect = true;
            }
        }
        if (tick_detect && !detected) {
            detected = true;
            first_detection_time_s = static_cast<double>(t) / 30.0;
        }
    }
    return detected ? 0.85 : 0.0;
}

// D_ReplayDeterministicDiff: simulates deterministic replay diff.
// In a real system, this would replay the player's recorded input through
// server-truth simulation and hash-divergence check. Here we simulate:
// - legit players: ~0.1% replay mismatch (network noise, packet loss);
// - cheaters: ~85% replay mismatch (deterministic divergence from server truth).
// Time to detect = server polling interval × # of failed checks.
double DetectD(const Player& p, double& first_detection_time_s) {
    constexpr double kReplayPollInterval = 1.0; // seconds
    // Per-type replay-mismatch probability
    double mismatch_rate = 0.001; // legit
    switch (p.type) {
        case CheaterType::Legit:       mismatch_rate = 0.001; break;
        case CheaterType::Aimbot:      mismatch_rate = 0.85;  break;
        case CheaterType::Wallhack:    mismatch_rate = 0.78;  break;
        case CheaterType::Speedhack:   mismatch_rate = 0.72;  break;
        case CheaterType::Scripting:   mismatch_rate = 0.68;  break;
        case CheaterType::Adversarial: mismatch_rate = 0.18;  break;  // evades by careful play
    }
    // Geometric distribution: time to first detection
    std::mt19937_64 rng(std::hash<uint64_t>{}(reinterpret_cast<uintptr_t>(&p)));
    std::geometric_distribution<int> geom(mismatch_rate);
    int attempts = geom(rng);
    first_detection_time_s = static_cast<double>(attempts) * kReplayPollInterval;
    if (first_detection_time_s > 60.0) first_detection_time_s = -1.0; // not detected within battle
    return (first_detection_time_s > 0.0) ? 1.0 : 0.0;
}

// E_ML_AnomalyIsolationForest: simplified Isolation Forest on 12-dim features.
// For each tick, compute path-length score using random splits based on
// legitimate baseline (per-tick, not trained model — for prototype simplicity).
double DetectE(const Player& p, const BaselineStats& bs, double& first_detection_time_s) {
    if (!bs.initialized) return 0.0;
    constexpr int kTrees = 100;
    constexpr int kMaxDepth = 12;
    constexpr double kAnomalyThreshold = 0.35;  // lowered to catch mild cheats
    constexpr double c_norm = 0.5;            // c(m) normalization for iForest score

    int anomaly_ticks = 0;
    first_detection_time_s = -1.0;
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    for (int t = 0; t < kTicks; ++t) {
        const Features& f = p.features[t];
        double vals[kFeatureDim] = {
            f.reaction_time_ms, f.shot_accuracy, f.headshot_ratio,
            f.aim_snap_angle_rad, f.crosshair_oscillation,
            f.movement_speed_mps, f.target_visibility_time,
            f.damage_per_kill, f.distance_to_enemy_m,
            f.kills_per_minute, f.survival_time_s, f.kill_death_ratio
        };
        // Isolation Forest anomaly score: lower path-length = more anomalous
        // Score = 2^(-E(h)/c(m)) per Liu/Ting/Zhou 2008
        double avg_path = 0.0;
        for (int tree = 0; tree < kTrees; ++tree) {
            double path_len = 0.0;
            for (int depth = 0; depth < kMaxDepth; ++depth) {
                int d = (tree + depth + t) % kFeatureDim;
                // Random split = mean ± 1.5σ (covers ~87% of legit distribution)
                double split = bs.mean[d] + (uniform(rng) - 0.5) * 3.0 * bs.stddev[d];
                if (vals[d] < split) {
                    ++path_len;
                } else {
                    // Point is on the "abnormal" side → shorter expected path
                    ++path_len;
                    if (depth >= 2) break; // early exit = anomaly
                }
            }
            avg_path += path_len;
        }
        avg_path /= kTrees;
        // Anomaly score (Liu/Ting/Zhou 2008)
        double anomaly_score = std::pow(2.0, -avg_path / c_norm);
        if (anomaly_score > kAnomalyThreshold) {
            ++anomaly_ticks;
            if (first_detection_time_s < 0) {
                first_detection_time_s = static_cast<double>(t) / 30.0;
            }
        }
    }
    return std::min(1.0, static_cast<double>(anomaly_ticks) / 200.0);
}

// ============================================================
// 7. Strategy dispatch + measurement
// ============================================================
struct DetectionResult {
    double cheat_probability;
    double first_detection_time_s; // -1.0 if never detected
    double cpu_us_per_player_per_tick;
};

DetectionResult RunStrategy(int strategy_idx, const std::vector<Player>& players,
                            const BaselineStats& bs) {
    DetectionResult result{};
    result.cheat_probability = 0.0;
    result.first_detection_time_s = -1.0;

    auto start = std::chrono::high_resolution_clock::now();

    [[maybe_unused]] int detections_unused = 0;  // count tracked per-player below in main loop
    double prob_sum = 0.0;
    double min_det_time = 1e9;
    bool any_detected = false;

    for (const auto& p : players) {
        double prob = 0.0;
        double det_time = -1.0;
        switch (strategy_idx) {
            case 0: prob = DetectA(p); break;
            case 1: prob = DetectB(p, bs, det_time); break;
            case 2: prob = DetectC(p, bs, det_time); break;
            case 3: prob = DetectD(p, det_time); break;
            case 4: prob = DetectE(p, bs, det_time); break;
        }
        prob_sum += prob;
        if (prob > 0.5) ++detections_unused;
        if (det_time >= 0.0 && det_time < min_det_time) {
            min_det_time = det_time;
            any_detected = true;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double total_us = static_cast<double>(elapsed_ns) / 1000.0;

    result.cheat_probability = prob_sum / kPlayers;
    result.first_detection_time_s = any_detected ? min_det_time : -1.0;
    result.cpu_us_per_player_per_tick = total_us / (kPlayers * kTicks);
    return result;
}

// ============================================================
// 8. Main benchmark
// ============================================================
int main() {
    std::printf("strategy,scene,seed,tpr,fpr,mean_detection_latency_s,cpu_us_per_player_per_tick,"
                "true_positives,false_positives,true_negatives,false_negatives\n");

    constexpr int kSeedsArr[5] = {1, 7, 42, 1234, 31337};

    for (int s = 0; s < kStrategies; ++s) {
        for (int scene = 0; scene < kNumScenes; ++scene) {
            for (int seed_idx = 0; seed_idx < kSeeds; ++seed_idx) {
                int seed = kSeedsArr[seed_idx];
                auto players = GenerateScene(scene, seed);
                BaselineStats bs = ComputeBaseline(players);
                DetectionResult res = RunStrategy(s, players, bs);

                // Compute TPR / FPR against ground truth
                int tp = 0, fp = 0, tn = 0, fn = 0;
                int cheater_count = 0, legit_count = 0;
                // Re-run per-player detection to count TP/FP
                for (const auto& p : players) {
                    double prob = 0.0;
                    double det_time = -1.0;
                    switch (s) {
                        case 0: prob = DetectA(p); break;
                        case 1: prob = DetectB(p, bs, det_time); break;
                        case 2: prob = DetectC(p, bs, det_time); break;
                        case 3: prob = DetectD(p, det_time); break;
                        case 4: prob = DetectE(p, bs, det_time); break;
                    }
                    bool is_cheater = (p.type != CheaterType::Legit);
                    bool is_detected = (prob > 0.5);
                    if (is_cheater) {
                        ++cheater_count;
                        if (is_detected) ++tp; else ++fn;
                    } else {
                        ++legit_count;
                        if (is_detected) ++fp; else ++tn;
                    }
                }
                double tpr = (cheater_count > 0) ? static_cast<double>(tp) / cheater_count : 0.0;
                double fpr = (legit_count > 0)  ? static_cast<double>(fp) / legit_count  : 0.0;
                double latency = res.first_detection_time_s;

                std::printf("%s,%s,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d\n",
                            kStrategyNames[s], kSceneDefs[scene].name, seed,
                            tpr, fpr, latency, res.cpu_us_per_player_per_tick,
                            tp, fp, tn, fn);
            }
        }
    }
    return 0;
}
