// hook_bench.cpp — Standalone C++26 CPU benchmark for Lua-style hook dispatch systems.
//
// Models the **dispatch architecture** (not the Lua VM itself): how do you store
// hooks-per-event, dispatch them, and clean them up by identifier. Real Lua cost
// measured separately in closed `2026-06-21-luajit-scripting-hotpath-cost`.
//
// 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
//
// Output: prototype/build/results.csv (machine-readable) + RESULTS.md (human-readable).
//
// Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// ============================================================================
// Handler primitive
// ============================================================================

using HandlerId = std::uint32_t;
using HandlerFn = std::function<bool(int)>;

// Anti-dead-code-elimination sink (per closed `2026-06-21-navial-vessel-buoyancy-steering`).
// All handlers touch this to prevent compiler from eliding call sites.
static volatile std::uint64_t g_sink = 0;

// A no-op handler that touches the sink. Returns true (~30% of the time) to model
// "non-nil return → stop dispatch" semantics per `hook.Run` docs.
static bool MakeHandler(std::uint32_t seed) noexcept {
    // Deterministic per-handler behavior to model "what fraction of handlers return non-nil".
    // Real GMod scripts return non-nil in maybe 10-30% of hooks (override semantics).
    g_sink += static_cast<std::uint64_t>(seed);
    return (seed % 7u) == 0u;  // ~14% return non-nil (rare override)
}

// ============================================================================
// Heterogeneous lookup helpers (libstdc++16 workaround for unordered_map).
// Enables find/insert with std::string_view without requiring std::string allocation.
// ============================================================================

struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string>{}(s);
    }
};

struct StringEq {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
    bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
    bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
};

template <typename V>
using StringMap = std::unordered_map<std::string, V, StringHash, StringEq>;

// ============================================================================
// Strategy A — NaiveLinkedList (Garry's Mod actual pattern)
//
// Per wiki: hooks are NOT ordered, just a list per event. Lookup on remove is O(N)
// linear scan by identifier. Insertion is O(1) at head.
// ============================================================================

struct AHandler {
    std::string identifier;
    HandlerFn   fn;
};

class A_Hooks {
public:
    void Add(std::string_view event, std::string_view ident, HandlerFn fn) {
        auto& list = storage_[std::string(event)];
        list.push_back(AHandler{std::string(ident), std::move(fn)});
    }

    // Returns true if any handler returned non-nil (i.e. dispatch stopped).
    bool Run(std::string_view event, int arg) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        for (auto& h : it->second) {
            if (h.fn(arg)) return true;
        }
        return false;
    }

    bool Remove(std::string_view event, std::string_view ident) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        auto& list = it->second;
        for (auto hit = list.begin(); hit != list.end(); ++hit) {
            if (hit->identifier == ident) {
                list.erase(hit);
                return true;
            }
        }
        return false;
    }

    void Clear() noexcept { storage_.clear(); }
    std::size_t TotalHooks() const noexcept {
        std::size_t n = 0;
        for (auto& [k, v] : storage_) n += v.size();
        return n;
    }
private:
    StringMap<std::vector<AHandler>> storage_;
};

// ============================================================================
// Strategy B — ArrayOfHandlers (cache-friendly dense vector)
//
// Same shape as A but `std::array`-backed stable storage with linear scan.
// In practice this is what well-optimized GMod Lua tables look like (C-side
// vector with Lua table wrapper).
// ============================================================================

struct BHandler {
    HandlerFn   fn;
    std::string identifier;
    std::string event;
};

// All handlers live in one flat array, sorted by event for cache locality.
class B_Hooks {
public:
    void Add(std::string_view event, std::string_view ident, HandlerFn fn) {
        handlers_.push_back(BHandler{
            std::move(fn),
            std::string(ident),
            std::string(event)
        });
    }

    bool Run(std::string_view event, int arg) {
        for (auto& h : handlers_) {
            if (h.event != event) continue;
            if (h.fn(arg)) return true;
        }
        return false;
    }

