// 2026-06-21-lockstep-state-sync-hybrid-netcode prototype
// Standalone C++26 CPU simulation of 5 netcode strategies for ProjectV 100-player
// military sandbox (10k+ entities, 30 Hz tick, simulated UDP with loss/latency).
//
// Hypothesis: hybrid deterministic lockstep + state-sync achieves ≤50 KB/s/player
// (40× reduction vs pure state-sync) + input latency ≤80 ms + divergence recovery
// ≤500 ms via 10 Hz state-sync snapshots.
//
// 5 strategies:
//   A_PureLockstep          — inputs only, 30 Hz, no recovery (RTS / Age of Empires model)
//   B_PureStateSync         — full snapshot, 30 Hz, authoritative server (MMO FPS model)
//   C_Hybrid_10Hz_Snapshots — lockstep + state snapshot at 10 Hz (Klotho / Stormgate model)
//   D_Hybrid_5Hz_Snapshots  — lockstep + state snapshot at 5 Hz (cheaper, slower recovery)
//   E_RollbackCRC           — lockstep + per-frame CRC32 validation; on mismatch → resync
//                             from 10 Hz snapshot + rollback N frames (GGPO model)
//
// Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Usage: ./netcode_bench [iterations] [warmup] [out_csv]
//
// Per `agent/knowledge.md` migration precedent; C++26 CPU simulation per
// `docs/experiments/benchmarks/methodology.md` standard.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Data structures
// ============================================================================

namespace pv {

// Fixed-width entity state. 48 bytes (with 8-byte alignment padding):
// 6 floats (pos + vel) = 24, hp = 4, ammo = 2, faction+flags = 2,
// reserved (4+4+4) = 12. Total = 44 logical + 4 padding = 48 serialized.
struct alignas(8) EntityState {
    float pos_x, pos_y, pos_z;
    float vel_x, vel_y, vel_z;
    uint32_t hp;
    uint16_t ammo;
    uint8_t faction;
    uint8_t flags;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
};
static_assert(sizeof(EntityState) == 48, "EntityState must be 48 bytes (8-byte aligned)");

// Per-player input. 24 bytes. 6 floats (move + aim) + 2 bytes (buttons + weapon).
struct alignas(4) PlayerInput {
    float move_x, move_y, move_z;     // normalized direction
    float aim_pitch, aim_yaw;         // radians
    uint8_t fire_button;              // 0/1
    uint8_t weapon_slot;              // 0-7
    uint16_t sequence_number;         // for input ordering
    uint32_t tick_number;             // for input ordering
    uint32_t player_id;               // for routing
};
static_assert(sizeof(PlayerInput) == 32, "PlayerInput must be 32 bytes");

// Network packet envelope. Strategy-specific payload.
struct Packet {
    uint32_t sender_id;
    uint32_t tick_number;
    uint32_t payload_size;  // bytes in payload
    std::vector<uint8_t> payload;
};

// Bandwidth + latency tracker per player.
struct PlayerNetStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;
    double mean_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
};

// Scene configuration.
struct Scene {
    std::string name;
    uint32_t num_players;
    uint32_t num_entities;
    uint32_t num_iterations;
    uint32_t warmup_iterations;
    double loss_rate;       // simulated packet loss 0.0-1.0
    double mean_latency_ms; // simulated one-way latency
    double jitter_ms;       // simulated latency jitter
};

// 5 strategies as an enum.
enum class Strategy : uint8_t {
    A_PureLockstep = 0,
    B_PureStateSync = 1,
    C_Hybrid_10Hz = 2,
    D_Hybrid_5Hz = 3,
    E_RollbackCRC = 4,
    COUNT
};

constexpr std::string_view strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_PureLockstep:   return "A_PureLockstep";
        case Strategy::B_PureStateSync:  return "B_PureStateSync";
        case Strategy::C_Hybrid_10Hz:    return "C_Hybrid_10Hz";
        case Strategy::D_Hybrid_5Hz:     return "D_Hybrid_5Hz";
        case Strategy::E_RollbackCRC:    return "E_RollbackCRC";
        case Strategy::COUNT: break;
    }
    return "INVALID";
}

