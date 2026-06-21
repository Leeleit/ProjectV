#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <random>

// ---- math primitives ----

struct Vec3 {
    double x{}, y{}, z{};
    constexpr Vec3() = default;
    constexpr Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr double dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr double len2() const { return dot(*this); }
    double len() const { return std::sqrt(len2()); }
    Vec3 norm() const { double l = len(); return l > 1e-12 ? *this / l : Vec3{0,1,0}; }
    constexpr Vec3 cross(Vec3 o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
};

struct Quat {
    double w{1}, x{}, y{}, z{};
    constexpr Quat() = default;
    constexpr Quat(double w, double x, double y, double z) : w(w), x(x), y(y), z(z) {}
    static Quat from_axis_angle(Vec3 axis, double angle) {
        double s = std::sin(angle * 0.5);
        return {std::cos(angle * 0.5), axis.x * s, axis.y * s, axis.z * s};
    }
    Quat operator*(Quat o) const {
        return {
            w * o.w - x * o.x - y * o.y - z * o.z,
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w
        };
    }
    Vec3 rotate(Vec3 v) const {
        Quat p{0, v.x, v.y, v.z};
        Quat conj{w, -x, -y, -z};
        Quat r = *this * p * conj;
        return {r.x, r.y, r.z};
    }
    Quat inv() const { return {w, -x, -y, -z}; }
    static Quat between(Vec3 from, Vec3 to) {
        Vec3 f = from.norm(), t = to.norm();
        double d = f.dot(t);
        if (d > 0.9999) return {1, 0, 0, 0};
        if (d < -0.9999) {
            Vec3 axis = Vec3{0, 0, 1}.cross(f);
            if (axis.len2() < 1e-8) axis = Vec3{0, 1, 0}.cross(f);
            return from_axis_angle(axis.norm(), std::numbers::pi);
        }
        Vec3 axis = f.cross(t);
        double s = std::sqrt((1 + d) * 2);
        return {s * 0.5, axis.x / s, axis.y / s, axis.z / s};
    }
};

// ---- arm chain model ----

struct JointState {
    Vec3 world_pos;
    Quat world_rot;
};

struct ArmChain {
    static constexpr int kNumBones = 3;
    std::array<double, kNumBones> bone_lengths = {0.30, 0.28, 0.10}; // upper, forearm, hand
    std::array<Vec3, kNumBones> bone_dirs;  // local Z direction of each bone
    std::array<JointState, kNumBones + 1> joints; // [0]=shoulder, [1]=elbow, [2]=wrist, [3]=hand_tip

    void reset(Vec3 shoulder_pos) {
        joints[0].world_pos = shoulder_pos;
        joints[0].world_rot = Quat{};
        for (int i = 0; i < kNumBones; ++i) {
            Vec3 dir = bone_dirs[i];
            joints[i + 1].world_pos = joints[i].world_pos + dir * bone_lengths[i];
            joints[i + 1].world_rot = joints[i].world_rot;
        }
    }

    Vec3 end_effector() const { return joints[kNumBones].world_pos; }

