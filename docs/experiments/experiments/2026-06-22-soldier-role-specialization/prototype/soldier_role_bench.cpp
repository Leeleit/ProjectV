#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>
#include <limits>
#include <fstream>
#include <string>

enum class RoleType { Rifleman, Medic, Engineer, AT, MG, Sniper, Commander };

enum class SceneType {
    UniformCombat,
    SpecializedSquads,
    RoleSwappingFrenzy,
    CommandSupport,
    MassCasualty
};

struct SceneConfig {
    std::string name;
    int num_entities;
    double rifleman_pct;
    double medic_pct;
    double engineer_pct;
    double at_pct;
    double mg_pct;
    double sniper_pct;
    double commander_pct;
    double swap_pct;
    int checks_per_tick;
};

const SceneConfig SCENES[] = {
    {"s1_uniform_combat", 10000, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10},
    {"s2_specialized_squads", 10000, 0.60, 0.15, 0.10, 0.05, 0.05, 0.03, 0.02, 0.001, 2},
    {"s3_role_swapping_frenzy", 5000, 0.30, 0.20, 0.20, 0.10, 0.10, 0.05, 0.05, 0.10, 1},
    {"s4_command_and_support", 2000, 0.40, 0.15, 0.15, 0.08, 0.08, 0.04, 0.10, 0.001, 5},
    {"s5_mass_casualty_event", 10000, 0.20, 0.30, 0.20, 0.10, 0.10, 0.05, 0.05, 0.01, 8}
};

const float GLOBAL_SKILL_MATRIX[7][7] = {
    {1.0f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f},
    {0.2f, 1.0f, 0.2f, 0.1f, 0.1f, 0.1f, 0.2f},
    {0.2f, 0.2f, 1.0f, 0.1f, 0.2f, 0.1f, 0.1f},
    {0.1f, 0.1f, 0.1f, 1.0f, 0.1f, 0.1f, 0.1f},
    {0.3f, 0.1f, 0.1f, 0.1f, 1.0f, 0.1f, 0.1f},
    {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 1.0f, 0.1f},
    {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f}
};

RoleType RollRole(double r, const SceneConfig& cfg) {
    if (r < cfg.rifleman_pct) return RoleType::Rifleman;
    r -= cfg.rifleman_pct;
    if (r < cfg.medic_pct) return RoleType::Medic;
    r -= cfg.medic_pct;
    if (r < cfg.engineer_pct) return RoleType::Engineer;
    r -= cfg.engineer_pct;
    if (r < cfg.at_pct) return RoleType::AT;
    r -= cfg.at_pct;
    if (r < cfg.mg_pct) return RoleType::MG;
    r -= cfg.mg_pct;
    if (r < cfg.sniper_pct) return RoleType::Sniper;
    return RoleType::Commander;
}

