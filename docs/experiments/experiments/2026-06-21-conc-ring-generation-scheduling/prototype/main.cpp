#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>
#include <ranges>
#include <vector>

static constexpr int CHUNK_SIZE = 8;
static constexpr int VIEW_RADIUS = 32;
static constexpr int N_WORKERS = 4;
static constexpr int MAX_FRAMES = 500;

struct ChunkPos { int32_t x{}, z{}; };
auto operator<=>(ChunkPos a, ChunkPos b) { return a.x != b.x ? a.x <=> b.x : a.z <=> b.z; }
bool operator==(ChunkPos a, ChunkPos b) { return a.x == b.x && a.z == b.z; }

static int gen_cost(ChunkPos pos, std::minstd_rand& rng) {
    std::minstd_rand local(pos.x * 7919 + pos.z * 5237 + 77777 + rng());
    std::uniform_int_distribution<int> d(4, 12);
    return d(local);
}

static double sqd(ChunkPos a, double px, double pz) {
    double dx = (a.x * CHUNK_SIZE + 4) - px;
    double dz = (a.z * CHUNK_SIZE + 4) - pz;
    return dx*dx + dz*dz;
}

static int ring3(ChunkPos a, double px, double pz) {
    double d = std::sqrt(sqd(a, px, pz));
    if (d <= 40)  return 0;
    if (d <= 120) return 1;
    return 2;
}

static int ring5(ChunkPos a, double px, double pz) {
    double d = std::sqrt(sqd(a, px, pz));
    if (d <= 20)  return 0;
    if (d <= 50)  return 1;
    if (d <= 100) return 2;
    if (d <= 180) return 3;
    return 4;
}

struct Metrics {
    int64_t completed = 0;
    int64_t stalls = 0;
    int64_t inner_total = 0, inner_done = 0;
    double covered_pct = 0;
};

static Metrics run_strategy(int strategy, bool seq_mode, int pattern, int seed) {
    std::minstd_rand rng(seed);

    // Pre-allocate known visible chunk positions
    static std::vector<ChunkPos> all_offsets;
    if (all_offsets.empty()) {
        for (int dz = -VIEW_RADIUS; dz <= VIEW_RADIUS; ++dz)
            for (int dx = -VIEW_RADIUS; dx <= VIEW_RADIUS; ++dx)
                if (dx*dx + dz*dz <= VIEW_RADIUS*VIEW_RADIUS)
                    all_offsets.push_back({dx, dz});
    }

    std::map<ChunkPos, int> remain;
    std::map<ChunkPos, bool> complete;
    std::map<ChunkPos, bool> active;
    Metrics m;
    double px{}, pz{};

    // Per-ring completion counts for fast dep-check
    int ring_complete[5] = {0, 0, 0, 0, 0};
    int ring_total[5] = {0, 0, 0, 0, 0};

    for (int frame = 0; frame < MAX_FRAMES; ++frame) {
        switch (pattern) {
            case 0: break;
            case 1: px = frame * 0.5; pz = 0; break;
            case 2:
                if (frame % 60 == 0) {
                    std::uniform_real_distribution<double> d(-100, 100);
                    px = d(rng); pz = d(rng);
                }
                break;
            case 3: { double a = frame * 0.02; px = 50*std::cos(a); pz = 50*std::sin(a); break; }
            case 4: {
                std::normal_distribution<double> w(0, 1.5);
                px += w(rng); pz += w(rng);
                break;
            }
        }

        int cx = int(px / CHUNK_SIZE);
        int cz = int(pz / CHUNK_SIZE);

        // Ensure all visible chunks have cost assigned and count rings
        for (auto& off : all_offsets) {
            ChunkPos c{cx + off.x, cz + off.z};
            if (!remain.contains(c)) {
                int cost = gen_cost(c, rng);
                remain[c] = cost;
                int r = (strategy == 2) ? ring5(c, px, pz) : ring3(c, px, pz);
                ++ring_total[r];
            }
        }

        // Tick active chunks: decrement, complete if done
        int done_this = 0;
        for (auto& [c, ip] : active) {
            if (!ip) continue;
            remain[c] -= 1;
            if (remain[c] <= 0) {
                complete[c] = true;
                ip = false;
                done_this++;
                int r = (strategy == 2) ? ring5(c, px, pz) : ring3(c, px, pz);
                ++ring_complete[r];
                if (r == 0) ++m.inner_done;
            }
        }
        m.completed += done_this;

        // Build pending list (from pre-allocated offsets)
        std::vector<ChunkPos> pending;
        pending.reserve(all_offsets.size());
        for (auto& off : all_offsets) {
            ChunkPos c{cx + off.x, cz + off.z};
            if (remain.contains(c) && !complete[c] && !active[c])
                pending.push_back(c);
        }

        // Sort pending
        if (!pending.empty()) {
            switch (strategy) {
                case 0:
                    std::ranges::sort(pending, [&](ChunkPos a, ChunkPos b) {
                        return sqd(a, px, pz) < sqd(b, px, pz);
                    });
                    break;
                case 1:
                    std::ranges::sort(pending, [&](ChunkPos a, ChunkPos b) {
                        int ra = ring3(a, px, pz), rb = ring3(b, px, pz);
                        if (ra != rb) return ra < rb;
                        return sqd(a, px, pz) < sqd(b, px, pz);
                    });
                    break;
                case 2:
                    std::ranges::sort(pending, [&](ChunkPos a, ChunkPos b) {
                        int ra = ring5(a, px, pz), rb = ring5(b, px, pz);
                        if (ra != rb) return ra < rb;
                        return sqd(a, px, pz) < sqd(b, px, pz);
                    });
                    break;
                case 3:
                    std::ranges::sort(pending, [&](ChunkPos a, ChunkPos b) {
                        int ra = ring3(a, px, pz), rb = ring3(b, px, pz);
                        if (ra != rb) return ra < rb;
                        return sqd(a, px, pz) < sqd(b, px, pz);
                    });
                    break;
            }
        }

        // Dispatch
        int dispatched = 0;
        for (auto& c : pending) {
            if (dispatched >= N_WORKERS) break;

            int cr = (strategy == 2) ? ring5(c, px, pz) : ring3(c, px, pz);

            if (seq_mode) {
                // Find lowest ring with pending work
                int min_pending_ring = 999;
                for (auto& pc : pending)
                    if (!complete[pc] && !active[pc]) {
                        int pr = (strategy == 2) ? ring5(pc, px, pz) : ring3(pc, px, pz);
                        min_pending_ring = std::min(min_pending_ring, pr);
                    }
                if (cr > min_pending_ring) continue;
            }

            if (strategy == 3) {
                // Ensure all lower rings are completely done
                bool deps_ok = true;
                for (int r = 0; r < cr; ++r)
                    if (ring_complete[r] < ring_total[r]) { deps_ok = false; break; }
                if (!deps_ok) continue;
            }

            active[c] = true;
            ++dispatched;
        }

        // Stall detection
        ChunkPos pc{cx, cz};
        if (!complete[pc]) ++m.stalls;
    }

    // Final coverage stats
    int pcx = int(px / CHUNK_SIZE), pcz = int(pz / CHUNK_SIZE);
    int total_vis = 0, covered = 0;
    for (auto& off : all_offsets) {
        ChunkPos c{pcx + off.x, pcz + off.z};
        ++total_vis;
        if (complete[c]) ++covered;
    }
    m.covered_pct = total_vis > 0 ? 100.0 * covered / total_vis : 0;
    m.inner_total = ring_total[0];

    return m;
}

