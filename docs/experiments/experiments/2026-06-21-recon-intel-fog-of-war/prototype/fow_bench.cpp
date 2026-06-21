#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

// ============================================================
// Constants
// ============================================================
constexpr int kMapSize = 512;          // 512x512 world grid
constexpr int kMaxEntities = 2000;
constexpr float kTileSize = 4.0f;      // meters per tile
constexpr float kWorldSize = kMapSize * kTileSize; // 2048 m

// Intel state machine
enum class IntelState : uint8_t {
    Unknown = 0,
    LastKnownDirection,
    StaleArea,
    RecentApprox,
    FreshExact
};

// Detection channels
enum class SensorChannel : uint8_t {
    Visual = 0,
    IR,
    Radar,
    Acoustic,
    SIGINT,
    Count
};
constexpr int kSensorCount = static_cast<int>(SensorChannel::Count);

// Environmental modifiers
struct EnvModifiers {
    float visibility;       // 0.0 (fog/night) to 1.0 (clear day)
    float rain;             // 0.0 to 1.0 (affects radar/visual)
    float ambient_noise;    // 0.0 to 1.0 (affects acoustic)
    float electronic_haze;  // 0.0 to 1.0 (affects radar/SIGINT)
};

// Unit type classification
enum class UnitType : uint8_t {
    Infantry,
    ScoutInfantry,
    VehicleLight,
    VehicleHeavy,
    Tank,
    Aircraft,
    Helicopter,
    Ship,
    Count
};

// Detectability signature per channel (0.0 = invisible, 1.0 = very visible)
struct Signature {
    std::array<float, kSensorCount> channels{}; // visual, IR, radar, acoustic, SIGINT
};

// Unit definition
struct UnitDef {
    UnitType type;
    Signature sig;
    float visual_range;    // max visual detection range (m)
    bool has_radio;
    float radio_range;     // m
    std::string_view name;
};

// Per-entity state
struct Entity {
    float x, y;            // world position
    float vx, vy;          // velocity (affects acoustic + visual)
    UnitType type;
    Signature sig;         // current signature (may be modified by cover/state)
    float visual_range;
    bool alive;
    bool firing;           // firing reveals position
    bool radio_active;
    bool in_cover;         // inside forest/building
    uint32_t id;
};

// Per-observer sensor
struct Sensor {
    SensorChannel channel;
    float range;            // max range (m)
    float resolution;       // quality (0.0-1.0)
    float x, y;             // position
    bool active;
};

// Detection event: observer detected a target at some confidence
struct Detection {
    uint32_t observer_id;
    uint32_t target_id;
    SensorChannel channel;
    float confidence;       // 0.0-1.0
    IntelState intel_state;
    int tick;
};

// Intel record per (observer, target) pair
struct IntelRecord {
    IntelState state;
    int last_seen_tick;
    float last_x, last_y;   // last known exact position
    float approx_x, approx_y; // approximate area center
    float confidence;
};

// ============================================================
// Unit definitions database
// ============================================================
constexpr std::array<UnitDef, static_cast<int>(UnitType::Count)> kUnitDefs = {{
    // Infantry
    { UnitType::Infantry,     { .3f, .4f, .05f, .3f, .0f },  400.f, true,  1000.f, "infantry" },
    // ScoutInfantry (stealthy, good optics)
    { UnitType::ScoutInfantry,{ .2f, .2f, .02f, .1f, .0f },  600.f, true,  2000.f, "scout_infantry" },
    // VehicleLight (jeep, technical)
    { UnitType::VehicleLight, { .5f, .6f, .3f,  .6f, .3f },  500.f, true,  3000.f, "vehicle_light" },
    // VehicleHeavy (truck, APC)
    { UnitType::VehicleHeavy, { .7f, .8f, .5f,  .7f, .5f },  400.f, true,  3000.f, "vehicle_heavy" },
    // Tank (large, hot engine, loud, radar-reflective)
    { UnitType::Tank,         { .8f, .9f, .8f,  .9f, .6f },  500.f, true,  5000.f, "tank" },
    // Aircraft (fast, hot exhaust, large RCS, loud)
    { UnitType::Aircraft,     { .6f, .9f, .9f,  .9f, .7f },  2000.f, true, 50000.f, "aircraft" },
    // Helicopter (distinct IR, moderate RCS, very loud)
    { UnitType::Helicopter,   { .6f, .8f, .4f,  .95f,.5f },  1500.f, true, 30000.f, "helicopter" },
    // Ship (large, cold(er) IR, large RCS, moderate acoustic)
    { UnitType::Ship,         { .9f, .5f, .9f,  .5f, .7f },  1500.f, true, 20000.f, "ship" },
}};

