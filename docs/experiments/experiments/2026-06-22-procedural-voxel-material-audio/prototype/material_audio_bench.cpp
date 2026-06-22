#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string_view>

// ---- constants ----
constexpr int SAMPLE_RATE     = 44100;
constexpr int MAX_EVENT_SAMPLES = SAMPLE_RATE / 8;  // 0.125 s
constexpr int BLOCK_SIZE      = 1024;
constexpr int N_MODES         = 8;
constexpr int N_MATERIALS     = 11;
constexpr int N_SEEDS         = 5;
constexpr int N_VELOCITIES    = 3;
constexpr int N_INTERACTIONS  = 2;  // impact, footstep
constexpr int WARMUP_ITER     = 10;
constexpr int MEASURE_ITER    = 100;

// ---- material enum ----
enum Material : int {
    Stone   = 0,
    Dirt    = 1,
    Grass   = 2,
    Wood    = 3,
    Metal   = 4,
    Gravel  = 5,
    Sand    = 6,
    Snow    = 7,
    Glass   = 8,
    Water   = 9,
    Concrete = 10,
};

constexpr const char* material_name(Material m) {
    switch (m) {
        case Stone:    return "stone";
        case Dirt:     return "dirt";
        case Grass:    return "grass";
        case Wood:     return "wood";
        case Metal:    return "metal";
        case Gravel:   return "gravel";
        case Sand:     return "sand";
        case Snow:     return "snow";
        case Glass:    return "glass";
        case Water:    return "water";
        case Concrete: return "concrete";
    }
    return "unknown";
}

constexpr bool is_rigid(Material m) {
    return m == Stone || m == Wood || m == Metal || m == Glass || m == Concrete;
}

constexpr bool is_aggregate(Material m) {
    return m == Dirt || m == Grass || m == Gravel || m == Sand || m == Snow;
}

constexpr bool is_liquid(Material m) {
    return m == Water;
}

// ---- material acoustic properties ----
struct MaterialProps {
    float density;       // kg/m^3
    float young_mod;     // GPa
    float loss_factor;   // η (internal friction damping)
    float modal_freqs[N_MODES];  // Hz
    float grain_rate;    // grains/s for granular
};

constexpr MaterialProps material_props[N_MATERIALS] = {
    // density, young, loss, freqs, grain_rate
    {2700, 50,  0.010, {200, 450, 800, 1200, 1800, 2500, 3500, 5000}, 0},    // Stone
    {1500, 0.01, 0.100, {0}, 300},                                             // Dirt
    {1200, 0.005, 0.150, {0}, 200},                                            // Grass
    {700,  12,  0.020, {150, 350, 600, 900, 1300, 1800, 2500, 3500}, 0},      // Wood
    {7800, 200, 0.005, {500, 1200, 2200, 3500, 5000, 7000, 9500, 13000}, 0},  // Metal
    {1700, 0.1, 0.080, {0}, 500},                                              // Gravel
    {1600, 0.001, 0.200, {0}, 400},                                            // Sand
    {300,  0.001, 0.300, {0}, 100},                                            // Snow
    {2500, 70,  0.002, {800, 1800, 3200, 5000, 7000, 9500, 12000, 16000}, 0}, // Glass
    {1000, 0.002, 0.500, {0}, 150},                                            // Water
    {2400, 30,  0.015, {180, 400, 700, 1100, 1600, 2200, 3000, 4200}, 0},     // Concrete
};

// ---- reference audio buffer (B strategy precomputed) ----
// Precompute high-quality reference (16 modes) for each material
struct RefAudio {
    std::vector<float> samples;  // MAX_EVENT_SAMPLES per material
    float peak;
};

struct RefBank {
    std::array<RefAudio, N_MATERIALS> impacts;
    std::array<RefAudio, N_MATERIALS> footsteps;
};

