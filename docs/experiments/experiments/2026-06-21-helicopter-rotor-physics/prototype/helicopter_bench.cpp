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
    Vector3D vel;
    Quaternion rot;
    Vector3D omega;
    double rotor_omega = 30.0; // rad/s
    std::array<double, 4> flapping_beta{0.0, 0.0, 0.0, 0.0};
    std::array<double, 4> flapping_dbeta{0.0, 0.0, 0.0, 0.0};

    State operator+(const struct Derivatives& d) const;
};

struct Derivatives {
    Vector3D d_pos;
    Vector3D d_vel;
    Quaternion d_rot;
    Vector3D d_omega;
    double d_rotor_omega = 0.0;
    std::array<double, 4> d_flapping_beta{0.0, 0.0, 0.0, 0.0};
    std::array<double, 4> d_flapping_dbeta{0.0, 0.0, 0.0, 0.0};

    Derivatives operator+(const Derivatives& o) const {
        Derivatives r;
        r.d_pos = d_pos + o.d_pos;
        r.d_vel = d_vel + o.d_vel;
        r.d_rot = Quaternion(d_rot.w + o.d_rot.w, d_rot.x + o.d_rot.x, d_rot.y + o.d_rot.y, d_rot.z + o.d_rot.z);
        r.d_omega = d_omega + o.d_omega;
        r.d_rotor_omega = d_rotor_omega + o.d_rotor_omega;
        for (int i = 0; i < 4; ++i) {
            r.d_flapping_beta[i] = d_flapping_beta[i] + o.d_flapping_beta[i];
            r.d_flapping_dbeta[i] = d_flapping_dbeta[i] + o.d_flapping_dbeta[i];
        }
        return r;
    }

    Derivatives operator*(double s) const {
        Derivatives r;
        r.d_pos = d_pos * s;
        r.d_vel = d_vel * s;
        r.d_rot = Quaternion(d_rot.w * s, d_rot.x * s, d_rot.y * s, d_rot.z * s);
        r.d_omega = d_omega * s;
        r.d_rotor_omega = d_rotor_omega * s;
        for (int i = 0; i < 4; ++i) {
            r.d_flapping_beta[i] = d_flapping_beta[i] * s;
            r.d_flapping_dbeta[i] = d_flapping_dbeta[i] * s;
        }
        return r;
    }
};

State State::operator+(const Derivatives& d) const {
    State s;
    s.pos = pos + d.d_pos;
    s.vel = vel + d.d_vel;
    s.rot = Quaternion(rot.w + d.d_rot.w, rot.x + d.d_rot.x, rot.y + d.d_rot.y, rot.z + d.d_rot.z).normalized();
    s.omega = omega + d.d_omega;
    s.rotor_omega = rotor_omega + d.d_rotor_omega;
    if (s.rotor_omega < 0.0) s.rotor_omega = 0.0;
    for (int i = 0; i < 4; ++i) {
        s.flapping_beta[i] = flapping_beta[i] + d.d_flapping_beta[i];
        s.flapping_dbeta[i] = flapping_dbeta[i] + d.d_flapping_dbeta[i];
    }
    return s;
}

struct Controls {
    double engine_throttle = 1.0;
    double collective = 0.1; // rad, general lift
    double cyclic_lat = 0.0;  // rad, roll control
    double cyclic_lon = 0.0;  // rad, pitch control
    double tail_pitch = 0.0;  // rad, yaw control
};

struct HelicopterConfig {
    double mass = 4500.0; // kg (UH-1 Huey class)
    Vector3D inertia{4000.0, 15000.0, 12000.0}; // kg m^2
    double engine_power = 1100000.0; // Watts (1400 HP)
    double rotor_radius = 7.3; // m
    double rotor_inertia = 1200.0; // kg m^2
    double blade_chord = 0.5; // m
    double blade_mass = 75.0; // kg
    double Lock_number = 6.0; // aerodynamic flapping inertia factor
};

// ============================================================================
// HELICOPTER PHYSICAL EQUATIONS
// ============================================================================

double GetAirDensity(double altitude) {
    if (altitude < 0.0) altitude = 0.0;
    return 1.225 * std::exp(-altitude / 8500.0);
}

// Solves induced inflow velocity using Newton-Raphson iteration
double SolveInflow(double vc, double vf, double omega_r, double ct, double density) {
    // Non-dimensional vertical and horizontal velocity
    double v_c = vc / omega_r;
    double v_f = vf / omega_r;

    // In hover, lambda = sqrt(ct/2)
    double lambda = std::sqrt(std::abs(ct) / 2.0);
    if (v_c < 0.0) { // descending
        lambda = std::max(0.01, lambda + v_c);
    }

    for (int iter = 0; iter < 5; ++iter) {
        double den = std::sqrt(v_f * v_f + lambda * lambda);
        if (den < 1e-4) den = 1e-4;
        
        double f = lambda - v_c - ct / (2.0 * den);
        double df = 1.0 + (ct * lambda) / (2.0 * den * den * den);
        
        double delta = f / df;
        lambda -= delta;
        if (std::abs(delta) < 1e-6) break;
    }
    
    return lambda * omega_r - vc; // return dimensional induced velocity
}