namespace tag_based {

struct TagSoldier {
    int id;
    RoleType role;
    float hp;
    float max_hp;
    int ammo;
    int max_ammo;
    float class_resource;
    float skill_modifiers[7];
};

std::vector<TagSoldier> soldiers;

void Initialize(const SceneConfig& cfg, int seed) {
    soldiers.clear();
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    for (int i = 0; i < cfg.num_entities; ++i) {
        RoleType r = RollRole(dis(gen), cfg);
        TagSoldier s;
        s.id = i;
        s.role = r;
        s.hp = 100.0f;
        s.max_hp = 100.0f;
        s.ammo = 90;
        s.max_ammo = 90;
        s.class_resource = 10.0f;
        int r_idx = static_cast<int>(r);
        for (int k = 0; k < 7; ++k) {
            s.skill_modifiers[k] = GLOBAL_SKILL_MATRIX[r_idx][k];
        }
        soldiers.push_back(s);
    }
}

void Update(int ticks, const SceneConfig& cfg, int seed, double& update_ns, double& swap_ns, double& check_ns, float& checksum) {
    std::mt19937 gen(seed + 1);
    std::uniform_int_distribution<int> role_dis(0, 6);
    std::uniform_int_distribution<int> skill_dis(0, 6);
    
    int num_swaps = static_cast<int>(cfg.num_entities * cfg.swap_pct);
    
    for (int t = 0; t < ticks; ++t) {
        auto start_up = std::chrono::high_resolution_clock::now();
        for (auto& s : soldiers) {
            s.hp = std::min(s.max_hp, s.hp + 0.1f);
            switch (s.role) {
                case RoleType::Rifleman:
                    s.ammo = std::max(0, s.ammo - 1);
                    break;
                case RoleType::Medic:
                    s.class_resource = std::max(0.0f, s.class_resource - 0.2f);
                    break;
                case RoleType::Engineer:
                    s.class_resource = std::max(0.0f, s.class_resource - 0.1f);
                    break;
                case RoleType::AT:
                    s.ammo = std::max(0, s.ammo - 1);
                    break;
                case RoleType::MG:
                    s.class_resource = std::min(100.0f, s.class_resource + 0.5f);
                    break;
                case RoleType::Sniper:
                    s.ammo = std::max(0, s.ammo - 1);
                    break;
                case RoleType::Commander:
                    s.class_resource = std::max(0.0f, s.class_resource - 0.05f);
                    break;
            }
        }
        auto end_up = std::chrono::high_resolution_clock::now();
        update_ns += std::chrono::duration<double, std::nano>(end_up - start_up).count() / cfg.num_entities;
        
        auto start_swap = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_swaps; ++i) {
            int idx = i % cfg.num_entities;
            RoleType next_r = static_cast<RoleType>(role_dis(gen));
            soldiers[idx].role = next_r;
            int r_idx = static_cast<int>(next_r);
            for (int k = 0; k < 7; ++k) {
                soldiers[idx].skill_modifiers[k] = GLOBAL_SKILL_MATRIX[r_idx][k];
            }
        }
        auto end_swap = std::chrono::high_resolution_clock::now();
        if (num_swaps > 0) {
            swap_ns += std::chrono::duration<double, std::nano>(end_swap - start_swap).count() / num_swaps;
        }
        
        auto start_check = std::chrono::high_resolution_clock::now();
        int checks_count = cfg.num_entities * cfg.checks_per_tick;
        for (int c = 0; c < checks_count; ++c) {
            int idx = c % cfg.num_entities;
            int sk = skill_dis(gen);
            checksum += soldiers[idx].skill_modifiers[sk];
        }
        auto end_check = std::chrono::high_resolution_clock::now();
        check_ns += std::chrono::duration<double, std::nano>(end_check - start_check).count() / checks_count;
    }
}

}