static RefBank generate_reference_bank() {
    RefBank bank;
    auto fill_ref = [](int mat, bool footstep) -> RefAudio {
        const auto& p = material_props[mat];
        std::vector<float> buf(MAX_EVENT_SAMPLES, 0.0f);
        float peak = 0.0f;

        if (is_rigid(static_cast<Material>(mat)) || mat == Concrete) {
            // modal with 16 modes (2× N_MODES) for higher quality reference
            int n16 = N_MODES * 2;
            for (int i = 0; i < n16; ++i) {
                float f = p.modal_freqs[i % N_MODES] * (1.0f + 0.1f * (i / N_MODES));
                float decay = p.loss_factor * f * 6.0f + 8.0f;
                float amp = 0.3f / (1.0f + 0.2f * i);
                if (footstep) amp *= 1.5f;
                for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                    float env = std::exp(-decay * t / SAMPLE_RATE);
                    buf[t] += amp * env * std::sin(std::numbers::pi_v<float> * 2 * f * t / SAMPLE_RATE);
                }
            }
        } else if (is_aggregate(static_cast<Material>(mat))) {
            // granular reference: dense Poisson grain cloud
            int n_grains = static_cast<int>(p.grain_rate * 0.3f);
            for (int g = 0; g < n_grains; ++g) {
                int start = (g * 123457) % MAX_EVENT_SAMPLES;
                float grain_amp = 0.1f + 0.4f * ((g * 78901) % 1000) / 1000.0f;
                float grain_len = 0.002f + 0.008f * ((g * 4567) % 1000) / 1000.0f;
                int n_grain = static_cast<int>(grain_len * SAMPLE_RATE);
                for (int i = 0; i < n_grain && start + i < MAX_EVENT_SAMPLES; ++i) {
                    float env = std::exp(-6.0f * i / n_grain);
                    float noise = 2.0f * (((start + i) * 7919 + g * 104729) % 1000) / 1000.0f - 1.0f;
                    buf[start + i] += grain_amp * env * noise;
                }
            }
        } else if (is_liquid(static_cast<Material>(mat))) {
            // liquid: filtered noise burst
            for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                float env = std::exp(-4.0f * t / SAMPLE_RATE);
                float phase = 2.0f * std::numbers::pi_v<float> * std::sin(2.0f * std::numbers::pi_v<float> * t / (0.5f * SAMPLE_RATE));
                float noise = std::sin(phase + std::numbers::pi_v<float> * ((t * 7919) % 1000) / 1000.0f);
                buf[t] = 0.2f * env * noise;
            }
        }

        for (auto s : buf) peak = std::max(peak, std::abs(s));
        if (peak > 0.0f) for (auto& s : buf) s /= peak;
        return {std::move(buf), peak};
    };

    for (int m = 0; m < N_MATERIALS; ++m) {
        bank.impacts[m]  = fill_ref(m, false);
        bank.footsteps[m] = fill_ref(m, true);
    }
    return bank;
}

// ---- Strategy A: NoAudio (baseline silence) ----
// Returns an empty buffer (silence); measures function-call overhead only.
struct A_NoAudio {
    static constexpr const char* name = "A_NoAudio";
    float buffer[MAX_EVENT_SAMPLES] = {};

    float* synthesize(Material, float, int, const RefBank&) {
        return buffer;
    }
};

// ---- Strategy B: SampleBased ----
// Copies from precomputed reference with gain + pitch variation.
struct B_SampleBased {
    static constexpr const char* name = "B_SampleBased";
    float local_buf[MAX_EVENT_SAMPLES] = {};

    float* synthesize(Material mat, float velocity, int type, const RefBank& bank) {
        const auto& ref = (type == 0) ? bank.impacts[mat] : bank.footsteps[mat];
        float gain = 0.3f + 0.7f * velocity;
        int n = std::min(MAX_EVENT_SAMPLES, static_cast<int>(ref.samples.size()));
        for (int i = 0; i < n; ++i)
            local_buf[i] = ref.samples[i] * gain;
        if (n < MAX_EVENT_SAMPLES)
            for (int i = n; i < MAX_EVENT_SAMPLES; ++i)
                local_buf[i] = 0.0f;
        return local_buf;
    }
};