// Evaluates derivatives for Strategy D (4-blade BET with Flapping & RK4)
Derivatives EvalDerivatives4SectionFlapping(const State& s, const Controls& ctrl, const Vector3D& wind_world, const HelicopterConfig& cfg, double time) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D vel_body = s.rot.inverseRotate(s.vel);
    Vector3D wind_body = s.rot.inverseRotate(wind_world);
    Vector3D relative_vel = vel_body - wind_body;

    double R = cfg.rotor_radius;
    double omega_r = s.rotor_omega * R;
    if (omega_r < 1.0) omega_r = 1.0;

    // Approximate total thrust to estimate induced inflow
    // In local hovering, Thrust ≈ Mass * G
    double estimated_thrust = cfg.mass * 9.81;
    double ct = estimated_thrust / (density * M_PI * R * R * omega_r * omega_r);
    double v_induced = SolveInflow(-relative_vel.z, std::sqrt(relative_vel.x * relative_vel.x + relative_vel.y * relative_vel.y), omega_r, ct, density);

    // Vortex Ring State (VRS) Lift Penalty
    double vrs_factor = 1.0;
    if (relative_vel.z < -5.0 && std::sqrt(relative_vel.x*relative_vel.x + relative_vel.y*relative_vel.y) < 5.0) {
        // rapid vertical descent in low horizontal speed
        double vrs_peak = -1.2 * v_induced;
        double dist = relative_vel.z - vrs_peak;
        vrs_factor = 1.0 - 0.5 * std::exp(- (dist * dist) / (0.4 * v_induced * v_induced));
    }

    // Translational Lift (ETL) ground effect modification
    // Ground effect reduces induced velocity
    if (s.pos.z < R) {
        double ratio = s.pos.z / (2.0 * R);
        v_induced *= std::clamp(1.0 - 1.0 / (16.0 * ratio * ratio), 0.5, 1.0);
    }

    Vector3D rotor_force_body(0, 0, 0);
    Vector3D rotor_torque_body(0, 0, 0);
    double rotor_drag_torque = 0.0;

    const int N = 4;
    double Ib = cfg.blade_mass * R * R / 3.0; // blade moment of inertia about hinge

    for (int i = 0; i < N; ++i) {
        double psi = s.rotor_omega * time + i * (2.0 * M_PI / N);
        double beta = s.flapping_beta[i];
        double dbeta = s.flapping_dbeta[i];

        // Blade span direction including flapping
        Vector3D span_dir(std::cos(psi) * std::cos(beta), std::sin(psi) * std::cos(beta), std::sin(beta));
        Vector3D chord_dir(-std::sin(psi), std::cos(psi), 0.0);
        Vector3D normal_dir(-std::cos(psi) * std::sin(beta), -std::sin(psi) * std::sin(beta), std::cos(beta));

        // Evaluate blade element at 3/4 radius
        double r = 0.75 * R;
        Vector3D r_pos = span_dir * r;

        // Local airflow relative to blade element
        Vector3D local_rot_vel = chord_dir * (s.rotor_omega * r);
        Vector3D flapping_vel = normal_dir * (r * dbeta);
        Vector3D local_vel = relative_vel + s.omega.cross(r_pos) + local_rot_vel + flapping_vel;
        
        // Subtract induced velocity (downwards relative to rotor plane)
        local_vel.z += v_induced;

        double ut = local_vel.dot(chord_dir);
        double up = local_vel.dot(normal_dir);
        double speed_sq = ut * ut + up * up;
        double speed = std::sqrt(speed_sq);

        if (speed > 1e-3) {
            double local_alpha = std::atan2(-up, ut);
            
            // Blade pitch control cyclic + collective
            double pitch_angle = ctrl.collective + ctrl.cyclic_lon * std::cos(psi) + ctrl.cyclic_lat * std::sin(psi);
            double alpha = pitch_angle + local_alpha;

            // Aerodynamic lift and drag coefficients
            double cl = 5.7 * std::sin(alpha); // NACA 0012 linear slope
            double cd = 0.012 + 0.1 * alpha * alpha; // drag polar

            double q = 0.5 * density * speed_sq;
            double lift = q * cfg.blade_chord * R * cl * vrs_factor;
            double drag = q * cfg.blade_chord * R * cd;

            // Lift acts perpendicular to local speed, drag acts opposite
            Vector3D lift_dir = (chord_dir * (-up) + normal_dir * ut) / speed;
            Vector3D drag_dir = (local_vel * -1.0) / speed;

            Vector3D force = lift_dir * lift + drag_dir * drag;
            rotor_force_body += force;
            rotor_torque_body += r_pos.cross(force);

            // Rotor resistance torque (against rotation)
            rotor_drag_torque += (force.dot(chord_dir) * -1.0) * r;

            // Dynamic blade flapping equation:
            // I_b * d2beta = Aero_moment - Centrifugal_moment - Gravity_moment
            double aero_moment = lift * r;
            double centrifugal_moment = cfg.blade_mass * r * s.rotor_omega * s.rotor_omega * std::sin(beta) * r;
            double gravity_moment = cfg.blade_mass * 9.81 * r * std::cos(beta);

            double d2beta = (aero_moment - centrifugal_moment - gravity_moment) / Ib;
            d.d_flapping_beta[i] = dbeta;
            d.d_flapping_dbeta[i] = d2beta; // aerodynamic damping is already modeled naturally through local lift velocity relative to flapping
        } else {
            d.d_flapping_beta[i] = 0;
            d.d_flapping_dbeta[i] = 0;
        }
    }

    // Tail Rotor yaw restoring thrust
    double tail_arm = -6.5;
    double tail_radius = 1.2;
    double tail_estimated_thrust = -density * M_PI * tail_radius * tail_radius * (120.0 * 120.0) * ctrl.tail_pitch;
    Vector3D tail_force_body(0.0, tail_estimated_thrust, 0.0);
    Vector3D tail_torque_body = Vector3D(0.0, 0.0, tail_arm).cross(tail_force_body);

    // Gravity
    Vector3D gravity_world(0.0, 0.0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    // Total forces and torques
    Vector3D total_force = rotor_force_body + tail_force_body + gravity_body;
    Vector3D total_torque = rotor_torque_body + tail_torque_body;

    d.d_vel = s.rot.rotate(total_force / cfg.mass);
    d.d_rot = s.rot.getDerivative(s.omega);

    // Euler angular dynamics
    Vector3D gyro(
        s.omega.y * s.omega.z * (cfg.inertia.z - cfg.inertia.y),
        s.omega.z * s.omega.x * (cfg.inertia.x - cfg.inertia.z),
        s.omega.x * s.omega.y * (cfg.inertia.y - cfg.inertia.x)
    );
    d.d_omega = Vector3D(
        (total_torque.x - gyro.x) / cfg.inertia.x,
        (total_torque.y - gyro.y) / cfg.inertia.y,
        (total_torque.z - gyro.z) / cfg.inertia.z
    );

    // Rotor RPM acceleration (Engine power vs aerodynamic drag)
    double engine_torque = 0.0;
    if (ctrl.engine_throttle > 0.05) {
        engine_torque = cfg.engine_power / std::max(s.rotor_omega, 5.0) * ctrl.engine_throttle;
    }
    d.d_rotor_omega = (engine_torque - rotor_drag_torque) / cfg.rotor_inertia;

    return d;
}