namespace component_bundle {

struct BaseComponent {
    int id;
    float hp;
    float max_hp;
    int ammo;
    int max_ammo;
};

struct RiflemanComponent { float reload_multiplier; };
struct MedicComponent { float medkits; float heal_rate; };
struct EngineerComponent { float repair_parts; float repair_rate; };
struct AtComponent { int rockets; float reload_rate; };
struct MgComponent { float barrel_heat; float cooldown_rate; };
struct SniperComponent { float zoom_factor; float steady_rate; };
struct CommanderComponent { float command_range; float command_buff; };

struct RiflemanData { BaseComponent base; RiflemanComponent comp; };
struct MedicData { BaseComponent base; MedicComponent comp; };
struct EngineerData { BaseComponent base; EngineerComponent comp; };
struct AtData { BaseComponent base; AtComponent comp; };
struct MgData { BaseComponent base; MgComponent comp; };
struct SniperData { BaseComponent base; SniperComponent comp; };
struct CommanderData { BaseComponent base; CommanderComponent comp; };

std::vector<RiflemanData> riflemen;
std::vector<MedicData> medics;
std::vector<EngineerData> engineers;
std::vector<AtData> ats;
std::vector<MgData> mgs;
std::vector<SniperData> snipers;
std::vector<CommanderData> commanders;

struct EntityLoc {
    RoleType role;
    int vec_idx;
};

std::vector<EntityLoc> entity_locs;

void Initialize(const SceneConfig& cfg, int seed) {
    riflemen.clear();
    medics.clear();
    engineers.clear();
    ats.clear();
    mgs.clear();
    snipers.clear();
    commanders.clear();
    entity_locs.resize(cfg.num_entities);
    
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    for (int i = 0; i < cfg.num_entities; ++i) {
        RoleType r = RollRole(dis(gen), cfg);
        BaseComponent base{i, 100.0f, 100.0f, 90, 90};
        entity_locs[i].role = r;
        
        switch (r) {
            case RoleType::Rifleman:
                entity_locs[i].vec_idx = static_cast<int>(riflemen.size());
                riflemen.push_back({base, {1.0f}});
                break;
            case RoleType::Medic:
                entity_locs[i].vec_idx = static_cast<int>(medics.size());
                medics.push_back({base, {10.0f, 1.0f}});
                break;
            case RoleType::Engineer:
                entity_locs[i].vec_idx = static_cast<int>(engineers.size());
                engineers.push_back({base, {10.0f, 1.0f}});
                break;
            case RoleType::AT:
                entity_locs[i].vec_idx = static_cast<int>(ats.size());
                ats.push_back({base, {5, 1.0f}});
                break;
            case RoleType::MG:
                entity_locs[i].vec_idx = static_cast<int>(mgs.size());
                mgs.push_back({base, {0.0f, 1.0f}});
                break;
            case RoleType::Sniper:
                entity_locs[i].vec_idx = static_cast<int>(snipers.size());
                snipers.push_back({base, {2.0f, 1.0f}});
                break;
            case RoleType::Commander:
                entity_locs[i].vec_idx = static_cast<int>(commanders.size());
                commanders.push_back({base, {50.0f, 1.0f}});
                break;
        }
    }
}

void RemoveEntityFromVector([[maybe_unused]] int id, RoleType role, int vec_idx) {
    switch (role) {
        case RoleType::Rifleman:
            if (vec_idx != static_cast<int>(riflemen.size()) - 1) {
                riflemen[vec_idx] = riflemen.back();
                entity_locs[riflemen[vec_idx].base.id].vec_idx = vec_idx;
            }
            riflemen.pop_back();
            break;
        case RoleType::Medic:
            if (vec_idx != static_cast<int>(medics.size()) - 1) {
                medics[vec_idx] = medics.back();
                entity_locs[medics[vec_idx].base.id].vec_idx = vec_idx;
            }
            medics.pop_back();
            break;
        case RoleType::Engineer:
            if (vec_idx != static_cast<int>(engineers.size()) - 1) {
                engineers[vec_idx] = engineers.back();
                entity_locs[engineers[vec_idx].base.id].vec_idx = vec_idx;
            }
            engineers.pop_back();
            break;
        case RoleType::AT:
            if (vec_idx != static_cast<int>(ats.size()) - 1) {
                ats[vec_idx] = ats.back();
                entity_locs[ats[vec_idx].base.id].vec_idx = vec_idx;
            }
            ats.pop_back();
            break;
        case RoleType::MG:
            if (vec_idx != static_cast<int>(mgs.size()) - 1) {
                mgs[vec_idx] = mgs.back();
                entity_locs[mgs[vec_idx].base.id].vec_idx = vec_idx;
            }
            mgs.pop_back();
            break;
        case RoleType::Sniper:
            if (vec_idx != static_cast<int>(snipers.size()) - 1) {
                snipers[vec_idx] = snipers.back();
                entity_locs[snipers[vec_idx].base.id].vec_idx = vec_idx;
            }
            snipers.pop_back();
            break;
        case RoleType::Commander:
            if (vec_idx != static_cast<int>(commanders.size()) - 1) {
                commanders[vec_idx] = commanders.back();
                entity_locs[commanders[vec_idx].base.id].vec_idx = vec_idx;
            }
            commanders.pop_back();
            break;
    }
}

void AddEntityToVector(int id, BaseComponent base, RoleType new_role) {
    entity_locs[id].role = new_role;
    switch (new_role) {
        case RoleType::Rifleman:
            entity_locs[id].vec_idx = static_cast<int>(riflemen.size());
            riflemen.push_back({base, {1.0f}});
            break;
        case RoleType::Medic:
            entity_locs[id].vec_idx = static_cast<int>(medics.size());
            medics.push_back({base, {10.0f, 1.0f}});
            break;
        case RoleType::Engineer:
            entity_locs[id].vec_idx = static_cast<int>(engineers.size());
            engineers.push_back({base, {10.0f, 1.0f}});
            break;
        case RoleType::AT:
            entity_locs[id].vec_idx = static_cast<int>(ats.size());
            ats.push_back({base, {5, 1.0f}});
            break;
        case RoleType::MG:
            entity_locs[id].vec_idx = static_cast<int>(mgs.size());
            mgs.push_back({base, {0.0f, 1.0f}});
            break;
        case RoleType::Sniper:
            entity_locs[id].vec_idx = static_cast<int>(snipers.size());
            snipers.push_back({base, {2.0f, 1.0f}});
            break;
        case RoleType::Commander:
            entity_locs[id].vec_idx = static_cast<int>(commanders.size());
            commanders.push_back({base, {50.0f, 1.0f}});
            break;
    }
}

BaseComponent GetBaseComponent([[maybe_unused]] int id, RoleType role, int vec_idx) {
    switch (role) {
        case RoleType::Rifleman: return riflemen[vec_idx].base;
        case RoleType::Medic: return medics[vec_idx].base;
        case RoleType::Engineer: return engineers[vec_idx].base;
        case RoleType::AT: return ats[vec_idx].base;
        case RoleType::MG: return mgs[vec_idx].base;
        case RoleType::Sniper: return snipers[vec_idx].base;
        case RoleType::Commander: return commanders[vec_idx].base;
    }
    return BaseComponent{};
}

void Update(int ticks, const SceneConfig& cfg, int seed, double& update_ns, double& swap_ns, double& check_ns, float& checksum) {
    std::mt19937 gen(seed + 1);
    std::uniform_int_distribution<int> role_dis(0, 6);
    std::uniform_int_distribution<int> skill_dis(0, 6);
    
    int num_swaps = static_cast<int>(cfg.num_entities * cfg.swap_pct);
    
    for (int t = 0; t < ticks; ++t) {
        auto start_up = std::chrono::high_resolution_clock::now();
        for (auto& r : riflemen) {
            r.base.hp = std::min(r.base.max_hp, r.base.hp + 0.1f);
            r.base.ammo = std::max(0, r.base.ammo - 1);
        }
        for (auto& m : medics) {
            m.base.hp = std::min(m.base.max_hp, m.base.hp + 0.1f);
            m.comp.medkits = std::max(0.0f, m.comp.medkits - 0.2f);
        }
        for (auto& e : engineers) {
            e.base.hp = std::min(e.base.max_hp, e.base.hp + 0.1f);
            e.comp.repair_parts = std::max(0.0f, e.comp.repair_parts - 0.1f);
        }
        for (auto& a : ats) {
            a.base.hp = std::min(a.base.max_hp, a.base.hp + 0.1f);
            a.base.ammo = std::max(0, a.base.ammo - 1);
        }
        for (auto& g : mgs) {
            g.base.hp = std::min(g.base.max_hp, g.base.hp + 0.1f);
            g.comp.barrel_heat = std::min(100.0f, g.comp.barrel_heat + 0.5f);
        }
        for (auto& s : snipers) {
            s.base.hp = std::min(s.base.max_hp, s.base.hp + 0.1f);
            s.base.ammo = std::max(0, s.base.ammo - 1);
        }
        for (auto& c : commanders) {
            c.base.hp = std::min(c.base.max_hp, c.base.hp + 0.1f);
            c.comp.command_buff = std::max(0.0f, c.comp.command_buff - 0.05f);
        }
        auto end_up = std::chrono::high_resolution_clock::now();
        update_ns += std::chrono::duration<double, std::nano>(end_up - start_up).count() / cfg.num_entities;
        
        auto start_swap = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_swaps; ++i) {
            int idx = i % cfg.num_entities;
            RoleType next_r = static_cast<RoleType>(role_dis(gen));
            RoleType old_r = entity_locs[idx].role;
            int old_v_idx = entity_locs[idx].vec_idx;
            
            if (next_r != old_r) {
                BaseComponent base = GetBaseComponent(idx, old_r, old_v_idx);
                RemoveEntityFromVector(idx, old_r, old_v_idx);
                AddEntityToVector(idx, base, next_r);
            }
        }
        auto end_swap = std::chrono::high_resolution_clock::now();
        if (num_swaps > 0) {
            swap_ns += std::chrono::duration<double, std::nano>(end_swap - start_swap).count() / num_swaps;
        }
        
        auto start_check = std::chrono::high_resolution_clock::now();
        int checks_count = cfg.num_entities * cfg.checks_per_tick;
        for (int c = 0; c < checks_count; ++c) {
            int idx = c % cfg.num_entities;
            int sk = skill_dis(gen);
            RoleType r = entity_locs[idx].role;
            int r_idx = static_cast<int>(r);
            
            float val = GLOBAL_SKILL_MATRIX[r_idx][sk];
            if (r == static_cast<RoleType>(sk)) {
                int v_idx = entity_locs[idx].vec_idx;
                switch (r) {
                    case RoleType::Rifleman: val *= riflemen[v_idx].comp.reload_multiplier; break;
                    case RoleType::Medic: val *= medics[v_idx].comp.heal_rate; break;
                    case RoleType::Engineer: val *= engineers[v_idx].comp.repair_rate; break;
                    case RoleType::AT: val *= ats[v_idx].comp.reload_rate; break;
                    case RoleType::MG: val *= mgs[v_idx].comp.cooldown_rate; break;
                    case RoleType::Sniper: val *= snipers[v_idx].comp.steady_rate; break;
                    case RoleType::Commander: val *= commanders[v_idx].comp.command_buff; break;
                }
            }
            checksum += val;
        }
        auto end_check = std::chrono::high_resolution_clock::now();
        check_ns += std::chrono::duration<double, std::nano>(end_check - start_check).count() / checks_count;
    }
}

}

