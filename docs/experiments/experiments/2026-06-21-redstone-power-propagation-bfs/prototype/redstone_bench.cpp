// redstone_bench.cpp — Redstone signal propagation BFS benchmark
// Standalone C++26 CPU prototype. Per docs/experiments protocol.
// Strategies: A_FullBFS, B_Queue256, C_Queue512, D_AltCurrent, E_TickSched
// Scenes: simple_line, torch_tower, repeater_chain, comparator_scale, full_adder_8bit
// 5 seeds, 1000 iter per config

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <queue>
#include <string>
#include <vector>

constexpr int kMaxSignal = 15;
constexpr int kSeedsList[] = {1, 7, 42, 1234, 31337};
constexpr int kNumSeeds = 5;
constexpr int kNumIter = 1000;
constexpr int kWarmup = 10;

enum NodeType : uint8_t {
    WIRE       = 0,
    SOURCE     = 1,
    REPEATER   = 2,
    COMPARATOR = 3,
    PISTON     = 4,
    TORCH      = 5,
    AIR        = 6,
};

struct Node {
    NodeType type = AIR;
    uint8_t signal_in = 0;
    uint8_t signal_out = 0;
    uint8_t target_level = 0;
    uint8_t delay_remaining = 0;
    uint8_t delay_max = 0;
    bool comparator_subtract = false;
    int x = 0, y = 0, z = 0;
    std::vector<int> inputs;
    std::vector<int> outputs;
};

struct RedstoneNetwork {
    std::vector<Node> nodes;
    std::vector<int> source_nodes;
    int num_wires = 0;

    void clear() { nodes.clear(); source_nodes.clear(); num_wires = 0; }

    int add_node(NodeType type, int x, int y, int z) {
        int idx = (int)nodes.size();
        Node n;
        n.type = type; n.x = x; n.y = y; n.z = z;
        nodes.push_back(n);
        if (type == SOURCE || type == TORCH) source_nodes.push_back(idx);
        if (type == WIRE) num_wires++;
        return idx;
    }

    void add_edge(int from, int to) {
        nodes[from].outputs.push_back(to);
        nodes[to].inputs.push_back(from);
    }

    uint64_t signal_energy() const {
        uint64_t e = 0;
        for (auto& n : nodes) e += n.signal_out;
        return e;
    }

    // deep copy including vectors
    RedstoneNetwork clone() const {
        RedstoneNetwork r;
        r.nodes = nodes;
        r.source_nodes = source_nodes;
        r.num_wires = num_wires;
        return r;
    }
};

// ============================================================
// Scene builders
// ============================================================

RedstoneNetwork build_simple_line() {
    RedstoneNetwork net;
    net.add_node(SOURCE, 0, 0, 0);
    for (int i = 1; i <= 15; i++) net.add_node(WIRE, i, 0, 0);
    for (int i = 0; i < 15; i++) net.add_edge(i, i + 1);
    return net;
}

RedstoneNetwork build_torch_tower() {
    RedstoneNetwork net;
    net.add_node(SOURCE, 0, 0, 0);
    net.add_node(TORCH, 0, 1, 0);
    for (int y = 2; y <= 8; y++) net.add_node(WIRE, 0, y, 0);
    net.add_edge(0, 1);
    for (int y = 1; y < 7; y++) net.add_edge(y + 1, y + 2);
    net.add_node(WIRE, 1, 4, 0);
    net.add_node(WIRE, -1, 4, 0);
    net.add_edge(5, 9);
    net.add_edge(5, 10);
    return net;
}

RedstoneNetwork build_repeater_chain() {
    RedstoneNetwork net;
    net.add_node(SOURCE, 0, 0, 0);
    net.add_node(WIRE, 1, 0, 0);
    for (int i = 0; i < 4; i++) {
        net.add_node(REPEATER, 3 + i * 3, 0, 0);
        net.add_node(WIRE, 4 + i * 3, 0, 0);
        net.add_node(WIRE, 5 + i * 3, 0, 0);
    }
    net.add_edge(0, 1);
    net.add_edge(1, 2);
    for (int i = 0; i < 4; i++) {
        int r = 2 + i * 3, w1 = 3 + i * 3, w2 = 4 + i * 3;
        int nr = (i < 3) ? (5 + i * 3) : -1;
        net.add_edge(r, w1); net.add_edge(w1, w2);
        if (nr >= 0) net.add_edge(w2, nr);
        net.nodes[r].delay_max = 2;
    }
    return net;
}