// Get base signature for a unit type
Signature getBaseSignature(UnitType t) {
    return kUnitDefs[static_cast<int>(t)].sig;
}

float getVisualRange(UnitType t) {
    return kUnitDefs[static_cast<int>(t)].visual_range;
}

// ============================================================
// Scene generation
// ============================================================
struct Scene {
    std::string name;
    std::vector<Entity> entities;
    std::vector<Sensor> sensors;
    EnvModifiers env;
};

// Coverage map: per-grid-cell terrain type
enum class CellType : uint8_t { Open, Forest, Building, Water, Road };
using CoverageMap = std::array<std::array<CellType, kMapSize>, kMapSize>;

// Simple terrain generated from seed
CoverageMap generateTerrain(int seed, Scene& scene) {
    CoverageMap map{};
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 99);

    // Determine terrain type from scene name
    bool is_forest_urban = (scene.name.find("forest") != std::string_view::npos ||
                            scene.name.find("urban") != std::string_view::npos);
    bool is_open = (scene.name.find("open") != std::string_view::npos);
    bool is_night = (scene.name.find("night") != std::string_view::npos);
    bool is_ew = (scene.name.find("ew") != std::string_view::npos);

    for (int y = 0; y < kMapSize; y++) {
        for (int x = 0; x < kMapSize; x++) {
            int r = dist(rng);
            if (is_open) {
                map[y][x] = (r < 5) ? CellType::Forest : CellType::Open;
            } else if (is_forest_urban) {
                if (r < 30) map[y][x] = CellType::Forest;
                else if (r < 40) map[y][x] = CellType::Building;
                else map[y][x] = CellType::Open;
            } else if (is_night) {
                map[y][x] = (r < 15) ? CellType::Forest : CellType::Open;
            } else if (is_ew) {
                map[y][x] = (r < 10) ? CellType::Forest : CellType::Open;
            } else {
                // combined_arms: mix of all
                if (r < 20) map[y][x] = CellType::Forest;
                else if (r < 25) map[y][x] = CellType::Building;
                else if (r < 28) map[y][x] = CellType::Water;
                else map[y][x] = CellType::Open;
            }
        }
    }
    return map;
}

