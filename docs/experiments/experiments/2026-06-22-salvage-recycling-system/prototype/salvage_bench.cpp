#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

// ============================================================================
// Enums & constexpr data tables
// ============================================================================

enum class Material : uint8_t {
    Steel, Aluminum, Copper, Titanium, Tungsten,
    Plastic, Glass, Rubber, Electronics, Composites, DepletedUranium, COUNT_
};

enum class DestructionMethod : uint8_t {
    Explosion, Fire, Kinetic, MagicalBeam, StructuralFailure, COUNT_
};

enum class EntityType : uint8_t {
    Tank, IFV, CommandVehicle, Fighter, Helicopter,
    Transport, Drone, Bunker, AmmoDepot, FuelDepot,
    Debris, Destroyer, PatrolBoat, Submarine, SupplyShip,
    SPG, Truck, InfantryKit, BuildingFragment, COUNT_
};

enum class StrategyID : uint8_t {
    A_NoSalvage, B_FixedPercentage, C_DestructionMethodModifier,
    D_ComponentBasedScrap, E_HybridSalvage, COUNT_
};

static constexpr std::string_view kStrategyName[] = {
    "A_NoSalvage", "B_FixedPercentage", "C_DestructionMethodModifier",
    "D_ComponentBasedScrap", "E_HybridSalvage"
};

static constexpr std::string_view kSceneName[] = {
    "S1_TankBattle", "S2_AircraftCrash", "S3_BuildingCollapse",
    "S4_NavalWreck", "S5_MixedBattlefield"
};

// Base recovery fraction per material (ideal conditions)
static constexpr double kBaseRecovery[] = {
    0.85, 0.80, 0.90, 0.75, 0.70, // Steel..Tungsten
    0.50, 0.10, 0.40, 0.30, 0.60, 0.85 // Plastic..DepletedUranium
};

// Scrap value (credits per kg) — higher = more valuable
static constexpr double kScrapValue[] = {
    0.15, 0.80, 2.50, 5.00, 8.00, // Steel..Tungsten
    0.10, 0.05, 0.08, 12.00, 3.00, 15.00 // Plastic..DepletedUranium
};

// Recovery modifier per destruction method
static constexpr double kMethodModifier[] = {
    0.45, 0.25, 0.70, 0.10, 0.85 // Explosion..StructuralFailure
};

// ============================================================================
// RNG — XorShift64 (deterministic, fast, good enough for procedural generation)
// ============================================================================

struct XorShift64 {
    uint64_t state;
    explicit XorShift64(uint64_t seed) : state(seed ? seed : 1) {}
    uint64_t operator()() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }
    double uniform() { return (operator()() >> 11) * (1.0 / 9007199254740992.0); }
    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }
};

// ============================================================================
// Component & EntityWreck
// ============================================================================

struct Component {
    Material material;
    double mass_kg;
};

struct EntityWreck {
    EntityType type;
    DestructionMethod method;
    double total_mass_kg;
    std::vector<Component> components;
};

// ============================================================================
// Salvage result
// ============================================================================

struct SalvageResult {
    double total_scrap_kg;
    double salvageable_mass_kg;
    double total_value_credits;
};

// ============================================================================
// Entity composition templates
// ============================================================================

struct MatFrac { Material mat; double frac; };

struct EntityTemplate {
    std::string_view name;
    double dry_mass_kg;
    std::vector<MatFrac> composition;
};