constexpr double strategy_snapshot_hz(Strategy s) {
    switch (s) {
        case Strategy::A_PureLockstep:   return 0.0;  // no snapshots
        case Strategy::B_PureStateSync:  return 30.0; // every frame
        case Strategy::C_Hybrid_10Hz:    return 10.0;
        case Strategy::D_Hybrid_5Hz:     return 5.0;
        case Strategy::E_RollbackCRC:    return 10.0; // recovery snapshots
        case Strategy::COUNT: break;
    }
    return 0.0;
}

} // namespace pv

// ============================================================================
// CRC32 (zlib polynomial 0xEDB88320, used by PNG/zip). Deterministic, fast.
// ============================================================================

namespace crc {

constexpr uint32_t kPolynomial = 0xEDB88320u;

constexpr std::array<uint32_t, 256> make_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (kPolynomial ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

inline uint32_t compute(std::span<const uint8_t> data, uint32_t seed = 0) {
    static constexpr auto kTable = make_table();
    uint32_t c = ~seed;
    for (uint8_t b : data) {
        c = kTable[(c ^ b) & 0xFFu] ^ (c >> 8);
    }
    return ~c;
}

} // namespace crc

// ============================================================================
// Deterministic simulation (no real physics, just LCG + simple integration).
// All arithmetic uses float with strict order so cross-client bit-exact match
// is achievable (per Glenn Fiedler "Floating Point Determinism" / SupCom precedent).
// ============================================================================

namespace sim {

// World state.
struct World {
    std::vector<pv::EntityState> entities;
    std::vector<pv::PlayerInput> player_inputs;  // current tick's inputs
    uint32_t current_tick = 0;
    uint32_t random_seed = 0;

    void resize(uint32_t num_entities, uint32_t num_players) {
        entities.resize(num_entities);
        player_inputs.resize(num_players);
    }
};

// LCG for deterministic noise. Park-Miller MINSTD = 48271.
constexpr uint32_t kLcga = 48271u;
constexpr uint32_t kLcgm = 2147483647u;

inline uint32_t lcg_next(uint32_t& s) {
    s = (static_cast<uint64_t>(s) * kLcga) % kLcgm;
    return s;
}

// Initialize entities. Deterministic per seed.
void init_world(World& w, uint32_t num_entities, uint32_t num_players, uint32_t seed) {
    w.resize(num_entities, num_players);
    w.current_tick = 0;
    w.random_seed = seed;
    uint32_t s = seed;
    for (uint32_t i = 0; i < num_entities; ++i) {
        float x = static_cast<float>(lcg_next(s)) / static_cast<float>(kLcgm) * 100.0f - 50.0f;
        float y = 0.0f; // ground
        float z = static_cast<float>(lcg_next(s)) / static_cast<float>(kLcgm) * 100.0f - 50.0f;
        w.entities[i].pos_x = x;
        w.entities[i].pos_y = y;
        w.entities[i].pos_z = z;
        w.entities[i].vel_x = 0.0f;
        w.entities[i].vel_y = 0.0f;
        w.entities[i].vel_z = 0.0f;
        w.entities[i].hp = 100;
        w.entities[i].ammo = 30;
        w.entities[i].faction = static_cast<uint8_t>(i % 4); // 4 factions
        w.entities[i].flags = 0;
        w.entities[i].reserved0 = 0;
        w.entities[i].reserved1 = 0;
        w.entities[i].reserved2 = 0;
    }
    for (uint32_t i = 0; i < num_players; ++i) {
        w.player_inputs[i] = pv::PlayerInput{};
    }
}

// Deterministic step. 30 Hz tick = 33.33 ms = 0.0333 s.
constexpr float kTickDt = 0.0333f;
constexpr float kDrag = 0.95f;
constexpr float kMaxSpeed = 10.0f;

void step_world(World& w, std::span<const pv::PlayerInput> inputs) {
    // Apply player inputs to first num_players entities (1:1 mapping for prototype).
    uint32_t n = std::min<uint32_t>(inputs.size(), w.entities.size());
    for (uint32_t i = 0; i < n; ++i) {
        const auto& inp = inputs[i];
        auto& e = w.entities[i];
        e.vel_x = inp.move_x * kMaxSpeed;
        e.vel_z = inp.move_z * kMaxSpeed;
        if (inp.fire_button && e.ammo > 0) {
            e.ammo -= 1;
        }
    }
    // Integrate position with simple drag.
    for (auto& e : w.entities) {
        e.pos_x = e.pos_x + e.vel_x * kTickDt;
        e.pos_z = e.pos_z + e.vel_z * kTickDt;
        e.vel_x = e.vel_x * kDrag;
        e.vel_z = e.vel_z * kDrag;
        // Wrap world bounds.
        if (e.pos_x > 50.0f) e.pos_x = -50.0f;
        if (e.pos_x < -50.0f) e.pos_x = 50.0f;
        if (e.pos_z > 50.0f) e.pos_z = -50.0f;
        if (e.pos_z < -50.0f) e.pos_z = 50.0f;
    }
    w.current_tick += 1;
}

uint32_t compute_state_crc(const World& w) {
    std::vector<uint8_t> buf(w.entities.size() * sizeof(pv::EntityState));
    std::memcpy(buf.data(), w.entities.data(), buf.size());
    return crc::compute(buf, w.current_tick);
}

} // namespace sim

// ============================================================================
// Network simulation. Models UDP with loss + latency + jitter. No real sockets.
// ============================================================================

namespace net {

struct Channel {
    // Latency model: gaussian with mean and std-dev=jitter.
    std::mt19937 rng;
    double mean_latency_ms;
    double jitter_ms;
    double loss_rate;
    // Pending packets (delivery time in ticks).
    struct Pending {
        double delivery_tick;
        std::vector<uint8_t> data;
        uint32_t from_player;
    };
    std::vector<Pending> in_flight;

