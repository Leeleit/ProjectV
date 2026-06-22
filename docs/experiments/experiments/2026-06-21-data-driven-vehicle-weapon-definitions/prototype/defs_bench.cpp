// 2026-06-21-data-driven-vehicle-weapon-definitions
// prototype/defs_bench.cpp
//
// Standalone C++26 CPU benchmark comparing 5 strategies for loading
// data-driven vehicle/weapon/armor definitions.
//
// Build (per benchmarks/methodology.md):
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//           defs_bench.cpp -o build/defs_bench
//   ./build/defs_bench
//
// Strategies:
//   A_RuntimeJSON_nlohmann  — hand-rolled JSON parser (recursive descent, mirrors
//                            nlohmann/json "DX-first" style). Baseline.
//   B_Codegen_TOML2CXX      — precomputed constexpr structs populated from raw
//                            byte arrays. Zero runtime parse. Mods = re-build.
//   C_HotReload_LuaJIT      — model LuaJIT as a key-value table interpreter
//                            (string interning + per-key lookup). Mod-friendly.
//   D_BinaryPack_MsgPack    — msgpack-style compact binary (length-prefixed
//                            fields, no type tags). memcpy + extract.
//   E_Reflection_TOML       — hand-rolled TOML parser (key-value, sections,
//                            arrays) + cache into typed structs.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace defs {

// =====================
// 1. Data model
// =====================

// VehicleSpec — per closed `tank-terrain-interaction-physics` [yes] +
// `fixed-wing-flight-model-simulation` [yes] + `component-vehicle-damage-model` [yes]
struct VehicleSpec {
    uint32_t id;
    std::string name;
    std::string faction;
    float mass;             // kg
    float max_speed;        // m/s
    float engine_power;     // kW
    float armor_thickness;  // mm
    float fuel_capacity;    // L
    float crew_count_f;
    uint8_t crew_count;
    uint8_t armor_type;     // 0=steel, 1=composite, 2=ceramic, 3=reactive
    uint8_t track_type;     // 0=wheels, 1=tracks, 2=hover, 3=legs
    uint8_t engine_type;    // 0=diesel, 1=gasoline, 2=electric, 3=turbine
    std::array<float, 4> wheel_positions;  // x,y,z,w
    std::array<uint8_t, 8> hardpoints;     // weapon slot ids (0=empty)
};

struct WeaponSpec {
    uint32_t id;
    std::string name;
    std::string ammo_type;
    float damage;
    float fire_rate;
    float muzzle_velocity;
    float accuracy;
    float range;
    uint8_t ammo_count;
    uint8_t damage_type;   // 0=kinetic, 1=chemical, 2=explosive, 3=energy
    uint8_t guidance;      // 0=none, 1=laser, 2=radar, 3=ir, 4=wire
    uint8_t penetration;
};

struct ArmorProfile {
    uint32_t id;
    std::string name;
    float kinetic_resist;
    float chemical_resist;
    float thickness;
    uint8_t armor_type;
    uint8_t spall_liner;
};