// Generate scene
Scene generateScene(std::string_view name, int seed, int num_entities) {
    Scene scene;
    scene.name = name;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos_dist(0.f, kWorldSize);
    std::uniform_real_distribution<float> vel_dist(-5.f, 5.f);
    std::uniform_int_distribution<int> type_dist(0, static_cast<int>(UnitType::Count) - 1);
    std::uniform_real_distribution<float> prob_dist(0.f, 1.f);

    // Set environmental modifiers based on scene
    if (name == "open_terrain") {
        scene.env = {1.0f, 0.0f, 0.1f, 0.0f};
    } else if (name == "forest_urban") {
        scene.env = {0.7f, 0.2f, 0.3f, 0.1f};
    } else if (name == "night_ambush") {
        scene.env = {0.2f, 0.3f, 0.2f, 0.0f};
    } else if (name == "electronic_warfare") {
        scene.env = {0.9f, 0.0f, 0.1f, 0.8f};
    } else if (name == "combined_arms") {
        scene.env = {0.8f, 0.1f, 0.3f, 0.2f};
    } else {
        scene.env = {1.0f, 0.0f, 0.0f, 0.0f};
    }

    auto terrain = generateTerrain(seed, scene);

    // Generate entities biased by scene type
    auto makeEntity = [&](UnitType ut) -> Entity {
        Entity e;
        e.x = pos_dist(rng);
        e.y = pos_dist(rng);
        e.vx = vel_dist(rng) * 0.5f;
        e.vy = vel_dist(rng) * 0.5f;
        e.type = ut;
        e.sig = getBaseSignature(ut);
        e.visual_range = getVisualRange(ut);
        e.alive = true;
        e.firing = prob_dist(rng) < 0.05f;
        e.radio_active = prob_dist(rng) < 0.8f;
        e.id = 0;

        // Check terrain at position
        int tx = std::clamp(static_cast<int>(e.x / kTileSize), 0, kMapSize - 1);
        int ty = std::clamp(static_cast<int>(e.y / kTileSize), 0, kMapSize - 1);
        e.in_cover = (terrain[ty][tx] == CellType::Forest ||
                      terrain[ty][tx] == CellType::Building);

        // Apply cover modifier to visual and IR
        if (e.in_cover) {
            e.sig.channels[static_cast<int>(SensorChannel::Visual)] *= 0.4f;
            e.sig.channels[static_cast<int>(SensorChannel::IR)] *= 0.6f;
        }

        return e;
    };

    // Bias unit types based on scene
    scene.entities.reserve(num_entities);
    if (name == "open_terrain") {
        for (int i = 0; i < num_entities; i++) {
            UnitType ut;
            float r = prob_dist(rng);
            if (r < 0.30f) ut = UnitType::Tank;
            else if (r < 0.50f) ut = UnitType::VehicleLight;
            else if (r < 0.65f) ut = UnitType::Infantry;
            else if (r < 0.80f) ut = UnitType::Aircraft;
            else ut = UnitType::Helicopter;
            auto e = makeEntity(ut);
            e.id = i;
            scene.entities.push_back(e);
        }
    } else if (name == "forest_urban") {
        for (int i = 0; i < num_entities; i++) {
            UnitType ut;
            float r = prob_dist(rng);
            if (r < 0.35f) ut = UnitType::Infantry;
            else if (r < 0.50f) ut = UnitType::ScoutInfantry;
            else if (r < 0.70f) ut = UnitType::VehicleLight;
            else if (r < 0.85f) ut = UnitType::VehicleHeavy;
            else ut = UnitType::Tank;
            auto e = makeEntity(ut);
            e.id = i;
            scene.entities.push_back(e);
        }
    } else if (name == "night_ambush") {
        for (int i = 0; i < num_entities; i++) {
            UnitType ut;
            float r = prob_dist(rng);
            if (r < 0.40f) ut = UnitType::ScoutInfantry; // stealthy
            else if (r < 0.55f) ut = UnitType::Infantry;
            else if (r < 0.75f) ut = UnitType::VehicleLight;
            else if (r < 0.90f) ut = UnitType::Tank;
            else ut = UnitType::Helicopter;
            auto e = makeEntity(ut);
            e.id = i;
            scene.entities.push_back(e);
        }
    } else if (name == "electronic_warfare") {
        for (int i = 0; i < num_entities; i++) {
            UnitType ut;
            float r = prob_dist(rng);
            if (r < 0.20f) ut = UnitType::Aircraft; // SEAD/EW
            else if (r < 0.35f) ut = UnitType::Helicopter;
            else if (r < 0.55f) ut = UnitType::Tank;
            else if (r < 0.75f) ut = UnitType::VehicleLight;
            else ut = UnitType::Infantry;
            auto e = makeEntity(ut);
            e.id = i;
            scene.entities.push_back(e);
        }
    } else { // combined_arms
        for (int i = 0; i < num_entities; i++) {
            UnitType ut;
            float r = prob_dist(rng);
            if (r < 0.15f) ut = UnitType::Infantry;
            else if (r < 0.25f) ut = UnitType::ScoutInfantry;
            else if (r < 0.40f) ut = UnitType::VehicleLight;
            else if (r < 0.55f) ut = UnitType::VehicleHeavy;
            else if (r < 0.70f) ut = UnitType::Tank;
            else if (r < 0.80f) ut = UnitType::Aircraft;
            else if (r < 0.90f) ut = UnitType::Helicopter;
            else ut = UnitType::Ship;
            auto e = makeEntity(ut);
            e.id = i;
            scene.entities.push_back(e);
        }
    }

    // Generate sensors (observers)
    int num_sensors;
    if (name == "electronic_warfare") {
        num_sensors = 8; // fewer sensors, EW-heavy
    } else if (name == "combined_arms") {
        num_sensors = 20;
    } else {
        num_sensors = 15;
    }

    std::uniform_int_distribution<int> sensor_channel_dist(0, kSensorCount - 1);
    std::uniform_real_distribution<float> sensor_range_dist(200.f, 800.f);

    for (int i = 0; i < num_sensors; i++) {
        Sensor s;
        s.channel = static_cast<SensorChannel>(sensor_channel_dist(rng));
        s.range = sensor_range_dist(rng);
        s.resolution = 0.5f + prob_dist(rng) * 0.5f;
        s.x = pos_dist(rng);
        s.y = pos_dist(rng);
        s.active = true;
        scene.sensors.push_back(s);
    }

    return scene;
}

