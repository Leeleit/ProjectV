#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <random>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// VECTOR AND QUATERNION MATHEMATICS
// ============================================================================

struct Vector3D {
    double x = 0.0, y = 0.0, z = 0.0;
    Vector3D() = default;
    Vector3D(double x, double y, double z) : x(x), y(y), z(z) {}
    Vector3D operator+(const Vector3D& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3D operator-(const Vector3D& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3D operator*(double s) const { return {x * s, y * s, z * s}; }
    Vector3D operator/(double s) const { return {x / s, y / s, z / s}; }
    Vector3D& operator+=(const Vector3D& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3D& operator-=(const Vector3D& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    double dot(const Vector3D& o) const { return x * o.x + y * o.y + z * o.z; }
    Vector3D cross(const Vector3D& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    double length() const { return std::sqrt(dot(*this)); }
    Vector3D normalized() const {
        double len = length();
        return len > 1e-9 ? (*this) / len : Vector3D(0, 0, 0);
    }
};

struct Quaternion {
    double w = 1.0, x = 0.0, y = 0.0, z = 0.0;
    Quaternion() = default;
    Quaternion(double w, double x, double y, double z) : w(w), x(x), y(y), z(z) {}
    Quaternion operator*(const Quaternion& o) const {
        return {
            w * o.w - x * o.x - y * o.y - z * o.z,
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w
        };
    }
    Vector3D rotate(const Vector3D& v) const {
        Quaternion p(0, v.x, v.y, v.z);
        Quaternion conjugate(w, -x, -y, -z);
        Quaternion r = (*this) * p * conjugate;
        return {r.x, r.y, r.z};
    }
    Vector3D inverseRotate(const Vector3D& v) const {
        Quaternion p(0, v.x, v.y, v.z);
        Quaternion conjugate(w, -x, -y, -z);
        Quaternion r = conjugate * p * (*this);
        return {r.x, r.y, r.z};
    }
    Quaternion normalized() const {
        double len = std::sqrt(w * w + x * x + y * y + z * z);
        return len > 1e-9 ? Quaternion(w / len, x / len, y / len, z / len) : Quaternion(1, 0, 0, 0);
    }
    Quaternion getDerivative(const Vector3D& omega) const {
        return (*this) * Quaternion(0, omega.x * 0.5, omega.y * 0.5, omega.z * 0.5);
    }
};

// ============================================================================
// BOUNDING VOLUMES
// ============================================================================

struct OBB {
    Vector3D center;
    Vector3D half_extents;
    Quaternion rotation;
};

struct Sphere {
    Vector3D center;
    double radius = 0.0;
};

// ============================================================================
// AIRCRAFT COMPONENTS
// ============================================================================

enum class ComponentType {
    Fuselage = 0,
    LeftWingInner,
    LeftWingOuter,
    RightWingInner,
    RightWingOuter,
    LeftSparInner,
    LeftSparOuter,
    RightSparInner,
    RightSparOuter,
    LeftAileron,
    RightAileron,
    Elevator,
    Rudder,
    Engine,
    FuelTankLeft,
    FuelTankRight,
    Pilot,
    Hydraulics,
    Count
};

inline std::string GetComponentName(ComponentType type) {
    switch (type) {
        case ComponentType::Fuselage: return "Fuselage";
        case ComponentType::LeftWingInner: return "LeftWingInner";
        case ComponentType::LeftWingOuter: return "LeftWingOuter";
        case ComponentType::RightWingInner: return "RightWingInner";
        case ComponentType::RightWingOuter: return "RightWingOuter";
        case ComponentType::LeftSparInner: return "LeftSparInner";
        case ComponentType::LeftSparOuter: return "LeftSparOuter";
        case ComponentType::RightSparInner: return "RightSparInner";
        case ComponentType::RightSparOuter: return "RightSparOuter";
        case ComponentType::LeftAileron: return "LeftAileron";
        case ComponentType::RightAileron: return "RightAileron";
        case ComponentType::Elevator: return "Elevator";
        case ComponentType::Rudder: return "Rudder";
        case ComponentType::Engine: return "Engine";
        case ComponentType::FuelTankLeft: return "FuelTankLeft";
        case ComponentType::FuelTankRight: return "FuelTankRight";
        case ComponentType::Pilot: return "Pilot";
        case ComponentType::Hydraulics: return "Hydraulics";
        default: return "Unknown";
    }
}

struct Component {
    ComponentType type;
    double health = 1.0;          // 0.0 to 1.0
    double max_health = 100.0;
    double armor = 2.0;           // mm equivalent steel
    double density = 0.1;         // energy attenuation factor per unit depth
    bool flammable = false;
    bool on_fire = false;
    double fire_intensity = 0.0;  // 0.0 to 1.0
    double fuel_mass = 0.0;       // kg
    double leak_rate = 0.0;       // kg/s
    double functional_eff = 1.0;  // 0.0 to 1.0 effectiveness

    OBB obb;
    Sphere sphere;
};

// ============================================================================
// PROJECTILE MODEL
// ============================================================================

enum class ProjectileType {
    AP,   // Armor Piercing
    HE,   // High Explosive
    HEI   // High Explosive Incendiary
};

struct Projectile {
    Vector3D pos;
    Vector3D vel;
    double mass = 0.1; // kg
    double caliber = 0.02; // 20mm
    double kinetic_energy = 40000.0; // Joules
    double penetration_cap = 30.0;   // mm steel equivalent
    double damage = 80.0;            // Base HP damage
    ProjectileType type = ProjectileType::AP;
};

// ============================================================================
// AIRCRAFT STATE
// ============================================================================

struct AircraftState {
    Vector3D pos;
    Vector3D vel;
    Quaternion rot;
    Vector3D omega;
    double mass = 4000.0; // kg
    Vector3D inertia{5000.0, 12000.0, 9000.0}; // kg m^2

    std::array<Component, static_cast<size_t>(ComponentType::Count)> components;
    
    bool left_wing_severed = false;
    bool right_wing_severed = false;
    bool controls_jammed = false;
    bool pilot_killed = false;
    bool stable = true;

    double lift_g_load = 1.0; // current G-load
};

// Derivatives for RK4
struct Derivatives {
    Vector3D d_pos;
    Vector3D d_vel;
    Quaternion d_rot;
    Vector3D d_omega;
};

Derivatives operator*(const Derivatives& d, double s) {
    return { d.d_pos * s, d.d_vel * s, Quaternion(d.d_rot.w * s, d.d_rot.x * s, d.d_rot.y * s, d.d_rot.z * s), d.d_omega * s };
}

Derivatives operator+(const Derivatives& a, const Derivatives& b) {
    return { a.d_pos + b.d_pos, a.d_vel + b.d_vel, Quaternion(a.d_rot.w + b.d_rot.w, a.d_rot.x + b.d_rot.x, a.d_rot.y + b.d_rot.y, a.d_rot.z + b.d_rot.z), a.d_omega + b.d_omega };
}

AircraftState operator+(const AircraftState& s, const Derivatives& d) {
    AircraftState r = s;
    r.pos = s.pos + d.d_pos;
    r.vel = s.vel + d.d_vel;
    r.rot = Quaternion(s.rot.w + d.d_rot.w, s.rot.x + d.d_rot.x, s.rot.y + d.d_rot.y, s.rot.z + d.d_rot.z).normalized();
    r.omega = s.omega + d.d_omega;
    return r;
}

// ============================================================================
// COLLISION DETECTION & INTERSECTIONS
// ============================================================================

bool RayOBBIntersect(const Vector3D& orig, const Vector3D& dir, const OBB& obb, double& t_entry, double& t_exit) {
    Vector3D local_orig = obb.rotation.inverseRotate(orig - obb.center);
    Vector3D local_dir = obb.rotation.inverseRotate(dir);

    double t_min = -1e30;
    double t_max = 1e30;

    double half[3] = { obb.half_extents.x, obb.half_extents.y, obb.half_extents.z };
    double o[3] = { local_orig.x, local_orig.y, local_orig.z };
    double d[3] = { local_dir.x, local_dir.y, local_dir.z };

    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1e-9) {
            if (o[i] < -half[i] || o[i] > half[i]) {
                return false;
            }
        } else {
            double t1 = (-half[i] - o[i]) / d[i];
            double t2 = (half[i] - o[i]) / d[i];

            if (t1 > t2) std::swap(t1, t2);

            t_min = std::max(t_min, t1);
            t_max = std::min(t_max, t2);

            if (t_min > t_max) return false;
        }
    }

    if (t_max < 0.0) return false;

    t_entry = t_min < 0.0 ? 0.0 : t_min;
    t_exit = t_max;
    return true;
}

bool RaySphereIntersect(const Vector3D& orig, const Vector3D& dir, const Sphere& sphere, double& t_entry, double& t_exit) {
    Vector3D oc = orig - sphere.center;
    double a = dir.dot(dir);
    double b = 2.0 * oc.dot(dir);
    double c = oc.dot(oc) - sphere.radius * sphere.radius;
    double discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0) return false;

    double t1 = (-b - std::sqrt(discriminant)) / (2.0 * a);
    double t2 = (-b + std::sqrt(discriminant)) / (2.0 * a);

    if (t1 > t2) std::swap(t1, t2);

    if (t2 < 0.0) return false;

    t_entry = t1 < 0.0 ? 0.0 : t1;
    t_exit = t2;
    return true;
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

Component CreateComponent(ComponentType type, Vector3D center, Vector3D half_extents, double health, double armor, double density, bool flammable) {
    Component c;
    c.type = type;
    c.health = health;
    c.max_health = health * 100.0;
    if (type == ComponentType::Fuselage) c.max_health = 300.0;
    if (type == ComponentType::Engine) c.max_health = 250.0;
    if (type == ComponentType::LeftSparInner || type == ComponentType::RightSparInner) c.max_health = 180.0;
    if (type == ComponentType::Pilot) c.max_health = 20.0;
    
    c.armor = armor;
    c.density = density;
    c.flammable = flammable;
    c.functional_eff = 1.0;

    c.obb.center = center;
    c.obb.half_extents = half_extents;
    c.obb.rotation = Quaternion(1, 0, 0, 0); // default aligned

    c.sphere.center = center;
    c.sphere.radius = half_extents.length(); // bounding sphere

    if (type == ComponentType::FuelTankLeft || type == ComponentType::FuelTankRight) {
        c.fuel_mass = 150.0; // 150 kg fuel
    }

    return c;
}

AircraftState GetInitialAircraftState(int seed) {
    AircraftState s;
    double offset = (seed - 2) * 5.0;
    s.pos = Vector3D(0.0, offset, 2000.0); // 2000m altitude
    s.vel = Vector3D(150.0, 0.0, 0.0);     // 150 m/s forward speed
    s.rot = Quaternion(1, 0, 0, 0);
    s.omega = Vector3D(0, 0, 0);

    // Initialize the 18 components
    s.components[0] = CreateComponent(ComponentType::Fuselage, {0, 0, 0}, {4.0, 0.6, 0.8}, 1.0, 4.0, 0.1, false);
    s.components[1] = CreateComponent(ComponentType::LeftWingInner, {-2.0, 1.5, -0.1}, {2.0, 1.2, 0.15}, 1.0, 2.0, 0.05, false);
    s.components[2] = CreateComponent(ComponentType::LeftWingOuter, {-4.0, 3.5, -0.05}, {2.0, 0.8, 0.1}, 1.0, 1.5, 0.04, false);
    s.components[3] = CreateComponent(ComponentType::RightWingInner, {2.0, 1.5, -0.1}, {2.0, 1.2, 0.15}, 1.0, 2.0, 0.05, false);
    s.components[4] = CreateComponent(ComponentType::RightWingOuter, {4.0, 3.5, -0.05}, {2.0, 0.8, 0.1}, 1.0, 1.5, 0.04, false);
    s.components[5] = CreateComponent(ComponentType::LeftSparInner, {-2.0, 1.5, -0.05}, {2.0, 0.1, 0.1}, 1.0, 6.0, 0.3, false);
    s.components[6] = CreateComponent(ComponentType::LeftSparOuter, {-4.0, 3.5, -0.02}, {2.0, 0.06, 0.06}, 1.0, 4.0, 0.2, false);
    s.components[7] = CreateComponent(ComponentType::RightSparInner, {2.0, 1.5, -0.05}, {2.0, 0.1, 0.1}, 1.0, 6.0, 0.3, false);
    s.components[8] = CreateComponent(ComponentType::RightSparOuter, {4.0, 3.5, -0.02}, {2.0, 0.06, 0.06}, 1.0, 4.0, 0.2, false);
    s.components[9] = CreateComponent(ComponentType::LeftAileron, {-4.0, 4.2, -0.1}, {1.5, 0.2, 0.03}, 1.0, 1.0, 0.02, false);
    s.components[10] = CreateComponent(ComponentType::RightAileron, {4.0, 4.2, -0.1}, {1.5, 0.2, 0.03}, 1.0, 1.0, 0.02, false);
    s.components[11] = CreateComponent(ComponentType::Elevator, {0, -4.5, 0.3}, {1.8, 0.3, 0.03}, 1.0, 1.0, 0.02, false);
    s.components[12] = CreateComponent(ComponentType::Rudder, {0, -4.7, 0.8}, {0.1, 0.3, 0.6}, 1.0, 1.5, 0.03, false);
    s.components[13] = CreateComponent(ComponentType::Engine, {0, 2.5, 0.1}, {0.8, 0.6, 0.6}, 1.0, 8.0, 0.5, true);
    s.components[14] = CreateComponent(ComponentType::FuelTankLeft, {-1.5, 0.8, -0.1}, {0.6, 0.5, 0.12}, 1.0, 3.0, 0.15, true);
    s.components[15] = CreateComponent(ComponentType::FuelTankRight, {1.5, 0.8, -0.1}, {0.6, 0.5, 0.12}, 1.0, 3.0, 0.15, true);
    s.components[16] = CreateComponent(ComponentType::Pilot, {0, 0.5, 0.2}, {0.4, 0.3, 0.4}, 1.0, 1.0, 0.8, false);
    s.components[17] = CreateComponent(ComponentType::Hydraulics, {0, -1.5, -0.2}, {0.1, 1.5, 0.1}, 1.0, 2.0, 0.1, true);

    // Some specific armor plates
    s.components[16].armor = 10.0; // armored pilot seat / windshield

    return s;
}

// ============================================================================
// DAMAGE PROCESSING
// ============================================================================

struct HitRecord {
    int comp_idx;
    double t_entry;
    double t_exit;
};

void ProcessProjectileHit(AircraftState& ac, Projectile& proj, int strategy_idx) {
    Vector3D local_orig = ac.rot.inverseRotate(proj.pos - ac.pos);
    Vector3D local_dir = ac.rot.inverseRotate(proj.vel.normalized());

    std::vector<HitRecord> hits;
    hits.reserve(10);

    for (int i = 0; i < static_cast<int>(ComponentType::Count); ++i) {
        double t_entry = 0.0, t_exit = 0.0;
        bool hit = false;

        if (strategy_idx == 0) { // Strategy A: sphere hitboxes
            hit = RaySphereIntersect(local_orig, local_dir, ac.components[i].sphere, t_entry, t_exit);
        } else { // Strategy B, C, D: OBB hitboxes
            hit = RayOBBIntersect(local_orig, local_dir, ac.components[i].obb, t_entry, t_exit);
        }

        if (hit) {
            hits.push_back({i, t_entry, t_exit});
        }
    }

    if (hits.empty()) return;

    std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
        return a.t_entry < b.t_entry;
    });

    for (const auto& hit : hits) {
        Component& comp = ac.components[hit.comp_idx];

        if (proj.penetration_cap >= comp.armor) {
            double depth = hit.t_exit - hit.t_entry;
            if (depth < 0.01) depth = 0.01;

            double damage_dealt = 0.0;
            if (proj.type == ProjectileType::AP) {
                damage_dealt = proj.damage * (depth / 0.5) * (proj.kinetic_energy / 40000.0);
            } else if (proj.type == ProjectileType::HE) {
                damage_dealt = proj.damage * 2.5;
            } else { // HEI
                damage_dealt = proj.damage * 1.8;
            }

            damage_dealt = std::min(damage_dealt, comp.health * comp.max_health);
            comp.health -= damage_dealt / comp.max_health;
            if (comp.health < 0.0) comp.health = 0.0;

            // Fire ignition logic
            if (strategy_idx >= 2 && comp.flammable && !comp.on_fire && comp.health > 0.0) {
                double ignition_chance = 0.0;
                if (proj.type == ProjectileType::HEI) {
                    ignition_chance = 0.6 * (damage_dealt / comp.max_health);
                } else if (proj.type == ProjectileType::HE) {
                    ignition_chance = 0.3 * (damage_dealt / comp.max_health);
                } else { // AP
                    ignition_chance = 0.05 * (damage_dealt / comp.max_health);
                }

                if (ignition_chance > 0.1) {
                    comp.on_fire = true;
                    comp.fire_intensity = 0.2 + 0.8 * (damage_dealt / comp.max_health);
                }
            }

            if (comp.health < 0.1) {
                comp.health = 0.0;
                comp.functional_eff = 0.0;
                if (comp.type == ComponentType::Pilot) ac.pilot_killed = true;
            } else {
                comp.functional_eff = comp.health;
            }

            double thickness_factor = comp.armor / proj.penetration_cap;
            proj.penetration_cap -= comp.armor + 10.0 * comp.density * depth;
            proj.kinetic_energy *= std::clamp(1.0 - thickness_factor - 0.2 * comp.density * depth, 0.0, 1.0);

            if (proj.type == ProjectileType::HE || proj.type == ProjectileType::HEI) {
                proj.penetration_cap = 0.0;
                proj.kinetic_energy = 0.0;
            }

            if (proj.penetration_cap <= 0.0 || proj.kinetic_energy <= 100.0) {
                break;
            }
        } else {
            break; // stopped by armor
        }
    }
}

// ============================================================================
// CASCADING EFFECTS & SIMULATION TICK
// ============================================================================

void UpdateCascadingEffects(AircraftState& ac, double dt, int strategy_idx) {
    if (ac.pilot_killed) {
        ac.stable = false;
        return;
    }

    // 1. Fuel leak and burning
    for (int i = 0; i < static_cast<int>(ComponentType::Count); ++i) {
        Component& comp = ac.components[i];
        
        if (comp.type == ComponentType::FuelTankLeft || comp.type == ComponentType::FuelTankRight) {
            if (comp.health < 0.9 && comp.fuel_mass > 0.0) {
                comp.leak_rate = (1.0 - comp.health) * 3.0; // max 3 kg/s
                comp.fuel_mass = std::max(0.0, comp.fuel_mass - comp.leak_rate * dt);
            }
            if (comp.fuel_mass <= 0.0) {
                comp.on_fire = false;
                comp.fire_intensity = 0.0;
            }
        }

        // 2. Fire propagation
        if (strategy_idx >= 2 && comp.on_fire) {
            comp.health = std::max(0.0, comp.health - comp.fire_intensity * 0.10 * dt);
            if (comp.health < 0.1) comp.functional_eff = 0.0;

            // Spreading to neighboring components
            if (comp.type == ComponentType::FuelTankLeft) {
                // spreads to left inner spar and wing
                if (ac.components[static_cast<int>(ComponentType::LeftSparInner)].health > 0.0 && !ac.components[static_cast<int>(ComponentType::LeftSparInner)].on_fire) {
                    ac.components[static_cast<int>(ComponentType::LeftSparInner)].on_fire = true;
                    ac.components[static_cast<int>(ComponentType::LeftSparInner)].fire_intensity = comp.fire_intensity * 0.5;
                }
            } else if (comp.type == ComponentType::FuelTankRight) {
                if (ac.components[static_cast<int>(ComponentType::RightSparInner)].health > 0.0 && !ac.components[static_cast<int>(ComponentType::RightSparInner)].on_fire) {
                    ac.components[static_cast<int>(ComponentType::RightSparInner)].on_fire = true;
                    ac.components[static_cast<int>(ComponentType::RightSparInner)].fire_intensity = comp.fire_intensity * 0.5;
                }
            } else if (comp.type == ComponentType::Engine) {
                // spreads to fuselage and hydraulics
                if (!ac.components[static_cast<int>(ComponentType::Fuselage)].on_fire) {
                    ac.components[static_cast<int>(ComponentType::Fuselage)].on_fire = true;
                    ac.components[static_cast<int>(ComponentType::Fuselage)].fire_intensity = 0.3;
                }
                if (!ac.components[static_cast<int>(ComponentType::Hydraulics)].on_fire) {
                    ac.components[static_cast<int>(ComponentType::Hydraulics)].on_fire = true;
                    ac.components[static_cast<int>(ComponentType::Hydraulics)].fire_intensity = 0.5;
                }
            }
        }
    }

    // 3. Control response and hydraulics
    if (ac.components[static_cast<int>(ComponentType::Hydraulics)].functional_eff < 0.2) {
        ac.controls_jammed = true;
    }

    // 4. Structural G-load snapping (Strategy D & E only)
    if (strategy_idx >= 3) {
        double left_spar_health = ac.components[static_cast<int>(ComponentType::LeftSparInner)].functional_eff;
        double right_spar_health = ac.components[static_cast<int>(ComponentType::RightSparInner)].functional_eff;

        // Bending stress in wing spar: stress = current_G_load / spar_health
        double left_stress = left_spar_health > 0.05 ? ac.lift_g_load / left_spar_health : 999.0;
        double right_stress = right_spar_health > 0.05 ? ac.lift_g_load / right_spar_health : 999.0;

        double yield_limit = 9.5; // snaps at 9.5G nominal

        if (left_stress > yield_limit && !ac.left_wing_severed) {
            ac.left_wing_severed = true;
            ac.components[static_cast<int>(ComponentType::LeftWingInner)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::LeftWingOuter)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::LeftSparInner)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::LeftSparOuter)].health = 0.0;
            ac.stable = false; // severe roll torque makes it crash
        }
        if (right_stress > yield_limit && !ac.right_wing_severed) {
            ac.right_wing_severed = true;
            ac.components[static_cast<int>(ComponentType::RightWingInner)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::RightWingOuter)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::RightSparInner)].health = 0.0;
            ac.components[static_cast<int>(ComponentType::RightSparOuter)].health = 0.0;
            ac.stable = false;
        }
    }
}

