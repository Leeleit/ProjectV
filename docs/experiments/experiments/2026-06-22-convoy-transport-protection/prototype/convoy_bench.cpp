#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <numbers>
#include <numeric>
#include <optional>
#include <print>
#include <random>
#include <ranges>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int kMapSize    = 64;
static constexpr int kMaxUnits   = 16;
static constexpr int kWarmup     = 20;
static constexpr int kIterations = 200;

// ---------------------------------------------------------------------------
// Vec2
// ---------------------------------------------------------------------------
struct Vec2 { int x{}, y{}; };
Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
int manhattan(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
int length2(Vec2 v) { return v.x * v.x + v.y * v.y; }

// ---------------------------------------------------------------------------
// RNG
// ---------------------------------------------------------------------------
struct Rng {
  std::mt19937 gen{std::random_device{}()};
  int uniform(int lo, int hi) { return std::uniform_int_distribution{lo, hi}(gen); }
  float uniform_f() { return std::uniform_real_distribution<float>{}(gen); }
  bool chance(float p) { return uniform_f() < p; }
};

// ---------------------------------------------------------------------------
// Terrain types
// ---------------------------------------------------------------------------
enum class Terrain : uint8_t { Open, Road, Urban, Mountain, Chokepoint };
static constexpr int kTerrainCost[] = {3, 1, 2, 8, 6};
static constexpr float kTerrainConceal[] = {0.2f, 0.0f, 0.7f, 0.8f, 0.5f};

struct Tile {
  Terrain t{Terrain::Open};
  int threat_id{-1};
};

// ---------------------------------------------------------------------------
// Threat
// ---------------------------------------------------------------------------
struct ThreatDef {
  Vec2 pos;
  int radius;
  float engage_prob;
  int cooldown_ticks;
  int strength;
};

struct Threat {
  ThreatDef def;
  int cooldown{0};
  int engagements{0};
};

// ---------------------------------------------------------------------------
// Unit
// ---------------------------------------------------------------------------
enum class UnitType : uint8_t { SupplyTruck, Escort, Scout };
struct Unit {
  UnitType type;
  Vec2 pos;
  int hp{10};
  bool alive{true};
  bool reached{false};
};

// ---------------------------------------------------------------------------
// GridMap
// ---------------------------------------------------------------------------
class GridMap {
  std::array<std::array<Tile, kMapSize>, kMapSize> tiles_;
public:
  Tile& at(Vec2 v) { return tiles_[v.y][v.x]; }
  const Tile& at(Vec2 v) const { return tiles_[v.y][v.x]; }
  bool in_bounds(Vec2 v) const {
    return v.x >= 0 && v.x < kMapSize && v.y >= 0 && v.y < kMapSize;
  }

  void set_terrain(Vec2 v, Terrain t) { at(v).t = t; }
  Terrain terrain(Vec2 v) const { return at(v).t; }

  static constexpr std::array<Vec2, 4> kDirs = {Vec2{1,0}, Vec2{-1,0}, Vec2{0,1}, Vec2{0,-1}};
  static constexpr std::array<Vec2, 8> kDirs8 = {
    Vec2{1,0}, Vec2{-1,0}, Vec2{0,1}, Vec2{0,-1},
    Vec2{1,1}, Vec2{1,-1}, Vec2{-1,1}, Vec2{-1,-1}
  };

  std::vector<Vec2> dijkstra(Vec2 src, Vec2 dst,
                              const std::vector<Vec2>& avoid = {}) const {
    struct Node { int dist; Vec2 parent; bool visited; };
    std::array<std::array<Node, kMapSize>, kMapSize> state{};
    for (auto& row : state)
      for (auto& n : row) n.dist = std::numeric_limits<int>::max();

    state[src.y][src.x].dist = 0;
    std::vector<Vec2> frontier{src};

    while (!frontier.empty()) {
      auto it = std::min_element(frontier.begin(), frontier.end(),
        [&](Vec2 a, Vec2 b) { return state[a.y][a.x].dist < state[b.y][b.x].dist; });
      Vec2 cur = *it;
      frontier.erase(it);
      auto& cn = state[cur.y][cur.x];
      if (cn.visited) continue;
      cn.visited = true;
      if (cur == dst) break;

      for (Vec2 d : kDirs) {
        Vec2 n = cur + d;
        if (!in_bounds(n)) continue;
        auto& nn = state[n.y][n.x];
        if (nn.visited) continue;
        if (std::ranges::find(avoid, n) != avoid.end()) continue;
        int terrain_cost = kTerrainCost[static_cast<int>(terrain(n))];
        int nd = cn.dist + terrain_cost;
        if (nd < nn.dist) {
          nn.dist = nd;
          nn.parent = cur;
          frontier.push_back(n);
        }
      }
    }

    std::vector<Vec2> path;
    Vec2 cur = dst;
    while (!(cur == src)) {
      path.push_back(cur);
      auto& n = state[cur.y][cur.x];
      if (n.dist == std::numeric_limits<int>::max()) return {};
      cur = n.parent;
    }
    std::ranges::reverse(path);
    return path;
  }

  std::vector<Vec2> find_threats_in_radius(Vec2 pos, int radius,
                                           const std::vector<Threat>& threats) const {
    std::vector<Vec2> result;
    for (const auto& t : threats)
      if (t.def.cooldown_ticks == 0 || t.cooldown == 0)
        if (manhattan(pos, t.def.pos) <= radius)
          result.push_back(t.def.pos);
    return result;
  }
};

// ---------------------------------------------------------------------------
// Scene definitions
// ---------------------------------------------------------------------------
struct Scene {
  std::string name;
  std::string description;
  Vec2 start, end;
  int escort_count;
  std::vector<ThreatDef> threats;
  GridMap map;

  void build(GridMap& out) const {
    out = map;
  }
};

static std::vector<Scene> make_scenes() {
  auto road_line = [&](GridMap& m, int y, int x0, int x1) {
    for (int x = x0; x <= x1; ++x) m.set_terrain({x, y}, Terrain::Road);
  };
  auto road_col = [&](GridMap& m, int x, int y0, int y1) {
    for (int y = y0; y <= y1; ++y) m.set_terrain({x, y}, Terrain::Road);
  };
  auto urban = [&](GridMap& m, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
      Vec2 p{cx+dx, cy+dy};
      if (m.in_bounds(p)) m.set_terrain(p, Terrain::Urban);
    }
  };
  auto mountain = [&](GridMap& m, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
      Vec2 p{cx+dx, cy+dy};
      if (m.in_bounds(p)) m.set_terrain(p, Terrain::Mountain);
    }
  };
  auto chokepoint = [&](GridMap& m, int cx, int cy) {
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
      Vec2 p{cx+dx, cy+dy};
      if (m.in_bounds(p)) m.set_terrain(p, Terrain::Chokepoint);
    }
  };

  GridMap m1, m2, m3, m4, m5;

  // s1: Simple supply run (short route, few threats)
  road_line(m1, 8, 2, 30); road_line(m1, 16, 5, 20);
  urban(m1, 15, 8, 3);

  // s2: Highway ambush
  road_line(m2, 16, 0, 63);
  chokepoint(m2, 30, 16);
  mountain(m2, 28, 14, 4); mountain(m2, 28, 18, 4);

  // s3: Mountain pass
  mountain(m3, 32, 10, 8); mountain(m3, 32, 30, 8);
  road_line(m3, 20, 0, 63);
  chokepoint(m3, 32, 20); chokepoint(m3, 16, 20); chokepoint(m3, 48, 20);

  // s4: Urban logistics
  urban(m4, 32, 32, 10);
  urban(m4, 10, 10, 5); urban(m4, 54, 10, 5); urban(m4, 10, 54, 5); urban(m4, 54, 54, 5);
  road_line(m4, 32, 0, 63); road_col(m4, 32, 0, 63);
  road_line(m4, 16, 0, 63); road_line(m4, 48, 0, 63);
  road_col(m4, 16, 0, 63); road_col(m4, 48, 0, 63);

  // s5: Long haul supply (multi-zone)
  road_line(m5, 8, 0, 63); road_line(m5, 24, 0, 63);
  road_line(m5, 40, 0, 63); road_line(m5, 56, 0, 63);
  urban(m5, 16, 8, 4); mountain(m5, 32, 24, 6);
  urban(m5, 48, 40, 4); mountain(m5, 16, 56, 5);
  chokepoint(m5, 48, 8); chokepoint(m5, 16, 24);
  chokepoint(m5, 48, 40); chokepoint(m5, 32, 56);

  std::vector<Scene> scenes;
  scenes.push_back(Scene{
    "s1_simple_supply_run", "Short route, open terrain, 2 threats",
    {2, 8}, {30, 8}, 1,
    { {{15, 6}, 4, 0.3f, 10, 1}, {{12, 12}, 3, 0.4f, 8, 1} },
    std::move(m1)
  });
  scenes.push_back(Scene{
    "s2_highway_ambush", "Long highway through mountain chokepoint, 4 threats clustered",
    {2, 16}, {62, 16}, 2,
    { {{30, 12}, 3, 0.6f, 6, 2}, {{30, 20}, 3, 0.5f, 6, 2},
      {{10, 14}, 4, 0.2f, 12, 1}, {{50, 18}, 4, 0.3f, 10, 1} },
    std::move(m2)
  });
  scenes.push_back(Scene{
    "s3_mountain_pass", "Three chokepoints, high threat density, narrow passages",
    {2, 20}, {62, 20}, 3,
    { {{32, 16}, 2, 0.7f, 4, 3}, {{32, 24}, 2, 0.6f, 4, 3},
      {{16, 16}, 2, 0.5f, 6, 2}, {{16, 24}, 2, 0.5f, 6, 2},
      {{48, 16}, 2, 0.5f, 6, 2}, {{48, 24}, 2, 0.5f, 6, 2} },
    std::move(m3)
  });
  scenes.push_back(Scene{
    "s4_urban_logistics", "City grid with scattered threats, multiple route options",
    {2, 32}, {62, 32}, 2,
    { {{10, 10}, 5, 0.3f, 8, 1}, {{32, 20}, 3, 0.4f, 6, 1},
      {{54, 10}, 4, 0.3f, 10, 1}, {{20, 32}, 3, 0.5f, 5, 2},
      {{44, 32}, 3, 0.5f, 5, 2}, {{10, 54}, 4, 0.2f, 8, 1},
      {{54, 54}, 4, 0.2f, 8, 1}, {{32, 48}, 3, 0.4f, 6, 1} },
    std::move(m4)
  });
  scenes.push_back(Scene{
    "s5_long_haul_supply", "Cross-zone route with chokepoints, mountains, urban",
    {0, 8}, {63, 56}, 3,
    { {{16, 4}, 3, 0.4f, 6, 2}, {{48, 4}, 3, 0.4f, 6, 1},
      {{8, 24}, 4, 0.3f, 8, 1}, {{48, 24}, 3, 0.4f, 6, 2},
      {{16, 40}, 3, 0.5f, 5, 2}, {{32, 40}, 4, 0.3f, 8, 1},
      {{8, 56}, 2, 0.6f, 4, 3}, {{32, 60}, 3, 0.3f, 10, 1},
      {{56, 40}, 4, 0.3f, 8, 1}, {{56, 24}, 3, 0.4f, 6, 2} },
    std::move(m5)
  });
  return scenes;
}