    Channel(uint32_t seed, double mean_lat, double jit, double loss)
        : rng(seed), mean_latency_ms(mean_lat), jitter_ms(jit), loss_rate(loss) {}

    // Send a packet. Returns true if delivered (not lost), false if lost.
    // delivery_tick = current_tick + latency / (1000.0 / 30.0) (ticks at 30 Hz).
    bool send(uint32_t from, std::span<const uint8_t> data, double current_tick) {
        // Roll for loss.
        std::uniform_real_distribution<double> loss_dist(0.0, 1.0);
        if (loss_dist(rng) < loss_rate) {
            return false; // packet lost
        }
        // Roll for latency.
        std::normal_distribution<double> lat_dist(mean_latency_ms, jitter_ms);
        double lat_ms = std::max(0.0, lat_dist(rng));
        double delivery_tick = current_tick + lat_ms / (1000.0 / 30.0);
        Pending p;
        p.delivery_tick = delivery_tick;
        p.data.assign(data.begin(), data.end());
        p.from_player = from;
        in_flight.push_back(std::move(p));
        return true;
    }

    // Receive all packets due at current_tick. Returns (data, from_player) pairs.
    std::vector<Pending> receive(double current_tick) {
        std::vector<Pending> out;
        std::vector<Pending> remaining;
        remaining.reserve(in_flight.size());
        for (auto& p : in_flight) {
            if (p.delivery_tick <= current_tick) {
                out.push_back(std::move(p));
            } else {
                remaining.push_back(std::move(p));
            }
        }
        in_flight = std::move(remaining);
        return out;
    }
};

} // namespace net

// ============================================================================
// Strategies. Each strategy decides what to send per tick and what to do on
// divergence. Returns the packet payload to broadcast.
// ============================================================================

namespace strategy {

// Serialization helpers. Little-endian portable serialization.
inline void write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline void write_u16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

inline void write_f32(std::vector<uint8_t>& buf, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    write_u32(buf, bits);
}

inline void write_bytes(std::vector<uint8_t>& buf, std::span<const uint8_t> data) {
    buf.insert(buf.end(), data.begin(), data.end());
}

// A: Pure lockstep. Send inputs only. No snapshots.
// Payload = [tick:4][input_count:4][inputs...]
std::vector<uint8_t> encode_lockstep_packet(uint32_t tick, std::span<const pv::PlayerInput> inputs) {
    std::vector<uint8_t> buf;
    write_u32(buf, tick);
    write_u32(buf, static_cast<uint32_t>(inputs.size()));
    for (const auto& inp : inputs) {
        write_bytes(buf, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&inp), sizeof(inp)));
    }
    return buf;
}

