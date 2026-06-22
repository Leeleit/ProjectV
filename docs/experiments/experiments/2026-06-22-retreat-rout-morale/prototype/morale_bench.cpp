// Standalone C++26 CPU prototype for 2026-06-22-retreat-rout-morale
// 5 strategies × 5 scenarios × 5 unit_counts × 5 casualty_rates × 5 seeds × 1000 iter + 10 warmup = 625,000 main measurements
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic morale_bench.cpp -o build/morale_bench

#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <fstream>
#include <string>
#include <map>
#include <cmath>

enum MoraleState { STEADY = 0, SHAKEN = 1, BREAKING = 2, ROUT = 3 };
const char* STATE_NAMES[] = {"STEADY", "SHAKEN", "BREAKING", "ROUT"};

struct Unit {
    int id;
    float morale;        // 0-100, higher = better
    int state;           // STEADY/SHAKEN/BREAKING/ROUT
    float cas_acc;       // casualty accumulator
    float sup_acc;       // suppression accumulator
    float iso_acc;       // isolation accumulator
    float pos_x, pos_z;
    bool leader_alive;
};

// Strategy A: NaiveLinearDecay
double RunNaive(std::vector<Unit>& units, float casualty_rate, float sup_rate, float dt) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& u : units) {
        float delta = casualty_rate * 5.0f + sup_rate * 2.0f + (u.leader_alive ? 0 : 10.0f);
        u.morale -= delta * dt;
        if (u.morale < 5.0f) u.state = ROUT;
        else if (u.morale < 20.0f) u.state = BREAKING;
        else if (u.morale < 50.0f) u.state = SHAKEN;
        else u.state = STEADY;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy B: SigmoidThreshold
double RunSigmoid(std::vector<Unit>& units, float casualty_rate, float sup_rate, float dt) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& u : units) {
        float x = casualty_rate * 0.3f + sup_rate * 0.1f + (u.leader_alive ? 0 : 0.5f);
        float sig = 1.0f / (1.0f + std::exp(-x));
        u.morale = 100.0f * (1.0f - sig);
        if (u.morale < 5.0f) u.state = ROUT;
        else if (u.morale < 20.0f) u.state = BREAKING;
        else if (u.morale < 50.0f) u.state = SHAKEN;
        else u.state = STEADY;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy C: AccumulatorDecay
double RunAccumulator(std::vector<Unit>& units, float casualty_rate, float sup_rate, float dt) {
    auto t0 = std::chrono::high_resolution_clock::now();
    float decay = std::exp(-dt * 0.5f); // τ = 2 sec
    for (auto& u : units) {
        u.cas_acc = u.cas_acc * decay + casualty_rate * dt;
        u.sup_acc = u.sup_acc * decay + sup_rate * dt;
        float morale_loss = u.cas_acc * 30.0f + u.sup_acc * 15.0f + (u.leader_alive ? 0 : 25.0f);
        u.morale = 100.0f - morale_loss;
        if (u.morale < 5.0f) u.state = ROUT;
        else if (u.morale < 20.0f) u.state = BREAKING;
        else if (u.morale < 50.0f) u.state = SHAKEN;
        else u.state = STEADY;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy D: StackedBreakpoint (HoI4-style)
double RunBreakpoint(std::vector<Unit>& units, float casualty_rate, float sup_rate, float dt) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& u : units) {
        // State machine transitions
        if (u.state == STEADY) {
            if (casualty_rate > 0.5f) u.state = SHAKEN;
            else if (sup_rate > 5.0f) u.state = SHAKEN;
            if (!u.leader_alive) u.state = BREAKING;
        } else if (u.state == SHAKEN) {
            if (casualty_rate > 1.0f) u.state = BREAKING;
            if (casualty_rate > 2.0f || sup_rate > 10.0f) u.state = ROUT;
            // Recovery
            if (casualty_rate < 0.1f && sup_rate < 1.0f) {
                u.morale += dt * 2.0f;
                if (u.morale > 50.0f) u.state = STEADY;
            }
        } else if (u.state == BREAKING) {
            if (casualty_rate > 2.0f || sup_rate > 10.0f) u.state = ROUT;
            if (casualty_rate < 0.05f && sup_rate < 0.5f) {
                u.morale += dt * 1.0f;
                if (u.morale > 20.0f) u.state = SHAKEN;
            }
        } else if (u.state == ROUT) {
            // Rout state: slow recovery only if threat completely removed
            if (casualty_rate < 0.01f && sup_rate < 0.1f) {
                u.morale += dt * 0.5f;
                if (u.morale > 5.0f) u.state = BREAKING;
            }
        }
        u.morale -= casualty_rate * 5.0f * dt + sup_rate * 2.0f * dt;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy E: Hybrid Sigmoid + State Machine
double RunHybrid(std::vector<Unit>& units, float casualty_rate, float sup_rate, float dt) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& u : units) {
        float x = casualty_rate * 0.3f + sup_rate * 0.1f + (u.leader_alive ? 0 : 0.5f);
        float sig = 1.0f / (1.0f + std::exp(-x));
        u.morale = 100.0f * (1.0f - sig);
        // State machine behavior layer
        if (u.morale < 5.0f) u.state = ROUT;
        else if (u.morale < 20.0f) u.state = BREAKING;
        else if (u.morale < 50.0f) u.state = SHAKEN;
        else u.state = STEADY;
        // Recovery: slow ramp when threats cleared
        if (casualty_rate < 0.1f && sup_rate < 1.0f && u.state > STEADY) {
            u.morale += dt * 1.0f;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

int main() {
    const char* strategies[] = {"A_NaiveLinearDecay", "B_SigmoidThreshold", "C_AccumulatorDecay", "D_StackedBreakpoint", "E_Hybrid_SigmoidWithStateMachine"};
    const char* scenarios[] = {"s1_steady_patrol", "s2_under_fire", "s3_heavy_casualties", "s4_isolated_squad", "s5_mixed_combined_arms"};
    int unit_counts[] = {32, 64, 128, 256, 512};
    float casualty_rates[] = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f};
    int seeds[] = {1, 7, 42, 1234, 31337};

    std::ofstream out("prototype/build/results.csv");
    out << "Strategy,Scenario,UnitCount,CasualtyRate,Seed,Mean_ns\n";

    for (int si = 0; si < 5; ++si) {
        for (int sci = 0; sci < 5; ++sci) {
            for (int ui = 0; ui < 5; ++ui) {
                for (int cri = 0; cri < 5; ++cri) {
                    for (int sdi = 0; sdi < 5; ++sdi) {
                        std::mt19937 rng(seeds[sdi]);
                        std::uniform_real_distribution<float> posDis(-50.0f, 50.0f);
                        std::bernoulli_distribution leaderDis(0.5f);

                        int N = unit_counts[ui];
                        std::vector<Unit> units;
                        units.reserve(N);
                        for (int i = 0; i < N; ++i) {
                            Unit u;
                            u.id = i;
                            u.morale = 100.0f;
                            u.state = STEADY;
                            u.cas_acc = 0;
                            u.sup_acc = 0;
                            u.iso_acc = 0;
                            u.pos_x = posDis(rng);
                            u.pos_z = posDis(rng);
                            u.leader_alive = leaderDis(rng);
                            units.push_back(u);
                        }
                        float cas_rate = casualty_rates[cri];
                        float sup_rate = cas_rate * 2.0f;
                        float dt = 1.0f / 30.0f;

                        std::vector<double> samples;
                        for (int i = 0; i < 1010; ++i) {
                            // Reset some units periodically for stable measurement
                            if (i % 100 == 0) {
                                for (auto& u : units) {
                                    u.morale = 100.0f;
                                    u.state = STEADY;
                                    u.cas_acc = 0;
                                    u.sup_acc = 0;
                                }
                            }
                            double t;
                            switch (si) {
                                case 0: t = RunNaive(units, cas_rate, sup_rate, dt); break;
                                case 1: t = RunSigmoid(units, cas_rate, sup_rate, dt); break;
                                case 2: t = RunAccumulator(units, cas_rate, sup_rate, dt); break;
                                case 3: t = RunBreakpoint(units, cas_rate, sup_rate, dt); break;
                                case 4: t = RunHybrid(units, cas_rate, sup_rate, dt); break;
                            }
                            if (i >= 10) samples.push_back(t);
                        }
                        double sum = 0;
                        for (double v : samples) sum += v;
                        double mean = sum / samples.size();
                        out << strategies[si] << "," << scenarios[sci] << "," << unit_counts[ui] << "," << casualty_rates[cri] << "," << seeds[sdi] << "," << mean << "\n";
                    }
                }
            }
        }
    }
    out.close();

    std::map<std::string, std::map<std::string, std::vector<double>>> sum_data;
    std::ifstream in("prototype/build/results.csv");
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        size_t p1 = line.find(',');
        std::string strat = line.substr(0, p1);
        size_t p2 = line.find(',', p1+1);
        std::string scenario = line.substr(p1+1, p2-p1-1);
        size_t p3 = line.find(',', p2+1);
        size_t p4 = line.find(',', p3+1);
        size_t p5 = line.find(',', p4+1);
        size_t p6 = line.find(',', p5+1);
        std::string nsStr = (p6 == std::string::npos) ? line.substr(p5+1) : line.substr(p6+1);
        try {
            double ns = std::stod(nsStr);
            sum_data[strat][scenario].push_back(ns);
        } catch (...) {}
    }
    in.close();

    std::ofstream sum("prototype/build/summary_means.csv");
    sum << "Strategy,Mean_ns\n";
    for (int si = 0; si < 5; ++si) {
        double total = 0; int count = 0;
        for (auto& scn_pair : sum_data[strategies[si]]) {
            for (double v : scn_pair.second) { total += v; ++count; }
        }
        sum << strategies[si] << "," << (total/count) << "\n";
    }
    sum.close();

    std::cout << "Wrote results.csv and summary_means.csv\n";
    return 0;
}