// ============================================================
// 5 Detection Strategies
// ============================================================

struct StrategyResult {
    std::string name;
    int64_t tick_ns;              // time per tick (ns)
    double detection_rate;         // fraction of possible detections found
    double false_positive_rate;    // fraction of non-existent detections flagged
    double mean_confidence;        // average detection confidence
    double intel_accuracy;         // how accurate intel positions are (m error)
};

// ============================================================
// Strategy A: Simple Distance + LOS (baseline)
// ============================================================
void strategy_A_simple_distance(
    const Scene& scene,
    std::vector<Detection>& detections,
    int tick
) {
    detections.clear();
    detections.reserve(scene.entities.size() * scene.sensors.size());

    for (const auto& sensor : scene.sensors) {
        if (!sensor.active) continue;
        if (sensor.channel != SensorChannel::Visual) continue;

        float range_sq = sensor.range * sensor.range;

        for (const auto& target : scene.entities) {
            if (!target.alive) continue;

            float dx = target.x - sensor.x;
            float dy = target.y - sensor.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= range_sq) {
                float dist = std::sqrt(dist_sq);
                float base_conf = 1.0f - (dist / sensor.range);
                float vis_sig = target.sig.channels[static_cast<int>(SensorChannel::Visual)];
                float confidence = base_conf * vis_sig * scene.env.visibility;

                if (confidence > 0.1f) {
                    Detection d;
                    d.observer_id = reinterpret_cast<uintptr_t>(&sensor) & 0xFFFF;
                    d.target_id = target.id;
                    d.channel = SensorChannel::Visual;
                    d.confidence = std::clamp(confidence, 0.0f, 1.0f);
                    d.intel_state = IntelState::FreshExact;
                    d.tick = tick;
                    detections.push_back(d);
                }
            }
        }
    }
}