// Strategy A: Analytical Momentum Theory (No flapping, lookup disc)
Derivatives EvalDerivativesMomentumLOD(const State& s, const Controls& ctrl, const Vector3D& wind_world, const HelicopterConfig& cfg) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D relative_vel = s.rot.inverseRotate(s.vel) - s.rot.inverseRotate(wind_world);
    double speed = relative_vel.length();

    // Simplify disc model thrust & torque
    double theta = ctrl.collective;
    double omega_r = s.rotor_omega * cfg.rotor_radius;
    if (omega_r < 1.0) omega_r = 1.0;

    double q = 0.5 * density * omega_r * omega_r;
    double thrust_coeff = 0.1 * theta; // simplified disc lift coefficient
    double thrust = q * M_PI * cfg.rotor_radius * cfg.rotor_radius * thrust_coeff;

    // Apply cyclic controls
    Vector3D rotor_force_body(0.0, 0.0, thrust);
    
    // Cyclic tilts the net thrust vector slightly
    rotor_force_body.x += thrust * std::sin(ctrl.cyclic_lon);
    rotor_force_body.y += thrust * std::sin(ctrl.cyclic_lat);

    // Damping torques
    Vector3D rotor_torque_body(
        thrust * (-cfg.rotor_radius * 0.1 * ctrl.cyclic_lat) - 1000.0 * s.omega.x,
        thrust * (cfg.rotor_radius * 0.1 * ctrl.cyclic_lon) - 1000.0 * s.omega.y,
        -0.08 * thrust * cfg.rotor_radius - 800.0 * s.omega.z // torque drag
    );

    // Tail rotor
    double tail_arm = -6.5;
    double tail_thrust = -density * 4.5 * (120.0 * 120.0) * ctrl.tail_pitch;
    Vector3D tail_force_body(0, tail_thrust, 0);
    Vector3D tail_torque_body(0, 0, tail_arm * tail_thrust);

    // Gravity
    Vector3D gravity_world(0.0, 0.0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    d.d_vel = s.rot.rotate((rotor_force_body + tail_force_body + gravity_body) / cfg.mass);
    d.d_rot = s.rot.getDerivative(s.omega);

    Vector3D gyro(
        s.omega.y * s.omega.z * (cfg.inertia.z - cfg.inertia.y),
        s.omega.z * s.omega.x * (cfg.inertia.x - cfg.inertia.z),
        s.omega.x * s.omega.y * (cfg.inertia.y - cfg.inertia.x)
    );
    d.d_omega = Vector3D(
        (rotor_torque_body.x + tail_torque_body.x - gyro.x) / cfg.inertia.x,
        (rotor_torque_body.y + tail_torque_body.y - gyro.y) / cfg.inertia.y,
        (rotor_torque_body.z + tail_torque_body.z - gyro.z) / cfg.inertia.z
    );

    double engine_torque = 0.0;
    if (ctrl.engine_throttle > 0.05) {
        engine_torque = cfg.engine_power / std::max(s.rotor_omega, 5.0) * ctrl.engine_throttle;
    }
    double drag_torque = 0.05 * thrust * cfg.rotor_radius;
    d.d_rotor_omega = (engine_torque - drag_torque) / cfg.rotor_inertia;

    return d;
}