// Evaluates flight derivatives (RK4 path simulation)
Derivatives EvalFlightDerivatives(const AircraftState& s, double t) {
    Derivatives d;
    d.d_pos = s.vel;

    // Simple aerodynamics: lift, drag, gravity
    double mass = s.mass;
    Vector3D gravity(0.0, 0.0, -9.81 * mass);
    Vector3D thrust(5000.0, 0.0, 0.0); // simple forward thrust

    // If wings severed, lift drops and roll torque explodes
    double lift_coeff = 0.3;
    if (s.left_wing_severed) lift_coeff -= 0.15;
    if (s.right_wing_severed) lift_coeff -= 0.15;

    double dynamic_pressure = 0.5 * 1.225 * s.vel.dot(s.vel);
    Vector3D lift = s.rot.rotate(Vector3D(0.0, 0.0, dynamic_pressure * 30.0 * lift_coeff));
    Vector3D drag = s.vel.normalized() * (-0.5 * 1.225 * s.vel.dot(s.vel) * 30.0 * 0.05);

    Vector3D total_force = lift + drag + gravity + s.rot.rotate(thrust);
    d.d_vel = total_force / mass;

    // Control rotations
    d.d_rot = s.rot.getDerivative(s.omega);

    // Torque
    Vector3D torque(0.0, 0.0, 0.0);
    if (s.left_wing_severed && !s.right_wing_severed) {
        torque.x = -dynamic_pressure * 15.0 * 2.0; // severe rolling moment
    } else if (s.right_wing_severed && !s.left_wing_severed) {
        torque.x = dynamic_pressure * 15.0 * 2.0;
    }

    // PD flight maneuver inputs (if not jammed)
    if (!s.controls_jammed) {
        // pull elevator to execute a loop
        if (t > 1.0 && t < 4.0) {
            torque.y = 12000.0; // nose pitch-up torque
        }
    }

    Vector3D gyro(
        s.omega.y * s.omega.z * (s.inertia.z - s.inertia.y),
        s.omega.z * s.omega.x * (s.inertia.x - s.inertia.z),
        s.omega.x * s.omega.y * (s.inertia.y - s.inertia.x)
    );

    d.d_omega = Vector3D(
        (torque.x - gyro.x) / s.inertia.x,
        (torque.y - gyro.y) / s.inertia.y,
        (torque.z - gyro.z) / s.inertia.z
    );

    return d;
}

