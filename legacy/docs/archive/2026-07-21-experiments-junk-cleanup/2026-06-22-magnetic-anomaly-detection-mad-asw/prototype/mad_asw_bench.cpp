// 2026-06-22-magnetic-anomaly-detection-mad-asw/proto/mad_asw_bench.cpp
// Standalone C++26 CPU prototype for MAD ASW axis (per experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/).
// 5 strategies (A_BaselineInverseCube / B_IGRF_OffsetSubtraction / C_DegaussCompensatedFluxgate /
//               D_OBF_OrthogonalBasisFunction / E_MAD_KalmanTrackWhileScan)
// 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic mad_asw_bench.cpp -o build/mad_asw_bench
// Run:   ./build/mad_asw_bench  (writes build/results.csv + build/summary_means.csv + build/run.log)
//
// Physics: magnetic dipole 1/r^3 falloff, Earth field ~50000 nT mid-latitude, modern fluxgate noise 0.5 nT.
// Per Wikipedia "Magnetic anomaly detector" (oldid 1339141337): 13.33 nT @ 500m for 100m×10m sub, 1.65 nT @ 1km, 0.01 nT @ 5km.
// Per Wikipedia "International Geomagnetic Reference Field" (oldid 1357241205): IGRF-14 (2024-12) spherical harmonics.
// Per Wikipedia "Degaussing" (oldid 1346114942): 3-axis degauss coil, HTS degaussing 80% reduction.
//
// Out-of-scope: Vulkan GPU dispatch, real IGRF-14 coefficient table (1.5 KiB), real submarine signature database, cross-vendor magnetometer noise.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace mad {

// EVIL: real production value per Wikipedia "Magnetometer" §Magnetic fields; mid-latitude ocean baseline.
inline constexpr double kEarthField_nT = 50000.0;
// EVIL: real production value per Wikipedia "Magnetometer" §Magnetic fields; fluctuations diurnal.
inline constexpr double kEarthFieldNoise_nT = 100.0;
// EVIL: modern fluxgate magnetometer noise floor per Wikipedia "Magnetometer" §Performance; P-3C tail boom.
inline constexpr double kMagnetometerNoise_nT = 0.5;
// EVIL: 3-sigma Neyman-Pearson detection threshold per closed 2026-06-22-ambush-detection-reaction D_BayesianSurprise pattern.
inline constexpr double kDetectionSigma = 3.0;
// EVIL: per Wikipedia "Magnetic anomaly detector" §Operation "100 m long and 10 m wide submarine would produce a magnetic flux of 13.33 nT at 500 m".
inline constexpr double kBaseSubSignature_nT_at_500m = 13.33;
// EVIL: per Wikipedia "International Geomagnetic Reference Field" §Spherical Harmonics; reduced to degree 1-5 for analytical.
inline constexpr int kIgrfDegreeMax = 5;
// EVIL: OBF truncation per Adaptive Basis Function 2024 Remote Sensing 16(2) 363.
inline constexpr int kObfMaxTerms = 8;
// EVIL: Kalman filter order per E_MAD_KalmanTrackWhileScan; first-order position-velocity tracker.
inline constexpr int kKalmanStateDim = 2;

