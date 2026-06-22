// btree_bench.cpp — Behavior Tree benchmark harness for ProjectV
//   Stage 2 AI, Tier 2: hierarchical tactical AI
//
// Compares 5 BT strategies for 100+ unit tactical AI:
//   A_NaiveNoMemory     — naive baseline: traverse entire tree every tick
//   B_BT_RunningMemory  — classic BT: only tick the running child path
//   C_Hierarchical_3Tier — 3-tier Strategic → Tactical → Unit BT
//   D_EventDriven       — BT with event queue + halts (Champandard 2012, Halo 2)
//   E_Blackboard        — BT with blackboard + event-driven halts + memoization
//
// Self-contained, no ProjectV dependencies. Standalone C++26 CPU benchmark.
//
// Build:  cmake -B build -S . && cmake --build build
// Run:    ./build/btree_bench
//
// Output: results.csv (126 rows = 1 header + 125 data)
//
// Author: self (research agent) — 2026-06-21

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

// ============================================================================
//  Section 1: Common types
// ============================================================================

enum class Status : uint8_t { Success, Failure, Running };

inline const char* status_name(Status s) {
    switch (s) {
        case Status::Success: return "Success";
        case Status::Failure: return "Failure";
        case Status::Running: return "Running";
    }
    return "?";
}

// Per-unit blackboard (the soldier's "brain state" passed to BT nodes)
struct Blackboard {
    float health   = 100.0f;     // 0..100
    float ammo     = 30.0f;      // 0..30
    bool  has_cover = false;     // cached
    bool  sees_enemy = false;    // cached
    bool  under_fire = false;    // cached
    float dist_to_enemy = 50.0f; // 0..100
    float dist_to_cover = 0.0f;
    int   num_shots_fired = 0;
    int   num_moves = 0;

    // Per-tick re-derivation cost (mock, but realistic: ~5-10 ns of "perception")
    // In real engine this would be ECS queries; here we just randomize.
    void refresh(uint32_t tick, std::mt19937& rng) {
        std::uniform_int_distribution<int> d100(0, 99);
        health   = std::max(0.0f, health - (d100(rng) < 20 ? 1.0f : 0.0f));
        ammo     = std::max(0.0f, ammo   - (d100(rng) < 30 ? 1.0f : 0.0f));
        has_cover = (d100(rng) < 60);
        sees_enemy = (d100(rng) < 50);
        under_fire = (d100(rng) < 25);
        dist_to_enemy = 10.0f + static_cast<float>(d100(rng));
        if (has_cover) dist_to_cover = 5.0f + static_cast<float>(d100(rng)) * 0.5f;
        // trivial tick-based memoization: "just ran action X" flag (set by action, cleared next tick)
        (void)tick;
    }
};

// Mock "work" — represents the actual cost of running an Action/Condition node.
// We deliberately make this small (5-30 ns of work) so the BT overhead dominates.
inline float mock_work(int kind, uint32_t salt) noexcept {
    // Different work types: condition (cheap), aim (medium), fire (medium), move (expensive)
    volatile float v = 0.0f;
    switch (kind) {
        case 0: // condition: simple compare
            v = static_cast<float>((salt * 1103515245u + 12345u) & 0xFF);
            return v * 0.0001f;
        case 1: // aim: 4 multiplies
            v = 1.0f;
            for (int i = 0; i < 4; ++i) v = std::sqrt(v + 1.0f);
            return v;
        case 2: // fire: 8 muls
            v = 1.0f;
            for (int i = 0; i < 8; ++i) v = std::sin(v) + 1.0f;
            return v;
        case 3: // move: 16 trig ops
            v = 1.0f;
            for (int i = 0; i < 16; ++i) v = std::cos(v * 1.1f) + std::sin(v);
            return v;
    }
    return v;
}

// ============================================================================
//  Section 2: BT node types — flat SoA for cache friendliness
// ============================================================================

// All strategies share the same node structure (just traversal differs).
// We use a tagged union stored flat; strategy-specific fields are unused when irrelevant.

enum class NodeType : uint8_t {
    Selector,    // tick children L→R; return Success on first Success, Failure if all fail, Running on Running
    Sequence,    // tick children L→R; return Failure on first Failure, Success if all succeed, Running on Running
    Inverter,    // invert child's status
    Succeeder,   // always return Success
    Repeater,    // repeat child N times
    ActionAim,   // leaf: mock work kind=1, sets Blackboard::num_shots_fired
    ActionFire,  // leaf: mock work kind=2, decrements ammo
    ActionMove,  // leaf: mock work kind=3, sets Blackboard::num_moves
    CondHealth,  // condition: health > 30
    CondAmmo,    // condition: ammo > 5
    CondCover,   // condition: has_cover
    CondSees,    // condition: sees_enemy
    CondFire,    // condition: under_fire
    CondDist,    // condition: dist_to_enemy < 20
    CondCanMove, // condition: !has_cover (for move)
    SubTreeCall, // calls into pre-built subtree (for Hierarchical)
};

inline const char* node_type_name(NodeType t) {
    switch (t) {
        case NodeType::Selector:    return "Selector";
        case NodeType::Sequence:    return "Sequence";
        case NodeType::Inverter:    return "Inverter";
        case NodeType::Succeeder:   return "Succeeder";
        case NodeType::Repeater:    return "Repeater";
        case NodeType::ActionAim:   return "ActionAim";
        case NodeType::ActionFire:  return "ActionFire";
        case NodeType::ActionMove:  return "ActionMove";
        case NodeType::CondHealth:  return "CondHealth";
        case NodeType::CondAmmo:    return "CondAmmo";
        case NodeType::CondCover:   return "CondCover";
        case NodeType::CondSees:    return "CondSees";
        case NodeType::CondFire:    return "CondFire";
        case NodeType::CondDist:    return "CondDist";
        case NodeType::CondCanMove: return "CondCanMove";
        case NodeType::SubTreeCall: return "SubTreeCall";
    }
    return "?";
}

