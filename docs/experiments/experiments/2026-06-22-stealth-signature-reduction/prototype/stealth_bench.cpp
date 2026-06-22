#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <random>
#include <fstream>
#include <string>
#include <algorithm>
#include <memory>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Simple 3D Vector structure
struct Vec3 {
    double x, y, z;

    constexpr Vec3() : x(0.0), y(0.0), z(0.0) {}
    constexpr Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    constexpr Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    constexpr Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    constexpr Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    constexpr Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }

    constexpr double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }

    double lengthSq() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        double len = length();
        if (len < 1e-9) return Vec3(0, 0, 0);
        return *this / len;
    }
};

enum class StealthStrategy {
    BaseSignature,
    RcsCoating,
    IrSuppression,
    AcousticQuieting,
    FullLowObservable
};

enum class EnvironmentType {
    ClearSkyDay,
    RainStorm,
    SeaClutter,
    LowAltitudeGroundEffect,
    ActiveJammedEnvironment
};

std::string toString(StealthStrategy strategy) {
    switch (strategy) {
        case StealthStrategy::BaseSignature: return "BaseSignature";
        case StealthStrategy::RcsCoating: return "RcsCoating";
        case StealthStrategy::IrSuppression: return "IrSuppression";
        case StealthStrategy::AcousticQuieting: return "AcousticQuieting";
        case StealthStrategy::FullLowObservable: return "FullLowObservable";
    }
    return "Unknown";
}

std::string toString(EnvironmentType env) {
    switch (env) {
        case EnvironmentType::ClearSkyDay: return "ClearSkyDay";
        case EnvironmentType::RainStorm: return "RainStorm";
        case EnvironmentType::SeaClutter: return "SeaClutter";
        case EnvironmentType::LowAltitudeGroundEffect: return "LowAltitudeGroundEffect";
        case EnvironmentType::ActiveJammedEnvironment: return "ActiveJammedEnvironment";
    }
    return "Unknown";
}

// 2D polar mapping class for aspect-dependent lookup
class AspectMap {
private:
    static constexpr int ELEVATION_STEPS = 36; // 5 deg steps (-90 to 90)
    static constexpr int AZIMUTH_STEPS = 72;   // 5 deg steps (-180 to 180)
    std::vector<double> rcs_grid;
    std::vector<double> ir_grid;

public:
    AspectMap() : rcs_grid(ELEVATION_STEPS * AZIMUTH_STEPS, 1.0),
                  ir_grid(ELEVATION_STEPS * AZIMUTH_STEPS, 1.0) {
        populateGrid();
    }

    // Populate aspect tables with typical physical profiles
    void populateGrid() {
        for (int e = 0; e < ELEVATION_STEPS; ++e) {
            double elevation_deg = -90.0 + e * 5.0;
            for (int a = 0; a < AZIMUTH_STEPS; ++a) {
                double azimuth_deg = -180.0 + a * 5.0;

                int idx = e * AZIMUTH_STEPS + a;

                // RCS model: lowest at front, highest at broadside
                double az_rad = azimuth_deg * M_PI / 180.0;
                double el_rad = elevation_deg * M_PI / 180.0;

                // Nose aspect (azimuth close to 0)
                double front_factor = 0.1 + 0.9 * (1.0 - std::cos(az_rad)); 
                // Broadside aspect (azimuth close to 90 or -90)
                double side_factor = 1.0 + 1.5 * std::pow(std::sin(az_rad), 4);
                // Elevation masking
                double el_factor = std::cos(el_rad);

                rcs_grid[idx] = front_factor * side_factor * el_factor;

                // IR model: highest at rear exhaust (azimuth close to 180 or -180)
                double rear_factor = 0.2 + 2.8 * (0.5 + 0.5 * std::cos(az_rad));
                ir_grid[idx] = rear_factor * el_factor;
            }
        }
    }