namespace dynamic_inheritance {

struct RolePrefab {
    float default_skill_modifiers[7];
    int default_max_ammo;
    float default_max_resource;
};

RolePrefab prefabs[7];

struct InheritanceSoldier {
    int id;
    float hp;
    float max_hp;
    int ammo;
    float class_resource;
    const RolePrefab* prefab;
};

std::vector<InheritanceSoldier> soldiers;

void Initialize(const SceneConfig& cfg, int seed) {
    for (int k = 0; k < 7; ++k) {
        for (int s = 0; s < 7; ++s) {
            prefabs[k].default_skill_modifiers[s] = GLOBAL_SKILL_MATRIX[k][s];
        }
        prefabs[k].default_max_ammo = 90;
        prefabs[k].default_max_resource = 10.0f;
    }
    
    soldiers.clear();
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    for (int i = 0; i < cfg.num_entities; ++i) {
        RoleType r = RollRole(dis(gen), cfg);
        InheritanceSoldier s;
        s.id = i;
        s.hp = 100.0f;
        s.max_hp = 100.0f;
        s.ammo = prefabs[static_cast<int>(r)].default_max_ammo;
        s.class_resource = prefabs[static_cast<int>(r)].default_max_resource;
        s.prefab = &prefabs[static_cast<int>(r)];
        soldiers.push_back(s);
    }
}

void Update(int ticks, const SceneConfig& cfg, int seed, double& update_ns, double& swap_ns, double& check_ns, float& checksum) {
    std::mt19937 gen(seed + 1);
    std::uniform_int_distribution<int> role_dis(0, 6);
    std::uniform_int_distribution<int> skill_dis(0, 6);
    
    int num_swaps = static_cast<int>(cfg.num_entities * cfg.swap_pct);
    
    for (int t = 0; t < ticks; ++t) {
        auto start_up = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < soldiers.size(); ++i) {
            auto& s = soldiers[i];
            s.hp = std::min(s.max_hp, s.hp + 0.1f);
            
            int p_idx = static_cast<int>(s.prefab - &prefabs[0]);
            switch (static_cast<RoleType>(p_idx)) {
                case RoleType::Rifleman: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::Medic: s.class_resource = std::max(0.0f, s.class_resource - 0.2f); break;
                case RoleType::Engineer: s.class_resource = std::max(0.0f, s.class_resource - 0.1f); break;
                case RoleType::AT: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::MG: s.class_resource = std::min(100.0f, s.class_resource + 0.5f); break;
                case RoleType::Sniper: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::Commander: s.class_resource = std::max(0.0f, s.class_resource - 0.05f); break;
            }
        }
        auto end_up = std::chrono::high_resolution_clock::now();
        update_ns += std::chrono::duration<double, std::nano>(end_up - start_up).count() / cfg.num_entities;
        
        auto start_swap = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_swaps; ++i) {
            int idx = i % cfg.num_entities;
            int next_r = role_dis(gen);
            soldiers[idx].prefab = &prefabs[next_r];
            soldiers[idx].ammo = prefabs[next_r].default_max_ammo;
            soldiers[idx].class_resource = prefabs[next_r].default_max_resource;
        }
        auto end_swap = std::chrono::high_resolution_clock::now();
        if (num_swaps > 0) {
            swap_ns += std::chrono::duration<double, std::nano>(end_swap - start_swap).count() / num_swaps;
        }
        
