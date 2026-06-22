# 2026-06-22-resource-harvesting-economy — Procedural Voxel Resource Harvesting Economy

**Status:** `in-progress`
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Tier 3 Economy — sandbox content)
**Estimated effort:** M
**Author:** agent (self)

---

## 1. Hypothesis

5-стратегийное сравнение ∈ {A_NoHarvesting (baseline — infinite resources per closed `factory-production-system` A_NaiveLinearScan), B_StaticNode_Depletion (Foxhole-style: fixed resource nodes with richness × depletion; vanilla extraction rate), C_ProceduralNode_DynamicRichness (procedurally seeded per-chunk nodes with depth-based richness gradient, replenish over time), D_ExtractorBuilding_Tiered (player-placed extractors with efficiency × proximity_bonus × adjacency bonus, per SupCom/Foxhole), E_FullEconomyChain (harvest → refine → produce — connects to closed `supply-logistics-simulation` + `factory-production-system`)} даст **<5 µs/chunk per tick update cost** (0.015% of 30 Hz for 1000 chunks) при **meaningful scarcity pressure** (players must expand to secure fresh nodes, not camp one spot forever). Alternative = infinite resource (A) = no scarcity, no logistics gameplay.

---

## 2. Prior art

Web-research planned (Phase 1). Expected sources:

- Foxhole resource nodes (scrap fields, sulfur, component mines, salvage) — production precedent for static node depletion + regional distribution.
- Supreme Commander mass extractors + adjacency bonus — extraction building placement meta.
- Minecraft ore distribution + mining — per-chunk seeded procedural vein placement.
- Wikipedia "Resource depletion" — economic scarcity as gameplay driver.
- Wikipedia "Peak oil" — non-renewable resource exhaustion curve.
- Factorio resource patches — infinite but yield-decreasing per drain cycle.
- Satisfactory pure nodes — fixed quality tiers (impure/normal/pure) + extractor overclocking.
- Dwarf Fortress — finite layered veins with depth dependence.
- No Man's Sky — hotspot-based extractors with diminishing returns.

Cross-refs: closed `procedural-voxel-resource-deposits` [mixed, ore deposit gen — direct prerequisite: this experiment = **extraction gameplay on top of deposit generation**] + `factory-production-system` [mixed, E_Hybrid_CP_LazyPQueue ⭐ universal default, downstream consumer of harvested materials] + `supply-logistics-simulation` [mixed, E_PersistentCache_Incremental ⭐ winner, transport of harvested goods] + `terrain-traction-variation` [yes, extraction vehicles] + `data-driven-vehicle-weapon-definitions` [mixed, harvester/excavator definitions].

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype + benchmark.
- **Scenes:** 5 synthetic scenarios testing different node distributions and extraction patterns:
  - s1_uniform — nodes evenly distributed, 10% cells have resources (Foxhole-like)
  - s2_clustered — rare high-yield clusters, 0.5% of chunks are rich (SupCom/Minecraft-like)
  - s3_deep_gradient — resources only below depth 20, richness increases with depth
  - s4_oil_field — single massive oil field in centre 4×4 chunks, rest barren
  - s5_multi_tier — 100% resource coverage: 60% iron, 25% copper, 10% coal, 5% rare
- **Metrics:** mean/median/p95/p99/std CPU ns per tick, ns/chunk.
- **Control:** A_NoHarvesting baseline (bare iteration).
- **Protocol:** N=1000 iter + 10 warmup per `benchmarks/methodology.md`. Governor=powersave. Zen 3 5800X per `hardware-profile.md §1`. Compiler: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`.

---

## 4. Prototype

Planned (Phase 2):

```bash
prototype/resource_harvest_bench.cpp
prototype/CMakeLists.txt
prototype/build/
```

---

## 5. Results

Phase 3.

---

## 6. Verdict

Phase 3.

---

## 7. Integration recommendation

Phase 3.

---

## 8. Sources

Phase 1.

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** per-chunk tick loop (Stage 4.1 world simulation) — resource extraction runs at 1-10 Hz per chunk/entity.
- **Assumptions:** CPU-only model; no GPU particle/VFX for mining; no Flecs overhead modeled.
- **Unmeasured:** Jolt physics for extractor buildings; network sync for multi-player extraction state.
- **Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §2 (DDR4 32 GiB).