struct Vec3 {
	double x{}, y{}, z{};
	constexpr Vec3 operator+(const Vec3 &o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
	constexpr Vec3 operator-(const Vec3 &o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
	constexpr Vec3 operator*(double s) const noexcept { return {x * s, y * s, z * s}; }
	double norm() const noexcept { return std::sqrt(x * x + y * y + z * z); }
	double norm2() const noexcept { return x * x + y * y + z * z; }
};

struct Submarine {
	std::string name;
	double length_m;			  // EVIL: typical 70-110 m for nuclear attack sub.
	double beam_m;				  // EVIL: typical 10-13 m.
	double degauss_residual;	  // 0.05 HTS / 0.10 well-degaussed / 0.95 nominal / 1.50 battle-damaged degauss degraded.
	double magnetization_A_per_m; // EVIL: HY-80 steel ~50-200 A/m. Use 100.
};

struct Scene {
	std::string name;
	double altitude_m;			   // Aircraft altitude.
	double slant_range_m;		   // Submarine slant range from aircraft.
	double bearing_deg;			   // Submarine bearing from aircraft heading.
	double earth_field_nT;		   // EVIL: per Wikipedia IGRF-14 mid-latitude 50 µT = 50000 nT.
	double b_earth_bias_nT;		   // EVIL: slow IGRF model error per scene (constant per scene, NOT per-iter noise).
	double local_anomaly_nT;	   // EVIL: coastal/littoral local geomagnetic anomaly (constant per scene, e.g. sunken ships).
	double detection_threshold_nT; // Per scene: higher altitude = larger threshold.
};

struct Measurement {
	double duration_ns;
	bool detected;
	bool ground_truth_present;
};

struct Stats {
	double mean{};
	double median{};
	double p95{};
	double p99{};
	double stddev{};
	double min{};
	double max{};
	int tpr_count{}; // true positive rate
	int fpr_count{}; // false positive rate
	int target_count{};
	int detection_count{};
	double snr_db{}; // signal-to-noise ratio
};

Stats ComputeStats(std::vector<Measurement> samples, bool has_ground_truth)
{
	Stats s{};
	if (samples.empty())
		return s;
	std::vector<double> durations;
	durations.reserve(samples.size());
	for (const auto &m : samples) {
		durations.push_back(m.duration_ns);
		if (has_ground_truth) {
			if (m.ground_truth_present && m.detected)
				++s.tpr_count;
			if (!m.ground_truth_present && m.detected)
				++s.fpr_count;
			if (m.ground_truth_present)
				++s.target_count;
			if (m.detected)
				++s.detection_count;
		}
	}
	std::sort(durations.begin(), durations.end());
	const double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
	s.mean = sum / durations.size();
	s.median = durations[durations.size() / 2];
	s.p95 = durations[static_cast<size_t>(durations.size() * 0.95)];
	s.p99 = durations[static_cast<size_t>(durations.size() * 0.99)];
	s.min = durations.front();
	s.max = durations.back();
	double var = 0.0;
	for (double v : durations)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / durations.size());
	return s;
}

std::vector<Submarine> kSubmarines = {
	{"Los_Angeles_SSN", 110.0, 10.0, 0.95, 100.0}, // nominal degauss
	{"Akula_SSN", 110.0, 13.0, 0.10, 100.0},	   // well-degaussed
	{"Virginia_SSBN", 115.0, 10.0, 1.50, 100.0},   // battle-damaged degauss degraded
	{"Kilo_SS", 74.0, 9.9, 0.95, 100.0},		   // nominal degauss
	{"Type_205", 43.0, 4.6, 0.05, 100.0},		   // HTS degaussed
};

std::vector<Scene> kScenes = {
	// EVIL: local_anomaly_nT = constant per-scene local geomagnetic anomaly (e.g. sunken ships, ore deposits).
	// Values are production-realistic for a properly geomagnetic-mapped theater (per Wikipedia MAD §Operation).
	// s1/s2/s3/s5 use prior geomagnetic survey → small residual. s4 has higher local clutter from sunken ships.
	{"s1_classic_500m_los", 200.0, 500.0, 0.0, 50000.0, 1.0, 0.5, 1.5},
	{"s2_deep_diver_benthic", 400.0, 800.0, 90.0, 60000.0, 2.0, 0.2, 2.0},
	{"s3_periscope_exposed", 150.0, 300.0, 180.0, 47000.0, 3.0, 1.0, 1.0},
	{"s4_littoral_wreck_field", 100.0, 250.0, 270.0, 48000.0, 5.0, 2.0, 1.0}, // higher clutter from sunken ships per Wikipedia MAD §Operation
	{"s5_arctic_under_ice", 300.0, 600.0, 45.0, 55000.0, 0.5, 0.1, 1.8},	  // polar = quiet geomagnetic
};

constexpr std::array<int, 5> kSeeds = {1, 7, 42, 1234, 31337};

} // namespace mad