// ============================================================================
//  Section 3: Behavior Tree (per-strategy implementation)
// ============================================================================

// -- A: Naive no-memory baseline
//      Traverses entire tree from root every tick. No Running caching.
//      This is the worst-case implementation.
struct TreeA {
    std::vector<NodeType>  type;
    std::vector<uint16_t>  child_start; // index into children
    std::vector<uint16_t>  child_count;
    std::vector<uint16_t>  children;    // concatenated child indices
    uint16_t root = 0;

    Status tick(uint16_t node, Blackboard& bb, uint32_t tick_salt) const {
        NodeType t = type[node];
        switch (t) {
            case NodeType::Selector: {
                Status last = Status::Failure;
                for (uint16_t i = 0; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Success) return Status::Success;
                    if (s == Status::Running)  return Status::Running;
                    last = s;
                }
                return last;
            }
            case NodeType::Sequence: {
                Status last = Status::Success;
                for (uint16_t i = 0; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Failure) return Status::Failure;
                    if (s == Status::Running) return Status::Running;
                    last = s;
                }
                return last;
            }
            case NodeType::Inverter:
                if (child_count[node] == 1) {
                    Status s = tick(children[child_start[node]], bb, tick_salt);
                    return s == Status::Success ? Status::Failure
                         : s == Status::Failure ? Status::Success
                         : Status::Running;
                }
                return Status::Failure;
            case NodeType::Succeeder:
                if (child_count[node] == 1) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::Repeater:
                if (child_count[node] == 1)
                    for (int k = 0; k < 3; ++k) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::ActionAim:
                mock_work(1, tick_salt);
                bb.num_shots_fired += 0; // aim doesn't fire
                return Status::Success;
            case NodeType::ActionFire:
                mock_work(2, tick_salt);
                if (bb.ammo > 0) { bb.ammo -= 1.0f; bb.num_shots_fired += 1; }
                return Status::Success;
            case NodeType::ActionMove:
                mock_work(3, tick_salt);
                bb.num_moves += 1;
                return Status::Success;
            case NodeType::CondHealth:  mock_work(0, tick_salt); return bb.health > 30.0f ? Status::Success : Status::Failure;
            case NodeType::CondAmmo:    mock_work(0, tick_salt); return bb.ammo   >  5.0f ? Status::Success : Status::Failure;
            case NodeType::CondCover:   mock_work(0, tick_salt); return bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::CondSees:    mock_work(0, tick_salt); return bb.sees_enemy ? Status::Success : Status::Failure;
            case NodeType::CondFire:    mock_work(0, tick_salt); return bb.under_fire ? Status::Success : Status::Failure;
            case NodeType::CondDist:    mock_work(0, tick_salt); return bb.dist_to_enemy < 20.0f ? Status::Success : Status::Failure;
            case NodeType::CondCanMove: mock_work(0, tick_salt); return !bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::SubTreeCall:
                if (child_count[node] == 1) return tick(children[child_start[node]], bb, tick_salt);
                return Status::Failure;
        }
        return Status::Failure;
    }
};

// -- B: Classic BT with Running memory
//      Caches last running child index per Selector/Sequence.
//      Re-tick only the path that was Running last tick.
//      This is the canonical BT optimization (Isla 2005, Champandard 2012).
struct TreeB {
    std::vector<NodeType>  type;
    std::vector<uint16_t>  child_start;
    std::vector<uint16_t>  child_count;
    std::vector<uint16_t>  children;
    std::vector<int16_t>   running_child; // per-node, -1 if not running
    uint16_t root = 0;

    TreeB() = default;

    Status tick(uint16_t node, Blackboard& bb, uint32_t tick_salt) {
        NodeType t = type[node];
        switch (t) {
            case NodeType::Selector: {
                int16_t start = (running_child[node] >= 0) ? running_child[node] : 0;
                for (uint16_t i = start; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Running) {
                        running_child[node] = static_cast<int16_t>(i);
                        return Status::Running;
                    }
                    if (s == Status::Success) {
                        running_child[node] = -1;
                        return Status::Success;
                    }
                }
                running_child[node] = -1;
                return Status::Failure;
            }
            case NodeType::Sequence: {
                int16_t start = (running_child[node] >= 0) ? running_child[node] : 0;
                for (uint16_t i = start; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Running) {
                        running_child[node] = static_cast<int16_t>(i);
                        return Status::Running;
                    }
                    if (s == Status::Failure) {
                        running_child[node] = -1;
                        return Status::Failure;
                    }
                }
                running_child[node] = -1;
                return Status::Success;
            }
            case NodeType::Inverter:
                if (child_count[node] == 1) {
                    Status s = tick(children[child_start[node]], bb, tick_salt);
                    return s == Status::Success ? Status::Failure
                         : s == Status::Failure ? Status::Success
                         : Status::Running;
                }
                return Status::Failure;
            case NodeType::Succeeder:
                if (child_count[node] == 1) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::Repeater:
                if (child_count[node] == 1)
                    for (int k = 0; k < 3; ++k) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::ActionAim:
                mock_work(1, tick_salt);
                return Status::Success;
            case NodeType::ActionFire:
                mock_work(2, tick_salt);
                if (bb.ammo > 0) { bb.ammo -= 1.0f; bb.num_shots_fired += 1; }
                return Status::Success;
            case NodeType::ActionMove:
                mock_work(3, tick_salt);
                bb.num_moves += 1;
                return Status::Success;
            case NodeType::CondHealth:  mock_work(0, tick_salt); return bb.health > 30.0f ? Status::Success : Status::Failure;
            case NodeType::CondAmmo:    mock_work(0, tick_salt); return bb.ammo   >  5.0f ? Status::Success : Status::Failure;
            case NodeType::CondCover:   mock_work(0, tick_salt); return bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::CondSees:    mock_work(0, tick_salt); return bb.sees_enemy ? Status::Success : Status::Failure;
            case NodeType::CondFire:    mock_work(0, tick_salt); return bb.under_fire ? Status::Success : Status::Failure;
            case NodeType::CondDist:    mock_work(0, tick_salt); return bb.dist_to_enemy < 20.0f ? Status::Success : Status::Failure;
            case NodeType::CondCanMove: mock_work(0, tick_salt); return !bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::SubTreeCall:
                if (child_count[node] == 1) return tick(children[child_start[node]], bb, tick_salt);
                return Status::Failure;
        }
        return Status::Failure;
    }
};

