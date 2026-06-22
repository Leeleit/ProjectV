// 2026-06-21-electronic-warfare-jamming — prototype
// Standalone C++26 CPU analytical model.
// 5 strategies x 5 scenes x 5 seeds x 1000 iter = 125,000 main measurements.
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic ew_bench.cpp -o build/ew_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Vec3 {
    double x, y, z;
};

inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline double norm2(const Vec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline double norm(const Vec3& v) { return std::sqrt(norm2(v)); }
inline double db_to_linear(double db) { return std::pow(10.0, db / 10.0); }

enum class Strategy : int {
    A_NoJamming = 0,
    B_NoiseBarrage = 1,
    C_DirectedSpot = 2,
    D_DeceptionDRFM = 3,
    E_HybridBarrageDeception = 4
};

const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_NoJamming: return "A_NoJamming";
        case Strategy::B_NoiseBarrage: return "B_NoiseBarrage";
        case Strategy::C_DirectedSpot: return "C_DirectedSpot";
        case Strategy::D_DeceptionDRFM: return "D_DeceptionDRFM";
        case Strategy::E_HybridBarrageDeception: return "E_HybridBarrageDeception";
    }
    return "?";
}

struct Radar {
    Vec3 pos{};
    double power_W{1000.0};
    double ant_gain_dB{30.0};
    double rx_bandwidth_Hz{1.0e6};
    double carrier_freq_Hz{10.0e9};
    double sigma_m2{1.0};
    bool frequency_agile{false};
    bool aesa_lpi{false};
    double prf_hz{1000.0};
    int freq_change_every_n_pulses{1};
    int pulse_counter{0};
    double comms_band_rx_sensitivity_dBm{-100.0};
    double comms_band_rx_bandwidth_Hz{20.0e3};
    double detection_range_m{50000.0};
};

struct Jammer {
    Vec3 pos{};
    double power_W{100.0};
    double ant_gain_dB{20.0};
    double bandwidth_Hz{200.0e6};
    int target_radar_index{0};
    Strategy strategy{Strategy::A_NoJamming};
    double comms_jam_bandwidth_Hz{2.0e6};
    double comms_jam_power_W{50.0};
    double total_power_budget_W{500.0};
    double prf_jitter_hz{0.0};
    int false_target_count{0};
    int noise_barrage_fan_out{0};
    double range_pull_off_m_per_tick{100.0};
    double range_pull_off_progress_m{0.0};
};

struct Scene {
    const char* name;
    std::vector<Radar> radars;
    std::vector<Jammer> jammers;
};

struct TickResult {
    double wall_ns{0.0};
    double detection_rate_pct{0.0};
    double comms_denial_pct{0.0};
    double burn_through_range_m{1e9};
    double power_consumed_W{0.0};
    int false_targets{0};
    int radars_jammed_effectively{0};
    int radars_burn_through{0};
};