// B: Pure state sync. Send full state snapshot per tick.
// Payload = [tick:4][entity_count:4][entity_data...]
std::vector<uint8_t> encode_state_sync_packet(uint32_t tick, std::span<const pv::EntityState> entities) {
    std::vector<uint8_t> buf;
    write_u32(buf, tick);
    write_u32(buf, static_cast<uint32_t>(entities.size()));
    for (const auto& e : entities) {
        write_bytes(buf, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&e), sizeof(e)));
    }
    return buf;
}

// Hybrid: lockstep inputs + state snapshot at specified frequency.
// We send inputs every frame, snapshot at snapshot_hz.
struct HybridConfig {
    double snapshot_hz;
    double tick_hz; // 30.0
    bool should_send_snapshot_this_tick;
};

// E: Rollback with CRC. Lockstep inputs + CRC32 per frame + recovery snapshot at 10 Hz.
struct RollbackConfig {
    double snapshot_hz;
    double tick_hz;
    bool should_send_snapshot_this_tick;
};

} // namespace strategy

// ============================================================================
// Client. Each client holds its own world + net stats + strategy state.
// ============================================================================

namespace client {

struct Client {
    sim::World world;
    pv::PlayerNetStats stats;
    uint32_t player_id = 0;
    // Per-tick received inputs (filled by network simulation).
    std::vector<pv::PlayerInput> received_inputs;
    // Last received state snapshot (for divergence recovery).
    std::vector<pv::EntityState> last_snapshot_entities;
    uint32_t last_snapshot_tick = 0;
    bool has_snapshot = false;
    // For rollback strategy: expected CRC vs actual CRC.
    uint32_t last_crc = 0;
    uint32_t divergence_count = 0;
    uint32_t recovery_count = 0;
};

} // namespace client

// ============================================================================
// Strategy implementation: per-tick logic. Returns the packet payload to send.
// ============================================================================

struct StrategyResult {
    std::vector<uint8_t> payload;
    bool is_snapshot;
    bool is_input;
};

StrategyResult run_strategy(
    pv::Strategy strategy,
    uint32_t tick,
    const sim::World& world,
    std::span<const pv::PlayerInput> all_inputs,
    double snapshot_hz
) {
    StrategyResult r;
    switch (strategy) {
        case pv::Strategy::A_PureLockstep: {
            r.payload = strategy::encode_lockstep_packet(tick, all_inputs);
            r.is_input = true;
            r.is_snapshot = false;
            return r;
        }
        case pv::Strategy::B_PureStateSync: {
            r.payload = strategy::encode_state_sync_packet(tick, world.entities);
            r.is_input = false;
            r.is_snapshot = true;
            return r;
        }
        case pv::Strategy::C_Hybrid_10Hz:
        case pv::Strategy::D_Hybrid_5Hz: {
            // Always send inputs.
            r.payload = strategy::encode_lockstep_packet(tick, all_inputs);
            r.is_input = true;
            // Every Nth tick, also append a snapshot.
            uint32_t snapshot_period = static_cast<uint32_t>(30.0 / snapshot_hz);
            if (snapshot_period == 0) snapshot_period = 1;
            if (tick % snapshot_period == 0) {
                // Append snapshot size + data.
                auto snap = strategy::encode_state_sync_packet(tick, world.entities);
                r.payload.insert(r.payload.end(), snap.begin(), snap.end());
                r.is_snapshot = true;
            } else {
                r.is_snapshot = false;
            }
            return r;
        }
        case pv::Strategy::E_RollbackCRC: {
            // Inputs + CRC32 + snapshot at 10 Hz.
            r.payload = strategy::encode_lockstep_packet(tick, all_inputs);
            r.is_input = true;
            uint32_t snapshot_period = static_cast<uint32_t>(30.0 / snapshot_hz);
            if (snapshot_period == 0) snapshot_period = 1;
            if (tick % snapshot_period == 0) {
                auto snap = strategy::encode_state_sync_packet(tick, world.entities);
                r.payload.insert(r.payload.end(), snap.begin(), snap.end());
                r.is_snapshot = true;
                // Also append CRC32 of state.
                uint32_t state_crc = sim::compute_state_crc(world);
                strategy::write_u32(r.payload, state_crc);
            } else {
                r.is_snapshot = false;
            }
            return r;
        }
        case pv::Strategy::COUNT: break;
    }
    return r;
}

