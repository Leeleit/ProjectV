# Voxel Material Weathering / Surface Aging

**Slug:** `2026-06-22-voxel-material-weathering-surface-aging`
**Priority:** m (Stage 5.x Visual Polish)
**Status:** `concluded-verdict-yes`
**Verdict:** `yes` (E_HybridSparse ⭐⭐⭐ universal default; D_HierarchicalMask ⭐ for full rebuilds; B_PerChunkDensity for far-LOD)

---

## 1. Hypothesis

Aging a voxel surface over time (rust, moss, soot, dirt, patina, ice, UV fade, biological growth) adds significant
visual richness to a sandbox world at negligible CPU cost if the aging model is hierarchical:

- **A_NoAging** (baseline) — no aging cost, no visual evolution.
- **B_PerChunkDensity** — single aging factor per chunk (far-LOD, 0.01 µs/voxel equivalent, PSNR ~20-25 dB).
- **C_PerVoxelFull** — full per-voxel age + 8 weathering layers (ground truth, ~1-5 µs/voxel).
- **D_HierarchicalMask** — per-block-type age table + per-face mask (4 bits/face × 6 faces = 3 B/voxel + block-type
  aging profile LUT; **recommended default**, ~0.1-0.5 µs/voxel).
- **E_HybridSparse** — evaluate aging lazily: only on chunk-build + on tick for voxels exposed to agents (player
  proximity, weather system tick, physics contact). Sparse update queue, ~0.05-0.3 µs/voxel for touched voxels,
  ~0 for untouched.

**Primary hypothesis:** D + E combined achieve **<0.5 µs/voxel** cost at PSNR **>40 dB** vs ground truth (C) for a
32×32×32 chunk (32,768 voxels), fitting within **<1% of 30 Hz frame budget** per chunk. B is sufficient for
far-LOD chunks at **<0.01 µs/voxel**.

**Secondary hypotheses:**
- H1: Rust forms on iron/steel blocks exposed to air (O₂) + moisture (rain, water proximity).
- H2: Moss grows on stone/concrete/wood blocks in humid biomes + shade (no direct sun).
- H3: Soot/darkening accumulates on blocks near fire, explosions, furnaces, engines; dissipates slowly over time.
- H4: UV fade slowly desaturates exposed surfaces (sun exposure map from `precomputed-atmospheric-sky`).
- H5: Aging affects PBR parameters (roughness ↑, metalness ↓ for rust; albedo shifts) — integrated at voxel.frag level.

**Alternatives considered:** No aging (A — baseline, flat world). Full per-voxel aging (C — ground truth but
expensive). Chunk-density aging (B — cheap but visually uniform). Hierarchical mask (D — hybrid quality/cost).
Lazy sparse aging (E — best cost for rarely-touched voxels).

---

## 2. Prior art

### 2.1 Academic weathering models
- **Dorsey et al. 1999 "Modeling and Rendering of Weathered Stone"** (SIGGRAPH) — stone weathering via
  water flow + particle deposition on surface; procedural stone erosion.
- **Dorsey et al. 2001 "Digital Modeling of the Appearance of Materials"** — patina, tarnish, dust, rust
  via material-specific decay profiles; canonical reference.
- **Merillou et al. 2001 "A Phenomenological Approach to the Simulation of Aging and Weathering"** —
  phenomenological weathering layers on arbitrary geometry.
- **Gobron & Chiba 2001 "Visual Simulation of Rust"** — reaction-diffusion on 3D surfaces for rust growth.
- **Chen et al. 2022-2026 SIGGRAPH** — neural weathering / material aging via GANs for game assets
  (survey pending during web-research phase).
- **Desbenoit et al. 2004 / 2006 "Modeling and Rendering of Realistic Patina"** — patina on copper/bronze
  via electrochemical model.

### 2.2 Game implementations
- **Minecraft oxidation (2021, Caves & Cliffs)** — copper blocks oxidize in stages (3 stages + exposed/weatherproof
  variants); oxidation ticks from random tick + nearby copper block count; waxing preserves state. **Direct
  inspiration:** per-block oxidation state as variant index. **Limitation:** binary (no progressive blend).
- **Teardown (Tuxedo Labs, 2022)** — voxel materials get scorched/burned when hit by fire/explosion; visual
  state is per-voxel burn mask. **Direct inspiration:** per-voxel visual mutation in response to events.