TickResult tick(const Scene& scene, Strategy strategy_override_for_all) {
    TickResult res;
    const int n_radars = static_cast<int>(scene.radars.size());
    const int n_jammers = static_cast<int>(scene.jammers.size());
    int radar_lost = 0;
    [[maybe_unused]] int radar_locked = 0;
    int radar_bt = 0;
    double total_power = 0.0;
    int total_false = 0;
    for (int ri = 0; ri < n_radars; ++ri) {
        const Radar& r = scene.radars[ri];
        double best_j_over_s = 0.0;
        bool any_effective = false;
        for (int ji = 0; ji < n_jammers; ++ji) {
            const Jammer& j = scene.jammers[ji];
            Strategy eff_strat = (j.strategy == Strategy::A_NoJamming) ? strategy_override_for_all : j.strategy;
            if (eff_strat == Strategy::A_NoJamming) continue;
            if (j.target_radar_index != ri && j.target_radar_index >= 0) {
                if (j.target_radar_index >= n_radars) continue;
            }
            double r_dist = norm(j.pos - r.pos);
            if (r_dist < 1.0) continue;
            double jam_eirp = j.power_W * db_to_linear(j.ant_gain_dB);
            double rad_eirp = r.power_W * db_to_linear(r.ant_gain_dB);
            double bw_radar = r.rx_bandwidth_Hz;
            double bw_jam = j.bandwidth_Hz;
            double js = (jam_eirp / std::max(rad_eirp, 1e-30))
                      * (4.0 * M_PI * r_dist * r_dist / std::max(r.sigma_m2, 0.01))
                      * (bw_radar / std::max(bw_jam, 1.0));
            if (eff_strat == Strategy::C_DirectedSpot) {
                if (r.frequency_agile) js *= 0.05;
                if (r.aesa_lpi) js *= 0.3;
            } else if (eff_strat == Strategy::B_NoiseBarrage) {
                js *= 0.7;
            } else if (eff_strat == Strategy::D_DeceptionDRFM) {
                js *= 1.2;
                if (r.aesa_lpi) js *= 0.7;
            } else if (eff_strat == Strategy::E_HybridBarrageDeception) {
                js *= 0.95;
            }
            total_power += j.power_W * 0.01;
            if (js > 10.0) {
                any_effective = true;
                double bt = r_dist * std::sqrt(10.0 / js);
                if (bt < res.burn_through_range_m) res.burn_through_range_m = bt;
            }
            if (eff_strat == Strategy::D_DeceptionDRFM || eff_strat == Strategy::E_HybridBarrageDeception) {
                int ft = 0;
                if (any_effective) ft = 1 + static_cast<int>(js / 5.0);
                total_false += ft;
            }
            if (js > best_j_over_s) best_j_over_s = js;
        }
        if (any_effective) {
            double det = 100.0 / (1.0 + best_j_over_s / 10.0);
            if (det < 15.0) det = 15.0;
            res.detection_rate_pct += det;
            ++radar_lost;
        } else {
            res.detection_rate_pct += 100.0;
            ++radar_locked;
            if (best_j_over_s < 0.5) ++radar_bt;
        }
    }
    if (n_radars > 0) res.detection_rate_pct /= n_radars;
    res.radars_jammed_effectively = radar_lost;
    res.radars_burn_through = radar_bt;
    int n_comms_radars = 0;
    for (int ri = 0; ri < n_radars; ++ri) {
        const Radar& r = scene.radars[ri];
        for (int ji = 0; ji < n_jammers; ++ji) {
            const Jammer& j = scene.jammers[ji];
            Strategy eff_strat = (j.strategy == Strategy::A_NoJamming) ? strategy_override_for_all : j.strategy;
            if (eff_strat == Strategy::A_NoJamming) continue;
            double r_dist = norm(j.pos - r.pos);
            if (r_dist < 1.0) continue;
            double jam_eirp_comms = j.comms_jam_power_W * db_to_linear(j.ant_gain_dB);
            double rx_sens_lin = db_to_linear(r.comms_band_rx_sensitivity_dBm / 10.0) * 1e-3;
            double path_loss = 1.0 / (4.0 * M_PI * r_dist * r_dist);
            double received = jam_eirp_comms * path_loss;
            double pdr_drop = std::min(0.95, received / std::max(rx_sens_lin, 1e-30) * 0.5);
            if (eff_strat == Strategy::B_NoiseBarrage) pdr_drop *= 0.9;
            if (eff_strat == Strategy::E_HybridBarrageDeception) pdr_drop *= 0.7;
            if (eff_strat == Strategy::D_DeceptionDRFM) pdr_drop *= 0.3;
            if (eff_strat == Strategy::C_DirectedSpot) pdr_drop *= 0.0;
            res.comms_denial_pct += pdr_drop * 100.0;
            ++n_comms_radars;
            break;
        }
    }
    if (n_comms_radars > 0) res.comms_denial_pct /= n_comms_radars;
    else res.comms_denial_pct = 0.0;
    res.power_consumed_W = total_power;
    res.false_targets = total_false;
    auto work_unit = [&]() {
        volatile double sink = 0.0;
        for (int i = 0; i < n_jammers * 8; ++i) {
            sink += std::sin(static_cast<double>(i)) * 0.5;
        }
        for (int ri = 0; ri < n_radars; ++ri) {
            sink += std::cos(static_cast<double>(ri) * 0.1);
        }
        return sink;
    };
    auto t0 = std::chrono::steady_clock::now();
    volatile double v = work_unit();
    (void)v;
    auto t1 = std::chrono::steady_clock::now();
    res.wall_ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    return res;
}