static const EntityTemplate kEntityTemplates[] = {
    { "MBT", 55000.0, {
        {Material::Steel, 0.55}, {Material::DepletedUranium, 0.10},
        {Material::Composites, 0.08}, {Material::Electronics, 0.05},
        {Material::Aluminum, 0.07}, {Material::Copper, 0.03},
        {Material::Titanium, 0.05}, {Material::Plastic, 0.03},
        {Material::Rubber, 0.03}, {Material::Glass, 0.01}
    }},
    { "IFV", 25000.0, {
        {Material::Steel, 0.50}, {Material::Aluminum, 0.20},
        {Material::Composites, 0.10}, {Material::Electronics, 0.06},
        {Material::Copper, 0.03}, {Material::Plastic, 0.05},
        {Material::Rubber, 0.04}, {Material::Glass, 0.02}
    }},
    { "CommandVehicle", 30000.0, {
        {Material::Steel, 0.40}, {Material::Aluminum, 0.15},
        {Material::Electronics, 0.20}, {Material::Composites, 0.10},
        {Material::Copper, 0.05}, {Material::Titanium, 0.03},
        {Material::Plastic, 0.04}, {Material::Rubber, 0.02},
        {Material::Glass, 0.01}
    }},
    { "Fighter", 12000.0, {
        {Material::Titanium, 0.25}, {Material::Aluminum, 0.35},
        {Material::Composites, 0.20}, {Material::Electronics, 0.10},
        {Material::Steel, 0.05}, {Material::Copper, 0.03},
        {Material::Plastic, 0.02}
    }},
    { "Helicopter", 8000.0, {
        {Material::Aluminum, 0.40}, {Material::Composites, 0.25},
        {Material::Steel, 0.15}, {Material::Electronics, 0.08},
        {Material::Plastic, 0.07}, {Material::Copper, 0.03},
        {Material::Glass, 0.02}
    }},
    { "Transport", 40000.0, {
        {Material::Steel, 0.60}, {Material::Aluminum, 0.20},
        {Material::Plastic, 0.08}, {Material::Rubber, 0.05},
        {Material::Electronics, 0.03}, {Material::Copper, 0.02},
        {Material::Glass, 0.02}
    }},
    { "Drone", 2000.0, {
        {Material::Composites, 0.50}, {Material::Electronics, 0.30},
        {Material::Aluminum, 0.10}, {Material::Plastic, 0.08},
        {Material::Copper, 0.02}
    }},
    { "Bunker", 500000.0, {
        {Material::Steel, 0.35}, {Material::Composites, 0.15},
        {Material::Copper, 0.02}, {Material::Electronics, 0.03},
        {Material::Plastic, 0.05}, {Material::Glass, 0.01},
        {Material::Aluminum, 0.02}, {Material::Titanium, 0.02}
    }},
    { "AmmoDepot", 80000.0, {
        {Material::Steel, 0.60}, {Material::Plastic, 0.15},
        {Material::Aluminum, 0.10}, {Material::Copper, 0.05},
        {Material::Electronics, 0.03}, {Material::Rubber, 0.02}
    }},
    { "FuelDepot", 50000.0, {
        {Material::Steel, 0.55}, {Material::Rubber, 0.20},
        {Material::Plastic, 0.15}, {Material::Aluminum, 0.05},
        {Material::Copper, 0.03}, {Material::Glass, 0.02}
    }},
    { "Debris", 8000.0, {
        {Material::Steel, 0.40}, {Material::Composites, 0.15},
        {Material::Aluminum, 0.15}, {Material::Plastic, 0.12},
        {Material::Copper, 0.08}, {Material::Rubber, 0.05},
        {Material::Glass, 0.03}, {Material::Electronics, 0.02}
    }},
    { "Destroyer", 4000000.0, {
        {Material::Steel, 0.75}, {Material::Aluminum, 0.10},
        {Material::Copper, 0.05}, {Material::Electronics, 0.04},
        {Material::Composites, 0.03}, {Material::Titanium, 0.02},
        {Material::Plastic, 0.01}
    }},
    { "PatrolBoat", 200000.0, {
        {Material::Aluminum, 0.50}, {Material::Steel, 0.25},
        {Material::Composites, 0.12}, {Material::Copper, 0.05},
        {Material::Electronics, 0.04}, {Material::Plastic, 0.03},
        {Material::Glass, 0.01}
    }},
    { "Submarine", 1500000.0, {
        {Material::Steel, 0.50}, {Material::Titanium, 0.25},
        {Material::Copper, 0.08}, {Material::Electronics, 0.07},
        {Material::Composites, 0.05}, {Material::Aluminum, 0.03},
        {Material::Plastic, 0.02}
    }},
    { "SupplyShip", 800000.0, {
        {Material::Steel, 0.65}, {Material::Aluminum, 0.15},
        {Material::Copper, 0.06}, {Material::Electronics, 0.05},
        {Material::Composites, 0.04}, {Material::Plastic, 0.03},
        {Material::Glass, 0.02}
    }},
    { "SPG", 40000.0, {
        {Material::Steel, 0.60}, {Material::Aluminum, 0.12},
        {Material::Copper, 0.05}, {Material::Electronics, 0.06},
        {Material::Composites, 0.08}, {Material::Titanium, 0.03},
        {Material::Plastic, 0.03}, {Material::Rubber, 0.02},
        {Material::Glass, 0.01}
    }},
    { "Truck", 8000.0, {
        {Material::Steel, 0.50}, {Material::Aluminum, 0.20},
        {Material::Plastic, 0.12}, {Material::Rubber, 0.08},
        {Material::Copper, 0.04}, {Material::Glass, 0.03},
        {Material::Electronics, 0.03}
    }},
    { "InfantryKit", 50.0, {
        {Material::Steel, 0.30}, {Material::Plastic, 0.35},
        {Material::Aluminum, 0.15}, {Material::Composites, 0.10},
        {Material::Electronics, 0.05}, {Material::Rubber, 0.05}
    }},
    { "BuildingFragment", 10000.0, {
        {Material::Steel, 0.30}, {Material::Composites, 0.25},
        {Material::Aluminum, 0.15}, {Material::Plastic, 0.10},
        {Material::Copper, 0.08}, {Material::Rubber, 0.05},
        {Material::Glass, 0.04}, {Material::Electronics, 0.03}
    }}
};