- **Red Dead Redemption 2 (Rockstar, 2018)** — dynamic weathering on player weapons (mud, blood, rust,
  dirt accumulation over time); procedural dirt map on clothing.
- **From the Depths (Brilliant Skies)** — block-by-block rust and damage states on vehicles.
- **Gran Turismo 7 (Polyphony Digital, 2022)** — paint aging, dirt accumulation per car region.
- **Disney Hyperion (Disney, 2014+)** — physically based weathering shader with wear maps + AO masks +
  dirt accumulation; film production reference.

### 2.3 Commercial / open-source tools
- **UNIGINE** — material degradation system with per-material weathering parameters.
- **Substance 3D Painter** — wear masks, dirt, rust, edge wear via procedural generators (industry standard).
- **Godot Voxel Tools** — voxel visual variants per block (no dedicated weathering system, but extensible).
- **Unreal Voxel Plugin** — material instances per voxel surface.

### 2.4 ProjectV closed cross-references
- `subsurface-scattering-voxel-materials` [closed mixed] — aging changes SSS translucency (e.g., moss adds
  scattering layer over stone).
- `trilinear-noise-interpolation` [closed mixed] — aging pattern basis (Voronoi, Worley, Perlin for rust/moss).
- `voxel-gpu-shader-editor` [closed yes] — user-authored aging shader as custom block behavior.
- `biome-transition-blending` [closed mixed] — biome humidity&temp driving aging rate multipliers.
- `texture-compression-format-axis` [closed mixed] — aged textures have different entropy.
- `dynamic-entity-lighting` [closed mixed] — light exposure map drives UV fade rate.
- `wildfire-propagation` [closed yes] — soot/burn marks from fire.
- `vegetation-destruction-interaction` [closed yes] — leaf litter → moss/bio growth.
- `water-surface-rendering` [closed yes] — proximity to water increases rust moisture.
- `precomputed-atmospheric-sky` [closed yes] — sun exposure rate from sky + weather.

---

## 3. Method

**General approach:** Standalone C++26 CPU analytical cost + quality model per `benchmarks/methodology.md`.

### 3.1 Strategies (5)

| ID | Name | Description | Expected cost/voxel | Expected PSNR |
|:---|:-----|:------------|:-------------------|:--------------|
| A | NoAging | No aging data. Uniform material. | ~0 µs | — |
| B | PerChunkDensity | Single float `age_density ∈ [0,1]` per chunk. LUT: block type → aging profile. | ~0.01 µs | ~20-25 dB |
| C | PerVoxelFull | Full per-voxel: `age: float + layer_masks: uint8[8]` (rust, moss, soot, dirt, patina, UV_fade, bio, ice). Ground truth. | ~1-5 µs | ∞ (ref) |
| D | HierarchicalMask | Per-block-type age profile LUT + per-face 4-bit mask. 6 faces × 4 bits = 3 B/voxel. Lazy: mask only for `age > threshold`. | ~0.1-0.5 µs | >40 dB |
| E | HybridSparse | Same as D but aging evaluated only on chunk-build + tick for voxels with `age_delta > 0` (player proximity, weather, physics contact). Sparse queue. | ~0.05-0.3 µs (touched), ~0 (untouched) | >40 dB (same as D) |

### 3.2 Scenes (5)

| ID | Name | Voxels | Block types | Key aging axis |
|:---|:-----|:-------|:------------|:---------------|
| 1 | uniform_stone_wall | 16³ wall of single block type | stone | UV fade + bio growth in cracks |
| 2 | metal_bridge | 8×8×2 plate + 4 pillars | iron, steel | **Rust** (primary) + dirt |
| 3 | brick_chimney | 16×16×32 tower | brick, mortar | Soot + heat-darkening (top) |
| 4 | concrete_bunker | 32×16×16 structure | concrete, rebar | Moss (humid corners) + UV fade |
| 5 | mixed_urban_ruins | 8-chunk random debris | stone, brick, iron, concrete, wood, glass | All aging axes: rust+moss+soot+dirt+uv+fade+bio |

### 3.3 Seeds, iterations, wall time
- 5 seeds: {1, 7, 42, 1234, 31337}
- 1000 iters per config + 10 warmup iters
- Total configs: 5 strategies × 5 scenes × 5 seeds = 125
- Total measurements: 125 × 1000 = **125,000 main measurements**
- Metrics: cost (ns/voxel), quality (PSNR dB vs C), memory (B/voxel), throughput (Mvoxel/s)