// ---------------------------------------------------------------------------
// Strategy base
// ---------------------------------------------------------------------------
struct Metrics {
  int deliveries{0};
  int total_ticks{0};
  int total_casualties{0};
  int total_engagements{0};
  int total_threat_zone_entries{0};
  float survival_rate() const {
    return float(deliveries) / std::max(1, deliveries + (kIterations - deliveries));
  }
  float avg_ticks() const { return float(total_ticks) / std::max(1, deliveries); }
  float avg_casualties() const { return float(total_casualties) / kIterations; }
  float engagement_rate() const {
    return float(total_engagements) / std::max(1, total_threat_zone_entries);
  }
};

struct RunState {
  GridMap map;
  std::vector<Threat> threats;
  std::vector<Unit> units;
  Vec2 start, end;
  int tick{0};
  bool delivered{false};
  Metrics metrics;
  Rng rng;
};

struct Strategy {
  virtual const char* name() const = 0;
  virtual void tick(RunState& s) = 0;
  virtual void reset() {}
  virtual ~Strategy() = default;
};

// ---------------------------------------------------------------------------
// A: Naive Direct Route (baseline)
// ---------------------------------------------------------------------------
struct A_NaiveDirectRoute : Strategy {
  const char* name() const override { return "A_NaiveDirectRoute"; }
  std::vector<Vec2> path;
  int path_idx{0};
  void reset() override { path.clear(); path_idx = 0; }