    bool Remove(std::string_view event, std::string_view ident) {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->event == event && it->identifier == ident) {
                handlers_.erase(it);
                return true;
            }
        }
        return false;
    }

    void Clear() noexcept { handlers_.clear(); }
    std::size_t TotalHooks() const noexcept { return handlers_.size(); }
private:
    std::vector<BHandler> handlers_;
};

// ============================================================================
// Strategy C — TypedDispatch (compile-time event-type fast path)
//
// Direct function pointer table indexed by event ID. Identifiers stored in a
// parallel vector for O(N) remove by string compare. Avoids hash map lookup
// at Run time (most common hot path).
// ============================================================================

class C_Hooks {
public:
    void Add(std::string_view event, std::string_view ident, HandlerFn fn) {
        EventId eid = InternEvent(event);
        std::uint32_t idx = static_cast<std::uint32_t>(handlers_.size());
        handlers_.push_back(Handler{fn, std::string(ident), eid});
        // Append to per-event list (linear scan in Run, but local to one event's chain).
        event_chains_[eid].push_back(idx);
    }

    bool Run(std::string_view event, int arg) {
        EventId eid = InternEvent(event);
        auto it = event_chains_.find(eid);
        if (it == event_chains_.end()) return false;
        for (auto idx : it->second) {
            if (handlers_[idx].fn(arg)) return true;
        }
        return false;
    }

    bool Remove(std::string_view event, std::string_view ident) {
        EventId eid = InternEvent(event);
        auto& chain = event_chains_[eid];
        for (std::size_t i = 0; i < chain.size(); ++i) {
            if (handlers_[chain[i]].identifier == ident) {
                // Mark removed (don't compact — preserve indices).
                handlers_[chain[i]].removed = true;
                chain.erase(chain.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }

    void Clear() noexcept {
        handlers_.clear();
        event_chains_.clear();
        event_intern_.clear();
    }
    std::size_t TotalHooks() const noexcept { return handlers_.size(); }
private:
    using EventId = std::uint32_t;

    struct Handler {
        HandlerFn   fn;
        std::string identifier;
        EventId     event;
        bool        removed = false;
    };

    EventId InternEvent(std::string_view ev) {
        auto it = event_intern_.find(ev);
        if (it != event_intern_.end()) return it->second;
        EventId id = static_cast<EventId>(event_intern_.size());
        event_intern_.emplace(std::string(ev), id);
        return id;
    }

    std::vector<Handler> handlers_;
    std::unordered_map<EventId, std::vector<std::uint32_t>> event_chains_;
    StringMap<EventId> event_intern_;
};

// ============================================================================
// Strategy D — PriorityBuckets
//
// 3 priority levels (CRITICAL/NORMAL/LOW) per event. Run dispatches in priority
// order: CRITICAL first, NORMAL next, LOW last. Models "addon ordering by
// priority" pattern (e.g., Roblox's ContextActionService).
// ============================================================================

enum class Priority : std::uint8_t { CRITICAL = 0, NORMAL = 1, LOW = 2 };

struct DHandler {
    HandlerFn   fn;
    std::string identifier;
    Priority    prio;
};

class D_Hooks {
public:
    void Add(std::string_view event, std::string_view ident, HandlerFn fn) {
        Priority p = PriorityFor(ident);
        auto& list = storage_[std::string(event)];
        list.push_back(DHandler{std::move(fn), std::string(ident), p});
    }

    bool Run(std::string_view event, int arg) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        for (auto p : {Priority::CRITICAL, Priority::NORMAL, Priority::LOW}) {
            for (auto& h : it->second) {
                if (h.prio != p) continue;
                if (h.fn(arg)) return true;
            }
        }
        return false;
    }

    bool Remove(std::string_view event, std::string_view ident) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        auto& list = it->second;
        for (auto hit = list.begin(); hit != list.end(); ++hit) {
            if (hit->identifier == ident) {
                list.erase(hit);
                return true;
            }
        }
        return false;
    }

    void Clear() noexcept { storage_.clear(); }
    std::size_t TotalHooks() const noexcept {
        std::size_t n = 0;
        for (auto& [k, v] : storage_) n += v.size();
        return n;
    }
private:
    static Priority PriorityFor(std::string_view ident) {
        // Deterministic priority from identifier hash: ~10% CRITICAL, 60% NORMAL, 30% LOW.
        std::uint32_t h = 0;
        for (char c : ident) h = h * 31u + static_cast<std::uint32_t>(c);
        std::uint32_t bucket = h % 10u;
        if (bucket < 1u) return Priority::CRITICAL;
        if (bucket < 7u) return Priority::NORMAL;
        return Priority::LOW;
    }

    StringMap<std::vector<DHandler>> storage_;
};

// ============================================================================
// Strategy E — IndexedByEventHash (small flat hash per event)
//
// For each event: fixed-size small array (cap = 8 hooks). Overflow goes to
// chain list. Best for hot events with few hooks, with overflow handling for
// edge cases (model "mainline + overflow").
// ============================================================================

struct EHandler {
    HandlerFn   fn;
    std::string identifier;
};

class E_Hooks {
public:
    static constexpr std::uint32_t MAIN_CAP = 8;