        auto start_check = std::chrono::high_resolution_clock::now();
        int checks_count = cfg.num_entities * cfg.checks_per_tick;
        for (int c = 0; c < checks_count; ++c) {
            int idx = c % cfg.num_entities;
            int sk = skill_dis(gen);
            checksum += soldiers[idx].prefab->default_skill_modifiers[sk];
        }
        auto end_check = std::chrono::high_resolution_clock::now();
        check_ns += std::chrono::duration<double, std::nano>(end_check - start_check).count() / checks_count;
    }
}

}

namespace cached_union {

struct FlatSoldier {
    int id;
    float hp;
    float max_hp;
    int ammo;
    int max_ammo;
    float class_resource;
    int role_index;
    const float* cached_skill_row;
};

std::vector<FlatSoldier> soldiers;

void Initialize(const SceneConfig& cfg, int seed) {
    soldiers.clear();
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    for (int i = 0; i < cfg.num_entities; ++i) {
        RoleType r = RollRole(dis(gen), cfg);
        FlatSoldier s;
        s.id = i;
        s.role_index = static_cast<int>(r);
        s.hp = 100.0f;
        s.max_hp = 100.0f;
        s.ammo = 90;
        s.max_ammo = 90;
        s.class_resource = 10.0f;
        s.cached_skill_row = GLOBAL_SKILL_MATRIX[s.role_index];
        soldiers.push_back(s);
    }
}

void Update(int ticks, const SceneConfig& cfg, int seed, double& update_ns, double& swap_ns, double& check_ns, float& checksum) {
    std::mt19937 gen(seed + 1);
    std::uniform_int_distribution<int> role_dis(0, 6);
    std::uniform_int_distribution<int> skill_dis(0, 6);
    
    int num_swaps = static_cast<int>(cfg.num_entities * cfg.swap_pct);
    
    for (int t = 0; t < ticks; ++t) {
        auto start_up = std::chrono::high_resolution_clock::now();
        for (auto& s : soldiers) {
            s.hp = std::min(s.max_hp, s.hp + 0.1f);
            switch (static_cast<RoleType>(s.role_index)) {
                case RoleType::Rifleman: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::Medic: s.class_resource = std::max(0.0f, s.class_resource - 0.2f); break;
                case RoleType::Engineer: s.class_resource = std::max(0.0f, s.class_resource - 0.1f); break;
                case RoleType::AT: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::MG: s.class_resource = std::min(100.0f, s.class_resource + 0.5f); break;
                case RoleType::Sniper: s.ammo = std::max(0, s.ammo - 1); break;
                case RoleType::Commander: s.class_resource = std::max(0.0f, s.class_resource - 0.05f); break;
            }
        }
        auto end_up = std::chrono::high_resolution_clock::now();
        update_ns += std::chrono::duration<double, std::nano>(end_up - start_up).count() / cfg.num_entities;
        
        auto start_swap = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_swaps; ++i) {
            int idx = i % cfg.num_entities;
            int next_r = role_dis(gen);
            soldiers[idx].role_index = next_r;
            soldiers[idx].cached_skill_row = GLOBAL_SKILL_MATRIX[next_r];
        }
        auto end_swap = std::chrono::high_resolution_clock::now();
        if (num_swaps > 0) {
            swap_ns += std::chrono::duration<double, std::nano>(end_swap - start_swap).count() / num_swaps;
        }
        
        auto start_check = std::chrono::high_resolution_clock::now();
        int checks_count = cfg.num_entities * cfg.checks_per_tick;
        for (int c = 0; c < checks_count; ++c) {
            int idx = c % cfg.num_entities;
            int sk = skill_dis(gen);
            checksum += soldiers[idx].cached_skill_row[sk];
        }
        auto end_check = std::chrono::high_resolution_clock::now();
        check_ns += std::chrono::duration<double, std::nano>(end_check - start_check).count() / checks_count;
    }
}

}