  void tick(RunState& s) override {
    if (s.delivered) return;

    auto truck_it = std::ranges::find_if(s.units, [](auto& u) {
      return u.type == UnitType::SupplyTruck && u.alive;
    });
    if (truck_it == s.units.end()) { s.delivered = true; return; }
    Unit* truck = &*truck_it;

    if (path.empty())
      path = s.map.dijkstra(truck->pos, s.end);

    // Follow path ignoring threats
    if (path_idx < (int)path.size()) {
      truck->pos = path[path_idx++];

      // Check threats along the way
      Vec2 truck_pos = truck->pos;
      for (auto& t : s.threats) {
        if (t.cooldown > 0) { t.cooldown--; continue; }
        if (manhattan(truck_pos, t.def.pos) <= t.def.radius) {
          if (s.rng.chance(t.def.engage_prob)) {
            truck->hp -= t.def.strength;
            t.engagements++;
            t.cooldown = t.def.cooldown_ticks;
            s.metrics.total_engagements++;
          }
        }
      }
      if (truck->hp <= 0) { truck->alive = false; s.delivered = true; }
    }

    if (truck->pos == s.end) {
      truck->reached = true;
      s.delivered = true;
    }
  }
};

// ---------------------------------------------------------------------------
// B: Waypoint Road Preference
// ---------------------------------------------------------------------------
struct B_WaypointRoadPreference : Strategy {
  const char* name() const override { return "B_WaypointRoadPreference"; }
  std::vector<Vec2> path;
  int path_idx{0};
  void reset() override { path.clear(); path_idx = 0; }