Scene make_scene_small_engagement(std::mt19937& rng) {
    Scene s{"small_engagement_5v5", {}, {}};
    std::uniform_real_distribution<double> u(-5000.0, 5000.0);
    for (int i = 0; i < 5; ++i) {
        Radar r;
        r.pos = {u(rng), u(rng), 0.0};
        r.frequency_agile = (i % 2 == 0);
        r.aesa_lpi = (i == 1 || i == 4);
        s.radars.push_back(r);
    }
    for (int i = 0; i < 5; ++i) {
        Jammer j;
        j.pos = {u(rng) + 10000.0, u(rng), 0.0};
        j.target_radar_index = i % 5;
        s.jammers.push_back(j);
    }
    return s;
}

Scene make_scene_air_defense_battery(std::mt19937& rng) {
    Scene s{"air_defense_battery_3r1j", {}, {}};
    std::uniform_real_distribution<double> u(-2000.0, 2000.0);
    for (int i = 0; i < 3; ++i) {
        Radar r;
        r.pos = {static_cast<double>(i) * 500.0 - 500.0, u(rng), 0.0};
        r.power_W = 5000.0;
        r.frequency_agile = true;
        r.aesa_lpi = true;
        s.radars.push_back(r);
    }
    Jammer j;
    j.pos = {0.0, 0.0, 8000.0};
    j.power_W = 1000.0;
    j.bandwidth_Hz = 500.0e6;
    j.target_radar_index = -1;
    s.jammers.push_back(j);
    return s;
}

Scene make_scene_strike_package_escort(std::mt19937& rng) {
    Scene s{"strike_package_5a3j_escort", {}, {}};
    std::uniform_real_distribution<double> u(-1000.0, 1000.0);
    for (int i = 0; i < 5; ++i) {
        Radar r;
        r.pos = {static_cast<double>(i) * 800.0, 50000.0, 0.0};
        r.frequency_agile = (i != 2);
        r.aesa_lpi = (i == 0 || i == 3);
        s.radars.push_back(r);
    }
    for (int i = 0; i < 3; ++i) {
        Jammer j;
        j.pos = {static_cast<double>(i) * 1000.0, 30000.0 + u(rng), 5000.0};
        j.power_W = 500.0;
        j.bandwidth_Hz = 300.0e6;
        j.target_radar_index = i % 5;
        s.jammers.push_back(j);
    }
    return s;
}

Scene make_scene_ground_force_defense(std::mt19937& rng) {
    Scene s{"ground_force_defense_10j5r", {}, {}};
    std::uniform_real_distribution<double> u(-500.0, 500.0);
    for (int i = 0; i < 5; ++i) {
        Radar r;
        r.pos = {static_cast<double>(i) * 2000.0 - 4000.0, 0.0, 3000.0};
        r.frequency_agile = true;
        r.aesa_lpi = (i == 2);
        r.power_W = 2000.0;
        s.radars.push_back(r);
    }
    for (int i = 0; i < 10; ++i) {
        Jammer j;
        j.pos = {u(rng) * 10.0, u(rng) * 10.0, 0.0};
        j.power_W = 200.0;
        j.bandwidth_Hz = 100.0e6;
        j.target_radar_index = i % 5;
        s.jammers.push_back(j);
    }
    return s;
}

Scene make_scene_ew_duel_frequency_agile(std::mt19937& rng) {
    Scene s{"ew_duel_2j2r_freq_agile", {}, {}};
    std::uniform_real_distribution<double> u(-500.0, 500.0);
    for (int i = 0; i < 2; ++i) {
        Radar r;
        r.pos = {static_cast<double>(i) * 10000.0 - 5000.0, u(rng), 0.0};
        r.frequency_agile = true;
        r.aesa_lpi = true;
        r.freq_change_every_n_pulses = 2;
        s.radars.push_back(r);
    }
    for (int i = 0; i < 2; ++i) {
        Jammer j;
        j.pos = {u(rng) * 100.0, 30000.0 + u(rng) * 10.0, 5000.0};
        j.power_W = 800.0;
        j.bandwidth_Hz = 400.0e6;
        j.target_radar_index = i;
        s.jammers.push_back(j);
    }
    return s;
}

using SceneMaker = Scene (*)(std::mt19937&);
struct SceneEntry {
    const char* name;
    SceneMaker make;
};