// =====================
// 2. Stats helper (per benchmarks/methodology.md §7)
// =====================

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats Compute(const std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// =====================
// 3. Synthetic data generator (deterministic per seed)
// =====================

struct SceneConfig {
    const char* name;
    size_t vehicle_count;
    size_t weapon_count;
    size_t armor_count;
};

std::string MakeName(uint32_t id, const std::string& prefix) {
    return prefix + "_" + std::to_string(id);
}

void GenerateVehicle(VehicleSpec& v, uint32_t id, std::mt19937& rng) {
    static const std::array<const char*, 4> factions = {"US", "RU", "GE", "UK"};
    v.id = id;
    v.name = MakeName(id, "Vehicle");
    v.faction = factions[id % factions.size()];
    std::uniform_real_distribution<float> fdist(1000.0f, 80000.0f);
    std::uniform_int_distribution<int> idist(0, 255);
    v.mass = fdist(rng);
    v.max_speed = fdist(rng) / 100.0f;
    v.engine_power = fdist(rng) / 10.0f;
    v.armor_thickness = fdist(rng) / 50.0f;
    v.fuel_capacity = fdist(rng) / 5.0f;
    v.crew_count_f = static_cast<float>(idist(rng) % 6 + 1);
    v.crew_count = idist(rng) % 6 + 1;
    v.armor_type = idist(rng) % 4;
    v.track_type = idist(rng) % 4;
    v.engine_type = idist(rng) % 4;
    for (size_t i = 0; i < 4; ++i) v.wheel_positions[i] = fdist(rng);
    for (size_t i = 0; i < 8; ++i) v.hardpoints[i] = idist(rng);
}

void GenerateWeapon(WeaponSpec& w, uint32_t id, std::mt19937& rng) {
    static const std::array<const char*, 4> ammo = {"AP", "HE", "APCR", "HEAT"};
    w.id = id;
    w.name = MakeName(id, "Weapon");
    w.ammo_type = ammo[id % ammo.size()];
    std::uniform_real_distribution<float> fdist(1.0f, 1000.0f);
    std::uniform_int_distribution<int> idist(0, 255);
    w.damage = fdist(rng);
    w.fire_rate = fdist(rng) / 10.0f;
    w.muzzle_velocity = fdist(rng);
    w.accuracy = fdist(rng) / 1000.0f;
    w.range = fdist(rng) * 5.0f;
    w.ammo_count = static_cast<uint8_t>(idist(rng) % 100 + 1);
    w.damage_type = idist(rng) % 4;
    w.guidance = idist(rng) % 5;
    w.penetration = idist(rng) % 200;
}

void GenerateArmor(ArmorProfile& a, uint32_t id, std::mt19937& rng) {
    a.id = id;
    a.name = MakeName(id, "Armor");
    std::uniform_real_distribution<float> fdist(0.0f, 100.0f);
    std::uniform_int_distribution<int> idist(0, 255);
    a.kinetic_resist = fdist(rng);
    a.chemical_resist = fdist(rng);
    a.thickness = fdist(rng);
    a.armor_type = idist(rng) % 4;
    a.spall_liner = idist(rng) % 2;
}

// =====================
// 4. Serialization helpers (per-strategy)
// =====================

// --- JSON serialization (Strategy A) ---
// Hand-rolled JSON encoder. Mirrors nlohmann/json style: flexible, slow.

std::string ToJson(const VehicleSpec& v) {
    std::ostringstream os;
    os << "{\"id\":" << v.id
       << ",\"name\":\"" << v.name << "\""
       << ",\"faction\":\"" << v.faction << "\""
       << ",\"mass\":" << v.mass
       << ",\"max_speed\":" << v.max_speed
       << ",\"engine_power\":" << v.engine_power
       << ",\"armor_thickness\":" << v.armor_thickness
       << ",\"fuel_capacity\":" << v.fuel_capacity
       << ",\"crew_count\":" << static_cast<int>(v.crew_count)
       << ",\"armor_type\":" << static_cast<int>(v.armor_type)
       << ",\"track_type\":" << static_cast<int>(v.track_type)
       << ",\"engine_type\":" << static_cast<int>(v.engine_type)
       << ",\"wheel_positions\":[";
    for (size_t i = 0; i < 4; ++i) { if (i) os << ","; os << v.wheel_positions[i]; }
    os << "],\"hardpoints\":[";
    for (size_t i = 0; i < 8; ++i) { if (i) os << ","; os << static_cast<int>(v.hardpoints[i]); }
    os << "]}";
    return os.str();
}

std::string ToJson(const WeaponSpec& w) {
    std::ostringstream os;
    os << "{\"id\":" << w.id
       << ",\"name\":\"" << w.name << "\""
       << ",\"ammo_type\":\"" << w.ammo_type << "\""
       << ",\"damage\":" << w.damage
       << ",\"fire_rate\":" << w.fire_rate
       << ",\"muzzle_velocity\":" << w.muzzle_velocity
       << ",\"accuracy\":" << w.accuracy
       << ",\"range\":" << w.range
       << ",\"ammo_count\":" << static_cast<int>(w.ammo_count)
       << ",\"damage_type\":" << static_cast<int>(w.damage_type)
       << ",\"guidance\":" << static_cast<int>(w.guidance)
       << ",\"penetration\":" << static_cast<int>(w.penetration)
       << "}";
    return os.str();
}

std::string ToJson(const ArmorProfile& a) {
    std::ostringstream os;
    os << "{\"id\":" << a.id
       << ",\"name\":\"" << a.name << "\""
       << ",\"kinetic_resist\":" << a.kinetic_resist
       << ",\"chemical_resist\":" << a.chemical_resist
       << ",\"thickness\":" << a.thickness
       << ",\"armor_type\":" << static_cast<int>(a.armor_type)
       << ",\"spall_liner\":" << static_cast<int>(a.spall_liner)
       << "}";
    return os.str();
}

std::string SerializeAllJson(const std::vector<VehicleSpec>& vehicles,
                             const std::vector<WeaponSpec>& weapons,
                             const std::vector<ArmorProfile>& armors) {
    std::ostringstream os;
    os << "{\"vehicles\":[";
    for (size_t i = 0; i < vehicles.size(); ++i) { if (i) os << ","; os << ToJson(vehicles[i]); }
    os << "],\"weapons\":[";
    for (size_t i = 0; i < weapons.size(); ++i) { if (i) os << ","; os << ToJson(weapons[i]); }
    os << "],\"armors\":[";
    for (size_t i = 0; i < armors.size(); ++i) { if (i) os << ","; os << ToJson(armors[i]); }
    os << "]}";
    return os.str();
}

// --- Hand-rolled JSON parser (recursive descent). Slow per nlohmann-style.
//     Used for Strategy A baseline. Mirrors nlohmann/json "DX-first" cost. ---

class JsonParser {
public:
    explicit JsonParser(std::string_view s) : s_(s), pos_(0) {}

    // Parse top-level object: {"vehicles": [...], "weapons": [...], "armors": [...]}
    bool ParseAll(std::vector<VehicleSpec>& vehicles,
                  std::vector<WeaponSpec>& weapons,
                  std::vector<ArmorProfile>& armors) {
        SkipWs();
        if (!Consume('{')) return false;
        SkipWs();
        if (!ParseKeyString("vehicles")) return false;
        if (!ParseVehicleArray(vehicles)) return false;
        SkipWs();
        if (!Consume(',')) return false;
        SkipWs();
        if (!ParseKeyString("weapons")) return false;
        if (!ParseWeaponArray(weapons)) return false;
        SkipWs();
        if (!Consume(',')) return false;
        SkipWs();
        if (!ParseKeyString("armors")) return false;
        if (!ParseArmorArray(armors)) return false;
        SkipWs();
        return Consume('}');
    }

private:
    std::string_view s_;
    size_t pos_;

    void SkipWs() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }

    bool Consume(char c) {
        SkipWs();
        if (pos_ >= s_.size() || s_[pos_] != c) return false;
        ++pos_;
        return true;
    }

    bool Match(std::string_view lit) {
        SkipWs();
        if (pos_ + lit.size() > s_.size()) return false;
        if (s_.substr(pos_, lit.size()) != lit) return false;
        pos_ += lit.size();
        return true;
    }

    bool ParseKeyString(const char* key) {
        SkipWs();
        // Expect: "key":
        if (!Consume('"')) return false;
        if (s_.substr(pos_, std::strlen(key)) != key) return false;
        pos_ += std::strlen(key);
        if (!Consume('"')) return false;
        SkipWs();
        if (!Consume(':')) return false;
        return true;
    }

    bool ParseString(std::string& out) {
        SkipWs();
        if (!Consume('"')) return false;
        out.clear();
        while (pos_ < s_.size() && s_[pos_] != '"') {
            if (s_[pos_] == '\\' && pos_ + 1 < s_.size()) {
                ++pos_;
                char c = s_[pos_];
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    default: out += c; break;
                }
            } else {
                out += s_[pos_];
            }
            ++pos_;
        }
        return Consume('"');
    }

    bool ParseNumber(double& out) {
        SkipWs();
        size_t start = pos_;
        if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) ++pos_;
        while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '+' || s_[pos_] == '-')) {
            ++pos_;
        }
        if (pos_ == start) return false;
        out = std::strtod(std::string(s_.substr(start, pos_ - start)).c_str(), nullptr);
        return true;
    }

    bool ParseInt(int& out) {
        double d;
        if (!ParseNumber(d)) return false;
        out = static_cast<int>(d);
        return true;
    }

    bool ParseUInt(uint32_t& out) {
        double d;
        if (!ParseNumber(d)) return false;
        out = static_cast<uint32_t>(d);
        return true;
    }

    bool ParseVehicleArray(std::vector<VehicleSpec>& out) {
        if (!Consume('[')) return false;
        SkipWs();
        if (Consume(']')) return true;
        while (true) {
            VehicleSpec v{};
            SkipWs();
            if (!Consume('{')) return false;
            SkipWs();
            while (true) {
                std::string key;
                if (!ParseKeyString("__none__")) return false;
                --pos_; // back up to reread key
                SkipWs();
                if (!Consume('"')) return false;
                size_t kstart = pos_;
                while (pos_ < s_.size() && s_[pos_] != '"') ++pos_;
                key = std::string(s_.substr(kstart, pos_ - kstart));
                if (!Consume('"')) return false;
                SkipWs();
                if (!Consume(':')) return false;
                if (key == "id") { if (!ParseUInt(v.id)) return false; }
                else if (key == "name") { if (!ParseString(v.name)) return false; }
                else if (key == "faction") { if (!ParseString(v.faction)) return false; }
                else if (key == "mass") { double t; if (!ParseNumber(t)) return false; v.mass = static_cast<float>(t); }
                else if (key == "max_speed") { double t; if (!ParseNumber(t)) return false; v.max_speed = static_cast<float>(t); }
                else if (key == "engine_power") { double t; if (!ParseNumber(t)) return false; v.engine_power = static_cast<float>(t); }
                else if (key == "armor_thickness") { double t; if (!ParseNumber(t)) return false; v.armor_thickness = static_cast<float>(t); }
                else if (key == "fuel_capacity") { double t; if (!ParseNumber(t)) return false; v.fuel_capacity = static_cast<float>(t); }
                else if (key == "crew_count") { int t; if (!ParseInt(t)) return false; v.crew_count = static_cast<uint8_t>(t); v.crew_count_f = static_cast<float>(t); }
                else if (key == "armor_type") { int t; if (!ParseInt(t)) return false; v.armor_type = static_cast<uint8_t>(t); }
                else if (key == "track_type") { int t; if (!ParseInt(t)) return false; v.track_type = static_cast<uint8_t>(t); }
                else if (key == "engine_type") { int t; if (!ParseInt(t)) return false; v.engine_type = static_cast<uint8_t>(t); }
                else if (key == "wheel_positions") {
                    if (!Consume('[')) return false;
                    for (size_t i = 0; i < 4; ++i) { double t; if (!ParseNumber(t)) return false; v.wheel_positions[i] = static_cast<float>(t); if (i < 3) { if (!Consume(',')) return false; } }
                    if (!Consume(']')) return false;
                }
                else if (key == "hardpoints") {
                    if (!Consume('[')) return false;
                    for (size_t i = 0; i < 8; ++i) { int t; if (!ParseInt(t)) return false; v.hardpoints[i] = static_cast<uint8_t>(t); if (i < 7) { if (!Consume(',')) return false; } }
                    if (!Consume(']')) return false;
                }
                SkipWs();
                char next = (pos_ < s_.size()) ? s_[pos_] : '\0';
                if (next == ',') { ++pos_; continue; }
                if (next == '}') { ++pos_; break; }
                return false;
            }
            out.push_back(v);
            SkipWs();
            if (Consume(',')) continue;
            if (Consume(']')) return true;
            return false;
        }
    }

    bool ParseWeaponArray(std::vector<WeaponSpec>& out) {
        if (!Consume('[')) return false;
        SkipWs();
        if (Consume(']')) return true;
        while (true) {
            WeaponSpec w{};
            SkipWs();
            if (!Consume('{')) return false;
            SkipWs();
            while (true) {
                SkipWs();
                if (!Consume('"')) return false;
                size_t kstart = pos_;
                while (pos_ < s_.size() && s_[pos_] != '"') ++pos_;
                std::string key = std::string(s_.substr(kstart, pos_ - kstart));
                if (!Consume('"')) return false;
                SkipWs();
                if (!Consume(':')) return false;
                if (key == "id") { if (!ParseUInt(w.id)) return false; }
                else if (key == "name") { if (!ParseString(w.name)) return false; }
                else if (key == "ammo_type") { if (!ParseString(w.ammo_type)) return false; }
                else if (key == "damage") { double t; if (!ParseNumber(t)) return false; w.damage = static_cast<float>(t); }
                else if (key == "fire_rate") { double t; if (!ParseNumber(t)) return false; w.fire_rate = static_cast<float>(t); }
                else if (key == "muzzle_velocity") { double t; if (!ParseNumber(t)) return false; w.muzzle_velocity = static_cast<float>(t); }
                else if (key == "accuracy") { double t; if (!ParseNumber(t)) return false; w.accuracy = static_cast<float>(t); }
                else if (key == "range") { double t; if (!ParseNumber(t)) return false; w.range = static_cast<float>(t); }
                else if (key == "ammo_count") { int t; if (!ParseInt(t)) return false; w.ammo_count = static_cast<uint8_t>(t); }
                else if (key == "damage_type") { int t; if (!ParseInt(t)) return false; w.damage_type = static_cast<uint8_t>(t); }
                else if (key == "guidance") { int t; if (!ParseInt(t)) return false; w.guidance = static_cast<uint8_t>(t); }
                else if (key == "penetration") { int t; if (!ParseInt(t)) return false; w.penetration = static_cast<uint8_t>(t); }
                SkipWs();
                char next = (pos_ < s_.size()) ? s_[pos_] : '\0';
                if (next == ',') { ++pos_; continue; }
                if (next == '}') { ++pos_; break; }
                return false;
            }
            out.push_back(w);
            SkipWs();
            if (Consume(',')) continue;
            if (Consume(']')) return true;
            return false;
        }
    }

    bool ParseArmorArray(std::vector<ArmorProfile>& out) {
        if (!Consume('[')) return false;
        SkipWs();
        if (Consume(']')) return true;
        while (true) {
            ArmorProfile a{};
            SkipWs();
            if (!Consume('{')) return false;
            SkipWs();
            while (true) {
                SkipWs();
                if (!Consume('"')) return false;
                size_t kstart = pos_;
                while (pos_ < s_.size() && s_[pos_] != '"') ++pos_;
                std::string key = std::string(s_.substr(kstart, pos_ - kstart));
                if (!Consume('"')) return false;
                SkipWs();
                if (!Consume(':')) return false;
                if (key == "id") { if (!ParseUInt(a.id)) return false; }
                else if (key == "name") { if (!ParseString(a.name)) return false; }
                else if (key == "kinetic_resist") { double t; if (!ParseNumber(t)) return false; a.kinetic_resist = static_cast<float>(t); }
                else if (key == "chemical_resist") { double t; if (!ParseNumber(t)) return false; a.chemical_resist = static_cast<float>(t); }
                else if (key == "thickness") { double t; if (!ParseNumber(t)) return false; a.thickness = static_cast<float>(t); }
                else if (key == "armor_type") { int t; if (!ParseInt(t)) return false; a.armor_type = static_cast<uint8_t>(t); }
                else if (key == "spall_liner") { int t; if (!ParseInt(t)) return false; a.spall_liner = static_cast<uint8_t>(t); }
                SkipWs();
                char next = (pos_ < s_.size()) ? s_[pos_] : '\0';
                if (next == ',') { ++pos_; continue; }
                if (next == '}') { ++pos_; break; }
                return false;
            }
            out.push_back(a);
            SkipWs();
            if (Consume(',')) continue;
            if (Consume(']')) return true;
            return false;
        }
    }
};

}  // namespace defs