  void tick(RunState& s) override {
    if (s.delivered) return;
    auto truck_it = std::ranges::find_if(s.units, [](auto& u) {
      return u.type == UnitType::SupplyTruck && u.alive;
    });
    if (truck_it == s.units.end()) { s.delivered = true; return; }
    Unit* truck = &*truck_it;

    if (path.empty()) {
      // Find road-aligned waypoints between start and end
      std::vector<Vec2> waypoints;
      // Scan for road tiles on the route corridor
      Vec2 d = {s.end.x - s.start.x, s.end.y - s.start.y};
      int steps = std::max(std::abs(d.x), std::abs(d.y)) / 8;
      if (steps < 1) steps = 1;
      for (int i = 1; i < steps; ++i) {
        float t = float(i) / steps;
        int wx = int(s.start.x + t * d.x);
        int wy = int(s.start.y + t * d.y);
        // Search for nearest road tile
        int best_dist = 9999; Vec2 best = {wx, wy};
        for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
          Vec2 p{wx+dx, wy+dy};
          if (s.map.in_bounds(p) && s.map.terrain(p) == Terrain::Road) {
            int dd = std::abs(dx) + std::abs(dy);
            if (dd < best_dist) { best_dist = dd; best = p; }
          }
        }
        waypoints.push_back(best);
      }
      waypoints.push_back(s.end);

      // Build full path through waypoints
      Vec2 cur = truck->pos;
      for (auto& wp : waypoints) {
        auto seg = s.map.dijkstra(cur, wp);
        path.insert(path.end(), seg.begin(), seg.end());
        cur = wp;
      }
    }