// Strategy B & C: Blade Element Theory (No Flapping, Euler, 2 or 4 Blades)
Derivatives EvalDerivativesBET(const State& s, const Controls& ctrl, const Vector3D& wind_world, const HelicopterConfig& cfg, double time, int num_blades) {
    Derivatives d;
    d.d_pos = s.vel;

    double density = GetAirDensity(s.pos.z);
    Vector3D vel_body = s.rot.inverseRotate(s.vel);
    Vector3D wind_body = s.rot.inverseRotate(wind_world);
    Vector3D relative_vel = vel_body - wind_body;

    double R = cfg.rotor_radius;
    double omega_r = s.rotor_omega * R;
    if (omega_r < 1.0) omega_r = 1.0;

    double estimated_thrust = cfg.mass * 9.81;
    double ct = estimated_thrust / (density * M_PI * R * R * omega_r * omega_r);
    double v_induced = SolveInflow(-relative_vel.z, std::sqrt(relative_vel.x * relative_vel.x + relative_vel.y * relative_vel.y), omega_r, ct, density);

    Vector3D rotor_force_body(0, 0, 0);
    Vector3D rotor_torque_body(0, 0, 0);
    double rotor_drag_torque = 0.0;

    for (int i = 0; i < num_blades; ++i) {
        double psi = s.rotor_omega * time + i * (2.0 * M_PI / num_blades);
        Vector3D span_dir(std::cos(psi), std::sin(psi), 0.0);
        Vector3D chord_dir(-std::sin(psi), std::cos(psi), 0.0);
        Vector3D normal_dir(0.0, 0.0, 1.0);

        double r = 0.75 * R;
        Vector3D r_pos = span_dir * r;

        Vector3D local_vel = relative_vel + s.omega.cross(r_pos) + chord_dir * (s.rotor_omega * r);
        local_vel.z += v_induced;

        double ut = local_vel.dot(chord_dir);
        double up = local_vel.dot(normal_dir);
        double speed = std::sqrt(ut * ut + up * up);

        if (speed > 1e-3) {
            double local_alpha = std::atan2(-up, ut);
            double pitch_angle = ctrl.collective + ctrl.cyclic_lon * std::cos(psi) + ctrl.cyclic_lat * std::sin(psi);
            double alpha = pitch_angle + local_alpha;

            double cl = 5.7 * std::sin(alpha);
            double cd = 0.012 + 0.1 * alpha * alpha;

            double q = 0.5 * density * speed * speed;
            double lift = q * cfg.blade_chord * R * cl;
            double drag = q * cfg.blade_chord * R * cd;

            Vector3D lift_dir = (chord_dir * (-up) + normal_dir * ut) / speed;
            Vector3D drag_dir = (local_vel * -1.0) / speed;

            Vector3D force = lift_dir * lift + drag_dir * drag;
            rotor_force_body += force;
            rotor_torque_body += r_pos.cross(force);
            rotor_drag_torque += (force.dot(chord_dir) * -1.0) * r;
        }
    }

    double tail_arm = -6.5;
    double tail_thrust = -density * 4.5 * (120.0 * 120.0) * ctrl.tail_pitch;
    Vector3D tail_force_body(0, tail_thrust, 0);
    Vector3D tail_torque_body(0, 0, tail_arm * tail_thrust);

    Vector3D gravity_world(0.0, 0.0, -9.81 * cfg.mass);
    Vector3D gravity_body = s.rot.inverseRotate(gravity_world);

    Vector3D total_force = rotor_force_body + tail_force_body + gravity_body;
    Vector3D total_torque = rotor_torque_body + tail_torque_body;

    d.d_vel = s.rot.rotate(total_force / cfg.mass);
    d.d_rot = s.rot.getDerivative(s.omega);

    Vector3D gyro(
        s.omega.y * s.omega.z * (cfg.inertia.z - cfg.inertia.y),
        s.omega.z * s.omega.x * (cfg.inertia.x - cfg.inertia.z),
        s.omega.x * s.omega.y * (cfg.inertia.y - cfg.inertia.x)
    );
    d.d_omega = Vector3D(
        (total_torque.x - gyro.x) / cfg.inertia.x,
        (total_torque.y - gyro.y) / cfg.inertia.y,
        (total_torque.z - gyro.z) / cfg.inertia.z
    );

    double engine_torque = 0.0;
    if (ctrl.engine_throttle > 0.05) {
        engine_torque = cfg.engine_power / std::max(s.rotor_omega, 5.0) * ctrl.engine_throttle;
    }
    d.d_rotor_omega = (engine_torque - rotor_drag_torque) / cfg.rotor_inertia;

    return d;
}

// ============================================================================
// INTEGRATION ROUTINES
// ============================================================================

State IntegrateEuler(const State& s, const Controls& ctrl, const Vector3D& wind, const HelicopterConfig& cfg, double dt, double time, int model_type) {
    Derivatives d;
    if (model_type == 0) { // Momentum LOD
        d = EvalDerivativesMomentumLOD(s, ctrl, wind, cfg);
    } else if (model_type == 1) { // 2-blade BET
        d = EvalDerivativesBET(s, ctrl, wind, cfg, time, 2);
    } else { // 4-blade BET
        d = EvalDerivativesBET(s, ctrl, wind, cfg, time, 4);
    }
    return s + d * dt;
}

