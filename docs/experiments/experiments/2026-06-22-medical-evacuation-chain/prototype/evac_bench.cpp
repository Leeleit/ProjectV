#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <random>
#include <memory>
#include <limits>
#include <fstream>
#include <string>

enum class NodeType { CCP, AP, FH, GH };
enum class PatientStatus { PENDING, IN_TRANSIT, STABILIZING, CURED, DEAD };

struct Patient {
    int id;
    double spawn_time;
    double current_hp;
    double bleed_out_rate;
    int triage_class;
    PatientStatus status;
    double stabilization_progress;
    int current_node;
    bool is_stabilized;
    double time_stabilized;
    double time_cured;
};

struct Edge {
    int target_id;
    double distance;
};

struct Node {
    int id;
    NodeType type;
    std::string name;
    int capacity;
    std::vector<int> current_patients;
    std::vector<int> queue;
};

struct Vehicle {
    int id;
    int capacity;
    double speed;
    int current_node;
    int target_node;
    double travel_time_remaining;
    std::vector<int> passengers;
    std::vector<int> route;
    bool is_empty;
};

struct Graph {
    std::vector<Node> nodes;
    std::unordered_map<int, std::vector<Edge>> adj;
};

struct SimulationScenario {
    std::string name;
    double spawn_interval;
    int num_vehicles;
};

struct RunResult {
    std::string strategy;
    std::string scenario;
    int seed;
    double survival_rate;
    double avg_stabilization_time;
    double avg_cure_time;
    double avg_routing_latency_ns;
};