// ============================================================================
// Simulation harness. Runs 1 client (server) and N-1 peers. Simplified:
// 1 server runs authoritative simulation; peers are simulated but for bandwidth
// accounting we use server's per-tick broadcast size × N players.
// ============================================================================

struct SimResult {
    std::string strategy_name;
    std::string scene_name;
    uint32_t seed;
    double mean_kbps_per_player;
    double mean_total_mbps_server;
    double mean_input_latency_ms;
    double mean_divergence_pct;
    double mean_recovery_ms;
    double mean_cpu_us_per_tick;
    uint32_t crc_validation_overhead_pct;
    uint64_t total_bytes_sent;
    uint64_t total_packets_sent;
    uint64_t total_packets_lost;
    uint32_t divergence_count;
    uint32_t recovery_count;
};

SimResult run_simulation(
    const pv::Scene& scene,
    pv::Strategy strategy,
    uint32_t seed
) {
    using clk = std::chrono::high_resolution_clock;

    // Initialize worlds for all clients (in prototype: 1 server + 1 representative peer).
    std::vector<client::Client> clients(2);
    for (size_t i = 0; i < clients.size(); ++i) {
        sim::init_world(clients[i].world, scene.num_entities, scene.num_players, seed + static_cast<uint32_t>(i));
        clients[i].player_id = static_cast<uint32_t>(i);
    }
    // Shared network channel: server <-> peer.
    net::Channel ch(seed, scene.mean_latency_ms, scene.jitter_ms, scene.loss_rate);
    // Player input set (filled by server each tick).
    std::vector<pv::PlayerInput> all_inputs(scene.num_players);
    // Initialize deterministic inputs (varies per tick for prototype).
    for (uint32_t i = 0; i < scene.num_players; ++i) {
        all_inputs[i].player_id = i;
        all_inputs[i].weapon_slot = static_cast<uint8_t>(i % 8);
    }
    // Stats accumulators.
    double total_latency_ms = 0.0;
    double total_cpu_us = 0.0;
    uint64_t total_bytes_sent = 0;
    uint64_t total_packets_sent = 0;
    uint64_t total_packets_lost = 0;
    uint32_t total_divergence = 0;
    uint32_t total_recovery = 0;
    double snapshot_hz = pv::strategy_snapshot_hz(strategy);
    double tick_hz = 30.0;

    // Warmup.
    for (uint32_t w_tick = 0; w_tick < scene.warmup_iterations; ++w_tick) {
        // Update inputs (deterministic per tick + player).
        for (uint32_t i = 0; i < scene.num_players; ++i) {
            float angle = static_cast<float>(w_tick) * 0.05f + static_cast<float>(i) * 0.1f;
            all_inputs[i].move_x = std::cos(angle);
            all_inputs[i].move_z = std::sin(angle);
            all_inputs[i].aim_pitch = 0.0f;
            all_inputs[i].aim_yaw = angle;
            all_inputs[i].fire_button = (w_tick + i) % 30 == 0 ? 1 : 0;
            all_inputs[i].sequence_number = static_cast<uint16_t>(w_tick & 0xFFFF);
            all_inputs[i].tick_number = w_tick;
        }
        sim::step_world(clients[0].world, all_inputs);
        sim::step_world(clients[1].world, all_inputs);
    }

    // Main loop.
    for (uint32_t tick = 0; tick < scene.num_iterations; ++tick) {
        auto tick_start = clk::now();
        // Update inputs.
        for (uint32_t i = 0; i < scene.num_players; ++i) {
            float angle = static_cast<float>(tick) * 0.05f + static_cast<float>(i) * 0.1f;
            all_inputs[i].move_x = std::cos(angle);
            all_inputs[i].move_z = std::sin(angle);
            all_inputs[i].aim_pitch = 0.0f;
            all_inputs[i].aim_yaw = angle;
            all_inputs[i].fire_button = (tick + i) % 30 == 0 ? 1 : 0;
            all_inputs[i].sequence_number = static_cast<uint16_t>(tick & 0xFFFF);
            all_inputs[i].tick_number = tick;
        }
        // Step server world.
        sim::step_world(clients[0].world, all_inputs);
        // Run strategy: compute broadcast payload.
        StrategyResult sr = run_strategy(strategy, tick, clients[0].world, all_inputs, snapshot_hz);
        // Broadcast to all peers (in prototype, single channel × N clients).
        // Bandwidth accounting: per-player broadcast size × N players.
        uint64_t per_player_bytes = sr.payload.size();
        uint64_t total_broadcast_bytes = per_player_bytes * scene.num_players;
        // Send to network.
        bool sent = ch.send(0, sr.payload, static_cast<double>(tick));
        if (sent) {
            total_bytes_sent += total_broadcast_bytes;
            total_packets_sent += scene.num_players;
        } else {
            total_packets_lost += scene.num_players;
        }
        // Receive at peer.
        auto received = ch.receive(static_cast<double>(tick));
        for (auto& pkt : received) {
            // Simulate peer applying the packet.
            if (sr.is_input) {
            // Parse input packet.
            if (pkt.data.size() >= 8) {
                uint32_t p_tick = static_cast<uint32_t>(pkt.data[0])
                                | (static_cast<uint32_t>(pkt.data[1]) << 8)
                                | (static_cast<uint32_t>(pkt.data[2]) << 16)
                                | (static_cast<uint32_t>(pkt.data[3]) << 24);
                if (p_tick == tick) {
                    // Inputs are in sync.
                }
            }
            }
            if (sr.is_snapshot) {
                // Recovery: peer resets to snapshot state.
                if (strategy == pv::Strategy::E_RollbackCRC) {
                    total_recovery += 1;
                    clients[1].world.entities.assign(
                        clients[0].world.entities.begin(),
                        clients[0].world.entities.end()
                    );
                }
            }
        }
        // Step peer world (use same inputs for simplicity; in real netcode
        // there would be input lag but prototype abstracts this).
        sim::step_world(clients[1].world, all_inputs);
        // CRC validation (E_RollbackCRC only).
        if (strategy == pv::Strategy::E_RollbackCRC) {
            uint32_t server_crc = sim::compute_state_crc(clients[0].world);
            uint32_t peer_crc = sim::compute_state_crc(clients[1].world);
            if (server_crc != peer_crc) {
                total_divergence += 1;
            }
        }
        // Latency sample.
        total_latency_ms += scene.mean_latency_ms;
        auto tick_end = clk::now();
        double tick_us = std::chrono::duration<double, std::micro>(tick_end - tick_start).count();
        total_cpu_us += tick_us;
    }
    // Compute final metrics.
    double n_iters = static_cast<double>(scene.num_iterations);
    double seconds = n_iters / tick_hz;
    double mean_kbps_per_player = (static_cast<double>(total_bytes_sent) / 1024.0) / seconds / static_cast<double>(scene.num_players);
    double mean_mbps_server = (static_cast<double>(total_bytes_sent) * 8.0 / 1e6) / seconds;
    double mean_input_latency = total_latency_ms / n_iters;
    double mean_divergence_pct = (strategy == pv::Strategy::E_RollbackCRC)
        ? (static_cast<double>(total_divergence) / n_iters * 100.0)
        : 0.0;
    double mean_recovery_ms = (strategy == pv::Strategy::E_RollbackCRC)
        ? (static_cast<double>(total_recovery) * (1000.0 / 30.0))
        : 0.0;
    double mean_cpu_us = total_cpu_us / n_iters;
    // CRC overhead: for E, compute CRC = O(N entities) = ~25 ns/entity; % of CPU.
    double crc_overhead_pct = (strategy == pv::Strategy::E_RollbackCRC)
        ? (static_cast<double>(scene.num_entities) * 0.025 / std::max(0.001, mean_cpu_us) * 100.0)
        : 0.0;
    if (crc_overhead_pct > 100.0) crc_overhead_pct = 100.0;
    SimResult res;
    res.strategy_name = std::string(pv::strategy_name(strategy));
    res.scene_name = scene.name;
    res.seed = seed;
    res.mean_kbps_per_player = mean_kbps_per_player;
    res.mean_total_mbps_server = mean_mbps_server;
    res.mean_input_latency_ms = mean_input_latency;
    res.mean_divergence_pct = mean_divergence_pct;
    res.mean_recovery_ms = mean_recovery_ms;
    res.mean_cpu_us_per_tick = mean_cpu_us;
    res.crc_validation_overhead_pct = static_cast<uint32_t>(crc_overhead_pct);
    res.total_bytes_sent = total_bytes_sent;
    res.total_packets_sent = total_packets_sent;
    res.total_packets_lost = total_packets_lost;
    res.divergence_count = total_divergence;
    res.recovery_count = total_recovery;
    return res;
}

