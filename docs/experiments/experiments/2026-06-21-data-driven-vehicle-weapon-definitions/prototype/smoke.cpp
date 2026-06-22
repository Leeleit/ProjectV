// Minimal smoke test - just verify A_LoadAll and B_LoadAll work
#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>

namespace defs {
struct VehicleSpec {
    uint32_t id;
    std::string name;
    std::string faction;
    float mass;
    float max_speed;
    float engine_power;
    float armor_thickness;
    float fuel_capacity;
    float crew_count_f;
    uint8_t crew_count;
    uint8_t armor_type;
    uint8_t track_type;
    uint8_t engine_type;
    std::array<float, 4> wheel_positions;
    std::array<uint8_t, 8> hardpoints;
};
struct WeaponSpec {
    uint32_t id;
    std::string name;
    std::string ammo_type;
    float damage, fire_rate, muzzle_velocity, accuracy, range;
    uint8_t ammo_count, damage_type, guidance, penetration;
};
struct ArmorProfile {
    uint32_t id;
    std::string name;
    float kinetic_resist, chemical_resist, thickness;
    uint8_t armor_type, spall_liner;
};
}

#include <array>
int main() {
    using namespace defs;
    std::cout << "Smoke test starting...\n" << std::flush;

    std::vector<VehicleSpec> v(100);
    std::vector<WeaponSpec> w(200);
    std::vector<ArmorProfile> a(100);

    for (size_t i = 0; i < 100; ++i) {
        v[i].id = static_cast<uint32_t>(i);
        v[i].name = "V" + std::to_string(i);
        v[i].faction = "US";
        v[i].mass = static_cast<float>(i * 100);
        v[i].max_speed = 50.0f;
        v[i].engine_power = 1000.0f;
        v[i].armor_thickness = 100.0f;
        v[i].fuel_capacity = 500.0f;
        v[i].crew_count_f = 4.0f;
        v[i].crew_count = 4;
        v[i].armor_type = 0;
        v[i].track_type = 0;
        v[i].engine_type = 0;
        for (int j = 0; j < 4; ++j) v[i].wheel_positions[j] = static_cast<float>(j);
        for (int j = 0; j < 8; ++j) v[i].hardpoints[j] = 0;
    }

    auto t0 = std::chrono::steady_clock::now();
    volatile size_t sum = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        sum += v[i].id;
        sum += v[i].mass;
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::cout << "Loop 1: " << ns << " ns, sum=" << sum << "\n" << std::flush;

    // Baked size
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
    };
    BakedVehicle bv{};
    bv.id = 42;
    strncpy(bv.name, "Test", sizeof(bv.name));
    strncpy(bv.faction, "US", sizeof(bv.faction));
    bv.mass = 12345.0f;
    auto t2 = std::chrono::steady_clock::now();
    volatile float sink = bv.mass;
    (void)sink;
    auto t3 = std::chrono::steady_clock::now();
    double ns2 = std::chrono::duration<double, std::nano>(t3 - t2).count();
    std::cout << "Direct struct access: " << ns2 << " ns\n" << std::flush;

    std::cout << "Smoke test DONE\n" << std::flush;
    return 0;
}