std::vector<int> DijkstraShortestPath(const Graph& graph, int start, int target, const std::unordered_map<int, double>& custom_weights) {
    std::unordered_map<int, double> dists;
    std::unordered_map<int, int> parent;
    for (const auto& node : graph.nodes) {
        dists[node.id] = std::numeric_limits<double>::infinity();
    }
    
    using PQEntry = std::pair<double, int>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
    
    dists[start] = 0.0;
    pq.push({0.0, start});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (d > dists[u]) continue;
        if (u == target) break;
        
        auto it = graph.adj.find(u);
        if (it != graph.adj.end()) {
            for (const auto& edge : it->second) {
                double weight = edge.distance;
                int key = u * 1000 + edge.target_id;
                auto w_it = custom_weights.find(key);
                if (w_it != custom_weights.end()) {
                    weight = w_it->second;
                }
                if (dists[u] + weight < dists[edge.target_id]) {
                    dists[edge.target_id] = dists[u] + weight;
                    parent[edge.target_id] = u;
                    pq.push({dists[edge.target_id], edge.target_id});
                }
            }
        }
    }
    
    std::vector<int> path;
    if (dists[target] == std::numeric_limits<double>::infinity()) return path;
    int curr = target;
    while (curr != start) {
        path.push_back(curr);
        curr = parent[curr];
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

Graph BuildScenarioGraph() {
    Graph g;
    for (int i = 1; i <= 10; ++i) {
        g.nodes.push_back({i, NodeType::CCP, "CCP_" + std::to_string(i), 0, {}, {}});
    }
    g.nodes.push_back({11, NodeType::AP, "AP_1", 2, {}, {}});
    g.nodes.push_back({12, NodeType::AP, "AP_2", 2, {}, {}});
    g.nodes.push_back({13, NodeType::AP, "AP_3", 2, {}, {}});
    g.nodes.push_back({14, NodeType::AP, "AP_4", 2, {}, {}});
    g.nodes.push_back({15, NodeType::FH, "FH_1", 5, {}, {}});
    g.nodes.push_back({16, NodeType::FH, "FH_2", 5, {}, {}});
    g.nodes.push_back({17, NodeType::GH, "GH_1", 20, {}, {}});

    auto add_edge = [&](int u, int v, double dist) {
        g.adj[u].push_back({v, dist});
        g.adj[v].push_back({u, dist});
    };

    add_edge(1, 11, 3000.0);
    add_edge(2, 11, 4000.0);
    add_edge(3, 11, 5000.0);
    add_edge(4, 12, 3500.0);
    add_edge(5, 12, 4500.0);
    add_edge(6, 13, 3000.0);
    add_edge(7, 13, 4000.0);
    add_edge(8, 14, 3200.0);
    add_edge(9, 14, 3800.0);
    add_edge(10, 14, 4800.0);

    add_edge(11, 15, 12000.0);
    add_edge(12, 15, 15000.0);
    add_edge(13, 16, 13000.0);
    add_edge(14, 16, 14000.0);

    add_edge(15, 17, 40000.0);
    add_edge(16, 17, 45000.0);

    return g;
}

std::vector<Patient> GeneratePatients(int seed, double duration, double interval) {
    std::mt19937 gen(seed);
    std::exponential_distribution<double> arrival_dist(1.0 / interval);
    std::uniform_int_distribution<int> ccp_dist(1, 10);
    std::uniform_real_distribution<double> triage_roll(0.0, 1.0);

    std::vector<Patient> list;
    double t = 0.0;
    int p_id = 0;

    while (t < duration) {
        t += arrival_dist(gen);
        if (t >= duration) break;

        int ccp = ccp_dist(gen);
        double roll = triage_roll(gen);

        int triage_class = 0;
        double bleed_rate = 0.05;
        if (roll > 0.85) {
            triage_class = 2;
            bleed_rate = 0.8;
        } else if (roll > 0.50) {
            triage_class = 1;
            bleed_rate = 0.2;
        }

        list.push_back({
            p_id++,
            t,
            100.0,
            bleed_rate,
            triage_class,
            PatientStatus::PENDING,
            0.0,
            ccp,
            false,
            -1.0,
            -1.0
        });
    }

    return list;
}

int FindBestFacility(const Graph& graph, int start_node, int p_triage, const std::string& strategy, const std::unordered_map<int, double>& custom_weights, double& route_dist) {
    int best_node = -1;
    double min_cost = std::numeric_limits<double>::infinity();

    std::vector<int> targets;
    if (start_node >= 11 && start_node <= 14) {
        targets = {15, 16, 17};
    } else if (start_node >= 15 && start_node <= 16) {
        targets = {17};
    } else {
        if (strategy == "E_HubSpoke_Heuristic") {
            if (p_triage == 2) {
                targets = {11, 12, 13, 14};
            } else {
                targets = {15, 16, 17};
            }
        } else {
            targets = {11, 12, 13, 14, 15, 16, 17};
        }
    }

    for (int tgt : targets) {
        std::vector<int> path = DijkstraShortestPath(graph, start_node, tgt, custom_weights);
        if (path.empty()) continue;

        double d = 0.0;
        for (size_t i = 0; i < path.size() - 1; ++i) {
            int u = path[i];
            int v = path[i+1];
            double edge_len = 0.0;
            auto it = graph.adj.find(u);
            if (it != graph.adj.end()) {
                for (const auto& edge : it->second) {
                    if (edge.target_id == v) {
                        edge_len = edge.distance;
                        break;
                    }
                }
            }
            d += edge_len;
        }

        double cost = d;
        if (strategy == "B_QueueLengthBalanced" || strategy == "C_BleedOutUrgency") {
            const auto& node = *std::find_if(graph.nodes.begin(), graph.nodes.end(), [tgt](const Node& n) { return n.id == tgt; });
            double est_wait = node.queue.size() * (node.type == NodeType::AP ? 20.0 : (node.type == NodeType::FH ? 25.0 : 10.0));
            cost += est_wait * 15.0;
        } else if (strategy == "D_DynamicRouting_Dijkstra") {
            const auto& node = *std::find_if(graph.nodes.begin(), graph.nodes.end(), [tgt](const Node& n) { return n.id == tgt; });
            double est_wait = node.queue.size() * (node.type == NodeType::AP ? 20.0 : (node.type == NodeType::FH ? 25.0 : 10.0));
            cost += est_wait * 15.0;
        }

        if (cost < min_cost) {
            min_cost = cost;
            best_node = tgt;
            route_dist = d;
        }
    }

    return best_node;
}

RunResult RunSimulation(const std::string& strategy, const SimulationScenario& scenario, int seed) {
    double duration = 14400.0;
    Graph graph = BuildScenarioGraph();
    std::vector<Patient> patients = GeneratePatients(seed, duration, scenario.spawn_interval);

    std::vector<Vehicle> vehicles;
    for (int i = 0; i < scenario.num_vehicles; ++i) {
        vehicles.push_back({i, 2, 15.0, 17, 17, 0.0, {}, {}, true});
    }

    std::unordered_map<int, double> edge_occupancy;
    std::unordered_map<int, double> custom_weights;

    double current_time = 0.0;
    double dt = 1.0;

    int total_decision_calls = 0;
    double total_routing_time_ns = 0.0;

    while (current_time < duration) {
        for (auto& node : graph.nodes) {
            node.queue.clear();
        }
        for (const auto& p : patients) {
            if (p.status == PatientStatus::PENDING && p.spawn_time <= current_time) {
                auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const Node& n) { return n.id == p.current_node; });
                if (it != graph.nodes.end()) {
                    it->queue.push_back(p.id);
                }
            }
        }

        for (auto& node : graph.nodes) {
            if (node.type == NodeType::CCP) continue;

            auto p_it = node.current_patients.begin();
            while (p_it != node.current_patients.end()) {
                Patient& p = patients[*p_it];
                if (p.current_hp <= 0) {
                    p.status = PatientStatus::DEAD;
                    p.time_cured = -1.0;
                    p_it = node.current_patients.erase(p_it);
                } else {
                    if (node.type == NodeType::AP) {
                        p.stabilization_progress += 0.05 * dt;
                        if (p.stabilization_progress >= 1.0) {
                            p.is_stabilized = true;
                            p.bleed_out_rate = 0.01;
                            p.status = PatientStatus::PENDING;
                            p.current_node = node.id;
                            p.time_stabilized = current_time;
                            p_it = node.current_patients.erase(p_it);
                        } else {
                            ++p_it;
                        }
                    } else {
                        double heal_rate = (node.type == NodeType::FH) ? 2.0 : 5.0;
                        p.current_hp += heal_rate * dt;
                        if (p.current_hp >= 100.0) {
                            p.current_hp = 100.0;
                            p.status = PatientStatus::CURED;
                            p.time_cured = current_time;
                            p_it = node.current_patients.erase(p_it);
                        } else {
                            ++p_it;
                        }
                    }
                }
            }

            while (static_cast<int>(node.current_patients.size()) < node.capacity && !node.queue.empty()) {
                if (strategy == "C_BleedOutUrgency") {
                    std::sort(node.queue.begin(), node.queue.end(), [&](int a, int b) {
                        return (patients[a].current_hp / patients[a].bleed_out_rate) < (patients[b].current_hp / patients[b].bleed_out_rate);
                    });
                }
                int next_p = node.queue.front();
                node.queue.erase(node.queue.begin());
                patients[next_p].status = PatientStatus::STABILIZING;
                node.current_patients.push_back(next_p);
            }
        }

        edge_occupancy.clear();
        for (const auto& v : vehicles) {
            if (v.travel_time_remaining > 0 && v.current_node != v.target_node) {
                int key = std::min(v.current_node, v.target_node) * 1000 + std::max(v.current_node, v.target_node);
                edge_occupancy[key] += 1.0;
            }
        }

        custom_weights.clear();
        if (strategy == "D_DynamicRouting_Dijkstra") {
            for (const auto& pair : graph.adj) {
                int u = pair.first;
                for (const auto& edge : pair.second) {
                    int v = edge.target_id;
                    int key = std::min(u, v) * 1000 + std::max(u, v);
                    double traffic = edge_occupancy[key];
                    double weight = edge.distance * (1.0 + traffic * 0.5);
                    custom_weights[u * 1000 + v] = weight;
                }
            }
        }

        for (auto& v : vehicles) {
            if (v.travel_time_remaining > 0) {
                v.travel_time_remaining -= dt;
                
                auto p_it = v.passengers.begin();
                while (p_it != v.passengers.end()) {
                    Patient& p = patients[*p_it];
                    p.current_hp -= p.bleed_out_rate * dt;
                    if (p.current_hp <= 0) {
                        p.current_hp = 0;
                        p.status = PatientStatus::DEAD;
                        p.time_cured = -1.0;
                        p_it = v.passengers.erase(p_it);
                    } else {
                        ++p_it;
                    }
                }

                if (v.travel_time_remaining <= 0) {
                    v.current_node = v.target_node;
                    if (!v.route.empty()) {
                        v.route.erase(v.route.begin());
                    }
                }
            } else {
                if (v.is_empty) {
                    int best_pickup_node = -1;
                    double min_dist = std::numeric_limits<double>::infinity();

                    for (const auto& node : graph.nodes) {
                        if (node.queue.empty()) continue;

                        bool is_ccp = (node.type == NodeType::CCP);
                        bool is_ap = (node.type == NodeType::AP);

                        if (!is_ccp && !is_ap) continue;

                        std::vector<int> path = DijkstraShortestPath(graph, v.current_node, node.id, custom_weights);
                        if (path.empty()) continue;

                        double d = 0.0;
                        for (size_t idx = 0; idx < path.size() - 1; ++idx) {
                            int next_u = path[idx];
                            int next_v = path[idx+1];
                            auto it = graph.adj.find(next_u);
                            if (it != graph.adj.end()) {
                                for (const auto& edge : it->second) {
                                    if (edge.target_id == next_v) {
                                        d += edge.distance;
                                        break;
                                    }
                                }
                            }
                        }

                        double cost = d;
                        if (strategy == "C_BleedOutUrgency") {
                            bool has_critical = false;
                            for (int p_id : node.queue) {
                                if (patients[p_id].triage_class == 2) {
                                    has_critical = true;
                                    break;
                                }
                            }
                            if (has_critical) cost -= 5000.0;
                        }

                        if (cost < min_dist) {
                            min_dist = cost;
                            best_pickup_node = node.id;
                        }
                    }

                    if (best_pickup_node != -1) {
                        v.route = DijkstraShortestPath(graph, v.current_node, best_pickup_node, custom_weights);
                        if (!v.route.empty()) {
                            v.route.erase(v.route.begin());
                        }
                        v.is_empty = false;
                        if (!v.route.empty()) {
                            v.target_node = v.route[0];
                            double edge_len = 0.0;
                            auto it = graph.adj.find(v.current_node);
                            if (it != graph.adj.end()) {
                                for (const auto& edge : it->second) {
                                    if (edge.target_id == v.target_node) {
                                        edge_len = edge.distance;
                                        break;
                                    }
                                }
                            }
                            v.travel_time_remaining = edge_len / v.speed;
                        } else {
                            auto& p_node = *std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const Node& n) { return n.id == best_pickup_node; });
                            if (strategy == "C_BleedOutUrgency") {
                                std::sort(p_node.queue.begin(), p_node.queue.end(), [&](int a, int b) {
                                    return (patients[a].current_hp / patients[a].bleed_out_rate) < (patients[b].current_hp / patients[b].bleed_out_rate);
                                });
                            }
                            while (static_cast<int>(v.passengers.size()) < v.capacity && !p_node.queue.empty()) {
                                int p_id = p_node.queue.front();
                                p_node.queue.erase(p_node.queue.begin());
                                patients[p_id].status = PatientStatus::IN_TRANSIT;
                                patients[p_id].current_node = -1;
                                v.passengers.push_back(p_id);
                            }

                            if (!v.passengers.empty()) {
                                auto start_t = std::chrono::high_resolution_clock::now();
                                double route_d = 0.0;
                                int first_p = v.passengers.front();
                                int dest = FindBestFacility(graph, v.current_node, patients[first_p].triage_class, strategy, custom_weights, route_d);
                                auto end_t = std::chrono::high_resolution_clock::now();
                                total_routing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                                total_decision_calls++;

                                if (dest != -1) {
                                    v.route = DijkstraShortestPath(graph, v.current_node, dest, custom_weights);
                                    if (!v.route.empty()) {
                                        v.route.erase(v.route.begin());
                                    }
                                    if (!v.route.empty()) {
                                        v.target_node = v.route[0];
                                        double edge_len = 0.0;
                                        auto it = graph.adj.find(v.current_node);
                                        if (it != graph.adj.end()) {
                                            for (const auto& edge : it->second) {
                                                if (edge.target_id == v.target_node) {
                                                    edge_len = edge.distance;
                                                    break;
                                                }
                                            }
                                        }
                                        v.travel_time_remaining = edge_len / v.speed;
                                    }
                                } else {
                                    v.is_empty = true;
                                }
                            } else {
                                v.is_empty = true;
                            }
                        }
                    }
                } else {
                    if (v.passengers.empty()) {
                        v.is_empty = true;
                    } else {
                        auto& dest_node = *std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const Node& n) { return n.id == v.current_node; });
                        if (dest_node.type != NodeType::CCP) {
                            for (int p_id : v.passengers) {
                                Patient& p = patients[p_id];
                                if (p.status == PatientStatus::IN_TRANSIT) {
                                    p.status = PatientStatus::PENDING;
                                    p.current_node = v.current_node;
                                    dest_node.queue.push_back(p_id);
                                }
                            }
                            v.passengers.clear();
                            v.is_empty = true;
                        } else {
                            if (!v.route.empty()) {
                                v.target_node = v.route[0];
                                double edge_len = 0.0;
                                auto it = graph.adj.find(v.current_node);
                                if (it != graph.adj.end()) {
                                    for (const auto& edge : it->second) {
                                        if (edge.target_id == v.target_node) {
                                            edge_len = edge.distance;
                                            break;
                                        }
                                    }
                                }
                                v.travel_time_remaining = edge_len / v.speed;
                            } else {
                                v.is_empty = true;
                            }
                        }
                    }
                }
            }
        }

        current_time += dt;
    }

    int total_patients = patients.size();
    int cured_count = 0;
    double sum_stab_time = 0.0;
    double sum_cure_time = 0.0;
    int stab_count = 0;

    for (const auto& p : patients) {
        if (p.status == PatientStatus::CURED) {
            cured_count++;
            sum_cure_time += (p.time_cured - p.spawn_time);
        }
        if (p.is_stabilized) {
            stab_count++;
            sum_stab_time += (p.time_stabilized - p.spawn_time);
        }
    }

    double survival_rate = (total_patients > 0) ? (100.0 * cured_count / total_patients) : 100.0;
    double avg_stab = (stab_count > 0) ? (sum_stab_time / stab_count) : 0.0;
    double avg_cure = (cured_count > 0) ? (sum_cure_time / cured_count) : 0.0;
    double avg_lat = (total_decision_calls > 0) ? (total_routing_time_ns / total_decision_calls) : 0.0;

    return {
        strategy,
        scenario.name,
        seed,
        survival_rate,
        avg_stab,
        avg_cure,
        avg_lat
    };
}