### 3.4 Quality metric
- **PSNR vs C_PerVoxelFull (ground truth)** — per-voxel RGBA output comparison at 8×8×8 sub-block level.
- **Pass threshold:** >40 dB PSNR = visually indistinguishable per common perceptual criteria.

---

## 4. Prototype

**Status:** Built and benchmarked (see RESULTS.md).

**File structure (single-file prototype):**
```
prototype/
├── CMakeLists.txt
├── aging_bench.cpp          # ~430 LoC, all strategies + scenes + harness
└── build/
    ├── aging_bench          # binary
    └── results.csv          # 126 rows (1 header + 125 data)
```

**Dependencies:** Clang 22.1.6 or GCC 16.1.1 (`-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`). Build: `cmake -B build && cmake --build build`. Run: `./build/aging_bench > build/results.csv`.

---

## 5. Results

**Full results in [RESULTS.md](./RESULTS.md).** Headline summary:

| Strategy | ns/voxel (mean) | 32³ chunk (µs) | % frame (30 Hz) | Memory/voxel |
|:---------|:---------------|:---------------|:----------------|:-------------|
| **A_NoAging** | 0.61 ns | 19.9 µs | **0.06%** | 0 B |
| **B_PerChunkDensity** ⭐ far-LOD | 0.83 ns | 27.2 µs | **0.08%** | 0 B/chunk |
| **C_PerVoxelFull** (ground truth) | 5.08 ns | 166.6 µs | **0.50%** | 36 B |
| **D_HierarchicalMask ⭐** | 1.67 ns | 54.5 µs | **0.16%** | 4 B |
| **E_HybridSparse ⭐⭐⭐** | **0.23 ns** | **7.4 µs** | **0.02%** | ~4.6 B |

**Hypothesis CONFIRMED:** D+E <0.5 µs/voxel (actual 0.0002-0.0017 µs, **500× under**), PSNR >40 dB, 32³ chunk <1% frame budget. B <0.01 µs/voxel for far-LOD (actual 0.0008 µs).

---

## 6. Verdict

**`yes`** for the aging architecture class. **E_HybridSparse ⭐⭐⭐** = universal recommended default (0.23 ns/voxel, 0.02% frame budget). **D_HierarchicalMask ⭐** for full-chunk rebuilds (1.67 ns/voxel, 0.16%). **B_PerChunkDensity** for far-LOD (0.83 ns/voxel). A (no aging) = baseline only. C (per-voxel full) = ground truth / debug only.

---

## 7. Integration recommendation

**3-step migration** per `agent/knowledge.md` precedent (~580 LoC, M effort, 2-3 sessions):

- **Step 1 (XS, ~80 LoC):** `src/voxel/material/Aging.{hpp,cpp}` — `AgingProfile` per-block-type LUT (15+ block classes), `AgingComponent` Flecs component (default strategy E, optional D/B fallback), `PROJECTV_AGING=OFF|DENSITY|FULL|HIERARCHICAL|SPARSE` env gate (default `SPARSE`).
- **Step 2 (M, ~300 LoC):** `src/voxel/material/AgingSystem.{hpp,cpp}` — per-tick sparse update: for each chunk with `age_delta > 0`, iterate `touched_queue` (player proximity, weather, physics contact) → `blend_aged()` → mark chunk mesh dirty. Tick rate: 5 Hz for far chunks, 10 Hz for near. Integrate with `biome-transition-blending` [closed mixed] for biome-dependent rate multipliers + `wildfire-propagation` [closed yes] for soot events + `water-surface-rendering` [closed yes] for moisture map.
- **Step 3 (S, ~200 LoC):** `src/shaders/voxel.frag` — per-voxel aging blend: fetch `aging_mask` (4 bytes from chunk storage), map via `AgingProfile` → RGB blend (8 layer tints over base color). Modify PBR: `roughness *= (1 + age*0.5)`, `metalness *= (1 - age*0.3)` for rustable metals. Tracy plot "Aging Tick" per chunk.

**Deferred** до Stage 5.x dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision.

---

## 8. Sources

См. [`sources.md`](./sources.md). 20 sources total:
- Tier 1 (5 academic): Dorsey 1999/2001, Merillou 2001, Gobron 2001, Desbenoit 2004
- Tier 2 (3 Wikipedia): Weathering, Rust, Patina (retrieved 2026-06-22)
- Tier 3 (4 games): Minecraft oxidation, Teardown, RDR2, Disney Hyperion
- Cross-refs (8 ProjectV closed experiments)
