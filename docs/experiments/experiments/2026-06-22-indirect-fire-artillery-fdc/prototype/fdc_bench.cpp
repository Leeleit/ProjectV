// fdc_bench.cpp — Fire Direction Center (FDC) + Forward Observer (FO) benchmark
// ProjectV docs/experiments/2026-06-22-indirect-fire-artillery-fdc/
// 5 strategies × 5 scenes × 5 seeds × 5 ammo × 1000 iter = 125,000 main measurements
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -fno-fast-math -fno-math-errno

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace fdc {

// Constants
inline constexpr double kG = 9.80665;          // gravity m/s²
inline constexpr double kDangerClose = 200.0;   // US doctrine min safe distance m
inline constexpr double kPI = 3.14159265358979323846;

// Ammo definition (M777 howitzer 155mm reference; charge zones 1-8)
struct Ammo {
    const char* name;
    double v[8];      // muzzle velocity per charge zone (m/s): reduced 257, 297, 357, 463, 538, 633, 762, 827
    double drag;      // G1 drag scaling
    double fuze_s;    // default fuze delay (s)
};
inline constexpr Ammo kAmmo[5] = {
    {"HE_M107",     {257, 297, 357, 463, 538, 633, 762, 827}, 1.0, 0.024},
    {"DPICM_M483A1",{257, 297, 357, 463, 538, 633, 762, 827}, 1.0, 0.024},
    {"WP_M825",     {257, 297, 357, 463, 538, 633, 762, 827}, 1.0, 0.0},
    {"Smoke_M825",  {257, 297, 357, 463, 538, 633, 762, 827}, 1.0, 0.0},
    {"Illum_M485",  {257, 297, 357, 463, 538, 633, 762, 827}, 1.0, 30.0},
};

struct Vec3 {
    double x{0}, y{0}, z{0};
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
};
inline double norm2d(const Vec3& v) { return std::sqrt(v.x*v.x + v.y*v.y); }
inline double dist3d(const Vec3& a, const Vec3& b) {
    double dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}

struct Wind { double speed{0}, bearing_rad{0}; };
struct FireMission {
    Vec3 gun_pos{0,0,100};
    Vec3 target_pos;
    Vec3 friendly_pos;
    int ammo_id{0};
    double corr_lat{0}, corr_rng{0};
    Wind wind;
};

struct FireSolution {
    int charge{-1};
    double elevation_rad{0};
    double bearing_rad{0};
    double time_of_flight{0};
    double impact_dist_to_friendly{0};
    int newton_iters{0};
    bool danger_close{false};
    bool converged{false};
};

// ============ Strategy A: LUT (precomputed at load) ============
class LutTable {
public:
    static constexpr int kR = 100;  // 0..32 km
    static constexpr int kA = 5;    // -200..+200m alt
    struct E { double theta{0}, t_flight{0}, v_horiz{0}; bool ok{false}; };
    E tbl[8][kR][kA];
    LutTable() {
        for (int ch=0; ch<8; ++ch)
            for (int r=0; r<kR; ++r)
                for (int a=0; a<kA; ++a) {
                    double v = kAmmo[0].v[ch];
                    double R = 320.0 * r; // m
                    double alt = 100.0 * (a - 2);
                    double s = R * kG / (v*v);
                    if (s < 0 || s > 1.0) continue;
                    double th = 0.5 * std::asin(s);
                    if (alt != 0) th += std::atan(alt / std::max(R, 1.0));
                    double t = 2.0 * v * std::sin(th) / kG;
                    tbl[ch][r][a] = {th, t, R / t, true};
                }
    }
    FireSolution solve(const FireMission& m) const {
        FireSolution s;
        Vec3 d = m.target_pos - m.gun_pos;
        double R = norm2d(d) + m.corr_rng;
        s.bearing_rad = std::atan2(d.x, d.y) + m.corr_lat / std::max(R, 1.0);
        double alt = m.target_pos.z - m.gun_pos.z;
        int ri = std::clamp((int)(R/320.0), 0, kR-1);
        int ai = std::clamp((int)(alt/100.0)+2, 0, kA-1);
        for (int ch=7; ch>=0; --ch)
            if (tbl[ch][ri][ai].ok) { s.charge = ch; s.elevation_rad = tbl[ch][ri][ai].theta; s.time_of_flight = tbl[ch][ri][ai].t_flight; break; }
        if (s.charge < 0) return s;
        // Wind correction
        double wind_along = m.wind.speed * std::cos(m.wind.bearing_rad - s.bearing_rad);
        s.bearing_rad += wind_along * s.time_of_flight / std::max(R, 1.0);
        s.converged = true;
        // Predict impact, danger-close
        double ix = m.gun_pos.x + R * std::sin(s.bearing_rad);
        double iy = m.gun_pos.y + R * std::cos(s.bearing_rad);
        double iz = m.gun_pos.z + alt;
        s.impact_dist_to_friendly = dist3d({ix,iy,iz}, m.friendly_pos);
        s.danger_close = s.impact_dist_to_friendly < kDangerClose;
        return s;
    }
};

// ============ Strategy B: Newton analytical ============
inline FireSolution solve_newton(const FireMission& m) {
    FireSolution s;
    Vec3 d = m.target_pos - m.gun_pos;
    double R = norm2d(d) + m.corr_rng;
    s.bearing_rad = std::atan2(d.x, d.y) + m.corr_lat / std::max(R, 1.0);
    double alt = m.target_pos.z - m.gun_pos.z;
    double min_err = 1e18;
    int newton_total = 0;
    for (int ch = 7; ch >= 0; --ch) {
        double vc = kAmmo[m.ammo_id].v[ch];
        double sin_arg = R * kG / (vc*vc);
        if (sin_arg > 1.0) continue;  // can't reach
        double th = 0.5 * std::asin(sin_arg);
        for (int it = 0; it < 8; ++it) {
            double s2 = std::sin(2*th);
            double cot = std::cos(th) / std::max(std::sin(th), 1e-9);
            double Rc = vc*vc*s2/kG + alt*cot;
            double err = R - Rc;
            double dRdth = 2.0*vc*vc*std::cos(2*th)/kG - alt/(std::sin(th)*std::sin(th));
            if (std::abs(dRdth) < 1e-10) break;
            th += err / dRdth;
            newton_total++;
            if (std::abs(err) < 0.5) break;
        }
        double Rc = vc*vc*std::sin(2*th)/kG + alt/std::tan(th);
        double e = std::abs(R - Rc);
        if (e < min_err && th > 0.01 && th < kPI/2 - 0.01) {
            min_err = e;
            s.charge = ch;
            s.elevation_rad = th;
            s.time_of_flight = 2.0 * vc * std::sin(th) / kG;
        }
    }
    s.newton_iters = newton_total;
    if (s.charge < 0) return s;
    s.converged = true;
    double wind_along = m.wind.speed * std::cos(m.wind.bearing_rad - s.bearing_rad);
    s.bearing_rad += wind_along * s.time_of_flight / std::max(R, 1.0);
    double ix = m.gun_pos.x + R*std::sin(s.bearing_rad);
    double iy = m.gun_pos.y + R*std::cos(s.bearing_rad);
    double iz = m.gun_pos.z + alt;
    s.impact_dist_to_friendly = dist3d({ix,iy,iz}, m.friendly_pos);
    s.danger_close = s.impact_dist_to_friendly < kDangerClose;
    return s;
}

// ============ Strategy C: PointMass RK4 (3DOF with drag) ============
inline FireSolution solve_pointmass(const FireMission& m) {
    FireSolution s;
    Vec3 d = m.target_pos - m.gun_pos;
    double R = norm2d(d);
    s.bearing_rad = std::atan2(d.x, d.y);
    // Find max charge that can reach
    int best_ch = 7;
    double v = kAmmo[m.ammo_id].v[7];
    for (int ch=7; ch>=0; --ch) {
        if (kAmmo[m.ammo_id].v[ch]*kAmmo[m.ammo_id].v[ch]/kG >= R*1000.0) { best_ch=ch; v=kAmmo[m.ammo_id].v[ch]; break; }
    }
    s.charge = best_ch;
    double sin2 = std::min(1.0, R*1000.0*kG/(v*v));
    double th = 0.5*std::asin(sin2);
    s.elevation_rad = th;
    s.bearing_rad += m.corr_lat / std::max(R, 1.0);
    double vx = v*std::cos(th)*std::sin(s.bearing_rad);
    double vy = v*std::cos(th)*std::cos(s.bearing_rad);
    double vz = v*std::sin(th);
    double x=0, y=0, z=0, t=0, dt=0.02;
    double drag_k = 1e-4 * m.ammo_id + 0.5e-4; // G1-like
    // Wind advection
    double wx = m.wind.speed * std::sin(m.wind.bearing_rad);
    double wy = m.wind.speed * std::cos(m.wind.bearing_rad);
    for (int step=0; step<2000; ++step) {
        double rel_vx = vx - wx, rel_vy = vy - wy;
        double rel_speed = std::sqrt(rel_vx*rel_vx + rel_vy*rel_vy + vz*vz);
        double drag = drag_k * rel_speed;
        double ax = -drag * rel_vx / std::max(rel_speed, 1.0);
        double ay = -drag * rel_vy / std::max(rel_speed, 1.0);
        double az = -kG - drag * vz / std::max(rel_speed, 1.0);
        vx += ax*dt; vy += ay*dt; vz += az*dt;
        x += vx*dt; y += vy*dt; z += vz*dt;
        t += dt;
        if (z < -200) break;
        if (std::hypot(x - R*1000.0*std::sin(s.bearing_rad), y - R*1000.0*std::cos(s.bearing_rad)) < 30) break;
    }
    s.time_of_flight = t;
    s.converged = (s.charge >= 0);
    double ix = m.gun_pos.x + x;
    double iy = m.gun_pos.y + y;
    double iz = m.gun_pos.z + z;
    s.impact_dist_to_friendly = dist3d({ix,iy,iz}, m.friendly_pos);
    s.danger_close = s.impact_dist_to_friendly < kDangerClose;
    s.newton_iters = 0;
    return s;
}

// ============ Strategy D: LUT + Adaptive Wind (per closed wind-simulation-ballistics) ============
inline FireSolution solve_lut_adaptive_wind(const FireMission& m, const LutTable& lut) {
    // Same as A but simulate additional wind query cost (80 µs per closed wind-simulation-ballistics)
    auto sol = lut.solve(m);
    // Simulate wind_query cost via dummy math
    volatile double sink = 0;
    for (int i = 0; i < 1000; ++i) {
        double wx = m.wind.speed * std::sin(m.wind.bearing_rad);
        double wy = m.wind.speed * std::cos(m.wind.bearing_rad);
        sink += wx * wy * 1e-9;
    }
    (void)sink;
    return sol;
}

// ============ Strategy E: Hybrid (A + 1-2 Newton + wind) ============
inline FireSolution solve_hybrid(const FireMission& m, const LutTable& lut) {
    FireSolution s = lut.solve(m);
    if (s.charge < 0) return s;
    double v = kAmmo[m.ammo_id].v[s.charge];
    double R = norm2d(m.target_pos - m.gun_pos) + m.corr_rng;
    double alt = m.target_pos.z - m.gun_pos.z;
    for (int it = 0; it < 2; ++it) {
        double s2 = std::sin(2*s.elevation_rad);
        double cot = std::cos(s.elevation_rad) / std::max(std::sin(s.elevation_rad), 1e-9);
        double Rc = v*v*s2/kG + alt*cot;
        double err = R - Rc;
        double dRdth = 2.0*v*v*std::cos(2*s.elevation_rad)/kG - alt/(std::sin(s.elevation_rad)*std::sin(s.elevation_rad));
        if (std::abs(dRdth) < 1e-10) break;
        s.elevation_rad += err / dRdth;
        s.newton_iters++;
    }
    s.time_of_flight = 2.0 * v * std::sin(s.elevation_rad) / kG;
    // Wind correction
    double wind_along = m.wind.speed * std::cos(m.wind.bearing_rad - s.bearing_rad);
    s.bearing_rad += wind_along * s.time_of_flight / std::max(R, 1.0);
    // Re-compute impact for danger-close
    double ix = m.gun_pos.x + R*std::sin(s.bearing_rad);
    double iy = m.gun_pos.y + R*std::cos(s.bearing_rad);
    double iz = m.gun_pos.z + alt;
    s.impact_dist_to_friendly = dist3d({ix,iy,iz}, m.friendly_pos);
    s.danger_close = s.impact_dist_to_friendly < kDangerClose;
    return s;
}

// ============ Bench harness ============
struct Stats { double mean, med, p95, p99, std, mn, mx; };
Stats stats(std::vector<double>& v) {
    Stats s{};
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (auto x : v) sum += x;
    s.mean = sum / v.size();
    s.med = v[v.size()/2];
    s.p95 = v[v.size()*95/100];
    s.p99 = v[v.size()*99/100];
    s.mn = v.front();
    s.mx = v.back();
    double var = 0;
    for (auto x : v) var += (x-s.mean)*(x-s.mean);
    s.std = std::sqrt(var / v.size());
    return s;
}

struct Scene {
    const char* name;
    std::vector<double> ranges;
    std::vector<Wind> winds;
};

} // namespace fdc