RedstoneNetwork build_comparator_scale() {
    RedstoneNetwork net;
    net.add_node(SOURCE, 0, 0, 0);
    net.add_node(WIRE, 1, 0, 0);
    net.add_node(COMPARATOR, 2, 0, 0);
    net.add_node(SOURCE, 0, 1, 0);
    net.add_node(WIRE, 1, 1, 0);
    net.add_node(WIRE, 3, 0, 0);
    net.add_edge(0, 1); net.add_edge(1, 2);
    net.add_edge(3, 4); net.add_edge(4, 2);
    net.add_edge(2, 5);
    return net;
}

RedstoneNetwork build_full_adder_8bit() {
    RedstoneNetwork net;
    int carry = net.add_node(WIRE, 0, 0, 0);
    for (int bit = 0; bit < 8; bit++) {
        int x = bit * 4;
        int a = net.add_node(SOURCE, x, 0, 0);
        int b = net.add_node(SOURCE, x + 1, 0, 0);
        int wa = net.add_node(WIRE, x, 1, 0);
        int wb = net.add_node(WIRE, x + 1, 1, 0);
        int xor1 = net.add_node(TORCH, x, 2, 0);
        int xor2 = net.add_node(TORCH, x + 1, 2, 0);
        int sum_pre = net.add_node(WIRE, x, 3, 0);
        int sum = net.add_node(TORCH, x, 4, 0);
        int and1 = net.add_node(TORCH, x + 2, 1, 0);
        int and2 = net.add_node(TORCH, x + 2, 2, 0);
        int carry_next = net.add_node(WIRE, x + 2, 3, 0);

        net.add_edge(a, wa); net.add_edge(b, wb);
        net.add_edge(wa, xor1); net.add_edge(wb, xor1);
        net.add_edge(wa, xor2); net.add_edge(wb, xor2);
        net.add_edge(xor1, sum_pre); net.add_edge(carry, sum_pre);
        net.add_edge(sum_pre, sum);
        net.add_edge(wa, and1); net.add_edge(wb, and1);
        net.add_edge(carry, and2); net.add_edge(wa, and2); net.add_edge(wb, and2);
        net.add_edge(and1, carry_next); net.add_edge(and2, carry_next);
        carry = carry_next;
    }
    return net;
}

using BuilderFunc = RedstoneNetwork (*)();
constexpr BuilderFunc kSceneBuilders[] = {
    build_simple_line, build_torch_tower, build_repeater_chain,
    build_comparator_scale, build_full_adder_8bit,
};
constexpr const char* kSceneNames[] = {
    "simple_line", "torch_tower", "repeater_chain",
    "comparator_scale", "full_adder_8bit",
};
constexpr int kNumScenes = 5;

// ============================================================
// Tick result + strategy implementations
// ============================================================

struct TickResult {
    uint64_t elapsed_ns = 0;
    uint64_t nodes_visited = 0;
    uint64_t queue_peak = 0;
    uint64_t signal_energy = 0;
    int convergence_ticks = 100;
    double psnr = 0.0;
};

// Compute PSNR between two signal arrays (uint8_t)
double compute_psnr(const std::vector<uint8_t>& ref, const std::vector<uint8_t>& test) {
    double mse = 0.0;
    int n = (int)ref.size();
    for (int i = 0; i < n; i++) {
        double d = (double)ref[i] - (double)test[i];
        mse += d * d;
    }
    mse /= (double)n;
    if (mse < 1e-10) return 99.9;
    return 10.0 * log10((255.0 * 255.0) / mse);
}

// Capture ground-truth signal state
std::vector<uint8_t> capture_signals(const RedstoneNetwork& net) {
    std::vector<uint8_t> sig(net.nodes.size(), 0);
    for (size_t i = 0; i < net.nodes.size(); i++)
        sig[i] = net.nodes[i].signal_out;
    return sig;
}