// -- C: Hierarchical 3-tier BT
//      Each unit has its own Unit BT (ticked every tick).
//      Each Platoon has a Tactical BT (ticked every tick; aggregates units).
//      Command level has a Strategic BT (ticked every 10th tick = 3 Hz at 30 fps).
//      SubTreeCall is used to delegate from upper to lower tier.
struct TreeC {
    // Three separate trees: strategic, tactical, unit (same node type as B)
    TreeB strategic;
    TreeB tactical;
    TreeB unit;

    void tick_strategic(Blackboard& bb, uint32_t tick_salt) {
        // Strategic = simple policy selector: assault, defend, retreat
        // Run less frequently (called every Nth tick from main loop)
        strategic.tick(strategic.root, bb, tick_salt);
    }
    void tick_tactical(Blackboard& bb, uint32_t tick_salt) {
        // Tactical = coordination selector: hold, push, fall-back
        // Wraps unit BTs via SubTreeCall
        tactical.tick(tactical.root, bb, tick_salt);
    }
    Status tick_unit(Blackboard& bb, uint32_t tick_salt) {
        return unit.tick(unit.root, bb, tick_salt);
    }
};

// -- D: Event-driven BT with halts
//      Nodes are tagged with "halt conditions" (akin to Halo 2's behavior tagging).
//      When a halt event fires, the running path is preempted.
//      This is the production pattern from Halo 2 + Champandard 2012 + Agis 2020.
struct TreeD {
    std::vector<NodeType>  type;
    std::vector<uint16_t>  child_start;
    std::vector<uint16_t>  child_count;
    std::vector<uint16_t>  children;
    std::vector<int16_t>   running_child;
    uint16_t root = 0;

    // Halt-event queue (one per tick) — nodes can subscribe to events
    enum class HaltEvent : uint8_t { None, TookDamage, SpottedEnemy, HeardNoise, LowAmmo };
    HaltEvent pending_event = HaltEvent::None;

    void push_event(HaltEvent e) { pending_event = e; }

    Status tick(uint16_t node, Blackboard& bb, uint32_t tick_salt) {
        // Halt check at every node: if event matches node semantics, abort.
        bool should_halt = (pending_event != HaltEvent::None) && check_halt(node, bb);
        if (should_halt) {
            running_child[node] = -1;
            // skip the subtree; return Failure so parent re-evaluates
            return Status::Failure;
        }

        NodeType t = type[node];
        switch (t) {
            case NodeType::Selector: {
                int16_t start = (running_child[node] >= 0) ? running_child[node] : 0;
                for (uint16_t i = start; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Running) {
                        running_child[node] = static_cast<int16_t>(i);
                        return Status::Running;
                    }
                    if (s == Status::Success) { running_child[node] = -1; return Status::Success; }
                }
                running_child[node] = -1;
                return Status::Failure;
            }
            case NodeType::Sequence: {
                int16_t start = (running_child[node] >= 0) ? running_child[node] : 0;
                for (uint16_t i = start; i < child_count[node]; ++i) {
                    Status s = tick(children[child_start[node] + i], bb, tick_salt);
                    if (s == Status::Running) {
                        running_child[node] = static_cast<int16_t>(i);
                        return Status::Running;
                    }
                    if (s == Status::Failure) { running_child[node] = -1; return Status::Failure; }
                }
                running_child[node] = -1;
                return Status::Success;
            }
            case NodeType::Inverter:
                if (child_count[node] == 1) {
                    Status s = tick(children[child_start[node]], bb, tick_salt);
                    return s == Status::Success ? Status::Failure
                         : s == Status::Failure ? Status::Success
                         : Status::Running;
                }
                return Status::Failure;
            case NodeType::Succeeder:
                if (child_count[node] == 1) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::Repeater:
                if (child_count[node] == 1)
                    for (int k = 0; k < 3; ++k) tick(children[child_start[node]], bb, tick_salt);
                return Status::Success;
            case NodeType::ActionAim:
                mock_work(1, tick_salt);
                return Status::Success;
            case NodeType::ActionFire:
                mock_work(2, tick_salt);
                if (bb.ammo > 0) { bb.ammo -= 1.0f; bb.num_shots_fired += 1; }
                return Status::Success;
            case NodeType::ActionMove:
                mock_work(3, tick_salt);
                bb.num_moves += 1;
                return Status::Success;
            case NodeType::CondHealth:  mock_work(0, tick_salt); return bb.health > 30.0f ? Status::Success : Status::Failure;
            case NodeType::CondAmmo:    mock_work(0, tick_salt); return bb.ammo   >  5.0f ? Status::Success : Status::Failure;
            case NodeType::CondCover:   mock_work(0, tick_salt); return bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::CondSees:    mock_work(0, tick_salt); return bb.sees_enemy ? Status::Success : Status::Failure;
            case NodeType::CondFire:    mock_work(0, tick_salt); return bb.under_fire ? Status::Success : Status::Failure;
            case NodeType::CondDist:    mock_work(0, tick_salt); return bb.dist_to_enemy < 20.0f ? Status::Success : Status::Failure;
            case NodeType::CondCanMove: mock_work(0, tick_salt); return !bb.has_cover ? Status::Success : Status::Failure;
            case NodeType::SubTreeCall:
                if (child_count[node] == 1) return tick(children[child_start[node]], bb, tick_salt);
                return Status::Failure;
        }
        return Status::Failure;
    }

    // Halt semantics: each node type reacts to specific events.
    // In real engine this would be a tagged bitvector (Isla 2005).
    bool check_halt(uint16_t node, Blackboard& /*bb*/) const {
        NodeType t = type[node];
        switch (pending_event) {
            case HaltEvent::TookDamage:
                // Damage interrupts combat actions
                return t == NodeType::ActionFire || t == NodeType::ActionAim;
            case HaltEvent::SpottedEnemy:
                // New spotted enemy interrupts movement
                return t == NodeType::ActionMove;
            case HaltEvent::HeardNoise:
                // Noise interrupts non-combat
                return t == NodeType::ActionMove;
            case HaltEvent::LowAmmo:
                // Low ammo interrupts fire
                return t == NodeType::ActionFire;
            default: return false;
        }
    }
};