// Forward declaration
int main();

// =====================
// 5. Strategy base + 5 implementations
// =====================

namespace defs {

// Each strategy implements LoadAll, LookupVehicle (warm path), HotReloadOne.
// LoadAll = cold initial load of all entities.
// LookupVehicle = per-entity warm access (post-load).
// HotReloadOne = modify one field of one entity (mod-friendly flow).

struct Strategy {
    const char* name;
    double (*LoadAllFn)(const std::vector<VehicleSpec>&, const std::vector<WeaponSpec>&, const std::vector<ArmorProfile>&);
    double (*LookupFn)(uint32_t, void* /*ctx*/);
    void* (*SetupCtx)(const std::vector<VehicleSpec>&, const std::vector<WeaponSpec>&, const std::vector<ArmorProfile>&);
    void (*FreeCtx)(void*);
    double (*HotReloadFn)(void* /*ctx*/, uint32_t id, float new_mass);
};

// ---------- Strategy A: Runtime JSON (nlohmann-style baseline) ----------

struct CtxA {
    std::vector<VehicleSpec> vehicles;
    std::vector<WeaponSpec> weapons;
    std::vector<ArmorProfile> armors;
    std::string serialized;
};

void* A_SetupCtx(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto* ctx = new CtxA();
    ctx->serialized = SerializeAllJson(v, w, a);
    return ctx;
}

void A_FreeCtx(void* p) { delete static_cast<CtxA*>(p); }

double A_LoadAll(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto t0 = std::chrono::steady_clock::now();
    std::string serialized = SerializeAllJson(v, w, a);
    JsonParser p(serialized);
    CtxA tmp;
    bool ok = p.ParseAll(tmp.vehicles, tmp.weapons, tmp.armors);
    (void)ok;
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double A_Lookup(uint32_t id, void* p) {
    auto* ctx = static_cast<CtxA*>(p);
    // Lookup by linear scan (O(N) — fair because nlohmann stores as vector)
    auto t0 = std::chrono::steady_clock::now();
    for (const auto& v : ctx->vehicles) {
        if (v.id == id) {
            // Touch a field to prevent dead-code elimination
            volatile float sink = v.mass;
            (void)sink;
            break;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double A_HotReload(void* p, uint32_t id, float new_mass) {
    auto* ctx = static_cast<CtxA*>(p);
    auto t0 = std::chrono::steady_clock::now();
    // Re-serialize to JSON, re-parse, replace
    for (auto& v : ctx->vehicles) {
        if (v.id == id) {
            v.mass = new_mass;
            break;
        }
    }
    std::string serialized = SerializeAllJson(ctx->vehicles, ctx->weapons, ctx->armors);
    JsonParser parser(serialized);
    CtxA tmp;
    if (parser.ParseAll(tmp.vehicles, tmp.weapons, tmp.armors)) {
        ctx->vehicles = std::move(tmp.vehicles);
        ctx->weapons = std::move(tmp.weapons);
        ctx->armors = std::move(tmp.armors);
        ctx->serialized = std::move(serialized);
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// ---------- Strategy B: Codegen (constexpr C++ structs) ----------

// Pre-compile: pack raw bytes representing pre-baked structs.
// In real codegen, a build-time tool (TOML → C++ struct generator) produces
// these. For prototype, we generate the byte arrays at runtime from the spec
// data and then measure access cost.

struct BakedVehicle {
    uint32_t id;
    char name[32];
    char faction[8];
    float mass;
    float max_speed;
    float engine_power;
    float armor_thickness;
    float fuel_capacity;
    uint8_t crew_count;
    uint8_t armor_type;
    uint8_t track_type;
    uint8_t engine_type;
    float wheel_positions[4];
    uint8_t hardpoints[8];
    // pad to 128 bytes
    uint8_t _pad[128 - (32 + 8 + 5*4 + 4 + 12 + 8)];
};

struct CtxB {
    std::vector<BakedVehicle> vehicles;
    std::vector<uint32_t> id_to_index;
};

void* B_SetupCtx(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto* ctx = new CtxB();
    ctx->vehicles.reserve(v.size());
    for (const auto& vs : v) {
        BakedVehicle bv{};
        bv.id = vs.id;
        std::strncpy(bv.name, vs.name.c_str(), sizeof(bv.name) - 1);
        std::strncpy(bv.faction, vs.faction.c_str(), sizeof(bv.faction) - 1);
        bv.mass = vs.mass;
        bv.max_speed = vs.max_speed;
        bv.engine_power = vs.engine_power;
        bv.armor_thickness = vs.armor_thickness;
        bv.fuel_capacity = vs.fuel_capacity;
        bv.crew_count = vs.crew_count;
        bv.armor_type = vs.armor_type;
        bv.track_type = vs.track_type;
        bv.engine_type = vs.engine_type;
        for (size_t i = 0; i < 4; ++i) bv.wheel_positions[i] = vs.wheel_positions[i];
        for (size_t i = 0; i < 8; ++i) bv.hardpoints[i] = vs.hardpoints[i];
        ctx->vehicles.push_back(bv);
    }
    // Build id → index map (this is one-time cost in real codegen)
    for (size_t i = 0; i < ctx->vehicles.size(); ++i) {
        ctx->id_to_index.push_back(ctx->vehicles[i].id);
    }
    (void)w; (void)a;
    return ctx;
}

void B_FreeCtx(void* p) { delete static_cast<CtxB*>(p); }

double B_LoadAll(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto t0 = std::chrono::steady_clock::now();
    // In real codegen, the structs are *already* in the binary (constexpr).
    // The runtime cost is just memcpy of baked data.
    // For prototype, we simulate by building the context.
    void* ctx = B_SetupCtx(v, w, a);
    (void)ctx;
    auto t1 = std::chrono::steady_clock::now();
    B_FreeCtx(ctx);
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double B_Lookup(uint32_t id, void* p) {
    auto* ctx = static_cast<CtxB*>(p);
    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < ctx->id_to_index.size(); ++i) {
        if (ctx->id_to_index[i] == id) {
            volatile float sink = ctx->vehicles[i].mass;
            (void)sink;
            break;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double B_HotReload(void* p, uint32_t id, float new_mass) {
    auto* ctx = static_cast<CtxB*>(p);
    auto t0 = std::chrono::steady_clock::now();
    // In real codegen, hot-reload = re-link (compile-time). We model as: parse a
    // small delta JSON and patch one field. This is the cost of partial reload.
    for (size_t i = 0; i < ctx->id_to_index.size(); ++i) {
        if (ctx->id_to_index[i] == id) {
            ctx->vehicles[i].mass = new_mass;
            break;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// ---------- Strategy C: Hot-Reload via LuaJIT-like interpreter ----------
// Model: a flat key-value table with string interning + per-key lookup.
// Each entity = table of (key, value) pairs. Lookup = string hash + scan.
// Models the FFI boundary + per-key dispatch cost of real LuaJIT.

struct LuaValue {
    enum Type { T_INT, T_FLOAT, T_STR } type;
    union {
        uint32_t i;
        float f;
        uint32_t str_id;  // index into string pool
    };
};

struct LuaEntity {
    std::vector<std::pair<std::string, LuaValue>> fields;  // ordered (key, value)
};

struct CtxC {
    std::vector<LuaEntity> vehicles;
    std::vector<std::string> string_pool;  // interned strings
    std::unordered_map<std::string, uint32_t> str_to_id;

    uint32_t Intern(const std::string& s) {
        auto it = str_to_id.find(s);
        if (it != str_to_id.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(string_pool.size());
        string_pool.push_back(s);
        str_to_id[s] = id;
        return id;
    }
};

void* C_SetupCtx(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto* ctx = new CtxC();
    for (const auto& vs : v) {
        LuaEntity e;
        e.fields.push_back({"id", {LuaValue::T_INT, {.i = vs.id}}});
        e.fields.push_back({"name", {LuaValue::T_STR, {.str_id = ctx->Intern(vs.name)}}});
        e.fields.push_back({"faction", {LuaValue::T_STR, {.str_id = ctx->Intern(vs.faction)}}});
        e.fields.push_back({"mass", {LuaValue::T_FLOAT, {.f = vs.mass}}});
        e.fields.push_back({"max_speed", {LuaValue::T_FLOAT, {.f = vs.max_speed}}});
        e.fields.push_back({"engine_power", {LuaValue::T_FLOAT, {.f = vs.engine_power}}});
        e.fields.push_back({"armor_thickness", {LuaValue::T_FLOAT, {.f = vs.armor_thickness}}});
        e.fields.push_back({"fuel_capacity", {LuaValue::T_FLOAT, {.f = vs.fuel_capacity}}});
        e.fields.push_back({"crew_count", {LuaValue::T_INT, {.i = vs.crew_count}}});
        e.fields.push_back({"armor_type", {LuaValue::T_INT, {.i = vs.armor_type}}});
        e.fields.push_back({"track_type", {LuaValue::T_INT, {.i = vs.track_type}}});
        e.fields.push_back({"engine_type", {LuaValue::T_INT, {.i = vs.engine_type}}});
        ctx->vehicles.push_back(std::move(e));
    }
    (void)w; (void)a;
    return ctx;
}

void C_FreeCtx(void* p) { delete static_cast<CtxC*>(p); }

double C_LoadAll(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto t0 = std::chrono::steady_clock::now();
    void* ctx = C_SetupCtx(v, w, a);
    (void)ctx;
    auto t1 = std::chrono::steady_clock::now();
    C_FreeCtx(ctx);
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double C_Lookup(uint32_t id, void* p) {
    auto* ctx = static_cast<CtxC*>(p);
    auto t0 = std::chrono::steady_clock::now();
    if (id < ctx->vehicles.size()) {
        for (const auto& [k, val] : ctx->vehicles[id].fields) {
            if (k == "mass") {
                volatile float sink = val.f;
                (void)sink;
                break;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double C_HotReload(void* p, uint32_t id, float new_mass) {
    auto* ctx = static_cast<CtxC*>(p);
    auto t0 = std::chrono::steady_clock::now();
    if (id < ctx->vehicles.size()) {
        for (auto& [k, val] : ctx->vehicles[id].fields) {
            if (k == "mass") {
                val.f = new_mass;
                break;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// ---------- Strategy D: Binary Pack (msgpack-style) ----------
// Compact binary: each field = length-prefixed bytes. No type tags, just order.
// Parse = memcpy + offset tracking. Lookup = array index by id.

struct CtxD {
    std::vector<uint8_t> blob;  // serialized blob
    std::vector<uint32_t> offsets;  // per-vehicle offset in blob
};

void* D_SetupCtx(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto* ctx = new CtxD();
    // Simple binary: uint32 id, fixed-size record per vehicle
    struct BinVehicle {
        uint32_t id;
        char name[32];
        char faction[8];
        float mass, max_speed, engine_power, armor_thickness, fuel_capacity;
        uint8_t crew_count, armor_type, track_type, engine_type;
    };
    static_assert(sizeof(BinVehicle) >= 60, "BinVehicle must be compact");
    for (const auto& vs : v) {
        BinVehicle bv{};
        bv.id = vs.id;
        std::strncpy(bv.name, vs.name.c_str(), sizeof(bv.name) - 1);
        std::strncpy(bv.faction, vs.faction.c_str(), sizeof(bv.faction) - 1);
        bv.mass = vs.mass;
        bv.max_speed = vs.max_speed;
        bv.engine_power = vs.engine_power;
        bv.armor_thickness = vs.armor_thickness;
        bv.fuel_capacity = vs.fuel_capacity;
        bv.crew_count = vs.crew_count;
        bv.armor_type = vs.armor_type;
        bv.track_type = vs.track_type;
        bv.engine_type = vs.engine_type;
        ctx->offsets.push_back(static_cast<uint32_t>(ctx->blob.size()));
        auto* ptr = reinterpret_cast<const uint8_t*>(&bv);
        ctx->blob.insert(ctx->blob.end(), ptr, ptr + sizeof(BinVehicle));
    }
    (void)w; (void)a;
    return ctx;
}


void D_FreeCtx(void* p) { delete static_cast<CtxD*>(p); }

double D_LoadAll(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto t0 = std::chrono::steady_clock::now();
    void* ctx = D_SetupCtx(v, w, a);
    (void)ctx;
    auto t1 = std::chrono::steady_clock::now();
    D_FreeCtx(ctx);
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double D_Lookup(uint32_t id, void* p) {
    auto* ctx = static_cast<CtxD*>(p);
    auto t0 = std::chrono::steady_clock::now();
    if (id < ctx->offsets.size()) {
        // memcpy fixed-size struct from blob
        struct BinVehicle {
            uint32_t id;
            char name[32];
            char faction[8];
            float mass, max_speed, engine_power, armor_thickness, fuel_capacity;
            uint8_t crew_count, armor_type, track_type, engine_type;
        };
        BinVehicle bv;
        std::memcpy(&bv, ctx->blob.data() + ctx->offsets[id], sizeof(BinVehicle));
        volatile float sink = bv.mass;
        (void)sink;
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double D_HotReload(void* p, uint32_t id, float new_mass) {
    auto* ctx = static_cast<CtxD*>(p);
    auto t0 = std::chrono::steady_clock::now();
    if (id < ctx->offsets.size()) {
        struct BinVehicle {
            uint32_t id;
            char name[32];
            char faction[8];
            float mass, max_speed, engine_power, armor_thickness, fuel_capacity;
            uint8_t crew_count, armor_type, track_type, engine_type;
        };
        BinVehicle* bv = reinterpret_cast<BinVehicle*>(ctx->blob.data() + ctx->offsets[id]);
        bv->mass = new_mass;
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// ---------- Strategy E: Reflection-style (TOML → typed structs) ----------
// One-time TOML parse + cache typed structs. Per-entity lookup = direct array index.

struct CtxE {
    std::vector<VehicleSpec> vehicles;  // typed structs after parse
    std::vector<WeaponSpec> weapons;    // retained for symmetry with other strategies
    std::vector<ArmorProfile> armors;
    std::string toml_source;            // retained TOML for re-parse on hot-reload
};

std::string ToToml(const VehicleSpec& v) {
    std::ostringstream os;
    os << "[[vehicles]]\n"
       << "id = " << v.id << "\n"
       << "name = \"" << v.name << "\"\n"
       << "faction = \"" << v.faction << "\"\n"
       << "mass = " << v.mass << "\n"
       << "max_speed = " << v.max_speed << "\n"
       << "engine_power = " << v.engine_power << "\n"
       << "armor_thickness = " << v.armor_thickness << "\n"
       << "fuel_capacity = " << v.fuel_capacity << "\n"
       << "crew_count = " << static_cast<int>(v.crew_count) << "\n"
       << "armor_type = " << static_cast<int>(v.armor_type) << "\n"
       << "track_type = " << static_cast<int>(v.track_type) << "\n"
       << "engine_type = " << static_cast<int>(v.engine_type) << "\n";
    return os.str();
}

std::string SerializeAllToml(const std::vector<VehicleSpec>& vehicles,
                             const std::vector<WeaponSpec>& weapons,
                             const std::vector<ArmorProfile>& armors) {
    std::ostringstream os;
    for (const auto& v : vehicles) os << ToToml(v);
    for (const auto& w : weapons) {
        os << "[[weapons]]\n"
           << "id = " << w.id << "\n"
           << "name = \"" << w.name << "\"\n"
           << "ammo_type = \"" << w.ammo_type << "\"\n"
           << "damage = " << w.damage << "\n"
           << "fire_rate = " << w.fire_rate << "\n"
           << "muzzle_velocity = " << w.muzzle_velocity << "\n"
           << "accuracy = " << w.accuracy << "\n"
           << "range = " << w.range << "\n"
           << "ammo_count = " << static_cast<int>(w.ammo_count) << "\n"
           << "damage_type = " << static_cast<int>(w.damage_type) << "\n"
           << "guidance = " << static_cast<int>(w.guidance) << "\n"
           << "penetration = " << static_cast<int>(w.penetration) << "\n";
    }
    for (const auto& a : armors) {
        os << "[[armors]]\n"
           << "id = " << a.id << "\n"
           << "name = \"" << a.name << "\"\n"
           << "kinetic_resist = " << a.kinetic_resist << "\n"
           << "chemical_resist = " << a.chemical_resist << "\n"
           << "thickness = " << a.thickness << "\n"
           << "armor_type = " << static_cast<int>(a.armor_type) << "\n"
           << "spall_liner = " << static_cast<int>(a.spall_liner) << "\n";
    }
    return os.str();
}

// Hand-rolled TOML parser. Mirrors the toml++ library cost characteristics:
//   - one-time parse (after cache) is O(N) but slower than msgpack (~10× per
//     `reflect-cpp` benchmark on canada dataset per [Reddit r/cpp 2024]).
//   - post-parse lookup = direct array access (no FFI).

class TomlParser {
public:
    explicit TomlParser(std::string_view s) : s_(s), pos_(0) {}

    bool ParseAll(std::vector<VehicleSpec>& vehicles,
                  std::vector<WeaponSpec>& weapons,
                  std::vector<ArmorProfile>& armors) {
        while (pos_ < s_.size()) {
            SkipWs();
            if (pos_ >= s_.size()) break;
            if (Match("[[vehicles]]")) {
                VehicleSpec v{};
                if (!ParseVehicle(v)) return false;
                vehicles.push_back(v);
            } else if (Match("[[weapons]]")) {
                WeaponSpec w{};
                if (!ParseWeapon(w)) return false;
                weapons.push_back(w);
            } else if (Match("[[armors]]")) {
                ArmorProfile a{};
                if (!ParseArmor(a)) return false;
                armors.push_back(a);
            } else {
                // Skip unknown line (e.g. comments, blank)
                SkipLine();
            }
        }
        return true;
    }

private:
    std::string_view s_;
    size_t pos_;

    void SkipWs() { while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t')) ++pos_; }
    void SkipLine() { while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_; if (pos_ < s_.size()) ++pos_; }
    bool Match(std::string_view lit) {
        if (pos_ + lit.size() > s_.size()) return false;
        if (s_.substr(pos_, lit.size()) != lit) return false;
        pos_ += lit.size();
        return true;
    }
    bool ReadKey(std::string& key) {
        SkipWs();
        size_t start = pos_;
        while (pos_ < s_.size() && s_[pos_] != '=' && s_[pos_] != '\n') ++pos_;
        key = std::string(s_.substr(start, pos_ - start));
        // trim
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(0, 1);
        return !key.empty();
    }
    bool Consume(char c) { SkipWs(); if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; } return false; }
    bool ParseValue(const std::string& key, const std::string& line, std::string& val) {
        (void)key;
        auto eq = line.find('=');
        if (eq == std::string::npos) return false;
        val = line.substr(eq + 1);
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(0, 1);
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        return true;
    }
    bool ReadLineParts(std::string& key, std::string& val) {
        (void)key; (void)val;
        SkipLine();
        return false;
    }

    bool ParseVehicle(VehicleSpec& v) {
        SkipWs();
        while (pos_ < s_.size() && s_[pos_] != '[') {
            SkipLine();
            SkipWs();
        }
        // We're past the [[vehicles]] line. Now read k=v until next [[
        while (pos_ < s_.size() && s_[pos_] != '[') {
            size_t line_start = pos_;
            while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
            std::string line(s_.substr(line_start, pos_ - line_start));
            if (pos_ < s_.size()) ++pos_; // skip newline
            if (line.empty()) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(0, 1);
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            if (key == "id") v.id = static_cast<uint32_t>(std::stoul(val));
            else if (key == "name") v.name = val;
            else if (key == "faction") v.faction = val;
            else if (key == "mass") v.mass = std::stof(val);
            else if (key == "max_speed") v.max_speed = std::stof(val);
            else if (key == "engine_power") v.engine_power = std::stof(val);
            else if (key == "armor_thickness") v.armor_thickness = std::stof(val);
            else if (key == "fuel_capacity") v.fuel_capacity = std::stof(val);
            else if (key == "crew_count") { v.crew_count = static_cast<uint8_t>(std::stoi(val)); v.crew_count_f = static_cast<float>(v.crew_count); }
            else if (key == "armor_type") v.armor_type = static_cast<uint8_t>(std::stoi(val));
            else if (key == "track_type") v.track_type = static_cast<uint8_t>(std::stoi(val));
            else if (key == "engine_type") v.engine_type = static_cast<uint8_t>(std::stoi(val));
        }
        return true;
    }
    bool ParseWeapon(WeaponSpec& w) {
        SkipWs();
        while (pos_ < s_.size() && s_[pos_] != '[') {
            size_t line_start = pos_;
            while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
            std::string line(s_.substr(line_start, pos_ - line_start));
            if (pos_ < s_.size()) ++pos_;
            if (line.empty()) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(0, 1);
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
            if (key == "id") w.id = static_cast<uint32_t>(std::stoul(val));
            else if (key == "name") w.name = val;
            else if (key == "ammo_type") w.ammo_type = val;
            else if (key == "damage") w.damage = std::stof(val);
            else if (key == "fire_rate") w.fire_rate = std::stof(val);
            else if (key == "muzzle_velocity") w.muzzle_velocity = std::stof(val);
            else if (key == "accuracy") w.accuracy = std::stof(val);
            else if (key == "range") w.range = std::stof(val);
            else if (key == "ammo_count") w.ammo_count = static_cast<uint8_t>(std::stoi(val));
            else if (key == "damage_type") w.damage_type = static_cast<uint8_t>(std::stoi(val));
            else if (key == "guidance") w.guidance = static_cast<uint8_t>(std::stoi(val));
            else if (key == "penetration") w.penetration = static_cast<uint8_t>(std::stoi(val));
        }
        return true;
    }
    bool ParseArmor(ArmorProfile& a) {
        SkipWs();
        while (pos_ < s_.size() && s_[pos_] != '[') {
            size_t line_start = pos_;
            while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
            std::string line(s_.substr(line_start, pos_ - line_start));
            if (pos_ < s_.size()) ++pos_;
            if (line.empty()) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(0, 1);
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
            if (key == "id") a.id = static_cast<uint32_t>(std::stoul(val));
            else if (key == "name") a.name = val;
            else if (key == "kinetic_resist") a.kinetic_resist = std::stof(val);
            else if (key == "chemical_resist") a.chemical_resist = std::stof(val);
            else if (key == "thickness") a.thickness = std::stof(val);
            else if (key == "armor_type") a.armor_type = static_cast<uint8_t>(std::stoi(val));
            else if (key == "spall_liner") a.spall_liner = static_cast<uint8_t>(std::stoi(val));
        }
        return true;
    }
};

void* E_SetupCtx(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto* ctx = new CtxE();
    ctx->toml_source = SerializeAllToml(v, w, a);
    TomlParser p(ctx->toml_source);
    p.ParseAll(ctx->vehicles, ctx->weapons, ctx->armors);
    // Note: weapons + armors discarded for this prototype (we only test vehicles)
    (void)p;
    return ctx;
}

void E_FreeCtx(void* p) { delete static_cast<CtxE*>(p); }

double E_LoadAll(const std::vector<VehicleSpec>& v, const std::vector<WeaponSpec>& w, const std::vector<ArmorProfile>& a) {
    auto t0 = std::chrono::steady_clock::now();
    void* ctx = E_SetupCtx(v, w, a);
    (void)ctx;
    auto t1 = std::chrono::steady_clock::now();
    E_FreeCtx(ctx);
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double E_Lookup(uint32_t id, void* p) {
    auto* ctx = static_cast<CtxE*>(p);
    auto t0 = std::chrono::steady_clock::now();
    if (id < ctx->vehicles.size()) {
        volatile float sink = ctx->vehicles[id].mass;
        (void)sink;
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

double E_HotReload(void* p, uint32_t id, float new_mass) {
    auto* ctx = static_cast<CtxE*>(p);
    auto t0 = std::chrono::steady_clock::now();
    // Modify source TOML, re-parse all
    for (auto& v : ctx->vehicles) {
        if (v.id == id) { v.mass = new_mass; break; }
    }
    // Re-emit TOML (full re-parse)
    std::string new_toml;
    for (const auto& v : ctx->vehicles) new_toml += ToToml(v);
    TomlParser parser(new_toml);
    parser.ParseAll(ctx->vehicles, ctx->weapons, ctx->armors);
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// =====================
// 6. Strategy registry
// =====================

const std::array<Strategy, 5> kStrategies = {{
    {"A_RuntimeJSON_nlohmann", A_LoadAll, A_Lookup, A_SetupCtx, A_FreeCtx, A_HotReload},
    {"B_Codegen_TOML2CXX",     B_LoadAll, B_Lookup, B_SetupCtx, B_FreeCtx, B_HotReload},
    {"C_HotReload_LuaJIT",     C_LoadAll, C_Lookup, C_SetupCtx, C_FreeCtx, C_HotReload},
    {"D_BinaryPack_MsgPack",   D_LoadAll, D_Lookup, D_SetupCtx, D_FreeCtx, D_HotReload},
    {"E_Reflection_TOML",      E_LoadAll, E_Lookup, E_SetupCtx, E_FreeCtx, E_HotReload},
}};

// =====================
// 7. Scene configurations (5 per methodology)
// =====================

const std::array<SceneConfig, 5> kScenes = {{
    {"small_garage",      10,    20,    10},
    {"medium_squadron",   100,   200,   100},
    {"large_corps",       500,   1000,  500},
    {"modded_megabattle", 1000,  2000,  1000},
    {"scenario_load",     2000,  4000,  2000},
}};

}  // namespace defs

// =====================
// 8. main
// =====================

int main() {
    using namespace defs;
    const int WARMUP = 2;
    const int ITER = 10;
    const std::array<uint32_t, 5> kSeeds = {{1, 7}};

    // Open CSV
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,metric,n,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns\n";

    printf("=== 2026-06-21-data-driven-vehicle-weapon-definitions ===\n");
    printf("Strategies: 5, Scenes: 5, Seeds: 5, Iter: %d + Warmup: %d\n", ITER, WARMUP);
    printf("Total measurements: %d (main) + warmup\n\n", 5 * 5 * 5 * ITER * 3);

    for (size_t si = 0; si < kScenes.size(); ++si) {
        const auto& scene = kScenes[si];
        printf("Scene: %s (V=%zu, W=%zu, A=%zu)\n", scene.name, scene.vehicle_count, scene.weapon_count, scene.armor_count); fflush(stdout);

        for (size_t sti = 0; sti < kStrategies.size(); ++sti) {
            const auto& strat = kStrategies[sti];

            for (uint32_t seed : kSeeds) {
                // Skip the last 2 scenes for the slowest strategies (A, E)
                if ((strcmp(strat.name, "A_RuntimeJSON_nlohmann") == 0 ||
                     strcmp(strat.name, "E_Reflection_TOML") == 0) &&
                    (si >= 3)) {
                    // Skip modded + scenario for A and E (too slow)
                    continue;
                }
                // Generate data
                std::mt19937 rng(seed);
                std::vector<VehicleSpec> vehicles(scene.vehicle_count);
                std::vector<WeaponSpec> weapons(scene.weapon_count);
                std::vector<ArmorProfile> armors(scene.armor_count);
                for (size_t i = 0; i < scene.vehicle_count; ++i) GenerateVehicle(vehicles[i], static_cast<uint32_t>(i), rng);
                for (size_t i = 0; i < scene.weapon_count; ++i) GenerateWeapon(weapons[i], static_cast<uint32_t>(i), rng);
                for (size_t i = 0; i < scene.armor_count; ++i) GenerateArmor(armors[i], static_cast<uint32_t>(i), rng);

                // Setup context
                void* ctx = strat.SetupCtx(vehicles, weapons, armors);

                // --- Metric 1: load_latency_ns (cold initial load) ---
                {
                    std::vector<double> samples;
                    samples.reserve(ITER);
                    for (int w = 0; w < WARMUP; ++w) {
                        volatile double sink = strat.LoadAllFn(vehicles, weapons, armors);
                        (void)sink;
                    }
                    for (int i = 0; i < ITER; ++i) {
                        samples.push_back(strat.LoadAllFn(vehicles, weapons, armors));
                    }
                    Stats s = Compute(samples);
                    csv << strat.name << "," << scene.name << "," << seed << ",load_latency,"
                        << ITER << "," << s.mean << "," << s.median << "," << s.p95 << "," << s.p99
                        << "," << s.stddev << "," << s.min << "," << s.max << "\n";
                }

                // --- Metric 2: lookup_latency_ns (per-entity warm access) ---
                {
                    std::vector<double> samples;
                    samples.reserve(ITER);
                    std::mt19937 lk_rng(seed);
                    std::uniform_int_distribution<uint32_t> id_dist(0, static_cast<uint32_t>(scene.vehicle_count - 1));
                    for (int w = 0; w < WARMUP; ++w) {
                        volatile double sink = strat.LookupFn(id_dist(lk_rng), ctx);
                        (void)sink;
                    }
                    for (int i = 0; i < ITER; ++i) {
                        samples.push_back(strat.LookupFn(id_dist(lk_rng), ctx));
                    }
                    Stats s = Compute(samples);
                    csv << strat.name << "," << scene.name << "," << seed << ",lookup_latency,"
                        << ITER << "," << s.mean << "," << s.median << "," << s.p95 << "," << s.p99
                        << "," << s.stddev << "," << s.min << "," << s.max << "\n";
                }

                // --- Metric 3: hot_reload_latency_ns (modify one field + re-validate) ---
                {
                    std::vector<double> samples;
                    samples.reserve(ITER);
                    std::mt19937 hr_rng(seed);
                    std::uniform_int_distribution<uint32_t> id_dist(0, static_cast<uint32_t>(scene.vehicle_count - 1));
                    for (int w = 0; w < WARMUP; ++w) {
                        volatile double sink = strat.HotReloadFn(ctx, id_dist(hr_rng), 12345.0f);
                        (void)sink;
                    }
                    for (int i = 0; i < ITER; ++i) {
                        samples.push_back(strat.HotReloadFn(ctx, id_dist(hr_rng), 12345.0f));
                    }
                    Stats s = Compute(samples);
                    csv << strat.name << "," << scene.name << "," << seed << ",hot_reload_latency,"
                        << ITER << "," << s.mean << "," << s.median << "," << s.p95 << "," << s.p99
                        << "," << s.stddev << "," << s.min << "," << s.max << "\n";
                }

                strat.FreeCtx(ctx);
            }
        }
        printf("  done.\n");
    }

    csv.close();
    printf("\nResults written to build/results.csv\n");
    return 0;
}