// ============================================================================
// Scene generation
// ============================================================================

struct SceneSpec {
    std::vector<std::pair<EntityType, DestructionMethod>> entities;
};

static const SceneSpec kSceneSpecs[] = {
    // S1 — TankBattle
    {{
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Fire},
        {EntityType::Tank, DestructionMethod::Fire},
        {EntityType::Tank, DestructionMethod::StructuralFailure},
        {EntityType::Tank, DestructionMethod::MagicalBeam},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Explosion},
        {EntityType::IFV, DestructionMethod::Explosion},
        {EntityType::IFV, DestructionMethod::Fire},
        {EntityType::IFV, DestructionMethod::StructuralFailure},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::CommandVehicle, DestructionMethod::Kinetic},
        {EntityType::CommandVehicle, DestructionMethod::Explosion},
    }},
    // S2 — AircraftCrash
    {{
        {EntityType::Fighter, DestructionMethod::Kinetic},
        {EntityType::Fighter, DestructionMethod::Kinetic},
        {EntityType::Fighter, DestructionMethod::Fire},
        {EntityType::Fighter, DestructionMethod::StructuralFailure},
        {EntityType::Helicopter, DestructionMethod::Kinetic},
        {EntityType::Helicopter, DestructionMethod::Explosion},
        {EntityType::Helicopter, DestructionMethod::StructuralFailure},
        {EntityType::Transport, DestructionMethod::StructuralFailure},
        {EntityType::Transport, DestructionMethod::Fire},
        {EntityType::Transport, DestructionMethod::Explosion},
        {EntityType::Drone, DestructionMethod::Kinetic},
        {EntityType::Drone, DestructionMethod::Explosion},
    }},
    // S3 — BuildingCollapse
    {{
        {EntityType::Bunker, DestructionMethod::Explosion},
        {EntityType::AmmoDepot, DestructionMethod::Explosion},
        {EntityType::AmmoDepot, DestructionMethod::Fire},
        {EntityType::AmmoDepot, DestructionMethod::StructuralFailure},
        {EntityType::FuelDepot, DestructionMethod::Fire},
        {EntityType::FuelDepot, DestructionMethod::Explosion},
        {EntityType::FuelDepot, DestructionMethod::StructuralFailure},
        {EntityType::FuelDepot, DestructionMethod::Fire},
        {EntityType::FuelDepot, DestructionMethod::Kinetic},
        {EntityType::FuelDepot, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::Explosion},
        {EntityType::Debris, DestructionMethod::Explosion},
        {EntityType::Debris, DestructionMethod::Explosion},
        {EntityType::Debris, DestructionMethod::Fire},
        {EntityType::Debris, DestructionMethod::Fire},
        {EntityType::Debris, DestructionMethod::Kinetic},
        {EntityType::Debris, DestructionMethod::Kinetic},
        {EntityType::Debris, DestructionMethod::MagicalBeam},
        {EntityType::Debris, DestructionMethod::MagicalBeam},
        {EntityType::Debris, DestructionMethod::MagicalBeam},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
        {EntityType::Debris, DestructionMethod::StructuralFailure},
    }},
    // S4 — NavalWreck
    {{
        {EntityType::Destroyer, DestructionMethod::Explosion},
        {EntityType::PatrolBoat, DestructionMethod::Kinetic},
        {EntityType::PatrolBoat, DestructionMethod::Explosion},
        {EntityType::Submarine, DestructionMethod::StructuralFailure},
        {EntityType::SupplyShip, DestructionMethod::Fire},
    }},
    // S5 — MixedBattlefield
    {{
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Fire},
        {EntityType::Tank, DestructionMethod::Fire},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::MagicalBeam},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::StructuralFailure},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Explosion},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::Tank, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Explosion},
        {EntityType::IFV, DestructionMethod::Explosion},
        {EntityType::IFV, DestructionMethod::Fire},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::StructuralFailure},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::IFV, DestructionMethod::Kinetic},
        {EntityType::SPG, DestructionMethod::Kinetic},
        {EntityType::SPG, DestructionMethod::Explosion},
        {EntityType::SPG, DestructionMethod::Explosion},
        {EntityType::SPG, DestructionMethod::Fire},
        {EntityType::SPG, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::Truck, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Explosion},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Fire},
        {EntityType::InfantryKit, DestructionMethod::Fire},
        {EntityType::InfantryKit, DestructionMethod::Fire},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::StructuralFailure},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::InfantryKit, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::Explosion},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::StructuralFailure},
        {EntityType::BuildingFragment, DestructionMethod::Fire},
        {EntityType::BuildingFragment, DestructionMethod::Fire},
        {EntityType::BuildingFragment, DestructionMethod::Fire},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
        {EntityType::BuildingFragment, DestructionMethod::Kinetic},
    }},
};