// ============================================================
// Strategy B: Signature-Threshold Detection
// Like WARNO: each unit has optics + concealment, detection is probability-based
// ============================================================
void strategy_B_signature_threshold(
    const Scene& scene,
    std::vector<Detection>& detections,
    int tick,
    std::mt19937& rng
) {
    detections.clear();
    detections.reserve(scene.entities.size() * scene.sensors.size());
    std::uniform_real_distribution<float> prob(0.f, 1.f);

    for (const auto& sensor : scene.sensors) {
        if (!sensor.active) continue;

        float range_sq = sensor.range * sensor.range;
        int ch = static_cast<int>(sensor.channel);

        for (const auto& target : scene.entities) {
            if (!target.alive) continue;

            float dx = target.x - sensor.x;
            float dy = target.y - sensor.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= range_sq) {
                float dist = std::sqrt(dist_sq);
                float sig = target.sig.channels[ch];

                // Base detection probability = f(signature, distance, resolution)
                float range_factor = 1.0f - (dist / sensor.range);
                float detection_prob = sig * sensor.resolution * range_factor;

                // Apply environmental modifier
                float env_mod = 1.0f;
                switch (sensor.channel) {
                    case SensorChannel::Visual: env_mod = scene.env.visibility; break;
                    case SensorChannel::Radar:  env_mod = 1.0f - scene.env.electronic_haze * 0.5f; break;
                    case SensorChannel::Acoustic: env_mod = 1.0f - scene.env.ambient_noise * 0.5f; break;
                    case SensorChannel::IR:    env_mod = scene.env.visibility * 0.5f + 0.5f; break;
                    case SensorChannel::SIGINT: env_mod = 1.0f - scene.env.electronic_haze * 0.7f; break;
                    default: break;
                }
                detection_prob *= env_mod;

                // Firing unit is always detected at close range
                if (target.firing && dist < sensor.range * 0.5f) {
                    detection_prob = std::max(detection_prob, 0.8f);
                }

                if (prob(rng) < detection_prob) {
                    float confidence = detection_prob;
                    Detection d;
                    d.observer_id = reinterpret_cast<uintptr_t>(&sensor) & 0xFFFF;
                    d.target_id = target.id;
                    d.channel = sensor.channel;
                    d.confidence = std::clamp(confidence, 0.0f, 1.0f);

                    // Intel state based on confidence
                    if (confidence > 0.7f) d.intel_state = IntelState::FreshExact;
                    else if (confidence > 0.4f) d.intel_state = IntelState::RecentApprox;
                    else d.intel_state = IntelState::StaleArea;

                    d.tick = tick;
                    detections.push_back(d);
                }
            }
        }
    }
}

// ============================================================
// Strategy C: Multi-Channel Sensor Fusion
// Like Command: Modern Operations - combine visual + IR + radar + acoustic
// ============================================================
void strategy_C_multi_channel_fusion(
    const Scene& scene,
    std::vector<Detection>& detections,
    int tick,
    std::mt19937& rng
) {
    detections.clear();
    detections.reserve(scene.entities.size() * scene.sensors.size());
    std::uniform_real_distribution<float> prob(0.f, 1.f);

    // Per-target fused state
    struct FusedTarget {
        uint32_t target_id;
        float best_confidence;
        SensorChannel best_channel;
        bool detected;
    };
    std::vector<FusedTarget> fused;
    fused.reserve(scene.entities.size());

    for (const auto& target : scene.entities) {
        if (!target.alive) continue;

        bool detected = false;
        float best_conf = 0.0f;
        SensorChannel best_ch = SensorChannel::Visual;

        // Check each sensor against this target
        for (const auto& sensor : scene.sensors) {
            if (!sensor.active) continue;

            float dx = target.x - sensor.x;
            float dy = target.y - sensor.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq > sensor.range * sensor.range) continue;

            float dist = std::sqrt(dist_sq);
            int ch = static_cast<int>(sensor.channel);
            float sig = target.sig.channels[ch];

            // Base probability
            float range_factor = std::max(0.0f, 1.0f - dist / sensor.range);
            float detection_prob = sig * sensor.resolution * range_factor * 0.8f;

            // Environmental modifiers per channel
            switch (sensor.channel) {
                case SensorChannel::Visual:
                    detection_prob *= scene.env.visibility;
                    if (target.in_cover) detection_prob *= 0.4f;
                    break;
                case SensorChannel::IR:
                    detection_prob *= (0.5f + scene.env.visibility * 0.5f);
                    break;
                case SensorChannel::Radar:
                    detection_prob *= (1.0f - scene.env.electronic_haze * 0.6f);
                    break;
                case SensorChannel::Acoustic: {
                    detection_prob *= (1.0f - scene.env.ambient_noise * 0.4f);
                    float speed = std::sqrt(target.vx * target.vx + target.vy * target.vy);
                    detection_prob *= (0.5f + speed / 10.0f);
                    break;
                }
                case SensorChannel::SIGINT:
                    detection_prob *= (1.0f - scene.env.electronic_haze * 0.8f);
                    if (!target.radio_active) detection_prob *= 0.1f;
                    break;
                case SensorChannel::Count: break;
            }

            // Firing bonus
            if (target.firing) detection_prob = std::max(detection_prob, detection_prob + 0.3f);

            if (prob(rng) < detection_prob) {
                detected = true;
                float conf = detection_prob * sensor.resolution;
                if (conf > best_conf) {
                    best_conf = conf;
                    best_ch = sensor.channel;
                }
            }
        }

        if (detected) {
            FusedTarget ft;
            ft.target_id = target.id;
            ft.best_confidence = std::clamp(best_conf, 0.0f, 1.0f);
            ft.best_channel = best_ch;
            ft.detected = true;
            fused.push_back(ft);
        }
    }

    // Convert to detection events
    for (const auto& ft : fused) {
        Detection d;
        d.observer_id = 0; // fused - no single observer
        d.target_id = ft.target_id;
        d.channel = ft.best_channel;
        d.confidence = ft.best_confidence;

        if (d.confidence > 0.7f) d.intel_state = IntelState::FreshExact;
        else if (d.confidence > 0.4f) d.intel_state = IntelState::RecentApprox;
        else d.intel_state = IntelState::StaleArea;

        d.tick = tick;
        detections.push_back(d);
    }
}