// ---- Strategy C: ModalSynthesis (phISAM) ----
// Bank of N=8 damped sinusoidal oscillators; parameters from material properties.
struct C_ModalSynthesis {
    static constexpr const char* name = "C_ModalSynthesis";
    float local_buf[MAX_EVENT_SAMPLES] = {};

    float* synthesize(Material mat, float velocity, int, const RefBank&) {
        const auto& p = material_props[mat];
        float vel_gain = 0.3f + 0.7f * velocity;

        for (int i = 0; i < MAX_EVENT_SAMPLES; ++i)
            local_buf[i] = 0.0f;

        if (is_rigid(mat) || mat == Concrete) {
            for (int m = 0; m < N_MODES; ++m) {
                float f  = p.modal_freqs[m];
                float decay = p.loss_factor * f * 6.0f + 6.0f;
                float amp = 0.35f / (1.0f + 0.3f * m);
                float phase = 0.0f;
                float phase_inc = 2.0f * std::numbers::pi_v<float> * f / SAMPLE_RATE;
                for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                    float env = std::exp(-decay * t / SAMPLE_RATE);
                    local_buf[t] += amp * vel_gain * env * std::sin(phase);
                    phase += phase_inc;
                }
            }
        } else {
            for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                float env = std::exp(-10.0f * t / SAMPLE_RATE);
                float phase = 2.0f * std::numbers::pi_v<float> * ((t * 7919) % 1000) / 1000.0f;
                local_buf[t] = 0.2f * vel_gain * env * std::sin(phase);
            }
        }

        return local_buf;
    }
};

// ---- Strategy D: GranularSynthesis (phISEM) ----
// Stochastic grain cloud; Poisson-distributed micro-impacts.
struct D_GranularSynthesis {
    static constexpr const char* name = "D_GranularSynthesis";
    float local_buf[MAX_EVENT_SAMPLES] = {};

    float* synthesize(Material mat, float velocity, int, const RefBank&) {
        const auto& p = material_props[mat];
        float vel_gain = 0.3f + 0.7f * velocity;

        for (int i = 0; i < MAX_EVENT_SAMPLES; ++i)
            local_buf[i] = 0.0f;

        if (is_aggregate(mat) || is_liquid(mat)) {
            // grain count scales with velocity and material grain_rate
            int n_grains = static_cast<int>(p.grain_rate * (0.1f + 0.4f * velocity));
            if (is_liquid(mat)) n_grains = static_cast<int>(p.grain_rate * (0.05f + 0.3f * velocity));

            for (int g = 0; g < n_grains; ++g) {
                int start = (g * 123457) % (MAX_EVENT_SAMPLES - 200);
                float grain_amp = 0.1f + 0.5f * ((g * 78901 + 42) % 1000) / 1000.0f;
                float grain_len = 0.002f + 0.010f * ((g * 4567 + 13) % 1000) / 1000.0f;
                int n_grain = static_cast<int>(grain_len * SAMPLE_RATE);
                for (int i = 0; i < n_grain && start + i < MAX_EVENT_SAMPLES; ++i) {
                    float env = std::exp(-5.0f * i / n_grain);
                    float noise = 2.0f * (((start + i) * 7919 + g * 104729 + 37) % 1000) / 1000.0f - 1.0f;
                    local_buf[start + i] += vel_gain * grain_amp * env * noise;
                }
            }
        } else {
            // rigid fallback → short noise burst
            for (int t = 0; t < SAMPLE_RATE / 50; ++t) {
                float env = std::exp(-50.0f * t / SAMPLE_RATE);
                float noise = 2.0f * ((t * 7919) % 1000) / 1000.0f - 1.0f;
                local_buf[t] = vel_gain * 0.3f * env * noise;
            }
        }

        return local_buf;
    }
};