    // Forward kinematics: recompute joint positions from rotations
    void fk() {
        for (int i = 0; i < kNumBones; ++i) {
            Vec3 local_z{0, 0, 1};
            Vec3 world_dir = joints[i].world_rot.rotate(local_z);
            joints[i + 1].world_pos = joints[i].world_pos + world_dir * bone_lengths[i];
            joints[i + 1].world_rot = joints[i].world_rot;
        }
    }
};

// ---- scene definitions ----

struct ReachTarget {
    const char* name;
    Vec3 position; // relative to shoulder
};

static ReachTarget kScenes[] = {
    {"forward_reach",  {0.25, -0.15, -0.50}},  // block at eye level (within 0.68m arm)
    {"up_reach",       {0.15,  0.25, -0.45}},  // block above head
    {"down_reach",     {0.30, -0.40, -0.40}},  // block near feet
    {"far_side",       {0.50, -0.10, -0.35}},  // block to the right (within 0.62m)
    {"rapid_switch",   {0.00,  0.00,  0.00}},  // special: alternates between 2 targets
};

static constexpr int kNumScenes = std::size(kScenes);
static constexpr int kNumSeeds = 5;
static constexpr int kNumWarmup = 10;
static constexpr int kNumIter = 1000;

// ---- IK strategies ----

struct IKResult {
    double solve_time_us{};
    int iterations{};
    double position_error_cm{};
    bool converged{};
};

// Strategy A: No hand (baseline) — just measure reachability test
IKResult strategy_A_no_hand(Vec3 /*target*/, ArmChain& /*chain*/, Vec3 /*shoulder*/) {
    // Baseline: 0 cost, 0 iterations
    return {.solve_time_us = 0, .iterations = 0, .position_error_cm = 999, .converged = false};
}

// Strategy B: Analytic two-bone IK (law-of-cosines)
// Solves shoulder + elbow analytically. Sets each joint independently (FABRIK-style),
// does NOT rely on chain.fk() since FK overwrites per-joint rotations.
IKResult strategy_B_analytic_two_bone(Vec3 target, ArmChain& chain, Vec3 shoulder) {
    auto start = std::chrono::high_resolution_clock::now();
    chain.reset(shoulder);

    double r1 = chain.bone_lengths[0], r2 = chain.bone_lengths[1], r3 = chain.bone_lengths[2];
    Vec3 p = target - shoulder;
    Vec3 rest_dir{0, 0, 1};
    Vec3 dir_to_target = p.norm();

    // Offset target by tool length to solve for wrist position
    Vec3 p_wrist = p - dir_to_target * r3;
    double d_wrist = p_wrist.len();

    // If wrist-target out of upper+forearm reach, point fully extended
    if (d_wrist > r1 + r2 - 0.001) {
        Quat aim = Quat::between(rest_dir, dir_to_target);
        chain.joints[0].world_rot = aim;
        Vec3 dir = chain.joints[0].world_rot.rotate(rest_dir);
        chain.joints[1].world_pos = shoulder + dir * r1;
        chain.joints[1].world_rot = aim;
        chain.joints[2].world_pos = shoulder + dir * (r1 + r2);
        chain.joints[2].world_rot = aim;
        chain.joints[3].world_pos = shoulder + dir * (r1 + r2 + r3);
        chain.joints[3].world_rot = aim;
        auto end0 = std::chrono::high_resolution_clock::now();
        double us0 = std::chrono::duration<double, std::micro>(end0 - start).count();
        Vec3 err = chain.end_effector() - target;
        return {.solve_time_us = us0, .iterations = 1,
                .position_error_cm = err.len() * 100, .converged = false};
    }

    // Law of cosines for wrist-target triangle
    double cos_shoulder = (r1 * r1 + d_wrist * d_wrist - r2 * r2) / (2 * r1 * d_wrist);
    cos_shoulder = std::clamp(cos_shoulder, -1.0, 1.0);
    double shoulder_angle = std::acos(cos_shoulder);

    // Rotation plane: perpendicular to rest_dir × target_dir
    Vec3 perp_axis = rest_dir.cross(dir_to_target).norm();
    if (perp_axis.len2() < 0.01) perp_axis = Vec3{0, 1, 0};

    // Upper arm direction: rotate target_dir away by shoulder_angle
    // +sign = elbow bends toward -Y for right arm
    Vec3 upper_dir = Quat::from_axis_angle(perp_axis, shoulder_angle)
                     .rotate(dir_to_target);
    Vec3 forearm_dir = (p_wrist - upper_dir * r1).norm();

    // Set each joint independently (like FABRIK)
    Vec3 elbow_pos = shoulder + upper_dir * r1;
    Vec3 wrist_pos = elbow_pos + forearm_dir * r2;
    Vec3 tip_pos  = wrist_pos + forearm_dir * r3;

    chain.joints[0].world_rot = Quat::between(rest_dir, upper_dir);
    chain.joints[0].world_pos = shoulder;
    chain.joints[1].world_rot = Quat::between(rest_dir, forearm_dir);
    chain.joints[1].world_pos = elbow_pos;
    chain.joints[2].world_rot = chain.joints[1].world_rot;
    chain.joints[2].world_pos = wrist_pos;
    chain.joints[3].world_rot = chain.joints[1].world_rot;
    chain.joints[3].world_pos = tip_pos;

    auto end1 = std::chrono::high_resolution_clock::now();
    double us1 = std::chrono::duration<double, std::micro>(end1 - start).count();
    Vec3 err = chain.end_effector() - target;
    return {.solve_time_us = us1, .iterations = 1, .position_error_cm = err.len() * 100,
            .converged = err.len() < 0.01};
}

// Strategy C: CCD (Cyclic Coordinate Descent) — no constraints
IKResult strategy_C_CCD(Vec3 target, ArmChain& chain, Vec3 shoulder) {
    auto start = std::chrono::high_resolution_clock::now();
    chain.reset(shoulder);

    const int kMaxIter = 50;
    const double kThreshold = 0.01;
    int iter;

    for (iter = 0; iter < kMaxIter; ++iter) {
        Vec3 ee = chain.end_effector();
        Vec3 err = target - ee;
        if (err.len() < kThreshold) break;

        for (int j = chain.kNumBones - 1; j >= 0; --j) {
            Vec3 joint_pos = chain.joints[j].world_pos;
            Vec3 to_ee = (ee - joint_pos).norm();
            Vec3 to_target = (target - joint_pos).norm();

            double cos_angle = std::clamp(to_ee.dot(to_target), -1.0, 1.0);
            if (cos_angle > 0.9999) continue;

            Vec3 axis = to_ee.cross(to_target).norm();
            double angle = std::acos(cos_angle);
            angle = std::clamp(angle, -0.5, 0.5);

            Quat delta = Quat::from_axis_angle(axis, angle);
            chain.joints[j].world_rot = delta * chain.joints[j].world_rot;
            chain.fk();
            ee = chain.end_effector();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    Vec3 err = chain.end_effector() - target;
    return {.solve_time_us = us, .iterations = iter, .position_error_cm = err.len() * 100,
            .converged = err.len() < kThreshold};
}

// Strategy D: FABRIK (Forward And Backward Reaching IK) — no constraints
IKResult strategy_D_FABRIK(Vec3 target, ArmChain& chain, Vec3 shoulder) {
    auto start = std::chrono::high_resolution_clock::now();
    chain.reset(shoulder);

    const int kMaxIter = 50;
    const double kThreshold = 0.01;
    int iter;

    std::array<Vec3, chain.kNumBones + 1> pos;
    for (iter = 0; iter < kMaxIter; ++iter) {
        for (int i = 0; i <= chain.kNumBones; ++i)
            pos[i] = chain.joints[i].world_pos;

        Vec3 ee = pos[chain.kNumBones];
        Vec3 err = target - ee;
        if (err.len() < kThreshold) break;

        pos[chain.kNumBones] = target;
        for (int i = chain.kNumBones - 1; i >= 0; --i) {
            Vec3 dir = (pos[i] - pos[i + 1]).norm();
            pos[i] = pos[i + 1] + dir * chain.bone_lengths[i];
        }

        pos[0] = shoulder;
        for (int i = 0; i < chain.kNumBones; ++i) {
            Vec3 dir = (pos[i + 1] - pos[i]).norm();
            pos[i + 1] = pos[i] + dir * chain.bone_lengths[i];
        }

        for (int i = 0; i < chain.kNumBones; ++i) {
            Vec3 dir = (pos[i + 1] - pos[i]).norm();
            chain.joints[i].world_rot = Quat::between({0, 0, 1}, dir);
            chain.joints[i].world_pos = pos[i];
        }
        chain.joints[chain.kNumBones].world_pos = pos[chain.kNumBones];
    }

    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    Vec3 err = chain.end_effector() - target;
    return {.solve_time_us = us, .iterations = iter, .position_error_cm = err.len() * 100,
            .converged = err.len() < kThreshold};
}

// Strategy E: FABRIK with joint constraints
IKResult strategy_E_FABRIK_constrained(Vec3 target, ArmChain& chain, Vec3 shoulder) {
    auto start = std::chrono::high_resolution_clock::now();
    chain.reset(shoulder);

    const int kMaxIter = 50;
    const double kThreshold = 0.01;
    int iter;

    std::array<Vec3, chain.kNumBones + 1> pos;
    for (iter = 0; iter < kMaxIter; ++iter) {
        for (int i = 0; i <= chain.kNumBones; ++i)
            pos[i] = chain.joints[i].world_pos;

        Vec3 ee = pos[chain.kNumBones];
        Vec3 err = target - ee;
        if (err.len() < kThreshold) break;

        pos[chain.kNumBones] = target;
        for (int i = chain.kNumBones - 1; i >= 0; --i) {
            Vec3 dir = (pos[i] - pos[i + 1]).norm();
            pos[i] = pos[i + 1] + dir * chain.bone_lengths[i];
        }

        pos[0] = shoulder;
        for (int i = 0; i < chain.kNumBones; ++i) {
            Vec3 dir = (pos[i + 1] - pos[i]).norm();
            pos[i + 1] = pos[i] + dir * chain.bone_lengths[i];
        }

        for (int i = 0; i < chain.kNumBones; ++i) {
            Vec3 dir = (pos[i + 1] - pos[i]).norm();
            Quat target_rot = Quat::between({0, 0, 1}, dir);

            if (i == 0) {
                double angle = 2 * std::acos(std::clamp(target_rot.w, -1.0, 1.0));
                angle = std::clamp(angle, 0.0, 2.5);
                if (target_rot.w < 1) {
                    double s = std::sqrt(1 - target_rot.w * target_rot.w);
                    if (s > 1e-8) {
                        Vec3 ax{target_rot.x / s, target_rot.y / s, target_rot.z / s};
                        target_rot = Quat::from_axis_angle(ax, angle);
                    }
                }
            }
            if (i == 1) {
                Vec3 elbow_dir = chain.joints[1].world_pos - chain.joints[0].world_pos;
                if (elbow_dir.y > 0.05) {
                    Vec3 correction_axis = elbow_dir.cross(Vec3{0, -1, 0}).norm();
                    if (correction_axis.len2() > 0.01)
                        target_rot = Quat::from_axis_angle(correction_axis, 0.1) * target_rot;
                }
            }
            if (i == 2) {
                double angle = 2 * std::acos(std::clamp(target_rot.w, -1.0, 1.0));
                angle = std::clamp(angle, 0.0, 0.8);
                if (target_rot.w < 1) {
                    double s = std::sqrt(1 - target_rot.w * target_rot.w);
                    if (s > 1e-8) {
                        Vec3 ax{target_rot.x / s, target_rot.y / s, target_rot.z / s};
                        target_rot = Quat::from_axis_angle(ax, angle);
                    }
                }
            }

            chain.joints[i].world_rot = target_rot;
            chain.joints[i].world_pos = pos[i];
        }
        chain.joints[chain.kNumBones].world_pos = pos[chain.kNumBones];
    }

    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    Vec3 err = chain.end_effector() - target;
    return {.solve_time_us = us, .iterations = iter, .position_error_cm = err.len() * 100,
            .converged = err.len() < kThreshold};
}

// Strategy F: CCD with joint constraints
IKResult strategy_F_CCD_constrained(Vec3 target, ArmChain& chain, Vec3 shoulder) {
    auto start = std::chrono::high_resolution_clock::now();
    chain.reset(shoulder);

    const int kMaxIter = 50;
    const double kThreshold = 0.01;
    int iter;

    for (iter = 0; iter < kMaxIter; ++iter) {
        Vec3 ee = chain.end_effector();
        Vec3 err = target - ee;
        if (err.len() < kThreshold) break;

        for (int j = chain.kNumBones - 1; j >= 0; --j) {
            Vec3 joint_pos = chain.joints[j].world_pos;
            Vec3 to_ee = (ee - joint_pos).norm();
            Vec3 to_target = (target - joint_pos).norm();

            double cos_angle = std::clamp(to_ee.dot(to_target), -1.0, 1.0);
            if (cos_angle > 0.9999) continue;

            Vec3 axis = to_ee.cross(to_target).norm();
            double angle = std::acos(cos_angle);
            angle = std::clamp(angle, -0.3, 0.3);

            if (j == 0) {
                double total = 2 * std::acos(std::clamp(chain.joints[0].world_rot.w, -1.0, 1.0));
                if (total + angle > 2.5)
                    angle = std::max(0.0, 2.5 - total);
            }
            if (j == 1) {
                Vec3 elbow_dir = chain.joints[1].world_pos - chain.joints[0].world_pos;
                if (elbow_dir.y > 0.05) {
                    Vec3 correction = elbow_dir.cross(Vec3{0, -1, 0}).norm();
                    if (correction.len2() > 0.01)
                        axis = (axis + correction * 0.3).norm();
                }
            }
            if (j == 2) {
                double wrist_total = 2 * std::acos(std::clamp(chain.joints[2].world_rot.w, -1.0, 1.0));
                if (wrist_total + angle > 0.8)
                    angle = std::max(0.0, 0.8 - wrist_total);
            }

            Quat delta = Quat::from_axis_angle(axis, angle);
            chain.joints[j].world_rot = delta * chain.joints[j].world_rot;
            chain.fk();
            ee = chain.end_effector();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    Vec3 err = chain.end_effector() - target;
    return {.solve_time_us = us, .iterations = iter, .position_error_cm = err.len() * 100,
            .converged = err.len() < kThreshold};
}

// ---- benchmark harness ----

struct Strategy {
    const char* name;
    IKResult (*func)(Vec3, ArmChain&, Vec3);
};

static constexpr Strategy kStrategies[] = {
    {"A_NoHand",           strategy_A_no_hand},
    {"B_AnalyticTwoBone",  strategy_B_analytic_two_bone},
    {"C_CCD",              strategy_C_CCD},
    {"D_FABRIK",           strategy_D_FABRIK},
    {"E_FABRIK_Constrained", strategy_E_FABRIK_constrained},
    {"F_CCD_Constrained",  strategy_F_CCD_constrained},
};

static constexpr int kNumStrategies = std::size(kStrategies);

struct Measurement {
    const char* strategy;
    const char* scene;
    int seed;
    double solve_time_us;
    int iterations;
    double position_error_cm;
    bool converged;
};

// ---- main ----

int main() {
    printf("=== IK First-Person Arm Benchmark ===\n");
    printf("Strategies: %d, Scenes: %d, Seeds: %d, Warmup: %d, Iter: %d\n",
           kNumStrategies, kNumScenes, kNumSeeds, kNumWarmup, kNumIter);
    printf("strategy,scene,seed,solve_time_us,iterations,position_error_cm,converged\n");

    uint32_t seed_data[kNumSeeds] = {1, 7, 42, 1234, 31337};

    for (int si = 0; si < kNumStrategies; ++si) {
        for (int sci = 0; sci < kNumScenes; ++sci) {
            // Rapid switch scene: target alternates each frame
            bool is_rapid = (std::strcmp(kScenes[sci].name, "rapid_switch") == 0);
            Vec3 rap_a{0.30, -0.10, -0.50};
            Vec3 rap_b{-0.20, 0.15, -0.40};

            for (int sei = 0; sei < kNumSeeds; ++sei) {
                std::mt19937_64 rng(seed_data[sei]);

                ArmChain chain;
                chain.bone_dirs = {Vec3{0, 0, -1}, Vec3{0, 0, -1}, Vec3{0, 0, -1}};
                Vec3 shoulder{0, 0, 0};

                // Warmup
                for (int w = 0; w < kNumWarmup; ++w) {
                    Vec3 warmup_target = kScenes[sci].position;
                    if (is_rapid) {
                        warmup_target = (w % 2 == 0) ? rap_a : rap_b;
                    } else {
                        // Add small jitter
                        warmup_target.x += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                        warmup_target.y += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                        warmup_target.z += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                    }
                    kStrategies[si].func(warmup_target, chain, shoulder);
                }

                // Measurement
                double total_time = 0;
                int total_iter = 0;
                double total_err = 0;
                int converged_count = 0;
                int samples = 0;

                for (int i = 0; i < kNumIter; ++i) {
                    Vec3 target = kScenes[sci].position;
                    if (is_rapid) {
                        target = (i % 2 == 0) ? rap_a : rap_b;
                    } else {
                        target.x += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                        target.y += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                        target.z += (std::uniform_real_distribution<double>{-0.05, 0.05})(rng);
                    }

                    IKResult r = kStrategies[si].func(target, chain, shoulder);
                    total_time += r.solve_time_us;
                    total_iter += r.iterations;
                    total_err += r.position_error_cm;
                    if (r.converged) ++converged_count;
                    ++samples;
                }

                printf("%s,%s,%d,%.3f,%.1f,%.3f,%d\n",
                       kStrategies[si].name,
                       kScenes[sci].name,
                       seed_data[sei],
                       total_time / samples,
                       (double)total_iter / samples,
                       total_err / samples,
                       converged_count);
            }
        }
    }

    return 0;
}