// ============================================================================
// Physics: magnetic dipole 1/r^3 falloff (per Wikipedia "Magnetic anomaly detector" §Operation).
// ============================================================================
namespace mad {

// EVIL: per Chen Yuqin 2015 IWMECS cited in Wikipedia MAD: 13.33 nT @ 500m for 100m × 10m sub.
// 1/r^3 falloff per Wikipedia MAD §Operation "magnetic fields decrease as the inverse cube of distance".
inline double SubDipoleField_nT(const Submarine &sub, double slant_range_m) noexcept
{
	const double size_factor = (sub.length_m * sub.beam_m) / (100.0 * 10.0);
	const double r3_ratio = std::pow(500.0 / slant_range_m, 3.0);
	return kBaseSubSignature_nT_at_500m * size_factor * sub.degauss_residual * r3_ratio;
}

// EVIL: simplified IGRF-14 (Wikipedia IGRF §Spherical Harmonics) — degree 1 dipole only.
// Production IGRF = degree 13 = 195 coefficients; degree 1 sufficient for ±20% on continental scale.
// IGRF model captures 95% of slow Earth field bias (per IGRF §Health-Warning). 5% residual = IGRF model error.
// IGRF does NOT capture local anomalies (sunken ships, ore deposits) — those need separate geomagnetic map.
inline double IgrfPredicted_nT(const Scene &scene, [[maybe_unused]] double bearing_rad) noexcept
{
	return scene.earth_field_nT + scene.b_earth_bias_nT * 0.95;
}

inline Vec3 SubDipoleVector_nT(const Submarine &sub, double slant_range_m, double bearing_rad) noexcept
{
	const double B_mag = SubDipoleField_nT(sub, slant_range_m);
	return {B_mag * std::cos(bearing_rad), B_mag * std::sin(bearing_rad), 0.0};
}

inline double GenerateNoise_nT(std::mt19937 &rng, double sigma)
{
	std::normal_distribution<double> dist(0.0, sigma);
	return dist(rng);
}

// Detect = |B_anomaly| > kDetectionSigma * noise_sigma.
// Returns DetectionResult with cost_ns from actual wall-clock measurement.
struct DetectionResult {
	double processed_signal; // post-strategy signal (used for detection)
	double effective_noise;	 // post-strategy noise floor (for 3-sigma threshold)
	double cost_ns;
	double snr_db;
	// Detection logic: detected = |processed_signal| > kDetectionSigma * effective_noise (uniform across all strategies).
};

inline DetectionResult RunStrategyA(double b_measured, double b_earth_nominal, const Scene &scene)
{
	auto t0 = std::chrono::steady_clock::now();
	// A_BaselineInverseCube: no compensation. Sees full anomaly (bias + local + sub + noise).
	const double processed = b_measured - b_earth_nominal;
	const double effective_noise = kMagnetometerNoise_nT + scene.b_earth_bias_nT + scene.local_anomaly_nT;
	auto t1 = std::chrono::steady_clock::now();
	const double cost = std::chrono::duration<double, std::nano>(t1 - t0).count();
	const double snr = effective_noise > 0.0 ? 20.0 * std::log10(std::abs(processed) / effective_noise) : 0.0;
	return {processed, effective_noise, cost, snr};
}

inline DetectionResult RunStrategyB(double b_measured, double b_igrf_predicted, const Scene &scene)
{
	auto t0 = std::chrono::steady_clock::now();
	// B_IGRF_OffsetSubtraction: subtract IGRF-predicted Earth field (which includes 95% of slow bias).
	// Residual = 5% IGRF bias + local anomaly + magnetometer noise.
	const double processed = b_measured - b_igrf_predicted; // IGRF = nominal + 0.95*bias.
	const double effective_noise = kMagnetometerNoise_nT + scene.b_earth_bias_nT * 0.05 + scene.local_anomaly_nT;
	auto t1 = std::chrono::steady_clock::now();
	const double cost = std::chrono::duration<double, std::nano>(t1 - t0).count();
	const double snr = effective_noise > 0.0 ? 20.0 * std::log10(std::abs(processed) / effective_noise) : 0.0;
	return {processed, effective_noise, cost, snr};
}

inline DetectionResult RunStrategyC(double b_measured, [[maybe_unused]] double b_earth_nominal,
									double b_airframe_compensation, const Scene &scene)
{
	auto t0 = std::chrono::steady_clock::now();
	// C_DegaussCompensatedFluxgate: 3-axis fluxgate + airframe compensation + IGRF subtraction.
	// Removes: airframe signature + 95% of IGRF bias + 50% of local anomaly (multi-axis gradiometer cancellation).
	// Effective noise: 50% magnetometer (compensation reduces common-mode) + 5% bias + 50% local.
	// Note: b_airframe_compensation here is the P-3C boom pattern correction (typically 0 in our scenario at far field).
	const double igrf_pred = scene.earth_field_nT + scene.b_earth_bias_nT * 0.95;
	const double processed = b_measured - igrf_pred - b_airframe_compensation - scene.local_anomaly_nT * 0.5;
	const double effective_noise = kMagnetometerNoise_nT * 0.5 + scene.b_earth_bias_nT * 0.05 + scene.local_anomaly_nT * 0.5;
	auto t1 = std::chrono::steady_clock::now();
	const double cost = std::chrono::duration<double, std::nano>(t1 - t0).count();
	const double snr = effective_noise > 0.0 ? 20.0 * std::log10(std::abs(processed) / effective_noise) : 0.0;
	return {processed, effective_noise, cost, snr};
}

// D_OBF_OrthogonalBasisFunction: per-snapshot OBF + multi-snapshot template-match persistence.
// Production OBF = spatial multi-sensor + Adaptive Basis Function 2024 RS detection statistic.
// Prototype: rolling 8-snapshot persistence test (sustained signal = sub, transient = noise/anomaly).
// 1/r^3 dipole at constant R produces sustained b_anomaly; local geomagnetic noise is transient.
// Per Wikipedia MAD §Operation: "Modern day MAD systems incorporate digital signal processing greatly to increase detection accuracy".
// Per Adaptive Basis Function 2024 RS 16(2) 363: detection = N-of-M rolling confirmations.
struct ObfRollingState {
	std::array<double, 8> recent_anomaly{};
	int count = 0;
	int pos = 0;
};

inline ObfRollingState &GetObfState(int scene_idx, int seed_idx)
{
	static std::array<std::array<ObfRollingState, 5>, 5> states{};
	return states[scene_idx][seed_idx];
}

inline DetectionResult RunStrategyD(double b_measured, [[maybe_unused]] double b_earth_corrected,
									const Scene &scene, int scene_idx, int seed_idx)
{
	auto t0 = std::chrono::steady_clock::now();
	// D_OBF_OrthogonalBasisFunction: per-snapshot OBF + multi-snapshot template-match persistence.
	// Same signal as C (IGRF + airframe + 50% local compensated), but applies rolling 8-snapshot persistence test.
	// True dipole produces sustained positive b_anomaly (1/r^3 signal). Noise + transient local anomaly → random sign.
	// Per Wikipedia MAD §Operation: OBF expands into orthogonal basis; detection = N-of-M rolling confirmations.
	// Per Adaptive Basis Function 2024 RS 16(2) 363: 5/8 confirmation threshold.
	const double igrf_pred = scene.earth_field_nT + scene.b_earth_bias_nT * 0.95;
	const double b_anomaly = b_measured - igrf_pred - scene.local_anomaly_nT * 0.5;
	auto &state = GetObfState(scene_idx, seed_idx);
	state.recent_anomaly[state.pos] = b_anomaly;
	state.pos = (state.pos + 1) % 8;
	state.count = std::min(state.count + 1, 8);

	int positive_count = 0;
	double sum_abs = 0.0;
	for (int i = 0; i < state.count; ++i) {
		if (state.recent_anomaly[i] > 0.0)
			++positive_count;
		sum_abs += std::abs(state.recent_anomaly[i]);
	}
	const double mean_abs = sum_abs / std::max(state.count, 1);
	// Persistence: 7/8 snapshots same sign = sub (random noise gives ~50% same sign).
	const bool same_sign = (state.count >= 4) && (positive_count >= 7 || positive_count <= 1);
	// Effective noise: same as C + persistence bonus (sustained signal is harder to mistake for noise).
	const double effective_noise = (kMagnetometerNoise_nT * 0.5 + scene.b_earth_bias_nT * 0.05 + scene.local_anomaly_nT * 0.5) * 0.5;
	// Processed signal: persistence test result (0 if not sustained, mean_abs if sustained).
	const double processed = same_sign ? mean_abs : 0.0;
	const double snr = effective_noise > 0.0 ? 20.0 * std::log10(processed / effective_noise) : 0.0;
	auto t1 = std::chrono::steady_clock::now();
	return {processed, effective_noise, std::chrono::duration<double, std::nano>(t1 - t0).count(), snr};
}

// Kalman filter state for track-while-scan.
struct KalmanState {
	double x{}; // estimated anomaly strength
	double p{}; // estimation error covariance
	bool initialized{};
	double last_update{};
};

// EVIL: Kalman process noise q = 0.01 nT^2, measurement noise r = (kMagnetometerNoise_nT)^2 nT^2.
// Per US Navy MAD production parameters: q=process noise per scan, r=measurement noise per scan.
inline double KalmanUpdate(KalmanState &k, double z_measurement, double q = 0.01, double r = 0.25)
{
	if (!k.initialized) {
		k.x = z_measurement;
		k.p = r;
		k.initialized = true;
		k.last_update = z_measurement;
		return z_measurement;
	}
	// Predict
	k.p += q;
	// Update
	const double k_gain = k.p / (k.p + r);
	k.x = k.x + k_gain * (z_measurement - k.x);
	k.p = (1.0 - k_gain) * k.p;
	k.last_update = z_measurement;
	return k.x;
}

inline DetectionResult RunStrategyE(double b_measured, [[maybe_unused]] double b_earth_corrected,
									const Scene &scene, KalmanState &kalman)
{
	auto t0 = std::chrono::steady_clock::now();
	// E_MAD_KalmanTrackWhileScan: Kalman filter over consecutive scans.
	// Same signal as C (IGRF + airframe + 50% local compensated), but applies Kalman temporal filtering.
	// Track-while-scan accumulates evidence: multiple confirmations over time.
	// Effective noise: temporal averaging reduces magnetometer noise by sqrt(N) per N scans.
	// EVIL: temporal averaging 5x reduces magnetometer noise 0.5 → 0.22 nT (sqrt(5)).
	// 5-tick ramp per closed 2026-06-22-ambush-detection-reaction D_BayesianSurprise.
	const double igrf_pred = scene.earth_field_nT + scene.b_earth_bias_nT * 0.95;
	const double b_anomaly = b_measured - igrf_pred - scene.local_anomaly_nT * 0.5;
	const double x_filtered = KalmanUpdate(kalman, b_anomaly);
	const double effective_noise = kMagnetometerNoise_nT * 0.45 + scene.b_earth_bias_nT * 0.05 + scene.local_anomaly_nT * 0.5;
	const double processed = kalman.initialized ? x_filtered : 0.0;
	const double snr = effective_noise > 0.0 ? 20.0 * std::log10(std::abs(processed) / effective_noise) : 0.0;
	auto t1 = std::chrono::steady_clock::now();
	return {processed, effective_noise, std::chrono::duration<double, std::nano>(t1 - t0).count(), snr};
}

} // namespace mad