// ---- Strategy E: Hybrid_ModalGranular (recommended default) ----
// Dispatches per material: rigid → modal, aggregate → granular, liquid → filtered noise.
struct E_Hybrid_ModalGranular {
    static constexpr const char* name = "E_Hybrid_ModalGranular";
    float local_buf[MAX_EVENT_SAMPLES] = {};

    float* synthesize(Material mat, float velocity, int type, const RefBank&) {
        const auto& p = material_props[mat];
        float vel_gain = 0.3f + 0.7f * velocity;

        for (int i = 0; i < MAX_EVENT_SAMPLES; ++i)
            local_buf[i] = 0.0f;

        if (is_rigid(mat) || mat == Concrete) {
            // modal (same as C but with velocity-shaped attack)
            for (int m = 0; m < N_MODES; ++m) {
                float f  = p.modal_freqs[m];
                float decay = p.loss_factor * f * 6.0f + 6.0f;
                float amp = 0.35f / (1.0f + 0.3f * m);
                float att = (type == 0) ? 1.0f : 1.5f;
                float phase = 0.0f;
                float phase_inc = 2.0f * std::numbers::pi_v<float> * f / SAMPLE_RATE;
                for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                    float env = std::exp(-decay * t / SAMPLE_RATE);
                    local_buf[t] += amp * vel_gain * att * env * std::sin(phase);
                    phase += phase_inc;
                }
            }
        } else if (is_aggregate(mat)) {
            // granular (same as D)
            int n_grains = static_cast<int>(p.grain_rate * (0.1f + 0.4f * velocity));
            if (type == 1) n_grains = static_cast<int>(n_grains * 0.7f);  // footsteps quieter for aggregate
            for (int g = 0; g < n_grains; ++g) {
                int start = (g * 123457) % (MAX_EVENT_SAMPLES - 200);
                float grain_amp = 0.1f + 0.5f * ((g * 78901 + 42) % 1000) / 1000.0f;
                float grain_len = 0.002f + 0.010f * ((g * 4567 + 13) % 1000) / 1000.0f;
                int n_grain = static_cast<int>(grain_len * SAMPLE_RATE);
                for (int i = 0; i < n_grain && start + i < MAX_EVENT_SAMPLES; ++i) {
                    float env = std::exp(-5.0f * i / n_grain);
                    float noise = 2.0f * (((start + i) * 7919 + g * 104729 + 37) % 1000) / 1000.0f - 1.0f;
                    local_buf[start + i] += vel_gain * grain_amp * env * noise;
                }
            }
        } else if (is_liquid(mat)) {
            // liquid: filtered noise + low tonal component
            for (int t = 0; t < MAX_EVENT_SAMPLES; ++t) {
                float env = std::exp(-4.0f * t / SAMPLE_RATE);
                float phase = 2.0f * std::numbers::pi_v<float>
                            * std::sin(2.0f * std::numbers::pi_v<float> * t / (0.5f * SAMPLE_RATE));
                float noise = std::sin(phase + std::numbers::pi_v<float> * ((t * 7919) % 1000) / 1000.0f);
                local_buf[t] = vel_gain * 0.2f * env * noise;
            }
        }

        return local_buf;
    }
};

// ---- PSNR computation ----
// Compare synthesized signal vs reference. Returns PSNR in dB.
static float compute_psnr(const float* synth, const float* ref, int n) {
    double mse = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; ++i) {
        double d = static_cast<double>(synth[i]) - static_cast<double>(ref[i]);
        mse += d * d;
        cnt++;
    }
    if (cnt == 0) return 0.0;
    mse /= cnt;
    if (mse < 1e-20) return 100.0;  // effectively identical
    return static_cast<float>(20.0 * std::log10(1.0 / std::sqrt(mse)));
}

