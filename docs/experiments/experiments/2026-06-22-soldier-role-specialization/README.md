# 2026-06-22-soldier-role-specialization — Soldier Role Specialization and Skill Systems

**Status:** `in-progress`
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent
**Estimated effort:** M
**Author:** Agent

---

## 1. Hypothesis

In a large-scale military simulation (10,000+ active soldier entities), representing class-based roles (Rifleman, Medic, Engineer, AT, MG, Sniper, Commander) and querying dynamic skills (healing speed, repair rate, AT reload speed, command range) requires balancing:
1. **Structural modification cost** (when a soldier switches classes at a supply crate or respawns).
2. **Access and iteration cost** (systems running logic per role: e.g., MedicSystem finding nearby wounded).
3. **Query/check cost** (ad-hoc checks: e.g., checking if a soldier has a tool or skill to repair a tank).

We hypothesize:
- **H1 (Access & Skill Check Cost):** Component bundle designs (**B**) and Cached Skill Union designs (**D**) will achieve the lowest check costs (<0.01 µs per check). Tag-based designs (**A**) will suffer from branch mispredictions, dynamic inheritance (**C**) from pointer chasing/relationship traversal, and sparse lists (**E**) from hash/search lookups.
- **H2 (Structural Swap Cost):** Bundles (**B**) and Sparse Lists (**E**) will exhibit massive performance penalties (>1.0 µs per swap) due to ECS archetype transitions. Flat Union Tables (**D**) and Tag-based (**A**) will allow O(1) instantaneous swaps (<0.02 µs per swap).
- **H3 (Memory Footprint):** Designs with flat unified storage (**A**, **D**) will be memory cache-friendly but have uniform footprints (~64-80 bytes), whereas prefabs (**C**) minimize local memory at the cost of lookup speed.

We evaluate 5 strategies:
1. **A_TagBasedRoles:** Exclusive role tags (e.g. `Role::Medic`). Flat component holds union/variant data.
2. **B_ComponentBundle:** Individual role components (`MedicComponent`, `EngineerComponent`) added/removed dynamically.
3. **C_DynamicInheritance:** Entities reference role prefabs via `IsA` relationship; components are resolved dynamically.
4. **D_CachedSkillTable_Union:** Entities have a flat `SoldierComponent` containing a cached pointer to a static class skill table and raw skill multipliers.
5. **E_SparseComponentList:** Individual skill tags/components (`CanHeal`, `CanRepair`) stored sparsely.

---

## 2. Prior Art

Web-research references:
- **Battlefield Class System:** Dynamic class change at supply boxes / spawn. Fast kit swaps.
- **Squad / Project Reality Roles:** Strict limits on specializations per squad (e.g., max 1 AT, max 1 medic).
- **Flecs Prefab Inheritance (`IsA`):** Traverses tree to find components. Efficient for static values, but query traversal has overhead.
- **Flecs Exclusive Relations:** `(Role, Medic)` where `Role` is exclusive, ensuring a soldier has exactly one role.

---

## 3. Method

- **Type of experiment:** prototype + benchmark.
- **Scene:** 5 scenes simulating different workloads:
  - `s1_uniform_combat`: All riflemen firing and checking reload skills.
  - `s2_specialized_squads`: Realistic military role distribution.
  - `s3_role_swapping_frenzy`: Soldiers swapping roles frequently (10% per tick).
  - `s4_command_and_support`: Commander active, issuing orders and reading skills.
  - `s5_mass_casualty_event`: 10k entities undergoing continuous triage and repair.
- **Metrics:** Update latency (µs), swap latency (µs), skill check latency (µs).
- **Control:** baseline (**A_TagBasedRoles**).

---

## 4. Prototype

Standalone C++26 CPU prototype:
- Code: `experiments/2026-06-22-soldier-role-specialization/prototype/soldier_role_bench.cpp`
- Build directory: `experiments/2026-06-22-soldier-role-specialization/prototype/build/`

---

## 5. Results

TBD.

---

## 6. Verdict

TBD.

---

## 7. Integration Recommendation

TBD.

---

## 8. Sources

TBD.

---

## 9. Mapping to ProjectV Hot-path

- Mainline folder: `src/ai/` or `src/flight/ecs/` for unit characteristics.
- Dev host `obvium` baseline per `hardware-profile.md`.