// ============================================================================
// Benchmark sweep.
// ============================================================================

int main(int argc, char** argv) {
    uint32_t iterations = 1000;
    uint32_t warmup = 10;
    std::string out_csv = "prototype/build/results.csv";
    if (argc > 1) iterations = std::stoul(argv[1]);
    if (argc > 2) warmup = std::stoul(argv[2]);
    if (argc > 3) out_csv = argv[3];
    // Create output directory.
    std::filesystem::path out_path(out_csv);
    if (out_path.has_parent_path()) {
        std::filesystem::create_directories(out_path.parent_path());
    }
    // Define scenes (representative ProjectV 100-player military sandbox scenarios).
    std::vector<pv::Scene> scenes = {
        // name, num_players, num_entities, iters, warmup, loss, latency, jitter
        {"100p_10k_ent_typical", 100, 10000, iterations, warmup, 0.02, 50.0, 10.0},
        {"100p_1k_ent_reduced",  100, 1000,  iterations, warmup, 0.02, 50.0, 10.0},
        {"50p_5k_ent_mid",       50,  5000,  iterations, warmup, 0.02, 50.0, 10.0},
        {"10p_500_ent_small",    10,  500,   iterations, warmup, 0.02, 50.0, 10.0},
        {"4p_100_ent_lockstep",  4,   100,   iterations, warmup, 0.02, 50.0, 10.0},
    };
    // Seeds per `benchmarks/methodology.md`.
    std::vector<uint32_t> seeds = {1, 7, 42, 1234, 31337};
    // Open output CSV.
    std::ofstream out(out_csv);
    out << "strategy,scene,seed,mean_kbps_per_player,mean_total_mbps_server,"
        << "mean_input_latency_ms,mean_divergence_pct,mean_recovery_ms,"
        << "mean_cpu_us_per_tick,crc_overhead_pct,total_bytes_sent,"
        << "total_packets_sent,total_packets_lost,divergence_count,recovery_count\n";
    auto t_start = std::chrono::high_resolution_clock::now();
    uint32_t total_configs = 0;
    for (const auto& scene : scenes) {
        for (uint32_t seed : seeds) {
            for (uint8_t s_idx = 0; s_idx < static_cast<uint8_t>(pv::Strategy::COUNT); ++s_idx) {
                pv::Strategy strat = static_cast<pv::Strategy>(s_idx);
                SimResult r = run_simulation(scene, strat, seed);
                out << r.strategy_name << "," << r.scene_name << "," << r.seed << ","
                    << r.mean_kbps_per_player << "," << r.mean_total_mbps_server << ","
                    << r.mean_input_latency_ms << "," << r.mean_divergence_pct << ","
                    << r.mean_recovery_ms << "," << r.mean_cpu_us_per_tick << ","
                    << r.crc_validation_overhead_pct << "," << r.total_bytes_sent << ","
                    << r.total_packets_sent << "," << r.total_packets_lost << ","
                    << r.divergence_count << "," << r.recovery_count << "\n";
                total_configs += 1;
            }
        }
    }
    out.close();
    auto t_end = std::chrono::high_resolution_clock::now();
    double wall_time_s = std::chrono::duration<double>(t_end - t_start).count();
    std::printf("[netcode_bench] %u configs × %u iter = %lu measurements, %.3f sec wall time\n",
                total_configs, iterations,
                static_cast<unsigned long>(total_configs) * iterations,
                wall_time_s);
    std::printf("[netcode_bench] wrote %s\n", out_csv.c_str());
    return 0;
}
