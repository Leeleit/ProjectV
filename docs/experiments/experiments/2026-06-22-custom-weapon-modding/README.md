# 2026-06-22-custom-weapon-modding — Custom Weapon Modding and Attachment System

**Status:** `in-progress`
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent
**Estimated effort:** M
**Author:** Agent

---

## 1. Hypothesis

A custom weapon modding system requires modeling a nested parent-child attachment hierarchy (e.g., base receiver -> handguard -> foregrip / rail covers / laser sight). During gameplay, weapon statistics (damage, recoil, ergonomics, weight, aim down sights speed) are queried on hot-paths (e.g., physics updates, bullet spawn, recoil simulation, movement speed calculation). 

We hypothesize that:
1. **Query Performance (H1):** Flat representations (Naive pre-composited struct `A` and Bitmask Feature Map `D`) will achieve sub-nanosecond query times, while Dynamic Heap Tree traversal `B` will suffer from pointer-chasing and cache misses (estimated at 20-50 ns). Contiguous cache-friendly flat buffer compiling `C` will achieve ~1-3 ns query time.
2. **Swap Performance (H2):** Swap/mod changes are rare events. Rebuilding a pre-composited struct or flat buffer (`A`, `C`, `D`) will take <15 ns, which is acceptable since swaps occur on the UI or loadout boundary, whereas dynamic tree traversal `B` or Flecs join `E` will be significantly slower.
3. **Memory Footprint (H3):** A contiguous flat buffer representation `C` can fit within a single cache line (e.g., <96 bytes per weapon instance), allowing massive weapon pools to reside in L1/L2 cache without heap allocations.
4. **Modding Flexibility (H4):** Rejection of `D` (Bitmask map) for production due to strict constraints on number/nesting of attachments. Flat compile-time schemas `C` with implicit tree indexing provide maximum modding flexibility with minimal runtime cost.

---

## 2. Prior art

We draw design patterns from:
1. **Escape from Tarkov (Battlestate Games):** Weapon modding uses a parent-child slot tree where attachments contain sub-slots. Recalculation is done upon modification and cached.
2. **Call of Duty Gunsmith (Infinity Ward / Activision):** Stat compilation from 5-10 attachments. Uses flat linear lists of modifiers (addition/multiplication) compiled to a cached status.
3. **Data-Oriented Design & ECS (flecs / Jolt):** Contiguous structures of components (SoA/AoS) to avoid pointer indirection during iteration and batch processing.

---

## 3. Method

- **Тип эксперимента:** prototype + benchmark.
- **Сцена:** 100 weapons with different attachment configurations (s1: naked, s2: standard 3 attachments, s3: heavy 7 attachments, s4: maximum 12 attachments, s5: nested invalid / random mods).
- **Метрики:** query time (ns per query), swap time (ns per attachment change), memory overhead (bytes).
- **Контроль:** A_NaiveFlatStruct (composited flat struct).
- **Протокол:** 
  1. Initialize weapon configurations.
  2. Run N=1000 iterations of query benchmarks.
  3. Run N=1000 iterations of swap benchmarks.
  4. Compare performance metrics across Zen 3 5800X governor=`powersave`.

---

## 4. Prototype

Standalone C++26 benchmark prototype.
Build and run commands:
```bash
mkdir -p prototype/build
clang++ -std=c++26 -O3 -march=native -DNDEBUG prototype/weapon_mod_bench.cpp -o prototype/build/weapon_mod_bench
./prototype/build/weapon_mod_bench
```

---

## 5. Results

_Pending benchmark execution._

---

## 6. Verdict

_Pending benchmark execution._

---

## 7. Integration recommendation

_Pending benchmark execution._

---

## 8. Sources

- Wikipedia "Entity-component-system"
- Wikipedia "Data-oriented design"
- Escape from Tarkov Wiki: Weapon modification mechanics.

---

## 9. Mapping to ProjectV hot-path

- `src/weapon/WeaponSystem` and `src/weapon/WeaponComponent`.
- Assumes synchronous query calls during shooting or weapon draw.
- Hardware baseline: dev host `obvium` (Ryzen 7 5800X).