int main() {
    using namespace fdc;
    LutTable lut;

    std::vector<Scene> scenes = {
        {"line_of_sight_clear", {5000, 8000, 10000, 12000, 15000}, {{3,0},{5,0},{7,0},{5,kPI/2},{0,0}}},
        {"urban_with_obstacles",{5000, 7000, 9000, 11000, 12000}, {{5,0},{10,0},{15,0},{5,kPI/2},{8,kPI/2}}},
        {"high_wind", {8000, 12000, 15000, 18000, 20000}, {{15,0},{20,0},{25,0},{18,kPI/2},{22,kPI/2}}},
        {"multi_gun_converge", {6000, 10000, 14000, 16000, 18000}, {{5,0},{10,0},{5,0},{10,kPI/2},{5,kPI/2}}},
        {"long_range_30km", {25000, 28000, 30000, 32000, 30000}, {{10,0},{15,0},{20,0},{5,kPI/2},{10,kPI/2}}},
    };

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> jitter(-50.0, 50.0);
    std::uniform_int_distribution<int> seed_offsets(1, 31337);

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,ammo,n,mean_ns,median_ns,p95_ns,p99_ns,std_ns,min_ns,max_ns,miss_m,charge_converge_pct,danger_close_violations\n";

    constexpr const char* STRAT_NAMES[5] = {"A_LUT", "B_Newton", "C_PointMass", "D_LUT_AdaptiveWind", "E_Hybrid"};

    for (int scene_idx = 0; scene_idx < (int)scenes.size(); ++scene_idx) {
        const auto& scene = scenes[scene_idx];
        for (int seed = 1; seed <= 5; ++seed) {
            std::mt19937 srng(seed * 100 + scene_idx);
            for (int ammo_id = 0; ammo_id < 5; ++ammo_id) {
                std::vector<double> ts[5], miss[5];
                int conv[5] = {}, dc[5] = {};

                for (int iter = 0; iter < 1000; ++iter) {
                    double range_m = scene.ranges[iter % scene.ranges.size()];
                    Wind wind = scene.winds[iter % scene.winds.size()];
                    double bearing = (iter * 0.137 + seed * 0.5) * 0.5;

                    FireMission m;
                    m.gun_pos = {0, 0, 100};
                    m.target_pos = {range_m * std::sin(bearing) + jitter(srng),
                                    range_m * std::cos(bearing) + jitter(srng),
                                    50.0 + (iter % 30)};
                    m.friendly_pos = {range_m * 0.95 * std::sin(bearing + 0.005),
                                      range_m * 0.95 * std::cos(bearing + 0.005),
                                      m.target_pos.z};
                    m.ammo_id = ammo_id;
                    m.wind = wind;

                    // A
                    { auto t0 = std::chrono::high_resolution_clock::now();
                      auto sol = lut.solve(m);
                      auto t1 = std::chrono::high_resolution_clock::now();
                      ts[0].push_back(std::chrono::duration<double, std::nano>(t1-t0).count());
                      if (sol.converged) { conv[0]++; miss[0].push_back(std::abs(sol.impact_dist_to_friendly)); }
                      if (sol.danger_close) dc[0]++; }
                    // B
                    { auto t0 = std::chrono::high_resolution_clock::now();
                      auto sol = solve_newton(m);
                      auto t1 = std::chrono::high_resolution_clock::now();
                      ts[1].push_back(std::chrono::duration<double, std::nano>(t1-t0).count());
                      if (sol.converged) { conv[1]++; miss[1].push_back(std::abs(sol.impact_dist_to_friendly)); }
                      if (sol.danger_close) dc[1]++; }
                    // C
                    { auto t0 = std::chrono::high_resolution_clock::now();
                      auto sol = solve_pointmass(m);
                      auto t1 = std::chrono::high_resolution_clock::now();
                      ts[2].push_back(std::chrono::duration<double, std::nano>(t1-t0).count());
                      if (sol.converged) { conv[2]++; miss[2].push_back(std::abs(sol.impact_dist_to_friendly)); }
                      if (sol.danger_close) dc[2]++; }
                    // D
                    { auto t0 = std::chrono::high_resolution_clock::now();
                      auto sol = solve_lut_adaptive_wind(m, lut);
                      auto t1 = std::chrono::high_resolution_clock::now();
                      ts[3].push_back(std::chrono::duration<double, std::nano>(t1-t0).count());
                      if (sol.converged) { conv[3]++; miss[3].push_back(std::abs(sol.impact_dist_to_friendly)); }
                      if (sol.danger_close) dc[3]++; }
                    // E
                    { auto t0 = std::chrono::high_resolution_clock::now();
                      auto sol = solve_hybrid(m, lut);
                      auto t1 = std::chrono::high_resolution_clock::now();
                      ts[4].push_back(std::chrono::duration<double, std::nano>(t1-t0).count());
                      if (sol.converged) { conv[4]++; miss[4].push_back(std::abs(sol.impact_dist_to_friendly)); }
                      if (sol.danger_close) dc[4]++; }
                }

                for (int s = 0; s < 5; ++s) {
                    auto st = stats(ts[s]);
                    double miss_mean = 0;
                    if (!miss[s].empty()) {
                        for (auto x : miss[s]) miss_mean += x;
                        miss_mean /= miss[s].size();
                    }
                    csv << STRAT_NAMES[s] << "," << scene.name << "," << seed << "," << kAmmo[ammo_id].name << ","
                        << ts[s].size() << ","
                        << st.mean << "," << st.med << "," << st.p95 << "," << st.p99 << ","
                        << st.std << "," << st.mn << "," << st.mx << ","
                        << miss_mean << ","
                        << (100.0 * conv[s] / ts[s].size()) << ","
                        << dc[s] << "\n";
                }
            }
        }
    }
    csv.close();
    std::printf("Done. Output: build/results.csv (125 rows)\n");
    return 0;
}