namespace sparse_list {

struct SparseSoldier {
    int id;
    float hp;
    float max_hp;
};

struct RiflemanComponent { float reload_multiplier; int ammo; };
struct MedicComponent { float medkits; float heal_rate; };
struct EngineerComponent { float repair_parts; float repair_rate; };
struct AtComponent { int rockets; float reload_rate; };
struct MgComponent { float barrel_heat; float cooldown_rate; };
struct SniperComponent { float zoom_factor; float steady_rate; int ammo; };
struct CommanderComponent { float command_range; float command_buff; };

std::vector<SparseSoldier> soldiers;
std::unordered_map<int, RiflemanComponent> riflemen;
std::unordered_map<int, MedicComponent> medics;
std::unordered_map<int, EngineerComponent> engineers;
std::unordered_map<int, AtComponent> ats;
std::unordered_map<int, MgComponent> mgs;
std::unordered_map<int, SniperComponent> snipers;
std::unordered_map<int, CommanderComponent> commanders;
std::vector<RoleType> initial_roles;

void ClearSparse() {
    riflemen.clear();
    medics.clear();
    engineers.clear();
    ats.clear();
    mgs.clear();
    snipers.clear();
    commanders.clear();
}

void Initialize(const SceneConfig& cfg, int seed) {
    soldiers.clear();
    ClearSparse();
    initial_roles.resize(cfg.num_entities);
    
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    for (int i = 0; i < cfg.num_entities; ++i) {
        RoleType r = RollRole(dis(gen), cfg);
        SparseSoldier s{i, 100.0f, 100.0f};
        soldiers.push_back(s);
        initial_roles[i] = r;
        
        switch (r) {
            case RoleType::Rifleman: riflemen[i] = {1.0f, 90}; break;
            case RoleType::Medic: medics[i] = {10.0f, 1.0f}; break;
            case RoleType::Engineer: engineers[i] = {10.0f, 1.0f}; break;
            case RoleType::AT: ats[i] = {5, 1.0f}; break;
            case RoleType::MG: mgs[i] = {0.0f, 1.0f}; break;
            case RoleType::Sniper: snipers[i] = {2.0f, 1.0f, 90}; break;
            case RoleType::Commander: commanders[i] = {50.0f, 1.0f}; break;
        }
    }
}

void RemoveRole(int id, RoleType role) {
    switch (role) {
        case RoleType::Rifleman: riflemen.erase(id); break;
        case RoleType::Medic: medics.erase(id); break;
        case RoleType::Engineer: engineers.erase(id); break;
        case RoleType::AT: ats.erase(id); break;
        case RoleType::MG: mgs.erase(id); break;
        case RoleType::Sniper: snipers.erase(id); break;
        case RoleType::Commander: commanders.erase(id); break;
    }
}

void AddRole(int id, RoleType role) {
    switch (role) {
        case RoleType::Rifleman: riflemen[id] = {1.0f, 90}; break;
        case RoleType::Medic: medics[id] = {10.0f, 1.0f}; break;
        case RoleType::Engineer: engineers[id] = {10.0f, 1.0f}; break;
        case RoleType::AT: ats[id] = {5, 1.0f}; break;
        case RoleType::MG: mgs[id] = {0.0f, 1.0f}; break;
        case RoleType::Sniper: snipers[id] = {2.0f, 1.0f, 90}; break;
        case RoleType::Commander: commanders[id] = {50.0f, 1.0f}; break;
    }
}

void Update(int ticks, const SceneConfig& cfg, int seed, double& update_ns, double& swap_ns, double& check_ns, float& checksum) {
    std::mt19937 gen(seed + 1);
    std::uniform_int_distribution<int> role_dis(0, 6);
    std::uniform_int_distribution<int> skill_dis(0, 6);
    
    int num_swaps = static_cast<int>(cfg.num_entities * cfg.swap_pct);
    
    for (int t = 0; t < ticks; ++t) {
        auto start_up = std::chrono::high_resolution_clock::now();
        for (auto& s : soldiers) {
            s.hp = std::min(s.max_hp, s.hp + 0.1f);
        }
        for (auto& [id, r] : riflemen) {
            r.ammo = std::max(0, r.ammo - 1);
        }
        for (auto& [id, m] : medics) {
            m.medkits = std::max(0.0f, m.medkits - 0.2f);
        }
        for (auto& [id, e] : engineers) {
            e.repair_parts = std::max(0.0f, e.repair_parts - 0.1f);
        }
        for (auto& [id, a] : mgs) {
            a.barrel_heat = std::min(100.0f, a.barrel_heat + 0.5f);
        }
        for (auto& [id, s] : snipers) {
            s.ammo = std::max(0, s.ammo - 1);
        }
        for (auto& [id, c] : commanders) {
            c.command_buff = std::max(0.0f, c.command_buff - 0.05f);
        }
        auto end_up = std::chrono::high_resolution_clock::now();
        update_ns += std::chrono::duration<double, std::nano>(end_up - start_up).count() / cfg.num_entities;
        
        auto start_swap = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_swaps; ++i) {
            int id = i % cfg.num_entities;
            RoleType next_r = static_cast<RoleType>(role_dis(gen));
            RoleType old_r = initial_roles[id];
            if (next_r != old_r) {
                RemoveRole(id, old_r);
                AddRole(id, next_r);
                initial_roles[id] = next_r;
            }
        }
        auto end_swap = std::chrono::high_resolution_clock::now();
        if (num_swaps > 0) {
            swap_ns += std::chrono::duration<double, std::nano>(end_swap - start_swap).count() / num_swaps;
        }
        
        auto start_check = std::chrono::high_resolution_clock::now();
        int checks_count = cfg.num_entities * cfg.checks_per_tick;
        for (int c = 0; c < checks_count; ++c) {
            int id = c % cfg.num_entities;
            int sk = skill_dis(gen);
            RoleType cur_r = initial_roles[id];
            int r_idx = static_cast<int>(cur_r);
            float val = GLOBAL_SKILL_MATRIX[r_idx][sk];
            if (cur_r == static_cast<RoleType>(sk)) {
                switch (cur_r) {
                    case RoleType::Rifleman: val *= riflemen[id].reload_multiplier; break;
                    case RoleType::Medic: val *= medics[id].heal_rate; break;
                    case RoleType::Engineer: val *= engineers[id].repair_rate; break;
                    case RoleType::AT: val *= ats[id].reload_rate; break;
                    case RoleType::MG: val *= mgs[id].cooldown_rate; break;
                    case RoleType::Sniper: val *= snipers[id].steady_rate; break;
                    case RoleType::Commander: val *= commanders[id].command_buff; break;
                }
            }
            checksum += val;
        }
        auto end_check = std::chrono::high_resolution_clock::now();
        check_ns += std::chrono::duration<double, std::nano>(end_check - start_check).count() / checks_count;
    }
}

}