State IntegrateRK4(const State& s, const Controls& ctrl, const Vector3D& wind, const HelicopterConfig& cfg, double dt, double time) {
    auto eval = [&](const State& st, double t) {
        return EvalDerivatives4SectionFlapping(st, ctrl, wind, cfg, t);
    };

    Derivatives k1 = eval(s, time);
    Derivatives k2 = eval(s + k1 * (0.5 * dt), time + 0.5 * dt);
    Derivatives k3 = eval(s + k2 * (0.5 * dt), time + 0.5 * dt);
    Derivatives k4 = eval(s + k3 * dt, time + dt);

    Derivatives d = (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (1.0 / 6.0);
    return s + d * dt;
}

// ============================================================================
// VECTORIZED (SoA) COMPUTATION FOR STRATEGY E
// ============================================================================

struct StateSoA {
    std::vector<double> px, py, pz;
    std::vector<double> vx, vy, vz;
    std::vector<double> rw, rx, ry, rz;
    std::vector<double> ox, oy, oz;
    std::vector<double> rotor_omega;
    std::vector<std::array<double, 4>> flap_b;
    std::vector<std::array<double, 4>> flap_db;

    void resize(size_t n) {
        px.resize(n); py.resize(n); pz.resize(n);
        vx.resize(n); vy.resize(n); vz.resize(n);
        rw.resize(n, 1.0); rx.resize(n, 0.0); ry.resize(n, 0.0); rz.resize(n, 0.0);
        ox.resize(n); oy.resize(n); oz.resize(n);
        rotor_omega.resize(n, 30.0);
        flap_b.resize(n, {0, 0, 0, 0});
        flap_db.resize(n, {0, 0, 0, 0});
    }
};

void IntegrateRK4_BatchSoA(StateSoA& soa, const std::vector<Controls>& ctrls, const Vector3D& wind_world, const HelicopterConfig& cfg, double dt, double time) {
    size_t n = soa.px.size();

    // Save initial state
    std::vector<double> s_px = soa.px, s_py = soa.py, s_pz = soa.pz;
    std::vector<double> s_vx = soa.vx, s_vy = soa.vy, s_vz = soa.vz;
    std::vector<double> s_rw = soa.rw, s_rx = soa.rx, s_ry = soa.ry, s_rz = soa.rz;
    std::vector<double> s_ox = soa.ox, s_oy = soa.oy, s_oz = soa.oz;
    std::vector<double> s_rotor_omega = soa.rotor_omega;
    auto s_flap_b = soa.flap_b;
    auto s_flap_db = soa.flap_db;

    // Accumulators
    std::vector<double> acc_px(n, 0.0), acc_py(n, 0.0), acc_pz(n, 0.0);
    std::vector<double> acc_vx(n, 0.0), acc_vy(n, 0.0), acc_vz(n, 0.0);
    std::vector<double> acc_rw(n, 0.0), acc_rx(n, 0.0), acc_ry(n, 0.0), acc_rz(n, 0.0);
    std::vector<double> acc_ox(n, 0.0), acc_oy(n, 0.0), acc_oz(n, 0.0);
    std::vector<double> acc_rotor_omega(n, 0.0);
    std::vector<std::array<double, 4>> acc_flap_b(n, {0, 0, 0, 0});
    std::vector<std::array<double, 4>> acc_flap_db(n, {0, 0, 0, 0});

    // Stage derivatives
    std::vector<double> k_px(n), k_py(n), k_pz(n);
    std::vector<double> k_vx(n), k_vy(n), k_vz(n);
    std::vector<double> k_rw(n), k_rx(n), k_ry(n), k_rz(n);
    std::vector<double> k_ox(n), k_oy(n), k_oz(n);
    std::vector<double> k_rotor_omega(n);
    std::vector<std::array<double, 4>> k_flap_b(n), k_flap_db(n);

    // Temporal states
    std::vector<double> t_px(n), t_py(n), t_pz(n);
    std::vector<double> t_vx(n), t_vy(n), t_vz(n);
    std::vector<double> t_rw(n), t_rx(n), t_ry(n), t_rz(n);
    std::vector<double> t_ox(n), t_oy(n), t_oz(n);
    std::vector<double> t_rotor_omega(n);
    std::vector<std::array<double, 4>> t_flap_b(n), t_flap_db(n);

    for (int stage = 0; stage < 4; ++stage) {
        double stage_dt = (stage == 0) ? 0.0 : ((stage == 3) ? dt : 0.5 * dt);
        double weight = (stage == 0 || stage == 3) ? 1.0 / 6.0 : 2.0 / 6.0;
        double t_stage = time + ((stage == 0) ? 0.0 : ((stage == 3) ? dt : 0.5 * dt));

        if (stage == 0) {
            t_px = s_px; t_py = s_py; t_pz = s_pz;
            t_vx = s_vx; t_vy = s_vy; t_vz = s_vz;
            t_rw = s_rw; t_rx = s_rx; t_ry = s_ry; t_rz = s_rz;
            t_ox = s_ox; t_oy = s_oy; t_oz = s_oz;
            t_rotor_omega = s_rotor_omega;
            t_flap_b = s_flap_b;
            t_flap_db = s_flap_db;
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
                t_rotor_omega[i] = s_rotor_omega[i] + k_rotor_omega[i] * stage_dt;
                for (int b = 0; b < 4; ++b) {
                    t_flap_b[i][b] = s_flap_b[i][b] + k_flap_b[i][b] * stage_dt;
                    t_flap_db[i][b] = s_flap_db[i][b] + k_flap_db[i][b] * stage_dt;
                }
            }
        }

        #pragma omp simd
        for (size_t i = 0; i < n; ++i) {
            State s_tmp;
            s_tmp.pos = {t_px[i], t_py[i], t_pz[i]};
            s_tmp.vel = {t_vx[i], t_vy[i], t_vz[i]};
            s_tmp.rot = {t_rw[i], t_rx[i], t_ry[i], t_rz[i]};
            s_tmp.omega = {t_ox[i], t_oy[i], t_oz[i]};
            s_tmp.rotor_omega = t_rotor_omega[i];
            s_tmp.flapping_beta = t_flap_b[i];
            s_tmp.flapping_dbeta = t_flap_db[i];

            Derivatives d_tmp = EvalDerivatives4SectionFlapping(s_tmp, ctrls[i], wind_world, cfg, t_stage);

            k_px[i] = d_tmp.d_pos.x; k_py[i] = d_tmp.d_pos.y; k_pz[i] = d_tmp.d_pos.z;
            k_vx[i] = d_tmp.d_vel.x; k_vy[i] = d_tmp.d_vel.y; k_vz[i] = d_tmp.d_vel.z;
            k_rw[i] = d_tmp.d_rot.w; k_rx[i] = d_tmp.d_rot.x; k_ry[i] = d_tmp.d_rot.y; k_rz[i] = d_tmp.d_rot.z;
            k_ox[i] = d_tmp.d_omega.x; k_oy[i] = d_tmp.d_omega.y; k_oz[i] = d_tmp.d_omega.z;
            k_rotor_omega[i] = d_tmp.d_rotor_omega;
            k_flap_b[i] = d_tmp.d_flapping_beta;
            k_flap_db[i] = d_tmp.d_flapping_dbeta;

            acc_px[i] += d_tmp.d_pos.x * weight;
            acc_py[i] += d_tmp.d_pos.y * weight;
            acc_pz[i] += d_tmp.d_pos.z * weight;
            acc_vx[i] += d_tmp.d_vel.x * weight;
            acc_vy[i] += d_tmp.d_vel.y * weight;
            acc_vz[i] += d_tmp.d_vel.z * weight;
            acc_rw[i] += d_tmp.d_rot.w * weight;
            acc_rx[i] += d_tmp.d_rot.x * weight;
            acc_ry[i] += d_tmp.d_rot.y * weight;
            acc_rz[i] += d_tmp.d_rot.z * weight;
            acc_ox[i] += d_tmp.d_omega.x * weight;
            acc_oy[i] += d_tmp.d_omega.y * weight;
            acc_oz[i] += d_tmp.d_omega.z * weight;
            acc_rotor_omega[i] += d_tmp.d_rotor_omega * weight;
            for (int b = 0; b < 4; ++b) {
                acc_flap_b[i][b] += d_tmp.d_flapping_beta[b] * weight;
                acc_flap_db[i][b] += d_tmp.d_flapping_dbeta[b] * weight;
            }
        }
    }

    // Apply updates
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
        soa.rotor_omega[i] = s_rotor_omega[i] + acc_rotor_omega[i] * dt;
        if (soa.rotor_omega[i] < 0.0) soa.rotor_omega[i] = 0.0;

        for (int b = 0; b < 4; ++b) {
            soa.flap_b[i][b] = s_flap_b[i][b] + acc_flap_b[i][b] * dt;
            soa.flap_db[i][b] = s_flap_db[i][b] + acc_flap_db[i][b] * dt;
        }
    }
}