int main() {
    std::vector<std::string> strategies = {
        "A_NearestFirst",
        "B_QueueLengthBalanced",
        "C_BleedOutUrgency",
        "D_DynamicRouting_Dijkstra",
        "E_HubSpoke_Heuristic"
    };

    std::vector<SimulationScenario> scenarios = {
        {"low_intensity", 120.0, 4},
        {"medium_intensity", 60.0, 6},
        {"high_intensity", 20.0, 8},
        {"mass_casualty", 8.0, 10},
        {"extreme_surge", 4.0, 12}
    };

    std::vector<int> seeds = {101, 102, 103, 104, 105};

    std::ofstream out("results.csv");
    out << "Strategy,Scenario,Seed,SurvivalRate,AvgStabilizationTime,AvgCureTime,AvgRoutingLatencyNs\n";

    std::cout << "Starting Medical Evacuation Chain Benchmarks (125 configurations)..." << std::endl;

    for (const auto& strat : strategies) {
        for (const auto& scen : scenarios) {
            for (int seed : seeds) {
                RunResult r = RunSimulation(strat, scen, seed);
                out << r.strategy << ","
                    << r.scenario << ","
                    << r.seed << ","
                    << r.survival_rate << ","
                    << r.avg_stabilization_time << ","
                    << r.avg_cure_time << ","
                    << r.avg_routing_latency_ns << "\n";
            }
        }
        std::cout << "Finished strategy: " << strat << std::endl;
    }

    out.close();
    std::cout << "Benchmarks completed. Results written to results.csv" << std::endl;
    return 0;
}