int main() {
    std::ofstream out("results.csv");
    if (!out.is_open()) {
        std::cerr << "Failed to open results.csv for writing" << std::endl;
        return 1;
    }
    
    out << "strategy,scene,seed,update_time_ns_per_ent,swap_time_ns_per_swap,check_time_ns_per_check\n";
    
    int seeds[] = {1, 7, 42, 1234, 31337};
    int ticks = 50;
    int warmup_ticks = 5;
    
    float checksum = 0.0f;
    
    for (const auto& scene : SCENES) {
        std::cout << "Running scene: " << scene.name << std::endl;
        for (int seed : seeds) {
            {
                tag_based::Initialize(scene, seed);
                double up_warm = 0.0, sw_warm = 0.0, ch_warm = 0.0;
                tag_based::Update(warmup_ticks, scene, seed, up_warm, sw_warm, ch_warm, checksum);
                
                double up_accum = 0.0, sw_accum = 0.0, ch_accum = 0.0;
                tag_based::Update(ticks, scene, seed, up_accum, sw_accum, ch_accum, checksum);
                
                out << "A_TagBasedRoles," << scene.name << "," << seed << ","
                    << (up_accum / ticks) << "," << (sw_accum / ticks) << "," << (ch_accum / ticks) << "\n";
            }
            {
                component_bundle::Initialize(scene, seed);
                double up_warm = 0.0, sw_warm = 0.0, ch_warm = 0.0;
                component_bundle::Update(warmup_ticks, scene, seed, up_warm, sw_warm, ch_warm, checksum);
                
                double up_accum = 0.0, sw_accum = 0.0, ch_accum = 0.0;
                component_bundle::Update(ticks, scene, seed, up_accum, sw_accum, ch_accum, checksum);
                
                out << "B_ComponentBundle," << scene.name << "," << seed << ","
                    << (up_accum / ticks) << "," << (sw_accum / ticks) << "," << (ch_accum / ticks) << "\n";
            }
            {
                dynamic_inheritance::Initialize(scene, seed);
                double up_warm = 0.0, sw_warm = 0.0, ch_warm = 0.0;
                dynamic_inheritance::Update(warmup_ticks, scene, seed, up_warm, sw_warm, ch_warm, checksum);
                
                double up_accum = 0.0, sw_accum = 0.0, ch_accum = 0.0;
                dynamic_inheritance::Update(ticks, scene, seed, up_accum, sw_accum, ch_accum, checksum);
                
                out << "C_DynamicInheritance," << scene.name << "," << seed << ","
                    << (up_accum / ticks) << "," << (sw_accum / ticks) << "," << (ch_accum / ticks) << "\n";
            }
            {
                cached_union::Initialize(scene, seed);
                double up_warm = 0.0, sw_warm = 0.0, ch_warm = 0.0;
                cached_union::Update(warmup_ticks, scene, seed, up_warm, sw_warm, ch_warm, checksum);
                
                double up_accum = 0.0, sw_accum = 0.0, ch_accum = 0.0;
                cached_union::Update(ticks, scene, seed, up_accum, sw_accum, ch_accum, checksum);
                
                out << "D_CachedSkillTable_Union," << scene.name << "," << seed << ","
                    << (up_accum / ticks) << "," << (sw_accum / ticks) << "," << (ch_accum / ticks) << "\n";
            }
            {
                sparse_list::Initialize(scene, seed);
                double up_warm = 0.0, sw_warm = 0.0, ch_warm = 0.0;
                sparse_list::Update(warmup_ticks, scene, seed, up_warm, sw_warm, ch_warm, checksum);
                
                double up_accum = 0.0, sw_accum = 0.0, ch_accum = 0.0;
                sparse_list::Update(ticks, scene, seed, up_accum, sw_accum, ch_accum, checksum);
                
                out << "E_SparseComponentList," << scene.name << "," << seed << ","
                    << (up_accum / ticks) << "," << (sw_accum / ticks) << "," << (ch_accum / ticks) << "\n";
            }
        }
    }
    
    std::cout << "Benchmark complete. Checksum: " << checksum << std::endl;
    out.close();
    return 0;
}