// ============================================================================
// Wreck generation
// ============================================================================

[[nodiscard]] std::vector<EntityWreck>
GenerateWrecks(const SceneSpec& spec, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<EntityWreck> wrecks;
    wrecks.reserve(spec.entities.size());
    for (auto [et, dm] : spec.entities) {
        const auto& tmpl = kEntityTemplates[static_cast<int>(et)];
        EntityWreck w;
        w.type = et;
        w.method = dm;
        w.total_mass_kg = tmpl.dry_mass_kg * rng.uniform(0.85, 1.0);
        double remaining = w.total_mass_kg;
        w.components.reserve(tmpl.composition.size());
        for (size_t i = 0; i < tmpl.composition.size(); ++i) {
            Component c;
            c.material = tmpl.composition[i].mat;
            double mass = w.total_mass_kg * tmpl.composition[i].frac;
            if (i == tmpl.composition.size() - 1) {
                mass = remaining;
            }
            mass = std::max(mass, 0.001);
            c.mass_kg = mass;
            remaining -= mass;
            w.components.push_back(c);
        }
        wrecks.push_back(std::move(w));
    }
    return wrecks;
}

// ============================================================================
// Salvage strategies
// ============================================================================

// A — NoSalvage: zero scrap (baseline, everything too damaged)
[[nodiscard]] SalvageResult StrategyA(std::span<const EntityWreck> wrecks) {
    SalvageResult r{};
    for (const auto& w : wrecks) {
        for (const auto& c : w.components) {
            r.salvageable_mass_kg += c.mass_kg;
        }
    }
    return r;
}