    if (path_idx < (int)path.size()) {
      truck->pos = path[path_idx++];
      // Same threat check as A
      for (auto& t : s.threats) {
        if (t.cooldown > 0) { t.cooldown--; continue; }
        if (manhattan(truck->pos, t.def.pos) <= t.def.radius) {
          if (s.rng.chance(t.def.engage_prob)) {
            truck->hp -= t.def.strength;
            t.engagements++;
            t.cooldown = t.def.cooldown_ticks;
            s.metrics.total_engagements++;
          }
        }
      }
      if (truck->hp <= 0) { truck->alive = false; s.delivered = true; }
    }

    if (truck->pos == s.end) { truck->reached = true; s.delivered = true; }
  }
};

// ---------------------------------------------------------------------------
// C: Dynamic Threat Avoidance
// ---------------------------------------------------------------------------
struct C_DynamicThreatAvoidance : Strategy {
  const char* name() const override { return "C_DynamicThreatAvoidance"; }
  std::vector<Vec2> path;
  int path_idx{0};
  int replan_timer{0};
  static constexpr int kReplanInterval = 5;

  void tick(RunState& s) override {
    if (s.delivered) return;
    auto truck_it = std::ranges::find_if(s.units, [](auto& u) {
      return u.type == UnitType::SupplyTruck && u.alive;
    });
    if (truck_it == s.units.end()) { s.delivered = true; return; }
    Unit* truck = &*truck_it;

    int threat_range = 6;
    auto nearby = s.map.find_threats_in_radius(truck->pos, threat_range, s.threats);

    // Replan if threat nearby or periodic
    if (!nearby.empty() || replan_timer <= 0 || path.empty() || path_idx >= (int)path.size()) {
      std::vector<Vec2> avoid;
      for (auto& tp : s.map.find_threats_in_radius(truck->pos, threat_range, s.threats))
        avoid.push_back(tp);
      path = s.map.dijkstra(truck->pos, s.end, avoid);
      path_idx = 0;
      replan_timer = kReplanInterval;
    }
    replan_timer--;

    if (path_idx < (int)path.size()) {
      truck->pos = path[path_idx++];
      for (auto& t : s.threats) {
        if (t.cooldown > 0) { t.cooldown--; continue; }
        if (manhattan(truck->pos, t.def.pos) <= t.def.radius) {
          if (s.rng.chance(t.def.engage_prob)) {
            truck->hp -= t.def.strength;
            t.engagements++;
            t.cooldown = t.def.cooldown_ticks;
            s.metrics.total_engagements++;
          }
        }
      }
      if (truck->hp <= 0) { truck->alive = false; s.delivered = true; }
    }

    if (truck->pos == s.end) { truck->reached = true; s.delivered = true; }
  }
};

// ---------------------------------------------------------------------------
// D: Escort Formation AI
// ---------------------------------------------------------------------------
struct D_EscortFormationAI : Strategy {
  const char* name() const override { return "D_EscortFormationAI"; }
  std::vector<Vec2> path;
  int path_idx{0};
  void reset() override { path.clear(); path_idx = 0; }

