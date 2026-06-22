#pragma once
// Scene configurations for ballistic-crack-thump benchmark.
// Per benchmarks/methodology.md: 5 scenes × 5 seeds × 1000 iter + 10 warmup.

#include <cmath>
#include <cstdint>

namespace scenes {

// Reference c_sound @ 20°C = 343 m/s per Wikipedia "Speed of sound".
// Mach 1.0 = 343 m/s. Rifle bullet typical = 700-900 m/s. Artillery = 800-1000 m/s.
inline constexpr double kC_Sound_20C = 343.0;  // m/s

struct Vec3 {
    double x{}, y{}, z{};
};

struct Projectile {
    Vec3 muzzle;     // shooter position
    Vec3 listener;   // listener position
    Vec3 v_dir;      // unit vector, projectile direction
    double v0;       // initial velocity (m/s)
    double mass_g;   // projectile mass (g)
    double caliber_mm;  // caliber (mm)
    double powder_g;    // propellant charge (g)
};

struct Scene {
    const char* name;
    Projectile proj;
};

// Per canonical "crack-thump" pattern:
//   t_thump = |listener - muzzle| / c_sound
//   t_crack = t_projectile_at_listener + sonic_boom_lag ≈ t_projectile_at_listener (10 ms)
//
// "rifle_100m": карабин 7.62x51, listener сбоку на 100 м — classic crack-thump.
// "sniper_500m": снайперская .300 Win Mag на 500 м — большая дистанция, яркий crack-thump.
// "artillery_2km": гаубица M777 на 2 км — массивный thump, слабый crack на слух.
// "aaa_300m": 30 мм зенитка на 300 м — очень быстрый снаряд, чёткий crack.
// "chaotic_50m": хаос — 5 стрелков + 50 снарядов одновременно, stress test.

inline std::array<Scene, 5> kScenes = {{
    {"rifle_100m", {{0, 1.5, 0}, {100, 1.5, 30}, {1, 0, 0.001}, 850, 9.5, 7.62, 3.0}},
    {"sniper_500m", {{0, 1.5, 0}, {400, 1.5, 100}, {1, 0, 0.001}, 900, 12.0, 7.62, 4.5}},
    {"artillery_2km", {{0, 5, 0}, {1500, 5, 200}, {1, 0, 0.005}, 820, 43000, 155, 8200}},
    {"aaa_300m", {{0, 2, 0}, {250, 2, 80}, {1, 0, 0.001}, 1000, 0.45, 30, 50}},
    {"chaotic_50m", {{0, 1.5, 0}, {40, 1.5, 15}, {1, 0, 0.002}, 870, 10.0, 7.62, 3.5}},
}};

}  // namespace scenes