int main() {
    std::fprintf(stderr, "=== Running 80 configs (500 frames each, 32 chunk radius, 4 workers) ===\n");

    std::printf("strategy,sname,movement,mname,seed,seq,"
                "completed,stalls,inner_done,inner_total,"
                "inner_pct,covered_pct\n");

    int total_configs = 4 * 5 * 2 * 2;
    int done_configs = 0;

    for (int s = 0; s < 4; ++s)
        for (int p = 0; p < 5; ++p)
            for (int seed : {1, 42})
                for (int seq : {0, 1}) {
                    auto m = run_strategy(s, seq == 1, p, seed);
                    const char* sn[] = {"A_DistSorted", "B_ConcRing3", "C_ConcRing5", "D_SeqRings"};
                    const char* mn[] = {"stationary", "linear_walk", "teleport", "circular", "rand_walk"};

                    double ipct = m.inner_total > 0 ? 100.0 * m.inner_done / m.inner_total : 0;

                    std::printf("%d,%s,%d,%s,%d,%d,"
                                "%ld,%ld,%ld,%ld,"
                                "%.1f,%.1f\n",
                                s, sn[s], p, mn[p], seed, seq,
                                m.completed, m.stalls,
                                m.inner_done, m.inner_total,
                                ipct, m.covered_pct);

                    ++done_configs;
                    if (done_configs % 20 == 0)
                        std::fprintf(stderr, "  progress: %d/%d\n", done_configs, total_configs);
                }

    std::fprintf(stderr, "\n=== MEANS (non-sequential dispatch) ===\n");
    for (int s = 0; s < 4; ++s) {
        const char* sn[] = {"A_DistSorted", "B_ConcRing3", "C_ConcRing5", "D_SeqRings"};
        double stalls = 0, inner_pct_sum = 0, covered_sum = 0, comp_sum = 0;
        int n = 0;
        for (int p = 0; p < 5; ++p)
            for (int seed : {1, 42}) {
                auto m = run_strategy(s, false, p, seed);
                stalls += m.stalls;
                double ipct = m.inner_total > 0 ? 100.0 * m.inner_done / m.inner_total : 0;
                inner_pct_sum += ipct;
                covered_sum += m.covered_pct;
                comp_sum += m.completed;
                ++n;
            }
        std::fprintf(stderr, "  %s: comp=%.0f stalls=%.1f inner=%.1f%% covered=%.1f%%\n",
                     sn[s], comp_sum / n, stalls / n, inner_pct_sum / n, covered_sum / n);
    }

    std::fprintf(stderr, "\n=== MEANS (sequential-ring dispatch) ===\n");
    for (int s = 0; s < 4; ++s) {
        const char* sn[] = {"A_DistSorted", "B_ConcRing3", "C_ConcRing5", "D_SeqRings"};
        double stalls = 0, inner_pct_sum = 0, covered_sum = 0, comp_sum = 0;
        int n = 0;
        for (int p = 0; p < 5; ++p)
            for (int seed : {1, 42}) {
                auto m = run_strategy(s, true, p, seed);
                stalls += m.stalls;
                double ipct = m.inner_total > 0 ? 100.0 * m.inner_done / m.inner_total : 0;
                inner_pct_sum += ipct;
                covered_sum += m.covered_pct;
                comp_sum += m.completed;
                ++n;
            }
        std::fprintf(stderr, "  %s: comp=%.0f stalls=%.1f inner=%.1f%% covered=%.1f%%\n",
                     sn[s], comp_sum / n, stalls / n, inner_pct_sum / n, covered_sum / n);
    }

    return 0;
}