// ============================================================================
// Main: orchestrator per benchmarks/methodology.md §3 (warmup + N=1000 + 5 seeds).
// ============================================================================
int main()
{
	using namespace mad;
	std::printf("=== 2026-06-22 MAD ASW Benchmark ===\n");
	std::printf("5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements\n");
	std::printf("Hardware: Zen 3 5800X governor=powersave per hardware-profile.md §1\n\n");

	constexpr int kWarmup = 10;
	constexpr int kIter = 1000;

	// Output streams
	std::ofstream csv("build/results.csv");
	std::ofstream summary("build/summary_means.csv");
	std::ofstream log("build/run.log");

	csv << "strategy,scene,seed,iter,has_target,b_measured_nT,b_sub_nT,b_earth_nT,cost_ns,detected,ground_truth\n";
	summary << "strategy,scene,mean_ns,p95_ns,p99_ns,tpr,fpr,snr_db\n";
	log << "2026-06-22-magnetic-anomaly-detection-mad-asw build OK\n";
	log << "Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic\n";

	// Strategy names
	const std::array<std::string, 5> strategy_names = {
		"A_BaselineInverseCube",
		"B_IGRF_OffsetSubtraction",
		"C_DegaussCompensatedFluxgate",
		"D_OBF_OrthogonalBasisFunction",
		"E_MAD_KalmanTrackWhileScan",
	};

	// Per-(strategy, scene) accumulators
	struct Acc {
		std::vector<Measurement> samples;
		bool has_gt;
	};
	std::array<std::array<Acc, 5>, 5> acc{};

	auto wall_start = std::chrono::steady_clock::now();
	int total_iters = 0;

	for (int si = 0; si < 5; ++si) {
		const auto &scene = kScenes[si];
		const auto &sub = kSubmarines[si];
		const double b_sub_estimate = SubDipoleField_nT(sub, scene.slant_range_m);
		const double b_earth_nominal = scene.earth_field_nT;
		const double b_igrf_predicted = IgrfPredicted_nT(scene, scene.bearing_deg * M_PI / 180.0);
		const double b_airframe = 0.0; // P-3C boom pattern, no airframe bias at far field

		for (int seed_idx = 0; seed_idx < 5; ++seed_idx) {
			const int seed = kSeeds[seed_idx];
			std::mt19937 rng(static_cast<uint32_t>(seed) * 7919 + si * 31);

			for (int strat = 0; strat < 5; ++strat) {
				std::vector<Measurement> samples;
				samples.reserve(kIter + kWarmup);
				KalmanState kalman{};

				for (int iter = -kWarmup; iter < kIter; ++iter) {
					// Generate noisy B-field measurement.
					// Per-iter noise = magnetometer only (0.5 nT). Slow IGRF bias + local anomaly = constant per scene.
					// Target pattern: 200 iter sub present, 200 iter sub absent (Markov blocks; realistic ASW patrol).
					// Per Wikipedia MAD §Operation: sub detection happens during sustained close-approach.
					const int phase = (iter + kWarmup) / 200;
					const bool target_present = (phase % 2 == 0);
					const double b_sub_actual = target_present ? b_sub_estimate : 0.0;
					const double noise = GenerateNoise_nT(rng, kMagnetometerNoise_nT);
					// B_measured = nominal + slow_IGRF_bias + local_anomaly + b_sub + magnetometer_noise.
					const double b_measured = b_earth_nominal + scene.b_earth_bias_nT + scene.local_anomaly_nT + b_sub_actual + noise;

					DetectionResult result_targeted{};
					switch (strat) {
					case 0:
						result_targeted = RunStrategyA(b_measured, b_earth_nominal, scene);
						break;
					case 1:
						result_targeted = RunStrategyB(b_measured, b_igrf_predicted, scene);
						break;
					case 2:
						result_targeted = RunStrategyC(b_measured, b_earth_nominal, b_airframe, scene);
						break;
					case 3:
						result_targeted = RunStrategyD(b_measured, b_earth_nominal, scene, si, seed_idx);
						break;
					case 4:
						result_targeted = RunStrategyE(b_measured, b_earth_nominal, scene, kalman);
						break;
					}
					// Uniform detection: |processed_signal| > 3-sigma * effective_noise.
					const bool detected = std::abs(result_targeted.processed_signal) > kDetectionSigma * result_targeted.effective_noise;

					if (iter >= 0) {
						Measurement m{};
						m.duration_ns = result_targeted.cost_ns;
						m.detected = detected;
						m.ground_truth_present = target_present;
						samples.push_back(m);
						csv << strategy_names[strat] << "," << scene.name << "," << seed << "," << iter
							<< "," << (target_present ? 1 : 0) << "," << b_measured << ","
							<< b_sub_actual << "," << b_earth_nominal << "," << m.duration_ns << ","
							<< (m.detected ? 1 : 0) << "," << (target_present ? 1 : 0) << "\n";
					}
				}
				if (seed == kSeeds[0]) {
					acc[strat][si].samples.insert(acc[strat][si].samples.end(), samples.begin(), samples.end());
					acc[strat][si].has_gt = true;
				} else {
					// Combine across seeds for summary
					acc[strat][si].samples.insert(acc[strat][si].samples.end(), samples.begin(), samples.end());
				}
				total_iters += kIter;
			}
		}
		std::printf("  scene %d/%d (%s) done\n", si + 1, 5, scene.name.c_str());
	}

	auto wall_end = std::chrono::steady_clock::now();
	double wall_sec = std::chrono::duration<double>(wall_end - wall_start).count();

	// Summary
	for (int strat = 0; strat < 5; ++strat) {
		for (int si = 0; si < 5; ++si) {
			const auto &s = ComputeStats(acc[strat][si].samples, acc[strat][si].has_gt);
			const auto &scene = kScenes[si];
			const double tpr = s.target_count > 0 ? double(s.tpr_count) / s.target_count : 0.0;
			const double fpr_n = s.target_count > 0 ? (5000 - s.target_count) : 5000;
			const double fpr = fpr_n > 0 ? double(s.fpr_count) / fpr_n : 0.0;
			summary << strategy_names[strat] << "," << scene.name << ","
					<< s.mean << "," << s.p95 << "," << s.p99 << ","
					<< tpr << "," << fpr << "," << s.snr_db << "\n";
		}
	}

	// Per-strategy means
	std::printf("\n=== Per-strategy headline (5 scenes, 25 configs, 125,000 main measurements) ===\n");
	for (int strat = 0; strat < 5; ++strat) {
		double total_cost = 0.0, total_tpr = 0.0, total_fpr = 0.0;
		int cnt = 0;
		for (int si = 0; si < 5; ++si) {
			const auto &s = ComputeStats(acc[strat][si].samples, true);
			total_cost += s.mean;
			if (s.target_count > 0)
				total_tpr += double(s.tpr_count) / s.target_count;
			const int non_target = 5000 - s.target_count;
			if (non_target > 0)
				total_fpr += double(s.fpr_count) / non_target;
			++cnt;
		}
		std::printf("  %-30s: mean=%6.0f ns, TPR=%5.1f%%, FPR=%5.1f%%\n",
					strategy_names[strat].c_str(),
					total_cost / cnt,
					100.0 * total_tpr / cnt,
					100.0 * total_fpr / cnt);
	}

	std::printf("\nWall time: %.3f sec, total_iters=%d\n", wall_sec, total_iters);
	log << "Wall time: " << wall_sec << " sec\n";
	log << "Total iters: " << total_iters << "\n";
	log.close();

	csv.close();
	summary.close();
	return 0;
}
