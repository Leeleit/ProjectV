#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <memory>
#include <array>
#include <random>
#include <algorithm>
#include <cmath>
#include <string>
#include <map>
#include <string_view>
#include <fstream>
#include <bit>

struct WeaponStats {
    float recoil = 100.0f;
    float ergonomics = 50.0f;
    float weight = 3.5f;
    float range = 400.0f;
    float accuracy = 0.8f;

    WeaponStats& operator+=(const WeaponStats& other) {
        recoil += other.recoil;
        ergonomics += other.ergonomics;
        weight += other.weight;
        range += other.range;
        accuracy += other.accuracy;
        return *this;
    }
};

struct AttachmentNodeB {
    uint32_t id;
    WeaponStats modifiers;
    std::vector<std::unique_ptr<AttachmentNodeB>> children;
};

struct AttachmentNodeC {
    uint32_t id;
    int8_t parent_index;
    WeaponStats modifiers;
};

struct AttachmentComponentE {
    uint32_t weapon_id;
    WeaponStats modifiers;
};

class StrategyA {
private:
    WeaponStats base_stats;
    WeaponStats cached_stats;
    std::vector<WeaponStats> attachment_list;

    void recalculate() {
        cached_stats = base_stats;
        for (const auto& mod : attachment_list) {
            cached_stats += mod;
        }
    }

public:
    StrategyA(const WeaponStats& base, const std::vector<WeaponStats>& attachments)
        : base_stats(base), attachment_list(attachments) {
        recalculate();
    }

    WeaponStats Query() const {
        return cached_stats;
    }

    void Swap(size_t index, const WeaponStats& new_mod) {
        if (index < attachment_list.size()) {
            attachment_list[index] = new_mod;
            recalculate();
        }
    }

    size_t GetMemorySize() const {
        return sizeof(StrategyA) + attachment_list.size() * sizeof(WeaponStats);
    }
};

class StrategyB {
private:
    WeaponStats base_stats;
    std::unique_ptr<AttachmentNodeB> root;

    void traverse(const AttachmentNodeB* node, WeaponStats& total) const {
        if (!node) return;
        total += node->modifiers;
        for (const auto& child : node->children) {
            traverse(child.get(), total);
        }
    }

    AttachmentNodeB* find_node(AttachmentNodeB* node, uint32_t id) {
        if (!node) return nullptr;
        if (node->id == id) return node;
        for (auto& child : node->children) {
            auto* found = find_node(child.get(), id);
            if (found) return found;
        }
        return nullptr;
    }

    size_t count_memory(const AttachmentNodeB* node) const {
        if (!node) return 0;
        size_t total = sizeof(AttachmentNodeB) + sizeof(std::unique_ptr<AttachmentNodeB>) * node->children.size();
        for (const auto& child : node->children) {
            total += count_memory(child.get());
        }
        return total;
    }

public:
    StrategyB(const WeaponStats& base, std::unique_ptr<AttachmentNodeB> attachment_root)
        : base_stats(base), root(std::move(attachment_root)) {}

    WeaponStats Query() const {
        WeaponStats total = base_stats;
        traverse(root.get(), total);
        return total;
    }

    void Swap(uint32_t id, const WeaponStats& new_mod) {
        auto* node = find_node(root.get(), id);
        if (node) {
            node->modifiers = new_mod;
        }
    }

    size_t GetMemorySize() const {
        return sizeof(StrategyB) + count_memory(root.get());
    }
};

class StrategyC {
private:
    WeaponStats base_stats;
    std::array<AttachmentNodeC, 16> nodes;
    uint8_t count = 0;

public:
    StrategyC(const WeaponStats& base, const std::vector<AttachmentNodeC>& initial_nodes)
        : base_stats(base) {
        count = static_cast<uint8_t>(std::min(initial_nodes.size(), size_t(16)));
        for (uint8_t i = 0; i < count; ++i) {
            nodes[i] = initial_nodes[i];
        }
    }

    WeaponStats Query() const {
        WeaponStats total = base_stats;
        for (uint8_t i = 0; i < count; ++i) {
            total += nodes[i].modifiers;
        }
        return total;
    }

    void Swap(uint32_t id, const WeaponStats& new_mod) {
        for (uint8_t i = 0; i < count; ++i) {
            if (nodes[i].id == id) {
                nodes[i].modifiers = new_mod;
                break;
            }
        }
    }

    size_t GetMemorySize() const {
        return sizeof(StrategyC);
    }
};

class StrategyD {
private:
    WeaponStats base_stats;
    uint64_t active_mask = 0;
    std::array<WeaponStats, 64> lookup_table;

public:
    StrategyD(const WeaponStats& base, uint64_t mask, const std::array<WeaponStats, 64>& table)
        : base_stats(base), active_mask(mask), lookup_table(table) {}

    WeaponStats Query() const {
        WeaponStats total = base_stats;
        uint64_t mask = active_mask;
        while (mask > 0) {
            int idx = std::countr_zero(mask);
            total += lookup_table[idx];
            mask &= mask - 1;
        }
        return total;
    }