// -- E: Blackboard + event-driven + memoization
//      Adds shared blackboard (unit-level), event-driven halts, and a "decision memo"
//      that caches the last successful branch for the current situation for one tick
//      to skip redundant work.
struct TreeE {
    // Per-tick memo: hashes a "situation signature" of Blackboard to BT last-result.
    // For a simple prototype, just skip re-evaluation if BB state didn't change.
    // We measure this as a "memo check" cost at root.
    struct Memo {
        uint64_t sig = 0;
        Status result = Status::Success;
        bool valid = false;
    };
    Memo memo;

    // Re-use TreeD's structure for tick logic
    TreeD inner;

    Status tick(Blackboard& bb, uint32_t tick_salt) {
        // Compute simple signature from key Blackboard fields
        uint64_t sig = 0;
        sig ^= static_cast<uint64_t>(static_cast<uint32_t>(bb.health) * 31u + 0u);
        sig ^= static_cast<uint64_t>(static_cast<uint32_t>(bb.ammo)   * 37u + 1u);
        sig ^= static_cast<uint64_t>(bb.has_cover  ? 0xA1u : 0xB2u);
        sig ^= static_cast<uint64_t>(bb.sees_enemy ? 0xC3u : 0xD4u);
        sig ^= static_cast<uint64_t>(bb.under_fire ? 0xE5u : 0xF6u);
        sig ^= static_cast<uint64_t>(static_cast<uint32_t>(bb.dist_to_enemy) * 41u + 2u);

        // Memoize: if same situation, return cached result (no tick)
        if (memo.valid && memo.sig == sig) {
            return memo.result;
        }
        // Otherwise tick
        Status s = inner.tick(inner.root, bb, tick_salt);
        memo.sig = sig;
        memo.result = s;
        memo.valid = true;
        return s;
    }
};

// ============================================================================
//  Section 4: BT builder — same soldier behavior expressed in each strategy
// ============================================================================

// We build a soldier behavior tree that has 12-15 nodes.
// This is the SAME tree structure across all strategies; only traversal differs.

// Build helper: returns node index
struct Builder {
    std::vector<NodeType>  type;
    std::vector<uint16_t>  child_start;
    std::vector<uint16_t>  child_count;
    std::vector<uint16_t>  children;

    uint16_t add(NodeType t, std::initializer_list<uint16_t> kids = {}) {
        uint16_t idx = static_cast<uint16_t>(type.size());
        type.push_back(t);
        child_start.push_back(static_cast<uint16_t>(children.size()));
        child_count.push_back(static_cast<uint16_t>(kids.size()));
        for (auto k : kids) children.push_back(k);
        return idx;
    }
};