    // Get interpolated properties based on relative vector from sensor to target
    void getAspectModifiers(const Vec3& target_forward, const Vec3& target_up, const Vec3& relative_sensor_dir, double& rcs_mod, double& ir_mod) const {
        // Build rotation matrix for target local coordinates
        Vec3 f = target_forward.normalized();
        Vec3 u = target_up.normalized();
        Vec3 r = f.cross(u).normalized();
        u = r.cross(f).normalized(); // ensure orthogonal

        // Project sensor direction into target local space
        Vec3 rel_sensor = relative_sensor_dir.normalized();
        double loc_x = rel_sensor.dot(r); // right
        double loc_y = rel_sensor.dot(f); // forward
        double loc_z = rel_sensor.dot(u); // up

        // Convert target local space vector to spherical angles
        double azimuth = std::atan2(loc_x, loc_y); // -pi to pi
        double elevation = std::asin(loc_z);       // -pi/2 to pi/2

        double azimuth_deg = azimuth * 180.0 / M_PI;
        double elevation_deg = elevation * 180.0 / M_PI;

        // Map to grid indices
        int e_idx = std::clamp(static_cast<int>((elevation_deg + 90.0) / 5.0), 0, ELEVATION_STEPS - 1);
        int a_idx = std::clamp(static_cast<int>((azimuth_deg + 180.0) / 5.0), 0, AZIMUTH_STEPS - 1);

        int idx = e_idx * AZIMUTH_STEPS + a_idx;
        rcs_mod = rcs_grid[idx];
        ir_mod = ir_grid[idx];
    }
};

// Global aspect map instance
const AspectMap g_aspectMap;

struct TargetEntity {
    Vec3 pos;
    Vec3 velocity;
    Vec3 forward;
    Vec3 up;
    double throttle = 0.7; // 0.0 to 1.0 (afterburner up to 1.3)
    double engine_power = 22000.0; // HP or thrust equivalents
    double base_rcs = 10.0; // m^2
    double base_ir = 500.0; // W/sr
    double base_acoustic = 120.0; // dB re 1uPa at 1m
};

struct SensorSpecs {
    // Radar specs
    double radar_power = 50000.0; // W
    double radar_gain = 35.0; // dB
    double radar_wavelength = 0.03; // m (X-band)
    double radar_nei = -135.0; // dBW (receiver sensitivity)

    // IR specs
    double ir_nei = 1e-12; // W/cm^2 (minimum detectable irradiance)
    double ir_band_min = 8.0; // um
    double ir_band_max = 12.0; // um

    // Acoustic specs
    double acoustic_nei = 45.0; // dB (detection threshold)
};

// Physics functions
double dbToLinear(double db) {
    return std::pow(10.0, db / 10.0);
}

double linearToDb(double val) {
    if (val < 1e-30) return -300.0;
    return 10.0 * std::log10(val);
}