    void Swap(uint32_t slot_idx, const WeaponStats& new_mod) {
        if (slot_idx < 64) {
            lookup_table[slot_idx] = new_mod;
            active_mask |= (1ULL << slot_idx);
        }
    }

    size_t GetMemorySize() const {
        return sizeof(StrategyD);
    }
};

class StrategyE {
private:
    WeaponStats base_stats;
    uint32_t weapon_id;
    const std::vector<AttachmentComponentE>* registry;

public:
    StrategyE(const WeaponStats& base, uint32_t id, const std::vector<AttachmentComponentE>* reg)
        : base_stats(base), weapon_id(id), registry(reg) {}

    WeaponStats Query() const {
        WeaponStats total = base_stats;
        for (const auto& comp : *registry) {
            if (comp.weapon_id == weapon_id) {
                total += comp.modifiers;
            }
        }
        return total;
    }

    void Swap(std::vector<AttachmentComponentE>& reg, uint32_t attachment_index, const WeaponStats& new_mod) {
        size_t match_count = 0;
        for (auto& comp : reg) {
            if (comp.weapon_id == weapon_id) {
                if (match_count == attachment_index) {
                    comp.modifiers = new_mod;
                    break;
                }
                match_count++;
            }
        }
    }

    size_t GetMemorySize() const {
        return sizeof(StrategyE);
    }
};

std::unique_ptr<AttachmentNodeB> create_tree_b(uint32_t& id_gen, int depth, int max_depth, std::mt19937& rng, std::uniform_real_distribution<float>& dist) {
    if (depth > max_depth) return nullptr;
    auto node = std::make_unique<AttachmentNodeB>();
    node->id = id_gen++;
    node->modifiers = { dist(rng), dist(rng), dist(rng), dist(rng), dist(rng) };
    int num_children = (max_depth - depth >= 2) ? 2 : 1;
    for (int i = 0; i < num_children; ++i) {
        auto child = create_tree_b(id_gen, depth + 1, max_depth, rng, dist);
        if (child) {
            node->children.push_back(std::move(child));
        }
    }
    return node;
}

void flatten_tree(const AttachmentNodeB* node, int8_t parent_idx, std::vector<AttachmentNodeC>& flat_nodes) {
    if (!node) return;
    int8_t current_idx = static_cast<int8_t>(flat_nodes.size());
    flat_nodes.push_back({ node->id, parent_idx, node->modifiers });
    for (const auto& child : node->children) {
        flatten_tree(child.get(), current_idx, flat_nodes);
    }
}