// ============================================================
// Strategy D: Intel Aging
// Like Foxhole/HoI4: intel decays over time if not refreshed
// ============================================================
void strategy_D_intel_aging(
    const Scene& scene,
    std::vector<Detection>& detections,
    int tick,
    std::mt19937& rng,
    std::vector<IntelRecord>& intel_state
) {
    // First run the signature-threshold detection
    strategy_B_signature_threshold(scene, detections, tick, rng);

    // Track per-target intel with a single global state (simplified)
    // In production this would be per (observer, target)
    static std::vector<IntelRecord> global_intel;
    if (global_intel.size() != scene.entities.size()) {
        global_intel.resize(scene.entities.size());
        for (auto& ir : global_intel) {
            ir.state = IntelState::Unknown;
            ir.last_seen_tick = -1000;
            ir.confidence = 0.0f;
        }
    }

    // Process detections: refresh intel
    for (const auto& det : detections) {
        if (det.target_id < global_intel.size()) {
            auto& ir = global_intel[det.target_id];
            ir.state = det.intel_state;
            ir.last_seen_tick = tick;
            ir.confidence = det.confidence;
            ir.last_x = scene.entities[det.target_id].x;
            ir.last_y = scene.entities[det.target_id].y;
            ir.approx_x = ir.last_x;
            ir.approx_y = ir.last_y;
        }
    }

    // Age all intel records
    for (auto& ir : global_intel) {
        int ticks_ago = tick - ir.last_seen_tick;

        if (ticks_ago <= 0) continue; // just seen

        if (ticks_ago > 120) {
            ir.state = IntelState::Unknown;
            ir.confidence = 0.0f;
        } else if (ticks_ago > 30) {
            ir.state = IntelState::LastKnownDirection;
            ir.confidence = std::max(0.0f, ir.confidence * (1.0f - ticks_ago / 200.0f));
        } else if (ticks_ago > 5) {
            ir.state = IntelState::StaleArea;
            ir.confidence = std::max(0.0f, ir.confidence * (1.0f - ticks_ago / 100.0f));
            // Approx area expands over time
            float drift = ticks_ago * 2.0f;
            ir.approx_x = ir.last_x + drift;
            ir.approx_y = ir.last_y + drift;
        }
        // <5 ticks: still RecentApprox or FreshExact
    }
}