// Compute detection ranges for a target from a sensor's perspective
void checkDetections(const TargetEntity& target, const Vec3& sensor_pos, const SensorSpecs& specs, StealthStrategy strategy, EnvironmentType env,
                     double& r_detect_radar, double& r_detect_ir, double& r_detect_acoustic, double noise_floor_multiplier = 1.0) {
    
    Vec3 rel_pos = target.pos - sensor_pos;
    double range = rel_pos.length();
    Vec3 sensor_dir = (sensor_pos - target.pos).normalized(); // from target to sensor

    // 1. Get aspect-dependent modifiers
    double aspect_rcs = 1.0;
    double aspect_ir = 1.0;
    g_aspectMap.getAspectModifiers(target.forward, target.up, sensor_dir, aspect_rcs, aspect_ir);

    // Apply strategy reductions
    double current_rcs = target.base_rcs * aspect_rcs;
    double current_ir = target.base_ir * aspect_ir * (0.2 + 0.8 * target.throttle);
    double current_acoustic = target.base_acoustic + 10.0 * std::log10(0.1 + target.throttle) + 15.0 * std::log10(1.0 + target.velocity.length() / 100.0);

    switch (strategy) {
        case StealthStrategy::RcsCoating:
            current_rcs *= dbToLinear(-15.0); // -15 dB RAM coating
            break;
        case StealthStrategy::IrSuppression:
            current_ir *= dbToLinear(-10.0); // -10 dB exhaust cooling
            break;
        case StealthStrategy::AcousticQuieting:
            current_acoustic -= 12.0; // -12 dB quieting
            break;
        case StealthStrategy::FullLowObservable:
            current_rcs *= dbToLinear(-15.0);
            current_ir *= dbToLinear(-10.0);
            current_acoustic -= 12.0;
            break;
        case StealthStrategy::BaseSignature:
            break;
    }

    // 2. Set environment parameters
    double attenuation_ir = 0.2; // dB/km in clear sky
    double radar_clutter_db = -40.0; // dB clutter background
    double acoustic_ambient_noise = specs.acoustic_nei; // dB
    double radar_noise_multiplier = 1.0;

    switch (env) {
        case EnvironmentType::RainStorm:
            attenuation_ir = 2.5; // severe atmospheric IR extinction
            radar_clutter_db = -25.0; // rain echoes raise clutter
            acoustic_ambient_noise = specs.acoustic_nei + 18.0; // heavy rain on sea/ground
            break;
        case EnvironmentType::SeaClutter:
            radar_clutter_db = -20.0; // strong sea return
            break;
        case EnvironmentType::LowAltitudeGroundEffect:
            radar_clutter_db = -15.0; // heavy ground clutter
            radar_noise_multiplier = 2.0; // multipath fluctuations
            break;
        case EnvironmentType::ActiveJammedEnvironment:
            radar_noise_multiplier = 100.0; // active EW barrage noise
            acoustic_ambient_noise = specs.acoustic_nei + 5.0; // crew stress/comms noise
            break;
        case EnvironmentType::ClearSkyDay:
            break;
    }

    // 3. Radar detection calculation
    // Radar range equation solving for max range under SNR threshold
    double P_t = specs.radar_power;
    double G = dbToLinear(specs.radar_gain);
    double lambda = specs.radar_wavelength;
    double min_detectable_power = dbToLinear(specs.radar_nei) * radar_noise_multiplier * noise_floor_multiplier;
    
    // R_max = ((P_t * G^2 * lambda^2 * RCS) / ((4pi)^3 * P_min))^(1/4)
    double numerator_radar = P_t * G * G * lambda * lambda * current_rcs;
    double denominator_radar = std::pow(4.0 * M_PI, 3.0) * min_detectable_power;
    double max_radar_range = std::pow(numerator_radar / denominator_radar, 0.25);

    // Clutter limitation: if target RCS is below the clutter echo floor at range R, detection fails
    // Clutter area increases with range, so clutter RCS increases with range
    double clutter_rcs = range * range * dbToLinear(radar_clutter_db);
    if (current_rcs < clutter_rcs) {
        max_radar_range = std::min(max_radar_range, range * 0.5); // severely cut range
    }
    r_detect_radar = max_radar_range;

    // 4. IRST detection calculation
    // Contrast irradiance E = (I * tau) / R^2, tau = 10^(-att_db/10 * R_km)
    // Solving for R using iterative approximation to account for exponential atmospheric attenuation
    double target_ir_watts = current_ir;
    double ir_nei = specs.ir_nei;

    double max_ir_range = 0.0;
    // Simple iterative solver for: E = (I * 10^(-att * R / 10000)) / R^2 >= NEI
    double low_r = 10.0;
    double high_r = 150000.0;
    for (int iter = 0; iter < 12; ++iter) {
        double mid_r = (low_r + high_r) * 0.5;
        double tau = std::pow(10.0, -(attenuation_ir * (mid_r / 1000.0)) / 10.0);
        double E = (target_ir_watts * tau) / (mid_r * mid_r);
        if (E >= ir_nei) {
            low_r = mid_r;
            max_ir_range = mid_r;
        } else {
            high_r = mid_r;
        }
    }
    r_detect_ir = max_ir_range;

    // 5. Acoustic detection calculation
    // Passive sonar equation: SL - 20log10(R) - alpha*R - NL >= Threshold
    // Simplifying to spherical spreading: 20log10(R) <= SL - NL - Threshold
    double acoustic_threshold = 0.0; // relative threshold above noise
    double sl = current_acoustic;
    double nl = acoustic_ambient_noise;
    double db_available = sl - nl - acoustic_threshold;
    if (db_available > 0.0) {
        r_detect_acoustic = std::pow(10.0, db_available / 20.0);
    } else {
        r_detect_acoustic = 0.0;
    }
}

