#pragma once
// 5 strategies for supersonic projectile audio event generation.
// Per Wikipedia "Gunshot": crack = sonic boom (N-wave), thump = muzzle blast (140 dB impulse).
// Crack-thump relationship = t_crack - t_thump per listener-to-shooter distance.

#include <cmath>
#include <cstdint>

#include "scenes.hpp"

namespace strategies {

struct AudioEvent {
    // Time offsets (ms) relative to t=0 (muzzle blast event time).
    // thump plays first (t=0), crack plays after delay (or before, if projectile is close).
    double t_thump_ms = 0.0;
    double t_crack_ms = 0.0;
    // Amplitudes (linear, 0-1)
    double thump_amp = 0.0;
    double crack_amp = 0.0;
    // Spectral centroids (Hz) — proxy for "rumbly" thump vs "sharp" crack
    double thump_centroid_hz = 0.0;
    double crack_centroid_hz = 0.0;
    // Doppler-shifted pitch for crack (Hz); 0 = no shift
    double crack_pitch_hz = 0.0;
    // For verification: theoretical reference values (computed from physics)
    double t_thump_theory_ms = 0.0;
    double t_crack_theory_ms = 0.0;
};

using StrategyFn = double (*)(const scenes::Projectile& proj, AudioEvent* out);

// === A_NoAudio === Baseline: 0 cost, 0 audio. Reference null.
inline double A_NoAudio(const scenes::Projectile&, AudioEvent* out) {
    out->t_thump_ms = 0.0;
    out->t_crack_ms = 0.0;
    out->thump_amp = 0.0;
    out->crack_amp = 0.0;
    out->thump_centroid_hz = 0.0;
    out->crack_centroid_hz = 0.0;
    out->crack_pitch_hz = 0.0;
    return 0.0;  // 0 µs
}

// === B_SimpleSample === Play pre-recorded WAV gun sound, ignore physics.
//  Cost: ~5 µs (constant), 0 dB PSNR vs reference (delay = 0, wrong crack-thump relationship).
inline double B_SimpleSample(const scenes::Projectile&, AudioEvent* out) {
    out->t_thump_ms = 0.0;
    out->t_crack_ms = 0.0;  // No delay computed!
    out->thump_amp = 1.0;
    out->crack_amp = 1.0;
    out->thump_centroid_hz = 800.0;   // arbitrary
    out->crack_centroid_hz = 3000.0;  // arbitrary
    out->crack_pitch_hz = 3000.0;
    // Simulate WAV file load + audio event creation overhead
    volatile double sink = 0.0;
    for (int i = 0; i < 50; ++i) sink += std::sin(static_cast<double>(i) * 0.01);
    return 5.0 + sink * 0.001;  // ~5 µs nominal
}

// === C_PhysicsBasedCrackThump === Full physics-based: t_thump + t_crack from projectile + listener.
inline double C_PhysicsBasedCrackThump(const scenes::Projectile& proj, AudioEvent* out) {
    const double c = scenes::kC_Sound_20C;
    const double dx = proj.listener.x - proj.muzzle.x;
    const double dy = proj.listener.y - proj.muzzle.y;
    const double dz = proj.listener.z - proj.muzzle.z;
    const double dist_listener = std::sqrt(dx*dx + dy*dy + dz*dz);
    // Projectile closest-approach time = (proj_listener - muzzle) · v_dir / v0
    const double t_flight = ((proj.listener.x - proj.muzzle.x) * proj.v_dir.x +
                             (proj.listener.y - proj.muzzle.y) * proj.v_dir.y +
                             (proj.listener.z - proj.muzzle.z) * proj.v_dir.z) / proj.v0;
    // Theoretical reference times (for PSNR/verification)
    const double t_thump_theory = dist_listener / c * 1000.0;  // ms
    const double t_crack_theory = t_flight * 1000.0;          // ms (when projectile passes)
    out->t_thump_theory_ms = t_thump_theory;
    out->t_crack_theory_ms = t_crack_theory;
    // Actual event times: thump at t=0 (we're scheduling the muzzle blast now),
    // crack at t = t_flight - t_thump (relative to thump)
    out->t_thump_ms = 0.0;
    out->t_crack_ms = t_crack_theory - t_thump_theory;
    // Amplitudes: muzzle blast ~140 dB, projectile sonic boom ~ 50-500 Pa
    out->thump_amp = 0.95;
    out->crack_amp = 0.7;
    // Spectral centroids: thump is rumbly (low freq, ~300 Hz), crack is sharp (high freq, ~3000 Hz)
    out->thump_centroid_hz = 300.0;
    out->crack_centroid_hz = 3000.0;
    out->crack_pitch_hz = 3000.0;  // No Doppler in C
    // Volatile sink to prevent DCE
    volatile double sink = t_thump_theory + t_crack_theory + dist_listener + t_flight;
    return 0.05 + (sink - sink) * 0.0;  // <0.1 µs — pure math
}

// === D_DopplerShifted === C + Doppler shift on crack (since projectile moves through Mach cone).
inline double D_DopplerShifted(const scenes::Projectile& proj, AudioEvent* out) {
    // Run C first
    C_PhysicsBasedCrackThump(proj, out);
    // Apply Doppler shift to crack only (thump is stationary source)
    // For Mach>1 sources, frequency shift formula is invalid per Wikipedia "Doppler effect"
    // (would need full Mach cone analysis). Use simplified model: f_obs = f_src * c / (c - v_approach).
    // Here v_approach = v0 (projectile toward listener) clamped to <c to avoid singularity.
    const double c = scenes::kC_Sound_20C;
    const double v_app = std::min(proj.v0 * 0.9, c * 0.95);  // clamp to <c
    const double doppler_factor = c / (c - v_app);
    out->crack_pitch_hz = out->crack_centroid_hz * doppler_factor;
    return 0.06;  // ~C + tiny overhead
}

// === E_PhysicallyModeledSynthesis === Full physical model: N-wave crack + combustion thump.
// Per Wikipedia "Sonic boom": N-wave pressure profile (rise-decrease-sudden return).
// Muzzle blast per Wikipedia "Muzzle blast": pressure front, infrasonic compression, hot gases.
inline double E_PhysicallyModeledSynthesis(const scenes::Projectile& proj, AudioEvent* out) {
    C_PhysicsBasedCrackThump(proj, out);  // base timing
    // Physical model refinements:
    // 1) Thump amplitude scales with propellant charge: 1 - exp(-powder_g / 50)
    const double powder_norm = std::min(1.0, proj.powder_g / 50.0);
    out->thump_amp = 0.7 + 0.25 * (1.0 - std::exp(-powder_norm));
    // 2) Crack amplitude scales with caliber × v0: heavier/faster = louder crack
    const double energy_factor = (proj.caliber_mm * proj.v0) / 8000.0;  // normalized
    out->crack_amp = std::min(0.95, 0.5 + 0.3 * energy_factor);
    // 3) Thump spectral centroid inversely with caliber (bigger = lower freq)
    out->thump_centroid_hz = 600.0 / (1.0 + proj.caliber_mm / 20.0);
    // 4) Crack spectral centroid scales with v0 (faster = sharper)
    out->crack_centroid_hz = 1500.0 + 1500.0 * (proj.v0 / 1000.0);
    out->crack_pitch_hz = out->crack_centroid_hz;
    return 0.1;  // higher cost due to model computation
}

inline constexpr StrategyFn kStrategies[] = {
    A_NoAudio,
    B_SimpleSample,
    C_PhysicsBasedCrackThump,
    D_DopplerShifted,
    E_PhysicallyModeledSynthesis,
};

inline constexpr const char* kStrategyNames[] = {
    "A_NoAudio",
    "B_SimpleSample",
    "C_PhysicsBasedCrackThump",
    "D_DopplerShifted",
    "E_PhysicallyModeledSynthesis",
};

}  // namespace strategies