// ============================================================
// Strategy E: Full Fusion + Intel Aging (complete model)
// Combines multi-channel fusion with intel aging and uncertainty
// ============================================================
void strategy_E_full_fusion(
    const Scene& scene,
    std::vector<Detection>& detections,
    int tick,
    std::mt19937& rng,
    std::vector<IntelRecord>&
) {
    // Use multi-channel fusion for detection
    strategy_C_multi_channel_fusion(scene, detections, tick, rng);

    // Initialize global intel tracking
    static std::vector<IntelRecord> global_intel;
    if (global_intel.size() != scene.entities.size()) {
        global_intel.resize(scene.entities.size());
        for (auto& ir : global_intel) {
            ir.state = IntelState::Unknown;
            ir.last_seen_tick = -1000;
            ir.confidence = 0.0f;
        }
    }

    // Refresh from detections
    for (const auto& det : detections) {
        if (det.target_id < global_intel.size()) {
            auto& ir = global_intel[det.target_id];
            ir.state = det.intel_state;
            ir.last_seen_tick = tick;
            ir.confidence = det.confidence;
            ir.last_x = scene.entities[det.target_id].x;
            ir.last_y = scene.entities[det.target_id].y;
            ir.approx_x = ir.last_x;
            ir.approx_y = ir.last_y;
        }
    }

    // Age with uncertainty growth
    for (auto& ir : global_intel) {
        int ticks_ago = tick - ir.last_seen_tick;
        if (ticks_ago <= 0) continue;

        // Multi-stage decay with position uncertainty
        if (ticks_ago > 120) {
            ir.state = IntelState::Unknown;
            ir.confidence = 0.0f;
        } else if (ticks_ago > 30) {
            ir.state = IntelState::LastKnownDirection;
            ir.confidence *= (1.0f - ticks_ago / 200.0f);
        } else if (ticks_ago > 5) {
            ir.state = IntelState::StaleArea;
            ir.confidence *= (1.0f - ticks_ago / 100.0f);
            // Growing uncertainty radius
            float uncertainty = ticks_ago * 1.5f;
            ir.approx_x = ir.last_x + uncertainty;
            ir.approx_y = ir.last_y + uncertainty;
        }
    }
}

// ============================================================
// Oracle: perfect knowledge (all entities visible)
// ============================================================
std::vector<Detection> generateOracle(const Scene& scene, int tick) {
    std::vector<Detection> oracle;
    oracle.reserve(scene.entities.size());
    for (const auto& e : scene.entities) {
        if (!e.alive) continue;
        Detection d;
        d.observer_id = 0;
        d.target_id = e.id;
        d.channel = SensorChannel::Visual;
        d.confidence = 1.0f;
        d.intel_state = IntelState::FreshExact;
        d.tick = tick;
        oracle.push_back(d);
    }
    return oracle;
}

// ============================================================
// Benchmark harness
// ============================================================
struct BenchConfig {
    std::string strategy_name;
    int num_entities;
    int num_seeds;
    int num_iter;
    int warmup_iter;
};

struct Measurement {
    std::string strategy;
    std::string scene;
    int seed;
    int64_t tick_ns;
    double detection_rate;
    double false_positive_rate;
    double mean_confidence;
};