int main() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

    WeaponStats base_weapon_stats = { 100.0f, 50.0f, 3.5f, 400.0f, 0.8f };

    std::vector<std::vector<WeaponStats>> attachments_s(5);
    std::vector<std::unique_ptr<AttachmentNodeB>> trees_b(5);
    std::vector<std::vector<AttachmentNodeC>> nodes_c(5);
    std::vector<uint64_t> masks_d(5);
    std::array<WeaponStats, 64> table_d;
    for (size_t i = 0; i < 64; ++i) {
        table_d[i] = { dist(rng), dist(rng), dist(rng), dist(rng), dist(rng) };
    }

    std::vector<AttachmentComponentE> registry_e;

    std::vector<int> scene_counts = { 0, 3, 6, 12, 16 };

    for (int s = 0; s < 5; ++s) {
        int count = scene_counts[s];
        for (int i = 0; i < count; ++i) {
            attachments_s[s].push_back({ dist(rng), dist(rng), dist(rng), dist(rng), dist(rng) });
        }

        uint32_t id_gen = 0;
        int max_depth = 1;
        if (count >= 12) max_depth = 4;
        else if (count >= 6) max_depth = 3;
        else if (count >= 3) max_depth = 2;

        trees_b[s] = create_tree_b(id_gen, 0, max_depth, rng, dist);
        flatten_tree(trees_b[s].get(), -1, nodes_c[s]);

        uint64_t mask = 0;
        for (int i = 0; i < count; ++i) {
            mask |= (1ULL << i);
        }
        masks_d[s] = mask;

        for (int i = 0; i < count; ++i) {
            registry_e.push_back({ static_cast<uint32_t>(s), { dist(rng), dist(rng), dist(rng), dist(rng), dist(rng) } });
        }
    }

    std::vector<std::string> scene_names = { "s1_naked", "s2_standard", "s3_tactical", "s4_heavy", "s5_max_nested" };
    std::vector<std::string> strategy_names = { "A_NaiveFlatStruct", "B_DynamicTreeQuery", "C_FlatCachedBuffer", "D_BitmaskFeatureMap", "E_SparseSoAComponent" };

    std::ofstream csv("prototype/build/results.csv");
    csv << "Strategy,Scene,Seed,Iteration,QueryTimeNs,SwapTimeNs\n";

    std::ofstream summary("prototype/build/summary_means.csv");
    summary << "Strategy,Scene,MeanQueryNs,MeanSwapNs,MemoryBytes\n";

    for (int strat = 0; strat < 5; ++strat) {
        for (int s = 0; s < 5; ++s) {
            double total_query_ns = 0.0;
            double total_swap_ns = 0.0;
            size_t memory_bytes = 0;

            for (int seed = 0; seed < 5; ++seed) {
                std::mt19937 run_rng(seed);
                std::uniform_real_distribution<float> run_dist(-2.0f, 2.0f);

                WeaponStats active_base = base_weapon_stats;

                if (strat == 0) {
                    StrategyA sa(active_base, attachments_s[s]);
                    memory_bytes = sa.GetMemorySize();

                    auto t1 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        auto stats = sa.Query();
                        volatile float temp = stats.recoil;
                        (void)temp;
                    }
                    auto t2 = std::chrono::high_resolution_clock::now();
                    total_query_ns += std::chrono::duration<double, std::nano>(t2 - t1).count() / 1000.0;

                    auto t3 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        sa.Swap(i % std::max(size_t(1), attachments_s[s].size()), { run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng) });
                    }
                    auto t4 = std::chrono::high_resolution_clock::now();
                    total_swap_ns += std::chrono::duration<double, std::nano>(t4 - t3).count() / 1000.0;

                } else if (strat == 1) {
                    uint32_t id_gen = 0;
                    auto root_copy = create_tree_b(id_gen, 0, (scene_counts[s] >= 12 ? 4 : (scene_counts[s] >= 6 ? 3 : (scene_counts[s] >= 3 ? 2 : 1))), run_rng, run_dist);
                    StrategyB sb(active_base, std::move(root_copy));
                    memory_bytes = sb.GetMemorySize();

                    auto t1 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        auto stats = sb.Query();
                        volatile float temp = stats.recoil;
                        (void)temp;
                    }
                    auto t2 = std::chrono::high_resolution_clock::now();
                    total_query_ns += std::chrono::duration<double, std::nano>(t2 - t1).count() / 1000.0;

                    auto t3 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        sb.Swap(i % std::max(1, scene_counts[s]), { run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng) });
                    }
                    auto t4 = std::chrono::high_resolution_clock::now();
                    total_swap_ns += std::chrono::duration<double, std::nano>(t4 - t3).count() / 1000.0;

                } else if (strat == 2) {
                    StrategyC sc(active_base, nodes_c[s]);
                    memory_bytes = sc.GetMemorySize();

                    auto t1 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        auto stats = sc.Query();
                        volatile float temp = stats.recoil;
                        (void)temp;
                    }
                    auto t2 = std::chrono::high_resolution_clock::now();
                    total_query_ns += std::chrono::duration<double, std::nano>(t2 - t1).count() / 1000.0;

                    auto t3 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        sc.Swap(i % std::max(1, scene_counts[s]), { run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng) });
                    }
                    auto t4 = std::chrono::high_resolution_clock::now();
                    total_swap_ns += std::chrono::duration<double, std::nano>(t4 - t3).count() / 1000.0;

                } else if (strat == 3) {
                    StrategyD sd(active_base, masks_d[s], table_d);
                    memory_bytes = sd.GetMemorySize();

                    auto t1 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        auto stats = sd.Query();
                        volatile float temp = stats.recoil;
                        (void)temp;
                    }
                    auto t2 = std::chrono::high_resolution_clock::now();
                    total_query_ns += std::chrono::duration<double, std::nano>(t2 - t1).count() / 1000.0;

                    auto t3 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        sd.Swap(i % 64, { run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng) });
                    }
                    auto t4 = std::chrono::high_resolution_clock::now();
                    total_swap_ns += std::chrono::duration<double, std::nano>(t4 - t3).count() / 1000.0;

                } else if (strat == 4) {
                    StrategyE se(active_base, static_cast<uint32_t>(s), &registry_e);
                    memory_bytes = se.GetMemorySize();

                    auto t1 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        auto stats = se.Query();
                        volatile float temp = stats.recoil;
                        (void)temp;
                    }
                    auto t2 = std::chrono::high_resolution_clock::now();
                    total_query_ns += std::chrono::duration<double, std::nano>(t2 - t1).count() / 1000.0;

                    auto registry_copy = registry_e;
                    auto t3 = std::chrono::high_resolution_clock::now();
                    for (int i = 0; i < 1000; ++i) {
                        se.Swap(registry_copy, i % std::max(1, scene_counts[s]), { run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng), run_dist(run_rng) });
                    }
                    auto t4 = std::chrono::high_resolution_clock::now();
                    total_swap_ns += std::chrono::duration<double, std::nano>(t4 - t3).count() / 1000.0;
                }
            }

            double mean_query = total_query_ns / 5.0;
            double mean_swap = total_swap_ns / 5.0;

            summary << strategy_names[strat] << "," << scene_names[s] << "," << mean_query << "," << mean_swap << "," << memory_bytes << "\n";
            std::cout << strategy_names[strat] << " | " << scene_names[s] << " | Query: " << mean_query << " ns | Swap: " << mean_swap << " ns | Memory: " << memory_bytes << " B\n";

            for (int seed = 0; seed < 5; ++seed) {
                csv << strategy_names[strat] << "," << scene_names[s] << "," << seed << ",0," << mean_query << "," << mean_swap << "\n";
            }
        }
    }

    return 0;
}