  void tick(RunState& s) override {
    if (s.delivered) return;
    auto truck_it = std::ranges::find_if(s.units, [](auto& u) {
      return u.type == UnitType::SupplyTruck && u.alive;
    });
    if (truck_it == s.units.end()) { s.delivered = true; return; }
    Unit* truck = &*truck_it;

    // Build path for truck
    if (path.empty())
      path = s.map.dijkstra(truck->pos, s.end);

    // Move truck
    if (path_idx < (int)path.size()) {
      truck->pos = path[path_idx++];
    }

    // Move escorts: maintain formation 2 tiles behind, 2 tiles flank
    std::vector<Unit*> escorts;
    for (auto& u : s.units)
      if (u.type == UnitType::Escort && u.alive)
        escorts.push_back(&u);

    static constexpr Vec2 kFormation[] = {{-2, -2}, {2, -2}, {-2, 2}, {2, 2}};
    Vec2 truck_pos = truck->pos;

    for (size_t i = 0; i < escorts.size() && i < 4; ++i) {
      Vec2 target = truck_pos + kFormation[i];

      // If threat nearby, escort intercepts
      bool intercepted = false;
      for (auto& t : s.threats) {
        if (t.cooldown > 0) continue;
        if (manhattan(escorts[i]->pos, t.def.pos) <= t.def.radius) {
          if (s.rng.chance(t.def.engage_prob)) {
            escorts[i]->hp -= t.def.strength;
            t.engagements++;
            t.cooldown = t.def.cooldown_ticks;
            s.metrics.total_engagements++;
            if (escorts[i]->hp <= 0) escorts[i]->alive = false;
            intercepted = true;
          }
        }
      }

      if (!intercepted && escorts[i]->alive) {
        // Move toward formation position
        auto escort_path = s.map.dijkstra(escorts[i]->pos, target);
        if (!escort_path.empty())
          escorts[i]->pos = escort_path[0];
      }
    }

    // Truck still checks threats (escorts may miss some)
    for (auto& t : s.threats) {
      if (t.cooldown > 0) { t.cooldown--; continue; }
      if (manhattan(truck->pos, t.def.pos) <= t.def.radius) {
        // Reduced engage prob due to escort presence
        float reduced_prob = t.def.engage_prob * 0.4f;
        if (s.rng.chance(reduced_prob)) {
          truck->hp -= t.def.strength;
          t.engagements++;
          t.cooldown = t.def.cooldown_ticks;
          s.metrics.total_engagements++;
        }
      }
    }

    if (truck->hp <= 0) { truck->alive = false; s.delivered = true; }
    if (truck->pos == s.end) { truck->reached = true; s.delivered = true; }
  }
};

// ---------------------------------------------------------------------------
// E: Hybrid Dynamic Convoy (threat-aware + escort + ambush response)
// ---------------------------------------------------------------------------
struct E_HybridDynamicConvoy : Strategy {
  const char* name() const override { return "E_HybridDynamicConvoy"; }
  std::vector<Vec2> path;
  int path_idx{0};
  int replan_timer{0};
  bool ambush_evasion_mode{false};
  int ambush_timer{0};
  void reset() override {
    path.clear(); path_idx = 0; replan_timer = 0;
    ambush_evasion_mode = false; ambush_timer = 0;
  }
  static constexpr int kReplanInterval = 4;
  static constexpr int kAmbushEvadeTicks = 6;

  void tick(RunState& s) override {
    if (s.delivered) return;
    auto truck_it = std::ranges::find_if(s.units, [](auto& u) {
      return u.type == UnitType::SupplyTruck && u.alive;
    });
    if (truck_it == s.units.end()) { s.delivered = true; return; }
    Unit* truck = &*truck_it;

    std::vector<Unit*> escorts;
    for (auto& u : s.units)
      if (u.type == UnitType::Escort && u.alive)
        escorts.push_back(&u);

    // Detect ambush: multiple threats in radius
    int threat_count = 0;
    Vec2 nearest_threat{0,0};
    int nearest_dist = 9999;
    for (auto& t : s.threats) {
      if (t.cooldown > 0) continue;
      int d = manhattan(truck->pos, t.def.pos);
      if (d <= t.def.radius) {
        threat_count++;
        if (d < nearest_dist) { nearest_dist = d; nearest_threat = t.def.pos; }
      }
      // Also check escorts
      for (auto* e : escorts) {
        if (manhattan(e->pos, t.def.pos) <= t.def.radius)
          threat_count++;
      }
    }

    if (threat_count >= 2 && !ambush_evasion_mode) {
      ambush_evasion_mode = true;
      ambush_timer = kAmbushEvadeTicks;
    }
    if (ambush_timer > 0) ambush_timer--;
    if (ambush_timer == 0) ambush_evasion_mode = false;

    // Replan path
    if (path.empty() || path_idx >= (int)path.size() || replan_timer <= 0) {
      std::vector<Vec2> avoid;
      if (ambush_evasion_mode) {
        // Avoid known threat zones
        for (auto& t : s.threats) {
          if (t.cooldown == 0) {
            for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
              Vec2 p{t.def.pos.x+dx, t.def.pos.y+dy};
              if (s.map.in_bounds(p)) avoid.push_back(p);
            }
          }
        }
      } else {
        auto nearby = s.map.find_threats_in_radius(truck->pos, 7, s.threats);
        for (auto& tp : nearby) avoid.push_back(tp);
      }
      path = s.map.dijkstra(truck->pos, s.end, avoid);
      path_idx = 0;
      replan_timer = kReplanInterval;
    }
    replan_timer--;

