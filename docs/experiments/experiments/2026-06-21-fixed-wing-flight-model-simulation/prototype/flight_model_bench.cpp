#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fstream>
#include <random>
#include <array>
#include <string>
#include <algorithm>
#include <iomanip>

// ============================================================================
// 3D MATH LIBRARY
// ============================================================================

struct Vector3D {
    double x = 0, y = 0, z = 0;
    constexpr Vector3D() = default;
    constexpr Vector3D(double x, double y, double z) : x(x), y(y), z(z) {}

    Vector3D operator+(const Vector3D& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3D operator-(const Vector3D& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3D operator*(double s) const { return {x * s, y * s, z * s}; }
    Vector3D operator/(double s) const { return {x / s, y / s, z / s}; }
    
    Vector3D& operator+=(const Vector3D& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3D& operator-=(const Vector3D& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vector3D& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }

    double dot(const Vector3D& o) const { return x * o.x + y * o.y + z * o.z; }
    Vector3D cross(const Vector3D& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vector3D normalized() const {
        double len = length();
        return len > 1e-9 ? *this / len : Vector3D{0, 0, 0};
    }
};

struct Quaternion {
    double w = 1, x = 0, y = 0, z = 0;
    constexpr Quaternion() = default;
    constexpr Quaternion(double w, double x, double y, double z) : w(w), x(x), y(y), z(z) {}

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
    
    Quaternion getDerivative(const Vector3D& omega_body) const {
        return (*this) * Quaternion(0, omega_body.x * 0.5, omega_body.y * 0.5, omega_body.z * 0.5);
    }
};

// ============================================================================
// SIMULATION STRUCTURES
// ============================================================================

struct State {
    Vector3D pos;
    Vector3D vel; // world space
    Quaternion rot;
    Vector3D omega; // body space

    State operator+(const struct Derivatives& d) const;
    State operator*(double factor) const;
};

struct Derivatives {
    Vector3D d_pos;
    Vector3D d_vel;
    Quaternion d_rot;
    Vector3D d_omega;

    Derivatives operator+(const Derivatives& o) const {
        return {
            d_pos + o.d_pos,
            d_vel + o.d_vel,
            Quaternion(d_rot.w + o.d_rot.w, d_rot.x + o.d_rot.x, d_rot.y + o.d_rot.y, d_rot.z + o.d_rot.z),
            d_omega + o.d_omega
        };
    }

    Derivatives operator*(double s) const {
        return {
            d_pos * s,
            d_vel * s,
            Quaternion(d_rot.w * s, d_rot.x * s, d_rot.y * s, d_rot.z * s),
            d_omega * s
        };
    }
};

State State::operator+(const Derivatives& d) const {
    return {
        pos + d.d_pos,
        vel + d.d_vel,
        Quaternion(rot.w + d.d_rot.w, rot.x + d.d_rot.x, rot.y + d.d_rot.y, rot.z + d.d_rot.z).normalized(),
        omega + d.d_omega
    };
}

State State::operator*(double factor) const {
    return {
        pos * factor,
        vel * factor,
        Quaternion(rot.w * factor, rot.x * factor, rot.y * factor, rot.z * factor),
        omega * factor
    };
}

struct Controls {
    double throttle = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double yaw = 0.0;
    bool afterburner = false;
};

// ============================================================================
// WING SEGMENT PARAMETERS
// ============================================================================

struct WingSegment {
    Vector3D r;    // position in body space
    Vector3D n;    // normal vector in body space
    Vector3D c;    // chord vector in body space
    Vector3D s;    // span vector in body space
    double area = 0.0;
};

struct AircraftConfig {
    double mass = 10000.0; // kg
    Vector3D inertia{10000.0, 50000.0, 60000.0}; // kg m^2
    double max_thrust = 80000.0; // N (F-16 class)
    std::array<WingSegment, 4> wings;
};

AircraftConfig GetF16Config() {
    AircraftConfig cfg;
    cfg.mass = 9000.0;
    cfg.inertia = Vector3D(9500.0, 55000.0, 63000.0);
    cfg.max_thrust = 76000.0; // dry thrust (afterburner adds 1.5x)
    
    // Left Wing
    cfg.wings[0] = {
        Vector3D(0.2, -2.5, 0.0), // center
        Vector3D(0.0, 0.0, 1.0),  // normal
        Vector3D(1.0, 0.0, 0.0),  // chord
        Vector3D(0.0, -1.0, 0.0), // span
        13.0                      // area m^2
    };
    // Right Wing
    cfg.wings[1] = {
        Vector3D(0.2, 2.5, 0.0),
        Vector3D(0.0, 0.0, 1.0),
        Vector3D(1.0, 0.0, 0.0),
        Vector3D(0.0, 1.0, 0.0),
        13.0
    };
    // Horizontal Stabilizer (Elevator)
    cfg.wings[2] = {
        Vector3D(-5.5, 0.0, 0.0),
        Vector3D(0.0, 0.0, 1.0),
        Vector3D(1.0, 0.0, 0.0),
        Vector3D(0.0, 1.0, 0.0),
        5.5
    };
    // Vertical Stabilizer (Rudder)
    cfg.wings[3] = {
        Vector3D(-5.5, 0.0, 1.2),
        Vector3D(0.0, 1.0, 0.0),  // normal facing right
        Vector3D(1.0, 0.0, 0.0),
        Vector3D(0.0, 0.0, 1.0),
        3.5
    };
    return cfg;
}

// ============================================================================
// PHYSICAL CALCULATIONS
// ============================================================================

double GetAirDensity(double altitude) {
    // Standard atmosphere model approximation
    if (altitude < 0.0) altitude = 0.0;
    return 1.225 * std::exp(-altitude / 8500.0);
}

// Calculate aerodynamic force and torque for a single segment
void CalcSegmentAero(const WingSegment& wing, int wing_idx, const State& s, const Controls& ctrl, 
                     const Vector3D& wind_body, double density, Vector3D& total_force_body, Vector3D& total_torque_body) 
{
    // local wing velocity including rotational speed
    Vector3D local_vel = s.rot.inverseRotate(s.vel) + s.omega.cross(wing.r) - wind_body;
    double speed = local_vel.length();
    if (speed < 1e-4) return;

    // project velocity into chord-normal plane
    double vc = local_vel.dot(wing.c);
    double vn = local_vel.dot(wing.n);
    double vcn = std::sqrt(vc * vc + vn * vn);
    if (vcn < 1e-4) return;

    // local angle of attack
    double alpha = std::atan2(-vn, vc);

    // lift coefficient based on angle of attack
    double cl = 2.0 * std::sin(2.0 * alpha) * std::cos(alpha);

    // Apply control surfaces
    if (wing_idx == 0) { // Left wing: affected by roll
        cl += 0.3 * ctrl.roll;
    } else if (wing_idx == 1) { // Right wing: affected by roll
        cl -= 0.3 * ctrl.roll;
    } else if (wing_idx == 2) { // Elevator
        cl += 0.6 * ctrl.pitch;
    } else if (wing_idx == 3) { // Rudder
        cl += 0.6 * ctrl.yaw;
    }

    // drag coefficient: parasitic + induced + high-alpha drag
    double cd = 0.02 + 0.05 * cl * cl + (1.0 - std::cos(alpha));

    // transonic drag rise (compressibility)
    double mach = speed / 340.0;
    if (mach > 0.7) {
        cd += 0.4 * (mach - 0.7) * (mach - 0.7);
    }

    // lift and drag forces
    double q = 0.5 * density * vcn * vcn;
    double lift_mag = q * wing.area * cl;
    double drag_mag = 0.5 * density * speed * speed * wing.area * cd;

    // lift direction: perpendicular to velocity in chord-normal plane, pointing towards positive normal
    Vector3D lift_dir = (wing.c * (-vn) + wing.n * vc) / vcn;
    Vector3D drag_dir = (local_vel * -1.0) / speed;

    Vector3D force = lift_dir * lift_mag + drag_dir * drag_mag;
    Vector3D torque = wing.r.cross(force);

    total_force_body += force;
    total_torque_body += torque;
}

// 6-DOF physics derivative evaluation (4-segment model)
Derivatives EvalDerivatives4Section(const State& s, const Controls& ctrl, const Vector3D& wind_world, const AircraftConfig& cfg) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D wind_body = s.rot.inverseRotate(wind_world);

    Vector3D aero_force_body(0, 0, 0);
    Vector3D aero_torque_body(0, 0, 0);

    for (int i = 0; i < 4; ++i) {
        CalcSegmentAero(cfg.wings[i], i, s, ctrl, wind_body, density, aero_force_body, aero_torque_body);
    }

    // Thrust
    double thrust_mag = cfg.max_thrust * ctrl.throttle;
    if (ctrl.afterburner && ctrl.throttle > 0.9) {
        thrust_mag *= 1.5;
    }
    Vector3D thrust_body(thrust_mag, 0, 0);

    // Gravity
    Vector3D gravity_world(0, 0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    // Total forces & torques
    Vector3D total_force_body = aero_force_body + thrust_body + gravity_body;
    Vector3D total_torque_body = aero_torque_body;

    // Convert acceleration to world space
    d.d_vel = s.rot.rotate(total_force_body / cfg.mass);

    // Angular kinematics
    d.d_rot = s.rot.getDerivative(s.omega);

    Vector3D gyro(
        s.omega.y * s.omega.z * (cfg.inertia.z - cfg.inertia.y),
        s.omega.z * s.omega.x * (cfg.inertia.x - cfg.inertia.z),
        s.omega.x * s.omega.y * (cfg.inertia.y - cfg.inertia.x)
    );

    d.d_omega = Vector3D(
        (total_torque_body.x - gyro.x) / cfg.inertia.x,
        (total_torque_body.y - gyro.y) / cfg.inertia.y,
        (total_torque_body.z - gyro.z) / cfg.inertia.z
    );

    return d;
}

// Strategy A: Euler 1-Section (Coarse Point Aerodynamics)
Derivatives EvalDerivatives1Section(const State& s, const Controls& ctrl, const Vector3D& wind_world, const AircraftConfig& cfg) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D vel_body = s.rot.inverseRotate(s.vel) - s.rot.inverseRotate(wind_world);
    double speed = vel_body.length();

    Vector3D aero_force_body(0, 0, 0);
    Vector3D aero_torque_body(0, 0, 0);

    if (speed > 1e-3) {
        double alpha = std::atan2(-vel_body.z, vel_body.x);
        double beta = std::atan2(vel_body.y, vel_body.x);

        // Single wing approximation coefficients
        double cl = 2.0 * std::sin(2.0 * alpha) * std::cos(alpha);
        double cd = 0.02 + 0.05 * cl * cl + (1.0 - std::cos(alpha));
        
        double mach = speed / 340.0;
        if (mach > 0.7) {
            cd += 0.4 * (mach - 0.7) * (mach - 0.7);
        }

        double q = 0.5 * density * speed * speed;
        double lift = q * 31.5 * cl; // total lift area sum
        double drag = q * 31.5 * cd;

        aero_force_body.x = -drag * std::cos(alpha) + lift * std::sin(alpha);
        aero_force_body.z = -drag * std::sin(alpha) + lift * std::cos(alpha);
        
        // sideslip restoring force
        aero_force_body.y = -q * 3.5 * 2.0 * beta;

        // Control torques + damping
        aero_torque_body.x = q * 13.0 * 2.5 * (0.3 * ctrl.roll) - q * 0.1 * s.omega.x;
        aero_torque_body.y = q * 5.5 * (-5.5) * (0.6 * ctrl.pitch) - q * 0.2 * s.omega.y;
        aero_torque_body.z = q * 3.5 * (-5.5) * (0.6 * ctrl.yaw) - q * 0.15 * s.omega.z;
    }

    double thrust_mag = cfg.max_thrust * ctrl.throttle;
    if (ctrl.afterburner && ctrl.throttle > 0.9) {
        thrust_mag *= 1.5;
    }
    Vector3D thrust_body(thrust_mag, 0, 0);

    Vector3D gravity_world(0, 0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    Vector3D total_force_body = aero_force_body + thrust_body + gravity_body;
    d.d_vel = s.rot.rotate(total_force_body / cfg.mass);
    d.d_rot = s.rot.getDerivative(s.omega);

    Vector3D gyro(
        s.omega.y * s.omega.z * (cfg.inertia.z - cfg.inertia.y),
        s.omega.z * s.omega.x * (cfg.inertia.x - cfg.inertia.z),
        s.omega.x * s.omega.y * (cfg.inertia.y - cfg.inertia.x)
    );
    d.d_omega = Vector3D(
        (aero_torque_body.x - gyro.x) / cfg.inertia.x,
        (aero_torque_body.y - gyro.y) / cfg.inertia.y,
        (aero_torque_body.z - gyro.z) / cfg.inertia.z
    );

    return d;
}

// Strategy D: Analytical LOD lookup
Derivatives EvalDerivativesAnalyticalLOD(const State& s, const Controls& ctrl, const Vector3D& wind_world, const AircraftConfig& cfg) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D vel_body = s.rot.inverseRotate(s.vel) - s.rot.inverseRotate(wind_world);
    double speed = vel_body.length();

    Vector3D aero_force_body(0, 0, 0);
    Vector3D aero_torque_body(0, 0, 0);

    if (speed > 1e-3) {
        double alpha = std::atan2(-vel_body.z, vel_body.x);
        double beta = std::atan2(vel_body.y, vel_body.x);

        // Fully precomputed analytical coefficients
        double cl = 4.0 * alpha; // simple linear lift
        double cd = 0.03 + 0.08 * alpha * alpha; // simple drag polar

        double mach = speed / 340.0;
        if (mach > 0.7) {
            cd += 0.5 * (mach - 0.7);
        }

        double q = 0.5 * density * speed * speed;
        double lift = q * 26.0 * cl;
        double drag = q * 26.0 * cd;

        aero_force_body = Vector3D(-drag, -q * 3.5 * beta, lift);

        // Damped control torques
        aero_torque_body = Vector3D(
            q * 10.0 * ctrl.roll - q * 0.05 * s.omega.x,
            q * (-25.0) * ctrl.pitch - q * 0.1 * s.omega.y,
            q * (-15.0) * ctrl.yaw - q * 0.08 * s.omega.z
        );
    }

    double thrust_mag = cfg.max_thrust * ctrl.throttle;
    if (ctrl.afterburner && ctrl.throttle > 0.9) thrust_mag *= 1.5;
    Vector3D thrust_body(thrust_mag, 0, 0);

    Vector3D gravity_world(0, 0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    d.d_vel = s.rot.rotate((aero_force_body + thrust_body + gravity_body) / cfg.mass);
    d.d_rot = s.rot.getDerivative(s.omega);
    
    d.d_omega = Vector3D(
        aero_torque_body.x / cfg.inertia.x,
        aero_torque_body.y / cfg.inertia.y,
        aero_torque_body.z / cfg.inertia.z
    );

    return d;
}

// ============================================================================
// INTEGRATION ALGORITHMS
// ============================================================================

State IntegrateEuler(const State& s, const Controls& ctrl, const Vector3D& wind, const AircraftConfig& cfg, double dt, int model_type) {
    Derivatives d;
    if (model_type == 0) {
        d = EvalDerivatives1Section(s, ctrl, wind, cfg);
    } else if (model_type == 1) {
        d = EvalDerivatives4Section(s, ctrl, wind, cfg);
    } else {
        d = EvalDerivativesAnalyticalLOD(s, ctrl, wind, cfg);
    }
    return s + d * dt;
}

State IntegrateRK4(const State& s, const Controls& ctrl, const Vector3D& wind, const AircraftConfig& cfg, double dt, int model_type) {
    auto eval = [&](const State& st) {
        if (model_type == 0) return EvalDerivatives1Section(st, ctrl, wind, cfg);
        if (model_type == 1) return EvalDerivatives4Section(st, ctrl, wind, cfg);
        return EvalDerivativesAnalyticalLOD(st, ctrl, wind, cfg);
    };

    Derivatives k1 = eval(s);
    Derivatives k2 = eval(s + k1 * (0.5 * dt));
    Derivatives k3 = eval(s + k2 * (0.5 * dt));
    Derivatives k4 = eval(s + k3 * dt);

    Derivatives d = (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (1.0 / 6.0);
    return s + d * dt;
}

// ============================================================================
// VECTORIZED (SoA) SIMULATION FOR STRATEGY E
// ============================================================================

struct StateSoA {
    std::vector<double> px, py, pz;
    std::vector<double> vx, vy, vz;
    std::vector<double> rw, rx, ry, rz;
    std::vector<double> ox, oy, oz;

    void resize(size_t n) {
        px.resize(n); py.resize(n); pz.resize(n);
        vx.resize(n); vy.resize(n); vz.resize(n);
        rw.resize(n, 1.0); rx.resize(n, 0.0); ry.resize(n, 0.0); rz.resize(n, 0.0);
        ox.resize(n); oy.resize(n); oz.resize(n);
    }
};

void IntegrateRK4_BatchSoA(StateSoA& soa, const std::vector<Controls>& ctrls, const Vector3D& wind_world, const AircraftConfig& cfg, double dt) {
    size_t n = soa.px.size();
    
    // Save initial state
    std::vector<double> s_px = soa.px, s_py = soa.py, s_pz = soa.pz;
    std::vector<double> s_vx = soa.vx, s_vy = soa.vy, s_vz = soa.vz;
    std::vector<double> s_rw = soa.rw, s_rx = soa.rx, s_ry = soa.ry, s_rz = soa.rz;
    std::vector<double> s_ox = soa.ox, s_oy = soa.oy, s_oz = soa.oz;

    // Accumulators for weighted derivatives
    std::vector<double> acc_px(n, 0.0), acc_py(n, 0.0), acc_pz(n, 0.0);
    std::vector<double> acc_vx(n, 0.0), acc_vy(n, 0.0), acc_vz(n, 0.0);
    std::vector<double> acc_rw(n, 0.0), acc_rx(n, 0.0), acc_ry(n, 0.0), acc_rz(n, 0.0);
    std::vector<double> acc_ox(n, 0.0), acc_oy(n, 0.0), acc_oz(n, 0.0);

    // Stage derivatives
    std::vector<double> k_px(n), k_py(n), k_pz(n);
    std::vector<double> k_vx(n), k_vy(n), k_vz(n);
    std::vector<double> k_rw(n), k_rx(n), k_ry(n), k_rz(n);
    std::vector<double> k_ox(n), k_oy(n), k_oz(n);

    // Temporal states
    std::vector<double> t_px(n), t_py(n), t_pz(n);
    std::vector<double> t_vx(n), t_vy(n), t_vz(n);
    std::vector<double> t_rw(n), t_rx(n), t_ry(n), t_rz(n);
    std::vector<double> t_ox(n), t_oy(n), t_oz(n);

    // Loop through 4 RK4 stages
    for (int stage = 0; stage < 4; ++stage) {
        double stage_dt = (stage == 0) ? 0.0 : ((stage == 3) ? dt : 0.5 * dt);
        double weight = (stage == 0 || stage == 3) ? 1.0 / 6.0 : 2.0 / 6.0;

        // Set up evaluation state
        if (stage == 0) {
            t_px = s_px; t_py = s_py; t_pz = s_pz;
            t_vx = s_vx; t_vy = s_vy; t_vz = s_vz;
            t_rw = s_rw; t_rx = s_rx; t_ry = s_ry; t_rz = s_rz;
            t_ox = s_ox; t_oy = s_oy; t_oz = s_oz;
        } else {
            for (size_t i = 0; i < n; ++i) {
                t_px[i] = s_px[i] + k_px[i] * stage_dt;
                t_py[i] = s_py[i] + k_py[i] * stage_dt;
                t_pz[i] = s_pz[i] + k_pz[i] * stage_dt;
                t_vx[i] = s_vx[i] + k_vx[i] * stage_dt;
                t_vy[i] = s_vy[i] + k_vy[i] * stage_dt;
                t_vz[i] = s_vz[i] + k_vz[i] * stage_dt;
                
                double rw_tmp = s_rw[i] + k_rw[i] * stage_dt;
                double rx_tmp = s_rx[i] + k_rx[i] * stage_dt;
                double ry_tmp = s_ry[i] + k_ry[i] * stage_dt;
                double rz_tmp = s_rz[i] + k_rz[i] * stage_dt;
                double len = std::sqrt(rw_tmp*rw_tmp + rx_tmp*rx_tmp + ry_tmp*ry_tmp + rz_tmp*rz_tmp);
                if (len > 1e-9) {
                    t_rw[i] = rw_tmp / len; t_rx[i] = rx_tmp / len; t_ry[i] = ry_tmp / len; t_rz[i] = rz_tmp / len;
                } else {
                    t_rw[i] = 1.0; t_rx[i] = 0.0; t_ry[i] = 0.0; t_rz[i] = 0.0;
                }
                
                t_ox[i] = s_ox[i] + k_ox[i] * stage_dt;
                t_oy[i] = s_oy[i] + k_oy[i] * stage_dt;
                t_oz[i] = s_oz[i] + k_oz[i] * stage_dt;
            }
        }

        // Calculate derivatives
        #pragma omp simd
        for (size_t i = 0; i < n; ++i) {
            Vector3D pos(t_px[i], t_py[i], t_pz[i]);
            Vector3D vel(t_vx[i], t_vy[i], t_vz[i]);
            Quaternion rot(t_rw[i], t_rx[i], t_ry[i], t_rz[i]);
            Vector3D omega(t_ox[i], t_oy[i], t_oz[i]);
            const Controls& ctrl = ctrls[i];

            double density = GetAirDensity(pos.z);
            Vector3D wind_body = rot.inverseRotate(wind_world);

            Vector3D aero_force_body(0, 0, 0);
            Vector3D aero_torque_body(0, 0, 0);

            for (int w = 0; w < 4; ++w) {
                const WingSegment& wing = cfg.wings[w];
                Vector3D local_vel = rot.inverseRotate(vel) + omega.cross(wing.r) - wind_body;
                double speed = local_vel.length();
                if (speed >= 1e-4) {
                    double vc = local_vel.dot(wing.c);
                    double vn = local_vel.dot(wing.n);
                    double vcn = std::sqrt(vc * vc + vn * vn);
                    if (vcn >= 1e-4) {
                        double alpha = std::atan2(-vn, vc);
                        double cl = 2.0 * std::sin(2.0 * alpha) * std::cos(alpha);
                        if (w == 0) cl += 0.3 * ctrl.roll;
                        else if (w == 1) cl -= 0.3 * ctrl.roll;
                        else if (w == 2) cl += 0.6 * ctrl.pitch;
                        else if (w == 3) cl += 0.6 * ctrl.yaw;

                        double cd = 0.02 + 0.05 * cl * cl + (1.0 - std::cos(alpha));
                        double mach = speed / 340.0;
                        if (mach > 0.7) cd += 0.4 * (mach - 0.7) * (mach - 0.7);

                        double q = 0.5 * density * vcn * vcn;
                        double lift_mag = q * wing.area * cl;
                        double drag_mag = 0.5 * density * speed * speed * wing.area * cd;

                        Vector3D lift_dir = (wing.c * (-vn) + wing.n * vc) / vcn;
                        Vector3D drag_dir = (local_vel * -1.0) / speed;

                        Vector3D force = lift_dir * lift_mag + drag_dir * drag_mag;
                        aero_force_body += force;
                        aero_torque_body += wing.r.cross(force);
                    }
                }
            }

            double thrust_mag = cfg.max_thrust * ctrl.throttle;
            if (ctrl.afterburner && ctrl.throttle > 0.9) thrust_mag *= 1.5;
            Vector3D thrust_body(thrust_mag, 0, 0);

            Vector3D gravity_world(0, 0, -9.81 * cfg.mass);
            Vector3D gravity_body = rot.inverseRotate(gravity_world);

            Vector3D total_force_body = aero_force_body + thrust_body + gravity_body;
            Vector3D accel_world = rot.rotate(total_force_body / cfg.mass);

            Quaternion d_rot = rot.getDerivative(omega);

            Vector3D gyro(
                omega.y * omega.z * (cfg.inertia.z - cfg.inertia.y),
                omega.z * omega.x * (cfg.inertia.x - cfg.inertia.z),
                omega.x * omega.y * (cfg.inertia.y - cfg.inertia.x)
            );
            Vector3D alpha_body(
                (aero_torque_body.x - gyro.x) / cfg.inertia.x,
                (aero_torque_body.y - gyro.y) / cfg.inertia.y,
                (aero_torque_body.z - gyro.z) / cfg.inertia.z
            );

            // Store derivatives for next stage evaluation
            k_px[i] = vel.x; k_py[i] = vel.y; k_pz[i] = vel.z;
            k_vx[i] = accel_world.x; k_vy[i] = accel_world.y; k_vz[i] = accel_world.z;
            k_rw[i] = d_rot.w; k_rx[i] = d_rot.x; k_ry[i] = d_rot.y; k_rz[i] = d_rot.z;
            k_ox[i] = alpha_body.x; k_oy[i] = alpha_body.y; k_oz[i] = alpha_body.z;

            // Accumulate RK4 weighted derivatives
            acc_px[i] += vel.x * weight;
            acc_py[i] += vel.y * weight;
            acc_pz[i] += vel.z * weight;
            acc_vx[i] += accel_world.x * weight;
            acc_vy[i] += accel_world.y * weight;
            acc_vz[i] += accel_world.z * weight;
            acc_rw[i] += d_rot.w * weight;
            acc_rx[i] += d_rot.x * weight;
            acc_ry[i] += d_rot.y * weight;
            acc_rz[i] += d_rot.z * weight;
            acc_ox[i] += alpha_body.x * weight;
            acc_oy[i] += alpha_body.y * weight;
            acc_oz[i] += alpha_body.z * weight;
        }
    }

    // Apply final accumulated updates
    for (size_t i = 0; i < n; ++i) {
        soa.px[i] = s_px[i] + acc_px[i] * dt;
        soa.py[i] = s_py[i] + acc_py[i] * dt;
        soa.pz[i] = s_pz[i] + acc_pz[i] * dt;
        soa.vx[i] = s_vx[i] + acc_vx[i] * dt;
        soa.vy[i] = s_vy[i] + acc_vy[i] * dt;
        soa.vz[i] = s_vz[i] + acc_vz[i] * dt;
        
        double rw_tmp = s_rw[i] + acc_rw[i] * dt;
        double rx_tmp = s_rx[i] + acc_rx[i] * dt;
        double ry_tmp = s_ry[i] + acc_ry[i] * dt;
        double rz_tmp = s_rz[i] + acc_rz[i] * dt;
        double len = std::sqrt(rw_tmp*rw_tmp + rx_tmp*rx_tmp + ry_tmp*ry_tmp + rz_tmp*rz_tmp);
        if (len > 1e-9) {
            soa.rw[i] = rw_tmp / len; soa.rx[i] = rx_tmp / len; soa.ry[i] = ry_tmp / len; soa.rz[i] = rz_tmp / len;
        } else {
            soa.rw[i] = 1.0; soa.rx[i] = 0.0; soa.ry[i] = 0.0; soa.rz[i] = 0.0;
        }
        
        soa.ox[i] = s_ox[i] + acc_ox[i] * dt;
        soa.oy[i] = s_oy[i] + acc_oy[i] * dt;
        soa.oz[i] = s_oz[i] + acc_oz[i] * dt;
    }
}

// ============================================================================
// SCENARIO SYSTEMS & PILOT CONTROLS
// ============================================================================

Controls UpdateControls(int scenario, double sim_time, const State& s, int seed) {
    Controls ctrl;
    ctrl.throttle = 0.55;
    ctrl.pitch = 0.0;
    ctrl.roll = 0.0;
    ctrl.yaw = 0.0;
    ctrl.afterburner = false;

    // Add minor variation based on seed
    double seed_offset = (seed - 2) * 0.05;

    switch (scenario) {
        case 0: // level_flight
            ctrl.throttle = 0.54 + seed_offset * 0.1;
            break;
        case 1: // high_g_turn
            ctrl.throttle = 0.8;
            ctrl.roll = 1.0; // max banking
            if (sim_time > 1.0) {
                ctrl.pitch = 0.9; // pull up hard in the turn
            }
            break;
        case 2: // stall_recovery
            if (sim_time < 8.0) {
                ctrl.throttle = 0.0; // cut engine
                ctrl.pitch = 0.7;    // pull up to induce stall
            } else {
                ctrl.throttle = 1.0; // full throttle
                ctrl.afterburner = true;
                ctrl.pitch = -1.0;   // push nose down to gain speed
            }
            break;
        case 3: // mach_dash
            ctrl.throttle = 1.0;
            ctrl.afterburner = true; // force supersonic flight
            break;
        case 4: // turbulence_handling (PD flight stability autopilot)
            {
                double target_alt = 5000.0 + seed_offset * 100.0;
                double target_roll = 0.0;

                // PD altitude controller -> pitch
                double alt_err = target_alt - s.pos.z;
                ctrl.pitch = 0.005 * alt_err - 0.02 * s.vel.z;
                ctrl.pitch = std::clamp(ctrl.pitch, -0.5, 0.5);

                // PD roll controller -> roll
                // Calculate current roll angle from quaternion
                // roll angle approximation: 2 * (w*x + y*z)
                double roll_approx = 2.0 * (s.rot.w * s.rot.x + s.rot.y * s.rot.z);
                double roll_err = target_roll - roll_approx;
                ctrl.roll = 0.8 * roll_err - 0.2 * s.omega.x;
                ctrl.roll = std::clamp(ctrl.roll, -0.6, 0.6);

                ctrl.throttle = 0.6;
            }
            break;
    }
    return ctrl;
}

Vector3D GetWindField(int scenario, double sim_time, int seed) {
    if (scenario != 4) return {0, 0, 0};
    
    // Simulating wind gusts using sine combinations
    double freq1 = 0.5 + seed * 0.1;
    double freq2 = 1.2 - seed * 0.05;
    
    double wind_x = 10.0 * std::sin(freq1 * sim_time);
    double wind_y = 15.0 * std::cos(freq2 * sim_time + 0.5);
    double wind_z = 5.0 * std::sin((freq1 + freq2) * sim_time);
    
    return {wind_x, wind_y, wind_z};
}

State GetInitialState(int scenario, int seed) {
    State s;
    double offset = (seed - 2) * 5.0; // slight spatial perturbation
    s.pos = Vector3D(offset, 0.0, 5000.0);
    s.rot = Quaternion(1, 0, 0, 0);
    s.omega = Vector3D(0, 0, 0);

    switch (scenario) {
        case 0: // level_flight
            s.vel = Vector3D(100.0, 0.0, 0.0);
            break;
        case 1: // high_g_turn
            s.vel = Vector3D(150.0, 0.0, 0.0);
            break;
        case 2: // stall_recovery
            s.vel = Vector3D(50.0, 0.0, 0.0); // slow start
            s.rot = Quaternion(0.92388, 0, 0.38268, 0); // pitched up 45 degrees
            break;
        case 3: // mach_dash
            s.vel = Vector3D(200.0, 0.0, 0.0);
            break;
        case 4: // turbulence_handling
            s.vel = Vector3D(100.0, 0.0, 0.0);
            break;
    }
    return s;
}

// ============================================================================
// RUN SCENARIO & BENCHMARK HARNESS
// ============================================================================

struct BenchResult {
    std::string strategy;
    std::string scenario;
    int tick_rate = 0;
    int seed = 0;
    double avg_step_time_ns = 0.0;
    bool stable = true;
    double traj_error = 0.0; // RMS distance from high-fidelity 200 Hz RK4 reference
};

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "ProjectV Fixed-Wing Flight Dynamics Simulation Benchmark" << std::endl;
    std::cout << "Target C++26 standard, Zen 3 hardware profile optimized" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    AircraftConfig f16 = GetF16Config();
    std::vector<std::string> strategy_names = {
        "A_Euler_1Section",
        "B_Euler_4Section",
        "C_RK4_4Section",
        "D_Analytical_LOD",
        "E_Vectorized_4Section"
    };

    std::vector<std::string> scenario_names = {
        "level_flight",
        "high_g_turn",
        "stall_recovery",
        "mach_dash",
        "turbulence_handling"
    };

    const std::vector<int> tick_rates = {20, 60};
    const int num_seeds = 5;
    const double sim_duration = 15.0; // seconds of simulation

    std::vector<BenchResult> all_results;

    // 1. PRECOMPUTE HIGH-FIDELITY REFERENCES (RK4 at 200 Hz)
    // Reference trajectories stored: [scenario][seed] -> vector of (time, pos)
    std::cout << "Generating high-fidelity references (RK4 @ 200 Hz)..." << std::endl;
    std::vector<std::vector<std::vector<std::pair<double, Vector3D>>>> references(5, std::vector<std::vector<std::pair<double, Vector3D>>>(num_seeds));
    
    double ref_dt = 1.0 / 200.0;
    for (int sc = 0; sc < 5; ++sc) {
        for (int seed = 0; seed < num_seeds; ++seed) {
            State s = GetInitialState(sc, seed);
            double t = 0.0;
            while (t <= sim_duration) {
                references[sc][seed].push_back({t, s.pos});
                Controls ctrl = UpdateControls(sc, t, s, seed);
                Vector3D wind = GetWindField(sc, t, seed);
                s = IntegrateRK4(s, ctrl, wind, f16, ref_dt, 1); // 4Section model
                t += ref_dt;
            }
        }
    }
    std::cout << "References precomputed successfully." << std::endl;

    // Helper to evaluate trajectory RMS error against the 200 Hz reference
    auto calc_traj_error = [&](int sc, int seed, const std::vector<std::pair<double, Vector3D>>& traj) {
        if (traj.empty()) return 999999.9;
        double sq_sum = 0.0;
        int count = 0;
        const auto& ref = references[sc][seed];
        
        for (const auto& tp : traj) {
            double time = tp.first;
            Vector3D pos = tp.second;
            
            // Find closest time in reference
            auto it = std::min_element(ref.begin(), ref.end(), [time](const auto& a, const auto& b) {
                return std::abs(a.first - time) < std::abs(b.first - time);
            });
            
            if (it != ref.end()) {
                Vector3D diff = pos - it->second;
                sq_sum += diff.dot(diff);
                count++;
            }
        }
        return count > 0 ? std::sqrt(sq_sum / count) : 999999.9;
    };

    // Warm-up to optimize CPU cache & instructions
    std::cout << "Warm-up in progress..." << std::endl;
    for (int i = 0; i < 20; ++i) {
        State dummy = GetInitialState(0, 0);
        dummy = IntegrateEuler(dummy, {}, {}, f16, 0.05, 0);
        dummy = IntegrateEuler(dummy, {}, {}, f16, 0.05, 1);
        dummy = IntegrateRK4(dummy, {}, {}, f16, 0.05, 1);
        dummy = IntegrateEuler(dummy, {}, {}, f16, 0.05, 2);
    }
    std::cout << "Warm-up finished." << std::endl;

    // Main loops
    for (int tick_rate : tick_rates) {
        double dt = 1.0 / static_cast<double>(tick_rate);
        std::cout << "Running benchmarks for tick rate: " << tick_rate << " Hz (dt = " << dt << " s)" << std::endl;

        for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
            std::string strat_name = strategy_names[strat_idx];
            
            for (int sc = 0; sc < 5; ++sc) {
                std::string sc_name = scenario_names[sc];
                
                for (int seed = 0; seed < num_seeds; ++seed) {
                    // Set up initial state
                    State s = GetInitialState(sc, seed);
                    
                    std::vector<std::pair<double, Vector3D>> trajectory;
                    bool stable = true;
                    
                    // Timing variables
                    std::vector<double> steps_nanoseconds;
                    steps_nanoseconds.reserve(300);

                    // If batch vectorized strategy E, we simulate updating a fleet of 10 aircraft in parallel
                    if (strat_idx == 4) {
                        const int batch_size = 10;
                        StateSoA soa;
                        soa.resize(batch_size);
                        
                        std::vector<Controls> batch_ctrls(batch_size);
                        for (int i = 0; i < batch_size; ++i) {
                            State start_s = GetInitialState(sc, seed);
                            soa.px[i] = start_s.pos.x + i * 2.0; // slight offset
                            soa.py[i] = start_s.pos.y;
                            soa.pz[i] = start_s.pos.z;
                            soa.vx[i] = start_s.vel.x;
                            soa.vy[i] = start_s.vel.y;
                            soa.vz[i] = start_s.vel.z;
                            soa.rw[i] = start_s.rot.w; soa.rx[i] = start_s.rot.x; soa.ry[i] = start_s.rot.y; soa.rz[i] = start_s.rot.z;
                            soa.ox[i] = start_s.omega.x; soa.oy[i] = start_s.omega.y; soa.oz[i] = start_s.omega.z;
                        }

                        double t = 0.0;
                        while (t <= sim_duration) {
                            // Collect controls
                            for (int i = 0; i < batch_size; ++i) {
                                State temp_s;
                                temp_s.pos = {soa.px[i], soa.py[i], soa.pz[i]};
                                temp_s.vel = {soa.vx[i], soa.vy[i], soa.vz[i]};
                                temp_s.rot = {soa.rw[i], soa.rx[i], soa.ry[i], soa.rz[i]};
                                temp_s.omega = {soa.ox[i], soa.oy[i], soa.oz[i]};
                                batch_ctrls[i] = UpdateControls(sc, t, temp_s, seed);
                            }

                            Vector3D wind = GetWindField(sc, t, seed);

                            auto start_t = std::chrono::high_resolution_clock::now();
                            IntegrateRK4_BatchSoA(soa, batch_ctrls, wind, f16, dt);
                            auto end_t = std::chrono::high_resolution_clock::now();
                            
                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            // Average time per aircraft in the batch
                            steps_nanoseconds.push_back(ns / batch_size);

                            trajectory.push_back({t, Vector3D(soa.px[0], soa.py[0], soa.pz[0])});

                            // check stability on lead plane
                            if (std::isnan(soa.px[0]) || std::isinf(soa.px[0]) || std::abs(soa.px[0]) > 1e6 || soa.pz[0] < -100.0) {
                                stable = false;
                                break;
                            }
                            t += dt;
                        }

                    } else {
                        // Standard single aircraft strategies
                        double t = 0.0;
                        while (t <= sim_duration) {
                            Controls ctrl = UpdateControls(sc, t, s, seed);
                            Vector3D wind = GetWindField(sc, t, seed);

                            auto start_t = std::chrono::high_resolution_clock::now();
                            if (strat_idx == 0) { // A_Euler_1Section
                                s = IntegrateEuler(s, ctrl, wind, f16, dt, 0);
                            } else if (strat_idx == 1) { // B_Euler_4Section
                                s = IntegrateEuler(s, ctrl, wind, f16, dt, 1);
                            } else if (strat_idx == 2) { // C_RK4_4Section
                                s = IntegrateRK4(s, ctrl, wind, f16, dt, 1);
                            } else if (strat_idx == 3) { // D_Analytical_LOD
                                s = IntegrateEuler(s, ctrl, wind, f16, dt, 2);
                            }
                            auto end_t = std::chrono::high_resolution_clock::now();

                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            steps_nanoseconds.push_back(ns);

                            trajectory.push_back({t, s.pos});

                            // Check stability
                            if (std::isnan(s.pos.x) || std::isinf(s.pos.x) || std::abs(s.pos.x) > 1e6 || s.pos.z < -100.0) {
                                stable = false;
                                break;
                            }
                            t += dt;
                        }
                    }

                    // Compute statistics
                    double sum = 0.0;
                    for (double ns : steps_nanoseconds) sum += ns;
                    double avg_ns = steps_nanoseconds.empty() ? 0.0 : sum / steps_nanoseconds.size();

                    double error = 0.0;
                    if (stable) {
                        error = calc_traj_error(sc, seed, trajectory);
                    } else {
                        error = 999999.9; // extreme error if crashed
                    }

                    BenchResult res{
                        strat_name,
                        sc_name,
                        tick_rate,
                        seed,
                        avg_ns,
                        stable,
                        error
                    };
                    all_results.push_back(res);
                }
            }
        }
    }

    // 2. EXPORT TO CSV
    std::string csv_path = "results.csv";
    std::cout << "Exporting " << all_results.size() << " entries to " << csv_path << "..." << std::endl;
    std::ofstream csv(csv_path);
    csv << "Strategy,Scenario,TickRate,Seed,StepTimeNs,Stability,TrajError\n";
    for (const auto& r : all_results) {
        csv << r.strategy << ","
            << r.scenario << ","
            << r.tick_rate << ","
            << r.seed << ","
            << std::fixed << std::setprecision(2) << r.avg_step_time_ns << ","
            << (r.stable ? 1 : 0) << ","
            << std::setprecision(4) << r.traj_error << "\n";
    }
    csv.close();

    // 3. PRINT SUMMARY STATISTICS
    std::cout << "\n=====================================================================" << std::endl;
    std::cout << "SUMMARY STATISTICS (Averages over all Scenarios and Seeds)" << std::endl;
    std::cout << "=====================================================================" << std::endl;
    std::cout << std::left << std::setw(25) << "Strategy" 
              << std::setw(15) << "TickRate (Hz)" 
              << std::setw(20) << "Mean Step Time (ns)" 
              << std::setw(15) << "Stability (%)" 
              << std::setw(20) << "Mean Traj Error (m)" << std::endl;
    std::cout << "---------------------------------------------------------------------" << std::endl;

    for (const auto& strat : strategy_names) {
        for (int tick_rate : tick_rates) {
            double time_sum = 0.0;
            double error_sum = 0.0;
            int stable_count = 0;
            int total_runs = 0;

            for (const auto& r : all_results) {
                if (r.strategy == strat && r.tick_rate == tick_rate) {
                    time_sum += r.avg_step_time_ns;
                    total_runs++;
                    if (r.stable) {
                        stable_count++;
                        error_sum += r.traj_error;
                    }
                }
            }

            double mean_time = time_sum / total_runs;
            double stab_pct = 100.0 * stable_count / total_runs;
            double mean_error = stable_count > 0 ? error_sum / stable_count : 999999.9;

            std::cout << std::left << std::setw(25) << strat 
                      << std::setw(15) << tick_rate 
                      << std::fixed << std::setprecision(1)
                      << std::setw(20) << mean_time 
                      << std::setw(15) << stab_pct 
                      << std::setw(20) << mean_error << std::endl;
        }
    }
    std::cout << "=====================================================================" << std::endl;

    return 0;
}
