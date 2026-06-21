#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <span>
#include <vector>

using f64 = double;
static constexpr f64 kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------
struct Vec3 {
    f64 x{}, y{}, z{};
    constexpr Vec3() = default;
    constexpr Vec3(f64 x, f64 y, f64 z) : x{x}, y{y}, z{z} {}
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(f64 s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(f64 s) const { return {x / s, y / s, z / s}; }
    Vec3 &operator+=(Vec3 o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3 &operator-=(Vec3 o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    f64 dot(Vec3 o) const { return x*o.x + y*o.y + z*o.z; }
    f64 len2() const { return dot(*this); }
    f64 len() const { return std::sqrt(len2()); }
    Vec3 norm() const { f64 l = len(); return l>1e-15 ? *this/l : Vec3{}; }
    Vec3 cross(Vec3 o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
};

// ---------------------------------------------------------------------------
// Terrain
// ---------------------------------------------------------------------------
enum class TerrainType : int { Flat, GentleSlope, Rocky, Trench, Cratered, Count };
static constexpr const char *kTerrainNames[] = {
    "Flat", "GentleSlope", "Rocky", "Trench", "Cratered"};

struct Terrain {
    explicit Terrain(TerrainType t, unsigned seed = 42) : type(t), rng(seed) {
        if (type == TerrainType::Rocky || type == TerrainType::Cratered) {
            for (auto &v : perlin) v = std::uniform_real_distribution<f64>{-1,1}(rng);
        }
    }

    TerrainType type;
    std::mt19937_64 rng;
    std::array<f64, 256> perlin{};

    f64 height(f64 x, f64 z) const {
        switch (type) {
        case TerrainType::Flat:        return 0.0;
        case TerrainType::GentleSlope: return 0.15 * x;
        case TerrainType::Rocky:       return rocky(x, z);
        case TerrainType::Trench:      return trench(x, z);
        case TerrainType::Cratered:    return cratered(x, z);
        default:                       return 0.0;
        }
    }

private:
    f64 lerp(f64 a, f64 b, f64 t) const { return a + t*(b-a); }
    f64 fade(f64 t) const { return t*t*t*(t*(t*6 - 15) + 10); }

    f64 grad(int h, f64 dx, f64 dz) const {
        h &= 15;
        f64 u = h<8 ? dx : dz;
        f64 v = h<4 ? dz : (h==12||h==14 ? dx : dz);
        return ((h&1) ? -u : u) + ((h&2) ? -v : v);
    }

    f64 perlin_noise(f64 x, f64 z) const {
        int xi = (int)std::floor(x) & 0xFF;
        int zi = (int)std::floor(z) & 0xFF;
        f64 xf = x - std::floor(x), zf = z - std::floor(z);
        f64 u = fade(xf), v = fade(zf);
        int aa = (int)perlin[xi] + zi, ab = (int)perlin[xi] + zi+1;
        int ba = (int)perlin[xi+1] + zi, bb = (int)perlin[xi+1] + zi+1;
        f64 v1 = lerp(grad((int)perlin[aa], xf, zf), grad((int)perlin[ba], xf-1, zf), u);
        f64 v2 = lerp(grad((int)perlin[ab], xf, zf-1), grad((int)perlin[bb], xf-1, zf-1), u);
        return lerp(v1, v2, v);
    }

    f64 rocky(f64 x, f64 z) const {
        f64 n = 0; f64 amp = 0.5, freq = 0.1;
        for (int i = 0; i < 4; ++i) { n += amp * perlin_noise(x*freq, z*freq); amp *= 0.5; freq *= 2.0; }
        return n * 0.3;
    }

    f64 trench(f64, f64 z) const {
        f64 dz = std::abs(z - 40);
        if (dz < 3) return -1.2;
        if (dz < 5) return -1.2 * (5-dz)/2;
        return 0.0;
    }

    f64 cratered(f64 x, f64 z) const {
        f64 n = 0; f64 amp = 0.3, freq = 0.05;
        for (int i = 0; i < 3; ++i) { n += amp * perlin_noise(x*freq, z*freq); amp *= 0.5; freq *= 2.0; }
        return n * 0.5 - 0.3;
    }
};

// ---------------------------------------------------------------------------
// Ray-cast suspension per wheel
// ---------------------------------------------------------------------------
struct WheelState {
    f64 suspension_length{};
    f64 prev_suspension_length{};
    Vec3 world_pos;
    f64 contact_normal_y{1.0};
    bool in_contact{};
};

struct WheelParams {
    f64 radius = 0.3;
    f64 mass = 30.0;
    f64 rest_length = 0.5;
    f64 stiffness = 60000.0;
    f64 damping = 4500.0;
    f64 max_travel = 0.3;
    Vec3 mount_offset;
};

struct SuspensionResult {
    Vec3 force;
    Vec3 world_pos;
    f64 length;
    bool contact;
};

SuspensionResult compute_suspension(
    const WheelParams &wp, WheelState &ws,
    Vec3 chassis_pos, f64 chassis_yaw, f64 dt,
    const Terrain &terrain)
{
    f64 c = std::cos(chassis_yaw), s = std::sin(chassis_yaw);
    Vec3 mount_world = chassis_pos + Vec3{
        wp.mount_offset.x*c - wp.mount_offset.z*s,
        wp.mount_offset.y,
        wp.mount_offset.x*s + wp.mount_offset.z*c,
    };

    ws.prev_suspension_length = ws.suspension_length;

    f64 terrain_h = terrain.height(mount_world.x, mount_world.z);
    f64 ground_y = terrain_h + wp.radius;
    f64 wheel_center_y = mount_world.y - wp.rest_length;

    if (wheel_center_y <= ground_y) {
        ws.suspension_length = wp.rest_length - (ground_y - wheel_center_y);
        ws.suspension_length = std::clamp(ws.suspension_length, 0.0, wp.rest_length + wp.max_travel);
        ws.in_contact = true;
        ws.contact_normal_y = 1.0;
        ws.world_pos = Vec3{mount_world.x, ground_y - wp.radius, mount_world.z};
    } else {
        ws.suspension_length = wp.rest_length;
        ws.in_contact = false;
        ws.contact_normal_y = 1.0;
        ws.world_pos = Vec3{mount_world.x, wheel_center_y - wp.radius, mount_world.z};
    }

    f64 vel = (ws.suspension_length - ws.prev_suspension_length) / std::max(dt, 1e-10);
    f64 force_mag = wp.stiffness * (wp.rest_length - ws.suspension_length) - wp.damping * vel;
    if (force_mag < 0) force_mag = 0;

    return {Vec3{0, force_mag, 0}, ws.world_pos, ws.suspension_length, ws.in_contact};
}

// ---------------------------------------------------------------------------
// XPBD articulated track chain
// ---------------------------------------------------------------------------
struct TrackLink {
    Vec3 pos;
    Vec3 vel;
    f64 inv_mass;
};

struct TrackAssembly {
    std::vector<TrackLink> links;
    f64 rest_distance;
    f64 compliance;
    f64 damping_coef;
    int num_links;
    f64 total_cost_us{};

    TrackAssembly(int n_links, f64 pitch, f64 compliance_val, f64 damp)
        : links(n_links), rest_distance(pitch), compliance(compliance_val),
          damping_coef(damp), num_links(n_links)
    {
        for (int i = 0; i < n_links; ++i) {
            links[i].pos = Vec3{f64(i) * pitch - f64(n_links)*pitch*0.5, -0.5, 0};
            links[i].vel = Vec3{};
            links[i].inv_mass = 1.0 / 5.0;
        }
    }

    void solve(int iterations, f64 dt) {
        auto t0 = std::chrono::steady_clock::now();
        f64 alpha = compliance / (dt * dt);

        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < num_links - 1; ++i) {
                auto &a = links[i];
                auto &b = links[i + 1];
                Vec3 delta = b.pos - a.pos;
                f64 dist = delta.len();
                if (dist < 1e-10) continue;
                f64 C = dist - rest_distance;
                Vec3 n = delta / dist;
                f64 w_sum = a.inv_mass + b.inv_mass;
                f64 dlambda = -C / (w_sum + alpha);
                a.pos -= n * (a.inv_mass * dlambda);
                b.pos += n * (b.inv_mass * dlambda);
            }
        }

        for (auto &link : links) {
            link.vel = Vec3{}; // XPBD: velocity inferred from position delta
        }

        auto t1 = std::chrono::steady_clock::now();
        total_cost_us += std::chrono::duration<f64, std::micro>(t1 - t0).count();
    }
};

// ---------------------------------------------------------------------------
// Hull tilt from wheel contacts
// ---------------------------------------------------------------------------
struct HullTiltResult {
    f64 pitch_rad;
    f64 roll_rad;
    int contacts;
};

HullTiltResult compute_hull_tilt(std::span<const SuspensionResult> wheel_results) {
    auto t0 = std::chrono::steady_clock::now();
    f64 pitch = 0, roll = 0;
    int n_contact = 0;

    for (auto &w : wheel_results) {
        if (w.contact) ++n_contact;
    }
    if (n_contact >= 2) {
        f64 avg_z = 0, avg_y = 0, avg_x = 0;
        for (auto &w : wheel_results) {
            if (w.contact) {
                avg_x += w.world_pos.x;
                avg_z += w.world_pos.z;
                avg_y += w.world_pos.y;
            }
        }
        avg_x /= n_contact; avg_z /= n_contact; avg_y /= n_contact;

        f64 xx = 0, zz = 0, xz = 0, xy = 0, zy = 0;
        for (auto &w : wheel_results) {
            if (w.contact) {
                f64 dx = w.world_pos.x - avg_x;
                f64 dz = w.world_pos.z - avg_z;
                f64 dy = w.world_pos.y - avg_y;
                xx += dx*dx; zz += dz*dz; xz += dx*dz;
                xy += dx*dy; zy += dz*dy;
            }
        }
        f64 denom = xx*zz - xz*xz;
        if (std::abs(denom) > 1e-10) {
            f64 A = (xy*zz - xz*zy) / denom;
            f64 B = (xx*zy - xy*xz) / denom;
            pitch = std::atan2(B, 1.0);
            roll  = std::atan2(A, 1.0);
        }
    }

    return {pitch, roll, n_contact};
}

// ---------------------------------------------------------------------------
// Complete tracked vehicle
// ---------------------------------------------------------------------------
struct Vehicle {
    Vec3 chassis_pos;
    f64 chassis_yaw{};
    f64 speed{};
    bool track_model_enabled{true};

    static constexpr int kWheelsPerSide = 6;
    static constexpr int kTotalWheels = kWheelsPerSide * 2;
    static constexpr int kTrackLinksPerSide = 24;

    std::array<WheelParams, kTotalWheels> wheel_params;
    std::array<WheelState, kTotalWheels> wheel_states;
    TrackAssembly left_track;
    TrackAssembly right_track;

    // Cost accumulators (µs)
    f64 suspension_cost_us{};
    f64 track_cost_us{};
    f64 tilt_cost_us{};
    int step_count{};

    Vehicle()
        : chassis_pos{0, 1.2, 0},
          left_track(kTrackLinksPerSide, 0.25, 1e-7, 0.1),
          right_track(kTrackLinksPerSide, 0.25, 1e-7, 0.1)
    {
        f64 half_len = 2.5;
        f64 half_wid = 0.9;
        for (int i = 0; i < kWheelsPerSide; ++i) {
            f64 t = f64(i) / (kWheelsPerSide - 1) - 0.5;
            f64 z_pos = t * half_len;

            wheel_params[i].mount_offset = Vec3{-half_wid, 0.4, z_pos};
            wheel_params[i].radius = 0.3;
            wheel_params[i].rest_length = 0.5;
            wheel_params[i].stiffness = 60000.0;
            wheel_params[i].damping = 4500.0;

            int ri = i + kWheelsPerSide;
            wheel_params[ri].mount_offset = Vec3{half_wid, 0.4, z_pos};
            wheel_params[ri].radius = 0.3;
            wheel_params[ri].rest_length = 0.5;
            wheel_params[ri].stiffness = 60000.0;
            wheel_params[ri].damping = 4500.0;
        }
    }

    void step(f64 dt, const Terrain &terrain) {
        // 1. Suspension ray-cast
        auto t0 = std::chrono::steady_clock::now();
        std::array<SuspensionResult, kTotalWheels> results;
        for (int i = 0; i < kTotalWheels; ++i) {
            results[i] = compute_suspension(
                wheel_params[i], wheel_states[i],
                chassis_pos, chassis_yaw, dt, terrain);
        }
        auto t1 = std::chrono::steady_clock::now();
        suspension_cost_us += std::chrono::duration<f64, std::micro>(t1 - t0).count();

        // 2. Hull tilt
        auto t2 = std::chrono::steady_clock::now();
        auto tilt = compute_hull_tilt(results);
        // Apply tilt to chassis (simplified: adjust position)
        f64 total_force_y = 0;
        for (auto &r : results) total_force_y += r.force.y;
        f64 chassis_mass = 8000.0;
        f64 ay = total_force_y / chassis_mass - 9.81;
        chassis_pos.y = std::max(chassis_pos.y + 0.5 * ay * dt * dt, 0.0);
        chassis_pos.x += speed * std::sin(chassis_yaw) * dt;
        chassis_pos.z += speed * std::cos(chassis_yaw) * dt;
        auto t3 = std::chrono::steady_clock::now();
        tilt_cost_us += std::chrono::duration<f64, std::micro>(t3 - t2).count();

        // 3. Track XPBD solve
        auto t4 = std::chrono::steady_clock::now();
        if (track_model_enabled) {
            // Position tracks relative to chassis
            for (int i = 0; i < kTrackLinksPerSide; ++i) {
                f64 t = f64(i) / (kTrackLinksPerSide - 1) - 0.5;
                f64 z_pos = t * 2.8;
                left_track.links[i].pos = chassis_pos + Vec3{-1.0, -0.3, z_pos};
                right_track.links[i].pos = chassis_pos + Vec3{1.0, -0.3, z_pos};
            }
            left_track.solve(8, dt);
            right_track.solve(8, dt);
        }
        auto t5 = std::chrono::steady_clock::now();
        track_cost_us += std::chrono::duration<f64, std::micro>(t5 - t4).count();

        ++step_count;
    }

    void reset() {
        chassis_pos = Vec3{0, 1.2, 0};
        chassis_yaw = 0;
        for (auto &ws : wheel_states) ws = WheelState{};
        left_track = TrackAssembly(kTrackLinksPerSide, 0.25, 1e-7, 0.1);
        right_track = TrackAssembly(kTrackLinksPerSide, 0.25, 1e-7, 0.1);
        suspension_cost_us = 0;
        track_cost_us = 0;
        tilt_cost_us = 0;
        step_count = 0;
    }
};

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
struct Stats {
    f64 mean, stddev, p95, min, max;
};

Stats compute_stats(std::span<const f64> samples) {
    f64 sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    f64 mean = sum / samples.size();
    f64 sq = 0;
    for (auto v : samples) sq += (v - mean) * (v - mean);
    f64 stddev = std::sqrt(sq / samples.size());

    auto sorted = std::vector<f64>(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    f64 min = sorted.front(), max = sorted.back();
    f64 p95 = sorted[(size_t)(sorted.size() * 0.95)];
    return {mean, stddev, p95, min, max};
}

// ---------------------------------------------------------------------------
// Main benchmark
// ---------------------------------------------------------------------------
int main() {
    static constexpr int kWarmup = 100;
    static constexpr int kIter = 1000;
    static constexpr f64 kDt = 1.0 / 60.0;
    static constexpr f64 kSpeeds[] = {2.0, 8.0, 14.0};
    static constexpr const char *kSpeedNames[] = {"Slow(2m/s)", "Medium(8m/s)", "Fast(14m/s)"};

    std::printf("=== Tank Suspension Physics Benchmark ===\n");
    std::printf("Compiler: clang++ %d\n", __clang_major__);
    std::printf("Iterations: %d  Warmup: %d  dt: %.4f\n\n", kIter, kWarmup, kDt);
    std::printf("%-14s %-14s %12s %12s %12s %12s %12s %12s\n",
        "Terrain", "Speed", "Susp(us)", "Track(us)", "Tilt(us)", "Total(us)",
        "Contacts", "HullPitch");
    std::printf("%s\n", std::string(110, '-').c_str());

    for (int ti = 0; ti < (int)TerrainType::Count; ++ti) {
        TerrainType ttype = (TerrainType)ti;
        Terrain terrain(ttype);

        for (int si = 0; si < 3; ++si) {
            f64 speed = kSpeeds[si];
            Vehicle vehicle;
            vehicle.speed = speed;

            // Warmup
            for (int i = 0; i < kWarmup; ++i) {
                vehicle.step(kDt, terrain);
            }

            // Reset accumulators after warmup
            vehicle.suspension_cost_us = 0;
            vehicle.track_cost_us = 0;
            vehicle.tilt_cost_us = 0;
            vehicle.step_count = 0;

            std::vector<f64> total_costs;
            std::vector<f64> contacts_hist;
            std::vector<f64> pitch_hist;

            for (int i = 0; i < kIter; ++i) {
                vehicle.step(kDt, terrain);
                total_costs.push_back(
                    vehicle.suspension_cost_us +
                    vehicle.track_cost_us +
                    vehicle.tilt_cost_us);

                // Per-step cost for this iteration
                f64 step_susp = vehicle.suspension_cost_us / vehicle.step_count;
                f64 step_track = vehicle.track_cost_us / vehicle.step_count;
                f64 step_tilt = vehicle.tilt_cost_us / vehicle.step_count;
                f64 step_total = step_susp + step_track + step_tilt;

                // Count contacts
                int n_contact = 0;
                for (auto &ws : vehicle.wheel_states) if (ws.in_contact) ++n_contact;
                contacts_hist.push_back((f64)n_contact / Vehicle::kTotalWheels);

                // Measure pitch from the tilt result
                std::array<SuspensionResult, Vehicle::kTotalWheels> last_results;
                for (int wi = 0; wi < Vehicle::kTotalWheels; ++wi) {
                    last_results[wi] = compute_suspension(
                        vehicle.wheel_params[wi], vehicle.wheel_states[wi],
                        vehicle.chassis_pos, vehicle.chassis_yaw, kDt, terrain);
                }
                auto tilt = compute_hull_tilt(last_results);
                pitch_hist.push_back(tilt.pitch_rad);
            }

            f64 avg_susp = vehicle.suspension_cost_us / vehicle.step_count;
            f64 avg_track = vehicle.track_cost_us / vehicle.step_count;
            f64 avg_tilt = vehicle.tilt_cost_us / vehicle.step_count;
            f64 avg_total = avg_susp + avg_track + avg_tilt;

            f64 avg_contacts = std::accumulate(contacts_hist.begin(), contacts_hist.end(), 0.0) / contacts_hist.size();
            f64 avg_pitch = std::accumulate(pitch_hist.begin(), pitch_hist.end(), 0.0) / pitch_hist.size();

            std::printf("%-14s %-14s %12.2f %12.2f %12.2f %12.2f %12.2f %+12.4f\n",
                kTerrainNames[ti], kSpeedNames[si],
                avg_susp, avg_track, avg_tilt, avg_total,
                avg_contacts * 100.0, avg_pitch);
        }
        std::printf("\n");
    }

    // Overall summary
    std::printf("\n=== Summary of cost components ===\n");
    std::printf("Ray-cast suspension (12 wheels): target < 0.12 ms (12 x 0.01)\n");
    std::printf("XPBD track chain (2 x 24 links, 8 iters): target < 0.06 ms\n");
    std::printf("Hull tilt (least-squares plane fit): target < 0.02 ms\n");
    std::printf("Total per vehicle: target < 0.20 ms\n");

    return 0;
}