    // Move truck
    if (path_idx < (int)path.size())
      truck->pos = path[path_idx++];

    // Move escorts with formation + intercept
    static constexpr Vec2 kFormation[] = {{-2, -2}, {2, -2}, {-2, 2}, {2, 2}};
    Vec2 truck_pos = truck->pos;

    for (size_t i = 0; i < escorts.size() && i < 4; ++i) {
      if (!escorts[i]->alive) continue;
      Vec2 target = ambush_evasion_mode ?
        truck_pos + Vec2{kFormation[i].x * 1, kFormation[i].y * 1} :
        truck_pos + kFormation[i];

      // Escorts prioritize threat interception
      Threat* nearest_enemy = nullptr;
      int nd = 9999;
      for (auto& t : s.threats) {
        if (t.cooldown > 0) continue;
        int d = manhattan(escorts[i]->pos, t.def.pos);
        if (d < nd) { nd = d; nearest_enemy = &t; }
      }
      if (nearest_enemy && nd <= 5 && ambush_evasion_mode) {
        // Intercept nearest threat
        target = nearest_enemy->def.pos;
      }

      // Check threat engagement on escort
      for (auto& t : s.threats) {
        if (t.cooldown > 0) continue;
        if (manhattan(escorts[i]->pos, t.def.pos) <= t.def.radius) {
          if (s.rng.chance(t.def.engage_prob)) {
            escorts[i]->hp -= t.def.strength;
            t.engagements++;
            t.cooldown = t.def.cooldown_ticks;
            s.metrics.total_engagements++;
            if (escorts[i]->hp <= 0) escorts[i]->alive = false;
          }
        }
      }

      if (escorts[i]->alive) {
        auto ep = s.map.dijkstra(escorts[i]->pos, target);
        if (!ep.empty()) escorts[i]->pos = ep[0];
      }
    }

    // Truck threat check with escort reduction
    for (auto& t : s.threats) {
      if (t.cooldown > 0) { t.cooldown--; continue; }
      if (manhattan(truck->pos, t.def.pos) <= t.def.radius) {
        float red = std::max(0.1f, 1.0f - 0.3f * escorts.size());
        if (s.rng.chance(t.def.engage_prob * red)) {
          truck->hp -= t.def.strength;
          t.engagements++;
          t.cooldown = t.def.cooldown_ticks;
          s.metrics.total_engagements++;
        }
      }
    }

    if (truck->hp <= 0) { truck->alive = false; s.delivered = true; }
    if (truck->pos == s.end) { truck->reached = true; s.delivered = true; }
  }
};

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------
struct BenchmarkHarness {
  Rng rng;

  RunState make_state(const Scene& scene) {
    RunState s;
    s.map = GridMap{};
    s.start = scene.start; s.end = scene.end;
    for (auto& td : scene.threats)
      s.threats.push_back({td, 0, 0});
    // Build terrain
    scene.build(s.map);
    // Spawn units
    s.units.push_back({UnitType::SupplyTruck, scene.start, 10, true, false});
    for (int i = 0; i < scene.escort_count; ++i)
      s.units.push_back({UnitType::Escort, scene.start, 8, true, false});
    return s;
  }