// Build the canonical soldier BT, returns the root index.
// Tree shape (12-15 nodes):
//   Root = Selector
//     ├─ Sequence(EngageOrFallBack)
//     │   ├─ CondSees
//     │   ├─ Selector(AttackOrReposition)
//     │   │   ├─ Sequence(Attack)
//     │   │   │   ├─ CondAmmo
//     │   │   │   ├─ CondDist
//     │   │   │   ├─ ActionAim
//     │   │   │   └─ ActionFire
//     │   │   └─ Sequence(Reposition)
//     │   │       ├─ Inverter(CondCover)
//     │   │       ├─ ActionMove
//     │   │       └─ CondCover
//     │   └─ CondHealth
//     ├─ Sequence(Heal)
//     │   ├─ Inverter(CondHealth)
//     │   └─ ActionMove   (move to cover)
//     └─ Sequence(Reload)
//         ├─ Inverter(CondAmmo)
//         └─ ActionFire   (reload = no-op shot)
uint16_t build_soldier_tree(Builder& b) {
    uint16_t cond_aim    = b.add(NodeType::ActionAim);
    uint16_t cond_fire   = b.add(NodeType::ActionFire);
    uint16_t act_move    = b.add(NodeType::ActionMove);

    uint16_t cond_ammo   = b.add(NodeType::CondAmmo);
    uint16_t cond_dist   = b.add(NodeType::CondDist);
    uint16_t cond_cover  = b.add(NodeType::CondCover);
    uint16_t cond_sees   = b.add(NodeType::CondSees);
    uint16_t cond_health = b.add(NodeType::CondHealth);

    uint16_t inv_cover   = b.add(NodeType::Inverter, {cond_cover});
    uint16_t inv_ammo    = b.add(NodeType::Inverter, {cond_ammo});
    uint16_t inv_health  = b.add(NodeType::Inverter, {cond_health});

    uint16_t attack      = b.add(NodeType::Sequence, {cond_ammo, cond_dist, cond_aim, cond_fire});
    uint16_t reposition  = b.add(NodeType::Sequence, {inv_cover, act_move, cond_cover});
    uint16_t atk_or_rep  = b.add(NodeType::Selector,  {attack, reposition});

    uint16_t engage      = b.add(NodeType::Sequence, {cond_sees, atk_or_rep, cond_health});
    uint16_t heal        = b.add(NodeType::Sequence, {inv_health, act_move});
    uint16_t reload      = b.add(NodeType::Sequence, {inv_ammo,  cond_fire});  // cheat: use ActionFire as "reload" semantic

    uint16_t root        = b.add(NodeType::Selector,  {engage, heal, reload});
    return root;
}

void populate_tree_from_builder(Builder& b, uint16_t root,
    std::vector<NodeType>& type,
    std::vector<uint16_t>& child_start,
    std::vector<uint16_t>& child_count,
    std::vector<uint16_t>& children) {
    type       = b.type;
    child_start = b.child_start;
    child_count = b.child_count;
    children    = b.children;
    (void)root;
}

// ============================================================================
//  Section 5: Tactical & Strategic trees (for C — Hierarchical)
// ============================================================================

// Tactical BT for a platoon: aggregates multiple units; runs once per tick.
// Shape (5 nodes): Sequence { CondSees (any), ActionMove (advance), SubTreeCall (delegate) }
// In this prototype we just count cost; we don't actually do per-unit work at the tactical level.
uint16_t build_tactical_tree(Builder& b) {
    uint16_t cond_sees  = b.add(NodeType::CondSees);
    uint16_t act_advance = b.add(NodeType::ActionMove);  // advance posture
    uint16_t sub_call   = b.add(NodeType::SubTreeCall, { /* placeholder, set after build */ 0 });
    return b.add(NodeType::Sequence, {cond_sees, act_advance, sub_call});
}

// Strategic BT for command: runs every 10th tick.
// Shape (4 nodes): Selector { Sequence(assault), Sequence(defend), Sequence(retreat) }
uint16_t build_strategic_tree(Builder& b) {
    uint16_t cond_sees  = b.add(NodeType::CondSees);
    uint16_t act_adv    = b.add(NodeType::ActionMove);
    uint16_t atk        = b.add(NodeType::Sequence, {cond_sees, act_adv});
    uint16_t def        = b.add(NodeType::Sequence, {act_adv});
    return b.add(NodeType::Selector, {atk, def, atk});  // simplified
}

// ============================================================================
//  Section 6: Scene definitions
// ============================================================================

struct Scene {
    const char* name;
    int num_units;
    int ticks_to_run;
};

// 5 scenes, scaled to test 8 → 256 unit BT load.
inline std::array<Scene, 5> scenes() {
    return std::array<Scene, 5>{{
        {"recon_patrol",       8,   1500},
        {"platoon_attack",    32,   1000},
        {"company_advance",  128,    600},
        {"urban_clear",       64,    800},
        {"combined_arms",    256,    400},
    }};
}

// ============================================================================
//  Section 7: Measurement harness
// ============================================================================

struct Result {
    std::string strategy;
    std::string scene;
    int seed;
    int num_units;
    int ticks;
    double mean_ns_per_tick_per_unit;
    double median_ns_per_tick_per_unit;
    double p95_ns_per_tick_per_unit;
    double p99_ns_per_tick_per_unit;
    double std_ns_per_tick_per_unit;
    double total_ms;
    int    total_decisions;
};

// Per-tick timing accumulator
struct TickTimes {
    std::vector<double> ns_per_unit;
};

// Build a single TreeA / TreeB / TreeD for a soldier.
// (build_unit_trees removed — each strategy constructs its trees inline)