// ---- Benchmark harness ----
template <typename Strategy>
static void run_strategy_bench(const RefBank& bank, std::FILE* csv) {
    Strategy strat;

    for (int seed = 0; seed < N_SEEDS; ++seed) {
        for (int vel = 0; vel < N_VELOCITIES; ++vel) {
            float velocity = (vel == 0) ? 0.2f : (vel == 1) ? 0.5f : 1.0f;
            for (int type = 0; type < N_INTERACTIONS; ++type) {
                const auto& ref_buf = (type == 0) ? bank.impacts : bank.footsteps;
                for (int m = 0; m < N_MATERIALS; ++m) {
                    Material mat = static_cast<Material>(m);

                    for (int w = 0; w < WARMUP_ITER; ++w)
                        strat.synthesize(mat, velocity, type, bank);

                    auto t0 = std::chrono::high_resolution_clock::now();
                    float* synth = nullptr;
                    for (int iter = 0; iter < MEASURE_ITER; ++iter)
                        synth = strat.synthesize(mat, velocity, type, bank);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    float us_per = std::chrono::duration<float, std::micro>(t1 - t0).count() / MEASURE_ITER;

                    float psnr = compute_psnr(synth, ref_buf[m].samples.data(),
                        std::min(MAX_EVENT_SAMPLES, static_cast<int>(ref_buf[m].samples.size())));

                    std::fprintf(csv, "%s,%s,%s,%s,%.1f,%d,%.3f,%.1f\n",
                        Strategy::name, material_name(mat),
                        (type == 0) ? "impact" : "footstep",
                        (vel == 0) ? "low" : (vel == 1) ? "med" : "high",
                        velocity, seed, us_per, psnr);
                }
            }
        }
    }
}