  Metrics run(Strategy& strat, const Scene& scene) {
    Metrics total;
    strat.reset();

    for (int iter = 0; iter < kIterations; ++iter) {
      auto state = make_state(scene);
      // Re-seed for variety
      state.rng = Rng{};

      while (!state.delivered && state.tick < 500) {
        strat.tick(state);
        state.tick++;
      }

      total.deliveries += state.delivered && state.units[0].alive ? 1 : 0;
      total.total_ticks += state.tick;
      int casualties = 0;
      for (auto& u : state.units)
        if (!u.alive) casualties++;
      total.total_casualties += casualties;
      total.total_engagements += state.metrics.total_engagements;
      total.total_threat_zone_entries += state.threats.size();
    }
    return total;
  }
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  std::println("=== Convoy Transport Protection Benchmark ===");
  std::println("Warmup: {} iterations | Measure: {} iterations per cell\n",
               kWarmup, kIterations);

  auto scenes = make_scenes();
  std::vector<std::unique_ptr<Strategy>> strategies;
  strategies.push_back(std::make_unique<A_NaiveDirectRoute>());
  strategies.push_back(std::make_unique<B_WaypointRoadPreference>());
  strategies.push_back(std::make_unique<C_DynamicThreatAvoidance>());
  strategies.push_back(std::make_unique<D_EscortFormationAI>());
  strategies.push_back(std::make_unique<E_HybridDynamicConvoy>());

  // CSV output
  std::ofstream csv("convoy_results.csv");
  csv << "strategy,scene,survival_rate,avg_ticks,avg_casualties,"
      << "engagement_rate,deliveries\n";

  // Table header
  std::println("{:<24} {:<22} {:>8} {:>8} {:>10} {:>14}",
               "Strategy", "Scene", "Surv%", "AvgT", "AvgCas", "EngRate");

  std::println("{:-^80}", "");

  BenchmarkHarness harness;

  for (auto& strat : strategies) {
    // Warmup
    for (int w = 0; w < kWarmup; ++w) {
      auto& scene = scenes[w % scenes.size()];
      auto state = harness.make_state(scene);
      while (!state.delivered && state.tick < 500) {
        strat->tick(state);
        state.tick++;
      }
    }

    // Measure
    for (auto& scene : scenes) {
      auto m = harness.run(*strat, scene);

      float sr = m.survival_rate();
      float at = m.avg_ticks();
      float ac = m.avg_casualties();
      float er = m.engagement_rate();

      std::println("{:<24} {:<22} {:>7.1f}% {:>8.1f} {:>10.2f} {:>13.2f}%",
                   strat->name(), scene.name, sr * 100.0f, at, ac, er * 100.0f);

      csv << std::format("{},{},{:.4f},{:.2f},{:.2f},{:.4f},{}\n",
                         strat->name(), scene.name, sr, at, ac, er, m.deliveries);
    }
    std::println("");
  }

  // Summary
  std::println("{:=^80}", "");
  std::println("Summary (averaged across all scenes):\n");

  csv << "\nsummary\n";
  for (auto& strat : strategies) {
    float avg_sr = 0, avg_at = 0, avg_ac = 0, avg_er = 0;
    for (auto& scene : scenes) {
      auto state = harness.make_state(scene);
      while (!state.delivered && state.tick < 500) {
        strat->tick(state);
        state.tick++;
      }
    }
    // Re-measure for summary
    int n = scenes.size();
    for (auto& scene : scenes) {
      auto m = harness.run(*strat, scene);
      avg_sr += m.survival_rate();
      avg_at += m.avg_ticks();
      avg_ac += m.avg_casualties();
      avg_er += m.engagement_rate();
    }
    avg_sr /= n; avg_at /= n; avg_ac /= n; avg_er /= n;
    std::println("{:<24} {:>7.1f}% {:>8.1f} {:>10.2f} {:>13.2f}%",
                 strat->name(), avg_sr * 100.0f, avg_at, avg_ac, avg_er * 100.0f);
    csv << std::format("{},average,{:.4f},{:.2f},{:.2f},{:.4f}\n",
                       strat->name(), avg_sr, avg_at, avg_ac, avg_er);
  }

  return 0;
}