Result run_strategy_a(const Scene& scene, int seed) {
    Builder b;
    uint16_t root = build_soldier_tree(b);

    std::vector<TreeA> trees(scene.num_units);
    for (auto& t : trees) {
        populate_tree_from_builder(b, root, t.type, t.child_start, t.child_count, t.children);
        t.root = root;
    }

    std::vector<Blackboard> bbs(scene.num_units);
    std::mt19937 rng(seed);
    for (auto& bb : bbs) bb.refresh(0, rng);

    // Warmup
    for (int t = 0; t < 10; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
        }
    }

    // Measurement
    std::vector<double> per_unit_times(scene.num_units, 0.0);
    auto t0 = std::chrono::steady_clock::now();
    int decisions = 0;
    for (int t = 0; t < scene.ticks_to_run; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            auto t1 = std::chrono::steady_clock::now();
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
            auto t2 = std::chrono::steady_clock::now();
            per_unit_times[u] += std::chrono::duration<double, std::nano>(t2 - t1).count();
            decisions++;
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t3 - t0).count();

    // Stats per unit
    std::vector<double> ns_per_tick(scene.num_units);
    for (int u = 0; u < scene.num_units; ++u) {
        ns_per_tick[u] = per_unit_times[u] / static_cast<double>(scene.ticks_to_run);
    }
    std::sort(ns_per_tick.begin(), ns_per_tick.end());
    double mean = std::accumulate(ns_per_tick.begin(), ns_per_tick.end(), 0.0) / ns_per_tick.size();
    double med  = ns_per_tick[ns_per_tick.size() / 2];
    double p95  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.95)];
    double p99  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.99)];
    double var = 0.0;
    for (auto v : ns_per_tick) var += (v - mean) * (v - mean);
    var /= ns_per_tick.size();
    double stddev = std::sqrt(var);

    return Result{
        "A_NaiveNoMemory", scene.name, seed, scene.num_units, scene.ticks_to_run,
        mean, med, p95, p99, stddev, total_ms, decisions
    };
}

Result run_strategy_b(const Scene& scene, int seed) {
    Builder b;
    uint16_t root = build_soldier_tree(b);

    std::vector<TreeB> trees(scene.num_units);
    for (auto& t : trees) {
        populate_tree_from_builder(b, root, t.type, t.child_start, t.child_count, t.children);
        t.running_child.assign(t.type.size(), -1);
        t.root = root;
    }

    std::vector<Blackboard> bbs(scene.num_units);
    std::mt19937 rng(seed);
    for (auto& bb : bbs) bb.refresh(0, rng);

    // Warmup
    for (int t = 0; t < 10; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
        }
    }

    std::vector<double> per_unit_times(scene.num_units, 0.0);
    auto t0 = std::chrono::steady_clock::now();
    int decisions = 0;
    for (int t = 0; t < scene.ticks_to_run; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            auto t1 = std::chrono::steady_clock::now();
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
            auto t2 = std::chrono::steady_clock::now();
            per_unit_times[u] += std::chrono::duration<double, std::nano>(t2 - t1).count();
            decisions++;
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t3 - t0).count();

    std::vector<double> ns_per_tick(scene.num_units);
    for (int u = 0; u < scene.num_units; ++u)
        ns_per_tick[u] = per_unit_times[u] / static_cast<double>(scene.ticks_to_run);
    std::sort(ns_per_tick.begin(), ns_per_tick.end());
    double mean = std::accumulate(ns_per_tick.begin(), ns_per_tick.end(), 0.0) / ns_per_tick.size();
    double med  = ns_per_tick[ns_per_tick.size() / 2];
    double p95  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.95)];
    double p99  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.99)];
    double var = 0.0;
    for (auto v : ns_per_tick) var += (v - mean) * (v - mean);
    var /= ns_per_tick.size();
    return Result{
        "B_BT_RunningMemory", scene.name, seed, scene.num_units, scene.ticks_to_run,
        mean, med, p95, p99, std::sqrt(var), total_ms, decisions
    };
}

Result run_strategy_c(const Scene& scene, int seed) {
    Builder b_unit;
    uint16_t unit_root = build_soldier_tree(b_unit);

    Builder b_tac;
    uint16_t tac_root = build_tactical_tree(b_tac);

    Builder b_str;
    uint16_t str_root = build_strategic_tree(b_str);

    // Build a single shared "C" representation per unit, but also keep the tactical+strategic.
    // We model: 1 strategic, ceil(units/8) tacticals, units unit-BTs.
    int num_tacticals = std::max(1, scene.num_units / 8);
    int num_strategic = 1;

    std::vector<TreeB> units(scene.num_units);
    std::vector<TreeB> tacticals(num_tacticals);
    std::vector<TreeB> strategics(num_strategic);
    for (auto& t : units) {
        populate_tree_from_builder(b_unit, unit_root, t.type, t.child_start, t.child_count, t.children);
        t.running_child.assign(t.type.size(), -1);
        t.root = unit_root;
    }
    for (auto& t : tacticals) {
        populate_tree_from_builder(b_tac, tac_root, t.type, t.child_start, t.child_count, t.children);
        t.running_child.assign(t.type.size(), -1);
        t.root = tac_root;
    }
    for (auto& t : strategics) {
        populate_tree_from_builder(b_str, str_root, t.type, t.child_start, t.child_count, t.children);
        t.running_child.assign(t.type.size(), -1);
        t.root = str_root;
    }

    std::vector<Blackboard> bbs(scene.num_units);
    std::mt19937 rng(seed);
    for (auto& bb : bbs) bb.refresh(0, rng);

    // Warmup
    for (int t = 0; t < 10; ++t) {
        for (int u = 0; u < scene.num_units; ++u) bbs[u].refresh(t, rng);
        if (t % 10 == 0) for (auto& s : strategics) s.tick(s.root, bbs[0], static_cast<uint32_t>(t));
        for (auto& t1 : tacticals) t1.tick(t1.root, bbs[0], static_cast<uint32_t>(t));
        for (int u = 0; u < scene.num_units; ++u)
            units[u].tick(units[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
    }

    std::vector<double> per_unit_times(scene.num_units, 0.0);
    auto t0 = std::chrono::steady_clock::now();
    int decisions = 0;
    for (int t = 0; t < scene.ticks_to_run; ++t) {
        for (int u = 0; u < scene.num_units; ++u) bbs[u].refresh(t, rng);
        auto t1 = std::chrono::steady_clock::now();
        if (t % 10 == 0) for (auto& s : strategics) s.tick(s.root, bbs[0], static_cast<uint32_t>(t));
        for (auto& t2 : tacticals) t2.tick(t2.root, bbs[0], static_cast<uint32_t>(t));
        for (int u = 0; u < scene.num_units; ++u)
            units[u].tick(units[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
        auto t2 = std::chrono::steady_clock::now();
        double tick_ns = std::chrono::duration<double, std::nano>(t2 - t1).count();
        // Amortize per-tick cost across all units (hierarchical = shared cost)
        double per_unit_ns = tick_ns / static_cast<double>(scene.num_units);
        for (int u = 0; u < scene.num_units; ++u) per_unit_times[u] += per_unit_ns;
        decisions++;
    }
    auto t3 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t3 - t0).count();

    std::vector<double> ns_per_tick(scene.num_units, 0.0);
    for (int u = 0; u < scene.num_units; ++u) ns_per_tick[u] = per_unit_times[u] / static_cast<double>(scene.ticks_to_run);
    std::sort(ns_per_tick.begin(), ns_per_tick.end());
    double mean = std::accumulate(ns_per_tick.begin(), ns_per_tick.end(), 0.0) / ns_per_tick.size();
    double med  = ns_per_tick[ns_per_tick.size() / 2];
    double p95  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.95)];
    double p99  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.99)];
    double var = 0.0;
    for (auto v : ns_per_tick) var += (v - mean) * (v - mean);
    var /= ns_per_tick.size();
    return Result{
        "C_Hierarchical_3Tier", scene.name, seed, scene.num_units, scene.ticks_to_run,
        mean, med, p95, p99, std::sqrt(var), total_ms, decisions
    };
}