// ============================================================================
// PILOT INPUTS & FLIGHT CONTROLLER
// ============================================================================

Controls UpdateControls(int scenario, double sim_time, const State& s, int seed) {
    Controls ctrl;
    ctrl.engine_throttle = 1.0;
    ctrl.collective = 0.09; // baseline collective pitch for hover
    ctrl.cyclic_lat = 0.0;
    ctrl.cyclic_lon = 0.0;
    ctrl.tail_pitch = 0.05; // yaw trim for main rotor torque

    double seed_offset = (seed - 2) * 0.005;

    switch (scenario) {
        case 0: // hover_stability (PD stabilization)
            {
                double target_alt = 100.0 + seed_offset * 100.0;
                // Altitude PD
                double alt_err = target_alt - s.pos.z;
                ctrl.collective = 0.088 + 0.002 * alt_err - 0.005 * s.vel.z;
                ctrl.collective = std::clamp(ctrl.collective, 0.03, 0.2);

                // Attitude stabilization PD (tuned for rotor lag)
                // lateral (roll) -> cyclic_lat
                double roll_approx = 2.0 * (s.rot.w * s.rot.x + s.rot.y * s.rot.z);
                ctrl.cyclic_lat = -0.15 * roll_approx - 0.05 * s.omega.x;
                ctrl.cyclic_lat = std::clamp(ctrl.cyclic_lat, -0.15, 0.15);

                // longitudinal (pitch) -> cyclic_lon
                double pitch_approx = 2.0 * (s.rot.w * s.rot.y - s.rot.z * s.rot.x);
                ctrl.cyclic_lon = -0.15 * pitch_approx - 0.05 * s.omega.y;
                ctrl.cyclic_lon = std::clamp(ctrl.cyclic_lon, -0.15, 0.15);

                // heading (yaw) -> tail_pitch
                double yaw_approx = 2.0 * (s.rot.w * s.rot.z + s.rot.x * s.rot.y);
                ctrl.tail_pitch = 0.045 - 0.15 * yaw_approx - 0.04 * s.omega.z;
                ctrl.tail_pitch = std::clamp(ctrl.tail_pitch, -0.15, 0.15);
            }
            break;

        case 1: // forward_flight (push nose down to fly forward)
            {
                ctrl.cyclic_lon = -0.06; // push stick forward
                ctrl.collective = 0.10 + seed_offset; // increase collective for forward climb
                
                // Roll attitude stabilization
                double roll_approx = 2.0 * (s.rot.w * s.rot.x + s.rot.y * s.rot.z);
                ctrl.cyclic_lat = -0.4 * roll_approx - 0.08 * s.omega.x;
            }
            break;

        case 2: // vortex_ring_state (induce fast vertical sink rate)
            if (sim_time < 5.0) {
                ctrl.collective = 0.03; // pull collective down to sink
                ctrl.engine_throttle = 1.0;
            } else {
                // Try to recover by pulling collective UP (fails or struggles in VRS!)
                ctrl.collective = 0.18;
                ctrl.engine_throttle = 1.0;
            }
            break;

        case 3: // autorotation (engine failure at high altitude)
            if (sim_time < 1.0) {
                ctrl.engine_throttle = 1.0;
                ctrl.collective = 0.09;
            } else {
                ctrl.engine_throttle = 0.0; // engine dead
                ctrl.collective = 0.03;           // lower collective to maintain rotor RPM
            }
            break;

        case 4: // cyclic_maneuver (steer aggressively)
            ctrl.collective = 0.09;
            if (sim_time > 1.0 && sim_time < 3.0) {
                ctrl.cyclic_lon = 0.12; // pull stick back (pitch up)
            } else if (sim_time >= 3.0 && sim_time < 5.0) {
                ctrl.cyclic_lat = 0.10; // push stick right (roll right)
            }
            break;
    }

    return ctrl;
}