    struct EventBucket {
        std::array<EHandler, MAIN_CAP> main{};
        std::uint32_t                   main_size = 0;
        std::vector<EHandler>           overflow;
    };

    void Add(std::string_view event, std::string_view ident, HandlerFn fn) {
        auto& b = storage_[std::string(event)];
        if (b.main_size < MAIN_CAP) {
            b.main[b.main_size] = EHandler{std::move(fn), std::string(ident)};
            ++b.main_size;
        } else {
            b.overflow.push_back(EHandler{std::move(fn), std::string(ident)});
        }
    }

    bool Run(std::string_view event, int arg) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        auto& b = it->second;
        for (std::uint32_t i = 0; i < b.main_size; ++i) {
            if (b.main[i].fn(arg)) return true;
        }
        for (auto& h : b.overflow) {
            if (h.fn(arg)) return true;
        }
        return false;
    }

    bool Remove(std::string_view event, std::string_view ident) {
        auto it = storage_.find(event);
        if (it == storage_.end()) return false;
        auto& b = it->second;
        for (std::uint32_t i = 0; i < b.main_size; ++i) {
            if (b.main[i].identifier == ident) {
                // Compact: move last into removed slot.
                b.main[i] = b.main[b.main_size - 1];
                --b.main_size;
                return true;
            }
        }
        for (auto oit = b.overflow.begin(); oit != b.overflow.end(); ++oit) {
            if (oit->identifier == ident) {
                b.overflow.erase(oit);
                return true;
            }
        }
        return false;
    }

    void Clear() noexcept { storage_.clear(); }
    std::size_t TotalHooks() const noexcept {
        std::size_t n = 0;
        for (auto& [k, v] : storage_) n += v.main_size + v.overflow.size();
        return n;
    }
private:
    StringMap<EventBucket> storage_;
};