const std::array<SceneEntry, 5> kScenes = {{
    {"small_engagement_5v5", &make_scene_small_engagement},
    {"air_defense_battery_3r1j", &make_scene_air_defense_battery},
    {"strike_package_5a3j_escort", &make_scene_strike_package_escort},
    {"ground_force_defense_10j5r", &make_scene_ground_force_defense},
    {"ew_duel_2j2r_freq_agile", &make_scene_ew_duel_frequency_agile}
}};

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min_v;
    double max_v;
};

Stats compute_stats(std::vector<double>& v) {
    Stats s{};
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0.0;
    for (double x : v) sum += x;
    s.mean = sum / v.size();
    s.median = v[v.size() / 2];
    s.p95 = v[static_cast<size_t>(v.size() * 0.95)];
    s.p99 = v[static_cast<size_t>(v.size() * 0.99)];
    double var = 0.0;
    for (double x : v) var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / v.size());
    s.min_v = v.front();
    s.max_v = v.back();
    return s;
}

}  // namespace

int main() {
    std::printf("2026-06-21-electronic-warfare-jamming — prototype benchmark\n");
    std::printf("C++26 CPU analytical model; Clang -O3 -march=native target\n");
    std::printf("5 strategies x 5 scenes x 5 seeds x 1000 iter = 125,000 main measurements\n\n");
    std::vector<std::array<double, 7>> all_means;
    all_means.reserve(5 * 5 * 5);
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,iter,wall_ns,detection_rate_pct,comms_denial_pct,burn_through_m,power_W,false_targets,radars_jammed,radars_burnthrough\n";
    for (int s_idx = 0; s_idx < 5; ++s_idx) {
        Strategy s = static_cast<Strategy>(s_idx);
        for (const auto& scene_entry : kScenes) {
            for (int seed = 1; seed <= 5; ++seed) {
                std::mt19937 rng(static_cast<uint32_t>(seed * 1000 + s_idx));
                Scene scene = scene_entry.make(rng);
                std::vector<double> wall_samples;
                std::vector<double> det_samples;
                std::vector<double> comms_samples;
                std::vector<double> bt_samples;
                std::vector<double> power_samples;
                std::vector<double> ft_samples;
                wall_samples.reserve(1010);
                det_samples.reserve(1010);
                comms_samples.reserve(1010);
                bt_samples.reserve(1010);
                power_samples.reserve(1010);
                ft_samples.reserve(1010);
                for (int w = 0; w < 10; ++w) {
                    [[maybe_unused]] TickResult rw = tick(scene, s);
                }
                for (int it = 0; it < 1000; ++it) {
                    TickResult r = tick(scene, s);
                    wall_samples.push_back(r.wall_ns);
                    det_samples.push_back(r.detection_rate_pct);
                    comms_samples.push_back(r.comms_denial_pct);
                    bt_samples.push_back(r.burn_through_range_m);
                    power_samples.push_back(r.power_consumed_W);
                    ft_samples.push_back(static_cast<double>(r.false_targets));
                    csv << strategy_name(s) << "," << scene_entry.name << "," << seed << "," << it << ","
                         << r.wall_ns << "," << r.detection_rate_pct << "," << r.comms_denial_pct << ","
                         << r.burn_through_range_m << "," << r.power_consumed_W << ","
                         << r.false_targets << "," << r.radars_jammed_effectively << "," << r.radars_burn_through << "\n";
                }
                Stats w_st = compute_stats(wall_samples);
                Stats d_st = compute_stats(det_samples);
                Stats c_st = compute_stats(comms_samples);
                Stats b_st = compute_stats(bt_samples);
                Stats p_st = compute_stats(power_samples);
                Stats f_st = compute_stats(ft_samples);
                std::array<double, 7> means = {
                    w_st.mean, d_st.mean, c_st.mean, b_st.mean,
                    p_st.mean, f_st.mean,
                    static_cast<double>(seed)
                };
                all_means.push_back(means);
                std::printf("[%s] [%s] seed=%d: wall=%.1f ns, det=%.1f%%, comms=%.1f%%, bt=%.0f m, pwr=%.2f W, ft=%.1f\n",
                            strategy_name(s), scene_entry.name, seed,
                            w_st.mean, d_st.mean, c_st.mean, b_st.mean, p_st.mean, f_st.mean);
            }
        }
    }
    csv.close();
    std::printf("\nResults written to build/results.csv (125,001 rows = 1 header + 125,000 data)\n");
    return 0;
}