// ---- Block-fill benchmark (simulates audio buffer generation) ----
template <typename Strategy>
static double run_block_bench(const RefBank& bank) {
    Strategy strat;
    // synthesize one impact for each material at medium velocity
    auto t0 = std::chrono::high_resolution_clock::now();
    int blocks = 0;
    for (int iter = 0; iter < 100; ++iter) {
        for (int m = 0; m < N_MATERIALS; ++m) {
            strat.synthesize(static_cast<Material>(m), 0.5f, 0, bank);
            // simulate filling a 1024-sample block from the generated buffer
            blocks++;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float, std::micro>(t1 - t0).count() / blocks;
}

// ---- memory usage estimate ----
struct MemEstimate {
    size_t code_bytes;
    size_t data_bytes;
};

static MemEstimate measure_strategy_mem() {
    MemEstimate m{};
    // code: rough estimate from object sizes
    m.code_bytes = sizeof(A_NoAudio) + sizeof(B_SampleBased) + sizeof(C_ModalSynthesis)
                 + sizeof(D_GranularSynthesis) + sizeof(E_Hybrid_ModalGranular);
    // data: per-strategy local buffers + material props
    m.data_bytes = sizeof(RefBank) + 5 * MAX_EVENT_SAMPLES * sizeof(float);
    return m;
}

// ---- main ----
int main() {
    std::printf("=== Procedural Voxel Material Audio Benchmark ===\n");
    std::printf("Sample rate: %d Hz, max event: %d samples (%.1f ms), block: %d\n",
                SAMPLE_RATE, MAX_EVENT_SAMPLES,
                1000.0f * MAX_EVENT_SAMPLES / SAMPLE_RATE, BLOCK_SIZE);
    std::printf("Materials: %d, Seeds: %d, Velocities: %d, Interactions: %d\n",
                N_MATERIALS, N_SEEDS, N_VELOCITIES, N_INTERACTIONS);
    std::printf("Configs: %d, Iter/config: %d, Total synth calls: %d\n",
                N_MATERIALS * N_SEEDS * N_VELOCITIES * N_INTERACTIONS,
                MEASURE_ITER,
                N_MATERIALS * N_SEEDS * N_VELOCITIES * N_INTERACTIONS * MEASURE_ITER);

    // generate reference bank
    std::printf("Generating reference audio bank...\n");
    RefBank bank = generate_reference_bank();

    // open CSV
    std::FILE* csv = std::fopen("results.csv", "w");
    if (!csv) { std::fprintf(stderr, "ERROR: cannot open results.csv\n"); return 1; }
    std::fprintf(csv, "strategy,material,interaction,velocity,vel_value,seed,time_us,psnr_db\n");

    // run each strategy
    std::printf("\n--- Strategy A: NoAudio (baseline) ---\n");
    run_strategy_bench<A_NoAudio>(bank, csv);

    std::printf("--- Strategy B: SampleBased (reference) ---\n");
    run_strategy_bench<B_SampleBased>(bank, csv);

    std::printf("--- Strategy C: ModalSynthesis ---\n");
    run_strategy_bench<C_ModalSynthesis>(bank, csv);

    std::printf("--- Strategy D: GranularSynthesis ---\n");
    run_strategy_bench<D_GranularSynthesis>(bank, csv);

    std::printf("--- Strategy E: Hybrid_ModalGranular ---\n");
    run_strategy_bench<E_Hybrid_ModalGranular>(bank, csv);

    std::fclose(csv);

    // block fill benchmark
    std::printf("\n--- Block-fill benchmark (1024-sample blocks) ---\n");
    auto block_a = run_block_bench<A_NoAudio>(bank);
    auto block_b = run_block_bench<B_SampleBased>(bank);
    auto block_c = run_block_bench<C_ModalSynthesis>(bank);
    auto block_d = run_block_bench<D_GranularSynthesis>(bank);
    auto block_e = run_block_bench<E_Hybrid_ModalGranular>(bank);

    std::printf("A_NoAudio:          %.3f µs/block\n", block_a);
    std::printf("B_SampleBased:      %.3f µs/block\n", block_b);
    std::printf("C_ModalSynthesis:   %.3f µs/block\n", block_c);
    std::printf("D_GranularSynthesis: %.3f µs/block\n", block_d);
    std::printf("E_Hybrid_ModalGranular: %.3f µs/block\n", block_e);

    // summary by strategy (mean across all configs)
    std::printf("\n--- Summary (mean across all configs) ---\n");

    // compute per-strategy means by reading back the CSV
    struct SummaryRow {
        const char* name;
        double sum_time = 0;
        double sum_psnr = 0;
        int count = 0;
    };
    SummaryRow rows[5] = {
        {"A_NoAudio"}, {"B_SampleBased"}, {"C_ModalSynthesis"},
        {"D_GranularSynthesis"}, {"E_Hybrid_ModalGranular"}
    };

    std::FILE* csv_r = std::fopen("results.csv", "r");
    if (!csv_r) { std::fprintf(stderr, "ERROR: cannot reopen results.csv for reading\n"); return 1; }
    char line[256];
    std::fgets(line, sizeof(line), csv_r);  // skip header
    while (std::fgets(line, sizeof(line), csv_r)) {
        char strat[64], mat[32], inter[16], vel[8];
        float vel_val, time_us, psnr;
        int seed;
        if (std::sscanf(line, "%63[^,],%31[^,],%15[^,],%7[^,],%f,%d,%f,%f",
                        strat, mat, inter, vel, &vel_val, &seed, &time_us, &psnr) == 8)
        {
            for (auto& r : rows) {
                if (std::string_view(r.name) == strat) {
                    r.sum_time += time_us;
                    r.sum_psnr += psnr;
                    r.count++;
                    break;
                }
            }
        }
    }
    std::fclose(csv_r);

    for (auto& r : rows) {
        if (r.count > 0) {
            std::printf("%-24s  mean_time=%.4f µs  mean_psnr=%.1f dB  n=%d\n",
                        r.name, r.sum_time / r.count, r.sum_psnr / r.count, r.count);
        }
    }

    // memory estimate
    auto mem = measure_strategy_mem();
    std::printf("\n--- Memory estimate ---\n");
    std::printf("Strategy objects:     %zu bytes\n", mem.code_bytes);
    std::printf("Audio buffers + refs: %zu bytes (%.1f KiB)\n",
                mem.data_bytes, mem.data_bytes / 1024.0f);

    std::printf("\nDone. Results written to results.csv\n");
    return 0;
}