int main() {
    std::cout << "Starting 2026-06-22-stealth-signature-reduction simulation..." << std::endl;

    // Setup benchmark variables
    SensorSpecs specs;
    std::vector<StealthStrategy> strategies = {
        StealthStrategy::BaseSignature,
        StealthStrategy::RcsCoating,
        StealthStrategy::IrSuppression,
        StealthStrategy::AcousticQuieting,
        StealthStrategy::FullLowObservable
    };

    std::vector<EnvironmentType> environments = {
        EnvironmentType::ClearSkyDay,
        EnvironmentType::RainStorm,
        EnvironmentType::SeaClutter,
        EnvironmentType::LowAltitudeGroundEffect,
        EnvironmentType::ActiveJammedEnvironment
    };

    std::vector<unsigned int> seeds = {1, 7, 42, 1234, 31337};
    int iterations = 1000;

    std::ofstream csv("results.csv");
    csv << "strategy,environment,seed,mean_radar_range,mean_ir_range,mean_acoustic_range,radar_success_pct,ir_success_pct,acoustic_success_pct,mean_cpu_ns\n";

    // Setup static scenario target values
    TargetEntity base_target;
    base_target.pos = Vec3(0, 0, 0);
    base_target.forward = Vec3(0, 1, 0); // flying north
    base_target.up = Vec3(0, 0, 1);
    base_target.velocity = Vec3(0, 250, 0); // 250 m/s

    // Outer loops
    for (auto strategy : strategies) {
        for (auto env : environments) {
            for (auto seed : seeds) {
                std::mt19937 gen(seed);
                std::uniform_real_distribution<double> dist_angle(-M_PI, M_PI);
                std::uniform_real_distribution<double> dist_pitch(-0.2, 0.2);
                std::uniform_real_distribution<double> dist_throttle(0.3, 1.3);
                std::uniform_real_distribution<double> dist_noise(0.8, 1.2);

                double total_radar = 0.0;
                double total_ir = 0.0;
                double total_acoustic = 0.0;

                int radar_success = 0;
                int ir_success = 0;
                int acoustic_success = 0;

                // Warmup
                double d_r, d_i, d_a;
                checkDetections(base_target, Vec3(1000, 1000, 100), specs, strategy, env, d_r, d_i, d_a);

                auto start_time = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < iterations; ++i) {
                    // Randomize target state slightly to model flight variations
                    TargetEntity target = base_target;
                    double yaw = dist_angle(gen);
                    double pitch = dist_pitch(gen);
                    target.forward = Vec3(std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch));
                    target.throttle = dist_throttle(gen);
                    
                    // Randomize sensor position on a hemisphere around target at 10 km default scaling
                    double s_yaw = dist_angle(gen);
                    double s_pitch = std::abs(dist_pitch(gen)) * 3.0; // keep it elevated
                    double s_r = 10000.0; // 10 km baseline reference range
                    Vec3 sensor_pos(s_r * std::sin(s_yaw) * std::cos(s_pitch),
                                    s_r * std::cos(s_yaw) * std::cos(s_pitch),
                                    s_r * std::sin(s_pitch));

                    double radar_noise = dist_noise(gen);

                    double r_radar = 0.0, r_ir = 0.0, r_acoustic = 0.0;
                    checkDetections(target, sensor_pos, specs, strategy, env, r_radar, r_ir, r_acoustic, radar_noise);

                    total_radar += r_radar;
                    total_ir += r_ir;
                    total_acoustic += r_acoustic;

                    // Success criteria: target detected before it enters 1500m close threat zone
                    if (r_radar >= 1500.0) radar_success++;
                    if (r_ir >= 1500.0) ir_success++;
                    if (r_acoustic >= 1500.0) acoustic_success++;
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                double cpu_ns_per_call = static_cast<double>(duration_ns) / iterations;

                double mean_radar = total_radar / iterations;
                double mean_ir = total_ir / iterations;
                double mean_acoustic = total_acoustic / iterations;

                double pct_radar = (static_cast<double>(radar_success) / iterations) * 100.0;
                double pct_ir = (static_cast<double>(ir_success) / iterations) * 100.0;
                double pct_acoustic = (static_cast<double>(acoustic_success) / iterations) * 100.0;

                csv << toString(strategy) << ","
                    << toString(env) << ","
                    << seed << ","
                    << std::fixed << std::setprecision(3)
                    << mean_radar << ","
                    << mean_ir << ","
                    << mean_acoustic << ","
                    << std::setprecision(1)
                    << pct_radar << ","
                    << pct_ir << ","
                    << pct_acoustic << ","
                    << std::setprecision(2)
                    << cpu_ns_per_call << "\n";
            }
        }
    }

    csv.close();
    std::cout << "Simulation successfully completed. Results written to results.csv" << std::endl;
    return 0;
}