Vector3D GetWindField(int scenario, double sim_time, int seed) {
    if (scenario != 0) return {0, 0, 0}; // only hover scenario gets wind turbulence here
    double freq = 0.4 + seed * 0.1;
    return Vector3D(8.0 * std::sin(freq * sim_time), 12.0 * std::cos(freq * sim_time), 3.0 * std::sin((freq + 0.5) * sim_time));
}

State GetInitialState(int scenario, int seed) {
    State s;
    double offset = (seed - 2) * 2.0;
    s.pos = Vector3D(offset, 0, 500.0);
    s.vel = Vector3D(0, 0, 0);
    s.rot = Quaternion(1, 0, 0, 0);
    s.omega = Vector3D(0, 0, 0);
    s.rotor_omega = 31.4; // 300 RPM nominal
    s.flapping_beta = {0.02, 0.02, 0.02, 0.02}; // initial coning
    s.flapping_dbeta = {0, 0, 0, 0};

    switch (scenario) {
        case 1: // forward_flight
            s.vel = Vector3D(10.0, 0, 0);
            break;
        case 2: // vortex_ring_state
            s.vel = Vector3D(0, 0, -2.0);
            break;
        case 3: // autorotation
            s.pos.z = 1000.0;
            s.vel = Vector3D(0, 0, -5.0);
            break;
    }
    return s;
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
    double traj_error = 0.0; // RMS distance from Strategy D @ 200 Hz
};

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "ProjectV Helicopter Rotor Physics Simulation Benchmark" << std::endl;
    std::cout << "Target C++26 standard, Zen 3 hardware profile optimized" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    HelicopterConfig cfg;
    std::vector<std::string> strategy_names = {
        "A_MomentumTheory_LOD",
        "B_BladeElement_2Blades",
        "C_BladeElement_4Blades",
        "D_BladeElement_4Blades_Flapping",
        "E_Vectorized_Helicopters"
    };

    std::vector<std::string> scenario_names = {
        "hover_stability",
        "forward_flight",
        "vortex_ring_state",
        "autorotation",
        "cyclic_maneuver"
    };

    const std::vector<int> tick_rates = {20, 60};
    const int num_seeds = 5;
    const double sim_duration = 10.0; // seconds

    // 1. PRECOMPUTE HIGH-FIDELITY REFERENCE (Strategy D at 200 Hz)
    std::cout << "Generating high-fidelity references (Strategy D @ 200 Hz)..." << std::endl;
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
                s = IntegrateRK4(s, ctrl, wind, cfg, ref_dt, t);
                t += ref_dt;
            }
        }
    }
    std::cout << "References precomputed successfully." << std::endl;

    auto calc_traj_error = [&](int sc, int seed, const std::vector<std::pair<double, Vector3D>>& traj) {
        if (traj.empty()) return 999999.9;
        double sq_sum = 0.0;
        int count = 0;
        const auto& ref = references[sc][seed];
        
        for (const auto& tp : traj) {
            double time = tp.first;
            Vector3D pos = tp.second;
            
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

    // Warm-up
    for (int i = 0; i < 20; ++i) {
        State dummy = GetInitialState(0, 0);
        dummy = IntegrateEuler(dummy, {}, {}, cfg, 0.05, 0.0, 0);
        dummy = IntegrateEuler(dummy, {}, {}, cfg, 0.05, 0.0, 1);
        dummy = IntegrateEuler(dummy, {}, {}, cfg, 0.05, 0.0, 2);
        dummy = IntegrateRK4(dummy, {}, {}, cfg, 0.05, 0.0);
    }

    std::vector<BenchResult> all_results;

    // Run Benchmarks
    for (int tick_rate : tick_rates) {
        double dt = 1.0 / static_cast<double>(tick_rate);
        std::cout << "Running benchmarks for tick rate: " << tick_rate << " Hz" << std::endl;

        for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
            std::string strat_name = strategy_names[strat_idx];

            for (int sc = 0; sc < 5; ++sc) {
                std::string sc_name = scenario_names[sc];

                for (int seed = 0; seed < num_seeds; ++seed) {
                    State s = GetInitialState(sc, seed);
                    std::vector<std::pair<double, Vector3D>> trajectory;
                    bool stable = true;

                    std::vector<double> step_times_ns;
                    step_times_ns.reserve(200);

                    if (strat_idx == 4) { // Strategy E: Vectorized SoA 10 helicopters
                        const int batch_size = 10;
                        StateSoA soa;
                        soa.resize(batch_size);

                        std::vector<Controls> batch_ctrls(batch_size);
                        for (int i = 0; i < batch_size; ++i) {
                            State start_s = GetInitialState(sc, seed);
                            soa.px[i] = start_s.pos.x + i * 5.0; // horizontal separation
                            soa.py[i] = start_s.pos.y;
                            soa.pz[i] = start_s.pos.z;
                            soa.vx[i] = start_s.vel.x;
                            soa.vy[i] = start_s.vel.y;
                            soa.vz[i] = start_s.vel.z;
                            soa.rw[i] = start_s.rot.w; soa.rx[i] = start_s.rot.x; soa.ry[i] = start_s.rot.y; soa.rz[i] = start_s.rot.z;
                            soa.ox[i] = start_s.omega.x; soa.oy[i] = start_s.omega.y; soa.oz[i] = start_s.omega.z;
                            soa.rotor_omega[i] = start_s.rotor_omega;
                            soa.flap_b[i] = start_s.flapping_beta;
                            soa.flap_db[i] = start_s.flapping_dbeta;
                        }

                        double t = 0.0;
                        while (t <= sim_duration) {
                            for (int i = 0; i < batch_size; ++i) {
                                State temp_s;
                                temp_s.pos = {soa.px[i], soa.py[i], soa.pz[i]};
                                temp_s.vel = {soa.vx[i], soa.vy[i], soa.vz[i]};
                                temp_s.rot = {soa.rw[i], soa.rx[i], soa.ry[i], soa.rz[i]};
                                temp_s.omega = {soa.ox[i], soa.oy[i], soa.oz[i]};
                                temp_s.rotor_omega = soa.rotor_omega[i];
                                temp_s.flapping_beta = soa.flap_b[i];
                                temp_s.flapping_dbeta = soa.flap_db[i];
                                batch_ctrls[i] = UpdateControls(sc, t, temp_s, seed);
                            }
                            Vector3D wind = GetWindField(sc, t, seed);

                            auto start_t = std::chrono::high_resolution_clock::now();
                            IntegrateRK4_BatchSoA(soa, batch_ctrls, wind, cfg, dt, t);
                            auto end_t = std::chrono::high_resolution_clock::now();

                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            step_times_ns.push_back(ns / batch_size);

                            trajectory.push_back({t, Vector3D(soa.px[0], soa.py[0], soa.pz[0])});

                            if (std::isnan(soa.px[0]) || std::isinf(soa.px[0]) || std::abs(soa.px[0]) > 1e6 || soa.pz[0] < -100.0) {
                                stable = false;
                                break;
                            }
                            t += dt;
                        }

                    } else { // Single aircraft strategies A, B, C, D
                        double t = 0.0;
                        while (t <= sim_duration) {
                            Controls ctrl = UpdateControls(sc, t, s, seed);
                            Vector3D wind = GetWindField(sc, t, seed);

                            auto start_t = std::chrono::high_resolution_clock::now();
                            if (strat_idx == 0) {
                                s = IntegrateEuler(s, ctrl, wind, cfg, dt, t, 0); // Momentum LOD
                            } else if (strat_idx == 1) {
                                s = IntegrateEuler(s, ctrl, wind, cfg, dt, t, 1); // 2-blade BET
                            } else if (strat_idx == 2) {
                                s = IntegrateEuler(s, ctrl, wind, cfg, dt, t, 2); // 4-blade BET
                            } else if (strat_idx == 3) {
                                s = IntegrateRK4(s, ctrl, wind, cfg, dt, t);      // 4-blade Flapping + RK4
                            }
                            auto end_t = std::chrono::high_resolution_clock::now();

                            double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                            step_times_ns.push_back(ns);

                            trajectory.push_back({t, s.pos});

                            if (std::isnan(s.pos.x) || std::isinf(s.pos.x) || std::abs(s.pos.x) > 1e6 || s.pos.z < -100.0) {
                                stable = false;
                                break;
                            }
                            t += dt;
                        }
                    }

                    double sum = 0.0;
                    for (double ns : step_times_ns) sum += ns;
                    double avg_ns = step_times_ns.empty() ? 0.0 : sum / step_times_ns.size();

                    double error = 0.0;
                    if (stable) {
                        error = calc_traj_error(sc, seed, trajectory);
                    } else {
                        error = 999999.9;
                    }

                    all_results.push_back({
                        strat_name,
                        sc_name,
                        tick_rate,
                        seed,
                        avg_ns,
                        stable,
                        error
                    });
                }
            }
        }
    }

    // 2. WRITE TO CSV
    std::string csv_path = "results.csv";
    std::cout << "Writing " << all_results.size() << " entries to " << csv_path << "..." << std::endl;
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
    std::cout << std::left << std::setw(30) << "Strategy" 
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

            std::cout << std::left << std::setw(30) << strat 
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