// B — FixedPercentage: flat 40% scrap recovery regardless of method/material
[[nodiscard]] SalvageResult StrategyB(std::span<const EntityWreck> wrecks) {
    static constexpr double kRecoveryRate = 0.40;
    SalvageResult r{};
    for (const auto& w : wrecks) {
        for (const auto& c : w.components) {
            r.salvageable_mass_kg += c.mass_kg;
            double scrap = c.mass_kg * kRecoveryRate;
            r.total_scrap_kg += scrap;
            r.total_value_credits += scrap * kScrapValue[static_cast<int>(c.material)];
        }
    }
    return r;
}

// C — DestructionMethodModifier: base recovery * method modifier
[[nodiscard]] SalvageResult StrategyC(std::span<const EntityWreck> wrecks) {
    SalvageResult r{};
    for (const auto& w : wrecks) {
        double method_mod = kMethodModifier[static_cast<int>(w.method)];
        for (const auto& c : w.components) {
            int mi = static_cast<int>(c.material);
            double base = kBaseRecovery[mi];
            double recovery = base * method_mod;
            r.salvageable_mass_kg += c.mass_kg;
            double scrap = c.mass_kg * recovery;
            r.total_scrap_kg += scrap;
            r.total_value_credits += scrap * kScrapValue[mi];
        }
    }
    return r;
}

// D — ComponentBasedScrap: per-component scrap value with condition factor
[[nodiscard]] SalvageResult StrategyD(std::span<const EntityWreck> wrecks, uint64_t base_seed) {
    SalvageResult r{};
    uint64_t comp_seed = base_seed;
    for (const auto& w : wrecks) {
        for (const auto& c : w.components) {
            int mi = static_cast<int>(c.material);
            // deterministic condition per component
            XorShift64 cond_rng(comp_seed++);
            double condition = cond_rng.uniform(0.05, 0.95);
            r.salvageable_mass_kg += c.mass_kg;
            double scrap = c.mass_kg * kBaseRecovery[mi] * condition;
            r.total_scrap_kg += scrap;
            r.total_value_credits += scrap * kScrapValue[mi];
        }
    }
    return r;
}

// E — HybridSalvage: combines C + D + salvage_time_decay + team_efficiency
[[nodiscard]] SalvageResult StrategyE(std::span<const EntityWreck> wrecks,
                                       uint64_t base_seed) {
    static constexpr double kTeamEfficiency = 1.15;      // 15% team bonus
    static constexpr double kTimeDecay = 0.70;            // 30% loss from time decay
    static constexpr double kHybridBlendC = 0.40;         // weight for destruction-method approach
    static constexpr double kHybridBlendD = 0.60;         // weight for component-based approach
    SalvageResult r{};
    uint64_t comp_idx = base_seed;
    for (const auto& w : wrecks) {
        double method_mod = kMethodModifier[static_cast<int>(w.method)];
        for (const auto& c : w.components) {
            int mi = static_cast<int>(c.material);
            XorShift64 cond_rng(comp_idx++);
            double condition = cond_rng.uniform(0.05, 0.95);
            // C-component
            double scrap_c = c.mass_kg * kBaseRecovery[mi] * method_mod;
            // D-component
            double scrap_d = c.mass_kg * kBaseRecovery[mi] * condition;
            // Blend
            double scrap = (scrap_c * kHybridBlendC + scrap_d * kHybridBlendD)
                         * kTimeDecay * kTeamEfficiency;
            r.salvageable_mass_kg += c.mass_kg;
            r.total_scrap_kg += scrap;
            r.total_value_credits += scrap * kScrapValue[mi];
        }
    }
    return r;
}

// ============================================================================
// Statistics
// ============================================================================

struct Stats {
    double mean, median, p95, p99, stddev, min, max;
};