AircraftState IntegrateRK4(const AircraftState& s, double dt, double time) {
    Derivatives k1 = EvalFlightDerivatives(s, time);
    Derivatives k2 = EvalFlightDerivatives(s + k1 * (0.5 * dt), time + 0.5 * dt);
    Derivatives k3 = EvalFlightDerivatives(s + k2 * (0.5 * dt), time + 0.5 * dt);
    Derivatives k4 = EvalFlightDerivatives(s + k3 * dt, time + dt);

    Derivatives d = (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (1.0 / 6.0);
    
    AircraftState out = s + d * dt;
    // Update G-load: L = mass * acc.length()
    Vector3D acc = d.d_vel;
    out.lift_g_load = acc.length() / 9.81;
    if (out.lift_g_load < 0.1) out.lift_g_load = 0.1;

    return out;
}

AircraftState IntegrateEuler(const AircraftState& s, double dt, double time) {
    Derivatives d = EvalFlightDerivatives(s, time);
    AircraftState out = s + d * dt;
    out.lift_g_load = d.d_vel.length() / 9.81;
    return out;
}

// ============================================================================
// VECTORIZED (SoA) COMPUTATION FOR STRATEGY E
// ============================================================================

struct ProjectileSoA {
    std::vector<double> px, py, pz;
    std::vector<double> vx, vy, vz;
    std::vector<double> mass;
    std::vector<double> kinetic_energy;
    std::vector<double> penetration_cap;
    std::vector<double> damage;
    std::vector<int> type;
    std::vector<bool> active;

    void resize(size_t n) {
        px.resize(n); py.resize(n); pz.resize(n);
        vx.resize(n); vy.resize(n); vz.resize(n);
        mass.resize(n);
        kinetic_energy.resize(n);
        penetration_cap.resize(n);
        damage.resize(n);
        type.resize(n);
        active.resize(n, true);
    }
};

void ProcessVectorizedHits(std::vector<AircraftState>& aircrafts, ProjectileSoA& projs, int num_projs) {
    #pragma omp simd
    for (int p = 0; p < num_projs; ++p) {
        if (!projs.active[p]) continue;

        Vector3D p_pos(projs.px[p], projs.py[p], projs.pz[p]);
        Vector3D p_vel(projs.vx[p], projs.vy[p], projs.vz[p]);
        Vector3D p_dir = p_vel.normalized();

        for (auto& ac : aircrafts) {
            Vector3D local_orig = ac.rot.inverseRotate(p_pos - ac.pos);
            Vector3D local_dir = ac.rot.inverseRotate(p_dir);

            // Fast boundary check (15m sphere)
            double dist_to_center = (ac.pos - p_pos).length();
            if (dist_to_center > 15.0) continue;

            double min_t_entry = 1e30;
            int best_comp = -1;
            double best_exit = 0.0;

            for (int i = 0; i < static_cast<int>(ComponentType::Count); ++i) {
                double t_entry = 0.0, t_exit = 0.0;
                if (RayOBBIntersect(local_orig, local_dir, ac.components[i].obb, t_entry, t_exit)) {
                    if (t_entry < min_t_entry) {
                        min_t_entry = t_entry;
                        best_comp = i;
                        best_exit = t_exit;
                    }
                }
            }

            if (best_comp != -1) {
                Component& comp = ac.components[best_comp];
                if (projs.penetration_cap[p] >= comp.armor) {
                    double depth = best_exit - min_t_entry;
                    if (depth < 0.01) depth = 0.01;

                    double damage_dealt = projs.damage[p] * std::min(1.0, depth / 0.5);
                    comp.health = std::max(0.0, comp.health - damage_dealt / comp.max_health);
                    comp.functional_eff = comp.health;

                    projs.penetration_cap[p] -= comp.armor + 10.0 * comp.density * depth;
                    if (projs.penetration_cap[p] <= 0.0) {
                        projs.active[p] = false;
                        break; // stopped
                    }
                }
            }
        }
    }
}

// ============================================================================
// MAIN BENCHMARK EXECUTIVE
// ============================================================================

struct BenchResult {
    std::string strategy;
    std::string scenario;
    int tick_rate = 0;
    int seed = 0;
    double avg_step_time_ns = 0.0;
    bool stable = true;
    double left_wing_health = 1.0;
    double engine_health = 1.0;
    bool wing_severed = false;
};

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "ProjectV Aircraft Damage Model Simulation Benchmark" << std::endl;
    std::cout << "Target C++26 standard, Zen 3 hardware profile optimized" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    std::vector<std::string> strategy_names = {
        "A_SpheroidHitboxes_Euler",
        "B_OBBHitboxes_Euler",
        "C_OBBHitboxes_Cascading",
        "D_OBBHitboxes_Cascading_GForce",
        "E_Vectorized_Projectiles"
    };

    std::vector<std::string> scenario_names = {
        "direct_wing_hit",
        "engine_fire_cascade",
        "high_g_maneuver_snapping",
        "hydraulics_failure",
        "pilot_sniping"
    };

    const std::vector<int> tick_rates = {20, 60};
    const int num_seeds = 5;
    const double sim_duration = 10.0; // seconds

    // Warm-up compiler and cache
    for (int i = 0; i < 20; ++i) {
        AircraftState ac = GetInitialAircraftState(0);
        Projectile p;
        p.pos = Vector3D(0, 0, 1999.0);
        p.vel = Vector3D(0, 0, 100.0);
        ProcessProjectileHit(ac, p, 3);
        UpdateCascadingEffects(ac, 0.05, 3);
    }

    std::vector<BenchResult> all_results;

    for (int tick_rate : tick_rates) {
        double dt = 1.0 / static_cast<double>(tick_rate);
        std::cout << "Running benchmarks for tick rate: " << tick_rate << " Hz" << std::endl;

        for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
            std::string strat_name = strategy_names[strat_idx];

            for (int sc = 0; sc < 5; ++sc) {
                std::string sc_name = scenario_names[sc];

                for (int seed = 0; seed < num_seeds; ++seed) {
                    AircraftState ac = GetInitialAircraftState(seed);
                    bool stable = true;

                    std::vector<double> step_times_ns;
                    step_times_ns.reserve(200);

                    if (strat_idx == 4) { // Strategy E: Vectorized projectiles (100 bullets vs 10 aircraft)
                        const int batch_size = 10;
                        const int num_projs = 100;
                        std::vector<AircraftState> batch_ac(batch_size);
                        for (int i = 0; i < batch_size; ++i) {
                            batch_ac[i] = GetInitialAircraftState(seed);
                            batch_ac[i].pos.y += i * 4.0; // separate in space
                        }

                        // Projectiles SoA
                        ProjectileSoA projs;
                        projs.resize(num_projs);
                        std::mt19937 gen(1337 + seed);
                        std::uniform_real_distribution<double> dist_pos(-5.0, 5.0);
                        std::uniform_real_distribution<double> dist_y(-5.0, 45.0);
                        std::uniform_real_distribution<double> dist_vel(-20.0, 20.0);

                        Vector3D start_pos = GetInitialAircraftState(seed).pos + GetInitialAircraftState(seed).vel * 1.0;

                        for (int p = 0; p < num_projs; ++p) {
                            projs.px[p] = start_pos.x + dist_pos(gen);
                            projs.py[p] = start_pos.y + dist_y(gen);
                            projs.pz[p] = start_pos.z + dist_pos(gen);
                            projs.vx[p] = 800.0 + dist_vel(gen); // firing forward
                            projs.vy[p] = dist_vel(gen);
                            projs.vz[p] = dist_vel(gen);
                            projs.mass[p] = 0.1;
                            projs.kinetic_energy[p] = 40000.0;
                            projs.penetration_cap[p] = 30.0;
                            projs.damage[p] = 85.0;
                            projs.type[p] = p % 3; // mix AP, HE, HEI
                        }

                        double t = 0.0;
                        while (t <= sim_duration) {
                            auto start_t = std::chrono::high_resolution_clock::now();
                            
                            // Process projectile hits in batch
                            if (t >= 1.0 && t <= 1.05) {
                                ProcessVectorizedHits(batch_ac, projs, num_projs);
                            }

                            // Cascade and integrate
                            for (int i = 0; i < batch_size; ++i) {
                                UpdateCascadingEffects(batch_ac[i], dt, strat_idx);
                                batch_ac[i] = IntegrateRK4(batch_ac[i], dt, t);
                            }

                            auto end_t = std::chrono::high_resolution_clock::now();
                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            step_times_ns.push_back(ns / batch_size);

                            if (!batch_ac[0].stable) {
                                stable = false;
                            }
                            t += dt;
                        }

                        ac = batch_ac[0]; // report status of the first aircraft in batch

                    } else { // Strategy A, B, C, D (Single aircraft)
                        double t = 0.0;
                        while (t <= sim_duration) {
                            // Inject scenario-specific hits
                            std::vector<Projectile> incident_projectiles;
                            
                            if (sc == 0 && std::abs(t - 1.0) < dt * 0.5) { // direct_wing_hit
                                // 5 projectiles hitting the left wing (inner/outer)
                                for (int i = 0; i < 5; ++i) {
                                    Projectile p;
                                    Vector3D local_hit_pos(-2.0, 1.5 + i * 0.2, 0.0);
                                    p.pos = ac.pos + ac.rot.rotate(local_hit_pos) - ac.rot.rotate(Vector3D(0.0, 0.0, -5.0));
                                    p.vel = ac.rot.rotate(Vector3D(0.0, 0.0, -900.0)); // firing vertically down in local frame
                                    p.type = ProjectileType::AP;
                                    incident_projectiles.push_back(p);
                                }
                            } else if (sc == 1 && std::abs(t - 0.5) < dt * 0.5) { // engine_fire_cascade
                                // 1 incendiary projectile hitting the engine
                                Projectile p;
                                Vector3D local_hit_pos(0.0, 2.5, 0.1);
                                p.pos = ac.pos + ac.rot.rotate(local_hit_pos) - ac.rot.rotate(Vector3D(0.0, -2.0, -4.0));
                                p.vel = ac.rot.rotate(Vector3D(0.0, -400.0, -800.0));
                                p.type = ProjectileType::HEI;
                                incident_projectiles.push_back(p);
                            } else if (sc == 3 && std::abs(t - 1.0) < dt * 0.5) { // hydraulics_failure
                                // bullets hitting hydraulics
                                for (int i = 0; i < 3; ++i) {
                                    Projectile p;
                                    Vector3D local_hit_pos(0.0, -1.5 - i * 0.2, -0.2);
                                    p.pos = ac.pos + ac.rot.rotate(local_hit_pos) - ac.rot.rotate(Vector3D(0.0, 0.0, -5.0));
                                    p.vel = ac.rot.rotate(Vector3D(0.0, 0.0, -900.0));
                                    p.type = ProjectileType::AP;
                                    incident_projectiles.push_back(p);
                                }
                            } else if (sc == 4 && std::abs(t - 1.0) < dt * 0.5) { // pilot_sniping
                                // head-on sniper shot to canopy
                                Projectile p;
                                Vector3D local_hit_pos(0.0, 0.5, 0.2);
                                p.pos = ac.pos + ac.rot.rotate(local_hit_pos) - ac.rot.rotate(Vector3D(0.0, 5.0, 0.0));
                                p.vel = ac.rot.rotate(Vector3D(0.0, -900.0, 0.0)); // firing head-on from front in local frame
                                p.type = ProjectileType::AP;
                                incident_projectiles.push_back(p);
                            }

                            // Scenario 2 (high_g_maneuver_snapping) G-force profile
                            if (sc == 2) {
                                // Undamaged wing snaps at 9.5G, but we inject spar damage to test yield snapping at lower Gs
                                if (seed == 0) { // clean flight
                                    // no initial damage
                                } else { // wing spar pre-damaged (e.g. spar = 0.4)
                                    ac.components[static_cast<int>(ComponentType::LeftSparInner)].health = 0.4;
                                    ac.components[static_cast<int>(ComponentType::LeftSparInner)].functional_eff = 0.4;
                                }
                            }

                            auto start_t = std::chrono::high_resolution_clock::now();

                            // Process projectile damage
                            for (auto& p : incident_projectiles) {
                                ProcessProjectileHit(ac, p, strat_idx);
                            }

                            // Update fire/leak cascades
                            UpdateCascadingEffects(ac, dt, strat_idx);

                            // Trajectory integration
                            if (strat_idx == 3) {
                                ac = IntegrateRK4(ac, dt, t);
                            } else {
                                ac = IntegrateEuler(ac, dt, t);
                            }

                            auto end_t = std::chrono::high_resolution_clock::now();
                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            step_times_ns.push_back(ns);

                            if (!ac.stable) {
                                stable = false;
                                break;
                            }

                            t += dt;
                        }
                    }

                    double sum = 0.0;
                    for (double ns : step_times_ns) sum += ns;
                    double avg_ns = step_times_ns.empty() ? 0.0 : sum / step_times_ns.size();

                    double left_wing_health = ac.components[static_cast<int>(ComponentType::LeftWingInner)].health;
                    double engine_health = ac.components[static_cast<int>(ComponentType::Engine)].health;

                    all_results.push_back({
                        strat_name,
                        sc_name,
                        tick_rate,
                        seed,
                        avg_ns,
                        stable,
                        left_wing_health,
                        engine_health,
                        ac.left_wing_severed
                    });
                }
            }
        }
    }

    // 2. WRITE TO CSV
    std::string csv_path = "results.csv";
    std::cout << "Writing " << all_results.size() << " entries to " << csv_path << "..." << std::endl;
    std::ofstream csv(csv_path);
    csv << "Strategy,Scenario,TickRate,Seed,StepTimeNs,Stability,LeftWingHealth,EngineHealth,WingSevered\n";
    for (const auto& r : all_results) {
        csv << r.strategy << ","
            << r.scenario << ","
            << r.tick_rate << ","
            << r.seed << ","
            << std::fixed << std::setprecision(2) << r.avg_step_time_ns << ","
            << (r.stable ? 1 : 0) << ","
            << std::setprecision(4) << r.left_wing_health << ","
            << r.engine_health << ","
            << (r.wing_severed ? 1 : 0) << "\n";
    }
    csv.close();

    // 3. PRINT SUMMARY STATISTICS
    std::cout << "\n=====================================================================" << std::endl;
    std::cout << "SUMMARY STATISTICS (Averages over all Scenarios and Seeds)" << std::endl;
    std::cout << "=====================================================================" << std::endl;
    std::cout << std::left << std::setw(30) << "Strategy" 
              << std::setw(15) << "TickRate (Hz)" 
              << std::setw(20) << "Mean Step Time (ns)" 
              << std::setw(15) << "Stability (%)" 
              << std::setw(20) << "Wing Snaps" << std::endl;
    std::cout << "---------------------------------------------------------------------" << std::endl;

    for (const auto& strat : strategy_names) {
        for (int tick_rate : tick_rates) {
            double time_sum = 0.0;
            int stable_count = 0;
            int snap_count = 0;
            int total_runs = 0;

            for (const auto& r : all_results) {
                if (r.strategy == strat && r.tick_rate == tick_rate) {
                    time_sum += r.avg_step_time_ns;
                    total_runs++;
                    if (r.stable) {
                        stable_count++;
                    }
                    if (r.wing_severed) {
                        snap_count++;
                    }
                }
            }

            double mean_time = time_sum / total_runs;
            double stab_pct = 100.0 * stable_count / total_runs;

            std::cout << std::left << std::setw(30) << strat 
                      << std::setw(15) << tick_rate 
                      << std::fixed << std::setprecision(1)
                      << std::setw(20) << mean_time 
                      << std::setw(15) << stab_pct 
                      << std::setw(20) << snap_count << std::endl;
        }
    }
    std::cout << "=====================================================================" << std::endl;

    return 0;
}