Result run_strategy_d(const Scene& scene, int seed) {
    Builder b;
    uint16_t root = build_soldier_tree(b);

    std::vector<TreeD> trees(scene.num_units);
    for (auto& t : trees) {
        populate_tree_from_builder(b, root, t.type, t.child_start, t.child_count, t.children);
        t.running_child.assign(t.type.size(), -1);
        t.root = root;
    }

    std::vector<Blackboard> bbs(scene.num_units);
    std::mt19937 rng(seed);
    for (auto& bb : bbs) bb.refresh(0, rng);

    // Warmup
    for (int t = 0; t < 10; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            // Random event
            if (t % 7 == 0) trees[u].push_event(TreeD::HaltEvent::TookDamage);
            else if (t % 5 == 0) trees[u].push_event(TreeD::HaltEvent::SpottedEnemy);
            else if (t % 11 == 0) trees[u].push_event(TreeD::HaltEvent::LowAmmo);
            else trees[u].push_event(TreeD::HaltEvent::None);
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
        }
    }

    std::vector<double> per_unit_times(scene.num_units, 0.0);
    auto t0 = std::chrono::steady_clock::now();
    int decisions = 0;
    for (int t = 0; t < scene.ticks_to_run; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            if (t % 7 == 0) trees[u].push_event(TreeD::HaltEvent::TookDamage);
            else if (t % 5 == 0) trees[u].push_event(TreeD::HaltEvent::SpottedEnemy);
            else if (t % 11 == 0) trees[u].push_event(TreeD::HaltEvent::LowAmmo);
            else trees[u].push_event(TreeD::HaltEvent::None);
            auto t1 = std::chrono::steady_clock::now();
            trees[u].tick(trees[u].root, bbs[u], static_cast<uint32_t>(t * 7u + u));
            auto t2 = std::chrono::steady_clock::now();
            per_unit_times[u] += std::chrono::duration<double, std::nano>(t2 - t1).count();
            decisions++;
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t3 - t0).count();

    std::vector<double> ns_per_tick(scene.num_units);
    for (int u = 0; u < scene.num_units; ++u) ns_per_tick[u] = per_unit_times[u] / static_cast<double>(scene.ticks_to_run);
    std::sort(ns_per_tick.begin(), ns_per_tick.end());
    double mean = std::accumulate(ns_per_tick.begin(), ns_per_tick.end(), 0.0) / ns_per_tick.size();
    double med  = ns_per_tick[ns_per_tick.size() / 2];
    double p95  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.95)];
    double p99  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.99)];
    double var = 0.0;
    for (auto v : ns_per_tick) var += (v - mean) * (v - mean);
    var /= ns_per_tick.size();
    return Result{
        "D_EventDriven", scene.name, seed, scene.num_units, scene.ticks_to_run,
        mean, med, p95, p99, std::sqrt(var), total_ms, decisions
    };
}