[[nodiscard]] Stats ComputeStats(const std::vector<double>& samples) {
    Stats s{};
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    size_t n = sorted.size();
    if (n == 0) return s;
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(n);
    s.median = (n % 2 == 0)
        ? (sorted[n / 2 - 1] + sorted[n / 2]) * 0.5
        : sorted[n / 2];
    s.p95 = sorted[static_cast<size_t>(static_cast<double>(n) * 0.95)];
    s.p99 = sorted[static_cast<size_t>(static_cast<double>(n) * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(n));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// ============================================================================
// Harness
// ============================================================================

static constexpr int kWarmupIter = 10;
static constexpr int kMeasIter  = 1000;
static constexpr uint64_t kSeeds[] = {42, 12345, 98765, 314159, 271828};

// do_not_optimize barrier
template<typename T>
inline void DoNotOptimize(const T& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

void RunBenchmark() {
    using Clock = std::chrono::high_resolution_clock;

    int n_strats  = static_cast<int>(StrategyID::COUNT_);
    int n_scenes  = static_cast<int>(std::size(kSceneSpecs));
    int n_seeds   = static_cast<int>(std::size(kSeeds));

    // Accumulators: [strategy * n_scenes + scene] -> list of times_us from all (seed × iter)
    std::vector<std::vector<double>> agg_times(n_strats * n_scenes);
    std::vector<double> agg_scrap_sum(n_strats * n_scenes, 0.0);
    std::vector<double> agg_value_sum(n_strats * n_scenes, 0.0);
    int agg_count = 0;

    // CSV header
    std::printf("strategy,scene,seed,iter,time_us,scrap_kg,value_credits\n");

    for (int si = 0; si < n_strats; ++si) {
        for (int sci = 0; sci < n_scenes; ++sci) {
            int agg_idx = si * n_scenes + sci;
            agg_times[agg_idx].reserve(n_seeds * kMeasIter);

            for (int sei = 0; sei < n_seeds; ++sei) {
                uint64_t seed = kSeeds[sei];
                auto wrecks = GenerateWrecks(kSceneSpecs[sci], seed);

                auto invoke = [&](StrategyID sid, uint64_t s) -> SalvageResult {
                    switch (sid) {
                    case StrategyID::A_NoSalvage:            return StrategyA(wrecks);
                    case StrategyID::B_FixedPercentage:      return StrategyB(wrecks);
                    case StrategyID::C_DestructionMethodModifier: return StrategyC(wrecks);
                    case StrategyID::D_ComponentBasedScrap:  return StrategyD(wrecks, s);
                    case StrategyID::E_HybridSalvage:        return StrategyE(wrecks, s);
                    default:                                 return SalvageResult{};
                    }
                };

                // Warmup
                for (int w = 0; w < kWarmupIter; ++w) {
                    auto r = invoke(static_cast<StrategyID>(si), seed);
                    DoNotOptimize(r);
                }

                // Measurement
                StrategyID sid = static_cast<StrategyID>(si);
                for (int iter = 0; iter < kMeasIter; ++iter) {
                    auto t0 = Clock::now();
                    auto r = invoke(sid, seed);
                    auto t1 = Clock::now();
                    DoNotOptimize(r);
                    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

                    std::printf("%s,%s,%llu,%d,%.3f,%.3f,%.3f\n",
                        kStrategyName[si].data(), kSceneName[sci].data(),
                        (unsigned long long)seed, iter, us,
                        r.total_scrap_kg, r.total_value_credits);

                    agg_times[agg_idx].push_back(us);
                    agg_scrap_sum[agg_idx] += r.total_scrap_kg;
                    agg_value_sum[agg_idx] += r.total_value_credits;
                    ++agg_count;
                }
            }
        }
    }

    std::fflush(stdout);

    // ----- Summary -----
    std::fprintf(stderr, "\n=== SALVAGE RECYCLING BENCHMARK SUMMARY ===\n");
    std::fprintf(stderr, "Total measurements: %d (warmup %d per config)\n\n",
                 agg_count, kWarmupIter);
    std::fprintf(stderr, "Hardware: AMD Ryzen 7 5800X (Zen3), governor=powersave\n");
    std::fprintf(stderr, "Compiler: Clang 22.1.6, flags: -O3 -march=native -DNDEBUG -std=c++26\n\n");

    // Throughput table (mean us per strategy per scene)
    std::fprintf(stderr, "--- Mean throughput (μs/iter, lower is better) ---\n");
    std::fprintf(stderr, "%-28s", "Strategy");
    for (int sci = 0; sci < n_scenes; ++sci)
        std::fprintf(stderr, " %-18s", kSceneName[sci].data());
    std::fprintf(stderr, " %-10s\n", "Avg");

    for (int si = 0; si < n_strats; ++si) {
        std::fprintf(stderr, "%-28s", kStrategyName[si].data());
        double row_avg = 0.0;
        for (int sci = 0; sci < n_scenes; ++sci) {
            int idx = si * n_scenes + sci;
            Stats st = ComputeStats(agg_times[idx]);
            std::fprintf(stderr, " %-10s %7s",
                (std::to_string(st.mean)).c_str(), "");
            row_avg += st.mean;
        }
        std::fprintf(stderr, " %-10.3f\n", row_avg / n_scenes);
    }

    // Detail per (strategy, scene): mean / median / p95 / p99 / std
    std::fprintf(stderr, "\n--- Detail: mean | median | p95 | p99 | std (all in μs) ---\n");
    std::fprintf(stderr, "%-28s %-18s %-8s %-8s %-8s %-8s %-8s\n",
        "Strategy", "Scene", "Mean", "Median", "P95", "P99", "Std");
    for (int si = 0; si < n_strats; ++si) {
        for (int sci = 0; sci < n_scenes; ++sci) {
            int idx = si * n_scenes + sci;
            Stats st = ComputeStats(agg_times[idx]);
            std::fprintf(stderr, "%-28s %-18s %-8.3f %-8.3f %-8.3f %-8.3f %-8.3f\n",
                kStrategyName[si].data(), kSceneName[sci].data(),
                st.mean, st.median, st.p95, st.p99, st.stddev);
        }
    }

    // Scrap yield summary
    std::fprintf(stderr, "\n--- Mean scrap yield (kg, higher = more salvage) ---\n");
    std::fprintf(stderr, "%-28s", "Strategy");
    for (int sci = 0; sci < n_scenes; ++sci)
        std::fprintf(stderr, " %-18s", kSceneName[sci].data());
    std::fprintf(stderr, " %-10s\n", "Avg");

    for (int si = 0; si < n_strats; ++si) {
        std::fprintf(stderr, "%-28s", kStrategyName[si].data());
        double row_avg = 0.0;
        for (int sci = 0; sci < n_scenes; ++sci) {
            int idx = si * n_scenes + sci;
            double mean_scrap = agg_scrap_sum[idx] / agg_times[idx].size();
            std::fprintf(stderr, " %-18.1f", mean_scrap);
            row_avg += mean_scrap;
        }
        std::fprintf(stderr, " %-10.1f\n", row_avg / n_scenes);
    }

    // Value credits summary
    std::fprintf(stderr, "\n--- Mean salvage value (credits, higher = more valuable) ---\n");
    std::fprintf(stderr, "%-28s", "Strategy");
    for (int sci = 0; sci < n_scenes; ++sci)
        std::fprintf(stderr, " %-18s", kSceneName[sci].data());
    std::fprintf(stderr, " %-10s\n", "Avg");

    for (int si = 0; si < n_strats; ++si) {
        std::fprintf(stderr, "%-28s", kStrategyName[si].data());
        double row_avg = 0.0;
        for (int sci = 0; sci < n_scenes; ++sci) {
            int idx = si * n_scenes + sci;
            double mean_val = agg_value_sum[idx] / agg_times[idx].size();
            std::fprintf(stderr, " %-18.1f", mean_val);
            row_avg += mean_val;
        }
        std::fprintf(stderr, " %-10.1f\n", row_avg / n_scenes);
    }

    std::fprintf(stderr, "\n");
}

int main() {
    RunBenchmark();
    return 0;
}
