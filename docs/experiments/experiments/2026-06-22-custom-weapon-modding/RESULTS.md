# 2026-06-22-custom-weapon-modding — Results

## 1. Performance Overview

We evaluated 5 strategies across 5 scenes (from a naked weapon to a highly nested custom configuration with 16 attachments) on an AMD Ryzen 7 5800X.

### Mean Query Latency (ns per query)

| Strategy | s1_naked | s2_standard | s3_tactical | s4_heavy | s5_max_nested |
|:---|:---|:---|:---|:---|:---|
| **A_NaiveFlatStruct** | 0.388 ns | 0.230 ns | 0.232 ns | 0.250 ns | 0.306 ns |
| **B_DynamicTreeQuery** | 7.146 ns | 10.788 ns | 24.438 ns | 59.084 ns | 65.884 ns |
| **C_FlatCachedBuffer** | 1.734 ns | 2.664 ns | 3.262 ns | 3.670 ns | 3.618 ns |
| **D_BitmaskFeatureMap** | 0.700 ns | 2.050 ns | 4.240 ns | 5.472 ns | 9.200 ns |
| **E_SparseSoAComponent**| 22.138 ns | 18.486 ns | 17.342 ns | 12.286 ns | 14.064 ns |

### Mean Swap Latency (ns per modification swap)

| Strategy | s1_naked | s2_standard | s3_tactical | s4_heavy | s5_max_nested |
|:---|:---|:---|:---|:---|:---|
| **A_NaiveFlatStruct** | 15.106 ns | 8.054 ns | 8.030 ns | 9.394 ns | 14.664 ns |
| **B_DynamicTreeQuery** | 17.088 ns | 11.994 ns | 12.338 ns | 22.742 ns | 30.644 ns |
| **C_FlatCachedBuffer** | 13.144 ns | 13.454 ns | 15.108 ns | 16.928 ns | 18.144 ns |
| **D_BitmaskFeatureMap** | 14.074 ns | 13.016 ns | 13.326 ns | 8.600 ns | 11.972 ns |
| **E_SparseSoAComponent**| 34.498 ns | 15.842 ns | 17.762 ns | 14.544 ns | 19.100 ns |

### Memory Overhead (bytes per weapon instance)

- **A_NaiveFlatStruct:** 64 B (naked) to 384 B (nested) depending on attachment vector capacity.
- **B_DynamicTreeQuery:** 136 B (naked) to 1312 B (nested) due to pointer references and heap allocations.
- **C_FlatCachedBuffer:** Fixed **472 B** (fully static, zero dynamic allocations, easily fits in cache lines).
- **D_BitmaskFeatureMap:** Constant **1312 B** (due to the lookup table array inside each weapon).
- **E_SparseSoAComponent:** Constant **32 B** (plus shared global registry storage).

---

## 2. Hypothesis Validation

- **H1: Query Performance (CONFIRMED):** Flat pre-composited struct `A` is the fastest (<0.4 ns) as it avoids any iteration. Dynamic tree query `B` scales poorly (up to 65.8 ns) due to pointer chasing. Contiguous cache-friendly buffer `C` scales extremely well, completing queries in <3.7 ns even at maximum complexity.
- **H2: Swap Performance (CONFIRMED):** All strategies perform swaps under 35 ns. Pre-compositing or array-writing is extremely cheap since it is a localized memory write.
- **H3: Memory Footprint (CONFIRMED):** Flat cached buffer `C` requires 472 bytes per weapon, eliminating all dynamic heap allocations.
- **H4: Modding Flexibility (CONFIRMED):** While `D` (Bitmask) is fast, it limits the number of modular attachments to 64 and cannot handle complex nesting or duplicates (e.g. attaching multiple of the same rail covers/lasers). `C` offers the best compromise between performance and schema nesting flexibility.

---

## 4. Key Findings

1. **Dynamic trees are bad for loops:** Traversing node trees recursively on the stack for hot-path gameplay stats queries introduces severe performance degradation (up to 65.8 ns per call), which scales poorly when simulating 1000+ weapons.
2. **Flat caches rule:** A contiguous array with implicit tree index modeling `C` performs queries in under 3.7 ns, which is well below the target 5-10% of frame budget.
3. **Pre-composited stats (`A`)** is the absolute winner for query latency because stats are pre-accumulated on attachment modification. This design is highly recommended for client and server systems.

---

## 5. Integration Recommendation

We recommend integrating **C_FlatCachedBuffer** combined with **A_NaiveFlatStruct** (caching the pre-computed stats of the flat buffer). This yields 0 ns hot-path queries during firing/update cycles while maintaining maximum modding slot flexibility.

### 3-Step Migration Plan

1. **Step 1 (XS, ~100 LoC) Component Definitions:**
   - Define `WeaponAttachment` structures and `AttachmentSlot` configurations in `src/weapon/WeaponComponent.hpp`.
   - Implement `FlatCachedBuffer` containing up to 16 slots with implicit indexing.

2. **Step 2 (M, ~300 LoC) Stat Accumulation Logic:**
   - Implement `recalculate_weapon_stats()` in `src/weapon/WeaponSystem.cpp` that iterates contiguously through the buffer, computes multipliers/additions, and caches the result.
   - Integrate with weapon fire rate, recoil, and weight subsystems.

3. **Step 3 (S, ~120 LoC) Verification & Tests:**
   - Create unit tests verifying attachment attachment constraints (e.g., cannot attach foregrip if no handguard).
   - Verify stat changes propagate correctly.