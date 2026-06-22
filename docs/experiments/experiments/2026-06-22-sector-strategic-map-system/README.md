# 2026-06-22-sector-strategic-map-system — HoI4-Style Strategic Sector Map System

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 6+ military sandbox — Tier 3 Economy & Grand Strategy)
**Estimated effort:** M
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

A grand-strategy sector map (HoI4/WARNO-style province system) sits as a 2D abstraction layer above the 3D voxel world. Sectors are hex/grid cells with control state, supply level, fortification level, and unit presence.

We hypothesize:
- **H1 cost:** sector simulation <0.05 ms/tick for 10,000 sectors (0.15% of 30 Hz budget).
- **H2 control:** sector control transitions per tick based on supply + unit presence + fortification at <1 µs/sector.
- **H3 memory:** compact per-sector struct <64 bytes → 10k sectors × 64 B = 640 KiB total fits in L2 cache.
- **H4 query:** efficient spatial query ("sectors within radius R") <0.1 ms for 10k sectors.

5 strategies compared:
- A_NaivePerSector: update each sector independently every tick.
- B_HexGridOffset: hex grid coordinate system (axial coords, ~30% fewer neighbors than square).
- C_SparseActiveSet: only update "active" sectors (within player AOI or changed last tick).
- D_DeltaEncodedState: only sectors with state changes are updated + delta-encoded replication.
- E_ChunkedSpatialHash: hash sectors into 32-cell chunks, query by chunk + linear scan.

---

## 2. Prior art

Web-research (planned sources.md):
- Hearts of Iron IV (Paradox) — province system, supply network, control.
- Total War (Sega) — region system, control, building management.
- WARNO (Eugen Systems) — sector control for territorial mode.
- Stellaris (Paradox) — hyperlane sector grid.
- Civilization VI — city-state system, hex grid.
- Crusader Kings III — de jure / de facto control.

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 map sizes (100 / 1000 / 5000 / 10000 / 50000 sectors) × 5 activity rates (10/25/50/75/100% sectors active) × 5 seeds.
- **Metrics:** update time (µs/tick), memory (bytes), query time (µs for radius query), control transitions (per tick), supply propagation (per tick).
- **Control:** A_NaivePerSector baseline.
- **Protocol:** 5 strategies × 5 map sizes × 5 activity rates × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.

---

## 4. Prototype

`prototype/sector_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic sector_bench.cpp -o build/sector_bench
./build/sector_bench
```

Output: `build/results.csv` (626 rows) + `build/summary_means.csv` (6 rows).

---

## 5. Results

_To be filled after benchmark._

---

## 6. Verdict

_To be filled after analysis._

---

## 7. Integration recommendation

_To be filled after analysis._

---

## 8. Sources

_To be filled — see §2 list, will move to `sources.md` if extensive._

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** Stage 6+ military sandbox — Tier 3 Economy & Grand Strategy. Sector map updates per server tick (10 Hz) for persistent world simulation.
- **Prototype maps to:** `src/strategic/SectorMap.{hpp,cpp}` — function `updateSectors(dt)`, `querySectorsInRadius(center, R)`.
- **Assumptions:** 2D hex grid (axial coords); per-sector struct ~32-48 bytes; activity rate derived from player AOI (closed `interest-management-aoi-battle` mixed) + recent changes.
- **Unmeasured:** GPU compute path (orth to closed `dec-pipelines-async-compute`); multiplayer sync per closed `lockstep-state-sync-hybrid-netcode` mixed; save/load per closed `save-game-persistence-architecture`.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X (per §1).