int64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char** argv) {
    // Default config
    int num_entities = 500;
    int num_seeds = 5;
    int num_iter = 1000;
    int warmup_iter = 10;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--entities") == 0 && i + 1 < argc)
            num_entities = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
            num_seeds = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--iter") == 0 && i + 1 < argc)
            num_iter = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc)
            warmup_iter = std::atoi(argv[++i]);
    }

    int seeds[] = {1, 7, 42, 1234, 31337};
    if (num_seeds > 5) num_seeds = 5;

    const char* scene_names[] = {
        "open_terrain",
        "forest_urban",
        "night_ambush",
        "electronic_warfare",
        "combined_arms"
    };
    constexpr int kNumScenes = 5;

    // Print CSV header
    std::printf("strategy,scene,seed,tick_ns,detection_rate,false_positive_rate,mean_confidence\n");

    for (int si = 0; si < kNumScenes; si++) {
        for (int seed_idx = 0; seed_idx < num_seeds; seed_idx++) {
            int seed = seeds[seed_idx];
            Scene scene = generateScene(scene_names[si], seed, num_entities);

            // Pre-compute oracle for this scene
            auto oracle = generateOracle(scene, 0);
            std::vector<Detection> detections;
            std::vector<IntelRecord> intel_state;
            intel_state.resize(scene.entities.size());

            // Per-strategy measurements
            struct Strategy {
                const char* name;
                void (*func)(const Scene&, std::vector<Detection>&, int, std::mt19937&,
                             std::vector<IntelRecord>&);
                Strategy() : name(nullptr), func(nullptr) {}
            };

            // We have 5 strategies. We'll measure each.
            using StrategyFn = void(*)(const Scene&, std::vector<Detection>&, int, std::mt19937&,
                                       std::vector<IntelRecord>&);

            StrategyFn strategies[] = {
                // A: Simple distance
                [](const Scene& s, std::vector<Detection>& d, int t, std::mt19937&, std::vector<IntelRecord>&) {
                    strategy_A_simple_distance(s, d, t);
                },
                // B: Signature threshold
                [](const Scene& s, std::vector<Detection>& d, int t, std::mt19937& rng, std::vector<IntelRecord>&) {
                    strategy_B_signature_threshold(s, d, t, rng);
                },
                // C: Multi-channel fusion
                [](const Scene& s, std::vector<Detection>& d, int t, std::mt19937& rng, std::vector<IntelRecord>&) {
                    strategy_C_multi_channel_fusion(s, d, t, rng);
                },
                // D: Intel aging
                [](const Scene& s, std::vector<Detection>& d, int t, std::mt19937& rng, std::vector<IntelRecord>& intel) {
                    strategy_D_intel_aging(s, d, t, rng, intel);
                },
                // E: Full fusion
                [](const Scene& s, std::vector<Detection>& d, int t, std::mt19937& rng, std::vector<IntelRecord>& intel) {
                    strategy_E_full_fusion(s, d, t, rng, intel);
                }
            };

            const char* strat_names[] = {
                "A_SimpleDistanceLOS",
                "B_SignatureThreshold",
                "C_MultiChannelFusion",
                "D_IntelAging",
                "E_FullFusionIntelAging"
            };
            constexpr int kNumStrategies = 5;

            for (int strat = 0; strat < kNumStrategies; strat++) {
                std::mt19937 rng(seed + strat * 9999);

                // Warmup
                for (int w = 0; w < warmup_iter; w++) {
                    strategies[strat](scene, detections, w, rng, intel_state);
                }

                int64_t total_ns = 0;
                double total_detection_rate = 0.0;
                double total_false_positive = 0.0;
                double total_conf = 0.0;

                for (int iter = 0; iter < num_iter; iter++) {
                    int64_t t0 = now_ns();
                    strategies[strat](scene, detections, iter, rng, intel_state);
                    int64_t t1 = now_ns();
                    total_ns += (t1 - t0);

                    // Compute metrics against oracle
                    int correct = 0;
                    int false_pos = 0;
                    int oracle_count = static_cast<int>(oracle.size());
                    int det_count = static_cast<int>(detections.size());

                    double conf_sum = 0.0;

                    for (const auto& det : detections) {
                        conf_sum += det.confidence;
                        bool in_oracle = false;
                        for (const auto& o : oracle) {
                            if (o.target_id == det.target_id) {
                                in_oracle = true;
                                break;
                            }
                        }
                        if (in_oracle) correct++;
                        else false_pos++;
                    }

                    double dr = oracle_count > 0 ? static_cast<double>(correct) / oracle_count : 0.0;
                    double fp = det_count > 0 ? static_cast<double>(false_pos) / det_count : 0.0;
                    double mc = det_count > 0 ? conf_sum / det_count : 0.0;

                    total_detection_rate += dr;
                    total_false_positive += fp;
                    total_conf += mc;
                }

                double mean_ns = static_cast<double>(total_ns) / num_iter;
                double mean_dr = total_detection_rate / num_iter;
                double mean_fp = total_false_positive / num_iter;
                double mean_conf = total_conf / num_iter;

                std::printf("%s,%s,%d,%.0f,%.6f,%.6f,%.6f\n",
                    strat_names[strat],
                    scene_names[si],
                    seed,
                    mean_ns,
                    mean_dr,
                    mean_fp,
                    mean_conf);
            }
        }
    }

    return 0;
}