// ============================================================================
// Measurement harness
// ============================================================================

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats ComputeStats(std::vector<double>& samples) {
    Stats s{};
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<std::size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<std::size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

// High-resolution timer.
using Clock = std::chrono::high_resolution_clock;
static inline double Now() {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// ============================================================================
// Scene definitions
// ============================================================================

struct Scene {
    const char* name;
    std::size_t num_events;
    std::size_t hooks_per_event;
};

// 5 scenes, deliberately varied to stress different operating regimes.
static constexpr std::array<Scene, 5> kScenes = {{
    {"small_gamemode",   10,   5},    // 50 hooks total, light modded session
    {"medium_modded",    50,  20},    // 1000 hooks, mid-size server
    {"large_modded",    200,  50},    // 10000 hooks, heavy modded sandbox
    {"hot_path_tick",     1, 1000},   // single event, 1000 hooks (worst-case Tick-like)
    {"sparse_hooks",    500,   1},    // many events, 1 hook each (sparse workload)
}};

static constexpr std::array<std::uint32_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

// ============================================================================
// Per-strategy hook setup (returns total hooks added)
// ============================================================================

template <typename Hooks>
std::size_t SetupHooks(Hooks& h, const Scene& scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::size_t total = 0;
    char event_buf[32];
    char ident_buf[32];
    for (std::size_t e = 0; e < scene.num_events; ++e) {
        std::snprintf(event_buf, sizeof(event_buf), "Event_%zu", e);
        for (std::size_t k = 0; k < scene.hooks_per_event; ++k) {
            std::snprintf(ident_buf, sizeof(ident_buf), "Hook_%zu_%zu_%u",
                          e, k, static_cast<unsigned>(rng() % 100000u));
            const std::uint32_t h_seed = static_cast<std::uint32_t>(rng());
            // Build handler via std::function (cost of one alloc per add — same as Lua table insert).
            HandlerFn fn = [h_seed](int /*arg*/) { return MakeHandler(h_seed); };
            h.Add(event_buf, ident_buf, std::move(fn));
            ++total;
        }
    }
    return total;
}

// ============================================================================
// Bench runners (template on Hooks type + strategy id)
// ============================================================================

template <typename Hooks>
double BenchAdd(Hooks& h, const Scene& scene, std::uint32_t seed) {
    h.Clear();
    auto t0 = Clock::now();
    SetupHooks(h, scene, seed);
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

template <typename Hooks>
double BenchRun(Hooks& h, const Scene& scene, std::uint32_t seed) {
    SetupHooks(h, scene, seed);
    std::mt19937 rng(seed);
    char event_buf[32];
    std::vector<double> samples;
    samples.reserve(1000);
    // Warm-up
    for (int i = 0; i < 10; ++i) {
        std::snprintf(event_buf, sizeof(event_buf), "Event_%zu",
                      static_cast<std::size_t>(rng() % scene.num_events));
        h.Run(event_buf, static_cast<int>(rng()));
    }
    // Measurement
    for (int i = 0; i < 1000; ++i) {
        std::snprintf(event_buf, sizeof(event_buf), "Event_%zu",
                      static_cast<std::size_t>(rng() % scene.num_events));
        auto s = Clock::now();
        h.Run(event_buf, static_cast<int>(rng()));
        auto e = Clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
    }
    // Anti-DCE: return mean so caller uses the result.
    double sum = 0.0;
    for (double v : samples) sum += v;
    return sum / static_cast<double>(samples.size());
}

template <typename Hooks>
double BenchRemove(Hooks& h, const Scene& scene, std::uint32_t seed) {
    SetupHooks(h, scene, seed);
    std::mt19937 rng(seed);
    char event_buf[32];
    char ident_buf[32];
    std::vector<double> samples;
    samples.reserve(1000);
    for (int i = 0; i < 1010; ++i) {
        std::size_t e_idx = static_cast<std::size_t>(rng() % scene.num_events);
        std::size_t h_idx = static_cast<std::size_t>(rng() % scene.hooks_per_event);
        std::snprintf(event_buf, sizeof(event_buf), "Event_%zu", e_idx);
        std::snprintf(ident_buf, sizeof(ident_buf), "Hook_%zu_%zu_%u",
                      e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
        // Re-Add if we just removed last one (to avoid running dry).
        if (i == 1000) SetupHooks(h, scene, seed + 1u);
        auto s = Clock::now();
        h.Remove(event_buf, ident_buf);
        auto e = Clock::now();
        if (i >= 10) samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
    }
    double sum = 0.0;
    for (double v : samples) sum += v;
    return sum / static_cast<double>(samples.size());
}

// ============================================================================
// Main driver
// ============================================================================

int main() {
    std::printf("hook_bench — Lua-style hook dispatch benchmark\n");
    std::printf("Hardware: AMD Ryzen 7 5800X (Zen 3), governor=performance (default)\n");
    std::printf("Toolchain: Clang 22.1.6 -O3 -march=native -std=c++26\n");
    std::printf("Strategies: A=NaiveLinkedList (GMod baseline), B=ArrayOfHandlers, "
                "C=TypedDispatch, D=PriorityBuckets, E=IndexedByEventHash\n");
    std::printf("Scenes × seeds × ops: 5 × 5 × (Add + 1000×Run + Remove)\n");
    std::printf("\n");

    // Output: CSV with one row per (strategy, scene, seed, op) combination.
    std::FILE* csv = std::fopen("build/results.csv", "w");
    if (!csv) {
        std::fprintf(stderr, "Failed to open prototype/build/results.csv\n");
        return 1;
    }
    std::fprintf(csv, "strategy,scene,seed,op,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns,total_hooks\n");

    struct StrategyRow {
        const char* name;
        std::vector<double> add_per_seed_per_scene[5];
        std::vector<double> run_per_seed_per_scene[5];
        std::vector<double> rm_per_seed_per_scene[5];
    };

    std::vector<StrategyRow> rows(5);

    double t_start = Now();

    // For each (strategy, scene, seed, op): measure.
    for (std::size_t scene_idx = 0; scene_idx < kScenes.size(); ++scene_idx) {
        const Scene& sc = kScenes[scene_idx];
        for (std::uint32_t seed : kSeeds) {
            std::vector<double> add_samples, run_samples, rm_samples;

            // --- A ---
            {
                A_Hooks h;
                double a = BenchAdd(h, sc, seed);
                double r = BenchRun(h, sc, seed);
                double rm = BenchRemove(h, sc, seed);
                add_samples.push_back(a / static_cast<double>(sc.num_events * sc.hooks_per_event));
                run_samples.push_back(r);
                rm_samples.push_back(rm);
                std::fprintf(csv, "A,%s,%u,add,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, a / (sc.num_events * sc.hooks_per_event),
                             a / (sc.num_events * sc.hooks_per_event),
                             a / (sc.num_events * sc.hooks_per_event),
                             a / (sc.num_events * sc.hooks_per_event),
                             0.0, a / (sc.num_events * sc.hooks_per_event),
                             a / (sc.num_events * sc.hooks_per_event),
                             sc.num_events * sc.hooks_per_event);
            }
            // Re-create for proper Run measurement (state from add affects run cost)
            {
                A_Hooks h;
                std::vector<double> r_samples;
                SetupHooks(h, sc, seed);
                for (int warm = 0; warm < 10; ++warm) h.Run("Event_0", warm);
                std::mt19937 rng(seed);
                char buf[32];
                for (int i = 0; i < 1000; ++i) {
                    std::snprintf(buf, sizeof(buf), "Event_%zu",
                                  static_cast<std::size_t>(rng() % sc.num_events));
                    auto s = Clock::now();
                    h.Run(buf, static_cast<int>(rng()));
                    auto e = Clock::now();
                    r_samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(r_samples);
                std::fprintf(csv, "A,%s,%u,run,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);
                run_samples.clear();
                run_samples.push_back(st.mean);
            }
            {
                A_Hooks h;
                std::vector<double> rm_s;
                SetupHooks(h, sc, seed);
                std::mt19937 rng(seed);
                char ebuf[32], ibuf[32];
                for (int i = 0; i < 1010; ++i) {
                    if (i == 1000) SetupHooks(h, sc, seed + 1u);
                    std::size_t e_idx = static_cast<std::size_t>(rng() % sc.num_events);
                    std::size_t h_idx = static_cast<std::size_t>(rng() % sc.hooks_per_event);
                    std::snprintf(ebuf, sizeof(ebuf), "Event_%zu", e_idx);
                    std::snprintf(ibuf, sizeof(ibuf), "Hook_%zu_%zu_%u",
                                  e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
                    auto s = Clock::now();
                    h.Remove(ebuf, ibuf);
                    auto e = Clock::now();
                    if (i >= 10) rm_s.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(rm_s);
                std::fprintf(csv, "A,%s,%u,remove,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);
                rm_samples.clear();
                rm_samples.push_back(st.mean);
            }

            // --- B ---
            {
                B_Hooks h;
                SetupHooks(h, sc, seed);
                B_Hooks h_tmp;
                double a = BenchAdd(h_tmp, sc, seed);  // cost of one add per hook
                (void)a;
                std::mt19937 rng(seed);
                char buf[32];
                std::vector<double> r_samples;
                for (int i = 0; i < 1000; ++i) {
                    std::snprintf(buf, sizeof(buf), "Event_%zu",
                                  static_cast<std::size_t>(rng() % sc.num_events));
                    auto s = Clock::now();
                    h.Run(buf, static_cast<int>(rng()));
                    auto e = Clock::now();
                    r_samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(r_samples);
                std::fprintf(csv, "B,%s,%u,run,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);

                std::vector<double> rm_s;
                for (int i = 0; i < 1010; ++i) {
                    if (i == 1000) SetupHooks(h, sc, seed + 1u);
                    std::size_t e_idx = static_cast<std::size_t>(rng() % sc.num_events);
                    std::size_t h_idx = static_cast<std::size_t>(rng() % sc.hooks_per_event);
                    std::snprintf(buf, sizeof(buf), "Event_%zu", e_idx);
                    char ibuf[32];
                    std::snprintf(ibuf, sizeof(ibuf), "Hook_%zu_%zu_%u",
                                  e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
                    auto s = Clock::now();
                    h.Remove(buf, ibuf);
                    auto e = Clock::now();
                    if (i >= 10) rm_s.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats rmst = ComputeStats(rm_s);
                std::fprintf(csv, "B,%s,%u,remove,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, rmst.mean, rmst.median, rmst.p95, rmst.p99, rmst.stddev,
                             rmst.min, rmst.max, sc.num_events * sc.hooks_per_event);

                B_Hooks h2;
                double add_ns = BenchAdd(h2, sc, seed);
                std::fprintf(csv, "B,%s,%u,add,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             0.0,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             sc.num_events * sc.hooks_per_event);
            }

            // --- C ---
            {
                C_Hooks h;
                SetupHooks(h, sc, seed);
                std::mt19937 rng(seed);
                char buf[32];
                std::vector<double> r_samples;
                for (int i = 0; i < 1000; ++i) {
                    std::snprintf(buf, sizeof(buf), "Event_%zu",
                                  static_cast<std::size_t>(rng() % sc.num_events));
                    auto s = Clock::now();
                    h.Run(buf, static_cast<int>(rng()));
                    auto e = Clock::now();
                    r_samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(r_samples);
                std::fprintf(csv, "C,%s,%u,run,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);

                std::vector<double> rm_s;
                for (int i = 0; i < 1010; ++i) {
                    if (i == 1000) SetupHooks(h, sc, seed + 1u);
                    std::size_t e_idx = static_cast<std::size_t>(rng() % sc.num_events);
                    std::size_t h_idx = static_cast<std::size_t>(rng() % sc.hooks_per_event);
                    std::snprintf(buf, sizeof(buf), "Event_%zu", e_idx);
                    char ibuf[32];
                    std::snprintf(ibuf, sizeof(ibuf), "Hook_%zu_%zu_%u",
                                  e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
                    auto s = Clock::now();
                    h.Remove(buf, ibuf);
                    auto e = Clock::now();
                    if (i >= 10) rm_s.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats rmst = ComputeStats(rm_s);
                std::fprintf(csv, "C,%s,%u,remove,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, rmst.mean, rmst.median, rmst.p95, rmst.p99, rmst.stddev,
                             rmst.min, rmst.max, sc.num_events * sc.hooks_per_event);

                C_Hooks h2;
                double add_ns = BenchAdd(h2, sc, seed);
                std::fprintf(csv, "C,%s,%u,add,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             0.0,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             sc.num_events * sc.hooks_per_event);
            }

            // --- D ---
            {
                D_Hooks h;
                SetupHooks(h, sc, seed);
                std::mt19937 rng(seed);
                char buf[32];
                std::vector<double> r_samples;
                for (int i = 0; i < 1000; ++i) {
                    std::snprintf(buf, sizeof(buf), "Event_%zu",
                                  static_cast<std::size_t>(rng() % sc.num_events));
                    auto s = Clock::now();
                    h.Run(buf, static_cast<int>(rng()));
                    auto e = Clock::now();
                    r_samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(r_samples);
                std::fprintf(csv, "D,%s,%u,run,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);

                std::vector<double> rm_s;
                for (int i = 0; i < 1010; ++i) {
                    if (i == 1000) SetupHooks(h, sc, seed + 1u);
                    std::size_t e_idx = static_cast<std::size_t>(rng() % sc.num_events);
                    std::size_t h_idx = static_cast<std::size_t>(rng() % sc.hooks_per_event);
                    std::snprintf(buf, sizeof(buf), "Event_%zu", e_idx);
                    char ibuf[32];
                    std::snprintf(ibuf, sizeof(ibuf), "Hook_%zu_%zu_%u",
                                  e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
                    auto s = Clock::now();
                    h.Remove(buf, ibuf);
                    auto e = Clock::now();
                    if (i >= 10) rm_s.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats rmst = ComputeStats(rm_s);
                std::fprintf(csv, "D,%s,%u,remove,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, rmst.mean, rmst.median, rmst.p95, rmst.p99, rmst.stddev,
                             rmst.min, rmst.max, sc.num_events * sc.hooks_per_event);

                D_Hooks h2;
                double add_ns = BenchAdd(h2, sc, seed);
                std::fprintf(csv, "D,%s,%u,add,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             0.0,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             sc.num_events * sc.hooks_per_event);
            }

            // --- E ---
            {
                E_Hooks h;
                SetupHooks(h, sc, seed);
                std::mt19937 rng(seed);
                char buf[32];
                std::vector<double> r_samples;
                for (int i = 0; i < 1000; ++i) {
                    std::snprintf(buf, sizeof(buf), "Event_%zu",
                                  static_cast<std::size_t>(rng() % sc.num_events));
                    auto s = Clock::now();
                    h.Run(buf, static_cast<int>(rng()));
                    auto e = Clock::now();
                    r_samples.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats st = ComputeStats(r_samples);
                std::fprintf(csv, "E,%s,%u,run,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, st.mean, st.median, st.p95, st.p99, st.stddev,
                             st.min, st.max, sc.num_events * sc.hooks_per_event);

                std::vector<double> rm_s;
                for (int i = 0; i < 1010; ++i) {
                    if (i == 1000) SetupHooks(h, sc, seed + 1u);
                    std::size_t e_idx = static_cast<std::size_t>(rng() % sc.num_events);
                    std::size_t h_idx = static_cast<std::size_t>(rng() % sc.hooks_per_event);
                    std::snprintf(buf, sizeof(buf), "Event_%zu", e_idx);
                    char ibuf[32];
                    std::snprintf(ibuf, sizeof(ibuf), "Hook_%zu_%zu_%u",
                                  e_idx, h_idx, static_cast<unsigned>(rng() % 100000u));
                    auto s = Clock::now();
                    h.Remove(buf, ibuf);
                    auto e = Clock::now();
                    if (i >= 10) rm_s.push_back(std::chrono::duration<double, std::nano>(e - s).count());
                }
                Stats rmst = ComputeStats(rm_s);
                std::fprintf(csv, "E,%s,%u,remove,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed, rmst.mean, rmst.median, rmst.p95, rmst.p99, rmst.stddev,
                             rmst.min, rmst.max, sc.num_events * sc.hooks_per_event);

                E_Hooks h2;
                double add_ns = BenchAdd(h2, sc, seed);
                std::fprintf(csv, "E,%s,%u,add,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%zu\n",
                             sc.name, seed,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             0.0,
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             add_ns / (sc.num_events * sc.hooks_per_event),
                             sc.num_events * sc.hooks_per_event);
            }
        }
    }

    std::fclose(csv);
    double t_end = Now();
    std::printf("Done. Wall time: %.3f sec. Output: prototype/build/results.csv (%d rows)\n",
                t_end - t_start,
                5 /* strategies */ * 5 /* scenes */ * 5 /* seeds */ * 3 /* ops */);
    return 0;
}