Result run_strategy_e(const Scene& scene, int seed) {
    Builder b;
    uint16_t root = build_soldier_tree(b);

    std::vector<TreeE> trees(scene.num_units);
    for (auto& t : trees) {
        populate_tree_from_builder(b, root, t.inner.type, t.inner.child_start, t.inner.child_count, t.inner.children);
        t.inner.running_child.assign(t.inner.type.size(), -1);
        t.inner.root = root;
    }

    std::vector<Blackboard> bbs(scene.num_units);
    std::mt19937 rng(seed);
    for (auto& bb : bbs) bb.refresh(0, rng);

    for (int t = 0; t < 10; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            if (t % 7 == 0) trees[u].inner.push_event(TreeD::HaltEvent::TookDamage);
            else if (t % 5 == 0) trees[u].inner.push_event(TreeD::HaltEvent::SpottedEnemy);
            else if (t % 11 == 0) trees[u].inner.push_event(TreeD::HaltEvent::LowAmmo);
            else trees[u].inner.push_event(TreeD::HaltEvent::None);
            trees[u].tick(bbs[u], static_cast<uint32_t>(t * 7u + u));
        }
    }

    std::vector<double> per_unit_times(scene.num_units, 0.0);
    auto t0 = std::chrono::steady_clock::now();
    int decisions = 0;
    for (int t = 0; t < scene.ticks_to_run; ++t) {
        for (int u = 0; u < scene.num_units; ++u) {
            bbs[u].refresh(t, rng);
            if (t % 7 == 0) trees[u].inner.push_event(TreeD::HaltEvent::TookDamage);
            else if (t % 5 == 0) trees[u].inner.push_event(TreeD::HaltEvent::SpottedEnemy);
            else if (t % 11 == 0) trees[u].inner.push_event(TreeD::HaltEvent::LowAmmo);
            else trees[u].inner.push_event(TreeD::HaltEvent::None);
            auto t1 = std::chrono::steady_clock::now();
            trees[u].tick(bbs[u], static_cast<uint32_t>(t * 7u + u));
            auto t2 = std::chrono::steady_clock::now();
            per_unit_times[u] += std::chrono::duration<double, std::nano>(t2 - t1).count();
            decisions++;
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t3 - t0).count();

    std::vector<double> ns_per_tick(scene.num_units);
    for (int u = 0; u < scene.num_units; ++u) ns_per_tick[u] = per_unit_times[u] / static_cast<double>(scene.ticks_to_run);
    std::sort(ns_per_tick.begin(), ns_per_tick.end());
    double mean = std::accumulate(ns_per_tick.begin(), ns_per_tick.end(), 0.0) / ns_per_tick.size();
    double med  = ns_per_tick[ns_per_tick.size() / 2];
    double p95  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.95)];
    double p99  = ns_per_tick[static_cast<size_t>(ns_per_tick.size() * 0.99)];
    double var = 0.0;
    for (auto v : ns_per_tick) var += (v - mean) * (v - mean);
    var /= ns_per_tick.size();
    return Result{
        "E_Blackboard", scene.name, seed, scene.num_units, scene.ticks_to_run,
        mean, med, p95, p99, std::sqrt(var), total_ms, decisions
    };
}

void write_csv_header(std::ofstream& out) {
    out << "strategy,scene,seed,num_units,ticks,mean_ns_per_unit_per_tick,median_ns_per_unit_per_tick,p95_ns_per_unit_per_tick,p99_ns_per_unit_per_tick,std_ns_per_unit_per_tick,total_ms,total_decisions\n";
}

void write_csv_row(std::ofstream& out, const Result& r) {
    out << r.strategy << "," << r.scene << "," << r.seed << "," << r.num_units << "," << r.ticks << ","
        << r.mean_ns_per_tick_per_unit << "," << r.median_ns_per_tick_per_unit << ","
        << r.p95_ns_per_tick_per_unit << "," << r.p99_ns_per_tick_per_unit << ","
        << r.std_ns_per_tick_per_unit << "," << r.total_ms << "," << r.total_decisions << "\n";
}

}  // anonymous namespace

int main() {
    std::printf("btree_bench — Hierarchical Tactical AI Behavior Tree benchmark\n");
    std::printf("Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG\n");

    const int seeds[5] = { 1, 7, 42, 1234, 31337 };
    auto scn = scenes();

    std::ofstream out("results.csv");
    if (!out) {
        std::fprintf(stderr, "ERROR: cannot open results.csv for writing\n");
        return 1;
    }
    write_csv_header(out);

    std::printf("\n%-25s %-20s %4s %8s %12s %12s %12s %12s %10s\n",
        "Strategy", "Scene", "Seed", "Units", "Mean (ns)", "Median (ns)", "p95 (ns)", "p99 (ns)", "Total (ms)");
    std::printf("---------------------------------------------------------------------------------------------------------------------\n");

    using RunFn = Result(*)(const Scene&, int);
    const RunFn runners[5] = {
        &run_strategy_a,
        &run_strategy_b,
        &run_strategy_c,
        &run_strategy_d,
        &run_strategy_e,
    };
    const char* strat_names[5] = {
        "A_NaiveNoMemory",
        "B_BT_RunningMemory",
        "C_Hierarchical_3Tier",
        "D_EventDriven",
        "E_Blackboard",
    };
    (void)strat_names;  // kept for reference; runners indexed by integer

    for (int si = 0; si < 5; ++si) {
        for (const auto& s : scn) {
            for (int seed : seeds) {
                Result r = runners[si](s, seed);
                write_csv_row(out, r);
                std::printf("%-25s %-20s %4d %8d %12.1f %12.1f %12.1f %12.1f %10.2f\n",
                    r.strategy.c_str(), r.scene.c_str(), r.seed, r.num_units,
                    r.mean_ns_per_tick_per_unit, r.median_ns_per_tick_per_unit,
                    r.p95_ns_per_tick_per_unit, r.p99_ns_per_tick_per_unit,
                    r.total_ms);
            }
        }
    }

    out.close();
    std::printf("\nResults written to results.csv\n");
    std::printf("Total configs: 5 strategies x 5 scenes x 5 seeds = 125 configs\n");
    return 0;
}