// A_FullBFS — full BFS recompute every tick (vanilla baseline)
TickResult tick_full_bfs(RedstoneNetwork& net, int ticks = 100) {
    TickResult r;
    auto t0 = std::chrono::steady_clock::now();

    for (int tick = 0; tick < ticks; tick++) {
        for (auto& n : net.nodes) { n.signal_in = 0; n.signal_out = 0; }

        std::queue<int> q;
        for (int s : net.source_nodes) {
            net.nodes[s].signal_out = kMaxSignal;
            q.push(s); r.nodes_visited++;
        }

        while (!q.empty()) {
            int v = q.front(); q.pop();
            if ((int)q.size() > (int)r.queue_peak)
                r.queue_peak = q.size();

            Node& node = net.nodes[v];

            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                r.nodes_visited++;

                switch (child.type) {
                case WIRE: {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > child.signal_in) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case REPEATER: {
                    if (node.signal_out > 0) {
                        child.target_level = kMaxSignal;
                        if (child.delay_remaining == 0 && child.signal_out == 0) {
                            child.delay_remaining = child.delay_max;
                            q.push(o);
                        }
                    }
                    break;
                }
                case COMPARATOR: {
                    uint8_t back = 0, side = 0;
                    for (int ci : child.inputs) {
                        if (ci == v) back = node.signal_out;
                        else side = (net.nodes[ci].signal_out > side) ? net.nodes[ci].signal_out : side;
                    }
                    uint8_t sig = (back >= side) ? back : 0;
                    if (child.comparator_subtract)
                        sig = (back > side) ? (back - side) : 0;
                    if (sig > child.signal_out) {
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case TORCH: {
                    uint8_t sig = (node.signal_out > 0) ? 0 : kMaxSignal;
                    if (sig != child.signal_out) {
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case PISTON:
                    child.signal_out = node.signal_out;
                    break;
                default: break;
                }
            }
        }

        for (auto& n : net.nodes) {
            if (n.type == REPEATER && n.delay_remaining > 0) {
                n.delay_remaining--;
                if (n.delay_remaining == 0) n.signal_out = n.target_level;
            }
        }

        r.signal_energy += net.signal_energy();
    }
    r.convergence_ticks = ticks;

    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return r;
}

// B_Queue256 — budget-limited BFS (256 entries/tick)
TickResult tick_budget256(RedstoneNetwork& net, int ticks = 100) {
    TickResult r;
    int budget = 256;
    auto t0 = std::chrono::steady_clock::now();

    for (int tick = 0; tick < ticks; tick++) {
        for (auto& n : net.nodes) { n.signal_in = 0; n.signal_out = 0; }

        std::queue<int> q;
        for (int s : net.source_nodes) {
            net.nodes[s].signal_out = kMaxSignal;
            q.push(s); r.nodes_visited++;
        }

        int entries_this_tick = 0;
        while (!q.empty() && entries_this_tick < budget) {
            int v = q.front(); q.pop();
            entries_this_tick++;
            if ((int)q.size() > (int)r.queue_peak)
                r.queue_peak = q.size();

            Node& node = net.nodes[v];
            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                r.nodes_visited++;

                switch (child.type) {
                case WIRE: {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > child.signal_in) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case REPEATER: {
                    if (node.signal_out > 0) {
                        child.target_level = kMaxSignal;
                        if (child.delay_remaining == 0 && child.signal_out == 0) {
                            child.delay_remaining = child.delay_max;
                            if (entries_this_tick < budget) q.push(o);
                        }
                    }
                    break;
                }
                case COMPARATOR: {
                    uint8_t back = 0, side = 0;
                    for (int ci : child.inputs) {
                        if (ci == v) back = node.signal_out;
                        else side = (net.nodes[ci].signal_out > side) ? net.nodes[ci].signal_out : side;
                    }
                    uint8_t sig = (back >= side) ? back : 0;
                    if (child.comparator_subtract)
                        sig = (back > side) ? (back - side) : 0;
                    if (sig > child.signal_out) {
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case TORCH: {
                    uint8_t sig = (node.signal_out > 0) ? 0 : kMaxSignal;
                    if (sig != child.signal_out) {
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case PISTON:
                    child.signal_out = node.signal_out;
                    break;
                default: break;
                }
            }
        }

        for (auto& n : net.nodes) {
            if (n.type == REPEATER && n.delay_remaining > 0) {
                n.delay_remaining--;
                if (n.delay_remaining == 0) n.signal_out = n.target_level;
            }
        }

        r.signal_energy += net.signal_energy();
    }
    r.convergence_ticks = ticks;

    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return r;
}

// C_Queue512 — budget-limited BFS (512 entries/tick)
TickResult tick_budget512(RedstoneNetwork& net, int ticks = 100) {
    TickResult r;
    int budget = 512;
    auto t0 = std::chrono::steady_clock::now();

    for (int tick = 0; tick < ticks; tick++) {
        for (auto& n : net.nodes) { n.signal_in = 0; n.signal_out = 0; }

        std::queue<int> q;
        for (int s : net.source_nodes) {
            net.nodes[s].signal_out = kMaxSignal;
            q.push(s); r.nodes_visited++;
        }

        int entries_this_tick = 0;
        while (!q.empty() && entries_this_tick < budget) {
            int v = q.front(); q.pop();
            entries_this_tick++;
            if ((int)q.size() > (int)r.queue_peak)
                r.queue_peak = q.size();

            Node& node = net.nodes[v];
            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                r.nodes_visited++;

                switch (child.type) {
                case WIRE: {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > child.signal_in) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case REPEATER: {
                    if (node.signal_out > 0) {
                        child.target_level = kMaxSignal;
                        if (child.delay_remaining == 0 && child.signal_out == 0) {
                            child.delay_remaining = child.delay_max;
                            if (entries_this_tick < budget) q.push(o);
                        }
                    }
                    break;
                }
                case COMPARATOR: {
                    uint8_t back = 0, side = 0;
                    for (int ci : child.inputs) {
                        if (ci == v) back = node.signal_out;
                        else side = (net.nodes[ci].signal_out > side) ? net.nodes[ci].signal_out : side;
                    }
                    uint8_t sig = (back >= side) ? back : 0;
                    if (child.comparator_subtract)
                        sig = (back > side) ? (back - side) : 0;
                    if (sig > child.signal_out) {
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case TORCH: {
                    uint8_t sig = (node.signal_out > 0) ? 0 : kMaxSignal;
                    if (sig != child.signal_out) {
                        child.signal_out = sig;
                        if (entries_this_tick < budget) q.push(o);
                    }
                    break;
                }
                case PISTON:
                    child.signal_out = node.signal_out;
                    break;
                default: break;
                }
            }
        }

        for (auto& n : net.nodes) {
            if (n.type == REPEATER && n.delay_remaining > 0) {
                n.delay_remaining--;
                if (n.delay_remaining == 0) n.signal_out = n.target_level;
            }
        }

        r.signal_energy += net.signal_energy();
    }
    r.convergence_ticks = ticks;

    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return r;
}

// D_AltCurrent — graph-based: find external sources, propagate from there
// Simplified Alternate Current pattern: build wire graph, find source wires,
// spread power in topological order, only emit block updates at final level
TickResult tick_altcurrent(RedstoneNetwork& net, int ticks = 100) {
    TickResult r;
    auto t0 = std::chrono::steady_clock::now();

    // Precompute topological order for propagation
    std::vector<int> topo;
    {
        std::vector<int> in_deg(net.nodes.size(), 0);
        for (auto& n : net.nodes)
            for (int o : n.outputs) in_deg[o]++;
        std::queue<int> q;
        for (int i = 0; i < (int)net.nodes.size(); i++)
            if (in_deg[i] == 0) q.push(i);
        while (!q.empty()) {
            int v = q.front(); q.pop();
            topo.push_back(v);
            for (int o : net.nodes[v].outputs)
                if (--in_deg[o] == 0) q.push(o);
        }
    }

    for (int tick = 0; tick < ticks; tick++) {
        for (auto& n : net.nodes) { n.signal_in = 0; n.signal_out = 0; }

        // Phase 1: set source signals
        for (int s : net.source_nodes) {
            net.nodes[s].signal_out = kMaxSignal;
            r.nodes_visited++;
        }

        // Phase 2: propagate in topological order (each wire touched at most once)
        for (int v : topo) {
            Node& node = net.nodes[v];
            if (node.signal_out == 0 && node.type != SOURCE && node.type != TORCH)
                continue;  // no incoming signal

            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                r.nodes_visited++;

                switch (child.type) {
                case WIRE: {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > child.signal_in) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                    }
                    break;
                }
                case REPEATER: {
                    if (node.signal_out > 0) {
                        child.target_level = kMaxSignal;
                        if (child.delay_remaining == 0 && child.signal_out == 0) {
                            child.delay_remaining = child.delay_max;
                        }
                    }
                    break;
                }
                case COMPARATOR: {
                    uint8_t back = 0, side = 0;
                    for (int ci : child.inputs) {
                        if (net.nodes[ci].signal_out > 0) {
                            if (ci == v) back = node.signal_out;
                            else side = (net.nodes[ci].signal_out > side) ? net.nodes[ci].signal_out : side;
                        }
                    }
                    uint8_t sig = (back >= side) ? back : 0;
                    if (child.comparator_subtract)
                        sig = (back > side) ? (back - side) : 0;
                    if (sig > child.signal_out) {
                        child.signal_out = sig;
                    }
                    break;
                }
                case TORCH: {
                    uint8_t sig = (node.signal_out > 0) ? 0 : kMaxSignal;
                    child.signal_out = sig;
                    break;
                }
                case PISTON:
                    child.signal_out = node.signal_out;
                    break;
                default: break;
                }
            }
        }

        // Phase 3: resolve repeater delays
        for (auto& n : net.nodes) {
            if (n.type == REPEATER && n.delay_remaining > 0) {
                n.delay_remaining--;
                if (n.delay_remaining == 0) n.signal_out = n.target_level;
            }
        }

        r.signal_energy += net.signal_energy();
    }
    r.convergence_ticks = ticks;

    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return r;
}

// E_TickScheduled — BFS with tick scheduling for delayed components
// Uses a per-tick event queue for repeaters/comparators instead of polling
TickResult tick_ticksched(RedstoneNetwork& net, int ticks = 100) {
    TickResult r;
    auto t0 = std::chrono::steady_clock::now();

    // Tick event queue: pairs of (node_index, tick_to_fire)
    std::queue<std::pair<int, int>> tick_queue;

    for (int tick = 0; tick < ticks; tick++) {
        for (auto& n : net.nodes) { n.signal_in = 0; n.signal_out = 0; }

        std::queue<int> q;
        for (int s : net.source_nodes) {
            net.nodes[s].signal_out = kMaxSignal;
            q.push(s); r.nodes_visited++;
        }

        while (!q.empty()) {
            int v = q.front(); q.pop();
            if ((int)q.size() > (int)r.queue_peak)
                r.queue_peak = q.size();

            Node& node = net.nodes[v];
            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                r.nodes_visited++;

                switch (child.type) {
                case WIRE: {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > child.signal_in) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case REPEATER: {
                    if (node.signal_out > 0 && child.signal_out == 0) {
                        // Schedule activation after delay
                        tick_queue.push({o, tick + child.delay_max + 1});
                    }
                    break;
                }
                case COMPARATOR: {
                    uint8_t back = 0, side = 0;
                    for (int ci : child.inputs) {
                        if (ci == v) back = node.signal_out;
                        else side = (net.nodes[ci].signal_out > side) ? net.nodes[ci].signal_out : side;
                    }
                    uint8_t sig = (back >= side) ? back : 0;
                    if (child.comparator_subtract)
                        sig = (back > side) ? (back - side) : 0;
                    if (sig > child.signal_out) {
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case TORCH: {
                    uint8_t sig = (node.signal_out > 0) ? 0 : kMaxSignal;
                    if (sig != child.signal_out) {
                        child.signal_out = sig;
                        q.push(o);
                    }
                    break;
                }
                case PISTON:
                    child.signal_out = node.signal_out;
                    break;
                default: break;
                }
            }
        }

        // Fire scheduled events for this tick
        while (!tick_queue.empty() && tick_queue.front().second <= tick) {
            int idx = tick_queue.front().first;
            tick_queue.pop();
            Node& n = net.nodes[idx];
            if (n.type == REPEATER && n.target_level > 0) {
                n.signal_out = n.target_level;
                // Re-enqueue outputs
                for (int o : n.outputs) {
                    if (net.nodes[o].type == WIRE) q.push(o);
                }
            }
        }

        // Drain remaining re-enqueues from fired repeaters
        while (!q.empty()) {
            int v = q.front(); q.pop();
            Node& node = net.nodes[v];
            for (int o : node.outputs) {
                Node& child = net.nodes[o];
                if (child.type == WIRE && child.signal_in == 0) {
                    uint8_t sig = (node.signal_out > 0) ? (node.signal_out - 1) : 0;
                    if (sig > 0) {
                        child.signal_in = sig;
                        child.signal_out = sig;
                        q.push(o);
                    }
                }
            }
        }

        r.signal_energy += net.signal_energy();
    }
    r.convergence_ticks = ticks;

    auto t1 = std::chrono::steady_clock::now();
    r.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return r;
}

// ============================================================
// Strategy table
// ============================================================
using StrategyFunc = TickResult (*)(RedstoneNetwork&, int);
struct Strategy { const char* name; StrategyFunc func; };
constexpr Strategy kStrategies[] = {
    {"A_FullBFS",      tick_full_bfs},
    {"B_Queue256",     tick_budget256},
    {"C_Queue512",     tick_budget512},
    {"D_AltCurrent",   tick_altcurrent},
    {"E_TickSched",    tick_ticksched},
};
constexpr int kNumStrategies = 5;

// ============================================================
// PSNR computation against FullBFS ground truth
// ============================================================
double compute_psnr_vs_fullbfs(RedstoneNetwork& net, StrategyFunc func, int ticks) {
    RedstoneNetwork ref_net = net.clone();
    TickResult ref_r = tick_full_bfs(ref_net, ticks);
    std::vector<uint8_t> ref_sig = capture_signals(ref_net);

    RedstoneNetwork test_net = net.clone();
    TickResult test_r = func(test_net, ticks);
    std::vector<uint8_t> test_sig = capture_signals(test_net);

    return compute_psnr(ref_sig, test_sig);
}

// ============================================================
// Main benchmark harness
// ============================================================
int main() {
    std::printf("strategy,scene,seed,elapsed_ns,nodes_visited,queue_peak,signal_energy,convergence_ticks,psnr_db\n");

    for (int si = 0; si < kNumStrategies; si++) {
        for (int sci = 0; sci < kNumScenes; sci++) {
            for (int seed_idx = 0; seed_idx < kNumSeeds; seed_idx++) {
                int seed = kSeedsList[seed_idx];
                (void)seed;

                RedstoneNetwork net = kSceneBuilders[sci]();
                StrategyFunc func = kStrategies[si].func;

                // Warmup
                for (int w = 0; w < kWarmup; w++) {
                    RedstoneNetwork wnet = net.clone();
                    func(wnet, 100);
                }

                // Measurement
                uint64_t total_ns = 0;
                uint64_t total_visited = 0;
                uint64_t total_queue_peak = 0;
                uint64_t total_energy = 0;
                int total_convergence = 0;
                double psnr_sum = 0.0;

                for (int iter = 0; iter < kNumIter; iter++) {
                    RedstoneNetwork inet = net.clone();
                    TickResult r = func(inet, 100);
                    total_ns += r.elapsed_ns;
                    total_visited += r.nodes_visited;
                    total_queue_peak += r.queue_peak;
                    total_energy += r.signal_energy;
                    total_convergence += r.convergence_ticks;
                    psnr_sum += compute_psnr_vs_fullbfs(net, func, 100);
                }

                double mean_ns = (double)total_ns / kNumIter;
                double mean_visited = (double)total_visited / kNumIter;
                double mean_queue_peak = (double)total_queue_peak / kNumIter;
                double mean_energy = (double)total_energy / kNumIter;
                double mean_convergence = (double)total_convergence / kNumIter;
                double mean_psnr = psnr_sum / kNumIter;

                std::printf("%s,%s,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f\n",
                    kStrategies[si].name,
                    kSceneNames[sci],
                    seed,
                    mean_ns,
                    mean_visited,
                    mean_queue_peak,
                    mean_energy,
                    mean_convergence,
                    mean_psnr);
            }
        }
    }

    return 0;
}
